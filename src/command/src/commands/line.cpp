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
    // THE RUN IS HELD UNTIL IT ENDS: every segment is on the canvas as it is
    // drawn (`rubber_chain`), and each is written as its own object when the run
    // is over. Written as it went, a wrong corner could only be undone with the
    // whole drawing — and taking one object back would have burned its key,
    // which is never reused, so the journal would replay to different keys
    // (TODOS C-02). ⌫, Ctrl+Z and `U` take the newest corner back; the snap
    // still finds the run's own corners (`SnapQuery::pending`).
    Value::Points drawn;
    while (drawn.empty()) {
        auto p1 = co_await ctx.point("noktalar", "İlk nokta");
        if (!p1) co_return; // ESC / arguments exhausted
        drawn.push_back(*p1);

        for (;;) {
            auto p2 = co_await ctx.point("noktalar", "Sonraki nokta — ⌫: son noktayı geri al",
                                         PointOptions{.rubber_band   = true,
                                                      .rubber_origin = drawn.back(),
                                                      .rubber_chain  = drawn,
                                                      .can_retract   = true});
            if (p2) {
                drawn.push_back(*p2);
                continue;
            }
            if (!ctx.took_back()) break;
            drawn.pop_back(); ///< back to the first point asks for it again
            if (drawn.empty()) break;
        }
    }

    for (std::size_t i = 0; i + 1 < drawn.size(); ++i) {
        const std::array<core::Point2, 2> segment{drawn[i], drawn[i + 1]};

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
    }

    // Record the run exactly as it happened, so replaying the journal from any
    // client reproduces it vertex for vertex (kentoscad.md §2.2).
    if (drawn.size() < 2) co_return; // ESC after the first point: nothing drawn
    const std::size_t segments = drawn.size() - 1;
    ctx.record("noktalar", Value::points(std::move(drawn)));

    // SAY WHAT WAS MADE, because the one thing that separates this tool from
    // ÇOKLUÇİZGİ is invisible on the canvas: three clicks give two objects here
    // and one object there, and the two drawings look identical until the user
    // tries to select the run and gets a piece of it. Naming the other tool at
    // the moment the difference is born is the cheapest place to teach it.
    if (segments == 1)
        ctx.echo("1 çizgi çizildi.");
    else
        ctx.echo(std::to_string(segments) +
                 " çizgi çizildi, her biri ayrı nesne (tek nesne için ÇOKLUÇİZGİ).");
}

} // namespace

KENTOS_COMMAND(line)
{
    return CommandSpec{
        .id       = "core.line",
        .names    = {"ÇİZGİ", "CIZGI", "LINE", "Ç", "L"},
        .title    = "Çizgi",
        .category = Category::Draw,
        .params   = {Param::points("noktalar", Arity::at_least(2),
                                   "Ardışık doğru parçalarının köşe noktaları")
                         .en("points")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "İki veya daha fazla nokta arasında doğru parçaları çizer.",
        .run      = &run,
    };
}

} // namespace kentos::command
