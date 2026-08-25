// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the main window.
//
// Layout (Qt Widgets shell, piricad.md §6.3):
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
// perspectives (piricad.md §6.3, .claude/ui.md).
#pragma once

#include "piricad/app/icons.hpp"
#include "piricad/app/theme.hpp"
#include "piricad/core/units.hpp"

#include <QMainWindow>

/// Qt widgets this header only holds pointers to. Forward-declared rather than
/// included so that touching a widget's header does not rebuild everything that
/// includes the main window.
class QComboBox;
class QStackedWidget;
class QFrame;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QToolBar;

namespace piricad::app {

/// The PostGIS window, opened from the File menu; see database_dialog.hpp.
class DatabaseDialog;
class SettingsDialog;

/// PiriCAD's own widgets and the controller, forward-declared for the same reason.
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

    /// Opens the style designer on one layer. Owned by the shell rather than by
    /// the panel that asked, so a panel never has to know what is in a window.
    void openStyleDesigner(const QString& layerName);

    /// Sends one line to the bus, exactly as the command line would. Public for
    /// `PIRICAD_SHOT_DIR`, which has to re-fit the drawing after it resizes the
    /// window; it buys no privilege — this IS the command line's own road.
    void runScriptLine(const QString& line);

    /// The other three windows, public for the same reason as the designer:
    /// `PIRICAD_SMOKE` opens every one of them in turn, so a dialog that crashes
    /// on construction fails a test rather than a user. A window nothing
    /// constructs is a window nothing is checking.
    void openSettings();
    void openAttributeTable();
    void openCommandSearch();

private slots:
    /// Bus observers. The shell SUBSCRIBES to the command bus and never reaches
    /// around it: a value on screen is there because a command put it there, so
    /// the same change made from the command line, a script or the AI updates the
    /// interface identically (Article 1.2).
    void onEcho(const QString& text);
    void onDocumentChanged();
    void onPromptChanged(const QString& prompt);
    void onUndoStateChanged(bool canUndo, bool canRedo);
    void onCursorMoved(core::Point2 world);
    void onViewRequested(const QString& mode, double factor);
    void onSettingChanged(const QString& id);
    void onCommandSubmitted(const QString& line);

    /// Re-reads the session modes and re-checks the F3 / F8 / F9 items. Every
    /// client writes those modes through `MOD`, so the toolbar state is derived
    /// from the store and never held separately (CLAUDE.md 5.10).
    void refreshAidActions();

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

private:
    void buildActions();
    void buildToolBars();

    /// Creates a disabled action for a command that does not exist yet. The
    /// tooltip names the phase it arrives in, so the interface never shows a
    /// button that silently does nothing (.claude/ui.md).
    QAction* placeholder(Glyph glyph, const QString& text, const QString& command,
                         const QString& phase);

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
    QAction* actUnion_{nullptr};
    QAction* actParcelSplit_{nullptr};
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
    QAction* actSettings_{nullptr};

    /// The PostGIS window, kept because it is modeless: a user connects once and
    /// goes on drawing. Null when it has never been opened or has been closed.
    DatabaseDialog* database_{nullptr};
    SettingsDialog* settings_{nullptr};
    QAction* actSelectAll_{nullptr};
    QAction* actSelectNone_{nullptr};
    QAction* actOrtho_{nullptr};
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
    QAction* actRectangle_{nullptr};
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

} // namespace piricad::app
