// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: which objects a hatch's boundary was taken from.
//
// A HATCH THAT DOES NOT FOLLOW ITS BOUNDARY FILLS SOMETHING THAT IS NOT THERE
// (TODOS C-11). A parcel's corner moves, a courtyard's hole is redrawn, and a
// hatch that kept its own copy of the old loops now spills into the courtyard
// and stops short of the new corner. So a hatch drawn over objects remembers
// them — by key, in a table of its own — and at the end of every transaction
// that reshapes one of them its loops are built again from what they are now,
// in the same undo step. An object erased, or one that no longer closes, is kept
// as a BROKEN source: the hatch stays as it was and says it no longer follows.
//
// THE DXF FLAG IS NOT THIS. A file's associative bit (group 71) says the hatch
// was associative where it was drawn; it names no object this program has, so
// it is kept for the round trip and counted as nothing more.
//
// WHICH LOOP IS A HOLE IS DERIVED, NEVER STORED: every closed ring and closed
// drawn run of the sources, nested by containment. A loop inside an odd number
// of the others is a hole, the way a DXF hatch in normal style fills — so a
// parcel's courtyard, an island parcel drawn inside it and an island inside
// that island all come out right from the geometry alone, today and after every
// edit.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace kentos::core {

/// The document the table lives in; `hatch_boundary` reads its geometry.
class Document;

/// One object a hatch's boundary was taken from.
struct HatchSource
{
    EntityKey source{EntityKey::None}; ///< the object, by key (model.md R1)
    bool broken{false};                ///< erased, or no longer closed: no longer followed

    /// Member-wise equality.
    friend bool operator==(const HatchSource&, const HatchSource&) = default;
};

/// Every linked hatch's sources, by the hatch's entity row — a row never moves,
/// where a slot changes on every edit (model.md R46).
class HatchLinkTable
{
public:
    /// The sources of `hatch`, or null when it has none.
    const std::vector<HatchSource>* get(EntityId hatch) const;

    /// Replaces the sources of `hatch`; an empty list clears it.
    void set(EntityId hatch, std::vector<HatchSource> sources);

    /// Every hatch that has sources, ascending: the order the writer, the fold
    /// and the commit-time update walk.
    std::vector<EntityId> linked() const;

    bool empty() const noexcept { return rows_.empty(); }

    std::size_t size() const noexcept { return rows_.size(); }

    /// Folds into the document hash; the seed comes back unchanged when the
    /// table is empty, so every drawing without a linked hatch keeps its
    /// fingerprint.
    std::uint64_t fold(std::uint64_t seed, std::span<const std::uint32_t> position = {}) const;

private:
    std::map<EntityId, std::vector<HatchSource>> rows_;
};

/// The sources as bytes — what an undo record carries.
std::vector<std::uint8_t> encode_hatch_links(std::span<const HatchSource> sources);

/// The sources back, refused when the bytes are not what the encoder writes.
Result<std::vector<HatchSource>> decode_hatch_links(std::span<const std::uint8_t> bytes);

/// A hatch's rings as its boundary objects make them: the loops, each with its
/// role and its part.
struct HatchBoundary
{
    std::vector<std::vector<Point2>> loops; ///< closed, the first vertex not repeated
    std::vector<RingRole> roles;            ///< Exterior or Interior, per loop
    std::vector<std::uint16_t> parts;       ///< the part each loop belongs to

    /// The rings, as `RingGeometry` takes them; the spans point into `loops`.
    std::vector<RingGeometry::RingInput> rings() const;
};

/// The closed loops of one object: a face's rings, a curve's drawn outline —
/// every closed run of it. Empty for an object that closes nothing.
std::vector<std::vector<Point2>> closed_loops_of(const Document& doc, EntityId e);

/// Closed `loops` nested by containment in `style` (see `hatch_boundary`): the
/// rings a hatch's own loops make when there are no sources to read them from.
HatchBoundary nest_loops(std::vector<std::vector<Point2>> loops, std::uint16_t style);

/// The boundary `sources` give a hatch in `style` (DXF group 75): 0, normal —
/// loops nested by containment, a loop inside an odd number of the others a
/// hole; 1, outermost — the outer loops and their first holes only; 2, ignore —
/// the outer loops alone. An error when the sources close nothing.
Result<HatchBoundary> hatch_boundary(const Document& doc, std::span<const EntityId> sources,
                                     std::uint16_t style);

} // namespace kentos::core
