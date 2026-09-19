// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the second sanctioned road to a decision.
//
// THERE ARE EXACTLY TWO, and this is the one that is not a click.
//
// `kentoscad.md` §5.2.1 (amended 20 September 2026) allows a command sequence to
// reach the document either through a preview a person approves, or through an
// approval policy that person set BEFOREHAND, deliberately and for themselves.
// CLAUDE.md 5.7 transcribes it and `.claude/ai.md` P15 makes the count
// structural: `Gate::approve` has two callers, the suggestion card and this
// file, and `scripts/ci-gate-ai.sh` fails the build on a third.
//
// WHAT MAKES THIS SAFE IS NOT THIS FILE. It is that the policy belongs to the
// person and nothing else can reach it: every authority setting is marked on its
// own `SettingSpec` and `ai::escalates` refuses any caller that writes one,
// whatever flags its command carries (CLAUDE.md 5.23). A model that hit a
// refusal and turned the policy down is the failure this whole arrangement
// exists to prevent, and it is prevented one layer below.
//
// AND THE OLD RULE'S REASON SURVIVES. A cadastral output is a legal document
// only a licensed engineer may sign (§5.2.4, which did NOT change). A policy is
// that engineer describing the scope of their own signature in advance — never a
// substitute for it — which is why every decision this file makes is written to
// the audit record naming the policy that made it (`AuditRecord::decided_by`,
// `AuditRecord::policy`, S-06).
#include "kentos_cad/ai/policy_path.hpp"

#include "kentos_cad/ai/policy.hpp"

namespace kentos::ai {

core::Result<bool> decide_by_policy(Gate& gate, const Plan& plan, const PolicyPreferences& prefs,
                                    const ClientScope& scope, command::Effect effect,
                                    const std::string& operator_name, std::int64_t utc_ms)
{
    // THE ENGINE ANSWERS FIRST, and its answer is the whole decision: this file
    // adds no judgement of its own. `decide` is pure and tested apart from any
    // model (`test_ai_policy.cpp`).
    const PolicyDecision verdict = decide(effect, prefs, scope);

    // ANYTHING BUT `Allow` GOES TO A PERSON. `ApprovalRequired` is the ordinary
    // case, `InputRequired` means the plan is not finished, and `Deny` is out of
    // scope and can never become an approval however the preferences are set.
    if (verdict.verdict != Verdict::Allow) return false;

    // THE APPROVAL IS BOUND TO THESE EXACT STEPS, the same way the card binds to
    // what it drew. A plan that grew between the policy reading it and the gate
    // running it is refused by `Gate::decide`, not applied (S-04).
    const Approval approval =
        gate.approve(plan.id, operator_name, Decision::Apply, utc_ms,
                     approval_policy_name(prefs.approval), plan.content_fingerprint());

    if (core::Status ran = gate.decide(approval); !ran) return ran.error();
    return true;
}

} // namespace kentos::ai
