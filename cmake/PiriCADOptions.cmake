# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every external dependency is behind an option that defaults to OFF and fails
# loudly when switched on without the dependency present. Phase 0 builds with
# nothing but Qt 6 (see CLAUDE.md, Article 8).

option(PIRICAD_BUILD_APP     "Build the Qt application shell" ON)
option(PIRICAD_BUILD_TESTS   "Build the test suite"           ON)
option(PIRICAD_BUILD_BENCH   "Build the benchmark suite"      OFF)

option(PIRICAD_WITH_GDAL     "Enable GDAL/OGR format support"        OFF)
option(PIRICAD_WITH_PROJ     "Enable PROJ coordinate transformation" OFF)
option(PIRICAD_WITH_GEOS     "Enable GEOS overlay operations"        OFF)
option(PIRICAD_WITH_CGAL     "Enable CGAL exact arithmetic"          OFF)
option(PIRICAD_WITH_LUA      "Enable the embedded Lua hot path"      OFF)
option(PIRICAD_WITH_PYTHON   "Enable the optional Python module"     OFF)
option(PIRICAD_WITH_RHI      "Enable the QRhi GPU canvas backend"    OFF)
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
