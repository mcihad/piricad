# UI Shell — Rules

> Scope: `/src/app` (target `kentos_app`, executable `kentos_cad`) — Qt Widgets shell, command line widget, docking, i18n, accessibility.  |  Depends on: every `kentos_*` target (CLAUDE.md 3.2, `app -> everything`), Qt 6 Widgets/Quick. The canvas half is governed by `.claude/render.md`, the AI panel by `.claude/ai.md`.  |  Source: kentoscad.md §3, §6.2, §6.3, §10.1, §13

## Hard Rules

**Shell & composition (§6.3)**
- **R1** The shell — the ribbon, docking, tables, dialogs — SHALL be Qt Widgets. (This supersedes the earlier "menus, toolbars": R46 replaced both with the ribbon.)
- **R2** The map canvas SHALL be a `QRhiWidget` hosting the `render::Backend` pipeline. The Phase-0 `QPainter` `MapCanvas` is a documented deviation; see `.claude/render.md`. No Qt painting inside the canvas rect.
- **R3** QML via `QQuickWidget` SHALL be used only for: welcome screen, animated property panels. Any additional QML panel needs an explicit entry in this rule. **This supersedes the earlier reading that listed the AI chat panel here**: the maintainer chose Qt Widgets for it, so the chat surface is built from the component set (R29) and painted by the one stylesheet in `theme.cpp` from `tokens.hpp` like every other surface. A QML chat would have been a second styling system, outside `buildComponentSheet` and `scripts/ci-gate-bilesenler.sh`, with the weaker accessibility §6.1 already warns about — and R21/R22 demand full keyboard operation and an `accessibleName` on every widget.
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

**Printing (§6.3, §13)**
- **R33** The PRINT FRAME is view state and nothing else (model.md R43): a sheet-shaped window of the viewport at the profile's printable aspect, the rest dimmed, the map panning and zooming under it exactly as it does with no frame up. It keeps its SIZE ON SCREEN — it is the paper, not a rectangle in the drawing — so zooming changes the scale rather than the frame. It runs no command, touches no document and does not disturb the selection. Esc, the right button, or capturing it puts it away; Enter and the button's second press capture it.
- **R34** The frame MUST show its centre: a cross at the exact middle and that point's `Sağa (Y)` / `Yukarı (X)` reading (model.md R37a). A sheet is placed by its centre, and `YAZDIR merkez=` is the same placement said as a coordinate — so the centre has to be both visible and typeable.
- **R35** THE SHEET'S MEASUREMENTS HAVE ONE HOME: the profile. The preview window shows the paper the chosen profile describes and MUST NOT offer paper, orientation, resolution or margin a second time; it links to the profile editor instead. Two places to set the orientation is two answers to one question, and the one the user did not look at wins.
- **R36** The preview MUST be rendered by the SAME pipeline the plot uses (`render::build_scene` plus the backend, through `PrintService::renderPreview`), at the preview's own pixel count. A window that drew an approximation of the sheet would be a window that lies about the sheet.
- **R37** THE CAPTURED AREA IS THE PRINTED AREA. The preview MUST put on the sheet exactly the box the frame held and MUST send it as `pencere=…` while its scale and centre fields are untouched; both fields REPORT that box until one of them is typed over, after which the area is that centre at that scale and the line says `merkez=… olcek=…`. A round plan scale (1, 2, 2.5 or 5 × a power of ten) is OFFERED by a button beside the scale and MUST NOT be applied on its own. This supersedes the earlier reading that the frame's scale arrives rounded up: rounding up enlarges the area, so the sheet covered ground the user had not aimed at — as much as twice it, from 1:101 to 1:200 — and the window silently overruled the gesture it exists to capture.
- **R38** A password field MUST be `FieldSpec::secret` (dots on screen), MUST NOT be echoed anywhere, and the window MUST show the command line it will send with the password in it — because the line is what is sent — while the COMMAND keeps it out of the journal (io.md P5a).

**The AI surfaces (§5, `.claude/ai.md`)**
- **R41** The chat surface is a DOCK built from the component set, not a window: the preview of a suggestion is drawn on the CANVAS (kentoscad.md §5.1's own diagram puts them side by side), and a modal window over the drawing would hide the thing being judged.
- **R42** Every model output carries the word **öneri** in every state, including loading, error and partial (ai.md R8), and no string from ai.md P4's blocklist appears anywhere near it. The control that applies a suggestion is labelled **Uygula**; "Onayla" is not used, because P4 reserves the vocabulary of approval for a licensed engineer's signature.
- **R43** Reasoning text is shown only when the profile configures it; otherwise an animated indicator says the model is thinking and nothing pretends to quote it. A streamed token appears as it arrives, and the surface stays usable while it does (ai.md R18, P8).
- **R44** The context readout is honest about being an estimate while a turn streams, and settles to the provider's own reported usage when the turn ends. A number presented as exact when it is a guess is worse than no number.
- **R45** The MCP server's state is visible whenever it is running: a status-strip cell naming the port, with a SHAPE as well as a colour (R31) — and when the endpoint has no token it says `KORUMASIZ` in words, because a colour nobody looks at is not a warning. Clicking the cell runs the same command the menu entry runs.

**The ribbon (§6.3)**
- **R46** The shell's commands SHALL be laid out on ONE ribbon — SARibbon's, in its compact three-row layout (`src/app/src/main_window_ribbon.cpp`) — which replaces the menu bar, the tool bar, the left tool column and the document tab strip; there is no second layout of the commands. A panel shows three sizes of button (large, row, picture), a family of tools is ONE split button (`RibbonFamily`), the select tool is the FIRST item of every tab, and every command with no chosen place lands in its tab's "Diğer komutlar" list, generated from `Registry` (5.10). A label shorter than the command's title (a family's word, a processing tool's) is the ribbon's choice of word; the command it runs, its tip and its names still come from the registry (R7).
- **R47** A value the ribbon SHOWS SHALL be READ from the document or a setting on every change and WRITTEN only through a command line: the layer list (`KATMAN` with no selection, `KATMANAT` with one — AutoCAD's rule), the colour boxes (`RENK`), the annotation defaults and the plot scale (`AYAR`), and the editor tabs' boxes (`YAZIDÜZENLE`, `ÖLÇÜDÜZENLE`, `TARAMADÜZENLE`). A box MUST NOT keep a value of its own, and a picture that is data — a hatch pattern, a text anchor — SHALL be drawn from the data (`hatch_swatch`, `anchor_icon`), never stored.
- **R48** An editor tab (a SARibbon context category) SHALL appear for a selection holding its kind while no command is asking for input, and SHALL act on that selection. It is marked by a 3 px accent cap over the folder tab and by nothing else (design.md §2); the text, dimension and hatch editors come forward when the selection is only their kind and hand the ribbon back when it empties, the area and block editors only appear. A tool on one that needs a figure first opens where the figure is typed and says so with `…`.
- **R49** The ribbon wears SARibbon's own Office 2021 theme with its palette filled from `tokens.hpp` (`ribbonStyleSheet`), set on the main window and nowhere else, with SARibbon's runtime theme passes disabled (`RibbonThemeUserDefine`) — one stylesheet, as R29 means it. The tabs are folder tabs on two surfaces (`ribbonTabs`, `ribbonBody`).
- **R50** The application menu (`app_menu.hpp`) SHALL be a popup, not a backstage page over the drawing: the file verbs with one line each, a pane of the recent documents or of the hovered verb's choices, the settings and quit at its foot. The ribbon SHALL stay within 134 px and, with the status strip, within 160 px (`KENTOS_FIT_PROBE`). Every ribbon button SHALL be in the Tab chain, open its list on Down or F4, and offer Press first (Toggle for a tool, ShowMenu for a split button) to the accessibility layer (`KENTOS_ACCESS_PROBE`); every action with a shortcut SHALL be registered on the window, so the shortcut works whichever tab is up.

- **R51** A job in flight (command.md R8b) SHALL show on the status strip as its label and figure (`<label> · %42`), a moving segment and a **Durdur** chip that requests the stop — the road Esc takes. Durdur SHALL always be reachable: where the gap beside the aid chips is too narrow, the job's cell takes the chips' place until it ends, and a click there toggles nothing. Enter and the right button SHALL NOT stop a job. A line or a button pressed while a job holds the drawing SHALL be answered once, in words, without "Hata" (`Controller::sayBusy`, command.md R13g), and a suggestion card's **Uygula** SHALL leave its plan waiting rather than fail it (`SuggestionCard::decide`).

**Panels that take several rows**
- **R39** A panel with multi-selection SHALL act on the WHOLE selection, and a right-click on a row outside the selection acts on that row alone — what every file manager does and what a user expects after right-clicking something they had not selected. How many rows are in play MUST be in the entry's own label, not on a line of its own. A highlight is view state (model.md R43): it changes no document data and never makes anything active.
- **R40** Several commands sent for ONE gesture SHALL go through ONE batch (`Controller::runLines`, which is `Bus::begin_batch`): one undo step for one click, and a refusal part way through rolls the whole gesture back (Article 1.6). A widget MUST NOT loop `runLine` over a selection — eleven undo steps for one click is not what Ctrl+Z means. The lines MUST be lines a script could carry unchanged (P3).

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
- **P15** NEVER let the chat or the suggestion panel load a remote resource, follow a link from model output, or render anything as clickable that the model named. A rendered suggestion fetches nothing off the machine (CLAUDE.md 5.22).
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
