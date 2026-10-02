# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every external dependency is behind an option that defaults to OFF and fails
# loudly when switched on without the dependency present. Phase 0 builds with
# nothing but Qt 6 (see CLAUDE.md, Article 8).

option(PIRICAD_BUILD_APP     "Build the Qt application shell" ON)
option(PIRICAD_BUILD_TESTS   "Build the test suite"           ON)
option(PIRICAD_BUILD_BENCH   "Build the benchmark suite"      OFF)

# GDAL follows the PROJ precedent above, and for the reason CLAUDE.md Article 8.2
# gives: "defaulting ON once found". A machine that has GDAL must not silently
# build a PiriCAD that cannot open a DXF, because the failure mode is a user who
# thinks the format is unsupported rather than uninstalled. Absent, it stays OFF
# and İÇEAKTAR/DIŞAAKTAR say exactly which package would change that.
find_package(GDAL 3.8 CONFIG QUIET)
if(NOT GDAL_FOUND)
    # Older distributions ship no CMake package config; the module is deprecated
    # upstream but is still the only way to find those.
    find_package(GDAL 3.8 MODULE QUIET)
endif()
if(NOT GDAL_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIRICAD_GDAL_PROBE QUIET gdal>=3.8)
    endif()
endif()
if(GDAL_FOUND OR PIRICAD_GDAL_PROBE_FOUND)
    option(PIRICAD_WITH_GDAL "Enable GDAL/OGR format support" ON)
else()
    option(PIRICAD_WITH_GDAL "Enable GDAL/OGR format support" OFF)
endif()
# PROJ is the one dependency the product cannot fake: §12 opens with TUREF/TM3.
# Default to ON when it is installed, so a machine that has it never silently
# builds a PiriCAD that cannot transform a coordinate.
find_package(PROJ QUIET)
if(NOT PROJ_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIRICAD_PROJ_PROBE QUIET proj)
    endif()
endif()
if(PROJ_FOUND OR PIRICAD_PROJ_PROBE_FOUND)
    option(PIRICAD_WITH_PROJ "Enable PROJ coordinate transformation" ON)
else()
    option(PIRICAD_WITH_PROJ "Enable PROJ coordinate transformation" OFF)
endif()
option(PIRICAD_WITH_GEOS     "Enable GEOS overlay operations"        OFF)
# CGAL follows GDAL and PROJ, for the reason Article 8.2 gives: "defaulting ON
# once found". Its arrangement is what finds a closed region from a click and
# what turns a network of boundary lines into parcels (core/planar.hpp, TODOS
# C-09); a machine that has CGAL must not silently build a PiriCAD whose
# SINIR command can only say it was built without it. Header-only, so the probe
# is the package config alone. Absent, it stays OFF and SINIR names the package.
set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
find_package(CGAL 5.6 CONFIG QUIET)
if(CGAL_FOUND)
    option(PIRICAD_WITH_CGAL "Enable CGAL exact arithmetic (planar arrangements)" ON)
else()
    option(PIRICAD_WITH_CGAL "Enable CGAL exact arithmetic (planar arrangements)" OFF)
endif()
# ---- the geometry kernel (CLAUDE.md 2.11) ------------------------------------
#
# OPENCASCADE IS THE KERNEL, by the maintainer's decision of 28 September 2026:
# every geometric operation OCCT does better than the code this program had —
# a boolean that keeps an arc, an offset with true arcs, fillets between
# curves, the crossings of ellipses and splines — goes through it. The document
# keeps its millimetres (Article 2.4) and every result is rounded to the
# millimetre on the way back, so the three platforms must still agree (§7.3).
#
# ON WHEREVER IT IS FOUND, and the sanctioned presets ask for it outright
# (`CMakePresets.json`), so a preset build on a machine without it STOPS with
# the package names (`cmake/PiriCADDependencies.cmake`) rather than producing
# a program whose kernel-backed operations only say the build has none. An
# ad-hoc configure without it still configures, OFF, the way the canvas does.
#
# NO VERSION IN THE CALL: OCCT's package config accepts only its own patch
# version (`SamePatchVersion`), so asking for 7.6 turns 7.9.3 away. The floor is
# checked against `OpenCASCADE_VERSION` instead (CLAUDE.md 5.12 asks for the
# minimum, not for the syntax).
find_package(OpenCASCADE CONFIG QUIET)
if(OpenCASCADE_FOUND AND NOT OpenCASCADE_VERSION VERSION_LESS 7.6)
    option(PIRICAD_WITH_OCCT "Enable the OpenCASCADE geometry kernel" ON)
else()
    option(PIRICAD_WITH_OCCT "Enable the OpenCASCADE geometry kernel" OFF)
endif()
option(PIRICAD_WITH_PYTHON   "Enable the embedded Python script host" OFF)
# ---- the two halves of the GPU canvas: ON once their toolchain is found ------
#
# CLAUDE.md Article 8.1's removal condition, met. The 5M-polygon question it was
# written to answer has been measured — 40 000 parcels, same scene, same machine,
# median of 20 frames, backend share only:
#
#     QRhi (GPU)          1.10 ms
#     built-in QPainter  22.41 ms
#     QGIS               72.63 ms
#
# against a §10.1 budget of 16 ms. So the GPU path is the default WHEREVER IT CAN
# BE BUILT, and the QPainter backend stays reachable with -DPIRICAD_WITH_RHI=OFF
# while the port settles (deleting it is the end of Phase 1).
#
# PROBED RATHER THAN ASSUMED, exactly as PROJ is above. A machine without Qt's
# private headers or without Qt Shader Tools still configures and still builds —
# it simply gets the QPainter canvas — while a machine that HAS them gets the
# fast one without being told to ask. Asking for ON and not having them is still
# a hard error with the package name in it (`src/app/CMakeLists.txt`), which is
# Article 8.2's rule: default ON once found, hard-fail when demanded and missing.
set(PIRICAD_RHI_AVAILABLE FALSE)
find_package(Qt6 6.7 QUIET COMPONENTS ShaderTools GuiPrivate)
if(Qt6ShaderTools_FOUND AND TARGET Qt6::GuiPrivate)
    get_target_property(PIRICAD_QT_PRIVATE_INC Qt6::GuiPrivate INTERFACE_INCLUDE_DIRECTORIES)
    # Generator expressions, not paths; only the value half holds a slash. The
    # long form of this and why `if(EXISTS)` cannot be used on the raw string is
    # in `src/app/CMakeLists.txt`, where the same look-up fails loudly.
    string(REGEX MATCHALL "/[^;>]+" PIRICAD_QT_PRIVATE_DIRS "${PIRICAD_QT_PRIVATE_INC}")
    foreach(_dir IN LISTS PIRICAD_QT_PRIVATE_DIRS)
        if(EXISTS "${_dir}/rhi/qrhi.h" OR EXISTS "${_dir}/QtGui/rhi/qrhi.h")
            set(PIRICAD_RHI_AVAILABLE TRUE)
        endif()
    endforeach()
endif()
option(PIRICAD_WITH_RHI "Enable the QRhi GPU canvas backend" ${PIRICAD_RHI_AVAILABLE})

# ---- the embedded agent server (CLAUDE.md 2.10, .claude/ai.md R28) ------------
#
# PROBED, like the canvas above and PROJ before it: a machine with Qt HttpServer
# gets the MCP listener without being told to ask, and a machine without it still
# configures, still builds and simply cannot serve agents — `MCPSUNUCU` says so
# rather than opening nothing. Asking for ON without the module is a hard error
# naming the package (`src/app/CMakeLists.txt`).
#
# 6.8 IS THE FLOOR. `QHttpServer` existed earlier, but `QHttpServerResponder`'s
# chunked writing — which an SSE response needs — and `QAbstractHttpServer::bind`
# are what this code is written against.
set(PIRICAD_MCP_AVAILABLE FALSE)
find_package(Qt6 6.8 QUIET COMPONENTS HttpServer)
if(Qt6HttpServer_FOUND)
    set(PIRICAD_MCP_AVAILABLE TRUE)
endif()

option(PIRICAD_WITH_MCP "Embed the MCP server so AI agents can drive the program"
       ${PIRICAD_MCP_AVAILABLE})

# ---- the system key store (CLAUDE.md 5.21, .claude/ai.md P11) -----------------
#
# WHERE AN API KEY LIVES. A model provider profile carries the NAME of a
# credential entry and never the credential (`ai/provider.hpp`), so something has
# to hold the secret — and the only right answer is the store the operating system
# already has: the macOS keychain, the Secret Service on Linux, the Windows
# credential store. All three are SYSTEM APIs, so nothing here is added to
# `/vcpkg.json` and nothing to `/NOTICE`.
#
# PROBED, like the MCP listener above: macOS and Windows always have theirs, and
# a Linux machine has one when libsecret-1 is installed. A machine without it
# still configures and still builds — the store then reads an ENVIRONMENT
# VARIABLE named by the profile and refuses to write, saying so (see
# `app/secret_store.hpp`), which is the same shape the PostGIS path has with
# `~/.pgpass`. Asking for ON without libsecret is a hard error naming the package
# (`src/app/CMakeLists.txt`).
set(PIRICAD_KEYCHAIN_AVAILABLE FALSE)
if(APPLE OR WIN32)
    set(PIRICAD_KEYCHAIN_AVAILABLE TRUE)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIRICAD_SECRET_PROBE QUIET libsecret-1)
        if(PIRICAD_SECRET_PROBE_FOUND)
            set(PIRICAD_KEYCHAIN_AVAILABLE TRUE)
        endif()
    endif()
endif()

option(PIRICAD_WITH_KEYCHAIN "Hold API keys in the operating system's key store"
       ${PIRICAD_KEYCHAIN_AVAILABLE})

# The text atlas needs FreeType and HarfBuzz from the system, and msdfgen and
# stb from pinned commits. The system half is probed; the pinned half is only
# defaulted ON when downloading is allowed, because a default that starts a
# network fetch on somebody's first configure is not a default.
set(PIRICAD_TEXT_AVAILABLE FALSE)
find_package(Freetype 2.10 QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PIRICAD_HB_PROBE QUIET harfbuzz)
endif()
if(FREETYPE_FOUND AND PIRICAD_HB_PROBE_FOUND AND NOT DEFINED PIRICAD_FETCH_DEPENDENCIES)
    set(PIRICAD_TEXT_AVAILABLE TRUE)
elseif(FREETYPE_FOUND AND PIRICAD_HB_PROBE_FOUND AND PIRICAD_FETCH_DEPENDENCIES)
    set(PIRICAD_TEXT_AVAILABLE TRUE)
endif()
option(PIRICAD_WITH_TEXT "Enable the msdfgen SDF text atlas" ${PIRICAD_TEXT_AVAILABLE})
# DWG, read only, through LibreDWG (`.claude/io.md` R13).
#
# OFF FOR AN AD-HOC CONFIGURE, ON IN EVERY SANCTIONED PRESET (`dev`, `debug`,
# `release`, `asan` — the `canvas` preset in CMakePresets.json). This option was
# OFF everywhere until the maintainer asked for DWG to be on (2 October 2026),
# for two reasons that no longer decide it: LibreDWG compiles with a page of its
# own warnings on every configuration this project builds (a build that is clean
# by rule, CLAUDE.md 5.14, was thought unable to carry it — they are upstream's
# warnings in upstream's files, which `DISABLE_WERROR` keeps from failing OUR
# build), and the reader was not part of the working set. The maintainer's
# decision supersedes both.
#
# The cost is paid once, on the first configure: 41 s wall and 147 s CPU on the
# reference machine, plus a 262 MB clone. `headless` stays OFF — it builds no
# application and exists to prove the Qt-free targets stand alone. Asking for ON
# without the source (an offline build, `PIRICAD_FETCH_DEPENDENCIES=OFF`) is a
# hard error naming the fix, per Article 8.2.
#
# The ODA Drawings SDK is banned outright (io.md P1) and GDAL's own CAD driver is
# libopencad, a DIFFERENT implementation than the rulebook chose — the allow-list
# in `PiriCADGdalDrivers.cmake` says why `.dwg` is not simply added there.
option(PIRICAD_WITH_DWG      "Enable DWG reading through LibreDWG"   OFF)

# DXF through libdxfrw (io.md R13: DXF is first-class, read AND write). ON wherever
# the pinned source can be obtained: it is pure C++11 with no dependency of its
# own, so the only thing that can stop it is an offline build with
# PIRICAD_FETCH_DEPENDENCIES=OFF — and asking for ON there is a hard error naming
# the fix, per Article 8.2, not a build that quietly reads DXF with the older
# GDAL path. GDAL's DXF driver flattens every curve before this program sees it;
# libdxfrw hands the CIRCLE, the ARC, the ELLIPSE, the SPLINE, the INSERT and
# the XDATA over as what they are, which is what "first-class" means.
if(NOT DEFINED PIRICAD_FETCH_DEPENDENCIES OR PIRICAD_FETCH_DEPENDENCIES)
    set(PIRICAD_DXFRW_AVAILABLE ON)
else()
    set(PIRICAD_DXFRW_AVAILABLE OFF)
endif()
option(PIRICAD_WITH_DXFRW    "Read and write DXF through libdxfrw"   ${PIRICAD_DXFRW_AVAILABLE})
option(PIRICAD_WITH_TRACY    "Enable Tracy frame profiling"          OFF)

function(piricad_require_dependency option_name package_name hint)
    if(${option_name})
        find_package(${package_name} QUIET)
        if(NOT ${package_name}_FOUND)
            message(FATAL_ERROR
                "${option_name}=ON but ${package_name} was not found.\n"
                "  ${hint}\n"
                "  Or configure with -D${option_name}=OFF to build without it.")
        endif()
    endif()
endfunction()
