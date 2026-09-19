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

/// Applies `plan` when the user's standing policy allows it, and says whether it
/// did.
///
/// `false` is NOT a failure: it is "this one needs a person", which is the
/// ordinary answer under the default policy and the only answer for work outside
/// the client's scope. An error is an attempt that was made and refused.
///
/// `effect` is what the plan would leave changed — `command::effect_of` over its
/// steps, never a guess from a command's name.
core::Result<bool> decide_by_policy(Gate& gate, const Plan& plan, const PolicyPreferences& prefs,
                                    const ClientScope& scope, command::Effect effect,
                                    const std::string& operator_name, std::int64_t utc_ms);

} // namespace kentos::ai
