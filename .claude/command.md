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
R8a. Work a command must not do on the bus thread — a file read (io.md P3) — MUST be a `Job` awaited through `run_job` (`job.hpp`). Where it runs is decided by the SESSION, never by the client: a session the client keeps and resumes later (`Bus::begin_interactive`, `client_driven`) parks in `SessionState::Working` and `Bus::on_job_host` runs the job off-thread; a session driven to completion in one call runs it in place. The command body is one body, and the document and the journal MUST be identical either way (Article 1.2). A host never resumes a session from inside the hook; `Bus::finish` refuses a `Working` session.
R9. `Context` (`context.hpp`) MUST resolve every await through the `InputSource` abstraction (`input.hpp`). The `Context` public API MUST NOT expose the input origin (mouse / cli / script / ai) to the command body. (§2.4)
R9a. The input aids — object snap, grid, dik mod, polar and tracking, surface normal, step — MUST act on a point AIMED by a hand and on nothing else. `Value::aimed_point` is that mark; only the canvas makes one (a click through `Controller::supplyAimedPoint`, a grip drag's `nokta`). A STATED point — typed on any command line, carried by an invocation, written in a script, proposed by an agent, replayed from a journal — is exact and `apply_input_aids` passes it untouched: the aperture is pixels, and a screen tolerance MUST NOT decide a written coordinate (TODOS F-03). The mark is not part of `Value` equality and is never serialised; the journal records the resolved point as a statement. This is not a branch on `InputSource` (P10): the value says what it is, whoever supplied it.
R10. Cancellation (ESC, `stop_token`, AI rejection) MUST unwind as `co_return` with the surrounding `Transaction` rolled back, never as a partial commit. (§2.5)
R11. One command = one `UndoStack` entry by default. Deviation is allowed only by declaring `UndoPolicy` in the `CommandSpec`. (§2.5)
R12. A script block, a JSON command array, or an AI suggestion MUST commit as **one merged** `Transaction` = one undo step. (§2.5)
R13. Any validation failure inside a `Transaction` MUST trigger full rollback; zero partial application. (§2.5)
R13a. Inside a batch the rollback is the FAILED COMMAND's edits, not the batch's: `Bus::finish` rolls the shared transaction back to the mark its `Session` took when it began (`Transaction::rollback_to`, `Session::transaction_mark`). The edits before the mark belong to commands that succeeded and are already journalled; taking them back left a journal describing a document that was no longer there, and a Python script that caught the error went on to finish half of itself. Whether the batch then continues is the runner's decision — JSON and an AI plan abort it whole, Python may catch — never the failed command's. (TODOS F-01)
R14. Topology checks, mevzuat rules and geometry validity MUST run inside `Bus` between dispatch and transact, never in a client. (§2.6)
R15. Argument arity, type and range MUST be declared as `Param` in the spec and checked by `Bus` **before** the command body runs. (§2.3, §2.6)
R16. `parser.hpp` MUST expose exactly one grammar object, used by both the command line and the script engine. (§3, implementation note)
R17. The parser MUST accept absolute `485320,4310220`, relative `@50,30`, polar `@100<45`, inline expressions `@(100*3),0`, and POINT FUNCTIONS `orta(A,B)` · `ile(P,@dx,dy)` · `dik(A,B,ayak,boy)` · `semt(S,açı,kenar)` · `kes(A,açı1,B,açı2)` · `kes(A,r1,B,r2,sol|sağ|yon=<nokta>)` · `kes(A,B,C,D)` · `ara(A,B,oran|mesafe m)` · `uzanti(A,B,mesafe)` · `xy(P,Q)` · `n(nokta_no)` · `son`, and MUST convert to `Mm` int64 fixed point exactly once, at the parse boundary. (§3, TODOS-CAD P1a)
R17a. A point function MUST resolve to one `Point2` BEFORE dispatch, so no `CommandSpec`, validator, body or journal line ever sees a call. A function's argument list MUST be matched against the declared signatures and MUST be refused when more than one fits: a point argument written as bare coordinates spends two comma-separated fields, so arity alone cannot say which shape was meant, and choosing one in silence is what R19 forbids. Where a function needs the document — `n(1284)` — it MUST reach it through a lookup the CALLER supplies (`ResolveContext::named_point`), never through a global; a caller with no document supplies none and the function refuses by saying so. (§3, TODOS-CAD P1a)
R18. Transparent commands (`Flags` transparent, e.g. `core.zoom`) MUST *suspend* the running coroutine and resume it, never cancel or restart it. (§3)
R19. Every failure MUST be returned as `Result<T>` with `Error{code, message}` and the message MUST name expectation and input — e.g. `"Expected: 2 numbers or an object snap. Got: 'abc'"`. (§3)
R19a. A command that DECLINES — the object is the wrong kind, the input names nothing, the edit cannot be made — MUST say so through `Context::refuse`, which fails the session with the sentence as the error. It MUST NOT `echo` the sentence and return: that reports SUCCESS to every client that is not reading the transcript, and a script, an agent and Python then believe the work was done (TODOS F-01 measured 38 such cells). `echo` is for what a command SAYS about work it did or an answer it gives — a listing, a measurement, an empty search result, "nothing to do" when the requested state already holds. A cancel is neither: Esc settles the session as `Cancelled`, and the bus reports it as such. `scripts/ci-gate-kapsam.sh` fails when the measured support matrix shows a silent refusal.
R20. `Journal` (`journal.hpp`) MUST be append-only JSONL, written after commit, and MUST be replayable to reconstruct document state. Exactly two line kinds exist: `{cmd, args, crs, katman}` for a committed command, and `{kind:"meta", ...}` for a non-command record (script sandbox level and consent — `.claude/script.md` R11/R20; plugin id, hash and user decision — `.claude/plugin-api.md` R10). Replay MUST apply the first kind and ignore the second. (§2.2)
R21. Journal writes MUST be enqueued from the calling thread and performed on a dedicated writer thread. (§10.4)
R22. The dispatch hot path MUST be allocation-free: arguments live in a POD union or a per-thread arena. Budgets: script dispatch ≤ 10 µs, command-line keystroke → screen ≤ 30 ms. (§10.4, §10.1)
R23. `TOPLU_BASLA` / `TOPLU_BITIR` MUST collapse N commands into one validation pass and one undo step. (§10.4)
R24. `kentos_command` MUST compile Qt-free and link only `kentos_core`; see `.claude/core.md` for the core purity rules.
R25. Commands owned by `/src/domain` (ifraz, tevhit, yola terk, DOP) MUST be registered through the same `Registry` and obey R1–R23; their domain semantics live in `.claude/domain.md`.

R26. A command's STRUCTURED answer is `Context::report`, surfaced on `DispatchResult::report`; what it SAID is `DispatchResult::lines`, captured per dispatch. A client that is not a person MUST NOT have to parse Turkish prose to learn a count, and `DispatchResult::message` is empty on success — which is why a read command's answer used to be unreachable to anything but the transcript.
R27. `Param` declares its word list (`choices`) and its numeric range, and the BUS validates both before the body runs (Article 1.3). A body MUST NOT be the only place a keyword list exists: the generated schema needs it as an `enum`, the command line needs it to print, and an agent cannot guess it from a help string. `ToolParam` has always carried both and `to_command_spec` used to drop them.

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
P10. NEVER let a command body ask, branch on, or log the `InputSource` kind. (§2.4) What decides behaviour instead is the EMPTINESS OF AN ARGUMENT: `POLİGON` asks for its readings when `aci=` was not supplied and does not when it was, which is "was I told" rather than "am I interactive" — the same body serves a script and a hand without knowing which is which. Enforced by `scripts/ci-gate-tek-yol.sh` over `/src/command/src/commands`, `/src/domain/*/src` and `/src/ai/src/commands`; the BUS may read the origin and does, because journalling who asked is the opposite of behaving differently because of who asked.
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
