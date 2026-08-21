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

#include "piricad/app/theme.hpp"
#include "piricad/core/units.hpp"

#include <QMainWindow>

class QDockWidget;
class QLabel;
class QPlainTextEdit;

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
    void resetLayout();

private:
    void buildActions();
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

    Controller* controller_{nullptr};
    MapCanvas* canvas_{nullptr};
    CommandLine* commandLine_{nullptr};
    ToolBox* toolBox_{nullptr};
    LayerPanel* layerPanel_{nullptr};
    PropertyPanel* propertyPanel_{nullptr};
    QPlainTextEdit* transcript_{nullptr};
    QPlainTextEdit* journalView_{nullptr};

    QDockWidget* layerDock_{nullptr};
    QDockWidget* propertyDock_{nullptr};
    QDockWidget* transcriptDock_{nullptr};
    QDockWidget* journalDock_{nullptr};

    QLabel* statusPrompt_{nullptr};
    QLabel* statusCoords_{nullptr};
    QLabel* statusScale_{nullptr};
    QLabel* statusLayer_{nullptr};
    QLabel* statusCrs_{nullptr};

    QAction* actSelect_{nullptr};
    QAction* actLine_{nullptr};
    QAction* actErase_{nullptr};
    QAction* actLayer_{nullptr};
    QAction* actMeasure_{nullptr};
    QAction* actPan_{nullptr};
    QAction* actZoomExtents_{nullptr};
    QAction* actZoomIn_{nullptr};
    QAction* actZoomOut_{nullptr};
    QAction* actUndo_{nullptr};
    QAction* actRedo_{nullptr};
    QAction* actScript_{nullptr};
    QAction* actAi_{nullptr};
    QAction* actTheme_{nullptr};
    QAction* actHud_{nullptr};
    QAction* actQuit_{nullptr};

    ThemeMode theme_{ThemeMode::Light}; ///< day mode is the default
};

} // namespace piricad::app
