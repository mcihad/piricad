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
/// What one layer of a symbol draws. A CAD entity needs one of these; a plan
/// gösterim usually needs several stacked.
enum class StrokeKind : std::uint8_t {
    Fill = 0, ///< the interior: colour, hatch, opacity
    Stroke,   ///< the boundary: colour, width, dash
    Marker,   ///< a repeated glyph: along a line, or at the centroid of a face
};

const char* stroke_kind_name(StrokeKind k) noexcept;

/// One layer of a symbol.
///
/// This is `Appearance` plus the two fields that make stacking meaningful: WHAT
/// this layer draws, and where it sits relative to the geometry. A symbol of one
/// Stroke layer is exactly what CAD has always had, which is why `Appearance`
/// remains the resolved single-layer form and this wraps it rather than replacing
/// it — 77 call sites, a file format and every golden fixture depend on that
/// record keeping its shape (R18).
struct SymbolLayer
{
    Appearance look{};
    StrokeKind kind{StrokeKind::Stroke};

    /// Perpendicular offset from the geometry, in PAPER micrometres (R20). A
    /// road casing is two strokes at the same offset with different widths; a
    /// boundary with an inner hatch band is a stroke and a fill at different
    /// ones.
    std::int32_t offset_um{0};

    friend bool operator==(const SymbolLayer&, const SymbolLayer&) = default;
};

/// An ordered stack of symbol layers, drawn back to front.
///
/// This is the shape QGIS reaches with QgsSymbol and its symbol layer list, and
/// the reason it is needed here is MPYY: a plan gösterim is routinely a fill, a
/// boundary stroke of a different colour, and a repeated glyph on top. A single
/// colour-and-width record cannot state one, and 476 of them are sitting in
/// /data waiting to be stated.
///
/// One layer is the overwhelmingly common case and costs one vector element.
/// The renderer walks the stack and emits one pass per layer; it still never
/// evaluates a rule (R14) — the stack was resolved at commit time like everything
/// else in the style column.
struct Symbol
{
    std::vector<SymbolLayer> layers;

    /// Below this scale denominator the symbol is not drawn; zero means always.
    /// Scale-dependent symbology is not decoration in planning work — an imar
    /// planı at 1/25000 shows a lekesi where 1/1000 shows its parcels.
    std::uint32_t min_scale{0};
    std::uint32_t max_scale{0}; ///< above this, not drawn; zero means always

    friend bool operator==(const Symbol&, const Symbol&) = default;

    /// The single-layer symbol a CAD entity has always had.
    static Symbol of(const Appearance& a)
    {
        Symbol s;
        s.layers.push_back(SymbolLayer{a, StrokeKind::Stroke, 0});
        return s;
    }

    /// The layer the CAD cascade resolves to — the first Stroke, or the first
    /// layer at all. `Appearance` is what a per-entity override and every file
    /// format field talks about, so a stack has to be able to name one.
    const Appearance& primary() const noexcept
    {
        for (const SymbolLayer& l : layers)
            if (l.kind == StrokeKind::Stroke) return l.look;
        static const Appearance fallback{};
        return layers.empty() ? fallback : layers.front().look;
    }
};

/// Folds a stack into a content hash, layer order included.
std::uint64_t fold_symbol(const Symbol& sym, std::uint64_t seed);

class StyleTable
{
public:
    StyleTable();

    /// Returns the id of `a`, adding it if new. Deterministic: the same sequence
    /// of interns always produces the same ids, because golden fixtures depend on
    /// it (§7.3).
    StyleId intern(const Appearance& a);

    /// Interns a full stack. A single-layer stack and the `Appearance` it wraps
    /// intern to the SAME id, so a drawing that never uses a stack is byte for
    /// byte the drawing it was before stacks existed.
    StyleId intern(const Symbol& sym);

    /// The resolved single-layer appearance — what the CAD cascade and every
    /// file-format field mean by "the style of this entity".
    const Appearance& at(StyleId id) const;

    /// The full stack behind `id`. Entries interned as a bare Appearance report a
    /// one-layer stack, so a caller never has to ask which form was used.
    const Symbol& symbol_at(StyleId id) const;

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

    std::vector<Appearance> entries_; ///< the resolved look, one per id
    std::vector<Symbol> symbols_;     ///< parallel to entries_, the full stack
    std::unordered_map<Appearance, StyleId, Hash> intern_;

    struct SymbolHash
    {
        std::size_t operator()(const Symbol& s) const noexcept;
    };

    std::unordered_map<Symbol, StyleId, SymbolHash> symbol_intern_;
};

/// Applies the cascade to produce the appearance actually drawn. Called at
/// COMMIT time by the command that changed something, never per frame (R14).
Appearance resolve_appearance(const Appearance& own, const Appearance& layer_default);

} // namespace piricad::core
