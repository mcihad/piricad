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
option(KENTOS_WITH_RHI      "Enable the QRhi GPU canvas backend"    OFF)
option(KENTOS_WITH_TEXT     "Enable the msdfgen SDF text atlas"     OFF)
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
