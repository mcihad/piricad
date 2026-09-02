// SPDX-License-Identifier: GPL-3.0-or-later
// The one and only list of built-in commands.
//
// kentoscad.md §2.3 — a command is defined once. This list is the registration
// order; every command's definition lives with its implementation. Adding a
// command means adding one factory and one line here. There is no other list.
#include "kentos_cad/command/registry.hpp"

#include "kentos_cad/command/log.hpp"

namespace kentos::command {

#define KENTOS_BUILTIN_COMMANDS(X)                                                                \
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
    X(ellipse_draw)                                                                                \
    X(points)                                                                                      \
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

#define KENTOS_DECLARE(sym) KENTOS_COMMAND(sym);
KENTOS_BUILTIN_COMMANDS(KENTOS_DECLARE)
#undef KENTOS_DECLARE

void register_builtin_commands(Registry& r)
{
    if (r.size() > 0) return; // idempotent

#define KENTOS_REGISTER(sym)                                                                      \
    if (auto st = r.add(kentos_command_##sym()); !st)                                             \
        log_error("command registration failed: " + st.error().message);
    KENTOS_BUILTIN_COMMANDS(KENTOS_REGISTER)
#undef KENTOS_REGISTER
}

#undef KENTOS_BUILTIN_COMMANDS

} // namespace kentos::command
