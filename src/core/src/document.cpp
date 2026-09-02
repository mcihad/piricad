// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/document.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/spatial_index.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>

namespace kentos::core {
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
    h = images_.fold(h);
    h = dashes_.fold(h);

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

    // `index_stale_` is not one of the heuristics above and is not allowed to be:
    // the others trade a stale tree for a cheaper rebuild, and a tree that is
    // merely over-inclusive costs a rejected candidate. An edited entity makes it
    // UNDER-inclusive, and no amount of scanning the tail finds an entity the tree
    // filed under a box it has left.
    if (grew || shrank || virgin || index_stale_) {
        index_->build(entities_);
        indexed_upto_ = static_cast<EntityId>(total);
        indexed_live_ = live_count_;
        index_stale_  = false;
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

Result<EntityId> Document::push_entity(LayerId lyr, std::uint32_t geometry_slot, KindId kind)
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
    entities_.kind.push_back(kind);
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
    // THE FLOOR LIVES HERE, where the kind is known. `RingGeometry::append` allows
    // a one-vertex open ring because a `core.point` is exactly that; a polyline
    // with one vertex is a line that goes nowhere and is refused.
    if (pts.size() < 2)
        return err(ErrorCode::ValidationFailed,
                   "Bir çizgi en az iki nokta ister, verilen: " + std::to_string(pts.size()) + ".");

    const RingGeometry::RingInput ring{pts, RingRole::Open, 0};
    return add_area(lyr, std::span<const RingGeometry::RingInput>(&ring, 1), undo_out);
}

Result<EntityId> Document::add_circle(LayerId lyr, Point2 centre, Mm radius, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (radius <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Daire yarıçapı sıfırdan büyük olmalı: " + std::to_string(radius));

    // The two DEFINING vertices, not the circle's picture: centre, and a handle
    // due east of it so the radius reads back as an exact integer subtraction
    // (see core.circle in entity_kind.cpp). The drawn form is the kind's `emit`.
    const Point2 pts[2]{centre, Point2{centre.x + radius, centre.y}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 2), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kCircleKind);
    if (!id) return id;

    // The bounding box of a circle is NOT the box of its two stored vertices: the
    // arena bounded the centre and the handle, which is a flat line to the east.
    // Every cull, every pick prefilter and every zoom-to-extents reads this, so it
    // is corrected here rather than left for the kind to answer later.
    const EntityId e   = id.value();
    entities_.min_x[e] = centre.x - radius;
    entities_.min_y[e] = centre.y - radius;
    entities_.max_x[e] = centre.x + radius;
    entities_.max_y[e] = centre.y + radius;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_ellipse(LayerId lyr, Point2 centre, Point2 major, Point2 minor,
                                       Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (centre.x == major.x && centre.y == major.y)
        return err(ErrorCode::InvalidArgument, "Elipsin birinci ekseni sıfır uzunlukta olamaz.");
    if (centre.x == minor.x && centre.y == minor.y)
        return err(ErrorCode::InvalidArgument, "Elipsin ikinci ekseni sıfır uzunlukta olamaz.");

    // The three DEFINING vertices: centre and the two axis endpoints. The
    // endpoints carry the rotation as vectors, so nothing stores an angle.
    const Point2 pts[3]{centre, major, minor};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 3), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kEllipseKind);
    if (!id) return id;

    // As for the circle: the arena bounded the three stored vertices, which for a
    // rotated ellipse is a triangle strictly inside the shape. Every cull, pick
    // prefilter and zoom-to-extents reads this box, so it is corrected here.
    const EntityId e = id.value();
    Box2 box{};
    const std::uint32_t one[1]{slot.value()};
    if (const KindSpec* spec = builtin_kinds().find(kEllipseKind); spec != nullptr)
        spec->bbox(geometry_, SlotSpan(one, 1), std::span<Box2>(&box, 1));

    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_point(LayerId lyr, Point2 at, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");

    const Point2 pts[1]{at};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 1), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kPointKind);
    if (!id) return id;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id.value();
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_arc(LayerId lyr, Point2 centre, Mm radius, Point2 start, Point2 end,
                                   Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (radius <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Yay yarıçapı sıfırdan büyük olmalı: " + std::to_string(radius));

    const Point2 pts[4]{centre, Point2{centre.x + radius, centre.y}, start, end};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 4), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kArcKind);
    if (!id) return id;

    // The box of an ARC, not of its four defining vertices: a sweep crossing due
    // north reaches higher than either end does, and the arena bounded the centre
    // and the radius handle along with them. Cull, pick and zoom-to-extents all
    // read this.
    const EntityId e     = id.value();
    const KindSpec* spec = builtin_kinds().find(kArcKind);
    if (spec != nullptr) {
        const std::uint32_t one[1]{slot.value()};
        Box2 box{};
        spec->bbox(geometry_, SlotSpan(one, 1), std::span<Box2>(&box, 1));
        entities_.min_x[e] = box.min_x;
        entities_.min_y[e] = box.min_y;
        entities_.max_x[e] = box.max_x;
        entities_.max_y[e] = box.max_y;
    }

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
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

    auto id = push_entity(lyr, slot.value(), kPolylineKind);
    if (!id) return id;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id.value();
    undo_out.bool_arg = false;
    return id;
}

Status Document::set_entity_layer(EntityId e, LayerId layer, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (layer >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(layer));
    if (layers_.all()[layer].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[layer].name + "' katmanı kilitli.");

    const LayerId was = entities_.layer[e];
    if (was == layer) {
        undo_out        = Op{};
        undo_out.kind   = Op::Kind::SetEntityLayer;
        undo_out.entity = e;
        undo_out.layer  = was;
        return ok();
    }

    // The per-layer live counts follow the object, or the layer panel would go on
    // reporting it where it no longer is.
    if (entities_.alive(e)) {
        --layer_live_[was];
        ++layer_live_[layer];
    }

    entities_.layer[e] = layer;

    // The mirrored visibility bit is the cull test's whole input (R7), so it is
    // re-taken from the NEW layer rather than carried over from the old one.
    if (const Layer* l = layers_.at(layer); l != nullptr && !l->visible)
        entities_.flags[e] |= FlagLayerHidden;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagLayerHidden);

    ++revision_;

    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetEntityLayer;
    undo_out.entity = e;
    undo_out.layer  = was;
    return ok();
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

namespace {

/// The bounding box of a slot AS ITS KIND SEES IT.
///
/// `RingGeometry::bounds_of` bounds the stored vertices, which for a curve are its
/// definition and not its shape: a circle's are a centre and a handle due east, so
/// the arena's answer is a flat line. Every cull, every pick prefilter and every
/// zoom-to-extents reads this box, so it is asked of the kind.
Box2 kind_bounds(const RingGeometry& geom, KindId kind, std::uint32_t slot)
{
    if (kind != kPolylineKind) {
        if (const KindSpec* spec = builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{slot};
            Box2 box{};
            spec->bbox(geom, SlotSpan(one, 1), std::span<Box2>(&box, 1));
            return box;
        }
    }
    return geom.bounds_of(slot);
}

} // namespace

Status Document::set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                              Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Silinmiş nesnenin geometrisi değiştirilemez: " + std::to_string(e));

    // Validation is the arena's, exactly as it is for a new entity: ring order,
    // minimum vertex counts and closure are the same rules whether the geometry is
    // being created or replaced (R11). A refusal here writes nothing, so the
    // entity keeps the geometry it had.
    auto slot = geometry_.append(rings);
    if (!slot) return slot.error();

    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot.value();

    const Box2 box     = kind_bounds(geometry_, entities_.kind[e], slot.value());
    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    // The tree filed this entity under the box it no longer has. See `index_stale_`.
    index_stale_ = true;
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

Status Document::restore_geometry(EntityId e, std::uint32_t slot, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (slot >= geometry_.slot_count())
        return err(ErrorCode::InvalidArgument,
                   "Bilinmeyen geometri yuvası: " + std::to_string(slot));

    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot;

    const Box2 box     = kind_bounds(geometry_, entities_.kind[e], slot);
    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    index_stale_ = true;
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
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

Status Document::add_guide(GuideAxis axis, Mm coordinate, Op& undo_out)
{
    undo_out              = Op{};
    undo_out.kind         = Op::Kind::SetGuides;
    undo_out.guide_axes   = guides_.axes();
    undo_out.guide_coords = guides_.coordinates();

    guides_.add(axis, coordinate);
    ++revision_;
    return ok();
}

Status Document::remove_guide(std::size_t index, Op& undo_out)
{
    if (index >= guides_.size())
        return err(ErrorCode::NotFound, "Kılavuz yok: " + std::to_string(index) + ". Çizimde " +
                                            std::to_string(guides_.size()) + " kılavuz var.");

    undo_out              = Op{};
    undo_out.kind         = Op::Kind::SetGuides;
    undo_out.guide_axes   = guides_.axes();
    undo_out.guide_coords = guides_.coordinates();

    guides_.remove(index);
    ++revision_;
    return ok();
}

void Document::set_guides(std::vector<GuideAxis> axes, std::vector<Mm> coords)
{
    guides_.load(std::move(axes), std::move(coords));
    ++revision_;
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

Status Document::set_layer_style(LayerId l, StyleId style, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));
    if (style != kByLayerStyle && !styles_.contains(style))
        return err(ErrorCode::NotFound, "Bilinmeyen stil kimliği: " + std::to_string(style));

    const StyleId was = layer->style;
    layer->style      = style;
    ++revision_;

    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetLayerStyle;
    undo_out.layer     = l;
    undo_out.style_arg = was;
    return ok();
}

Status Document::set_layer_group(LayerId l, std::string group, Op& undo_out)
{
    Layer* record = layers_.at(l);
    if (record == nullptr)
        return err(ErrorCode::NotFound, "Katman bulunamadı: " + std::to_string(l) + ".");

    std::string was = record->group;
    record->group   = std::move(group);
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::SetLayerGroup;
    undo_out.layer   = l;
    undo_out.str_arg = std::move(was);
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

Result<ImageId> Document::intern_image(std::span<const std::byte> bytes, std::string_view origin)
{
    return images_.intern(bytes, origin);
}

Result<DashId> Document::intern_dash(const DashPattern& pattern, std::string_view origin)
{
    return dashes_.intern(pattern, origin);
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
    case Op::Kind::SetLayerStyle: return set_layer_style(op.layer, op.style_arg, inverse);
    case Op::Kind::SetLayerGroup: return set_layer_group(op.layer, op.str_arg, inverse);
    case Op::Kind::SetCrs: return set_crs(op.crs_arg, inverse);
    case Op::Kind::SetGuides: {
        // The inverse of "restore this list" is "restore the list that is here
        // now", which is what makes a guide change redoable as well as undoable.
        inverse              = Op{};
        inverse.kind         = Op::Kind::SetGuides;
        inverse.guide_axes   = guides_.axes();
        inverse.guide_coords = guides_.coordinates();
        set_guides(op.guide_axes, op.guide_coords);
        return ok();
    }
    case Op::Kind::SetGeometry: return restore_geometry(op.entity, op.geometry_slot, inverse);
    case Op::Kind::SetEntityLayer: return set_entity_layer(op.entity, op.layer, inverse);
    }
    return err(ErrorCode::Internal, "İşlenmemiş Op::Kind");
}

} // namespace kentos::core
