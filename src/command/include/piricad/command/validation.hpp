// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: validation.
//
// piricad.md §2.6: topology checks, regulatory rules and geometry validity run ON
// THE COMMAND BUS, not in the UI. It must be impossible for the AI or a script to
// skip a rule. The rule engine itself is data-driven (§9.9, §15).
#pragma once

#include "piricad/command/input.hpp"
#include "piricad/command/spec.hpp"
#include "piricad/core/document.hpp"

#include <memory>
#include <string>
#include <vector>

namespace piricad::command {

/// Everything a rule needs to judge one invocation.
///
/// Assembled by the bus and passed to every rule, so a rule cannot reach for
/// state nobody handed it — which is what keeps validation the same for a mouse
/// click, a script line and an AI suggestion (Article 1.3).
struct ValidationRequest
{
    const CommandSpec& spec;        ///< what the command declared it accepts
    const Args& args;               ///< the resolved arguments
    Origin origin;                  ///< recorded; a rule that BRANCHED on this
                                    ///< would be giving one client a privilege
    const core::Document& document; ///< read-only: validation never mutates
};

/// A pluggable rule. Domain modules register topology and regulatory rules here;
/// none of them may live in the UI layer.
class Rule
{
public:
    /// Virtual: rules are owned polymorphically by the validator.
    virtual ~Rule() = default;

    /// A stable name, for the message when this rule is the one that refused.
    virtual std::string name() const = 0;

    /// Judges one invocation. Failing here rolls the WHOLE transaction back
    /// (Article 1.6): a half-applied ifraz is the failure mode this exists to
    /// prevent, so a rule must not have written anything by the time it answers.
    virtual core::Status check(const ValidationRequest& req) const = 0;
};

class Validator
{
public:
    /// Structural check against the command's declared parameters. Always runs
    /// first, for every client, with no opt-out.
    static core::Status check_against_spec(const CommandSpec& spec, const Args& args);

    void add_rule(std::shared_ptr<Rule> rule);
    core::Status run(const ValidationRequest& req) const;

    std::size_t rule_count() const noexcept { return rules_.size(); }

private:
    std::vector<std::shared_ptr<Rule>> rules_;
};

} // namespace piricad::command
