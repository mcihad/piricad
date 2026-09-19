# SPDX-License-Identifier: GPL-3.0-or-later
#
# kentos_ai — the AI layer, and it is SANS-IO.
#
# Everything here is computation over data: the tool catalogue projected from the
# command registry, the two generated documents, and (as they land) the MCP
# protocol engine, the provider dialects and the suggestion machine. No socket,
# no file dialog, no widget, and above all NO Qt — `.claude/ai.md` P10 and
# CLAUDE.md 3.3/3.4 make that a build failure, and `scripts/ci-gate-layering.sh`
# already watches this directory for it.
#
# The application supplies what this module deliberately cannot have: the inbound
# and outbound transports, the key store and the windows. That is the same split
# `render::Backend` uses, and it is what lets every line below be tested with no
# Qt, no network and no display.
include(sources_mcp.cmake)
include(sources_chat.cmake)

add_library(kentos_ai STATIC
    src/catalog.cpp
    src/policy.cpp
    src/llmstxt.cpp
    src/handles.cpp
    src/plan.cpp
    src/audit.cpp
    src/gate.cpp
    src/commands/read_tools.cpp
    src/commands/ai_commands.cpp
    src/commands/provider_command.cpp
    ${KENTOS_AI_MCP_SOURCES}
    ${KENTOS_AI_CHAT_SOURCES}
)

target_include_directories(kentos_ai PUBLIC include)

# UP TO command AND core ONLY (Article 3.2: `ai -> command`). Not io, not render,
# not script, not domain, not app.
target_link_libraries(kentos_ai PUBLIC kentos_command PRIVATE kentos_flags)

add_library(KentOSCad::ai ALIAS kentos_ai)
