// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the one projection from the command registry to a tool catalogue.
//
// THE WHOLE AI SURFACE IS DERIVED, and this file is where the deriving happens
// (.claude/ai.md R12: "The AI tool catalogue MUST be generated from `Registry`
// via the `Flags::AiAccessible` bit. Setting that bit in a `CommandSpec` MUST be
// the only action needed to expose a command to AI.")
//
// WHY THE INTERNAL SCHEMA WAS NOT ENOUGH. `CommandSpec::to_schema()` emitted the
// program's own vocabulary — `"type": "point"`, a `min`/`max`/`required` triple —
// which is a fine description for a Turkish reference table and unusable as a
// JSON Schema. A model calling a tool needs to know that a point is two integers
// in millimetres with easting first, and no amount of prose in a `help` field
// makes a schema out of that. So the projection is explicit, it lives here, and
// `to_schema()` is gone: one registry, one catalogue.
//
// COORDINATES ARE NOT NUMBERS HERE. In the AI-facing schema a `Point`,
// `PointList` or `Selection` parameter accepts a HANDLE — a string minted by a
// read tool — and not a literal. That is CLAUDE.md 5.8 and ai.md R9/R10/P2 made
// unbreakable rather than checked: a model cannot express a coordinate it
// invented, so there is no code path in which one arrives. `Style::Human` keeps
// the literal form for the command line and the reference, which are used by a
// person whose numbers come from their own instrument.
#pragma once

#include "kentos_cad/ai/tool.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/spec.hpp"

namespace kentos::ai {

/// Who the schema is being written for.
enum class Style : std::uint8_t {
    Agent, ///< handles only where a coordinate is declared (ai.md R26)
    Human, ///< literal coordinates, for the generated reference and `llms.txt`
};

struct CatalogOptions
{
    /// Who the catalogue is being written for; see `Style`.
    Style style{Style::Agent};

    /// Leave out the tools that change something. What the MCP server serves
    /// when the project is marked sensitive, and what a "read-only session"
    /// means; never a second exposure bit.
    bool include_mutating{true};
};

/// The JSON Schema for one parameter.
core::Json schema_for(const command::Param& param, Style style);

/// The whole `inputSchema` object for one command.
core::Json input_schema_for(const command::CommandSpec& spec, Style style);

/// The wire name of a command: its id with `.` replaced by `_`
/// (`core.line` → `core_line`), except the read tools `.claude/ai.md` R11 names
/// by hand, which carry those names exactly.
std::string tool_name_for(const command::CommandSpec& spec);

/// One tool from one command.
ToolDef tool_for(const command::CommandSpec& spec, Style style);

/// Every `AiAccessible` command in the registry, sorted by wire name.
Catalog build_catalog(const command::Registry& registry, const CatalogOptions& options = {});

} // namespace kentos::ai
