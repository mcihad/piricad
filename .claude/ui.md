# UI Shell — Rules

> Scope: `/src/app` (target `kentos_app`, executable `kentos_cad`) — Qt Widgets shell, command line widget, docking, i18n, accessibility.  |  Depends on: every `kentos_*` target (CLAUDE.md 3.2, `app -> everything`), Qt 6 Widgets/Quick. The canvas half is governed by `.claude/render.md`, the AI panel by `.claude/ai.md`.  |  Source: kentoscad.md §3, §6.2, §6.3, §10.1, §13

## Hard Rules

**Shell & composition (§6.3)**
- **R1** The shell — menus, toolbars, docking, tables, dialogs — SHALL be Qt Widgets.
- **R2** The map canvas SHALL be a `QRhiWidget` hosting the `render::Backend` pipeline. The Phase-0 `QPainter` `MapCanvas` is a documented deviation; see `.claude/render.md`. No Qt painting inside the canvas rect.
- **R3** QML via `QQuickWidget` SHALL be used only for: AI chat panel, welcome screen, animated property panels. Any additional QML panel needs an explicit entry in this rule.
- **R4** Docking SHALL use Qt Advanced Docking System (LGPLv2.1, §9.4) and MUST support tabbed groups, floating windows, and named perspectives saved/restored via `QSettings`.
- **R5** Dear ImGui MUST be confined to a developer debug overlay inside the canvas, behind `KENTOS_WITH_IMGUI` and never in a release build; the option is defined once in `.claude/render.md` R17.

**Command bus client (§2.1, §3)**
- **R6** Every user-visible action SHALL be dispatched as `Bus::dispatch(id, args)` with a `Value` argument list. A toolbar button MUST invoke the identical command id and args a script would.
- **R7** Every toolbar/menu action SHALL derive its label, icon, shortcut and enabled state from the `CommandSpec` in `Registry`. A hand-built action table is BANNED (CLAUDE.md 5.10).
- **R8** Interactive input SHALL be supplied through `InputSource` (mouse variant) so `co_await ctx.point(...)` / `ctx.number(...)` resolve identically for GUI, CLI, script and AI.
- **R9** Undo/redo SHALL only call `core.undo` / `core.redo`; the UI MUST NOT touch `UndoStack` or `Transaction` directly.

**Command line widget (§3) — first-class engineering surface**
- **R10** The command line SHALL resolve bilingual names and abbreviations from `Registry` (e.g. `ÇİZGİ`/`CIZGI`/`LINE`/`Ç`/`L` → `core.line`).
- **R11** It SHALL provide inline autocomplete and live parameter hints derived from `CommandSpec::Param`.
- **R12** It SHALL provide history (Up/Down), reverse search (Ctrl+R), and a transcript dock listing every dispatched command with its result.
- **R13** It SHALL accept mid-command coordinate entry in all three forms: absolute `485320,4310220`, relative `@50,30`, polar `@100<45`.
- **R14** It SHALL accept mid-command modifiers (e.g. typing `ORTA` while `ÇİZGİ` runs = temporary osnap override) without aborting the running coroutine.
- **R15** User aliases SHALL be loaded from `alias.json` in the user config dir; an alias colliding with a `Registry` name MUST be rejected at load with the colliding name in the message.
- **R16** Inline expressions SHALL be evaluated (`@(100*3),0`), by the same evaluator the parser uses.
- **R17** Transparent commands (`core.zoom`, PAN) SHALL suspend and resume the running command; `CommandSpec::Flags` transparency is the only source of that decision.
- **R18** Parsing SHALL go through `kentos_cad/command/parser.hpp` — one grammar shared with the script engine (§3 implementation note); see `.claude/command.md`.
- **R19** Keystroke-to-screen latency SHALL be ≤ 30 ms (§10.1), measured by the bench in `/tests/bench`.
- **R20** Errors surfaced from `Result<T>` / `Error` SHALL name the expectation and the input: "Expected: 2 numbers or an object snap. Got: 'abc'" (§3).

**Accessibility, i18n, platform (§13)**
- **R21** Every operation SHALL be reachable from the keyboard; the app MUST be fully operable with no mouse.
- **R22** Every interactive widget SHALL set `accessibleName` and `accessibleDescription`, and every custom-painted widget SHALL implement `QAccessibleInterface`. Screen-reader announcement (NVDA, VoiceOver, Orca) is verified on the manual release-test checklist, not by a gate.
- **R23** Layout code SHALL contain no pixel literal — sizes in layout units only — and every icon SHALL be SVG or multi-resolution. High-DPI, mixed-DPI multi-monitor and dark-theme correctness is verified on the manual release-test checklist.
- **R24** Every user-visible string SHALL be wrapped in `tr()` and shipped in both `kentos_tr.ts` and `kentos_en.ts` under `/src/app/i18n`.
- **R25** Turkish case conversion SHALL use `QLocale(QLocale::Turkish).toUpper/toLower` (i/İ, ı/I). Numbers, dates and units SHALL be formatted via `QLocale`.
- **R26** Autosave SHALL run on a timer and crash recovery SHALL replay the `Journal` JSONL log through `Bus`, never by reading a private UI snapshot (§13).
- **R27** Cold start SHALL be ≤ 2 s and RAM on an empty project ≤ 300 MB (§10.1); both are bench-gated.
- **R28** All I/O, geometry and parsing work SHALL run off the UI thread and return via `Task<T>` / queued connection.

**Component set (`data/design/bileşen_standardı.png`, `design.md` §11–§13)**
- **R29** Every control in the shell SHALL be an instance of the component set in `widgets.hpp` and `fields.hpp` — `Button` (roles Primary, Secondary, Ghost, Danger, Mode, Icon), `CheckBox`, `RadioButton`, `ToggleSwitch`, `ComboBox` (the one drop-down list, self-drawn — a raw `QComboBox` is a gate failure), `Segment`, `Slider`, `Chip`, `Badge`, `Banner`, `ProgressStrip`, `FormRow`, `FormSection`, `Field`, `DataGrid` (the one table, §9) and `ExpressionEdit` (the one-line filter bar) — sized by `ControlSize` (24 / 30 / 36 px), radius 4, coloured by the one stylesheet from `tokens.hpp`. A role is a constructor argument, never an object name a caller remembers to set.
- **R30** One primary button per screen. A page hosted inside a framed dialog (the settings window, the layer properties window) uses secondaries for its own actions; the frame's footer holds the primary.
- **R31** No state SHALL be told by colour alone: every state carries a shape too — a glyph, a badge's text, a ring, the knob's side — and the keyboard focus ring appears only for keyboard focus (§13).
- **R32** A component added to the set SHALL appear in the living standard (`buildComponentSheet`, opened by `KENTOS_WIDGETS_PROBE`), in `scripts/ci-gate-bilesenler.sh`'s expected inventory and in `docs/baslangic/bilesenler.md`, in the same change.

## Absolute Prohibitions

- **P1** NEVER mutate `Document`, `Layer` or entity data from a widget, slot or event handler. Only a command inside a `Transaction` may.
- **P2** NEVER place business, geodetic, cadastral or validation logic in `/src/app`. Validation belongs to the command layer (§2.6) and the domain modules.
- **P3** NEVER give the GUI a privilege the command bus does not offer — no private entry point, no "GUI-only" argument, no bypass of `Bus`.
- **P4** NEVER call `std::toupper` / `std::tolower` / `QString::toUpper()` without a Turkish `QLocale` on user-visible or user-entered text (BANNED, canon).
- **P5** NEVER block the UI thread: no sleeping, no synchronous file/network I/O, no long loops in a paint or event handler.
- **P6** NEVER build the whole shell in QML; QML is confined to the panels in R3.
- **P7** NEVER ship a feature reachable only by mouse, and never one reachable only by pointing at a canvas.
- **P8** NEVER hard-code a user-visible string outside `tr()`, and never build a sentence by concatenating translated fragments.
- **P9** NEVER show a non-actionable error ("Invalid point", "Error", a raw error code) — R20 is the required shape.
- **P10** NEVER read or write the `Journal`, `UndoStack` or on-disk project format directly from `/src/app`; go through the command/io APIs (`.claude/command.md`, `.claude/io.md`).
- **P11** NEVER `#include` a `/src/domain` internal header in the UI; the UI knows command ids and `Value`, nothing more.
- **P12** NEVER let AI output act on the UI without preview + explicit user approval; see `.claude/ai.md`.
- **P13** NEVER construct a raw `QPushButton`, `QCheckBox`, `QRadioButton`, `QSlider`, `QSpinBox`, `QDoubleSpinBox`, `QProgressBar`, `QGroupBox` or `QDialogButtonBox` outside `widgets.cpp` and `fields.cpp`, and never give a widget a private stylesheet (CLAUDE.md 5.19). The single allowance — the style designer's three numeric property editors — is named in `scripts/ci-gate-bilesenler.sh` with its removal condition and a ceiling that may only fall.

## Definitions of Done

- [ ] New action exists as a `CommandSpec` in `Registry` and is driven from it (R7); a `/tests/unit` UI-free test dispatches it through `Bus`.
- [ ] Reachable and completable by keyboard alone; `accessibleName` set; tab order verified.
- [ ] TR + EN strings present in both `.ts` files; `lupdate` reports no untranslated entry.
- [ ] Turkish case/format paths use `QLocale`.
- [ ] Command line: abbreviation, autocomplete, history, transcript entry and error text checked for the new command.
- [ ] No `Document` mutation outside a command; no new work on the UI thread.
- [ ] Latency / cold-start / RAM benches still within §10.1 budgets.
- [ ] Every control is from the component set and the screen has one primary (R29, R30); a new component is on the living standard, in the gate's inventory and in the manual (R32).

## Enforcement

- `/tests/unit` — UI-free command tests via `Bus`; a command that only works from `/src/app` fails review.
- `/tests/bench` — keystroke→screen ≤ 30 ms, cold start ≤ 2 s, empty-project RAM ≤ 300 MB; >10% regression breaks the build.
- `scripts/ci-gate-bilesenler.sh` — no raw control outside the set (P13, with its one shrinking allowance), every role and every size styled in the one sheet, and the living standard opened by `KENTOS_WIDGETS_PROBE` reporting the standard's own heights and states (R29, R32).
- `scripts/ci-gate-i18n.sh` — translation completeness (`lupdate` + missing-string scan), literal-string scan for un-`tr()`'d user text, pixel-literal scan in layout code (R23), and a `std::toupper|std::tolower` grep over `/src/app`.
- Manual release-test checklist (not a gate): keyboard-only path, screen-reader announcement, dark theme, high DPI (R22, R23).
- `/tests/journal` — autosave/crash-recovery replay of the command journal reproduces the document byte-identically.
