# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_domain_geodesy — TUREF/TM3, ED50, geoid. Depends on core; never the reverse.
#
# kentoscad.md §12 opens with this module. PROJ does the mathematics; the wrapper
# exists for the axis-order decision PROJ will not make for you.

add_library(kentos_domain_geodesy STATIC
    src/crs_catalog.cpp
    src/crs_service.cpp
    src/helmert.cpp
    src/fit_command.cpp
    src/stakeout_command.cpp
    src/transform.cpp
)

target_include_directories(kentos_domain_geodesy PUBLIC include)
# kentos_command is PUBLIC because crs_service.hpp exposes command::Bus in its
# interface. Article 3.2 allows domain -> core, command; the arrow that would be
# a build failure is the reverse one.
target_link_libraries(kentos_domain_geodesy
    PUBLIC  kentos_core kentos_command
    PRIVATE kentos_flags
)

if(KENTOS_WITH_PROJ)
    find_package(PROJ QUIET)
    if(NOT PROJ_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PROJ_PC IMPORTED_TARGET proj)
        endif()
    endif()

    if(TARGET PROJ::proj)
        target_link_libraries(kentos_domain_geodesy PRIVATE PROJ::proj)
    elseif(TARGET PkgConfig::PROJ_PC)
        target_link_libraries(kentos_domain_geodesy PRIVATE PkgConfig::PROJ_PC)
    else()
        message(FATAL_ERROR
            "KENTOS_WITH_PROJ=ON but PROJ was not found.\n"
            "  Debian/Ubuntu: sudo apt install libproj-dev\n"
            "  macOS:         brew install proj\n"
            "  vcpkg:         the 'proj' feature in /vcpkg.json\n"
            "  Or configure with -DKENTOS_WITH_PROJ=OFF; coordinate transformation\n"
            "  then reports an error rather than silently returning identity.")
    endif()

    target_compile_definitions(kentos_domain_geodesy PUBLIC KENTOS_HAVE_PROJ=1)
    message(STATUS "  PROJ ..... etkin (koordinat dönüşümü açık)")
else()
    message(STATUS "  PROJ ..... kapalı (koordinat dönüşümü hata döndürecek)")
endif()

add_library(KentOSCad::domain_geodesy ALIAS kentos_domain_geodesy)
