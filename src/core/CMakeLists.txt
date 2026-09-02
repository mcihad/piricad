# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_core — the Qt-free foundation. Depends on nothing.
# Constitution Article 3: no reverse dependency, and no <Q...> include ever.

add_library(piricad_core STATIC
    src/attribute.cpp
    src/document.cpp
    src/arc.cpp
    src/circle.cpp
    src/entity_kind.cpp
    src/transform.cpp
    src/geometry.cpp
    src/offset.cpp
    src/json.cpp
    src/layer.cpp
    src/pick.cpp
    src/settings.cpp
    src/snap.cpp
    src/spatial_index.cpp
    src/style.cpp
    src/dash_store.cpp
    src/image_store.cpp
    src/style_library.cpp
    src/style_rule.cpp
    src/text.cpp
    src/text_store.cpp
)

target_include_directories(piricad_core PUBLIC include)
target_link_libraries(piricad_core PRIVATE piricad_flags)

# PRIVATE, deliberately: the JSON library is an implementation detail of
# core/json.cpp and must not reach anything that links piricad_core. The facade
# in core/json.hpp is what the rest of the product sees (Article 9: keep a
# dependency's headers inside the owning module's .cpp files).
if(PIRICAD_WITH_JSON)
    target_link_libraries(piricad_core PRIVATE nlohmann_json::nlohmann_json)
endif()

# PRIVATE for the same reason: Clipper2 lives inside core/offset.cpp and the
# facade in core/offset.hpp is what the product sees. Nothing outside that one
# translation unit learns that Clipper2 exists.
if(PIRICAD_WITH_CLIPPER2)
    target_link_libraries(piricad_core PRIVATE Clipper2)
    target_compile_definitions(piricad_core PRIVATE PIRICAD_HAVE_CLIPPER2=1)
endif()

set_target_properties(piricad_core PROPERTIES
    EXPORT_NAME core
    OUTPUT_NAME piricad_core
)

add_library(PiriCAD::core ALIAS piricad_core)
