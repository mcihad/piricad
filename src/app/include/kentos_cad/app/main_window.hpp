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

    /// The other three windows, public for the same reason as the designer:
    /// `KENTOS_SMOKE` opens every one of them in turn, so a dialog that crashes
    /// on construction fails a test rather than a user. A window nothing
    /// constructs is a window nothing is checking.
    void openSettings();

    /// Opens `Proje Ayarları`: what the .pcad file carries, and its schema.
    void openProjectSettings();

    /// Opens the attribute table on `layerName`, or on the ACTIVE layer when it is
    /// empty — which is what the Katman menu and `KENTOS_SMOKE` ask for.
    void openAttributeTable(const QString& layerName = QString());

    void openCommandSearch();

    /// Asks which of `candidates` the click meant, and sends the answer to the
    /// bus with `modifiers` applied.
    ///
    /// Public for the same reason the three windows above are: `KENTOS_PICK_PROBE`
    /// drives it, and a chooser nothing constructs is a chooser nothing checks.
    void choosePick(const std::vector<core::EntityId>& candidates, Qt::KeyboardModifiers modifiers);

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

    /// Opens the import wizard WITHOUT blocking, on `path` when one is given.
    ///
    /// `importData()` runs it modally and then runs the command line it built;
    /// this one shows it and returns, which is what `KENTOS_SMOKE` and
    /// `KENTOS_SHOT_DIR` need — a window that never returns cannot be
    /// photographed or closed by a timer.
    /// Returns the window it opened, so a probe can photograph the file page and
    /// only then start the read. A caller with no such need ignores it.
    ImportWizard* openImportWizard(const QString& path = QString());

    /// Clicks the layer panel's eye and lock with real mouse events and prints
    /// what the document did. See `LayerPanel::probeByHand`.
    void probeLayerPanel();

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
    void onInteractiveFinished(const QString& id, bool mutated);
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

    void showCommandReference();
    void openScript();
    void openDatabase();

    // The five file actions. Each one collects a path and dispatches the SAME
    // command a user could type; the dialog is not the feature (Article 1.2).
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

    /// Creates a disabled action for a command that does not exist yet. The
    /// tooltip names the phase it arrives in, so the interface never shows a
    /// button that silently does nothing (.claude/ui.md).
    QAction* placeholder(Glyph glyph, const QString& text, const QString& command,
                         const QString& phase);

    /// Creates a button for a command that EDITS THE SELECTION and then finishes.
    ///
    /// `runCommand` and not `runLine`: these ask for points, so they begin an
    /// interactive session the way a typed command does — `runLine` dispatches a
    /// complete invocation and a transform with no arguments would end before it
    /// started. They are not modal draw tools and do not join the exclusive group:
    /// nothing stays armed after one runs.
    QAction* modifyTool(Glyph glyph, const QString& text, const QString& command,
                        const QString& tip);

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
    void buildToolBox();
    void buildPanels();
    void buildStatusBar();
    void applyTheme();
    void refreshStatus();

    /// R38: persistence, validation, UI and documentation all come from the one
    /// SettingSpec declaration. QSettings is the backing FILE and nothing more —
    /// it is keyed by SettingSpec::id and its contents are validated by
    /// core::parse_setting, so the shell cannot hold an opinion the catalogue does
    /// not know about (CLAUDE.md 5.10: there is no second settings list).
    void loadPreferences();
    void savePreferences();
    ThemeMode themeFromPreferences() const;

    Controller* controller_{nullptr};
    MapCanvas* canvas_{nullptr};
    TitleBar* titleBar_      = nullptr;
    CommandPalette* palette_ = nullptr;
    CommandLine* commandLine_{nullptr};
    ToolBox* toolBox_{nullptr};
    LayerPanel* layerPanel_{nullptr};
    AttributePanel* attributePanel_{nullptr};
    QStackedWidget* propertyStack_{nullptr};
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
    QDockWidget* transcriptDock_{nullptr};
    QDockWidget* journalDock_{nullptr};

    // ---- actions, each of which dispatches one command ----

    /// The modal draw tools, exclusive so exactly one is lit. Held because
    /// `syncToolSelection` walks it: the group already IS the list of tools, so
    /// keeping a second one beside it is the duplication CLAUDE.md 5.10 forbids.
    QActionGroup* drawingTools_{nullptr};

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
    QAction* actTrim_{nullptr};
    QAction* actCombine_{nullptr};     ///< BİRLEŞTİR — generic; on the tool column
    QAction* actUnion_{nullptr};       ///< TEVHİT — cadastral; Kadastro menu
    QAction* actParcelSplit_{nullptr}; ///< İFRAZ — cadastral; Kadastro menu
    QAction* actAreaSplit_{nullptr};   ///< ALANİFRAZ — cadastral; Kadastro menu
    QAction* actMeasureArea_{nullptr};
    QAction* actCoordinate_{nullptr};
    QAction* actStyleCopy_{nullptr};
    QAction* actTopology_{nullptr};
    QAction* actLine_{nullptr};
    QAction* actErase_{nullptr};
    QAction* actLayer_{nullptr};
    QAction* actZoomExtents_{nullptr};
    QAction* actZoomIn_{nullptr};
    QAction* actZoomOut_{nullptr};
    QAction* actUndo_{nullptr};
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
    QAction* actPolyline_{nullptr};
    QAction* actArc_{nullptr};
    QAction* actCircle_{nullptr};
    QAction* actEllipse_{nullptr};
    QAction* actSector_{nullptr};
    QAction* actAnnulus_{nullptr};
    QAction* actRectangle_{nullptr};
    QAction* actScale_{nullptr};
    QAction* actMirror_{nullptr};
    QAction* actArray_{nullptr};
    QAction* actExtend_{nullptr};
    QAction* actSplit_{nullptr};
    QAction* actChamfer_{nullptr};
    QAction* actFillet_{nullptr};
    QAction* actSetLayer_{nullptr};
    QAction* actPoint_{nullptr};
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
