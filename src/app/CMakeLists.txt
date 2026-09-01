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

if(PIRICAD_WITH_RHI)
    # THE GPU CANVAS (piricad.md §6.3, CLAUDE.md Article 8.1). Three things must
    # be present and each fails with the package that supplies it, because "QRhi
    # is missing" is not an actionable message on any of the three platforms.
    #
    # 1. Qt Shader Tools, for `qsb`. Qt does not put its tools on PATH anywhere,
    #    which is why `find_program(qsb)` reported it missing on a machine that
    #    had it; the CMake package carries the target and the location both.
    # 2. Qt Gui's PRIVATE headers, because QRhi lives there. Debian splits them
    #    into qt6-base-private-dev, and nothing in the public headers hints at it.
    # 3. Qt 6.7 or newer, which is where QRhiWidget landed.
    find_package(Qt6 6.7 QUIET COMPONENTS ShaderTools GuiPrivate)

    if(NOT Qt6ShaderTools_FOUND)
        message(FATAL_ERROR
            "PIRICAD_WITH_RHI=ON but Qt Shader Tools was not found, so the shader "
            "packs cannot be baked.\n"
            "  Debian/Ubuntu: sudo apt install qt6-shadertools-dev\n"
            "  vcpkg:         qtshadertools\n"
            "  Or configure with -DPIRICAD_WITH_RHI=OFF to use the QPainter backend.")
    endif()

    # THE TARGET IS NOT THE HEADERS. `Qt6::GuiPrivate` comes from qt6-base-dev's
    # CMake files, so `find_package` succeeds on a machine that has no private
    # headers at all — and the failure then arrives as CMake complaining that an
    # imported target "includes non-existent path .../QtGui/6.10.2", which names
    # neither the option that asked for it nor the package that supplies it. Look
    # for the header itself.
    set(PIRICAD_QRHI_HEADER_FOUND FALSE)
    if(TARGET Qt6::GuiPrivate)
        get_target_property(PIRICAD_QT_GUI_PRIVATE_INC Qt6::GuiPrivate
                            INTERFACE_INCLUDE_DIRECTORIES)

        # THE PROPERTY IS GENERATOR EXPRESSIONS, not paths. Qt writes each private
        # include directory as
        #     $<$<BOOL:$<TARGET_PROPERTY:Qt6::Gui,_qt_module_has_private_headers>>:/usr/...>
        # which no `if(EXISTS)` can evaluate: generator expressions are resolved
        # after configure, so testing the raw string always says "missing" — on a
        # machine that has the headers as readily as on one that does not. Pull the
        # absolute paths back out. Only the value half of the expression contains a
        # slash, so matching them is unambiguous.
        string(REGEX MATCHALL "/[^;>]+" PIRICAD_QT_GUI_PRIVATE_DIRS
               "${PIRICAD_QT_GUI_PRIVATE_INC}")

        foreach(_dir IN LISTS PIRICAD_QT_GUI_PRIVATE_DIRS)
            if(EXISTS "${_dir}/rhi/qrhi.h" OR EXISTS "${_dir}/QtGui/rhi/qrhi.h")
                set(PIRICAD_QRHI_HEADER_FOUND TRUE)
            endif()
        endforeach()
    endif()

    if(NOT PIRICAD_QRHI_HEADER_FOUND)
        message(FATAL_ERROR
            "PIRICAD_WITH_RHI=ON but <rhi/qrhi.h> was not found. QRhi lives in Qt "
            "Gui's PRIVATE headers, which most distributions package separately "
            "from the public ones.\n"
            "  Debian/Ubuntu: sudo apt install qt6-base-private-dev\n"
            "  Fedora:        sudo dnf install qt6-qtbase-private-devel\n"
            "  Arch:          included in qt6-base\n"
            "  vcpkg:         installed with qtbase\n"
            "  Or configure with -DPIRICAD_WITH_RHI=OFF to use the QPainter backend.")
    endif()

    # NEVER compiled or patched at runtime (render.md P11): the packs are baked
    # here, at build time, and land in the binary as Qt resources.
    # THE TARGETS ARE DELIBERATE. qsb's default set starts at ESSL 100, which is
    # OpenGL ES 2.0 — a 2007 profile with no `textureSize` and no derivatives, so
    # the SDF text shader cannot be expressed in it at all. GLES 3.0 / GL 3.3 is
    # the floor a GPU-canvas CAD program can ask for, and every desktop and mobile
    # GPU that can run this drawing at 16 ms/frame has had it for a decade.
    qt_add_shaders(piricad "piricad_shaders"
        GLSL "300es,330"
        HLSL 50
        MSL 12
        PREFIX "/piricad"
        FILES
            shaders/fill.vert
            shaders/fill.frag
            shaders/line.vert
            shaders/line.frag
            shaders/text.vert
            shaders/text.frag
    )

    target_sources(piricad PRIVATE src/rhi_backend.cpp)
    target_link_libraries(piricad PRIVATE Qt6::GuiPrivate)
endif()

if(PIRICAD_WITH_QGIS)
    # QGIS lives in /src/app alone (Article 3.4, 8.5): nothing below the shell
    # learns that it exists, and the canvas reaches it through render::Backend.
    # SYSTEM, and this is not a suppression. CLAUDE.md 5.14 forbids silencing a
    # warning; what this does is tell the compiler which headers are OURS. QGIS's
    # own headers carry conversion and shadow warnings that no edit of this
    # repository can fix, and a wall of them is how a real finding in our own code
    # gets scrolled past.
    # The backend TU is compiled ONLY here. Its call site in painter_backend.cpp
    # is already behind `#if PIRICAD_HAVE_QGIS`, so on a machine without the QGIS
    # development files this file has no callers and nothing but unfindable
    # includes — listing it unconditionally broke the build everywhere QGIS is
    # absent, which on macOS is everywhere, since Homebrew ships QGIS as an
    # application cask with no headers.
    target_sources(piricad PRIVATE src/qgis_backend.cpp)
    target_include_directories(piricad SYSTEM PRIVATE ${QGIS_INCLUDE_DIR})
    target_link_libraries(piricad PRIVATE ${QGIS_CORE_LIBRARY} Qt6::Xml)
    target_compile_definitions(piricad PRIVATE PIRICAD_HAVE_QGIS=1)
endif()
