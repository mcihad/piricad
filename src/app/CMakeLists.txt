# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_app — the Qt Widgets shell.
#
# Shell: Qt Widgets. Canvas: render::Backend (QRhiWidget target, QPainter in Phase 0).
# Docking: QDockWidget in Phase 0; Qt Advanced Docking System in Phase 1 (§6.3).

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

# Q_OBJECT headers must be listed so AUTOMOC processes them: they live under
# include/piricad/app/ rather than beside their .cpp, which is the layout the rest
# of the project uses.
qt_add_executable(piricad
    src/command_line.cpp
    src/controller.cpp
    src/flow_layout.cpp
    src/icons.cpp
    src/main.cpp
    src/main_window.cpp
    src/map_canvas.cpp
    src/panels.cpp
    src/theme.cpp
    src/toolbox.cpp

    include/piricad/app/command_line.hpp
    include/piricad/app/controller.hpp
    include/piricad/app/main_window.hpp
    include/piricad/app/map_canvas.hpp
    include/piricad/app/panels.hpp
    include/piricad/app/flow_layout.hpp
    include/piricad/app/toolbox.hpp
)

target_include_directories(piricad PRIVATE include)
target_compile_definitions(piricad PRIVATE PIRICAD_VERSION="${PROJECT_VERSION}")

target_link_libraries(piricad PRIVATE
    piricad_core
    piricad_command
    piricad_io
    piricad_render
    piricad_script
    piricad_flags
    Qt6::Core
    Qt6::Gui
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
