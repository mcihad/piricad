// SPDX-License-Identifier: GPL-3.0-or-later
// core.erase (SİL), core.undo (GERİAL), core.redo (YİNELE)
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <vector>

namespace kentos::command {
namespace {

// `nesneler` carries persistent KEYS, not dense slots.
//
// model.md R5 and P4: every id that reaches a user, a file, a journal line or an
// AI tool call is a key; a slot is valid only inside one in-memory Document. A
// journalled slot replays onto whatever entity happens to hold that index next,
// and in a cadastral drawing that is the neighbouring parcel. `SEÇ` reports keys
// for the same reason, so the two commands speak one language: what SEÇ prints,
// SİL accepts.
//
// Keys start at 1 (EntityKey{0} is "none"), so the first object a drawing creates
// is `1`, not `0`.
Task<void> run_erase(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    std::vector<std::int64_t> requested;

    if (const Value given = ctx.argument("nesneler"); !given.empty()) {
        requested = given.as_ids();
    } else {
        // No argument: the active selection is what the user means, which is the
        // select-then-erase order every CAD user works in. The selection is
        // session state (model.md R43), so the ids are copied out here and the
        // journal records them — a replay must not depend on what happened to be
        // highlighted at the time.
        for (core::EntityKey k : bus.selection().keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

        if (requested.empty()) {
            ctx.echo("Silinecek nesne belirtilmedi ve seçim boş. Örnek: SİL nesneler=1");
            co_return;
        }
    }

    std::size_t removed = 0;
    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            co_return;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.echo("Nesne bulunamadı veya zaten silinmiş: " + std::to_string(raw));
            co_return;
        }
        auto st = ctx.transaction().erase_entity(slot);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++removed;
    }

    // An erased entity cannot stay selected: its key is retired and never reused
    // (R4), so a stale selection would point at nothing for the rest of the
    // session. Undo restores the object, not the highlight.
    bool touched = false;
    for (std::int64_t raw : requested)
        touched |=
            bus.selection().remove(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
    if (touched && bus.on_selection_changed) bus.on_selection_changed();

    ctx.record("nesneler", Value::ids(requested));
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

KENTOS_COMMAND(erase)
{
    return CommandSpec{
        .id       = "core.erase",
        .names    = {"SİL", "SIL", "ERASE", "E"},
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Silinecek nesnelerin kimlikleri; yoksa etkin seçim"}},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Seçilen nesneleri siler.",
        .run      = &run_erase,
    };
}

// GERİAL and YİNELE walk the command journal rather than editing the document
// themselves, so they are marked ReadOnly: they must never become an undo step.
KENTOS_COMMAND(undo)
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

KENTOS_COMMAND(redo)
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

} // namespace kentos::command
