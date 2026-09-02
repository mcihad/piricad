// SPDX-License-Identifier: GPL-3.0-or-later
// core.set_layer (KATMANAT), core.match_style (STİLKOPYALA)
//
// The two housekeeping edits a drawing needs constantly and no amount of careful
// drawing avoids: something was drawn on the wrong layer, and something should
// look like the thing beside it.
//
// Both keep IDENTITY. An object moved to another layer is the same object with
// the same key and the same attributes — moving a parsel from PARSEL_TASLAK to
// PARSEL must not mint a new ada/parsel row (model.md R4, R28).
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// The entities a command works on: the named ones, or the selection.
bool gather(Context& ctx, std::vector<std::int64_t>& requested,
            std::vector<core::EntityId>& slots, const char* example)
{
    Bus& bus = ctx.session().bus();

    if (const Value given = ctx.argument("nesneler"); !given.empty()) {
        requested = given.as_ids();
    } else {
        for (core::EntityKey k : bus.selection().keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

        if (requested.empty()) {
            ctx.echo(std::string("İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ") +
                     example);
            return false;
        }
    }

    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            return false;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            return false;
        }
        slots.push_back(slot);
    }
    return true;
}

Task<void> run_set_layer(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!gather(ctx, requested, slots, "KATMANAT nesneler=1 katman=PARSEL")) co_return;

    auto name = co_await ctx.text("katman", "Taşınacak katmanın adı");
    if (!name) co_return;

    if (name->empty()) {
        ctx.echo("Katman adı boş olamaz.");
        co_return;
    }

    // CREATED IF ABSENT, exactly as drawing on a new layer creates it: a command
    // that refused an unknown name would make the user run KATMAN first for no
    // reason the drawing cares about.
    const core::LayerId layer = ctx.transaction().ensure_layer(*name);
    if (layer == core::kNoLayer) {
        ctx.echo("Katman oluşturulamadı: " + *name);
        co_return;
    }

    for (core::EntityId slot : slots) {
        auto st = ctx.transaction().set_entity_layer(slot, layer);
        if (!st) {
            ctx.echo(st.error().message);
            co_return; // the bus rolls the whole transaction back
        }
    }

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("katman", Value::text(*name));
    ctx.echo(std::to_string(slots.size()) + " nesne '" + *name + "' katmanına taşındı.");
}

Task<void> run_match_style(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!gather(ctx, requested, slots, "STİLKOPYALA kaynak=1 nesneler=2 nesneler=3")) co_return;

    const Value source_arg = ctx.argument("kaynak");
    if (source_arg.empty()) {
        ctx.echo("Stili kopyalanacak kaynak nesne belirtilmedi. Örnek: "
                 "STİLKOPYALA kaynak=1 nesneler=2");
        co_return;
    }

    const std::int64_t source_id =
        source_arg.kind() == Value::Kind::IdList
            ? (source_arg.as_ids().size() == 1 ? source_arg.as_ids()[0] : 0)
            : source_arg.as_int();

    if (source_id <= 0) {
        ctx.echo("Geçersiz kaynak kimliği: " + std::to_string(source_id) +
                 ". Kimlikler 1'den başlar.");
        co_return;
    }

    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(source_id));
    const core::EntityId source = ctx.document().slot_of(key);
    if (source == core::kNoEntity || !ctx.document().alive(source)) {
        ctx.echo("Kaynak nesne bulunamadı veya silinmiş: " + std::to_string(source_id));
        co_return;
    }

    // THE EFFECTIVE STYLE, not the column. A source that inherits its layer's look
    // carries `kByLayerStyle`, and copying that sentinel onto an object on a
    // DIFFERENT layer would leave it looking like ITS own layer — which is not
    // what "make this look like that" means.
    //
    // So the inheritance is resolved: a layer that carries a whole symbol stack
    // hands over its `style`, and a layer that only carries colours and widths has
    // its `appearance` interned into one. Interning is additive and hands out an
    // id that stays valid for the document's lifetime (transaction.hpp), so the
    // copy cannot go stale.
    core::StyleId style = ctx.document().entities().style[source];
    if (style == core::kByLayerStyle) {
        const core::LayerId layer = ctx.document().entities().layer[source];
        if (const core::Layer* l = ctx.document().layer_table().at(layer); l != nullptr)
            style = l->style != core::kByLayerStyle ? l->style
                                                    : ctx.transaction().intern_style(l->appearance);
    }

    std::size_t changed = 0;
    for (core::EntityId slot : slots) {
        if (slot == source) continue; // copying onto itself changes nothing

        auto st = ctx.transaction().set_entity_style(slot, style);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++changed;
    }

    ctx.record("kaynak", source_arg);
    ctx.record("nesneler", Value::ids(requested));
    ctx.echo(std::to_string(changed) + " nesne kaynağın stilini aldı.");
}

} // namespace

PIRICAD_COMMAND(set_layer)
{
    return CommandSpec{
        .id       = "core.set_layer",
        .names    = {"KATMANAT", "KATMANATA", "SETLAYER", "KA"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::text("katman", Arity::exactly(1), "Hedef katmanın adı; yoksa oluşturulur"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri başka bir katmana taşır.",
        .run     = &run_set_layer,
    };
}

PIRICAD_COMMAND(match_style)
{
    return CommandSpec{
        .id       = "core.match_style",
        .names    = {"STİLKOPYALA", "STILKOPYALA", "MATCHPROP", "SK"},
        .category = Category::Modify,
        .params =
            {
                Param{"kaynak", ParamKind::Selection, Arity::exactly(1),
                      "Stili kopyalanacak nesnenin kimliği"},
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Stili alacak nesnelerin kimlikleri; yoksa etkin seçim"},
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesnenin stilini seçilen nesnelere uygular.",
        .run     = &run_match_style,
    };
}

} // namespace piricad::command
