// SPDX-License-Identifier: GPL-3.0-or-later
// core.line — ÇİZGİ. The reference implementation of piricad.md §2.4.
//
// The body below is the entire command. It contains no branch on where its input
// comes from: a mouse click, a typed coordinate, the next element of a script's
// point list and an AI-produced point all arrive through the same co_await.
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include <array>

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    auto p1 = co_await ctx.point("noktalar", "İlk nokta");
    if (!p1) co_return; // ESC / arguments exhausted

    Value::Points drawn{*p1};
    core::Point2 previous = *p1;

    while (auto p2 =
               co_await ctx.point("noktalar", "Sonraki nokta", PointOptions{true, previous})) {
        const std::array<core::Point2, 2> segment{previous, *p2};

        auto created = ctx.transaction().add_polyline(ctx.active_layer(), segment);
        if (!created) {
            ctx.echo(created.error().message);
            break; // transaction rolls back on the bus
        }

        drawn.push_back(*p2);
        previous = *p2;
    }

    // Record the run exactly as it happened, so replaying the journal from any
    // client reproduces it vertex for vertex (piricad.md §2.2).
    if (drawn.size() >= 2) ctx.record("noktalar", Value::points(std::move(drawn)));
}

} // namespace

PIRICAD_COMMAND(line)
{
    return CommandSpec{
        .id       = "core.line",
        .names    = {"ÇİZGİ", "CIZGI", "LINE", "Ç", "L"},
        .category = Category::Draw,
        .params   = {Param::points("noktalar", Arity::at_least(2),
                                   "Ardışık doğru parçalarının köşe noktaları")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "İki veya daha fazla nokta arasında doğru parçaları çizer.",
        .run      = &run,
    };
}

} // namespace piricad::command
