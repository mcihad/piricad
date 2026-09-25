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

/// `BlockDef::flags`. An EXTERNAL REFERENCE (TODOS C-13/C-14) is a definition
/// whose members come from a file: loaded when the drawing opens and whenever
/// it is reloaded, drawn and snapped to through its references like any block,
/// edited by nobody here, and NEVER written to the project file — the file
/// holds the name and the path, the source holds the drawing.
///
/// `unsigned` rather than `std::uint8_t`, the field's type: two byte constants
/// combined (`kBlockExternal | kBlockDependent`, `~kBlockUnloaded`) are promoted
/// to a signed `int`, and a mask built of signed bits is what a flag test must
/// not lean on. Stored, each is narrowed back to the byte it fits.
inline constexpr unsigned kBlockExternal = 1u << 0;
/// An external reference the user unloaded: kept, drawn empty, not loaded on
/// open until loaded again.
inline constexpr unsigned kBlockUnloaded = 1u << 1;
/// A block an external reference's FILE defines, named `REF|BLOCK`: filled and
/// emptied with its external reference, never written.
inline constexpr unsigned kBlockDependent = 1u << 2;
/// An external reference taken off the drawing (`DIŞREFERANS islem=kaldir`):
/// the table is append-only, so the record stays, empty and never loaded.
inline constexpr unsigned kBlockDetached = 1u << 3;

/// The file name at the end of `path`, under either separator — what an
/// external reference's fingerprint holds of where it points.
std::string_view file_name_of(std::string_view path) noexcept;

/// One block definition.
struct BlockDef
{
    std::string name;        ///< as the user typed it
    std::string folded;      ///< Turkish-folded key, unique in the table
    std::string description; ///< free text, may be empty
    Point2 base{};           ///< the base point references are placed by

    /// The definition's entities, by persistent key (R1: a stored membership is
    /// a key, never a slot). APPEND-ONLY like the table: `BLOKDÜZENLE` writes a
    /// changed member in as a new one and takes the old one out by killing it,
    /// so a key here may name a dead entity — every reader skips those, and a
    /// file writes them dead and reads them back so (TODOS C-13).
    std::vector<EntityKey> members;

    /// The blocks this definition's members REFERENCE, so a cycle can be refused
    /// without decoding a payload: a definition that contains itself, at any
    /// depth, would expand for ever.
    std::vector<BlockId> uses;

    std::uint8_t flags{0}; ///< `kBlockExternal` and its companions; zero for a drawn block

    /// An external reference's file, as stored: relative to the project file
    /// when it could be written so, which is what lets a project folder move.
    std::string path;

    /// Whether the members come from somewhere else and are never written.
    bool external() const noexcept { return (flags & (kBlockExternal | kBlockDependent)) != 0; }
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

    /// Moves the base point of `id` — BLOKDÜZENLE's `taban` (TODOS C-13). The
    /// references are the caller's to keep in place; this is the one field.
    Status set_base(BlockId id, Point2 base);

    /// Makes `id` an external reference to `path` — or, with `flags` zero and
    /// no path, a drawn block again (`DIŞREFERANS islem=bagla`).
    Status set_external(BlockId id, std::string path, std::uint8_t flags);

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
