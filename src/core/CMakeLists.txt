# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_core — the Qt-free foundation. Depends on nothing.
# Constitution Article 3: no reverse dependency, and no <Q...> include ever.

add_library(piricad_core STATIC
    src/document.cpp
    src/json.cpp
    src/log.cpp
    src/text.cpp
)

target_include_directories(piricad_core PUBLIC include)
target_link_libraries(piricad_core PRIVATE piricad_flags)

set_target_properties(piricad_core PROPERTIES
    EXPORT_NAME core
    OUTPUT_NAME piricad_core
)

add_library(PiriCAD::core ALIAS piricad_core)
