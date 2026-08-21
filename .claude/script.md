# Script Engine — Rules

> Scope: `/src/script` (target `piricad_script`) + the future optional Python module  |  Depends on: `piricad_command` only (→ `piricad_core`)  |  Source: piricad.md §3, §4.1–§4.3, §2.5, §2.6, §10.1, §10.4

## Hard Rules

R1. Every state mutation from a script MUST go through `h.komut(...)` → `Bus::dispatch` in `piricad/command/bus.hpp`, so undo, validation and journalling run automatically (§4.3).
R2. `/src/script` MUST link only `piricad_command`. No `/src/app`, `/src/io`, `/src/domain`, `/src/render`, no Qt (canon dependency graph).
R3. Phase 0 MUST ship exactly one host: `piricad/script/json_runner.hpp`, replaying a JSON command array through the same `Bus`. Lua and Python land in Phase 2.
R4. Script text MUST be parsed by the `Parser` object in `piricad/command/parser.hpp` — the same grammar instance the command line uses (§3, implementation note).
R5. Layer roles are fixed (§4.1): every expression evaluator, style rule, label expression and area calculator MUST be Lua (sol2); plugins, batch processing and data pipelines MUST be Python (pybind11). A hot-path evaluator written in Python is a defect.
R6. Lua and Python MUST sit behind `PIRICAD_WITH_LUA` / `PIRICAD_WITH_PYTHON`, both defaulting to `OFF`; `piricad` MUST build, start and pass all tests with both OFF (§4.2, "optional module"). Build-option mechanics: see `.claude/build.md`.
R7. CPython MUST be pinned to 3.12 and shipped embedded inside the optional Python module package — never in the base installer — resolved relative to the install root (§4.2; `.claude/build.md` R23).
R8. `pip` packages MUST install into an isolated venv under the project data directory, never into the embedded interpreter's own `site-packages` (§4.2).
R9. Read APIs MAY be rich and direct, but MUST return values or const views only — `Mm`, `Point2`, `Box2`, `EntityId`, `Value`, const spans. Write APIs are `h.komut()` and nothing else (§4.3).
R10. Every binding signature exposed to a script MUST be free of `Document&`, `Document*`, `Layer&`, `Layer*` and any non-const core reference (§4.3).
R11. Sandbox level MUST be one of `güvenli` / `proje` / `tam`, MUST default to `güvenli`, MUST be passed explicitly per run, and MUST be recorded as a `{kind:"meta"}` `Journal` line for that run (`.claude/command.md` R20) (§4.3).
R12. `tam` MUST require interactive per-script user consent recorded with the script's identity hash (§4.3).
R13. Long-running scripts MUST run on `std::jthread` or a separate process, driven by a `std::stop_token` polled at least every 50 ms; `core.script` MUST return `Task<T>` and MUST NOT block the UI thread (§4.2, §7.1).
R14. One script block = one merged undo step (§2.5). A validation failure anywhere in the block MUST roll the whole block back — partial application is forbidden.
R15. Bulk work MUST use `TOPLU_BASLA` / `TOPLU_BITIR`: a script creating 100 000 objects performs exactly one validation pass and writes exactly one undo record (§10.4).
R16. Command dispatch cost from a script MUST stay ≤ 10 µs, measured by a `/tests/bench` gate; >10 % regression breaks the build (§10.1).
R17. The script dispatch hot path MUST be allocation-free — arguments packed into the POD `Value` union or an arena; no `std::function`, no `shared_ptr` (§10.4).
R18. Command names and aliases MUST be resolved through `Registry` (`core.line`, `ÇİZGİ`/`LINE`, …); `/src/script` MUST NOT hold its own name table (§2.3).
R19. Turkish identifiers and command names MUST be case-folded with the shared Turkish folding table exposed by `piricad_command` (`.claude/command.md` R7), never `std::toupper`/`std::tolower` (CLAUDE.md 5.6, i/I).
R20. Plugins MUST come from the signed plugin repository; an unsigned plugin loads only after an explicit warning the user must accept, and the acceptance MUST be written as a `{kind:"meta"}` `Journal` line (`.claude/command.md` R20, `.claude/plugin-api.md` R10) (§4.3).
R21. Script errors MUST propagate as `Result<T>` / `Error{code,message}` with actionable text (expected vs. received), matching the command-line error contract (§3).

## Absolute Prohibitions

P1. NEVER call Python on a per-feature hot path — label expression, style rule, or per-object evaluation callback. Use Lua (§4.1, §10.4).
P2. NEVER use, probe, or fall back to the system Python interpreter or system `site-packages` (§4.2).
P3. NEVER give a script a path that bypasses validation, the transaction boundary, or the journal — including "fast" internal helpers (§2.6).
P4. NEVER hand a script a raw pointer, mutable reference, iterator, or non-const span into `Document`, `Layer`, or the SoA polyline store (§4.3).
P5. NEVER write a second parser, tokenizer, or expression grammar inside `/src/script`; extend `piricad/command/parser.hpp` instead — see `.claude/command.md` (§3).
P6. NEVER execute a script body, plugin `import`, or interpreter startup on the UI thread (§4.2).
P7. NEVER auto-grant or auto-escalate the `tam` sandbox level. No config key, environment variable, CLI flag, script header, or plugin manifest may raise the level without interactive consent (§4.3).
P8. NEVER touch the filesystem or network at `güvenli`; NEVER touch anything outside the project directory at `proje` (§4.3).
P9. NEVER `#include <Q...>` or any `/src/domain`, `/src/io`, `/src/render` header inside `/src/script` (canon dependency graph).
P10. NEVER let a script register, redefine, or shadow a command outside `Registry`, and NEVER let it construct a `Transaction` directly.
P11. NEVER ignore or reset a `std::stop_token` request; cancellation is not advisory.
P12. NEVER expose raw memory addresses, `id()`-style handles, `ctypes`, or FFI escape hatches from the Python or Lua binding surface.
P13. NEVER put the Python layer in the base installer; it ships only as the separate downloadable module package (§4.2, `.claude/build.md` R23).
P14. NEVER let a script drive AI providers or emit AI-produced coordinates — see `.claude/ai.md` (§5.2).

## Definitions of Done

- [ ] Builds and tests green with `PIRICAD_WITH_LUA=OFF` and `PIRICAD_WITH_PYTHON=OFF`.
- [ ] New binding has zero non-const core types in its signature (grep-checkable).
- [ ] New script entry point takes a sandbox level parameter and a `std::stop_token`.
- [ ] Journal shows exactly one undo step per script block; bulk case shows one validation pass.
- [ ] `/tests/bench` dispatch benchmark still ≤ 10 µs, no >10 % regression.
- [ ] Sandbox test added for the new surface at `güvenli`, `proje`, and `tam`.
- [ ] No new grammar/keyword handling outside `piricad/command/parser.hpp`.

## Enforcement

- `/tests/unit` — script sandbox tests: `güvenli` denies file/network, `proje` denies escape from the project directory, `tam` fails without recorded consent.
- `/tests/journal` — identical-journal proof: the same script replayed through `json_runner.hpp` and through the command line yields byte-identical JSONL.
- `/tests/bench` — script→command dispatch gate (≤ 10 µs) and the 100k-object batch case (one validation, one undo record).
- `scripts/ci-gate-layering.sh` — fails the build if `/src/script` contains `#include <Q`, a `/src/domain`/`/src/io`/`/src/render` include, `Document&`/`Document*`/`Layer*` in an exported binding signature, any direct `Document::` mutating call, or a second parser/lexer symbol (R2, R4, P4, P5, P9).
