# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_io — format I/O. Depends on core + command + format libraries, nothing else.
#
# .claude/io.md R3: no Qt anywhere in this module, and the link list is
# kentos_core, kentos_command and the format libraries. Article 3.2 makes
# `io -> command` one-way; nothing in /src/command may include a piricad/io header,
# and scripts/ci-gate-layering.sh fails the build if one does.
#
# .claude/io.md R2/P2: gdal*.h, ogr*.h and cpl_*.h appear only in .cpp files here.
# They are never in the link INTERFACE either — PRIVATE below is what keeps a GDAL
# header off the include path of everything that links kentos_io.

add_library(kentos_io STATIC
    src/adopt.cpp
    src/block_view.cpp
    src/database.cpp
    src/dwg.cpp
    src/mapped_file.cpp
    src/project_reader.cpp
    src/project_writer.cpp
    src/postgis.cpp
    src/qgis_style.cpp
    src/service.cpp
    src/point_list.cpp
    src/vector.cpp
)

target_include_directories(kentos_io
    PUBLIC  include
    PRIVATE src            # block_view.hpp / mapped_file.hpp are NOT public (R1)
)

target_link_libraries(kentos_io
    PUBLIC  kentos_core kentos_command
    PRIVATE kentos_flags
)

set_target_properties(kentos_io PROPERTIES
    EXPORT_NAME io
    OUTPUT_NAME kentos_io
)

add_library(KentOSCad::io ALIAS kentos_io)

# ------------------------------------------------------------------ GDAL ----
# io.md R22 and CLAUDE.md Article 8.2: behind KENTOS_WITH_GDAL, hard-failing with
# an actionable message when ON but missing. When OFF, İÇEAKTAR and DIŞAAKTAR
# return an Error that names this option and the install command — they never
# silently succeed and never silently return an empty layer (data.md Enforcement).

include(KentOSCadGdalDrivers)

target_compile_definitions(kentos_io PRIVATE
    KENTOS_GDAL_DRIVERS="${KENTOS_GDAL_DRIVERS_STRING}"
)

if(KENTOS_WITH_GDAL)
    find_package(GDAL 3.8 CONFIG QUIET)
    if(NOT GDAL_FOUND)
        # Deprecated upstream, but still the only route on older distributions.
        find_package(GDAL 3.8 MODULE QUIET)
    endif()
    if(NOT GDAL_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(KENTOS_GDAL_PC IMPORTED_TARGET gdal>=3.8)
        endif()
    endif()

    if(TARGET GDAL::GDAL)
        target_link_libraries(kentos_io PRIVATE GDAL::GDAL)
    elseif(TARGET PkgConfig::KENTOS_GDAL_PC)
        target_link_libraries(kentos_io PRIVATE PkgConfig::KENTOS_GDAL_PC)
    else()
        message(FATAL_ERROR
            "KENTOS_WITH_GDAL=ON but GDAL 3.8+ was not found.\n"
            "  Debian/Ubuntu: sudo apt install libgdal-dev\n"
            "  macOS:         brew install gdal\n"
            "  Windows:       vcpkg install gdal   (the 'gdal' feature in /vcpkg.json)\n"
            "  Or configure with -DKENTOS_WITH_GDAL=OFF; İÇEAKTAR and DIŞAAKTAR then\n"
            "  report that the backend is off instead of silently doing nothing.")
    endif()

    target_compile_definitions(kentos_io PRIVATE KENTOS_HAVE_GDAL=1)
    message(STATUS "  GDAL ..... etkin (izinli sürücüler: ${KENTOS_GDAL_DRIVERS_STRING})")
else()
    message(STATUS "  GDAL ..... kapalı (İÇEAKTAR/DIŞAAKTAR hata döndürecek)")
endif()

if(KENTOS_WITH_DWG)
    # PRIVATE, like every format library here: `dwg.h` is a twelve-thousand-line C
    # header that defines `restrict` and a hundred BITCODE_* macros, and it must
    # not reach the include path of everything that links kentos_io (io.md R2/P2).
    # The whole dependency lives inside src/dwg.cpp.
    target_link_libraries(kentos_io PRIVATE redwg)
    target_compile_definitions(kentos_io PRIVATE KENTOS_HAVE_DWG=1)

    # LibreDWG generates its own config header into the build tree and keeps the
    # public ones in `include/`; neither is exported as a target property.
    target_include_directories(kentos_io PRIVATE
        "${libredwg_SOURCE_DIR}/include" "${libredwg_BINARY_DIR}")

    message(STATUS "  DWG ...... etkin (LibreDWG, salt okunur)")
else()
    message(STATUS "  DWG ...... kapalı (-DKENTOS_WITH_DWG=ON ile açılır)")
endif()

if(KENTOS_WITH_POSTGIS)
    # PRIVATE, like every format library here: a pqxx header must not reach the
    # include path of everything that links kentos_io (io.md R2/P2). The whole
    # dependency lives inside src/postgis.cpp, behind the pimpl in its header.
    target_link_libraries(kentos_io PRIVATE libpqxx::pqxx)
    target_compile_definitions(kentos_io PRIVATE KENTOS_HAVE_POSTGIS=1)
endif()
