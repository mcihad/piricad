# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_render — scene building and the backend interface.
#
# The GPU canvas target is QRhiWidget (kentoscad.md §6.3). This target links no Qt,
# so the shader packs, the QRhi implementation of render::Backend and the widget
# that hosts them all live in src/app; what happens here is the define that tells
# the interface and the canvas which surface the build has.

add_library(kentos_render STATIC
    src/backend.cpp
    src/drawlist.cpp
    src/scene.cpp
    src/symbology.cpp
    src/view.cpp
)

target_include_directories(kentos_render PUBLIC include)
target_link_libraries(kentos_render
    PUBLIC  kentos_core
    PRIVATE kentos_flags
)

if(KENTOS_WITH_TEXT)
    # THE ATLAS IS QT-FREE, and that is what makes it testable: /tests links no Qt
    # (CLAUDE.md 3.4), so a suite that could not construct a backend can still
    # assert that `ğ` and the dotted `İ` come out of the shaper as their own
    # glyphs. What this target produces is PIXELS and NUMBERS; putting them on the
    # GPU is the backend's job, one layer up in /src/app.
    target_sources(kentos_render PRIVATE src/text_atlas.cpp)
    target_link_libraries(kentos_render PRIVATE
        Freetype::Freetype
        PkgConfig::KENTOS_HARFBUZZ
        msdfgen::msdfgen-core
        kentos_stb)
    target_compile_definitions(kentos_render PUBLIC KENTOS_HAVE_TEXT=1)
endif()

if(KENTOS_WITH_RHI)
    # The tool and header checks live in src/app, because that is where the shader
    # packs are baked and where the GPU implementation of render::Backend is
    # compiled — this target links no Qt at all (CLAUDE.md 3.4, Article 8.5).
    # Here the option only turns the define on for everything that links render,
    # which is what `src/backend.cpp` and `map_canvas.hpp` read.
    target_compile_definitions(kentos_render PUBLIC KENTOS_HAVE_RHI=1)
endif()

add_library(KentOSCad::render ALIAS kentos_render)
