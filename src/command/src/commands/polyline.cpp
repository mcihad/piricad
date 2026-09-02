// SPDX-License-Identifier: GPL-3.0-or-later
// core.polyline — ÇOKLUÇİZGİ. Many vertices, ONE object.
//
// `ÇİZGİ` writes each segment as its own entity, which is the right answer when a
// user is drawing unrelated lines and the wrong one when they are drawing a
// boundary: a road edge that is fourteen separate objects cannot be selected as
// one, styled as one, given one attribute row, or exported as one feature. A
// GeoPackage LineString is one geometry with many vertices, and so is what comes
// back from a total station.
//
// So this is the same gesture producing one open ring. The two commands stay
// separate because both intentions are real and neither is a special case of the
// other — and because changing what `ÇİZGİ` produces would change every drawing
// already made with it.
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    std::vector<core::Point2> points;

    auto p1 = co_await ctx.point("noktalar", "İlk nokta");
    if (!p1) co_return; // ESC before anything was drawn

    points.push_back(*p1);
    core::Point2 previous = *p1;

    // The whole run is drawn at the end, so the guide carries every vertex fixed
    // so far — otherwise each click would appear to erase the one before it, the
    // way it did for ALAN before `rubber_chain` existed (input.hpp).
    while (auto next = co_await ctx.point("noktalar", "Sonraki nokta",
                                          PointOptions{.rubber_band   = true,
                                                       .rubber_origin = previous,
                                                       .rubber_chain  = points})) {
        points.push_back(*next);
        previous = *next;
    }

    if (points.size() < 2) {
        ctx.echo("Bir çoklu çizgi en az iki nokta ister; " + std::to_string(points.size()) +
                 " nokta verildi.");
        co_return;
    }

    auto created = ctx.transaction().add_polyline(ctx.active_layer(), points);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    ctx.record("noktalar", Value::points(points));
    ctx.echo(std::to_string(points.size()) + " noktalı tek çoklu çizgi çizildi.");
}

} // namespace

PIRICAD_COMMAND(polyline)
{
    return CommandSpec{
        .id       = "core.polyline",
        .names    = {"ÇOKLUÇİZGİ", "COKLUCIZGI", "POLYLINE", "ÇÇ", "PL"},
        .category = Category::Draw,
        .params   = {Param::points("noktalar", Arity::at_least(2),
                                   "Çoklu çizginin köşe noktaları; hepsi tek nesne olur")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Birden çok noktadan TEK bir çizgi nesnesi çizer.",
        .run      = &run,
    };
}

} // namespace piricad::command
