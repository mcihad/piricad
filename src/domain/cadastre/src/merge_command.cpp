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
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

bool polygon_of(const core::Document& doc, core::EntityId slot, core::Polygon& out)
{
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

    out.exterior.clear();
    out.holes.clear();

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;

        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);

        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});

        if (geom.ring_role[r] == core::RingRole::Exterior && out.exterior.empty())
            out.exterior = std::move(ring);
        else if (ring.size() >= 3)
            out.holes.push_back(std::move(ring));
    }
    return out.exterior.size() >= 3;
}

Task<void> run(Context& ctx)
{
    const Selection& selection = ctx.session().bus().selection();

    Value::Ints requested = ctx.argument("nesneler").as_ids();
    if (requested.empty())
        for (core::EntityKey k : selection.keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

    if (requested.size() < 2) {
        ctx.echo("Tevhit en az iki parsel ister. Seçili: " + std::to_string(requested.size()) +
                 ". Birleştirilecek parselleri seçin.");
        co_return;
    }

    const core::Document& doc = ctx.document();

    std::vector<core::Polygon> parcels;
    std::vector<core::EntityId> slots;
    for (std::int64_t raw : requested) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return; // the bus rolls the whole transaction back
        }

        core::Polygon poly;
        if (!polygon_of(doc, slot, poly)) {
            ctx.echo("Nesne " + std::to_string(raw) +
                     " kapalı bir alan değil; tevhit yalnız alanlar üzerinde çalışır.");
            co_return;
        }
        parcels.push_back(std::move(poly));
        slots.push_back(slot);
    }

    // ---- the union ----
    std::vector<core::Polygon> subject{parcels.front()};
    std::vector<core::Polygon> clip(parcels.begin() + 1, parcels.end());

    auto merged = core::polygon_boolean(subject, clip, core::BooleanOp::Union);
    if (!merged) {
        ctx.echo(merged.error().message);
        co_return;
    }

    if (merged.value().empty()) {
        ctx.echo("Birleşme sonucu boş çıktı; parseller bir alan kapatmıyor.");
        co_return;
    }

    // MORE THAN ONE PIECE MEANS THEY DO NOT TOUCH, and a tevhit of parcels that do
    // not adjoin is not a tevhit. Saying so is the honest answer; drawing two
    // parcels and calling them one would produce a record TKGM would reject.
    if (merged.value().size() > 1) {
        ctx.echo("Bu parseller bitişik değil: birleşme " +
                 std::to_string(merged.value().size()) +
                 " ayrı parça veriyor. Tevhit yalnız komşu parseller içindir.");
        co_return;
    }

    // ---- write it ----
    const core::Polygon& result = merged.value().front();

    std::vector<core::RingGeometry::RingInput> rings;
    rings.push_back(core::RingGeometry::RingInput{result.exterior, core::RingRole::Exterior, 0});
    for (const std::vector<core::Point2>& hole : result.holes)
        rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});

    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.echo(created.error().message);
        co_return;
    }

    // ---- the attributes, only where every input agrees ----
    const core::AttrTable& table = doc.attributes();
    std::vector<std::string> dropped;

    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col = static_cast<core::AttrId>(c);
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
            ctx.echo(st.error().message);
            co_return;
        }
    }

    // ---- and the originals go ----
    for (core::EntityId slot : slots) {
        if (auto st = ctx.transaction().erase_entity(slot); !st) {
            ctx.echo(st.error().message);
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

KENTOS_COMMAND(merge)
{
    return CommandSpec{
        .id       = "core.merge",
        .names    = {"TEVHİT", "TEVHIT", "MERGE", "TVH"},
        .category = Category::Modify,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Birleştirilecek parseller; yoksa etkin seçim"}},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Komşu parselleri tek parselde birleştirir (tevhit).",
        .run      = &run,
    };
}

} // namespace kentos::command
