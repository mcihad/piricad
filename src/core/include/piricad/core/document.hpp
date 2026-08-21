// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the document model.
//
// Layout rules (piricad.md §10.2, .claude/core.md):
//   * struct-of-arrays, never std::vector<Point2>
//   * 32-bit indices, never pointers
//   * hot (geometry) and cold (attributes) blocks kept apart
//
// The Document exposes ONLY primitive, inverse-producing mutators. Higher layers
// must reach them through a Transaction, which the command bus owns.
#pragma once

#include "piricad/core/crs.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

using EntityId = std::uint32_t;
using LayerId  = std::uint32_t;

inline constexpr EntityId kNoEntity = 0xFFFFFFFFu;
inline constexpr LayerId kNoLayer   = 0xFFFFFFFFu;

struct LayerStyle
{
    std::uint32_t rgba{0xFFD0D0D0u}; ///< 0xAARRGGBB
    float width_px{1.0f};

    friend bool operator==(const LayerStyle&, const LayerStyle&) = default;
};

struct Layer
{
    std::string name;
    std::string folded; ///< turkish_upper(name), for lookup
    bool visible{true};
    bool locked{false};
    LayerStyle style{};
};

/// Structure-of-arrays polyline store. All vertices of all entities live in one
/// pair of coordinate arrays; an entity is a (start, count) window into them.
class PolylineStore
{
public:
    // ---- hot block: geometry only, never touched by attribute code ----
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    // ---- cold block: per-entity records ----
    std::vector<std::uint32_t> start;
    std::vector<std::uint32_t> count;
    std::vector<LayerId> layer;
    std::vector<std::uint8_t> alive;

    std::size_t size() const noexcept { return start.size(); }

    std::span<const Mm> xs_of(EntityId e) const { return {xs.data() + start[e], count[e]}; }

    std::span<const Mm> ys_of(EntityId e) const { return {ys.data() + start[e], count[e]}; }

    Point2 vertex(EntityId e, std::uint32_t i) const
    {
        const std::uint32_t k = start[e] + i;
        return Point2{xs[k], ys[k]};
    }
};

/// A reversible primitive edit. Produced by Document mutators, consumed by Transaction.
struct Op
{
    enum class Kind : std::uint8_t {
        None,
        SetEntityAlive,  ///< entity, bool_arg  (covers both create-undo and erase)
        SetLayerVisible, ///< layer,  bool_arg
        SetLayerLocked,  ///< layer,  bool_arg
        SetLayerStyle,   ///< layer,  style_arg
        SetCrs,          ///< str_arg
    };

    Kind kind{Kind::None};
    EntityId entity{kNoEntity};
    LayerId layer{kNoLayer};
    bool bool_arg{false};
    LayerStyle style_arg{};
    std::string str_arg;
};

class Document
{
public:
    Document();

    // ---- read API: rich and direct (performance), never mutating ----
    const Crs& crs() const noexcept { return crs_; }

    const PolylineStore& polylines() const noexcept { return poly_; }

    const std::vector<Layer>& layers() const noexcept { return layers_; }

    std::uint64_t revision() const noexcept { return revision_; }

    LayerId find_layer(std::string_view name) const;
    const Layer* layer(LayerId id) const;
    bool alive(EntityId e) const;
    std::size_t live_entity_count() const;
    Box2 extent() const;
    Box2 entity_extent(EntityId e) const;

    /// Order-independent, platform-independent content fingerprint.
    /// Two documents with the same fingerprint hold the same data.
    std::uint64_t content_hash() const;

    // ---- mutators: PRIMITIVE ONLY. Each returns the Op that undoes it. ----
    // Reaching these outside a Transaction is a constitution violation
    // (Article 1) and is caught by scripts/ci-gate-command-mutation.sh.

    /// Creates the layer if absent. Layer creation is not undoable by design:
    /// an empty layer is inert and removing it would invalidate stored ids.
    LayerId ensure_layer(std::string_view name);

    Result<EntityId> add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out);
    Status set_entity_alive(EntityId e, bool alive, Op& undo_out);
    Status set_layer_visible(LayerId l, bool visible, Op& undo_out);
    Status set_layer_locked(LayerId l, bool locked, Op& undo_out);
    Status set_layer_style(LayerId l, LayerStyle s, Op& undo_out);
    Status set_crs(std::string id, Op& undo_out);

    /// Applies a previously produced Op. Used only by Transaction rollback and by
    /// the undo stack. When `undo_out` is non-null it receives the Op that reverses
    /// this one, which is how undo builds its redo record.
    Status apply(const Op& op, Op* undo_out = nullptr);

private:
    Crs crs_{};
    PolylineStore poly_{};
    std::vector<Layer> layers_{};
    std::uint64_t revision_{0};
};

} // namespace piricad::core
