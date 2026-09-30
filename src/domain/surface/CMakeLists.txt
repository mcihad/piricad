# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_domain_surface — the terrain model: triangulation and contours.
#
# piricad.md §12: a levelling survey becomes a surface, and the surface becomes
# the eş yükselti eğrileri a plan sheet carries. Depends on core and command;
# never the reverse (CLAUDE.md Article 3.2).
add_library(piricad_domain_surface STATIC
    src/contour.cpp
    src/contour_command.cpp
    src/earthwork_command.cpp
    src/commands.cpp
)

target_include_directories(piricad_domain_surface PUBLIC include)
target_link_libraries(piricad_domain_surface
    PUBLIC  piricad_core piricad_command
    PRIVATE piricad_flags
)

# PRIVATE: CDT lives inside contour.cpp and nothing outside it learns it exists
# (Article 9 — a dependency's headers stay in the owning module's .cpp files).
if(PIRICAD_WITH_CDT)
    target_link_libraries(piricad_domain_surface PRIVATE piricad_cdt)
    target_compile_definitions(piricad_domain_surface PRIVATE PIRICAD_HAVE_CDT=1)
endif()
