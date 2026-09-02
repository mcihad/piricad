# AI Layer — Rules

> Scope: `/src/ai`, the AI panel in `/src/app`, `/tests/ai-eval`, `/data/corpus`  |  Depends on: `kentos_command` (`Registry`, `Bus`, `Transaction`, `UndoStack`, `Journal`, `Task<T>`, `Value`, `InputSource`), `kentos_core`  |  Source: kentoscad.md §2.3, §2.5, §2.6, §5.1–§5.6, §10.4, §12

## Hard Rules

R1. **AI does not touch geometry. AI emits commands.** (§5.1, verbatim.) No translation unit in `/src/ai` may mutate `Document`, `Layer` or any entity store; the only write path is `Bus` dispatch of a registered command id.
R2. Every AI turn MUST follow the §5.1 pipeline in order, no stage skipped or reordered: context gathering (read tools) → model → validation → preview → user approval → apply as one transaction → audit record.
R3. No automatic application (§5.2.1). Every AI-produced command sequence MUST be rendered as a preview and MUST require an explicit user approval action before dispatch.
R4. One approved suggestion = ONE undo step (§5.2.2, §2.5). The whole sequence MUST execute inside a single `Transaction` producing exactly one `UndoStack` entry, reversible by one `Ctrl+Z`.
R5. A validation failure anywhere in the sequence MUST roll the whole transaction back (§2.5). Partial application is forbidden.
R6. An audit record is mandatory (§5.2.3) and MUST capture: prompt text, model identity and version, provider kind and endpoint, the generated command sequence, the user decision (approve / reject / edit), and a UTC timestamp. Rejected suggestions MUST be recorded too.
R7. Every `EntityId` created by an approved suggestion MUST resolve back to its audit record id, so "why is this line here?" is answerable (§5.2.3).
R8. Under BÖHHBÜY the surveying/geomatics engineer is responsible and AI cannot sign (§5.2.4): the AI panel and the preview MUST label every AI output **"öneri"** in every state, including loading, error and partial states.
R9. Coordinate hallucination defence (§5.2.5): every `Point2` / `Mm` in a generated command MUST originate from a tool-call result handle (`nesne_seç`, `kesişim_bul`, `ofset_hesapla`). Model text supplies only command ids, names, ids and tool references.
R10. A generated command carrying a coordinate `Value` not traceable to a recorded tool-call result MUST be rejected before validation, with the rejection audit-logged.
R11. Context MUST be supplied only through queryable read tools (§5.3): `katmanlari_listele()`, `oznitelik_semasi(katman)`, `sorgula(katman, ifade, limit)`, `secimi_al()`, `gorunum_bilgisi()`, `mevzuat_ara(sorgu)`. Read tools MUST be read-only and `sorgula` MUST enforce a hard result cap.
R12. The AI tool catalogue MUST be generated from `Registry` via the `Flags::AiAccessible` bit (§2.3). Setting that bit in a `CommandSpec` MUST be the only action needed to expose a command to AI.
R13. `llama.cpp`, an in-institution vLLM/Ollama server, and a cloud API MUST sit behind one provider abstraction, and the institution MUST choose the provider by configuration; local model support is mandatory, not optional (§5.4).
R14. The project MUST carry a sensitivity flag; when set, provider selection MUST be restricted to local / in-institution endpoints, enforced in `/src/ai`, not in UI code.
R15. Every legislation RAG answer MUST cite the madde number, the publication date and the corpus document id (§5.5). An answer without a citation MUST be suppressed, not shown with a warning.
R16. The legislation corpus and its embedding index live in `/data/corpus` as DATA, never as compiled-in code.
R17. AI-generated commands MUST pass the same `Bus` validation stage (schema, parameter, topology, mevzuat rules) as mouse, command line and script input (§2.6). Commands MUST be submitted with `InputSource` = ai; see `.claude/command.md`.
R18. All AI calls MUST be fully async and cancellable via `Task<T>` (§10.4). The application MUST stay fully functional while a model response is pending, and a cancel MUST abort the in-flight request leaving `Document` untouched.
R19. Model output MUST be parsed against a strict schema (command id + named `Value` arguments). Unknown command id, unknown parameter or malformed argument MUST be a hard reject.
R20. Preview MUST be non-destructive: a rejected or cancelled suggestion MUST leave `Document` bit-identical to its pre-suggestion state.
R21. Turkish domain-language performance MUST be tested separately from English benchmarks (§5.6), against an evaluation set of 200–300 real Turkish requests paired with the expected command sequence, stored in `/tests/ai-eval` and run in CI.
R22. The eval set MUST cover Turkish command mapping (e.g. "üç metre içeri kaydır" → `OFSET mesafe=-3`) and the domain glossary: ifraz, tevhit, ihdas, irtifak, DOP, TAKS, KAKS, nazım imar planı, uygulama imar planı, muhdesat (§5.6).
R23. Turkish text in `/src/ai` MUST be cased and normalised with the shared Turkish-aware helper in `/src/command` (see `.claude/command.md`).

## Absolute Prohibitions

P1. NEVER auto-apply an AI command sequence. No "trust mode", no setting, no CLI flag, no "remember my choice" that bypasses R3.
P2. NEVER let a coordinate originate in model text. A numeric literal parsed out of model prose into an `Mm` or `Point2` is BANNED.
P3. NEVER send document data, attributes, coordinates or corpus excerpts to a cloud provider when the project is marked sensitive.
P4. NEVER render, in the AI panel or the preview, any string from this blocklist: "onaylandı", "kontrol edildi", "uygundur", "mevzuata uygundur", "onay", "imzalandı". Extending the list is an edit to this line. (R8 carries the positive requirement.)
P5. NEVER answer a legislation question without a madde reference and publication date (§5.5) — an uncited answer is worthless and dangerous.
P6. NEVER let AI skip the validation layer: calling a command's `run()` directly, touching the entity store, or any AI-only fast path around `Bus` is BANNED (§2.6).
P7. NEVER hand-maintain a second tool schema. A checked-in static tool/JSON catalogue duplicating `Registry` is BANNED (§2.3).
P8. NEVER block the UI on a model response. Synchronous HTTP, blocking waits, or joining a model thread on the UI thread are BANNED (§10.4).
P9. NEVER dump the drawing into the prompt (§5.3) — no bulk entity, coordinate or attribute serialisation into prompt text.
P10. NEVER include `<Q...>` in `/src/ai`, and NEVER link `/src/ai` against `kentos_render`, `/src/domain`, `/src/io` or `/src/app`. The panel is the only Qt part and lives in `/src/app`.
P11. NEVER write API keys, tokens or endpoint credentials into the audit record, the `Journal`, or the repository.
P12. NEVER use `std::toupper` / `std::tolower` on Turkish text; `/src/ai` folds through the `kentos_command` table (R23, CLAUDE.md 5.6).

## Definitions of Done

- [ ] New/changed AI-reachable command is exposed only via `Flags::AiAccessible`; no schema file edited by hand.
- [ ] Suggestion path proven: preview shown, approval required, one `UndoStack` entry, full rollback on failure.
- [ ] Audit record written for approve AND reject, with all six fields of R6.
- [ ] Every coordinate in the generated sequence traces to a tool-call result.
- [ ] Provider abstraction still builds and runs with the local backend; sensitivity flag path tested.
- [ ] Any legislation answer path returns madde number + publication date + corpus doc id.
- [ ] `/tests/ai-eval` Turkish set updated if a new Turkish phrasing or domain term is supported; CI green, no regression.
- [ ] UI strings label output "öneri"; translations updated.

## Enforcement

- `/tests/ai-eval` CI suite — Turkish request → expected command sequence, 200–300 cases; accuracy regression breaks the build (§5.6).
- `tests/ai-eval/audit_record_completeness_test.cpp` — fails if any approve/reject path emits a record missing prompt, model identity+version, commands, decision or timestamp (R6, §12 AI checklist).
- `tests/ai-eval/tool_schema_matches_registry_test.cpp` — regenerates the tool catalogue from `Registry` and fails on any diff against what `/src/ai` serves (R12, P7).
- `tests/ai-eval/no_coordinate_literal_test.cpp` — feeds recorded model outputs containing coordinate literals; fails if any reaches `Bus` (R9, R10, P2).
- `tests/ai-eval/no_autoapply_test.cpp` — dispatches without approval and asserts rejection; also asserts a rejected suggestion leaves `Document` bit-identical (R3, R20).
- `tests/ai-eval/citation_required_test.cpp` — legislation answers without madde + date are suppressed (R15, P5).
- `scripts/ci-gate-layering.sh` — `<Q...>`, forbidden module includes, `std::toupper`/`std::tolower`, or any P4 blocklist string under `/src/ai` or the AI panel of `/src/app` breaks the build (P4, P10, P12).
