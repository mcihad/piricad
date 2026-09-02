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
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/guide.hpp"
#include "piricad/core/text.hpp"
#include "piricad/core/units.hpp"

#include <string>

namespace piricad::command {
namespace {

std::string axis_name(core::GuideAxis a)
{
    return a == core::GuideAxis::Horizontal ? "yatay" : "düşey";
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

    std::string out = std::to_string(guides.size()) + " kılavuz:";
    for (std::size_t i = 0; i < guides.size(); ++i)
        out += "\n  " + axis_name(guides.axis(i)) + "  " + metres(guides.coordinate(i));
    ctx.echo(out);
}

Task<void> run(Context& ctx)
{
    const Value axis_arg = ctx.argument("yon");
    const Value value    = ctx.argument("deger");
    const Value remove   = ctx.argument("sil");

    if (axis_arg.empty() && value.empty()) {
        list_guides(ctx);
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
            ctx.echo("Beklenen yön: yatay | düşey. Girilen: '" + typed + "'");
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
        const std::size_t hit = ctx.document().guides().nearest(axis, coordinate, kReach);
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

PIRICAD_COMMAND(guide)
{
    return CommandSpec{
        .id       = "core.guide",
        .names    = {"KILAVUZ", "GUIDE", "KLV"},
        .category = Category::Draw,
        .params =
            {
                Param::text("yon", Arity::optional(), "yatay | düşey; yoksa kılavuzlar listelenir"),
                Param::integer("deger", Arity::optional(),
                               "Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa"),
                Param::boolean("sil", Arity::optional(), "Verilen yerdeki kılavuzu siler"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Cetvel kılavuzu ekler, listeler ve siler.",
        .run     = &run,
    };
}

} // namespace piricad::command
