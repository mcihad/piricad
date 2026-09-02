// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the layer table.
//
// .claude/model.md R30–R33. A layer is identified by its LayerKey, never by name
// or slot: renaming a layer must not touch a single entity, and a slot is not
// stable across a save. QGIS separates the layer registry from the layer tree for
// the same reason, and merging them turns a regroup into a data migration.
//
// The tree — grouping, ordering, presentation — is a separate structure and is a
// Phase-1 deliverable. This is the table.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/style.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// Scale denominator, as in 1:`value`. Zero means unbounded.
using ScaleDenominator = std::uint32_t;

struct Layer
{
    /// PERSISTENT identity. The slot a layer sits at is an allocation detail; this
    /// is what a file and a journal refer to (model.md R1-R5).
    LayerKey key{LayerKey::None};

    std::string name;        ///< free text, what the user typed
    std::string folded;      ///< turkish_upper(name), the uniqueness key
    std::string description; ///< free text, shown in the property panel

    /// Where this layer sits in the layer TREE, levels separated by `>`.
    ///
    /// Empty means the root. A drawing organises its layers the way its author
    /// thinks about the work — `PLAN > SINIRLAR`, `KADASTRO > PARSEL` — and that
    /// grouping is part of the drawing rather than of the window: it is written
    /// to the file, it comes back on another machine, and it is what somebody
    /// opening the sheet in five years reads first.
    ///
    /// Same separator as the symbol shelf's group path, and for the same reason:
    /// `>` is what MPYY prints between the levels of its own section paths, and no
    /// name in the package contains it.
    std::string group;

    bool visible{true};
    bool locked{false};   ///< entities on it are drawn but cannot be selected
    bool plottable{true}; ///< off = draw on screen, omit from the pafta

    Appearance appearance{}; ///< the ByLayer source for this layer's entities

    /// The full ByLayer symbol. `appearance` remains the fixed-width cascade
    /// baseline; this id retains a fill/boundary/marker stack selected for the
    /// layer even before it owns an entity.
    StyleId style{kByLayerStyle};

    ScaleDenominator min_scale{0}; ///< hide when zoomed out past 1:min_scale
    ScaleDenominator max_scale{0}; ///< hide when zoomed in past 1:max_scale
    std::uint8_t opacity{255};     ///< screen only; a pafta is plotted opaque

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
    /// Builds a table already holding layer 0, which every CAD document has and
    /// which cannot be removed.
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

} // namespace kentos::core
