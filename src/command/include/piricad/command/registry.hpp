// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the command registry.
//
// The one and only catalogue of commands. CLI completion, script bindings, AI tool
// schemas and generated docs all read from here (piricad.md §2.3).
#pragma once

#include "piricad/command/spec.hpp"
#include "piricad/core/result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace piricad::command {

class Registry
{
public:
    /// Adds a command. Fails on a duplicate id or a name already claimed by
    /// another command — silent shadowing is not permitted.
    core::Status add(CommandSpec spec);

    const CommandSpec* by_id(std::string_view id) const;

    /// Resolves a user-typed name. Turkish-aware, case-insensitive, accepts every
    /// declared alias and abbreviation. Returns nullptr when unresolved.
    const CommandSpec* resolve(std::string_view typed) const;

    /// Names starting with the given prefix, for command-line completion.
    std::vector<std::string> complete(std::string_view prefix, std::size_t limit = 16) const;

    const std::vector<CommandSpec>& all() const noexcept { return specs_; }

    /// Every command carrying Flags::AiAccessible, as a JSON tool catalogue.
    /// This is the AI's entire view of the application (piricad.md §5.1).
    core::Json ai_tool_schema() const;

    /// Generated command reference, Markdown. Never hand-written.
    std::string markdown_reference() const;

    std::size_t size() const noexcept { return specs_.size(); }

private:
    std::vector<CommandSpec> specs_;
    std::unordered_map<std::string, std::size_t> by_id_;
    std::unordered_map<std::string, std::size_t> by_name_; ///< keyed on turkish_fold_key(name)
};

/// The process-wide registry, populated once by register_builtin_commands().
Registry& registry();

/// Registers every built-in command. Idempotent.
void register_builtin_commands(Registry& r);

} // namespace piricad::command
