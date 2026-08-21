// SPDX-License-Identifier: GPL-3.0-or-later
// core.layer — KATMAN
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    auto name = co_await ctx.text("ad", "Katman adı");
    if (!name || name->empty()) co_return;

    Bus& bus = ctx.session().bus();

    // Creating an empty layer is inert and would invalidate stored ids if undone,
    // so it is deliberately not an undo step (see .claude/core.md).
    const core::LayerId id = bus.document().find_layer(*name) != core::kNoLayer
                                 ? bus.document().find_layer(*name)
                                 : bus.document().ensure_layer(*name);

    if (const Value v = ctx.argument("gorunur"); !v.empty()) {
        auto st = ctx.transaction().set_layer_visible(id, v.as_bool());
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ctx.record("gorunur", v);
    }

    if (const Value v = ctx.argument("kilitli"); !v.empty()) {
        auto st = ctx.transaction().set_layer_locked(id, v.as_bool());
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ctx.record("kilitli", v);
    }

    if (const Value v = ctx.argument("renk"); !v.empty()) {
        core::LayerStyle style = bus.document().layer(id)->style;
        style.rgba = static_cast<std::uint32_t>(v.as_int(static_cast<std::int64_t>(style.rgba)));
        auto st    = ctx.transaction().set_layer_style(id, style);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ctx.record("renk", v);
    }

    bus.set_active_layer(id);
    ctx.echo("Aktif katman: " + *name);
}

} // namespace

PIRICAD_COMMAND(layer)
{
    return CommandSpec{
        .id       = "core.layer",
        .names    = {"KATMAN", "LAYER", "KAT"},
        .category = Category::Layer,
        .params =
            {
                Param::text("ad", Arity::exactly(1),
                            "Katman adı; yoksa oluşturulur ve aktif yapılır"),
                Param::boolean("gorunur", Arity::optional(), "Katmanın görünürlüğü"),
                Param::boolean("kilitli", Arity::optional(), "Katmanın kilit durumu"),
                Param::integer("renk", Arity::optional(), "Çizim rengi, 0xAARRGGBB"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katman oluşturur, aktif yapar ve özelliklerini değiştirir.",
        .run     = &run,
    };
}

} // namespace piricad::command
