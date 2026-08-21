// SPDX-License-Identifier: GPL-3.0-or-later
// core.erase (SİL), core.undo (GERİAL), core.redo (YİNELE)
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

namespace piricad::command {
namespace {

Task<void> run_erase(Context& ctx)
{
    const Value selection = ctx.argument("nesneler");
    if (selection.empty()) {
        ctx.echo("Silinecek nesne belirtilmedi. Örnek: SİL nesneler=0");
        co_return;
    }

    std::size_t removed = 0;
    for (std::int64_t raw : selection.as_ids()) {
        if (raw < 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw));
            co_return;
        }
        const auto id = static_cast<core::EntityId>(raw);
        if (!ctx.document().alive(id)) {
            ctx.echo("Nesne bulunamadı veya zaten silinmiş: " + std::to_string(raw));
            co_return;
        }
        auto st = ctx.transaction().erase_entity(id);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++removed;
    }

    ctx.record("nesneler", selection);
    ctx.echo(std::to_string(removed) + " nesne silindi.");
}

Task<void> run_undo(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    std::string label;

    auto st = bus.undo_stack().undo(bus.document(), &label);
    if (!st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.echo("Geri alındı: " + label);
    if (bus.on_document_changed) bus.on_document_changed();
}

Task<void> run_redo(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    std::string label;

    auto st = bus.undo_stack().redo(bus.document(), &label);
    if (!st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.echo("Yinelendi: " + label);
    if (bus.on_document_changed) bus.on_document_changed();
}

} // namespace

PIRICAD_COMMAND(erase)
{
    return CommandSpec{
        .id       = "core.erase",
        .names    = {"SİL", "SIL", "ERASE", "E"},
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity::at_least(1),
                         "Silinecek nesnelerin kimlikleri"}},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Seçilen nesneleri siler.",
        .run      = &run_erase,
    };
}

// GERİAL and YİNELE walk the command journal rather than editing the document
// themselves, so they are marked ReadOnly: they must never become an undo step.
PIRICAD_COMMAND(undo)
{
    return CommandSpec{
        .id       = "core.undo",
        .names    = {"GERİAL", "GERIAL", "UNDO", "U"},
        .category = Category::System,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::ReadOnly,
        .summary  = "Son işlemi geri alır.",
        .run      = &run_undo,
    };
}

PIRICAD_COMMAND(redo)
{
    return CommandSpec{
        .id       = "core.redo",
        .names    = {"YİNELE", "YINELE", "REDO"},
        .category = Category::System,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::ReadOnly,
        .summary  = "Geri alınan işlemi yineler.",
        .run      = &run_redo,
    };
}

} // namespace piricad::command
