# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every external dependency is behind an option that defaults to OFF and fails
# loudly when switched on without the dependency present. Phase 0 builds with
# nothing but Qt 6 (see CLAUDE.md, Article 8).

option(KENTOS_BUILD_APP     "Build the Qt application shell" ON)
option(KENTOS_BUILD_TESTS   "Build the test suite"           ON)
option(KENTOS_BUILD_BENCH   "Build the benchmark suite"      OFF)

# GDAL follows the PROJ precedent above, and for the reason CLAUDE.md Article 8.2
# gives: "defaulting ON once found". A machine that has GDAL must not silently
# build a KentOSCad that cannot open a DXF, because the failure mode is a user who
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
        pkg_check_modules(KENTOS_GDAL_PROBE QUIET gdal>=3.8)
    endif()
endif()
if(GDAL_FOUND OR KENTOS_GDAL_PROBE_FOUND)
    option(KENTOS_WITH_GDAL "Enable GDAL/OGR format support" ON)
else()
    option(KENTOS_WITH_GDAL "Enable GDAL/OGR format support" OFF)
endif()
# PROJ is the one dependency the product cannot fake: §12 opens with TUREF/TM3.
# Default to ON when it is installed, so a machine that has it never silently
# builds a KentOSCad that cannot transform a coordinate.
find_package(PROJ QUIET)
if(NOT PROJ_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(KENTOS_PROJ_PROBE QUIET proj)
    endif()
endif()
if(PROJ_FOUND OR KENTOS_PROJ_PROBE_FOUND)
    option(KENTOS_WITH_PROJ "Enable PROJ coordinate transformation" ON)
else()
    option(KENTOS_WITH_PROJ "Enable PROJ coordinate transformation" OFF)
endif()
option(KENTOS_WITH_GEOS     "Enable GEOS overlay operations"        OFF)
option(KENTOS_WITH_CGAL     "Enable CGAL exact arithmetic"          OFF)
option(KENTOS_WITH_LUA      "Enable the embedded Lua hot path"      OFF)
option(KENTOS_WITH_PYTHON   "Enable the optional Python module"     OFF)
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
# BE BUILT, and the QPainter backend stays reachable with -DKENTOS_WITH_RHI=OFF
# while the port settles (deleting it is the end of Phase 1).
#
# PROBED RATHER THAN ASSUMED, exactly as PROJ is above. A machine without Qt's
# private headers or without Qt Shader Tools still configures and still builds —
# it simply gets the QPainter canvas — while a machine that HAS them gets the
# fast one without being told to ask. Asking for ON and not having them is still
# a hard error with the package name in it (`src/app/CMakeLists.txt`), which is
# Article 8.2's rule: default ON once found, hard-fail when demanded and missing.
set(KENTOS_RHI_AVAILABLE FALSE)
find_package(Qt6 6.7 QUIET COMPONENTS ShaderTools GuiPrivate)
if(Qt6ShaderTools_FOUND AND TARGET Qt6::GuiPrivate)
    get_target_property(KENTOS_QT_PRIVATE_INC Qt6::GuiPrivate INTERFACE_INCLUDE_DIRECTORIES)
    # Generator expressions, not paths; only the value half holds a slash. The
    # long form of this and why `if(EXISTS)` cannot be used on the raw string is
    # in `src/app/CMakeLists.txt`, where the same look-up fails loudly.
    string(REGEX MATCHALL "/[^;>]+" KENTOS_QT_PRIVATE_DIRS "${KENTOS_QT_PRIVATE_INC}")
    foreach(_dir IN LISTS KENTOS_QT_PRIVATE_DIRS)
        if(EXISTS "${_dir}/rhi/qrhi.h" OR EXISTS "${_dir}/QtGui/rhi/qrhi.h")
            set(KENTOS_RHI_AVAILABLE TRUE)
        endif()
    endforeach()
endif()
option(KENTOS_WITH_RHI "Enable the QRhi GPU canvas backend" ${KENTOS_RHI_AVAILABLE})

# The text atlas needs FreeType and HarfBuzz from the system, and msdfgen and
# stb from pinned commits. The system half is probed; the pinned half is only
# defaulted ON when downloading is allowed, because a default that starts a
# network fetch on somebody's first configure is not a default.
set(KENTOS_TEXT_AVAILABLE FALSE)
find_package(Freetype 2.10 QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(KENTOS_HB_PROBE QUIET harfbuzz)
endif()
if(FREETYPE_FOUND AND KENTOS_HB_PROBE_FOUND AND NOT DEFINED KENTOS_FETCH_DEPENDENCIES)
    set(KENTOS_TEXT_AVAILABLE TRUE)
elseif(FREETYPE_FOUND AND KENTOS_HB_PROBE_FOUND AND KENTOS_FETCH_DEPENDENCIES)
    set(KENTOS_TEXT_AVAILABLE TRUE)
endif()
option(KENTOS_WITH_TEXT "Enable the msdfgen SDF text atlas" ${KENTOS_TEXT_AVAILABLE})
# DWG, read only, through LibreDWG (`.claude/io.md` R13).
#
# OFF BY DEFAULT. The cost of building it was measured on the reference machine —
# 41 s wall, 147 s CPU, once, plus a 262 MB clone — and that is not the reason.
# The reason is that LibreDWG compiles with a page of its own warnings on every
# configuration this project builds, and a build that is clean by rule (CLAUDE.md
# 5.14) cannot carry a dependency that is not; and the DWG reader is not part of
# the working set today (six entity types, `io.md` R14's coverage corpus not yet
# assembled). `-DKENTOS_WITH_DWG=ON` turns it on for the machine that wants it,
# and asking for ON without the source (an offline build,
# `KENTOS_FETCH_DEPENDENCIES=OFF`) is a hard error naming the fix, per Article 8.2.
#
# The ODA Drawings SDK is banned outright (io.md P1) and GDAL's own CAD driver is
# libopencad, a DIFFERENT implementation than the rulebook chose — the allow-list
# in `KentOSCadGdalDrivers.cmake` says why `.dwg` is not simply added there.
option(KENTOS_WITH_DWG      "Enable DWG reading through LibreDWG"   OFF)

# DXF through libdxfrw (io.md R13: DXF is first-class, read AND write). ON wherever
# the pinned source can be obtained: it is pure C++11 with no dependency of its
# own, so the only thing that can stop it is an offline build with
# KENTOS_FETCH_DEPENDENCIES=OFF — and asking for ON there is a hard error naming
# the fix, per Article 8.2, not a build that quietly reads DXF with the older
# GDAL path. GDAL's DXF driver flattens every curve before this program sees it;
# libdxfrw hands the CIRCLE, the ARC, the ELLIPSE, the SPLINE, the INSERT and
# the XDATA over as what they are, which is what "first-class" means.
if(NOT DEFINED KENTOS_FETCH_DEPENDENCIES OR KENTOS_FETCH_DEPENDENCIES)
    set(KENTOS_DXFRW_AVAILABLE ON)
else()
    set(KENTOS_DXFRW_AVAILABLE OFF)
endif()
option(KENTOS_WITH_DXFRW    "Read and write DXF through libdxfrw"   ${KENTOS_DXFRW_AVAILABLE})
option(KENTOS_WITH_TRACY    "Enable Tracy frame profiling"          OFF)

function(kentos_require_dependency option_name package_name hint)
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
