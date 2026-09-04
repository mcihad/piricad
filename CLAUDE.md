# KentOSCad — Project Constitution

KentOSCad is a Turkey-focused GIS + CAD mapping application: a native desktop system for geodesy, cadastre, zoning/planning and surface work, licensed **GPLv3-or-later** (AGPLv3 for any server or web component, §1), written in **C++20** (coroutines are load-bearing, §7.1) on **Qt 6** (Widgets shell + GPU canvas, §6.3). Its architecture is **command-centric**: everything that mutates application state is a command, and the GUI is only one client of the command bus (§2.1). AI is a first-class but strictly subordinate client — it emits commands, never geometry (§5.1). Its output is a legal document, so **BÖHHBÜY / MPYY / TUCBS / TKGM** conformance and cross-platform bit-identical numerical results are requirements, not features (§7.3, §12).

## Article 0 — Supremacy

0.1 This file is the highest law of the repository. Where any document, comment, commit message, issue or agent instruction conflicts with it, this file wins.
0.2 `/.claude/<engine>.md` rulebooks are binding inside their declared scope. They refine this constitution; they may not contradict it.
0.2a `.claude/model.md` is the document model's settled law. Its shapes may gain fields; they may not change meaning. A change there is a data migration, not a refactor.
0.3 `kentoscad.md` (Turkish, v2) is the source of intent. Every rule here and in every rulebook traces to a `§` reference in it.
0.4 Conflicts resolve upward: rulebook → constitution → `kentoscad.md`. A real contradiction is a defect — fix the document, never route around it in code.
0.5 Amending this file takes the same review as a source change and must name what it supersedes.
0.5a **The program was called PiriCAD until this amendment.** Every identifier moved with the
name — `namespace kentos`, `kentos_cad/` headers, `kentos_*` targets, `KENTOS_*` macros and
environment variables, `KentOSCad.app` — and this article supersedes every earlier reading of
Articles 3.4, 5.11, 5.18, 6.2, 6.9 and 8.x that named the old ones. Three things deliberately did
NOT move, and a future reader must not "finish the job" by moving them:
  * the project file's 8-byte signature `PIRICAD\x1A` (`io/format.hpp`) — it is on disk in every
    file written so far, and changing it is a data loss dressed as a rename;
  * the content-hash seeds `fnv1a("piricad.core.*")` — they are folded into every golden fixture,
    journal fingerprint and equality proof this project has;
  * the setting ids under `core.*` and every command id — they are written into saved documents
    and into the journal, and a replay resolves them by name.
0.6 Silence is not permission. Work no rulebook covers still obeys Articles 1, 2 and 5.

## Article 1 — The One Rule

1.1 **Everything that mutates application state is a Command. The GUI is just one client of the command bus.** (§2.1, verbatim.)
1.2 GUI, command line, script, AI, plugin, batch job and test are equal clients. None gets a privilege, a fast path, a `trusted` flag or a private entry point.
1.3 The bus order is fixed and complete: **dispatch → validate → transact → journal** (§2.1, §2.6).
1.4 A command invocation is data, not a function call: fully expressible as `Value`, round-tripping losslessly through JSONL `{cmd, args, crs, katman}` (§2.2).
1.5 One command = one `Transaction` = one `UndoStack` entry, unless the `CommandSpec` declares another `UndoPolicy` (§2.5).
1.6 Any validation failure rolls the entire transaction back. A half-applied ifraz, tevhit or 18. madde parcelation is a build-blocking defect (§2.5).
1.7 `Registry` is the single source of truth for commands. CLI help, script bindings, AI tool schema, menu/toolbar actions and docs are **generated** from it (§2.3).

## Article 2 — Non-Negotiable Decisions

| # | Decision | Ruling | Reason |
|---|---|---|---|
| 2.1 | License | GPLv3-or-later; AGPLv3 for server/web components | CGAL, LibreDWG, Qt Charts and every Apache-2.0 dependency are GPLv3-compatible and GPLv2-only-incompatible (§1) |
| 2.2 | Language standard | C++20 baseline; C++23 only behind `#if __cpp_lib_*` with a compiled, tested C++20 fallback | Coroutines carry the interactive command model (§7.1, §2.4) |
| 2.3 | UI toolkit | Qt 6 — Widgets shell, `QRhiWidget` canvas, QML only for the panels named in `ui.md` R3 | Only toolkit with the widget depth, i18n, accessibility and RHI abstraction this product needs (§6.2, §6.3) |
| 2.4 | Coordinate storage | `Mm` = `int64` fixed-point millimetres; `Point2`/`Box2` hold `Mm` members only | Exact and deterministic; `double` is a transient local, never a stored format (§10.2) |
| 2.5 | Floating point | `-fno-fast-math -ffp-contract=off` (MSVC `/fp:precise`) on every TU in every config, Debug and ASan included | FMA does not round the intermediate; x86 and Apple Silicon must agree bit-for-bit in official documents (§7.3) |
| 2.6 | Turkish-first | `.names` = Turkish primary, ASCII-folded Turkish, English, abbreviations (`ÇİZGİ, CIZGI, LINE, Ç, L`); casing via `QLocale` | The users are Turkish surveying engineers and planners; the Turkish domain language is the product (§5.6, §13) |
| 2.7 | Dependencies | A mature, excellent, cross-platform library is used; it is never reimplemented. Hand-rolled code is permitted only where no such library exists, or where §7.1 names the hand-rolled version as the decision | Reimplementing a solved problem costs correctness, portability and every bug the library already fixed. "Multiplatform" is part of the test: a Linux-only package is not a candidate (§9) |
| 2.8 | AI scope | AI emits commands only — preview, explicit approval, one undo step, full audit record; coordinates only from tool-call results | Cadastral and zoning output is a legal document that only a licensed engineer can sign (§5.1, §5.2). The command set is also the AI's TRAINING SURFACE: the goal is an engineer who states the work in Turkish and gets commands that draw and analyse it, so a capability that exists only as a mouse gesture is a capability the AI can never be taught (§5.1, Article 1.2) |
| 2.9 | Spatial database | **PostGIS is a first-class store, not an export target.** Read, write and edit against a live PostGIS connection, through libpqxx (BSD-3), behind `KENTOS_WITH_POSTGIS` | Turkish municipalities and TKGM run their corporate data on PostGIS; a CAD/GIS program that can only import a dump cannot sit in that workflow. GDAL's PG driver is the fallback, never the design: it cannot express a transaction that spans a command, and Article 1.6 requires an ifraz to roll back whole (§9.3, §12) |

## Article 3 — Module Map and Dependency Direction

3.1 The repository tree is fixed (§8). Adding a top-level directory requires amending this article.

```
/cmake  /scripts  /packaging  /docs
/src   core  command  io  render  script  ai  domain{geodesy,cadastre,planning,surface}  app  plugin-api
/data  catalogs (BÖHHBÜY codes, MPYY symbology — DATA, never code)  crs (geoid grids, transform params)  corpus (legislation + index)
/tests unit  golden  bench  fuzz  journal  ai-eval  support
```

3.2 The arrows are strict and one-way (§8):

```
core       -> nothing (not even Qt)
command    -> core
io         -> core, command
domain     -> core, command
render     -> core  (+ Qt Gui when the QRhi backend lands, Article 8.1)
script     -> command
ai         -> command
app        -> everything, Qt Widgets/Quick
plugin-api -> stable C ABI only (core types by value, bus via handle)
```

3.3 **A reverse or lateral dependency is a build failure, not a review comment.** `#include <Q...>` under `/src/core`, `/src/command`, `/src/script`, `/src/io`, `/src/domain` or `/src/ai` breaks the build; so does direct geometry mutation outside a command under `/src/domain` (§8 CI gates).
3.4 Qt-free targets: `kentos_core`, `kentos_command`, `kentos_io`, `kentos_script`, `kentos_ai`, the four `kentos_domain_*`, `kentos_plugin_api`, and `kentos_render` for as long as Article 8.1 holds. Qt-linked: `kentos_app` (Widgets), and `kentos_render` once the QRhi backend lands.
3.5 `/data` holds data, schemas and manifests only — no C++, shell, Python or Lua, and no compile-time embedding of a catalogue or grid.

## Article 4 — Rulebooks

| Rulebook | Scope | Governs |
|---|---|---|
| `.claude/core.md` | `/src/core` | Qt-free purity, `Mm` fixed point, SoA layout, arenas, `Result<T>`, determinism, Shewchuk predicates |
| `.claude/command.md` | `/src/command` | `Bus`, `Registry`, `CommandSpec`, `Value`, `Task<T>` coroutines, `Context`/`InputSource`, transactions, journal, the one parser |
| `.claude/io.md` | `/src/io`, format tests | GDAL/DWG/DXF/GML/LAZ wrappers, PostGIS connections and their transactions, mmap-able native format, versioning, untrusted-input handling, fuzzing |
| `.claude/render.md` | `/src/render`, canvas half of `/src/app` | `render::Backend`, origin offset, precomputed LOD, GPU line widening, render thread, 16 ms gate |
| `.claude/script.md` | `/src/script` | JSON runner today, Lua/Python later, sandbox levels, read-only bindings, batch merge, 10 µs dispatch |
| `.claude/ai.md` | `/src/ai`, AI panel, `/tests/ai-eval`, `/data/corpus` | Suggestion pipeline, approval, audit records, coordinate-hallucination defence, local models, mevzuat RAG citations |
| `.claude/domain.md` | `/src/domain/*` | Geodesy, cadastre, planning, surface semantics; catalogue-driven regulation; TUREF/TM3; ifraz/tevhit/DOP/PlanGML |
| `.claude/data.md` | `/data/*` | Catalogue versioning and schemas, CRS provenance, corpus chunk metadata, data permits, LFS/size policy |
| `.claude/ui.md` | `/src/app` | Widgets shell, command line widget, `Registry`-driven actions, i18n, accessibility, latency and cold-start budgets |
| `.claude/plugin-api.md` | `/src/plugin-api` | C99 ABI, version handshake, additive-only evolution, capability sandbox, signing, crash isolation, GPLv3-compatible licences |
| `.claude/build.md` | CMake, presets, Makefile, vcpkg, packaging, CI | Presets and targets, compiler floors, flag strings, pinned deps, warnings-as-errors, SBOM, reproducible signed releases |
| `.claude/model.md` | the document model, everywhere it is stored or rendered | Slot vs key identity, the closed cull block, ring geometry, the interned style column, units, kind registration, attributes, layers, catalogue provenance, CRS + epoch, setting scopes |
| `.claude/docs.md` | `/docs` | Turkish user manual, per-command pages, generated reference, examples that run, glossary, dead-link and TODO bans |
| `.claude/test.md` | `/tests/*`, `/scripts` gates | Six test kinds, the GUI=CLI=script equality proof, golden bit-identity, bench gates, fuzzing, sanitizers, determinism |

## Article 5 — Absolute Prohibitions

Project-wide. A violation is a build failure or a merge block, never a discussion.

5.1 NEVER `-ffast-math`, `/fp:fast`, `-ffp-contract=fast`, or any per-target override of the Article 2.5 flag strings (§7.3).
5.2 NEVER C++20 modules — no `import`, no `export module`, no `FILE_SET CXX_MODULES`. Header + PCH only (§7.1).
5.3 NEVER `#include <Q...>`, `Q_OBJECT` or a Qt type in a Qt-free target (Article 3.4).
5.4 NEVER link or vendor **Triangle** (Shewchuk triangulator) — GPL-incompatible; use CDT/CGAL/Clipper2. Shewchuk `predicates.c` alone is permitted and required (§9.2).
5.5 NEVER link, vendor or optionally depend on the **ODA Drawings SDK**, and never a GPLv2-only dependency (§9.8, §1).
5.6 NEVER `std::toupper`/`std::tolower`/`<cctype>` classifiers on Turkish text (i/I dotted–dotless). Use `QLocale(QLocale::Turkish)` at the Qt layers, or the shared Turkish folding table in `/src/command` (§13).
5.7 NEVER auto-apply AI output. No trust mode, setting, CLI flag or "remember my choice" that bypasses preview + explicit approval (§5.2.1).
5.8 NEVER let a coordinate originate in model text; every `Mm`/`Point2` in a generated command traces to a recorded tool-call result (§5.2.5).
5.9 NEVER mutate `Document`, `Layer` or any entity store outside a command executing inside a `Transaction` — from UI, render, io, script, plugin, AI or test (§2.1, §2.5).
5.10 NEVER hand-maintain a second command list: no menu table, CLI table, AI tool JSON, docs table or binding table that is not generated from `Registry` (§2.3).
5.11 NEVER add a second parser, lexer, grammar or expression evaluator; `kentos_cad/command/parser.hpp` is the only one, shared by command line and script engine (§3).
5.12 NEVER add an unpinned dependency: no floating vcpkg baseline, no branch ref, no `FetchContent` without a commit SHA, no `find_package` without a minimum version, no unrecorded LICENSE (§8, §9.11).
5.13 NEVER hard-code a regulatory value in C++ — no detail code, gösterim, symbol, colour, TAKS/KAKS row, threshold or TUCBS theme id as literal, `constexpr` or enum (§15).
5.14 NEVER silence a warning (`-Wno-*`, `#pragma warning(disable)`, `/WX-`, `-w`), and never merge on a red CI or with `continue-on-error` on a gate (§9.11, §14).
5.15 NEVER give one client a capability another lacks, and never ship a feature reachable only by mouse (§2.1, §13).
5.16 NEVER hand-roll what a mature, cross-platform library already does well — JSON, testing, benchmarking, spatial indexing, Unicode casing, logging, formatting, hashing, compression, geometry predicates, polygon boolean, triangulation, coordinate transformation, format I/O, linear algebra. Reach for `/vcpkg.json` or a pinned `FetchContent` entry first, and justify in the PR why a hand-rolled version is the exception (Article 2.7, §9).
5.17 NEVER ship a user-facing feature — command, script API, sandbox level, file format, CLI flag, panel or dialog — without its Turkish Markdown page under `/docs`, linked from `docs/README.md`. Undocumented is unshipped (Article 11).
5.18 NEVER hand-edit `docs/komutlar/referans.md`, and never hand-write a second command or parameter table anywhere in `/docs` — it is generated from `Registry` by `kentos_docgen` (§2.3).

## Article 6 — Definition of Done

A change is finished only when every clause holds.

6.1 `cmake --preset release && cmake --build --preset release` is clean; `make check` is green locally; the CI matrix (3 OS × Debug/Release, plus the ASan/UBSan and headless jobs) is green. CI is triggered manually, so a green matrix is something the change's author asks for and waits on — not something that happens to the branch later.
6.2 `make check` is green: every `scripts/ci-gate-*.sh`, the full build, `kentos_tests`, and clang-format. clang-tidy and IWYU run when installed and are skipped with a printed notice when they are not — a skipped tool is reported, never silently passed.
6.3 Zero warnings, zero new suppressions in the diff.
6.4 A new or changed command carries the equality proof: identical `Document` **and** byte-identical `Journal` JSONL from GUI, command line and JSON script; plus a journal-replay case reproducing the golden document (§16.5, §11 Faz 0).
6.5 Numeric or geometric change: `/tests/golden` fixture added or updated and bit-identical across Linux, Windows and macOS in the same CI run (§7.3, §10.5).
6.6 Benchmarks are within the Article 7 budgets and within 10% of the stored baseline; a touched hot path has a Tracy zone (§10.1, §10.5).
6.7 Parser or format change ships its libFuzzer harness and seed corpus in the same PR (§13).
6.8 Bug fix ships the regression test that would have caught it, in the directory matching its test kind.
6.9 User-visible strings are `tr()`-wrapped and present in `kentos_tr.ts` and `kentos_en.ts`; keyboard path and `accessibleName` verified (§13).
6.10 Docs updated: `/docs/api-stability.md` on any `/src/plugin-api` change; `/NOTICE` and the CycloneDX SBOM regenerated on any dependency change (§13, §9.11).
6.11 Regulatory change carries a domain-expert (harita mühendisi / şehir plancısı) sign-off on the PR (§16.9).
6.12 The `/docs` page for every touched user-facing behaviour is written or updated in the same change, `make reference` has been run, and `scripts/ci-gate-docs.sh` is green. A feature without its page is not finished (Article 11).

## Article 7 — Performance Budgets (§10.1)

CI gates, not aspirations. A benchmark more than **10% worse than the stored baseline breaks the build**, and a budget is never relaxed to make a bench pass.

| Scenario | Gate |
|---|---|
| Pan/zoom, 5M-polygon cadastral layer | ≤ 16 ms/frame |
| Open 200 MB DWG | ≤ 3 s |
| First paint, 50M-point LAZ | ≤ 5 s |
| Topological validation, 100k parcels | ≤ 2 s |
| Application cold start | ≤ 2 s |
| Command-line keystroke → screen | ≤ 30 ms |
| Command dispatch from script | ≤ 10 µs |
| RAM on empty project | ≤ 300 MB |

## Article 8 — Phase-0 Deviations

Three deviations from this constitution exist today. Three more were removed when their condition was met: `Document` now sits on `RingGeometry`, `StyleTable`, `LayerTable` and the `EntityKey` column; the cull test reads only the R7 flags byte and the bounding box; and **logging has moved to spdlog outside `/src/core`** (this supersedes the former row 8.4, whose stated condition was exactly that move — `core/src/log.cpp` is now `command/src/log.cpp` backed by spdlog, and core keeps no sink at all). Each is documented, time-boxed and removed on its stated condition. Nothing else may be added to this list without amending it.

| # | Deviation | Why | Removal condition |
|---|---|---|---|
| 8.1 | Mostly lifted, and this row supersedes the earlier reading that the QRhi backend is "OFF by default". **Both options now default ON wherever their toolchain is found** (`cmake/KentOSCadOptions.cmake` probes for Qt Shader Tools, `<rhi/qrhi.h>`, FreeType and HarfBuzz; a machine without them still configures and gets the `QPainter` canvas, and asking for ON without them is still a hard error naming the package). The question this row was written to answer has been MEASURED — 40 000 parcels, same scene, same machine, median of 20 frames, backend share only: **QRhi 1.10 ms, built-in QPainter 22.41 ms, QGIS 72.63 ms**, against a §10.1 budget of 16 ms. Everything the picture needs is on the GPU: all eleven symbol layer types of `/data/catalogs/mpyy-vektor`, the three published picture types, the R8 SDF text atlas, and the lone-vertex point marker. What remains of the row is the LAST half of its condition | Delete the QPainter backend by end of Phase 1 (`render.md` P3). Until then it stays reachable with `-DKENTOS_WITH_RHI=OFF`, because a port settles faster with the thing it replaced still runnable beside it |
| 8.2 | Partly lifted. **Linked and on**: Qt 6, nlohmann/json, spdlog + fmt, doctest, Google Benchmark, GDAL, PROJ, libpqxx (Article 2.9). **Linked behind an option**: Lua 5.4 + sol2 (`KENTOS_WITH_LUA`, Article 8.3). **Pinned but not linked**: xxHash, Clipper2, CDT. **Installed but not linked**: GEOS, CGAL, libxml2. **Absent**: Python | Article 2.7 now requires the mature library wherever one exists, so this row shrinks with every integration rather than expiring at once | Each remaining library stays behind `KENTOS_WITH_<NAME>`, hard-failing with an actionable message when ON but missing, and defaulting ON once found. The row is deleted when GDAL, GEOS and CGAL are linked. A gated test reports **pending**, never passing (`data.md` Enforcement) |
| 8.3 | Half lifted. **Lua has landed**: `script/lua_runner.hpp` runs a Lua 5.4 chunk through sol2 behind `KENTOS_WITH_LUA`, dispatching through the same `Bus` and the same `Parser`, and `BETİK` picks the host by file extension. `KENTOS_WITH_PYTHON` still hard-fails. This supersedes the row's former condition "Lua lands in Phase 2 behind `KENTOS_WITH_LUA`", which is now met | Python (pybind11) is the ecosystem layer of §4.2 and ships as a separate downloadable module, never in the base installer | The row is deleted when Python lands behind `KENTOS_WITH_PYTHON` as an optional module. Neither host is a precedent for a second grammar: both go through `kentos_cad/command/parser.hpp` (CLAUDE.md 5.11) |


8.5 Because of 8.1, `kentos_render` links no Qt today: the scene builder, view transform and backend interface are Qt-free, and the `QPainter` implementation of `render::Backend` lives in `/src/app`. The QRhi backend restores the `render -> Qt Gui` edge of Article 3.2.

8.6 Every rule in every rulebook is written against the target design and binds the stand-in too. A rule is waived for a stand-in only by a row in the table above naming that rule id.

## Article 9 — Working Agreement

**To add or change a command** — read `.claude/command.md`.
1. Declare it once with `KENTOS_COMMAND`; set `.id` (stable, lowercase, namespaced), `.names` (Turkish, ASCII-folded, English, abbreviations), `.category`, `.params`, `.undo`, `.flags`, `.summary`.
2. Write the body as a `Task<T>` coroutine taking input via `co_await ctx.point(...)` / `ctx.number(...)`; never branch on `InputSource`.
3. Declare argument arity, type and range as `Param` so `Bus` validates before the body runs.
4. Regenerate CLI help, script bindings, AI schema and docs from `Registry` — never hand-edit a file to match.
5. Land the equality proof (Article 6.4), a cancellation test with an empty undo delta, and a `Value` round-trip test.

**Before writing any non-trivial algorithm** — ask whether a mature library already does it.
1. Check §9 of `kentoscad.md`: it already names the chosen library for most problems this product has.
2. The test is three-part: **mature** (used in production by others, maintained), **excellent** (the best available answer, not merely the first), and **cross-platform** (Windows, macOS and Linux — a Linux-only package fails).
3. If one exists, use it. If none does, say so in the PR and in a comment on the hand-rolled code.
4. `kentos_cad/command/task.hpp` is the standing exception, because §7.1 makes writing it the decision.

**To add a dependency** — read `.claude/build.md`, and `.claude/io.md` if it is a format library.
1. Read its LICENSE first and confirm GPLv3 compatibility; GPLv2-only, Triangle and ODA SDK are refused outright.
2. Pin it in `/vcpkg.json` with an exact `version>=`/`overrides` entry against the pinned `builtin-baseline`.
3. Gate it behind `KENTOS_WITH_<NAME>`, default OFF, hard-failing with the package, port and install command when ON but missing.
4. Record it in `/NOTICE` and regenerate the CycloneDX SBOM in the same PR.
5. Keep its headers inside the owning module's `.cpp` files, behind pimpl or forward declarations.

**To change a regulatory rule** — read `.claude/data.md` and `.claude/domain.md`.
1. Change the catalogue under `/data/catalogs` or `/data/crs`, never C++. A legislation update is a data release, not a rebuild.
2. Bump `package_version`, keep `schema_version`, `source` (regulation + annex + madde), `published` and `licence` complete; validate against the schema in `/data/catalogs/schema/`.
3. Add the `CHANGELOG.md` line naming the regulation or genelge that caused it, in the same commit.
4. Never delete or reuse a retired id — mark `deprecated: true` with `valid_until`; retain superseded text with `valid_from`/`valid_until`.
5. Add a `/tests/golden` case with TKGM/official reference values and obtain domain-expert sign-off.

**To touch AI behaviour** — read `.claude/ai.md`.
1. Expose a command to AI only by setting `Flags::AiAccessible` in its `CommandSpec`; the tool catalogue is generated from `Registry`.
2. Keep the pipeline intact and in order: read tools → model → validation → preview → user approval → one transaction → audit record.
3. Prove every coordinate traces to a tool-call result handle; reject and audit-log anything else before validation.
4. Write the audit record for rejects as well as approves, with all six fields, and never with credentials in it.
5. Extend the `/tests/ai-eval` Turkish set for any new phrasing or domain term; a drop below the stored accuracy baseline breaks the build.
6. Label output "öneri" everywhere; never render AI text as an approval, a check, or a signature.

## Article 10 — Build and Run

Intent only — `.claude/build.md` is the law here and a separate agent owns `/Makefile`. The Makefile is a thin wrapper: one documented line per target calling `cmake --preset`, `cmake --build --preset`, `ctest --preset` or a `/scripts` script. **No build logic in the Makefile** — no flags, no OS conditionals, no source lists.

| Target | Intent |
|---|---|
| `make setup` | Configure the default preset; resolve vcpkg manifest dependencies |
| `make build` | Build every `kentos_*` target through the configured preset |
| `make run` | Launch the `kentos_cad` executable from the build tree |
| `make test` | `kentos_tests` over unit + golden + ai-eval; blocks merge |
| `make bench` | `/tests/bench` against stored baselines; >10% regression fails |
| `make check` | format + tidy + IWYU + every `scripts/ci-gate-*.sh` + `kentos_tests` — the pre-push gate |
| `make format` | Apply `.clang-format` in place |
| `make doctor` | Report toolchain, Qt, generator and optional-dependency status with actionable fixes |
| `make clean` | Remove build trees; never touch `/data` or the source tree |
| `make help` | List every target — the authoritative, self-describing index |

CMake presets are the only sanctioned build entry points, and they are platform-independent because Ninja is the only generator on all three platforms:

| Preset | Purpose |
|---|---|
| `dev` | Default. RelWithDebInfo, application and tests on |
| `debug` | Debug build, used by the CI test job |
| `release` | Optimised, tests off, the packaging input |
| `asan` | AddressSanitizer + UndefinedBehaviorSanitizer, its own CI job |
| `headless` | Core, command, script and tests without Qt — proves Article 3.3 |

`make PRESET=<name> <target>` selects one. Ad-hoc `cmake -S -B` lines are not sanctioned.

## Article 11 — Documentation

11.1 **Everything a user can do is documented in `/docs`, in Markdown, in Turkish, to publishable quality — the command system included.** This is a hard rule, not a preference: an undocumented feature is an unshipped feature (§13).
11.2 `/docs` is the single home of user documentation. `README.md` at the repository root is a signpost into it; directory `README.md` files are one-line pointers. Nothing else.
11.3 `docs/README.md` is the index and links every page. An orphan page or a dead link fails the build.
11.4 Every command in `Registry` has a page at `docs/komutlar/<slug>.md` carrying all eight sections named in `.claude/docs.md` R7, and shows the command invoked from the command line, from the GUI and from a script — because those three are equal clients (Article 1.2).
11.5 `docs/komutlar/referans.md` is generated by `kentos_docgen` from `Registry` and regenerated in the same commit as any registry change. Hand-editing it is a defect (Article 5.18).
11.6 Every example in the manual runs exactly as printed. Every error message a user can hit is listed with its cause and its fix.
11.7 Every regulatory statement cites its regulation, annex, madde and publication date (§5.5).
11.8 Behaviour that does not exist yet is written in the future tense and names its phase. Aspirational present tense is forbidden.
11.9 The audience split is absolute: `/docs` is Turkish and tells a user how to do their work; `CLAUDE.md` and `.claude/` are English and tell a contributor what the rules are; `kentoscad.md` is the source of intent. Never mix two of these in one file.
11.10 Enforcement is `scripts/ci-gate-docs.sh`, wired into `make docs`, `make check` and `ctest`.
