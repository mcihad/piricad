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
// THE VERTICES ARE EXACT WHERE THEY CAN BE. The sine and cosine come from
// `core::sin_cos_udeg`, which is integer-exact on the four axes, so a square at
// 0° has its corners on the millimetre rather than one off it (§7.3).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Çokgenin merkezi");
    if (!centre) co_return;

    auto sides = co_await ctx.integer("kenar_sayisi", "Kenar sayısı (3–1024)");
    if (!sides) co_return;
    if (*sides < 3 || *sides > 1024) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Kenar sayısı 3 ile 1024 arasında olmalı; " +
                                         std::to_string(*sides) + " geldi."));
        co_return;
    }
    const auto n = static_cast<double>(*sides);

    std::string how = "ic";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };

    // ONE NUMBER, AND WHAT IT MEANS DEPENDS ON THE METHOD. Asking for a
    // "radius" under `kenar` would be asking for something the user does not
    // have; the prompt says which of the three it wants.
    auto given =
        co_await ctx.number(is("kenar") ? "kenar_uzunlugu" : "yaricap",
                            is("kenar") ? "Kenar uzunluğu (m)"
                            : is("dis") ? "Kenarlara teğet çemberin yarıçapı (m)"
                                        : "Köşelerin üzerinde olduğu çemberin yarıçapı (m)");
    if (!given) co_return;
    if (*given <= 0.0) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Yarıçap ya da kenar uzunluğu sıfır ya da eksi olamaz."));
        co_return;
    }

    // Every method reduces to the CIRCUMRADIUS, which is what the vertices sit
    // on. Inscribed is it directly; circumscribed divides by cos(π/n) because
    // the edge midpoint is that much closer than the vertex; a side length s
    // gives R = s / (2·sin(π/n)).
    const double pi_over_n = std::acos(-1.0) / n;
    double circumradius    = *given;
    if (is("dis"))
        circumradius = *given / std::cos(pi_over_n);
    else if (is("kenar"))
        circumradius = *given / (2.0 * std::sin(pi_over_n));

    double turn = 0.0;
    if (const Value v = ctx.argument("aci"); !v.empty()) turn = v.as_number();

    const core::AngleConvention convention = ctx.session().bus().angle_convention();
    const double start_turns = core::turns_from_udeg(core::udeg_from_angle(turn, convention.unit));

    std::vector<core::Point2> corners;
    corners.reserve(static_cast<std::size_t>(*sides));
    for (std::int64_t i = 0; i < *sides; ++i) {
        // COUNTER-CLOCKWISE IN THE DRAWING, whichever angle rule the session is
        // in: a ring's winding is geometry, not a reading convention (model.md
        // R11). Under semt an increasing angle turns clockwise, so the step is
        // negated there to keep the ring wound the one way the model stores.
        const double step = static_cast<double>(i) / n;
        const double at =
            convention.rule == core::AngleRule::Semt ? start_turns - step : start_turns + step;
        corners.push_back(*centre + core::polar_offset_turns(circumradius, at, convention.rule));
    }

    std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{corners, core::RingRole::Exterior, 0}};
    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.session().fail(created.error());
        co_return;
    }

    // WHAT WAS ASKED, not the corners it derived. Replaying the four inputs
    // through this command gives the same ring, and recording the corners would
    // let a later edit move one and leave a "regular polygon" that is not one
    // (Article 1.4).
    ctx.record("merkez", Value::point(*centre));
    ctx.record("kenar_sayisi", Value::integer(*sides));
    ctx.record(is("kenar") ? "kenar_uzunlugu" : "yaricap", Value::number(*given));
    if (!is("ic")) ctx.record("yontem", Value::text(how));

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
                Param::point("merkez", "Çokgenin merkezi"),
                Param::integer_range("kenar_sayisi", Arity::exactly(1), 3, 1024, "Kenar sayısı"),
                Param::choice("yontem", Arity::optional(), {"ic", "dis", "kenar"},
                              "ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · "
                              "kenar: kenar uzunluğundan"),
                Param::number("yaricap", Arity::optional(), "ic/dis yönteminin yarıçapı (m)")
                    .measured_in("m"),
                Param::number("kenar_uzunlugu", Arity::optional(), "kenar yönteminin uzunluğu (m)")
                    .measured_in("m"),
                Param::number("aci", Arity::optional(),
                              "İlk köşenin merkeze göre doğrultusu; varsayılan 0"),
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
