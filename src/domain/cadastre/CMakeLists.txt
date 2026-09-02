# SPDX-License-Identifier: GPL-3.0-or-later
# piricad_domain_cadastre — tevhit, ifraz, topoloji. Depends on core and command;
# never the reverse (CLAUDE.md Article 3.2).
#
# piricad.md §12: what this module produces is a legal document. Every rule it
# applies that comes from a regulation lives in /data, never here (5.13), and a
# regulatory change carries a domain-expert sign-off (6.11).
add_library(piricad_domain_cadastre STATIC
    src/topology.cpp
    src/merge_command.cpp
    src/split_command.cpp
    src/commands.cpp
)

target_include_directories(piricad_domain_cadastre PUBLIC include)
target_link_libraries(piricad_domain_cadastre
    PUBLIC  piricad_core piricad_command
    PRIVATE piricad_flags
)
