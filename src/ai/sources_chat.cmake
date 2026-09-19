# SPDX-License-Identifier: GPL-3.0-or-later
#
# The model-facing half: provider profiles, the four dialects, the stream
# decoders and the conversation model. See sources_mcp.cmake for why this is a
# separate file.
set(KENTOS_AI_CHAT_SOURCES
    src/provider.cpp
    src/provider_catalog.cpp
    src/sse.cpp
    src/redact.cpp
    src/chat.cpp
    src/dialect_openai_chat.cpp
    src/dialect_openai_responses.cpp
    src/dialect_anthropic.cpp
    src/dialect_ollama.cpp
)
