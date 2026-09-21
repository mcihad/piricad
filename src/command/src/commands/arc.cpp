// SPDX-License-Identifier: GPL-3.0-or-later
// core.arc_draw — YAY. An arc from its centre and its two ends.
//
// The curve a road bend, a junction turn or a watercourse is actually made of.
// Like the circle it is stored as its DEFINITION — centre, exact radius, and the
// two measured ends — and the many-sided run is only what gets drawn
// (core/arc.hpp).
//
// THE SWEEP IS ALWAYS COUNTER-CLOCKWISE from the first end to the second, so the
// two ends given the other way round are the other arc of the same circle. That
// is the whole direction control: no flag, no "major arc" option, and no two
// records that could mean one picture.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::command {
namespace {

/// The radius `centre`->`p` implies, rounded to the millimetre the record stores.
core::Mm radius_between(core::Point2 centre, core::Point2 p)
{
    // Metres before squaring: the square of a TM3 coordinate difference in
    // millimetres leaves the 53-bit mantissa long before it leaves int64
    // (core.md R3). `double` is transient and never stored.
    const double dx = core::mm_to_metres(p.x - centre.x);
    const double dy = core::mm_to_metres(p.y - centre.y);
    return core::mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(core::kMmPerMetre));
}

Task<void> run(Context& ctx)
{
    // FOUR WAYS TO FIX ONE ARC, because an arc arrives in a drawing four ways
    // (TODOS-CAD P2-2):
    //   merkez — the centre, then the two ends. The daily one, and the default.
    //   3n     — three points ON the arc: the start, a point it passes through,
    //            and the end. How a measured curve is recovered, and the same
    //            `core::circumcircle` the circle command's own three-point
    //            method uses, so a circle and the arc cut from it agree
    //            (CLAUDE.md 5.10).
    //   bma    — start, centre, and a swept ANGLE: how a highway curve is given.
    //   bby    — start, end, and a RADIUS: how a fillet is given.
    std::string how = "merkez";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };

    if (is("3n")) {
        auto a = co_await ctx.point("baslangic", "Yayın başlangıç noktası");
        if (!a) co_return;
        auto b = co_await ctx.point("uzerinden", "Yayın üzerinden geçtiği nokta",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *a,
                                                 .rubber_shape  = RubberShape::Line});
        if (!b) co_return;
        auto c = co_await ctx.point("bitis", "Yayın bitiş noktası",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *b,
                                                 .rubber_shape  = RubberShape::Line});
        if (!c) co_return;

        core::Point2 middle{};
        core::Mm r = 0;
        if (!core::circumcircle(*a, *b, *c, middle, r)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Üç nokta aynı doğru üzerinde; onlardan geçen bir yay yok. Orta noktayı "
                          "yayın kavisinden seçin."));
            co_return;
        }

        // WHICH WAY ROUND. An arc is stored counter-clockwise from start to end
        // (core/arc.hpp), and the three points say which of the two arcs between
        // the ends was meant: the one the middle point is on. The cross product
        // of start→middle and start→end is the test, in metres so the product
        // stays inside the mantissa.
        const double sx = core::mm_to_metres(b->x - a->x);
        const double sy = core::mm_to_metres(b->y - a->y);
        const double ex = core::mm_to_metres(c->x - a->x);
        const double ey = core::mm_to_metres(c->y - a->y);
        const bool ccw  = (sx * ey - sy * ex) > 0.0;

        auto made =
            ctx.transaction().add_arc(ctx.active_layer(), middle, r, ccw ? *a : *c, ccw ? *c : *a);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ctx.record("yontem", Value::text(how));
        ctx.record("baslangic", Value::point(*a));
        ctx.record("uzerinden", Value::point(*b));
        ctx.record("bitis", Value::point(*c));
        co_return;
    }

    if (is("bby")) {
        // START, END AND A RADIUS: the fillet form. Two arcs of that radius join
        // two points — one bulging left of start→end and one right — so the user
        // says which by pointing at the side the curve goes.
        auto a = co_await ctx.point("baslangic", "Yayın başlangıç noktası");
        if (!a) co_return;
        auto c = co_await ctx.point("bitis", "Yayın bitiş noktası",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *a,
                                                 .rubber_shape  = RubberShape::Line});
        if (!c) co_return;
        auto wanted = co_await ctx.number("yaricap", "Yarıçap (m)");
        if (!wanted) co_return;

        const core::Mm r = core::mm_from_metres(*wanted);
        core::Point2 left{};
        core::Point2 right{};
        const core::CircleMeet meet = core::circle_intersection(*a, r, *c, r, left, right);
        if (meet != core::CircleMeet::Two && meet != core::CircleMeet::Tangent) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Bu yarıçap iki noktayı birleştirmiyor: yarıçap, iki nokta arasının "
                          "yarısından küçük olamaz."));
            co_return;
        }

        std::string side = "sol";
        if (const Value v = ctx.argument("yon"); !v.empty()) side = v.as_text();
        const bool to_right       = core::turkish_key_equals(side, "sag");
        const core::Point2 middle = to_right ? right : left;

        // The centre on the LEFT of start→end bulges the arc to the RIGHT, and
        // the arc is stored counter-clockwise, so the ends swap with the side.
        auto made = ctx.transaction().add_arc(ctx.active_layer(), middle, r, to_right ? *c : *a,
                                              to_right ? *a : *c);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ctx.record("yontem", Value::text(how));
        ctx.record("baslangic", Value::point(*a));
        ctx.record("bitis", Value::point(*c));
        ctx.record("yaricap", Value::number(*wanted));
        ctx.record("yon", Value::text(side));
        co_return;
    }

    auto centre = co_await ctx.point("merkez", "Yayın merkezi");
    if (!centre) co_return; // ESC before anything was drawn

    // The first end fixes the radius, so the guide is the whole circle it lies on:
    // the user is choosing a radius here, and a radius is a circle.
    auto start = co_await ctx.point("baslangic", "Yayın başlangıç noktası",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Circle});
    if (!start) co_return;

    const core::Mm radius = radius_between(*centre, *start);
    if (radius <= 0) {
        ctx.echo("Başlangıç noktası merkezle aynı yerde; yarıçap sıfır olamaz.");
        co_return;
    }

    // BMA: START, CENTRE AND A SWEPT ANGLE, which is how a highway curve is
    // given on a plan. The end is computed from the sweep rather than pointed
    // at, because a sweep of 47,5000 grad is not a point anybody can click.
    if (is("bma")) {
        auto sweep = co_await ctx.number("supurme", "Süpürme açısı");
        if (!sweep) co_return;

        const core::AngleConvention convention = ctx.session().bus().angle_convention();
        const double from_turns = core::direction_turns(*centre, *start, convention.rule);

        // SIGNED, AND NOT FOLDED INTO ONE TURN. `turns_from_udeg` folds into
        // [0, 1), which is right for a DIRECTION and wrong for a SWEEP: a
        // highway curve of −100 grad turns the other way, and folded it would
        // turn the same way by 300.
        const double by_turns =
            static_cast<double>(core::udeg_from_angle(*sweep, convention.unit)) /
            static_cast<double>(core::kUDegFullCircle);
        if (by_turns == 0.0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Süpürme açısı sıfır olamaz; yay bir noktaya iner."));
            co_return;
        }

        // THE SWEEP IS IN THE SESSION'S SENSE; THE ARC IS STORED IN THE MODEL'S.
        // A positive sweep means clockwise under semt — which is what a surveyor
        // means by one — and counter-clockwise under matematik. An arc is stored
        // counter-clockwise from start to end (core/arc.hpp), so a clockwise
        // sweep is the same arc with its ends the other way round. Getting this
        // wrong does not draw a slightly different arc: it draws the other
        // three quarters of the circle.
        const double to_turns       = from_turns + by_turns;
        const core::Point2 computed = *centre + core::polar_offset_turns(core::mm_to_metres(radius),
                                                                         to_turns, convention.rule);

        const bool ccw_in_drawing =
            convention.rule == core::AngleRule::Matematik ? by_turns > 0.0 : by_turns < 0.0;
        auto made = ctx.transaction().add_arc(ctx.active_layer(), *centre, radius,
                                              ccw_in_drawing ? *start : computed,
                                              ccw_in_drawing ? computed : *start);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ctx.record("yontem", Value::text(how));
        ctx.record("merkez", Value::point(*centre));
        ctx.record("baslangic", Value::point(*start));
        ctx.record("supurme", Value::number(*sweep));
        co_return;
    }

    // The second end fixes the sweep, and the guide is the ARC that sweep makes.
    auto end = co_await ctx.point("bitis", "Yayın bitiş noktası (saat yönünün tersine)",
                                  PointOptions{.rubber_band   = true,
                                               .rubber_origin = *centre,
                                               .rubber_shape  = RubberShape::Arc,
                                               .rubber_chain  = {*start}});
    if (!end) co_return;

    if (end->x == centre->x && end->y == centre->y) {
        ctx.echo("Bitiş noktası merkezle aynı yerde; yayın nereye kadar gideceği belirsiz.");
        co_return;
    }

    auto created = ctx.transaction().add_arc(ctx.active_layer(), *centre, radius, *start, *end);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // RECORDED AS IT WAS ASKED. The radius is what the centre and the first end
    // MEAN; recording the derived number instead would let a replay disagree with
    // the run that produced it.
    ctx.record("merkez", Value::point(*centre));
    ctx.record("baslangic", Value::point(*start));
    ctx.record("bitis", Value::point(*end));
}

} // namespace

KENTOS_COMMAND(arc_draw)
{
    return CommandSpec{
        .id       = "core.arc_draw",
        .names    = {"YAY", "ARC", "YY"},
        .title    = "Yay",
        .category = Category::Draw,
        .params =
            {
                // THE DEFAULT METHOD'S THREE POINTS COME FIRST: positional
                // arguments bind in declaration order, and `YAY 0,0 50,0 0,50`
                // is the form every page, script and journal already uses.
                Param::points("merkez", Arity::optional(), "Yayın merkezi"),
                Param::points("baslangic", Arity::optional(),
                              "Yayın başlangıç noktası; merkez yönteminde yarıçapı bu belirler"),
                Param::points("bitis", Arity::optional(),
                              "Yayın bitiş noktası; süpürme saat yönünün tersinedir"),
                Param::choice("yontem", Arity::optional(), {"merkez", "3n", "bma", "bby"},
                              "merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: "
                              "başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve "
                              "yarıçap"),
                Param::points("uzerinden", Arity::optional(), "3n: yayın üzerinden geçtiği nokta"),
                Param::number("supurme", Arity::optional(), "bma: süpürme açısı"),
                Param::number("yaricap", Arity::optional(), "bby: yarıçap (m)").measured_in("m"),
                Param::choice("yon", Arity::optional(), {"sol", "sag"},
                              "bby: yayın hangi tarafa kavis yaptığı; başlangıç→bitiş yönüne göre"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da "
                   "başlangıç+bitiş+yarıçapla yay çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
