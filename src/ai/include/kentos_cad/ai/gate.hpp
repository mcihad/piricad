// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the one place a suggestion becomes an edit, and the person who
// has to be there for it.
//
// THE RULE, VERBATIM. CLAUDE.md 5.7: "NEVER auto-apply AI output. No trust mode,
// setting, CLI flag or 'remember my choice' that bypasses preview + explicit
// approval." .claude/ai.md R3 says the same and P1 forbids any exception. The
// maintainer was asked directly and confirmed it for the remote case too: an
// external agent may read everything and compose anything, and cannot complete a
// single write while nobody is at the keyboard.
//
// HOW THAT IS MADE TRUE RATHER THAN PROMISED. `apply` demands an `Approval`, and
// an `Approval` cannot be built by an argument, a header, a token or a setting:
// its constructor is private and the only factory is `Gate::approve` — which
// takes the operator's own name and is called from exactly one file in the
// program, the suggestion card the person clicked (`scripts/ci-gate-ai.sh`
// checks that there is exactly one caller and where it lives). Nothing in the
// MCP server, in the chat loop or in a provider dialect can reach it.
//
// AND APPLYING IS ONE TRANSACTION. Every step of the plan runs inside one batch,
// so a plan of eleven commands is one `UndoStack` entry and one Ctrl+Z (ai.md
// R4); a refusal part way through rolls all of it back and the document is left
// bit-identical (Article 1.6, ai.md R20).
#pragma once

#include "kentos_cad/ai/audit.hpp"
#include "kentos_cad/ai/plan.hpp"

#include "kentos_cad/core/result.hpp"

#include <functional>
#include <string>

namespace kentos::ai {

/// What the person decided.
enum class Decision : std::uint8_t {
    Apply,  ///< apply the plan, as one transaction
    Reject, ///< do not apply it, and record that
};

/// Proof that a human at this workstation decided.
///
/// Not copyable from thin air: no default constructor, no aggregate
/// initialisation, and the only factory is `Gate::approve`.
class Approval
{
public:
    /// Which plan was decided.
    const std::string& plan_id() const noexcept { return plan_id_; }

    /// Who decided, as the audit record will name them.
    const std::string& operator_name() const noexcept { return operator_; }

    /// What they decided.
    Decision decision() const noexcept { return decision_; }

    /// When, in UTC milliseconds.
    std::int64_t utc_ms() const noexcept { return utc_ms_; }

    /// THE PLAN AS IT WAS WHEN THE PERSON READ IT (`Plan::content_fingerprint`).
    ///
    /// An approval is for the command lines on the card, not for whatever the
    /// plan happens to hold when the decision is carried out. Between the card
    /// being drawn and the button being pressed, the client that filed the plan
    /// can APPEND to it — that is how a sequence becomes one undo entry — so a
    /// card showing two lines could otherwise apply three, and the audit record
    /// would say the engineer approved all of them (TODOS S-04).
    ///
    /// Zero means the caller did not say, and `Gate::decide` then applies without
    /// this check: a caller that cannot fingerprint is not silently trusted, it
    /// is simply not making the claim.
    std::uint64_t content() const noexcept { return content_; }

    /// The approval policy that was in force at the moment of the decision, by
    /// its setting word — empty when the caller did not say.
    ///
    /// CARRIED BY THE APPROVAL, not looked up when the record is written, and
    /// the difference matters: the setting may change between the click and the
    /// line reaching the log, and what a record has to preserve is the rule the
    /// decision was made UNDER (TODOS S-06).
    const std::string& policy() const noexcept { return policy_; }

private:
    friend class Gate;

    Approval(std::string plan_id, std::string operator_name, Decision decision, std::int64_t utc_ms,
             std::string policy, std::uint64_t content)
        : plan_id_(std::move(plan_id)), operator_(std::move(operator_name)),
          policy_(std::move(policy)), content_(content), decision_(decision), utc_ms_(utc_ms)
    {}

    std::string plan_id_;
    std::string operator_;
    std::string policy_;
    std::uint64_t content_{0};
    Decision decision_{Decision::Reject};
    std::int64_t utc_ms_{0};
};

/// Turns a decided plan into an edit — and records the decision either way.
class Gate
{
public:
    /// How a plan's steps are actually run. The application supplies it: one
    /// batch, the lines dispatched through the bus with `Origin::Ai`, one undo
    /// entry. Returns the refusal when something was refused, in which case the
    /// whole batch has already been rolled back.
    using Runner = std::function<core::Status(const Plan&)>;

    Gate(PlanStore& plans, AuditLog& audit, Runner runner);

    /// THE ONE FACTORY FOR AN `Approval`, and the one place a human decision
    /// enters the program. Called from the suggestion card and from nowhere
    /// else; `scripts/ci-gate-ai.sh` fails the build if a second caller appears.
    ///
    /// `operator_name` is who is at the workstation, as the program knows them —
    /// it reaches the audit record, because "who approved this parcel" is the
    /// question BÖHHBÜY makes somebody answer (ai.md R8, R6).
    /// `policy` is the approval policy in force at the moment of the click, by
    /// its setting word. Recorded because a decision is only explicable against
    /// the rule it was made under, and that rule may change twice before anybody
    /// reads the log (TODOS S-06).
    /// `content` is `Plan::content_fingerprint()` as the card read it. Zero means
    /// the caller is not making that claim; see `Approval::content`.
    Approval approve(std::string plan_id, std::string operator_name, Decision decision,
                     std::int64_t utc_ms, std::string policy = {}, std::uint64_t content = 0);

    /// Applies or rejects, writes the audit record, and settles the plan.
    ///
    /// The audit record is written for a REJECTION too (ai.md R6: "Rejected
    /// suggestions MUST be recorded too"), because the interesting question
    /// months later is often what the engineer refused.
    core::Status decide(const Approval& approval);

private:
    PlanStore& plans_;
    AuditLog& audit_;
    Runner runner_;
};

} // namespace kentos::ai
