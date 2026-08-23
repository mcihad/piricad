// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/document.hpp"

#include "piricad/core/spatial_index.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::core {
namespace {

/// Grows geometrically, so appending N entities is O(N) rather than O(N²).
/// Calling reserve(size() + n) unconditionally pins capacity to the exact size
/// and turns a bulk load into quadratic copying; that cost was measured once.
template<class T> void reserve_for(std::vector<T>& v, std::size_t extra)
{
    const std::size_t needed = v.size() + extra;
    if (needed <= v.capacity()) return;
    v.reserve(std::max(needed, v.capacity() * 2));
}

} // namespace

Document::~Document()                              = default;
Document::Document(Document&&) noexcept            = default;
Document& Document::operator=(Document&&) noexcept = default;

Document::Document()
{
    // LayerTable creates layer "0" itself — the default every CAD lineage opens
    // with — so the per-layer counters are sized from the table rather than
    // assumed empty. Getting this wrong wrote past the end of layer_live_.
    layer_live_.assign(layers_.size(), 0);
}

// ------------------------------------------------------------------ read ----

LayerId Document::find_layer(std::string_view name) const
{
    return layers_.find(name);
}

std::size_t Document::layer_entity_count(LayerId l) const noexcept
{
    return l < layer_live_.size() ? layer_live_[l] : 0;
}

Box2 Document::entity_extent(EntityId e) const
{
    return (e < entities_.size() && entities_.alive(e)) ? entities_.box_of(e) : Box2{};
}

Box2 Document::extent() const
{
    Box2 b{};
    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (!entities_.visible(e)) continue;

        if (b.empty()) {
            b = entities_.box_of(e);
            continue;
        }
        if (entities_.min_x[e] < b.min_x) b.min_x = entities_.min_x[e];
        if (entities_.min_y[e] < b.min_y) b.min_y = entities_.min_y[e];
        if (entities_.max_x[e] > b.max_x) b.max_x = entities_.max_x[e];
        if (entities_.max_y[e] > b.max_y) b.max_y = entities_.max_y[e];
    }
    return b;
}

Mm2 Document::entity_area(EntityId e) const
{
    if (e >= entities_.size() || !entities_.alive(e)) return 0;
    return geometry_.area_of(entities_.slot[e]);
}

Mm Document::entity_perimeter(EntityId e) const
{
    if (e >= entities_.size() || !entities_.alive(e)) return 0;
    return geometry_.perimeter_of(entities_.slot[e]);
}

std::uint64_t Document::content_hash() const
{
    // Keys are deliberately excluded: two documents built the same way from the
    // same input must agree, and key assignment is an allocation detail (§7.3).
    std::uint64_t h = fnv1a(crs_.id());
    h               = layers_.fold(h);
    h               = styles_.fold(h);

    // Attributes ARE document content: an ada number is not a view preference,
    // it is what the parcel is. Folding the table once here rather than per
    // entity keeps the loop below reading only the hot columns, and folds the
    // SCHEMA too — declaring a column changes what the document says it holds
    // even before a single cell is written.
    h = attributes_.fold(h);
    h = texts_.fold(h);

    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (!entities_.alive(e)) continue;

        h = fnv1a(layers_.all()[entities_.layer[e]].folded, h);
        h = fnv1a_int(static_cast<std::int64_t>(entities_.style[e]), h);
        h = fnv1a_int(entities_.flags[e] & (FlagAlive | FlagHidden), h);

        const RingSpan span = geometry_.rings_of(entities_.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            h = fnv1a_int(static_cast<std::int64_t>(geometry_.ring_role[r]), h);
            h = fnv1a_int(geometry_.ring_part[r], h);

            const auto xs = geometry_.ring_xs(r);
            const auto ys = geometry_.ring_ys(r);
            for (std::size_t i = 0; i < xs.size(); ++i) {
                h = fnv1a_int(xs[i], h);
                h = fnv1a_int(ys[i], h);
            }
            h = fnv1a_int(-2, h); // ring terminator
        }
        h = fnv1a_int(-1, h); // entity terminator
    }
    return h;
}

// -------------------------------------------------------------- identity ----

EntityKey Document::key_of(EntityId e) const noexcept
{
    return e < entities_.size() ? entities_.key[e] : EntityKey::None;
}

EntityId Document::slot_of(EntityKey k) const noexcept
{
    if (k == EntityKey::None || entities_.key.empty()) return kNoEntity;

    if (!keys_sorted_) {
        // Only reachable if a loader appended out of order; linear rather than
        // wrong, and the loader is expected to restore the invariant.
        for (EntityId e = 0; e < entities_.size(); ++e)
            if (entities_.key[e] == k) return e;
        return kNoEntity;
    }

    const auto begin = entities_.key.begin();
    const auto end   = entities_.key.end();
    const auto it =
        std::lower_bound(begin, end, k, [](EntityKey a, EntityKey b) { return raw(a) < raw(b); });
    if (it == end || *it != k) return kNoEntity;
    return static_cast<EntityId>(it - begin);
}

LayerKey Document::layer_key_of(LayerId l) const noexcept
{
    return layers_.key_of(l);
}

LayerId Document::layer_slot_of(LayerKey k) const noexcept
{
    return layers_.slot_of(k);
}

// ----------------------------------------------------------------- index ----

const SpatialIndex& Document::spatial_index() const
{
    if (!index_) index_ = std::make_unique<SpatialIndex>();

    const std::size_t total = entities_.size();
    const std::size_t added = total - indexed_upto_;

    // Rebuild when the unindexed tail has grown past 5% of what is indexed, or
    // when enough entities have been erased that the tree mostly points at
    // corpses. Otherwise the tail is short and scanning it beats repacking.
    const std::size_t growth_limit = std::max<std::size_t>(4096, indexed_upto_ / 20);
    const bool grew                = added > growth_limit;
    const bool shrank              = indexed_live_ > 1024 && live_count_ * 2 < indexed_live_;
    const bool virgin              = index_->empty() && total > 0;

    if (grew || shrank || virgin) {
        index_->build(entities_);
        indexed_upto_ = static_cast<EntityId>(total);
        indexed_live_ = live_count_;
    }
    return *index_;
}

// ------------------------------------------------------------- mutators -----

LayerId Document::ensure_layer(std::string_view name)
{
    if (const LayerId existing = layers_.find(name); existing != kNoLayer) return existing;

    Layer l;
    l.name   = std::string(name);
    l.folded = turkish_upper(name);

    auto added = layers_.add(std::move(l), keys_);
    if (!added) return kNoLayer;

    // Resized from the table, never appended blind: LayerTable owns how many
    // layers exist and the counters follow it.
    layer_live_.resize(layers_.size(), 0);
    ++revision_;
    return added.value();
}

Result<EntityId> Document::push_entity(LayerId lyr, std::uint32_t geometry_slot)
{
    const EntityKey key = keys_.mint_entity();
    if (key == EntityKey::None)
        return err(ErrorCode::Internal,
                   "Nesne anahtarı alanı tükendi; bu belgeye yeni nesne eklenemez.");

    const Box2 box = geometry_.bounds_of(geometry_slot);
    const auto id  = static_cast<EntityId>(entities_.size());

    reserve_for(entities_.min_x, 1);
    reserve_for(entities_.min_y, 1);
    reserve_for(entities_.max_x, 1);
    reserve_for(entities_.max_y, 1);
    reserve_for(entities_.flags, 1);
    reserve_for(entities_.layer, 1);
    reserve_for(entities_.style, 1);
    reserve_for(entities_.kind, 1);
    reserve_for(entities_.slot, 1);
    reserve_for(entities_.key, 1);

    entities_.min_x.push_back(box.min_x);
    entities_.min_y.push_back(box.min_y);
    entities_.max_x.push_back(box.max_x);
    entities_.max_y.push_back(box.max_y);

    // The layer bit is mirrored at creation and refreshed by the command that
    // toggles the layer, so the cull test stays one byte (R7).
    std::uint8_t flags = FlagAlive;
    if (const Layer* l = layers_.at(lyr); l && !l->visible) flags |= FlagLayerHidden;
    entities_.flags.push_back(flags);

    entities_.layer.push_back(lyr);
    entities_.style.push_back(kByLayerStyle);
    entities_.kind.push_back(0);
    entities_.slot.push_back(geometry_slot);

    // The binary search in slot_of() depends on this staying true.
    if (!entities_.key.empty() && raw(entities_.key.back()) >= raw(key)) keys_sorted_ = false;
    entities_.key.push_back(key);

    // The slot-indexed side tables follow the geometry, ALWAYS.
    //
    // Growing them lazily — on the first write to each — made content_hash()
    // depend on the ORDER of a session rather than on its result: a drawing whose
    // ada number was typed before its caption hashed differently from the same
    // drawing typed the other way round, and reloading either of them produced a
    // third answer. Two documents that hold the same facts must fingerprint the
    // same, or the fingerprint is not about the document.
    attributes_.resize(geometry_.slot_count());
    texts_.resize(geometry_.slot_count());

    ++live_count_;
    ++layer_live_[lyr];
    ++revision_;
    return id;
}

Result<EntityId> Document::add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out)
{
    const RingGeometry::RingInput ring{pts, RingRole::Open, 0};
    return add_area(lyr, std::span<const RingGeometry::RingInput>(&ring, 1), undo_out);
}

Result<EntityId> Document::add_area(LayerId lyr, std::span<const RingGeometry::RingInput> rings,
                                    Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");

    // RingGeometry::append owns every geometric rule — ring ordering, closure,
    // minimum vertex counts, zero area, holes escaping their exterior, and the
    // coordinate limit. The document does not restate them (model.md R9–R12).
    auto slot = geometry_.append(rings);
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value());
    if (!id) return id;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id.value();
    undo_out.bool_arg = false;
    return id;
}

Status Document::set_entity_alive(EntityId e, bool alive, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const bool was = entities_.alive(e);
    if (was != alive) {
        const std::size_t delta = alive ? 1 : static_cast<std::size_t>(-1);
        live_count_ += delta;
        layer_live_[entities_.layer[e]] += delta;
    }
    if (alive)
        entities_.flags[e] |= FlagAlive;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagAlive);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_entity_hidden(EntityId e, bool hidden, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const bool was = (entities_.flags[e] & FlagHidden) != 0;
    if (hidden)
        entities_.flags[e] |= FlagHidden;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagHidden);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityHidden;
    undo_out.entity   = e;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_entity_style(EntityId e, StyleId style, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!styles_.contains(style))
        return err(ErrorCode::InvalidArgument, "Bilinmeyen stil: " + std::to_string(style));

    const StyleId was  = entities_.style[e];
    entities_.style[e] = style;
    ++revision_;

    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetEntityStyle;
    undo_out.entity    = e;
    undo_out.style_arg = was;
    return ok();
}

// ------------------------------------------------------------ attributes ----

Result<AttrId> Document::declare_attribute(AttrSpec spec)
{
    // A CodeRef column must name a catalogue this document actually holds, or
    // its codes cannot be validated — and an unvalidatable regulatory reference
    // on a parcel is worse than no reference at all (R34).
    if (spec.type == AttrType::CodeRef && catalogues_.find(spec.catalog) == nullptr)
        return err(ErrorCode::InvalidArgument,
                   "'" + spec.id + "' özniteliği '" + spec.catalog +
                       "' kataloğuna dayanıyor ama belge o kataloğu taşımıyor.");

    auto col = attributes_.add(std::move(spec));
    if (!col) return col;

    // Rows follow slots, always. A column declared after the drawing was made
    // still has a cell for every entity in it, all of them absent.
    attributes_.resize(geometry_.slot_count());
    ++revision_;
    return col;
}

Status Document::set_attribute(AttrId col, EntityId e, const AttrValue& v, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const AttrColumn* column = attributes_.column(col);
    if (column == nullptr)
        return err(ErrorCode::NotFound, "Bilinmeyen öznitelik sütunu: " + std::to_string(col));

    // A CodeRef cell may only carry a code the catalogue knows. This is where a
    // detay kodu is checked, and it is checked against /data rather than against
    // anything written here (CLAUDE.md 5.13).
    if (column->type() == AttrType::CodeRef && v.present) {
        const Catalogue* cat = catalogues_.find(column->spec().catalog);
        if (cat == nullptr || !cat->contains(v.text))
            return err(ErrorCode::InvalidArgument,
                       "'" + v.text + "' kodu '" + column->spec().catalog +
                           "' kataloğunda yok. Katalog sürümünü ve kodu denetleyin.");
    }

    attributes_.resize(geometry_.slot_count());

    auto was = attributes_.set(col, entities_.slot[e], v);
    if (!was) return was.error();

    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetAttribute;
    undo_out.entity   = e;
    undo_out.attr_col = col;
    undo_out.attr_arg = was.value();
    return ok();
}

Result<AttrValue> Document::attribute(AttrId col, EntityId e) const
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    return attributes_.get(col, entities_.slot[e]);
}

Status Document::set_text(EntityId e, std::string content, Mm height, TextAnchor anchor,
                          Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    texts_.resize(geometry_.slot_count());
    const std::uint32_t slot = entities_.slot[e];

    // The inverse is built BEFORE the write, from what is there now. An empty
    // string with a zero height is how "there was no text here" is expressed, and
    // undo therefore detaches rather than writing an empty caption.
    undo_out             = Op{};
    undo_out.kind        = Op::Kind::SetText;
    undo_out.entity      = e;
    undo_out.str_arg     = std::string(texts_.text(slot));
    undo_out.text_height = texts_.has(slot) ? texts_.height(slot) : 0;
    undo_out.text_anchor = texts_.anchor(slot);

    if (content.empty() || height <= 0) {
        texts_.clear(slot);
        ++revision_;
        return ok();
    }

    if (auto st = texts_.set(slot, content, height, anchor); !st) return st;
    ++revision_;
    return ok();
}

void Document::mirror_layer_visibility(LayerId l, bool visible)
{
    // One pass per toggle instead of an indirect load per entity per frame (R7).
    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (entities_.layer[e] != l) continue;
        if (visible)
            entities_.flags[e] &= static_cast<std::uint8_t>(~FlagLayerHidden);
        else
            entities_.flags[e] |= FlagLayerHidden;
    }
}

Status Document::set_layer_visible(LayerId l, bool visible, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was = layer->visible;
    layer->visible = visible;
    mirror_layer_visibility(l, visible);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerVisible;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_locked(LayerId l, bool locked, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was = layer->locked;
    layer->locked  = locked;
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerLocked;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_appearance(LayerId l, const Appearance& a, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const Appearance was = layer->appearance;
    layer->appearance    = a;
    ++revision_;

    undo_out                = Op{};
    undo_out.kind           = Op::Kind::SetLayerAppearance;
    undo_out.layer          = l;
    undo_out.appearance_arg = was;
    return ok();
}

Status Document::set_crs(Crs crs, Op& undo_out)
{
    if (crs.id().empty())
        return err(ErrorCode::InvalidArgument, "Koordinat sistemi kimliği boş olamaz.");

    Crs was = crs_;
    crs_    = std::move(crs);
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::SetCrs;
    undo_out.crs_arg = std::move(was);
    return ok();
}

StyleId Document::intern_style(const Appearance& a)
{
    return styles_.intern(a);
}

StyleId Document::intern_symbol(const Symbol& sym)
{
    return styles_.intern(sym);
}

Status Document::apply(const Op& op, Op* undo_out)
{
    Op scratch;
    Op& inverse = undo_out ? *undo_out : scratch;

    switch (op.kind) {
    case Op::Kind::None: return ok();
    case Op::Kind::SetEntityAlive: return set_entity_alive(op.entity, op.bool_arg, inverse);
    case Op::Kind::SetEntityHidden: return set_entity_hidden(op.entity, op.bool_arg, inverse);
    case Op::Kind::SetEntityStyle: return set_entity_style(op.entity, op.style_arg, inverse);
    case Op::Kind::SetAttribute: return set_attribute(op.attr_col, op.entity, op.attr_arg, inverse);
    case Op::Kind::SetText:
        return set_text(op.entity, op.str_arg, op.text_height, op.text_anchor, inverse);
    case Op::Kind::SetLayerVisible: return set_layer_visible(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerLocked: return set_layer_locked(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerAppearance:
        return set_layer_appearance(op.layer, op.appearance_arg, inverse);
    case Op::Kind::SetCrs: return set_crs(op.crs_arg, inverse);
    }
    return err(ErrorCode::Internal, "İşlenmemiş Op::Kind");
}

} // namespace piricad::core
