// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/surface/commands.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/spec.hpp"

namespace kentos::command {
KENTOS_COMMAND(contour);
KENTOS_COMMAND(earthwork);
} // namespace kentos::command

namespace kentos::domain::surface {

void register_surface_commands(kentos::command::Registry& r)
{
    (void)r.add(kentos::command::kentos_command_contour());
    (void)r.add(kentos::command::kentos_command_earthwork());
}

} // namespace kentos::domain::surface
