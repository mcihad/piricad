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

# The command reference is generated from the registry, never hand-written
# (CLAUDE.md 5.10). The generator lives with the module that owns the registry,
# so no new top-level directory is needed (Article 3.1).
add_executable(piricad_docgen tools/docgen.cpp)
target_link_libraries(piricad_docgen PRIVATE piricad_command piricad_flags)
set_target_properties(piricad_docgen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
