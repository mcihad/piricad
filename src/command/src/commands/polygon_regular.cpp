// SPDX-License-Identifier: GPL-3.0-or-later
// core.polygon_regular — ÇOKGEN. A regular polygon.
//
// A ÇOKGEN, not a POLİGON: in Turkish surveying a poligon is a traverse and a
// çokgen is the shape — the word this command wanted was taken by the wrong
// thing for a while (see `geodesy.traverse`).
//
// Three ways to fix one, because a regular polygon is specified three ways on a
// plan sheet:
//   ic    — inscribed: the vertices sit ON a circle of the given radius. A bolt
//           circle, a pylon footing, a manhole ring.
//   dis   — circumscribed: the EDGES touch a circle of the given radius. A nut,
//           a kerbstone chamfer, anything sized by the flat.
//   kenar — by the side length, which is how a paving pattern is drawn.
//
// THE ARITHMETIC IS `core::polygon`'S, NOT THIS FILE'S, and that is the fix a
// user's report forced. The canvas has to draw the guide that promises what this
// command will make, and a guide computed a second way is a guide that is
// eventually wrong. Both callers now ask `core::regular_polygon_corners` for the
// same corners (core/polygon.hpp).
//
// THE ORDER OF THE QUESTIONS IS PART OF THE FIX. It used to be centre → side
// count → size, so pressing the tool put the crosshair on a canvas that showed
// nothing, and the first click was answered by a line of text at the bottom of
// the window asking for a number. The user's words were "nothing happens, and
// there is nowhere to enter the side count". The side count is now asked FIRST —
// before anything is on screen to look at, when the question is the only thing
// happening — and by the time the centre is down the polygon itself follows the
// cursor.
//
// AND THE SIZE CAN BE POINTED AT. `yaricap=` still works and still means what it
// meant; what is new is that a hand with no number in it can point at where a
// corner goes, which sets the radius AND the rotation in one click, previewed.
// A run that was given its size as an ARGUMENT asks nothing more and keeps the
// old behaviour exactly — which is what makes every journal line written before
// this change replay to the same document (Article 1.4).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    std::string how = "ic";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };
    const core::PolygonFit fit = is("kenar") ? core::PolygonFit::Side
                                 : is("dis") ? core::PolygonFit::Circumscribed
                                             : core::PolygonFit::Inscribed;
    const char* size_param     = fit == core::PolygonFit::Side ? "kenar_uzunlugu" : "yaricap";

    // WHETHER THE SIZE CAME WITH THE INVOCATION, read BEFORE anything is
    // awaited. This is not a branch on which client is asking (command.md P10):
    // it is a branch on whether a value was given, which is the same question
    // for a script, a journal replay, a typed line and the AI. A run that was
    // handed its size is complete as soon as it has a centre, and asking it to
    // point at a rotation it never asked about would make every line written
    // before this change unreplayable.
    const bool size_given = ctx.has_argument(size_param);

    // 1. HOW MANY SIDES, first of all. See the note at the top of the file.
    auto sides = co_await ctx.integer("kenar_sayisi", "Kenar sayısı (3–1024)");
    if (!sides) co_return;
    if (*sides < core::kPolygonMinSides || *sides > core::kPolygonMaxSides) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Kenar sayısı 3 ile 1024 arasında olmalı; " +
                                         std::to_string(*sides) + " geldi."));
        co_return;
    }

    // 2. WHERE ITS CENTRE IS.
    auto centre = co_await ctx.point("merkez", "Çokgenin merkezi");
    if (!centre) co_return;

    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // 3. HOW BIG, AND WHICH WAY ROUND.
    double measured = 0.0;
    if (size_given) {
        measured = ctx.argument(size_param).as_number();
    } else if (fit == core::PolygonFit::Side) {
        // A SIDE LENGTH CANNOT BE POINTED AT from the centre: the distance to
        // the cursor is a radius, and calling it a side length would be a lie
        // the preview would then draw. So it is typed, and the cursor is left
        // the rotation — which it CAN give, and which is previewed.
        auto typed = co_await ctx.number("kenar_uzunlugu", "Kenar uzunluğu (m)");
        if (!typed) co_return;
        measured = *typed;
    }
    if (size_given || fit == core::PolygonFit::Side) {
        if (!(measured > 0.0)) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Yarıçap ya da kenar uzunluğu sıfır ya da eksi olamaz."));
            co_return;
        }
    }

    double start_turns      = 0.0;
    bool angle_from_hand    = false;
    double pointed_angle    = 0.0;
    const Value given_angle = ctx.argument("aci");
    if (!given_angle.empty())
        start_turns =
            core::turns_from_udeg(core::udeg_from_angle(given_angle.as_number(), convention.unit));

    std::vector<core::Point2> corners;

    // The one pointed answer. It is asked when the size still has to come from
    // somewhere, and — under `kenar`, whose size is already typed — when the
    // rotation does. A run whose size AND angle both arrived as arguments asks
    // nothing, which is the scripted and the replayed path.
    const bool want_point = !size_given && (fit != core::PolygonFit::Side || given_angle.empty());
    if (want_point) {
        // ONE ANSWER FOR THE GUIDE AND THE CLICK: `core::polygon_from_guide`
        // draws the ghost under the cursor and makes the corners below, from
        // the numbers this run records. Under `kenar` the size is settled and
        // the cursor only turns the shape; with `aci` given the rotation is
        // settled and the cursor only sizes it — which the guide used to ignore,
        // turning the ghost towards the cursor while the click drew it at `aci`.
        core::PolygonGuide guide{.sides    = *sides,
                                 .fit      = fit,
                                 .measured = fit == core::PolygonFit::Side ? measured : 0.0};
        if (!given_angle.empty()) {
            guide.angle_given = true;
            guide.angle_udeg  = core::udeg_from_angle(given_angle.as_number(), convention.unit);
        }

        const char* asked = fit == core::PolygonFit::Side
                                ? "Çokgenin yönü: bir köşenin geçtiği nokta"
                            : fit == core::PolygonFit::Circumscribed
                                ? "Bir kenarın geçtiği nokta (iç yarıçap ve yön)"
                                : "Bir köşenin geçtiği nokta (yarıçap ve yön)";

        auto at =
            co_await ctx.point("kose", asked,
                               PointOptions{.rubber_band    = true,
                                            .rubber_origin  = *centre,
                                            .rubber_shape   = RubberShape::Polygon,
                                            .rubber_payload = core::encode_polygon_guide(guide)});
        if (!at) co_return;

        core::PolygonPick pick;
        if (!core::polygon_from_guide(guide, *centre, *at, convention, pick)) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Nokta merkezle aynı yerde; çokgenin boyu sıfır olamaz."));
            co_return;
        }
        measured        = pick.measured;
        corners         = std::move(pick.corners);
        angle_from_hand = pick.angle_pointed;
        pointed_angle   = pick.angle;

        // NOT PART OF THE RECORD. The awaiter writes whatever it resolved under
        // the name it was given, and this point is a GESTURE, not an input the
        // shape is defined by: what defines it is the size and the angle derived
        // just above, both recorded below. A line carrying both the point and
        // the numbers would have two answers to one question (Article 1.4).
        ctx.record("kose", Value{});
    } else {
        const double circumradius = core::polygon_circumradius(measured, *sides, fit);
        if (!(circumradius > 0.0)) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Yarıçap ya da kenar uzunluğu sıfır ya da eksi olamaz."));
            co_return;
        }
        corners = core::regular_polygon_corners(*centre, *sides, circumradius, start_turns,
                                                convention.rule);
    }

    std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{corners, core::RingRole::Exterior, 0}};
    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.session().fail(created.error());
        co_return;
    }

    // WHAT WAS ASKED, not the corners it derived. Replaying the inputs through
    // this command gives the same ring, and recording the corners would let a
    // later edit move one and leave a "regular polygon" that is not one
    // (Article 1.4).
    ctx.record("merkez", Value::point(*centre));
    ctx.record("kenar_sayisi", Value::integer(*sides));
    ctx.record(size_param, Value::number(measured));
    if (!is("ic")) ctx.record("yontem", Value::text(how));
    if (angle_from_hand) ctx.record("aci", Value::number(pointed_angle));

    ctx.echo(std::to_string(*sides) + " kenarlı çokgen çizildi.");
}

} // namespace

KENTOS_COMMAND(polygon_regular)
{
    return CommandSpec{
        .id       = "core.polygon_regular",
        .names    = {"ÇOKGEN", "COKGEN", "POLYGONREG", "ÇKG", "CKG"},
        .title    = "Düzgün Çokgen",
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Çokgenin merkezi").en("center"),
                Param::integer_range("kenar_sayisi", Arity::exactly(1), 3, 1024, "Kenar sayısı")
                    .en("sides"),
                Param::choice("yontem", Arity::optional(), {"ic", "dis", "kenar"},
                              "ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · "
                              "kenar: kenar uzunluğundan")
                    .en("method"),
                Param::number("yaricap", Arity::optional(), "ic/dis yönteminin yarıçapı (m)")
                    .measured_in("m")
                    .en("radius"),
                Param::number("kenar_uzunlugu", Arity::optional(), "kenar yönteminin uzunluğu (m)")
                    .measured_in("m")
                    .en("side_length"),
                Param::number("aci", Arity::optional(),
                              "İlk köşenin merkeze göre doğrultusu; varsayılan 0")
                    .en("angle"),
                Param::points("kose", Arity::optional(),
                              "Yerine işaret edilen nokta: yarıçapı ve yönü verir; yaricap "
                              "verilmişse sorulmaz")
                    .en("corner"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar "
                   "uzunluğundan.",
        .run    = &run,
        .effect = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
