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
    unit/test_docs.cpp
    unit/test_geodesy.cpp
    unit/test_geometry.cpp
    unit/test_golden.cpp
    unit/test_index.cpp
    unit/test_jitter.cpp
    unit/test_settings.cpp
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
)

target_include_directories(piricad_tests PRIVATE support)
target_link_libraries(piricad_tests PRIVATE
    piricad_core
    piricad_command
    piricad_domain_geodesy
    piricad_render
    piricad_script
    piricad_flags
)

set_target_properties(piricad_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME unit COMMAND piricad_tests)

# Every gate script runs as a test, so a violation fails `ctest`, not only CI.
# Discovered rather than listed: a new gate is picked up by re-running CMake, and
# there is no second list to keep in sync (CLAUDE.md 5.10, in spirit).
file(GLOB PIRICAD_GATES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/scripts/ci-gate-*.sh")
foreach(_gate IN LISTS PIRICAD_GATES)
    get_filename_component(_name "${_gate}" NAME_WE)
    add_test(NAME ${_name}
        COMMAND "${CMAKE_COMMAND}" -E env bash "${_gate}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

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
    bench/bench_pending.cpp
    bench/bench_render.cpp
)

target_include_directories(piricad_bench PRIVATE support bench)
target_compile_definitions(piricad_bench PRIVATE
    PIRICAD_BENCH_DIR="${CMAKE_SOURCE_DIR}/tests/bench"
)
target_link_libraries(piricad_bench PRIVATE
    piricad_core
    piricad_command
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
