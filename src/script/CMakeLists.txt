# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_script — script hosts. Depends on command; never the reverse.
#
# Two hosts, one architecture. The JSON command-array runner needs no external
# dependency and is always built; the Python host sits behind PIRICAD_WITH_PYTHON,
# which defaults OFF and which the application must build, start and test without
# (.claude/script.md R6). An embedded Lua stood beside them until the amendment
# that removed it: one embedded language, and the per-feature hot path it was
# there for belongs to the compiled expression engine in command/parser.hpp.

add_library(piricad_script STATIC
    src/json_runner.cpp
    src/python_doc.cpp
    src/sandbox.cpp
)

target_include_directories(piricad_script PUBLIC include)
target_link_libraries(piricad_script
    PUBLIC  piricad_command
    PRIVATE piricad_flags
)

if(PIRICAD_WITH_PYTHON)
    # THIS BLOCK USED TO BE A FATAL_ERROR saying the Python module was a Phase-2
    # deliverable. It is the deliverable now.
    if(NOT TARGET pybind11::embed)
        message(FATAL_ERROR
            "PIRICAD_WITH_PYTHON=ON but pybind11 was not acquired. "
            "cmake/PiriCADDependencies.cmake finds the package or fetches a "
            "pinned commit; see the message it prints, or configure with "
            "-DPIRICAD_WITH_PYTHON=OFF.")
    endif()

    target_sources(piricad_script PRIVATE src/python_runner.cpp src/python_api.cpp)

    # PRIVATE, and that is the point of the pimpl in python_runner.hpp: nothing
    # that includes the header pays for <pybind11/embed.h> — which drags in
    # Python.h and every macro CPython defines — and swapping the binding library
    # touches one .cpp (CLAUDE.md Article 9).
    #
    # `pybind11::embed` and not `pybind11::module`: this links the interpreter in,
    # which is what an embedder needs and what an extension module must not do.
    target_link_libraries(piricad_script PRIVATE pybind11::embed)
    target_compile_definitions(piricad_script PUBLIC PIRICAD_HAVE_PYTHON=1)
endif()

add_library(PiriCAD::script ALIAS piricad_script)
