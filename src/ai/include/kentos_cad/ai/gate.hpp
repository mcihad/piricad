// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the one place a suggestion becomes an edit, and the permission
// that has to exist for it.
//
// THE RULE. CLAUDE.md 5.7 (amended 20 September 2026 with `kentoscad.md` §5.2.1):
// AI output reaches the document through exactly two roads — a preview a person
// approves, or an approval policy that person set BEFOREHAND, deliberately and
// for themselves. Never through a claim made by a client, a header, a prompt or
// a model. `.claude/ai.md` R3 says the same and P1 forbids everything else.
//
// WHAT DID NOT CHANGE, and it is the load-bearing half: an external agent may
// read everything and compose anything, and can complete no write of its own.
// The second road is not the client's; it is the user's, exercised in advance.
//
// HOW THAT IS MADE TRUE RATHER THAN PROMISED. `apply` demands an `Approval`, and
// an `Approval` cannot be built by an argument, a header, a token or a setting:
// its constructor is private and the only factory is `Gate::approve` — called
// from exactly TWO files, the suggestion card the person clicked and the policy
// path acting on permission they gave (`scripts/ci-gate-ai.sh` fails the build on
// a third). Nothing in the MCP server, in the chat loop or in a provider dialect
// can reach either.
//
// AND THE POLICY ITSELF IS OUT OF THEIR REACH TOO (CLAUDE.md 5.23): every
// authority setting is refused to an agent by `ai::escalates`, so a model that
// met a refusal cannot turn the policy down to get past it. That is what makes
// the second road a permission rather than a trust mode.
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

    /// THE ONE FACTORY FOR AN `Approval`, and the only way a decision enters the
    /// program. Called from exactly two places — the suggestion card, where a
    /// person clicked, and the policy path, acting on permission that person gave
    /// beforehand; `scripts/ci-gate-ai.sh` fails the build on a third caller.
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
