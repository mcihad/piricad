// SPDX-License-Identifier: GPL-3.0-or-later
// core.zoom — YAKINLAŞ. A transparent command (piricad.md §3): it may interrupt
// another running command, and it changes view state, never document state.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/text.hpp"

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::string mode = "KAPSAM";
    double factor    = 1.0;

    if (const Value v = ctx.argument("mod"); !v.empty()) {
        mode = core::turkish_upper(v.as_text());
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

} // namespace

PIRICAD_COMMAND(zoom)
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

} // namespace piricad::command
