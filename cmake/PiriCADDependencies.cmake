# SPDX-License-Identifier: GPL-3.0-or-later
#
# Dependencies. CLAUDE.md Article 2.7 and 5.16: a mature, excellent, cross-platform
# library is used, never reimplemented.
#
# Two acquisition routes, chosen by what the library needs to build:
#
#   FetchContent, pinned to a COMMIT SHA — small, pure C++, no system dependencies.
#     Builds identically on Windows, macOS and Linux with nothing installed. A tag
#     can be moved; a SHA cannot, which is what CLAUDE.md 5.12 requires.
#
#   find_package — large libraries with native dependencies (GDAL, PROJ, GEOS, CGAL,
#     Qt, ICU, SQLite). Supplied by vcpkg on any platform, or by the system package
#     manager. /vcpkg.json pins them against a real baseline.
#
# Every dependency's licence is recorded in /NOTICE in the same change (Article 9).

include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

# Offline builds and packagers must not be forced onto the network.
option(PIRICAD_FETCH_DEPENDENCIES
       "Download pinned third-party sources when they are not found locally" ON)

if(NOT PIRICAD_FETCH_DEPENDENCIES)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON)
endif()

# --------------------------------------------------------------- pinned SHAs --
# Bumping one of these is a deliberate change: read the upstream changelog, update
# /NOTICE, regenerate the SBOM, and re-record the benchmark baseline if it is on a
# hot path.

set(PIRICAD_DEP_JSON_REPO      https://github.com/nlohmann/json.git)
set(PIRICAD_DEP_JSON_SHA       9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)  # v3.11.3

set(PIRICAD_DEP_DOCTEST_REPO   https://github.com/doctest/doctest.git)
# v2.4.12, not the v2.4.11 that sat here unused: 2.4.11 declares
# `cmake_minimum_required(VERSION 3.0)`, and CMake 4 removed compatibility with
# anything below 3.5. The pin was never exercised, so the breakage was invisible.
set(PIRICAD_DEP_DOCTEST_SHA    1da23a3e8119ec5cce4f9388e91b065e20bf06f5)  # v2.4.12

set(PIRICAD_DEP_BENCHMARK_REPO https://github.com/google/benchmark.git)
set(PIRICAD_DEP_BENCHMARK_SHA  c58e6d0710581e3a08d65c349664128a8d9a2461)  # v1.9.1

set(PIRICAD_DEP_XXHASH_REPO    https://github.com/Cyan4973/xxHash.git)
set(PIRICAD_DEP_XXHASH_SHA     e626a72bc2321cd320e953a0ccf1584cad60f363)  # v0.8.3

set(PIRICAD_DEP_CLIPPER2_REPO  https://github.com/AngusJohnson/Clipper2.git)
set(PIRICAD_DEP_CLIPPER2_SHA   736ddb0b53d97fd5f65dd3d9bbf8a0993eaf387c)  # Clipper2_1.4.0

set(PIRICAD_DEP_CDT_REPO       https://github.com/artem-ogre/CDT.git)
set(PIRICAD_DEP_CDT_SHA        21fae3ba957551b46130349c318b030f85451be4)  # 1.4.1

set(PIRICAD_DEP_FMT_REPO       https://github.com/fmtlib/fmt.git)
set(PIRICAD_DEP_FMT_SHA        0c9fce2ffefecfdce794e1859584e25877b7b592)  # 11.0.2

set(PIRICAD_DEP_SPDLOG_REPO    https://github.com/gabime/spdlog.git)
# v1.15.3, not the v1.14.1 that was pinned first: 1.14.1 predates fmt 11 and its
# SPDLOG_LOGGER_CATCH macro calls FMT_STRING, whose lambda trips fmt 11's consteval
# format-string constructor. GCC accepts it; clang REJECTS it, so the pairing built
# here and would have broken the macOS and clang CI jobs Article 6.1 requires.
# Found by clang-tidy, which parses with clang on a GCC build — the one tool in the
# pipeline that sees the other compiler's opinion.
set(PIRICAD_DEP_SPDLOG_SHA     6fa36017cfd5731d617e1a934f0e5ea9c4445b13)  # v1.15.3

# ------------------------------------------------------------------ helpers --

# Prefers an installed copy, falls back to the pinned source. A distribution
# packaging PiriCAD gets its own build of the library; a developer on a fresh
# machine gets a working build with nothing installed.
function(piricad_dependency name)
    cmake_parse_arguments(ARG "" "REPO;SHA;PACKAGE;VERSION" "" ${ARGN})

    if(ARG_PACKAGE)
        find_package(${ARG_PACKAGE} ${ARG_VERSION} QUIET)
        if(${ARG_PACKAGE}_FOUND)
            message(STATUS "  ${name}: sistemden (${ARG_PACKAGE} ${${ARG_PACKAGE}_VERSION})")
            return()
        endif()
    endif()

    if(NOT PIRICAD_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "${name} bulunamadı ve PIRICAD_FETCH_DEPENDENCIES=OFF.\n"
            "  Ya paketi kurun, ya vcpkg kullanın, ya da indirmeye izin verin.")
    endif()

    message(STATUS "  ${name}: sabitlenmiş kaynaktan (${ARG_SHA})")
    FetchContent_Declare(${name}
        GIT_REPOSITORY ${ARG_REPO}
        GIT_TAG        ${ARG_SHA}
        GIT_SHALLOW    FALSE
        SYSTEM                       # third-party warnings are not our warnings
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(${name})
endfunction()

message(STATUS "PiriCAD bağımlılıkları:")

# ------------------------------------------------------------- acquisition --
#
# Only what is actually LINKED is fetched. A SHA sitting in this file and nothing
# asking for it is what the eight entries above were until now: reviewed, recorded
# in /NOTICE as though they shipped, and compiled into nothing. Each library moves
# down here in the change that starts using it, and /NOTICE moves with it.

option(PIRICAD_WITH_JSON "Use nlohmann/json for the JSON facade" ON)

if(PIRICAD_WITH_JSON)
    # ordered_json, NOT json. The default container sorts object keys, and the
    # journal is compared byte for byte across three clients (CLAUDE.md 6.4) with
    # golden fixtures recording the exact bytes. Sorted keys would rewrite every
    # fixture and, worse, would make the file format's key order an accident of
    # the alphabet rather than a decision.
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    piricad_dependency(nlohmann_json
        REPO ${PIRICAD_DEP_JSON_REPO}
        SHA  ${PIRICAD_DEP_JSON_SHA}
        PACKAGE nlohmann_json
        VERSION 3.11.0)
endif()

option(PIRICAD_WITH_SPDLOG "Use spdlog for logging" ON)

if(PIRICAD_WITH_SPDLOG)
    # fmt as an external dependency of spdlog rather than its bundled copy: two
    # copies of fmt in one binary is the ODR violation that shows up as a crash in
    # a formatting call nobody changed.
    set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "")
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
    set(SPDLOG_INSTALL OFF CACHE INTERNAL "")
    set(FMT_INSTALL OFF CACHE INTERNAL "")
    set(FMT_TEST OFF CACHE INTERNAL "")
    piricad_dependency(fmt
        REPO ${PIRICAD_DEP_FMT_REPO}
        SHA  ${PIRICAD_DEP_FMT_SHA}
        PACKAGE fmt
        VERSION 10.0)
    piricad_dependency(spdlog
        REPO ${PIRICAD_DEP_SPDLOG_REPO}
        SHA  ${PIRICAD_DEP_SPDLOG_SHA}
        PACKAGE spdlog
        VERSION 1.12)
endif()

option(PIRICAD_WITH_DOCTEST "Use doctest as the unit-test framework" ON)

if(PIRICAD_WITH_DOCTEST)
    # Header-only and self-registering, so a test file needs no CMake entry beyond
    # its source line. The reason it is doctest rather than Catch2 or GoogleTest is
    # compile time: this suite is one binary of ~280 cases that every `make check`
    # rebuilds, and doctest's headers cost a fraction of the alternatives'.
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")
    piricad_dependency(doctest
        REPO ${PIRICAD_DEP_DOCTEST_REPO}
        SHA  ${PIRICAD_DEP_DOCTEST_SHA}
        PACKAGE doctest
        VERSION 2.4)
endif()

option(PIRICAD_WITH_BENCHMARK "Use Google Benchmark to time the Article 7 budgets" ON)

if(PIRICAD_WITH_BENCHMARK)
    # Google Benchmark MEASURES; it does not gate. The Article 7 budgets and the
    # per-machine baseline stay ours (tests/support/benchmark.hpp), because no
    # library knows that 16 ms is a product requirement. What it replaces is the
    # part that is genuinely hard and that we had hand-rolled: choosing an
    # iteration count, discarding warm-up, and reporting a statistic that is not
    # an artefact of the scheduler.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE INTERNAL "")
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
    set(BENCHMARK_INSTALL_DOCS OFF CACHE INTERNAL "")
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE INTERNAL "")
    # Its warnings are not our warnings, and -Werror inside a dependency turns a
    # compiler upgrade into a broken build of code we do not own (CLAUDE.md 5.14
    # is about OUR warnings, which stay fatal).
    set(BENCHMARK_ENABLE_WERROR OFF CACHE INTERNAL "")
    piricad_dependency(benchmark
        REPO ${PIRICAD_DEP_BENCHMARK_REPO}
        SHA  ${PIRICAD_DEP_BENCHMARK_SHA}
        PACKAGE benchmark
        VERSION 1.8)
endif()
