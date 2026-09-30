// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — cadastre: the commands this module owns.
//
// Registered separately from the builtins for the reason `geodesy/commands.hpp`
// gives: `/src/command` may depend on `/src/core` and nothing else, so it cannot
// name a command whose body needs a polygon boolean and a cadastral rule.
#pragma once

namespace kentos::command {
/// The command table every client resolves a name against; see command/registry.hpp.
class Registry;
} // namespace kentos::command

namespace kentos::domain::cadastre {

/// Adds every command this module owns to `r`.
void register_cadastre_commands(kentos::command::Registry& r);

} // namespace kentos::domain::cadastre
