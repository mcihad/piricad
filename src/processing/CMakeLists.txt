# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_processing — analysis tools applied to many objects at once
# (.claude/processing.md). Qt-free; depends on core, command and io, never the
# reverse (CLAUDE.md Article 3.2). Every tool is one generated command.
add_library(piricad_processing STATIC
    src/tool.cpp
    src/registry.cpp
    src/tools/label_length.cpp
    src/tools/number_vertices.cpp
    src/tools/area_edit.cpp
    src/tools/attach.cpp
    src/tools/detach.cpp
    src/tools/buffer.cpp
    src/tools/polygonize.cpp
)

target_include_directories(piricad_processing PUBLIC include)
target_link_libraries(piricad_processing
    PUBLIC  piricad_core piricad_command
    PRIVATE piricad_io piricad_flags
)
