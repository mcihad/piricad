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
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/text.hpp"

#include <array>

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    // THREE POINTS INSTEAD OF TWO, when the building is not square to the grid.
    // A cadastral sheet is full of them: a block along a road that does not run
    // east–west, a wall on a skewed boundary. `yontem=3n` gives one EDGE and
    // then a height, which is how such a shape is actually measured — two
    // corners of the wall and the depth off it (TODOS-CAD P2-4).
    std::string how = "2n";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const bool by_edge = core::turkish_key_equals(how, "3n");

    auto first = co_await ctx.point("noktalar",
                                    by_edge ? "Bir kenarın ilk köşesi" : "Dikdörtgenin ilk köşesi");
    if (!first) co_return; // ESC before anything was drawn

    // The rubber band starts at the first corner, so the diagonal lock — polar,
    // which at half a right angle draws a square — and every object snap measure
    // from it exactly as they do for a line. Dik mod does NOT: locked to an axis
    // the opposite corner makes a rectangle with no width or no height, so the
    // aids leave it out of every rectangle's corner (`command::aids_for`).
    // Nothing here is a private input path (kentoscad.md §2.4).
    auto second = co_await ctx.point(
        "noktalar", by_edge ? "Aynı kenarın öteki köşesi" : "Karşı köşe",
        PointOptions{.rubber_band   = true,
                     .rubber_origin = *first,
                     .rubber_shape  = by_edge ? RubberShape::Line : RubberShape::Rectangle});
    if (!second) co_return;

    std::vector<core::Point2> corners;

    if (by_edge) {
        if (*first == *second) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Kenarın iki köşesi aynı nokta; bir kenar tanımlamıyor."));
            co_return;
        }
        // THE GUIDE SHOWS THE RECTANGLE, and this is a fix a user's report
        // forced: it used to ask for this point under a `Ring` preview with no
        // chain, which is an origin and a cursor — two points, and two points
        // draw nothing. Pressing "rotated rectangle" therefore gave a tool that
        // drew correctly and showed nothing on the way, which from the chair is
        // a tool that does not work. `EdgeRectangle` is handed the edge and
        // draws the four corners this command is about to make, from the very
        // function that makes them (core/polygon.hpp).
        auto across = co_await ctx.point("noktalar", "Karşı kenarın geçtiği nokta",
                                         PointOptions{.rubber_band   = true,
                                                      .rubber_origin = *second,
                                                      .rubber_shape  = RubberShape::EdgeRectangle,
                                                      .rubber_chain  = {*first, *second}});
        if (!across) co_return;

        // THE THIRD POINT GIVES THE DEPTH, not a corner: it is projected onto
        // the edge's own normal, so a hand that is a few millimetres off still
        // gets a rectangle rather than a parallelogram.
        std::array<core::Point2, 4> four{};
        if (!core::edge_rectangle_corners(*first, *second, *across, four)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Üçüncü nokta kenarın üzerinde; dikdörtgenin yüksekliği sıfır olamaz."));
            co_return;
        }

        corners.assign(four.begin(), four.end());
        ctx.record("yontem", Value::text(how));
    } else {
        if (first->x == second->x || first->y == second->y) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Bu iki köşe bir alan kapatmaz: karşı köşenin hem doğusu hem kuzeyi "
                       "ilkinden farklı olmalı.");
            co_return;
        }

        // Counter-clockwise from the first corner. The ring geometry closes it,
        // so the fourth vertex is the last one written (R11).
        corners = {
            core::Point2{first->x, first->y},
            core::Point2{second->x, first->y},
            core::Point2{second->x, second->y},
            core::Point2{first->x, second->y},
        };
    }

    std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{corners, core::RingRole::Exterior, 0}};

    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.refuse(created.error());
        co_return; // the bus rolls the transaction back
    }

    // THE TWO CORNERS ARE RECORDED, not the four that were derived from them.
    // A journal line has to replay to the same document, and replaying the two
    // corners through this command derives the same four — while recording four
    // would let a later edit move one of them and leave a "rectangle" that is
    // not one (Article 1.4).
    if (by_edge)
        ctx.record("noktalar", Value::points(Value::Points{
                                   *first, *second, core::Point2{corners[3].x, corners[3].y}}));
    else
        ctx.record("noktalar", Value::points(Value::Points{*first, *second}));
}

} // namespace

KENTOS_COMMAND(rectangle)
{
    return CommandSpec{
        .id       = "core.rectangle",
        .names    = {"DİKDÖRTGEN", "DIKDORTGEN", "RECTANGLE", "DKD", "REC"},
        .title    = "Dikdörtgen",
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity{2, 3},
                              "2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı "
                              "kenarın geçtiği nokta")
                    .en("points"),
                Param::choice("yontem", Arity::optional(), {"2n", "3n"},
                              "2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve "
                              "yükseklik, döndürülmüş")
                    .en("method"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı "
                   "bir alan çizer.",
        .run = &run,
    };
}

} // namespace kentos::command
