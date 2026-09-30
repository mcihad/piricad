// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/surface/commands.hpp"

#include "piricad/command/registry.hpp"
#include "piricad/command/spec.hpp"

namespace piricad::command {
PIRICAD_COMMAND(contour);
PIRICAD_COMMAND(earthwork);
} // namespace piricad::command

namespace piricad::domain::surface {

void register_surface_commands(piricad::command::Registry& r)
{
    (void)r.add(piricad::command::piricad_command_contour());
    (void)r.add(piricad::command::piricad_command_earthwork());
}

} // namespace piricad::domain::surface
