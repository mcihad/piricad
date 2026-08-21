// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the layer table.
//
// .claude/model.md R30–R33. A layer is identified by its LayerKey, never by name
// or slot: renaming a layer must not touch a single entity, and a slot is not
// stable across a save. QGIS separates the layer registry from the layer tree for
// the same reason, and merging them turns a regroup into a data migration.
//
// The tree — grouping, ordering, presentation — is a separate structure and is a
// Phase-1 deliverable. This is the table.
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/style.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace piricad::core {

/// Scale denominator, as in 1:`value`. Zero means unbounded.
using ScaleDenominator = std::uint32_t;

struct Layer
{
    LayerKey key{LayerKey::None};

    std::string name;   ///< free text, what the user typed
    std::string folded; ///< turkish_upper(name), the uniqueness key
    std::string description;

    bool visible{true};
    bool locked{false};
    bool plottable{true}; ///< off = draw on screen, omit from the pafta

    Appearance appearance{}; ///< the ByLayer source for this layer's entities

    ScaleDenominator min_scale{0}; ///< hide when zoomed out past 1:min_scale
    ScaleDenominator max_scale{0}; ///< hide when zoomed in past 1:max_scale
    std::uint8_t opacity{255};

    /// Default catalogue reference applied to entities drawn on this layer. The
    /// canonical per-entity value lives in the attribute column; this is the
    /// default the draw-time command copies from (R34).
    std::string catalog_ref;

    friend bool operator==(const Layer&, const Layer&) = default;
};

/// Layers by stable key, with a slot view for the frame path.
///
/// Slots stay dense and stable for the lifetime of the in-memory document; a
/// layer is never removed from the middle, only marked. That keeps `layer[e]` in
/// the entity table valid without a rewrite.
class LayerTable
{
public:
    LayerTable();

    /// Creates a layer and returns its slot. Fails if the folded name is taken.
    Result<LayerId> add(Layer layer, KeyAllocator& keys);

    /// Slot of the layer with this folded name, or kNoLayer.
    LayerId find(std::string_view name) const;

    /// Slot of the layer with this key, or kNoLayer.
    LayerId slot_of(LayerKey key) const;

    LayerKey key_of(LayerId slot) const noexcept;

    const Layer* at(LayerId slot) const noexcept;
    Layer* at(LayerId slot) noexcept;

    const std::vector<Layer>& all() const noexcept { return layers_; }

    std::size_t size() const noexcept { return layers_.size(); }

    /// Renames without touching any entity. Fails if the new folded name is taken.
    Status rename(LayerId slot, std::string name);

    /// True when the layer is drawn at this scale denominator (R32).
    bool visible_at(LayerId slot, ScaleDenominator scale) const;

    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<Layer> layers_;
    std::unordered_map<std::string, LayerId> by_folded_;
    std::unordered_map<std::uint64_t, LayerId> by_key_;
};

} // namespace piricad::core
