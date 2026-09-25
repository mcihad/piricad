// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: where a derived object came from (TODOS F-02).
//
// AN ANALYSIS RESULT KNOWS ITS ORIGIN. A buffer drawn round a well, the two
// parcels an ifraz made of one, the piece a trim left, the boundary SINIR found
// between six lines: each is a new object, with a new key, and until now the
// only thing that remembered what it was made from was the answer of the one
// call that made it — gone the moment the next command ran, and never in the
// file. So a derived object carries its ORIGIN: the operation that made it (the
// command's or the processing tool's stable id) and the objects it was made
// from, by key. It is kept with the drawing, saved and read back with it,
// undone with the edit that set it, and folded into the fingerprint, so the
// same work done by any client leaves the same record.
//
// A KEY IS NEVER REUSED (model.md R1), so an origin that names a parent the
// same command erased — an ifraz takes its parcel off the sheet — still names
// exactly that parcel: its row stays in the document and in the file, dead.
//
// NOT A TIE. A caption that follows a parcel (core/attach.hpp), a dimension
// that measures one (core/dimension_link.hpp), a hatch that fills one
// (core/hatch_link.hpp) are kept up to date when the parcel changes. An origin
// is history: the buffer was drawn from these wells as they were then, and it
// does not move when they do.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kentos::core {

/// What a derived object was made by and from.
struct Lineage
{
    std::string operation;          ///< the command's or the tool's id: `islem.tampon`
    std::vector<EntityKey> sources; ///< the objects it was made from, ascending, none twice

    /// Member-wise equality.
    friend bool operator==(const Lineage&, const Lineage&) = default;
};

/// Every derived object's origin, by its entity row — a row never moves, where
/// a slot changes on every edit (model.md R46).
class LineageTable
{
public:
    /// The origin of `e`, or null when it has none.
    const Lineage* get(EntityId e) const;

    /// Records, or replaces, the origin of `e`; an origin with no operation
    /// clears it.
    void set(EntityId e, Lineage origin);

    /// Every object that has an origin, ascending: the order the writer and the
    /// fold walk.
    std::vector<EntityId> derived() const;

    /// The objects made from `source`, ascending; `out` is cleared first.
    void made_from(EntityKey source, std::vector<EntityId>& out) const;

    bool empty() const noexcept { return rows_.empty(); }

    std::size_t size() const noexcept { return rows_.size(); }

    /// Folds into the document hash; the seed comes back unchanged when the
    /// table is empty, so every drawing without a derived object keeps its
    /// fingerprint.
    std::uint64_t fold(std::uint64_t seed, std::span<const std::uint32_t> position = {}) const;

private:
    std::map<EntityId, Lineage> rows_;
};

/// `sources` sorted, with no key twice — the form an `Lineage` stores.
std::vector<EntityKey> lineage_sources(std::span<const EntityKey> sources);

/// The origin as bytes — what an undo record carries. An empty origin encodes
/// to "none".
std::vector<std::uint8_t> encode_lineage(const Lineage* origin);

/// The origin back — an empty operation for "none" — refused when the bytes
/// are not what the encoder writes.
Result<Lineage> decode_lineage(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
