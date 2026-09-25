// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/transaction.hpp"

#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/text_fields.hpp"

#include "kentos_cad/core/dimension_link.hpp"

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/lineage.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/ties.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {

Transaction::Transaction(Document& doc, std::string label)
    : doc_(doc), label_(std::move(label)), first_born_(static_cast<EntityId>(doc.entities().size()))
{}

Status Transaction::may_change(EntityId e) const
{
    if (e >= first_born_) return core::ok();
    return doc_.editable(e);
}

LayerId Transaction::ensure_layer(std::string_view name)
{
    // No inverse is recorded: see the header. Creating a layer is additive and
    // slot-stable, and an undo that removed it would leave `entity.layer` values
    // pointing at nothing.
    return doc_.ensure_layer(name);
}

StyleId Transaction::intern_style(const Appearance& a)
{
    return doc_.intern_style(a);
}

Result<EntityId> Transaction::add_polyline(LayerId layer, std::span<const Point2> pts)
{
    core::Op undo;
    auto id = doc_.add_polyline(layer, pts, undo);
    if (!id) return id;
    inverse_.push_back(std::move(undo));
    return id;
}

Result<EntityId> Transaction::add_area(LayerId layer,
                                       std::span<const RingGeometry::RingInput> rings)
{
    core::Op undo;
    auto id = doc_.add_area(layer, rings, undo);
    if (!id) return id;
    inverse_.push_back(std::move(undo));
    return id;
}

Status Transaction::set_entity_layer(EntityId e, LayerId layer)
{
    if (auto st = may_change(e); !st) return st;
    core::Op undo;
    auto st = doc_.set_entity_layer(e, layer, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::erase_entity(EntityId e)
{
    // THE LAYER'S LOCK, asked here rather than in `Document::set_entity_alive`.
    // That function is also the UNDO path for both an erase and its inverse, so a
    // lock check inside it would trap an earlier deletion in the undo stack the
    // moment a user locked the layer. This is the road a command takes; undo
    // takes the other one.
    if (auto st = doc_.editable(e); !st) return st.error();

    core::Op undo;
    auto st = doc_.set_entity_alive(e, false, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::erase_member(EntityId e)
{
    const core::EntityTable& ents = doc_.entities();
    if (e >= ents.size() || (ents.flags[e] & core::FlagInBlock) == 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Blok tanımının üyesi olmayan bir nesne tanımdan çıkarılamaz: " +
                             std::to_string(e));
    core::Op undo;
    auto st = doc_.set_entity_alive(e, false, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_block_base(core::BlockId block, Point2 base)
{
    core::Op undo;
    auto st = doc_.set_block_base(block, base, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_block_external(core::BlockId block, std::string path, std::uint8_t flags)
{
    core::Op undo;
    auto st = doc_.set_block_external(block, std::move(path), flags, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

core::Result<std::size_t> Transaction::clear_block_members(core::BlockId block)
{
    if (block >= doc_.blocks().size())
        return core::err(core::ErrorCode::NotFound,
                         "Bilinmeyen blok kimliği: " + std::to_string(block));
    // Copied: `erase_member` leaves the list as it is (dead keys stay), but the
    // loop must not lean on that.
    const std::vector<core::EntityKey> members = doc_.blocks().at(block).members;
    std::size_t gone                           = 0;
    for (const core::EntityKey k : members) {
        const EntityId m = doc_.slot_of(k);
        if (m == core::kNoEntity || !doc_.alive(m)) continue;
        if (auto st = erase_member(m); !st) return st.error();
        ++gone;
    }
    return gone;
}

core::Result<std::size_t> Transaction::refresh_block_references(core::BlockId block)
{
    const core::BlockTable& blocks = doc_.blocks();

    // THE BLOCKS THAT DRAW IT, at any depth: the definition itself, and every
    // definition that uses one that does. A use list may name a block no member
    // places any more; refreshing such a reference is a comparison and no write.
    std::vector<bool> draws(blocks.size(), false);
    if (block < draws.size()) draws[block] = true;
    for (bool grew = true; grew;) {
        grew = false;
        for (core::BlockId b = 0; b < blocks.size(); ++b) {
            if (draws[b]) continue;
            for (const core::BlockId used : blocks.at(b).uses)
                if (used < draws.size() && draws[used]) {
                    draws[b] = true;
                    grew     = true;
                    break;
                }
        }
    }

    std::size_t changed = 0;
    for (EntityId e = 0; e < doc_.entities().size(); ++e) {
        if (!doc_.alive(e) || doc_.entities().kind[e] != core::kBlockReferenceKind) continue;
        auto ref = core::block_reference_of(doc_.geometry(), doc_.entities().slot[e]);
        if (!ref || ref.value().block >= draws.size() || !draws[ref.value().block]) continue;
        const std::uint32_t was = doc_.entities().slot[e];
        if (auto st = refresh_reference_bounds(e); !st) return st.error();
        if (doc_.entities().slot[e] != was) ++changed;
    }
    return changed;
}

Status Transaction::skip_entity_keys_to(core::EntityKey next)
{
    if (!doc_.skip_entity_keys_to(next))
        return core::err(core::ErrorCode::ValidationFailed, "Nesne anahtarı " +
                                                                std::to_string(core::raw(next)) +
                                                                " anahtar uzayının dışında.");
    return core::ok();
}

Status Transaction::move_reference(EntityId e, Point2 insertion)
{
    core::Op undo;
    auto st = doc_.move_reference(e, insertion, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::refresh_reference_bounds(EntityId e)
{
    core::Op undo;
    auto st = doc_.refresh_reference_bounds(e, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::restore_entity(EntityId e)
{
    core::Op undo;
    auto st = doc_.set_entity_alive(e, true, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_visible(LayerId l, bool visible)
{
    core::Op undo;
    auto st = doc_.set_layer_visible(l, visible, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_locked(LayerId l, bool locked)
{
    core::Op undo;
    auto st = doc_.set_layer_locked(l, locked, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_group(LayerId l, std::string group)
{
    core::Op undo;
    auto st = doc_.set_layer_group(l, std::move(group), undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_appearance(LayerId l, const Appearance& a)
{
    core::Op undo;
    auto st = doc_.set_layer_appearance(l, a, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_style(LayerId l, StyleId style)
{
    core::Op undo;
    auto st = doc_.set_layer_style(l, style, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_entity_style(EntityId e, StyleId style)
{
    if (auto st = may_change(e); !st) return st;
    core::Op undo;
    auto st = doc_.set_entity_style(e, style, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Result<EntityId> Transaction::add_circle(LayerId layer, Point2 centre, core::Mm radius)
{
    core::Op undo;
    auto made = doc_.add_circle(layer, centre, radius, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Result<EntityId> Transaction::add_kind(LayerId layer, core::KindId kind,
                                       std::span<const RingGeometry::RingInput> rings,
                                       std::span<const std::uint8_t> payload,
                                       core::BlockId in_block)
{
    core::Op undo;
    auto made = doc_.add_kind(layer, kind, rings, payload, undo, in_block);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Status Transaction::attach_foreign(EntityId e, std::string_view tag,
                                   std::span<const std::uint8_t> bytes)
{
    core::Op undo;
    if (auto st = doc_.attach_foreign(e, tag, bytes, undo); !st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

core::Result<core::BlockId> Transaction::add_block(std::string_view name,
                                                   std::string_view description, Point2 base)
{
    return doc_.add_block(name, description, base);
}

Status Transaction::add_block_use(core::BlockId block, core::BlockId uses)
{
    return doc_.add_block_use(block, uses);
}

Status Transaction::set_kind_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                                      std::span<const std::uint8_t> payload)
{
    core::Op undo;
    if (auto st = doc_.set_kind_geometry(e, rings, payload, undo); !st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_kind_geometry(EntityId e, core::KindId kind,
                                      std::span<const RingGeometry::RingInput> rings,
                                      std::span<const std::uint8_t> payload)
{
    core::Op undo;
    if (auto st = doc_.set_kind_geometry(e, kind, rings, payload, undo); !st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_kind_payload(EntityId e, std::span<const std::uint8_t> payload)
{
    core::Op undo;
    if (auto st = doc_.set_kind_payload(e, payload, undo); !st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Result<EntityId> Transaction::add_ellipse(LayerId layer, Point2 centre, Point2 major, Point2 minor)
{
    core::Op undo;
    auto made = doc_.add_ellipse(layer, centre, major, minor, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Result<EntityId> Transaction::add_arc(LayerId layer, Point2 centre, core::Mm radius, Point2 start,
                                      Point2 end)
{
    core::Op undo;
    auto made = doc_.add_arc(layer, centre, radius, start, end, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Result<EntityId> Transaction::add_point(LayerId layer, Point2 at)
{
    core::Op undo;
    auto made = doc_.add_point(layer, at, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Status Transaction::set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings)
{
    core::Op undo;
    auto st = doc_.set_geometry(e, rings, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

StyleId Transaction::intern_symbol(const core::Symbol& sym)
{
    return doc_.intern_symbol(sym);
}

core::Result<core::ImageId> Transaction::intern_image(std::span<const std::byte> bytes,
                                                      std::string_view origin)
{
    return doc_.intern_image(bytes, origin);
}

core::Result<core::DashId> Transaction::intern_dash(const core::DashPattern& pattern,
                                                    std::string_view origin)
{
    return doc_.intern_dash(pattern, origin);
}

Status Transaction::set_attribute(core::AttrId col, EntityId e, const core::AttrValue& v)
{
    if (auto st = may_change(e); !st) return st;
    core::Op undo;
    auto st = doc_.set_attribute(col, e, v, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_text(EntityId e, std::string content, core::Mm height,
                             core::TextAnchor anchor)
{
    if (auto st = may_change(e); !st) return st;
    core::Op undo;
    auto st = doc_.set_text(e, std::move(content), height, anchor, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_text(EntityId e, std::string content, core::Mm height,
                             core::TextAnchor anchor, core::TextLines lines)
{
    if (auto st = may_change(e); !st) return st;
    core::Op undo;
    auto st = doc_.set_text(e, std::move(content), height, anchor, lines, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

core::Result<core::AttrId> Transaction::declare_attribute(core::AttrSpec spec)
{
    return doc_.declare_attribute(std::move(spec));
}

core::Status Transaction::drop_attribute(std::string_view id)
{
    return doc_.drop_attribute(id);
}

core::Status Transaction::amend_attribute(std::string_view id, const core::AttrSpec& next)
{
    return doc_.amend_attribute(id, next);
}

Status Transaction::add_guide(core::GuideAxis axis, core::Mm coordinate)
{
    core::Op undo;
    auto st = doc_.add_guide(axis, coordinate, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::add_angled_guide(core::Point2 at, std::int64_t angle, bool ray)
{
    core::Op undo;
    auto st = doc_.add_angled_guide(at, angle, ray, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::add_guide_row(const core::GuideRow& row)
{
    return row.axis == core::GuideAxis::Angled ? add_angled_guide(row.through, row.angle, row.ray)
                                               : add_guide(row.axis, row.coordinate);
}

Status Transaction::set_layouts(std::vector<core::Layout> layouts)
{
    core::Op undo;
    auto st = doc_.set_layouts(std::move(layouts), undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::remove_guide(std::size_t index)
{
    core::Op undo;
    auto st = doc_.remove_guide(index, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_entity_hidden(EntityId e, bool hidden)
{
    core::Op undo;
    auto st = doc_.set_entity_hidden(e, hidden, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_crs(core::Crs crs)
{
    core::Op undo;
    auto st = doc_.set_crs(std::move(crs), undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_attachment(EntityId e, const core::Attachment& a)
{
    core::Op undo;
    auto st = doc_.set_attachment(e, &a, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_dimension_links(EntityId dim, std::span<const core::DimLink> links)
{
    core::Op undo;
    auto st = doc_.set_dimension_links(dim, links, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_lineage(EntityId e, core::Lineage origin)
{
    core::Op undo;
    auto st = doc_.set_lineage(e, std::move(origin), undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_hatch_links(EntityId hatch, std::span<const core::HatchSource> sources)
{
    core::Op undo;
    auto st = doc_.set_hatch_links(hatch, sources, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::clear_attachment(EntityId e)
{
    core::Op undo;
    auto st = doc_.set_attachment(e, nullptr, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

namespace {

/// One ring of a geometry slot, copied out so it can be compared with a later
/// slot of the same entity.
struct RingCopy
{
    std::vector<Point2> points;
    bool closed{false};
    bool ok{false};
};

RingCopy ring_copy(const RingGeometry& geom, std::uint32_t slot, std::uint16_t ring)
{
    RingCopy out;
    if (slot >= geom.slot_count()) return out;
    const core::RingSpan rs = geom.rings_of(slot);
    if (ring >= rs.count) return out;
    const std::uint32_t r = rs.first + ring;
    const auto xs         = geom.ring_xs(r);
    const auto ys         = geom.ring_ys(r);
    out.points.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i)
        out.points.push_back(Point2{xs[i], ys[i]});
    out.closed = geom.ring_role[r] != core::RingRole::Open;
    out.ok     = true;
    return out;
}

bool contains(const std::vector<EntityId>& sorted, EntityId e)
{
    return std::binary_search(sorted.begin(), sorted.end(), e);
}

} // namespace

void Transaction::settle_texts()
{
    const core::EntityTable& ents = doc_.entities();
    const RingGeometry& geom      = doc_.geometry();
    const core::TextTable& texts  = doc_.texts();

    std::vector<EntityId> touched;
    for (std::size_t i = texts_settled_upto_; i < inverse_.size(); ++i) {
        const Op& op = inverse_[i];
        if (op.kind == Op::Kind::SetText || op.kind == Op::Kind::SetGeometry ||
            op.kind == Op::Kind::SetKindGeometry)
            touched.push_back(op.entity);
    }
    std::ranges::sort(touched);
    touched.erase(std::ranges::unique(touched).begin(), touched.end());

    for (const EntityId e : touched) {
        if (e >= ents.size() || !doc_.alive(e) || ents.kind[e] != core::kPolylineKind) continue;
        const std::uint32_t slot = ents.slot[e];
        if (!texts.has(slot) || texts.lines(slot).wrap) continue;
        if (doc_.attachments().get(e) != nullptr) continue; // `settle_attachments` places it
        const core::RingSpan rs = geom.rings_of(slot);
        if (rs.count != 1 || geom.ring_count[rs.first] != 2 ||
            geom.ring_role[rs.first] != core::RingRole::Open)
            continue;
        const core::Point2 a = geom.vertex(rs.first, 0);
        const core::Point2 b = geom.vertex(rs.first, 1);
        const core::Mm width =
            std::max<core::Mm>(1, core::text_width(texts.text(slot), texts.height(slot)));
        if (std::llabs(core::text_baseline_length(a, b) - width) <= 1) continue;
        const auto base = core::text_baseline(a, b, width);
        const core::RingGeometry::RingInput ring{base, core::RingRole::Open, 0};
        (void)set_geometry(e, std::span<const core::RingGeometry::RingInput>(&ring, 1));
    }
    texts_settled_upto_ = inverse_.size();
}

Transaction::SettleReport Transaction::settle_attachments()
{
    SettleReport rep;
    const core::AttachTable& tab = doc_.attachments();
    if (tab.empty()) {
        settled_upto_ = inverse_.size();
        return rep;
    }
    const core::EntityTable& ents = doc_.entities();
    const RingGeometry& geom      = doc_.geometry();
    const core::TextTable& texts  = doc_.texts();

    // ROUNDS, because a dependent may itself be followed: what this round
    // re-places is what the next round reads as moved. A round that reads
    // nothing new ends it; the cap is a guard against a cycle the document
    // refused to store but a file might still carry.
    std::vector<EntityId> deps;
    std::vector<EntityId> stuck; ///< dependents counted in `left`, each once
    for (int round = 0; round < 16; ++round) {
        std::vector<EntityId> moved;
        std::vector<EntityId> erased;
        std::vector<EntityId> restated; // a column changed: a caption filled from it is stale
        std::map<EntityId, std::uint32_t> before; // the slot an entity had before this range
        for (std::size_t i = settled_upto_; i < inverse_.size(); ++i) {
            const Op& op = inverse_[i];
            if (op.kind == Op::Kind::SetGeometry || op.kind == Op::Kind::SetKindGeometry) {
                moved.push_back(op.entity);
                before.emplace(op.entity, op.geometry_slot); // the OLDEST wins
            } else if (op.kind == Op::Kind::SetEntityAlive && op.bool_arg) {
                // The inverse restores it, so the command erased it.
                erased.push_back(op.entity);
            } else if (op.kind == Op::Kind::SetAttribute) {
                restated.push_back(op.entity);
            }
        }
        settled_upto_ = inverse_.size();
        if (moved.empty() && erased.empty() && restated.empty()) break;
        std::sort(moved.begin(), moved.end());
        moved.erase(std::unique(moved.begin(), moved.end()), moved.end());
        std::sort(erased.begin(), erased.end());
        erased.erase(std::unique(erased.begin(), erased.end()), erased.end());
        // Every source whose captions may say something new: the moved ones, and
        // the ones whose columns a caption is filled from (TODOS C-12). A source
        // that only changed its columns is placed where it was, and only the
        // words of a `Fields` caption can come out different.
        std::ranges::sort(restated);
        restated.erase(std::ranges::unique(restated).begin(), restated.end());
        std::vector<EntityId> carrying;
        std::ranges::set_union(moved, restated, std::back_inserter(carrying));
        carrying.erase(std::ranges::unique(carrying).begin(), carrying.end());

        // ---- an erased source takes its dependents with it ----
        for (const EntityId src : erased) {
            if (src >= ents.size() || ents.alive(src)) continue; // restored again meanwhile
            tab.dependents_of(doc_.key_of(src), deps);
            for (const EntityId d : deps)
                if (doc_.alive(d) && erase_entity(d)) ++rep.erased;
        }

        // ---- a dependent moved BY HAND keeps that offset ----
        //
        // Its source did not move, so the rule's place is where it was and the
        // difference to where the caption now stands is what the user meant.
        // Measured in the rule's reading frame, so it survives the source
        // turning later.
        for (const EntityId d : moved) {
            const core::Attachment* a = tab.get(d);
            if (a == nullptr || !doc_.alive(d)) continue;
            const EntityId src = doc_.slot_of(a->source);
            if (src == core::kNoEntity || !doc_.alive(src)) continue;
            if (contains(moved, src) || contains(erased, src)) continue;
            const std::uint32_t dslot = ents.slot[d];
            if (!texts.has(dslot)) continue;
            const RingCopy ring = ring_copy(geom, ents.slot[src], a->ring);
            if (!ring.ok) continue;
            const auto rule =
                core::attach_place(ring.points, ring.closed, *a, texts.height(dslot), false);
            if (!rule) continue;
            const core::RingSpan rs = geom.rings_of(dslot);
            if (rs.count == 0 || geom.ring_count[rs.first] == 0) continue;
            core::Attachment next = *a;
            core::attach_measure_offset(*rule, geom.vertex(rs.first, 0), next);
            if (next != *a && set_attachment(d, next)) ++rep.reoffset;
        }

        // ---- a moved source carries its dependents ----
        for (const EntityId src : carrying) {
            if (!doc_.alive(src)) continue;
            tab.dependents_of(doc_.key_of(src), deps);
            if (deps.empty()) continue;
            const std::uint32_t now_slot = ents.slot[src];
            const auto was               = before.find(src);
            for (const EntityId d : deps) {
                if (!doc_.alive(d)) continue;
                const core::Attachment* stored = tab.get(d);
                if (stored == nullptr) continue;
                core::Attachment a = *stored;

                const RingCopy now = ring_copy(geom, now_slot, a.ring);
                if (!now.ok) continue;
                if (was != before.end() && was->second != now_slot) {
                    const RingCopy old = ring_copy(geom, was->second, a.ring);
                    if (old.ok && old.points.size() != now.points.size())
                        a = core::attach_reanchor(old.points, old.closed, now.points, now.closed,
                                                  a);
                }

                // A DEPENDENT THAT CANNOT BE EDITED stays where it is — a caption
                // on a locked layer — and is counted, so the command can say it
                // is now standing apart from what it describes (TODOS C-07). Its
                // TIE still names the same corner (TODOS F-04): a corner that came
                // or went renumbers it, so the day the layer is unlocked the
                // caption follows the corner it always did.
                if (!doc_.editable(d)) {
                    if (a != *stored) (void)retie_attachment(d, a);
                    // COUNTED ONLY WHEN IT IS NOW WRONG: the caption of an edge
                    // the move did not change still stands where its rule puts
                    // it, and saying it "could not follow" would be a false alarm.
                    const auto should       = core::caption_follow(doc_, d, a);
                    const core::RingSpan rs = geom.rings_of(ents.slot[d]);
                    const bool standing     = should && rs.count == 1 &&
                                          geom.ring_count[rs.first] == 2 &&
                                          geom.vertex(rs.first, 0) == should->base[0] &&
                                          geom.vertex(rs.first, 1) == should->base[1] &&
                                          should->text == texts.text(ents.slot[d]);
                    if (!standing && !contains(stuck, d)) {
                        stuck.push_back(d);
                        ++rep.left;
                    }
                    continue;
                }
                (void)place_caption(d, *stored, a, rep);
            }
        }
    }
    return rep;
}

bool Transaction::place_caption(EntityId d, const core::Attachment& stored,
                                const core::Attachment& a, SettleReport& rep)
{
    // THE RULE'S PLACE HAS ONE HOME (core/ties.hpp): what is written here is what
    // the check compares with, so a caption this put in place is current.
    const auto should = core::caption_follow(doc_, d, a);
    if (!should) return false;
    const core::TextTable& texts = doc_.texts();
    const RingGeometry& geom     = doc_.geometry();
    const std::uint32_t dslot    = doc_.entities().slot[d];

    // Nothing is written that is already so: a caption the command moved
    // together with its source is already where the rule puts it, and appending
    // an identical slot would be an edit that changed nothing but the file.
    const core::RingSpan rs = geom.rings_of(dslot);
    const bool same_place   = rs.count == 1 && geom.ring_count[rs.first] == 2 &&
                            geom.ring_role[rs.first] == core::RingRole::Open &&
                            geom.vertex(rs.first, 0) == should->base[0] &&
                            geom.vertex(rs.first, 1) == should->base[1];
    const bool same_text   = should->text == texts.text(dslot);
    const bool same_anchor = should->anchor == texts.anchor(dslot);

    if (a != stored && !set_attachment(d, a)) return false;
    if (!same_text || !same_anchor) {
        if (!set_text(d, should->text, should->height, should->anchor)) return false;
        if (!same_text) ++rep.relabelled;
    }
    if (!same_place) {
        const std::vector<core::Point2> base{should->base[0], should->base[1]};
        const core::RingGeometry::RingInput ring{base, core::RingRole::Open, 0};
        if (set_geometry(d, std::span<const core::RingGeometry::RingInput>(&ring, 1)))
            ++rep.followed;
    }
    return true;
}

Status Transaction::retie_attachment(EntityId e, const core::Attachment& a)
{
    core::Op undo;
    auto st = doc_.retie_attachment(e, a, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::retie_dimension(EntityId dim, std::span<const core::DimLink> links)
{
    core::Op undo;
    auto st = doc_.retie_dimension(dim, links, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::retie_hatch(EntityId hatch, std::span<const core::HatchSource> sources)
{
    core::Op undo;
    auto st = doc_.retie_hatch(hatch, sources, undo);
    if (!st) return st;
    if (undo.kind != core::Op::Kind::None) inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::follow_caption(EntityId d, SettleReport& rep)
{
    const core::Attachment* stored = doc_.attachments().get(d);
    if (stored == nullptr)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Nesne " + std::to_string(core::raw(doc_.key_of(d))) +
                             " bir şeyi izleyen bir yazı değil.");
    if (auto st = doc_.editable(d); !st) return st;
    const core::Attachment a = *stored; // a copy: the write below may move the table
    if (!place_caption(d, a, a, rep))
        return core::err(core::ErrorCode::InvalidArgument,
                         "Yazı " + std::to_string(core::raw(doc_.key_of(d))) +
                             " kaynağına yerleştirilemedi: bağlı olduğu kenar ya da köşe artık "
                             "yok. BAĞIMLILIK islem=coz ile bağından çözün ya da BAĞLA ile "
                             "yeniden bağlayın.");
    return core::ok();
}

namespace {

/// Whether two rings hold the same corners, in any order: reversed, or begun at
/// another corner. Such a ring moved nothing, only renumbered.
bool same_corners(std::span<const core::Mm> ax, std::span<const core::Mm> ay,
                  std::span<const core::Mm> bx, std::span<const core::Mm> by)
{
    if (ax.size() != bx.size()) return false;
    std::vector<std::pair<core::Mm, core::Mm>> a;
    std::vector<std::pair<core::Mm, core::Mm>> b;
    a.reserve(ax.size());
    b.reserve(bx.size());
    for (std::size_t i = 0; i < ax.size(); ++i) {
        a.emplace_back(ax[i], ay[i]);
        b.emplace_back(bx[i], by[i]);
    }
    std::ranges::sort(a);
    std::ranges::sort(b);
    return a == b;
}

} // namespace

Transaction::SettleReport Transaction::settle_dimensions(core::DrawingUnit unit, core::Mm tolerance)
{
    SettleReport rep;
    const core::DimLinkTable& table = doc_.dimension_links();
    if (table.empty()) {
        dims_settled_upto_ = inverse_.size();
        return rep;
    }
    // WHAT THIS RANGE MOVED, ERASED OR CREATED, read from the inverse record
    // the way `settle_attachments` reads it — but from this function's own
    // cursor, so a dimension it re-lays out below is not read back later as one
    // the user moved (which would release its links). A moved entity's FIRST
    // inverse in the range names the geometry the range found it with: the
    // arena keeps those rings, and they say which corner a link meant.
    std::vector<EntityId> moved;
    std::vector<EntityId> erased;
    std::vector<EntityId> created;
    std::map<EntityId, std::uint32_t> found_as;
    for (std::size_t i = dims_settled_upto_; i < inverse_.size(); ++i) {
        const Op& op = inverse_[i];
        if (op.kind == Op::Kind::SetGeometry || op.kind == Op::Kind::SetKindGeometry) {
            moved.push_back(op.entity);
            found_as.try_emplace(op.entity, op.geometry_slot);
        } else if (op.kind == Op::Kind::SetEntityAlive) {
            (op.bool_arg ? erased : created).push_back(op.entity);
        }
    }
    dims_settled_upto_ = inverse_.size();
    if (moved.empty() && erased.empty()) return rep;
    for (std::vector<EntityId>* list : {&moved, &erased, &created}) {
        std::ranges::sort(*list);
        list->erase(std::ranges::unique(*list).begin(), list->end());
    }

    // WHO MAY INHERIT A LINK: what this range created or reshaped. UÇUCA keeps
    // the first line and erases the rest, PATLAT erases the line and creates
    // its pieces, BİRLEŞTİR reshapes one parcel into the union — in every case
    // the object that now holds the corner was touched by the same command. An
    // object the command left alone never inherits, so erasing a parcel does
    // not tie its dimension to the neighbour that shares the corner.
    std::vector<EntityId> heirs;
    std::ranges::set_union(created, moved, std::back_inserter(heirs));

    const core::EntityTable& ents = doc_.entities();
    const RingGeometry& geom      = doc_.geometry();

    // WHICH CORNER, NOT WHICH NUMBER. A link names a vertex by its index, and an
    // index is only a corner while the ring keeps its corners: KÖŞEEKLE before
    // it, KÖŞESİL of another, a reversed line all renumber it without moving it.
    // Nothing when the corner it measured is gone.
    enum class Corner : std::uint8_t { Kept, Renumbered, Gone };
    const auto renumber = [&](core::DimLink& l, EntityId src) -> Corner {
        const auto it = found_as.find(src);
        if (l.anchor != core::DimAnchor::Vertex || it == found_as.end()) return Corner::Kept;
        const core::RingSpan was = geom.rings_of(it->second);
        const core::RingSpan is  = geom.rings_of(ents.slot[src]);
        if (l.ring >= was.count || l.ring >= is.count) return Corner::Gone;
        const auto ox = geom.ring_xs(was.first + l.ring);
        const auto oy = geom.ring_ys(was.first + l.ring);
        const auto nx = geom.ring_xs(is.first + l.ring);
        const auto ny = geom.ring_ys(is.first + l.ring);
        if (l.index >= ox.size()) return Corner::Gone;
        const core::Point2 corner{ox[l.index], oy[l.index]};
        if (ox.size() == nx.size()) {
            // As many corners as before: the number still names the corner that
            // moved — unless the ring is the same corners in another order.
            if (!same_corners(ox, oy, nx, ny)) return Corner::Kept;
            for (std::size_t v = 0; v < nx.size(); ++v)
                if (nx[v] == corner.x && ny[v] == corner.y) {
                    if (v == l.index) return Corner::Kept;
                    l.index = static_cast<std::uint32_t>(v);
                    return Corner::Renumbered;
                }
            return Corner::Kept;
        }
        // Corners came or went: the one measured is the one where it was, or
        // within the node tolerance of it (a repeated corner cleaned away).
        std::size_t best   = nx.size();
        double best_d2     = 0.0;
        const double limit = static_cast<double>(tolerance) * static_cast<double>(tolerance);
        for (std::size_t v = 0; v < nx.size(); ++v) {
            const auto dx   = static_cast<double>(nx[v] - corner.x);
            const auto dy   = static_cast<double>(ny[v] - corner.y);
            const double d2 = dx * dx + dy * dy;
            if (d2 <= limit && (best == nx.size() || d2 < best_d2)) {
                best    = v;
                best_d2 = d2;
            }
        }
        if (best == nx.size()) return Corner::Gone;
        l.index = static_cast<std::uint32_t>(best);
        return Corner::Renumbered;
    };

    for (const EntityId dim : table.linked()) {
        if (!doc_.alive(dim)) continue;
        const std::vector<core::DimLink>* stored = table.get(dim);
        if (stored == nullptr) continue;
        const std::vector<core::DimLink> was = *stored;
        const bool dim_moved                 = contains(moved, dim);
        bool touched                         = dim_moved;
        for (const core::DimLink& l : was) {
            const EntityId src = doc_.slot_of(l.source);
            touched =
                touched || (src != core::kNoEntity &&
                            (contains(moved, src) || contains(erased, src) || !doc_.alive(src)));
        }
        if (!touched) continue;

        const core::RingSpan span = geom.rings_of(ents.slot[dim]);
        if (span.count < 2) continue;
        const std::uint32_t def_ring = span.first + 1;
        auto decoded                 = core::dimension_of(geom, ents.slot[dim]);
        if (!decoded) continue;
        const std::vector<std::optional<core::DimRole>> roles =
            core::dim_roles(decoded.value().type);

        std::vector<core::DimLink> now;
        std::vector<std::pair<std::size_t, core::Point2>> moves;
        for (const core::DimLink& l : was) {
            if (l.broken) {
                now.push_back(l);
                continue;
            }
            if (l.point >= geom.ring_count[def_ring]) {
                core::DimLink b = l;
                b.broken        = true;
                now.push_back(b);
                ++rep.dims_broken;
                continue;
            }
            const core::Point2 def = geom.vertex(def_ring, l.point);
            const EntityId src     = doc_.slot_of(l.source);
            if (src == core::kNoEntity || !doc_.alive(src)) {
                // REPLACED, NOT ERASED: the line UÇUCA joins, the pieces PATLAT
                // leaves and the union BİRLEŞTİR makes erase what they were made
                // from and hold the same corner. The link goes to the object the same command made
                // or reshaped at the same point (`heirs`).
                if (!heirs.empty() && l.point < roles.size() && roles[l.point]) {
                    if (auto next = core::dim_anchor_at(doc_, def, *roles[l.point], dim, heirs)) {
                        next->point = l.point;
                        now.push_back(*next);
                        ++rep.dims_relinked;
                        continue;
                    }
                }
                // BROKEN, NOT DROPPED: the dimension stays where it was and the
                // link remembers what it measured, so the canvas and a query
                // can say it no longer measures anything.
                core::DimLink b = l;
                b.broken        = true;
                now.push_back(b);
                ++rep.dims_broken;
                continue;
            }
            core::DimLink link = l;
            if (renumber(link, src) == Corner::Gone) {
                link.broken = true;
                now.push_back(link);
                ++rep.dims_cornerless;
                continue;
            }
            const auto at = core::dim_anchor_point(doc_, link);
            if (!at) {
                link.broken = true;
                now.push_back(link);
                ++rep.dims_cornerless;
                continue;
            }
            if (*at == def) {
                now.push_back(link);
                continue;
            }
            if (contains(moved, src) || !dim_moved) {
                moves.emplace_back(l.point, *at);
                now.push_back(link);
            } else {
                // The dimension's own point was moved off its feature and the
                // feature stood still: the user said the point is elsewhere now.
                ++rep.dims_released;
            }
        }

        if (!moves.empty()) {
            if (!doc_.editable(dim)) {
                ++rep.dims_left;
                // ITS TIE STILL NAMES THE CORNER IT MEASURED (TODOS F-04):
                // renumbered, or broken, behind the lock — the dimension itself
                // stays where it is.
                if (now != was) (void)retie_dimension(dim, now);
                continue;
            }
            (void)rebuild_dimension(dim, moves, unit, rep);
        }
        if (now != was) {
            if (doc_.editable(dim))
                (void)set_dimension_links(dim, now);
            else
                (void)retie_dimension(dim, now);
        }
    }
    // The writes above are this function's own: not a dimension the user moved.
    dims_settled_upto_ = inverse_.size();
    return rep;
}

bool Transaction::rebuild_dimension(EntityId dim,
                                    std::span<const std::pair<std::size_t, core::Point2>> moves,
                                    core::DrawingUnit unit, SettleReport& rep)
{
    const std::uint32_t slot = doc_.entities().slot[dim];
    auto decoded             = core::dimension_of(doc_.geometry(), slot);
    if (!decoded) {
        ++rep.dims_left;
        return false;
    }
    auto rebuilt = core::dimension_follow(doc_, dim, moves, unit);
    if (!rebuilt) {
        ++rep.dims_left;
        return false;
    }
    const core::DimensionRebuild& r = rebuilt.value();
    const std::array<core::RingGeometry::RingInput, 2> rings{
        core::RingGeometry::RingInput{r.baseline, core::RingRole::Open, 0},
        core::RingGeometry::RingInput{r.defs, core::RingRole::Open, 0}};
    const std::int64_t measured_before = decoded.value().measurement;
    if (!set_kind_geometry(dim, rings, r.payload)) return false;
    if (r.text != doc_.texts().text(doc_.entities().slot[dim]))
        (void)set_text(dim, r.text, r.text_height, core::TextAnchor::MiddleCentre);
    ++rep.dims_followed;
    // A FIGURE TYPED BY HAND DOES NOT FOLLOW, and that is said: the side is
    // 13,60 now and the sheet still says 12,50.
    if (core::dimension_text_is_manual(r.def) && r.def.measurement != measured_before)
        ++rep.dims_manual;
    return true;
}

Status Transaction::follow_dimension(EntityId dim, core::DrawingUnit unit, SettleReport& rep)
{
    const std::vector<core::DimLink>* links = doc_.dimension_links().get(dim);
    if (links == nullptr)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Nesne " + std::to_string(core::raw(doc_.key_of(dim))) +
                             " bir şeye bağlı bir ölçü değil.");
    if (auto st = doc_.editable(dim); !st) return st;
    const RingGeometry& geom  = doc_.geometry();
    const core::RingSpan span = geom.rings_of(doc_.entities().slot[dim]);
    if (span.count < 2) return core::ok();
    // EVERY POINT BACK ON ITS FEATURE: what its links name, where it is now.
    std::vector<std::pair<std::size_t, core::Point2>> moves;
    for (const core::DimLink& l : *links) {
        if (l.broken || l.point >= geom.ring_count[span.first + 1]) continue;
        const auto at = core::dim_anchor_point(doc_, l);
        if (at && *at != geom.vertex(span.first + 1, l.point)) moves.emplace_back(l.point, *at);
    }
    if (moves.empty()) return core::ok();
    if (!rebuild_dimension(dim, moves, unit, rep))
        return core::err(core::ErrorCode::InvalidArgument,
                         "Ölçü " + std::to_string(core::raw(doc_.key_of(dim))) +
                             " ölçtüğü noktalara yeniden kurulamadı.");
    return core::ok();
}

Status Transaction::follow_hatch(EntityId hatch, SettleReport& rep)
{
    const std::vector<core::HatchSource>* sources = doc_.hatch_links().get(hatch);
    if (sources == nullptr)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Nesne " + std::to_string(core::raw(doc_.key_of(hatch))) +
                             " sınırına bağlı bir tarama değil.");
    if (auto st = doc_.editable(hatch); !st) return st;
    std::vector<EntityId> live;
    for (const core::HatchSource& s : *sources) {
        const EntityId src = doc_.slot_of(s.source);
        if (s.broken || src == core::kNoEntity || !doc_.alive(src) ||
            core::closed_loops_of(doc_, src).empty())
            return core::err(core::ErrorCode::InvalidArgument,
                             "Tarama " + std::to_string(core::raw(doc_.key_of(hatch))) +
                                 " sınırının bir parçasını kaybetmiş; kalanlardan kurulmaz. "
                                 "TARAMADÜZENLE ile yeni sınır verin ya da BAĞIMLILIK islem=coz "
                                 "ile bağından çözün.");
        live.push_back(src);
    }
    if (core::hatch_fills(doc_, hatch, *sources)) return core::ok();
    const std::uint32_t slot = doc_.entities().slot[hatch];
    auto def                 = core::hatch_of(doc_.geometry(), slot);
    if (!def) return def.error();
    auto boundary = core::hatch_boundary(doc_, live, def.value().style);
    if (!boundary) return boundary.error();
    const auto rings = boundary.value().rings();
    if (auto st = set_kind_geometry(hatch, rings, core::encode_hatch(def.value())); !st)
        return st.error();
    ++rep.hatches_followed;
    return core::ok();
}

namespace {

/// The one offset every vertex of `e` moved by since `slot` held it, or
/// nothing when it did not move as a whole (a corner moved, a ring grew).
std::optional<core::Point2> shift_of(const Document& doc, EntityId e, std::uint32_t slot)
{
    const RingGeometry& g    = doc.geometry();
    const core::RingSpan was = g.rings_of(slot);
    const core::RingSpan is  = g.rings_of(doc.entities().slot[e]);
    if (was.count != is.count || was.count == 0) return std::nullopt;
    std::optional<core::Point2> by;
    for (std::uint32_t r = 0; r < was.count; ++r) {
        const auto ox = g.ring_xs(was.first + r);
        const auto oy = g.ring_ys(was.first + r);
        const auto nx = g.ring_xs(is.first + r);
        const auto ny = g.ring_ys(is.first + r);
        if (ox.size() != nx.size()) return std::nullopt;
        for (std::size_t v = 0; v < ox.size(); ++v) {
            const core::Point2 d{nx[v] - ox[v], ny[v] - oy[v]};
            if (!by) by = d;
            if (*by != d) return std::nullopt;
        }
    }
    return by;
}

} // namespace

bool Transaction::fills_boundary(EntityId hatch, std::span<const core::HatchSource> sources) const
{
    return core::hatch_fills(doc_, hatch, sources);
}

Transaction::SettleReport Transaction::settle_hatches()
{
    SettleReport rep;
    const core::HatchLinkTable& table = doc_.hatch_links();
    if (table.empty()) {
        hatches_settled_upto_ = inverse_.size();
        return rep;
    }
    std::vector<EntityId> moved;
    std::vector<EntityId> erased;
    std::map<EntityId, std::uint32_t> found_as;
    for (std::size_t i = hatches_settled_upto_; i < inverse_.size(); ++i) {
        const Op& op = inverse_[i];
        if (op.kind == Op::Kind::SetGeometry || op.kind == Op::Kind::SetKindGeometry) {
            moved.push_back(op.entity);
            found_as.try_emplace(op.entity, op.geometry_slot);
        } else if (op.kind == Op::Kind::SetEntityAlive && op.bool_arg) {
            erased.push_back(op.entity);
        }
    }
    hatches_settled_upto_ = inverse_.size();
    if (moved.empty() && erased.empty()) return rep;
    for (std::vector<EntityId>* list : {&moved, &erased}) {
        std::ranges::sort(*list);
        list->erase(std::ranges::unique(*list).begin(), list->end());
    }

    for (const EntityId hatch : table.linked()) {
        if (!doc_.alive(hatch)) continue;
        const std::vector<core::HatchSource>* stored = table.get(hatch);
        if (stored == nullptr) continue;
        const std::vector<core::HatchSource> was = *stored;
        bool touched                             = false;
        for (const core::HatchSource& s : was) {
            if (s.broken) continue;
            const EntityId src = doc_.slot_of(s.source);
            touched            = touched || src == core::kNoEntity || !doc_.alive(src) ||
                      contains(moved, src) || contains(erased, src);
        }
        if (!touched) {
            // THE HATCH MOVED ON ITS OWN: it no longer fills its boundary, which
            // is the user saying it is a hatch of its own now. Reshaped to what
            // its boundary gives — an island rule changed, TARAMADÜZENLE — it
            // still fills it, and stays tied.
            if (contains(moved, hatch) && doc_.editable(hatch) && !fills_boundary(hatch, was) &&
                set_hatch_links(hatch, std::span<const core::HatchSource>{}))
                ++rep.hatches_released;
            continue;
        }

        // ONE BOUNDARY GONE AND THE HATCH STOPS FOLLOWING, all of it. Built from
        // what is left, a parcel erased from under its hatch would leave the
        // pool inside it as the only boundary — and the pool, a hole a moment
        // ago, would be what is filled. It stays as it was, and says so.
        bool broken = std::ranges::any_of(was, [](const core::HatchSource& s) { return s.broken; });
        std::vector<core::HatchSource> now = was;
        std::vector<EntityId> live;
        for (core::HatchSource& s : now) {
            if (s.broken) continue;
            const EntityId src = doc_.slot_of(s.source);
            if (src == core::kNoEntity || !doc_.alive(src)) {
                s.broken = true;
                broken   = true;
                ++rep.hatches_broken;
                continue;
            }
            if (core::closed_loops_of(doc_, src).empty()) {
                s.broken = true;
                broken   = true;
                ++rep.hatches_open;
                continue;
            }
            live.push_back(src);
        }
        if (!broken && !live.empty()) {
            if (!doc_.editable(hatch)) {
                ++rep.hatches_left;
                continue;
            }
            const std::uint32_t slot = doc_.entities().slot[hatch];
            auto def                 = core::hatch_of(doc_.geometry(), slot);
            auto boundary            = def ? core::hatch_boundary(doc_, live, def.value().style)
                                           : core::Result<core::HatchBoundary>(def.error());
            if (def && boundary) {
                // CARRIED ALONG when the whole boundary moved as one: the pattern
                // stays where it was on the parcel, not where it was on the sheet.
                std::optional<core::Point2> shift;
                bool whole = true;
                for (const EntityId src : live) {
                    const auto at = found_as.find(src);
                    if (at == found_as.end()) {
                        whole = false;
                        break;
                    }
                    const auto by = shift_of(doc_, src, at->second);
                    if (!by || (shift && *shift != *by)) {
                        whole = false;
                        break;
                    }
                    shift = by;
                }
                // A hatch the command moved itself already carries its origin
                // along (TAŞI moves the payload's point with the rings).
                core::HatchDef next = def.value();
                if (whole && shift && !contains(moved, hatch)) next.origin = next.origin + *shift;
                const auto rings = boundary.value().rings();
                if (set_kind_geometry(hatch, rings, core::encode_hatch(next))) {
                    ++rep.hatches_followed;
                    // THE PATTERN'S PHASE IS THE ORIGIN'S (drawing_catalogs.hpp):
                    // an origin carried along is a symbol drawn again.
                    if (next.origin != def.value().origin) {
                        const core::StyleId st = doc_.entities().style[hatch];
                        std::uint32_t ink      = 0xFF000000u;
                        if (st != core::kByLayerStyle && st < doc_.styles().size())
                            ink = doc_.styles().symbol_at(st).primary().rgba;
                        (void)set_entity_style(hatch, intern_symbol(hatch_symbol(next, ink)));
                    }
                }
            }
        }
        if (now != was) {
            // A LOCKED HATCH'S TIE is broken behind the lock too (TODOS F-04):
            // the hatch stays as it was, and says it follows nothing more.
            if (doc_.editable(hatch))
                (void)set_hatch_links(hatch, now);
            else
                (void)retie_hatch(hatch, now);
        }
    }
    // The writes above are this function's own: not a hatch the user moved.
    hatches_settled_upto_ = inverse_.size();
    return rep;
}

void Transaction::rollback()
{
    rollback_to(0);
}

std::vector<EntityId> Transaction::created_since(std::size_t mark) const
{
    std::vector<EntityId> out;
    for (std::size_t i = mark; i < inverse_.size(); ++i)
        if (inverse_[i].kind == Op::Kind::SetEntityAlive && !inverse_[i].bool_arg &&
            std::ranges::find(out, inverse_[i].entity) == out.end())
            out.push_back(inverse_[i].entity);
    return out;
}

Transaction::SettleReport Transaction::settle_unlocked(core::DrawingUnit unit)
{
    SettleReport rep;
    const std::size_t from = std::min(unlocked_settled_upto_, inverse_.size());
    unlocked_settled_upto_ = inverse_.size();

    // WHAT THIS RANGE SET FREE: a layer unlocked, an object moved to another
    // layer. A follower that could not follow while it was locked may now.
    std::vector<core::LayerId> freed_layers;
    std::vector<EntityId> relayered;
    for (std::size_t i = from; i < inverse_.size(); ++i) {
        const Op& op = inverse_[i];
        if (op.kind == Op::Kind::SetLayerLocked && op.bool_arg) freed_layers.push_back(op.layer);
        if (op.kind == Op::Kind::SetEntityLayer) relayered.push_back(op.entity);
    }
    if (freed_layers.empty() && relayered.empty()) return rep;
    std::ranges::sort(freed_layers);
    std::ranges::sort(relayered);
    const core::EntityTable& ents = doc_.entities();
    const auto freed              = [&](EntityId e) {
        if (!doc_.alive(e) || !doc_.editable(e)) return false;
        return contains(relayered, e) || std::ranges::binary_search(freed_layers, ents.layer[e]);
    };
    const auto behind = [this](EntityId e, core::TieKind kind) {
        for (const core::Tie& t : core::ties_of(doc_, e))
            if (t.kind == kind) return t.state == core::TieState::Behind;
        return false;
    };

    // CAUGHT UP, each the way the settle would have placed it: the rule's place
    // for a caption, the features for a dimension, the boundary for a hatch.
    // What they write is counted once here, not again as a follow.
    SettleReport quiet;
    for (const EntityId e : doc_.attachments().attached())
        if (freed(e) && behind(e, core::TieKind::Caption) && follow_caption(e, quiet))
            ++rep.caught_up;
    for (const EntityId e : doc_.dimension_links().linked())
        if (freed(e) && behind(e, core::TieKind::Dimension) && follow_dimension(e, unit, quiet))
            ++rep.caught_up;
    for (const EntityId e : doc_.hatch_links().linked())
        if (freed(e) && behind(e, core::TieKind::Hatch) && follow_hatch(e, quiet)) ++rep.caught_up;

    // These writes are this function's own: no other settle may read a caught-up
    // caption as one the user moved by hand, or a dimension as one pulled off
    // its feature.
    for (std::size_t* cursor : {&settled_upto_, &dims_settled_upto_, &hatches_settled_upto_,
                                &texts_settled_upto_, &unlocked_settled_upto_})
        *cursor = inverse_.size();
    return rep;
}

Transaction::SettleReport Transaction::settle_results()
{
    SettleReport rep;
    const core::LineageTable& table = doc_.lineage();
    const std::size_t from          = std::min(results_settled_upto_, inverse_.size());
    if (table.empty()) {
        results_settled_upto_ = inverse_.size();
        return rep;
    }

    // WHAT THIS RANGE CHANGED about an object's content: its shape, its
    // words, a value in its row, or its being there at all — and what it
    // made, which nothing has had a chance to change under yet.
    std::vector<EntityId> touched;
    std::vector<EntityId> reshaped; // shape or words: what a result itself is
    std::vector<EntityId> created;
    std::vector<EntityId> recomputed; // a result whose origin this range wrote
    for (std::size_t i = from; i < inverse_.size(); ++i) {
        const Op& op = inverse_[i];
        if (op.kind == Op::Kind::SetLineage) recomputed.push_back(op.entity);
        const bool shape = op.kind == Op::Kind::SetGeometry ||
                           op.kind == Op::Kind::SetKindGeometry || op.kind == Op::Kind::SetText;
        if (shape) reshaped.push_back(op.entity);
        if (shape || op.kind == Op::Kind::SetAttribute ||
            (op.kind == Op::Kind::SetEntityAlive && op.bool_arg))
            touched.push_back(op.entity);
        if (op.kind == Op::Kind::SetEntityAlive && !op.bool_arg) created.push_back(op.entity);
    }
    for (std::vector<EntityId>* list : {&touched, &reshaped, &created, &recomputed}) {
        std::ranges::sort(*list);
        list->erase(std::ranges::unique(*list).begin(), list->end());
    }

    // ---- a result that was itself reshaped ----
    //
    // CARRIED WITH ITS SOURCES — a move, a turn of the well and its buffer
    // together — it still says what they say, and is recorded against them as
    // they are now. RESHAPED ON ITS OWN, it is no longer what was computed: the
    // user has made it theirs, and it is released — its history kept, the
    // claim dropped — the way a dimension dragged off its feature and a hatch
    // moved away from its boundary are (core/dimension_link.hpp, hatch_link.hpp).
    std::vector<EntityId> answered;
    for (const EntityId e : reshaped) {
        // RECOMPUTED, NOT RESHAPED BY HAND: a result whose shape and origin the
        // same range wrote was computed again (BAĞIMLILIK islem=yenile).
        if (contains(created, e) || contains(recomputed, e) || !doc_.alive(e)) continue;
        const core::Lineage* origin = table.get(e);
        if (origin == nullptr || !origin->result()) continue;
        // A COPY: the write below may grow the table and move what `origin` points at.
        const core::Lineage was = *origin;
        core::Lineage next      = was;
        bool carried            = false;
        bool left_behind        = false;
        for (std::size_t i = 0; i < next.sources.size(); ++i) {
            const EntityId src = doc_.slot_of(next.sources[i]);
            if (src == core::kNoEntity || !doc_.alive(src)) continue;
            if (!contains(reshaped, src)) {
                left_behind = true;
                break;
            }
            next.revisions[i] = doc_.content_revision(src);
            carried           = true;
        }
        if (carried && !left_behind) {
            if (next != was && set_lineage(e, std::move(next))) ++rep.results_carried;
        } else {
            next.revisions.clear();
            if (set_lineage(e, std::move(next))) {
                ++rep.results_released;
                rep.released_by.push_back(was.operation);
            }
        }
        answered.push_back(e);
    }

    // ---- the results made from what changed, and only those ----
    std::vector<EntityId> results;
    std::vector<EntityId> made;
    for (const EntityId e : touched) {
        if (e >= doc_.entities().size()) continue;
        table.made_from(doc_.key_of(e), made);
        results.insert(results.end(), made.begin(), made.end());
    }
    std::ranges::sort(results);
    results.erase(std::ranges::unique(results).begin(), results.end());

    std::map<std::uint32_t, core::ResultState> asked; // one answer per shared origin
    for (const EntityId r : results) {
        if (!doc_.alive(r) || contains(created, r) || contains(answered, r)) continue;
        const std::uint32_t at = table.origin_of(r);
        if (at == core::kNoOrigin || !table.origin(at).result()) continue;
        auto it = asked.find(at);
        if (it == asked.end())
            it = asked.emplace(at, core::check_origin(doc_, table.origin(at)).state).first;
        if (it->second == core::ResultState::Stale)
            ++rep.results_stale;
        else if (it->second == core::ResultState::Sourceless)
            ++rep.results_sourceless;
        else
            continue;
        rep.stale_by.push_back(table.origin(at).operation);
    }
    rep.results_asked = asked.size();
    for (std::vector<std::string>* list : {&rep.stale_by, &rep.released_by}) {
        std::ranges::sort(*list);
        list->erase(std::ranges::unique(*list).begin(), list->end());
    }
    // Past the lineage writes above too: they are this settle's own answer.
    results_settled_upto_ = inverse_.size();
    return rep;
}

void Transaction::rollback_to(std::size_t mark)
{
    // Newest first: the inverse of a sequence is the reversed sequence of inverses.
    while (inverse_.size() > mark) {
        (void)doc_.apply(inverse_.back());
        inverse_.pop_back();
    }
    // A cursor past what is left would pass over the next command's first ops.
    for (std::size_t* cursor :
         {&settled_upto_, &dims_settled_upto_, &hatches_settled_upto_, &texts_settled_upto_,
          &results_settled_upto_, &unlocked_settled_upto_})
        *cursor = std::min(*cursor, inverse_.size());
}

std::vector<core::Op> Transaction::release()
{
    return std::move(inverse_);
}

void UndoStack::push(UndoEntry e)
{
    if (e.inverse.empty()) return; // a no-op command never occupies an undo step
    undo_.push_back(std::move(e));
    redo_.clear(); // a new edit invalidates the redo branch
}

Status UndoStack::undo(Document& doc, std::string* label_out)
{
    if (undo_.empty()) return core::err(core::ErrorCode::NotFound, "Geri alınacak işlem yok");

    UndoEntry entry = std::move(undo_.back());
    undo_.pop_back();

    UndoEntry redo_entry;
    redo_entry.label = entry.label;
    redo_entry.inverse.reserve(entry.inverse.size());

    for (auto it = entry.inverse.rbegin(); it != entry.inverse.rend(); ++it) {
        core::Op back;
        auto st = doc.apply(*it, &back);
        if (!st) return st;
        redo_entry.inverse.push_back(std::move(back));
    }
    std::reverse(redo_entry.inverse.begin(), redo_entry.inverse.end());

    if (label_out) *label_out = entry.label;
    redo_.push_back(std::move(redo_entry));
    return core::ok();
}

Status UndoStack::redo(Document& doc, std::string* label_out)
{
    if (redo_.empty()) return core::err(core::ErrorCode::NotFound, "Yinelenecek işlem yok");

    UndoEntry entry = std::move(redo_.back());
    redo_.pop_back();

    UndoEntry undo_entry;
    undo_entry.label = entry.label;
    undo_entry.inverse.reserve(entry.inverse.size());

    for (auto it = entry.inverse.rbegin(); it != entry.inverse.rend(); ++it) {
        core::Op back;
        auto st = doc.apply(*it, &back);
        if (!st) return st;
        undo_entry.inverse.push_back(std::move(back));
    }
    std::reverse(undo_entry.inverse.begin(), undo_entry.inverse.end());

    if (label_out) *label_out = entry.label;
    undo_.push_back(std::move(undo_entry));
    return core::ok();
}

void UndoStack::clear()
{
    undo_.clear();
    redo_.clear();
}

core::Result<Transaction::AdoptSummary>
Transaction::adopt_from(const core::Document& scratch, std::span<const core::EntityKey> only,
                        std::span<const core::BlockId> blocks, core::BlockId into)
{
    AdoptOptions options;
    options.only   = only;
    options.blocks = blocks;
    options.into   = into;
    return adopt_from(scratch, options);
}

core::Result<Transaction::AdoptSummary> Transaction::adopt_from(const core::Document& scratch,
                                                                const AdoptOptions& options)
{
    const std::span<const core::EntityKey> only = options.only;
    const std::span<const core::BlockId> blocks = options.blocks;
    const core::BlockId into                    = options.into;
    const std::string& prefix                   = options.prefix;
    const bool external                         = !prefix.empty();
    // "ALTLIK|" names the reference "ALTLIK": the group its layers go under.
    const std::string reference_group = external ? prefix.substr(0, prefix.size() - 1) : "";
    const auto named                  = [&prefix, external](const std::string& theirs) {
        return external && theirs != "0" ? prefix + theirs : theirs;
    };

    // WHICH ENTITIES, resolved once into a slot set rather than searched per
    // entity: a clipboard copy of five hundred parcels would otherwise be a
    // linear scan five hundred times over.
    std::vector<bool> wanted;
    if (!only.empty() || !blocks.empty()) {
        wanted.assign(scratch.entities().size(), false);
        for (const core::EntityKey k : only) {
            const core::EntityId e = scratch.slot_of(k);
            if (e != core::kNoEntity && e < wanted.size()) wanted[e] = true;
        }
    }

    using core::Appearance;
    using core::kByLayerStyle;
    AdoptSummary summary;
    const auto note = [&summary](std::string text) {
        if (summary.notes.size() < 8) summary.notes.push_back(std::move(text));
    };

    // ---- dash patterns and pictures the styles reach for ----
    //
    // Slot 0 of both stores is the sentinel every fresh document holds — the
    // solid dash, "no picture" — and is never interned; it maps to itself.
    std::vector<std::uint16_t> dash_map(scratch.dashes().size(), core::kSolidDash);
    for (std::size_t d = 1; d < scratch.dashes().size(); ++d) {
        auto id = intern_dash(scratch.dashes().at(static_cast<core::DashId>(d)),
                              scratch.dashes().origin(static_cast<core::DashId>(d)));
        if (!id) return id.error();
        dash_map[d] = static_cast<std::uint16_t>(id.value());
    }
    std::vector<core::ImageId> image_map(scratch.images().size(), core::kNoImage);
    for (std::size_t i = 1; i < scratch.images().size(); ++i) {
        auto id = intern_image(scratch.images().bytes(static_cast<core::ImageId>(i)),
                               scratch.images().origin(static_cast<core::ImageId>(i)));
        if (!id) return id.error();
        image_map[i] = id.value();
    }
    const auto remap_look = [&dash_map](Appearance a) {
        if (a.dash != 0 && a.dash < dash_map.size()) a.dash = dash_map[a.dash];
        return a;
    };

    // ---- styles, interned on first use ----
    std::vector<StyleId> style_map(scratch.styles().size(), kByLayerStyle);
    std::vector<bool> style_done(scratch.styles().size(), false);
    const auto own_style = [&](StyleId theirs) -> StyleId {
        if (theirs == kByLayerStyle || theirs >= scratch.styles().size()) return kByLayerStyle;
        if (!style_done[theirs]) {
            core::Symbol sym = scratch.styles().symbol_at(theirs);
            for (core::SymbolLayer& layer : sym.layers) {
                layer.look = remap_look(layer.look);
                if (layer.image != core::kNoImage && layer.image < image_map.size())
                    layer.image = image_map[layer.image];
            }
            style_map[theirs]  = intern_symbol(sym);
            style_done[theirs] = true;
        }
        return style_map[theirs];
    };

    // ---- layers, by name; locks deferred until the entities are in ----
    const std::size_t had_layers = doc_.layer_table().size();
    std::vector<LayerId> layer_map(scratch.layer_table().size(), core::kNoLayer);
    std::vector<LayerId> lock_later;
    for (std::size_t l = 0; l < scratch.layer_table().size(); ++l) {
        const core::Layer* theirs = scratch.layer_table().at(static_cast<LayerId>(l));
        if (theirs == nullptr) continue;
        const std::string want = named(theirs->name);
        const bool had         = doc_.find_layer(want) != core::kNoLayer;
        const LayerId mine     = ensure_layer(want);
        if (mine == core::kNoLayer)
            return core::err(core::ErrorCode::ValidationFailed,
                             "'" + want + "' katmanı oluşturulamadı.");
        layer_map[l] = mine;
        // A REFERENCE'S LAYER THE DRAWING ALREADY HAS is the user's to set: a
        // reload leaves its colour, visibility and lock as they were made here.
        if (external && had) continue;
        if (external && theirs->name != "0")
            if (auto st = set_layer_group(mine, theirs->group.empty()
                                                    ? reference_group
                                                    : reference_group + " > " + theirs->group);
                !st)
                return st.error();

        // Only what differs from a fresh layer is written, so adopting into a
        // layer the drawing already had does not disturb what the user set on it.
        if (!(theirs->appearance == Appearance{}))
            if (auto st = set_layer_appearance(mine, remap_look(theirs->appearance)); !st)
                return st.error();
        if (theirs->style != kByLayerStyle)
            if (auto st = set_layer_style(mine, own_style(theirs->style)); !st) return st.error();
        if (!theirs->visible)
            if (auto st = set_layer_visible(mine, false); !st) return st.error();
        if (!external && !theirs->group.empty())
            if (auto st = set_layer_group(mine, theirs->group); !st) return st.error();
        if (theirs->locked) lock_later.push_back(mine);
    }
    summary.layers = doc_.layer_table().size() - had_layers;

    // ---- block definitions, before their members (TODOS C-13) ----
    //
    // WHICH COME ACROSS. Everything, for an import. For a SELECTION, the
    // definitions its references draw — whole, members and blocks inside them —
    // and no other: a payload that carried a reference and not what it draws
    // pasted a symbol that drew nothing, and one that carried every name the
    // source drawing had filled the other drawing with empty definitions.
    //
    // A NAME THE DRAWING ALREADY HAS KEEPS THE DRAWING'S DEFINITION, the rule
    // every CAD keeps: the incoming references draw it, and the incoming
    // members are not piled onto it — they used to be, and a symbol pasted into
    // a drawing that had it drew twice over.
    //
    // AN EXTERNAL REFERENCE'S BLOCKS are its own, under its prefix: one the
    // drawing already holds as a dependent of it is FILLED again (kRefill),
    // which is what reloading the reference is.
    enum : std::uint8_t { kUntouched, kCreate, kTheirs, kRefill };

    std::vector<std::uint8_t> fate(scratch.blocks().size(), kUntouched);
    std::vector<core::BlockId> pending;
    const auto want_block = [&](core::BlockId b) {
        if (b >= fate.size() || fate[b] != kUntouched) return;
        const core::BlockId have = doc_.blocks().find(named(scratch.blocks().at(b).name));
        const bool refill        = external && have != core::kNoBlock &&
                            (doc_.blocks().at(have).flags & core::kBlockDependent) != 0;
        fate[b] = have == core::kNoBlock ? kCreate : refill ? kRefill : kTheirs;
        if (fate[b] != kTheirs) pending.push_back(b);
    };
    const auto referenced = [&scratch](core::EntityId e) -> core::BlockId {
        if (scratch.entities().kind[e] != core::kBlockReferenceKind) return core::kNoBlock;
        auto ref = core::block_reference_of(scratch.geometry(), scratch.entities().slot[e]);
        return ref ? ref.value().block : core::kNoBlock;
    };
    if (wanted.empty()) {
        for (core::BlockId b = 0; b < scratch.blocks().size(); ++b)
            want_block(b);
    } else {
        for (EntityId e = 0; e < wanted.size(); ++e)
            if (wanted[e] && scratch.entities().alive(e)) want_block(referenced(e));
        for (const core::BlockId b : blocks)
            want_block(b);
    }
    while (!pending.empty()) {
        const core::BlockId b = pending.back();
        pending.pop_back();
        for (const core::EntityKey k : scratch.blocks().at(b).members) {
            const EntityId m = scratch.slot_of(k);
            if (m == core::kNoEntity || !scratch.entities().alive(m)) continue;
            want_block(referenced(m));
            if (!wanted.empty() && m < wanted.size()) wanted[m] = true;
        }
    }
    std::vector<core::BlockId> block_map(scratch.blocks().size(), core::kNoBlock);
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b) {
        if (fate[b] == kUntouched) continue;
        const core::BlockDef& def = scratch.blocks().at(static_cast<core::BlockId>(b));
        if (fate[b] == kTheirs) {
            block_map[b] = doc_.blocks().find(named(def.name));
            note("'" + named(def.name) +
                 "' bloğu çizimde zaten vardı; çizimdeki tanım kullanıldı, "
                 "gelen tanımın üyeleri alınmadı.");
            continue;
        }
        if (fate[b] == kRefill) {
            block_map[b] = doc_.blocks().find(named(def.name));
            continue;
        }
        auto made = add_block(named(def.name), def.description, def.base);
        if (!made) return made.error();
        block_map[b] = made.value();
        if (external)
            if (auto st = set_block_external(made.value(), {}, core::kBlockDependent); !st)
                return st.error();
    }
    std::map<std::uint64_t, core::BlockId> block_of_key;
    std::vector<bool> kept_out(scratch.entities().size(), false); ///< members of a kept definition
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b)
        for (const core::EntityKey k : scratch.blocks().at(static_cast<core::BlockId>(b)).members) {
            block_of_key[core::raw(k)] = block_map[b];
            if (fate[b] != kCreate && fate[b] != kRefill)
                if (const EntityId m = scratch.slot_of(k);
                    m != core::kNoEntity && m < kept_out.size())
                    kept_out[m] = true;
        }

    // ---- attribute columns, by id ----
    const core::AttrTable& theirs_attrs = scratch.attributes();
    std::vector<core::AttrId> column_map(theirs_attrs.columns(), core::kNoAttr);
    for (std::size_t c = 0; c < theirs_attrs.columns(); ++c) {
        const core::AttrColumn* col = theirs_attrs.column(static_cast<core::AttrId>(c));
        if (col == nullptr) continue;
        const core::AttrSpec& spec = col->spec();
        const core::AttrId have    = doc_.attributes().find(spec.id);
        if (have != core::kNoAttr) {
            if (doc_.attributes().column(have)->spec().type != spec.type) {
                note("'" + spec.id + "' sütunu çizimde başka türde; dosyadaki değerler atlandı.");
                continue;
            }
            column_map[c] = have;
            continue;
        }
        auto made = declare_attribute(spec);
        if (!made) return made.error();
        column_map[c] = made.value();
        ++summary.columns;
    }

    // ---- entities, in slot order, every kind through one door ----
    const core::EntityTable& ents = scratch.entities();
    const core::RingGeometry& geo = scratch.geometry();
    std::vector<core::Point2> points;
    std::vector<core::RingGeometry::RingInput> rings;
    std::vector<EntityId> adopted(ents.size(), core::kNoEntity); ///< theirs → mine
    std::vector<EntityId> references;                            ///< the block references made
    for (EntityId e = 0; e < ents.size(); ++e) {
        if (!ents.alive(e)) continue;
        if (!wanted.empty() && !wanted[e]) continue;
        if (kept_out[e]) continue;
        const std::uint32_t slot  = ents.slot[e];
        const core::RingSpan span = geo.rings_of(slot);
        std::size_t total         = 0;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            total += geo.ring_count[r];
        points.clear();
        points.reserve(total);
        rings.clear();
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geo.ring_xs(r);
            const auto ys = geo.ring_ys(r);
            for (std::size_t i = 0; i < xs.size(); ++i)
                points.push_back(core::Point2{xs[i], ys[i]});
        }
        std::size_t cursor = 0;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            rings.push_back(core::RingGeometry::RingInput{
                std::span<const core::Point2>(points.data() + cursor, geo.ring_count[r]),
                geo.ring_role[r], geo.ring_part[r]});
            cursor += geo.ring_count[r];
        }

        const auto found             = block_of_key.find(core::raw(ents.key[e]));
        const core::BlockId in_block = found == block_of_key.end() ? into : found->second;
        const LayerId layer =
            ents.layer[e] < layer_map.size() ? layer_map[ents.layer[e]] : core::kNoLayer;
        if (layer == core::kNoLayer)
            return core::err(core::ErrorCode::Internal,
                             std::to_string(e) + ". nesnenin katmanı eşlenemedi.");

        // A REFERENCE NAMES ITS BLOCK BY NUMBER, and the number is this
        // document's now: carried as it was, a reference pasted into a drawing
        // whose table already held blocks drew one of THEM. Its box is brought
        // up to date once every definition is in (below).
        std::span<const std::uint8_t> payload = geo.payload_of(slot);
        std::vector<std::uint8_t> renumbered;
        if (ents.kind[e] == core::kBlockReferenceKind) {
            auto ref = core::decode_block_reference(payload);
            if (!ref) return ref.error();
            if (ref.value().block >= block_map.size() ||
                block_map[ref.value().block] == core::kNoBlock)
                return core::err(core::ErrorCode::Internal,
                                 std::to_string(e) + ". nesnenin bloğu eşlenemedi.");
            ref.value().block = block_map[ref.value().block];
            renumbered        = core::encode_block_reference(ref.value());
            payload           = renumbered;
        }
        auto made = add_kind(layer, ents.kind[e], rings, payload, in_block);
        if (!made) return made.error();
        const EntityId mine = made.value();
        adopted[e]          = mine;
        if (ents.kind[e] == core::kBlockReferenceKind) references.push_back(mine);

        if ((ents.flags[e] & core::FlagHidden) != 0)
            if (auto st = set_entity_hidden(mine, true); !st) return st.error();
        if (ents.style[e] != kByLayerStyle)
            if (auto st = set_entity_style(mine, own_style(ents.style[e])); !st) return st.error();
        if (scratch.texts().has(slot))
            if (auto st = set_text(mine, std::string(scratch.texts().text(slot)),
                                   scratch.texts().height(slot), scratch.texts().anchor(slot),
                                   scratch.texts().lines(slot));
                !st)
                return st.error();

        for (std::size_t c = 0; c < column_map.size(); ++c) {
            if (column_map[c] == core::kNoAttr) continue;
            auto cell = theirs_attrs.get(static_cast<core::AttrId>(c), slot);
            if (!cell || !cell.value().present) continue;
            if (auto st = set_attribute(column_map[c], mine, cell.value()); !st) return st.error();
        }

        for (const core::ForeignTable::Record& rec : scratch.foreign().records()) {
            if (rec.slot != slot) continue;
            const std::string_view tag = scratch.foreign().tags()[rec.tag];
            if (auto st = attach_foreign(mine, tag, scratch.foreign().bytes(slot, tag)); !st)
                return st.error();
        }
        ++summary.entities;
        if (in_block == core::kNoBlock) ++summary.standalone;
    }

    // ---- ties, once every object they join exists (TODOS C-12) ----
    //
    // A caption that follows its object follows it here too — a leader's words
    // read from a DXF, a label pasted with its parcel — its source named by the
    // key it has in THIS drawing. A tie whose source was not brought across is
    // left behind with it: the caption stays, free.
    for (EntityId e = 0; !external && e < adopted.size(); ++e) {
        const core::Attachment* tie = scratch.attachments().get(e);
        if (tie == nullptr || adopted[e] == core::kNoEntity) continue;
        const EntityId source = scratch.slot_of(tie->source);
        if (source == core::kNoEntity || source >= adopted.size() ||
            adopted[source] == core::kNoEntity)
            continue;
        core::Attachment mine = *tie;
        mine.source           = doc_.key_of(adopted[source]);
        if (auto st = set_attachment(adopted[e], mine); !st) return st.error();
    }

    // ---- the references' boxes, once every definition they draw is in ----
    //
    // The box came with the payload, drawn from the SOURCE's definition; the
    // definition drawn here may be this document's own (a name it had), and
    // members arrive in table order, not definition order.
    for (const EntityId r : references)
        if (auto st = refresh_reference_bounds(r); !st) return st.error();

    // ---- block uses, once every block exists: the definitions made here ----
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b) {
        if (fate[b] != kCreate && fate[b] != kRefill) continue;
        for (const core::BlockId used : scratch.blocks().at(static_cast<core::BlockId>(b)).uses)
            if (used < block_map.size() && block_map[used] != core::kNoBlock)
                if (auto st = add_block_use(block_map[b], block_map[used]); !st) return st.error();
    }

    for (const LayerId l : lock_later)
        if (auto st = set_layer_locked(l, true); !st) return st.error();

    return summary;
}

} // namespace kentos::command
