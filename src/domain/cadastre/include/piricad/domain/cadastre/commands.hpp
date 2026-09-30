// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — cadastre: the commands this module owns.
//
// Registered separately from the builtins for the reason `geodesy/commands.hpp`
// gives: `/src/command` may depend on `/src/core` and nothing else, so it cannot
// name a command whose body needs a polygon boolean and a cadastral rule.
#pragma once

namespace piricad::command {
/// The command table every client resolves a name against; see command/registry.hpp.
class Registry;
} // namespace piricad::command

namespace piricad::domain::cadastre {

/// Adds every command this module owns to `r`.
void register_cadastre_commands(piricad::command::Registry& r);

} // namespace piricad::domain::cadastre
