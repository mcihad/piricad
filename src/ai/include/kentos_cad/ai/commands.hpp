// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the read tools, and they are COMMANDS.
//
// WHY COMMANDS RATHER THAN AN AI-ONLY API. `.claude/ai.md` R11 names six read
// tools and says context "MUST be supplied only through queryable read tools".
// It does not say they may be a private interface, and CLAUDE.md 5.15 forbids
// giving one client a capability another lacks. "Which layers are there, and how
// many objects does each hold" is a question a surveyor asks too, at the command
// line, in a script, in a macro — so it is `KATMANLAR`, a registered command
// like every other, and the agent reaches it the same way the keyboard does
// (Article 1.2).
//
// THEY CHANGE NOTHING, and they say so in their flags: `ReadOnly | NoEffect`.
// That pair is what lets an agent run them with no approval (gate.hpp), and
// `NoEffect` is the narrow claim — `ReadOnly` alone means only "skips the
// transaction path" and `core.undo` carries it too.
//
// AND THEY ANSWER TWICE. Every one echoes a Turkish sentence for the person at
// the keyboard and reports the same facts as structured data through
// `Context::report`, because an agent should not have to parse prose to read a
// count. The dispatcher turns the reported keys and corners into handles
// (dispatcher.hpp).
#pragma once

#include "kentos_cad/command/registry.hpp"

#include <vector>

namespace kentos::ai {

/// Registers the read tools on `registry`.
///
/// CALLED BY THE APPLICATION AND BY THE GENERATOR, exactly as
/// `processing::register_processing_commands` is: `/src/command` may not depend
/// on `/src/ai` (Article 3.2), so the builtin roster cannot name these and
/// whoever assembles a registry adds them.
void register_ai_commands(command::Registry& registry);

/// The specs themselves, one factory per file that owns them.
///
/// SEPARATE FROM THE REGISTRATION so that `register_ai_commands` stays a single
/// list in a single place, the way `commands/builtin.cpp` keeps the roster of
/// the built-ins: a module with two registration functions is a module where one
/// of them will be forgotten.
namespace detail {

/// `KATMANLAR`, `ÖZNİTELİKŞEMASI`, `SORGULA`, `SEÇİMBİLGİSİ`, `GÖRÜNÜMBİLGİSİ`.
std::vector<command::CommandSpec> read_tool_specs();

/// `ÖNERİ` and `MCPSUNUCU`.
std::vector<command::CommandSpec> ai_command_specs();

/// `YAPAYZEKAMODELİ`, from `commands/provider_command.cpp`.
///
/// ONE SPEC RATHER THAN A VECTOR, because that file owns exactly one command;
/// `ai_command_specs` appends it, so the registration roster stays the single
/// list `register_ai_commands` walks.
command::CommandSpec provider_command_spec();

} // namespace detail

} // namespace kentos::ai
