// SPDX-License-Identifier: GPL-3.0-or-later
// core.spline — SPLINE. A NURBS curve through its control points.
//
// The user gives CONTROL POINTS, as `SPLINE` in every CAD program lets them, and
// a degree; the knots are uniform and clamped, so the curve starts at the first
// point and ends at the last. What is stored is the definition (core/spline.hpp):
// the points, the degree, the knots. The drawn curve is de Boor's algorithm over
// them, the same on every machine.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/spline.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    std::vector<core::Point2> points;

    // The degree: three unless asked otherwise, and never more than the points
    // allow — a cubic through two points is a line, and is drawn as one.
    std::int64_t degree = 3;
    if (const Value given = ctx.argument("derece"); !given.empty()) degree = given.as_int();
    if (degree < 1 || degree > 15) {
        ctx.echo("Spline derecesi 1 ile 15 arasında olmalı; verilen " + std::to_string(degree) +
                 ".");
        co_return;
    }
    bool closed = false;
    if (const Value given = ctx.argument("kapali"); !given.empty()) closed = given.as_bool();

    // What the preview draws with: the degree and the closure, so the curve
    // under the cursor is the curve the click will make (core/spline.hpp).
    core::SplineDef preview;
    preview.degree                                  = static_cast<std::uint8_t>(degree);
    preview.closed                                  = closed;
    const std::vector<std::uint8_t> preview_payload = core::encode_spline(preview);

    auto p1 = co_await ctx.point("noktalar", "İlk kontrol noktası");
    if (!p1) co_return;
    points.push_back(*p1);
    core::Point2 previous = *p1;

    while (auto next = co_await ctx.point("noktalar", "Sonraki kontrol noktası",
                                          PointOptions{.rubber_band    = true,
                                                       .rubber_origin  = previous,
                                                       .rubber_shape   = RubberShape::Curve,
                                                       .rubber_chain   = points,
                                                       .rubber_payload = preview_payload})) {
        points.push_back(*next);
        previous = *next;
    }

    if (points.size() < 2) {
        ctx.echo("Bir spline en az iki kontrol noktası ister; " + std::to_string(points.size()) +
                 " nokta verildi.");
        co_return;
    }
    if (static_cast<std::size_t>(degree) + 1 > points.size()) {
        degree = static_cast<std::int64_t>(points.size()) - 1;
        ctx.echo("Nokta sayısı " + std::to_string(points.size()) + " olduğu için derece " +
                 std::to_string(degree) + "'e düşürüldü.");
    }

    core::SplineDef def;
    def.degree     = static_cast<std::uint8_t>(degree);
    def.closed     = closed;
    def.knots_nano = core::uniform_clamped_knots(points.size(), static_cast<int>(degree));

    const core::RingGeometry::RingInput ring{points, core::RingRole::Open, 0};
    const std::vector<std::uint8_t> payload = core::encode_spline(def);
    auto created                            = ctx.transaction().add_kind(
        ctx.active_layer(), core::kSplineKind,
        std::span<const core::RingGeometry::RingInput>(&ring, 1), payload);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    ctx.record("noktalar", Value::points(points));
    ctx.record("derece", Value::integer(degree));
    if (closed) ctx.record("kapali", Value::boolean(true));
    ctx.echo(std::to_string(points.size()) + " kontrol noktalı, " + std::to_string(degree) +
             ". dereceden spline çizildi.");
}

} // namespace

KENTOS_COMMAND(spline)
{
    return CommandSpec{
        .id       = "core.spline",
        .names    = {"SPLINE", "SPLINE", "SPLINE", "SPL"},
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::at_least(2), "Kontrol noktaları"),
                Param::integer("derece", Arity::optional(), "Eğrinin derecesi, 1–15; varsayılan 3"),
                Param::boolean("kapali", Arity::optional(),
                               "Son noktadan ilkine kapansın mı; varsayılan hayır"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kontrol noktalarından NURBS eğrisi (spline) çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
