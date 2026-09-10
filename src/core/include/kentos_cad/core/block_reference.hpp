// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a placed block.
//
// A block definition (core/block.hpp) is drawn once; a REFERENCE places it — at a
// point, scaled, turned, mirrored, and as a grid of copies. The reference's ring
// is its insertion point alone; everything else is the payload (model.md R9a).
// The members stay where they are, flagged `FlagInBlock`, and are drawn through
// the reference by `expand_block_reference`, which is what `entity_outline`
// calls for this kind (outline.hpp) — the renderer, the pick test and the snap
// engine see the members as RUNS of the reference and never as entities.
//
// THE TRANSFORM IS EXACT INTEGER ARITHMETIC: scale is a rational applied with
// `mul_div_round`, rotation is `rotate_udeg`, translation is addition. Two
// machines place the same manhole cover on the same millimetre (§7.3).
#pragma once

#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// The document a reference draws its members from (document.hpp).
class Document;

/// The payload of a `core.block_reference` slot.
struct BlockReference
{
    BlockId block{kNoBlock};       ///< the definition placed
    Ratio sx{1, 1};                ///< scale along the definition's x; negative mirrors
    Ratio sy{1, 1};                ///< scale along the definition's y; negative mirrors
    std::int64_t rotation_udeg{0}; ///< turned counter-clockwise about the insertion point
    std::uint16_t columns{1};      ///< copies along the turned x axis
    std::uint16_t rows{1};         ///< copies along the turned y axis
    Mm column_spacing{0};          ///< between copies along x, unscaled, in the turned frame
    Mm row_spacing{0};             ///< between copies along y, unscaled, in the turned frame
    Box2 bounds{};                 ///< the drawn form's box, computed when the reference is made

    /// Member-wise equality.
    friend constexpr bool operator==(const BlockReference&,
                                     const BlockReference&) noexcept = default;
};

/// The payload layout version `encode_block_reference` writes.
inline constexpr std::uint16_t kBlockReferenceLayout = 1;

/// How deep references inside definitions are followed when drawing.
inline constexpr int kMaxBlockDepth = 32;

/// The payload bytes.
std::vector<std::uint8_t> encode_block_reference(const BlockReference& ref);

/// The payload back, refused when the bytes are not what `encode_block_reference`
/// writes, a scale has no denominator or is zero, or the grid is empty.
Result<BlockReference> decode_block_reference(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<BlockReference> block_reference_of(const RingGeometry& geom, std::uint32_t slot);

/// Where the reference in `slot` is placed.
Point2 block_reference_insertion(const RingGeometry& geom, std::uint32_t slot);

/// A definition-space point placed by the reference, for copy (`column`, `row`)
/// of its grid: `insertion + rotate(scale(p − base) + grid offset)`.
Point2 place_block_point(const BlockReference& ref, Point2 insertion, Point2 base, Point2 p,
                         int column, int row) noexcept;

/// Appends the members of the reference `e` places, transformed, as runs — a
/// member's own style, layer and caption on each run (`EmitBuffer::run_style`
/// and friends), a member on the drawing's layer `0` and a ByBlock style
/// inheriting the reference's. Nested references are followed to
/// `kMaxBlockDepth`. False when the payload does not decode or the block is
/// unknown; the caller then draws nothing for it.
bool expand_block_reference(const Document& doc, EntityId e, EmitBuffer& into, int depth = 0);

/// The same expansion for a reference that does not exist yet: `ref` placed at
/// `insertion`. What BLOKEKLE shows under the cursor before the click, drawn by
/// the code that will draw the reference once it is placed.
bool expand_block_definition(const Document& doc, Point2 insertion, const BlockReference& ref,
                             EmitBuffer& into, int depth = 0);

/// The box of the drawn form a reference with `ref` placed at `insertion` would
/// have — what a creator stores in `BlockReference::bounds` before `add_kind`.
/// Empty for a definition with no drawable member.
Box2 block_reference_bounds(const Document& doc, Point2 insertion, const BlockReference& ref);

} // namespace kentos::core
