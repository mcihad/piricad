// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/transaction.hpp"

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/dimension.hpp"

#include <algorithm>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {

Transaction::Transaction(Document& doc, std::string label) : doc_(doc), label_(std::move(label)) {}

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
    core::Op undo;
    auto st = doc_.set_attribute(col, e, v, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_text(EntityId e, std::string content, core::Mm height,
                             core::TextAnchor anchor)
{
    core::Op undo;
    auto st = doc_.set_text(e, std::move(content), height, anchor, undo);
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
    for (int round = 0; round < 16; ++round) {
        std::vector<EntityId> moved;
        std::vector<EntityId> erased;
        std::map<EntityId, std::uint32_t> before; // the slot an entity had before this range
        for (std::size_t i = settled_upto_; i < inverse_.size(); ++i) {
            const Op& op = inverse_[i];
            if (op.kind == Op::Kind::SetGeometry) {
                moved.push_back(op.entity);
                before.emplace(op.entity, op.geometry_slot); // the OLDEST wins
            } else if (op.kind == Op::Kind::SetEntityAlive && op.bool_arg) {
                // The inverse restores it, so the command erased it.
                erased.push_back(op.entity);
            }
        }
        settled_upto_ = inverse_.size();
        if (moved.empty() && erased.empty()) break;
        std::sort(moved.begin(), moved.end());
        moved.erase(std::unique(moved.begin(), moved.end()), moved.end());
        std::sort(erased.begin(), erased.end());
        erased.erase(std::unique(erased.begin(), erased.end()), erased.end());

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
        for (const EntityId src : moved) {
            if (!doc_.alive(src)) continue;
            tab.dependents_of(doc_.key_of(src), deps);
            if (deps.empty()) continue;
            const std::uint32_t now_slot = ents.slot[src];
            const auto was               = before.find(src);
            for (const EntityId d : deps) {
                if (!doc_.alive(d) || !doc_.editable(d)) continue;
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

                const std::uint32_t dslot = ents.slot[d];
                if (!texts.has(dslot)) continue; // only captions follow today
                const core::Mm height = texts.height(dslot);
                const auto place      = core::attach_place(now.points, now.closed, a, height, true);
                if (!place) continue;

                std::string text(texts.text(dslot));
                if (const auto derived = core::attach_text(now.points, now.closed, a); derived)
                    text = *derived;
                const auto base = core::dimension_baseline(place->centre, place->dir_x,
                                                           place->dir_y, height, text);

                // Nothing is written that is already so: a caption the command
                // moved together with its source is already where the rule puts
                // it, and appending an identical slot would be an edit that
                // changed nothing but the file.
                const core::RingSpan rs = geom.rings_of(dslot);
                const bool same_place   = rs.count == 1 && geom.ring_count[rs.first] == 2 &&
                                        geom.ring_role[rs.first] == core::RingRole::Open &&
                                        geom.vertex(rs.first, 0) == base[0] &&
                                        geom.vertex(rs.first, 1) == base[1];
                const bool same_text = text == texts.text(dslot);

                if (a != *stored && !set_attachment(d, a)) continue;
                if (!same_text) {
                    if (!set_text(d, text, height, texts.anchor(dslot))) continue;
                    ++rep.relabelled;
                }
                if (!same_place) {
                    const core::RingGeometry::RingInput ring{base, core::RingRole::Open, 0};
                    if (set_geometry(d, std::span<const core::RingGeometry::RingInput>(&ring, 1)))
                        ++rep.followed;
                }
            }
        }
    }
    return rep;
}

void Transaction::rollback()
{
    // Newest first: the inverse of a sequence is the reversed sequence of inverses.
    for (auto it = inverse_.rbegin(); it != inverse_.rend(); ++it)
        (void)doc_.apply(*it);
    inverse_.clear();
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
Transaction::adopt_from(const core::Document& scratch, std::span<const core::EntityKey> only)
{
    // WHICH ENTITIES, resolved once into a slot set rather than searched per
    // entity: a clipboard copy of five hundred parcels would otherwise be a
    // linear scan five hundred times over.
    std::vector<bool> wanted;
    if (!only.empty()) {
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
        const LayerId mine = ensure_layer(theirs->name);
        if (mine == core::kNoLayer)
            return core::err(core::ErrorCode::ValidationFailed,
                             "'" + theirs->name + "' katmanı oluşturulamadı.");
        layer_map[l] = mine;

        // Only what differs from a fresh layer is written, so adopting into a
        // layer the drawing already had does not disturb what the user set on it.
        if (!(theirs->appearance == Appearance{}))
            if (auto st = set_layer_appearance(mine, remap_look(theirs->appearance)); !st)
                return st.error();
        if (theirs->style != kByLayerStyle)
            if (auto st = set_layer_style(mine, own_style(theirs->style)); !st) return st.error();
        if (!theirs->visible)
            if (auto st = set_layer_visible(mine, false); !st) return st.error();
        if (!theirs->group.empty())
            if (auto st = set_layer_group(mine, theirs->group); !st) return st.error();
        if (theirs->locked) lock_later.push_back(mine);
    }
    summary.layers = doc_.layer_table().size() - had_layers;

    // ---- block definitions, before their members ----
    std::vector<core::BlockId> block_map(scratch.blocks().size(), core::kNoBlock);
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b) {
        const core::BlockDef& def = scratch.blocks().at(static_cast<core::BlockId>(b));
        core::BlockId mine        = doc_.blocks().find(def.name);
        if (mine == core::kNoBlock) {
            auto made = add_block(def.name, def.description, def.base);
            if (!made) return made.error();
            mine = made.value();
        } else {
            note("'" + def.name + "' bloğu çizimde zaten vardı; dosyadaki tanım onun üyesi oldu.");
        }
        block_map[b] = mine;
    }
    std::map<std::uint64_t, core::BlockId> block_of_key;
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b)
        for (const core::EntityKey k : scratch.blocks().at(static_cast<core::BlockId>(b)).members)
            block_of_key[core::raw(k)] = block_map[b];

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
    for (EntityId e = 0; e < ents.size(); ++e) {
        if (!ents.alive(e)) continue;
        if (!wanted.empty() && !wanted[e]) continue;
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
        const core::BlockId in_block = found == block_of_key.end() ? core::kNoBlock : found->second;
        const LayerId layer =
            ents.layer[e] < layer_map.size() ? layer_map[ents.layer[e]] : core::kNoLayer;
        if (layer == core::kNoLayer)
            return core::err(core::ErrorCode::Internal,
                             std::to_string(e) + ". nesnenin katmanı eşlenemedi.");

        auto made = add_kind(layer, ents.kind[e], rings, geo.payload_of(slot), in_block);
        if (!made) return made.error();
        const EntityId mine = made.value();

        if ((ents.flags[e] & core::FlagHidden) != 0)
            if (auto st = set_entity_hidden(mine, true); !st) return st.error();
        if (ents.style[e] != kByLayerStyle)
            if (auto st = set_entity_style(mine, own_style(ents.style[e])); !st) return st.error();
        if (scratch.texts().has(slot))
            if (auto st = set_text(mine, std::string(scratch.texts().text(slot)),
                                   scratch.texts().height(slot), scratch.texts().anchor(slot));
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
    }

    // ---- block uses, once every block exists ----
    for (std::size_t b = 0; b < scratch.blocks().size(); ++b)
        for (const core::BlockId used : scratch.blocks().at(static_cast<core::BlockId>(b)).uses)
            if (used < block_map.size())
                if (auto st = add_block_use(block_map[b], block_map[used]); !st) return st.error();

    for (const LayerId l : lock_later)
        if (auto st = set_layer_locked(l, true); !st) return st.error();

    return summary;
}

} // namespace kentos::command
