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

struct ValidationRequest
{
    const CommandSpec& spec;
    const Args& args;
    Origin origin;
    const core::Document& document;
};

/// A pluggable rule. Domain modules register topology and regulatory rules here;
/// none of them may live in the UI layer.
class Rule
{
public:
    virtual ~Rule()                                                = default;
    virtual std::string name() const                               = 0;
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
