// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the main window.
//
// Layout (Qt Widgets shell, kentoscad.md §6.3):
//
//   ┌ menu ─────────────────────────────────────────────────────────────┐
//   │ ┌──────┐ ┌──────────────────────────────┐ ┌────────────────────┐ │
//   │ │ Araç │ │  Harita canvas'ı             │ │ Katmanlar │ Öznit. │ │
//   │ │ kutu │ │                              │ │  (sekmeli)         │ │
//   │ │ sürü-│ │                              │ │                    │ │
//   │ │ lebi-│ ├──────────────────────────────┤ │                    │ │
//   │ │ lir  │ │  Komut satırı                │ │                    │ │
//   │ └──────┘ └──────────────────────────────┘ └────────────────────┘ │
//   │ ┌ Transkript │ Komut Günlüğü (sekmeli) ───────────────────────── ┐│
//   └ durum çubuğu ─────────────────────────────────────────────────────┘
//
// Every dock is movable between the left and right edges and can be floated.
// Qt Advanced Docking System replaces QDockWidget in Phase 1 for saved
// perspectives (kentoscad.md §6.3, .claude/ui.md).
#pragma once

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/units.hpp"

#include <vector>

#include <QMainWindow>
#include <QPointer>

/// Qt widgets this header only holds pointers to. Forward-declared rather than
/// included so that touching a widget's header does not rebuild everything that
/// includes the main window.
class QActionGroup;
class QComboBox;
class QStackedWidget;
class QFrame;
class QDockWidget;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QToolBar;

namespace kentos::app {

/// The two-page import window this shell opens; see `import_wizard.hpp`.
class ImportWizard;

/// The PostGIS window, opened from the File menu; see database_dialog.hpp.
class DatabaseDialog;
class SettingsDialog;

/// KentOSCad's own widgets and the controller, forward-declared for the same reason.
class CommandLine;
class Controller;
class LayerPanel;
class MapCanvas;
class CommandPalette;
class AttributePanel;
class DocumentTabs;
class PanelHeader;
class StatusStrip;
class ReadoutStrip;
class TitleBar;
class ToolBox;
class ToolsPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// Builds the shell: canvas, docks, toolbars, menus and the command line,
    /// then connects them to the controller. Every action created here dispatches
    /// a command; none of them touches the document directly (Article 1.2, 5.9).
    explicit MainWindow(QWidget* parent = nullptr);

    /// Saves the window geometry and dock layout, which are not declared settings
    /// and deliberately not part of the document.
    ~MainWindow() override;

    /// Runs a script file through the bus, exactly as the BETİK command does.
    void runScriptFile(const QString& path);

    /// The canvas, for the headless frame-timing hook in `main.cpp`. Developer
    /// tooling only; nothing user-facing reaches for this.
    MapCanvas* canvas() const noexcept { return canvas_; }

    /// Opens the style designer on one layer. Owned by the shell rather than by
    /// the panel that asked, so a panel never has to know what is in a window.
    void openStyleDesigner(const QString& layerName);

    /// Sends one line to the bus, exactly as the command line would. Public for
    /// `KENTOS_SHOT_DIR`, which has to re-fit the drawing after it resizes the
    /// window; it buys no privilege — this IS the command line's own road.
    void runScriptLine(const QString& line);

    /// Ends a command that is waiting for more input, the way the right button
    /// and Enter do. Public for the same reason `runScriptLine` is: a probe and
    /// a screenshot run have to be able to finish a draw command before the next
    /// line, because a draw command takes an unbounded run of points and PARKS —
    /// and a transparent line typed after it (`İZ`, `YAKINLAŞ`) runs beside it
    /// rather than ending it. It buys no privilege: this is the gesture's own
    /// road (`Controller::finishInteractive`).
    void endCommand();

    /// Abandons a command that is waiting, the way Esc does — with nothing it
    /// had not yet committed left behind (`Controller::cancelInteractive`).
    /// Public for the screenshot run, which starts a tool, photographs what it
    /// previews, and has to put it down again before the next frame.
    void cancelCommand();

    /// Brings the transcript in front: the properties dock, on its `Geçmiş` tab.
    ///
    /// THE TRANSCRIPT IS A TAB, not a dock of its own — which is why a
    /// `transcriptDock_` member sat null for its whole life and two theme loops
    /// quietly skipped it. A query command answers here and changes nothing on
    /// the canvas, so a menu row that runs one has to put its answer where
    /// somebody is looking. Public so `KENTOS_SHOT_DIR` can photograph an answer
    /// where a user would read it; it is the same call the menu makes.
    /// Brings the attribute panel forward — the tab and the page together, as
    /// `showTranscript` does for the transcript.
    void showAttributes();

    void showTranscript();

    /// Opens the Python console and puts the cursor in its prompt. Public for the
    /// reason `runScriptLine` is: a probe and a menu action are the same client,
    /// and a capability only one of them could reach would be the private entry
    /// point Article 1.2 forbids.
    ///
    /// `source` is typed into the prompt and sent when it is not empty, so a
    /// screenshot shows the panel in the state a user leaves it in rather than
    /// empty.
    void showPythonConsole(const QString& source = {});

    /// Opens the chat dock where it belongs — docked on the right unless the
    /// person floated it on purpose — and brings it forward. Every road that
    /// opens it comes here: the tool bar, an outside client's suggestion.
    void showChat();

    /// Types into the Python prompt without sending, so a probe can photograph
    /// the completion popup and the signature hint in the states a user sees
    /// them in. Same reason `runScriptLine` is public.
    void typeIntoPythonPrompt(const QString& source);

    /// The Python prompt's signature strip, for the probe that photographs it.
    QWidget* pythonSignatureHint() const;

    /// The Python prompt's completion popup — a window of its own, like the hint.
    QWidget* pythonCompletionPopup() const;

    /// Empties the Python prompt, for a probe that types a fresh line.
    void clearPythonPrompt();

    /// Drives the Python prompt and checks what it offers. Returns the number of
    /// failures, so `ctest` fails on any of them.
    int probePython();

    /// The other three windows, public for the same reason as the designer:
    /// `KENTOS_SMOKE` opens every one of them in turn, so a dialog that crashes
    /// on construction fails a test rather than a user. A window nothing
    /// constructs is a window nothing is checking.
    void openSettings();

    /// Opens `Seçenekler` on the section whose title is `title` — what the
    /// print menu's "Profilleri Yönet…" asks for. An unknown title opens the
    /// window on its first section.
    void openSettingsSection(const QString& title);

    /// WHAT THE TOOLBAR'S YAZDIR DOES, and it does two different things by
    /// design. With no print frame up it OPENS ONE: a sheet-shaped window in
    /// the middle of the canvas at the default profile's printable aspect, the
    /// rest greyed, the map free to be dragged and zoomed under it. Pressed
    /// again it CAPTURES what is inside the frame and opens the preview window.
    /// Esc or the right button puts the frame away without printing.
    ///
    /// `profile` names the profile to frame and print with; empty is the
    /// default one (`PrintService`). The menu beside the button passes a name.
    void printWithProfile(const QString& profile = QString());

    /// Starts — or finishes — aiming a PAFTA on the canvas.
    ///
    /// THE FLOW THE MAINTAINER ASKED FOR: pick a sheet from the print menu, drag
    /// a rectangle on the drawing, and the designer opens with the map frame
    /// already looking at what was dragged. The first press begins the frame at
    /// the sheet's MAP aspect, so what is framed is what the map will hold; the
    /// second press captures it and opens the window.
    void layoutWithFrame(const QString& layout);

    /// Opens the layout designer on `layout`, aimed at `window` when it is not
    /// empty.
    void openLayoutDesigner(const QString& layout, core::Box2 window = {});

    /// Floats the properties panel and drags it by its header, reporting where
    /// it started and where it ended up.
    ///
    /// WHAT IT GUARDS. The panel headers ARE the docks' title bars, and a title
    /// bar that keeps the press it is given is a panel that cannot be moved —
    /// docked it cannot be re-docked, floated it cannot be moved at all. Only a
    /// real press and a real drag can tell whether the press reached the dock.
    QStringList probeDockDrag();

    /// Whether this process is a probe driving the real shell, in which case it
    /// persists nothing: no preference, no geometry, no dock layout.
    static bool isProbeRun();

    /// Asks for a name, creates a layout and opens the designer on it.
    void newLayout();

    /// Rebuilds `Dosya ▸ Çıktı Yerleşimleri` from the document. Wired to the menu's own
    /// `aboutToShow`, so a sheet added at the command line is there the next
    /// time the menu opens without anything having to be told.
    void rebuildLayoutMenu();

    /// Opens the layout list: open, rename, duplicate, remove.
    void openLayoutManager();

    /// Asks what to call the new sheet and runs `PAFTAŞABLON islem=uygula`, then
    /// opens the designer on it — a sheet from a template still needs aiming.
    void applyLayoutTemplate(const QString& templateName);

    /// Asks which layout and what to call it, then runs `islem=kaydet`.
    void saveLayoutTemplate();

    /// Asks for a path and runs `YAZDIR yerlesim=`.
    void exportLayout(const QString& layout);

    /// Opens the preview window directly on `window` — the frame's capture, or
    /// the current view when something else asks.
    void openPrintDialog(core::Box2 window, const QString& profile = QString());

    /// Opens `Proje Ayarları`: what the .pcad file carries, and its schema.
    void openProjectSettings();

    /// Opens the attribute table on `layerName`, or on the ACTIVE layer when it is
    /// empty — which is what the Katman menu and `KENTOS_SMOKE` ask for.
    void openAttributeTable(const QString& layerName = QString());

    /// Opens the command list page, `YARDIM`'s answer and `Ctrl+K`'s.
    ///
    /// `focus_on` is a command name to open on — what `YARDIM komut=ÇİZGİ` asks
    /// for; empty opens on the first row.
    void openCommandSearch(const QString& focus_on = QString());

    /// Asks which of `candidates` the click meant, and sends the answer to the
    /// bus with `modifiers` applied.
    ///
    /// Public for the same reason the three windows above are: `KENTOS_PICK_PROBE`
    /// drives it, and a chooser nothing constructs is a chooser nothing checks.
    void choosePick(const std::vector<core::EntityId>& candidates, Qt::KeyboardModifiers modifiers);

    /// A form field's object pick landed on several objects: the same "Hangisi?"
    /// list as a click on the drawing, and the row chosen answers the field.
    void chooseCapture(const std::vector<core::EntityId>& candidates);

    /// Clicks the middle of the canvas with a REAL mouse event, answers the
    /// chooser that opens by taking its SECOND row, and prints what the document
    /// ended up with.
    ///
    /// It starts at the click for the reason `LayerPanel::probeByHand` does:
    /// calling `choosePick` would prove `choosePick` works and say nothing about
    /// whether a click can reach it. `scripts/ci-gate-secim-listesi.sh` drives it.
    void probePickList();

    /// Draws a slanted edge, starts ÇİZGİ on it, holds SHIFT on the canvas and
    /// prints the angle the rubber band actually settled on.
    ///
    /// It starts at the key event and the mouse move for the reason
    /// `probePickList` starts at the click: the engine half of the surface-normal
    /// lock has its own unit test in `/tests/unit/test_snap.cpp`, and what no
    /// Qt-free test can reach is whether HOLDING A KEY on a real canvas engages
    /// it. `scripts/ci-gate-yuzey-normali.sh` drives it.
    void probeSurfaceNormal();

    /// Presses and holds the line tool button, prints the family card that opens
    /// and takes a row other than the first.
    ///
    /// Same bargain as the probes around it: a flyout nothing opens is a flyout
    /// nothing checks. `scripts/ci-gate-arac-ailesi.sh` drives it.
    void probeToolFamily();

    /// Opens Katman Özellikleri on its Öznitelikler page, declares one column of
    /// every type through the page's own dialog line, edits one, drops one, and
    /// prints the table after each step.
    ///
    /// It goes through the page rather than through `SÜTUN` directly for the
    /// reason every probe here does: the command has its own tests, and what
    /// those cannot reach is whether the window a user opens can send it.
    /// `scripts/ci-gate-oznitelik-semasi.sh` drives it.
    void probeSchemaPage();

    /// Drives the attribute grid: refuses to open a cell with the edit mode off,
    /// then turns it on, types across a row, and reports where Enter landed —
    /// including the wrap from the last column to the next row's first.
    /// `scripts/ci-gate-tablo-giris.sh` drives it.
    void probeAttributeGrid();

    /// Opens the living component standard — every component in every state,
    /// laid out as `bileşen_standardı.png` is — prints its inventory, and
    /// photographs it when `KENTOS_WIDGETS_PROBE` carries a directory.
    ///
    /// It is how a change to a component is SEEN before it ships, and how the
    /// picture in `docs/baslangic/bilesenler.md` is regenerated (docs.md R15).
    /// `scripts/ci-gate-bilesenler.sh` drives it.
    void probeWidgets();

    /// Photographs every window of the program — the main window with an object
    /// selected, both settings windows, the layer properties, the attribute
    /// table, the PostGIS window, the import wizard, the column dialog — into
    /// the directory `KENTOS_DIALOG_PROBE` names, in the theme the mockups are
    /// drawn in unless `KENTOS_PROBE_THEME=acik`.
    ///
    /// It exists because a window is judged by looking at it, and a reviewer who
    /// has to build a drawing by hand before every look does not look.
    /// `KENTOS_HELP_PROBE`: runs the help command the way its menu entry does
    /// and checks what the user gets. Returns the number of failures.
    ///
    /// The help menu used to pour the generated reference into a `QMessageBox`,
    /// whose text has no scroll area: the window grew past the bottom of the
    /// screen and there was no way to move it. That is what this asserts
    /// against — a page of a fixed height whose list scrolls, opened by the
    /// command rather than by a private path.
    int probeHelpPage();

    /// `KENTOS_MENU_PROBE`: opens every menu in turn, photographs it and prints
    /// what it holds. Returns the failure count.
    ///
    /// A menu is the one part of a shell nothing else can show you: it is not in
    /// the window until it is opened, so a screenshot of the program proves
    /// nothing about it. This opens them, so the bar can be looked at rather
    /// than reasoned about.
    int probeMenus();

    /// `KENTOS_REACH_PROBE`: how much of the program a hand can reach. Returns
    /// the number of commands with no button and no menu entry.
    ///
    /// CLAUDE.md 5.15 forbids a feature reachable only by mouse. Its mirror is
    /// what shipped: of 97 commands, 33 could be started ONLY by typing a name,
    /// so a mouse user did not have them — and Article 1.2 makes the GUI an
    /// equal client, not a poorer one. This is the gate that keeps it at zero as
    /// commands are added.
    int probeReach();

    /// `KENTOS_ANSWER_PROBE`: presses the tools that ask for a NAME or a NUMBER
    /// and checks a hand could answer. Returns the failure count.
    ///
    /// Reachability (`probeReach`) says a button exists. This says pressing it
    /// leaves the user somewhere they can act: the keyboard in the field that
    /// takes the answer, and — when the set of answers is known — that set
    /// offered rather than remembered. Both were missing, which is why the block
    /// and measuring tools looked dead when pressed.
    int probeAnswerable();

    /// `KENTOS_FLYOUT_PROBE`: opens every tool family with a real mouse and runs
    /// every member. Returns the failure count.
    ///
    /// Eleven tools live behind a family button — ÇOKLUÇİZGİ, SPLINE, ÇOKGEN,
    /// TARAMA, ELİPS, HALKA, DİLİM, BLOK, LİDER, ALANÖLÇ, KOORDİNAT — and the
    /// column's own probe presses only the face, so none of them had ever been
    /// pressed by a test. They are also the ones the user could not run. The card
    /// is opened the three ways a hand opens it (hold, right click, corner mark)
    /// and each member is then chosen from it.
    int probeFlyouts();

    /// `KENTOS_REALMOUSE_PROBE`: drawing driven the way a REAL mouse arrives.
    /// Returns the failure count.
    ///
    /// Every other probe sends its events straight to the widget it means, which
    /// bypasses the two things a real click goes through first: the hit test that
    /// decides which widget is under the pointer, and the press-to-release
    /// interval. A user reported that drawing never works with the mouse and that
    /// no guide follows it, while all of those probes were green — so what they
    /// do not cover is exactly where the defect has to be.
    int probeRealMouse();

    /// `KENTOS_OSCLICK_PROBE=<dir>`: the window held open for REAL window-system
    /// events. Returns 0 — nothing is asserted here.
    ///
    /// Every probe above synthesises its events inside the process. What none
    /// of them can produce is a click that arrives from the operating system —
    /// through Cocoa, the platform plugin and Qt's own hit test — which is the
    /// only kind a user ever makes. This one prints where its buttons and its
    /// canvas are in GLOBAL coordinates, then pumps events until `<dir>/dur`
    /// appears or `KENTOS_OSCLICK_SECONDS` pass, reporting every change it sees:
    /// the lit tool, the prompt, the object count, new transcript lines, and an
    /// open family card with its rows. A driver outside the process — `osascript`
    /// System Events — does the clicking, and its log against this one is the
    /// evidence. A picture is left at the end.
    int probeOsClicks();

    /// `KENTOS_ACCESS_PROBE`: the tool column reached the way a SCREEN READER
    /// reaches it. Returns the failure count.
    ///
    /// This is the probe the accessibility hole got past, and it got past
    /// everything: `tool-reach` walks the actions, `real-mouse` and `tool-flyouts`
    /// send mouse events, `os-clicks` needs a person at the machine. None of them
    /// goes through `QAccessibleInterface`, which is the road VoiceOver, NVDA and
    /// Orca take — and on that road Qt answers a checkable tool button's press
    /// with `toggle()`, which lights the button and runs nothing.
    ///
    /// So every button in the column is driven here through its accessible action
    /// interface, and the assertion is not that something happened but that the
    /// ACTION FIRED: an exclusive `QActionGroup` lights a button on its own, so a
    /// lit button has never been proof that a command ran. The keyboard road is
    /// checked in the same pass, with real key events into the column.
    int probeAccessible();

    /// The window as the user sees it: `grab()` with the live GPU canvas pasted
    /// over the canvas widget, which `grab()` alone leaves blank under QRhi.
    QImage probePicture();

    /// `KENTOS_STRIP_PROBE`: the status strip under a long message. Returns the
    /// failure count.
    ///
    /// The strip's right end holds three cells — the render backend, the
    /// database, the agent listener — and the message beside the aid chips is
    /// given whatever gap they leave. The gap subtracted only two of them, so a
    /// long line was elided to a box that ran under the third and the two were
    /// drawn on top of each other. `ÖLÇ` writes one of the longest lines in the
    /// program, which is why the measuring tool was the one reported as broken.
    /// Proved in PIXELS: the right-hand cells must come out identical whatever
    /// the message says.
    int probeStatusStrip();

    /// DOES THE WINDOW FIT A LAPTOP? Asks for the sizes of the screens this
    /// program is run on — 1280×720 and 1440×860 of usable desktop — and reports
    /// the size the window actually took, the minimum each part of it imposes,
    /// and every tool-column button that lies outside the column's visible
    /// height. A window taller than the screen puts its status bar, its console
    /// and the foot of the tool column below the edge, where nobody can see
    /// them; the defect is invisible on the machine that has the big screen.
    int probeFit();

    /// THE OPERATING SYSTEM'S CLIPBOARD, end to end. `/tests` links no Qt, so
    /// nothing there can see whether the payload reached `QClipboard` under the
    /// agreed MIME type and came back out of it. Copies two parcels, clears the
    /// temp file the io side would otherwise read, and pastes from the system
    /// clipboard alone. Returns the number of defects.
    int probeClipboard();

    void probeDialogs();

    /// Builds the drawing every window probe photographs: a named layer, three
    /// objects, five typed columns and values on the first parcel.
    void seedProbeDrawing();

    /// Opens the layer properties window on the probe drawing, classifies its
    /// layer by a text column, applies, and prints what the document ended up
    /// with — `KENTOS_DESIGNER_PROBE`, photographed when given a directory.
    void probeDesigner();

    /// Opens the import wizard WITHOUT blocking, on `path` when one is given.
    ///
    /// `importData()` runs it modally and then runs the command line it built;
    /// this one shows it and returns, which is what `KENTOS_SMOKE` and
    /// `KENTOS_SHOT_DIR` need — a window that never returns cannot be
    /// photographed or closed by a timer.
    /// Returns the window it opened, so a probe can photograph the file page and
    /// only then start the read. A caller with no such need ignores it.
    ImportWizard* openImportWizard(const QString& path = QString());

    /// The controller behind this window, for a probe that must reach a service
    /// the shell owns (the AI layer's plan store, the agent listener).
    ///
    /// NOT A BACK DOOR FOR THE UI: every widget in this program reaches the
    /// document through a command, and this accessor exists so that an
    /// app-level test can assert what a service did without a socket of its own.
    Controller* controller() noexcept { return controller_; }

    /// Clicks the layer panel's eye and lock with real mouse events and prints
    /// what the document did. See `LayerPanel::probeByHand`.
    void probeLayerPanel();

    /// Opens the print preview on `box`, runs nothing, and returns the command
    /// line it would send. `round` presses the rounding button first.
    ///
    /// THE CHECK NOTHING ELSE MAKES. The frame and the sheet are joined by one
    /// thing — the area — and when the preview rounded the frame's scale up by
    /// itself the two stopped agreeing: the sheet covered a fifth more ground
    /// than the frame the user had just dragged. Every test stayed green, because
    /// each one asked the command to print whatever the window had already
    /// decided. The line is where the two meet, so the line is what has to be
    /// read.
    ///
    /// Developer tooling behind `KENTOS_PRINT_PROBE`.
    QString probePrintLine(core::Box2 box, const QString& profile, const QString& pdf, bool round);

    /// What a second press on a print control would open, for the print probe.
    ///
    /// `yerlesim:<ad>` when the frame up was started from a layout, `yazdir` when
    /// it was started from a paper profile, and empty when no frame is up.
    ///
    /// WHY THE DECISION IS EXPOSED RATHER THAN THE DIALOG. Both destinations are
    /// modal, so a headless probe cannot let either open — but the routing is
    /// exactly where the bug was: the toolbar's printer icon captured a frame
    /// begun for a layout into the plain print dialog, throwing the sheet away
    /// without a word, while the layout's own menu entry did the right thing.
    QString probeFrameDestination() const;

    /// Starts a print frame the way the layout menu entry does, for the probe.
    void probeBeginLayoutFrame(const QString& layout);

    /// Starts one the way a paper profile does, for the probe.
    void probeBeginPlainFrame();

    /// Presses every button on the tool column in turn and prints what the
    /// program answered, one line per tool.
    ///
    /// THE CHECK NOTHING ELSE MAKES. A tool button is four things that have to
    /// agree — the action, the command string on it, a command registered under
    /// exactly that string, and a body that does something useful with an empty
    /// argument list — and every unit test in the tree exercises the fourth
    /// through `execute_line`, which is not the road the button takes. That is how
    /// "Alan Seç" shipped sending `SEÇ mod=KUTU` to a lookup that can only resolve
    /// a bare name: dead on every click, in every session, with a green suite.
    ///
    /// Developer tooling behind `KENTOS_TOOL_PROBE`, the same category as
    /// `KENTOS_EDIT_PROBE`; nothing user-facing calls it.
    void probeToolBox();

    /// Drives a RECORDED provider stream into the chat dock and reports what the
    /// panel did with it, one line at a time.
    ///
    /// THE CHECK NOTHING ELSE MAKES. `tests/unit/test_ai_chat.cpp` proves every
    /// dialect decodes and `test_ai_tools.cpp` proves a plan applies, both
    /// without a window; what neither can reach is the seam between them — that
    /// a decoded tool call becomes a bubble, a card and an undo entry in a
    /// running shell, and that a write call leaves the drawing untouched until
    /// the card is pressed. Returns 0 when everything held.
    ///
    /// Developer tooling behind `KENTOS_CHAT_PROBE`.
    int probeChat();

    /// Every entry of `Dosya ▸ Çıktı Yerleşimleri`, as a menu walk would find it, one
    /// line per action and indented for a submenu.
    ///
    /// THE CHECK NOTHING ELSE MAKES. A menu entry is four things that have to
    /// agree — the submenu exists, it is rebuilt from the document, each sheet
    /// has its own entries, and each entry is CONNECTED. A screenshot shows the
    /// first; only walking the actions and triggering one shows the rest.
    /// Every entry of the toolbar's print list, as a menu walk would find it.
    /// For the probe that proves a layout read off disk reaches it.
    QStringList probePrintMenu();

    QStringList probeLayoutMenu();

    /// Drives the six modify tools with REAL mouse and key events, the way a hand
    /// does — `action->trigger()`, then presses on the canvas, then Enter sent to
    /// whatever actually holds focus.
    ///
    /// `probeToolBox` above calls `supplyPoint`/`supplyObjects` directly, which
    /// proves the session accepts a value and proves NOTHING about whether the
    /// user can give it one. That gap is exactly where "Alan Seç" hid, and it is
    /// where the next one will hide too.
    ///
    /// Developer tooling behind `KENTOS_HAND_PROBE`.
    void probeToolsByHand();

private slots:
    /// Bus observers. The shell SUBSCRIBES to the command bus and never reaches
    /// around it: a value on screen is there because a command put it there, so
    /// the same change made from the command line, a script or the AI updates the
    /// interface identically (Article 1.2).
    void onEcho(const QString& text);
    void onDocumentChanged();
    void onPromptChanged(const QString& prompt);

    /// Re-arms a modal draw tool after it has finished a shape, so the next one
    /// can be drawn without going back to the tool column.
    /// Starts `action`'s tool again after a run of `id` finished, clearing a
    /// standing selection first for a tool that begins by asking for objects.
    void rearm(QAction* action, const QString& id);

    /// Starts a typed tool again by its line (`command::rearm_line`) — a method
    /// that no button carries. Queued and standing down like `rearm`.
    void rearmLine(const QString& line);

    void onInteractiveFinished(const QString& id, bool mutated, bool dismissed);
    void onUndoStateChanged(bool canUndo, bool canRedo);
    void onCursorMoved(core::Point2 world);
    void onViewRequested(const QString& mode, double factor);

    /// KAYDIR: slide the canvas so `from` ends up at `to`, keeping the scale.
    void onPanRequested(core::Point2 from, core::Point2 to);
    void onSettingChanged(const QString& id);
    void onCommandSubmitted(const QString& line);

    /// Re-reads the session modes and re-checks the F3 / F8 / F9 items. Every
    /// client writes those modes through `MOD`, so the toolbar state is derived
    /// from the store and never held separately (CLAUDE.md 5.10).
    void refreshAidActions();

    /// Opens the object-snap mode list: one checkable entry per bit of
    /// `core.yakalama.modlar`, plus all-and-none.
    ///
    /// The engine has offered thirteen modes since it was written and the shell
    /// showed three switches — F3 for "any of them", F8 for ortho, F9 for grid —
    /// so KESİŞİM, DİK, YAKIN, DÜĞÜM, UZANTI, PARALEL and UZATILMIŞ KESİŞİM were
    /// implemented, tested, and unreachable from the program. Every entry writes
    /// the mask through `MOD`, so the panel is a client of the bus like any other.
    void openSnapModes();

    void openScript();
    void openDatabase();

    // The six file actions. Each one collects a path and dispatches the SAME
    // command a user could type; the dialog is not the feature (Article 1.2).

    /// Asks whether unsaved work should be saved, and reports whether to go on.
    ///
    /// ONE COPY, because there are now two ways to lose a drawing — closing the
    /// window and starting a new one — and the delicate part is the same for
    /// both: THREE answers, not two. A person who reached for the wrong control
    /// must be able to say "no, I did not mean that", and a Save that was itself
    /// cancelled at the file dialog must not become a silent discard.
    ///
    /// `question` is the second line, naming what is about to happen. Returns
    /// true when the caller may proceed — including the ordinary case where
    /// nothing was dirty and nothing was asked.
    bool confirmDiscard(const QString& question);

    /// Starts an empty drawing — `YENİ` — asking first when work would be lost.
    ///
    /// THE QUESTION IS HERE AND NOT IN THE COMMAND. `core.new` replaces the
    /// document without asking, because a command body runs identically for a
    /// script, an agent and a hand, and "kaydedilsin mi?" has no answer in a
    /// batch run. So the window asks the same three-button question
    /// `closeEvent` asks — Kaydet, Atla, Vazgeç — and dispatches only once the
    /// user has answered. `AÇ` settles it the same way.
    void newProject();

    void openProject();
    void saveProject();
    void saveProjectAs();
    void importData();
    void exportData();
    void showAbout();
    void toggleTheme(bool dark);
    void showCommandLine(bool visible);
    void resetLayout();

protected:
    /// Asks before losing work, and lets the user say no.
    ///
    /// THE ONE PLACE A CLOSE CAN BE REFUSED. Every road out of the application —
    /// the window's own close box, the File menu's quit item, Alt+F4, the desktop
    /// asking politely — arrives here, because all of them end in
    /// `QWidget::close()`. Hooking the menu item instead would have covered
    /// exactly one of them.
    void closeEvent(QCloseEvent* event) override;

private:
    void buildActions();
    void buildToolBars();

    /// Lights the tool whose command is actually running, and the select tool when
    /// none is.
    ///
    /// The tool that is lit used to be decided by whether ANY command was waiting
    /// for input, which lit ÇİZGİ for every one of them — so picking the polygon
    /// tool and clicking a corner moved the highlight back to the line. Which tool
    /// is running is a question only the session can answer, and the answer is
    /// matched through `Registry`: the command each action sends is the one thing
    /// that identifies it, and there is no second table of tools to keep in step
    /// (CLAUDE.md 5.10).
    void syncToolSelection();

    /// Asks before erasing, when `core.duzenleme.silme_onayi` says to. Returns
    /// whether to go ahead.
    ///
    /// The question is the SHELL'S and never the command's. `SİL` has to run in a
    /// journal replay, in a batch and from a script, and a command body that
    /// stopped to open a dialog could do none of them — so the interface asks
    /// first and then sends exactly the command a script would send (Article 1.2).
    bool confirmErase();

    /// Creates an action for a feature that does not exist yet: it stays live and
    /// EXPLAINS ITSELF when pressed.
    ///
    /// It used to be `setEnabled(false)` with the phase in a tooltip, and a
    /// disabled menu row answers nothing when clicked — the user's words were
    /// that they could not tell why the item was there. So the row opens a box
    /// that says what the feature will do, which phase it lands in, and what to
    /// use in the meantime. `explains` is that sentence, in Turkish.
    QAction* placeholder(Glyph glyph, const QString& text, const QString& command,
                         const QString& phase, const QString& explains);

    /// Creates a button for a command that EDITS THE SELECTION and then finishes.
    ///
    /// `runCommand` and not `runLine`: these ask for points, so they begin an
    /// interactive session the way a typed command does — `runLine` dispatches a
    /// complete invocation and a transform with no arguments would end before it
    /// started. They are not modal draw tools and do not join the exclusive group:
    /// nothing stays armed after one runs.
    /// A construction METHOD as a tool (`YAY yontem=3n`): the whole line rides on
    /// the button, so the family card teaches it. Without these, every classical
    /// method P2 added was reachable only by typing (§2.6a, CLAUDE.md 5.15).
    QAction* methodTool(Glyph glyph, const QString& text, const QString& line, const QString& tip);

    QAction* modifyTool(Glyph glyph, const QString& text, const QString& command,
                        const QString& tip);

    /// Whether the command's whole answer is WORDS, so the transcript has to be
    /// open before it runs. Asked of the registry, so it is one decision rather
    /// than a flag repeated at every call site (CLAUDE.md 5.10).
    bool answersInWords(const QString& command) const;

    /// Creates an action that dispatches `line` through the bus. The GUI is a
    /// client of the command bus and gets no private path (CLAUDE.md Article 1).
    QAction* commandAction(Glyph glyph, const QString& text, const QString& line,
                           const QString& tip, const QKeySequence& shortcut = {});

    /// The file dialog filter, generated from the io module's driver allow-list.
    /// There is no second list of formats to keep in step (CLAUDE.md 5.10 in
    /// spirit): adding a driver in /cmake changes this dialog too.
    QString externalFormatFilter(bool for_writing) const;

    /// Puts the current project file in the title bar, so a user always knows
    /// which drawing they are about to overwrite.
    void refreshWindowTitle();

    void refreshLayerCombo();
    /// A tabified dock shows its name on the tab, so its own title bar would say
    /// it twice. Hidden while tabbed, restored when the dock is floated or torn
    /// out — dragging the tab still detaches it.
    /// Loads the gösterim package named by `core.stil.kutuphane` onto the session's
    /// symbol shelf, through the bus like any other client.
    void loadSymbolLibrary();

    void syncDockTitles();
    void buildMenus();

    /// Appends every command the menus do not already offer, to the menu of its
    /// category — GENERATED FROM `Registry`, run once after the menus are built.
    ///
    /// The curated entries above it are the ones with a chosen place, a shortcut
    /// and a hand-written label; this is what makes the bar COMPLETE. Without it
    /// the menus were a hand-kept list of the commands somebody remembered, and
    /// what that cost was measurable: of 97 commands, 64 could be started from a
    /// button or a menu and 33 could be started only by typing their name. A
    /// mouse user simply did not have them.
    ///
    /// That is CLAUDE.md 5.10 read the way it is written — a menu table not
    /// generated from `Registry` is forbidden — and Article 1.2, which makes the
    /// GUI an equal client rather than a poorer one. It also means the next
    /// command to be declared arrives in its menu with nothing told to do it.
    void completeMenusFromRegistry();
    void buildToolBox();
    void buildPanels();
    void buildStatusBar();
    void applyTheme();
    void refreshStatus();

    /// Paints the colour chips with what is in hand: the first selected
    /// object's stroke and fill, or the active layer's with nothing selected —
    /// the colours a new object is drawn in.
    void refreshColourChips();

    /// Opens the menu of chip `which` (0 stroke, 1 fill) beside it: the named
    /// colours as swatches, any other colour, the layer's, and — for a fill —
    /// none. Every choice runs `RENK` (`applyColour`).
    void openColourMenu(int which);

    /// Runs `RENK` with `word` as the stroke or the fill. On the selection when
    /// there is one; with none, RENK asks for the objects to paint.
    void applyColour(bool fill, const QString& word);

    /// Repaints the status strip's agent cell and the menu entry's wording from
    /// what the listener is actually doing. Called on `McpService::stateChanged`,
    /// so the cell cannot claim a port that is closed.
    void refreshAgentCell();

    /// R38: persistence, validation, UI and documentation all come from the one
    /// SettingSpec declaration. QSettings is the backing FILE and nothing more —
    /// it is keyed by SettingSpec::id and its contents are validated by
    /// core::parse_setting, so the shell cannot hold an opinion the catalogue does
    /// not know about (CLAUDE.md 5.10: there is no second settings list).
    void loadPreferences();
    void savePreferences();
    ThemeMode themeFromPreferences() const;

public:
    /// The theme the shell is painting in, for a window a probe opens outside
    /// the shell's own `openSettings`-style helpers.
    ThemeMode themeMode() const noexcept { return theme_; }

private:
    Controller* controller_{nullptr};
    MapCanvas* canvas_{nullptr};

    /// The form field waiting for a scene pick, as the function that takes the
    /// answer; empty when none is (`ScenePicker`, tools_panel.hpp).
    std::function<void(std::optional<QString>)> pendingPick_;
    TitleBar* titleBar_      = nullptr;
    CommandPalette* palette_ = nullptr;
    CommandLine* commandLine_{nullptr};
    ToolBox* toolBox_{nullptr};
    LayerPanel* layerPanel_{nullptr};
    AttributePanel* attributePanel_{nullptr};
    QStackedWidget* propertyStack_{nullptr};
    ToolsPanel* toolsPanel_{nullptr};
    QTimer* jobPulse_{nullptr}; ///< reads the running job's progress figure for the status strip
    QString jobLabel_;          ///< the running job's label, so the figure can be appended to it
    PanelHeader* propertyHeader_{nullptr};
    PanelHeader* layerHeader_{nullptr};
    PanelHeader* journalHeader_{nullptr};
    QPlainTextEdit* transcript_{nullptr};
    QPlainTextEdit* journalView_{nullptr};

    /// design.md 7 draws ONE 46 px strip, so there is one bar. The old five —
    /// dosya, düzen, görünüm, katman, CBS — are its seven groups, separated by
    /// 1 px rules rather than by five drag handles.
    QToolBar* tbMain_{nullptr};
    ReadoutStrip* readout_{nullptr};
    StatusStrip* statusStrip_{nullptr};
    DocumentTabs* docTabs_{nullptr};
    QFrame* commandLineRule_{nullptr};

    QDockWidget* layerDock_{nullptr};
    QDockWidget* propertyDock_{nullptr};

    /// The conversation panel and its dock. Built with the window rather than on
    /// demand, so the toolbar mark and `Pencere ▸ Yapay Zeka` both just show it.
    class ChatPanel* chatPanel_{nullptr};
    PanelHeader* chatHeader_{nullptr};
    class PythonConsole* pythonConsole_{nullptr};
    PanelHeader* pythonHeader_{nullptr};
    QDockWidget* chatDock_{nullptr};
    QDockWidget* journalDock_{nullptr};
    QDockWidget* pythonDock_{nullptr};

    // ---- actions, each of which dispatches one command ----

    /// The modal draw tools, exclusive so exactly one is lit. Held because
    /// `syncToolSelection` walks it: the group already IS the list of tools, so
    /// keeping a second one beside it is the duplication CLAUDE.md 5.10 forbids.
    QActionGroup* drawingTools_{nullptr};

    /// The import wizard asked for a zoom to what its import brings, once the
    /// hosted read lands (`onInteractiveFinished`).
    bool zoomAfterImport_{false};

    QAction* actSelect_{nullptr};
    QAction* actCut_{nullptr};
    QAction* actCopyClip_{nullptr};
    QAction* actPaste_{nullptr};
    QAction* actStyle_{nullptr};

    /// The five tool-box groups of design.md 7. Several are Phase 2 commands;
    /// they exist as disabled buttons so the column has the shape the reference
    /// draws, with the phase named in each tooltip rather than silently absent.
    QAction* actSelectArea_{nullptr};
    QAction* actPolygon_{nullptr};
    QAction* actRegular_{nullptr};
    QAction* actTrim_{nullptr};
    QAction* actCombine_{nullptr};     ///< BİRLEŞTİR — generic; on the tool column
    QAction* actUnion_{nullptr};       ///< TEVHİT — cadastral; Kadastro menu
    QAction* actParcelSplit_{nullptr}; ///< İFRAZ — cadastral; Kadastro menu
    QAction* actAreaSplit_{nullptr};   ///< ALANİFRAZ — cadastral; Kadastro menu
    QAction* actMeasureArea_{nullptr};
    QAction* actCoordinate_{nullptr};
    QAction* actEntityInfo_{nullptr};   ///< NESNEBİLGİ — what is this
    QAction* actMeasureAngle_{nullptr}; ///< AÇIÖLÇ — the angle at this corner
    QAction* actStretch_{nullptr};      ///< ESNET — the window is the vertex filter
    QAction* actStyleCopy_{nullptr};
    QAction* actColour_{nullptr}; ///< RENK — the colour chips' command, on a menu row too
    QAction* actTopology_{nullptr};
    QAction* actLine_{nullptr};
    QAction* actErase_{nullptr};
    /// ⌫: the run's last point while ÇİZGİ, ÇOKLUÇİZGİ, ALAN or SPLINE is
    /// drawing, SİL otherwise — one key, two answers, never both.
    QAction* actBackspace_{nullptr};
    QAction* actLayer_{nullptr};
    QAction* actZoomExtents_{nullptr};
    QAction* actZoomIn_{nullptr};
    QAction* actZoomOut_{nullptr};
    QAction* actUndo_{nullptr};
    bool canUndo_{false}; ///< the stack's answer; Ctrl+Z also retracts a point mid-run
    QAction* actRedo_{nullptr};
    QAction* actScript_{nullptr};
    QAction* actDatabase_{nullptr};
    QAction* actProjectSettings_{nullptr};
    QAction* actSettings_{nullptr};

    /// The PostGIS window, kept because it is modeless: a user connects once and
    /// goes on drawing. Null when it has never been opened or has been closed.
    /// The two modeless windows, held as GUARDED pointers.
    ///
    /// A RAW POINTER PLUS A `destroyed` LAMBDA IS A WRITE AFTER DESTRUCTION, and
    /// UBSan says so: both windows are CHILDREN of this one and carry
    /// `WA_DeleteOnClose`, so on shutdown Qt deletes them from `~QObject` — which
    /// runs AFTER `~MainWindow`'s body. The lambda then wrote `nullptr` into a
    /// MainWindow whose lifetime had ended, over whatever the stack had put
    /// there. That is exactly the shape of a crash "at a meaningless point": the
    /// damage is done at teardown and lands somewhere else entirely.
    ///
    /// `QPointer` nulls itself when the object goes, needs no connection at all,
    /// and cannot outlive anything.
    QPointer<DatabaseDialog> database_;
    QPointer<SettingsDialog> settings_;

    /// `Proje Ayarları`, its own window and its own instance: the two answer
    /// different questions and a person often has both open, one to change a
    /// preference and one to check what the file carries.
    QPointer<SettingsDialog> projectSettings_;
    QAction* actSelectAll_{nullptr};
    QAction* actSelectNone_{nullptr};
    QAction* actOrtho_{nullptr};
    QAction* actNormal_{nullptr};
    QAction* actGridSnap_{nullptr};

    // ---- dosya ----
    QAction* actOpen_{nullptr};
    QAction* actSave_{nullptr};
    QAction* actSaveAs_{nullptr};
    QAction* actImport_{nullptr};
    QAction* actExport_{nullptr};

    // ---- sonraki fazlarda gelecek eylemler, pasif ----
    QAction* actNew_{nullptr};
    QAction* actPrint_{nullptr};

    /// The profile the frame was opened with, so the preview window opens on the
    /// same one the frame was shaped by. Empty means the default profile.
    QString printProfile_;

    /// The layout being aimed on the canvas, or empty when the frame belongs to a
    /// plain print. It is what tells the frame's acceptance which of the two
    /// windows to open.
    QString pendingLayout_;

    /// `Dosya ▸ Çıktı Yerleşimleri`, rebuilt from the document each time it opens.
    QMenu* layoutMenu_{nullptr};

    /// Rebuilds the print button's menu from `PrintService::profiles()`. There
    /// is no second profile list: the menu is the store, drawn (CLAUDE.md 5.10).
    void rebuildPrintMenu();

    QMenu* printMenu_{nullptr};

    /// The narrow button BESIDE YAZDIR that opens the profile list. Its own
    /// action rather than `QToolButton::MenuButtonPopup`: with the shell's one
    /// stylesheet on `QToolButton`, Qt draws a menu button's indicator as a
    /// triangle in the icon's corner — it sat on the printer glyph. Two buttons
    /// is also what a print control looks like everywhere else: the face prints,
    /// the arrow chooses.
    QAction* actPrintMenu_{nullptr};
    QAction* actPolyline_{nullptr};
    QAction* actArc_{nullptr};
    QAction* actCircle_{nullptr};
    QAction* actEllipse_{nullptr};
    QAction* actSector_{nullptr};
    QAction* actAnnulus_{nullptr};
    QAction* actRectangle_{nullptr};
    QAction* actSpline_{nullptr};
    QAction* actHatch_{nullptr};
    QAction* actHatchEdit_{nullptr}; ///< TARAMADÜZENLE
    QAction* actBlock_{nullptr};
    QAction* actInsert_{nullptr};
    QAction* actDimension_{nullptr};
    QAction* actLeader_{nullptr};
    QAction* actScale_{nullptr};
    QAction* actMirror_{nullptr};
    QAction* actArray_{nullptr};
    QAction* actArrayPolar_{nullptr}; ///< DİZİ mod=KUTUPSAL
    QAction* actArrayPath_{nullptr};  ///< DİZİ mod=YOL
    QAction* actRotateRef_{nullptr};  ///< DÖNDÜR yontem=referans
    QAction* actScaleRef_{nullptr};   ///< ÖLÇEKLE yontem=referans
    QAction* actMirrorCopy_{nullptr}; ///< AYNALA kopya=evet
    QAction* actCopyBase_{nullptr};   ///< PANOYAKOPYALA tabanli=evet
    QAction* actExtend_{nullptr};
    QAction* actTrimFence_{nullptr};   ///< BUDA yontem=çit
    QAction* actTrimKeep_{nullptr};    ///< BUDA tut=evet
    QAction* actTrimCarry_{nullptr};   ///< BUDA uzanti=evet
    QAction* actExtendFence_{nullptr}; ///< UZAT yontem=çit
    QAction* actExtendCarry_{nullptr}; ///< UZAT uzanti=evet
    QAction* actSplit_{nullptr};
    QAction* actSplitPoint_{nullptr};    ///< BÖL yontem=nokta
    QAction* actSplitCross_{nullptr};    ///< BÖL yontem=kesisim
    QAction* actSplitEqual_{nullptr};    ///< BÖL yontem=esit
    QAction* actSplitDistance_{nullptr}; ///< BÖL yontem=mesafe
    QAction* actChamfer_{nullptr};
    QAction* actChamferAll_{nullptr}; ///< PAH hepsi=evet
    QAction* actFillet_{nullptr};
    QAction* actFilletAll_{nullptr}; ///< YUVARLA hepsi=evet
    QAction* actSetLayer_{nullptr};
    // THE EDIT VERBS THAT WERE MENU ROWS ONLY. Held as tools now, so the column
    // can carry them in families and the menus and the column press one action.
    QAction* actBreak_{nullptr};         ///< KIR
    QAction* actLengthen_{nullptr};      ///< UZUNLUK
    QAction* actJoin_{nullptr};          ///< UÇUCA
    QAction* actExplode_{nullptr};       ///< PATLAT
    QAction* actAlign_{nullptr};         ///< HİZALA
    QAction* actAlignScaled_{nullptr};   ///< HİZALA olcekle=evet
    QAction* actDivide_{nullptr};        ///< BÖLÜMLE
    QAction* actPolylineEdit_{nullptr};  ///< ÇİZGİDÜZENLE
    QAction* actVertexMove_{nullptr};    ///< KÖŞETAŞI
    QAction* actVertexAdd_{nullptr};     ///< KÖŞEEKLE
    QAction* actVertexDelete_{nullptr};  ///< KÖŞESİL
    QAction* actEdgeKind_{nullptr};      ///< KENARTÜRÜ
    QAction* actToArea_{nullptr};        ///< ALANAÇEVİR
    QAction* actBoundary_{nullptr};      ///< SINIR
    QAction* actTextEdit_{nullptr};      ///< YAZIDÜZENLE
    QAction* actDimensionEdit_{nullptr}; ///< ÖLÇÜDÜZENLE
    QAction* actDimChain_{nullptr};      ///< ZİNCİRÖLÇÜ
    QAction* actDimBaseline_{nullptr};   ///< BAZÖLÇÜ
    QAction* actTraverse_{nullptr};      ///< POLİGON, on both the Çizim and the Harita menu
    QAction* actStakeout_{nullptr};      ///< APLİKASYON
    QAction* actLabel_{nullptr};         ///< ETİKET
    QAction* actAngledGuide_{nullptr};   ///< KILAVUZ yon=45g
    QAction* actPoint_{nullptr};
    QAction* actPerpOffset_{nullptr};
    QAction* actSurvey_{nullptr};
    QAction* actIntersect_{nullptr};
    QAction* actAlong_{nullptr};
    QAction* actText_{nullptr};
    QAction* actMove_{nullptr};
    QAction* actCopy_{nullptr};
    QAction* actRotate_{nullptr};
    QAction* actOffset_{nullptr};
    QAction* actMeasure_{nullptr};
    QAction* actPan_{nullptr};
    QAction* actSnap_{nullptr};
    QAction* actIdentify_{nullptr};
    QAction* actTable_{nullptr};
    QAction* actLayerManager_{nullptr};
    QAction* actAi_{nullptr};

    /// Starts and stops the agent listener. One entry whose WORDING follows the
    /// state, because a `Başlat/Durdur` label makes the reader work out which of
    /// the two they are about to do.
    QAction* actMcp_{nullptr};

    // ---- arayüz eylemleri ----
    QAction* actTheme_{nullptr};
    QAction* actHud_{nullptr};
    QAction* actCommandLine_{nullptr};
    QAction* actQuit_{nullptr};

    ThemeMode theme_{ThemeMode::Light}; ///< day mode is the default

    /// The object-snap mask to restore when F3 switches snapping back on. The
    /// value in force is always `core.yakalama.modlar`; this only remembers what
    /// to put back, so turning snapping off and on does not silently reset a
    /// carefully chosen set of modes.
    int snapMaskMemory_{0x7};
};

} // namespace kentos::app
