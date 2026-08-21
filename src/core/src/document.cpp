// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/document.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::core {

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

std::size_t Document::live_entity_count() const
{
    return static_cast<std::size_t>(
        std::count(poly_.alive.begin(), poly_.alive.end(), std::uint8_t{1}));
}

Box2 Document::entity_extent(EntityId e) const
{
    Box2 b{};
    if (!alive(e)) return b;
    const auto xs = poly_.xs_of(e);
    const auto ys = poly_.ys_of(e);
    for (std::size_t i = 0; i < xs.size(); ++i)
        b.extend(Point2{xs[i], ys[i]});
    return b;
}

Box2 Document::extent() const
{
    Box2 b{};
    for (EntityId e = 0; e < poly_.size(); ++e) {
        if (!poly_.alive[e]) continue;
        const auto& lyr = layers_[poly_.layer[e]];
        if (!lyr.visible) continue;
        b.extend(entity_extent(e));
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
    ++revision_;
    return static_cast<LayerId>(layers_.size() - 1);
}

Result<EntityId> Document::add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Unknown layer id " + std::to_string(lyr));
    if (layers_[lyr].locked)
        return err(ErrorCode::ValidationFailed, "Layer '" + layers_[lyr].name + "' is locked");
    if (pts.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "A polyline needs at least 2 vertices, got " + std::to_string(pts.size()));

    const auto start = static_cast<std::uint32_t>(poly_.xs.size());
    poly_.xs.reserve(poly_.xs.size() + pts.size());
    poly_.ys.reserve(poly_.ys.size() + pts.size());
    for (const auto& p : pts) {
        poly_.xs.push_back(p.x);
        poly_.ys.push_back(p.y);
    }

    const auto id = static_cast<EntityId>(poly_.start.size());
    poly_.start.push_back(start);
    poly_.count.push_back(static_cast<std::uint32_t>(pts.size()));
    poly_.layer.push_back(lyr);
    poly_.alive.push_back(1);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id;
    undo_out.bool_arg = false;
    return id;
}

Status Document::set_entity_alive(EntityId e, bool a, Op& undo_out)
{
    if (e >= poly_.size())
        return err(ErrorCode::NotFound, "Unknown entity id " + std::to_string(e));

    const bool was = poly_.alive[e] != 0;
    poly_.alive[e] = a ? 1 : 0;
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
        return err(ErrorCode::NotFound, "Unknown layer id " + std::to_string(l));

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
        return err(ErrorCode::NotFound, "Unknown layer id " + std::to_string(l));

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
        return err(ErrorCode::NotFound, "Unknown layer id " + std::to_string(l));

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
    if (id.empty()) return err(ErrorCode::InvalidArgument, "CRS id must not be empty");

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
    return err(ErrorCode::Internal, "Unhandled Op::Kind");
}

} // namespace piricad::core
