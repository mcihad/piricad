# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_script — script hosts. Depends on command; never the reverse.
#
# Two hosts, one architecture. The JSON command-array runner needs no external
# dependency and is always built; the Python host sits behind KENTOS_WITH_PYTHON,
# which defaults OFF and which the application must build, start and test without
# (.claude/script.md R6). An embedded Lua stood beside them until the amendment
# that removed it: one embedded language, and the per-feature hot path it was
# there for belongs to the compiled expression engine in command/parser.hpp.

add_library(kentos_script STATIC
    src/json_runner.cpp
    src/python_doc.cpp
    src/sandbox.cpp
)

target_include_directories(kentos_script PUBLIC include)
target_link_libraries(kentos_script
    PUBLIC  kentos_command
    PRIVATE kentos_flags
)

if(KENTOS_WITH_PYTHON)
    # THIS BLOCK USED TO BE A FATAL_ERROR saying the Python module was a Phase-2
    # deliverable. It is the deliverable now.
    if(NOT TARGET pybind11::embed)
        message(FATAL_ERROR
            "KENTOS_WITH_PYTHON=ON but pybind11 was not acquired. "
            "cmake/KentOSCadDependencies.cmake finds the package or fetches a "
            "pinned commit; see the message it prints, or configure with "
            "-DKENTOS_WITH_PYTHON=OFF.")
    endif()

    target_sources(kentos_script PRIVATE src/python_runner.cpp src/python_api.cpp)

    # PRIVATE, and that is the point of the pimpl in python_runner.hpp: nothing
    # that includes the header pays for <pybind11/embed.h> — which drags in
    # Python.h and every macro CPython defines — and swapping the binding library
    # touches one .cpp (CLAUDE.md Article 9).
    #
    # `pybind11::embed` and not `pybind11::module`: this links the interpreter in,
    # which is what an embedder needs and what an extension module must not do.
    target_link_libraries(kentos_script PRIVATE pybind11::embed)
    target_compile_definitions(kentos_script PUBLIC KENTOS_HAVE_PYTHON=1)
endif()

add_library(KentOSCad::script ALIAS kentos_script)
