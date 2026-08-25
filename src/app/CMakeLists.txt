# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_app — the Qt Widgets shell.
#
# Shell: Qt Widgets. Canvas: render::Backend (QRhiWidget target, QPainter in Phase 0).
# Docking: QDockWidget in Phase 0; Qt Advanced Docking System in Phase 1 (§6.3).

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg Xml)

qt_standard_project_setup()

# Q_OBJECT headers must be listed so AUTOMOC processes them: they live under
# include/piricad/app/ rather than beside their .cpp, which is the layout the rest
# of the project uses.
qt_add_executable(piricad
    src/attribute_panel.cpp
    src/attribute_table.cpp
    src/command_line.cpp
    src/command_palette.cpp
    src/controller.cpp
    src/data_root.cpp
    src/dialog_chrome.cpp
    src/database_dialog.cpp
    src/flow_layout.cpp
    src/icons.cpp
    src/main.cpp
    src/main_window.cpp
    src/map_canvas.cpp
    src/painter_backend.cpp
    src/qgis_backend.cpp
    src/style_designer.cpp
    src/symbol_preview.cpp
    src/panels.cpp
    src/settings_dialog.cpp
    src/shell_chrome.cpp
    src/theme.cpp
    src/title_bar.cpp
    src/tokens.cpp
    src/toolbox.cpp

    include/piricad/app/command_line.hpp
    include/piricad/app/attribute_panel.hpp
    include/piricad/app/attribute_table.hpp
    include/piricad/app/command_palette.hpp
    include/piricad/app/controller.hpp
    include/piricad/app/data_root.hpp
    include/piricad/app/dialog_chrome.hpp
    include/piricad/app/database_dialog.hpp
    include/piricad/app/main_window.hpp
    include/piricad/app/map_canvas.hpp
    include/piricad/app/panels.hpp
    include/piricad/app/qgis_backend.hpp
    include/piricad/app/settings_dialog.hpp
    include/piricad/app/shell_chrome.hpp
    include/piricad/app/style_designer.hpp
    include/piricad/app/flow_layout.hpp
    include/piricad/app/title_bar.hpp
    include/piricad/app/toolbox.hpp
)

target_include_directories(piricad PRIVATE include)
target_compile_definitions(piricad PRIVATE
    PIRICAD_VERSION="${PROJECT_VERSION}"
    PIRICAD_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

target_link_libraries(piricad PRIVATE
    piricad_core
    piricad_command
    piricad_io
    piricad_domain_geodesy
    piricad_render
    piricad_script
    piricad_flags
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Widgets
)

set_target_properties(piricad PROPERTIES
    WIN32_EXECUTABLE FALSE       # keep the console: the command line is the product
    MACOSX_BUNDLE    TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS piricad
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

# THE CATALOGUES SHIP WITH THE PROGRAM. Installing the binary alone leaves every
# machine but the build host with an empty gösterim shelf — MPYY's symbology, the
# TM3 dilim table and the corpus are what make this a Turkish planning program
# (CLAUDE.md 3.5, §12), and they are data the application reads at runtime rather
# than anything compiled in.
#
# `data_root()` looks here first among the installed layouts; see data_root.hpp.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION share/piricad/data
    PATTERN "*.md" EXCLUDE
)

if(PIRICAD_WITH_QGIS)
    # QGIS lives in /src/app alone (Article 3.4, 8.5): nothing below the shell
    # learns that it exists, and the canvas reaches it through render::Backend.
    # SYSTEM, and this is not a suppression. CLAUDE.md 5.14 forbids silencing a
    # warning; what this does is tell the compiler which headers are OURS. QGIS's
    # own headers carry conversion and shadow warnings that no edit of this
    # repository can fix, and a wall of them is how a real finding in our own code
    # gets scrolled past.
    target_include_directories(piricad SYSTEM PRIVATE ${QGIS_INCLUDE_DIR})
    target_link_libraries(piricad PRIVATE ${QGIS_CORE_LIBRARY} Qt6::Xml)
    target_compile_definitions(piricad PRIVATE PIRICAD_HAVE_QGIS=1)
endif()
