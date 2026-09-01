# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_render — scene building and the backend interface.
#
# The GPU canvas target is QRhiWidget (piricad.md §6.3). This target links no Qt,
# so the shader packs, the QRhi implementation of render::Backend and the widget
# that hosts them all live in src/app; what happens here is the define that tells
# the interface and the canvas which surface the build has.

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
    # The tool and header checks live in src/app, because that is where the shader
    # packs are baked and where the GPU implementation of render::Backend is
    # compiled — this target links no Qt at all (CLAUDE.md 3.4, Article 8.5).
    # Here the option only turns the define on for everything that links render,
    # which is what `src/backend.cpp` and `map_canvas.hpp` read.
    target_compile_definitions(piricad_render PUBLIC PIRICAD_HAVE_RHI=1)
endif()

add_library(PiriCAD::render ALIAS piricad_render)
