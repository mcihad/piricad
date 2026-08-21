# /src/ai — AI layer

Empty in Phase 0. Rules already binding: `.claude/ai.md`.

The one principle (piricad.md §5.1): **AI does not touch geometry. AI emits
commands.** The tool catalogue is already generated — `Registry::ai_tool_schema()`
in `/src/command` produces it from the `Flags::AiAccessible` bit, so a new command
becomes available to the model with no second list to maintain (§2.3).

What lands here: the provider abstraction (llama.cpp / local vLLM / cloud behind
one interface), the read-tool set, the preview-and-approve pipeline, the audit
record, and the mevzuat RAG.
