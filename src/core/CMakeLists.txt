# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_core — the Qt-free foundation. Depends on nothing.
# Constitution Article 3: no reverse dependency, and no <Q...> include ever.

add_library(piricad_core STATIC
    src/attribute.cpp
    src/document.cpp
    src/entity_kind.cpp
    src/geometry.cpp
    src/json.cpp
    src/layer.cpp
    src/log.cpp
    src/settings.cpp
    src/spatial_index.cpp
    src/style.cpp
    src/style_rule.cpp
    src/text.cpp
)

target_include_directories(piricad_core PUBLIC include)
target_link_libraries(piricad_core PRIVATE piricad_flags)

set_target_properties(piricad_core PROPERTIES
    EXPORT_NAME core
    OUTPUT_NAME piricad_core
)

add_library(PiriCAD::core ALIAS piricad_core)
