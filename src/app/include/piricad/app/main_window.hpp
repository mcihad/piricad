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

class QComboBox;
class QFrame;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QToolBar;

namespace piricad::app {

class CommandLine;
class Controller;
class LayerPanel;
class MapCanvas;
class PropertyPanel;
class ToolBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Runs a script file through the bus, exactly as the BETİK command does.
    void runScriptFile(const QString& path);

private slots:
    void onEcho(const QString& text);
    void onDocumentChanged();
    void onPromptChanged(const QString& prompt);
    void onUndoStateChanged(bool canUndo, bool canRedo);
    void onCursorMoved(core::Point2 world);
    void onViewRequested(const QString& mode, double factor);
    void onCommandSubmitted(const QString& line);

    void showCommandReference();
    void openScript();
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

    void refreshLayerCombo();
    /// A tabified dock shows its name on the tab, so its own title bar would say
    /// it twice. Hidden while tabbed, restored when the dock is floated or torn
    /// out — dragging the tab still detaches it.
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
    CommandLine* commandLine_{nullptr};
    ToolBox* toolBox_{nullptr};
    LayerPanel* layerPanel_{nullptr};
    PropertyPanel* propertyPanel_{nullptr};
    QPlainTextEdit* transcript_{nullptr};
    QPlainTextEdit* journalView_{nullptr};

    QToolBar* tbFile_{nullptr};
    QToolBar* tbEdit_{nullptr};
    QToolBar* tbView_{nullptr};
    QToolBar* tbLayer_{nullptr};
    QToolBar* tbGis_{nullptr};
    QComboBox* layerCombo_{nullptr};
    QFrame* commandLineRule_{nullptr};

    QDockWidget* layerDock_{nullptr};
    QDockWidget* propertyDock_{nullptr};
    QDockWidget* transcriptDock_{nullptr};
    QDockWidget* journalDock_{nullptr};

    QLabel* statusPrompt_{nullptr};
    QLabel* statusCoords_{nullptr};
    QLabel* statusScale_{nullptr};
    QLabel* statusLayer_{nullptr};
    QLabel* statusCrs_{nullptr};

    // ---- komuta karşılık gelen eylemler ----
    QAction* actSelect_{nullptr};
    QAction* actLine_{nullptr};
    QAction* actErase_{nullptr};
    QAction* actLayer_{nullptr};
    QAction* actZoomExtents_{nullptr};
    QAction* actZoomIn_{nullptr};
    QAction* actZoomOut_{nullptr};
    QAction* actUndo_{nullptr};
    QAction* actRedo_{nullptr};
    QAction* actScript_{nullptr};

    // ---- sonraki fazlarda gelecek eylemler, pasif ----
    QAction* actNew_{nullptr};
    QAction* actOpen_{nullptr};
    QAction* actSave_{nullptr};
    QAction* actExport_{nullptr};
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
};

} // namespace piricad::app
