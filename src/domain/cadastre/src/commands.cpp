// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/cadastre/commands.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/spec.hpp"

namespace kentos::command {
KENTOS_COMMAND(merge);
KENTOS_COMMAND(split_parcel);
KENTOS_COMMAND(topology);
} // namespace kentos::command

namespace kentos::domain::cadastre {

void register_cadastre_commands(kentos::command::Registry& r)
{
    // These sit here rather than in the builtin X-macro list because that list
    // lives in `/src/command`, which may not depend on a domain module
    // (CLAUDE.md Article 3.2).
    (void)r.add(kentos::command::kentos_command_merge());
    (void)r.add(kentos::command::kentos_command_split_parcel());
    (void)r.add(kentos::command::kentos_command_topology());
}

} // namespace kentos::domain::cadastre
