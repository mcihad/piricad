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

    // THE ORDER EVERY MODIFY COMMAND KEEPS (`want_objects`): the named ids, then
    // the active selection — the select-then-erase order every CAD user works in
    // — and then ASKED FOR. The third step is new, and it is why the Delete key
    // and the Sil menu entry are no longer dead with nothing highlighted: they
    // used to answer "seçim boş" and stop, where every CAD program asks which
    // objects. The selection is session state (model.md R43), so the ids are
    // copied out and the journal records them — a replay must not depend on what
    // happened to be highlighted at the time.
    std::vector<std::int64_t> requested;
    if (!co_await want_objects(ctx, "nesneler", "Silinecek nesneleri seçin, sonra Enter", requested,
                               0, "SİL nesneler=1"))
        co_return;

    std::size_t removed = 0;
    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Geçersiz nesne kimliği: " + std::to_string(raw) +
                           ". Kimlikler 1'den başlar.");
            co_return;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya zaten silinmiş: " + std::to_string(raw));
            co_return;
        }
        auto st = ctx.transaction().erase_entity(slot);
        if (!st) {
            ctx.refuse(st.error());
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

/// NOT AFTER A BATCH HAS EDITED. A batch is one undo step that does not exist
/// until it closes, so an undo inside it cannot reach the batch's own edits: it
/// reaches the entry below — the work done before the script ran — and applies
/// it under the batch's open transaction. The close then pushes the batch, which
/// empties the redo stack, and that earlier work is gone with no way back. The
/// manual's "Çiz ve geri al" script looked like it undid its own line; it undid
/// nothing, and the silent refusal hid which of the two it was (TODOS F-01).
///
/// Before the batch's first edit there is nothing to cut across: a console line
/// that only undoes is the same GERİAL as the command line's, and a batch that
/// ends with no edit of its own pushes no step and leaves the redo stack alone.
bool refuse_inside_batch(Context& ctx, const char* verb)
{
    if (!ctx.session().bus().batch_has_edits()) return false;
    ctx.refuse(core::ErrorCode::Unsupported,
               std::string("Düzenleme yapmış bir toplu işin içinde ") + verb +
                   ": toplu iş bittiğinde tek bir geri alma adımı olur. Onu bittikten "
                   "sonra GERİAL ile bütünüyle geri alabilirsiniz.");
    return true;
}

Task<void> run_undo(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    std::string label;
    if (refuse_inside_batch(ctx, "geri alınamaz")) co_return;

    auto st = bus.undo_stack().undo(bus.document(), &label);
    if (!st) {
        ctx.refuse(st.error());
        co_return;
    }

    ctx.echo("Geri alındı: " + label);
    if (bus.on_document_changed) bus.on_document_changed();
}

Task<void> run_redo(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    std::string label;
    if (refuse_inside_batch(ctx, "yinelenemez")) co_return;

    auto st = bus.undo_stack().redo(bus.document(), &label);
    if (!st) {
        ctx.refuse(st.error());
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
        .title    = "Sil",
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Silinecek nesnelerin kimlikleri; yoksa etkin seçim"}
                         .en("objects")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
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
        .title    = "Geri Al",
        .category = Category::System,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::ReadOnly,
        .summary  = "Son işlemi geri alır.",
        .run      = &run_undo,
        // REVERSES THE DRAWING. `ReadOnly` is on it because it opens no
        // transaction of its own; the document is different afterwards.
        .effect = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(redo)
{
    return CommandSpec{
        .id       = "core.redo",
        .names    = {"YİNELE", "YINELE", "REDO"},
        .title    = "Yinele",
        .category = Category::System,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::ReadOnly,
        .summary  = "Geri alınan işlemi yineler.",
        .run      = &run_redo,
        .effect   = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
