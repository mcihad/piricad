// SPDX-License-Identifier: GPL-3.0-or-later
// core.rectangle — DİKDÖRTGEN. A four-cornered face from two opposite corners.
//
// WHY A COMMAND OF ITS OWN, when ALAN already draws any face.
//
// Because a rectangle is what a plan is mostly made of — a building footprint, a
// setback boundary, a plan sheet's own frame — and asking for four corners when
// two determine the shape is asking the user to place two points that the
// program can compute and can guarantee are square. Clicking them by hand gives
// four corners that are nearly right, and "nearly right" in a cadastral drawing
// is a defect that survives into the signed sheet.
//
// The second corner is the one OPPOSITE the first, not the next one along, which
// is what every drawing program means by a rectangle drag and what makes the
// diagonal lock produce a square: lock the aim to 45° from the first corner and
// |dx| equals |dy| (see `core.yakalama.kosegen`).
//
// Degenerate input is refused rather than drawn: two corners sharing an x or a y
// enclose nothing, and the geometry layer would reject the ring anyway — saying
// so here names the corner the user has to move.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/geometry.hpp"

#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto first = co_await ctx.point("noktalar", "Dikdörtgenin ilk köşesi");
    if (!first) co_return; // ESC before anything was drawn

    // The rubber band starts at the first corner, so the diagonal lock — and
    // ortho, and polar, and every object snap — measure from it exactly as they
    // do for a line. Nothing here is a private input path (kentoscad.md §2.4).
    auto second = co_await ctx.point("noktalar", "Karşı köşe",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *first,
                                                  .rubber_shape  = RubberShape::Rectangle});
    if (!second) co_return;

    if (first->x == second->x || first->y == second->y) {
        ctx.echo("Bu iki köşe bir alan kapatmaz: karşı köşenin hem doğusu hem kuzeyi "
                 "ilkinden farklı olmalı.");
        co_return;
    }

    // Counter-clockwise from the first corner. The ring geometry closes it, so
    // the fourth vertex is the last one written (R11).
    const std::vector<core::Point2> corners{
        core::Point2{first->x, first->y},
        core::Point2{second->x, first->y},
        core::Point2{second->x, second->y},
        core::Point2{first->x, second->y},
    };

    std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{corners, core::RingRole::Exterior, 0}};

    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // THE TWO CORNERS ARE RECORDED, not the four that were derived from them.
    // A journal line has to replay to the same document, and replaying the two
    // corners through this command derives the same four — while recording four
    // would let a later edit move one of them and leave a "rectangle" that is
    // not one (Article 1.4).
    ctx.record("noktalar", Value::points(Value::Points{*first, *second}));
}

} // namespace

KENTOS_COMMAND(rectangle)
{
    return CommandSpec{
        .id       = "core.rectangle",
        .names    = {"DİKDÖRTGEN", "DIKDORTGEN", "RECTANGLE", "DKD", "REC"},
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::exactly(2),
                              "Karşılıklı iki köşe; kalan ikisi bunlardan türetilir"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
