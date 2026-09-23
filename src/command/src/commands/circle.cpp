// SPDX-License-Identifier: GPL-3.0-or-later
// core.circle_draw — DAİRE. A circle from its centre and a point on its rim.
//
// The first curve this program can draw, and the reason it is a KIND rather than
// a many-sided polygon: a circle stored as its picture has a circumference that
// is not 2·pi·r and an area that is not pi·r², and on a cadastral sheet those are
// the numbers that reach the tapu (§12). What is stored is the centre and the
// radius; the 128-gon is only what gets drawn (core/circle.hpp).
//
// The rim point is asked for rather than a typed radius because that is how a
// circle is drawn with a mouse, and because every input aid then applies to it
// for free: snapping the rim to a parsel corner puts the circle exactly through
// that corner, which is what a çekme mesafesi or a monument radius needs.
//
// FOUR WAYS TO FIX ONE CIRCLE, because a circle arrives in a drawing four ways
// (TODOS-CAD P2-1):
//   merkez — the centre and a rim point. The daily one, and the default.
//   2n     — the two ends of a DIAMETER: a doorway, a pipe, a bore.
//   3n     — three points on the RIM, which is how a measured curve is
//            recovered: a kerb, a roundabout, the sweep of a wall.
//   ttr    — tangent to two lines with a given radius: the fillet a road
//            junction is drawn with, and the one method whose answer is a
//            CHOICE rather than a computation.
// The construction for `3n` is `core::circumcircle`, shared with YAY's own three
// point method so a circle and the arc cut from it agree (CLAUDE.md 5.10).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The circle a construction makes, asked of `core` rather than worked out here.
///
/// EVERY ONE OF THE FOUR METHODS GOES THROUGH THIS, including `merkez`, which
/// used to carry its own distance helper. The reason is the guide: the canvas
/// previews three of these constructions and has to arrive at the same circle,
/// and a second copy of the arithmetic is a copy that eventually disagrees
/// (core/circle.hpp).
bool built(core::CircleBuild how, std::initializer_list<core::Point2> fixed, core::Point2 cursor,
           core::Point2& centre, core::Mm& radius, core::Mm given = 0)
{
    const std::vector<core::Point2> chain(fixed);
    return core::circle_from_guide(core::CircleGuide{.build = how, .radius = given}, chain, cursor,
                                   centre, radius);
}

Task<void> run(Context& ctx)
{
    std::string how = "merkez";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };

    core::Point2 centre{};
    core::Mm radius = 0;

    if (is("2n")) {
        // THE TWO ENDS OF A DIAMETER. The centre is their midpoint and the
        // radius is half their distance — and the radius is computed from the
        // FULL span rather than from the midpoint, because halving a rounded
        // half is a rounding twice over.
        auto first = co_await ctx.point("birinci", "Çapın bir ucu");
        if (!first) co_return;
        // THE CIRCLE, NOT ITS DIAMETER. Previewed as a line, this method showed
        // the user a rubber band and told them nothing about the circle it was
        // about to make — the same defect the rotated rectangle had. The guide
        // is built by `core::circle_from_guide`, which is the arithmetic below.
        auto second = co_await ctx.point(
            "ikinci", "Çapın öteki ucu",
            PointOptions{.rubber_band    = true,
                         .rubber_origin  = *first,
                         .rubber_shape   = RubberShape::CircleBuild,
                         .rubber_chain   = {*first},
                         .rubber_payload = core::encode_circle_guide(
                             core::CircleGuide{.build = core::CircleBuild::Diameter})});
        if (!second) co_return;

        if (!built(core::CircleBuild::Diameter, {*first}, *second, centre, radius)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Çapın iki ucu aynı nokta; yarıçap sıfır olamaz.");
            co_return;
        }
        ctx.record("birinci", Value::point(*first));
        ctx.record("ikinci", Value::point(*second));
    } else if (is("3n")) {
        // THREE POINTS ON THE RIM, which is how a measured curve is recovered.
        auto first = co_await ctx.point("birinci", "Çember üzerinde birinci nokta");
        if (!first) co_return;
        auto second = co_await ctx.point("ikinci", "Çember üzerinde ikinci nokta",
                                         PointOptions{.rubber_band   = true,
                                                      .rubber_origin = *first,
                                                      .rubber_shape  = RubberShape::Line});
        if (!second) co_return;
        // TWO POINTS DETERMINE NO CIRCLE, so the second is asked for with a
        // line; the THIRD one settles it, and from there the guide is the
        // circumcircle itself — drawn by `core::circumcircle`, which is what
        // this method computes below (CLAUDE.md 5.10).
        auto third = co_await ctx.point(
            "ucuncu", "Çember üzerinde üçüncü nokta",
            PointOptions{.rubber_band    = true,
                         .rubber_origin  = *second,
                         .rubber_shape   = RubberShape::CircleBuild,
                         .rubber_chain   = {*first, *second},
                         .rubber_payload = core::encode_circle_guide(
                             core::CircleGuide{.build = core::CircleBuild::ThreePoint})});
        if (!third) co_return;

        if (!core::circumcircle(*first, *second, *third, centre, radius)) {
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "Üç nokta aynı doğru üzerinde; onlardan geçen bir çember yok. Noktalardan "
                "birini çemberin başka bir yerinden seçin."));
            co_return;
        }
        ctx.record("birinci", Value::point(*first));
        ctx.record("ikinci", Value::point(*second));
        ctx.record("ucuncu", Value::point(*third));
    } else if (is("ttr")) {
        // TANGENT TO TWO LINES, WITH A GIVEN RADIUS: the fillet a road junction
        // is drawn with. There are FOUR circles of a given radius tangent to two
        // crossing lines, one in each quadrant the lines make, and which one is
        // wanted is not in the numbers — so the user points at the quadrant and
        // the nearest of the four is taken. A silent pick would put the fillet
        // on the wrong corner of the junction.
        auto a1 = co_await ctx.point("birinci", "Birinci doğrunun ilk noktası");
        if (!a1) co_return;
        auto a2 = co_await ctx.point("ikinci", "Birinci doğrunun ikinci noktası",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *a1,
                                                  .rubber_shape  = RubberShape::Line});
        if (!a2) co_return;
        auto b1 = co_await ctx.point("ucuncu", "İkinci doğrunun ilk noktası");
        if (!b1) co_return;
        auto b2 = co_await ctx.point("dorduncu", "İkinci doğrunun ikinci noktası",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *b1,
                                                  .rubber_shape  = RubberShape::Line});
        if (!b2) co_return;
        auto wanted = co_await ctx.number("yaricap", "Yarıçap (m)");
        if (!wanted) co_return;
        const core::Mm r = core::mm_from_metres(*wanted);
        if (r <= 0) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Yarıçap sıfır ya da eksi olamaz."));
            co_return;
        }

        // THE ONE PICK WHERE SEEING IT MATTERS MOST, and it had no guide at all.
        // Four circles of this radius are tangent to both lines, one in each
        // quadrant they make, and the user chooses by pointing at a corner: the
        // fillet follows the cursor from quadrant to quadrant so the junction is
        // right before the click, not after it.
        auto near = co_await ctx.point(
            "yon", "Dairenin geleceği köşeyi gösterin",
            PointOptions{.rubber_band    = true,
                         .rubber_origin  = *a1,
                         .rubber_shape   = RubberShape::CircleBuild,
                         .rubber_chain   = {*a1, *a2, *b1, *b2},
                         .rubber_payload = core::encode_circle_guide(
                             core::CircleGuide{.build = core::CircleBuild::Tangent, .radius = r})});
        if (!near) co_return;

        // THE FOUR CENTRES, chosen by the corner the user pointed at. The search
        // is `core::tangent_circle_centre`, shared with the guide above so the
        // circle that was previewed is the circle that is drawn.
        core::Point2 best{};
        if (!core::tangent_circle_centre(*a1, *a2, *b1, *b2, r, *near, best)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "İki doğru paralel; verilen yarıçapta ikisine de teğet bir daire yok."));
            co_return;
        }
        centre = best;
        radius = r;

        ctx.record("birinci", Value::point(*a1));
        ctx.record("ikinci", Value::point(*a2));
        ctx.record("ucuncu", Value::point(*b1));
        ctx.record("dorduncu", Value::point(*b2));
        ctx.record("yaricap", Value::number(*wanted));
        ctx.record("yon", Value::point(*near));
    } else {
        auto middle = co_await ctx.point("merkez", "Dairenin merkezi");
        if (!middle) co_return; // ESC before anything was drawn

        // THE CIRCLE ITSELF, not a radius line. A guide is the only thing that
        // tells a user what the next click will make before they make it
        // (input.hpp), and a circle previewed as a line says nothing about the
        // circle.
        auto rim = co_await ctx.point("cevre", "Çember üzerinde bir nokta",
                                      PointOptions{.rubber_band   = true,
                                                   .rubber_origin = *middle,
                                                   .rubber_shape  = RubberShape::Circle});
        if (!rim) co_return;

        if (!built(core::CircleBuild::Centre, {*middle}, *rim, centre, radius)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Çember noktası merkezle aynı yerde; yarıçap sıfır olamaz.");
            co_return;
        }

        // RECORDED AS IT WAS ASKED, so a replay draws the same circle: the rim
        // point and not the radius, because the radius is what the two points
        // MEAN and recording a derived number would let a replay disagree with
        // the run.
        ctx.record("merkez", Value::point(*middle));
        ctx.record("cevre", Value::point(*rim));
    }

    auto created = ctx.transaction().add_circle(ctx.active_layer(), centre, radius);
    if (!created) {
        ctx.refuse(created.error());
        co_return; // the bus rolls the transaction back
    }
    if (!is("merkez")) ctx.record("yontem", Value::text(how));
}

} // namespace

KENTOS_COMMAND(circle_draw)
{
    return CommandSpec{
        .id       = "core.circle_draw",
        .names    = {"DAİRE", "DAIRE", "CIRCLE", "DR"},
        .title    = "Daire",
        .category = Category::Draw,
        .params =
            {
                // THE DEFAULT METHOD'S TWO POINTS COME FIRST, and that is not
                // cosmetic: positional arguments bind in declaration order, so
                // a `yontem` declared ahead of them would swallow the first
                // coordinate of `DAİRE 50,50 100,50` — the form every page,
                // script and journal already uses. The other methods name their
                // points, which is what a method with four of them wants anyway.
                Param::points("merkez", Arity::optional(), "Dairenin merkezi").en("center"),
                Param::points("cevre", Arity::optional(),
                              "Çember üzerinde bir nokta; yarıçapı bu belirler")
                    .en("rim"),
                Param::choice("yontem", Arity::optional(), {"merkez", "2n", "3n", "ttr"},
                              "merkez: merkez + çevre · 2n: çapın iki ucu · 3n: çember üzerinde "
                              "üç nokta · ttr: iki doğruya teğet, verilen yarıçapla")
                    .en("method"),
                Param::points("birinci", Arity::optional(),
                              "2n: çapın bir ucu · 3n: birinci nokta · ttr: birinci doğrunun ilk "
                              "noktası")
                    .en("first"),
                Param::points("ikinci", Arity::optional(), "İkinci nokta").en("second"),
                Param::points("ucuncu", Arity::optional(),
                              "3n: üçüncü nokta · ttr: ikinci "
                              "doğrunun ilk noktası")
                    .en("third"),
                Param::points("dorduncu", Arity::optional(), "ttr: ikinci doğrunun ikinci noktası")
                    .en("fourth"),
                Param::number("yaricap", Arity::optional(), "ttr: teğet dairenin yarıçapı (m)")
                    .measured_in("m")
                    .en("radius"),
                Param::points("yon", Arity::optional(),
                              "ttr: dairenin geleceği köşe; dört çözümden en yakını alınır")
                    .en("side"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya "
                   "teğet yarıçapla daire çizer.",
        .run = &run,
    };
}

} // namespace kentos::command
