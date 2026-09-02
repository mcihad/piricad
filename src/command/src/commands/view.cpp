// SPDX-License-Identifier: GPL-3.0-or-later
// core.zoom — YAKINLAŞ. A transparent command (kentoscad.md §3): it may interrupt
// another running command, and it changes view state, never document state.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::string mode = "KAPSAM";
    double factor    = 1.0;

    if (const Value v = ctx.argument("mod"); !v.empty()) {
        mode = core::turkish_fold_key(v.as_text());
        ctx.record("mod", Value::text(mode));
    }
    if (const Value v = ctx.argument("carpan"); !v.empty()) {
        factor = v.as_number(1.0);
        ctx.record("carpan", v);
    }

    if (mode != "KAPSAM" && mode != "EXTENTS" && mode != "ÇARPAN" && mode != "CARPAN" &&
        mode != "FACTOR" && mode != "SIFIRLA" && mode != "RESET") {
        ctx.echo("Beklenen mod: KAPSAM | ÇARPAN | SIFIRLA. Girilen: '" + mode + "'");
        co_return;
    }

    if (bus.on_view_request)
        bus.on_view_request(mode, factor);
    else
        ctx.echo("Görünüm istemcisi bağlı değil (başsız çalışma).");
}

/// KAYDIR — move the view without changing its scale.
///
/// Two points: the drawing slides so that the first lands on the second. That is
/// the gesture every CAD calls pan, and stating it as a pair of DOCUMENT points
/// rather than a pixel delta is what lets a script, the AI and the mouse all
/// express the same move (Article 1.2, 1.4).
Task<void> run_pan(Context& ctx)
{
    auto from = co_await ctx.point("baslangic", "Kaydırmanın tutulacağı nokta");
    if (!from) co_return; // ESC before the view moved

    auto to = co_await ctx.point("bitis", "Bu noktaya taşınacak",
                                 PointOptions{.rubber_band = true, .rubber_origin = *from});
    if (!to) co_return;

    Bus& bus = ctx.session().bus();
    if (bus.on_pan_request)
        bus.on_pan_request(*from, *to);
    else
        ctx.echo("Görünüm istemcisi bağlı değil (başsız çalışma).");

    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::point(*to));
}

} // namespace

KENTOS_COMMAND(pan)
{
    return CommandSpec{
        .id       = "core.pan",
        .names    = {"KAYDIR", "PAN", "KY"},
        .category = Category::View,
        .params =
            {
                Param::point("baslangic", "Kaydırmanın tutulacağı nokta"),
                Param::point("bitis", "O noktanın taşınacağı yer"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible |
                 Flags::Transparent | Flags::ReadOnly,
        .summary = "Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.",
        .run     = &run_pan,
    };
}

KENTOS_COMMAND(zoom)
{
    return CommandSpec{
        .id       = "core.zoom",
        .names    = {"YAKINLAŞ", "YAKINLAS", "ZOOM", "Z"},
        .category = Category::View,
        .params =
            {
                Param::text("mod", Arity::optional(), "KAPSAM | ÇARPAN | SIFIRLA"),
                Param::number("carpan", Arity::optional(), "ÇARPAN modunda ölçek katsayısı"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::AiAccessible | Flags::Transparent | Flags::ReadOnly,
        .summary = "Görünümü çizim kapsamına veya verilen çarpana ayarlar.",
        .run     = &run,
    };
}

} // namespace kentos::command
