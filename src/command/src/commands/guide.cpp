// SPDX-License-Identifier: GPL-3.0-or-later
// core.guide — KILAVUZ. The construction lines a drafter pulls off the ruler.
//
// A horizontal or vertical line at a fixed coordinate that the cursor snaps to
// and the plot never prints. Dragging one off the ruler is the gesture; this is
// the command behind it, so a script and the AI can place the same guides a hand
// can — which is the whole reason the drag is not allowed to touch the document
// itself (Article 1.1, 1.2).
//
// ADDRESSED BY COORDINATE, NOT BY INDEX. Removing a guide shifts the ones after
// it, so an index a user typed a moment ago names a different line by the time
// they press Enter. The coordinate is what they can see on the ruler.
//
// AN ANGLED GUIDE IS THE SAME COMMAND with a number where the word was: `yon`
// takes `yatay`, `düşey`, OR an angle — `yon=45g`, `yon=50`, `yon=30d` — and then
// `nokta=` says where it passes and `tur=isin` makes it one-sided. The angle is
// read under the session's own convention (`core.aci.birim`, `core.aci.kural`), so
// what `@mesafe<açı` means and what a guide's direction means are one setting pair
// (TODOS-CAD P0-4). A second word for "the angled kind" would have been a second
// way to say one thing.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/guide.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::command {
namespace {

std::string axis_name(core::GuideAxis a)
{
    switch (a) {
    case core::GuideAxis::Horizontal: return "yatay";
    case core::GuideAxis::Vertical: return "düşey";
    case core::GuideAxis::Angled: return "açılı";
    }
    return "?";
}

/// The reading a guide shows on the ruler, in metres.
std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac + " m";
}

void list_guides(Context& ctx)
{
    const core::GuideStore& guides = ctx.document().guides();
    if (guides.empty()) {
        ctx.echo("Çizimde kılavuz yok. Cetvelden sürükleyin ya da "
                 "KILAVUZ yon=yatay deger=<metre> yazın.");
        return;
    }

    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    std::string out = std::to_string(guides.size()) + " kılavuz:";
    for (std::size_t i = 0; i < guides.size(); ++i) {
        out += "\n  " + axis_name(guides.axis(i)) + "  ";
        if (guides.axis(i) != core::GuideAxis::Angled) {
            out += metres(guides.coordinate(i));
            continue;
        }
        // THE STORED ANGLE IS MATHEMATICAL — counter-clockwise from east, which is
        // what `atan2_udeg` answers and what the snap test needs. It is PRINTED in
        // the session's own convention, so a list read out loud matches what the
        // user typed (`core::direction_turns` does the same conversion for ÖLÇ).
        const core::SinCos dir = core::sin_cos_udeg(guides.angle(i));
        const core::Point2 at  = guides.through(i);
        const core::Point2 along{at.x + core::mm_round(dir.cos * 1000000.0),
                                 at.y + core::mm_round(dir.sin * 1000000.0)};
        out += core::angle_text(core::direction_turns(at, along, convention.rule), convention.unit);
        out += "  (" + metres(at.x) + ", " + metres(at.y) + ")";
        if (guides.ray(i)) out += "  ışın";
    }
    ctx.echo(out);
}

/// Reads `yon` as an ANGLE when it is one: a number, optionally with a `g`, `d`
/// or `r` suffix naming its unit. False when the word is not a number at all,
/// which is how the cardinal words fall through to their own branch.
///
/// Its own reader rather than the grammar's, because `yon` is declared as text
/// and has to keep taking `yatay`. The suffix rule is the parser's own
/// (`core::angle_unit_from_suffix`), so `@100<45g` and `KILAVUZ yon=45g` read the
/// same letter to mean the same unit (CLAUDE.md 5.11 in spirit: one rule, not one
/// parser copied).
bool angle_from_word(const std::string& word, core::AngleConvention session, double& out,
                     core::AngleUnit& unit)
{
    if (word.empty()) return false;
    unit = session.unit;

    std::string digits = word;
    core::AngleUnit named{};
    if (digits.size() > 1 && core::angle_unit_from_suffix(digits.back(), named)) {
        unit = named;
        digits.pop_back();
    }

    // A COMMA IS A DECIMAL POINT HERE, because a Turkish keyboard writes 45,5 and
    // every number this program prints uses one.
    for (char& c : digits)
        if (c == ',') c = '.';

    try {
        std::size_t used = 0;
        const double v   = std::stod(digits, &used);
        if (used != digits.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

Task<void> run(Context& ctx)
{
    const Value axis_arg = ctx.argument("yon");
    const Value value    = ctx.argument("deger");
    const Value remove   = ctx.argument("sil");
    const Value at_arg   = ctx.argument("nokta");
    const Value kind_arg = ctx.argument("tur");

    if (axis_arg.empty() && value.empty() && at_arg.empty()) {
        list_guides(ctx);
        co_return;
    }

    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // ---- an ANGLE in `yon` makes an angled guide ---------------------------
    double typed_angle = 0.0;
    core::AngleUnit typed_unit{};
    if (!axis_arg.empty() &&
        angle_from_word(axis_arg.as_text(), convention, typed_angle, typed_unit)) {
        core::Point2 at{};
        if (!at_arg.empty() && !at_arg.as_points().empty()) {
            at = at_arg.as_point();
        } else {
            auto asked = co_await ctx.point("nokta", "Kılavuzun geçtiği nokta");
            if (!asked) co_return;
            at = *asked;
        }

        bool ray = false;
        if (!kind_arg.empty()) {
            const std::string kind = kind_arg.as_text();
            if (core::turkish_iequals(kind, "isin") || core::turkish_iequals(kind, "ışın") ||
                core::turkish_iequals(kind, "ray")) {
                ray = true;
            } else if (!core::turkish_iequals(kind, "dogru") &&
                       !core::turkish_iequals(kind, "doğru") &&
                       !core::turkish_iequals(kind, "line")) {
                ctx.echo("Beklenen tür: doğru | ışın. Girilen: '" + kind + "'");
                co_return;
            }
        }

        const std::int64_t math = core::math_udeg_from_angle(
            typed_angle, core::AngleConvention{typed_unit, convention.rule});
        if (auto st = ctx.transaction().add_angled_guide(at, math, ray); !st) {
            ctx.echo(st.error().message);
            co_return;
        }

        // RECORDED AS THE SESSION READ IT, with the unit spelled out. A journal
        // line that said `yon=45` would mean one direction today and another after
        // somebody changed `core.aci.birim`; the suffix pins it (Article 1.4).
        ctx.record("yon", Value::text(std::to_string(typed_angle) +
                                      std::string(1, core::angle_unit_suffix(typed_unit))));
        ctx.record("nokta", Value::point(at));
        if (ray) ctx.record("tur", Value::text("ışın"));

        const core::SinCos dir = core::sin_cos_udeg(math);
        const core::Point2 along{at.x + core::mm_round(dir.cos * 1000000.0),
                                 at.y + core::mm_round(dir.sin * 1000000.0)};
        ctx.echo(
            std::string(ray ? "Işın" : "Açılı") + " kılavuz eklendi: " +
            core::angle_text(core::direction_turns(at, along, convention.rule), convention.unit) +
            "  (" + metres(at.x) + ", " + metres(at.y) + ")");
        co_return;
    }

    core::GuideAxis axis = core::GuideAxis::Horizontal;
    if (!axis_arg.empty()) {
        const std::string typed = axis_arg.as_text();
        if (core::turkish_iequals(typed, "düşey") || core::turkish_iequals(typed, "dusey") ||
            core::turkish_iequals(typed, "dikey") || core::turkish_iequals(typed, "vertical") ||
            core::turkish_iequals(typed, "v")) {
            axis = core::GuideAxis::Vertical;
        } else if (!core::turkish_iequals(typed, "yatay") &&
                   !core::turkish_iequals(typed, "horizontal") &&
                   !core::turkish_iequals(typed, "y")) {
            ctx.echo("Beklenen yön: yatay | düşey | bir açı (örn. yon=45g). Girilen: '" + typed +
                     "'");
            co_return;
        }
    }

    if (value.empty()) {
        ctx.echo("Kılavuzun koordinatı eksik. Örnek: KILAVUZ yon=yatay deger=4310220.5");
        co_return;
    }
    const auto coordinate = static_cast<core::Mm>(value.as_int());

    if (!remove.empty() && remove.as_bool()) {
        // A guide is removed by naming where it is, within half a metre — which is
        // as precise as anyone can point at a line on a ruler.
        constexpr core::Mm kReach = 500;
        const std::size_t hit     = ctx.document().guides().nearest(axis, coordinate, kReach);
        if (hit >= ctx.document().guides().size()) {
            ctx.echo("Orada " + axis_name(axis) + " kılavuz yok: " + metres(coordinate));
            co_return;
        }
        if (auto st = ctx.transaction().remove_guide(hit); !st) {
            ctx.echo(st.error().message);
            co_return; // the bus rolls the transaction back
        }

        ctx.record("yon", Value::text(axis_name(axis)));
        ctx.record("deger", Value::integer(coordinate));
        ctx.record("sil", Value::boolean(true));
        ctx.echo("Kılavuz silindi: " + axis_name(axis) + " " + metres(coordinate));
        co_return;
    }

    if (auto st = ctx.transaction().add_guide(axis, coordinate); !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.record("yon", Value::text(axis_name(axis)));
    ctx.record("deger", Value::integer(coordinate));
    ctx.echo("Kılavuz eklendi: " + axis_name(axis) + " " + metres(coordinate));
}

} // namespace

KENTOS_COMMAND(guide)
{
    return CommandSpec{
        .id       = "core.guide",
        .names    = {"KILAVUZ", "GUIDE", "KLV"},
        .title    = "Kılavuz",
        .category = Category::Draw,
        .params =
            {
                Param::text("yon", Arity::optional(),
                            "yatay | düşey | bir açı (45, 45g, 30d); yoksa kılavuzlar listelenir")
                    .en("direction"),
                Param::integer("deger", Arity::optional(),
                               "Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa")
                    .en("value"),
                Param::points("nokta", Arity::optional(),
                              "Açılı kılavuzun geçtiği nokta; yalnız `yon` bir açıysa")
                    .en("point"),
                Param::choice("tur", Arity::optional(), {"dogru", "isin"},
                              "doğru: iki yöne sonsuz · ışın: noktadan ileriye")
                    .en("type"),
                Param::boolean("sil", Arity::optional(), "Verilen yerdeki kılavuzu siler")
                    .en("delete"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler.",
        .run     = &run,
    };
}

} // namespace kentos::command
