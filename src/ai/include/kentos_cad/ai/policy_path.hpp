// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: applying a plan on permission given beforehand.
//
// THE SECOND OF TWO SANCTIONED ROADS. See `policy_path.cpp` for why there are
// two, why the count is enforced by a CI gate, and why the thing that makes this
// safe lives one layer below (CLAUDE.md 5.23).
#pragma once

#include "kentos_cad/ai/gate.hpp"
#include "kentos_cad/ai/plan.hpp"
#include "kentos_cad/ai/policy.hpp"

#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>

namespace kentos::ai {

/// What the policy road did with one plan.
struct PolicyOutcome
{
    Verdict verdict{Verdict::ApprovalRequired}; ///< the engine's answer
    std::string reason;                         ///< why, in Turkish — never empty
    bool applied{false};                        ///< applied under the policy, as one batch
};

/// What a policy decision is recorded as: `politika:<word>` (`Plan::decided_by`).
std::string policy_decider(ApprovalPolicy policy);

/// Applies `plan` when the user's standing policy allows it, and says what was
/// decided and why.
///
/// `applied == false` is NOT a failure: `ApprovalRequired` is the ordinary
/// answer under the default policy, and `Deny` the only answer for work outside
/// the client's scope — which no approval can widen, so the caller refuses such
/// a plan rather than filing it for a person. An error is an attempt that was
/// made and refused by the gate or the runner.
///
/// `effect` is what the plan would leave changed — `command::effect_of` over its
/// steps, never a guess from a command's name. `overwrites` says a step would
/// write over a file that is already there; asked of the caller, so this stays
/// free of the disk.
core::Result<PolicyOutcome> decide_by_policy(Gate& gate, const Plan& plan,
                                             const PolicyPreferences& prefs,
                                             const ClientScope& scope, command::Effect effect,
                                             const std::string& operator_name, std::int64_t utc_ms,
                                             bool overwrites = false);

} // namespace kentos::ai
