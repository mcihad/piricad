# Command Bus — Rules

> Scope: `/src/command` (CMake target `kentos_command`) — `Bus`, `Registry`, `CommandSpec`/`KENTOS_COMMAND`, `Value`, `Task<T>`, `Context`, `InputSource`, `Transaction`/`UndoStack`, `Journal`, `Parser`. | Depends on: `kentos_core` only. | Source: kentoscad.md §2.1–§2.6, §3, §10.4.

## Hard Rules

R1. Every change to `Document` state MUST enter through `Bus::dispatch` (`kentos_cad/command/bus.hpp`) in the fixed order **dispatch → validate → transact → journal**. (§2.1)
R2. GUI, command line, `kentos_script` JSON runner, AI and batch MUST call the *same* `Bus::dispatch` overload. No client-specific entry point, fast path, or trusted flag. (§2.1)
R3. A command invocation MUST be fully expressible as `Value` (`value.hpp`) and MUST round-trip losslessly through the JSONL form `{cmd, args, crs, katman}`; `dispatch(parse(serialize(x)))` MUST produce identical document state as `dispatch(x)`. (§2.2)
R4. Every command MUST be declared exactly once via `KENTOS_COMMAND` yielding a `CommandSpec` registered in `Registry` (`spec.hpp`, `registry.hpp`) with `.id`, `.names`, `.category`, `.params`, `.undo`, `.flags`, `.summary` all set. `Registry`-generated help and the AI tool catalogue group by `.category` (§2.3).
R5. CLI help, script bindings, AI tool schema and docs MUST be **generated** by walking `Registry` at build or run time. (§2.3)
R6. `.id` MUST be stable, lowercase and namespaced (`core.line`, `core.undo`, `core.layer`, `core.zoom`, `core.erase`, `core.script`, `core.help`). Changing an id is a breaking change and MUST ship a journal migration entry.
R7. `.names` MUST list, in order: Turkish primary, ASCII-folded Turkish variant, English equivalent, abbreviations — e.g. `core.line` = `ÇİZGİ, CIZGI, LINE, Ç, L`. Name matching MUST use the module's explicit Turkish case-folding table (i/I dotted–dotless), never locale-free C functions.
R8. Interactive commands MUST be C++20 coroutines returning `Task<T>` (`task.hpp`) and MUST obtain input via `co_await ctx.point(...)` / `co_await ctx.number(...)`. (§2.4)
R9. `Context` (`context.hpp`) MUST resolve every await through the `InputSource` abstraction (`input.hpp`). The `Context` public API MUST NOT expose the input origin (mouse / cli / script / ai) to the command body. (§2.4)
R10. Cancellation (ESC, `stop_token`, AI rejection) MUST unwind as `co_return` with the surrounding `Transaction` rolled back, never as a partial commit. (§2.5)
R11. One command = one `UndoStack` entry by default. Deviation is allowed only by declaring `UndoPolicy` in the `CommandSpec`. (§2.5)
R12. A script block, a JSON command array, or an AI suggestion MUST commit as **one merged** `Transaction` = one undo step. (§2.5)
R13. Any validation failure inside a `Transaction` MUST trigger full rollback; zero partial application. (§2.5)
R14. Topology checks, mevzuat rules and geometry validity MUST run inside `Bus` between dispatch and transact, never in a client. (§2.6)
R15. Argument arity, type and range MUST be declared as `Param` in the spec and checked by `Bus` **before** the command body runs. (§2.3, §2.6)
R16. `parser.hpp` MUST expose exactly one grammar object, used by both the command line and the script engine. (§3, implementation note)
R17. The parser MUST accept absolute `485320,4310220`, relative `@50,30`, polar `@100<45`, and inline expressions `@(100*3),0`, and MUST convert to `Mm` int64 fixed point exactly once, at the parse boundary. (§3)
R18. Transparent commands (`Flags` transparent, e.g. `core.zoom`) MUST *suspend* the running coroutine and resume it, never cancel or restart it. (§3)
R19. Every failure MUST be returned as `Result<T>` with `Error{code, message}` and the message MUST name expectation and input — e.g. `"Expected: 2 numbers or an object snap. Got: 'abc'"`. (§3)
R20. `Journal` (`journal.hpp`) MUST be append-only JSONL, written after commit, and MUST be replayable to reconstruct document state. Exactly two line kinds exist: `{cmd, args, crs, katman}` for a committed command, and `{kind:"meta", ...}` for a non-command record (script sandbox level and consent — `.claude/script.md` R11/R20; plugin id, hash and user decision — `.claude/plugin-api.md` R10). Replay MUST apply the first kind and ignore the second. (§2.2)
R21. Journal writes MUST be enqueued from the calling thread and performed on a dedicated writer thread. (§10.4)
R22. The dispatch hot path MUST be allocation-free: arguments live in a POD union or a per-thread arena. Budgets: script dispatch ≤ 10 µs, command-line keystroke → screen ≤ 30 ms. (§10.4, §10.1)
R23. `TOPLU_BASLA` / `TOPLU_BITIR` MUST collapse N commands into one validation pass and one undo step. (§10.4)
R24. `kentos_command` MUST compile Qt-free and link only `kentos_core`; see `.claude/core.md` for the core purity rules.
R25. Commands owned by `/src/domain` (ifraz, tevhit, yola terk, DOP) MUST be registered through the same `Registry` and obey R1–R23; their domain semantics live in `.claude/domain.md`.

## Absolute Prohibitions

P1. NEVER add a mutation path reachable from the GUI that is not reachable from CLI, script and AI. (§2.1)
P2. NEVER mutate `Document` outside an open `Transaction`.
P3. NEVER maintain a second, hand-synced command list — no menu table, no CLI table, no AI tool JSON file, no docs table that is not generated from `Registry`. (§2.3)
P4. NEVER grant a client a privilege, bypass token, `trusted` flag, or validation-skipping mode.
P5. NEVER flush or `fsync` the `Journal` synchronously on the UI thread. (§10.4)
P6. NEVER commit an undo step that leaves a half-applied cadastral or imar edit. (§2.5)
P7. NEVER check argument arity, type or numeric range inside a command body; those three MUST be `Param` declarations validated by `Bus` (R15).
P8. NEVER hand-roll a state machine, `enum State` member, or callback chain for an interactive command; coroutines only. (§2.4)
P9. NEVER use `std::function`, `std::shared_ptr`, `new`, or any container that allocates in the dispatch hot path. (§10.4)
P10. NEVER let a command body ask, branch on, or log the `InputSource` kind. (§2.4)
P11. NEVER add a second parser or grammar for the command line. (§3)
P12. NEVER `#include <Q...>` anywhere in `/src/command`.
P13. NEVER call `std::toupper` / `std::tolower` on command names or Turkish arguments.
P14. NEVER let a client hand raw coordinates to a command body without passing through `Parser` and `Value`; AI-produced coordinates are additionally governed by `.claude/ai.md`.
P15. NEVER silently ignore an unknown or surplus argument — reject with `Error`.
P16. NEVER let a `Task<T>` command capture a raw pointer or reference to `Document` across a `co_await`.

## Definitions of Done

- [ ] New/changed command is declared once with `KENTOS_COMMAND` and appears in `Registry` with Turkish + English + abbreviations.
- [ ] `Registry`-generated CLI help, script binding and AI schema were regenerated; no file was hand-edited to match.
- [ ] Interactive path is a `Task<T>` coroutine; cancellation test passes with empty undo stack delta.
- [ ] Same command driven from GUI, CLI and JSON produces byte-identical `Document` state and identical `Journal` lines.
- [ ] Journal replay of the new command reproduces the golden document.
- [ ] `Value` round-trip test added; validation-failure test asserts full rollback.
- [ ] Error strings state expectation and received input.
- [ ] `tests/bench` shows dispatch ≤ 10 µs and no allocations on the hot path.

## Enforcement

- `scripts/ci-gate-core-purity.sh` — fails on `#include <Q...>` or any non-`kentos_core` link in `kentos_command` (R24, P12).
- `scripts/ci-gate-command-mutation.sh` — fails on `Document` mutation outside a `Transaction`, on any second command list not generated from `Registry`, and on `std::function`/`shared_ptr` in dispatch (P2, P3, P9).
- `/tests/journal` — journal replay regression suite: replay must reproduce golden documents bit-identically (R3, R20).
- `/tests/unit` — the equality proof test: one command issued from GUI client, CLI string and JSON array must yield identical document state and identical journal lines (R2, R5); plus rollback, cancellation, `TOPLU_BASLA`/`TOPLU_BITIR` merge and error-message-format tests (R10, R13, R19, R23).
- `/tests/bench` — dispatch latency and allocation-count gates; > 10 % regression breaks the build (R22).
