# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_io — format I/O. Depends on core + command + format libraries, nothing else.
#
# .claude/io.md R3: no Qt anywhere in this module, and the link list is
# piricad_core, piricad_command and the format libraries. Article 3.2 makes
# `io -> command` one-way; nothing in /src/command may include a piricad/io header,
# and scripts/ci-gate-layering.sh fails the build if one does.
#
# .claude/io.md R2/P2: gdal*.h, ogr*.h and cpl_*.h appear only in .cpp files here.
# They are never in the link INTERFACE either — PRIVATE below is what keeps a GDAL
# header off the include path of everything that links piricad_io.

add_library(piricad_io STATIC
    src/adopt.cpp
    src/block_view.cpp
    src/xref.cpp
    src/database.cpp
    src/diagnostics.cpp
    src/dwg.cpp
    src/dxf_common.cpp
    src/dxf_reader.cpp
    src/dxf_writer.cpp
    src/mapped_file.cpp
    src/ncz_format.cpp
    src/ncz_reader.cpp
    src/ncz_sheets.cpp
    src/ncz_symbols.cpp
    src/project_reader.cpp
    src/project_writer.cpp
    src/postgis.cpp
    src/pdf_encrypt.cpp
    src/print_profiles.cpp
    src/prj_sidecar.cpp
    src/source_info.cpp
    src/staging.cpp
    src/qgis_style.cpp
    src/service.cpp
    src/point_list.cpp
    src/vector.cpp
    src/dxf_multileader.hpp
)

target_include_directories(piricad_io
    PUBLIC  include
    PRIVATE src            # block_view.hpp / mapped_file.hpp are NOT public (R1)
)

target_link_libraries(piricad_io
    PUBLIC  piricad_core piricad_command
    PRIVATE piricad_flags
)

set_target_properties(piricad_io PROPERTIES
    EXPORT_NAME io
    OUTPUT_NAME piricad_io
)

add_library(PiriCAD::io ALIAS piricad_io)

# ------------------------------------------------------------------ GDAL ----
# io.md R22 and CLAUDE.md Article 8.2: behind PIRICAD_WITH_GDAL, hard-failing with
# an actionable message when ON but missing. When OFF, İÇEAKTAR and DIŞAAKTAR
# return an Error that names this option and the install command — they never
# silently succeed and never silently return an empty layer (data.md Enforcement).

include(PiriCADGdalDrivers)

target_compile_definitions(piricad_io PRIVATE
    PIRICAD_GDAL_DRIVERS="${PIRICAD_GDAL_DRIVERS_STRING}"
)

if(PIRICAD_WITH_GDAL)
    find_package(GDAL 3.8 CONFIG QUIET)
    if(NOT GDAL_FOUND)
        # Deprecated upstream, but still the only route on older distributions.
        find_package(GDAL 3.8 MODULE QUIET)
    endif()
    if(NOT GDAL_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PIRICAD_GDAL_PC IMPORTED_TARGET gdal>=3.8)
        endif()
    endif()

    if(TARGET GDAL::GDAL)
        target_link_libraries(piricad_io PRIVATE GDAL::GDAL)
    elseif(TARGET PkgConfig::PIRICAD_GDAL_PC)
        target_link_libraries(piricad_io PRIVATE PkgConfig::PIRICAD_GDAL_PC)
    else()
        message(FATAL_ERROR
            "PIRICAD_WITH_GDAL=ON but GDAL 3.8+ was not found.\n"
            "  Debian/Ubuntu: sudo apt install libgdal-dev\n"
            "  macOS:         brew install gdal\n"
            "  Windows:       vcpkg install gdal   (the 'gdal' feature in /vcpkg.json)\n"
            "  Or configure with -DPIRICAD_WITH_GDAL=OFF; İÇEAKTAR and DIŞAAKTAR then\n"
            "  report that the backend is off instead of silently doing nothing.")
    endif()

    target_compile_definitions(piricad_io PRIVATE PIRICAD_HAVE_GDAL=1)
    message(STATUS "  GDAL ..... etkin (izinli sürücüler: ${PIRICAD_GDAL_DRIVERS_STRING})")
else()
    message(STATUS "  GDAL ..... kapalı (İÇEAKTAR/DIŞAAKTAR hata döndürecek)")
endif()

if(PIRICAD_WITH_DWG)
    # PRIVATE, like every format library here: `dwg.h` is a twelve-thousand-line C
    # header that defines `restrict` and a hundred BITCODE_* macros, and it must
    # not reach the include path of everything that links piricad_io (io.md R2/P2).
    # The whole dependency lives inside src/dwg.cpp.
    target_link_libraries(piricad_io PRIVATE redwg)
    target_compile_definitions(piricad_io PRIVATE PIRICAD_HAVE_DWG=1)

    # LibreDWG generates its own config header into the build tree and keeps the
    # public ones in `include/`; neither is exported as a target property.
    target_include_directories(piricad_io PRIVATE
        "${libredwg_SOURCE_DIR}/include" "${libredwg_BINARY_DIR}")

    message(STATUS "  DWG ...... etkin (LibreDWG, salt okunur)")
else()
    message(STATUS "  DWG ...... kapalı (-DPIRICAD_WITH_DWG=ON ile açılır)")
endif()

if(PIRICAD_WITH_DXFRW)
    # PRIVATE, like every format library here (io.md R2/P2): the DRW_* headers
    # reach dxf_reader.cpp, dxf_writer.cpp and dxf_common.cpp, and nothing else.
    # libdxfrw's target exports no build-tree include directory (only the install
    # interface), so its source directory is named here.
    target_link_libraries(piricad_io PRIVATE dxfrw)
    target_compile_definitions(piricad_io PRIVATE PIRICAD_HAVE_DXFRW=1)
    target_include_directories(piricad_io SYSTEM PRIVATE "${libdxfrw_SOURCE_DIR}/src")
    message(STATUS "  DXF ...... etkin (libdxfrw, okuma ve yazma)")
else()
    message(STATUS "  DXF ...... libdxfrw kapalı; DXF GDAL sürücüsüyle okunur/yazılır")
endif()

if(PIRICAD_WITH_POSTGIS)
    # PRIVATE, like every format library here: a pqxx header must not reach the
    # include path of everything that links piricad_io (io.md R2/P2). The whole
    # dependency lives inside src/postgis.cpp, behind the pimpl in its header.
    target_link_libraries(piricad_io PRIVATE libpqxx::pqxx)
    target_compile_definitions(piricad_io PRIVATE PIRICAD_HAVE_POSTGIS=1)
endif()

if(PIRICAD_WITH_QPDF)
    # PRIVATE, like every library here: a qpdf header stays inside
    # src/pdf_encrypt.cpp; the facade in io/pdf_encrypt.hpp is what the product
    # sees (io.md R2/P2, CLAUDE.md Article 9).
    target_link_libraries(piricad_io PRIVATE qpdf::libqpdf)
    target_compile_definitions(piricad_io PRIVATE PIRICAD_HAVE_QPDF=1)
endif()
