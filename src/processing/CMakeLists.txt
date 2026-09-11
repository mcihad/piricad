# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_processing — analysis tools applied to many objects at once
# (.claude/processing.md). Qt-free; depends on core, command and io, never the
# reverse (CLAUDE.md Article 3.2). Every tool is one generated command.
add_library(kentos_processing STATIC
    src/tool.cpp
    src/registry.cpp
    src/tools/label_length.cpp
    src/tools/number_vertices.cpp
    src/tools/area_edit.cpp
)

target_include_directories(kentos_processing PUBLIC include)
target_link_libraries(kentos_processing
    PUBLIC  kentos_core kentos_command
    PRIVATE kentos_io kentos_flags
)
