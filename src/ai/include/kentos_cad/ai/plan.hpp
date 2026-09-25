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
// THIS IS NOT A POLICY THIS FILE CHOSE. CLAUDE.md 5.7 and ai.md R3/P1 allow AI
// output to reach the document by exactly two roads — a preview a person
// approves, or an approval policy that person set beforehand for themselves —
// and forbid every other. Which road it took is recorded, because a cadastral or
// zoning output is a legal document and only a licensed engineer may stand
// behind it (§5.2.4, ai.md R8). An external agent composes; it never applies,
// and it cannot widen the policy that decides (CLAUDE.md 5.23).
//
// ONE PLAN IS ONE UNDO STEP (ai.md R4). Applying runs every step inside one
// batch, so eleven commands collapse into one `UndoStack` entry and one Ctrl+Z
// puts all of it back; a refusal part way through rolls the whole plan back
// (Article 1.6).
#pragma once

#include "kentos_cad/ai/handles.hpp"

#include "kentos_cad/command/changes.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstddef>
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

    /// A person said yes and the steps are running NOW.
    ///
    /// NOT A DECISION — the decision was `Applied`'s to make and this is the
    /// stretch of time before it. It exists because an atlas over four hundred
    /// parcels is minutes of work, and a client polling in the middle of it was
    /// told `beklemede`: "still waiting for a person", which is false and sends
    /// the agent to ask the user why they have not clicked yet (TODOS M-06).
    ///
    /// ADDED AT THE END, like every enum whose values reach a client.
    Running,
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
    /// The relative points among them: the base handle and the dimension it was
    /// moved by (`@….0 + doğu 10000, kuzey 0 mm`), for the audit record.
    std::vector<std::string> constructions;
    /// What the client says it assumed to compose this step (`kAssumptions`).
    std::vector<std::string> assumptions;
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

    /// FILED BY THIS PROGRAM'S OWN CHAT, for the person at the keyboard. Its
    /// scope may reach outward acts (a print) — with the approval its policy
    /// asks for — where an outside client's may not; and its card is shown by
    /// the chat itself, not by the window's MCP road.
    bool in_app{false};

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

    /// What the one step changed in the drawing, counted (TODOS F-05,
    /// command/changes.hpp): what a client verifies against rather than infers
    /// from "applied". Empty until it was applied.
    command::ChangeSummary changes;

    /// What the plan could not honour without failing. Not errors: a sheet that
    /// printed with one broken map link did print.
    std::vector<std::string> warnings;

    /// Who or what decided: `insan` for a person at the card, `politika:<word>`
    /// for the approval policy the user set beforehand (S-06). Empty until it
    /// was decided.
    std::string decided_by;

    /// WHY IT IS WAITING, when the policy was asked and said a person must
    /// decide — the engine's own reason (`PolicyDecision::reason`), for the card
    /// and for the client. A plan that waits without saying why reads as a plan
    /// that is stuck.
    std::string waiting_reason;

    /// Every step's assumptions, in step order, without repeats.
    std::vector<std::string> assumptions() const;

    // ---- how far it has got, and why asking twice is safe (TODOS M-06) ------

    /// How many steps have finished, while the state is `Running`.
    ///
    /// A COUNT RATHER THAN A PERCENTAGE. The steps are the unit a person
    /// approved and the unit a client can name; a percentage would be this
    /// number divided by `steps.size()` and rounded, which is the same fact
    /// with the interesting half thrown away.
    std::size_t done_steps{0};

    /// The client's own name for the request this plan came from.
    ///
    /// WHY IT EXISTS. An agent that loses its connection mid-call cannot tell
    /// whether the call arrived. Retrying is the only thing it can do, and
    /// without this the retry files a SECOND suggestion — so the person at the
    /// workstation gets two identical cards for one piece of work and has to
    /// work out which one to apply. With it, the retry gets the id of the plan
    /// that is already on the screen.
    ///
    /// SCOPED TO THE REQUESTER, always. Two clients may use the same word for
    /// two different jobs, and one client must not be handed another's plan by
    /// guessing a key (M-07).
    std::string idempotency_key;

    /// A FINGERPRINT OF WHAT WOULD ACTUALLY RUN: the steps, in order, with their
    /// command ids and their resolved arguments.
    ///
    /// WHY AN APPROVAL NEEDS ONE. A person approves the command lines they READ
    /// on the card. The plan they approve is looked up again when the decision is
    /// carried out, and between those two moments the plan can CHANGE: a client
    /// may append a step to its own pending suggestion (`PlanStore::append_for`,
    /// which is how a sequence becomes one undo entry). Without this, a card
    /// drawn showing two lines could apply three — and the audit record would say
    /// the engineer approved all of them (TODOS S-04).
    ///
    /// THE LINES ARE NOT WHAT IS HASHED. The arguments are: a line is what a
    /// person reads, and the arguments are what runs. Two plans that read the
    /// same and run differently must not share a fingerprint.
    std::uint64_t content_fingerprint() const;

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

    // ---- WHOSE PLAN IS IT (TODOS M-07) --------------------------------------
    //
    // A plan id is `p` + 16 hex digits and it travels in a client's answer, so
    // it is not a secret and must not be treated as one. What keeps one agent
    // out of another's suggestion is the OWNER CHECK, not the id's width.
    //
    // WHY IT MATTERS MORE THAN READING. The approval a person gives is for the
    // command lines they READ on the card. A second client that could append a
    // step to a pending plan would be having its work signed by somebody who
    // never saw it, and the audit record would name the wrong requester for that
    // step — which is exactly the failure ai.md R6 and R8 exist to prevent.

    /// Whether `requester` may reach `plan`.
    ///
    /// THE PERSON AT THE KEYBOARD MAY REACH EVERYTHING, and that is not a hole:
    /// they are the one who applies plans, and the suggestion panel has to list
    /// what every client filed. Anything with a non-empty label — anything that
    /// arrived over a socket, and the chat — reaches only what it filed itself.
    static bool owned_by(const Plan& plan, std::string_view requester);

    /// `find`, but only when `requester` owns it.
    ///
    /// ONE ANSWER FOR "NOT THERE" AND FOR "NOT YOURS": both are a null. A caller
    /// that told the two apart would be an oracle for what another agent is
    /// composing, and a client could walk ids to use it.
    const Plan* find_for(std::string_view id, std::string_view requester) const;

    /// `append`, but only when `requester` owns the plan. A plan it does not own
    /// refuses with the same words an absent one does.
    core::Status append_for(std::string_view id, std::string_view requester, PlanStep step);

    /// Marks the outcome. The only writer is `Gate`, and only after a person has
    /// decided; nothing else may move a plan out of `Pending` or `Running`.
    core::Status settle(std::string_view id, PlanState state, std::string refusal = {});

    /// Moves a plan from `Pending` to `Running` and reports its progress there.
    ///
    /// SEPARATE FROM `settle` BECAUSE IT IS NOT A DECISION. `settle` records
    /// what a person decided and refuses a second answer; this records that the
    /// decision is being carried out. Calling it on a plan that is not pending
    /// is refused for the same reason a second decision is.
    core::Status begin_apply(std::string_view id);

    /// Records that `done` steps of a running plan have finished.
    void report_progress(std::string_view id, std::size_t done);

    /// The plan `requester` filed under `key`, or null. See
    /// `Plan::idempotency_key` for why this is scoped and what it prevents.
    const Plan* find_by_key(std::string_view key, std::string_view requester) const;

    /// Every plan still waiting, oldest first — what the suggestion panel lists.
    std::vector<const Plan*> pending() const;

    std::size_t size() const noexcept { return plans_.size(); }

private:
    std::vector<Plan> plans_;
    std::uint64_t filed_{0};
};

} // namespace kentos::ai
