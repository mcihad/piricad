// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: a suggestion, and the fact that it is only a suggestion.
//
// WHAT HAPPENS WHEN AN AGENT CALLS A WRITE TOOL. Nothing is applied. The call is
// compiled into a `Plan` — command ids with resolved arguments, and the exact
// command lines a person can read — and the answer the client gets is the plan's
// id and its state. The drawing is untouched until somebody at the workstation
// applies it (`ai::Gate`), and a plan that is rejected or withdrawn leaves the
// document bit-identical (ai.md R20).
//
// THIS IS NOT A POLICY THIS FILE CHOSE. CLAUDE.md 5.7 and ai.md R3/P1 forbid
// applying AI output without a preview and an explicit human approval, with no
// trust mode, no setting and no flag — because a cadastral or zoning output is a
// legal document and only a licensed engineer may stand behind it (ai.md R8).
// The maintainer was asked and confirmed it: an external agent composes, a person
// applies.
//
// ONE PLAN IS ONE UNDO STEP (ai.md R4). Applying runs every step inside one
// batch, so eleven commands collapse into one `UndoStack` entry and one Ctrl+Z
// puts all of it back; a refusal part way through rolls the whole plan back
// (Article 1.6).
#pragma once

#include "kentos_cad/ai/handles.hpp"

#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::ai {

/// Where a plan is in its short life.
enum class PlanState : std::uint8_t {
    Pending,   ///< waiting for a person
    Applied,   ///< applied, as one transaction
    Rejected,  ///< the person said no
    Withdrawn, ///< the client went away (it closed the stream) before a decision
    Failed,    ///< applying it was refused by validation; nothing was applied
};

/// The state's Turkish word, for a client's answer and for the card.
const char* plan_state_name(PlanState state);

/// One command inside a plan.
struct PlanStep
{
    std::string command_id; ///< a registered id, resolved from the tool name
    command::Args args;     ///< arguments with every handle already resolved
    std::string line;       ///< the command line a person reads, e.g. `ÇİZGİ 0,0 10,10`
    std::vector<std::string> handles; ///< which handles the arguments came from
};

/// A whole suggestion.
struct Plan
{
    std::string id;                      ///< `p` + 16 hex digits
    std::vector<PlanStep> steps;         ///< the commands, in the order they run
    PlanState state{PlanState::Pending}; ///< where the plan stands

    /// Who asked. For the MCP server this is the client's declared name and its
    /// token's fingerprint; for the chat it is the provider and model. It reaches
    /// the audit record and never a credential (ai.md P11).
    std::string requester;

    std::string prompt;   ///< the request the steps came from, as the client stated it
    std::string model;    ///< model identity and version, when there is one
    std::string endpoint; ///< provider kind and endpoint, when there is one

    std::uint64_t revision{0}; ///< the document revision it was composed against
    std::string refusal;       ///< why it failed or was rejected, when it was

    // ---- what applying it left behind (TODOS C-03) --------------------------
    //
    // "PLANNED", "WAITING", "APPLIED" AND "PRODUCED A FILE" ARE DIFFERENT STATES.
    // `PlanState` says the first three; these say the fourth and what it cost, so
    // a client does not have to infer a result from a state word.

    /// The document revision after it was applied. Zero until it was.
    std::uint64_t applied_revision{0};

    /// What the whole plan put on the undo stack — one entry, because a plan is
    /// one batch (ai.md R4). Empty until it was applied.
    std::string undo_label;

    /// Files the plan wrote, in the order they were written. Empty for a plan
    /// that only drew.
    std::vector<std::string> outputs;

    /// What the plan could not honour without failing. Not errors: a sheet that
    /// printed with one broken map link did print.
    std::vector<std::string> warnings;

    /// Which policy decided, and why. Empty when a person decided, in which case
    /// the audit record names them instead (S-06).
    std::string decided_by;

    /// What the client is told: id, state, the lines, and the rule that a person
    /// must apply it. Never the raw arguments — the lines are the readable form.
    core::Json to_json() const;
};

/// The plans one program has open. Bounded, because a client that composes and
/// never waits for an answer must not grow the program's memory.
class PlanStore
{
public:
    /// Beyond this many pending plans the oldest pending one is withdrawn.
    static constexpr std::size_t kMaxPending = 32;

    /// Files a new plan and returns its id.
    std::string add(Plan plan);

    /// Appends a step to a pending plan, so an agent can compose a sequence that
    /// one approval applies (ai.md R4). Refuses a plan that is not pending.
    core::Status append(std::string_view id, PlanStep step);

    Plan* find(std::string_view id);
    const Plan* find(std::string_view id) const;

    /// Marks the outcome. The only writer is `Gate`, and only after a person has
    /// decided; nothing else may move a plan out of `Pending`.
    core::Status settle(std::string_view id, PlanState state, std::string refusal = {});

    /// Every plan still waiting, oldest first — what the suggestion panel lists.
    std::vector<const Plan*> pending() const;

    std::size_t size() const noexcept { return plans_.size(); }

private:
    std::vector<Plan> plans_;
    std::uint64_t filed_{0};
};

} // namespace kentos::ai
