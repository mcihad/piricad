// SPDX-License-Identifier: GPL-3.0-or-later
// core.ellipse_draw — ELİPS. Centre and the two axis endpoints.
//
// STORED AS ITS DEFINITION, like DAİRE and YAY: five numbers, from which the
// whole shape follows. The many-sided run is only what gets drawn, and a drawing
// that stored the run would lose the shape's identity the moment somebody zoomed
// in (core/ellipse.hpp).
//
// THE SECOND AXIS IS TAKEN PERPENDICULAR to the first, at the distance the user
// pointed at. Letting them place it freely would let them draw a sheared shape
// that is not an ellipse at all — and the record would then hold two axes that no
// ellipse has, which nothing downstream could draw.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/units.hpp"

#include <cmath>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Elipsin merkezi");
    if (!centre) co_return; // ESC before anything was drawn

    auto major = co_await ctx.point("birinci", "Birinci eksenin ucu",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Line});
    if (!major) co_return;

    if (major->x == centre->x && major->y == centre->y) {
        ctx.echo("Birinci eksenin ucu merkezle aynı yerde; elipsin ekseni sıfır olamaz.");
        co_return;
    }

    auto reach = co_await ctx.point("ikinci", "İkinci eksenin uzaklığı",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Line});
    if (!reach) co_return;

    // The first axis as a vector, and the perpendicular to it.
    const auto ax = static_cast<double>(major->x - centre->x);
    const auto ay = static_cast<double>(major->y - centre->y);
    const double a_len = std::sqrt(ax * ax + ay * ay);

    // How far the third click reached, measured ACROSS the first axis: the
    // component perpendicular to it. Clicking along the first axis therefore
    // gives a second axis of zero, and the command says so rather than drawing a
    // line and calling it an ellipse.
    const auto rx = static_cast<double>(reach->x - centre->x);
    const auto ry = static_cast<double>(reach->y - centre->y);
    const double across = (rx * -ay + ry * ax) / a_len;

    const double b = across < 0.0 ? -across : across;
    if (b < 1.0) {
        ctx.echo("İkinci eksen sıfır: üçüncü nokta birinci eksenin üzerinde. "
                 "Eksene dik bir yer seçin.");
        co_return;
    }

    // The perpendicular unit vector, scaled to that distance.
    const core::Point2 minor{centre->x + core::mm_round(-ay / a_len * b),
                             centre->y + core::mm_round(ax / a_len * b)};

    auto created = ctx.transaction().add_ellipse(ctx.active_layer(), *centre, *major, minor);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // RECORDED AS IT WAS ASKED, like YAY: the three points the user gave, not the
    // perpendicular this derived from them. A replay must walk the same arithmetic
    // rather than trust a number this run happened to round.
    ctx.record("merkez", Value::point(*centre));
    ctx.record("birinci", Value::point(*major));
    ctx.record("ikinci", Value::point(*reach));
}

} // namespace

KENTOS_COMMAND(ellipse_draw)
{
    return CommandSpec{
        .id       = "core.ellipse_draw",
        .names    = {"ELİPS", "ELIPS", "ELLIPSE", "EL"},
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Elipsin merkezi"),
                Param::point("birinci", "Birinci eksenin ucu"),
                Param::point("ikinci", "İkinci eksenin uzaklığı; eksene dik ölçülür"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.",
        .run     = &run,
    };
}

} // namespace kentos::command
