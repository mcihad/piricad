# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.28)

project(KentOSCad
    VERSION 0.1.0
    DESCRIPTION "Türkiye odaklı CBS + CAD harita yazılımı"
    HOMEPAGE_URL "https://github.com/mcihad/kentos_cad"
    LANGUAGES CXX
)

# C++20 is the baseline. Coroutines are load-bearing (kentoscad.md §2.4, §7.1).
# C++23 features are used only behind __cpp_lib_* feature tests.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C++20 Modules are banned for now: toolchain behaviour is still inconsistent and
# mixing them with header-based dependencies (Qt, GDAL) is unreliable (§7.1).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(KentOSCadOptions)
include(KentOSCadFlags)
include(KentOSCadDependencies)

include(CTest)

add_subdirectory(src/core)
add_subdirectory(src/command)
add_subdirectory(src/io)
add_subdirectory(src/script)
add_subdirectory(src/render)
add_subdirectory(src/domain/geodesy)
add_subdirectory(src/domain/cadastre)
add_subdirectory(src/domain/surface)

if(KENTOS_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(KENTOS_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "KentOSCad ${PROJECT_VERSION}")
message(STATUS "  build type ....... ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler ......... ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  application ...... ${KENTOS_BUILD_APP}")
message(STATUS "  tests ............ ${KENTOS_BUILD_TESTS}")
message(STATUS "  GDAL / PROJ ...... ${KENTOS_WITH_GDAL} / ${KENTOS_WITH_PROJ}")
message(STATUS "  DWG (LibreDWG) ... ${KENTOS_WITH_DWG}")
message(STATUS "  Lua / Python ..... ${KENTOS_WITH_LUA} / ${KENTOS_WITH_PYTHON}")
message(STATUS "  QRhi canvas ...... ${KENTOS_WITH_RHI}")
message(STATUS "  Yazı atlası ...... ${KENTOS_WITH_TEXT}")
message(STATUS "")
