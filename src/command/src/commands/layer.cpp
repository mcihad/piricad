// SPDX-License-Identifier: GPL-3.0-or-later
// core.layer — KATMAN
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto name = co_await ctx.text("ad", "Katman adı");
    if (!name || name->empty()) co_return;

    Bus& bus = ctx.session().bus();

    // Creating an empty layer is inert and would invalidate stored ids if undone,
    // so it is deliberately not an undo step (see .claude/core.md).
    const core::LayerId existing = bus.document().find_layer(*name);
    const core::LayerId id =
        existing != core::kNoLayer ? existing : ctx.transaction().ensure_layer(*name);

    if (const Value v = ctx.argument("grup"); !v.empty()) {
        auto st = ctx.transaction().set_layer_group(id, v.as_text());
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ctx.record("grup", v);
    }

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
        core::Appearance appearance = bus.document().layer(id)->appearance;
        appearance.rgba =
            static_cast<std::uint32_t>(v.as_int(static_cast<std::int64_t>(appearance.rgba)));
        auto st = ctx.transaction().set_layer_appearance(id, appearance);
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

KENTOS_COMMAND(layer)
{
    return CommandSpec{
        .id       = "core.layer",
        .names    = {"KATMAN", "LAYER", "KAT"},
        .category = Category::Layer,
        .params =
            {
                Param::text("ad", Arity::exactly(1),
                            "Katman adı; yoksa oluşturulur ve aktif yapılır"),
                Param::text("grup", Arity::optional(),
                            "Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök"),
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

} // namespace kentos::command
