# Test & Quality Gates — Rules

> Scope: `/tests/{unit,golden,bench,fuzz,journal,ai-eval,support}` + the CI gate scripts in `/scripts` | Depends on: `piricad_core`, `piricad_command`, `piricad_script`, `piricad_render`, target `piricad_tests` | Source: piricad.md §7.3, §9.11, §10.1, §10.5, §11 (Faz 0), §13, §14, §16.5

## Hard Rules

R1. Six test kinds, one job each: `unit` = logic, `golden` = bit-identical cross-platform reference output, `bench` = §10.1 budgets, `fuzz` = parsers, `journal` = recorded sessions replayed, `ai-eval` = Turkish request → expected command sequence. A test file MUST live in the directory matching its job (§9.11, §14).
R2. The unit framework is **doctest**, pinned by commit SHA. `/tests/support` holds only what doctest does not do — `PENDING(reason)` for a case an optional dependency makes unrunnable (R8b), and `FAIL_WITH(what, detail)` for a two-part failure message. It MUST stay Qt-free. All tests link into the `piricad_tests` executable.
R3. Every command registered in `Registry` MUST ship a test proving that invoking it from the GUI (`InputSource` mouse), from the command line via `command/parser.hpp`, and from a JSON array via `script/json_runner.hpp` yields (a) an identical `Document` and (b) an identical `Journal` JSONL byte stream. This is the Phase-0 keystone proof (§16.5, §11 Faz 0).
R4. Document equality MUST be asserted over int64 `Mm` fixed-point coordinates, `EntityId` order and `Layer` state — never over rendered pixels or floating-point coordinates.
R5. Golden files MUST match bit-for-bit on Linux, Windows and macOS within the same CI run; passing on fewer than three OSes is a failure (§7.3, §10.5).
R6. Regenerating a golden file, a bench baseline or a journal recording MUST be its own commit carrying the regeneration command, the diff summary and the reason, approved by a reviewer who is not the author.
R7. Every `/tests/bench` case MUST assert against the §10.1 table (≤16 ms frame on 5M polygons, ≤3 s 200 MB DWG, ≤5 s 50M-point LAZ, ≤2 s topological validation of 100k parcels, ≤2 s cold start, ≤30 ms keystroke→screen, ≤10 µs script command dispatch, ≤300 MB RAM on empty project). Budgets are gates, not goals.
R8. A benchmark more than 10% worse than the stored baseline MUST break the build (§10.1), provided the slowdown also exceeds that run's own sample spread — a relative threshold alone reports scheduler jitter as a regression on sub-microsecond cases.

R8a. A baseline MUST be scoped to the machine that recorded it (CPU, compiler, optimisation level). A baseline copied between machines measures the machine, not the code, and the regression gate MUST disable itself rather than compare across machines. Budgets (R7) are absolute and are checked regardless.
R8b. A §10.1 budget that cannot be measured yet MUST still appear in the report as BEKLEMEDE with the reason. A budget that quietly disappears is a budget nobody is accountable for.

R8c. Timing is **Google Benchmark**'s job; the gate is ours. A scenario declares its budget, its unit and its repetition count, and the harness compares. A body that MUTATES its fixture, writes a file, or costs a second on its own MUST pin `iterations`: left to the library it runs thousands of times, and what it then measures is not what the scenario describes. Teardown that must stay out of the measurement MUST be released explicitly inside `state.PauseTiming()` — a destructor at the closing brace runs after `ResumeTiming()` and is timed.
R9. Every parser — DXF, DWG, GML/PlanGML, LAS/LAZ, native format, `command/parser.hpp`, JSON script — MUST have a libFuzzer/AFL++ target in `/tests/fuzz` running continuously; file reading is the largest attack surface (§13).
R10. Every fuzz crash MUST be minimised and committed as a `/tests/unit` or `/tests/golden` regression case before the fix merges.
R11. `/tests/journal` MUST hold recorded real sessions as JSONL; the nightly job replays each through `Bus` and compares the resulting `Document` and the re-emitted `Journal` against the stored expectation, ignoring `{kind:"meta"}` lines (`.claude/command.md` R20) (§14).
R12. A journal replay mismatch MUST fail the nightly job and block the next release; it is closed only by a fix or by a re-record under R6.
R13. `/tests/ai-eval` MUST hold 200–300 Turkish request → expected command-sequence cases covering ifraz, tevhit, ihdas, irtifak, yola terk, cins değişikliği, DOP, TAKS/KAKS, pafta, aplikasyon, röper krokisi (§14, §5.6); CI MUST record the success rate per run and fail on a drop below the stored baseline. Model and prompt rules: see `.claude/ai.md`.
R14. ai-eval MUST assert only on emitted command ids and `Value` arguments; any coordinate in an expectation MUST come from the fixture `Document`, never from model text (§5.2).
R15. ASan, UBSan and TSan MUST run as three separate CI jobs over the full unit + golden suite (§9.11, §14).
R16. Every new hot path MUST carry Tracy zone markers, and any diff touching a file covered by a `/tests/bench` case MUST update that case's recorded baseline number in the same PR (§10.5).
R17. Every bug fix MUST land together with the regression test that would have caught it, in the directory matching its test kind, referencing the issue id.
R18. **Release gate, not a per-change rule.** Crash-free session rate MUST be measured per release against a > 99.5% target, and every Crashpad report MUST be triaged: duplicate, or converted into a regression test (§13).
R19. Tests MUST be deterministic: fixed RNG seeds, injected clock, explicit `Crs`, explicit locale, sorted directory iteration, temp dirs created per test.
R20. A test exercising `/src/core` or any other Qt-free target MUST compare Turkish text through the `/src/command` folding table and MUST NOT include a Qt header; a Qt-linked test MUST use `QLocale(QLocale::Turkish)` casing. `std::toupper`/`std::tolower` on Turkish text is banned everywhere (CLAUDE.md 5.6, §13).
R21. `make test` MUST cover unit + golden + ai-eval and complete within the <20 min-per-PR budget on the 3 OS × (Debug + Release) matrix (§14).
R22. Each CI gate script in `/scripts` MUST have a `/tests/unit` case feeding it a known-violating fixture and asserting a non-zero exit; an untested gate is an absent gate.

## Absolute Prohibitions

P1. NEVER commit a failing, disabled or skipped test without a linked issue id in the skip reason.
P2. NEVER assert floating-point equality without an explicit tolerance and a one-clause documented reason on the same line; NEVER widen a tolerance to hide a determinism bug (§7.3).
P3. NEVER let a test depend on wall-clock time, timezone, system locale, filesystem iteration order, hostname, environment leakage or network access.
P4. NEVER add a retry, `--repeat-until-pass`, an expected-fail marker or a quarantine list to the test runner. A flaky test is skipped only under P1, with its issue id.
P5. NEVER delete or overwrite a golden file to make a test pass; regenerate only under R6.
P6. NEVER land a change for performance without a `/tests/bench` number in the PR body (§10.5, R16).
P7. NEVER relax, raise or delete a §10.1 budget to make a bench pass.
P8. NEVER build test binaries with `-ffast-math`, `/fp:fast` or `-ffp-contract=fast`, and NEVER with FP flags differing from the product build (see `.claude/build.md`).
P9. NEVER mutate a `Document` from a test outside a command dispatched through `Bus` — tests are just another client of the command bus, with no privileges (§2.1); see `.claude/command.md`.
P10. NEVER call a live LLM provider or any network endpoint from ai-eval in CI; use recorded provider fixtures.
P11. NEVER `#include <Q...>` in a unit test that exercises `/src/core`; see `.claude/core.md`.
P12. NEVER add test-only hooks (`#ifdef TESTING`, friend-for-test, exported internals) to `/src`; test through the public API and the command bus.

## Definitions of Done

- [ ] New command: GUI + command line + JSON script test asserts identical `Document` **and** identical `Journal` (R3).
- [ ] Golden outputs verified bit-for-bit on Linux, Windows, macOS (R5).
- [ ] Touched hot path: Tracy zone added, bench within §10.1 budget and within 10% of baseline (R7, R8, R16).
- [ ] Parser change: fuzz target updated, corpus seeded, clean continuous run (R9).
- [ ] Bug fix: regression test present with issue id (R17).
- [ ] ASan, UBSan, TSan jobs green (R15).
- [ ] No new skipped test; `make test` and `make bench` green in Debug and Release.

## Enforcement

- `make test` — `piricad_tests` over unit + golden + ai-eval; blocks merge.
- `make bench` — Google Benchmark vs stored baselines; >10% regression = build failure (§10.1).
- Nightly journal replay job — `/tests/journal` through `Bus`, `Document` + `Journal` diff (§14).
- Sanitizer matrix — ASan, UBSan, TSan as three separate CI jobs (§9.11).
- Continuous fuzzing job — libFuzzer/AFL++ over every parser target; any crash is a release blocker.
- Every script in the `scripts/ci-gate-*.sh` glob (`.claude/build.md` R24) has a `/tests/unit` violating-fixture case asserting a non-zero exit (R22).
- Crashpad dashboard — crash-free session rate per release, > 99.5%; release gate only, never a PR gate (R18, §13).
