// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: what one tool IS, as data.
//
// ONE FORM, THREE READERS. The MCP server serves it, `llms.txt` is written from
// it, and the in-app chat projects it into whichever dialect its provider
// speaks. None of them keeps a second description of a command, because a second
// description is a second answer to one question (.claude/ai.md R12, P7).
//
// AND IT IS GENERATED, always. Every field below is derived from a
// `command::CommandSpec` by `ai::build_catalog`; nothing here is hand-written and
// nothing here is checked in. Setting `Flags::AiAccessible` on a command is the
// only action needed to put it in front of a model (ai.md R12).
#pragma once

#include "kentos_cad/core/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::ai {

/// What a client may assume about calling a tool.
///
/// ALL FOUR ARE ALWAYS EMITTED, and that is not tidiness. The MCP specification
/// defaults `destructiveHint` and `openWorldHint` to TRUE, so a tool published
/// with no annotations is advertised as destructive and open-world — which is
/// exactly wrong for `gorunum_bilgisi` and would make a careful client refuse to
/// call the safest thing in the program.
struct ToolAnnotations
{
    bool read_only{false};  ///< changes nothing the caller can observe
    bool destructive{true}; ///< may remove or overwrite something
    bool idempotent{false}; ///< calling it twice is the same as calling it once
    bool open_world{true};  ///< touches something outside this program (disk, network)
};

/// One tool, as a client sees it.
struct ToolDef
{
    std::string name;        ///< the wire name: `core_line`, `gorunum_bilgisi`
    std::string title;       ///< the Turkish LABEL: `Çizgi`, `Blok Ekle`
    std::string description; ///< what it does, its names, its units, its approval rule
    core::Json input_schema; ///< a real JSON Schema object
    ToolAnnotations annotations;
    core::Json meta; ///< `_meta`: the command id, category, undo policy, aliases

    std::string command_id; ///< which command it dispatches
    bool mutates{false};    ///< true unless the command declared `Flags::NoEffect`
};

/// Everything a client may reach, and the fingerprint that says which version of
/// it they are holding.
struct Catalog
{
    /// The MCP revision this catalogue is shaped for. One string, in one place:
    /// a version scattered across a server, a document and a test is a version
    /// that will disagree with itself.
    static constexpr const char* kProtocolVersion = "2026-07-28";

    std::uint64_t fingerprint{0}; ///< `command::Registry::fingerprint()`
    std::vector<ToolDef> tools;   ///< sorted by name, so two runs agree byte for byte

    /// The `tools` array of an MCP `tools/list` result.
    core::Json to_tools_list() const;

    /// The tool with this wire name, or null.
    const ToolDef* find(std::string_view name) const;

    /// How many tools change something. What the status line and the docs quote.
    std::size_t mutating_count() const;
};

} // namespace kentos::ai
