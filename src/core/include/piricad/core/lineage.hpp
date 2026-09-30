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
//
// BUT A RESULT KNOWS WHEN IT IS OUT OF DATE (TODOS F-04). A copy, a trim's
// piece, an ifraz's parcel are new objects in their own right; a buffer, an
// area generated from a line network, a boundary found between lines and a
// contour traced through levelled points are STATEMENTS about their sources,
// and the moment a well moves its buffer is a statement about somewhere else.
// So a result records, beside each source, the source's CONTENT REVISION when
// it was computed (`Document::content_revision`), and `check_result` compares
// that with the source as it is now. Nothing is stored that says "out of
// date": the answer is computed from the drawing itself, so an undo that puts
// the well back makes the buffer current again, on every client, with no flag
// for the undo to find.
//
// ONE RECORD FOR ONE RUN. Thirty contours traced through two thousand points
// share one origin — the same operation, sources and revisions — and the table
// holds it once; each object's row names it.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// The document the checks read.
class Document;

/// What a derived object was made by and from.
struct Lineage
{
    std::string operation;          ///< the command's or the tool's id: `islem.tampon`
    std::vector<EntityKey> sources; ///< the objects it was made from, ascending, none twice

    /// A RESULT's evidence (TODOS F-04): each source's content revision when
    /// the result was computed, in the order of `sources`. Empty for HISTORY —
    /// a copy, a piece, a parcel an ifraz made — which is not a statement about
    /// its sources and cannot go out of date.
    std::vector<std::uint64_t> revisions;

    /// HOW IT WAS RUN (TODOS F-04): the arguments the operation was given, as
    /// the journal writes them (a JSON object), less the objects it read —
    /// those are `sources`. What computing the result again passes back, with
    /// the sources as they are now. Opaque here; empty for history and for a
    /// result recorded before this existed, which cannot be computed again.
    std::string arguments;

    /// Whether this origin is a result's: whether it can go out of date.
    bool result() const noexcept { return !revisions.empty(); }

    /// Member-wise equality.
    friend bool operator==(const Lineage&, const Lineage&) = default;
};

/// "This object shares no origin record", what `origin_of` says for a row with none.
inline constexpr std::uint32_t kNoOrigin = 0xFFFFFFFFu;

/// Every derived object's origin, by its entity row — a row never moves, where
/// a slot changes on every edit (model.md R46).
class LineageTable
{
public:
    /// The origin of `e`, or null when it has none.
    const Lineage* get(EntityId e) const;

    /// Records, or replaces, the origin of `e`; an origin with no operation
    /// clears it. An origin equal to one already held is shared, not copied.
    void set(EntityId e, Lineage origin);

    /// Every object that has an origin, ascending: the order the writer and the
    /// fold walk.
    std::vector<EntityId> derived() const;

    /// The objects made from `source`, ascending; `out` is cleared first.
    void made_from(EntityKey source, std::vector<EntityId>& out) const;

    /// The record `e`'s origin is held in — the same number for every object
    /// one run made — or `kNoOrigin`. What a reader keys a memo on, and what
    /// the writer writes a run's origin once by.
    std::uint32_t origin_of(EntityId e) const;

    /// The record `origin_of` named.
    const Lineage& origin(std::uint32_t index) const { return pool_[index]; }

    bool empty() const noexcept { return rows_.empty(); }

    std::size_t size() const noexcept { return rows_.size(); }

    /// Folds into the document hash; the seed comes back unchanged when the
    /// table is empty, so every drawing without a derived object keeps its
    /// fingerprint, and a history origin folds exactly as it did before
    /// results existed.
    std::uint64_t fold(std::uint64_t seed, std::span<const std::uint32_t> position = {}) const;

    /// How many origins the pool holds: what `truncate` cuts back to.
    std::size_t pool_size() const noexcept { return pool_.size(); }

    /// CUTS THE TABLE BACK (TODOS F-05): the origins of rows at or past `rows`
    /// go, and the pooled origins at or past `pool` when no row names one — what
    /// a rolled-back step recorded.
    void truncate(EntityId rows, std::size_t pool);

private:
    /// Every origin recorded this session, each once. Append-only, like the
    /// geometry arena: a record an edit replaced stays for the undo that may
    /// bring it back, and only the ones a row names are written or folded.
    std::vector<Lineage> pool_;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> by_hash_;
    std::map<EntityId, std::uint32_t> rows_;
};

// ----------------------------------------------------- is it up to date ----

/// Whether a derived object still says what its sources say (TODOS F-04).
enum class ResultState : std::uint8_t {
    History    = 0, ///< not a result — no origin, or history: nothing to be out of date with
    Current    = 1, ///< every source is there and as it was when the result was computed
    Stale      = 2, ///< a source that is still there has changed since: out of date
    Sourceless = 3, ///< nothing it reads has changed, but a source is gone: it stands alone
};

/// The state's stable machine word, for a report, a file and a test.
const char* result_state_id(ResultState s) noexcept;

/// A result compared with its sources as they are now.
struct ResultCheck
{
    ResultState state{ResultState::History}; ///< what the comparison found
    std::vector<EntityKey> changed; ///< sources still there whose content changed, ascending
    std::vector<EntityKey> gone;    ///< sources erased since, ascending
};

/// `origin` compared with `doc` as it is now. A source that changed makes the
/// result `Stale` whatever else is gone; one that is only gone, `Sourceless`.
ResultCheck check_origin(const Document& doc, const Lineage& origin);

/// The same for the object `e`: `History` when it has no result's origin.
ResultCheck check_result(const Document& doc, EntityId e);

/// Each of `sources` (ascending, as a `Lineage` holds them) with its content
/// revision in `doc` now: the evidence a result records when it is computed.
std::vector<std::uint64_t> lineage_revisions(const Document& doc,
                                             std::span<const EntityKey> sources);

/// `sources` sorted, with no key twice — the form an `Lineage` stores.
std::vector<EntityKey> lineage_sources(std::span<const EntityKey> sources);

/// The origin as bytes — what an undo record carries. An empty origin encodes
/// to "none"; a history origin encodes exactly as it did before results
/// existed, and a result's carries its revisions after its sources.
std::vector<std::uint8_t> encode_lineage(const Lineage* origin);

/// The origin back — an empty operation for "none" — refused when the bytes
/// are not what the encoder writes.
Result<Lineage> decode_lineage(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
