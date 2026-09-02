// SPDX-License-Identifier: GPL-3.0-or-later
// The one and only list of built-in commands.
//
// piricad.md §2.3 — a command is defined once. This list is the registration
// order; every command's definition lives with its implementation. Adding a
// command means adding one factory and one line here. There is no other list.
#include "piricad/command/registry.hpp"

#include "piricad/command/log.hpp"

namespace piricad::command {

#define PIRICAD_BUILTIN_COMMANDS(X)                                                                \
    X(line)                                                                                        \
    X(polyline)                                                                                    \
    X(point_draw)                                                                                  \
    X(text)                                                                                        \
    X(exportstyle)                                                                                 \
    X(area)                                                                                        \
    X(rectangle)                                                                                   \
    X(circle_draw)                                                                                 \
    X(arc_draw)                                                                                    \
    X(vertex_move)                                                                                 \
    X(vertex_insert)                                                                               \
    X(to_area)                                                                                     \
    X(move)                                                                                        \
    X(copy_objects)                                                                                \
    X(array_objects)                                                                               \
    X(split)                                                                                       \
    X(trim)                                                                                        \
    X(extend)                                                                                      \
    X(chamfer)                                                                                     \
    X(fillet)                                                                                      \
    X(set_layer)                                                                                   \
    X(match_style)                                                                                 \
    X(rotate)                                                                                      \
    X(scale)                                                                                       \
    X(mirror)                                                                                      \
    X(measure)                                                                                     \
    X(measure_area)                                                                                \
    X(coordinate)                                                                                  \
    X(pan)                                                                                         \
    X(offset)                                                                                      \
    X(sector)                                                                                      \
    X(annulus)                                                                                     \
    X(guide)                                                                                       \
    X(attribute)                                                                                   \
    X(column)                                                                                      \
    X(erase)                                                                                       \
    X(select)                                                                                      \
    X(label)                                                                                       \
    X(layer)                                                                                       \
    X(style)                                                                                       \
    X(symbol)                                                                                      \
    X(zoom)                                                                                        \
    X(undo)                                                                                        \
    X(redo)                                                                                        \
    X(open)                                                                                        \
    X(save)                                                                                        \
    X(saveas)                                                                                      \
    X(import)                                                                                      \
    X(exportfile)                                                                                  \
    X(script)                                                                                      \
    X(database)                                                                                    \
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
        log_error("command registration failed: " + st.error().message);
    PIRICAD_BUILTIN_COMMANDS(PIRICAD_REGISTER)
#undef PIRICAD_REGISTER
}

#undef PIRICAD_BUILTIN_COMMANDS

} // namespace piricad::command
