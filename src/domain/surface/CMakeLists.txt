# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_domain_surface — the terrain model: triangulation and contours.
#
# kentoscad.md §12: a levelling survey becomes a surface, and the surface becomes
# the eş yükselti eğrileri a plan sheet carries. Depends on core and command;
# never the reverse (CLAUDE.md Article 3.2).
add_library(kentos_domain_surface STATIC
    src/contour.cpp
    src/contour_command.cpp
    src/commands.cpp
)

target_include_directories(kentos_domain_surface PUBLIC include)
target_link_libraries(kentos_domain_surface
    PUBLIC  kentos_core kentos_command
    PRIVATE kentos_flags
)

# PRIVATE: CDT lives inside contour.cpp and nothing outside it learns it exists
# (Article 9 — a dependency's headers stay in the owning module's .cpp files).
if(KENTOS_WITH_CDT)
    target_link_libraries(kentos_domain_surface PRIVATE kentos_cdt)
    target_compile_definitions(kentos_domain_surface PRIVATE KENTOS_HAVE_CDT=1)
endif()
