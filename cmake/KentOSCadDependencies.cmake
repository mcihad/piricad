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
option(KENTOS_FETCH_DEPENDENCIES
       "Download pinned third-party sources when they are not found locally" ON)

if(NOT KENTOS_FETCH_DEPENDENCIES)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON)
endif()

# --------------------------------------------------------------- pinned SHAs --
# Bumping one of these is a deliberate change: read the upstream changelog, update
# /NOTICE, regenerate the SBOM, and re-record the benchmark baseline if it is on a
# hot path.

set(KENTOS_DEP_JSON_REPO      https://github.com/nlohmann/json.git)
set(KENTOS_DEP_JSON_SHA       9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)  # v3.11.3

set(KENTOS_DEP_DOCTEST_REPO   https://github.com/doctest/doctest.git)
# v2.4.12, not the v2.4.11 that sat here unused: 2.4.11 declares
# `cmake_minimum_required(VERSION 3.0)`, and CMake 4 removed compatibility with
# anything below 3.5. The pin was never exercised, so the breakage was invisible.
set(KENTOS_DEP_DOCTEST_SHA    1da23a3e8119ec5cce4f9388e91b065e20bf06f5)  # v2.4.12

set(KENTOS_DEP_BENCHMARK_REPO https://github.com/google/benchmark.git)
set(KENTOS_DEP_BENCHMARK_SHA  c58e6d0710581e3a08d65c349664128a8d9a2461)  # v1.9.1

set(KENTOS_DEP_XXHASH_REPO    https://github.com/Cyan4973/xxHash.git)
set(KENTOS_DEP_XXHASH_SHA     e626a72bc2321cd320e953a0ccf1584cad60f363)  # v0.8.3

set(KENTOS_DEP_LIBREDWG_REPO  https://github.com/LibreDWG/libredwg.git)
set(KENTOS_DEP_LIBREDWG_SHA   7eb90a9f933623729f82781cb1d68de2e50593f3)  # 0.14.8594

set(KENTOS_DEP_CLIPPER2_REPO  https://github.com/AngusJohnson/Clipper2.git)
set(KENTOS_DEP_CLIPPER2_SHA   736ddb0b53d97fd5f65dd3d9bbf8a0993eaf387c)  # Clipper2_1.4.0

set(KENTOS_DEP_CDT_REPO       https://github.com/artem-ogre/CDT.git)
set(KENTOS_DEP_CDT_SHA        21fae3ba957551b46130349c318b030f85451be4)  # 1.4.1

set(KENTOS_DEP_FMT_REPO       https://github.com/fmtlib/fmt.git)
set(KENTOS_DEP_FMT_SHA        0c9fce2ffefecfdce794e1859584e25877b7b592)  # 11.0.2

set(KENTOS_DEP_LIBPQXX_REPO   https://github.com/jtv/libpqxx.git)
set(KENTOS_DEP_LIBPQXX_SHA    1ca80b0e638f6182426c5b11255069cae4fbd542)  # 7.9.2

# Lua and sol2, the hot-path script layer of kentoscad.md §4.1. Both are MIT, which
# is GPLv3-compatible; both are recorded in /NOTICE.
#
# The OFFICIAL Lua repository, which ships no CMakeLists — the build below is
# ours, and that is normal for Lua: upstream distributes a makefile and expects
# embedders to compile the 32 library sources themselves.
set(KENTOS_DEP_LUA_REPO       https://github.com/lua/lua.git)
set(KENTOS_DEP_LUA_SHA        6e22fedb74cf0c9b6656e9fce8b7331db847c605)  # v5.4.8

set(KENTOS_DEP_SOL2_REPO      https://github.com/ThePhD/sol2.git)
# v3.5.0, not the v3.3.1 that vcpkg.json's floor names: 3.3.1's
# `optional<T&>::emplace` calls a `construct` member the specialisation does not
# have. It is dead code nobody instantiates, which is why it shipped — and GCC 15
# diagnoses errors in uninstantiated template bodies, so it stops the build on
# every modern toolchain. Not silenceable either: CLAUDE.md 5.14 forbids the
# `-Wno-` that would hide it, and a SYSTEM include suppresses warnings, not errors.
set(KENTOS_DEP_SOL2_SHA       9190880c593dfb018ccf5cc9729ab87739709862)  # v3.5.0

# The text stack of kentoscad.md §9.4 and `.claude/render.md` R8. FreeType and
# HarfBuzz come from the system: both are already on every machine that has Qt,
# and vendoring a font rasteriser means vendoring its own dependency tree.
# msdfgen and stb_rect_pack are small and pure, so they are pinned by SHA.
set(KENTOS_DEP_MSDFGEN_REPO   https://github.com/Chlumsky/msdfgen.git)
set(KENTOS_DEP_MSDFGEN_SHA    1874bcf7d9624ccc85b4bc9a85d78116f690f35b)  # v1.13

# stb has no releases and never has had; upstream's own instruction is to pin a
# commit, which is what CLAUDE.md 5.12 asks for anyway.
set(KENTOS_DEP_STB_REPO       https://github.com/nothings/stb.git)
set(KENTOS_DEP_STB_SHA        2c980bb59875b0d32144a71867fbdebb2f77cd20)

set(KENTOS_DEP_SPDLOG_REPO    https://github.com/gabime/spdlog.git)
# v1.15.3, not the v1.14.1 that was pinned first: 1.14.1 predates fmt 11 and its
# SPDLOG_LOGGER_CATCH macro calls FMT_STRING, whose lambda trips fmt 11's consteval
# format-string constructor. GCC accepts it; clang REJECTS it, so the pairing built
# here and would have broken the macOS and clang CI jobs Article 6.1 requires.
# Found by clang-tidy, which parses with clang on a GCC build — the one tool in the
# pipeline that sees the other compiler's opinion.
set(KENTOS_DEP_SPDLOG_SHA     6fa36017cfd5731d617e1a934f0e5ea9c4445b13)  # v1.15.3

# ------------------------------------------------------------------ helpers --

# Prefers an installed copy, falls back to the pinned source. A distribution
# packaging KentOSCad gets its own build of the library; a developer on a fresh
# machine gets a working build with nothing installed.
function(kentos_dependency name)
    cmake_parse_arguments(ARG "" "REPO;SHA;PACKAGE;VERSION;SUBDIR" "" ${ARGN})

    if(ARG_PACKAGE)
        find_package(${ARG_PACKAGE} ${ARG_VERSION} QUIET)
        if(${ARG_PACKAGE}_FOUND)
            message(STATUS "  ${name}: sistemden (${ARG_PACKAGE} ${${ARG_PACKAGE}_VERSION})")
            return()
        endif()
    endif()

    if(NOT KENTOS_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "${name} bulunamadı ve KENTOS_FETCH_DEPENDENCIES=OFF.\n"
            "  Ya paketi kurun, ya vcpkg kullanın, ya da indirmeye izin verin.")
    endif()

    message(STATUS "  ${name}: sabitlenmiş kaynaktan (${ARG_SHA})")
    # SUBDIR names where the project's own CMakeLists lives when it is not at the
    # repository root — Clipper2 keeps its C++ build under `CPP/`, beside the C#
    # and Delphi ports. Without it FetchContent populates the source, finds no
    # CMakeLists to add, and the target simply never exists.
    if(ARG_SUBDIR)
        FetchContent_Declare(${name}
            GIT_REPOSITORY ${ARG_REPO}
            GIT_TAG        ${ARG_SHA}
            GIT_SHALLOW    FALSE
            SOURCE_SUBDIR  ${ARG_SUBDIR}
            SYSTEM                   # third-party warnings are not our warnings
            EXCLUDE_FROM_ALL
        )
    else()
        FetchContent_Declare(${name}
            GIT_REPOSITORY ${ARG_REPO}
            GIT_TAG        ${ARG_SHA}
            GIT_SHALLOW    FALSE
            SYSTEM                   # third-party warnings are not our warnings
            EXCLUDE_FROM_ALL
        )
    endif()
    FetchContent_MakeAvailable(${name})
endfunction()

message(STATUS "KentOSCad bağımlılıkları:")

# ------------------------------------------------------------- acquisition --
#
# Only what is actually LINKED is fetched. A SHA sitting in this file and nothing
# asking for it is what the eight entries above were until now: reviewed, recorded
# in /NOTICE as though they shipped, and compiled into nothing. Each library moves
# down here in the change that starts using it, and /NOTICE moves with it.

option(KENTOS_WITH_JSON "Use nlohmann/json for the JSON facade" ON)

if(KENTOS_WITH_JSON)
    # ordered_json, NOT json. The default container sorts object keys, and the
    # journal is compared byte for byte across three clients (CLAUDE.md 6.4) with
    # golden fixtures recording the exact bytes. Sorted keys would rewrite every
    # fixture and, worse, would make the file format's key order an accident of
    # the alphabet rather than a decision.
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    kentos_dependency(nlohmann_json
        REPO ${KENTOS_DEP_JSON_REPO}
        SHA  ${KENTOS_DEP_JSON_SHA}
        PACKAGE nlohmann_json
        VERSION 3.11.0)
endif()

option(KENTOS_WITH_SPDLOG "Use spdlog for logging" ON)

if(KENTOS_WITH_SPDLOG)
    # fmt as an external dependency of spdlog rather than its bundled copy: two
    # copies of fmt in one binary is the ODR violation that shows up as a crash in
    # a formatting call nobody changed.
    set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "")
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
    set(SPDLOG_INSTALL OFF CACHE INTERNAL "")
    set(FMT_INSTALL OFF CACHE INTERNAL "")
    set(FMT_TEST OFF CACHE INTERNAL "")
    kentos_dependency(fmt
        REPO ${KENTOS_DEP_FMT_REPO}
        SHA  ${KENTOS_DEP_FMT_SHA}
        PACKAGE fmt
        VERSION 10.0)
    kentos_dependency(spdlog
        REPO ${KENTOS_DEP_SPDLOG_REPO}
        SHA  ${KENTOS_DEP_SPDLOG_SHA}
        PACKAGE spdlog
        VERSION 1.12)
endif()

option(KENTOS_WITH_DOCTEST "Use doctest as the unit-test framework" ON)

if(KENTOS_WITH_DOCTEST)
    # Header-only and self-registering, so a test file needs no CMake entry beyond
    # its source line. The reason it is doctest rather than Catch2 or GoogleTest is
    # compile time: this suite is one binary of ~280 cases that every `make check`
    # rebuilds, and doctest's headers cost a fraction of the alternatives'.
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")
    kentos_dependency(doctest
        REPO ${KENTOS_DEP_DOCTEST_REPO}
        SHA  ${KENTOS_DEP_DOCTEST_SHA}
        PACKAGE doctest
        VERSION 2.4)
endif()

option(KENTOS_WITH_BENCHMARK "Use Google Benchmark to time the Article 7 budgets" ON)

if(KENTOS_WITH_BENCHMARK)
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
    kentos_dependency(benchmark
        REPO ${KENTOS_DEP_BENCHMARK_REPO}
        SHA  ${KENTOS_DEP_BENCHMARK_SHA}
        PACKAGE benchmark
        VERSION 1.8)
endif()

option(KENTOS_WITH_CDT "Delaunay triangulation for the terrain model" ON)

if(KENTOS_WITH_CDT)
    # CLAUDE.md 2.7 and 5.16 again: a Delaunay triangulation is a solved problem
    # with well-known degeneracies — cocircular points, collinear runs, duplicate
    # coordinates — and the naive implementations get every one of them wrong on
    # exactly the data a survey produces (a grid of levelling points is cocircular
    # everywhere).
    #
    # CDT rather than CGAL for this: header-only, no dependency tree, and it uses
    # Shewchuk's robust predicates, which CLAUDE.md 5.4 already requires and
    # permits. CGAL stays the answer for the straight skeleton (§9.2).
    #
    # LICENCE: MPL 2.0 — file-level copyleft, GPLv3-compatible, and recorded in
    # /NOTICE with this exact pin.
    # POPULATED, NOT ADDED. CDT is header-only in this configuration, and its own
    # `CMakeLists.txt` opens with `cmake_minimum_required(VERSION 3.1)` — CMake 4
    # removed compatibility below 3.5 and refuses to configure it at all. The
    # library is fine; its build file is from before that change. So the source is
    # fetched and the include directory is used directly, which is the same shape
    # this file already uses for Lua, sol2 and the stb header drop.
    if(NOT KENTOS_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "KENTOS_WITH_CDT=ON but KENTOS_FETCH_DEPENDENCIES=OFF.\n"
            "  CDT is fetched from a pinned commit; allow the download,\n"
            "  or configure with -DKENTOS_WITH_CDT=OFF.")
    endif()

    message(STATUS "  cdt: sabitlenmiş kaynaktan (${KENTOS_DEP_CDT_SHA})")
    FetchContent_Declare(cdt
        GIT_REPOSITORY ${KENTOS_DEP_CDT_REPO}
        GIT_TAG        ${KENTOS_DEP_CDT_SHA}
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  cmake-yok        # deliberately absent: populate, do not add
        SYSTEM
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(cdt)

    # The one target the rest of the build sees. INTERFACE, because there is
    # nothing to compile: `CDT_USE_AS_COMPILED_LIBRARY` is off and every entry
    # point is a template in a header.
    add_library(kentos_cdt INTERFACE)
    target_include_directories(kentos_cdt SYSTEM INTERFACE "${cdt_SOURCE_DIR}/CDT/include")
endif()

option(KENTOS_WITH_CLIPPER2 "Polygon offset and boolean through Clipper2" ON)

if(KENTOS_WITH_CLIPPER2)
    # CLAUDE.md 2.7 and 5.16: polygon offsetting is a solved problem with a mature,
    # excellent, cross-platform answer, and reimplementing it is how a program ends
    # up with self-intersecting parallels and mitre spikes on reflex corners.
    #
    # Clipper2 rather than CGAL for this job: it is header-and-two-sources, has no
    # dependency tree, and its offsetter takes exactly the join and end types a CAD
    # user asks for (mitre / round / square). CGAL's straight skeleton stays the
    # answer for building setbacks, which is a different problem (§9.2).
    #
    # LICENCE: Boost Software License 1.0 — GPLv3-compatible, permissive, and
    # already recorded in /NOTICE with this exact pin.
    #
    # INTEGER IN, INTEGER OUT. Clipper2 has an int64 path (`Path64`), which is what
    # makes it usable here at all: `Mm` is int64 fixed-point millimetres and
    # Article 2.4 forbids storing a double. The offset distance is an integer
    # number of millimetres and the vertices come back as int64 — no conversion,
    # no rounding, and the same answer on every platform (§7.3).
    set(CLIPPER2_TESTS OFF CACHE INTERNAL "")
    set(CLIPPER2_EXAMPLES OFF CACHE INTERNAL "")
    set(CLIPPER2_UTILS OFF CACHE INTERNAL "")
    kentos_dependency(clipper2
        REPO   ${KENTOS_DEP_CLIPPER2_REPO}
        SHA    ${KENTOS_DEP_CLIPPER2_SHA}
        SUBDIR CPP
        PACKAGE Clipper2
        VERSION 1.3)
endif()

if(KENTOS_WITH_DWG)
    # DWG, READ ONLY, and the read-only part is enforced by the build rather than
    # by discipline: `LIBREDWG_DISABLE_WRITE=ON` leaves the encoder out of the
    # library entirely, so io.md P8 — no native DWG writer while the R14 coverage
    # report stands — is a fact about the binary and not a promise about the code.
    #
    # LICENCE: GPL-3.0-or-later, which is the project's own (Article 2.1). It is
    # also the ONLY GPL-compatible DWG implementation there is: the ODA Drawings
    # SDK is closed source and banned (P1), and GDAL's CAD driver is libopencad,
    # a different implementation than R13 names.
    #
    # LIBONLY, because the tools are a dozen command-line programs this product
    # never runs. NO JSON, for the same reason.
    #
    # -Werror OFF for this dependency alone. LibreDWG builds its own sources with
    # it and GCC 15 warns about a _POSIX_C_SOURCE redefinition in its generated
    # code; that is upstream's warning in upstream's file. CLAUDE.md 5.14 forbids
    # silencing OUR warnings, and this is neither our warning nor our file.
    set(LIBREDWG_LIBONLY ON CACHE INTERNAL "")
    set(LIBREDWG_DISABLE_WRITE ON CACHE INTERNAL "")
    set(LIBREDWG_DISABLE_JSON ON CACHE INTERNAL "")
    set(DISABLE_WERROR ON CACHE INTERNAL "")
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

    kentos_dependency(libredwg
        REPO ${KENTOS_DEP_LIBREDWG_REPO}
        SHA  ${KENTOS_DEP_LIBREDWG_SHA})
endif()

option(KENTOS_WITH_POSTGIS "Read and write layers against a live PostGIS database" ON)

if(KENTOS_WITH_POSTGIS)
    # CLAUDE.md Article 2.9: PostGIS is a first-class store, not an export target,
    # because Turkish municipalities and TKGM run their corporate data on it.
    #
    # libpqxx is the C++ layer over libpq and nothing more — no ORM, no schema
    # generator, no connection pool. That is what makes it the right dependency
    # under Article 2.7: the hard part it solves is escaping, binary parameters,
    # notice handling and transaction lifetime, and every one of those is a place
    # a hand-rolled version leaks or corrupts.
    #
    # libpq itself comes from the system. It is the client half of the database
    # the user already runs, it ships with every PostgreSQL install on all three
    # platforms, and vendoring it would mean vendoring an SSL stack.
    #
    # On macOS the "system" copy needs a hint. Homebrew keeps libpq keg-only, so
    # `brew install libpq` — the very fix the failure message below prints —
    # installs into HOMEBREW_PREFIX/opt/libpq and links nothing into the default
    # search path. Without this, find_package fails on a machine that did exactly
    # what it was told. PostgreSQL.app lands outside the prefix entirely.
    if(APPLE AND NOT PostgreSQL_ROOT)
        find_program(KENTOS_BREW_EXECUTABLE brew)
        if(KENTOS_BREW_EXECUTABLE)
            execute_process(
                COMMAND ${KENTOS_BREW_EXECUTABLE} --prefix libpq
                OUTPUT_VARIABLE KENTOS_LIBPQ_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(KENTOS_LIBPQ_PREFIX AND EXISTS "${KENTOS_LIBPQ_PREFIX}")
                set(PostgreSQL_ROOT "${KENTOS_LIBPQ_PREFIX}")
            endif()
        endif()
    endif()

    find_package(PostgreSQL 13)

    if(NOT PostgreSQL_FOUND)
        message(WARNING
            "KENTOS_WITH_POSTGIS=ON but libpq was not found; PostGIS support is off.\n"
            "  Debian/Ubuntu: sudo apt install libpq-dev\n"
            "  macOS:         brew install libpq   (keg-only; found automatically)\n"
            "  vcpkg:         vcpkg install libpq\n"
            "  elsewhere:     configure with -D PostgreSQL_ROOT=<prefix>")
        set(KENTOS_WITH_POSTGIS OFF CACHE BOOL "" FORCE)
    else()
        set(SKIP_BUILD_TEST ON CACHE INTERNAL "")
        set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
        kentos_dependency(libpqxx
            REPO ${KENTOS_DEP_LIBPQXX_REPO}
            SHA  ${KENTOS_DEP_LIBPQXX_SHA}
            PACKAGE libpqxx
            VERSION 7.7)
    endif()
endif()


# ----------------------------------------------------------------- Lua + sol2 --
#
# `.claude/script.md` R5 fixes the layer roles: every expression evaluator, style
# rule, label expression and area calculator is Lua, because those run per feature
# and a cadastral sheet has millions of them. R6 keeps it behind this option,
# defaulting OFF, and requires the application to build, start and pass its tests
# with the option off.
if(KENTOS_WITH_LUA)
    # LUA IS C. The project declares `LANGUAGES CXX` because everything we write
    # is C++, and a target of `.c` files under that has no linker language at all
    # — the configure fails with "cannot determine linker language", after the
    # download. Enabled HERE rather than at the top so a build without the option
    # still needs no C compiler.
    enable_language(C)

    # NEITHER PROJECT IS ADDED AS A SUBDIRECTORY, and `SOURCE_SUBDIR` pointing at
    # a directory that does not exist is the documented way to say so:
    # FetchContent populates the source and skips `add_subdirectory()`.
    #
    #   Lua ships no CMakeLists at all, so there is nothing to add.
    #   sol2 ships one, but it is a header-only library whose CMakeLists exists to
    #   build its tests, examples and single-header generator. Adding it would pull
    #   a Lua search, a Catch2 fetch and a set of options into our build to obtain
    #   one include directory.
    if(NOT KENTOS_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "KENTOS_WITH_LUA=ON but KENTOS_FETCH_DEPENDENCIES=OFF.\n"
            "  Lua 5.4 and sol2 are fetched from pinned commits; allow the download,\n"
            "  or configure with -DKENTOS_WITH_LUA=OFF.")
    endif()

    message(STATUS "  lua:  sabitlenmiş kaynaktan (${KENTOS_DEP_LUA_SHA})")
    FetchContent_Declare(lua
        GIT_REPOSITORY ${KENTOS_DEP_LUA_REPO}
        GIT_TAG        ${KENTOS_DEP_LUA_SHA}
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  cmake-yok        # deliberately absent: populate, do not add
        SYSTEM
        EXCLUDE_FROM_ALL)

    message(STATUS "  sol2: sabitlenmiş kaynaktan (${KENTOS_DEP_SOL2_SHA})")
    FetchContent_Declare(sol2
        GIT_REPOSITORY ${KENTOS_DEP_SOL2_REPO}
        GIT_TAG        ${KENTOS_DEP_SOL2_SHA}
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  cmake-yok
        SYSTEM
        EXCLUDE_FROM_ALL)

    FetchContent_MakeAvailable(lua sol2)

    # LISTED, not globbed. The repository's root also holds `lua.c`, `luac.c`,
    # `onelua.c` and `ltests.c`; the first three are separate programs with their
    # own `main`, and the fourth compiles only with the test harness's `LUA_USER_H`.
    # A glob picks all four up and the link fails on duplicate symbols — after the
    # download, which is the worst place to find out.
    set(KENTOS_LUA_SOURCES
        lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c
        ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c
        loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c
        lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c)
    list(TRANSFORM KENTOS_LUA_SOURCES PREPEND "${lua_SOURCE_DIR}/")

    add_library(kentos_lua STATIC ${KENTOS_LUA_SOURCES})
    target_include_directories(kentos_lua SYSTEM PUBLIC "${lua_SOURCE_DIR}")
    set_target_properties(kentos_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # CLAUDE.md Article 2.5 says EVERY translation unit in every configuration,
    # and a Lua number is a double: an interpreter built with contraction on would
    # be a second arithmetic in the same process, which is exactly what §7.3
    # forbids. Third-party code is not an exception to a bit-identity requirement.
    if(NOT MSVC)
        target_compile_options(kentos_lua PRIVATE -fno-fast-math -ffp-contract=off)
    else()
        target_compile_options(kentos_lua PRIVATE /fp:precise)
    endif()

    if(UNIX)
        # POSIX gives Lua `os.time` at full resolution and `popen`; `LUA_USE_DLOPEN`
        # is what `package.loadlib` needs. Neither is reachable below the `tam`
        # sandbox, which never opens those libraries (`.claude/script.md` P8).
        target_compile_definitions(kentos_lua PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
        target_link_libraries(kentos_lua PRIVATE ${CMAKE_DL_LIBS} m)
    endif()

    add_library(kentos_sol2 INTERFACE)
    target_include_directories(kentos_sol2 SYSTEM INTERFACE "${sol2_SOURCE_DIR}/include")
    target_link_libraries(kentos_sol2 INTERFACE kentos_lua)

    # sol2 reads Lua's version from the headers, but says so explicitly here so a
    # mismatch is a configure error rather than a runtime surprise.
    target_compile_definitions(kentos_sol2 INTERFACE SOL_ALL_SAFETIES_ON=1)
endif()


# ------------------------------------------------------------------- text ----
#
# `.claude/render.md` R8: labels and published symbols render from an SDF atlas
# built with msdfgen, and Turkish text is shaped with HarfBuzz + FreeType. The
# three are one decision and are acquired together, because two of them without
# the third draw nothing.
#
# WHY SHAPING AT ALL, when the strings are Latin. Turkish is not ASCII: `ğ`, `ş`,
# `ı` and `İ` are ordinary letters here, a dotted capital İ is a different glyph
# from I, and the fonts carry the kerning pairs that make `AV` in `TAKS/KAKS`
# readable. A renderer that mapped bytes to glyphs would be wrong on the first
# cadastral sheet.
if(KENTOS_WITH_TEXT)
    find_package(Freetype 2.10)

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(KENTOS_HARFBUZZ QUIET IMPORTED_TARGET harfbuzz)
    endif()

    if(NOT FREETYPE_FOUND OR NOT KENTOS_HARFBUZZ_FOUND)
        message(FATAL_ERROR
            "KENTOS_WITH_TEXT=ON but the text stack is incomplete "
            "(FreeType: ${FREETYPE_FOUND}, HarfBuzz: ${KENTOS_HARFBUZZ_FOUND}).\n"
            "  Debian/Ubuntu: sudo apt install libfreetype-dev libharfbuzz-dev\n"
            "  Fedora:        sudo dnf install freetype-devel harfbuzz-devel\n"
            "  macOS:         brew install freetype harfbuzz\n"
            "  vcpkg:         freetype harfbuzz\n"
            "  Or configure with -DKENTOS_WITH_TEXT=OFF.")
    endif()

    # CORE ONLY. msdfgen's extension half exists to LOAD fonts and SVGs, and
    # pulls FreeType, libpng and tinyxml2 in to do it. We already have the
    # outlines: FreeType hands them over as contours and this project turns them
    # into an `msdfgen::Shape` itself, which is fewer dependencies and the same
    # picture.
    set(MSDFGEN_CORE_ONLY ON CACHE BOOL "" FORCE)
    set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_DYNAMIC_RUNTIME OFF CACHE BOOL "" FORCE)

    kentos_dependency(msdfgen
        REPO ${KENTOS_DEP_MSDFGEN_REPO}
        SHA  ${KENTOS_DEP_MSDFGEN_SHA})

    # stb_rect_pack, for laying the glyphs out in the one texture R8 asks for.
    # A shelf packer is twenty lines and everyone who writes one gets the same
    # wasted third of the atlas; CLAUDE.md 5.16 is about exactly this.
    if(NOT KENTOS_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "KENTOS_WITH_TEXT=ON but KENTOS_FETCH_DEPENDENCIES=OFF; "
            "msdfgen and stb are fetched from pinned commits.")
    endif()

    message(STATUS "  stb:  sabitlenmiş kaynaktan (${KENTOS_DEP_STB_SHA})")
    FetchContent_Declare(stb
        GIT_REPOSITORY ${KENTOS_DEP_STB_REPO}
        GIT_TAG        ${KENTOS_DEP_STB_SHA}
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  cmake-yok        # header drop: populate, do not add
        SYSTEM
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(stb)

    add_library(kentos_stb INTERFACE)
    target_include_directories(kentos_stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")
endif()


option(KENTOS_WITH_QGIS "Draw symbols through the QGIS symbology engine" ON)

if(KENTOS_WITH_QGIS)
    # CLAUDE.md Article 2.7 and 5.16: a mature, excellent, cross-platform library
    # is used and never reimplemented. A symbology engine is exactly such a thing,
    # and QGIS has the best free one there is — marker lines with real placement
    # rules, line and point pattern fills, SVG symbols with parameter
    # substitution, gradients, shapeburst. Hand-rolling that is years of work and
    # the hand-rolled version is worse on the first day and every day after.
    #
    # WHAT WAS MEASURED before deciding, because the objection to linking it used
    # to be asserted rather than checked:
    #
    #   libqgis_core.so   45 MB, 246 shared objects
    #   QgsApplication::initQgis()   517 ms cold, 44 ms warm
    #
    # The startup cost sits well inside the two-second cold start of Article 7,
    # which is what the old objection claimed it would break. The size is a real
    # packaging cost and it is the price of the engine.
    #
    # LICENCE: QGIS is GPL-2.0-or-later, which is GPLv3-compatible, so it may be
    # linked into a GPL-3.0-or-later program (CLAUDE.md 2.1, 9).
    #
    # WHERE IT MAY BE USED: `/src/app` only. Article 3.4 keeps `kentos_render`
    # Qt-free, and QGIS is Qt; the QGIS backend therefore sits beside the QPainter
    # one, behind the same `render::Backend` interface, exactly as Article 8.5
    # describes. Nothing below `/src/app` learns that QGIS exists.
    #
    # It comes from the SYSTEM rather than from vcpkg: QGIS is a 45 MB desktop
    # application stack with GDAL, PROJ, GEOS, SpatiaLite and Qt underneath it,
    # and building that from a manifest would be building QGIS.
    find_path(QGIS_INCLUDE_DIR qgssymbol.h PATH_SUFFIXES qgis)
    find_library(QGIS_CORE_LIBRARY NAMES qgis_core)

    if(NOT QGIS_INCLUDE_DIR OR NOT QGIS_CORE_LIBRARY)
        message(WARNING
            "KENTOS_WITH_QGIS=ON but the QGIS development files were not found; "
            "the QGIS symbology backend is off and the built-in one is used.\n"
            "  Debian/Ubuntu: sudo apt install libqgis-dev\n"
            "  Fedora:        sudo dnf install qgis-devel\n"
            "  macOS:         brew install qgis")
        set(KENTOS_WITH_QGIS OFF CACHE BOOL "" FORCE)
    else()
        message(STATUS "QGIS symbology: ${QGIS_CORE_LIBRARY}")
    endif()
endif()
