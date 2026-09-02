# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_command — the command bus. Depends on core only.
# Constitution Article 1: every state mutation passes through this library.

add_library(kentos_command STATIC
    src/aids.cpp
    src/bus.cpp
    src/context.cpp
    src/input.cpp
    src/journal.cpp
    src/log.cpp
    src/parser.cpp
    src/registry.cpp
    src/selection.cpp
    src/session.cpp
    src/spec.cpp
    src/transaction.cpp
    src/validation.cpp
    src/value.cpp
    src/commands/builtin.cpp
    src/commands/corner.cpp
    src/commands/database.cpp
    src/commands/edit.cpp
    src/commands/file.cpp
    src/commands/label.cpp
    src/commands/layer.cpp
    src/commands/area.cpp
    src/commands/rectangle.cpp
    src/commands/arc.cpp
    src/commands/assign.cpp
    src/commands/attribute.cpp
    src/commands/circle.cpp
    src/commands/line.cpp
    src/commands/text.cpp
    src/commands/measure.cpp
    src/commands/ellipse.cpp
    src/commands/guide.cpp
    src/commands/offset.cpp
    src/commands/sector.cpp
    src/commands/pick.cpp
    src/commands/point.cpp
    src/commands/polyline.cpp
    src/commands/settings.cpp
    src/commands/style.cpp
    src/commands/symbol.cpp
    src/commands/system.cpp
    src/commands/to_area.cpp
    src/commands/transform.cpp
    src/commands/trim.cpp
    src/commands/vertex.cpp
    src/commands/view.cpp
)

target_include_directories(kentos_command PUBLIC include)
target_link_libraries(kentos_command
    PUBLIC  kentos_core
    PRIVATE kentos_flags
)
find_package(Threads REQUIRED)
target_link_libraries(kentos_command PUBLIC Threads::Threads)

# PRIVATE: spdlog is an implementation detail of command/log.cpp. The facade in
# command/log.hpp is what the rest of the product sees, so swapping the backend
# again touches one file (Article 9).
if(KENTOS_WITH_SPDLOG)
    target_link_libraries(kentos_command PRIVATE spdlog::spdlog)
endif()

add_library(KentOSCad::command ALIAS kentos_command)

# The command reference is generated from the registry, never hand-written
# (CLAUDE.md 5.10). The generator lives with the module that owns the registry,
# so no new top-level directory is needed (Article 3.1).
add_executable(kentos_docgen tools/docgen.cpp)
target_link_libraries(kentos_docgen PRIVATE kentos_command kentos_flags)
set_target_properties(kentos_docgen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
