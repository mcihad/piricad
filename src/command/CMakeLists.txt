# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_command — the command bus. Depends on core only.
# Constitution Article 1: every state mutation passes through this library.

add_library(piricad_command STATIC
    src/bus.cpp
    src/context.cpp
    src/input.cpp
    src/journal.cpp
    src/parser.cpp
    src/registry.cpp
    src/session.cpp
    src/spec.cpp
    src/transaction.cpp
    src/validation.cpp
    src/value.cpp
    src/commands/builtin.cpp
    src/commands/edit.cpp
    src/commands/layer.cpp
    src/commands/line.cpp
    src/commands/system.cpp
    src/commands/view.cpp
)

target_include_directories(piricad_command PUBLIC include)
target_link_libraries(piricad_command
    PUBLIC  piricad_core
    PRIVATE piricad_flags
)
find_package(Threads REQUIRED)
target_link_libraries(piricad_command PUBLIC Threads::Threads)

add_library(PiriCAD::command ALIAS piricad_command)
