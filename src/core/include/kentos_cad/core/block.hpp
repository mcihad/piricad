// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: block definitions.
//
// model.md R45. A block is a named group of entities drawn once and placed many
// times: a manhole symbol, a north arrow, a title block. The DEFINITION's
// entities live in the same entity table as everything else, flagged
// `FlagInBlock` so the cull, the index and the pick never see them directly;
// a `core.block_reference` entity (Phase 2, C4) draws them at its own insertion
// point, scale and rotation.
//
// The table is APPEND-ONLY, like the layer table and for the same reason: a
// block id reaches the file and the payload of every reference, so a retired
// one can never be reused. Names are unique under Turkish folding, exactly as
// layer names are.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Dense index into the document's block table. Reaches the file (R45).
using BlockId = std::uint32_t;

/// "Not in a block", and "no block referenced".
inline constexpr BlockId kNoBlock = 0xFFFFFFFFu;

/// One block definition.
struct BlockDef
{
    std::string name;        ///< as the user typed it
    std::string folded;      ///< Turkish-folded key, unique in the table
    std::string description; ///< free text, may be empty
    Point2 base{};           ///< the base point references are placed by

    /// The definition's entities, by persistent key (R1: a stored membership is
    /// a key, never a slot). Fixed at creation of each member; `BLOKDÜZENLE`
    /// (Phase 2) is what changes it.
    std::vector<EntityKey> members;

    /// The blocks this definition's members REFERENCE, so a cycle can be refused
    /// without decoding a payload: a definition that contains itself, at any
    /// depth, would expand for ever.
    std::vector<BlockId> uses;

    std::uint8_t flags{0}; ///< reserved for the file; zero today
};

/// The block definitions of one document (R45).
class BlockTable
{
public:
    /// Adds a definition. Refuses an empty name and a name already taken under
    /// Turkish folding ("Kapak" and "KAPAK" are the same block).
    Result<BlockId> add(std::string_view name, std::string_view description, Point2 base);

    /// The block with `name` (folded), or kNoBlock.
    BlockId find(std::string_view name) const;

    const BlockDef& at(BlockId id) const noexcept { return defs_[id]; }

    std::size_t size() const noexcept { return defs_.size(); }

    const std::vector<BlockDef>& all() const noexcept { return defs_; }

    /// Records `member` as part of `id`; `uses` names the block that member
    /// references, or kNoBlock. Refuses a membership that would make a cycle.
    Status add_member(BlockId id, EntityKey member, BlockId uses);

    /// Whether placing a reference to `referenced` inside `container` would let
    /// `container` reach itself: true when `referenced` is `container` or uses
    /// it at any depth.
    bool would_cycle(BlockId container, BlockId referenced) const;

    /// Folds into the document hash. An empty table folds to the seed unchanged.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<BlockDef> defs_;
};

} // namespace kentos::core
