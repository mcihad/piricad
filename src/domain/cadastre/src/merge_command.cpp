// SPDX-License-Identifier: GPL-3.0-or-later
// core.merge — TEVHİT. Two or more parcels become one.
//
// The geometry is a union and is objective: the merged boundary is the outline of
// what the inputs covered together, and no rule of any regulation changes that.
//
// THE ATTRIBUTES ARE NOT OBJECTIVE, and this file is deliberately conservative
// about them. A merged parcel's ada, pafta, malik and nitelik follow from
// cadastral practice and from what TKGM will accept, not from arithmetic. So the
// rule here is the only one that cannot be wrong: a column whose value is the
// SAME on every input keeps that value; a column where the inputs disagree comes
// back EMPTY, and the command says which ones it emptied. Filling one in by
// picking the first parcel's would be inventing a record.
//
// PENDING SIGN-OFF (CLAUDE.md 6.11): a rule that decides those columns is a
// regulatory rule, belongs in /data (5.13), and needs a harita mühendisi to
// approve it. Until then this refuses to guess rather than guessing quietly.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/geometry.hpp"
#include "piricad/core/offset.hpp"

#include "parcel_face.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    // The parcels: named, highlighted, or ASKED FOR. Pressed with nothing
    // highlighted, Tevhit used to refuse where it could have asked.
    Value::Ints requested;
    if (!co_await want_objects(ctx, "nesneler", "Birleştirilecek parselleri seçin, sonra Enter",
                               requested, 0, "TEVHİT nesneler=1 nesneler=2"))
        co_return;

    if (requested.size() < 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tevhit en az iki parsel ister. Seçili: " + std::to_string(requested.size()) +
                       ". Birleştirilecek parselleri seçin.");
        co_return;
    }

    const core::Document& doc = ctx.document();

    // THE PARCELS AS FACES, their arcs as arcs (parcel_face.hpp, TODOS O-3):
    // one rounded parcel among them sends the union through the kernel.
    std::vector<core::KernelFace> parcels;
    std::vector<core::EntityId> slots;
    bool curved = false;
    for (std::int64_t raw : requested) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return; // the bus rolls the whole transaction back
        }

        std::optional<cadastre::ParcelFace> face = cadastre::parcel_face(doc, slot);
        if (!face) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(raw) +
                           " kapalı bir alan değil; tevhit yalnız alanlar üzerinde çalışır.");
            co_return;
        }
        curved = curved || face->curved;
        parcels.push_back(std::move(face->face));
        slots.push_back(slot);
    }

    // ---- the union ----
    auto merged = cadastre::parcel_boolean(
        std::span(parcels).first(1), std::span(parcels).subspan(1), core::BooleanOp::Union, curved);
    if (!merged) {
        ctx.refuse(merged.error());
        co_return;
    }

    if (merged.value().empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Birleşme sonucu boş çıktı; parseller bir alan kapatmıyor.");
        co_return;
    }

    // MORE THAN ONE PIECE MEANS THEY DO NOT TOUCH, and a tevhit of parcels that do
    // not adjoin is not a tevhit. Saying so is the honest answer; drawing two
    // parcels and calling them one would produce a record TKGM would reject.
    if (merged.value().size() > 1) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu parseller bitişik değil: birleşme " + std::to_string(merged.value().size()) +
                       " ayrı parça veriyor. Tevhit yalnız komşu parseller içindir.");
        co_return;
    }

    // ---- write it ----
    auto created = cadastre::add_face(ctx, ctx.active_layer(), merged.value().front());
    if (!created) {
        ctx.refuse(created.error());
        co_return;
    }
    // Every parcel merged into it, by key (core/lineage.hpp).
    if (auto st = ctx.derive(created.value(), std::span<const core::EntityId>(slots)); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    // ---- the attributes, only where every input agrees ----
    const core::AttrTable& table = doc.attributes();
    std::vector<std::string> dropped;

    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col                 = static_cast<core::AttrId>(c);
        const core::AttrColumn* column = table.column(col);
        if (column == nullptr) continue;

        bool agreed = true;
        core::AttrValue shared{};
        for (std::size_t i = 0; i < slots.size(); ++i) {
            auto had = doc.attribute(col, slots[i]);
            if (!had) {
                agreed = false;
                break;
            }
            if (i == 0)
                shared = had.value();
            else if (!(had.value() == shared)) {
                agreed = false;
                break;
            }
        }

        if (!agreed) {
            if (column->spec().id.size() > 0) dropped.push_back(column->spec().id);
            continue;
        }
        if (!shared.present) continue;

        if (auto st = ctx.transaction().set_attribute(col, created.value(), shared); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }

    // ---- and the originals go ----
    for (core::EntityId slot : slots) {
        if (auto st = ctx.transaction().erase_entity(slot); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }

    ctx.record("nesneler", Value::ids(requested));

    std::string said = std::to_string(slots.size()) + " parsel tevhit edildi.";
    if (!dropped.empty()) {
        said += "\n  Girdiler şu sütunlarda ayrıştığı için yeni parselde BOŞ bırakıldı:";
        for (const std::string& id : dropped)
            said += "\n    " + id;
        said += "\n  Bunları doldurmak mevzuata ait bir karardır; ÖZNİTELİK ile yazın.";
    }
    ctx.echo(said);
}

} // namespace

PIRICAD_COMMAND(merge)
{
    return CommandSpec{
        .id       = "core.merge",
        .names    = {"TEVHİT", "TEVHIT", "MERGE", "TVH"},
        .title    = "Tevhit",
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                           "Birleştirilecek parseller; yoksa etkin seçim"}
                         .en("objects")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Komşu parselleri tek parselde birleştirir (tevhit).",
        .run      = &run,
        .targets  = Targets::Faces,
    };
}

} // namespace piricad::command
