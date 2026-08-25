# SPDX-License-Identifier: GPL-3.0-or-later
# Test suites. piricad.md §9.11 / §14 name six kinds; Phase 0 ships unit + journal.
#
#   unit     logic
#   golden   bit-identical cross-platform reference output   (Phase 1)
#   bench    the §10.1 performance budgets, as gates         (Phase 1)
#   fuzz     every parser, continuously                      (Phase 1)
#   journal  recorded real sessions, replayed nightly        (seeded here)
#   ai-eval  200-300 Turkish requests -> expected commands   (Phase 3)

add_executable(piricad_tests
    unit/main.cpp
    unit/test_core.cpp
    unit/test_command.cpp
    unit/test_attribute.cpp
    unit/test_database.cpp
    unit/test_docs.cpp
    unit/test_geodesy.cpp
    unit/test_geometry.cpp
    unit/test_golden.cpp
    unit/test_index.cpp
    unit/test_io.cpp
    unit/test_jitter.cpp
    unit/test_settings.cpp
    unit/test_snap.cpp
    unit/test_style_layer.cpp
    unit/test_style_rule.cpp
    unit/test_proof.cpp
)

# The manual's examples are executed straight out of the Markdown, so there is no
# second copy of them to drift (.claude/docs.md R10).
target_compile_definitions(piricad_tests PRIVATE
    PIRICAD_DOCS_DIR="${CMAKE_SOURCE_DIR}/docs"
    PIRICAD_JOURNAL_DIR="${CMAKE_SOURCE_DIR}/tests/journal"
    PIRICAD_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
    PIRICAD_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    PIRICAD_FUZZ_DIR="${CMAKE_SOURCE_DIR}/tests/fuzz"
)

target_include_directories(piricad_tests PRIVATE support)
target_link_libraries(piricad_tests PRIVATE
    doctest::doctest
    piricad_core
    piricad_command
    piricad_io
    piricad_domain_geodesy
    piricad_render
    piricad_script
    piricad_flags
)

set_target_properties(piricad_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# WORKING_DIRECTORY, not the build tree ctest would otherwise pick: test_docs.cpp
# executes the manual's examples exactly as printed (.claude/docs.md R10), and the
# manual writes catalogue paths the way a user types them — relative to the project
# root that holds /data. Running them from elsewhere would test a working directory
# rather than the example.
add_test(NAME unit COMMAND piricad_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ------------------------------------------------------------- shell start-up --
# The application must REACH its first frame. Nothing else in the suite runs the
# shell: the unit tests link no Qt and the gates read source, so a program that
# crashed on the line after `main` would pass every one of them — which is what
# happened when `installShellStyle()` and `loadShellFonts()` were called before
# `QApplication` existed. This starts the real binary offscreen and requires it
# to exit cleanly.
#
# `--version` IS NOT ENOUGH, and that is not a guess: it was the first version of
# this test and it passed while `make run` segfaulted. QCommandLineParser answers
# --version and exits before `MainWindow` is ever constructed, so the test proved
# only that the parser worked. `PIRICAD_FRAME_DUMP` builds the whole shell, paints
# one frame and exits — which is the thing that has to keep working.
if(TARGET piricad)
    add_test(NAME shell-starts
        COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                "PIRICAD_FRAME_DUMP=${CMAKE_CURRENT_BINARY_DIR}/shell-starts.png"
                "PIRICAD_DATA=${CMAKE_SOURCE_DIR}/data"
                $<TARGET_FILE:piricad>)
    set_tests_properties(shell-starts PROPERTIES TIMEOUT 120)

    # And every WINDOW, not only the shell. A dialog that crashes the moment it
    # opens passed `shell-starts` twice — once when a null was connected to, and
    # once when a theme walk recursed into itself — because nothing in the suite
    # had ever constructed one. `PIRICAD_SMOKE` opens the style designer, the
    # attribute table, the settings window and the command palette in turn.
    add_test(NAME windows-open
        COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                PIRICAD_SMOKE=1
                "PIRICAD_DATA=${CMAKE_SOURCE_DIR}/data"
                $<TARGET_FILE:piricad>)
    set_tests_properties(windows-open PROPERTIES TIMEOUT 120)
endif()

# Every gate script runs as a test, so a violation fails `ctest`, not only CI.
# Discovered rather than listed: a new gate is picked up by re-running CMake, and
# there is no second list to keep in sync (CLAUDE.md 5.10, in spirit).
file(GLOB PIRICAD_GATES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/scripts/ci-gate-*.sh")
file(GLOB PIRICAD_GATES_PY CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/scripts/ci-gate-*.py")
foreach(_gate IN LISTS PIRICAD_GATES)
    get_filename_component(_name "${_gate}" NAME_WE)
    add_test(NAME ${_name}
        COMMAND "${CMAKE_COMMAND}" -E env bash "${_gate}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

# A gate that has to read a catalogue is written in Python rather than bent into
# shell. It runs the same way and fails the same way; only the interpreter differs.
foreach(_gate IN LISTS PIRICAD_GATES_PY)
    get_filename_component(_name "${_gate}" NAME_WE)
    add_test(NAME ${_name}
        COMMAND "${CMAKE_COMMAND}" -E env python3 "${_gate}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
list(APPEND PIRICAD_GATES ${PIRICAD_GATES_PY})

if(NOT PIRICAD_GATES)
    message(FATAL_ERROR "No CI gate scripts found in ${CMAKE_SOURCE_DIR}/scripts")
endif()

# ---------------------------------------------------------------- benchmarks --
# piricad.md §10.1 budgets as gates. Kept out of `ctest` by default because the
# 5M-parcel fixture takes seconds and a baseline is machine-specific; CI runs it
# as its own job. `make bench` is the local entry point.

add_executable(piricad_bench
    bench/main.cpp
    bench/support.cpp
    bench/bench_command.cpp
    bench/bench_io.cpp
    bench/bench_pending.cpp
    bench/bench_render.cpp
    bench/bench_snap.cpp
)

target_include_directories(piricad_bench PRIVATE support bench)
target_compile_definitions(piricad_bench PRIVATE
    PIRICAD_BENCH_DIR="${CMAKE_SOURCE_DIR}/tests/bench"
)
target_link_libraries(piricad_bench PRIVATE
    benchmark::benchmark
    piricad_core
    piricad_command
    piricad_io
    piricad_render
    piricad_script
    piricad_flags
)
set_target_properties(piricad_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(PIRICAD_BUILD_BENCH)
    add_test(NAME bench COMMAND piricad_bench)
endif()

# ------------------------------------------------------------------- fuzz ----
# .claude/io.md R19: every parser has a libFuzzer target. Off by default and
# Clang-only; the seed corpora are replayed by piricad_tests regardless, so they
# defend the reader on every build (tests/unit/test_io.cpp).
add_subdirectory(fuzz)
