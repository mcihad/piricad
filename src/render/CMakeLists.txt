# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_render — scene building and the backend interface.
#
# The GPU canvas target is QRhiWidget (piricad.md §6.3). Building it needs `qsb`
# from qt6-shadertools to bake shader packs; when that is unavailable the QPainter
# backend in src/app takes over behind the same render::Backend interface.

add_library(piricad_render STATIC
    src/backend.cpp
    src/drawlist.cpp
    src/scene.cpp
    src/view.cpp
)

target_include_directories(piricad_render PUBLIC include)
target_link_libraries(piricad_render
    PUBLIC  piricad_core
    PRIVATE piricad_flags
)

if(PIRICAD_WITH_RHI)
    find_program(PIRICAD_QSB NAMES qsb qsb6)
    if(NOT PIRICAD_QSB)
        message(FATAL_ERROR
            "PIRICAD_WITH_RHI=ON but `qsb` was not found. Install qt6-shadertools "
            "(Debian/Ubuntu: qt6-shadertools-dev-tools) or configure with "
            "-DPIRICAD_WITH_RHI=OFF to use the QPainter backend.")
    endif()
    target_compile_definitions(piricad_render PUBLIC PIRICAD_HAVE_RHI=1)
endif()

add_library(PiriCAD::render ALIAS piricad_render)
