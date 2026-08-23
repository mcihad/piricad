// SPDX-License-Identifier: GPL-3.0-or-later
// The one and only list of built-in commands.
//
// piricad.md §2.3 — a command is defined once. This list is the registration
// order; every command's definition lives with its implementation. Adding a
// command means adding one factory and one line here. There is no other list.
#include "piricad/command/registry.hpp"

#include "piricad/core/log.hpp"

namespace piricad::command {

#define PIRICAD_BUILTIN_COMMANDS(X)                                                                \
    X(line)                                                                                        \
    X(text)                                                                                        \
    X(exportstyle)                                                                                 \
    X(area)                                                                                        \
    X(attribute)                                                                                   \
    X(column)                                                                                      \
    X(erase)                                                                                       \
    X(select)                                                                                      \
    X(layer)                                                                                       \
    X(style)                                                                                       \
    X(zoom)                                                                                        \
    X(undo)                                                                                        \
    X(redo)                                                                                        \
    X(open)                                                                                        \
    X(save)                                                                                        \
    X(saveas)                                                                                      \
    X(import)                                                                                      \
    X(exportfile)                                                                                  \
    X(script)                                                                                      \
    X(setting)                                                                                     \
    X(preference)                                                                                  \
    X(mode)                                                                                        \
    X(help)

#define PIRICAD_DECLARE(sym) PIRICAD_COMMAND(sym);
PIRICAD_BUILTIN_COMMANDS(PIRICAD_DECLARE)
#undef PIRICAD_DECLARE

void register_builtin_commands(Registry& r)
{
    if (r.size() > 0) return; // idempotent

#define PIRICAD_REGISTER(sym)                                                                      \
    if (auto st = r.add(piricad_command_##sym()); !st)                                             \
        core::log_error("command registration failed: " + st.error().message);
    PIRICAD_BUILTIN_COMMANDS(PIRICAD_REGISTER)
#undef PIRICAD_REGISTER
}

#undef PIRICAD_BUILTIN_COMMANDS

} // namespace piricad::command
