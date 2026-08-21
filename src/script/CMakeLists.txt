# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_script — script hosts. Depends on command; never the reverse.
#
# Phase 0: JSON command-array runner (no external dependency).
# Phase 2: Lua/sol2 hot path (PIRICAD_WITH_LUA), Python/pybind11 optional module
#          (PIRICAD_WITH_PYTHON) — see .claude/script.md.

add_library(piricad_script STATIC
    src/json_runner.cpp
)

target_include_directories(piricad_script PUBLIC include)
target_link_libraries(piricad_script
    PUBLIC  piricad_command
    PRIVATE piricad_flags
)

if(PIRICAD_WITH_LUA)
    message(FATAL_ERROR
        "PIRICAD_WITH_LUA=ON but the Lua host is a Phase-2 deliverable "
        "(.claude/script.md). Configure with -DPIRICAD_WITH_LUA=OFF.")
endif()

if(PIRICAD_WITH_PYTHON)
    message(FATAL_ERROR
        "PIRICAD_WITH_PYTHON=ON but the Python module is a Phase-2 deliverable "
        "(.claude/script.md). Configure with -DPIRICAD_WITH_PYTHON=OFF.")
endif()

add_library(PiriCAD::script ALIAS piricad_script)
