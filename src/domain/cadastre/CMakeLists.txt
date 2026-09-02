# SPDX-License-Identifier: GPL-3.0-or-later
# kentos_domain_cadastre — tevhit, ifraz, topoloji. Depends on core and command;
# never the reverse (CLAUDE.md Article 3.2).
#
# kentoscad.md §12: what this module produces is a legal document. Every rule it
# applies that comes from a regulation lives in /data, never here (5.13), and a
# regulatory change carries a domain-expert sign-off (6.11).
add_library(kentos_domain_cadastre STATIC
    src/topology.cpp
    src/merge_command.cpp
    src/split_command.cpp
    src/commands.cpp
)

target_include_directories(kentos_domain_cadastre PUBLIC include)
target_link_libraries(kentos_domain_cadastre
    PUBLIC  kentos_core kentos_command
    PRIVATE kentos_flags
)
