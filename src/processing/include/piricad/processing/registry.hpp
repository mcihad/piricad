// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — processing: the one list of tools.
//
// Every tool is declared once with `KENTOS_PROCESSING_TOOL` and listed once in
// registry.cpp, the way builtin commands are (command.md). From that list come
// the commands the bus validates, the rows the panel shows, and the reference
// the docs generator writes — there is no second list anywhere (CLAUDE.md 5.10).
#pragma once

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/processing/tool.hpp"

#include <span>
#include <string_view>

namespace kentos::processing {

/// Every tool this build ships, in tree order (by group, then by title).
std::span<const ProcessingTool* const> processing_tools();

/// The tool whose command id or command name is `id_or_name`, or null.
const ProcessingTool* find_tool(std::string_view id_or_name);

/// Registers one command per tool. Called wherever the builtin commands are
/// registered (the shell, the docs generator, the tests).
void register_processing_commands(command::Registry& registry);

/// Declares the factory for one tool. The body returns a reference to a
/// function-local static instance.
#define KENTOS_PROCESSING_TOOL(sym) const ::kentos::processing::ProcessingTool& kentos_tool_##sym()

} // namespace kentos::processing
