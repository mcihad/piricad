// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: what one step did to the drawing, counted (TODOS F-05).
//
// A COMMAND SAYS WHAT IT DID IN ITS OWN WORDS — "3 nesne taşındı" — and a batch
// said only how many commands it ran. A script of a thousand lines, an agent's
// plan, an undo step: nothing told the person, or the client, what the one
// logical operation had changed as a whole — how many objects it added and took
// away, how many it moved or reshaped, how many captions and values it
// re-wrote, including the followers that moved with their sources. This header
// reads that off the step's own undo record, which is the one complete account
// of what the step did: every edit a command, a follower settle or a nested run
// makes goes through it.
//
// NET OF ITSELF. An object drawn and erased in the same step is neither added
// nor erased; an added object is not also "reshaped" because its rings were set
// after it was born. Computed, never stored: it is a reading of the record and
// the drawing as they are when the step closes.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/json.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace kentos::command {

/// What one step changed, object by object and net of itself.
struct ChangeSummary
{
    std::size_t created{0};   ///< objects that exist now and did not before
    std::size_t erased{0};    ///< objects that existed before and do not now
    std::size_t reshaped{0};  ///< existing objects whose geometry or kind changed
    std::size_t reworded{0};  ///< existing captions whose words, height or anchor changed
    std::size_t revalued{0};  ///< existing objects with a changed attribute cell or foreign data
    std::size_t relayered{0}; ///< existing objects moved to another layer
    std::size_t restyled{0};  ///< existing objects whose own style or hidden flag changed
    std::size_t retied{0};    ///< existing objects whose tie to another object or origin changed
    std::size_t layers{0};    ///< layers whose visibility, lock, look or group changed
    bool blocks{false};       ///< a block's members, base point or external reference changed
    bool sheets{false};       ///< the sheet (layout) list changed
    bool guides{false};       ///< the guide list changed
    bool crs{false};          ///< the coordinate system changed

    /// Whether the step changed nothing this counts.
    [[nodiscard]] bool empty() const noexcept;
};

/// What `step` — a transaction's undo record, oldest first — did, read against
/// `doc` as it is now. One pass over the record and a sort of the objects it
/// touched: the cost is the step's, never the drawing's.
[[nodiscard]] ChangeSummary summarize_changes(const core::Document& doc,
                                              std::span<const core::Op> step);

/// Whether a sentence tells what a step DID or what it WOULD do: the verbs
/// change with it ("eklendi" / "eklenecek").
enum class ChangeTense : std::uint8_t {
    Done,  ///< a step that ran
    Would, ///< a step a preview ran and took back
};

/// The summary as a person reads it: one sentence in the user's language,
/// without a full stop — the additions and erasures first, then every kind of
/// change listed and joined, one verb for all (the wording
/// `docs/betik/README.md` shows). Empty when nothing changed.
[[nodiscard]] std::string describe_changes(const ChangeSummary& s,
                                           ChangeTense tense = ChangeTense::Done);

/// The summary as a client reads it: one key per count and per flag, every
/// key always present — the keys `docs/betik/README.md` documents, stable
/// across versions.
[[nodiscard]] core::Json changes_json(const ChangeSummary& s);

} // namespace kentos::command
