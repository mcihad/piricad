// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: appearance and the interned style column.
//
// .claude/model.md R13–R19. The reconciliation between CAD and GIS, stated once:
//
//   **A GIS renderer is a command that writes the style column.
//     Style is never derived at frame time.**
//
// CAD gives every object its own colour and linetype; GIS derives appearance
// from a per-layer renderer over feature attributes. PiriCAD needs both, and the
// way to have both without paying for it every frame is to resolve at commit
// time inside a Transaction and materialise the answer into one u32 per entity.
// The renderer then reads an index; it never evaluates a rule, an expression or
// a cascade.
//
// This is Esri's RuleID column, and Esri ships it at cadastral scale.
#pragma once

#include "piricad/core/identity.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace piricad::core {

/// Where a single appearance property gets its value. In memory the cascade is
/// this enum; the wire sentinels (DXF 62 == 256, 370 == -1, "BYLAYER") exist
/// only inside /src/io (R19).
enum class Source : std::uint8_t {
    Explicit = 0, ///< the value in this record
    ByLayer  = 1, ///< take the layer's value
    ByBlock  = 2, ///< take the containing block reference's value
};

/// One resolved appearance. POD, trivially copyable, deduplicated in a table.
/// Carries line AND area symbology: MPYY EK-1 plan gösterim is overwhelmingly
/// area symbology, and a line-only record cannot represent an imar planı (R18).
struct Appearance
{
    std::uint32_t rgba{0xFF6C7686u}; ///< 0xAARRGGBB, stroke colour
    std::int32_t width_um{0};        ///< PAPER micrometres, 1 µm = 1/1000 mm (R20)
    std::uint16_t dash{0};           ///< index into the dash table, from /data
    std::uint16_t symbol{0};         ///< index into the MPYY/BÖHHBÜY symbol atlas
    std::uint32_t fill_rgba{0};      ///< 0 = no fill
    std::uint16_t hatch{0};          ///< index into the hatch table, from /data
    std::int16_t z_order{0};         ///< MPYY prescribes a draw order

    Source src_colour{Source::ByLayer};
    Source src_width{Source::ByLayer};
    Source src_dash{Source::ByLayer};
    Source src_fill{Source::ByLayer};

    friend bool operator==(const Appearance&, const Appearance&) = default;
};

static_assert(sizeof(Appearance) <= 32, "Appearance is interned; keep it small");

/// Index into a Document's StyleTable. Slot 0 always means "inherit everything
/// from my layer", so the overwhelmingly common case costs a zero.
using StyleId = std::uint32_t;

inline constexpr StyleId kByLayerStyle = 0;

/// Deduplicated appearance pool owned by the Document.
///
/// In a real cadastral drawing almost every entity is pure ByLayer or pure
/// ByCatalog, so interning collapses millions of entities onto a handful of
/// distinct values. That is what keeps the batch key `(layer, style, kind)`
/// bounded and the draw-call count under the §10.3 target of 100 per frame.
class StyleTable
{
public:
    StyleTable();

    /// Returns the id of `a`, adding it if new. Deterministic: the same sequence
    /// of interns always produces the same ids, because golden fixtures depend on
    /// it (§7.3).
    StyleId intern(const Appearance& a);

    const Appearance& at(StyleId id) const;

    bool contains(StyleId id) const noexcept { return id < entries_.size(); }

    std::size_t size() const noexcept { return entries_.size(); }

    const std::vector<Appearance>& entries() const noexcept { return entries_; }

    /// Folds a hash of every entry into `seed`, for `Document::content_hash()`.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    struct Hash
    {
        std::size_t operator()(const Appearance& a) const noexcept;
    };

    std::vector<Appearance> entries_;
    std::unordered_map<Appearance, StyleId, Hash> intern_;
};

/// Applies the cascade to produce the appearance actually drawn. Called at
/// COMMIT time by the command that changed something, never per frame (R14).
Appearance resolve_appearance(const Appearance& own, const Appearance& layer_default);

} // namespace piricad::core
