# SPDX-License-Identifier: GPL-3.0-or-later
#
# The protocol half of the AI layer, listed in its own file so that the people
# and agents working on the transport and on the model dialects never edit the
# same line of CMake. `src/ai/CMakeLists.txt` includes this and reads the
# variable below; an empty list is a valid state.
#
# THE THREE MCP SOURCES CARRY AGPL-3.0-or-later HEADERS, not the tree's
# GPL-3.0-or-later. CLAUDE.md Article 2.1 rules "AGPLv3 for server/web
# components" and these three files are the protocol engine of a server; the
# maintainer confirmed the reading. This CMake file is build plumbing rather than
# part of the server, so it keeps the tree's licence. `NOTICE` and the SBOM are
# where the mixed licensing is recorded for a distributor.
set(KENTOS_AI_MCP_SOURCES
    src/jsonrpc.cpp
    src/endpoint.cpp
    src/mcp.cpp
)
