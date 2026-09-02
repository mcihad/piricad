// SPDX-License-Identifier: GPL-3.0-or-later
// core.line — ÇİZGİ. The reference implementation of kentoscad.md §2.4.
//
// The body below is the entire command. It contains no branch on where its input
// comes from: a mouse click, a typed coordinate, the next element of a script's
// point list and an AI-produced point all arrive through the same co_await.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <array>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto p1 = co_await ctx.point("noktalar", "İlk nokta");
    if (!p1) co_return; // ESC / arguments exhausted

    Value::Points drawn{*p1};
    core::Point2 previous = *p1;

    while (auto p2 =
               co_await ctx.point("noktalar", "Sonraki nokta",
                                  PointOptions{.rubber_band = true, .rubber_origin = previous})) {
        const std::array<core::Point2, 2> segment{previous, *p2};

        auto created = ctx.transaction().add_polyline(ctx.active_layer(), segment);
        if (!created) {
            // FAIL, not echo-and-break. Two things were wrong with breaking.
            //
            // The whole transaction has to roll back (Article 1.6): a run that
            // drew four segments and was refused the fifth left the four
            // committed, which is a half-applied command.
            //
            // And the user was told twice. The refusal was echoed, the loop
            // ended with one point in hand, and the invocation was then reported
            // as `'noktalar' wants at least 2 values, 1 came` — a second message
            // about the shape of the arguments, when what actually happened is
            // that the layer is locked.
            ctx.session().fail(created.error());
            co_return;
        }

        drawn.push_back(*p2);
        previous = *p2;
    }

    // Record the run exactly as it happened, so replaying the journal from any
    // client reproduces it vertex for vertex (kentoscad.md §2.2).
    if (drawn.size() >= 2) ctx.record("noktalar", Value::points(std::move(drawn)));
}

} // namespace

KENTOS_COMMAND(line)
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

} // namespace kentos::command
