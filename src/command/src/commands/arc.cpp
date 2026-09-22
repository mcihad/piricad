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
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

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
        // THE ARC, NOT A LINE. Two points on a curve determine nothing, so the
        // second is asked for with a line; the THIRD settles it, and from there
        // the guide is the arc itself — built by `core::arc_from_guide`, which is
        // the construction this branch commits (CLAUDE.md 5.10).
        const std::vector<core::Point2> so_far{*a, *b};
        const core::ArcGuide guide{.build = core::ArcBuild::ThreePoint};
        auto c = co_await ctx.point("bitis", "Yayın bitiş noktası",
                                    PointOptions{.rubber_band    = true,
                                                 .rubber_origin  = *b,
                                                 .rubber_shape   = RubberShape::ArcBuild,
                                                 .rubber_chain   = so_far,
                                                 .rubber_payload = core::encode_arc_guide(guide)});
        if (!c) co_return;

        core::Point2 middle{};
        core::Mm r = 0;
        core::Point2 from_end{};
        core::Point2 to_end{};
        if (!core::arc_from_guide(guide, so_far, *c, middle, r, from_end, to_end)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Üç nokta aynı doğru üzerinde; onlardan geçen bir yay yok. Orta noktayı "
                          "yayın kavisinden seçin."));
            co_return;
        }

        auto made = ctx.transaction().add_arc(ctx.active_layer(), middle, r, from_end, to_end);
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

    if (is("devam")) {
        // TANGENT CONTINUATION, which is how a road transition, a kerb return and
        // a fillet chain are actually drawn: the arc leaves the last thing drawn
        // in the SAME DIRECTION it ended in, so the join has no kink in it.
        //
        // THE SOURCE IS THE DRAWING, not a remembered session field. The deferral
        // note said this needed "`Session`'a son segment yönü" — it does not:
        // `SEÇ SON` already answers "the most recently created live entity" from
        // the document, and reading the direction off that entity is the same
        // question asked of the same place. One less piece of session state to
        // keep in step with undo, a replay and a reload.
        const core::Document& doc         = ctx.document();
        const core::EntityTable& entities = doc.entities();
        core::Point2 from{};
        double tx  = 0.0;
        double ty  = 0.0;
        bool found = false;
        for (auto e = static_cast<core::EntityId>(entities.size()); e-- > 0;) {
            if (!doc.alive(e)) continue;
            const core::KindId kind   = entities.kind[e];
            const std::uint32_t gslot = entities.slot[e];

            if (kind == core::kPolylineKind) {
                const core::RingSpan span = doc.geometry().rings_of(gslot);
                if (span.count == 0) continue;
                const auto xs = doc.geometry().ring_xs(span.first);
                const auto ys = doc.geometry().ring_ys(span.first);
                if (xs.size() < 2) continue;
                from  = core::Point2{xs.back(), ys.back()};
                tx    = static_cast<double>(xs.back() - xs[xs.size() - 2]);
                ty    = static_cast<double>(ys.back() - ys[ys.size() - 2]);
                found = true;
                break;
            }
            if (kind == core::kArcKind) {
                // AN ARC'S END TANGENT is perpendicular to its end radius, and
                // which of the two perpendiculars it is follows the stored sweep:
                // the model keeps an arc counter-clockwise (arc.hpp), so at the
                // end the motion is the radius turned a quarter turn the same way.
                const core::Point2 centre = core::arc_centre_of(doc.geometry(), gslot);
                const core::Point2 end    = core::arc_end_of(doc.geometry(), gslot);
                from                      = end;
                tx                        = -static_cast<double>(end.y - centre.y);
                ty                        = static_cast<double>(end.x - centre.x);
                found                     = true;
                break;
            }
        }
        if (!found) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Devam edilecek bir çizgi ya da yay yok. Önce bir "
                                         "çizgi veya yay çizin, sonra YAY yontem=devam yazın."));
            co_return;
        }

        const double t_len = std::sqrt(tx * tx + ty * ty);
        if (t_len <= 0.0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Son çizginin son kenarı sıfır uzunlukta; teğet "
                                         "doğrultusu yok."));
            co_return;
        }
        tx /= t_len;
        ty /= t_len;

        // THE TANGENT ARC, previewed. This end used to be asked for with a
        // straight line, which is the one shape the answer is NOT: the whole
        // point of the method is that the curve leaves the last thing drawn
        // without a kink, and the user could not see whether it did until after
        // the click.
        //
        // The direction travels as a POINT a kilometre along the tangent, so the
        // guide and this branch read the same direction from the same two points
        // rather than each rounding its own (`core::arc_from_guide`).
        const std::vector<core::Point2> ray{
            from, core::Point2{from.x + core::mm_round(tx * 1'000'000.0),
                               from.y + core::mm_round(ty * 1'000'000.0)}};
        const core::ArcGuide guide{.build = core::ArcBuild::Tangent};
        auto c = co_await ctx.point("bitis", "Yayın bitiş noktası (teğet devam)",
                                    PointOptions{.rubber_band    = true,
                                                 .rubber_origin  = from,
                                                 .rubber_shape   = RubberShape::ArcBuild,
                                                 .rubber_chain   = ray,
                                                 .rubber_payload = core::encode_arc_guide(guide)});
        if (!c) co_return;

        core::Point2 centre{};
        core::Mm radius = 0;
        core::Point2 first{};
        core::Point2 last{};
        if (!core::arc_from_guide(guide, ray, *c, centre, radius, first, last)) {
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "Bitiş noktası teğetin üzerinde: buradan devam eden şey bir yay değil bir "
                "doğrudur. ÇİZGİ kullanın ya da yanda bir nokta seçin."));
            co_return;
        }

        auto made = ctx.transaction().add_arc(ctx.active_layer(), centre, radius, first, last);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ctx.record("yontem", Value::text(how));
        // THE RESOLVED ARC, not the drawing it was read from: a replay must build
        // this arc and not whatever the newest entity happens to be in the
        // document it is replayed into (model.md P4). Recorded as the `merkez`
        // form, which is what the default branch reads back.
        ctx.record("merkez", Value::point(centre));
        ctx.record("baslangic", Value::point(first));
        ctx.record("bitis", Value::point(last));
        co_return;
    }

    if (is("bby")) {
        // START, END AND A RADIUS: the fillet form. Two arcs of that radius join
        // two points — one bulging left of start→end and one right — so the user
        // says which by pointing at the side the curve goes.
        auto a = co_await ctx.point("baslangic", "Yayın başlangıç noktası");
        if (!a) co_return;
        // WHETHER THE SIDE CAME WITH THE INVOCATION, read before anything is
        // awaited — a branch on whether a value was given, not on which client
        // gave it (command.md P10). A run that was handed `yon` asks nothing
        // more, which is what keeps every journal line written before this
        // change replayable (Article 1.4).
        const bool side_given = ctx.has_argument("yon");

        auto c = co_await ctx.point("bitis", "Yayın bitiş noktası",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *a,
                                                 .rubber_shape  = RubberShape::Line});
        if (!c) co_return;
        auto wanted = co_await ctx.number("yaricap", "Yarıçap (m)");
        if (!wanted) co_return;

        const core::Mm r = core::mm_from_metres(*wanted);
        const std::vector<core::Point2> ends{*a, *c};
        const core::ArcGuide guide{.build = core::ArcBuild::Radius, .radius = r};

        // THE RADIUS IS JUDGED THE MOMENT IT IS TYPED, before the side is asked
        // for: a radius shorter than half the span joins nothing, and asking
        // which side of an arc that cannot exist would be a question with no
        // answer.
        core::Point2 middle{};
        core::Mm made_radius = 0;
        core::Point2 first{};
        core::Point2 last{};
        if (!core::arc_by_radius(*a, *c, r, false, middle, made_radius, first, last)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Bu yarıçap iki noktayı birleştirmiyor: yarıçap, iki nokta arasının "
                          "yarısından küçük olamaz."));
            co_return;
        }

        std::string side = "sol";
        bool to_right    = false;
        if (side_given) {
            side     = ctx.argument("yon").as_text();
            to_right = core::turkish_key_equals(side, "sag");
        } else {
            // TWO ARCS OF THAT RADIUS JOIN TWO POINTS, one bulging each side of
            // the chord, and which one was meant is not in the numbers. The
            // command used to read `yon` from its arguments and default to
            // `sol`, so from the interface it ALWAYS drew one of them and the
            // other was unreachable by mouse — although the note above it said
            // the user points at the side. Now they do, with the arc following
            // the cursor from one side of the chord to the other.
            auto pointed =
                co_await ctx.point("yon_nokta", "Yayın hangi yandan geçeceğini gösterin",
                                   PointOptions{.rubber_band    = true,
                                                .rubber_origin  = *a,
                                                .rubber_shape   = RubberShape::ArcBuild,
                                                .rubber_chain   = ends,
                                                .rubber_payload = core::encode_arc_guide(guide)});
            if (!pointed) co_return;

            to_right = core::arc_radius_side(*a, *c, *pointed);
            side     = to_right ? "sag" : "sol";

            // NOT PART OF THE RECORD: the gesture is HOW the side was chosen and
            // `yon` is what the side IS. A line carrying both would have two
            // answers to one question (Article 1.4).
            ctx.record("yon_nokta", Value{});
        }

        if (!core::arc_by_radius(*a, *c, r, to_right, middle, made_radius, first, last)) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Bu yarıçapta bir yay kurulamadı."));
            co_return;
        }

        auto made = ctx.transaction().add_arc(ctx.active_layer(), middle, made_radius, first, last);
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
                Param::choice("yontem", Arity::optional(), {"merkez", "3n", "bma", "bby", "devam"},
                              "merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: "
                              "başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve "
                              "yarıçap"),
                Param::points("uzerinden", Arity::optional(), "3n: yayın üzerinden geçtiği nokta"),
                Param::number("supurme", Arity::optional(), "bma: süpürme açısı"),
                Param::number("yaricap", Arity::optional(), "bby: yarıçap (m)").measured_in("m"),
                Param::points("yon_nokta", Arity::optional(),
                              "bby: yayın hangi yandan geçeceği gösterilen nokta; yon "
                              "verilmişse sorulmaz"),
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
