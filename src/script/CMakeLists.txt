# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_script — script hosts. Depends on command; never the reverse.
#
# Two hosts, one architecture. The JSON command-array runner needs no external
# dependency and is always built; the Lua host sits behind KENTOS_WITH_LUA, which
# defaults OFF and which the application must build, start and test without
# (.claude/script.md R6). Python/pybind11 (KENTOS_WITH_PYTHON) is still Phase 2.

add_library(kentos_script STATIC
    src/json_runner.cpp
    src/sandbox.cpp
)

target_include_directories(kentos_script PUBLIC include)
target_link_libraries(kentos_script
    PUBLIC  kentos_command
    PRIVATE kentos_flags
)

if(KENTOS_WITH_LUA)
    if(NOT TARGET kentos_sol2)
        message(FATAL_ERROR
            "KENTOS_WITH_LUA=ON but Lua and sol2 were not acquired. "
            "cmake/KentOSCadDependencies.cmake fetches both from pinned commits; "
            "see the message it prints, or configure with -DKENTOS_WITH_LUA=OFF.")
    endif()

    target_sources(kentos_script PRIVATE src/lua_runner.cpp)

    # PRIVATE, and that is the point of the pimpl in lua_runner.hpp: nothing that
    # includes the header pays for <sol/sol.hpp>, and swapping the binding library
    # touches one .cpp (CLAUDE.md Article 9).
    target_link_libraries(kentos_script PRIVATE kentos_sol2)
    target_compile_definitions(kentos_script PUBLIC KENTOS_HAVE_LUA=1)
endif()

if(KENTOS_WITH_PYTHON)
    message(FATAL_ERROR
        "KENTOS_WITH_PYTHON=ON but the Python module is a Phase-2 deliverable "
        "(.claude/script.md). Configure with -DKENTOS_WITH_PYTHON=OFF.")
endif()

add_library(KentOSCad::script ALIAS kentos_script)
