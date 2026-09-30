// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — geodesy: the commands this module owns.
//
// Registered separately from `command::register_builtin_commands`, and that is
// Article 3.2 rather than a preference: `/src/command` may depend on `/src/core`
// and nothing else, so it cannot name a command whose body needs a Helmert fit.
// The domain module owns the command, and `/src/app` — which is allowed to depend
// on everything — registers both lists on the one registry.
#pragma once

namespace kentos::command {
/// The command table every client resolves a name against; see command/registry.hpp.
class Registry;
} // namespace kentos::command

namespace kentos::domain::geodesy {

/// Adds every command this module owns to `r`. Idempotent by the registry's own
/// duplicate-name refusal, so calling it twice is a no-op rather than a defect.
void register_geodesy_commands(kentos::command::Registry& r);

} // namespace kentos::domain::geodesy
