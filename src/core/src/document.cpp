// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/document.hpp"

#include "piricad/core/spatial_index.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::core {
namespace {

/// Grows geometrically, so appending N entities is O(N) rather than O(N²).
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
    // "0" is the default layer in every CAD lineage the users come from.
    ensure_layer("0");
}

LayerId Document::find_layer(std::string_view name) const
{
    const std::string folded = turkish_upper(name);
    for (std::size_t i = 0; i < layers_.size(); ++i)
        if (layers_[i].folded == folded) return static_cast<LayerId>(i);
    return kNoLayer;
}

const Layer* Document::layer(LayerId id) const
{
    return id < layers_.size() ? &layers_[id] : nullptr;
}

bool Document::alive(EntityId e) const
{
    return e < poly_.size() && poly_.alive[e] != 0;
}

Box2 Document::entity_extent(EntityId e) const
{
    // Read the cached box rather than walking the vertices: this call sits in the
    // per-frame culling loop (piricad.md §10.5).
    return alive(e) ? poly_.box_of(e) : Box2{};
}

Box2 Document::extent() const
{
    Box2 b{};
    for (EntityId e = 0; e < poly_.size(); ++e) {
        if (!poly_.alive[e]) continue;
        if (!layers_[poly_.layer[e]].visible) continue;

        if (b.empty()) {
            b = poly_.box_of(e);
            continue;
        }
        if (poly_.min_x[e] < b.min_x) b.min_x = poly_.min_x[e];
        if (poly_.min_y[e] < b.min_y) b.min_y = poly_.min_y[e];
        if (poly_.max_x[e] > b.max_x) b.max_x = poly_.max_x[e];
        if (poly_.max_y[e] > b.max_y) b.max_y = poly_.max_y[e];
    }
    return b;
}

std::uint64_t Document::content_hash() const
{
    std::uint64_t h = fnv1a(crs_.id());

    for (const auto& l : layers_) {
        h = fnv1a(l.folded, h);
        h = fnv1a_int(l.visible ? 1 : 0, h);
        h = fnv1a_int(l.locked ? 1 : 0, h);
        h = fnv1a_int(static_cast<std::int64_t>(l.style.rgba), h);
    }

    for (EntityId e = 0; e < poly_.size(); ++e) {
        if (!poly_.alive[e]) continue;
        h             = fnv1a(layers_[poly_.layer[e]].folded, h);
        const auto xs = poly_.xs_of(e);
        const auto ys = poly_.ys_of(e);
        for (std::size_t i = 0; i < xs.size(); ++i) {
            h = fnv1a_int(xs[i], h);
            h = fnv1a_int(ys[i], h);
        }
        h = fnv1a_int(-1, h); // entity terminator
    }
    return h;
}

LayerId Document::ensure_layer(std::string_view name)
{
    if (const LayerId existing = find_layer(name); existing != kNoLayer) return existing;

    Layer l;
    l.name   = std::string(name);
    l.folded = turkish_upper(name);
    layers_.push_back(std::move(l));
    layer_live_.push_back(0);
    ++revision_;
    return static_cast<LayerId>(layers_.size() - 1);
}

Result<EntityId> Document::add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_[lyr].locked)
        return err(ErrorCode::ValidationFailed, "'" + layers_[lyr].name + "' katmanı kilitli.");
    if (pts.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Bir çoklu çizgi en az 2 tepe noktası ister, verilen: " +
                       std::to_string(pts.size()));

    const auto start = static_cast<std::uint32_t>(poly_.xs.size());

    // Reserve only when the exact request would otherwise force a reallocation,
    // and then grow geometrically. Calling reserve(size() + n) unconditionally
    // pins capacity to the exact size and turns bulk loading into O(n²) copying.
    reserve_for(poly_.xs, pts.size());
    reserve_for(poly_.ys, pts.size());

    for (const auto& p : pts) {
        poly_.xs.push_back(p.x);
        poly_.ys.push_back(p.y);
    }

    Mm bx0 = pts.front().x, by0 = pts.front().y;
    Mm bx1 = bx0, by1 = by0;
    for (const auto& p : pts) {
        if (p.x < bx0) bx0 = p.x;
        if (p.y < by0) by0 = p.y;
        if (p.x > bx1) bx1 = p.x;
        if (p.y > by1) by1 = p.y;
    }

    const auto id = static_cast<EntityId>(poly_.start.size());
    poly_.start.push_back(start);
    poly_.count.push_back(static_cast<std::uint32_t>(pts.size()));
    poly_.layer.push_back(lyr);
    poly_.alive.push_back(1);
    poly_.min_x.push_back(bx0);
    poly_.min_y.push_back(by0);
    poly_.max_x.push_back(bx1);
    poly_.max_y.push_back(by1);
    ++revision_;
    ++live_count_;
    ++layer_live_[lyr];

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id;
    undo_out.bool_arg = false;
    return id;
}

Status Document::set_entity_alive(EntityId e, bool a, Op& undo_out)
{
    if (e >= poly_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const bool was = poly_.alive[e] != 0;
    poly_.alive[e] = a ? 1 : 0;
    if (was != a) {
        const std::size_t delta = a ? 1 : static_cast<std::size_t>(-1);
        live_count_ += delta;
        layer_live_[poly_.layer[e]] += delta;
    }
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_visible(LayerId l, bool v, Op& undo_out)
{
    if (l >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was     = layers_[l].visible;
    layers_[l].visible = v;
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerVisible;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_locked(LayerId l, bool v, Op& undo_out)
{
    if (l >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was    = layers_[l].locked;
    layers_[l].locked = v;
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerLocked;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_style(LayerId l, LayerStyle s, Op& undo_out)
{
    if (l >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const LayerStyle was = layers_[l].style;
    layers_[l].style     = s;
    ++revision_;

    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetLayerStyle;
    undo_out.layer     = l;
    undo_out.style_arg = was;
    return ok();
}

Status Document::set_crs(std::string id, Op& undo_out)
{
    if (id.empty()) return err(ErrorCode::InvalidArgument, "Koordinat sistemi kimliği boş olamaz.");

    std::string was = crs_.id();
    crs_            = Crs(std::move(id));
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::SetCrs;
    undo_out.str_arg = std::move(was);
    return ok();
}

Status Document::apply(const Op& op, Op* undo_out)
{
    Op scratch;
    Op& inverse = undo_out ? *undo_out : scratch;

    switch (op.kind) {
    case Op::Kind::None: return ok();
    case Op::Kind::SetEntityAlive: return set_entity_alive(op.entity, op.bool_arg, inverse);
    case Op::Kind::SetLayerVisible: return set_layer_visible(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerLocked: return set_layer_locked(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerStyle: return set_layer_style(op.layer, op.style_arg, inverse);
    case Op::Kind::SetCrs: return set_crs(op.str_arg, inverse);
    }
    return err(ErrorCode::Internal, "İşlenmemiş Op::Kind");
}

const SpatialIndex& Document::spatial_index() const
{
    if (!index_) index_ = std::make_unique<SpatialIndex>();

    const std::size_t total = poly_.size();
    const std::size_t added = total - indexed_upto_;

    // Rebuild when the unindexed tail has grown past 5% of what is indexed, or
    // when enough entities have been erased that the tree mostly points at
    // corpses. Otherwise the tail is short and scanning it directly is cheaper
    // than repacking the tree.
    const std::size_t growth_limit = std::max<std::size_t>(4096, indexed_upto_ / 20);
    const bool grew                = added > growth_limit;
    const bool shrank              = indexed_live_ > 1024 && live_count_ * 2 < indexed_live_;
    const bool virgin              = index_->empty() && total > 0;

    if (grew || shrank || virgin) {
        index_->build(poly_);
        indexed_upto_ = static_cast<EntityId>(total);
        indexed_live_ = live_count_;
    }
    return *index_;
}

} // namespace piricad::core
