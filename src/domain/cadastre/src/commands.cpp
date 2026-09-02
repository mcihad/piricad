// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/cadastre/commands.hpp"

#include "piricad/command/registry.hpp"
#include "piricad/command/spec.hpp"

namespace piricad::command {
PIRICAD_COMMAND(merge);
PIRICAD_COMMAND(split_parcel);
PIRICAD_COMMAND(topology);
} // namespace piricad::command

namespace piricad::domain::cadastre {

void register_cadastre_commands(piricad::command::Registry& r)
{
    // These sit here rather than in the builtin X-macro list because that list
    // lives in `/src/command`, which may not depend on a domain module
    // (CLAUDE.md Article 3.2).
    (void)r.add(piricad::command::piricad_command_merge());
    (void)r.add(piricad::command::piricad_command_split_parcel());
    (void)r.add(piricad::command::piricad_command_topology());
}

} // namespace piricad::domain::cadastre
