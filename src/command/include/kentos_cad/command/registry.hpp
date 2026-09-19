// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the command registry.
//
// The one and only catalogue of commands. CLI completion, script bindings, AI tool
// schemas and generated docs all read from here (kentoscad.md §2.3).
#pragma once

#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::command {

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

    // THE AI TOOL CATALOGUE IS NOT HERE, and its absence is deliberate.
    // `ai_tool_schema()` used to project the registry into a JSON tool list from
    // this class, and `/src/ai` needed a different projection — a real JSON
    // Schema, handles instead of coordinate literals, MCP annotations. Two
    // projections of one registry are two answers to one question
    // (.claude/ai.md P7), so the survivor is `ai::build_catalog`, which is what
    // the server serves, what `llms.txt` is written from and what the generated
    // reference embeds. `Registry` keeps what belongs to it: the commands, the
    // names, and the fingerprint below.

    /// A fingerprint over EVERY declared field of every command.
    ///
    /// WHY A RUNNING PROGRAM NEEDS ONE. An agent caches the tool list it was
    /// served; the moment a command gains a parameter, that cache is a lie. The
    /// fingerprint is what `tools/list` carries and what tells a client its copy
    /// is stale, and it is what the freshness gate hashes so that "the smallest
    /// change to a parameter regenerates the surface" is a check rather than a
    /// hope (CLAUDE.md 6.14). Stable across runs and platforms: it hashes
    /// declared text and integers, never an address or an iteration order that
    /// could differ.
    std::uint64_t fingerprint() const;

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

} // namespace kentos::command
