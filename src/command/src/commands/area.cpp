// SPDX-License-Identifier: GPL-3.0-or-later
// core.area — ALAN. A closed face: one exterior ring, optionally with holes.
//
// This is the command a parcel is drawn with, and until it existed the document
// could hold a face but nothing could create one: Transaction::add_area was
// written, tested through the io layer, and reachable from no client at all. A
// cadastral program whose only draw command produces open polylines cannot state
// that a parcel encloses an area, which is the one thing a parcel does.
//
// Like ÇİZGİ it never asks where its points came from (piricad.md §2.4).
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/geometry.hpp"

#include <vector>

namespace piricad::command {
namespace {

/// Rings arrive as one flat point list plus a per-vertex marker of where each
/// ring ends, because a command invocation has to round-trip losslessly through
/// `Value` (Article 1.4) and `Value::Points` is a flat list. A ring is closed by
/// the geometry layer, so the caller never repeats the first vertex.
struct RingBuild
{
    std::vector<core::Point2> points;
    core::RingRole role{core::RingRole::Exterior};
};

/// R11 wants the exterior first and its holes after it. The interactive path
/// produces exactly that order, and a script's flat list is split the same way.
Task<void> run(Context& ctx)
{
    std::vector<RingBuild> built;
    Value::Points recorded;

    // `bolum` marks where one ring ends and the next begins: the count of
    // vertices in each ring, in order. Absent means "one exterior ring".
    const Value breaks = ctx.argument("bolum");

    auto p1 = co_await ctx.point("noktalar", "Alanın ilk köşesi");
    if (!p1) co_return; // ESC before anything was drawn

    built.push_back(RingBuild{{*p1}, core::RingRole::Exterior});
    recorded.push_back(*p1);
    core::Point2 previous = *p1;

    while (auto p2 = co_await ctx.point("noktalar", "Sonraki köşe", PointOptions{true, previous})) {
        built.back().points.push_back(*p2);
        recorded.push_back(*p2);
        previous = *p2;
    }

    if (built.back().points.size() < 3) {
        ctx.echo("Bir alan en az üç köşe ister; " + std::to_string(built.back().points.size()) +
                 " nokta verildi.");
        co_return;
    }

    // Split the flat list into rings when the caller declared the breaks. The
    // first run is the exterior; every run after it is a hole in that exterior,
    // which is the only shape a single ALAN produces. A separate face is a
    // separate ALAN, so nothing here has to guess at part numbering.
    if (!breaks.empty()) {
        const Value::Ints& counts = breaks.as_ids();
        std::vector<RingBuild> split;
        std::size_t at = 0;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            const auto n = counts[i] > 0 ? static_cast<std::size_t>(counts[i]) : 0;
            if (n < 3 || at + n > recorded.size()) {
                ctx.echo("'bolum' değerleri nokta listesiyle uyuşmuyor: " +
                         std::to_string(recorded.size()) + " nokta verildi, " +
                         "halka uzunlukları toplamı bunu aşıyor ya da üçten kısa bir halka var.");
                co_return;
            }
            RingBuild r;
            r.points.assign(recorded.begin() + static_cast<std::ptrdiff_t>(at),
                            recorded.begin() + static_cast<std::ptrdiff_t>(at + n));
            r.role = i == 0 ? core::RingRole::Exterior : core::RingRole::Interior;
            split.push_back(std::move(r));
            at += n;
        }
        if (at != recorded.size()) {
            ctx.echo("'bolum' değerleri nokta listesini tam kapatmıyor: " +
                     std::to_string(recorded.size()) + " noktanın " + std::to_string(at) +
                     " tanesi halkalara dağıtıldı.");
            co_return;
        }
        built = std::move(split);
    }

    std::vector<core::RingGeometry::RingInput> rings;
    rings.reserve(built.size());
    for (const RingBuild& r : built)
        rings.push_back(core::RingGeometry::RingInput{r.points, r.role, 0});

    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        // The geometry layer refuses a zero-area ring, a hole that escapes its
        // exterior and a ring that crosses itself. Its message names which.
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    ctx.record("noktalar", Value::points(std::move(recorded)));
    if (!breaks.empty()) ctx.record("bolum", breaks);
}

} // namespace

PIRICAD_COMMAND(area)
{
    return CommandSpec{
        .id       = "core.area",
        .names    = {"ALAN", "AREA", "POLİGON", "POLIGON", "AL"},
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::at_least(3),
                              "Alanın köşe noktaları; kapanış noktası tekrarlanmaz"),
                Param::integer("bolum", Arity::at_least(0),
                               "Halka uzunlukları: ilki dış sınır, sonrakiler delik"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kapalı bir alan çizer; istenirse içine delik açar.",
        .run     = &run,
    };
}

} // namespace piricad::command
