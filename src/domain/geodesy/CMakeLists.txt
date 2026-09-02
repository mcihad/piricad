# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_domain_geodesy — TUREF/TM3, ED50, geoid. Depends on core; never the reverse.
#
# piricad.md §12 opens with this module. PROJ does the mathematics; the wrapper
# exists for the axis-order decision PROJ will not make for you.

add_library(piricad_domain_geodesy STATIC
    src/crs_catalog.cpp
    src/crs_service.cpp
    src/helmert.cpp
    src/fit_command.cpp
    src/transform.cpp
)

target_include_directories(piricad_domain_geodesy PUBLIC include)
# piricad_command is PUBLIC because crs_service.hpp exposes command::Bus in its
# interface. Article 3.2 allows domain -> core, command; the arrow that would be
# a build failure is the reverse one.
target_link_libraries(piricad_domain_geodesy
    PUBLIC  piricad_core piricad_command
    PRIVATE piricad_flags
)

if(PIRICAD_WITH_PROJ)
    find_package(PROJ QUIET)
    if(NOT PROJ_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PROJ_PC IMPORTED_TARGET proj)
        endif()
    endif()

    if(TARGET PROJ::proj)
        target_link_libraries(piricad_domain_geodesy PRIVATE PROJ::proj)
    elseif(TARGET PkgConfig::PROJ_PC)
        target_link_libraries(piricad_domain_geodesy PRIVATE PkgConfig::PROJ_PC)
    else()
        message(FATAL_ERROR
            "PIRICAD_WITH_PROJ=ON but PROJ was not found.\n"
            "  Debian/Ubuntu: sudo apt install libproj-dev\n"
            "  macOS:         brew install proj\n"
            "  vcpkg:         the 'proj' feature in /vcpkg.json\n"
            "  Or configure with -DPIRICAD_WITH_PROJ=OFF; coordinate transformation\n"
            "  then reports an error rather than silently returning identity.")
    endif()

    target_compile_definitions(piricad_domain_geodesy PUBLIC PIRICAD_HAVE_PROJ=1)
    message(STATUS "  PROJ ..... etkin (koordinat dönüşümü açık)")
else()
    message(STATUS "  PROJ ..... kapalı (koordinat dönüşümü hata döndürecek)")
endif()

add_library(PiriCAD::domain_geodesy ALIAS piricad_domain_geodesy)
