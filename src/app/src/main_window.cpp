// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/main_window.hpp"

#include "kentos_cad/app/about_dialog.hpp"
#include "kentos_cad/app/ai_transport.hpp"
#include "kentos_cad/app/attribute_panel.hpp"
#include "kentos_cad/app/attribute_table.hpp"
#include "kentos_cad/app/chat_panel.hpp"
#include "kentos_cad/app/command_line.hpp"
#include "kentos_cad/app/command_palette.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/data_root.hpp"
#include "kentos_cad/app/database_dialog.hpp"
#include "kentos_cad/app/export_dialog.hpp"
#include "kentos_cad/app/find_replace_dialog.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/import_wizard.hpp"
#include "kentos_cad/app/layout_designer.hpp"
#include "kentos_cad/app/layout_manager.hpp"
#include "kentos_cad/app/map_canvas.hpp"
#include "kentos_cad/app/panels.hpp"
#include "kentos_cad/app/pick_list.hpp"
#include "kentos_cad/app/print_dialog.hpp"
#include "kentos_cad/app/print_service.hpp"
#include "kentos_cad/app/provider_service.hpp"
#include "kentos_cad/app/python_editor.hpp"
#include "kentos_cad/app/ribbon.hpp"
#include "kentos_cad/app/schema_page.hpp"
#include "kentos_cad/app/settings_dialog.hpp"
#include "kentos_cad/app/shell_chrome.hpp"
#include "kentos_cad/app/style_designer.hpp"
#include "kentos_cad/app/suggestion_card.hpp"
#include "kentos_cad/app/swatch_row.hpp"
#include "kentos_cad/app/text_engine.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/tools_panel.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/app/xref_panel.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/format.hpp"
#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/render/backend.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/colour.hpp"
#include "kentos_cad/command/selection.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/planar.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/text_metrics.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/ties.hpp"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QToolButton>

#include <array>
#include <cmath>
#include <limits>
#include <span>

#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRawFont>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSysInfo>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace kentos::app {

namespace {

/// The two status chips that are a face on the snap MASK rather than a setting of
/// their own: object snap as a whole, and the kutupsal (polar) bit.
constexpr const char* kChipOsnap = "chip.osnap";
constexpr const char* kChipPolar = "chip.polar";

} // namespace

namespace {

/// Property name under which a tool button carries the command it sends.
/// `QAction::data()` is taken by the glyph, and a tool that did not say which
/// command it runs would have to be recognised from a hand-written table — the
/// second command list CLAUDE.md 5.10 forbids.
constexpr const char* kToolCommand = kToolCommandProperty;

/// Whether finishing this tool should ARM IT AGAIN.
///
/// True of every modal tool now — draw, modify, measure. A tool stays in the
/// hand until the hand puts it down: the right button closes the shape and the
/// tool waits for the next one, Esc or the select arrow puts it away. A TAŞI
/// that re-armed used to eat the next click as a selection for a command nobody
/// had asked for; the cure is not to stop re-arming but to CLEAR the selection
/// first, so the re-armed tool asks its question again from nothing
/// (`onInteractiveFinished`).
constexpr const char* kToolRepeats = "piricad.repeats";

/// The same swatch the layer panel draws, so the combo and the panel agree.
/// `1 000 000` — thin-space thousands, the way a Turkish layout prints a scale.
/// Not `QLocale::toString`: that puts a full stop in tr_TR, and the reference
/// (and every map sheet) uses a space.
QString groupedNumber(qint64 value)
{
    QString digits = QString::number(value);
    for (qsizetype at = digits.size() - 3; at > 0; at -= 3)
        digits.insert(at, QLatin1Char(' '));
    return digits;
}

/// Bumped whenever the shell's dock layout changes shape. See `restoreState`.
///
/// 3: the right dock became one 312 px column with a `PanelHeader` on each
/// panel. A state saved by version 2 restores the old sizes and leaves a band of
/// empty window above the Öznitelikler tab — which is what a user saw and
/// reported. A version bump is how `restoreState` declines it.
///
/// 4: the canvas can now be a `QRhiWidget` (CLAUDE.md 8.1), and a central widget
/// that changed class changes the sizes every dock around it was saved against.
/// The symptom is the same one version 3 was bumped for and it was reported the
/// same way: the Öznitelikler panel restored collapsed to a blank thirty-pixel
/// strip, so the drawing looked like the whole shell had come apart. It was the
/// STATE that was stale, not the renderer — which took an afternoon to establish,
/// because a saved layout survives a rebuild and looks exactly like a new bug.
/// 5: the right column is never TABIFIED. `resetLayout` used to tabify the
/// layers panel with the properties panel, which is not what the constructor
/// builds and not what the panels are drawn for: each carries its own
/// `PanelHeader`, and Qt's tab bar over the top of them is a second row of tabs
/// nobody designed — reported as "huge tabs at the top of the right sidebar,
/// meaningless". Any state saved while they were tabbed has to be declined or
/// it comes back on the next start.
/// 6 because the Python console is a NEW BOTTOM DOCK. `restoreState` hides a
/// dock the saved state does not name, and Qt then treats it as floating when it
/// is shown — so on every machine that had opened an earlier build, the console
/// came up as a loose window in the middle of the screen instead of the strip
/// under the canvas. Declining the old state is exactly what this number is for.
constexpr int kLayoutVersion = 6;

/// Whether this process is a probe driving the real shell.
///
/// Named here because the shell is what must not write. See the save path.
bool probe_run()
{
    for (const char* probe :
         {"KENTOS_PRINT_PROBE",    "KENTOS_LAYOUT_PROBE",   "KENTOS_SHOT_DIR",
          "KENTOS_DESIGNER_PROBE", "KENTOS_WIDGETS_PROBE",  "KENTOS_DIALOG_PROBE",
          "KENTOS_HAND_PROBE",     "KENTOS_LAYER_PROBE",    "KENTOS_PICK_PROBE",
          "KENTOS_TABLE_PROBE",    "KENTOS_SCHEMA_PROBE",   "KENTOS_CHAT_PROBE",
          "KENTOS_TOOL_PROBE",     "KENTOS_NORMAL_PROBE",   "KENTOS_FAMILY_PROBE",
          "KENTOS_BUDGET_PROBE",   "KENTOS_PROBE_LINE",     "KENTOS_FRAME_DUMP",
          "KENTOS_MCP_PROBE",      "KENTOS_EDIT_PROBE",     "KENTOS_MENU_PROBE",
          "KENTOS_FIT_PROBE",      "KENTOS_REALMOUSE_PROBE"})
        if (qEnvironmentVariableIsSet(probe)) return true;
    return false;
}

QString format_metres(core::Mm v)
{
    return QString::number(core::mm_to_metres(v), 'f', 3);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent,
                         SARibbonMainWindowStyles(SARibbonMainWindowStyleFlag::UseNativeFrame |
                                                  SARibbonMainWindowStyleFlag::UseRibbonMenuBar))
{
    // ONE STYLESHEET (`.claude/ui.md` R49). SARibbon paints itself with a theme
    // of its own the moment it is built, and once more after the event loop
    // starts; either would sit on the window above the application's sheet and
    // win whatever the specificity. `UserDefine` makes the second pass a no-op
    // and clearing the window's sheet removes the first. The ribbon still wears
    // SARibbon's Office 2021 theme — its base and template, with our colours —
    // from `themeStyleSheet`, like every other surface.
    setRibbonTheme(SARibbonTheme::RibbonThemeUserDefine);
    setStyleSheet(QString());

    // Every button the ribbon makes from here on knows the keyboard (`ribbon.hpp`).
    SARibbonElementManager::instance()->setupFactory(new RibbonElementFactory());
    // And the bar is ours, for the editor tabs' caps (`RibbonBar`); the one the
    // base class built before the factory was ours goes with it.
    setRibbonBar(new RibbonBar(this));

    controller_ = new Controller(this);

    setWindowTitle(tr("KentOSCad — Türkiye Odaklı CBS + CAD"));
    resize(1560, 1000);

    // design.md 7: the frame belongs to the window manager. The shell used to be
    // frameless and drew its own buttons; that cost the resize edges, snapping
    // and the window menu the platform already gives away for free. The ribbon
    // is the menu widget now (`buildRibbon`), under the system's own caption.

    auto* search = new QAction(this);
    search->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    search->setShortcutContext(Qt::ApplicationShortcut);
    connect(search, &QAction::triggered, this, [this] { openCommandSearch(); });
    addAction(search);
    // NO `AllowTabbedDocks`, and that is a decision rather than an omission.
    //
    // Every panel here wears its own `PanelHeader`, which draws the tabs this
    // program has: the properties dock carries three of them over one
    // `QStackedWidget`. Let Qt tabify two DOCKS on top of that and it adds a tab
    // bar of its own above their headers — two rows of tabs, one of them nobody
    // styled, which is what a user reported the moment dragging a panel started
    // working and they could drop one onto another.
    //
    // design.md §6 does want a merge-as-tab drop target eventually; what it
    // describes is tabs drawn in the panel's own language, not Qt's strip. Until
    // that is drawn, dropping a panel on another docks it beside or under.
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks);

    canvas_ = new MapCanvas(*controller_, this);

    commandLine_ = new CommandLine(*controller_, this);
    commandLine_->setObjectName(QStringLiteral("commandLine"));

    // design.md 7: the canvas column and the right dock sit side by side inside
    // the body, and the status bar runs under both. NO DOCUMENT TAB STRIP over
    // the canvas: one drawing is open at a time, its name is in the window's
    // caption, and a strip of one tab was thirty pixels of map spent on a
    // label — the user asked for it gone. Several open drawings bring it back
    // with them (Faz 2).
    commandLineRule_ = new QFrame(this);
    commandLineRule_->setFrameShape(QFrame::HLine);
    commandLineRule_->setFrameShadow(QFrame::Plain);

    buildActions();

    auto* column = new QWidget(this);
    auto* stack  = new QVBoxLayout(column);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    // THE WAY OUT OF A REFUSAL, over the canvas it is about (`offerRemedy`).
    remedyBanner_ = new Banner(Tone::Warn, tr("Bu nesne burada düzenlenemez"), QString(), column);
    remedyButton_ = remedyBanner_->addButton(tr("Yerel Kopya"));
    remedyBanner_->setVisible(false);
    connect(remedyButton_, &QPushButton::clicked, this, [this] {
        const QString line = remedy_;
        remedy_.clear();
        remedyBanner_->setVisible(false);
        if (!line.isEmpty()) runScriptLine(line);
    });
    stack->addWidget(remedyBanner_);
    stack->addWidget(canvas_, 1);
    stack->addWidget(commandLineRule_);
    stack->addWidget(commandLine_);

    auto* central = new QWidget(this);
    auto* body    = new QHBoxLayout(central);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(column, 1);
    setCentralWidget(central);

    buildPanels();
    buildStatusBar();

    // The strip runs under EVERYTHING — tool box, canvas and right dock alike —
    // and QMainWindow's status bar is the one slot that already does. A bottom
    // DOCK is a pixel shorter than its widget, because the dock area keeps a
    // separator between itself and the body; the status bar has no such gap.
    setStatusBar(statusStrip_);
    buildRibbon();

    // The command line is the shell's conversation and design.md 7 draws it as a
    // permanent 28 px strip above the status bar. It was hidden while the tool
    // bars carried the work; the reference puts it back where every CAD user's
    // hand already reaches for it.
    commandLine_->setVisible(true);
    commandLineRule_->setVisible(false);
    // ITS SWITCH SAYS SO. `Görünüm ▸ Pencereler ▸ Komut Satırı` and Ctrl+9 read
    // off while the line was showing, so the first press did nothing a user
    // could see and only the second one hid it.
    {
        const QSignalBlocker quiet(actCommandLine_);
        actCommandLine_->setChecked(true);
    }

    loadPreferences();
    theme_ = themeFromPreferences();
    {
        // Setting the action fires toggled(), which would write the value straight
        // back through the bus; block it while the shell is only catching up with
        // what the store already says.
        QSignalBlocker block(actTheme_);
        actTheme_->setChecked(theme_ == ThemeMode::Dark);
    }
    applyTheme();
    refreshWindowTitle();

    connect(controller_, &Controller::echoed, this, &MainWindow::onEcho);
    connect(controller_, &Controller::documentChanged, this, &MainWindow::onDocumentChanged);
    connect(controller_, &Controller::promptChanged, this, &MainWindow::onPromptChanged);
    connect(controller_, &Controller::interactiveFinished, this,
            &MainWindow::onInteractiveFinished);
    connect(controller_, &Controller::commandFinished, this, &MainWindow::onCommandFinished);
    connect(controller_, &Controller::remedyOffered, this, &MainWindow::offerRemedy);
    // An offer is about the refusal just made: the next command that finishes
    // — the offered one, or any other — has moved past it.
    connect(controller_, &Controller::commandFinished, this, [this] {
        remedy_.clear();
        if (remedyBanner_ != nullptr) remedyBanner_->setVisible(false);
    });

    connect(controller_, &Controller::undoStateChanged, this, &MainWindow::onUndoStateChanged);
    connect(controller_, &Controller::viewRequested, this, &MainWindow::onViewRequested);
    connect(controller_, &Controller::panRequested, this, &MainWindow::onPanRequested);
    connect(controller_, &Controller::settingChanged, this, &MainWindow::onSettingChanged);
    connect(controller_, &Controller::selectionChanged, this, [this] {
        attributePanel_->refresh();
        if (toolsPanel_ != nullptr) toolsPanel_->refresh();
        refreshRibbon();

        // Picking a parcel on the map and then hunting for its layer in a list of
        // forty is work the program can do. Only when the whole selection agrees:
        // with two layers in it there is no single right answer, and moving the
        // highlight to whichever came first would be a guess.
        const command::Selection& picked = controller_->bus().selection();
        const core::Document& doc        = controller_->document();

        core::LayerId only = core::kNoLayer;
        for (core::EntityKey k : picked.keys()) {
            const core::EntityId e = doc.slot_of(k);
            if (e == core::kNoEntity) continue;

            const core::LayerId on = doc.entities().layer[e];
            if (only != core::kNoLayer && on != only) return; // mixed — leave it alone
            only = on;
        }
        if (only != core::kNoLayer) layerPanel_->selectLayer(only);
    });
    connect(controller_, &Controller::selectionChanged, canvas_,
            QOverload<>::of(&MapCanvas::update));
    connect(canvas_, &MapCanvas::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(canvas_, &MapCanvas::viewChanged, this, &MainWindow::refreshStatus);
    connect(canvas_, &MapCanvas::echoRequested, this, &MainWindow::onEcho);
    connect(canvas_, &MapCanvas::pickAmbiguous, this, &MainWindow::choosePick);
    connect(canvas_, &MapCanvas::entityActivated, this, &MainWindow::activateEntity);

    // A FORM FIELD PICKED FROM THE SCENE (tools_panel.hpp `ScenePicker`). The
    // canvas says what landed; the field that asked gets the text the command
    // line would take for it. The status line says what is wanted meanwhile and
    // goes back to the running command's prompt after.
    connect(canvas_, &MapCanvas::captureBegan, this,
            [this](const QString& prompt) { commandLine_->setPrompt(prompt); });
    connect(canvas_, &MapCanvas::pointCaptured, this, [this](core::Point2 world) {
        if (!pendingPick_) return;
        auto done    = std::move(pendingPick_);
        pendingPick_ = nullptr;
        done(QStringLiteral("%1,%2").arg(
            QString::number(static_cast<double>(world.x) / 1000.0, 'f', 3),
            QString::number(static_cast<double>(world.y) / 1000.0, 'f', 3)));
    });
    connect(canvas_, &MapCanvas::objectCaptured, this, [this](core::EntityKey key) {
        if (!pendingPick_) return;
        auto done    = std::move(pendingPick_);
        pendingPick_ = nullptr;
        done(QString::number(static_cast<qulonglong>(core::raw(key))));
    });
    connect(canvas_, &MapCanvas::captureAmbiguous, this, &MainWindow::chooseCapture);

    // THE PRINT FRAME says what it wants on the status line, and clears it when
    // it goes — the same two lines the field pick uses.
    connect(canvas_, &MapCanvas::printFrameBegan, this,
            [this](const QString& prompt) { commandLine_->setPrompt(prompt); });
    connect(canvas_, &MapCanvas::printFrameAccepted, this, [this] {
        // ONE GESTURE, TWO DESTINATIONS. The frame does not know whether a
        // profile or a layout started it; `pendingLayout_` is what remembers,
        // and it is cleared as soon as it is used.
        if (!pendingLayout_.isEmpty())
            layoutWithFrame(pendingLayout_);
        else
            printWithProfile();
    });
    connect(canvas_, &MapCanvas::printFrameEnded, this, [this] {
        const command::Session* session = controller_->session();
        commandLine_->setPrompt(session != nullptr && session->waiting()
                                    ? QString::fromStdString(session->prompt().message)
                                    : QString());
    });
    connect(canvas_, &MapCanvas::captureEnded, this, [this] {
        if (pendingPick_) {
            auto done    = std::move(pendingPick_);
            pendingPick_ = nullptr;
            done(std::nullopt);
        }
        const command::Session* session = controller_->session();
        commandLine_->setPrompt(session != nullptr && session->waiting()
                                    ? QString::fromStdString(session->prompt().message)
                                    : QString());
    });

    // Enter on an empty command line is "done pointing". Focus is here far more
    // often than on the canvas, so without this the gesture had nowhere to land.
    connect(commandLine_, &CommandLine::accepted, this, [this] {
        // AN EMPTY ENTER, in the order the three things it can mean are asked
        // for: those objects · that wanted area · that is the shape, done.
        if (controller_->supplyPickedObjects()) return;
        if (canvas_->acceptGuide()) return;
        (void)canvas_->finishPointRun();
    });
    connect(commandLine_, &CommandLine::submitted, this, &MainWindow::onCommandSubmitted);
    connect(layerPanel_, &LayerPanel::layerSelected, attributePanel_, &AttributePanel::setLayer);
    connect(layerPanel_, &LayerPanel::propertiesRequested, this, &MainWindow::openStyleDesigner);
    connect(layerPanel_, &LayerPanel::attributeTableRequested, this,
            [this](const QString& layer) { openAttributeTable(layer); });

    onEcho(tr("KentOSCad %1 — komut merkezli mimari, GPLv3.").arg(QStringLiteral(KENTOS_VERSION)));
    onEcho(tr("Aynı komut arayüzden, komut satırından ve betikten tıpatıp aynı yolu izler."));
    if (const std::string status = render::text_backend_status(); !status.empty())
        onEcho(tr("Not: %1").arg(QString::fromStdString(status)));
    if (const std::string status = render::gpu_backend_status(); !status.empty())
        onEcho(tr("Not: %1").arg(QString::fromStdString(status)));
    onEcho(tr("Başlamak için: ÇİZGİ  ·  ÇİZGİ 485320,4310220 @50,30 @100<45  ·  YARDIM"));

    // Persistence is installed as a HOOK rather than done in the destructor. It
    // used to run only when the main window closed, so `TERCİH` from a script
    // changed the value for that run and lost it while the same line from the
    // menu survived. Now every client's write persists, because none of them
    // does it — this does (Article 1.2).
    controller_->bus().on_settings_changed = [this](core::SettingScope scope) {
        if (scope == core::SettingScope::App) savePreferences();
    };

    loadSymbolLibrary();

    syncDockTitles();
    refreshAidActions();
    onDocumentChanged();
    commandLine_->setFocus();
}

MainWindow::~MainWindow()
{
    // EVERY CONNECTION INTO THIS WINDOW IS CUT FIRST, and this is not tidiness.
    //
    // A shell owns its docks, its canvas and its panels as CHILDREN, so Qt
    // deletes them from `~QObject` — which runs after this body and after every
    // member of MainWindow has been destroyed. A child that emits on the way out
    // (a dock hiding emits `visibilityChanged`) then reaches a slot on a
    // MainWindow whose lifetime has ended: UBSan calls it a member call on an
    // address that is not a MainWindow, and what it is in practice is a write or
    // a call into memory that now belongs to something else. That is the shape of
    // a crash "at a meaningless point" — the damage is done at shutdown and lands
    // wherever the stack happens to be reused.
    //
    // Qt severs these in `~QObject`, which is too late by exactly the window this
    // destructor opens. Doing it here closes that window.
    //
    // BY SENDER, one at a time. `QObject::disconnect(nullptr, …)` looks like it
    // would say "from anyone", and it does nothing at all: Qt documents the
    // sender as the one argument that may not be a wildcard. The docks are the
    // senders that emit on the way out, and they are named here for that reason.
    for (QDockWidget* dock : {propertyDock_, layerDock_, journalDock_})
        if (dock != nullptr) dock->disconnect(this);

    // AND THE BUS HOOK THAT DRAWS INTO THIS WINDOW'S CANVAS, for the same
    // reason: the controller may outlive the shell by a command or two.
    if (controller_ != nullptr) controller_->bus().on_measure_mark = nullptr;

    // A PROBE RUN WRITES NOTHING. Every probe drives the REAL shell, and the
    // shell saves its preferences, its geometry and its dock layout here on the
    // way out — so a probe that floats a panel and re-docks it saved THAT over
    // the person's own arrangement, and they opened the program next to find
    // the right column rebuilt around whatever the test had left.
    //
    // `QStandardPaths::setTestModeEnabled` does not cover this: on macOS
    // `QSettings` writes through CFPreferences, which test mode does not
    // redirect. The only reliable guard is not writing.
    if (isProbeRun()) return;

    savePreferences();

    // Window geometry and dock layout are not declared settings and deliberately
    // so: they are opaque per-machine blobs with no SettingSpec, no range and no
    // meaning to a user typing TERCİH. They stay raw QSettings keys.
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/state"), saveState(kLayoutVersion));
}

void MainWindow::loadPreferences()
{
    // Generated from the catalogue, never a hand-written key list: a new App-scope
    // SettingSpec is persisted by this loop the day it is declared.
    core::Settings& store = controller_->bus().app_settings();
    QSettings file;

    for (const auto& spec : store.catalogue().all()) {
        if (spec.scope != core::SettingScope::App) continue;

        const QString key    = QString::fromStdString(spec.id);
        const QVariant saved = file.value(key);
        if (!saved.isValid()) continue;

        // R42: a value the running build cannot honour is clamped with a recorded
        // warning, never a hard failure — a preferences file written by another
        // version must not stop the application from opening.
        auto parsed = core::parse_setting(spec, saved.toString().toStdString());
        if (!parsed) continue;
        (void)store.set(spec.id, parsed.value());
    }
}

void MainWindow::savePreferences()
{
    const core::Settings& store = controller_->bus().app_settings();
    QSettings file;

    // Only what the user actually set. Writing the defaults too would freeze
    // today's default into every profile and make changing one a no-op.
    for (const std::string& id : store.explicit_ids()) {
        const std::uint32_t index = store.catalogue().find(id);
        if (index == core::kNoSetting) continue;

        const core::SettingSpec& spec = store.catalogue().at(index);
        file.setValue(QString::fromStdString(spec.id),
                      QString::fromStdString(core::format_setting(spec, store.get(id))));
    }
}

ThemeMode MainWindow::themeFromPreferences() const
{
    // "sistem" is the declared default and has no detection behind it yet, so it
    // resolves to the day theme — the same answer the old hard-coded default gave.
    const core::Settings& store = controller_->bus().app_settings();
    return core::format_setting(store.catalogue().at(store.catalogue().find("core.arayuz.tema")),
                                store.get("core.arayuz.tema")) == "koyu"
               ? ThemeMode::Dark
               : ThemeMode::Light;
}

// EVERY ACTION THAT RUNS A COMMAND SAYS WHICH ONE. It is not decoration: the
// flyout prints the word beside the name so the mouse teaches the keyboard,
// `syncToolSelection` lights the right button when the same command arrives from
// the prompt or a script, and `KENTOS_TOOL_PROBE` answers "how much of this
// program can a hand reach" by walking the actions rather than by a hand-kept
// list of what has a button — which would be the second command list CLAUDE.md
// 5.10 exists to forbid.
/// Whether this command's whole answer is WORDS, so the transcript has to be in
/// front of somebody before it runs.
///
/// Asked of the registry rather than decided per call site: a query answers by
/// writing and changes nothing on the canvas, so with the transcript closed —
/// which is how the window starts — pressing one looked exactly like pressing a
/// dead menu row. The user said so in those words. Deciding it here instead of at
/// each `modifyTool`/`commandAction` call keeps it ONE decision (CLAUDE.md 5.10):
/// a command that becomes read-only, or a new one that lands read-only, gets the
/// panel without anybody remembering to ask for it.
bool MainWindow::answersInWords(const QString& command) const
{
    const command::CommandSpec* spec = controller_->registry().resolve(command.toStdString());
    return spec != nullptr && has_flag(spec->flags, command::Flags::ReadOnly) &&
           spec->category != command::Category::View && spec->category != command::Category::File;
}

QAction* MainWindow::commandAction(Glyph glyph, const QString& text, const QString& line,
                                   const QString& tip, const QKeySequence& shortcut)
{
    auto* action = new QAction(text, this);
    action->setToolTip(tip);
    action->setStatusTip(tip);
    action->setData(static_cast<int>(glyph));
    if (!shortcut.isEmpty()) action->setShortcut(shortcut);

    // The first word of the line is the command a user would type, and the tool
    // flyout prints it beside the name. Without it a family row would name a tool
    // and then leave the column that teaches its name blank.
    action->setProperty(kToolCommand, line.section(QLatin1Char(' '), 0, 0));

    connect(action, &QAction::triggered, this, [this, line] {
        // WHERE THE ANSWER LANDS HAS TO BE VISIBLE. A query command answers by
        // writing to the transcript and changes nothing on the canvas, so with
        // the transcript panel closed — which is how it starts — pressing a
        // listing or an info command looked exactly like pressing a dead menu
        // row. The user said as much: they could not tell why the items were
        // there.
        //
        // The panel is brought up BEFORE the line runs, so the answer arrives in
        // front of somebody. Only for a command that answers in words: one that
        // draws, moves or deletes shows its work on the canvas and a panel
        // opening over it would be in the way.
        if (answersInWords(line.section(QLatin1Char(' '), 0, 0))) showTranscript();
        controller_->runLine(line, command::Origin::Gui);
    });
    return action;
}

/// A construction METHOD as a tool: the whole line on the button.
///
/// P2 gave DAİRE, YAY, DİKDÖRTGEN, ÇOKGEN and ELİPS their classical methods and
/// every one of them was reachable ONLY by typing `yontem=`: a hand could not
/// draw a three-point circle at all. The command is on a button, so `probeReach`
/// was satisfied and the method was not — which is the shape of CLAUDE.md 5.15
/// one level down, and §2.6a says a tool arrives with its interface.
///
/// The WHOLE LINE rides in `kToolCommand`, not just the first word. The flyout
/// prints that property in its right-hand column, so the card teaches
/// `YAY yontem=3n` — the thing a user should learn — instead of printing `YAY`
/// five times. `probeReach` and `Controller::runCommand` both take the first word
/// out themselves, so nothing downstream needs telling.
QAction* MainWindow::methodTool(Glyph glyph, const QString& text, const QString& line,
                                const QString& tip)
{
    auto* action = new QAction(text, this);
    action->setCheckable(true);
    action->setToolTip(tip);
    action->setStatusTip(tip);
    action->setData(static_cast<int>(glyph));
    action->setProperty(kToolCommand, line);
    action->setProperty(kToolRepeats, true);
    action->setObjectName(QStringLiteral("toolAction.") + line);
    drawingTools_->addAction(action);
    connect(action, &QAction::triggered, this, [this, line] { controller_->runCommand(line); });
    return action;
}

QAction* MainWindow::modifyTool(Glyph glyph, const QString& text, const QString& command,
                                const QString& tip)
{
    auto* action = new QAction(text, this);
    action->setToolTip(tip);
    action->setStatusTip(tip);
    action->setData(static_cast<int>(glyph));
    action->setProperty(kToolCommand, command);
    action->setObjectName(QStringLiteral("toolAction.") + command);

    // CHECKABLE AND IN THE EXCLUSIVE GROUP, exactly like a draw tool — because it
    // IS one. These used to be neither, and the consequences compounded:
    //
    //   * the button could not show a pressed state at all, so pressing Buda gave
    //     no answer of any kind;
    //   * `syncToolSelection` only ever lights an action inside `drawingTools_`,
    //     so it could never light these;
    //   * and worse, its fallback lights the SELECT ARROW. So while BUDA was
    //     genuinely armed and waiting for a click, the tool column positively
    //     asserted that no tool was running.
    //
    // Together that is a tool that "cannot be selected and does not work", which
    // is exactly how it was reported.
    action->setCheckable(true);
    action->setProperty(kToolRepeats, true);
    drawingTools_->addAction(action);

    // NO SELECTION GUARD. It used to refuse an empty selection with a sentence in
    // the status line and never start the command — so the ordinary order of work,
    // reach for the tool and then point at the thing, did nothing at all. The
    // commands ask for their objects now (`want_objects`), which is the order
    // every CAD trains and the one a script never sees.
    connect(action, &QAction::triggered, this, [this, command] {
        // A READ-ONLY TOOL ANSWERS IN WORDS, and this road used to skip the
        // panel that `commandAction` opens — so ALANÖLÇ and NESNEBİLGİ armed
        // from the tool column, did their work, and printed the answer into a
        // closed drawer.
        if (answersInWords(command)) showTranscript();
        controller_->runCommand(command);
    });
    return action;
}

QAction* MainWindow::placeholder(Glyph glyph, const QString& text, const QString& command,
                                 const QString& phase, const QString& explains)
{
    auto* action = new QAction(text, this);
    action->setData(static_cast<int>(glyph));

    const QString tip = command.isEmpty() ? tr("%1 — %2'de gelecek").arg(text, phase)
                                          : tr("%1 — %2'de gelecek").arg(command, phase);
    action->setToolTip(tip);
    action->setStatusTip(tip);

    // LIVE, AND IT SAYS WHY IT IS THERE. A disabled row answers nothing when it
    // is clicked, so an entry for work that has not landed read as an entry that
    // was simply broken. It now opens a box naming the feature, the phase it
    // arrives in and what to use today — which is what a user asked for when
    // they could not tell what these items were for.
    connect(action, &QAction::triggered, this, [this, text, command, phase, explains] {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(text);
        box.setText(tr("<b>%1</b> — henüz yok, %2'de gelecek.").arg(text, phase));
        box.setInformativeText(explains);
        box.setStandardButtons(QMessageBox::Ok);
        applyThemeToChildren(&box, theme_);
        box.exec();
    });
    return action;
}

void MainWindow::buildActions()
{
    // ---- dosya ----
    actNew_ = new QAction(tr("Yeni"), this);
    actNew_->setShortcut(QKeySequence::New);
    actNew_->setToolTip(tr("YENİ — boş bir çizim açar"));
    actNew_->setData(static_cast<int>(Glyph::New));
    actNew_->setProperty(kToolCommand, QStringLiteral("YENİ"));
    connect(actNew_, &QAction::triggered, this, &MainWindow::newProject);

    actOpen_ = new QAction(tr("Aç…"), this);
    actOpen_->setShortcut(QKeySequence::Open);
    actOpen_->setToolTip(tr("AÇ — bir KentOSCad proje dosyası açar"));
    actOpen_->setData(static_cast<int>(Glyph::Open));
    actOpen_->setProperty(kToolCommand, QStringLiteral("AÇ"));
    connect(actOpen_, &QAction::triggered, this, &MainWindow::openProject);

    actSave_ = new QAction(tr("Kaydet"), this);
    actSave_->setShortcut(QKeySequence::Save);
    actSave_->setToolTip(tr("KAYDET — çizimi bağlı olduğu dosyaya yazar"));
    actSave_->setData(static_cast<int>(Glyph::Save));
    actSave_->setProperty(kToolCommand, QStringLiteral("KAYDET"));
    connect(actSave_, &QAction::triggered, this, &MainWindow::saveProject);

    actSaveAs_ = new QAction(tr("Farklı Kaydet…"), this);
    actSaveAs_->setShortcut(QKeySequence::SaveAs);
    actSaveAs_->setToolTip(tr("FARKLIKAYDET — çizimi yeni bir dosyaya yazar"));
    actSaveAs_->setData(static_cast<int>(Glyph::Save));
    actSaveAs_->setProperty(kToolCommand, QStringLiteral("FARKLIKAYDET"));
    connect(actSaveAs_, &QAction::triggered, this, &MainWindow::saveProjectAs);

    actImport_ = new QAction(tr("İçe Aktar…"), this);
    actImport_->setToolTip(tr("İÇEAKTAR — dış bir veri dosyasını çizime ekler"));
    actImport_->setData(static_cast<int>(Glyph::Import));
    actImport_->setProperty(kToolCommand, QStringLiteral("İÇEAKTAR"));
    connect(actImport_, &QAction::triggered, this, &MainWindow::importData);

    actExport_ = new QAction(tr("Dışa Aktar…"), this);
    actExport_->setToolTip(tr("DIŞAAKTAR — çizimi dış bir veri biçimine yazar"));
    actExport_->setData(static_cast<int>(Glyph::Export));
    actExport_->setProperty(kToolCommand, QStringLiteral("DIŞAAKTAR"));
    connect(actExport_, &QAction::triggered, this, &MainWindow::exportData);

    // YAZDIR, and it is two presses rather than one window: the first opens the
    // sheet-shaped frame on the canvas so the user can aim, the second captures
    // what is in it (see `printWithProfile`). The arrow beside the button opens
    // the profile list.
    actPrint_ = new QAction(tr("Yazdır"), this);
    actPrint_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    actPrint_->setToolTip(tr("YAZDIR — yazdırma alanını aç, sonra önizlemeye geç (Ctrl+P)"));
    actPrint_->setData(static_cast<int>(Glyph::Print));
    actPrint_->setProperty(kToolCommand, QStringLiteral("YAZDIR"));
    connect(actPrint_, &QAction::triggered, this, [this] { printWithProfile(); });

    actPrintMenu_ = new QAction(tr("Yazdırma profilleri"), this);
    actPrintMenu_->setToolTip(tr("Yazdırma profilleri — hangi kâğıda, hangi çözünürlükte"));
    actPrintMenu_->setData(static_cast<int>(Glyph::ChevronDown));

    actScript_ = new QAction(tr("Betik Çalıştır…"), this);
    actScript_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actScript_->setToolTip(tr("BETİK — bir JSON betiğini komut veri yolundan çalıştırır"));
    actScript_->setData(static_cast<int>(Glyph::Script));
    actScript_->setProperty(kToolCommand, QStringLiteral("BETİK"));
    connect(actScript_, &QAction::triggered, this, &MainWindow::openScript);

    // THE DRY RUN BESIDE THE RUN (TODOS F-05): what a script would change, said
    // before anything is — the same preview the suggestion card shows.
    actScriptPreview_ = new QAction(tr("Betiği Önizle…"), this);
    actScriptPreview_->setToolTip(
        tr("BETİK onizle=evet — bir JSON betiğinin çizimde ne değiştireceğini, çizime "
           "dokunmadan söyler"));
    actScriptPreview_->setData(static_cast<int>(Glyph::Script));
    actScriptPreview_->setProperty(kToolCommand, QStringLiteral("BETİK"));
    connect(actScriptPreview_, &QAction::triggered, this, &MainWindow::openScriptPreview);

    actDatabase_ = new QAction(tr("Veritabanı…"), this);
    actDatabase_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    actDatabase_->setToolTip(tr("VERİTABANI — PostGIS sunucusuna bağlanır, katmanları tablo, "
                                "projeleri kayıt olarak yazar"));
    actDatabase_->setData(static_cast<int>(Glyph::Database));
    actDatabase_->setProperty(kToolCommand, QStringLiteral("VERİTABANI"));
    connect(actDatabase_, &QAction::triggered, this, &MainWindow::openDatabase);

    // THE PROJECT'S OWN WINDOW, in the Dosya menu because the project IS the
    // file: what it holds travels with the file and nothing on that window is
    // about this computer.
    actProjectSettings_ = new QAction(tr("Proje Ayarları…"), this);
    actProjectSettings_->setData(static_cast<int>(Glyph::ProjectSettings));
    actProjectSettings_->setToolTip(
        tr("Çizimle birlikte giden ayarlar ve projenin öznitelik sütunları"));
    actProjectSettings_->setProperty(kToolCommand, QStringLiteral("AYAR"));
    connect(actProjectSettings_, &QAction::triggered, this, &MainWindow::openProjectSettings);

    actSettings_ = new QAction(tr("Seçenekler…"), this);
    actSettings_->setData(static_cast<int>(Glyph::Settings));
    actSettings_->setShortcut(QKeySequence::Preferences);
    actSettings_->setToolTip(tr("Bildirilen her ayarı kapsamına göre gösterir; her "
                                "değişiklik AYAR, TERCİH ya da MOD komutu olarak geçer"));
    actSettings_->setProperty(kToolCommand, QStringLiteral("TERCİH"));
    connect(actSettings_, &QAction::triggered, this, &MainWindow::openSettings);

    actQuit_ = new QAction(tr("Çıkış"), this);
    actQuit_->setShortcut(QKeySequence::Quit);
    connect(actQuit_, &QAction::triggered, qApp, &QApplication::quit);

    // ---- seçim ve çizim ----
    actSelect_ = new QAction(tr("Seç"), this);
    actSelect_->setCheckable(true);
    actSelect_->setChecked(true);
    actSelect_->setToolTip(
        tr("Seçim aracı — eldeki aracı bırakır, çalışan komutu iptal eder (Esc)"));
    actSelect_->setData(static_cast<int>(Glyph::Select));
    connect(actSelect_, &QAction::triggered, this, [this] { controller_->cancelInteractive(); });

    actLine_ = new QAction(tr("Çizgi"), this);
    actLine_->setCheckable(true);
    actLine_->setToolTip(tr("ÇİZGİ — ardışık doğru parçaları çizer; her parça AYRI nesne, tek tek "
                            "seçilir (bütünü tek nesne için ÇOKLUÇİZGİ)  ·  kısaltma: Ç, L"));
    actLine_->setData(static_cast<int>(Glyph::Line));
    actLine_->setProperty(kToolCommand, QStringLiteral("ÇİZGİ"));
    actLine_->setProperty(kToolRepeats, true); // the archetypal repeating draw tool
    actLine_->setObjectName(QStringLiteral("toolAction.ÇİZGİ"));
    connect(actLine_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("ÇİZGİ")); });

    // A modal draw tool is a CHECKABLE action that runs its command; the command
    // is the feature and the button is one client of it (Article 1.2). Anything
    // that only ever echoes "Faz 2" stays a `placeholder`, disabled, so the tool
    // box never offers a button that does nothing.
    const auto drawTool = [this](Glyph glyph, const QString& text, const QString& command,
                                 const QString& tip) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setToolTip(tip);
        action->setData(static_cast<int>(glyph));

        // The command the button sends, kept ON the button. `data()` already
        // carries the glyph, so this rides as a dynamic property rather than
        // displacing it. `syncToolSelection` resolves it through `Registry` to
        // recognise the running command, which is why nothing here needs a second
        // table mapping tools to commands (CLAUDE.md 5.10).
        action->setProperty(kToolCommand, command);
        action->setProperty(kToolRepeats, true);

        // Named so a test can reach the button a user would press. Nothing in the
        // shell looks an action up by name; this exists for `KENTOS_EDIT_PROBE`,
        // which drives the tool column the way a hand does.
        action->setObjectName(QStringLiteral("toolAction.") + command);

        connect(action, &QAction::triggered, this,
                [this, command] { controller_->runCommand(command); });
        return action;
    };

    actPolygon_ = drawTool(Glyph::Polygon, tr("Alan"), QStringLiteral("ALAN"),
                           tr("ALAN — kapalı bir alan çizer  ·  kısaltma: AL"));
    actRegular_ = drawTool(Glyph::Polygon, tr("Düzgün Çokgen"), QStringLiteral("ÇOKGEN"),
                           tr("ÇOKGEN — merkez ve kenar sayısından düzgün çokgen; içten, dıştan "
                              "ya da kenar uzunluğundan  ·  kısaltma: ÇKG"));
    // The group is created below, so this one joins it there with the others: an
    // `addAction` on a null group is a crash rather than a misplaced tool, and
    // the layer-menu probe found it as a segfault rather than as a wrong menu.
    actRectangle_ =
        drawTool(Glyph::Rectangle, tr("Dikdörtgen"), QStringLiteral("DİKDÖRTGEN"),
                 tr("DİKDÖRTGEN — karşılıklı iki köşeden çizer; Ctrl basılıyken kare  ·  "
                    "kısaltma: DKD"));

    drawingTools_ = new QActionGroup(this);
    drawingTools_->setExclusive(true);
    drawingTools_->addAction(actSelect_);
    drawingTools_->addAction(actLine_);
    drawingTools_->addAction(actPolygon_);
    drawingTools_->addAction(actRectangle_);
    drawingTools_->addAction(actRegular_);

    actPolyline_ = drawTool(Glyph::Polyline, tr("Çoklu Çizgi"), QStringLiteral("ÇOKLUÇİZGİ"),
                            tr("ÇOKLUÇİZGİ — çok köşeli TEK çizgi nesnesi; bir tıkla bütünü "
                               "seçilir (parçaları ayrı nesne için ÇİZGİ)  ·  kısaltma: ÇÇ"));
    drawingTools_->addAction(actPolyline_);
    actArc_ = drawTool(Glyph::Arc, tr("Yay"), QStringLiteral("YAY"),
                       tr("YAY — merkez ve iki uçtan yay çizer; süpürme saat yönünün "
                          "tersine  ·  kısaltma: YY"));
    drawingTools_->addAction(actArc_);
    actCircle_ = drawTool(Glyph::Circle, tr("Daire"), QStringLiteral("DAİRE"),
                          tr("DAİRE — merkez ve çember noktasından daire çizer  ·  "
                             "kısaltma: DR"));

    // Into the exclusive group like every other modal tool, so exactly one stays
    // lit and `syncToolSelection` can find it by the command it sends.
    drawingTools_->addAction(actCircle_);
    // IN THE MENU, NOT THE TOOL COLUMN. The column is a curated 46 px strip of the
    // tools a hand reaches for constantly, and these two are shapes a plan sheet
    // asks for occasionally — a junction fillet, a protection band around a well.
    // Giving them a column button each would also mean two more glyphs sharing a
    // circle, which reads as one control drawn twice.
    actSector_ = drawTool(Glyph::Sector, tr("Daire Dilimi"), QStringLiteral("DİLİM"),
                          tr("DİLİM — merkez ve iki kenardan daire dilimi  ·  kısaltma: DL"));
    drawingTools_->addAction(actSector_);
    actEllipse_ = drawTool(Glyph::Ellipse, tr("Elips"), QStringLiteral("ELİPS"),
                           tr("ELİPS — merkez ve iki eksenden elips; ikinci eksen birincisine "
                              "diktir  ·  kısaltma: EL"));
    drawingTools_->addAction(actEllipse_);
    actAnnulus_ = drawTool(Glyph::Annulus, tr("Halka"), QStringLiteral("HALKA"),
                           tr("HALKA — merkez, iç ve dış yarıçaptan delikli halka  ·  "
                              "kısaltma: HLK"));
    drawingTools_->addAction(actAnnulus_);

    actIntersect_ =
        drawTool(Glyph::PointIntersect, tr("Kesişim Noktası"), QStringLiteral("KESİŞİMNOKTA"),
                 tr("KESİŞİMNOKTA — iki doğrultu, iki uzaklık ya da iki doğrunun "
                    "kesişimi; yöntem yontem= ile  ·  kısaltma: KSN"));
    drawingTools_->addAction(actIntersect_);
    actAlong_ = drawTool(Glyph::PointAlong, tr("Ara Nokta"), QStringLiteral("ARANOKTA"),
                         tr("ARANOKTA — doğru üzerinde oran, uzaklık ya da sayi= ile eşit "
                            "bölme  ·  kısaltma: ARN"));
    drawingTools_->addAction(actAlong_);
    actSurvey_ = drawTool(Glyph::Survey, tr("Alım"), QStringLiteral("ALIM"),
                          tr("ALIM — istasyon, sonra açı ve kenar çiftleri; bağlama verilirse "
                             "açılar ondan itibaren  ·  kısaltma: ALM"));
    drawingTools_->addAction(actSurvey_);
    actPerpOffset_ =
        drawTool(Glyph::PerpOffset, tr("Dik Ayak"), QStringLiteral("DİKAYAK"),
                 tr("DİKAYAK — taban çizgisi, sonra ayak ve boy çiftleri; A→B yönünde SOL "
                    "pozitiftir  ·  kısaltma: DA"));
    drawingTools_->addAction(actPerpOffset_);
    actPoint_ = drawTool(Glyph::Point, tr("Nokta"), QStringLiteral("NOKTA"),
                         tr("NOKTA — ölçülmüş nokta: nirengi, poligon noktası, röper  ·  "
                            "kısaltma: NK"));
    drawingTools_->addAction(actPoint_);
    actText_ = drawTool(Glyph::Text, tr("Metin"), QStringLiteral("METİN"),
                        tr("METİN — çizime yazı yazar  ·  kısaltma: MT"));
    drawingTools_->addAction(actText_);

    // ---- Faz 2 türleri: spline, tarama, blok, ölçü, lider ----
    actSpline_ = drawTool(Glyph::Spline, tr("Spline"), QStringLiteral("SPLINE"),
                          tr("SPLINE — kontrol noktalarından pürüzsüz eğri  ·  kısaltma: SPL"));
    drawingTools_->addAction(actSpline_);
    // TARAMA and BLOK work on objects, so they go the way BUDA does: the command
    // asks for its objects when nothing is selected (`want_objects`).
    actHatch_ = modifyTool(Glyph::Hatch, tr("Tarama"), QStringLiteral("TARAMA"),
                           tr("TARAMA — kapalı nesnelerin içini katalogdaki bir desenle tarar; "
                              "tarama sınırına bağlıdır, sınır değişince güncellenir  ·  "
                              "kısaltma: TRM"));
    // THE HATCH'S OWN EDIT (TODOS C-11): pattern, angle, scale, spacing, origin
    // and island rule of a hatch already drawn, keeping its tie to its parcel.
    actHatchEdit_ =
        modifyTool(Glyph::Hatch, tr("Taramayı Düzenle"), QStringLiteral("TARAMADÜZENLE"),
                   tr("TARAMADÜZENLE — taramayı seçin, desenini yazın; açı, ölçek, "
                      "aralık, başlangıç ve ada kuralı nitelik panelinde  ·  "
                      "kısaltma: TDZ"));
    actBlock_  = modifyTool(Glyph::Duplicate, tr("Blok"), QStringLiteral("BLOK"),
                            tr("BLOK — seçilen nesnelerden adlı blok tanımlar ve yerine bir "
                                "referans koyar  ·  kısaltma: BLK"));
    actInsert_ = drawTool(Glyph::BlockInsert, tr("Blok Ekle"), QStringLiteral("BLOKEKLE"),
                          tr("BLOKEKLE — tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle "
                             "yerleştirir  ·  kısaltma: BE"));
    drawingTools_->addAction(actInsert_);
    // THE DEFINITION EDITED ON THE SHEET (TODOS C-13): the block's members out
    // as ordinary objects where the reference stands, and back in with the
    // ribbon's save — every reference then draws the new picture. A double
    // click on a block runs it (`activateEntity`).
    actBlockEdit_ = modifyTool(Glyph::BlockEdit, tr("Bloğu Düzenle"), QStringLiteral("BLOKDÜZENLE"),
                               tr("BLOKDÜZENLE — seçili bloğun tanımını düzenlemeye açar; Bloğu "
                                  "Kaydet ile bütün referanslar yeni biçimi çizer  ·  "
                                  "kısaltma: BDZ"));
    // THE BASE POINT, shown on the selected reference; the references stay
    // where they are drawn and the next insertion stands on the new point.
    actBlockBase_ =
        methodTool(Glyph::BlockBase, tr("Taban Noktası"), QStringLiteral("BLOKDÜZENLE islem=taban"),
                   tr("BLOKDÜZENLE islem=taban — seçili bloğun taban noktasını "
                      "taşır; referanslar çizildikleri yerde kalır  ·  kısaltma: BDZ"));
    // A BLOCK FROM A LIBRARY FILE: the file is chosen here, and the rest is
    // BLOKEKLE's — which block when the file holds several, where, and each
    // field's value — asked as it asks for a block of the drawing's own.
    actBlockLibrary_ = new QAction(tr("Kitaplıktan Ekle"), this);
    actBlockLibrary_->setData(static_cast<int>(Glyph::BlockInsert));
    // No `kToolCommand`: the press opens a file window before any command runs,
    // like Proje Aç, and a probe that presses every tool must not be held by it.
    actBlockLibrary_->setObjectName(QStringLiteral("blockLibrary"));
    actBlockLibrary_->setToolTip(tr("BLOKEKLE dosya= — bir proje, DXF ya da DWG dosyasındaki "
                                    "bloğu bu çizime getirir ve yerleştirir"));
    actBlockLibrary_->setStatusTip(actBlockLibrary_->toolTip());
    connect(actBlockLibrary_, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Blok kitaplığı"), QFileInfo(controller_->currentFile()).absolutePath(),
            tr("Blok kitaplığı (*.pcad *.dxf *.dwg);;Tüm dosyalar (*)"));
        if (path.isEmpty()) return;
        controller_->runCommand(QStringLiteral("BLOKEKLE dosya=\"%1\"").arg(path));
    });
    // AN EXTERNAL REFERENCE (TODOS C-14): the file is chosen here and the rest
    // is DIŞREFERANS's — read, defined, and placed in the file's own
    // coordinates, which is where a base map or a neighbouring sheet belongs.
    actXref_ = new QAction(tr("Dış Referans"), this);
    actXref_->setData(static_cast<int>(Glyph::Xref));
    // No `kToolCommand`, for the library button's reason: the press opens a
    // file window before any command runs.
    actXref_->setObjectName(QStringLiteral("xrefAttach"));
    actXref_->setToolTip(tr("DIŞREFERANS — bir proje, DXF, DWG ya da CBS dosyasını (GeoPackage, "
                            "Shapefile) dış referans olarak bağlar: yerinde çizilir, düzenlenmez, "
                            "dosyası değişince yenilenir  ·  kısaltma: DRF"));
    actXref_->setStatusTip(actXref_->toolTip());
    connect(actXref_, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Dış referans"), QFileInfo(controller_->currentFile()).absolutePath(),
            tr("Çizim ya da CBS dosyası (*.pcad *.dxf *.dwg *.gpkg *.shp);;Tüm dosyalar (*)"));
        if (path.isEmpty()) return;
        controller_->runLine(QStringLiteral("DIŞREFERANS dosya=\"%1\"").arg(path),
                             command::Origin::Gui);
    });
    actXrefReload_ = commandAction(Glyph::XrefReload, tr("Dış Referansları Yenile"),
                                   QStringLiteral("DIŞREFERANS islem=yenile"),
                                   tr("DIŞREFERANS islem=yenile — bağlı dış referansları "
                                      "dosyalarından yeniden okur"));
    actXrefReload_->setObjectName(QStringLiteral("xrefReload"));
    // A LINKED FILE'S OBJECTS, COPIED AS THIS DRAWING'S (TODOS F-02): the way
    // out every refused edit of one offers, as a tool of its own too.
    actLocalCopy_ = modifyTool(
        Glyph::XrefLocalCopy, tr("Yerel Kopya"), QStringLiteral("YERELKOPYA"),
        tr("YERELKOPYA — bir dış referanstaki nesnelerin düzenlenebilir kopyalarını bu "
           "çizime alır, kaynak katman adlarıyla; bağlantı yerinde kalır  ·  kısaltma: YK"));
    actLocalCopy_->setObjectName(QStringLiteral("xrefLocalCopy"));
    // A REFERENCE CLIPPED (TODOS C-14): it shows what a boundary holds — two
    // corners of a rectangle, a polygon's corners, or a closed object already
    // on the drawing — and the boundary can be drawn out and taken off again.
    // Each is BLOKKIRP's own line, so the button and the typed line are one.
    actBlockClip_ =
        modifyTool(Glyph::BlockClip, tr("Kırp"), QStringLiteral("BLOKKIRP"),
                   tr("BLOKKIRP — blok ya da dış referansı iki köşeli bir dikdörtgenle "
                      "kırpar: içi çizilir, dışı çizilmez ve yakalanmaz  ·  kısaltma: BKR"));
    actBlockClipPolygon_ = modifyTool(
        Glyph::BlockClipPolygon, tr("Çokgenle Kırp"), QStringLiteral("BLOKKIRP tur=cokgen"),
        tr("BLOKKIRP tur=cokgen — sınırı köşe köşe çizerek kırpar; Enter sınırı kapatır"));
    actBlockClipObject_ = modifyTool(
        Glyph::BlockClipObject, tr("Nesneyle Kırp"), QStringLiteral("BLOKKIRP tur=cizgi"),
        tr("BLOKKIRP tur=cizgi — çizimdeki kapalı bir çizgi, alan, daire ya da "
           "elipsle kırpar"));
    actBlockClipBoundary_ = modifyTool(
        Glyph::BlockClipBoundary, tr("Kırpma Sınırını Çiz"), QStringLiteral("BLOKKIRP islem=sinir"),
        tr("BLOKKIRP islem=sinir — kırpma sınırını etkin katmana kapalı çizgi olarak "
           "çizer"));
    actBlockUnclip_ = modifyTool(
        Glyph::BlockUnclip, tr("Kırpmayı Kaldır"), QStringLiteral("BLOKKIRP islem=kaldir"),
        tr("BLOKKIRP islem=kaldir — kırpma sınırını kaldırır; referans yeniden bütün "
           "çizilir"));
    // SAVE AND GIVE UP take the objects of the open edit, which only the shell
    // knows (`blockEditLine`), so their lines are made when they are pressed.
    const auto editStep = [this](Glyph glyph, const QString& text, const QString& name,
                                 const QString& tip, bool save) {
        auto* a = new QAction(text, this);
        a->setData(static_cast<int>(glyph));
        a->setObjectName(name);
        a->setToolTip(tip);
        a->setStatusTip(tip);
        a->setProperty(kToolCommand, QStringLiteral("BLOKDÜZENLE"));
        // ALWAYS PRESSABLE, like every button on the ribbon (the accessibility
        // tree offers a press a switch user can reach): with no edit open it
        // says how to open one rather than lying dead.
        connect(a, &QAction::triggered, this, [this, save] {
            refreshBlockEdit();
            if (blockEdit_)
                controller_->runLine(blockEditLine(save), command::Origin::Gui);
            else
                onEcho(tr("Açık bir blok düzenlemesi yok: bir bloğa çift tıklayın ya da "
                          "BLOKDÜZENLE ile açın."));
        });
        return a;
    };
    actBlockSave_   = editStep(Glyph::Check, tr("Bloğu Kaydet"), QStringLiteral("blockEditSave"),
                               tr("BLOKDÜZENLE islem=kaydet — bloğun tanımını düzenlenen "
                                    "nesnelerden yeniden kurar; bütün referanslar yeni biçimi "
                                    "çizer"),
                               true);
    actBlockCancel_ = editStep(Glyph::Close, tr("Vazgeç"), QStringLiteral("blockEditCancel"),
                               tr("BLOKDÜZENLE islem=vazgec — açılan nesneleri kaldırır, tanım "
                                  "değişmez"),
                               false);
    // THE ALIGNED ONE, which is what ÖLÇÜ draws when no type is named; the other
    // six types are method tools beside it in the ribbon's Ölçü family.
    actDimension_ =
        drawTool(Glyph::DimAligned, tr("Hizalı Ölçü"), QStringLiteral("ÖLÇÜ"),
                 tr("ÖLÇÜ — iki noktanın arasını aralarındaki doğru boyunca ölçüp yazısı "
                    "ve oklarıyla çizer; öteki türler düğmenin okunda  ·  kısaltma: ÖÇ"));
    drawingTools_->addAction(actDimension_);
    // A ROW OF FIGURES (TODOS C-10): each from the last point, or each from
    // the first, carried on the newest linear or aligned dimension.
    actDimChain_ = drawTool(Glyph::DimChain, tr("Zincir Ölçü"), QStringLiteral("ZİNCİRÖLÇÜ"),
                            tr("ZİNCİRÖLÇÜ — son ölçünün ucundan aynı çizgide art arda ölçer; "
                               "noktaları tıklayın, Enter bitirir, toplamı söyler  ·  kısaltma: "
                               "ZÖ"));
    drawingTools_->addAction(actDimChain_);
    actDimBaseline_ = drawTool(Glyph::DimBaseline, tr("Baz Ölçü"), QStringLiteral("BAZÖLÇÜ"),
                               tr("BAZÖLÇÜ — son ölçünün ilk noktasından ölçer, çizgileri stilin "
                                  "aralığıyla üst üste dizer; Enter bitirir  ·  kısaltma: BÖ"));
    drawingTools_->addAction(actDimBaseline_);
    // "KILAVUZ ÇİZGİ", the name AutoCAD's Turkish documentation gives LEADER
    // (MLEADER is "Çoklu Kılavuz"); the command keeps its word LİDER.
    actLeader_ = drawTool(Glyph::Leader, tr("Kılavuz Çizgi"), QStringLiteral("LİDER"),
                          tr("LİDER — bir noktayı gösteren oklu kılavuz çizgi, istenirse yanına "
                             "yazı  ·  kısaltma: LD"));
    drawingTools_->addAction(actLeader_);

    // ---- düzenleme ----
    actErase_ = new QAction(tr("Sil"), this);
    actErase_->setToolTip(tr("SİL — seçilen nesneleri siler  ·  Del / ⌫"));
    actErase_->setData(static_cast<int>(Glyph::Erase));

    // Del, the key every drawing program deletes with. A WINDOW shortcut rather
    // than one the canvas handles, so it works with the focus in the layer list
    // or the attribute table too — the selection is the same selection whichever
    // panel the user is looking at.
    // BOTH DELETE KEYS, because a Mac keyboard has only one of them.
    // `QKeySequence::Delete` is ⌦ (forward delete), which on a Mac laptop is
    // fn+⌫ and on most Mac keyboards does not exist as a key at all; the key a
    // Mac user presses to delete is ⌫ (backspace). Bound only to the first, SİL
    // was unreachable from the keyboard on a Mac, and the user said as much: they
    // could not work out how to delete anything. A text field that has focus
    // consumes ⌫ before the
    // shortcut sees it, so typing in the command line is unaffected.
    //
    // ⌫ IS ITS OWN ACTION NOW, because between two points of a run it means
    // "not that corner" and nothing else: bound to SİL, it wrote the drawing
    // out and started asking which objects to delete (TODOS C-02). The button
    // stays SİL whatever is running — a press on it is a request to erase.
    actErase_->setShortcut(QKeySequence(QKeySequence::Delete));
    actErase_->setShortcutContext(Qt::WindowShortcut);
    addAction(actErase_);

    actBackspace_ = new QAction(tr("Son noktayı geri al ya da sil"), this);
    actBackspace_->setShortcut(QKeySequence(Qt::Key_Backspace));
    actBackspace_->setShortcutContext(Qt::WindowShortcut);
    addAction(actBackspace_);
    connect(actBackspace_, &QAction::triggered, this, [this] {
        if (controller_->retractPoint()) return;
        actErase_->trigger();
    });

    actErase_->setProperty(kToolCommand, QStringLiteral("SİL"));
    connect(actErase_, &QAction::triggered, this, [this] {
        // With a selection the button IS the command, exactly as typing `SİL`
        // would be. With nothing selected the COMMAND ASKS which objects, the
        // way every CAD's erase does — this used to put `SİL nesneler=` on the
        // command line and wait for a key the user did not know, which is a
        // button that answers a press with homework.
        if (!controller_->bus().selection().empty() && !confirmErase()) return;
        controller_->runCommand(QStringLiteral("SİL"));
    });

    // THE CLIPBOARD GROUP IS REAL NOW. These three were `placeholder`s — rows
    // that opened a box saying "Faz 2" — and the commands behind them landed with
    // P6, so the rows run them.
    //
    // `Ctrl+X`, `Ctrl+C`, `Ctrl+V` are stated rather than taken from
    // `QKeySequence::Cut` and friends: the manual prints these three and a
    // shortcut the manual prints has to be the shortcut on all three platforms.
    actCut_ = commandAction(Glyph::Cut, tr("Kes"), QStringLiteral("KES"),
                            tr("KES — seçili nesneleri panoya alır ve çizimden siler; tek geri "
                               "alma adımı  ·  kısaltma: KS"),
                            QKeySequence(Qt::CTRL | Qt::Key_X));
    actCopyClip_ =
        commandAction(Glyph::Duplicate, tr("Panoya Kopyala"), QStringLiteral("PANOYAKOPYALA"),
                      tr("PANOYAKOPYALA — seçili nesneleri çizimin kendi biçiminde panoya yazar  "
                         "·  kısaltma: PKP"),
                      QKeySequence(Qt::CTRL | Qt::Key_C));
    actCopyBase_ = commandAction(
        Glyph::CopyBase, tr("Taban Noktasıyla Kopyala"),
        QStringLiteral("PANOYAKOPYALA tabanli=evet"),
        tr("PANOYAKOPYALA tabanli=evet — nesneleri seçin, sonra taban noktasını gösterin; "
           "yapıştırırken o nokta gösterdiğiniz yere gelir"));
    actPaste_ = commandAction(Glyph::Paste, tr("Yapıştır"), QStringLiteral("YAPIŞTIR"),
                              tr("YAPIŞTIR — panodaki nesneleri tıkladığınız yere koyar; "
                                 "yerinde=evet kopyalandığı koordinatlara  ·  kısaltma: YP"),
                              QKeySequence(Qt::CTRL | Qt::Key_V));

    // A MODAL TOOL that collects its own two corners. It shipped disabled because
    // SEÇ could take a box as arguments and could not ask for one.
    actSelectArea_ = new QAction(tr("Alan Seç"), this);
    actSelectArea_->setCheckable(true);
    actSelectArea_->setToolTip(tr("SEÇ KUTU — iki köşe tıklayın; soldan sağa içinde "
                                  "kalanları, sağdan sola değdiklerini seçer"));
    actSelectArea_->setData(static_cast<int>(Glyph::SelectArea));
    actSelectArea_->setProperty(kToolCommand, QStringLiteral("SEÇ"));
    actSelectArea_->setObjectName(QStringLiteral("toolAction.SEÇ KUTU"));
    connect(actSelectArea_, &QAction::triggered, this, [this] {
        // KUTU rather than PENCERE: the direction of the drag chooses between
        // window and crossing, which is the muscle memory every CAD user has.
        controller_->beginInteractive(QStringLiteral("SEÇ mod=KUTU"));
    });

    // IN THE EXCLUSIVE GROUP, like every other modal tool. It is checkable and
    // nothing ever unchecked it, so the button latched lit for the rest of the
    // session and two tools showed armed at once. `syncToolSelection` resolves it
    // through the `kToolCommand` property set above, so the group is all it needed.
    drawingTools_->addAction(actSelectArea_);
    actTrim_ = modifyTool(Glyph::Trim, tr("Buda"), QStringLiteral("BUDA"),
                          tr("BUDA — tıkladığınız parçayı sınırlar arasından atar: çizgide, yayda, "
                             "dairede, elipste ve spline'da  ·  kısaltma: BD"));
    // THE GENERIC PAIR AND THE CADASTRAL PAIR ARE DIFFERENT TOOLS, and the tool
    // column carries the generic one. `BİRLEŞTİR`/`BÖL` are geometry: they work on
    // any area or line and say what came out. `TEVHİT`/`İFRAZ` are cadastral acts
    // with a regulation behind them — a tevhit is only valid between adjoining
    // parcels and always yields exactly one — so they live in their own Kadastro
    // menu rather than sitting on the column looking like the everyday tools.
    // Merging two woodland patches on opposite sides of a valley is a correct map
    // operation and a refused tevhit; one button cannot honestly be both.
    actCombine_ = modifyTool(Glyph::Union, tr("Birleştir"), QStringLiteral("BİRLEŞTİR"),
                             tr("BİRLEŞTİR — seçili alanları tek alanda birleştirir, uç uca "
                                "değen çizgileri tek çizgi yapar  ·  kısaltma: BRL"));
    actUnion_   = modifyTool(Glyph::Union, tr("Tevhit"), QStringLiteral("TEVHİT"),
                             tr("TEVHİT — komşu parselleri tek parselde birleştirir (kadastro)"));
    actParcelSplit_ =
        modifyTool(Glyph::ParcelSplit, tr("İfraz"), QStringLiteral("İFRAZ"),
                   tr("İFRAZ — bir parseli düz bir ayırma çizgisiyle ikiye böler (kadastro)"));
    actAreaSplit_ =
        modifyTool(Glyph::AreaSplit, tr("Alana Göre İfraz"), QStringLiteral("ALANİFRAZ"),
                   tr("ALANİFRAZ — parselden istenen yüzölçümünde parça ayırır (kadastro)"));
    actMeasureArea_ = modifyTool(Glyph::MeasureArea, tr("Alan Ölç"), QStringLiteral("ALANÖLÇ"),
                                 tr("ALANÖLÇ — seçili nesnelerin alanını ve çevresini yazar; sonuç "
                                    "tuvalde kalır"));

    actStyleCopy_ = modifyTool(Glyph::StyleCopy, tr("Stil Kopyala"), QStringLiteral("STİLKOPYALA"),
                               tr("STİLKOPYALA — bir nesnenin stilini seçili nesnelere uygular"));
    // THE CHIPS' COMMAND, ON A ROW OF ITS OWN TOO. The chips are the quick road
    // for a hand; this is the one a keyboard and a menu reach, and it asks for
    // the colour on the command line with the colour words offered beside it.
    actColour_ = modifyTool(Glyph::Colour, tr("Renk"), QStringLiteral("RENK"),
                            tr("RENK — seçili nesnelerin çizgi ve dolgu rengini değiştirir; katman "
                               "yazılırsa katmanın rengine döner"));
    // NOT a `modifyTool`: the check runs on the whole drawing when nothing is
    // selected, and refusing an empty selection would refuse its most useful form.
    actTopology_ = new QAction(tr("Topoloji Denetimi"), this);
    actTopology_->setToolTip(tr("TOPOLOJİ — kendini kesen sınır, sıfır alan, örtüşen parsel, "
                                "yinelenen ve boş nesne, tekrarlanan köşe ve çizgi ağındaki "
                                "boşlukları raporlar; hiçbir şeyi düzeltmez"));
    actTopology_->setData(static_cast<int>(Glyph::Topology));
    actTopology_->setObjectName(QStringLiteral("toolAction.TOPOLOJİ"));
    connect(actTopology_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("TOPOLOJİ")); });
    // THE WHOLE DRAWING by default, like the topology check: which results are
    // out of date with their sources (TODOS F-04).
    actDependency_ = new QAction(tr("Bağımlılıklar"), this);
    actDependency_->setToolTip(tr("BAĞIMLILIK — tampon, üretilen alan, sınır ve eş yükselti "
                                  "eğrileri kaynaklarına göre güncel mi; güncel olmayanları "
                                  "yazar  ·  kısaltma: BĞM"));
    actDependency_->setData(static_cast<int>(Glyph::Dependency));
    actDependency_->setObjectName(QStringLiteral("toolAction.BAĞIMLILIK"));
    connect(actDependency_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("BAĞIMLILIK")); });
    actDependencyRefresh_ = new QAction(tr("Güncelle"), this);
    actDependencyRefresh_->setToolTip(
        tr("BAĞIMLILIK islem=yenile — kaynağının gerisinde kalan bağlı yazıları, ölçüleri ve "
           "taramaları yetiştirir; güncel olmayan sonuçları yeniden hesaplar"));
    actDependencyRefresh_->setData(static_cast<int>(Glyph::Refresh));
    actDependencyRefresh_->setObjectName(QStringLiteral("toolAction.BAĞIMLILIK.yenile"));
    connect(actDependencyRefresh_, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("BAĞIMLILIK islem=yenile"), command::Origin::Gui);
    });

    actStyle_ = new QAction(tr("Stil Tasarımcısı"), this);
    actStyle_->setData(static_cast<int>(Glyph::Palette));
    actStyle_->setToolTip(tr("Katmanın çizim stilini düzenle"));
    connect(actStyle_, &QAction::triggered, this, [this] { openStyleDesigner(QString()); });

    // The transform tools act on the SELECTION and finish, so they are ordinary
    // command buttons rather than modal draw tools: nothing stays armed after one
    // runs, and putting them in the exclusive group would light a tool that is no
    // longer waiting for anything.
    actMove_   = modifyTool(Glyph::Move, tr("Taşı"), QStringLiteral("TAŞI"),
                            tr("TAŞI — seçili nesneleri iki nokta arasındaki kadar taşır"));
    actCopy_   = modifyTool(Glyph::Copy, tr("Kopyala"), QStringLiteral("KOPYALA"),
                            tr("KOPYALA — seçili nesnelerin kopyasını koyar"));
    actRotate_ = modifyTool(Glyph::Rotate, tr("Döndür"), QStringLiteral("DÖNDÜR"),
                            tr("DÖNDÜR — seçili nesneleri bir merkez etrafında döndürür"));
    actScale_  = modifyTool(Glyph::Scale, tr("Ölçekle"), QStringLiteral("ÖLÇEKLE"),
                            tr("ÖLÇEKLE — seçili nesneleri bir merkeze göre büyütür/küçültür"));
    actMirror_ = modifyTool(Glyph::Mirror, tr("Aynala"), QStringLiteral("AYNALA"),
                            tr("AYNALA — seçili nesneleri bir eksende yansıtır"));
    actArray_ =
        modifyTool(Glyph::Array, tr("Dizi"), QStringLiteral("DİZİ"),
                   tr("DİZİ — seçili nesneleri satır/sütun ya da merkez etrafında çoğaltır"));
    // THE OTHER WAYS A TRANSFORM IS ASKED FOR (TODOS C-08), each its own entry
    // so a hand reaches it: a turn or a scale found from a reference on the
    // drawing, a mirrored copy, and arrays round a centre and along a path.
    actArrayPolar_ =
        modifyTool(Glyph::Array, tr("Dizi — kutupsal"), QStringLiteral("DİZİ mod=KUTUPSAL"),
                   tr("DİZİ mod=KUTUPSAL — seçili nesneleri bir merkez etrafında çoğaltır"));
    actArrayPath_ =
        modifyTool(Glyph::Array, tr("Dizi — yol boyunca"), QStringLiteral("DİZİ mod=YOL"),
                   tr("DİZİ mod=YOL — seçili nesneleri bir çizgi ya da yay boyunca eşit aralıkla "
                      "dizer, her kopyayı yolun doğrultusuna döndürür"));
    actRotateRef_ = modifyTool(
        Glyph::Rotate, tr("Döndür — referansla"), QStringLiteral("DÖNDÜR yontem=referans"),
        tr("DÖNDÜR yontem=referans — iki noktayla gösterilen doğrultuyu yeni "
           "doğrultuya döndürür"));
    actScaleRef_ = modifyTool(
        Glyph::Scale, tr("Ölçekle — referansla"), QStringLiteral("ÖLÇEKLE yontem=referans"),
        tr("ÖLÇEKLE yontem=referans — iki noktayla gösterilen uzunluğu yeni uzunluğa "
           "getirir"));
    actMirrorCopy_ =
        modifyTool(Glyph::Mirror, tr("Aynala — kopyalayarak"), QStringLiteral("AYNALA kopya=evet"),
                   tr("AYNALA kopya=evet — özgün yerinde kalır, aynalanmış kopyası çizilir"));
    actExtend_ = modifyTool(Glyph::Extend, tr("Uzat"), QStringLiteral("UZAT"),
                            tr("UZAT — ucu en yakın sınıra kadar uzatır: çizgiyi doğrultusunda, "
                               "yayı çemberi boyunca"));
    // THE OTHER WAYS TO SHOW BUDA AND UZAT THEIR WORK, each its own entry so a
    // hand reaches it: a fence across many pieces at once, the piece to keep
    // rather than the one to lose, and a boundary that stops short taken as
    // running on. They were parameters only a typed line could set.
    actTrimFence_ =
        modifyTool(Glyph::Trim, tr("Buda — çitle"), QStringLiteral("BUDA yontem=çit"),
                   tr("BUDA yontem=çit — çizdiğiniz çitin geçtiği bütün parçaları tek seferde "
                      "budar; Enter uygular"));
    actTrimKeep_ =
        modifyTool(Glyph::Trim, tr("Buda — tıklanan kalsın"), QStringLiteral("BUDA tut=evet"),
                   tr("BUDA tut=evet — tıkladığınız parça kalır, iki yanındaki kesimlerin dışında "
                      "kalan gider"));
    actTrimCarry_ =
        modifyTool(Glyph::Trim, tr("Buda — sınırları uzatarak"), QStringLiteral("BUDA uzanti=evet"),
                   tr("BUDA uzanti=evet — nesneye yetişmeyen bir sınır kendi doğrultusunda "
                      "uzatılmış sayılır"));
    actExtendFence_ =
        modifyTool(Glyph::Extend, tr("Uzat — çitle"), QStringLiteral("UZAT yontem=çit"),
                   tr("UZAT yontem=çit — çizdiğiniz çitin yanından geçtiği bütün uçları sınıra "
                      "uzatır; Enter uygular"));
    actExtendCarry_ = modifyTool(
        Glyph::Extend, tr("Uzat — sınırları uzatarak"), QStringLiteral("UZAT uzanti=evet"),
        tr("UZAT uzanti=evet — uca yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır"));
    actSplit_ = modifyTool(Glyph::Split, tr("Böl"), QStringLiteral("BÖL"),
                           tr("BÖL — çizdiğiniz kesme çizgisiyle böler: çizgi, yay, daire, yaylı "
                              "çoklu çizgi ve alan  ·  kısaltma: BL"));
    // THE OTHER FOUR WAYS TO SAY WHERE (TODOS C-05), each its own entry: they
    // were not there at all, and BÖL could cut nothing but a line.
    actSplitPoint_ =
        modifyTool(Glyph::Split, tr("Böl — noktalardan"), QStringLiteral("BÖL yontem=nokta"),
                   tr("BÖL yontem=nokta — nesnenin üstüne tıkladığınız noktalardan böler; "
                      "parçalar Enter'dan önce görünür, ⌫ son noktayı geri alır"));
    actSplitCross_ =
        modifyTool(Glyph::Split, tr("Böl — kesişimlerden"), QStringLiteral("BÖL yontem=kesisim"),
                   tr("BÖL yontem=kesisim — seçtiğiniz nesneleri birbirini kestikleri her yerden "
                      "böler"));
    actSplitEqual_ =
        modifyTool(Glyph::Split, tr("Böl — eşit parçaya"), QStringLiteral("BÖL yontem=esit"),
                   tr("BÖL yontem=esit — seçtiğiniz nesneleri verdiğiniz sayıda eşit parçaya "
                      "böler"));
    actSplitDistance_ = modifyTool(
        Glyph::Split, tr("Böl — baştan uzaklıkla"), QStringLiteral("BÖL yontem=mesafe"),
        tr("BÖL yontem=mesafe — seçtiğiniz nesneleri başından verdiğiniz uzaklıkta böler"));
    actChamfer_ = modifyTool(Glyph::Chamfer, tr("Pah"), QStringLiteral("PAH"),
                             tr("PAH — köşeye ya da iki çizgiye kalacak parçalarından tıklayın; "
                                "köşeyi düz bir kenarla keser, mesafe yazılır ya da "
                                "gösterilir  ·  kısaltma: PH"));
    actFillet_  = modifyTool(Glyph::Fillet, tr("Yuvarla"), QStringLiteral("YUVARLA"),
                             tr("YUVARLA — köşeye ya da iki nesneye kalacak parçalarından "
                                 "tıklayın; köşeyi teğet bir yayla yuvarlatır, 0 keskin köşe  ·  "
                                 "kısaltma: YV"));
    // EVERY CORNER AT ONCE (TODOS C-06), each its own entry so a hand reaches
    // it: a chain rounded or cut by one size, the corners it does not fit
    // passed over and counted.
    actChamferAll_ =
        modifyTool(Glyph::Chamfer, tr("Pah — bütün köşeler"), QStringLiteral("PAH hepsi=evet"),
                   tr("PAH hepsi=evet — çizgiye ya da alana tıklayın; bütün köşelerine aynı "
                      "mesafeyle pah kırar"));
    actFilletAll_ = modifyTool(
        Glyph::Fillet, tr("Yuvarla — bütün köşeler"), QStringLiteral("YUVARLA hepsi=evet"),
        tr("YUVARLA hepsi=evet — çizgiye ya da alana tıklayın; bütün köşelerini aynı "
           "yarıçapla yuvarlatır"));
    actSetLayer_ = modifyTool(Glyph::LayerManager, tr("Katmana Taşı"), QStringLiteral("KATMANAT"),
                              tr("KATMANAT — seçili nesneleri başka bir katmana taşır"));
    actOffset_   = modifyTool(Glyph::Offset, tr("Ofset"), QStringLiteral("OFSET"),
                              tr("OFSET — seçili nesnelerin paralelini çizer; eksi mesafe içeri"));

    // ONE ACTION WHEREVER IT IS SHOWN. POLİGON once sat on two menus as two
    // separate rows, which could drift apart in what they said. Made HERE, with
    // the other tools, so the ribbon and the palette show this one.
    actTraverse_ = commandAction(Glyph::Traverse, tr("Poligon Hesabı"), QStringLiteral("POLİGON"),
                                 tr("POLİGON — kırılma açısı ve kenarlardan poligon "
                                    "koordinatları, kapanma dağıtımı ve mevzuat toleransı  ·  "
                                    "kısaltma: PLG"));
    actAngledGuide_ =
        commandAction(Glyph::Guide, tr("Açılı Cetvel Kılavuzu"), QStringLiteral("KILAVUZ yon=45g"),
                      tr("KILAVUZ yon=<açı> — verilen noktadan geçen açılı kılavuz; açı "
                         "oturumun birim ve kuralıyla okunur  ·  kısaltma: KLV"));
    actLabel_    = commandAction(Glyph::Label, tr("Etiket"), QStringLiteral("ETİKET"),
                                 tr("ETİKET — katmandaki nesneleri özniteliklerinden okuyarak "
                                       "etiketler  ·  kısaltma: ETK"));
    actStakeout_ = commandAction(Glyph::Locate, tr("Aplikasyon"), QStringLiteral("APLİKASYON"),
                                 tr("APLİKASYON — istasyondan hedefe semt açısı ve kenar  ·  "
                                    "kısaltma: APL"));

    // THE VERBS THAT WERE MENU ROWS ONLY, made tools: checkable, lit while they
    // run and put back in the hand when they finish, like every other tool the
    // column holds. As `commandAction` rows they worked from the menu and could
    // not be in the column at all — which is where a hand looks for PAH.
    actBreak_    = modifyTool(Glyph::Break, tr("Kır"), QStringLiteral("KIR"),
                              tr("KIR — iki nokta arasındaki parçayı çıkarır; tek nokta boşluksuz "
                                    "böler  ·  kısaltma: KR"));
    actLengthen_ = modifyTool(Glyph::Lengthen, tr("Uzunluk"), QStringLiteral("UZUNLUK"),
                              tr("UZUNLUK — bir ucu kendi doğrultusunda hareket ettirir  ·  "
                                 "kısaltma: UZN"));
    actJoin_     = modifyTool(Glyph::Join, tr("Uç Uca Ekle"), QStringLiteral("UÇUCA"),
                              tr("UÇUCA — uçları değen çizgileri tek çizgiye ekler; BİRLEŞTİR ile "
                                     "karıştırmayın  ·  kısaltma: UÇE"));
    actExplode_  = modifyTool(Glyph::Explode, tr("Patlat"), QStringLiteral("PATLAT"),
                              tr("PATLAT — çizgiyi kenarlara, alanı sınırına, bloğu bileşenlerine "
                                  "ayırır  ·  kısaltma: PTL"));
    actAlign_    = modifyTool(Glyph::Align, tr("Hizala"), QStringLiteral("HİZALA"),
                              tr("HİZALA — bir ya da iki nokta çiftiyle taşır, döndürür ve istenirse "
                                    "ölçekler  ·  kısaltma: HZL"));
    // AND THE SCALING FORM, whose whole line rides on the button: the second
    // pair's length then stretches the objects too — a sketch fitted onto its
    // surveyed corners. It was reachable only by typing `olcekle=evet`.
    actAlignScaled_ =
        modifyTool(Glyph::Align, tr("Hizala — ölçekleyerek"), QStringLiteral("HİZALA olcekle=evet"),
                   tr("HİZALA olcekle=evet — iki nokta çiftiyle taşır, döndürür ve "
                      "ikinci çiftin uzunluğuna göre ölçekler"));
    actDivide_ = modifyTool(Glyph::Divide, tr("Bölümle"), QStringLiteral("BÖLÜMLE"),
                            tr("BÖLÜMLE — nesne boyunca eşit parçalara ya da sabit aralıkla nokta "
                               "koyar  ·  kısaltma: BLM"));
    actPolylineEdit_ =
        modifyTool(Glyph::PolylineEdit, tr("Çizgi Düzenle"), QStringLiteral("ÇİZGİDÜZENLE"),
                   tr("ÇİZGİDÜZENLE — kapatır, açar, yönünü çevirir ya da "
                      "sadeleştirir  ·  kısaltma: ÇZD"));
    actVertexMove_ = modifyTool(Glyph::VertexMove, tr("Köşe Taşı"), QStringLiteral("KÖŞETAŞI"),
                                tr("KÖŞETAŞI — köşeye tıklayın, yeni yerini gösterin; kenarlar "
                                   "imleci izler  ·  kısaltma: KT"));
    actVertexAdd_ = modifyTool(Glyph::VertexAdd, tr("Köşe Ekle"), QStringLiteral("KÖŞEEKLE"),
                               tr("KÖŞEEKLE — kenara tıklayın, yeni köşenin yerini gösterin  ·  "
                                  "kısaltma: KE"));
    // THE TWO EDITS A CORNER AND AN EDGE STILL LACKED (TODOS C-07): a corner
    // taken out, and an edge's kind changed — straight to arc and back.
    actVertexDelete_ =
        modifyTool(Glyph::VertexDelete, tr("Köşe Sil"), QStringLiteral("KÖŞESİL"),
                   tr("KÖŞESİL — köşeye tıklayın; iki kenar tek kenar olur. Seçili parsellerin "
                      "ortak köşesi ikisinden birden silinir  ·  kısaltma: KSL"));
    actEdgeKind_ =
        modifyTool(Glyph::EdgeKind, tr("Kenar Türü"), QStringLiteral("KENARTÜRÜ"),
                   tr("KENARTÜRÜ — kenara tıklayın: düz kenar gösterdiğiniz noktadan geçen yaya, "
                      "yay düz kenara döner; nesnenin kimliği korunur  ·  kısaltma: KNT"));
    actToArea_ = modifyTool(Glyph::ToArea, tr("Alana Çevir"), QStringLiteral("ALANAÇEVİR"),
                            tr("ALANAÇEVİR — uç uca değen çizgilerden kapalı bir alan kurar  ·  "
                               "kısaltma: ALÇ"));
    // SINIR: a click inside ground the linework closes. The tool previews the
    // region under the cursor with the same call the click makes, and where the
    // lines do not close it marks the open ends instead (TODOS C-09).
    actBoundary_ =
        modifyTool(Glyph::Boundary, tr("Sınır Bul"), QStringLiteral("SINIR"),
                   tr("SINIR — kapalı bir bölgenin içine tıklayın: sınırı yeni bir alan "
                      "olur, içerideki adalar delik; kapanmıyorsa açık uçlar gösterilir  "
                      "·  kısaltma: SNR"));
    actTextEdit_ = modifyTool(Glyph::TextEdit, tr("Yazıyı Düzenle"), QStringLiteral("YAZIDÜZENLE"),
                              tr("YAZIDÜZENLE — yazıyı seçin, yeni metni yazın; eskisi önerilir  "
                                 "·  kısaltma: YZD"));
    // THE DIMENSION'S OWN EDIT (TODOS C-10): pick the dimension, type its
    // caption with `<>` for the measured figure. The rest — prefix, tolerance,
    // unit, decimals — is on the ÖLÇÜ group of the attribute panel, one cell
    // per field, and on the command line.
    actDimensionEdit_ =
        modifyTool(Glyph::DimEdit, tr("Ölçüyü Düzenle"), QStringLiteral("ÖLÇÜDÜZENLE"),
                   tr("ÖLÇÜDÜZENLE — ölçüyü seçin, yazısını yazın: <> ölçülen değerdir, <> "
                      "taşımayan yazı elle yazılmış sayılır ve öyle gösterilir  ·  kısaltma: ÖDZ"));

    actUndo_ = new QAction(tr("Geri Al"), this);
    actUndo_->setShortcut(QKeySequence::Undo);
    actUndo_->setToolTip(
        tr("GERİAL — son işlemi geri alır; çizerken yalnız son noktayı  ·  Ctrl+Z"));
    actUndo_->setData(static_cast<int>(Glyph::Undo));
    actUndo_->setProperty(kToolCommand, QStringLiteral("GERİAL"));
    // BETWEEN TWO POINTS, THE POINT. GERİAL there wrote the run out and undid
    // all of it, so one wrong corner cost the whole boundary (TODOS C-02).
    connect(actUndo_, &QAction::triggered, this, [this] {
        if (controller_->retractPoint()) return;
        controller_->runCommand(QStringLiteral("GERİAL"));
    });

    actRedo_ = new QAction(tr("Yinele"), this);
    actRedo_->setShortcut(QKeySequence::Redo);
    actRedo_->setToolTip(tr("YİNELE — geri alınan işlemi yineler"));
    actRedo_->setData(static_cast<int>(Glyph::Redo));
    actRedo_->setProperty(kToolCommand, QStringLiteral("YİNELE"));
    connect(actRedo_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("YİNELE")); });

    // ---- görünüm ----
    actZoomExtents_ = commandAction(
        Glyph::ZoomExtents, tr("Kapsama Yakınlaş"), QStringLiteral("YAKINLAŞ KAPSAM"),
        tr("YAKINLAŞ KAPSAM — çizimin tamamını göster"), QKeySequence(Qt::CTRL | Qt::Key_0));
    actZoomIn_  = commandAction(Glyph::ZoomIn, tr("Yakınlaştır"),
                                QStringLiteral("YAKINLAŞ ÇARPAN carpan=1.25"),
                                tr("YAKINLAŞ ÇARPAN carpan=1.25"), QKeySequence::ZoomIn);
    actZoomOut_ = commandAction(Glyph::ZoomOut, tr("Uzaklaştır"),
                                QStringLiteral("YAKINLAŞ ÇARPAN carpan=0.8"),
                                tr("YAKINLAŞ ÇARPAN carpan=0.8"), QKeySequence::ZoomOut);

    actPan_ = new QAction(tr("Kaydır"), this);
    actPan_->setCheckable(true);
    actPan_->setToolTip(tr("KAYDIR — bir noktayı tutup başka bir yere taşır; ölçek değişmez  ·  "
                           "orta fare tuşu sürükleme her zaman çalışır"));
    actPan_->setData(static_cast<int>(Glyph::Pan));
    actPan_->setProperty(kToolCommand, QStringLiteral("KAYDIR"));
    actPan_->setObjectName(QStringLiteral("toolAction.KAYDIR"));
    connect(actPan_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KAYDIR")); });
    drawingTools_->addAction(actPan_);

    // ---- input aids ----
    //
    // Each of these writes a SESSION setting through `MOD`. They are not a second
    // way to change a mode: the value lives in one store, the menu item reads it
    // back, and typing `MOD dik_mod evet` moves the tick exactly as F8 does
    // (model.md R38, R41; CLAUDE.md 5.10).
    actSnap_ = new QAction(tr("Nesne Yakalama"), this);
    actSnap_->setCheckable(true);
    actSnap_->setShortcut(QKeySequence(Qt::Key_F3));
    actSnap_->setData(static_cast<int>(Glyph::Snap));
    actSnap_->setToolTip(tr("MOD yakalama_modları — nesne yakalamayı açar/kapatır (F3)"));
    connect(actSnap_, &QAction::toggled, this, [this](bool on) {
        controller_->runLine(
            QStringLiteral("MOD yakalama_modları %1").arg(on ? snapMaskMemory_ : 0),
            command::Origin::Gui);
    });

    actOrtho_ = new QAction(tr("Dik Mod"), this);
    actOrtho_->setData(static_cast<int>(Glyph::Ortho));
    actOrtho_->setCheckable(true);
    actOrtho_->setShortcut(QKeySequence(Qt::Key_F8));
    actOrtho_->setToolTip(tr("MOD dik_mod — imleci yatay ve düşey eksene kilitler (F8)"));
    connect(actOrtho_, &QAction::toggled, this, [this](bool on) {
        controller_->runLine(QStringLiteral("MOD dik_mod %1")
                                 .arg(on ? QStringLiteral("evet") : QStringLiteral("hayır")),
                             command::Origin::Gui);
    });

    // PERPENDICULAR TO THE SURFACE, not to the page. Ortho locks to the world
    // axes; this one locks to the normal of whatever edge the line started on, so
    // a setback off a 37 degree parcel boundary is drawn at exactly 127 rather
    // than eyeballed. Shift held on the canvas engages it for as long as it is
    // held; F10 and `MOD yüzey_normali evet` latch it (Article 1.2).
    actNormal_ = new QAction(tr("Yüzey Normali"), this);
    actNormal_->setData(static_cast<int>(Glyph::SurfaceNormal));
    actNormal_->setCheckable(true);
    actNormal_->setShortcut(QKeySequence(Qt::Key_F10));
    actNormal_->setToolTip(
        tr("MOD yüzey_normali — çizgiyi başladığı yüzeye dik kilitler (F10, tuval üzerinde "
           "Shift basılı)"));
    connect(actNormal_, &QAction::toggled, this, [this](bool on) {
        controller_->runLine(QStringLiteral("MOD yüzey_normali %1")
                                 .arg(on ? QStringLiteral("evet") : QStringLiteral("hayır")),
                             command::Origin::Gui);
    });

    actGridSnap_ = new QAction(tr("Izgaraya Yakala"), this);
    actGridSnap_->setData(static_cast<int>(Glyph::Grid));
    actGridSnap_->setCheckable(true);
    actGridSnap_->setShortcut(QKeySequence(Qt::Key_F9));
    actGridSnap_->setToolTip(
        tr("MOD ızgaraya_yakala — noktayı en yakın ızgara kesişimine oturtur (F9)"));
    connect(actGridSnap_, &QAction::toggled, this, [this](bool on) {
        controller_->runLine(QStringLiteral("MOD ızgaraya_yakala %1")
                                 .arg(on ? QStringLiteral("evet") : QStringLiteral("hayır")),
                             command::Origin::Gui);
    });

    // ---- seçim ----
    actSelectAll_ = commandAction(Glyph::Select, tr("Tümünü Seç"), QStringLiteral("SEÇ TÜMÜ"),
                                  tr("SEÇ TÜMÜ — görünür bütün nesneleri seçer"),
                                  QKeySequence(Qt::CTRL | Qt::Key_A));
    actSelectNone_ = commandAction(
        Glyph::Select, tr("Seçimi Temizle"), QStringLiteral("SEÇ TEMİZLE"),
        tr("SEÇ TEMİZLE — seçimi boşaltır"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));

    // BULDEĞİŞTİR through its dialog: the fields, the preview table and the
    // buttons are a face on the command, which does the finding (C-12).
    actFindReplace_ = new QAction(tr("Bul ve Değiştir…"), this);
    // Ctrl+H is Replace on Windows and on Linux desktops. On a Mac Cmd+H hides
    // the application — the key would never reach this window — so there it is
    // Cmd+Option+F, as in TextEdit and Xcode. `QKeySequence::Replace` is no
    // help: it is empty on a Mac and Ctrl+R on KDE, which already runs a script.
#ifdef Q_OS_MACOS
    actFindReplace_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
#else
    actFindReplace_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
#endif
    actFindReplace_->setToolTip(
        tr("BULDEĞİŞTİR — yazılarda bir sözcüğü bulur, önizler ve değiştirir (%1)")
            .arg(actFindReplace_->shortcut().toString(QKeySequence::NativeText)));
    actFindReplace_->setData(static_cast<int>(Glyph::Search));
    actFindReplace_->setProperty(kToolCommand, QStringLiteral("BULDEĞİŞTİR"));
    connect(actFindReplace_, &QAction::triggered, this, &MainWindow::openFindReplace);

    // ---- `katman` ve CBS ----
    actLayer_ = new QAction(tr("Katman"), this);
    actLayer_->setToolTip(tr("KATMAN — katman oluşturur ve aktif yapar"));
    actLayer_->setData(static_cast<int>(Glyph::Layer));
    actLayer_->setProperty(kToolCommand, QStringLiteral("KATMAN"));
    connect(actLayer_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KATMAN")); });

    actLayerManager_ =
        placeholder(Glyph::LayerManager, tr("Katman Yöneticisi"),
                    QStringLiteral("KATMANYÖNETİCİSİ"), tr("Faz 1"),
                    tr("Bütün katmanları tek pencerede gösterecek: adı, görünürlüğü, kilidi, "
                       "rengi, çizgi tipi ve nesne sayısı bir tabloda; çoklu seçimle topluca "
                       "değiştirilebilecek.\n\nBugün: sağdaki Katmanlar paneli katmanları "
                       "listeler ve görünürlüğü ile kilidi oradan değiştirilir; adı, rengi ve "
                       "stili için KATMAN ve STİL komutları; hepsini birden açıp kapatmak için "
                       "Katman menüsü."));
    // A MODAL TOOL like every other two-click tool. It was a plain action, so it
    // ran but never lit: the user clicked twice on a canvas that gave no sign a
    // measurement was in progress, and the answer went to a hidden tab.
    actMeasure_ = new QAction(tr("Ölç"), this);
    actMeasure_->setCheckable(true);
    actMeasure_->setToolTip(tr("ÖLÇ — noktadan noktaya: her kenar, açısı ve toplam uzunluk; Enter "
                               "bitirir, sonuç tuvalde kalır"));
    actMeasure_->setData(static_cast<int>(Glyph::Measure));
    actMeasure_->setProperty(kToolCommand, QStringLiteral("ÖLÇ"));
    actMeasure_->setProperty(kToolRepeats, true);
    actMeasure_->setObjectName(QStringLiteral("toolAction.ÖLÇ"));
    connect(actMeasure_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("ÖLÇ")); });
    drawingTools_->addAction(actMeasure_);

    actCoordinate_ = new QAction(tr("Koordinat Oku"), this);
    actCoordinate_->setCheckable(true);
    actCoordinate_->setToolTip(tr("KOORDİNAT — tıklanan noktanın sağa/yukarı değerini yazar"));
    actCoordinate_->setData(static_cast<int>(Glyph::Coordinate));
    actCoordinate_->setProperty(kToolCommand, QStringLiteral("KOORDİNAT"));
    actCoordinate_->setProperty(kToolRepeats, true);
    actCoordinate_->setObjectName(QStringLiteral("toolAction.KOORDİNAT"));
    connect(actCoordinate_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KOORDİNAT")); });
    drawingTools_->addAction(actCoordinate_);

    // THE TWO QUESTIONS OF P7, as tools rather than as menu rows only: both arm
    // and then wait for the hand, so they belong in the exclusive group beside
    // ÖLÇ and ALANÖLÇ, and `modifyTool` is what puts them there.
    actEntityInfo_ = modifyTool(Glyph::Identify, tr("Nesne Bilgisi"), QStringLiteral("NESNEBİLGİ"),
                                tr("NESNEBİLGİ — tür, katman, köşe sayısı, çevre, alan ve "
                                   "öznitelikler  ·  kısaltma: NB"));
    actMeasureAngle_ = modifyTool(Glyph::MeasureAngle, tr("Açı Ölç"), QStringLiteral("AÇIÖLÇ"),
                                  tr("AÇIÖLÇ — tepe ve iki kol; açıyı oturumun birim ve "
                                     "kuralıyla yazar  ·  kısaltma: AÇÖ"));

    // ESNET IS A MODAL TOOL, not a menu row: it arms, then asks for a window and
    // an offset. `modifyTool` puts it in the exclusive group with TAŞI and BUDA,
    // which is where the tool column can light it while it waits.
    actStretch_ = modifyTool(Glyph::Stretch, tr("Esnet"), QStringLiteral("ESNET"),
                             tr("ESNET — pencere içindeki köşeleri taşır, dışındakileri "
                                "yerinde bırakır  ·  kısaltma: ES"));
    // THE COMMAND EXISTS, so the button is not a placeholder any more. `SORGULA`
    // shipped with the read tools and the menu still carried a disabled `Faz 2`
    // stub beside it — a dead entry with a live command's name, which is worse
    // than no entry at all because it says the feature is missing.
    actIdentify_ = commandAction(Glyph::Identify, tr("Sorgula"), QStringLiteral("SORGULA"),
                                 tr("SORGULA — katman ve öznitelik koşuluna uyan nesneleri "
                                    "sayar ve seçer  ·  kısaltma: SRG"));
    actTable_    = new QAction(tr("Öznitelik Tablosu"), this);
    actTable_->setData(static_cast<int>(Glyph::Table));
    actTable_->setToolTip(tr("Katmanın satırlarını ve sütunlarını aç"));
    actTable_->setShortcut(QKeySequence(Qt::Key_F6));
    // A lambda and not the member: `QAction::triggered` carries a `bool` and the
    // slot now takes a layer name, so a direct connect would hand the checked
    // state in as a layer. The menu means "the layer I am drawing on", which is
    // what the empty name asks for.
    connect(actTable_, &QAction::triggered, this, [this] { openAttributeTable(); });

    // THE CONVERSATION. Was a `Faz 3` placeholder; it is the panel now. The mark
    // is `Chat` rather than `Ai` because what the button opens is a conversation
    // and `Glyph::Ai` is what an AI-authored SUGGESTION is marked with — two
    // different things that must not share a picture.
    actAi_ = new QAction(tr("Yapay Zeka"), this);
    actAi_->setData(static_cast<int>(Glyph::Chat));
    actAi_->setToolTip(tr("Yapay zeka sohbeti — model komut önerir; ne zaman uygulanacağını "
                          "onay politikanız belirler (Ayarlar ▸ Çalışma Davranışı)"));
    actAi_->setStatusTip(actAi_->toolTip());
    // CTRL+SHIFT+K, BESIDE THE PALETTE'S CTRL+K: ask for commands, next to the
    // key that searches them. Not Ctrl+Shift+A, which clears the selection and
    // held that key first; two actions on one key trigger neither.
    actAi_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    actAi_->setProperty(kToolCommand, QStringLiteral("ÖNERİ"));
    connect(actAi_, &QAction::triggered, this, [this] {
        showChat();
        if (chatPanel_ != nullptr) chatPanel_->refreshProfiles();
    });

    // ---- arayüz ----
    actTheme_ = new QAction(tr("Koyu Tema"), this);
    actTheme_->setData(static_cast<int>(Glyph::Theme));
    actTheme_->setToolTip(tr("Koyu ve açık tema arasında geçer (TERCİH arayüz_teması)"));
    actTheme_->setCheckable(true);
    connect(actTheme_, &QAction::toggled, this, &MainWindow::toggleTheme);

    // Render statistics are a developer overlay, never a user-facing feature
    // (kentoscad.md §6.3, .claude/render.md). Off by default.
    actHud_ = new QAction(tr("Geliştirici Bilgisi"), this);
    actHud_->setData(static_cast<int>(Glyph::Hud));
    actHud_->setToolTip(tr("Kare süresi, çizilen nesne sayısı ve arka uç: geliştirici için (F12)"));
    actHud_->setCheckable(true);
    actHud_->setShortcut(QKeySequence(Qt::Key_F12));
    connect(actHud_, &QAction::toggled, this, [this](bool on) { canvas_->setDebugHud(on); });

    // The command line shows from the start (see the constructor); this hides
    // and brings it back. Ctrl+9 matches the shortcut CAD users already have in
    // their fingers.
    actCommandLine_ = new QAction(tr("Komut Satırı"), this);
    actCommandLine_->setData(static_cast<int>(Glyph::CommandLine));
    actCommandLine_->setCheckable(true);
    actCommandLine_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_9));
    connect(actCommandLine_, &QAction::toggled, this, &MainWindow::showCommandLine);
}

void MainWindow::buildPanels()
{
    // design.md 6 and 7: every panel wears ONE 29 px header that carries its tabs
    // AND its buttons. Qt's tabified docks give a tab bar and a title bar — two
    // rows, 58 px — so the tabs are drawn by `PanelHeader` and each dock's own
    // title bar is replaced by it. Switching tabs switches a QStackedWidget
    // rather than raising another dock.
    const auto makeDock = [this](const QString& name, PanelHeader* header, QWidget* body) {
        auto* dock = new QDockWidget(this);
        dock->setObjectName(name);
        dock->setTitleBarWidget(header);
        dock->setWidget(body);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
                              Qt::BottomDockWidgetArea);
        connect(header, &PanelHeader::buttonPressed, this, [dock](int button) {
            if (button == PanelHeader::Collapse) dock->hide();
            if (button == PanelHeader::Float) dock->setFloating(!dock->isFloating());
            if (button == PanelHeader::Close) dock->hide();
        });
        return dock;
    };

    // ---- the attributes / history panel ----
    attributePanel_ = new AttributePanel(*controller_, this);

    // The object's menu asks; the shell answers, because the shell is what owns
    // windows. The panel never learns what is in any of them.
    connect(attributePanel_, &AttributePanel::exportCoordinatesRequested, this, [this] {
        ExportDialog window(*controller_, ExportSubject::Coordinates, QString(), this);
        window.applyTheme(theme_);
        window.exec();
    });
    connect(attributePanel_, &AttributePanel::tableRequested, this,
            [this](const QString& layer) { openAttributeTable(layer); });
    connect(attributePanel_, &AttributePanel::propertiesRequested, this,
            [this](const QString& layer) { openStyleDesigner(layer); });

    transcript_ = new QPlainTextEdit(this);
    transcript_->setReadOnly(true);
    transcript_->setMaximumBlockCount(2000);
    transcript_->setFrameShape(QFrame::NoFrame);

    journalView_ = new QPlainTextEdit(this);
    journalView_->setReadOnly(true);
    journalView_->setLineWrapMode(QPlainTextEdit::NoWrap);
    journalView_->setFrameShape(QFrame::NoFrame);
    journalView_->setPlaceholderText(
        tr("Her komut buraya JSON olarak yazılır. Bu günlük geri almanın, makro "
           "kaydının, regresyon testinin ve çökme kurtarmanın ortak kaynağıdır."));

    // ---- the tools panel: the processing registry as a tree and a form ----
    toolsPanel_ = new ToolsPanel(*controller_, this);
    toolsPanel_->setViewportProvider([this] { return canvas_->view().visible_box(); });

    // WHAT THE VIEWPORT IS SHOWING, for the client that cannot see the screen.
    // The write hooks (`on_view_request`, `on_pan_request`) have always let a
    // command MOVE the view; nothing could read it, so no command could answer
    // "which corner coordinates am I looking at" — the first question any agent,
    // script or macro asks before it draws. Installed beside the provider above
    // and from the same source, so the two cannot disagree. View state is not
    // document state (model.md R43): read here, never hashed, never journalled.
    // WHAT A MEASUREMENT MEASURED stays on the canvas it was measured on
    // (`command/measure_mark.hpp`). View state, from the same seam family as
    // the view hooks: the command says what, the canvas draws it.
    controller_->bus().on_measure_mark = [this](const command::MeasureMark& mark) {
        if (canvas_ != nullptr) canvas_->addMeasureMark(mark);
    };
    controller_->bus().on_view_query = [this] {
        const render::ViewTransform& view = canvas_->view();
        command::ViewInfo info;
        info.window       = view.visible_box();
        info.centre       = view.centre();
        info.mm_per_pixel = view.mm_per_pixel();
        info.width_px     = view.width();
        info.height_px    = view.height();
        // The denominator the status bar shows, at this screen's own DPI: a 1:N
        // taken at a guessed DPI is a different plan scale on every monitor.
        const double dpi = canvas_->logicalDpiX() > 0 ? canvas_->logicalDpiX() : 96.0;
        info.scale       = static_cast<std::int64_t>(std::llround(view.scale_denominator(dpi)));
        info.crs         = controller_->document().crs().id();
        return info;
    };
    // THE LETTERS THE TYPEFACE LACKS (TODOS C-12), asked of the shaper the
    // canvas draws with, so NESNEBİLGİ names exactly what shows as a box.
    controller_->bus().on_glyph_query = [](std::string_view utf8) { return missing_glyphs(utf8); };
    // `YARDIM` ANSWERS WITH A PAGE HERE, AND WITH TEXT EVERYWHERE ELSE — the
    // same seam as `on_print_request` and for the same reason: the command knows
    // WHAT to show and nothing about windows, and a headless client still gets
    // its answer because the command echoes and reports either way. It used to
    // append ninety-eight lines to the transcript, which is a list you scroll
    // past rather than a list you read.
    controller_->bus().on_help_page = [this](const std::string& focus_on) {
        openCommandSearch(QString::fromStdString(focus_on));
    };
    toolsPanel_->setScenePicker(
        [this](FieldKind kind, std::function<void(std::optional<QString>)> done) {
            // One pick at a time: a field that asks while another is waiting
            // takes the pick over, and the first is answered with nothing.
            if (pendingPick_) {
                auto earlier = std::move(pendingPick_);
                pendingPick_ = nullptr;
                earlier(std::nullopt);
            }
            pendingPick_ = std::move(done);
            canvas_->beginCapture(kind == FieldKind::Object ? MapCanvas::Capture::Object
                                                            : MapCanvas::Capture::Point);
        });
    connect(toolsPanel_, &ToolsPanel::runRequested, this,
            [this](const QString& line) { controller_->runLine(line, command::Origin::Gui); });

    propertyStack_ = new QStackedWidget(this);
    propertyStack_->addWidget(attributePanel_);
    propertyStack_->addWidget(transcript_);
    propertyStack_->addWidget(toolsPanel_);

    propertyHeader_ = new PanelHeader(this);
    propertyHeader_->addTab(tr("Öznitelikler"), static_cast<int>(Glyph::Table));
    propertyHeader_->addTab(tr("Geçmiş"), static_cast<int>(Glyph::History));
    propertyHeader_->addTab(tr("Araçlar"), static_cast<int>(Glyph::Function));
    connect(propertyHeader_, &PanelHeader::tabChanged, propertyStack_,
            &QStackedWidget::setCurrentIndex);

    propertyDock_ = makeDock(QStringLiteral("propertyDock"), propertyHeader_, propertyStack_);
    propertyDock_->toggleViewAction()->setText(tr("Öznitelikler"));

    // ---- the layers panel ----
    layerPanel_ = new LayerPanel(*controller_, this);

    // ---- and the external references, a tab beside them (TODOS C-14) ----
    //
    // BESIDE THE LAYERS because that is where a reference's own layers are,
    // and a panel of its own would be one more dock on a right edge that has
    // two already. The `+` attaches a file on this tab as it adds a layer on
    // the other.
    xrefPanel_ = new XrefPanel(*controller_, this);
    connect(xrefPanel_, &XrefPanel::attachRequested, this, [this] { actXref_->trigger(); });
    connect(xrefPanel_, &XrefPanel::notice, this, [this](const QString& text) {
        statusBar()->showMessage(text, 12000);
        if (statusStrip_ != nullptr) statusStrip_->setMessage(text);
    });
    connect(controller_, &Controller::commandFinished, xrefPanel_, &XrefPanel::onCommandFinished);
    layerStack_ = new QStackedWidget(this);
    layerStack_->addWidget(layerPanel_);
    layerStack_->addWidget(xrefPanel_);

    layerHeader_ = new PanelHeader(this);
    layerHeader_->addTab(tr("Katmanlar"), static_cast<int>(Glyph::Layer));
    layerHeader_->addTab(tr("Dış Referanslar"), static_cast<int>(Glyph::Xref));
    constexpr unsigned kDockMarks = PanelHeader::Grip | PanelHeader::Collapse | PanelHeader::Float;
    layerHeader_->setButtons(PanelHeader::Add | PanelHeader::Filter | kDockMarks);
    connect(layerHeader_, &PanelHeader::tabChanged, this, [this](int index) {
        layerStack_->setCurrentIndex(index);
        // The filter narrows a list of layers; the references need only the `+`.
        layerHeader_->setButtons(index == 0 ? PanelHeader::Add | PanelHeader::Filter | kDockMarks
                                            : PanelHeader::Add | kDockMarks);
    });
    connect(layerHeader_, &PanelHeader::buttonPressed, this, [this](int button) {
        // The panel's own two marks; the dock marks are answered where every
        // header's are.
        const bool references = layerStack_->currentWidget() == xrefPanel_;
        if (button == PanelHeader::Add && references) actXref_->trigger();
        if (button == PanelHeader::Add && !references) layerPanel_->addLayerInteractively();
        if (button == PanelHeader::Filter) layerPanel_->toggleFilter();
    });

    layerDock_ = makeDock(QStringLiteral("layerDock"), layerHeader_, layerStack_);
    layerDock_->toggleViewAction()->setText(tr("Katmanlar"));

    // ---- the conversation, hidden until asked for ----
    //
    // A DOCK AND NOT A WINDOW: the drawing is what the conversation is about, and
    // a modal chat would put the program's own subject behind the talk about it.
    // It opens on the RIGHT, under the layers, because that is where the panels
    // the user reads while drawing live.
    chatPanel_ =
        new ChatPanel(*controller_, controller_->aiService(), controller_->aiTransport(), this);
    chatPanel_->setProfileSource([this] { return controller_->providerService().profiles(); });
    connect(&controller_->providerService(), &ProviderService::profilesChanged, chatPanel_,
            &ChatPanel::refreshProfiles);
    connect(chatPanel_, &ChatPanel::said, this, &MainWindow::onEcho);

    // AN OUTSIDE CLIENT'S SUGGESTION REACHES THE PERSON. An MCP client's plan
    // had no card anywhere in the program, so under the default policy nobody
    // could apply it: the client was told to wait for an engineer who was never
    // shown anything. It goes in the chat dock — opened if closed — with the
    // same card the chat's own suggestions use (TODOS A-03).
    // WHAT A WAITING SUGGESTION WOULD LEAVE, on the canvas (TODOS F-05): drawn
    // dashed while the card waits, gone when it is decided.
    connect(&controller_->aiService(), &AiService::previewChanged, this, [this] {
        if (canvas_ != nullptr) canvas_->setPreviewGhosts(controller_->aiService().shownPreview());
    });

    connect(&controller_->aiService(), &AiService::suggestionFiled, this,
            [this](const QString& id) {
                if (chatPanel_ == nullptr) return;
                const core::Result<ai::Plan> held =
                    controller_->aiService().plan_state(id.toStdString(), std::string());
                if (!held || held.value().in_app) return;
                chatPanel_->showClientSuggestion(id);
                if (held.value().state == ai::PlanState::Pending) {
                    showChat();
                    // TALL ENOUGH TO READ THE CARD: a dock squeezed under the
                    // layers showed the notice's first line and hid its buttons.
                    if (chatDock_ != nullptr && chatDock_->height() < height() / 2)
                        resizeDocks({chatDock_}, {height() / 2}, Qt::Vertical);
                }
            });

    chatHeader_ = new PanelHeader(this);
    chatHeader_->addTab(tr("Yapay Zeka"), static_cast<int>(Glyph::Chat));
    chatHeader_->setButtons(PanelHeader::Grip | PanelHeader::Float | PanelHeader::Close);

    chatDock_ = makeDock(QStringLiteral("chatDock"), chatHeader_, chatPanel_);
    chatDock_->toggleViewAction()->setText(tr("Yapay Zeka"));

    // ---- the Python console, hidden until asked for ----
    //
    // AT THE BOTTOM, BESIDE THE JOURNAL, because it is the command line's sibling:
    // a place where the user types and the program answers. The right-hand docks
    // are things read WHILE drawing; this is a thing typed INSTEAD of drawing, and
    // it wants the full width a line of code needs.
    pythonConsole_ = new PythonConsole(*controller_, this);

    pythonHeader_ = new PanelHeader(this);
    pythonHeader_->addTab(tr("Python"), static_cast<int>(Glyph::Script));
    pythonHeader_->setButtons(PanelHeader::Grip | PanelHeader::Float | PanelHeader::Close);

    pythonDock_ = makeDock(QStringLiteral("pythonDock"), pythonHeader_, pythonConsole_);
    pythonDock_->toggleViewAction()->setText(tr("Python Konsolu"));

    // ---- the command journal, hidden until asked for ----
    journalHeader_ = new PanelHeader(this);
    journalHeader_->addTab(tr("Komut Günlüğü"), static_cast<int>(Glyph::Script));
    journalHeader_->setButtons(PanelHeader::Grip | PanelHeader::Float | PanelHeader::Close);

    journalDock_ = makeDock(QStringLiteral("journalDock"), journalHeader_, journalView_);
    journalDock_->toggleViewAction()->setText(tr("Komut Günlüğü"));

    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    addDockWidget(Qt::RightDockWidgetArea, layerDock_);
    addDockWidget(Qt::RightDockWidgetArea, chatDock_);
    addDockWidget(Qt::BottomDockWidgetArea, journalDock_);
    addDockWidget(Qt::BottomDockWidgetArea, pythonDock_);

    // The reference has no bottom panel open: the command line carries the
    // conversation and the journal is there when a user asks for it. The chat is
    // closed for the same reason — an assistant panel nobody opened is an
    // assistant panel taking a third of the screen.
    journalDock_->hide();
    chatDock_->hide();
    pythonDock_->hide();

    // 312 px wide, and the layers panel 268 px tall — both from design.md 7.
    resizeDocks({propertyDock_, layerDock_}, {312, 312}, Qt::Horizontal);
    resizeDocks({propertyDock_, layerDock_}, {600, 268}, Qt::Vertical);

    // These fire during TEARDOWN as well as during use — see the note in
    // `~MainWindow`, which is where the connection is severed.
    for (QDockWidget* dock : {propertyDock_, layerDock_, chatDock_, journalDock_, pythonDock_}) {
        connect(dock, &QDockWidget::topLevelChanged, this, [this] { syncDockTitles(); });
        connect(dock, &QDockWidget::visibilityChanged, this, [this] { syncDockTitles(); });
    }

    QSettings settings;
    if (settings.contains(QStringLiteral("ui/state"))) {
        restoreGeometry(settings.value(QStringLiteral("ui/geometry")).toByteArray());
        // The VERSION is the point: a layout saved by an older shell names docks
        // that no longer exist and sizes areas that have moved, and Qt restores it
        // faithfully — which is how a redesigned window opens looking like the old
        // one with holes in it. A bumped version makes `restoreState` decline.
        restoreState(settings.value(QStringLiteral("ui/state")).toByteArray(), kLayoutVersion);
    }
}

void MainWindow::openStyleDesigner(const QString& layerName)
{
    // The shell owns the window; the panel that asked for it does not have to know
    // what is in it. Everything the dialog changes leaves as a command, so what a
    // user designs here a script can write and the AI can be taught (Article 1.2).
    StyleDesigner designer(*controller_, layerName, this);
    designer.applyTheme(theme_);
    designer.exec();
}

void MainWindow::openSettings()
{
    // MODELESS, for the reason the database window is: a user changes an aid and
    // then wants to see what it did to the drawing, without the window that
    // changed it standing in front of the drawing. `settingChanged` keeps it in
    // step, so a value typed at the command line while it is open shows through.
    if (settings_ == nullptr) {
        settings_ = new SettingsDialog(*controller_, SettingsDialog::Mode::All, this);
        settings_->setAttribute(Qt::WA_DeleteOnClose);
        // No `destroyed` lambda: `settings_` is a QPointer and nulls itself. See
        // the note on the member for why the lambda was a write after this
        // window's own lifetime had ended.
    }
    settings_->applyTheme(theme_);
    settings_->show();
    settings_->raise();
    settings_->activateWindow();
}

void MainWindow::openProjectSettings()
{
    // ITS OWN WINDOW AND ITS OWN INSTANCE. `Seçenekler` answers "how do I want
    // this program to behave" and this one answers "what is inside this file";
    // a person often has both open, and sharing one window would make checking
    // the second close the first.
    if (projectSettings_ == nullptr) {
        projectSettings_ = new SettingsDialog(*controller_, SettingsDialog::Mode::Project, this);
        projectSettings_->setAttribute(Qt::WA_DeleteOnClose);
    }
    projectSettings_->applyTheme(theme_);
    projectSettings_->show();
    projectSettings_->raise();
    projectSettings_->activateWindow();
}

void MainWindow::openDatabase()
{
    // MODELESS, unlike the style designer: a user connects once and then keeps
    // drawing, writing a layer out whenever a piece of the work is finished.
    // Parented to the window so it closes with it, and `DeleteOnClose` so a
    // second Ctrl+Shift+D does not stack a second connection behind the first.
    if (database_ == nullptr) {
        database_ = new DatabaseDialog(*controller_, this);
        database_->setAttribute(Qt::WA_DeleteOnClose);
        // No `destroyed` lambda; see the note on the member.
    }
    database_->show();
    database_->raise();
    database_->activateWindow();
}

void MainWindow::loadSymbolLibrary()
{
    // Through the BUS, as a command, exactly like every other client (Article
    // 1.2). The shell gets no private road to the shelf: what it does here, a
    // script or the AI can do with the same line.
    //
    // TWO PACKAGES, IN THIS ORDER, and the order is the whole point. The annex's
    // own package carries every published row as the picture the regulation
    // printed; the vector package is loaded OVER it and replaces the rows that
    // have been redrawn — the shelf keeps one entry per id and a later package
    // restating a row updates it in place. A row nobody has redrawn yet keeps its
    // picture rather than going missing.
    const core::Settings& app = controller_->bus().app_settings();
    const QStringList declared{
        QString::fromStdString(std::string(app.get("core.stil.kutuphane").as_text())),
        QString::fromStdString(std::string(app.get("core.stil.vektor").as_text())),
    };

    for (const QString& one : declared) {
        if (one.trimmed().isEmpty()) continue;
        // Resolved against the SHIPPED data tree. The setting names its package
        // the way the documentation prints it — `data/catalogs/...` — and a
        // relative path is otherwise resolved against the working directory,
        // which is the one place it is guaranteed not to be. See data_root.hpp.
        const std::string path = data_path(one.trimmed().toStdString());
        if (path.empty()) continue;

        // Quoted, because a package path may contain a space and the parser is
        // the one parser (CLAUDE.md 5.11) rather than a second one written here.
        const auto result =
            controller_->bus().execute_line("SEMBOL paket=\"" + path + "\"", command::Origin::Gui);

        // A missing package is NOT an error the user has to dismiss. A fresh
        // machine may not have the data package installed yet, and the drawing
        // still opens: a document carries the symbols it uses in its own style
        // table. The note says what happened and the application carries on.
        if (!result)
            onEcho(tr("Sembol rafı eksik: %1").arg(QString::fromStdString(result.error().message)));
    }
}

void MainWindow::refreshAgentCell()
{
#if KENTOS_HAVE_MCP
    McpService* server = controller_->mcpService();
    if (server == nullptr) {
        statusStrip_->setAgent(tr("MCP yok"), StatusStrip::AgentState::Off);
        return;
    }
    if (!server->listening()) {
        statusStrip_->setAgent(tr("MCP kapalı"), StatusStrip::AgentState::Off);
        if (actMcp_ != nullptr) actMcp_->setText(tr("MCP Sunucusunu Başlat"));
        return;
    }
    if (actMcp_ != nullptr) actMcp_->setText(tr("MCP Sunucusunu Durdur"));

    // AN OPEN ENDPOINT IS NAMED, EVERY TIME THE OPERATOR LOOKS. The token is
    // required by default and only a deliberately cleared setting takes it away;
    // when it is gone, every process on this machine can drive the drawing, and
    // the cell says so in a word rather than in a shade of red (ui.md R31).
    if (!server->tokenRequired()) {
        statusStrip_->setAgent(tr("MCP %1 KORUMASIZ").arg(server->port()),
                               StatusStrip::AgentState::Unprotected);
        return;
    }
    statusStrip_->setAgent(tr("MCP %1").arg(server->port()), StatusStrip::AgentState::Guarded);
#else
    // A BUILD WITHOUT THE LISTENER SAYS SO rather than showing nothing: an empty
    // space where a state belongs reads as "off", and off and absent are
    // different answers to "can an agent connect to this machine".
    statusStrip_->setAgent(tr("MCP yok"), StatusStrip::AgentState::Off);
    if (actMcp_ != nullptr) actMcp_->setEnabled(false);
#endif
}

void MainWindow::syncDockTitles()
{
    // Every dock now wears a `PanelHeader` permanently (design.md 6), so there is
    // nothing to synchronise. This used to swap Qt's own title bar in and out
    // depending on whether a dock was tabbed — and with a custom header on every
    // dock it DELETED that header, which is how the panels lost their 29 px row
    // and picked up Qt's 19 px one with float and close buttons drawn by Fusion.
    //
    // Kept as a no-op rather than removed because the visibility and float
    // signals still land here; when a panel gains behaviour that depends on being
    // tabbed, this is where it goes.
}

void MainWindow::buildStatusBar()
{
    // design.md 7: one 26 px strip, painted rather than assembled. A QStatusBar
    // of QLabels reaches the reference's offsets only by accident — each label
    // brings its own margin and the style its own frame — so the strip draws
    // itself and the numbers in `shell_chrome.cpp` ARE the specification.
    statusStrip_ = new StatusStrip(this);
    // A job in flight (an import reading on a thread) shows on the strip with a
    // Durdur; the chip asks the controller to stop it, the same road Esc takes.
    // A job that COUNTS shows its figure beside its label: "<label> · %42". One
    // that streams (a file read) leaves the figure at zero and
    // the strip stays as it was. Polled, because the figure is written by the
    // worker thread and read here; a signal per object would be the wrong tool.
    jobPulse_ = new QTimer(this);
    jobPulse_->setInterval(150);
    connect(jobPulse_, &QTimer::timeout, this, [this] {
        const int permille = controller_->jobPermille();
        if (permille < 0) return;
        statusStrip_->setBusyLabel(tr("%1 · %%2").arg(jobLabel_).arg(permille / 10));
    });
    connect(controller_, &Controller::jobStarted, this, [this](const QString& label) {
        jobLabel_ = label;
        statusStrip_->setBusy(label, true);
        jobPulse_->start();
    });
    connect(controller_, &Controller::jobFinished, this, [this] {
        jobPulse_->stop();
        statusStrip_->setBusy(QString(), false);
    });
    connect(statusStrip_, &StatusStrip::stopRequested, this,
            [this] { controller_->cancelInteractive(); });

    // Each chip is the command it names. The mouse gets no private road: clicking
    // DİK runs `MOD dik_mod`, exactly as F8 and typing it do (Article 1.2). The
    // id on a chip is the SETTING it reads its state from; four of the seven are
    // session modes (model.md R43) and go through `MOD`, two are application
    // preferences and go through `TERCİH`. Two chips are not a setting of their
    // own but a face on the snap mask: OSNAP is "any object mode on", POLAR is
    // the kutupsal bit — both written as the mask, the way the F3 action writes it.
    statusStrip_->addToggle(tr("IZGARA"), QStringLiteral("core.izgara.gorunur"));
    statusStrip_->addToggle(tr("YAKALAMA"), QStringLiteral("core.yakalama.izgara"));
    statusStrip_->addToggle(tr("DİK"), QStringLiteral("core.yakalama.dik_mod"));
    statusStrip_->addToggle(tr("POLAR"), QString::fromLatin1(kChipPolar));
    statusStrip_->addToggle(tr("NORMAL"), QStringLiteral("core.yakalama.yuzey_normali"));
    statusStrip_->addToggle(tr("OSNAP"), QString::fromLatin1(kChipOsnap));
    statusStrip_->addToggle(tr("DİNAMİK GİRDİ"), QStringLiteral("core.arayuz.dinamik_girdi"));
    statusStrip_->addToggle(tr("KALINLIK"), QStringLiteral("core.harita.kalinlik"));

    // ---- the agent listener's cell ----
    //
    // CLICKING IT RUNS THE COMMAND, which is the same thing the menu entry and a
    // script do (Article 1.2). The cell follows `McpService::stateChanged`, so it
    // cannot claim a port that is closed — and it says `KORUMASIZ` in words, not
    // only in red, when the listener is open with no token.
    connect(statusStrip_, &StatusStrip::agentClicked, this, [this] {
        if (actMcp_ != nullptr) actMcp_->trigger();
    });
#if KENTOS_HAVE_MCP
    if (McpService* server = controller_->mcpService(); server != nullptr)
        connect(server, &McpService::stateChanged, this, &MainWindow::refreshAgentCell);
#endif
    refreshAgentCell();

    connect(statusStrip_, &StatusStrip::configureRequested, this, [this](const QString& id) {
        if (id == QString::fromLatin1(kChipOsnap) || id == QString::fromLatin1(kChipPolar)) {
            openSnapModes();
            return;
        }
        // Every other chip is a plain on/off, so "configure" means the page of
        // Ayarlar it lives on rather than a list of its own.
        openSettings();
    });

    connect(statusStrip_, &StatusStrip::toggled, this, [this](const QString& id) {
        const core::Settings& session = controller_->bus().session_settings();
        const int mask = static_cast<int>(session.get("core.yakalama.modlar").as_int());
        if (id == QString::fromLatin1(kChipOsnap)) {
            // The same words the F3 action sends: the mask remembered while
            // object snap was on, or nothing.
            const bool on = (static_cast<unsigned>(mask) & core::SnapObjectMask) != 0U;
            controller_->runLine(
                QStringLiteral("MOD yakalama_modları %1").arg(on ? 0 : snapMaskMemory_),
                command::Origin::Gui);
            return;
        }
        if (id == QString::fromLatin1(kChipPolar)) {
            controller_->runLine(
                QStringLiteral("MOD yakalama_modları %1")
                    .arg(static_cast<int>(static_cast<unsigned>(mask) ^ core::SnapPolar)),
                command::Origin::Gui);
            return;
        }
        const std::uint32_t index = core::builtin_settings().find(id.toStdString());
        if (index == core::kNoSetting) {
            onEcho(tr("Bu yardımcı henüz bir ayara bağlı değil: %1").arg(id));
            return;
        }
        const core::SettingSpec& spec = core::builtin_settings().at(index);
        // The store the SCOPE names, and the command that scope owns (R41): a
        // session mode is `MOD`, a preference is `TERCİH`, a project setting `AYAR`.
        const core::Settings& store =
            spec.scope == core::SettingScope::Session ? controller_->bus().session_settings()
            : spec.scope == core::SettingScope::App   ? controller_->bus().app_settings()
                                                      : controller_->bus().project_settings();
        const bool now     = store.get(id.toStdString()).as_bool();
        const QString verb = spec.scope == core::SettingScope::Session ? QStringLiteral("MOD")
                             : spec.scope == core::SettingScope::App   ? QStringLiteral("TERCİH")
                                                                       : QStringLiteral("AYAR");
        controller_->runLine(
            QStringLiteral("%1 %2 %3")
                .arg(verb, id, now ? QStringLiteral("hayır") : QStringLiteral("evet")),
            command::Origin::Gui);
    });
}

void MainWindow::applyTheme()
{
    const Palette& p = themePalette(theme_);
    qApp->setStyleSheet(themeStyleSheet(theme_));
    setStyleSheet(ribbonStyleSheet(theme_));

    // THE APPLICATION BUTTON IS AS WIDE AS ITS NAME. The ribbon sizes it from
    // its hint scaled to the tab row, which elides a two-word label, and every
    // sheet re-polishes a `min-width` onto it — so the floor is set after both:
    // the label in the face the ribbon sheet gives it (12 px, semibold), its
    // 14 px of padding a side, its 6 + 6 px of margin and a little air.
    if (appButton_ != nullptr) {
        QFont face(QStringLiteral("IBM Plex Sans"));
        face.setPixelSize(12);
        face.setWeight(QFont::DemiBold);
        constexpr int kAppButtonChrome = (2 * 14) + 6 + 6 + 8;
        appButton_->setMinimumWidth(QFontMetrics(face).horizontalAdvance(appButton_->text()) +
                                    kAppButtonChrome);
    }

    // AlternateBase comes from the palette, not the stylesheet: a stylesheet rule
    // for it loses to the more specific background rule on the same widget, and
    // the alternating rows then wash out in the dark theme.
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Base, p.field);
    palette.setColor(QPalette::AlternateBase, p.alternate);
    palette.setColor(QPalette::Text, p.text);
    palette.setColor(QPalette::WindowText, p.text);
    palette.setColor(QPalette::Highlight, p.accent);
    palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
    qApp->setPalette(palette);

    // Icons are drawn, not loaded, so they re-tint with the palette. Each action
    // carries its glyph in data(), which keeps this loop from being a second list
    // of actions to maintain. AN ACTION'S PICTURE IS IN COLOUR (`design.md` §5):
    // each role in its own ink, from the theme's tokens.
    const GlyphInks inks = actionInks();
    for (QAction* action : findChildren<QAction*>()) {
        const QVariant glyph = action->data();
        if (!glyph.isValid()) continue;
        action->setIcon(colour_icon(static_cast<Glyph>(glyph.toInt()), inks));
    }
    refreshRibbonPictures();

    // ONE WALK, not a list of calls. A list is a thing to forget an entry in,
    // and the settings window's sidebar proved it: every painted widget declares
    // `Themed` and this hands the theme to all of them at once.
    applyThemeToChildren(this, theme_);
    if (palette_) palette_->applyTheme(theme_);
    if (settings_) settings_->applyTheme(theme_);
}

GlyphInks MainWindow::actionInks() const
{
    const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
    return GlyphInks{t.iconInk,  t.iconShape, t.iconFill, t.iconCut,
                     t.iconNote, t.iconData,  t.iconAdd,  t.iconPaper};
}

void MainWindow::onSettingChanged(const QString& id)
{
    // The annotation defaults and the plot scale the ribbon shows are settings.
    refreshRibbonDefaults();
    // A preference written from the command line, a script or the AI must land on
    // screen exactly as the menu item does. Reading the value back from the store
    // rather than trusting the caller keeps one source of truth.
    if (id.startsWith(QLatin1String("core.islem.")) && toolsPanel_ != nullptr)
        toolsPanel_->refresh();
    if (id.startsWith(QLatin1String("core.izgara."))) {
        canvas_->reloadGridSettings();
        canvas_->reloadSnapSettings();
        refreshAidActions();
        refreshStatus();
        canvas_->update();
        return;
    }

    if (id.startsWith(QLatin1String("core.yakalama.")) ||
        id.startsWith(QLatin1String("core.secim."))) {
        canvas_->reloadSnapSettings();
        refreshAidActions();
        refreshStatus();
        canvas_->update();
        return;
    }

    // The canvas's own look: the coordinate readout by the cursor, the line
    // weights, the scale bar and the north arrow. Re-read into the canvas, and the
    // strip's chips follow; the scene is rebuilt on the next paint.
    //
    // The angle unit and rule are in this list because the readout writes its
    // bearing with them: `MOD kural matematik` or `AYAR açı_birimi derece` must
    // change the figure on the dragged line at once, not on the next restart.
    if (id == QLatin1String("core.arayuz.dinamik_girdi") ||
        id.startsWith(QLatin1String("core.harita.")) ||
        id.startsWith(QLatin1String("core.cetvel.")) || id.startsWith(QLatin1String("core.aci."))) {
        canvas_->reloadGridSettings();
        refreshStatus();
        canvas_->update();
        return;
    }

    if (id != QLatin1String("core.arayuz.tema")) return;

    const ThemeMode wanted = themeFromPreferences();
    if (wanted == theme_) return;

    theme_ = wanted;
    applyTheme();

    QSignalBlocker block(actTheme_);
    actTheme_->setChecked(theme_ == ThemeMode::Dark);
}

void MainWindow::openSnapModes()
{
    // THE LIST IS THE ENGINE'S, not a table kept here. Every bit of `SnapAllMask`
    // becomes a row and takes its Turkish label from `core::snap_mode_label`, so a
    // mode added to the engine appears here without an edit (CLAUDE.md 5.10).
    QMenu menu(this);
    menu.setTitle(tr("Nesne yakalama modları"));

    const core::Settings& session = controller_->bus().session_settings();
    const auto mask = static_cast<std::uint32_t>(session.get("core.yakalama.modlar").as_int());

    // Writes the whole mask through MOD, which is the only road there is: the
    // command line, a script and this menu all set the same bits.
    //
    // THIRTY-TWO OF THEM, as the engine has. Sixteen was the width before
    // `SnapCentroid` filled the last one, and "Hepsi" truncating a mask it had
    // just been handed is the kind of defect that only shows up on the day a
    // seventeenth mode lands.
    const auto write = [this](std::uint32_t next) {
        controller_->runLine(QStringLiteral("MOD ad=yakalama_modları deger=%1").arg(next),
                             command::Origin::Gui);
    };

    for (std::uint32_t bit = 1; bit != 0; bit = static_cast<std::uint32_t>(bit << 1)) {
        if ((core::SnapAllMask & bit) == 0) continue;

        auto* row = menu.addAction(QString::fromUtf8(core::snap_mode_label(bit)));
        row->setCheckable(true);
        row->setChecked((mask & bit) != 0);

        // The machine name in the tip, because it is what a script writes and what
        // the transcript prints.
        row->setToolTip(QString::fromUtf8(core::snap_mode_id(bit)));
        connect(row, &QAction::triggered, this, [write, mask, bit](bool on) {
            write(static_cast<std::uint32_t>(on ? (mask | bit) : (mask & ~bit)));
        });
    }

    menu.addSeparator();
    connect(menu.addAction(tr("Hepsi")), &QAction::triggered, this,
            [write] { write(static_cast<std::uint32_t>(core::SnapAllMask)); });
    connect(menu.addAction(tr("Hiçbiri")), &QAction::triggered, this, [write] { write(0); });

    // ---- the STEP, which is not one of the mask's bits ----
    //
    // It constrains HOW FAR rather than WHERE, so it is a length rather than a
    // mode; it lives here because this is the menu a user opens when they are
    // thinking about how the cursor behaves.
    menu.addSeparator();
    const core::Mm step = session.get("core.yakalama.adim").as_length();
    auto* stepRow       = menu.addAction(
        step > 0 ? tr("Adım: %1 m…").arg(static_cast<double>(step) / 1000.0, 0, 'f', 3)
                 : tr("Adım: yok…"));
    connect(stepRow, &QAction::triggered, this, [this, step] {
        bool ok             = false;
        const double metres = QInputDialog::getDouble(
            this, tr("Çizim adımı"),
            tr("İmlecin bir önceki noktaya uzaklığı bu değerin katlarında durur.\n"
               "0 kapatır."),
            static_cast<double>(step) / 1000.0, 0.0, 1000000.0, 3, &ok);
        if (!ok) return;
        controller_->runLine(
            QStringLiteral("MOD ad=adım deger=%1").arg(static_cast<qlonglong>(metres * 1000.0)),
            command::Origin::Gui);
    });

    // Under the pointer when it came from the strip, and under the OSNAP chip
    // when it came from the keyboard — a menu that opens off-screen for a
    // keyboard user is a feature reachable only by mouse (ui.md P7).
    menu.exec(QCursor::pos().isNull() ? statusStrip_->mapToGlobal(QPoint(0, 0)) : QCursor::pos());
}

void MainWindow::refreshAidActions()
{
    const core::Settings& session = controller_->bus().session_settings();

    const int mask        = static_cast<int>(session.get("core.yakalama.modlar").as_int());
    const bool objectSnap = (static_cast<unsigned>(mask) & core::SnapObjectMask) != 0U;
    if (objectSnap) snapMaskMemory_ = mask;

    // Blocked because the tick is DERIVED from the store: writing it back would
    // dispatch the command again and fight whichever client just changed it.
    {
        QSignalBlocker block(actSnap_);
        actSnap_->setChecked(objectSnap);
    }
    {
        QSignalBlocker block(actOrtho_);
        actOrtho_->setChecked(session.get("core.yakalama.dik_mod").as_bool());
    }
    {
        QSignalBlocker block(actNormal_);
        actNormal_->setChecked(session.get("core.yakalama.yuzey_normali").as_bool());
    }
    {
        QSignalBlocker block(actGridSnap_);
        actGridSnap_->setChecked(session.get("core.yakalama.izgara").as_bool());
    }
}

void MainWindow::toggleTheme(bool dark)
{
    // The menu item is a client of the command bus, not a second way to set a
    // preference. `TERCİH tema koyu` typed into the command line and this toggle
    // are now literally the same write, which is what R38 asks for and what
    // CLAUDE.md 5.10 forbids duplicating — before this, TERCİH reported success
    // and changed nothing, and the theme in force was invisible to TERCİH.
    auto written = controller_->bus().execute_line(dark ? "TERCİH tema koyu" : "TERCİH tema acik",
                                                   command::Origin::Gui);
    if (!written) onEcho(QString::fromStdString(written.error().message));

    theme_ = themeFromPreferences();
    applyTheme();
}

void MainWindow::showCommandLine(bool visible)
{
    commandLine_->setVisible(visible);
    commandLineRule_->setVisible(visible);

    if (actCommandLine_->isChecked() != visible) {
        QSignalBlocker block(actCommandLine_);
        actCommandLine_->setChecked(visible);
    }
    if (visible) commandLine_->setFocus();
}

void MainWindow::openAttributeTable(const QString& layerName)
{
    // Rebuilt each time rather than kept: the window reads the document through
    // the controller and holds no copy, so there is nothing to keep alive, and a
    // stale one is one more thing that can disagree with the drawing.
    //
    // NAMED WHEN THE CALLER NAMES ONE. The Katman menu has no layer in mind and
    // means "the one I am drawing on"; the layer panel's context menu has one
    // under the pointer and means that one. Falling back to the active layer for
    // both would make the panel's entry lie about which layer it opened.
    const QString on = layerName.isEmpty() ? controller_->activeLayerName() : layerName;
    auto* table      = new AttributeTable(*controller_, on, this);
    table->setAttribute(Qt::WA_DeleteOnClose, true);
    table->applyTheme(theme_);
    table->show();
}

void MainWindow::openFindReplace()
{
    // ONE WINDOW: the key pressed again brings the open one forward rather
    // than a second window over it.
    if (findReplace_ == nullptr) {
        findReplace_ = new FindReplaceDialog(*controller_, this);
        findReplace_->setModal(false);
        findReplace_->applyTheme(theme_);
    }
    findReplace_->show();
    findReplace_->raise();
    findReplace_->activateWindow();
}

void MainWindow::activateEntity(core::EntityKey key)
{
    const core::Document& doc = controller_->document();
    const core::EntityId e    = doc.slot_of(key);
    if (e == core::kNoEntity || !doc.alive(e)) return;
    // EXACTLY THIS ONE, through SEÇ like every other pick: the first of the two
    // clicks chose it already unless it was added to a selection, and an edit
    // opened on a selection of five would edit all five.
    controller_->runLine(QStringLiteral("SEÇ mod=NESNE nesneler=%1").arg(core::raw(key)),
                         command::Origin::Gui);
    // Its editor tab's own edit, the one list that says what edits what.
    if (const std::optional<RibbonContext> which = ribbon_context_of(doc, e)) {
        const QAction* editor = ribbonLive_->editors[static_cast<std::size_t>(*which)];
        if (editor != nullptr && editor->isEnabled()) {
            // ONE EDIT, NOT A TOOL PICKED UP: the tool re-arms itself when its
            // run ends and would sit asking for the next objects, so the next
            // double click would be taken as its answer (`beginOneShot`).
            controller_->beginOneShot(editor->property(kToolCommand).toString(),
                                      command::Origin::Gui);
            // AND WHEN THE EDIT ASKS FOR WORDS FIRST — a caption's, a
            // dimension's — they are answered where they stand, in the box over
            // them holding what they say now, instead of at the bottom of the
            // window (`MapCanvas::editTextAt`).
            const command::Session* live = controller_->session();
            if (live != nullptr && live->waiting() &&
                live->prompt().kind == command::ParamKind::Text && doc.alive(e)) {
                const command::Prompt& asked = live->prompt();
                const QString now            = asked.choices.empty()
                                                   ? QString()
                                                   : QString::fromStdString(asked.choices.front());
                const std::uint32_t slot     = doc.entities().slot[e];
                const core::RingSpan rs      = doc.geometry().rings_of(slot);
                if (rs.count > 0 && doc.geometry().ring_count[rs.first] > 0) {
                    // A dimension's caption is centred on its first point; any
                    // other caption runs along its baseline, first point to last.
                    const auto xs         = doc.geometry().ring_xs(rs.first);
                    const auto ys         = doc.geometry().ring_ys(rs.first);
                    const bool dim        = doc.entities().kind[e] == core::kDimensionKind;
                    const core::Point2 at = dim ? core::Point2{xs.front(), ys.front()}
                                                : core::Point2{(xs.front() + xs.back()) / 2,
                                                               (ys.front() + ys.back()) / 2};
                    canvas_->editTextAt(at, now);
                }
            }
            return;
        }
    }
    // Anything else is edited where its geometry and attributes are: the
    // attribute panel, brought forward on it.
    propertyDock_->show();
    propertyDock_->raise();
    propertyHeader_->setCurrent(0);
}

void MainWindow::choosePick(const std::vector<core::EntityId>& candidates,
                            Qt::KeyboardModifiers modifiers)
{
    if (candidates.empty()) return;

    // WHAT WAS SELECTED BEFORE, so Esc means what Esc means. The window changes
    // the selection while the user walks the rows — that is how they see which
    // row is which — and a cancel that left the last row highlighted would have
    // silently made the choice it was cancelling.
    const std::vector<core::EntityKey> before = controller_->bus().selection().keys();

    // ONE LINE, ALWAYS THE SAME LINE. Browsing, choosing and cancelling all leave
    // through `core.select`, so the transcript of a click in this window reads
    // exactly like the command a script would have sent (Article 1.2).
    const auto send = [this](const std::vector<core::EntityKey>& keys, const char* how) {
        command::Args args;
        if (keys.empty()) {
            args.set("mod", command::Value::text("TEMİZLE"));
        } else {
            std::vector<std::int64_t> ids;
            ids.reserve(keys.size());
            for (const core::EntityKey key : keys)
                ids.push_back(static_cast<std::int64_t>(static_cast<std::uint64_t>(key)));
            args.set("mod", command::Value::text("NESNE"));
            args.set("nesneler", command::Value::ids(std::move(ids)));
            if (how != nullptr) args.set("islem", command::Value::text(how));
        }
        controller_->runInvocation(
            command::Invocation{"core.select", std::move(args), command::Origin::Gui});
    };

    PickList chooser(*controller_, candidates, this);
    chooser.applyTheme(theme_);

    // Replace while browsing whatever the modifiers say, because the question the
    // preview answers is "which one is this row", and a Shift-add preview would
    // answer a different one.
    connect(&chooser, &PickList::highlighted, this,
            [&send](core::EntityKey key) { send({key}, nullptr); });

    if (chooser.exec() != QDialog::Accepted) {
        send(before, nullptr);
        return;
    }

    // AND NOW THE MODIFIERS, against the selection as it was before the window
    // opened rather than against the preview it left behind. QGIS keys, the same
    // three `dispatchSelection` sends: Shift adds, Ctrl removes, a plain click
    // replaces.
    send(before, nullptr);
    const core::EntityKey key = chooser.picked();
    if (key == core::EntityKey::None) return;

    if (modifiers.testFlag(Qt::ShiftModifier))
        send({key}, "EKLE");
    else if (modifiers.testFlag(Qt::ControlModifier))
        send({key}, "ÇIKAR");
    else
        send({key}, nullptr);
}

void MainWindow::probePickList()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[secim] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    if (canvas_ == nullptr) {
        say(QStringLiteral("tuval yok"));
        return;
    }

    // ARMED BEFORE THE CLICK, because the click opens a MODAL window and does not
    // return until it closes. The timer fires inside that nested event loop,
    // which is the only place the chooser can be answered from.
    QTimer::singleShot(200, this, [say] {
        auto* chooser = qobject_cast<PickList*>(QApplication::activeModalWidget());
        if (chooser == nullptr) {
            say(QStringLiteral("liste açılmadı"));
            return;
        }

        auto* grid = chooser->findChild<QTableWidget*>();
        if (grid == nullptr || grid->rowCount() < 2) {
            say(QStringLiteral("listede iki satır yok"));
            chooser->reject();
            return;
        }

        say(QStringLiteral("liste: %1 satır").arg(grid->rowCount()));
        for (int row = 0; row < grid->rowCount(); ++row)
            say(QStringLiteral("satır %1: %2 · %3 · %4 · %5")
                    .arg(row + 1)
                    .arg(grid->item(row, 0) != nullptr ? grid->item(row, 0)->text() : QString())
                    .arg(grid->item(row, 1) != nullptr ? grid->item(row, 1)->text() : QString())
                    .arg(grid->item(row, 2) != nullptr ? grid->item(row, 2)->text() : QString())
                    .arg(grid->item(row, 3) != nullptr ? grid->item(row, 3)->text() : QString()));

        // THE SECOND ROW, which is the whole point: the first is what
        // `pick_nearest` was already choosing, and reaching past it is the
        // capability that did not exist.
        grid->setCurrentCell(1, 0);

        // Photographed when the variable carries a path, the same bargain
        // `KENTOS_HAND_PROBE` makes: a transcript proves the rows are right and
        // says nothing about whether a person can read them.
        if (const QByteArray into = qgetenv("KENTOS_PICK_PROBE"); !into.isEmpty() && into != "1") {
            const QString file = QString::fromLocal8Bit(into);
            if (chooser->grab().save(file))
                say(QStringLiteral("kare yazıldı: %1").arg(file));
            else
                say(QStringLiteral("kare yazılamadı: %1").arg(file));
        }

        chooser->accept();
    });

    const QPoint at(canvas_->width() / 2, canvas_->height() / 2);
    QMouseEvent press(QEvent::MouseButtonPress, at, canvas_->mapToGlobal(at), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, at, canvas_->mapToGlobal(at), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas_, &press);
    QCoreApplication::sendEvent(canvas_, &release);

    const command::Selection& picked = controller_->bus().selection();
    say(QStringLiteral("seçim: %1 nesne%2")
            .arg(picked.size())
            .arg(picked.size() == 1 ? QStringLiteral(", kimlik %1")
                                          .arg(static_cast<qulonglong>(
                                              static_cast<std::uint64_t>(picked.keys().front())))
                                    : QString()));
}

void MainWindow::probeSurfaceNormal()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[normal] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    if (canvas_ == nullptr) {
        say(QStringLiteral("tuval yok"));
        return;
    }

    // A 45 degree edge, so the answer is a round number and a wrong answer is
    // obvious: its normal is 135 or 315, and nothing else is within 40 degrees.
    runScriptLine(QStringLiteral("KATMAN ad=SINIR"));
    runScriptLine(QStringLiteral("ÇİZGİ 0,0 40,40"));
    endCommand(); ///< a run is written when it ends (TODOS C-02)
    canvas_->zoomToExtents();
    QCoreApplication::processEvents();

    const auto at = [this](core::Point2 world) {
        const auto p = canvas_->view().to_screen(world);
        return QPointF(p.x, p.y);
    };

    const auto click = [this](const QPointF& where) {
        QMouseEvent press(QEvent::MouseButtonPress, where, canvas_->mapToGlobal(where),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, where, canvas_->mapToGlobal(where),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &press);
        QCoreApplication::sendEvent(canvas_, &release);
        QCoreApplication::processEvents();
    };

    const auto key = [this](QEvent::Type type, int which) {
        QKeyEvent event(type, which, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &event);
        QCoreApplication::processEvents();
    };

    // Where the last line the document was given ends, in metres, to three
    // decimals — the figure a perpendicular is drawn to produce.
    const auto lastEnd = [this]() -> QString {
        const core::Document& doc  = controller_->document();
        const auto last            = static_cast<core::EntityId>(doc.entities().size() - 1);
        const core::RingSpan rings = doc.geometry().rings_of(doc.entities().slot[last]);
        if (rings.count == 0) return {};
        const std::span<const core::Mm> xs = doc.geometry().ring_xs(rings.first);
        const std::span<const core::Mm> ys = doc.geometry().ring_ys(rings.first);
        if (xs.empty()) return {};
        return QStringLiteral("%1, %2")
            .arg(static_cast<double>(xs.back()) / 1000.0, 0, 'f', 3)
            .arg(static_cast<double>(ys.back()) / 1000.0, 0, 'f', 3);
    };

    // The angle of the last line the document was given, in degrees, measured
    // from its first vertex to its last.
    const auto drawnAngle = [this]() -> double {
        const core::Document& doc  = controller_->document();
        const auto last            = static_cast<core::EntityId>(doc.entities().size() - 1);
        const core::RingSpan rings = doc.geometry().rings_of(doc.entities().slot[last]);
        if (rings.count == 0) return std::numeric_limits<double>::quiet_NaN();
        const std::span<const core::Mm> xs = doc.geometry().ring_xs(rings.first);
        const std::span<const core::Mm> ys = doc.geometry().ring_ys(rings.first);
        if (xs.size() < 2) return std::numeric_limits<double>::quiet_NaN();
        const double dx = static_cast<double>(xs.back() - xs.front());
        const double dy = static_cast<double>(ys.back() - ys.front());
        double turn     = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        if (turn < 0.0) turn += 360.0;
        return turn;
    };

    // The anchor is the MIDPOINT of that edge. Two cursor positions, both well
    // clear of the edge so no object snap can claim the click, and the pair is
    // the whole test: one aimed roughly along the perpendicular and one nowhere
    // near it. Millimetres here, because that is what the document stores; the
    // line above is a command line and speaks metres.
    const core::Point2 anchor{20000, 20000};
    const core::Point2 away{38000, 18000};  ///< 353.7°, 38° off the normal
    const core::Point2 toward{38000, 6000}; ///< 322.1°, 7° off it

    // UNDONE AFTER EACH RUN, and it is not tidiness. The first line drawn leaves
    // an ENDPOINT at the cursor position, and the next run's click snaps to it —
    // object snap outranks every direction lock, so run two and run three both
    // came back with the aim untouched and the lock looked broken when it was
    // the scaffolding that was.
    const auto draw = [&](int hold, core::Point2 target) {
        controller_->runCommand(QStringLiteral("ÇİZGİ"));
        QCoreApplication::processEvents();
        click(at(anchor));
        if (hold != 0) key(QEvent::KeyPress, hold);
        click(at(target));
        if (hold != 0) key(QEvent::KeyRelease, hold);
        // ESC, not Enter. The line command's loop ends when its point awaiter is
        // cancelled, and what it has already drawn is committed — the same
        // "finish the polyline" that ESC has meant in every CAD program. Enter
        // left the command parked, and everything after it measured a line that
        // had not landed yet.
        key(QEvent::KeyPress, Qt::Key_Escape);
        QCoreApplication::processEvents();

        const double turn = drawnAngle();
        (void)controller_->bus().execute_line("GERİAL", command::Origin::Gui);
        QCoreApplication::processEvents();
        return turn;
    };

    const double freeTurn = draw(0, toward);
    say(QStringLiteral("serbest: %1°").arg(freeTurn, 0, 'f', 3));

    // The control: dik mod on the same click squares the line to the SHEET, which
    // on this edge is exactly the wrong answer. Printing both is what makes the
    // third figure mean something.
    (void)controller_->bus().execute_line("MOD dik_mod evet", command::Origin::Gui);
    say(QStringLiteral("dik mod: %1°").arg(draw(0, toward), 0, 'f', 3));
    (void)controller_->bus().execute_line("MOD dik_mod hayır", command::Origin::Gui);

    // INSIDE THE CONE: the aim is 7 degrees off the perpendicular and lands
    // exactly on it.
    const double lockedTurn = draw(Qt::Key_Shift, toward);
    say(QStringLiteral("kilitli: %1°").arg(lockedTurn, 0, 'f', 3));

    // OUTSIDE IT, and this is the line the user complained about. Held as an
    // absolute lock the aid answered 315 here too, which meant that with it on
    // there was no other direction left to draw or to measure in. A snap offers a
    // point when the aim is near it and stands aside when it is not.
    say(QStringLiteral("koni dışı: %1°").arg(draw(Qt::Key_Shift, away), 0, 'f', 3));

    // THE FAR EDGE, which is what a perpendicular is actually for: it runs from
    // one boundary ACROSS to another and ends there. A second line parallel to
    // the first, so the normal from the midpoint meets it square.
    //
    // STRAIGHT TO THE BUS, not through `runScriptLine`. ÇİZGİ repeats, so the
    // shell re-arms it the moment a run ends, and `Controller::runLine` hands a
    // typed line to a PARKED command before treating it as a command of its own
    // — correctly, that is how a coordinate gets typed at a prompt. A scene being
    // set up is not a user answering a prompt, so it goes in as a dispatch. The
    // first version of this used `runScriptLine`, the line was swallowed as an
    // answer, and the check below quietly measured a drawing with nothing across
    // it and still passed the angle.
    (void)controller_->bus().execute_line("ÇİZGİ 30,-10 50,10", command::Origin::Gui);
    canvas_->zoomToExtents();
    QCoreApplication::processEvents();

    const core::Point2 across{40000, 500};
    say(QStringLiteral("karşı kenar: %1°").arg(draw(Qt::Key_Shift, across), 0, 'f', 3));
    say(QStringLiteral("indiği nokta: %1").arg(lastEnd()));

    // CTRL, THE OTHER HELD LOCK, and it is here because it was broken in exactly
    // the way the one above was: both sent `MOD` a BOOLEAN for a parameter
    // declared as text, so the bus refused the call, the echo carried the refusal
    // where nobody was looking, and holding Ctrl locked nothing at all. A second
    // aim, 33.7 degrees off, so the 45 degree ray is a different answer from both
    // the free bearing and dik mod's.
    const core::Point2 aslant{38000, 32000};
    say(QStringLiteral("köşegen: %1°").arg(draw(Qt::Key_Control, aslant), 0, 'f', 3));

    // AND THE KEY LET GO OF IT. A lock that stayed on after the key came up would
    // steer every line drawn afterwards, which is the failure a held modifier
    // makes and a latched one cannot.
    say(QStringLiteral("bırakınca: %1")
            .arg(controller_->bus().session_settings().get("core.yakalama.yuzey_normali").as_bool()
                     ? QStringLiteral("açık")
                     : QStringLiteral("kapalı")));
}

void MainWindow::probeToolFamily()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[aile] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    // THE CIRCLE'S SPLIT BUTTON on the tab the window opens on — found by what
    // its face sends rather than by its place on the ribbon, so re-ordering a
    // panel cannot make this probe silently check a different button.
    const RibbonFamily* circle = nullptr;
    for (const RibbonFamily* f : std::as_const(families_))
        if (f->face() != nullptr &&
            f->face()->property(kToolCommand).toString() == QStringLiteral("DAİRE")) {
            circle = f;
            break;
        }
    if (circle == nullptr) {
        say(QStringLiteral("DAİRE ailesi yok"));
        return;
    }
    QMenu* menu  = circle->head()->menu();
    auto* button = qobject_cast<SARibbonToolButton*>(ribbonButton(circle->head()));
    if (menu == nullptr || button == nullptr) {
        say(QStringLiteral("ailenin bölünmüş düğmesi yok"));
        return;
    }

    // WHAT IT LISTS, in order, by the line each member sends. The list is the
    // only place a user is told that the circle through three points is
    // `DAİRE yontem=3n`, which is what they will type tomorrow.
    QStringList members;
    for (const QAction* member : menu->actions())
        if (!member->isSeparator()) members << member->property(kToolCommand).toString();
    say(QStringLiteral("üyeler: %1").arg(members.join(QStringLiteral(" · "))));

    QString prompt;
    const QMetaObject::Connection watch =
        connect(controller_, &Controller::promptChanged, this,
                [&prompt](const QString& asked) { prompt = asked; });

    // THE ARROW, PRESSED THE WAY A HAND PRESSES IT. A large button opens its
    // list from its lower half, the label and the arrow; a row button from the
    // arrow at its right.
    QCoreApplication::processEvents();
    const bool large           = button->buttonType() == SARibbonToolButton::LargeButton;
    const QPoint arrow         = large ? QPoint(button->width() / 2, button->height() - 6)
                                       : QPoint(button->width() - 6, button->height() / 2);
    const QPoint arrowOnScreen = button->mapToGlobal(arrow);
    // The list is modal and runs its own loop, so what happens while it is open
    // is set up before the press that opens it.
    QTimer::singleShot(150, menu, [menu, arrowOnScreen, say] {
        say(menu->isVisible() ? QStringLiteral("liste açıldı") : QStringLiteral("liste açılmadı"));
        if (!menu->isVisible()) return;
        // AND IT SURVIVES THE HAND LETTING GO. The press that opened the list
        // is still down; its release arrives with the pointer on the button,
        // on no row, and closing on it would make the list open and vanish in
        // the same motion.
        QMouseEvent letGo(QEvent::MouseButtonRelease, menu->mapFromGlobal(arrowOnScreen),
                          arrowOnScreen, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(menu, &letGo);
        QCoreApplication::processEvents();
        say(QStringLiteral("bırakınca: %1")
                .arg(menu->isVisible() ? QStringLiteral("açık") : QStringLiteral("kapalı")));
        if (!menu->isVisible()) return;
        // THE SECOND MEMBER, because taking the first would prove nothing the
        // face did not already do — clicked where it is drawn.
        QAction* second = nullptr;
        int seen        = 0;
        for (QAction* a : menu->actions())
            if (!a->isSeparator() && ++seen == 2) {
                second = a;
                break;
            }
        if (second == nullptr) return;
        const QPoint row         = menu->actionGeometry(second).center();
        const QPoint rowOnScreen = menu->mapToGlobal(row);
        QMouseEvent over(QEvent::MouseMove, row, rowOnScreen, Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(menu, &over);
        QMouseEvent down(QEvent::MouseButtonPress, row, rowOnScreen, Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(menu, &down);
        QMouseEvent up(QEvent::MouseButtonRelease, row, rowOnScreen, Qt::LeftButton, Qt::NoButton,
                       Qt::NoModifier);
        QCoreApplication::sendEvent(menu, &up);
    });
    QMouseEvent press(QEvent::MouseButtonPress, arrow, arrowOnScreen, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &press); // returns when the list has closed
    QCoreApplication::processEvents();

    // THE FACE FOLLOWS THE CHOICE, and the choice armed that method — not merely
    // repainted a button.
    say(QStringLiteral("düğmenin yüzü: %1")
            .arg(circle->face() != nullptr ? circle->face()->property(kToolCommand).toString()
                                           : QString()));
    const command::Session* running = controller_->session();
    say(QStringLiteral("çalışan komut: %1")
            .arg(running != nullptr ? QString::fromStdString(running->spec().id)
                                    : QStringLiteral("yok")));
    say(QStringLiteral("istem: %1").arg(prompt));
    disconnect(watch);
}

void MainWindow::probeSchemaPage()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[sema] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    // The window a user opens from the layer's context menu, on the page they
    // would click. Built here rather than shown modally, because a modal `exec`
    // never returns to a probe.
    StyleDesigner properties(*controller_, controller_->activeLayerName(), this);
    properties.applyTheme(theme_);

    auto* page = properties.findChild<SchemaPage*>();
    if (page == nullptr) {
        say(QStringLiteral("Öznitelikler sayfası yok"));
        return;
    }

    // EVERY TYPE, because the point of the page is that a column is not always a
    // line of text: a date gets a calendar, a decimal gets its digits, a yes/no
    // gets two words.
    const QStringList declarations{
        QStringLiteral("SÜTUN kimlik=\"ada\" tur=tam_sayi ad=\"Ada No\" zorunlu=evet"),
        QStringLiteral("SÜTUN kimlik=\"direk\" tur=uzunluk ad=\"Direk\" katman=\"ENERJİ\""),
        QStringLiteral("SÜTUN kimlik=\"oran\" tur=ondalik ad=\"Oran\" basamak=2 zorunlu=hayır"),
        QStringLiteral("SÜTUN kimlik=\"onay\" tur=tarih ad=\"Onay Tarihi\" zorunlu=hayır"),
        QStringLiteral("SÜTUN kimlik=\"tescilli\" tur=evet_hayir ad=\"Tescilli\" zorunlu=hayır"),
        QStringLiteral("SÜTUN kimlik=\"cephe\" tur=uzunluk ad=\"Cephe\" zorunlu=hayır"),
    };
    for (const QString& line : declarations)
        if (!page->probeAction(QStringLiteral("satir"), -1, line))
            say(QStringLiteral("gönderilemedi: %1").arg(line));

    // WHOSE PAGE THIS IS. The window was opened on a layer, so its page lists that
    // layer's columns and the project's; the `ENERJİ` column above belongs to
    // neither and must not appear.
    say(QStringLiteral("kapsam: %1").arg(controller_->activeLayerName()));
    say(QStringLiteral("sütun sayısı: %1").arg(page->probeRows().size()));
    for (const QString& row : page->probeRows())
        say(QStringLiteral("satır: %1").arg(row));

    // AN EDIT, which may change what a column SAYS and not what it is.
    page->probeAction(QStringLiteral("satir"), -1,
                      QStringLiteral("SÜTUN kimlik=\"oran\" ad=\"Ölçülen Oran\" "
                                     "basamak=3 zorunlu=evet"));
    say(QStringLiteral("düzenlendi: %1").arg(page->probeRows().value(1)));

    // AND A DROP, which is the one edit that loses data.
    page->probeAction(QStringLiteral("sil"), 4, QString());
    say(QStringLiteral("silindikten sonra: %1 sütun").arg(page->probeRows().size()));

    // THE PROJECT'S OWN WINDOW, which is the other half of the story: this page
    // declares a LAYER's columns, and the project's live there — beside the
    // settings that travel in the same file.
    {
        SettingsDialog project(*controller_, SettingsDialog::Mode::Project, this);
        project.applyTheme(theme_);
        say(QStringLiteral("proje penceresi: %1")
                .arg(project.probeSections().join(QStringLiteral(" | "))));
        say(QStringLiteral("proje ayarı: %1").arg(project.probeProjectSettings().size()));
        say(QStringLiteral("proje ayarları: %1")
                .arg(project.probeProjectSettings().join(QStringLiteral(", "))));

        // AND `Seçenekler` NO LONGER CARRIES THEM. Two windows both holding the
        // project's pages would be two places to look and one of them wrong.
        SettingsDialog options(*controller_, SettingsDialog::Mode::All, this);
        options.applyTheme(theme_);
        say(QStringLiteral("seçenekler son bölüm: %1").arg(options.probeSections().back()));
    }

    // PHOTOGRAPHED WHEN ASKED, the same bargain `KENTOS_PICK_PROBE` makes: a
    // transcript proves the rows are right and says nothing about whether a
    // person can read them. The frame around an input is exactly the kind of
    // thing only a picture answers.
    const QByteArray into = qgetenv("KENTOS_SCHEMA_PROBE");
    if (into.isEmpty() || into == "1") return;

    const QString dir = QString::fromLocal8Bit(into);
    QDir().mkpath(dir);

    properties.resize(1280, 880);
    properties.show();
    QCoreApplication::processEvents();
    if (properties.grab().save(dir + QStringLiteral("/katman-ozellikleri.png")))
        say(QStringLiteral("kare: katman-ozellikleri.png"));

    // THE INSPECTOR WITH A CELL OPEN, which is the picture the transcript cannot
    // take: the row's stored value used to be painted under the editor, so the
    // old text and the typed one sat on top of each other.
    runScriptLine(QStringLiteral("ÇİZGİ 0,0 10,10"));
    endCommand(); ///< a run is written when it ends (TODOS C-02)
    runScriptLine(QStringLiteral("SEÇ nesneler=1"));
    QCoreApplication::processEvents();
    // SIZED BEFORE IT IS ASKED. The panel lives in a dock that may be collapsed
    // while a probe runs, and `beginEdit` refuses a row whose rectangle has no
    // width — correctly, since there is nowhere to put the editor.
    if (attributePanel_ != nullptr) attributePanel_->resize(340, 620);
    if (attributePanel_ != nullptr) attributePanel_->refresh();
    QCoreApplication::processEvents();
    if (attributePanel_ != nullptr && attributePanel_->openRowForProbe(QStringLiteral("Ada No"))) {
        QCoreApplication::processEvents();
        if (attributePanel_->grab().save(dir + QStringLiteral("/denetci-duzenleme.png")))
            say(QStringLiteral("kare: denetci-duzenleme.png"));
    } else {
        say(QStringLiteral("denetçi satırı açılmadı; seçim %1 nesne, satırlar: %2")
                .arg(controller_->bus().selection().size())
                .arg(attributePanel_ != nullptr
                         ? attributePanel_->probeRowKeys().join(QStringLiteral(" | "))
                         : QString()));
    }

    ColumnDialog form(*controller_, QString(), controller_->activeLayerName(), &properties);
    form.applyTheme(theme_);
    form.show();
    QCoreApplication::processEvents();
    if (form.grab().save(dir + QStringLiteral("/sutun-formu.png")))
        say(QStringLiteral("kare: sutun-formu.png"));
}

void MainWindow::probeAttributeGrid()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[tablo] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    // A drawing with two objects and three columns, so the wrap at the end of a
    // row has somewhere to wrap TO.
    command::Bus& bus = controller_->bus();
    for (const char* line : {"KATMAN ad=PARSEL", "ÇİZGİ 0,0 10,0", "ÇİZGİ 0,5 10,5",
                             "SÜTUN kimlik=\"ada\" tur=tam_sayi ad=\"Ada\" zorunlu=evet",
                             "SÜTUN kimlik=\"oran\" tur=ondalik basamak=2 ad=\"Oran\"",
                             "SÜTUN kimlik=\"onay\" tur=tarih ad=\"Onay\""})
        (void)bus.execute_line(line, command::Origin::Gui);
    QCoreApplication::processEvents();

    AttributeTable table(*controller_, QString(), this);
    table.applyTheme(theme_);
    table.resize(1100, 640);
    table.show();
    QCoreApplication::processEvents();

    const auto drive = [&table](const char* action, const QString& value = QString()) {
        return table.probeGrid(QString::fromUtf8(action), value);
    };

    /// Puts a value in the open editor WITHOUT committing it, so the picture
    /// below shows a cell mid-edit rather than a cell that has been left.
    const auto drive2 = [](AttributeTable& t) {
        t.probeGrid(QStringLiteral("koy"), QStringLiteral("3325"));
        QCoreApplication::processEvents();
    };

    // ---- 1. the mode is off, so nothing opens ----
    drive("git", QStringLiteral("0,1"));
    say(QStringLiteral("kip kapalıyken: %1").arg(drive("ac")));

    // ---- 2. on, and the cursor is typed across ----
    say(QStringLiteral("kip: %1").arg(drive("kip", QStringLiteral("evet"))));
    drive("git", QStringLiteral("0,1"));
    drive("ac");
    say(QStringLiteral("kip açıkken: %1").arg(table.probeGrid(QStringLiteral("ac"), QString())));

    // ENTER CONFIRMS AND STAYS PUT — and the line each write reports is where
    // the cursor ended up, which must be the cell that was just confirmed.
    //
    // Enter used to open the next cell too, and the probe walked the row by
    // pressing it; that was taken back out at the user's request, so the cursor
    // is moved here the way a person moves it. Committing resets the model, so
    // "stays put" is not free: without the grid putting the cursor back, every
    // one of these reported `yok`.
    say(QStringLiteral("enter 1 -> %1").arg(drive("yaz", QStringLiteral("128"))));
    drive("git", QStringLiteral("0,2"));
    drive("ac");
    say(QStringLiteral("enter 2 -> %1").arg(drive("yaz", QStringLiteral("0,40"))));
    drive("git", QStringLiteral("0,3"));
    drive("ac");
    say(QStringLiteral("enter 3 -> %1").arg(drive("yaz", QStringLiteral("2026-09-08"))));

    // ---- 4. and a bad value is refused before it is sent ----
    drive("git", QStringLiteral("1,1"));
    drive("ac");
    drive("yaz", QStringLiteral("abc"));
    say(QStringLiteral("reddedilen: %1").arg(drive("sikayet").left(60)));

    // READ FROM THE MODEL, not from wherever the cursor ended up: an open editor
    // moves the focus around and `currentIndex` is about the view, while what is
    // being checked here is what the DOCUMENT ended up holding.
    say(QStringLiteral("tam sayı: %1").arg(drive("hucre", QStringLiteral("0,1"))));
    say(QStringLiteral("ondalık: %1").arg(drive("hucre", QStringLiteral("0,2"))));
    say(QStringLiteral("tarih: %1").arg(drive("hucre", QStringLiteral("0,3"))));

    // PHOTOGRAPHED WITH A CELL OPEN. Whether an editor covers what it replaces is
    // a question only a picture answers: the ground used to be the selection
    // accent at twelve per cent, so the stored value and the typed one were both
    // legible at once, at different alignments, in the same box.
    const QByteArray into = qgetenv("KENTOS_TABLE_PROBE");
    if (!into.isEmpty() && into != "1") {
        const QString dir = QString::fromLocal8Bit(into);
        QDir().mkpath(dir);

        // A FRESH WINDOW FOR THE PICTURES. The run above deliberately leaves
        // editors half-open — a refused value, a cell typed into and left — and
        // photographing that state would photograph the probe rather than the
        // program. A second table starts where a user would.
        AttributeTable shot(*controller_, QString(), this);
        shot.applyTheme(theme_);
        shot.resize(1100, 640);
        shot.show();
        QCoreApplication::processEvents();

        const auto pose = [&shot](const char* action, const QString& value = QString()) {
            return shot.probeGrid(QString::fromUtf8(action), value);
        };
        pose("kip", QStringLiteral("evet"));

        // THE CALENDAR FIRST, and on a cell that already HOLDS a date — the
        // question is not only whether it looks right but whether it opens on
        // the day the cell carries.
        pose("git", QStringLiteral("0,3"));
        pose("ac");
        say(QStringLiteral("takvim: %1").arg(pose("takvim")));
        say(QStringLiteral("takvim günü: %1").arg(pose("takvimgun")));
        if (auto* card = shot.findChild<DatePopup*>(); card != nullptr)
            if (card->grab().save(dir + QStringLiteral("/takvim.png")))
                say(QStringLiteral("kare: takvim.png"));

        pose("git", QStringLiteral("0,1"));
        pose("ac");
        drive2(shot);
        if (shot.grab().save(dir + QStringLiteral("/tablo-duzenleme.png")))
            say(QStringLiteral("kare: tablo-duzenleme.png"));
        shot.close();
    }

    table.close();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!settleBlockEdit(tr("Pencere kapanmadan önce")) ||
        !confirmDiscard(tr("Kapatmadan önce kaydedilsin mi?"))) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::probeWidgets()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[bilesen] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    // A top-level window of its own, so the sheet is measured at the sizes the
    // components take when nothing around them constrains them.
    QWidget* sheet = buildComponentSheet(theme_);
    sheet->setWindowTitle(tr("Bileşen Standardı"));
    sheet->resize(1240, 860);
    sheet->show();
    QCoreApplication::processEvents();

    for (const QString& line : componentSheetInventory(sheet))
        say(line);

    // Photographed when the variable carries a path, the same bargain every probe
    // here makes: an inventory proves the components exist at their heights, and
    // says nothing about whether a person would call them one set.
    const QByteArray into = qgetenv("KENTOS_WIDGETS_PROBE");
    if (!into.isEmpty() && into != "1") {
        const QString dir = QString::fromLocal8Bit(into);
        QDir().mkpath(dir);
        if (sheet->grab().save(dir + QStringLiteral("/bilesenler.png")))
            say(QStringLiteral("kare: bilesenler.png"));
    }

    sheet->close();
    delete sheet;
}

void MainWindow::seedProbeDrawing()
{
    // A drawing with enough in it for every window to show something: a named
    // layer, three objects, five typed columns and values on the parcels.
    command::Bus& bus = controller_->bus();
    for (const char* line :
         {"KATMAN ad=\"Kadastro Parselleri\"", "ALAN 0,0 100,0 100,80 0,80",
          "ALAN 120,0 220,0 220,80 120,80", "ÇİZGİ 0,100 220,100",
          "SÜTUN kimlik=\"ada_no\" tur=tam_sayi ad=\"Ada No\" zorunlu=evet",
          "SÜTUN kimlik=\"parsel_no\" tur=tam_sayi ad=\"Parsel No\" zorunlu=evet",
          "SÜTUN kimlik=\"alan_m2\" tur=ondalik basamak=2 ad=\"Alan\"",
          "SÜTUN kimlik=\"nitelik\" tur=metin ad=\"Nitelik\"",
          "SÜTUN kimlik=\"tapu_tarih\" tur=tarih ad=\"Tapu Tarihi\"",
          "ÖZNİTELİK ad=ada_no nesne=1 deger=1284", "ÖZNİTELİK ad=parsel_no nesne=1 deger=21",
          "ÖZNİTELİK ad=nitelik nesne=1 deger=Arsa",
          "ÖZNİTELİK ad=tapu_tarih nesne=1 deger=2019-03-14",
          "ÖZNİTELİK ad=ada_no nesne=2 deger=1284", "ÖZNİTELİK ad=parsel_no nesne=2 deger=22",
          "ÖZNİTELİK ad=nitelik nesne=2 deger=Tarla"})
        (void)bus.execute_line(line, command::Origin::Gui);
    QCoreApplication::processEvents();
}

void MainWindow::probeDesigner()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[tasarimci] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };
    theme_ = ThemeMode::Dark;
    applyTheme();
    seedProbeDrawing();

    StyleDesigner designer(*controller_, QStringLiteral("Kadastro Parselleri"), this);
    designer.applyTheme(theme_);
    designer.resize(1280, 756);
    designer.show();
    QCoreApplication::processEvents();

    for (const QString& line : designer.probeRenderer(QStringLiteral("nitelik")))
        say(line);

    const QByteArray into = qgetenv("KENTOS_DESIGNER_PROBE");
    if (!into.isEmpty() && into != "1") {
        const QString dir = QString::fromLocal8Bit(into);
        QDir().mkpath(dir);
        QCoreApplication::processEvents();
        if (designer.grab().save(dir + QStringLiteral("/tasarimci-kategori.png")))
            say(QStringLiteral("kare: tasarimci-kategori.png"));
    }

    // And a second window on the same layer finds the classes again.
    StyleDesigner again(*controller_, QStringLiteral("Kadastro Parselleri"), this);
    again.applyTheme(theme_);
    say(QStringLiteral("yeniden açılış: %1")
            .arg(again.symbol().layers.empty() ? QStringLiteral("boş")
                                               : QStringLiteral("sembol var")));
    designer.close();
}

int MainWindow::probeHelpPage()
{
    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[yardim] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        if (!ok) ++failures;
    };

    // EXACTLY WHAT THE MENU DOES. Not `openCommandSearch`: the point is that the
    // command reaches the page, which is the seam the menu, `Ctrl+K`, a typed
    // `YARDIM` and an agent all share.
    //
    // The transcript's own sink is borrowed for the call so the lines can be
    // counted as well as shown: the page is the answer for a person, and the
    // text is still the answer for a script, and BOTH have to happen.
    std::vector<std::string> said;
    auto transcript            = controller_->bus().on_echo;
    controller_->bus().on_echo = [&said](std::string_view line) { said.emplace_back(line); };
    controller_->runLine(QStringLiteral("YARDIM"), command::Origin::Gui);
    controller_->bus().on_echo = std::move(transcript);
    QCoreApplication::processEvents();

    for (const std::string& line : said)
        (void)std::fprintf(stdout, "[yardim] metin| %s\n", line.c_str());
    check(
        said.size() > 1 && said.size() < 20,
        QStringLiteral("metin de yazıldı, ama bir liste dökümü değil (%1 satır)").arg(said.size()));

    check(palette_ != nullptr, QStringLiteral("YARDIM sayfayı açtı"));
    if (palette_ == nullptr) return failures;
    check(palette_->isVisible(), QStringLiteral("sayfa görünür"));

    const CommandPalette::Shown page = palette_->shown();
    (void)std::fprintf(stdout, "[yardim] %d komut, %d başlık, kaydırma %d, yükseklik %d\n",
                       page.commands, page.headings, page.scroll_max, page.height);

    check(page.commands == static_cast<int>(controller_->registry().size()),
          QStringLiteral("her komut listede (%1)").arg(controller_->registry().size()));
    check(page.headings > 1, QStringLiteral("kategori başlıkları var"));
    // The two complaints, in order: it scrolls, and it does not grow past a
    // screen. A list that fits has `maximum() == 0`; this one cannot fit.
    check(page.scroll_max > 0, QStringLiteral("liste kaydırılabiliyor"));
    check(page.height > 0 && page.height < 900, QStringLiteral("sayfa ekranı aşmıyor"));
    check(!page.selected.isEmpty(), QStringLiteral("bir komut seçili"));
    check(!page.detail.isEmpty(), QStringLiteral("sağ bölme dolu"));

    // And asked about ONE command, the page opens on it — the `komut=` form.
    controller_->runLine(QStringLiteral("YARDIM komut=ÖLÇÜ"), command::Origin::Gui);
    QCoreApplication::processEvents();
    const CommandPalette::Shown on_one = palette_->shown();
    check(on_one.selected == QStringLiteral("ÖLÇÜ"),
          QStringLiteral("YARDIM komut=ÖLÇÜ ÖLÇÜ'yü seçti (%1)").arg(on_one.selected));
    check(on_one.detail == QStringLiteral("ÖLÇÜ"), QStringLiteral("sağ bölme ÖLÇÜ'yü anlatıyor"));

    palette_->hide();
    return failures;
}

int MainWindow::probeClipboard()
{
    // WHAT ONLY A WINDOW CAN ANSWER. `/tests` links no Qt, so nothing there can
    // see whether the payload reached `QClipboard` under the agreed type and came
    // back out of it. The temp file is DELETED between the copy and the paste, so
    // a paste that still worked can only have come from the system clipboard.
    int failures   = 0;
    const auto say = [&](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[pano] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        if (!ok) ++failures;
    };

    // ONE LINE AT A TIME, each settled before the next is read. `runScriptLine`
    // goes through the bus the way the command line does and the window keeps
    // running in between, so counting straight after the call counts a document
    // that has not been written yet — which is what made the first run of this
    // probe report one parcel out of two.
    const auto run = [this](const QString& line) {
        runScriptLine(line);
        QCoreApplication::processEvents();
        // AND ENDED, the way the right button ends one. A draw command takes an
        // unbounded run of points and PARKS waiting for the next; the line after
        // it would then be swallowed as input rather than run as a command — the
        // first version of this probe drew one parcel out of two and the second
        // ALAN never appeared in the transcript at all.
        controller_->finishInteractive();
        QCoreApplication::processEvents();
    };

    // FROM AN EMPTY DRAWING, because the window opens on sample content and
    // `SEÇ HEPSİ` would then copy that too.
    run(QStringLiteral("YENİ"));
    run(QStringLiteral("KATMAN ad=PANO"));
    run(QStringLiteral("ALAN 0,0 10,0 10,10 0,10"));
    run(QStringLiteral("ALAN 20,0 30,0 30,10 20,10"));
    run(QStringLiteral("SEÇ HEPSİ"));
    const std::size_t before = controller_->document().live_entity_count();
    say(before == 2, QStringLiteral("iki parsel çizildi: %1").arg(static_cast<int>(before)));

    run(QStringLiteral("PANOYAKOPYALA"));

    const QMimeData* held = QGuiApplication::clipboard()->mimeData();
    const QString mime    = QString::fromUtf8(io::FileService::kClipboardMime);
    say(held != nullptr && held->hasFormat(mime),
        QStringLiteral("yük işletim sistemi panosunda, kendi MIME türüyle"));
    const qsizetype bytes = held != nullptr && held->hasFormat(mime) ? held->data(mime).size() : 0;
    say(bytes > 0, QStringLiteral("yük boş değil: %1 bayt").arg(bytes));

    // THE TEMP FILE GOES AWAY, so the paste below cannot read it. Whatever comes
    // back has come out of the system clipboard and nowhere else.
    const QString scratch = QString::fromStdString(controller_->clipboardPath());
    QFile::remove(scratch);
    say(!QFile::exists(scratch), QStringLiteral("geçici dosya silindi"));

    run(QStringLiteral("YENİ"));
    say(controller_->document().live_entity_count() == 0, QStringLiteral("yeni çizim boş"));

    run(QStringLiteral("YAPIŞTIR yerinde=evet"));
    const std::size_t after = controller_->document().live_entity_count();
    say(after == 2, QStringLiteral("pano yalnız sistem panosundan yapıştırdı: %1 nesne")
                        .arg(static_cast<int>(after)));

    (void)std::fprintf(stdout, "[pano] %d kusur\n", failures);
    return failures;
}

int MainWindow::probeFit()
{
    int defects      = 0;
    const auto count = [](const QWidget* w) { return w != nullptr && w->isVisible(); };
    const auto say   = [](const char* what, QSize s) {
        (void)std::fprintf(stdout, "[sığ] %-26s %4d × %4d\n", what, s.width(), s.height());
    };

    say("pencere en küçük", minimumSizeHint());
    if (count(ribbonBar())) say("şerit en küçük", ribbonBar()->minimumSizeHint());
    if (count(pythonDock_)) say("Python paneli en küçük", pythonDock_->minimumSizeHint());
    if (count(propertyDock_)) say("özellik paneli en küçük", propertyDock_->minimumSizeHint());
    if (count(layerDock_)) say("katman paneli en küçük", layerDock_->minimumSizeHint());
    if (count(journalDock_)) say("geçmiş paneli en küçük", journalDock_->minimumSizeHint());
    if (count(chatDock_)) say("sohbet paneli en küçük", chatDock_->minimumSizeHint());
    if (centralWidget() != nullptr) say("orta alan en küçük", centralWidget()->minimumSizeHint());

    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_FIT_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    // AND WITH THE CONSOLE OPEN, which is how the person who reported this
    // works: the bottom dock takes its height out of the same window.
    for (const bool console : {false, true})
        for (const QSize want : {QSize(1280, 720), QSize(1440, 860)}) {
            // The console is opened the way its menu entry opens it, which is
            // also what repairs a dock a saved layout left collapsed or floating.
            if (console)
                showPythonConsole();
            else if (pythonDock_ != nullptr)
                pythonDock_->hide();
            resize(want);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            (void)std::fprintf(stdout, "[sığ] Python paneli %s\n", console ? "açık" : "kapalı");
            const QSize got = size();
            const bool fits = got.width() <= want.width() && got.height() <= want.height();
            (void)std::fprintf(fits ? stdout : stderr,
                               "[sığ] %4d × %4d istendi, %4d × %4d oldu — %s\n", want.width(),
                               want.height(), got.width(), got.height(),
                               fits ? "sığıyor" : "EKRANA SIĞMIYOR");
            if (!fits) ++defects;

            // THE CHROME'S BUDGET (`design.md` §1, `.claude/ui.md` R50): the
            // ribbon within 134 px and, with the status strip, within 160 px, at
            // every size. A window too narrow for a tab scrolls its panels
            // rather than hiding them, so what can go wrong here is height.
            if (const SARibbonBar* bar = ribbonBar(); bar != nullptr) {
                constexpr int kRibbonBudget = 134;
                constexpr int kChromeBudget = 160;
                const int strip             = statusStrip_ != nullptr ? statusStrip_->height() : 0;
                const int chrome            = bar->height() + strip;
                const bool within = bar->height() <= kRibbonBudget && chrome <= kChromeBudget;
                (void)std::fprintf(within ? stdout : stderr,
                                   "[sığ]   şerit %d px, şerit ve durum çubuğu %d px — %s\n",
                                   bar->height(), chrome, within ? "bütçede" : "BÜTÇEYİ AŞIYOR");
                if (!within) ++defects;
            }
            if (shooting)
                (void)grab().save(
                    into + QStringLiteral("/sigma-%1x%2-%3.png")
                               .arg(want.width())
                               .arg(want.height())
                               .arg(console ? QStringLiteral("konsol") : QStringLiteral("yalin")));
        }
    if (pythonDock_ != nullptr) pythonDock_->hide();
    (void)std::fprintf(stdout, "[sığ] %d kusur\n", defects);
    return defects;
}

int MainWindow::probeStatusStrip()
{
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_STRIP_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[şerit] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        (void)std::fflush(stdout);
        if (!ok) ++failures;
    };

    if (statusStrip_ == nullptr) {
        check(false, QStringLiteral("durum şeridi var"));
        return failures;
    }

    resize(1600, 1000);
    QCoreApplication::processEvents();

    const auto frameOf = [this](const QString& message) {
        statusStrip_->setMessage(message);
        statusStrip_->update();
        QCoreApplication::processEvents();
        return statusStrip_->grab().toImage();
    };

    // The real line `ÖLÇ` writes, and then one far longer, because a status line
    // is given whatever a command says and a command is not asked to be brief.
    const QString measured = QStringLiteral("Mesafe: 58,941 m   ΔY: 57,000 m   ΔX: 15,000 m   "
                                            "Açı: 83,6183 grad (kuzeyden saat yönünde)");
    const QString absurd =
        measured + QStringLiteral("  ") + measured + QStringLiteral("  ") + measured;

    const QImage quiet = frameOf(QString());
    const QImage some  = frameOf(measured);
    const QImage lots  = frameOf(absurd);

    check(!quiet.isNull() && !some.isNull() && !lots.isNull(),
          QStringLiteral("şerit çiziliyor (%1×%2)").arg(quiet.width()).arg(quiet.height()));
    if (quiet.isNull() || some.isNull() || lots.isNull()) return failures;

    // THE RIGHT-HAND CELLS, PIXEL FOR PIXEL. Whatever the message says, the
    // backend cell, the database cell and the listener cell have to come out the
    // same: a message that changed them is a message drawn over them.
    const int cells = statusStrip_->probeRightCellsWidth();
    (void)std::fprintf(stdout, "[şerit] sağdaki hücreler %d px (şerit %d px)\n", cells,
                       quiet.width());
    check(cells > 0 && cells < quiet.width(),
          QStringLiteral("sağdaki hücrelerin genişliği ölçülebiliyor"));

    const auto rightBand = [cells](const QImage& whole) {
        const int from = whole.width() - qRound(cells * whole.devicePixelRatio());
        return whole.copy(QRect(from, 0, whole.width() - from, whole.height()));
    };

    const QImage a = rightBand(quiet);
    const QImage b = rightBand(some);
    const QImage c = rightBand(lots);
    check(a == b, QStringLiteral("ölçüm satırı sağdaki hücrelere dokunmuyor"));
    check(a == c, QStringLiteral("üç katı uzun satır da dokunmuyor"));

    if (shooting) {
        (void)some.save(into + QStringLiteral("/serit-olcum.png"));
        (void)lots.save(into + QStringLiteral("/serit-uzun.png"));
    }

    statusStrip_->setMessage(QString());
    (void)std::fprintf(stdout, "[şerit] %d kusur\n", failures);
    return failures;
}

int MainWindow::probeRealMouse()
{
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_REALMOUSE_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[fare] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        (void)std::fflush(stdout);
        if (!ok) ++failures;
    };

    resize(1600, 1000);
    QCoreApplication::processEvents();
    runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
    runScriptLine(QStringLiteral("SİL"));
    runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
    QCoreApplication::processEvents();

    // ---- 1. WHAT IS ACTUALLY UNDER THE POINTER -----------------------------
    //
    // A real click is delivered to whatever widget the platform finds at that
    // point. A probe that sends to `canvas_` proves the canvas HANDLES a click
    // and nothing about whether one ever reaches it.
    {
        int wrong = 0;
        for (const QPointF& where :
             {QPointF(0.35, 0.35), QPointF(0.5, 0.5), QPointF(0.65, 0.6), QPointF(0.5, 0.8)}) {
            const QPoint at =
                canvas_->mapToGlobal(QPoint(static_cast<int>(canvas_->width() * where.x()),
                                            static_cast<int>(canvas_->height() * where.y())));
            QWidget* found      = QApplication::widgetAt(at);
            const QString named = found == nullptr
                                      ? QStringLiteral("(hiç)")
                                      : (found->objectName().isEmpty()
                                             ? QString::fromLatin1(found->metaObject()->className())
                                             : found->objectName());
            // NO ANSWER IS NOT A WRONG ANSWER. Under the offscreen platform there
            // is no window manager, a resize is a request rather than a fact, and
            // `widgetAt` returns nothing for a point the platform does not place.
            // A DIFFERENT widget is the defect this looks for — something sitting
            // over the canvas and eating the click; nothing at all says only that
            // there is no window to ask.
            if (found != nullptr && found != canvas_) ++wrong;
            (void)std::fprintf(stdout, "[fare] tuvalin %.0f%%,%.0f%% noktasında: %s%s\n",
                               where.x() * 100, where.y() * 100, named.toUtf8().constData(),
                               found == nullptr ? "  (platform yerleştirmedi, sayılmadı)" : "");
        }
        check(wrong == 0, QStringLiteral("tuvalin üstünde başka bir öğe yok"));
    }

    // ---- 2. A SLOW CLICK ON A FAMILY BUTTON --------------------------------
    //
    // A family is a split button: its FACE runs the member used last, its
    // ARROW opens the list. A human click is not instant, and the tool column's
    // card once opened on a slow press and swallowed the click, so the tool the
    // user reached for never started ("drawing never works, all three of them").
    // The ribbon's face has no hold at all — so every interval a hand produces
    // must run the tool, and none may open a menu in its place.
    const auto clickButton = [](QToolButton* button, int heldMs) {
        // THE FACE: the icon's half of a large split button, above the label
        // and the arrow.
        const QPointF face(button->width() / 2.0, button->height() * 0.3);
        QMouseEvent down(QEvent::MouseButtonPress, face, button->mapToGlobal(face), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(button, &down);

        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < heldMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

        const bool opened = QApplication::activePopupWidget() != nullptr;

        // RELEASED WHERE IT WAS PRESSED, which is what a click is.
        QMouseEvent up(QEvent::MouseButtonRelease, face, button->mapToGlobal(face), Qt::LeftButton,
                       Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(button, &up);
        QCoreApplication::processEvents();
        return opened;
    };

    // The `Giriş` tab's line family: its face runs the tool, its arrow opens the
    // list — and a slow press on the FACE must still run the tool.
    QToolButton* lineButton = ribbonButton(actLine_);

    if (lineButton == nullptr) {
        check(false, QStringLiteral("Çizgi düğmesi bulundu"));
        return failures;
    }

    // A HAND'S CLICK IS NOT INSTANT. Every one of these intervals is a click a
    // person makes, and every one of them has to leave the tool in the hand.
    for (const int heldMs : {40, 150, 260, 320, 500, 700, 1000}) {
        controller_->cancelInteractive();
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        QCoreApplication::processEvents();

        const bool opened            = clickButton(lineButton, heldMs);
        const command::Session* live = controller_->session();
        const bool running           = live != nullptr && live->waiting();
        (void)std::fprintf(stdout, "[fare] %3d ms basılı: menü=%s komut=%s\n", heldMs,
                           opened ? "AÇILDI" : "hayır", running ? "çalıştı" : "HAYIR");
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        QCoreApplication::processEvents();

        // A CLICK IS A CLICK, at any interval a hand produces. Not "something
        // happened": the TOOL has to run. A menu opening in place of the tool is
        // the defect — an open Qt popup grabs the mouse, so the next press on the
        // same button only dismisses it and is swallowed, and a user whose
        // clicks are all slow never reaches the tool at all.
        check(running && !opened, QStringLiteral("%1 ms'lik tıklama aracı çalıştırdı").arg(heldMs));
    }

    // ---- 3. DRAWING, THROUGH THE HIT TEST ----------------------------------
    controller_->cancelInteractive();
    QCoreApplication::processEvents();
    runScriptLine(QStringLiteral("ÇİZGİ"));
    QCoreApplication::processEvents();

    const auto onCanvas = [this](QEvent::Type t, QPointF at, Qt::MouseButton b) {
        const QPoint global = canvas_->mapToGlobal(at.toPoint());
        QWidget* target     = QApplication::widgetAt(global);
        if (target == nullptr) target = canvas_;
        const QPointF local = target->mapFromGlobal(global);
        QMouseEvent ev(t, local, global, b, t == QEvent::MouseButtonRelease ? Qt::NoButton : b,
                       Qt::NoModifier);
        QCoreApplication::sendEvent(target, &ev);
        QCoreApplication::processEvents();
    };

    const QPointF first(canvas_->width() * 0.35, canvas_->height() * 0.4);
    const QPointF second(canvas_->width() * 0.6, canvas_->height() * 0.6);

    onCanvas(QEvent::MouseMove, first, Qt::NoButton);
    onCanvas(QEvent::MouseButtonPress, first, Qt::LeftButton);
    onCanvas(QEvent::MouseButtonRelease, first, Qt::LeftButton);

    const command::Session* after = controller_->session();
    check(after != nullptr && after->waiting() &&
              QString::fromStdString(after->prompt().message).contains(QStringLiteral("Sonraki")),
          QStringLiteral("ilk tıklama geçti, sıradaki nokta soruluyor"));
    check(after != nullptr && after->waiting() && after->prompt().has_rubber_band,
          QStringLiteral("ikinci nokta için kılavuz isteniyor"));

    // THE GUIDE FOLLOWS THE POINTER, and that is a pixel question: the scene can
    // hold a guide the backend never draws.
    onCanvas(QEvent::MouseMove, second, Qt::NoButton);
    const int drawn = static_cast<int>(canvas_->guideVertexCountForProbe());
    (void)std::fprintf(stdout, "[fare] fare izleyen kılavuz: %d köşe\n", drawn);
    // ASSERTED ONLY WHERE THERE IS A CANVAS TO DRAW ON. The scene is built in
    // `paintEvent`, and the offscreen platform gives a `QRhiWidget` no `QRhi`:
    // it paints nothing, so it builds nothing, and a guide count of zero there
    // is the absence of a backend rather than the absence of a guide. Run with a
    // real window — `KENTOS_REALMOUSE_PROBE` and no `QT_QPA_PLATFORM` — and this
    // is the check that matters.
    if (!canvas_->grabCanvas().isNull())
        check(drawn > 0, QStringLiteral("kılavuz fareyi izliyor"));
    else
        (void)std::fprintf(stdout, "[fare] BEKLEMEDE: tuval çizmiyor (QRhi yok); kılavuzun "
                                   "ekranda olduğu ancak gerçek pencerede sınanır\n");

    /// The window plus the live canvas, under the given name, for the pictures
    /// this probe leaves behind.
    const auto shoot = [this, shooting, &into](const char* name) {
        if (!shooting) return;
        (void)probePicture().save(into + QLatin1Char('/') + QLatin1String(name) +
                                  QStringLiteral(".png"));
    };
    shoot("cizgi-kilavuz");

    onCanvas(QEvent::MouseButtonPress, second, Qt::LeftButton);
    onCanvas(QEvent::MouseButtonRelease, second, Qt::LeftButton);
    onCanvas(QEvent::MouseButtonPress, second, Qt::RightButton);
    onCanvas(QEvent::MouseButtonRelease, second, Qt::RightButton);
    QCoreApplication::processEvents();

    check(controller_->document().live_entity_count() > 0,
          QStringLiteral("fareyle çizilen çizgi belgeye girdi (%1 nesne)")
              .arg(controller_->document().live_entity_count()));

    // ---- 4. A METHOD TOOL, PRESSED: it must LIGHT and it must GUIDE ------------
    //
    // The user's report: a newly chosen draw tool does not stay highlighted in
    // the column, and no live guide follows the mouse while drawing with it. A
    // method tool carries a whole line (`YAY yontem=3n`); the
    // column has to light THAT button while it runs — not the plain YAY, and not
    // the select arrow — and the canvas has to draw the guide from the first
    // point exactly as it does for a bare YAY.
    {
        controller_->cancelInteractive();
        QCoreApplication::processEvents();

        QAction* method = nullptr;
        for (QAction* action : drawingTools_->actions())
            if (action->property(kToolCommand).toString() == QLatin1String("YAY yontem=3n"))
                method = action;
        check(method != nullptr, QStringLiteral("YAY yontem=3n aracı kolonda var"));
        if (method != nullptr) {
            method->trigger();
            QCoreApplication::processEvents();

            const command::Session* live = controller_->session();
            check(live != nullptr && live->waiting(),
                  QStringLiteral("yöntem aracı komutu kolladı"));
            QAction* lit = drawingTools_->checkedAction();
            check(lit == method, QStringLiteral("yanan düğme yöntem aracının kendisi (yanan: %1)")
                                     .arg(lit == nullptr ? QStringLiteral("hiç")
                                                         : lit->property(kToolCommand).toString()));

            // FIRST POINT BY MOUSE, then the guide has to follow to the second.
            onCanvas(QEvent::MouseMove, first, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, first, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, first, Qt::LeftButton);
            live = controller_->session();
            check(live != nullptr && live->waiting() && live->prompt().has_rubber_band,
                  QStringLiteral("yöntem aracı ikinci nokta için kılavuz istiyor"));
            // AND IT IS STILL LIT after the click — a click that dropped the
            // highlight is the "selection gets dropped" the user described.
            check(drawingTools_->checkedAction() == method,
                  QStringLiteral("ilk tıklamadan sonra düğme hâlâ yanıyor"));
            onCanvas(QEvent::MouseMove, second, Qt::NoButton);
            if (!canvas_->grabCanvas().isNull())
                check(canvas_->guideVertexCountForProbe() > 0,
                      QStringLiteral("yöntem aracının kılavuzu fareyi izliyor"));
            shoot("yontem-araci-yaniyor");
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
        }
    }

    // ---- 5. ALANÖLÇ FROM THE TOOL COLUMN, BY MOUSE ---------------------------
    //
    // "area measurement does not work", the user said. The tool is a
    // `modifyTool`: press it, click the
    // parcel, right-click. The answer has to land in the transcript.
    {
        runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
        runScriptLine(QStringLiteral("ALAN 0,0 20,0 20,10 0,10"));
        endCommand();
        runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
        QCoreApplication::processEvents();

        QAction* measure = nullptr;
        for (QAction* action : drawingTools_->actions())
            if (action->property(kToolCommand).toString() == QStringLiteral("ALANÖLÇ"))
                measure = action;
        check(measure != nullptr, QStringLiteral("ALANÖLÇ aracı kolonda var"));
        if (measure != nullptr) {
            const QString before = transcript_->toPlainText();
            measure->trigger();
            QCoreApplication::processEvents();
            const command::Session* live = controller_->session();
            check(live != nullptr && live->waiting(), QStringLiteral("ALANÖLÇ nesne bekliyor"));
            check(drawingTools_->checkedAction() == measure,
                  QStringLiteral("ALANÖLÇ düğmesi yanıyor"));

            // CLICK INSIDE THE PARCEL — the way a hand points at an area — then the
            // right button says "those, go". A user does not aim at an edge to
            // mean a face.
            const core::Point2 on_edge{10'000, 5'000};
            const render::ScreenPoint px = canvas_->view().to_screen(on_edge);
            const QPointF edge(px.x, px.y);
            onCanvas(QEvent::MouseMove, edge, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, edge, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, edge, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonPress, edge, Qt::RightButton);
            onCanvas(QEvent::MouseButtonRelease, edge, Qt::RightButton);
            QCoreApplication::processEvents();

            const QString after_text = transcript_->toPlainText().mid(before.size());
            check(after_text.contains(QStringLiteral("alan")),
                  QStringLiteral("ALANÖLÇ fareyle cevap verdi: %1")
                      .arg(after_text.trimmed().right(80)));
            shoot("alanolc-cevap");
        }
    }

    // ---- 6. BLOKEKLE WITH NO BLOCKS ----------------------------------------
    //
    // The user could not tell what Blok Ekle was for and called it pointless.
    // With no block defined, pressing it must SAY what a block is and what to do
    // first — not
    // sit at a blank name prompt.
    {
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
        const QString before = transcript_->toPlainText();
        runScriptLine(QStringLiteral("BLOKEKLE"));
        QCoreApplication::processEvents();
        const command::Session* live = controller_->session();
        const QString said           = transcript_->toPlainText().mid(before.size());
        check(!(live != nullptr && live->waiting()),
              QStringLiteral("blok yokken BLOKEKLE boş bir isim istemiyle beklemiyor"));
        check(said.contains(QStringLiteral("BLOK")),
              QStringLiteral("blok yokken ne yapılacağı söyleniyor: %1").arg(said.trimmed()));
        shoot("blokekle-aciklama");
        controller_->cancelInteractive();
    }

    // ---- 7. ⌫ DELETES ON A MAC, AND DOES NOT WHILE TYPING -------------------
    //
    // The user, on a Mac, could not work out how to delete objects at all. A Mac
    // keyboard has ⌫ and, on most models, no ⌦; SİL was bound to ⌦ alone. The key
    // goes to the CANVAS here, the way a hand's does after clicking a parcel —
    // and then to the command line, where it must edit text and delete nothing.
    {
        // A WINDOW THAT CAN TAKE A SHORTCUT. Started from a terminal the window
        // is not the key window, and a `WindowShortcut` matches only the active
        // one — so it is asked for once and waited for briefly. Where the
        // platform refuses (offscreen), the checks below say BEKLEMEDE.
        if (!isActiveWindow()) {
            raise();
            activateWindow();
            for (int wait = 0; wait < 40 && !isActiveWindow(); ++wait)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
        runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
        runScriptLine(QStringLiteral("ÇOKLUÇİZGİ 0,30 20,30"));
        endCommand();
        runScriptLine(QStringLiteral("SEÇ SON"));
        QCoreApplication::processEvents();
        const std::size_t before = controller_->document().live_entity_count();
        check(controller_->bus().selection().size() == 1, QStringLiteral("silinecek nesne seçili"));

        canvas_->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        // ASSERTED ONLY WHERE A WINDOW CAN BE ACTIVE. A `WindowShortcut` is
        // matched against the ACTIVE window, and the offscreen platform never
        // activates one — so there the key reaches the canvas and no shortcut
        // fires, which is the platform's absence rather than the binding's. Run
        // with a real window and this is the check that matters; the binding
        // itself is asserted below on both.
        if (isActiveWindow()) {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Backspace, Qt::NoModifier);
            QCoreApplication::sendEvent(canvas_, &press);
            QCoreApplication::sendEvent(canvas_, &release);
            QCoreApplication::processEvents();
            check(controller_->document().live_entity_count() == before - 1,
                  QStringLiteral("⌫ tuvalde seçili nesneyi sildi (%1 → %2)")
                      .arg(before)
                      .arg(controller_->document().live_entity_count()));
        } else {
            (void)std::fprintf(stdout, "[fare] BEKLEMEDE: etkin pencere yok (offscreen); ⌫'nin "
                                       "sildiği ancak gerçek pencerede sınanır\n");
        }
        check(actBackspace_->shortcut() == QKeySequence(Qt::Key_Backspace),
              QStringLiteral("⌫ bağlı: çizerken son noktayı geri alır, değilse SİL"));
        check(actErase_->shortcuts().contains(QKeySequence(QKeySequence::Delete)),
              QStringLiteral("SİL, Del tuşuna da bağlı"));

        // AND NOT WHILE TYPING. Focus in the command line, a character and a ⌫:
        // the character goes, the drawing stays.
        runScriptLine(QStringLiteral("SEÇ SON"));
        QCoreApplication::processEvents();
        const std::size_t still = controller_->document().live_entity_count();
        commandLine_->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        // NO FOCUS WIDGET WITHOUT AN ACTIVE WINDOW, which is the offscreen case:
        // sending a key to a null receiver is a crash, not a test. Where there is
        // one, a character and a ⌫ go in and the drawing must be untouched.
        if (QWidget* typing = QApplication::focusWidget(); typing != nullptr) {
            QKeyEvent letter(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
            QCoreApplication::sendEvent(typing, &letter);
            QKeyEvent back(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QCoreApplication::sendEvent(typing, &back);
            QCoreApplication::processEvents();
            check(controller_->document().live_entity_count() == still,
                  QStringLiteral("komut satırında ⌫ yazıyı düzeltir, nesne silmez"));
        } else {
            (void)std::fprintf(stdout, "[fare] BEKLEMEDE: odak alan widget yok (offscreen); komut "
                                       "satırındaki ⌫ ancak gerçek pencerede sınanır\n");
        }
        canvas_->setFocus(Qt::OtherFocusReason);
    }

    // ---- 7b. BETWEEN TWO POINTS ⌫, CTRL+Z AND `G` TAKE THE LAST ONE BACK -----
    //
    // One wrong corner used to cost the whole run: ⌫ wrote it out and started
    // SİL, Ctrl+Z wrote it out and undid all of it (TODOS C-02). The key where a
    // window can be active, the action where it cannot; the typed word on both.
    {
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
        // At the scale a hand draws a 40 m run at: the aperture is centimetres
        // there, and a probe clicking a zoomed-out view would be measuring the
        // snap rather than the key.
        canvas_->zoomToBox(core::Box2{-5'000, 50'000, 45'000, 95'000});
        QCoreApplication::processEvents();
        const std::size_t before     = controller_->document().live_entity_count();
        const std::size_t journalled = controller_->bus().journal().entries().size();
        const auto corners           = [this] {
            const command::Session* live = controller_->session();
            return live != nullptr && live->waiting() ? live->prompt().rubber_chain.size()
                                                                : std::size_t{0};
        };
        const auto press = [this](int key, Qt::KeyboardModifiers mods, QAction* action) {
            if (isActiveWindow()) {
                canvas_->setFocus(Qt::OtherFocusReason);
                QKeyEvent down(QEvent::KeyPress, key, mods);
                QKeyEvent up(QEvent::KeyRelease, key, mods);
                QCoreApplication::sendEvent(canvas_, &down);
                QCoreApplication::sendEvent(canvas_, &up);
            } else {
                action->trigger();
            }
            QCoreApplication::processEvents();
        };
        const core::Point2 wrong{40'000, 90'000};
        const core::Point2 right{40'000, 60'000};
        (void)std::fprintf(stdout, "[fare] geri alma tuşları %s\n",
                           isActiveWindow() ? "gerçek kısayol yolundan (etkin pencere)"
                                            : "eylem üzerinden (etkin pencere yok)");

        controller_->beginInteractive(QStringLiteral("ÇOKLUÇİZGİ"));
        controller_->supplyPoint(core::Point2{0, 60'000});
        controller_->supplyPoint(core::Point2{20'000, 60'000});
        controller_->supplyPoint(wrong);
        QCoreApplication::processEvents();
        check(corners() == 3, QStringLiteral("çalışmada üç köşe var (%1)").arg(corners()));

        press(Qt::Key_Backspace, Qt::NoModifier, actBackspace_);
        check(corners() == 2,
              QStringLiteral("⌫ yalnız son köşeyi geri aldı (%1 köşe kaldı)").arg(corners()));
        check(controller_->document().live_entity_count() == before,
              QStringLiteral("⌫ ne sildi ne yazdı: çalışma sürüyor"));

        controller_->supplyPoint(wrong);
        QCoreApplication::processEvents();
        check(actUndo_->isEnabled(), QStringLiteral("çalışma ortasında Geri Al etkin"));
        press(Qt::Key_Z, Qt::ControlModifier, actUndo_);
        check(corners() == 2,
              QStringLiteral("Ctrl+Z yalnız son köşeyi geri aldı (%1 köşe kaldı)").arg(corners()));

        controller_->supplyPoint(wrong);
        controller_->runLine(QStringLiteral("G"), command::Origin::CommandLine);
        QCoreApplication::processEvents();
        check(
            corners() == 2,
            QStringLiteral("G yazmak yalnız son köşeyi geri aldı (%1 köşe kaldı)").arg(corners()));
        controller_->supplyPoint(wrong);
        controller_->runLine(QStringLiteral("U"), command::Origin::CommandLine);
        QCoreApplication::processEvents();
        check(corners() == 2,
              QStringLiteral("U yazmak GERİAL değil, son köşeyi geri aldı (%1)").arg(corners()));

        // AND ⌫ IN AN EMPTY COMMAND LINE, which is where the focus is after a
        // typed `@10,0`. Not a shortcut: the line's own key handler, so a real
        // key event reaches it with or without an active window.
        controller_->supplyPoint(wrong);
        QCoreApplication::processEvents();
        commandLine_->clear();
        {
            QKeyEvent back(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QCoreApplication::sendEvent(commandLine_, &back);
            QCoreApplication::processEvents();
        }
        check(corners() == 2,
              QStringLiteral("boş komut satırında ⌫ son köşeyi geri aldı (%1 köşe kaldı)")
                  .arg(corners()));
        shoot("son-nokta-geri");

        controller_->supplyPoint(right);
        controller_->finishInteractive();
        QCoreApplication::processEvents();
        check(controller_->document().live_entity_count() == before + 1,
              QStringLiteral("düzeltilen çalışma tek çoklu çizgi olarak yazıldı"));
        const auto& entries = controller_->bus().journal().entries();
        const command::Value* run =
            entries.size() == journalled + 1 ? entries.back().args.find("noktalar") : nullptr;
        check(run != nullptr && run->as_points().size() == 3 && run->as_points().back() == right,
              QStringLiteral("günlük düzeltilmiş çalışmayı yazdı: üç köşe, sonuncusu (40; 60)"));
    }

    // ---- 7c. A TYPED METHOD COMES BACK AS ITSELF ------------------------------
    //
    // `DAİRE yontem=3n` typed at the command line, three rim points, and the
    // tool that re-arms must be the three-point one: it used to be the family's
    // first button, the centre-and-rim circle (TODOS C-02).
    {
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
        controller_->runLine(QStringLiteral("DAİRE yontem=3n"), command::Origin::CommandLine);
        QCoreApplication::processEvents();
        controller_->supplyPoint(core::Point2{0, 70'000});
        controller_->supplyPoint(core::Point2{10'000, 70'000});
        controller_->supplyPoint(core::Point2{5'000, 76'000});
        for (int wait = 0; wait < 20 && controller_->session() == nullptr; ++wait)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        const command::Session* again = controller_->session();
        const QString asked           = again != nullptr && again->waiting()
                                            ? QString::fromStdString(again->prompt().message)
                                            : QString();
        check(asked == QStringLiteral("Çember üzerinde birinci nokta"),
              QStringLiteral("yazılan DAİRE yontem=3n üç noktalı olarak yeniden kuruldu (istem: "
                             "\"%1\")")
                  .arg(asked));
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
    }

    // ---- 8. THE TOOL STAYS IN THE HAND AFTER A DRAW -------------------------
    //
    // The user's report: "after drawing, the default tool gets selected again".
    // Two separate defects were behind it, and both are asserted here.
    {
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
        QCoreApplication::processEvents();

        /// Enter, sent where a hand sends it: to the widget that has the focus.
        /// The command line owns it far more often than the canvas does.
        const auto press_enter = [this] {
            QWidget* target = commandLine_;
            QKeyEvent down(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(target, &down);
            QCoreApplication::processEvents();
        };

        // ENTER FINISHES A POINT RUN and leaves the tool armed. Before this, Enter
        // fell through to nothing: the only key that ended such a run was Esc, and
        // Esc puts the tool away too, so every line cost another trip to the tool
        // column.
        actLine_->trigger();
        QCoreApplication::processEvents();
        const std::size_t before = controller_->document().live_entity_count();
        for (const core::Point2 at : {core::Point2{0, 0}, core::Point2{20'000, 0}}) {
            const render::ScreenPoint px = canvas_->view().to_screen(at);
            const QPointF on(px.x, px.y);
            onCanvas(QEvent::MouseMove, on, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, on, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, on, Qt::LeftButton);
        }
        QCoreApplication::processEvents();
        press_enter();

        check(controller_->document().live_entity_count() > before,
              QStringLiteral("Enter çizgiyi bitirdi (%1 nesne)")
                  .arg(controller_->document().live_entity_count()));
        const command::Session* again = controller_->session();
        check(again != nullptr && again->waiting(),
              QStringLiteral("Enter'dan sonra araç elde kaldı ve yeniden soruyor"));
        check(drawingTools_->checkedAction() == actLine_,
              QStringLiteral("yanan düğme hâlâ ÇİZGİ (yanan: %1)")
                  .arg(drawingTools_->checkedAction() == nullptr
                           ? QStringLiteral("-")
                           : drawingTools_->checkedAction()->property(kToolCommand).toString()));
        controller_->cancelInteractive();
        QCoreApplication::processEvents();

        // AND THE TOOL THAT COMES BACK IS THE ONE THAT RAN, not its family's
        // first member. A method tool carries a whole line, which the repeat
        // resolved as a command NAME, found nothing, and walked on to the plain
        // tool — so picking "Çokgen — dıştan" and drawing one silently left the
        // inscribed one armed for the next.
        QAction* method = nullptr;
        for (QAction* action : drawingTools_->actions())
            if (action->property(kToolCommand).toString() == QStringLiteral("ÇOKGEN yontem=dis"))
                method = action;
        check(method != nullptr, QStringLiteral("ÇOKGEN yontem=dis aracı kolonda var"));
        if (method != nullptr) {
            method->trigger();
            QCoreApplication::processEvents();
            // Answered the way a hand answers it: typed at the command line,
            // which is where the focus goes for a number (`onPromptChanged`).
            controller_->runLine(QStringLiteral("5"), command::Origin::CommandLine);
            QCoreApplication::processEvents();
            controller_->supplyPoint(core::Point2{0, 100'000}); // merkez
            QCoreApplication::processEvents();
            controller_->supplyPoint(core::Point2{0, 110'000}); // kenar, 10 m kuzeyde
            QCoreApplication::processEvents();

            check(
                drawingTools_->checkedAction() == method,
                QStringLiteral("çizimden sonra yanan düğme hâlâ yöntem aracı (yanan: %1)")
                    .arg(drawingTools_->checkedAction() == nullptr
                             ? QStringLiteral("-")
                             : drawingTools_->checkedAction()->property(kToolCommand).toString()));
            const command::Session* repeat = controller_->session();
            check(repeat != nullptr && repeat->waiting(),
                  QStringLiteral("yöntem aracı kendini yeniden kurdu"));
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
        }
    }

    // ---- 7. CORNERS, BY HAND: PAH and YUVARLA ---------------------------------
    //
    // The report: "köşe yuvarla mouse ile çalışmıyor". Each tool is pressed as
    // the column presses it, the corner CLICKED through the hit test and the
    // size SHOWN with a second click — every step the way a hand takes it — on
    // an open line and on a closed parcel, the two shapes a corner is cut on.
    {
        const auto screen = [this](core::Point2 world) {
            const auto at = canvas_->view().to_screen(world);
            return QPointF(at.x, at.y);
        };
        const auto click = [&onCanvas](QPointF at) {
            onCanvas(QEvent::MouseMove, at, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, at, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
        };
        const auto lastSaid = [this] {
            const QStringList lines = transcript_->toPlainText().split(QLatin1Char('\n'));
            return lines.isEmpty() ? QString() : lines.back();
        };
        // THE CLICKS BEFORE THE SIZE: the corner, or — between two objects —
        // the part of each that stays, or for every corner the object itself.
        const auto byHand = [&](QAction* tool, const QStringList& shapes,
                                std::initializer_list<core::Point2> picks, core::Point2 shown,
                                const QString& what) {
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
            runScriptLine(QStringLiteral("SİL"));
            for (const QString& shape : shapes) {
                runScriptLine(shape);
                endCommand();
            }
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.7"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();

            const std::uint64_t revision = controller_->document().revision();
            tool->trigger();
            QCoreApplication::processEvents();
            for (const core::Point2 pick : picks) {
                click(screen(pick));
                QCoreApplication::processEvents();
            }

            const command::Session* live = controller_->session();
            const QString asked          = live != nullptr && live->waiting()
                                               ? QString::fromStdString(live->prompt().message)
                                               : QString();
            check(asked.contains(QStringLiteral("metre")),
                  QStringLiteral("%1: tıklamalardan sonra boyut soruluyor (istem: \"%2\", son "
                                 "söz: \"%3\")")
                      .arg(what, asked, lastSaid()));
            if (asked.isEmpty()) return;

            click(screen(shown));
            QCoreApplication::processEvents();
            check(controller_->document().revision() != revision,
                  QStringLiteral("%1: gösterilen boyutla köşe kesildi (son söz: \"%2\")")
                      .arg(what, lastSaid()));
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
        };

        byHand(actFillet_, {QStringLiteral("ÇOKLUÇİZGİ 0,0 20,0 20,12")}, {{20'000, 0}},
               {17'000, 0}, QStringLiteral("YUVARLA, açık çizgi"));
        byHand(actFillet_, {QStringLiteral("ALAN 0,0 20,0 20,12 0,12")}, {{20'000, 12'000}},
               {17'000, 12'000}, QStringLiteral("YUVARLA, kapalı alan"));
        byHand(actFillet_, {QStringLiteral("DİKDÖRTGEN 0,0 20,12")}, {{20'000, 12'000}},
               {17'000, 12'000}, QStringLiteral("YUVARLA, dikdörtgen"));
        byHand(actChamfer_, {QStringLiteral("ÇOKLUÇİZGİ 0,0 20,0 20,12")}, {{20'000, 0}},
               {17'000, 0}, QStringLiteral("PAH, açık çizgi"));
        byHand(actChamfer_, {QStringLiteral("ALAN 0,0 20,0 20,12 0,12")}, {{20'000, 12'000}},
               {17'000, 12'000}, QStringLiteral("PAH, kapalı alan"));
        // BETWEEN TWO OBJECTS AND ON EVERY CORNER (TODOS C-06): each object
        // clicked on the part that stays, near the corner as a hand clicks.
        const QStringList pair{QStringLiteral("ÇİZGİ 0,0 20,0"),
                               QStringLiteral("ÇİZGİ 20,0 20,12")};
        byHand(actFillet_, pair, {{18'500, 0}, {20'000, 1'500}}, {17'000, 0},
               QStringLiteral("YUVARLA, iki çizgi arasında"));
        byHand(actChamfer_, pair, {{18'500, 0}, {20'000, 1'500}}, {17'000, 0},
               QStringLiteral("PAH, iki çizgi arasında"));
        byHand(actFilletAll_, {QStringLiteral("ÇOKLUÇİZGİ 0,0 20,0 20,12 30,12")}, {{10'000, 0}},
               {18'000, 0}, QStringLiteral("YUVARLA, bütün köşeler"));
        byHand(actChamferAll_, {QStringLiteral("ALAN 0,0 20,0 20,12 0,12")}, {{10'000, 0}},
               {2'000, 0}, QStringLiteral("PAH, bütün köşeler"));
    }

    // ---- 8. A SELECTED SYMBOL IS LIT WHOLE --------------------------------------
    //
    // A block reference draws every member; its selection outline lit only the
    // first, so a selected pole symbol looked half selected (TODOS C-07).
    {
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
        runScriptLine(QStringLiteral("SİL"));
        runScriptLine(QStringLiteral("ÇİZGİ 0,0 6,0"));
        endCommand();
        runScriptLine(QStringLiteral("ÇİZGİ 6,0 6,4"));
        endCommand();
        QStringList members;
        const core::Document& doc = controller_->document();
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kPolylineKind)
                members << QString::number(core::raw(doc.key_of(e)));
        runScriptLine(QStringLiteral("BLOK ad=PROBSEMBOL taban=0,0 nesneler=") +
                      members.join(QLatin1Char(' ')));
        runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
        runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
        canvas_->repaint();
        QCoreApplication::processEvents();
        // ASSERTED ONLY WHERE THERE IS A CANVAS TO DRAW ON, as the guide above:
        // the overlay is built in `paintEvent`, which the offscreen platform
        // never reaches for a `QRhiWidget`.
        if (!canvas_->grabCanvas().isNull())
            check(canvas_->selectionRunCountForProbe() >= 2,
                  QStringLiteral("seçili blok referansının iki parçası da vurgulu (%1 çizgi)")
                      .arg(canvas_->selectionRunCountForProbe()));
        else
            (void)std::fprintf(stdout, "[fare] BEKLEMEDE: tuval çizmiyor (QRhi yok); seçim "
                                       "vurgusu ancak gerçek pencerede sınanır\n");
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
    }

    // ---- 9. GRIPS BY HAND: HOT, SHARED AND LOCKED ------------------------------
    //
    // TODOS C-07. A click on a corner makes it hot — the object follows the
    // pointer and the next click puts it down; a corner two selected parcels
    // share is dragged in both; a locked parcel's handle says why it will not
    // move. Each through the canvas's own press, move and release.
    {
        const auto screen = [this](core::Point2 world) {
            const auto at = canvas_->view().to_screen(world);
            return QPointF(at.x, at.y);
        };
        const auto press = [&onCanvas](QPointF at) {
            onCanvas(QEvent::MouseMove, at, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, at, Qt::LeftButton);
        };
        const auto release = [&onCanvas](QPointF at) {
            onCanvas(QEvent::MouseMove, at, Qt::NoButton);
            onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
        };
        const auto vertex = [this](std::int64_t key, std::size_t at) {
            const core::Document& doc = controller_->document();
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
            return core::Point2{doc.geometry().ring_xs(span.first)[at],
                                doc.geometry().ring_ys(span.first)[at]};
        };
        const auto lastSaid = [this] {
            const QStringList lines = transcript_->toPlainText().split(QLatin1Char('\n'));
            return lines.isEmpty() ? QString() : lines.back();
        };
        const auto fresh = [this](const QStringList& lines) {
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
            runScriptLine(QStringLiteral("SİL"));
            for (const QString& line : lines) {
                runScriptLine(line);
                endCommand();
            }
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.7"));
            runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
            QCoreApplication::processEvents();
        };
        // The first object drawn since the sheet was cleared — not a member of
        // a block an earlier section defined, which SİL leaves in its definition.
        const auto first_key = [this] {
            const core::Document& doc = controller_->document();
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && (doc.entities().flags[e] & core::FlagInBlock) == 0)
                    return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
            return std::int64_t{0};
        };

        // HOT: click the corner, move away, click again.
        fresh({QStringLiteral("ALAN 0,0 20,0 20,12 0,12")});
        const std::int64_t parcel = first_key();
        press(screen({20'000, 12'000}));
        release(screen({20'000, 12'000}));
        const command::Session* hot = controller_->session();
        check(hot != nullptr && hot->waiting() && hot->prompt().param == "nokta",
              QStringLiteral("köşeye tıklamak tutamağı sıcak yaptı: yeni yer soruluyor (son söz: "
                             "\"%1\")")
                  .arg(lastSaid()));
        if (hot != nullptr && hot->waiting()) {
            onCanvas(QEvent::MouseMove, screen({23'000, 15'000}), Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, screen({23'000, 15'000}), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen({23'000, 15'000}), Qt::LeftButton);
            const core::Point2 now = vertex(parcel, 2);
            check(std::abs(now.x - 23'000) <= 300 && std::abs(now.y - 15'000) <= 300,
                  QStringLiteral("ikinci tıklama köşeyi bıraktı (%1, %2)")
                      .arg(static_cast<double>(now.x) / 1000.0)
                      .arg(static_cast<double>(now.y) / 1000.0));
            check(!controller_->selectedSlots().empty(),
                  QStringLiteral("seçim yerinde kaldı; sıradaki tutamak bir tıklama uzakta"));
            const command::Session* left = controller_->session();
            check(left == nullptr || !left->waiting(),
                  QStringLiteral("tutamak düzenlemesi bir araç kurmadı"));
        }

        // SHARED: two parcels side by side, their common corner dragged once.
        fresh({QStringLiteral("ALAN 0,0 10,0 10,10 0,10"),
               QStringLiteral("ALAN 10,0 20,0 20,10 10,10")});
        const std::int64_t left  = first_key();
        const std::int64_t right = left + 1;
        const std::size_t depth  = controller_->undoStack().undo_depth();
        press(screen({10'000, 10'000}));
        release(screen({12'000, 13'000}));
        const core::Point2 a = vertex(left, 2);
        const core::Point2 b = vertex(right, 3);
        check(a == b && std::abs(a.x - 12'000) <= 300 && std::abs(a.y - 13'000) <= 300,
              QStringLiteral("ortak köşe iki parselde birlikte taşındı: (%1, %2) ve (%3, %4)")
                  .arg(static_cast<double>(a.x) / 1000.0)
                  .arg(static_cast<double>(a.y) / 1000.0)
                  .arg(static_cast<double>(b.x) / 1000.0)
                  .arg(static_cast<double>(b.y) / 1000.0));
        check(controller_->undoStack().undo_depth() == depth + 1,
              QStringLiteral("ortak köşe tek geri alma adımı"));

        // LOCKED: the handle says why, and the corner stays.
        fresh({QStringLiteral("KATMAN ad=PROBTAPU"), QStringLiteral("ALAN 0,0 20,0 20,12 0,12"),
               QStringLiteral("KATMAN ad=PROBTAPU kilitli=evet")});
        const std::int64_t titled = first_key();
        press(screen({20'000, 12'000}));
        release(screen({24'000, 16'000}));
        check(vertex(titled, 2) == (core::Point2{20'000, 12'000}),
              QStringLiteral("kilitli parselin köşesi yerinde kaldı"));
        check(lastSaid().contains(QStringLiteral("kilitli")),
              QStringLiteral("kilitli tutamak sebebini söyledi (son söz: \"%1\")").arg(lastSaid()));
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("KATMAN ad=PROBTAPU kilitli=hayır"));
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));

        const auto corners = [this](std::int64_t key) {
            const core::Document& doc = controller_->document();
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            if (e == core::kNoEntity) return std::size_t{0};
            const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
            return doc.geometry().ring_xs(span.first).size();
        };

        // KÖŞESİL FROM THE COLUMN: one click on a corner takes it out.
        fresh({QStringLiteral("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10")});
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        const std::int64_t run = first_key();
        actVertexDelete_->trigger();
        QCoreApplication::processEvents();
        press(screen({10'000, 0}));
        release(screen({10'000, 0}));
        check(corners(run) == 3,
              QStringLiteral("Köşe Sil: tıklanan köşe silindi (%1 köşe kaldı; son söz: \"%2\")")
                  .arg(corners(run))
                  .arg(lastSaid()));
        controller_->cancelInteractive();

        // KENARTÜRÜ FROM THE COLUMN: the edge clicked, the bend shown, clicked.
        fresh({QStringLiteral("ALAN 0,0 20,0 20,10 0,10")});
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        const std::int64_t bent = first_key();
        actEdgeKind_->trigger();
        QCoreApplication::processEvents();
        press(screen({10'000, 0}));
        release(screen({10'000, 0}));
        const command::Session* bending = controller_->session();
        check(bending != nullptr && bending->waiting() &&
                  bending->prompt().rubber_shape == command::RubberShape::EdgeArc,
              QStringLiteral("Kenar Türü: kenara tıklayınca yayın noktası soruluyor, yay imleci "
                             "izliyor (son söz: \"%1\")")
                  .arg(lastSaid()));
        if (bending != nullptr && bending->waiting()) {
            press(screen({10'000, -3'000}));
            release(screen({10'000, -3'000}));
            const core::Document& doc = controller_->document();
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(bent)));
            check(e != core::kNoEntity && doc.entities().kind[e] == core::kArcPolylineKind,
                  QStringLiteral("Kenar Türü: kenar yay oldu, parsel aynı kimlikle yaylı çoklu "
                                 "çizgi (son söz: \"%1\")")
                      .arg(lastSaid()));
        }
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));

        // ---- 10. TRANSFORMS BY HAND (TODOS C-08) ------------------------------
        //
        // DÖNDÜR by reference from the column: the centre, the two ends of the
        // wall, the new direction — four clicks, and the wall now runs north.
        fresh({QStringLiteral("ÇİZGİ 0,0 10,10")});
        const std::int64_t wall = first_key();
        actRotateRef_->trigger();
        QCoreApplication::processEvents();
        for (const core::Point2 p : {core::Point2{0, 0}, core::Point2{0, 0},
                                     core::Point2{10'000, 10'000}, core::Point2{0, 20'000}}) {
            press(screen(p));
            release(screen(p));
        }
        const core::Point2 top = vertex(wall, 1);
        check(std::abs(top.x) <= 300 && std::abs(top.y - 14'142) <= 300,
              QStringLiteral("Döndür — referansla: duvar kuzeye döndü (%1, %2; son söz: \"%3\")")
                  .arg(static_cast<double>(top.x) / 1000.0)
                  .arg(static_cast<double>(top.y) / 1000.0)
                  .arg(lastSaid()));
        controller_->cancelInteractive();

        // DİZİ along a path from the column: the pole selected, the path
        // clicked, the count typed.
        fresh({QStringLiteral("ÇİZGİ 0,0 30,0"), QStringLiteral("ÇİZGİ 0,0 0,2")});
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        const std::int64_t road = first_key();
        runScriptLine(QStringLiteral("SEÇ NESNE nesneler=") + QString::number(road + 1));
        const std::size_t before = controller_->document().live_entity_count();
        actArrayPath_->trigger();
        QCoreApplication::processEvents();
        press(screen({15'000, 0}));
        release(screen({15'000, 0}));
        // Typed, as a hand types a count: the command line hands it over as a
        // number, which the prompt for a count records as a whole one.
        controller_->runLine(QStringLiteral("4"), command::Origin::CommandLine);
        QCoreApplication::processEvents();
        check(controller_->document().live_entity_count() == before + 3,
              QStringLiteral("Dizi — yol boyunca: üç kopya dizildi (%1 nesne; son söz: \"%2\")")
                  .arg(controller_->document().live_entity_count())
                  .arg(lastSaid()));
        controller_->cancelInteractive();
        runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));

        // ---- 11. SINIR BY HAND (TODOS C-09) -----------------------------------
        //
        // Four loose lines and a pool; the tool from the column, the cursor moved
        // inside, the region previewed, clicked — a face with the pool as a hole.
        if (core::network_available()) {
            fresh({QStringLiteral("ÇİZGİ 0,0 20,0"), QStringLiteral("ÇİZGİ 20,0 20,10"),
                   QStringLiteral("ÇİZGİ 20,10 0,10"), QStringLiteral("ÇİZGİ 0,10 0,0"),
                   QStringLiteral("DAİRE 10,5 12,5")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::size_t lines = controller_->document().live_entity_count();
            actBoundary_->trigger();
            QCoreApplication::processEvents();
            const command::Session* asking = controller_->session();
            check(asking != nullptr && asking->waiting() &&
                      asking->prompt().rubber_shape == command::RubberShape::Region,
                  QStringLiteral("Sınır Bul: araç bölgenin içini soruyor, bölge imleçle "
                                 "önizleniyor (son söz: \"%1\")")
                      .arg(lastSaid()));
            onCanvas(QEvent::MouseMove, screen({3'000, 3'000}), Qt::NoButton);
            press(screen({3'000, 3'000}));
            release(screen({3'000, 3'000}));
            const core::Document& doc = controller_->document();
            bool holed                = false;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
                if (!doc.alive(e) || doc.entities().kind[e] != core::kPolylineKind) continue;
                holed = holed || doc.geometry().rings_of(doc.entities().slot[e]).count == 2;
            }
            check(doc.live_entity_count() == lines + 1 && holed,
                  QStringLiteral("Sınır Bul: tıklanan bölge delikli alan oldu (%1 nesne; son "
                                 "söz: \"%2\")")
                      .arg(doc.live_entity_count())
                      .arg(lastSaid()));
            controller_->cancelInteractive();

            // A side stopping 1.5 m short of the corner: nothing is written and
            // the open ends are said, not bridged. (Wider than the snap aperture
            // at this zoom, which is two thirds of a metre: a typed end inside it
            // is pulled onto the corner, as a click would be.)
            fresh({QStringLiteral("ÇİZGİ 0,0 20,0"), QStringLiteral("ÇİZGİ 20,0 20,10"),
                   QStringLiteral("ÇİZGİ 20,10 0,10"), QStringLiteral("ÇİZGİ 0,10 0,1.5")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::size_t open = controller_->document().live_entity_count();
            actBoundary_->trigger();
            QCoreApplication::processEvents();
            press(screen({5'000, 5'000}));
            release(screen({5'000, 5'000}));
            check(controller_->document().live_entity_count() == open &&
                      lastSaid().contains(QStringLiteral("kapanmıyor")),
                  QStringLiteral("Sınır Bul: köşeye 1,5 m varmayan kenar kapanmadı, açık uçlar "
                                 "söylendi (%1 → %2 nesne; son söz: \"%3\")")
                      .arg(open)
                      .arg(controller_->document().live_entity_count())
                      .arg(lastSaid()));
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 12. A DIMENSION BY HAND FOLLOWS ITS CORNER (TODOS C-10) ----------
        //
        // The tool from the column; the two corners of the bottom side clicked,
        // the line's place clicked below it; then the parcel alone selected and
        // its corner dragged by the grip. The dimension is tied to the corners it
        // was clicked on, and measures the side it was drawn on after the drag.
        {
            fresh({QStringLiteral("ALAN 0,0 20,0 20,10 0,10")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::int64_t measured_parcel = first_key();
            actDimension_->trigger();
            QCoreApplication::processEvents();
            for (const core::Point2 at :
                 {core::Point2{0, 0}, core::Point2{20'000, 0}, core::Point2{10'000, -3'000}}) {
                press(screen(at));
                release(screen(at));
            }
            controller_->cancelInteractive();
            const core::Document& doc = controller_->document();
            core::EntityId dim        = core::kNoEntity;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kDimensionKind) dim = e;
            const std::vector<core::DimLink>* links =
                dim == core::kNoEntity ? nullptr : doc.dimension_links().get(dim);
            check(links != nullptr && links->size() == 2,
                  QStringLiteral("Ölçü: fareyle iki köşeye tıklanan ölçü iki köşeye bağlandı (son "
                                 "söz: \"%1\")")
                      .arg(lastSaid()));

            runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1").arg(measured_parcel));
            QCoreApplication::processEvents();
            press(screen({20'000, 0}));
            release(screen({20'000, 0}));
            onCanvas(QEvent::MouseMove, screen({26'000, 0}), Qt::NoButton);
            press(screen({26'000, 0}));
            release(screen({26'000, 0}));
            std::int64_t measured = 0;
            if (dim != core::kNoEntity)
                if (auto def = core::dimension_of(doc.geometry(), doc.entities().slot[dim]); def)
                    measured = def.value().measurement;
            check(
                std::abs(measured - 26'000) <= 300,
                QStringLiteral("Ölçü: köşe tutamağından sürüklenince bağlı ölçü onu izledi (%1 m; "
                               "son söz: \"%2\")")
                    .arg(static_cast<double>(measured) / 1000.0)
                    .arg(lastSaid()));
            shoot("olcu-izler");
            controller_->cancelInteractive();

            // THE ÖLÇÜ GROUP OF THE ATTRIBUTE PANEL, cell by cell: a prefix,
            // then a figure typed by hand — which the model must know is typed.
            if (dim != core::kNoEntity) {
                runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1")
                                  .arg(static_cast<qulonglong>(core::raw(doc.key_of(dim)))));
                showAttributes();
                QCoreApplication::processEvents();
                const bool prefixed =
                    attributePanel_->editRowForProbe(QStringLiteral("onek"), QStringLiteral("≈"));
                const std::string after_prefix(doc.texts().text(doc.entities().slot[dim]));
                check(prefixed && after_prefix.starts_with("≈"),
                      QStringLiteral("Ölçü: nitelik panelinde önek hücresi ölçüye yazıldı (\"%1\")")
                          .arg(QString::fromStdString(after_prefix)));
                const bool typed = attributePanel_->editRowForProbe(QStringLiteral("metin"),
                                                                    QStringLiteral("26,01"));
                auto now         = core::dimension_of(doc.geometry(), doc.entities().slot[dim]);
                check(typed && now && core::dimension_text_is_manual(now.value()) &&
                          attributePanel_->probeRowKeys().contains(QStringLiteral("olculen")),
                      QStringLiteral("Ölçü: panelde elle yazılan değer elle yazılmış sayıldı, "
                                     "ölçülen değer satırı yerinde"));
                showTranscript();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 13. A HATCH BY HAND FOLLOWS ITS PARCEL (TODOS C-11) --------------
        //
        // The hatch tool from the column, the parcel clicked inside and the
        // choice confirmed; then the parcel alone selected and its corner
        // dragged by the grip. The hatch is tied to the parcel and fills the
        // parcel's new shape.
        {
            fresh({QStringLiteral("ALAN 0,0 20,0 20,10 0,10")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::int64_t hatched = first_key();
            actHatch_->trigger();
            QCoreApplication::processEvents();
            press(screen({10'000, 5'000}));
            release(screen({10'000, 5'000}));
            onCanvas(QEvent::MouseButtonPress, screen({10'000, 5'000}), Qt::RightButton);
            onCanvas(QEvent::MouseButtonRelease, screen({10'000, 5'000}), Qt::RightButton);
            controller_->cancelInteractive();
            const core::Document& doc = controller_->document();
            core::EntityId hatch      = core::kNoEntity;
            std::size_t hatches       = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kHatchKind) {
                    hatch = e;
                    ++hatches;
                }
            check(hatches == 1,
                  QStringLiteral("Tarama: bir tıklama ve onay bir tarama çizdi (%1)").arg(hatches));
            const auto* sources = hatch == core::kNoEntity ? nullptr : doc.hatch_links().get(hatch);
            check(sources != nullptr && sources->size() == 1,
                  QStringLiteral("Tarama: fareyle parselin içine tıklanarak çizilen tarama parsele "
                                 "bağlandı (son söz: \"%1\")")
                      .arg(lastSaid()));

            runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1").arg(hatched));
            QCoreApplication::processEvents();
            press(screen({20'000, 10'000}));
            release(screen({20'000, 10'000}));
            // Inside the view the parcel was zoomed to: a click off the canvas is
            // no click at all.
            onCanvas(QEvent::MouseMove, screen({23'000, 11'500}), Qt::NoButton);
            press(screen({23'000, 11'500}));
            release(screen({23'000, 11'500}));
            bool followed = false;
            if (hatch != core::kNoEntity) {
                const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[hatch]);
                for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                    const auto xs = doc.geometry().ring_xs(r);
                    const auto ys = doc.geometry().ring_ys(r);
                    for (std::size_t v = 0; v < xs.size(); ++v)
                        followed = followed || (std::abs(xs[v] - 23'000) <= 300 &&
                                                std::abs(ys[v] - 11'500) <= 300);
                }
            }
            check(followed, QStringLiteral("Tarama: parselin köşesi tutamaktan sürüklenince tarama "
                                           "yeni sınıra oturdu (son söz: \"%1\")")
                                .arg(lastSaid()));
            shoot("tarama-izler");
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));

            // TARAMADÜZENLE BY HAND: the tool from the column, then a click on
            // the parcel's bottom edge — where the hatch lies too. The tool asks
            // for hatches, so the click takes the hatch without asking which;
            // Enter hands it over and the pattern is typed where a hand types.
            actHatchEdit_->trigger();
            QCoreApplication::processEvents();
            press(screen({10'000, 0}));
            release(screen({10'000, 0}));
            const std::vector<core::EntityId>& chosen = controller_->selectedSlots();
            check(hatch != core::kNoEntity && chosen.size() == 1 && chosen.front() == hatch,
                  QStringLiteral("Taramayı Düzenle: parselle taramanın ortak kenarına tıklamak "
                                 "sormadan taramayı seçti (%1 seçili)")
                      .arg(chosen.size()));
            const auto enter = [this](const QString& typed) {
                commandLine_->setText(typed);
                QKeyEvent down(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(commandLine_, &down);
                QCoreApplication::processEvents();
            };
            enter(QString());
            enter(QStringLiteral("ANSI37"));
            const auto pattern = [&doc, hatch] {
                if (hatch == core::kNoEntity) return core::HatchDef{};
                auto def = core::hatch_of(doc.geometry(), doc.entities().slot[hatch]);
                return def ? def.value() : core::HatchDef{};
            };
            check(pattern().name == "ANSI37",
                  QStringLiteral("Taramayı Düzenle: yazılan desen taramaya geçti (\"%1\"; son söz: "
                                 "\"%2\")")
                      .arg(QString::fromStdString(pattern().name), lastSaid()));
            controller_->cancelInteractive();

            // THE TARAMA GROUP OF THE ATTRIBUTE PANEL: the angle, cell by cell.
            if (hatch != core::kNoEntity) {
                runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1")
                                  .arg(static_cast<qulonglong>(core::raw(doc.key_of(hatch)))));
                showAttributes();
                QCoreApplication::processEvents();
                const bool turned =
                    attributePanel_->editRowForProbe(QStringLiteral("aci"), QStringLiteral("15"));
                check(turned && pattern().angle_udeg == 15'000'000 &&
                          doc.hatch_links().get(hatch) != nullptr,
                      QStringLiteral("Tarama: nitelik panelinde açı hücresi taramaya yazıldı, bağ "
                                     "sürüyor (%1°)")
                          .arg(static_cast<double>(pattern().angle_udeg) / 1e6));
                shoot("tarama-paneli");
                showTranscript();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 14. A TWO-LINE CAPTION BY HAND, LAID OUT FROM THE PANEL (C-12) ---
        //
        // The text tool from the column, its point clicked, two lines typed at
        // the prompt with `\n`; then the caption selected and its alignment,
        // spacing and words changed cell by cell — the words with a quote in
        // them, which used to end the command line the cell builds.
        {
            fresh({QStringLiteral("ÇİZGİ -10,-10 30,-10")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            actText_->trigger();
            QCoreApplication::processEvents();
            press(screen({0, 0}));
            release(screen({0, 0}));
            commandLine_->setText(QStringLiteral("ADA 101\\nPARSEL 4"));
            QKeyEvent typed(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(commandLine_, &typed);
            QCoreApplication::processEvents();
            controller_->cancelInteractive();
            const core::Document& doc = controller_->document();
            core::EntityId caption    = core::kNoEntity;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.texts().has(doc.entities().slot[e])) caption = e;
            const auto words = [&doc, caption] {
                return caption == core::kNoEntity
                           ? std::string()
                           : std::string(doc.texts().text(doc.entities().slot[caption]));
            };
            check(words() == "ADA 101\nPARSEL 4",
                  QStringLiteral("Metin: araçla tıklanıp yazılan iki satır iki satır oldu (\"%1\")")
                      .arg(QString::fromStdString(words())));

            if (caption != core::kNoEntity) {
                runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1")
                                  .arg(static_cast<qulonglong>(core::raw(doc.key_of(caption)))));
                showAttributes();
                QCoreApplication::processEvents();
                const bool centred = attributePanel_->editRowForProbe(QStringLiteral("hizalama"),
                                                                      QStringLiteral("merkez"));
                const bool spaced  = attributePanel_->editRowForProbe(
                    QStringLiteral("satir_araligi"), QStringLiteral("1.5"));
                const std::uint32_t slot = doc.entities().slot[caption];
                check(
                    centred && spaced &&
                        doc.texts().anchor(slot) == core::TextAnchor::MiddleCentre &&
                        doc.texts().lines(slot).spacing == 1500,
                    QStringLiteral("Metin: panelde hizalama ve satır aralığı hücreleri yazıya "
                                   "yazıldı (%1, %2)")
                        .arg(QString::fromLatin1(core::text_anchor_name(doc.texts().anchor(slot))))
                        .arg(doc.texts().lines(slot).spacing));
                const bool rewritten = attributePanel_->editRowForProbe(
                    QStringLiteral("icerik"), QStringLiteral("\"KÖY\" YOLU\\nADA 101"));
                check(rewritten && words() == "\"KÖY\" YOLU\nADA 101",
                      QStringLiteral("Metin: panelde tırnaklı ve iki satırlı yazı olduğu gibi "
                                     "yazıldı (\"%1\")")
                          .arg(QString::fromStdString(words())));
                shoot("yazi-paneli");
                showTranscript();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 15. A LABEL SAYS THE NEW AREA WHEN THE CORNER IS DRAGGED (C-12) --
        //
        // A parcel labelled with its number and its measured area; then the
        // parcel alone selected and its corner dragged by the grip. The label
        // follows the parcel's middle and writes the area the drag made.
        {
            fresh({QStringLiteral("SÜTUN kimlik=ada tur=tam_sayi"),
                   QStringLiteral("ALAN 0,0 20,0 20,10 0,10")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::int64_t labelled = first_key();
            runScriptLine(QStringLiteral("ÖZNİTELİK ada %1 101").arg(labelled));
            const core::Document& doc = controller_->document();
            const core::EntityId parcel_row =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(labelled)));
            const core::Layer* on = parcel_row == core::kNoEntity
                                        ? nullptr
                                        : doc.layer(doc.entities().layer[parcel_row]);
            runScriptLine(
                QStringLiteral("ETİKET katman=\"%1\" bicim=\"{ada}\\n{#alan} m²\" hedef=PROBETIKET")
                    .arg(on == nullptr ? QString() : QString::fromStdString(on->name)));
            endCommand();
            const auto label_text = [&doc] {
                const core::LayerId l = doc.find_layer("PROBETIKET");
                for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                    if (doc.alive(e) && doc.entities().layer[e] == l &&
                        doc.texts().has(doc.entities().slot[e]))
                        return std::string(doc.texts().text(doc.entities().slot[e]));
                return std::string();
            };
            check(label_text() == "101\n200,00 m²",
                  QStringLiteral("Etiket: numara ve ölçülen alan yazıldı (\"%1\")")
                      .arg(QString::fromStdString(label_text())));

            runScriptLine(QStringLiteral("SEÇ NESNE nesneler=%1").arg(labelled));
            QCoreApplication::processEvents();
            press(screen({20'000, 10'000}));
            release(screen({20'000, 10'000}));
            // Inside the view the parcel was zoomed to (see section 13).
            onCanvas(QEvent::MouseMove, screen({23'000, 11'500}), Qt::NoButton);
            press(screen({23'000, 11'500}));
            release(screen({23'000, 11'500}));
            // (0,0) (20,0) (23,11.5) (0,10): 230 m² by the shoelace, give or
            // take the centimetres a pixel is. The label must say the area the
            // parcel now HAS, not a figure this probe expected.
            const core::EntityId dragged =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(labelled)));
            const core::Mm2 now_area = dragged == core::kNoEntity ? 0 : doc.entity_area(dragged);
            const bool near_230      = now_area > 229'000'000 && now_area < 231'000'000;
            check(near_230 && label_text() == "101\n" + core::format_area(now_area, 2, ',') + " m²",
                  QStringLiteral("Etiket: köşe tutamaktan sürüklenince alan yeniden yazıldı "
                                 "(\"%1\"; son söz: \"%2\")")
                      .arg(QString::fromStdString(label_text()), lastSaid()));
            shoot("etiket-izler");
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 16. FIND AND REPLACE THROUGH ITS WINDOW (C-12) --------------------
        //
        // The window, driven by its buttons: find-all selects what it finds;
        // preview fills the table and changes nothing; a field changed after
        // the preview greys the replace-all button out and its click writes
        // nothing; pressed after a fresh preview it changes every caption in
        // one undo step.
        {
            fresh({QStringLiteral("METİN noktalar=0,0 yazi=\"ADA 101\""),
                   QStringLiteral("METİN noktalar=0,6 yazi=\"ADA 102 / ADA 103\"")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const core::Document& doc = controller_->document();
            const auto words          = [&doc] {
                std::vector<std::string> out;
                for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                    if (doc.alive(e) && doc.texts().has(doc.entities().slot[e]))
                        out.emplace_back(doc.texts().text(doc.entities().slot[e]));
                return out;
            };
            using Step = FindReplaceDialog::Step;
            // THE WINDOW'S OWN KEY, through the real shortcut road when this
            // window is active — the combination read off the action, so what
            // is proved is what this platform registered.
            const QKeySequence key = actFindReplace_->shortcut();
            const bool keyed       = isActiveWindow() && !key.isEmpty();
            if (keyed) {
                canvas_->setFocus(Qt::OtherFocusReason);
                QKeyEvent down(QEvent::KeyPress, key[0].key(), key[0].keyboardModifiers());
                QKeyEvent up(QEvent::KeyRelease, key[0].key(), key[0].keyboardModifiers());
                QCoreApplication::sendEvent(canvas_, &down);
                QCoreApplication::sendEvent(canvas_, &up);
            } else {
                actFindReplace_->trigger();
            }
            QCoreApplication::processEvents();
            check(findReplace_ != nullptr && findReplace_->isVisible(),
                  QStringLiteral("Bul ve Değiştir: %1 penceresi açtı (%2)")
                      .arg(key.toString(QKeySequence::NativeText),
                           keyed ? QStringLiteral("gerçek kısayol yolu")
                                 : QStringLiteral("eylem üzerinden; etkin pencere yok")));
#ifdef Q_OS_MACOS
            check(key != QKeySequence(Qt::CTRL | Qt::Key_H),
                  QStringLiteral("Bul ve Değiştir: kısayol macOS'un Gizle tuşu Cmd+H değil"));
#endif
            if (findReplace_ != nullptr) {
                const std::vector<std::string> captions_before = words();
                findReplace_->runForProbe(QStringLiteral("ADA"), QString(), Step::Find);
                QCoreApplication::processEvents();
                check(controller_->bus().selection().size() == 2 &&
                          findReplace_->previewRows() == 2 && !findReplace_->applyEnabled(),
                      QStringLiteral("Bul ve Değiştir: Tümünü Bul iki yazıyı çizimde seçti (%1 "
                                     "seçili), yazma yolu kapalı")
                          .arg(controller_->bus().selection().size()));

                findReplace_->runForProbe(QStringLiteral("ADA"), QStringLiteral("Ada"),
                                          Step::Preview);
                QCoreApplication::processEvents();
                check(findReplace_->previewRows() == 2 && words() == captions_before &&
                          findReplace_->applyEnabled(),
                      QStringLiteral("Bul ve Değiştir: önizleme iki yazıyı listeledi, hiçbir şey "
                                     "değişmedi; Tümünü Değiştir açıldı (%1 satır)")
                          .arg(findReplace_->previewRows()));
                if (shooting)
                    (void)findReplace_->grab().save(into + QStringLiteral("/bul-degistir.png"));

                // The replacement typed anew after the preview: the table no
                // longer says what would be written, so nothing is.
                const std::size_t undo_before = controller_->undoStack().undo_depth();
                findReplace_->runForProbe(QStringLiteral("ADA"), QStringLiteral("Parsel"),
                                          Step::Apply);
                QCoreApplication::processEvents();
                check(words() == captions_before && !findReplace_->applyEnabled() &&
                          controller_->undoStack().undo_depth() == undo_before,
                      QStringLiteral("Bul ve Değiştir: önizlemeden sonra değişen alan Tümünü "
                                     "Değiştir'i kapattı, hiçbir şey yazılmadı"));

                findReplace_->runForProbe(QStringLiteral("ADA"), QStringLiteral("Ada"),
                                          Step::Preview);
                findReplace_->runForProbe(QStringLiteral("ADA"), QStringLiteral("Ada"),
                                          Step::Apply);
                QCoreApplication::processEvents();
                const std::vector<std::string> captions_after = words();
                check(captions_after.size() == 2 && captions_after[0] == "Ada 101" &&
                          captions_after[1] == "Ada 102 / Ada 103" &&
                          controller_->undoStack().undo_depth() == undo_before + 1,
                      QStringLiteral("Bul ve Değiştir: Tümünü Değiştir iki yazıyı bir geri alma "
                                     "adımında değiştirdi"));
                if (shooting)
                    (void)findReplace_->grab().save(into +
                                                    QStringLiteral("/bul-degistir-uygulandi.png"));
                findReplace_->hide();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 17. EVERY DIMENSION TYPE FROM ITS OWN TOOL (TODOS C-17) ---------------
        //
        // Each of the seven from its row of the ribbon's Ölçü family, drawn with
        // the mouse and nothing typed: a radius, a diameter and an arc length take
        // the curve they are clicked on, an angle asks for its vertex first. While
        // the last point is aimed the canvas writes the figure the click will
        // write, where it will write it — it used to write the distance from the
        // first point, which is the one number a dimension does not say.
        {
            struct Case
            {
                const char* name;                 ///< the pictures' name
                const char* line;                 ///< the tool's command line
                QStringList scene;                ///< drawn first
                std::vector<core::Point2> clicks; ///< the points before the last
                core::Point2 last;                ///< where the figure goes
                core::DimensionType type;         ///< what must be drawn
                std::int64_t measured;            ///< and what it must measure
            };

            using T = core::DimensionType;
            const std::vector<Case> cases{
                {"olcu-hizali",
                 "ÖLÇÜ",
                 {QStringLiteral("ALAN 0,0 20,0 20,10 0,10")},
                 {{0, 0}, {20'000, 0}},
                 {10'000, -3'000},
                 T::Aligned,
                 20'000},
                {"olcu-dogrusal",
                 "ÖLÇÜ tur=dogrusal",
                 {QStringLiteral("ÇİZGİ 0,0 16,12")},
                 {{0, 0}, {16'000, 12'000}},
                 {8'000, 15'000},
                 T::Linear,
                 16'000},
                {"olcu-aci",
                 "ÖLÇÜ tur=acisal",
                 {QStringLiteral("ÇİZGİ 0,0 20,0"), QStringLiteral("ÇİZGİ 0,0 0,15")},
                 {{0, 0}, {20'000, 0}, {0, 15'000}},
                 {6'000, 6'000},
                 T::Angular3P,
                 90'000'000},
                {"olcu-yay",
                 "ÖLÇÜ tur=yay",
                 {QStringLiteral("YAY merkez=10,0 baslangic=20,0 bitis=0,0")},
                 {{10'000, 10'000}},
                 {10'000, 14'000},
                 T::ArcLength,
                 31'416},
                {"olcu-yaricap",
                 "ÖLÇÜ tur=yaricap",
                 {QStringLiteral("DAİRE merkez=10,6 cevre=16,6")},
                 {{16'000, 6'000}},
                 {19'000, 12'000},
                 T::Radial,
                 6'000},
                {"olcu-cap",
                 "ÖLÇÜ tur=cap",
                 {QStringLiteral("DAİRE merkez=10,6 cevre=16,6")},
                 {{10'000, 12'000}},
                 {17'000, 14'000},
                 T::Diametric,
                 12'000},
                {"olcu-koordinat",
                 "ÖLÇÜ tur=koordinat",
                 {QStringLiteral("ALAN 0,0 20,0 20,10 0,10")},
                 {{0, 0}, {20'000, 10'000}},
                 {26'000, 10'000},
                 T::Ordinate,
                 20'000},
            };
            // NO TEXT BOX LEFT FLOATING over the drawing by the captions above: it
            // closed on Enter and Esc only, stayed where a caption was given up
            // another way, and swallowed the click that placed a dimension there.
            const auto* box = canvas_->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
            check(box == nullptr || !box->isVisible(),
                  QStringLiteral("Tuval: yazı istemi bittiğinde yazı kutusu kapandı"));

            const bool drawing = !canvas_->grabCanvas().isNull();
            for (const Case& c : cases) {
                fresh(c.scene);
                // ROOM FOR THE FIGURE: the caption goes beyond what is measured,
                // and a click past the canvas's edge lands on the ribbon.
                runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.6"));
                runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
                const QString line = QString::fromUtf8(c.line);
                QAction* tool      = line == QStringLiteral("ÖLÇÜ")
                                         ? actDimension_
                                         : findChild<QAction*>(QStringLiteral("toolAction.") + line);
                check(tool != nullptr && ribbonButton(tool, false) != nullptr,
                      QStringLiteral("Ölçü: %1 şeritteki Ölçü ailesinde").arg(line));
                if (tool == nullptr) continue;
                tool->trigger();
                QCoreApplication::processEvents();
                for (const core::Point2 at : c.clicks) {
                    press(screen(at));
                    release(screen(at));
                }
                // THE LAST POINT AIMED, the pointer over it and not yet pressed.
                onCanvas(QEvent::MouseMove, screen(c.last), Qt::NoButton);
                (void)canvas_->grabCanvas();
                QCoreApplication::processEvents();
                const QString ghost = QString::fromStdString(canvas_->guideLabelForProbe());
                shoot((std::string(c.name) + "-hayalet").c_str());
                press(screen(c.last));
                release(screen(c.last));
                const core::Document& doc = controller_->document();
                core::EntityId dim        = core::kNoEntity;
                for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                    if (doc.alive(e) && doc.entities().kind[e] == core::kDimensionKind) dim = e;
                core::DimensionDef def;
                QString caption;
                if (dim != core::kNoEntity) {
                    const std::uint32_t slot = doc.entities().slot[dim];
                    if (auto d = core::dimension_of(doc.geometry(), slot); d) def = d.value();
                    caption = QString::fromUtf8(doc.texts().text(slot));
                }
                check(
                    dim != core::kNoEntity && def.type == c.type && def.measurement == c.measured,
                    QStringLiteral("Ölçü: %1 fareyle çizildi, türü %2, ölçtüğü %3 (yazısı \"%4\"; "
                                   "son söz: \"%5\")")
                        .arg(line, QString::fromLatin1(core::dimension_type_name(def.type)))
                        .arg(def.measurement)
                        .arg(caption, lastSaid()));
                if (drawing)
                    check(!ghost.isEmpty() && ghost == caption,
                          QStringLiteral("Ölçü: %1 nişanlanırken hayalet yazacağı değeri yazdı "
                                         "(\"%2\", yazısı \"%3\")")
                              .arg(line, ghost, caption));
                shoot(c.name);
                controller_->cancelInteractive();
            }
            if (!drawing)
                (void)std::fprintf(stdout, "[fare] BEKLEMEDE: tuval çizmiyor (QRhi yok); ölçü "
                                           "hayaletinin yazdığı değer gerçek pencerede sınanır\n");

            // THE SHEET (TODOS C-17, 2nd stage): every type together and two that
            // do not fit, on screen and printed from the same scene — the
            // figures above their lines, the closed heads filled, R and Ø, the
            // arc length's own arc, the angle in grad; the short line's figure
            // and heads outside, and the narrow angle's past its upper arm, on
            // that arm's tangent, with room below the parcel to stand in.
            if (shooting) {
                // Four metres out, not three: the figure stands between its line
                // and the parcel, and ISO-25 at 1/1000 needs 3,125 m for it.
                fresh({QStringLiteral("ALAN 0,0 20,0 20,10 0,10"),
                       QStringLiteral("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-4"),
                       QStringLiteral("ÖLÇÜ tur=dogrusal birinci=20,0 ikinci=20,10 konum=24,5"),
                       QStringLiteral("ÇİZGİ 0,14 1,14"),
                       QStringLiteral("ÖLÇÜ birinci=0,14 ikinci=1,14 konum=0.5,16"),
                       QStringLiteral("DAİRE merkez=34,5 cevre=38,5"),
                       QStringLiteral("ÖLÇÜ tur=yaricap nokta=38,5 konum=42,10"),
                       QStringLiteral("ÖLÇÜ tur=cap nokta=30,5 konum=26,-2"),
                       QStringLiteral("YAY merkez=34,-14 baslangic=40,-14 bitis=28,-14"),
                       QStringLiteral("ÖLÇÜ tur=yay nokta=34,-8 konum=34,-5"),
                       QStringLiteral("ÇİZGİ 0,-24 12,-24"), QStringLiteral("ÇİZGİ 0,-24 8,-18"),
                       QStringLiteral("ÖLÇÜ tur=acisal tepe=0,-24 birinci=12,-24 ikinci=8,-18 "
                                      "konum=7,-21")});
                runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
                runScriptLine(QStringLiteral("YAZDIR merkez=20,1 olcek=500 dosya=\"%1\"")
                                  .arg(into + QStringLiteral("/olcu-paftasi.pdf")));
                endCommand();
                QCoreApplication::processEvents();
                shoot("olcu-paftasi");
                check(QFileInfo::exists(into + QStringLiteral("/olcu-paftasi.pdf")),
                      QStringLiteral("Ölçü paftası PDF'e yazıldı"));
                // AND THE SAME SHEET AS A DXF (TODOS C-17, 3rd stage): every
                // dimension with its picture block, for a reader that draws
                // DIMENSION from it to be compared against the screen and the PDF.
                runScriptLine(QStringLiteral("DIŞAAKTAR dosya=\"%1\"")
                                  .arg(into + QStringLiteral("/olcu-paftasi.dxf")));
                endCommand();
                check(QFileInfo::exists(into + QStringLiteral("/olcu-paftasi.dxf")),
                      QStringLiteral("Ölçü paftası DXF'e yazıldı"));
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 18. A DOUBLE CLICK OPENS WHAT EDITS IT (TODOS C-17, 3rd stage) ----
        //
        // With nothing asking for a click, a double click on a dimension opens
        // its figure in the box over it, on a caption its words, and on anything
        // else the attribute panel — each through the real hit test, press,
        // release, double click, release, as a hand sends them.
        {
            fresh({QStringLiteral("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-4"),
                   QStringLiteral("METİN 0,12 \"Ada 12\" 2500"),
                   QStringLiteral("ÇİZGİ 0,24 20,24")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            const auto twice = [&onCanvas](QPointF at) {
                onCanvas(QEvent::MouseMove, at, Qt::NoButton);
                onCanvas(QEvent::MouseButtonPress, at, Qt::LeftButton);
                onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
                onCanvas(QEvent::MouseButtonDblClick, at, Qt::LeftButton);
                onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
            };
            const auto asked = [this] {
                const command::Session* live = controller_->session();
                return live != nullptr && live->waiting()
                           ? QString::fromStdString(live->prompt().message)
                           : QString();
            };
            const auto caption = [this](std::int64_t key) {
                const core::Document& doc = controller_->document();
                const core::EntityId e =
                    doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                return e == core::kNoEntity ? QString()
                                            : QString::fromUtf8(std::string(
                                                  doc.texts().text(doc.entities().slot[e])));
            };
            const std::int64_t dim = first_key();

            // THE DIMENSION: on its line, between the heads.
            twice(screen(core::Point2{10'000, -4'000}));
            check(asked().contains(QStringLiteral("Ölçünün yazısı")),
                  QStringLiteral("ölçüye çift tıklama ÖLÇÜDÜZENLE'nin yazı sorusunu açtı (soru: "
                                 "\"%1\")")
                      .arg(asked()));
            auto* box = canvas_->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
            check(box != nullptr && box->isVisible() && box->text() == QStringLiteral("<>") &&
                      box->selectedText() == QStringLiteral("<>"),
                  QStringLiteral("yazı kutusu ölçünün üstünde açıldı, içinde seçili <> (kutu: "
                                 "\"%1\")")
                      .arg(box != nullptr ? box->text() : QStringLiteral("-")));
            check(box != nullptr && window()->focusWidget() == box,
                  QStringLiteral("klavye kutuda, komut satırında değil"));
            shoot("olcu-cift-tiklama");
            if (box != nullptr && box->isVisible()) {
                box->setText(QStringLiteral("<> (tapu)"));
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(box, &enter);
                QCoreApplication::processEvents();
            }
            check(caption(dim) == QStringLiteral("20,00 (tapu)"),
                  QStringLiteral("kutuya yazılan kalıp ölçünün yazısı oldu (yazı: \"%1\")")
                      .arg(caption(dim)));
            shoot("olcu-cift-tiklama-sonuc");

            // THE CAPTION: its words in the box, and Esc gives the edit up.
            twice(screen(core::Point2{3'000, 12'500}));
            box = canvas_->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
            check(asked().contains(QStringLiteral("Yeni metin")) && box != nullptr &&
                      box->isVisible() && box->text() == QStringLiteral("Ada 12"),
                  QStringLiteral("yazıya çift tıklama YAZIDÜZENLE'yi yazının kendisiyle açtı "
                                 "(soru: \"%1\", kutu: \"%2\")")
                      .arg(asked(), box != nullptr ? box->text() : QStringLiteral("-")));
            // ESC IS THE BOX'S OWN SHORTCUT, which a key sent straight to the box
            // never reaches: Qt matches shortcuts on the window's key path, before
            // delivery. So the shortcut is taken the way that path takes it.
            if (box != nullptr && box->isVisible()) {
                for (QShortcut* esc : box->findChildren<QShortcut*>())
                    if (esc->key() == QKeySequence(Qt::Key_Escape)) emit esc->activated();
                QCoreApplication::processEvents();
            }
            check(asked().isEmpty() && (box == nullptr || !box->isVisible()) &&
                      caption(first_key() + 1) == QStringLiteral("Ada 12"),
                  QStringLiteral("Esc düzenlemeyi bıraktı: soru da kutu da kapandı, yazı aynı"));

            // ANYTHING ELSE: the attribute panel, on the object.
            propertyHeader_->setCurrent(1);
            twice(screen(core::Point2{10'000, 24'000}));
            check(asked().isEmpty() && propertyDock_->isVisible() &&
                      propertyHeader_->current() == 0 &&
                      controller_->bus().selection().keys().size() == 1,
                  QStringLiteral("çizgiye çift tıklama onu seçti ve öznitelik panelini öne "
                                 "getirdi"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 19. A LETTER THE TYPEFACE LACKS IS SHOWN, NOT BORROWED (TODOS C-12) ----
        //
        // It prints as the face's own box on the screen and on the sheet, the
        // canvas says which letter it is, and the PDF carries no font but the
        // bundled one — Qt, left alone, takes the letter from whatever other
        // font this machine has, and the sheet then differs from the screen.
        {
            fresh({QStringLiteral("METİN 0,0 \"Ada 漢 12\" 2500"),
                   QStringLiteral("METİN 0,-6 \"Şişli İlçesi ğüç\" 2500")});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            canvas_->repaint();
            QCoreApplication::processEvents();
            if (!canvas_->grabCanvas().isNull()) {
                int noted  = 0;
                bool named = false;
                for (const std::string& note : canvas_->noteTextsForProbe()) {
                    const QString said = QString::fromStdString(note);
                    if (!said.startsWith(QStringLiteral("yazı tipinde yok"))) continue;
                    ++noted;
                    named = named || said == QStringLiteral("yazı tipinde yok: 漢 (U+6F22)");
                }
                check(noted == 1 && named,
                      QStringLiteral("tuval eksik harfi adıyla söylüyor, Türkçe yazı için bir şey "
                                     "demiyor (not sayısı %1)")
                          .arg(noted));
            } else {
                (void)std::fprintf(stdout, "[fare] BEKLEMEDE: tuval çizmiyor (QRhi yok); eksik "
                                           "harf notu gerçek pencerede sınanır\n");
            }
            shoot("yazi-eksik-harf");

            const QTemporaryDir scratch;
            const QString sheet =
                (shooting ? into : scratch.path()) + QStringLiteral("/yazi-eksik-harf.pdf");
            runScriptLine(QStringLiteral("YAZDIR merkez=10,-3 olcek=500 dosya=\"%1\"").arg(sheet));
            endCommand();
            QFile pdf(sheet);
            QStringList faces;
            QByteArray bytes;
            if (pdf.open(QIODevice::ReadOnly)) {
                bytes = pdf.readAll();
                for (qsizetype at = bytes.indexOf("/BaseFont"); at >= 0;
                     at           = bytes.indexOf("/BaseFont", at + 1)) {
                    const qsizetype slash = bytes.indexOf('/', at + 9);
                    qsizetype end         = slash + 1;
                    while (end < bytes.size() && bytes[end] > ' ' && bytes[end] != '/' &&
                           bytes[end] != '>')
                        ++end;
                    faces << QString::fromLatin1(bytes.mid(slash + 1, end - slash - 1));
                }
            }
            (void)std::fprintf(stdout, "[fare] PDF yazı tipleri: %s\n",
                               faces.join(QStringLiteral(", ")).toUtf8().constData());
            const bool only_plex =
                !faces.isEmpty() && std::ranges::all_of(faces, [](const QString& f) {
                    return f.contains(QStringLiteral("IBMPlex"));
                });
            check(only_plex, QStringLiteral("PDF yalnız IBM Plex taşıyor; eksik harf başka yazı "
                                            "tipinden alınmadı"));
            // AND SAYS SO, in its metadata and in the line that reports it.
            check(bytes.contains("<stFnt:fontFamily>IBM Plex Sans</stFnt:fontFamily>") &&
                      bytes.contains("SIL Open Font License 1.1"),
                  QStringLiteral("PDF üst verisi yazı tipini ve lisansını söylüyor"));
            check(transcript_->toPlainText().contains(
                      QStringLiteral("yazı tipi IBM Plex Sans gömülü (SIL Open Font License 1.1)")),
                  QStringLiteral("YAZDIR iletisi gömülü yazı tipini ve lisansını söylüyor"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 20. A LEADER'S WORDS, TYPED WHERE THEY GO AND TIED TO IT (TODOS C-12) ----
        //
        // Drawn from the ribbon with clicks and ended with Enter: the words are
        // asked in the box beside the landing, and once written they follow the
        // landing and change side when the line comes in from the other way.
        {
            fresh({QStringLiteral("DAİRE merkez=0,0 cevre=2,0")});
            // FRAMED, not zoomed by a factor: the leader's points, its turned end
            // and the room its words take to the right of the landing are all on
            // the canvas whatever its shape. A click off it still reaches the
            // canvas, but the box is kept inside it and would not stand where the
            // words go — which a fixed factor let happen on a narrower canvas.
            canvas_->zoomToBox(core::Box2{-6'000, -8'000, 40'000, 14'000});
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            const std::int64_t circle = first_key();
            const auto click          = [&onCanvas](QPointF at) {
                onCanvas(QEvent::MouseMove, at, Qt::NoButton);
                onCanvas(QEvent::MouseButtonPress, at, Qt::LeftButton);
                onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
            };
            actLeader_->trigger();
            QCoreApplication::processEvents();
            for (const core::Point2 at : {core::Point2{1'414, 1'414}, core::Point2{8'000, 6'000},
                                          core::Point2{14'000, 6'000}})
                click(screen(at));
            {
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(commandLine_, &enter);
                QCoreApplication::processEvents();
            }
            const core::Document& doc = controller_->document();
            const core::EntityId leader =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(circle + 1)));
            core::Point2 landing{};
            if (leader != core::kNoEntity && doc.alive(leader)) {
                const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[leader]);
                const auto xs           = doc.geometry().ring_xs(rs.first);
                const auto ys           = doc.geometry().ring_ys(rs.first);
                landing                 = core::Point2{xs.back(), ys.back()};
            }
            auto* box = canvas_->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
            const QPointF where = screen(core::Point2{landing.x + 625, landing.y});
            check(box != nullptr && box->isVisible() &&
                      std::abs(box->geometry().left() - where.x()) <= 3.0 &&
                      window()->focusWidget() == box,
                  QStringLiteral("kılavuzun yazısı ucunun yanındaki kutuda soruluyor, klavye "
                                 "orada (kutu x=%1, uç x=%2)")
                      .arg(box != nullptr ? box->geometry().left() : -1)
                      .arg(where.x(), 0, 'f', 0));
            shoot("kilavuz-yazi-kutusu");
            if (box != nullptr && box->isVisible()) {
                box->setText(QStringLiteral("Rögar K-12"));
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(box, &enter);
                QCoreApplication::processEvents();
            }
            controller_->cancelInteractive(); // the tool re-armed for the next leader
            QCoreApplication::processEvents();
            const core::EntityId words =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(circle + 2)));
            const core::Attachment* tie =
                words != core::kNoEntity ? doc.attachments().get(words) : nullptr;
            check(tie != nullptr && tie->anchor == core::AttachAnchor::Landing &&
                      core::raw(tie->source) == static_cast<std::uint64_t>(circle + 1) &&
                      doc.texts().text(doc.entities().slot[words]) == "Rögar K-12",
                  QStringLiteral("yazı kılavuzun ucuna bağlı"));
            runScriptLine(QStringLiteral("KÖŞETAŞI nesne=%1 kose=3 nokta=4,10").arg(circle + 1));
            endCommand();
            bool turned = false;
            if (words != core::kNoEntity && doc.alive(words)) {
                const std::uint32_t slot = doc.entities().slot[words];
                const core::RingSpan rs  = doc.geometry().rings_of(slot);
                turned = doc.texts().anchor(slot) == core::TextAnchor::MiddleRight &&
                         doc.geometry().ring_xs(rs.first)[0] == 4'000 - 625 &&
                         doc.geometry().ring_ys(rs.first)[0] == 10'000;
            }
            check(turned, QStringLiteral("kılavuzun ucu sola dönünce yazı ucu izledi, sola geçti "
                                         "ve sağa yaslandı"));
            shoot("kilavuz-yazi-izler");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 21. A DXF'S MULTILEADER ARRIVES AS A LEADER WITH TIED WORDS (TODOS C-12) ----
        {
            fresh({});
            const QString seed = QString::fromStdString(data_root()) +
                                 QStringLiteral("/../tests/fuzz/tohum/dxf/26-multileader.dxf");
            if (QFileInfo::exists(seed)) {
                runScriptLine(QStringLiteral("İÇEAKTAR dosya=\"%1\"").arg(seed));
                // The reading is on a worker thread; the drawing lands when it is done.
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 10000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
                runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.6"));
                const core::Document& doc = controller_->document();
                std::size_t leaders       = 0;
                std::size_t tied          = 0;
                for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
                    if (!doc.alive(e)) continue;
                    if (doc.entities().kind[e] == core::kLeaderKind) ++leaders;
                    if (const core::Attachment* tie = doc.attachments().get(e);
                        tie != nullptr && tie->anchor == core::AttachAnchor::Landing)
                        ++tied;
                }
                check(leaders == 1 && tied == 1,
                      QStringLiteral("DXF'in MULTILEADER'ı bir kılavuz çizgi ve ona bağlı yazı "
                                     "olarak geldi (kılavuz %1, bağlı yazı %2)")
                          .arg(leaders)
                          .arg(tied));
                shoot("multileader-ice-aktarim");
            } else {
                (void)std::fprintf(stdout, "[fare] BEKLEMEDE: MULTILEADER tohumu bulunamadı\n");
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 22. A BLOCK TAKEN APART LOOKS AS IT DID (TODOS C-13) ----
        //
        // A symbol with a line, a circle, an arc, a caption and a face, inserted
        // turned, doubled and mirrored. The pointer finds a member's corner on
        // the reference; PATLAT takes it apart from the ribbon; the picture is
        // the same after as before, and the same corner is under the pointer.
        {
            fresh({QStringLiteral("ÇOKLUÇİZGİ 0,0 3,0 3,1"),
                   QStringLiteral("DAİRE merkez=1,3 cevre=2,3"),
                   QStringLiteral("YAY merkez=5,4 baslangic=6,4 bitis=5,5"),
                   QStringLiteral("METİN noktalar=0,6 yazi=K-12 yukseklik=600"),
                   QStringLiteral("ALAN 4,0 6,0 6,2 4,2")});
            const std::int64_t opening = first_key();
            QString members;
            for (std::int64_t k = opening; k < opening + 5; ++k)
                members += QStringLiteral(" nesneler=%1").arg(k);
            runScriptLine(QStringLiteral("BLOK ad=ROGAR taban=0,0") + members);
            endCommand();
            runScriptLine(
                QStringLiteral("BLOKEKLE ad=ROGAR nokta=16,-4 olcek=-2 olcek_y=2 aci=30"));
            endCommand();
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.8"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();

            const core::Document& doc = controller_->document();
            std::int64_t reference    = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind &&
                    (doc.entities().flags[e] & core::FlagInBlock) == 0)
                    reference = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
            const core::EntityId ref_slot =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(reference)));
            core::Point2 corner{};
            if (ref_slot != core::kNoEntity) {
                const std::uint32_t gs = doc.entities().slot[ref_slot];
                const auto placed      = core::block_reference_of(doc.geometry(), gs);
                if (placed)
                    corner = core::place_block_point(
                        placed.value(), core::block_reference_insertion(doc.geometry(), gs),
                        doc.blocks().at(placed.value().block).base, core::Point2{3'000, 1'000}, 0,
                        0);
            }
            // BOTH PICTURES WITH THE POINTER IN ONE EMPTY PLACE: the crosshair,
            // the snap marker and the coordinate readout follow the pointer,
            // and they are not the drawing being compared.
            const QPointF resting(canvas_->width() * 0.15, canvas_->height() * 0.2);
            onCanvas(QEvent::MouseMove, resting, Qt::NoButton);
            const QImage whole = canvas_->grabCanvas();
            shoot("blok-once");

            // THE POINTER ON A MEMBER'S CORNER, a few pixels off it, while a
            // point is asked: the marker sits on the corner as drawn.
            const auto marker_at = [this, &onCanvas, &screen](core::Point2 at) {
                runScriptLine(QStringLiteral("ÇİZGİ"));
                QCoreApplication::processEvents();
                const QPointF near = screen(at) + QPointF(4.0, -3.0);
                onCanvas(QEvent::MouseMove, near, Qt::NoButton);
                const core::SnapResult* shown = canvas_->snapPreviewForProbe();
                const core::SnapResult got    = shown != nullptr ? *shown : core::SnapResult{};
                return got;
            };
            const core::SnapResult on_reference = marker_at(corner);
            check(on_reference.mode == core::SnapEndpoint && on_reference.point == corner &&
                      on_reference.entity == ref_slot,
                  QStringLiteral("dönük, ölçekli ve aynalı bloğun üye köşesi UÇ olarak yakalandı "
                                 "(%1, %2)")
                      .arg(static_cast<double>(on_reference.point.x) / 1000.0, 0, 'f', 3)
                      .arg(static_cast<double>(on_reference.point.y) / 1000.0, 0, 'f', 3));
            shoot("blok-uc-yakalama");
            controller_->cancelInteractive();
            QCoreApplication::processEvents();

            // TAKEN APART FROM THE RIBBON, the reference selected first.
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(reference));
            QCoreApplication::processEvents();
            actExplode_->trigger();
            QCoreApplication::processEvents();
            endCommand();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            std::size_t circles = 0;
            std::size_t arcs    = 0;
            std::size_t words   = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
                if (!doc.alive(e) || (doc.entities().flags[e] & core::FlagInBlock) != 0) continue;
                if (doc.entities().kind[e] == core::kCircleKind) ++circles;
                if (doc.entities().kind[e] == core::kArcKind) ++arcs;
                if (doc.texts().has(doc.entities().slot[e]) &&
                    doc.texts().height(doc.entities().slot[e]) == 1'200)
                    ++words;
            }
            check(!doc.alive(ref_slot) && circles == 1 && arcs == 1 && words == 1,
                  QStringLiteral("PATLAT bloğu kendi türünde parçalara ayırdı (daire %1, yay %2, "
                                 "iki kat yazı %3)")
                      .arg(circles)
                      .arg(arcs)
                      .arg(words));
            onCanvas(QEvent::MouseMove, resting, Qt::NoButton);
            const QImage apart = canvas_->grabCanvas();
            shoot("blok-sonra");
            if (!whole.isNull() && whole.size() == apart.size()) {
                // THE SAME PICTURE: pixels that changed by more than a trace of
                // anti-aliasing, as a share of the canvas.
                qint64 changed = 0;
                for (int y = 0; y < whole.height(); ++y)
                    for (int x = 0; x < whole.width(); ++x) {
                        const QRgb was = whole.pixel(x, y);
                        const QRgb now = apart.pixel(x, y);
                        if (std::abs(qRed(was) - qRed(now)) + std::abs(qGreen(was) - qGreen(now)) +
                                std::abs(qBlue(was) - qBlue(now)) >
                            96)
                            ++changed;
                    }
                const double share = static_cast<double>(changed) /
                                     static_cast<double>(whole.width() * whole.height());
                check(share < 0.002, QStringLiteral("patlatmadan önceki ve sonraki resim aynı "
                                                    "(değişen piksel %1, %2‰)")
                                         .arg(changed)
                                         .arg(share * 1000.0, 0, 'f', 2));
            } else {
                (void)std::fprintf(stdout,
                                   "[fare] BEKLEMEDE: tuval çizmiyor; resim karşılaştırması "
                                   "gerçek pencerede\n");
            }
            const core::SnapResult on_piece = marker_at(corner);
            check(on_piece.mode == core::SnapEndpoint && on_piece.point == corner &&
                      on_piece.entity != ref_slot,
                  QStringLiteral("patlatıldıktan sonra aynı köşe, bu kez parçada yakalandı"));
            shoot("blok-sonra-uc-yakalama");
            controller_->cancelInteractive();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 23. A BLOCK EDITED WHERE IT STANDS CHANGES EVERY COPY (TODOS C-13) ----
        //
        // A double click on a block opens its definition in place; a corner of
        // it is dragged by hand; the ribbon's save puts the change into every
        // reference — the one edited, a doubled one and a turned one — and an
        // undo of the save takes the user back into the edit.
        {
            fresh({QStringLiteral("ÇİZGİ 0,0 3,0"),
                   QStringLiteral("DAİRE merkez=1.5,1.5 cevre=2.5,1.5")});
            const std::int64_t opening = first_key();
            runScriptLine(QStringLiteral("BLOK ad=DIREK taban=0,0 nesneler=%1 nesneler=%2")
                              .arg(opening)
                              .arg(opening + 1));
            endCommand();
            const core::Document& doc   = controller_->document();
            const auto newest_reference = [&doc] {
                std::int64_t found = 0;
                for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                    if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind &&
                        (doc.entities().flags[e] & core::FlagInBlock) == 0)
                        found = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
                return found;
            };
            const std::int64_t home = newest_reference();
            runScriptLine(QStringLiteral("BLOKEKLE ad=DIREK nokta=12,-2 olcek=2"));
            endCommand();
            const std::int64_t doubled = newest_reference();
            runScriptLine(QStringLiteral("BLOKEKLE ad=DIREK nokta=26,-2 aci=45"));
            endCommand();
            const std::int64_t turned = newest_reference();
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.8"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            shoot("blok-duzenle-once");

            const auto slot_of = [&doc](std::int64_t key) {
                return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            };
            const auto hidden = [&doc, &slot_of](std::int64_t key) {
                const core::EntityId e = slot_of(key);
                return e != core::kNoEntity && (doc.entities().flags[e] & core::FlagHidden) != 0;
            };
            // THE DOUBLE CLICK, on the reference's drawn line, as a hand sends it.
            SARibbonBar* bar = ribbonBar();
            if (bar != nullptr) bar->setCurrentIndex(0);
            QCoreApplication::processEvents();
            const int tab_before  = bar != nullptr ? bar->currentIndex() : -1;
            const QPointF on_line = screen(core::Point2{1'500, 0});
            onCanvas(QEvent::MouseMove, on_line, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, on_line, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, on_line, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonDblClick, on_line, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, on_line, Qt::LeftButton);
            endCommand();
            const auto tab_up = [this, bar] {
                return bar != nullptr && blockEditTab_ != nullptr &&
                       bar->isContextCategoryVisible(blockEditTab_);
            };
            check(blockEdit_.has_value() && blockEdit_->reference == home && hidden(home) &&
                      blockEdit_->opened.size() == 2 && tab_up() &&
                      !controller_->bus().selection().contains(
                          static_cast<core::EntityKey>(static_cast<std::uint64_t>(home))),
                  QStringLiteral("çift tıklama bloğu yerinde açtı: referans gizli ve seçimden "
                                 "çıktı, iki nesne sayfada, 'Blok: DIREK' sekmesi açık"));
            shoot("blok-duzenle-acik");

            // THE LINE'S FAR END, dragged by hand: a click makes the corner hot,
            // the next click puts it down (TODOS C-07's grips).
            std::int64_t line = 0;
            if (blockEdit_)
                for (const std::int64_t k : blockEdit_->opened)
                    if (const core::EntityId e = slot_of(k);
                        e != core::kNoEntity && doc.entities().kind[e] == core::kPolylineKind)
                        line = k;
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(line));
            QCoreApplication::processEvents();
            const QPointF end_now  = screen(core::Point2{3'000, 0});
            const QPointF end_then = screen(core::Point2{3'000, 2'500});
            onCanvas(QEvent::MouseMove, end_now, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, end_now, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, end_now, Qt::LeftButton);
            onCanvas(QEvent::MouseMove, end_then, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, end_then, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, end_then, Qt::LeftButton);
            endCommand();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            // Where the corner landed, from the document: a hand is a few pixels off.
            core::Point2 moved{};
            if (const core::EntityId e = slot_of(line); e != core::kNoEntity && doc.alive(e)) {
                const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
                moved                   = core::Point2{doc.geometry().ring_xs(rs.first).back(),
                                     doc.geometry().ring_ys(rs.first).back()};
            }
            check(std::abs(moved.y - 2'500) <= 300,
                  QStringLiteral("açılan çizginin ucu tutamaktan sürüklendi (%1, %2)")
                      .arg(static_cast<double>(moved.x) / 1000.0, 0, 'f', 3)
                      .arg(static_cast<double>(moved.y) / 1000.0, 0, 'f', 3));
            shoot("blok-duzenle-degisti");

            // THE RIBBON'S SAVE: every reference draws the moved end.
            actBlockSave_->trigger();
            QCoreApplication::processEvents();
            endCommand();
            const auto drawn_end = [&doc, &slot_of](std::int64_t key) {
                core::EmitBuffer runs;
                const core::EntityId e = slot_of(key);
                if (e == core::kNoEntity || !core::entity_outline(doc, e, runs))
                    return core::Point2{};
                for (std::size_t r = 0; r < runs.run_total(); ++r)
                    if (runs.run_count[r] == 2)
                        return core::Point2{runs.run_xs(r)[1], runs.run_ys(r)[1]};
                return core::Point2{};
            };
            const auto placed = [&doc, &slot_of](std::int64_t key, core::Point2 p) {
                const core::EntityId e = slot_of(key);
                const std::uint32_t gs = doc.entities().slot[e];
                const auto ref         = core::block_reference_of(doc.geometry(), gs);
                if (!ref) return core::Point2{};
                return core::place_block_point(ref.value(),
                                               core::block_reference_insertion(doc.geometry(), gs),
                                               doc.blocks().at(ref.value().block).base, p, 0, 0);
            };
            const bool everywhere = drawn_end(home) == placed(home, moved) &&
                                    drawn_end(doubled) == placed(doubled, moved) &&
                                    drawn_end(turned) == placed(turned, moved);
            check(!blockEdit_.has_value() && !hidden(home) && everywhere && !tab_up() &&
                      bar != nullptr && bar->currentIndex() == tab_before,
                  QStringLiteral("Bloğu Kaydet değişikliği üç referansın üçüne de işledi; "
                                 "sekme kapandı, şerit önceki sekmesine döndü, referans göründü"));
            shoot("blok-duzenle-kaydedildi");

            // AN UNDO OF THE SAVE takes the user back into the edit.
            runScriptLine(QStringLiteral("GERİAL"));
            QCoreApplication::processEvents();
            check(blockEdit_.has_value() && hidden(home) && tab_up(),
                  QStringLiteral("kaydetmeyi geri almak düzenlemeye geri götürdü"));
            actBlockCancel_->trigger();
            QCoreApplication::processEvents();
            endCommand();
            check(!blockEdit_.has_value() && !hidden(home) &&
                      drawn_end(home) == core::Point2{3'000, 0},
                  QStringLiteral("Vazgeç açılanı kaldırdı; tanım eski hâlinde"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 24. A NUMBERED SYMBOL: ITS VALUE ASKED WHERE IT STANDS, ITS BASE MOVED (C-13) ----
        //
        // A block whose caption is the field `{no}`: placed from the ribbon, its
        // value is asked in the box where the number will stand and drawn so;
        // its base point, moved from the ribbon, leaves the symbol where it is.
        {
            fresh({QStringLiteral("METİN noktalar=0,1 yazi={no} yukseklik=600"),
                   QStringLiteral("DAİRE merkez=0,0 cevre=0.4,0")});
            const std::int64_t opening = first_key();
            runScriptLine(QStringLiteral("BLOK ad=NOKTA taban=0,0 nesneler=%1 nesneler=%2")
                              .arg(opening)
                              .arg(opening + 1));
            endCommand();
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.5"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            const core::Document& doc = controller_->document();

            // BLOK EKLE from the ribbon: the name, the click, then the value.
            actInsert_->trigger();
            QCoreApplication::processEvents();
            controller_->supplyText(QStringLiteral("NOKTA"));
            QCoreApplication::processEvents();
            // In the left half of the canvas, so the box that opens to the
            // right of the number is not nudged back off it by the edge.
            const core::Point2 aimed = canvas_->view().to_world(
                render::ScreenPoint{canvas_->width() * 0.3, canvas_->height() * 0.6});
            const core::Point2 where{(aimed.x / 1'000) * 1'000, (aimed.y / 1'000) * 1'000};
            onCanvas(QEvent::MouseMove, screen(where), Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, screen(where), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(where), Qt::LeftButton);
            QCoreApplication::processEvents();
            auto* box = canvas_->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
            // The number stands at (0,1) inside the block: 1 m above where the
            // click landed — read from the document, since a hand's click is a
            // few centimetres off the point aimed at.
            core::Point2 landed = where;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind &&
                    (doc.entities().flags[e] & core::FlagInBlock) == 0)
                    landed =
                        core::block_reference_insertion(doc.geometry(), doc.entities().slot[e]);
            // Where the caption's baseline begins inside the block, placed.
            core::Point2 caption_start{0, 1'000};
            if (const core::BlockId symbol = doc.blocks().find("NOKTA"); symbol != core::kNoBlock)
                for (const core::EntityKey k : doc.blocks().at(symbol).members) {
                    const core::EntityId m = doc.slot_of(k);
                    if (m == core::kNoEntity || !doc.texts().has(doc.entities().slot[m])) continue;
                    const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[m]);
                    caption_start           = core::Point2{doc.geometry().ring_xs(rs.first)[0],
                                                 doc.geometry().ring_ys(rs.first)[0]};
                }
            const QPointF number_at =
                screen(core::Point2{landed.x + caption_start.x, landed.y + caption_start.y});
            const command::Session* live = controller_->session();
            const bool asked_there =
                live != nullptr && live->waiting() &&
                QString::fromStdString(live->prompt().message).contains(QStringLiteral("'no'")) &&
                box != nullptr && box->isVisible() &&
                std::abs(box->geometry().left() - number_at.x()) <= 3.0 &&
                std::abs(box->geometry().center().y() - number_at.y()) <= 3.0;
            check(asked_there, QStringLiteral("alanın değeri, sayının duracağı yerde açılan kutuda "
                                              "soruluyor (kutu %1,%2; yazı %3,%4)")
                                   .arg(box != nullptr ? box->geometry().left() : -1)
                                   .arg(box != nullptr ? box->geometry().center().y() : -1)
                                   .arg(number_at.x(), 0, 'f', 0)
                                   .arg(number_at.y(), 0, 'f', 0));
            shoot("oznitelik-deger-kutusu");
            if (box != nullptr && box->isVisible()) {
                box->setText(QStringLiteral("K-7"));
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(box, &enter);
                QCoreApplication::processEvents();
            }
            controller_->cancelInteractive(); // the tool re-armed for the next one
            QCoreApplication::processEvents();
            std::int64_t placed = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind &&
                    (doc.entities().flags[e] & core::FlagInBlock) == 0)
                    placed = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
            const core::EntityId placed_slot =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(placed)));
            const core::AttrId no = doc.attributes().find("no");
            const bool valued     = placed_slot != core::kNoEntity && no != core::kNoAttr &&
                                doc.attribute(no, placed_slot).value().text == "K-7";
            check(valued, QStringLiteral("yazılan değer referansın 'no' hücresinde: K-7"));
            shoot("oznitelik-cizildi");

            // THE BASE, moved from the ribbon to the circle's east point: the
            // symbol stays where it is drawn.
            core::EmitBuffer before_runs;
            (void)core::entity_outline(doc, placed_slot, before_runs);
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(placed));
            QCoreApplication::processEvents();
            actBlockBase_->trigger();
            QCoreApplication::processEvents();
            const core::Point2 east{landed.x + 400, landed.y};
            onCanvas(QEvent::MouseMove, screen(east), Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, screen(east), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(east), Qt::LeftButton);
            endCommand();
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
            core::EmitBuffer after_runs;
            (void)core::entity_outline(doc, placed_slot, after_runs);
            const core::BlockId nokta = doc.blocks().find("NOKTA");
            const core::Point2 base =
                nokta != core::kNoBlock ? doc.blocks().at(nokta).base : core::Point2{};
            const bool kept = before_runs.xs == after_runs.xs && before_runs.ys == after_runs.ys;
            check(std::abs(base.x - 400) <= 60 && std::abs(base.y) <= 60 && kept,
                  QStringLiteral("Taban Noktası tabanı dairenin doğusuna taşıdı (%1, %2); "
                                 "sembol çizildiği yerde kaldı")
                      .arg(static_cast<double>(base.x) / 1000.0, 0, 'f', 3)
                      .arg(static_cast<double>(base.y) / 1000.0, 0, 'f', 3));
            shoot("blok-taban-tasindi");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 25. A SYMBOL FROM A LIBRARY FILE (TODOS C-13) ----
        //
        // A file holding two symbols is written through the clipboard's own
        // payload; in a fresh drawing BLOKEKLE dosya= asks which of the two,
        // offering both, and the one picked is placed where the canvas is clicked.
        {
            fresh({QStringLiteral("DAİRE merkez=0,0 cevre=1,0"), QStringLiteral("ÇİZGİ -1,0 1,0"),
                   QStringLiteral("ÇİZGİ 5,0 5,3")});
            const std::int64_t opening = first_key();
            runScriptLine(QStringLiteral("BLOK ad=KUYU taban=0,0 nesneler=%1 nesneler=%2")
                              .arg(opening)
                              .arg(opening + 1));
            endCommand();
            runScriptLine(QStringLiteral("BLOK ad=AGAC taban=5,0 nesneler=%1").arg(opening + 2));
            endCommand();
            const core::Document& doc = controller_->document();
            QString references;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind &&
                    (doc.entities().flags[e] & core::FlagInBlock) == 0)
                    references += QStringLiteral(" nesneler=%1").arg(core::raw(doc.key_of(e)));
            const QString library =
                (shooting ? into : QDir::tempPath()) + QStringLiteral("/kitaplik-probe.pcad");
            runScriptLine(QStringLiteral("PANOYAKOPYALA") + references +
                          QStringLiteral(" dosya=\"%1\"").arg(library));
            endCommand();
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            QCoreApplication::processEvents();

            runScriptLine(QStringLiteral("BLOKEKLE dosya=\"%1\"").arg(library));
            QCoreApplication::processEvents();
            const command::Session* asked = controller_->session();
            QStringList offered;
            if (asked != nullptr && asked->waiting())
                for (const std::string& c : asked->prompt().choices)
                    offered << QString::fromStdString(c);
            check(offered.contains(QStringLiteral("KUYU")) &&
                      offered.contains(QStringLiteral("AGAC")),
                  QStringLiteral("kitaplıktaki iki blok soruda seçenek olarak sunuldu (%1)")
                      .arg(offered.join(QStringLiteral(", "))));
            shoot("kitaplik-hangi-blok");
            controller_->supplyText(QStringLiteral("KUYU"));
            QCoreApplication::processEvents();
            const QPointF there(canvas_->width() * 0.5, canvas_->height() * 0.5);
            onCanvas(QEvent::MouseMove, there, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, there, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, there, Qt::LeftButton);
            endCommand();
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
            const core::BlockId kuyu = doc.blocks().find("KUYU");
            std::size_t members      = 0;
            if (kuyu != core::kNoBlock)
                for (const core::EntityKey k : doc.blocks().at(kuyu).members)
                    if (const core::EntityId m = doc.slot_of(k);
                        m != core::kNoEntity && doc.alive(m))
                        ++members;
            std::size_t runs_drawn = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
                if (!doc.alive(e) || doc.entities().kind[e] != core::kBlockReferenceKind) continue;
                core::EmitBuffer runs;
                if (core::entity_outline(doc, e, runs)) runs_drawn += runs.run_total();
            }
            check(kuyu != core::kNoBlock && doc.blocks().find("AGAC") == core::kNoBlock &&
                      members == 2 && runs_drawn == 2,
                  QStringLiteral("seçilen KUYU kitaplıktan bütün geldi ve tıklanan yere kondu "
                                 "(üye %1, çizilen %2); AGAC gelmedi")
                      .arg(members)
                      .arg(runs_drawn));
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.3"));
            QCoreApplication::processEvents();
            shoot("kitaplik-yerlesti");
        }

        // ---- 26. A DRAWING KEPT IN ITS OWN FILE (TODOS C-14) ----
        //
        // A base map is written by a program of its own; the drawing it serves
        // is saved beside it and attaches it by name. It is drawn where it was
        // drawn and snapped to; a double click refuses to open it for editing;
        // the base map changed elsewhere comes in from the ribbon's reload; with
        // its file gone the drawing still opens and says which file is missing.
        {
            const QString folder =
                (shooting ? into : QDir::tempPath()) + QStringLiteral("/disreferans-probe");
            QDir(folder).removeRecursively();
            QDir().mkpath(folder);
            const QString base_map    = folder + QStringLiteral("/altlik.pcad");
            const QString project     = folder + QStringLiteral("/proje.pcad");
            const auto write_base_map = [&base_map](bool second_road) {
                core::Document side_doc;
                command::Registry side_reg;
                command::Journal side_journal;
                command::UndoStack side_undo;
                command::Bus side{side_doc, side_reg, side_journal, side_undo};
                command::register_builtin_commands(side_reg);
                io::FileService side_files{side};
                for (const std::string& line :
                     {std::string("KATMAN ad=PARSEL"), std::string("ALAN 0,0 30,0 30,20 0,20"),
                      std::string("ALAN 30,0 55,0 55,20 30,20"), std::string("KATMAN ad=YOL"),
                      std::string("ÇİZGİ -5,-4 60,-4")})
                    (void)side.execute_line(line, command::Origin::Test);
                if (second_road)
                    (void)side.execute_line("ÇİZGİ -5,24 60,24", command::Origin::Test);
                (void)side.execute_line("FARKLIKAYDET \"" + base_map.toStdString() + "\"",
                                        command::Origin::Test);
            };
            write_base_map(false);

            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("ÇİZGİ 10,5 20,15"));
            endCommand();
            runScriptLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(project));
            endCommand();
            check(actXref_ != nullptr && actXrefReload_ != nullptr && actXref_->isEnabled() &&
                      actXrefReload_->isEnabled(),
                  QStringLiteral("şeritte Dış Referans ve Dış Referansları Yenile var"));

            // BY NAME, beside the project: the path the file will hold.
            runScriptLine(QStringLiteral("DIŞREFERANS dosya=altlik.pcad"));
            endCommand();
            const core::Document& doc = controller_->document();
            const auto live_members   = [&doc] {
                const core::BlockId b = doc.blocks().find("altlik");
                std::size_t n         = 0;
                if (b != core::kNoBlock)
                    for (const core::EntityKey k : doc.blocks().at(b).members)
                        if (const core::EntityId m = doc.slot_of(k);
                            m != core::kNoEntity && doc.alive(m))
                            ++n;
                return n;
            };
            core::EntityId xref_reference = core::kNoEntity;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.entities().standalone(e) &&
                    doc.entities().kind[e] == core::kBlockReferenceKind)
                    xref_reference = e;
            check(live_members() == 3 && xref_reference != core::kNoEntity,
                  QStringLiteral("altlık bağlandı: iki parsel ve yol, bir referansla (%1 üye)")
                      .arg(live_members()));
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.8"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            shoot("disreferans-baglandi");

            // THE REFERENCE'S LAYERS, grouped under its name: the group row shows
            // them seen, and its eye — pressed by hand — puts both out in one
            // step and brings them back on undo.
            const auto layer_seen = [&doc](const char* name) {
                const core::LayerId l = doc.find_layer(name);
                return l != core::kNoLayer && doc.layer(l) != nullptr && doc.layer(l)->visible;
            };
            const std::optional<bool> group_seen =
                layerPanel_ != nullptr ? layerPanel_->probeGroupEye(QStringLiteral("altlik"), false)
                                       : std::nullopt;
            const std::optional<bool> after_click =
                layerPanel_ != nullptr ? layerPanel_->probeGroupEye(QStringLiteral("altlik"), true)
                                       : std::nullopt;
            check(group_seen.value_or(false) && after_click.has_value() && !*after_click &&
                      !layer_seen("altlik|PARSEL") && !layer_seen("altlik|YOL"),
                  QStringLiteral("katman panelinde 'altlik' grubu görünür; gözüne tıklamak iki "
                                 "katmanını birden gizledi"));
            runScriptLine(QStringLiteral("GERİAL"));
            endCommand();
            QCoreApplication::processEvents();
            check(layer_seen("altlik|PARSEL") && layer_seen("altlik|YOL") &&
                      layerPanel_->probeGroupEye(QStringLiteral("altlik"), false).value_or(false),
                  QStringLiteral("tek geri alma grubun iki katmanını birden geri getirdi"));

            // THE POINTER ON A PARCEL CORNER, a few pixels off: the marker sits
            // on the corner the base map drew, through its reference.
            runScriptLine(QStringLiteral("ÇİZGİ"));
            QCoreApplication::processEvents();
            const core::Point2 corner{30'000, 20'000};
            onCanvas(QEvent::MouseMove, screen(corner) + QPointF(4.0, -3.0), Qt::NoButton);
            const core::SnapResult* shown = canvas_->snapPreviewForProbe();
            const core::SnapResult got    = shown != nullptr ? *shown : core::SnapResult{};
            check(got.mode == core::SnapEndpoint && got.point == corner &&
                      got.entity == xref_reference,
                  QStringLiteral("dış referanstaki parsel köşesi UÇ olarak yakalandı (%1, %2)")
                      .arg(static_cast<double>(got.point.x) / 1000.0, 0, 'f', 3)
                      .arg(static_cast<double>(got.point.y) / 1000.0, 0, 'f', 3));
            shoot("disreferans-yakalama");
            controller_->cancelInteractive();
            QCoreApplication::processEvents();

            // A DOUBLE CLICK on the road: refused, by name, and nothing opens.
            const std::uint64_t untouched = doc.content_hash();
            const QPointF on_road         = screen(core::Point2{20'000, -4'000});
            onCanvas(QEvent::MouseMove, on_road, Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, on_road, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, on_road, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonDblClick, on_road, Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, on_road, Qt::LeftButton);
            endCommand();
            showTranscript();
            QCoreApplication::processEvents();
            check(!blockEdit_.has_value() && doc.content_hash() == untouched &&
                      transcript_->toPlainText().contains(QStringLiteral("dış referansın parçası")),
                  QStringLiteral("çift tıklama dış referansı düzenlemeye açmadı ve nedenini "
                                 "söyledi"));
            shoot("disreferans-duzenlenmez");

            // THE BASE MAP CHANGES ELSEWHERE, and the ribbon's reload brings it.
            write_base_map(true);
            actXrefReload_->trigger();
            endCommand();
            QCoreApplication::processEvents();
            check(live_members() == 4,
                  QStringLiteral("Dış Referansları Yenile ikinci yolu getirdi (%1 üye)")
                      .arg(live_members()));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.8"));
            QCoreApplication::processEvents();
            shoot("disreferans-yenilendi");

            // SAVED: the name and the path, not the objects.
            runScriptLine(QStringLiteral("KAYDET"));
            endCommand();
            QFile saved(project);
            io::FileHeader header{};
            if (saved.open(QIODevice::ReadOnly))
                (void)saved.read(reinterpret_cast<char*>(&header), sizeof(header));
            saved.close();
            check(header.min_reader_version == io::kMinReaderVersionExternal &&
                      transcript_->toPlainText().contains(QStringLiteral("(2 nesne")),
                  QStringLiteral("proje dosyası dış referansın nesnelerini tutmuyor: çizgi ve "
                                 "referans kaydedildi"));

            // GONE: the drawing still opens, and says which file is missing.
            QFile::rename(base_map, folder + QStringLiteral("/altlik-arsiv.pcad"));
            runScriptLine(QStringLiteral("AÇ \"%1\"").arg(project));
            endCommand();
            QCoreApplication::processEvents();
            const core::Document& reopened = controller_->document();
            bool reference_kept            = false;
            for (core::EntityId e = 0; e < reopened.entities().size(); ++e)
                reference_kept =
                    reference_kept || (reopened.entities().standalone(e) &&
                                       reopened.entities().kind[e] == core::kBlockReferenceKind);
            check(reference_kept && live_members() == 0 &&
                      transcript_->toPlainText().contains(QStringLiteral("yüklenemedi")),
                  QStringLiteral("kaynak dosya yokken çizim açıldı, referans yerinde, eksik "
                                 "dosya söylendi"));
            shoot("disreferans-kayip");

            runScriptLine(
                QStringLiteral("DIŞREFERANS islem=yol ad=altlik dosya=altlik-arsiv.pcad"));
            endCommand();
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.8"));
            QCoreApplication::processEvents();
            check(live_members() == 4,
                  QStringLiteral("yeni yeri gösterilen dış referans geri geldi (%1 üye)")
                      .arg(live_members()));
            shoot("disreferans-yeni-yol");
        }

        // ---- 27. THE REFERENCES PANEL AND THE WATCH OVER THEIR FILES (TODOS C-14) ----
        //
        // The external references tab lists the reference with its state; the base
        // map saved elsewhere is noticed — the row reads DEĞİŞTİ and a banner
        // offers the reload — and the banner's button reads it again; the
        // panel's own steps put it aside and bring it back; its eye puts the
        // reference's layers out and back.
        {
            const QString folder =
                (shooting ? into : QDir::tempPath()) + QStringLiteral("/disreferans-panel-probe");
            QDir(folder).removeRecursively();
            QDir().mkpath(folder);
            const QString base_map = folder + QStringLiteral("/halihazir.pcad");
            const QString project  = folder + QStringLiteral("/proje.pcad");
            const auto write_map   = [&base_map](int roads) {
                core::Document side_doc;
                command::Registry side_reg;
                command::Journal side_journal;
                command::UndoStack side_undo;
                command::Bus side{side_doc, side_reg, side_journal, side_undo};
                command::register_builtin_commands(side_reg);
                io::FileService side_files{side};
                (void)side.execute_line("KATMAN ad=BINA", command::Origin::Test);
                (void)side.execute_line("ALAN 0,0 12,0 12,9 0,9", command::Origin::Test);
                (void)side.execute_line("KATMAN ad=YOL", command::Origin::Test);
                for (int r = 0; r < roads; ++r)
                    (void)side.execute_line("ÇİZGİ -4," + std::to_string(-3 - 4 * r) + " 30," +
                                                  std::to_string(-3 - 4 * r),
                                              command::Origin::Test);
                (void)side.execute_line("FARKLIKAYDET \"" + base_map.toStdString() + "\"",
                                          command::Origin::Test);
            };
            write_map(1);
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("ÇİZGİ 2,2 10,7"));
            endCommand();
            runScriptLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(project));
            endCommand();
            runScriptLine(QStringLiteral("DIŞREFERANS dosya=halihazir.pcad"));
            endCommand();
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.7"));
            QCoreApplication::processEvents();
            const QStringList rows =
                xrefPanel_ != nullptr ? xrefPanel_->probeRows() : QStringList();
            check(layerHeader_ != nullptr && layerHeader_->current() == 1 && rows.size() == 1 &&
                      rows.front().startsWith(QStringLiteral("halihazir|YÜKLÜ|2 nesne")),
                  QStringLiteral("bağlayınca Dış Referanslar sekmesi açıldı, satır YÜKLÜ (%1)")
                      .arg(rows.join(QStringLiteral("; "))));
            shoot("disreferans-panel");

            // SAVED ELSEWHERE while the drawing is open: the watch notices.
            write_map(3);
            bool noticed = false;
            for (int wait = 0; wait < 60 && !noticed; ++wait) {
                QThread::msleep(50);
                QCoreApplication::processEvents();
                noticed = xrefPanel_->probeBanner(false);
            }
            const QStringList changed = xrefPanel_->probeRows();
            check(noticed && changed.size() == 1 &&
                      changed.front().startsWith(QStringLiteral("halihazir|DEĞİŞTİ")),
                  QStringLiteral("kaynak başka yerde kaydedilince bant çıktı, satır DEĞİŞTİ (%1)")
                      .arg(changed.join(QStringLiteral("; "))));
            shoot("disreferans-panel-degisti");

            // THE BANNER'S BUTTON reads it again, and the news goes away.
            xrefPanel_->probeBanner(true);
            endCommand();
            QCoreApplication::processEvents();
            const QStringList reread = xrefPanel_->probeRows();
            check(!xrefPanel_->probeBanner(false) && reread.size() == 1 &&
                      reread.front().startsWith(QStringLiteral("halihazir|YÜKLÜ|4 nesne")),
                  QStringLiteral("bandın Yenile düğmesi üç yolu getirdi, bant kalktı (%1)")
                      .arg(reread.join(QStringLiteral("; "))));
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.7"));
            QCoreApplication::processEvents();
            shoot("disreferans-panel-yenilendi");

            // THE PANEL'S OWN STEPS: put aside, brought back.
            const bool unloaded = xrefPanel_->probePress(QStringLiteral("halihazir"), tr("Boşalt"));
            endCommand();
            const QStringList aside = xrefPanel_->probeRows();
            const bool loaded = xrefPanel_->probePress(QStringLiteral("halihazir"), tr("Yükle"));
            endCommand();
            const QStringList back = xrefPanel_->probeRows();
            check(unloaded && loaded && aside.size() == 1 &&
                      aside.front().startsWith(QStringLiteral("halihazir|BOŞALTILDI|0 nesne")) &&
                      back.size() == 1 &&
                      back.front().startsWith(QStringLiteral("halihazir|YÜKLÜ|4 nesne")),
                  QStringLiteral("panelden Boşalt ve Yükle (%1 → %2)")
                      .arg(aside.join(QStringLiteral("; ")), back.join(QStringLiteral("; "))));

            // ITS EYE: the reference's layers out, and back.
            const core::Document& doc = controller_->document();
            const auto shown          = [&doc](const char* layer) {
                const core::LayerId l = doc.find_layer(layer);
                return l != core::kNoLayer && doc.layer(l)->visible;
            };
            xrefPanel_->probeEye(QStringLiteral("halihazir"));
            endCommand();
            const bool out = !shown("halihazir|BINA") && !shown("halihazir|YOL");
            shoot("disreferans-panel-goz");
            xrefPanel_->probeEye(QStringLiteral("halihazir"));
            endCommand();
            check(
                out && shown("halihazir|BINA") && shown("halihazir|YOL"),
                QStringLiteral("satırın gözü dış referansın katmanlarını gizledi ve geri getirdi"));
        }

        // ---- 28. A REFERENCE CLIPPED TO THE STUDY AREA (TODOS C-14) ----
        //
        // A zoning plan attached whole, and the sheet wants the part round the
        // study area. The ribbon's clip button is pressed with a real click,
        // the two corners are clicked on the canvas, and what is outside goes:
        // not drawn, not snapped to; the park's fill stays filled inside and is
        // not stroked along the cut. A polygon drawn corner by corner replaces
        // it; the boundary is drawn out as a line; the remove button brings the
        // whole plan back and one undo clips it again. Saved, the file asks for
        // a reader that knows a clip.
        {
            const QString folder =
                (shooting ? into : QDir::tempPath()) + QStringLiteral("/blokkirp-probe");
            QDir(folder).removeRecursively();
            QDir().mkpath(folder);
            const QString plan    = folder + QStringLiteral("/imar.pcad");
            const QString project = folder + QStringLiteral("/pafta.pcad");
            {
                core::Document side_doc;
                command::Registry side_reg;
                command::Journal side_journal;
                command::UndoStack side_undo;
                command::Bus side{side_doc, side_reg, side_journal, side_undo};
                command::register_builtin_commands(side_reg);
                io::FileService side_files{side};
                for (const std::string& line :
                     {std::string("KATMAN ad=PARSEL"), std::string("ALAN 0,0 20,0 20,15 0,15"),
                      std::string("ALAN 20,0 40,0 40,15 20,15"),
                      std::string("ALAN 40,0 60,0 60,15 40,15"),
                      std::string("ALAN 0,15 20,15 20,30 0,30"),
                      std::string("ALAN 20,15 40,15 40,30 20,30"),
                      std::string("ALAN 40,15 60,15 60,30 40,30"), std::string("KATMAN ad=PARK"),
                      std::string("TARAMA noktalar=21,16 39,16 39,29 21,29"),
                      std::string("KATMAN ad=YOL"), std::string("ÇİZGİ -5,-4 65,-4"),
                      std::string("KATMAN ad=AGAC"), std::string("DAİRE merkez=50,22 cevre=52,22")})
                    (void)side.execute_line(line, command::Origin::Test);
                (void)side.execute_line("FARKLIKAYDET \"" + plan.toStdString() + "\"",
                                        command::Origin::Test);
            }
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(project));
            endCommand();
            runScriptLine(QStringLiteral("DIŞREFERANS dosya=imar.pcad"));
            endCommand();
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            runScriptLine(QStringLiteral("YAKINLAŞ mod=ÇARPAN carpan=0.75"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            const core::Document& doc = controller_->document();
            std::int64_t plan_key     = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.entities().standalone(e) &&
                    doc.entities().kind[e] == core::kBlockReferenceKind)
                    plan_key = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
            const auto plan_slot = [&doc, &plan_key] {
                return doc.slot_of(
                    static_cast<core::EntityKey>(static_cast<std::uint64_t>(plan_key)));
            };
            const auto clip_of = [&doc, &plan_slot] {
                auto ref =
                    core::block_reference_of(doc.geometry(), doc.entities().slot[plan_slot()]);
                return ref ? ref.value().clip : std::vector<core::Point2>{};
            };
            check(plan_key != 0 && doc.entities().box_of(plan_slot()).max_x == 65'000,
                  QStringLiteral("imar planı dış referans olarak bağlandı, bütün çiziliyor"));
            shoot("blokkirp-oncesi");

            // KIRP, PRESSED BY HAND, then the rectangle's two corners clicked.
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(plan_key));
            QCoreApplication::processEvents();
            QToolButton* clip_button = ribbonButton(actBlockClip_, true);
            if (clip_button != nullptr) clickButton(clip_button, 60);
            const command::Session* asking = controller_->session();
            check(clip_button != nullptr && asking != nullptr && asking->waiting() &&
                      QString::fromStdString(asking->prompt().message)
                          .contains(QStringLiteral("ilk köşesi")),
                  QStringLiteral("şeritteki Kırp düğmesi seçili referans için köşe soruyor"));
            const core::Point2 from{10'000, -8'000};
            const core::Point2 to{45'000, 25'000};
            onCanvas(QEvent::MouseMove, screen(from), Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, screen(from), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(from), Qt::LeftButton);
            onCanvas(QEvent::MouseMove, screen(to), Qt::NoButton);
            QCoreApplication::processEvents();
            shoot("blokkirp-dikdortgen");
            onCanvas(QEvent::MouseButtonPress, screen(to), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(to), Qt::LeftButton);
            endCommand();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            // Clicks land on what the pointer is near (`probe-yazili-nokta`), so
            // the box is read from the document, not assumed.
            const core::Box2 kept = doc.entities().box_of(plan_slot());
            check(clip_of().size() == 4 && kept.min_x >= 9'000 && kept.max_x <= 46'000 &&
                      kept.max_y <= 26'000,
                  QStringLiteral("iki tıkla kırpıldı: çizilen kutu %1–%2 m doğu, en çok %3 m kuzey")
                      .arg(static_cast<double>(kept.min_x) / 1000.0, 0, 'f', 1)
                      .arg(static_cast<double>(kept.max_x) / 1000.0, 0, 'f', 1)
                      .arg(static_cast<double>(kept.max_y) / 1000.0, 0, 'f', 1));
            // THE PARK STAYS FILLED where the clip shows it: a fill-only face.
            core::EmitBuffer cropped;
            core::entity_outline(doc, plan_slot(), cropped);
            std::size_t faces = 0;
            for (std::size_t r = 0; r < cropped.run_total(); ++r)
                if (!cropped.run_edge(r)) ++faces;
            check(faces > 0,
                  QStringLiteral("kesilen parkın dolgusu içeride kaldı (%1 yüz)").arg(faces));
            shoot("blokkirp-dikdortgen-sonrasi");

            // SELECTED, it shows where its boundary runs — dashed, on the
            // screen only, as AutoCAD's clip frame.
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(plan_key));
            canvas_->repaint();
            QCoreApplication::processEvents();
            if (!canvas_->grabCanvas().isNull())
                check(canvas_->clipFrameCountForProbe() == 1,
                      QStringLiteral("seçili kırpılmış referans sınırını kesik çizgiyle gösteriyor "
                                     "(%1 çerçeve)")
                          .arg(canvas_->clipFrameCountForProbe()));
            shoot("blokkirp-secili");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();

            // SNAPPED where it shows, and nowhere it does not.
            runScriptLine(QStringLiteral("ÇİZGİ"));
            QCoreApplication::processEvents();
            const core::Point2 shown_corner{20'000, 15'000};
            onCanvas(QEvent::MouseMove, screen(shown_corner) + QPointF(4.0, -3.0), Qt::NoButton);
            const core::SnapResult* near_shown = canvas_->snapPreviewForProbe();
            const bool took_shown              = near_shown != nullptr &&
                                    near_shown->mode == core::SnapEndpoint &&
                                    near_shown->point == shown_corner;
            const core::Point2 hidden_corner{60'000, 30'000};
            onCanvas(QEvent::MouseMove, screen(hidden_corner) + QPointF(4.0, -3.0), Qt::NoButton);
            const core::SnapResult* near_hidden = canvas_->snapPreviewForProbe();
            const bool took_hidden              = near_hidden != nullptr &&
                                     near_hidden->mode == core::SnapEndpoint &&
                                     near_hidden->point == hidden_corner;
            check(took_shown && !took_hidden,
                  QStringLiteral("görünen parsel köşesi yakalandı, sınır dışındaki köşe "
                                 "yakalanmadı"));
            controller_->cancelInteractive();
            QCoreApplication::processEvents();

            // A POLYGON, corner by corner, from the ribbon's own button; the
            // right click closes it.
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(plan_key));
            QCoreApplication::processEvents();
            // Its own button on the block context tab: the family's face on the
            // drawing tab runs whichever member was used last.
            QToolButton* polygon_button = ribbonButton(actBlockClipPolygon_, true, true);
            QCoreApplication::processEvents();
            shoot("blokkirp-serit");
            if (polygon_button != nullptr) clickButton(polygon_button, 60);
            const core::Point2 outline[5] = {{5'000, -6'000},
                                             {50'000, -6'000},
                                             {58'000, 20'000},
                                             {30'000, 33'000},
                                             {2'000, 20'000}};
            for (const core::Point2 c : outline) {
                onCanvas(QEvent::MouseMove, screen(c), Qt::NoButton);
                onCanvas(QEvent::MouseButtonPress, screen(c), Qt::LeftButton);
                onCanvas(QEvent::MouseButtonRelease, screen(c), Qt::LeftButton);
            }
            QCoreApplication::processEvents();
            shoot("blokkirp-cokgen-cizilirken");
            onCanvas(QEvent::MouseButtonPress, screen(outline[4]), Qt::RightButton);
            onCanvas(QEvent::MouseButtonRelease, screen(outline[4]), Qt::RightButton);
            endCommand();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            check(polygon_button != nullptr && clip_of().size() == 5,
                  QStringLiteral("Çokgenle Kırp beş köşeli sınırı koydu (%1 köşe)")
                      .arg(clip_of().size()));
            shoot("blokkirp-cokgen");

            // THE BOUNDARY DRAWN OUT, from the context tab's button.
            const std::size_t before_line = doc.live_entity_count();
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(plan_key));
            QCoreApplication::processEvents();
            QToolButton* boundary_button = ribbonButton(actBlockClipBoundary_, true);
            if (boundary_button != nullptr) clickButton(boundary_button, 60);
            endCommand();
            check(boundary_button != nullptr && doc.live_entity_count() == before_line + 1,
                  QStringLiteral("Kırpma Sınırını Çiz sınırı kapalı çizgi olarak çizdi"));

            // TAKEN OFF, by hand; one undo clips it again.
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(plan_key));
            QCoreApplication::processEvents();
            QToolButton* unclip_button = ribbonButton(actBlockUnclip_, true);
            if (unclip_button != nullptr) clickButton(unclip_button, 60);
            endCommand();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            const bool whole =
                clip_of().empty() && doc.entities().box_of(plan_slot()).max_x == 65'000;
            shoot("blokkirp-kaldirildi");
            runScriptLine(QStringLiteral("GERİAL"));
            endCommand();
            QCoreApplication::processEvents();
            check(unclip_button != nullptr && whole && clip_of().size() == 5,
                  QStringLiteral("Kırpmayı Kaldır bütün planı getirdi, tek geri alma yeniden "
                                 "kırptı"));

            // SAVED: a clip asks for a reader that knows one.
            runScriptLine(QStringLiteral("KAYDET"));
            endCommand();
            QFile saved(project);
            io::FileHeader header{};
            if (saved.open(QIODevice::ReadOnly))
                (void)saved.read(reinterpret_cast<char*>(&header), sizeof(header));
            saved.close();
            check(header.min_reader_version == io::kMinReaderVersionClip,
                  QStringLiteral("kırpılmış referanslı proje biçim %1 istiyor")
                      .arg(header.min_reader_version));
        }

        // ---- 29. AN ELLIPSE AND A SPLINE CUT LIKE LINES (TODOS C-01) ----
        //
        // An ellipse and a parabola-shaped spline, each crossed by a road. BUDA
        // armed with nothing selected cuts with everything near: the pointer on
        // the ellipse's top shows the arc that will go — cut where the road
        // really meets the curve, x = ±8 — and the click takes it, leaving an
        // elliptic arc; the spline's top goes the same way and two splines stay.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", "ÇİZGİ -20,3 20,3",
                  "SPLINE noktalar=30,0 35,10 40,0 derece=2", "ÇİZGİ 25,3.2 45,3.2"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            canvas_->zoomToBox(core::Box2{-22'000, -8'000, 48'000, 14'000});
            QCoreApplication::processEvents();
            shoot("egri-oncesi");

            const core::Document& doc = controller_->document();
            actTrim_->trigger();
            QCoreApplication::processEvents();
            const core::Point2 ellipse_top{0, 5'000};
            onCanvas(QEvent::MouseMove, screen(ellipse_top), Qt::NoButton);
            canvas_->repaint();
            QCoreApplication::processEvents();
            const QString said = QString::fromStdString(canvas_->guideLabelForProbe());
            if (!canvas_->grabCanvas().isNull())
                check(said.contains(QStringLiteral("2 kesişim")) &&
                          said.contains(QStringLiteral("16,71")),
                      QStringLiteral("elipsin tepesinde budanacak yay önizleniyor (%1)").arg(said));
            shoot("egri-buda-onizleme");
            onCanvas(QEvent::MouseButtonPress, screen(ellipse_top), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(ellipse_top), Qt::LeftButton);
            QCoreApplication::processEvents();
            const auto arc_of = [&doc](std::int64_t key) {
                const core::EntityId e =
                    doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                return e == core::kNoEntity
                           ? std::optional<core::EllipseArc>{}
                           : core::ellipse_arc_of(doc.geometry(), doc.entities().slot[e]);
            };
            const std::optional<core::EllipseArc> arc = arc_of(1);
            check(arc.has_value() && std::llabs(arc->start_udeg - 143'130'102) <= 2 &&
                      std::llabs(arc->end_udeg - 36'869'898) <= 2,
                  QStringLiteral("tık elipsin üst yayını attı; x = ±8'de kesilen elips yayı "
                                 "kaldı"));

            const core::Point2 spline_top{35'000, 5'000};
            onCanvas(QEvent::MouseMove, screen(spline_top), Qt::NoButton);
            QCoreApplication::processEvents();
            shoot("egri-buda-spline-onizleme");
            onCanvas(QEvent::MouseButtonPress, screen(spline_top), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, screen(spline_top), Qt::LeftButton);
            QCoreApplication::processEvents();
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
            std::size_t splines = 0;
            bool ends_on_road   = true;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
                if (!doc.alive(e) || doc.entities().kind[e] != core::kSplineKind) continue;
                ++splines;
                const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
                const auto ys             = doc.geometry().ring_ys(span.first);
                ends_on_road = ends_on_road && (ys.front() == 3'200 || ys.back() == 3'200);
            }
            check(splines == 2 && ends_on_road,
                  QStringLiteral("spline'ın tepesi gitti; yola değen iki spline kaldı (%1)")
                      .arg(splines));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();
            shoot("egri-buda-sonrasi");
        }

        // ---- 30. ONE MEASURE FOR A TEXT'S WIDTH (TODOS C-18) ----
        //
        // "<> (tapu)" on a 20 m dimension is 20,10 m of words at a 2,5 m
        // capital: it goes out past the extension line, and what the canvas
        // DRAWS is as wide as the box the core measured — the caption's ink,
        // found in the real frame, starts past the extension line and fills
        // its box to within the letters' own side bearings. A road name turned
        // toward a point 120 m away has a box as long as its words. The same
        // sheet printed to PDF and written to DXF puts the caption in the same
        // place.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line : {"ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3",
                                     "ÖLÇÜDÜZENLE nesneler=1 metin=\"<> (tapu)\"",
                                     "METİN 0,-14 \"ATATÜRK CADDESİ\" 2500 bitis=120,-4"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
            canvas_->zoomToBox(core::Box2{-4'000, -18'000, 46'000, 6'000});
            canvas_->repaint();
            QCoreApplication::processEvents();

            const core::Document& doc = controller_->document();
            const auto entity         = [&doc](std::int64_t key) {
                return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            };
            const auto box_of = [&doc](core::EntityId e) {
                std::array<core::Point2, 4> quad{};
                core::Box2 box;
                if (e != core::kNoEntity && core::text_quad(doc, e, quad))
                    for (const core::Point2 p : quad)
                        box.extend(p);
                return box;
            };
            const core::EntityId dim    = entity(1);
            const core::EntityId street = entity(2);
            const std::string caption =
                dim == core::kNoEntity ? std::string()
                                       : std::string(doc.texts().text(doc.entities().slot[dim]));
            const core::Mm wide     = core::text_width(caption, 2'500);
            const core::Box2 figure = box_of(dim);
            check(caption == "20,00 (tapu)" && wide == 20'097 && figure.min_x == 20'625 &&
                      std::llabs((figure.max_x - figure.min_x) - wide) <= 1,
                  QStringLiteral("20 m'lik ölçünün \"%1\" yazısı (%2 m) uzatma çizgisinin "
                                 "dışında, bir boşluk ötede (%3 m'de başlıyor)")
                      .arg(QString::fromStdString(caption))
                      .arg(static_cast<double>(wide) / 1000.0, 0, 'f', 3)
                      .arg(static_cast<double>(figure.min_x) / 1000.0, 0, 'f', 3));
            const core::Box2 named = box_of(street);
            const core::Mm words   = core::text_width("ATATÜRK CADDESİ", 2'500);
            {
                const core::RingSpan rs    = doc.geometry().rings_of(doc.entities().slot[street]);
                const core::Point2 from_pt = doc.geometry().vertex(rs.first, 0);
                const core::Point2 to_pt   = doc.geometry().vertex(rs.first, 1);
                check(std::llabs(core::text_baseline_length(from_pt, to_pt) - words) <= 1 &&
                          named.max_x < 40'000,
                      QStringLiteral("120 m ötedeki yön noktası kutuyu uzatmadı: %1 m yazı, %2 "
                                     "m taban çizgisi")
                          .arg(static_cast<double>(words) / 1000.0, 0, 'f', 3)
                          .arg(static_cast<double>(core::text_baseline_length(from_pt, to_pt)) /
                                   1000.0,
                               0, 'f', 3));
            }

            // THE INK, in the frame the GPU drew: the caption's rows between
            // its baseline and its capital tops, right of the extension line.
            // The pointer is parked in a corner first, so its crosshair does
            // not cross the rows.
            {
                const auto parked = canvas_->view().to_screen(core::Point2{45'000, -17'000});
                onCanvas(QEvent::MouseMove, QPointF(parked.x, parked.y), Qt::NoButton);
                canvas_->repaint();
                QCoreApplication::processEvents();
            }
            const QImage frame = canvas_->grabCanvas();
            if (!frame.isNull() && canvas_->width() > 0) {
                const double k = static_cast<double>(frame.width()) / canvas_->width();
                const auto px  = [&](core::Point2 world) {
                    const auto at = canvas_->view().to_screen(world);
                    return QPointF(at.x * k, at.y * k);
                };
                const core::Mm base =
                    figure.max_y - 2'500; // the caption's baseline (middle-centred)
                const QPointF cap_left  = px(core::Point2{figure.min_x, base});
                const QPointF cap_right = px(core::Point2{figure.max_x, base});
                const QPointF cap_top   = px(core::Point2{figure.min_x, figure.max_y});
                const QPointF line      = px(core::Point2{20'000, base});
                const int y0            = std::max(0, qRound(cap_top.y()) + 2);
                const int y1            = std::min(frame.height() - 1, qRound(cap_left.y()) - 2);
                const int x0 = qRound(line.x()) + qRound(3 * k); // clear of the extension line
                const int x1 = std::min(frame.width() - 1, qRound(cap_right.x()) + 40);
                // THE GROUND is the rows' commonest colour: letters and the odd
                // grid line are thin, the sheet between them is not.
                std::map<QRgb, int> tally;
                for (int x = x0; x <= x1; ++x)
                    for (int y = y0; y <= y1; ++y)
                        ++tally[frame.pixel(x, y)];
                QRgb ground = 0;
                int most    = 0;
                for (const auto& [colour, n] : tally)
                    if (n > most) {
                        most   = n;
                        ground = colour;
                    }
                const auto apart = [ground](QRgb c) {
                    return std::abs(qRed(c) - qRed(ground)) + std::abs(qGreen(c) - qGreen(ground)) +
                           std::abs(qBlue(c) - qBlue(ground));
                };
                // THE INK IS THE LETTERS' COLOUR, whatever the theme: a letter's
                // stem is the rows' pixel farthest from the ground, and a pixel
                // counts when it is more than halfway there — a grid line
                // crossing the rows is not, and neither is anti-aliasing.
                int deepest = 0;
                for (int x = x0; x <= x1; ++x)
                    for (int y = y0; y <= y1; ++y)
                        deepest = std::max(deepest, apart(frame.pixel(x, y)));
                int first_ink = -1;
                int last_ink  = -1;
                for (int x = x0; x <= x1; ++x)
                    for (int y = y0; y <= y1; ++y)
                        if (2 * apart(frame.pixel(x, y)) > deepest) {
                            if (first_ink < 0) first_ink = x;
                            last_ink = x;
                            break;
                        }
                // WHERE THE INK MUST BE: the box shrunk by the first letter's
                // left side bearing and the last one's right, read off the face
                // itself (at 1000 px an EM, a pixel is a font unit).
                const core::TextFace face = core::text_face();
                const double em =
                    (cap_left.y() - cap_top.y()) * face.units_per_em / face.cap_height;
                const QRawFont raw(QString::fromStdString(data_path("fonts")) +
                                       QStringLiteral("/IBMPlexSans-Regular.ttf"),
                                   face.units_per_em, QFont::PreferNoHinting);
                const QList<quint32> ends = raw.glyphIndexesForString(QStringLiteral("2)"));
                double lead               = 0.0; // the `2`'s left side bearing, in EM
                double trail              = 0.0; // the `)`'s right one
                if (raw.isValid() && ends.size() == 2) {
                    const QList<QPointF> step = raw.advancesForGlyphIndexes(ends);
                    lead  = raw.boundingRect(ends[0]).left() / face.units_per_em;
                    trail = (step[1].x() - raw.boundingRect(ends[1]).right()) / face.units_per_em;
                }
                const double want_first = cap_left.x() + (lead * em);
                const double want_last  = cap_right.x() - (trail * em);
                check(raw.isValid() && first_ink > qRound(line.x()) &&
                          std::abs(first_ink - want_first) <= 3.0 &&
                          std::abs(last_ink - want_last) <= 4.0,
                      QStringLiteral("tuvalde yazının mürekkebi kutusunun bittiği yerde bitiyor: "
                                     "mürekkep %1–%2 px, beklenen %3–%4 px (kutu %5–%6 px, EM "
                                     "%7 px); uzatma çizgisi %8 px'te")
                          .arg(first_ink)
                          .arg(last_ink)
                          .arg(want_first, 0, 'f', 1)
                          .arg(want_last, 0, 'f', 1)
                          .arg(cap_left.x(), 0, 'f', 1)
                          .arg(cap_right.x(), 0, 'f', 1)
                          .arg(em, 0, 'f', 1)
                          .arg(line.x(), 0, 'f', 1));
            } else {
                (void)std::fprintf(stdout, "[fare] BEKLEMEDE: tuval çizmiyor (QRhi yok); yazının "
                                           "mürekkebi gerçek pencerede ölçülür\n");
            }
            shoot("yazi-olcusu");

            const QTemporaryDir scratch;
            const QString folder = shooting ? into : scratch.path();
            runScriptLine(QStringLiteral("YAZDIR merkez=20,-6 olcek=500 dosya=\"%1\"")
                              .arg(folder + QStringLiteral("/yazi-olcusu.pdf")));
            endCommand();
            check(QFileInfo::exists(folder + QStringLiteral("/yazi-olcusu.pdf")),
                  QStringLiteral("aynı pafta PDF'e yazıldı"));

            // THE DXF: the caption's TEXT in the dimension's picture block, centred
            // on its point — a reader with the same face draws it from x - w/2.
            const QString dxf = folder + QStringLiteral("/yazi-olcusu.dxf");
            runScriptLine(QStringLiteral("DIŞAAKTAR dosya=\"%1\"").arg(dxf));
            endCommand();
            QFile file(dxf);
            double centre_x = 0.0;
            bool found      = false;
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QStringList lines =
                    QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
                for (qsizetype i = 0; i + 1 < lines.size() && !found; ++i) {
                    if (lines[i].trimmed() != QLatin1String("TEXT")) continue;
                    double x11 = 0.0;
                    bool ours  = false;
                    for (qsizetype j = i + 1; j + 1 < lines.size(); j += 2) {
                        const QString code = lines[j].trimmed();
                        if (code == QLatin1String("0")) break;
                        if (code == QLatin1String("11")) x11 = lines[j + 1].trimmed().toDouble();
                        if (code == QLatin1String("1") &&
                            lines[j + 1].trimmed() == QLatin1String("20,00 (tapu)"))
                            ours = true;
                    }
                    if (ours) {
                        centre_x = x11;
                        found    = true;
                    }
                }
            }
            const double from = centre_x - (static_cast<double>(wide) / 2000.0);
            check(found && std::abs(from - 20.625) < 0.002,
                  QStringLiteral("DXF'te yazı aynı yerde: %1 m merkezli, %2 m'de başlıyor")
                      .arg(centre_x, 0, 'f', 4)
                      .arg(from, 0, 'f', 4));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 31. A PARCEL DRAGGED BY ITS CORNER IS STILL ITS ROW (TODOS F-02) ----
        //
        // The attribute table read a cell by the object's ROW NUMBER, and a
        // grip moves an object to a new geometry slot — so after the first drag
        // the table showed the parcel's old value, or its neighbour's, and a
        // value typed into its row seemed not to stick. Two parcels with values
        // of their own; the first dragged by a corner with the mouse; then the
        // table asked, and written through.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=PARSEL", "SÜTUN kimlik=ada tur=tam_sayi", "ALAN 0,0 20,0 20,10 0,10",
                  "ALAN 30,0 40,0 40,10 30,10", "ÖZNİTELİK ada 1 101", "ÖZNİTELİK ada 2 202"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            canvas_->zoomToBox(core::Box2{-5'000, -10'000, 50'000, 20'000});
            runScriptLine(QStringLiteral("SEÇ nesneler=1"));
            QCoreApplication::processEvents();
            const auto at = [this](core::Point2 world) {
                const auto on = canvas_->view().to_screen(world);
                return QPointF(on.x, on.y);
            };
            onCanvas(QEvent::MouseMove, at(core::Point2{20'000, 0}), Qt::NoButton);
            onCanvas(QEvent::MouseButtonPress, at(core::Point2{20'000, 0}), Qt::LeftButton);
            onCanvas(QEvent::MouseMove, at(core::Point2{22'500, 0}), Qt::LeftButton);
            onCanvas(QEvent::MouseMove, at(core::Point2{25'000, 0}), Qt::LeftButton);
            onCanvas(QEvent::MouseButtonRelease, at(core::Point2{25'000, 0}), Qt::LeftButton);
            controller_->cancelInteractive();
            QCoreApplication::processEvents();

            const core::Document& doc = controller_->document();
            const core::EntityId dragged =
                doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
            const core::RingSpan outline = doc.geometry().rings_of(doc.entities().slot[dragged]);
            const core::Point2 corner{doc.geometry().ring_xs(outline.first)[1],
                                      doc.geometry().ring_ys(outline.first)[1]};
            check(std::llabs(corner.x - 25'000) <= 300 && std::llabs(corner.y) <= 300 &&
                      doc.entities().slot[dragged] != static_cast<std::uint32_t>(dragged),
                  QStringLiteral("parselin köşesi fareyle 25 m'ye çekildi; nesne yeni geometri "
                                 "yuvasında (%1, %2)")
                      .arg(static_cast<double>(corner.x) / 1000.0)
                      .arg(static_cast<double>(corner.y) / 1000.0));

            AttributeTable table(*controller_, QString(), this);
            table.applyTheme(theme_);
            table.resize(900, 360);
            table.show();
            QCoreApplication::processEvents();
            const auto grid = [&table](const char* action, const QString& value = QString()) {
                return table.probeGrid(QString::fromUtf8(action), value);
            };
            check(grid("satirlar") == QStringLiteral("2") &&
                      grid("hucre", QStringLiteral("0,0")) == QStringLiteral("1") &&
                      grid("hucre", QStringLiteral("0,1")) == QStringLiteral("101") &&
                      grid("hucre", QStringLiteral("1,1")) == QStringLiteral("202"),
                  QStringLiteral("tabloda çekilen parselin satırı kendi değerini gösteriyor "
                                 "(%1 satır; 1: %2, 2: %3)")
                      .arg(grid("satirlar"), grid("hucre", QStringLiteral("0,1")),
                           grid("hucre", QStringLiteral("1,1"))));
            if (shooting) (void)table.grab().save(into + QStringLiteral("/kimlik-tablo.png"));

            grid("kip", QStringLiteral("evet"));
            grid("git", QStringLiteral("0,1"));
            grid("ac");
            grid("yaz", QStringLiteral("103"));
            const auto value_of = [&doc](std::uint64_t key) {
                const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
                const auto v           = doc.attribute(doc.attributes().find("ada"), e);
                return v.ok() && v.value().present ? v.value().number : std::int64_t{-1};
            };
            check(value_of(1) == 103 && value_of(2) == 202 &&
                      grid("hucre", QStringLiteral("0,1")) == QStringLiteral("103"),
                  QStringLiteral("tabloya yazılan değer çekilen parsele gitti ve orada göründü "
                                 "(1: %1, 2: %2)")
                      .arg(value_of(1))
                      .arg(value_of(2)));
            if (shooting)
                (void)table.grab().save(into + QStringLiteral("/kimlik-tablo-yazildi.png"));
            table.close();
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 32. A PIECE OF AN IFRAZ KNOWS ITS PARCEL (TODOS F-02) ----
        //
        // A parcel cut in two leaves the sheet; each piece still says what it
        // was cut from — in the attribute panel, by the name a user types, the
        // parcel marked as gone — and NESNEBİLGİ says the same to the command
        // line and, in its report, to every other client.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line : {"KATMAN ad=PARSEL", "ALAN 0,0 20,0 20,10 0,10",
                                     "İFRAZ nesneler=1 noktalar=10,-5 10,15"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            canvas_->zoomToBox(core::Box2{-5'000, -10'000, 25'000, 20'000});
            runScriptLine(QStringLiteral("SEÇ nesneler=2"));
            attributePanel_->refresh();
            QCoreApplication::processEvents();
            const QString origin = attributePanel_->probeRowValue(QStringLiteral("koken"));
            check(origin == QStringLiteral("İFRAZ ← 1 (silinmiş)"),
                  QStringLiteral("seçilen ifraz parçasının panelinde kökeni yazıyor (\"%1\")")
                      .arg(origin));
            if (shooting) {
                attributePanel_->resize(420, 560);
                (void)attributePanel_->grab().save(into + QStringLiteral("/koken-panel.png"));
            }
            shoot("koken-ifraz");
            runScriptLine(QStringLiteral("NESNEBİLGİ nesneler=3"));
            endCommand();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("kökeni: İFRAZ (kaynak: nesne 1 (artık çizimde değil))")),
                  QStringLiteral("NESNEBİLGİ öbür parçanın kökenini söylüyor"));
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 33. A LINKED FILE'S OBJECT: REFUSED, AND THE WAY OUT OFFERED (TODOS F-02) ----
        //
        // A parcel sheet linked as an external reference is drawn here and
        // edited in its own file. Taking it apart is refused — and the refusal
        // is not a dead end: a strip over the canvas says why and offers Yerel
        // Kopya, which, pressed, brings the parcels in as this drawing's own on
        // the layer their file calls them by, the link left as it was.
        {
            const QTemporaryDir scratch;
            const QString folder = shooting ? into : scratch.path();
            const QString source = folder + QStringLiteral("/altlik-kaynak.pcad");
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=PARSEL", "ALAN 0,0 20,0 20,10 0,10", "ALAN 20,0 40,0 40,10 20,10"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            runScriptLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(source));
            endCommand();
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("DIŞREFERANS dosya=\"%1\" ad=altlik").arg(source));
            endCommand();
            canvas_->zoomToBox(core::Box2{-5'000, -10'000, 45'000, 20'000});
            const core::Document& doc = controller_->document();
            std::int64_t reference    = 0;
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.entities().standalone(e) &&
                    doc.entities().kind[e] == core::kBlockReferenceKind)
                    reference = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
            runScriptLine(QStringLiteral("PATLAT nesne=%1").arg(reference));
            endCommand();
            QCoreApplication::processEvents();
            const QString offered = remedyForProbe();
            check(offered == QStringLiteral("YERELKOPYA nesneler=%1").arg(reference) &&
                      transcript_->toPlainText().contains(
                          QStringLiteral("Öneri: YERELKOPYA nesneler=%1").arg(reference)),
                  QStringLiteral("dış referansı patlatma reddi yolu gösteriyor: şeritte ve "
                                 "transkriptte \"%1\"")
                      .arg(offered));
            shoot("yerel-kopya-oneri");
            check(pressRemedyForProbe(), QStringLiteral("şeritteki Yerel Kopya düğmesine basıldı"));
            QCoreApplication::processEvents();
            std::size_t own             = 0;
            const core::LayerId parcels = doc.find_layer("PARSEL");
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().standalone(e) &&
                    doc.entities().layer[e] == parcels &&
                    doc.entities().kind[e] != core::kBlockReferenceKind)
                    ++own;
            check(own == 2 && remedyForProbe().isEmpty() &&
                      doc.blocks().find("altlik") != core::kNoBlock,
                  QStringLiteral("iki parsel PARSEL katmanına bu çizimin kendi nesnesi olarak "
                                 "geldi, bağlantı yerinde, şerit kapandı (%1 nesne)")
                      .arg(own));
            shoot("yerel-kopya-sonra");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 34. A SYSTEM THAT COUNTS DEGREES IS REFUSED AT EVERY DOOR (TODOS F-03) ----
        //
        // The store holds millimetres, so a drawing cannot be "in" WGS 84. Asked
        // for as the drawing's system it is refused and the drawing keeps the
        // one it had; a GeoPackage in degrees is refused with the way to convert
        // it; and a reading is written to the project's coordinate precision,
        // rounded in integers.
        {
            const QTemporaryDir scratch;
            const QString folder = shooting ? into : scratch.path();
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("AYAR koordinat_sistemi EPSG:5256"));
            endCommand();
            const core::Document& drawing = controller_->document();
            const std::string kept_crs    = drawing.crs().id();
            // A read or a write of a file runs on a worker; its answer lands when
            // the worker is done, and not a line before.
            const auto settled = [this] {
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 20000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                QCoreApplication::processEvents();
            };

            if (!controller_->bus().on_crs_resolve) {
                (void)std::fprintf(stdout, "[fare] not: CRS çözücüsü yok (KENTOS_DATA?); "
                                           "sistem reddi sınanmadı\n");
            } else {
                transcript_->clear();
                runScriptLine(QStringLiteral("AYAR koordinat_sistemi EPSG:4326"));
                endCommand();
                check(drawing.crs().id() == kept_crs &&
                          transcript_->toPlainText().contains(
                              QStringLiteral("Çizimin koordinat sistemi değişmedi")) &&
                          transcript_->toPlainText().contains(QStringLiteral("derece")),
                      QStringLiteral("çizimin sistemi coğrafi yapılamadı ve nedeni söylendi (%1 "
                                     "kaldı)")
                          .arg(QString::fromStdString(drawing.crs().id())));
                shoot("sistem-derece-reddi");
            }

            // A parcel exported in TM36, carried to degrees by GDAL's own tool,
            // and offered back.
            for (const char* line :
                 {"KATMAN ad=PARSEL", "ALAN 485300,4310200 485360,4310200 485360,4310245 "
                                      "485300,4310245"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            const QString tm36 = folder + QStringLiteral("/tm36.gpkg");
            const QString wgs  = folder + QStringLiteral("/wgs84.gpkg");
            QFile::remove(tm36);
            QFile::remove(wgs);
            runScriptLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(tm36));
            settled();
            QProcess convert;
            convert.start(QStringLiteral("ogr2ogr"),
                          {QStringLiteral("-t_srs"), QStringLiteral("EPSG:4326"), wgs, tm36});
            const bool converted = convert.waitForFinished(30'000) &&
                                   convert.exitStatus() == QProcess::NormalExit &&
                                   convert.exitCode() == 0 && QFileInfo::exists(wgs);
            if (!converted) {
                (void)std::fprintf(stdout, "[fare] not: ogr2ogr yok; WGS 84 dosyası sınanmadı\n");
            } else {
                runScriptLine(QStringLiteral("YENİ"));
                endCommand();
                runScriptLine(QStringLiteral("AYAR koordinat_sistemi EPSG:5256"));
                endCommand();
                transcript_->clear();
                runScriptLine(QStringLiteral("İÇEAKTAR \"%1\"").arg(wgs));
                settled();
                const QString said = transcript_->toPlainText();
                check(controller_->document().live_entity_count() == 0 &&
                          said.contains(QStringLiteral("içe alınmadı")) &&
                          said.contains(QStringLiteral("ogr2ogr -t_srs")),
                      QStringLiteral("derece sayan GeoPackage reddedildi, dönüştürme yolu "
                                     "söylendi"));
                shoot("derece-geopackage-reddi");
            }

            // THE READING, to the project's decimals.
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("AYAR koordinat_hassasiyeti 2"));
            endCommand();
            canvas_->zoomToBox(core::Box2{485'310'000, 4'310'215'000, 485'330'000, 4'310'225'000});
            transcript_->clear();
            runScriptLine(QStringLiteral("KOORDİNAT nokta=485320.155,4310220.254"));
            endCommand();
            QCoreApplication::processEvents();
            check(transcript_->toPlainText().contains(QStringLiteral("485320,16 m")) &&
                      transcript_->toPlainText().contains(QStringLiteral("4310220,25 m")),
                  QStringLiteral("KOORDİNAT okuması iki ondalıkla, yarımdan uzağa yazıldı"));
            shoot("koordinat-hassasiyeti");
            runScriptLine(QStringLiteral("AYAR koordinat_hassasiyeti varsayilan"));
            endCommand();
        }

        // ---- 35. A TYPED COORDINATE IS KEPT; A CLICK IS SNAPPED (TODOS F-03) ----
        //
        // Zoomed out far enough for the aperture to reach a metre, a line is
        // TYPED to start fifteen centimetres off a corner: it starts there, not on
        // the corner, because a written coordinate is not a guess. A click as far
        // off the corner on its OTHER side — nearer the corner than the typed
        // line's own end, which is an end point too — with the ribbon's Çizgi is
        // taken to the corner, because that is what the hand meant.
        {
            fresh({QStringLiteral("ÇİZGİ 0,0 20,0")});
            canvas_->zoomToBox(core::Box2{-40'000, -30'000, 60'000, 40'000});
            QCoreApplication::processEvents();
            const auto first_vertex_of_last = [this] {
                const core::Document& doc = controller_->document();
                core::EntityId last       = core::kNoEntity;
                for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                    if (doc.alive(e) && doc.entities().kind[e] == core::kPolylineKind) last = e;
                if (last == core::kNoEntity) return core::Point2{};
                const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[last]);
                return core::Point2{doc.geometry().ring_xs(span.first)[0],
                                    doc.geometry().ring_ys(span.first)[0]};
            };

            runScriptLine(QStringLiteral("ÇİZGİ 20.15,0.1 30,10"));
            endCommand();
            const core::Point2 typed = first_vertex_of_last();
            check(typed == core::Point2{20'150, 100},
                  QStringLiteral("yazılan nokta köşeye çekilmedi, yazıldığı yere düştü (%1, %2)")
                      .arg(typed.x)
                      .arg(typed.y));

            const auto click = [&onCanvas](QPointF at) {
                onCanvas(QEvent::MouseMove, at, Qt::NoButton);
                onCanvas(QEvent::MouseButtonPress, at, Qt::LeftButton);
                onCanvas(QEvent::MouseButtonRelease, at, Qt::LeftButton);
            };
            actLine_->trigger();
            QCoreApplication::processEvents();
            click(screen({19'850, -100}));
            click(screen({30'000, -10'000}));
            {
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(commandLine_, &enter);
                QCoreApplication::processEvents();
            }
            endCommand();
            const core::Point2 aimed = first_vertex_of_last();
            check(aimed == core::Point2{20'000, 0},
                  QStringLiteral("köşenin yanına fareyle tıklanan nokta köşeye yakalandı (%1, %2)")
                      .arg(aimed.x)
                      .arg(aimed.y));

            // Close in on the corner so both starts are on the picture: the typed
            // line fifteen centimetres off, the clicked one on it.
            canvas_->zoomToBox(core::Box2{19'700, -300, 20'500, 400});
            QCoreApplication::processEvents();
            shoot("yazilan-tam-tiklanan-yakalanir");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 36. A CURVE LEAVES FOR A GEOPACKAGE WITHIN A STATED ERROR (TODOS F-03) ----
        //
        // A 300 m road curve exported: the picture's 128 chords would stand nine
        // centimetres off it, and the file receives chords within the project's
        // curve tolerance instead — and the result says how far. The setting sits
        // in the project's own window, under the output topic.
        {
            const QTemporaryDir scratch;
            const QString folder = shooting ? into : scratch.path();
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line : {"AYAR koordinat_sistemi EPSG:5256", "KATMAN ad=YOL",
                                     "DAİRE merkez=485300,4310200 cevre=485600,4310200"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM"));
            const QString gpkg = folder + QStringLiteral("/egri.gpkg");
            QFile::remove(gpkg);
            transcript_->clear();
            runScriptLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(gpkg));
            {
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 20000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                QCoreApplication::processEvents();
            }
            const QString said = transcript_->toPlainText();
            check(QFileInfo::exists(gpkg) &&
                      said.contains(QStringLiteral("kirişlere kırılarak yazıldı")) &&
                      said.contains(QStringLiteral("AYAR eğri_sapması 1 mm")),
                  QStringLiteral("daire GeoPackage'a eğri sapmasıyla kırıldı ve sonuç sapmayı "
                                 "söyledi"));
            shoot("egri-sapmasi-disa-aktarma");

            SettingsDialog project(*controller_, SettingsDialog::Mode::Project, this);
            project.applyTheme(theme_);
            check(project.probeProjectSettings().contains(
                      QStringLiteral("core.aktarim.egri_sapmasi")),
                  QStringLiteral("eğri sapması proje ayarları penceresinde"));
            if (shooting) {
                project.resize(1100, 760);
                project.show();
                // Found the way a user finds it: by typing into the window's own
                // search box.
                const auto boxes = project.findChildren<QLineEdit*>();
                for (QLineEdit* box : boxes)
                    if (box->placeholderText().startsWith(QStringLiteral("Ayarlarda ara")))
                        box->setText(QStringLiteral("sapma"));
                QCoreApplication::processEvents();
                (void)project.grab().save(into + QStringLiteral("/egri-sapmasi-ayari.png"));
                project.hide();
            }
        }

        // ---- 37. WHAT THE MILLIMETRE TAKES IS SAID (TODOS F-03) ----
        //
        // The sample the sub-millimetre decision was made on: a detail drawn in
        // millimetres, finer than the store. Imported into a millimetre drawing,
        // the transcript says how many values carried detail below the storage
        // unit and how far the rounding moved them.
        {
            const QString seed = QString::fromStdString(data_root()) +
                                 QStringLiteral("/../tests/fuzz/tohum/dxf/29-milimetre-alti.dxf");
            if (!QFileInfo::exists(seed)) {
                (void)std::fprintf(stdout, "[fare] not: tohum 29 yok; milimetre altı sınanmadı\n");
            } else {
                runScriptLine(QStringLiteral("YENİ"));
                endCommand();
                runScriptLine(QStringLiteral("AYAR cizim_birimi milimetre"));
                endCommand();
                runScriptLine(QStringLiteral("AYAR koordinat_sistemi EPSG:5254"));
                endCommand();
                transcript_->clear();
                runScriptLine(QStringLiteral("İÇEAKTAR dosya=\"%1\"").arg(seed));
                {
                    QElapsedTimer waited;
                    waited.start();
                    while (controller_->session() != nullptr && controller_->session()->working() &&
                           waited.elapsed() < 20000)
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                    endCommand();
                    QCoreApplication::processEvents();
                }
                canvas_->zoomToBox(core::Box2{990, 990, 1060, 1020});
                QCoreApplication::processEvents();
                check(transcript_->toPlainText().contains(
                          QStringLiteral("4 değer milimetrenin altında ayrıntı taşıyordu")),
                      QStringLiteral("milimetre altı ayrıntı içe aktarmada sayıldı ve söylendi"));
                shoot("milimetre-alti-ayrinti");
                runScriptLine(QStringLiteral("AYAR cizim_birimi metre"));
                endCommand();
            }
        }

        // ---- 38. A RESULT SAYS IT IS OUT OF DATE (TODOS F-04) ----
        //
        // Two wells and a protection zone round each. One well is moved: its
        // zone says so at once — on the command line, on the canvas where it
        // stands, and in the panel's origin row — and the other zone does not.
        // The ribbon's dependency button counts them; accepting the moved one
        // makes it current, and the mark goes.
        {
            // A processing tool runs on a worker; its output lands when the
            // worker is done, and not a line before.
            const auto settled = [this] {
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 20000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                QCoreApplication::processEvents();
            };
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=KUYU", "NOKTA 485320,4310220", "NOKTA 485360,4310220",
                  "TAMPON nesneler=1 2 mesafe=8 birlestir=hayir katman=KORUMA"}) {
                runScriptLine(QString::fromUtf8(line));
                settled();
            }
            canvas_->zoomToBox(core::Box2{485'300'000, 4'310'200'000, 485'380'000, 4'310'240'000});
            const auto painted = [this] {
                canvas_->update();
                QCoreApplication::processEvents();
                canvas_->repaint();
                QCoreApplication::processEvents();
            };
            painted();
            check(canvas_->staleResultCountForProbe() == 0,
                  QStringLiteral("iki koruma alanı çizildiğinde ikisi de güncel"));

            transcript_->clear();
            runScriptLine(QStringLiteral("TAŞI nesneler=1 baslangic=485320,4310220 "
                                         "bitis=485324,4310222"));
            endCommand();
            painted();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON)")),
                  QStringLiteral("kuyu taşınınca koruma alanının güncel olmadığı söylendi"));
            check(canvas_->staleResultCountForProbe() == 1,
                  QStringLiteral("tuvalde yalnız taşınan kuyunun koruma alanı işaretli (%1)")
                      .arg(canvas_->staleResultCountForProbe()));
            runScriptLine(QStringLiteral("SEÇ nesneler=3"));
            attributePanel_->refresh();
            QCoreApplication::processEvents();
            check(attributePanel_->probeRowBadge(QStringLiteral("koken")) ==
                          QStringLiteral("GÜNCEL DEĞİL") &&
                      attributePanel_->probeRowValue(QStringLiteral("koken"))
                          .contains(QStringLiteral("(değişti)")),
                  QStringLiteral("panelde köken satırı GÜNCEL DEĞİL ve değişen kaynağı gösteriyor "
                                 "(\"%1\")")
                      .arg(attributePanel_->probeRowValue(QStringLiteral("koken"))));
            if (shooting) {
                attributePanel_->resize(420, 560);
                (void)attributePanel_->grab().save(into + QStringLiteral("/sonuc-panel.png"));
            }
            shoot("sonuc-guncel-degil");

            transcript_->clear();
            actDependency_->trigger();
            endCommand();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("2 sonuç: 1 güncel, 1 güncel değil, 0 kaynaksız.")),
                  QStringLiteral("şeritteki Bağımlılıklar sonuçları saydı"));
            shoot("sonuc-bagimlilik");

            runScriptLine(QStringLiteral("BAĞIMLILIK islem=kabul nesneler=3"));
            endCommand();
            painted();
            attributePanel_->refresh();
            QCoreApplication::processEvents();
            check(canvas_->staleResultCountForProbe() == 0 &&
                      attributePanel_->probeRowBadge(QStringLiteral("koken")) ==
                          QStringLiteral("GÜNCEL"),
                  QStringLiteral("kabul edilen koruma alanı güncel; tuvaldeki işaret kalktı"));
            shoot("sonuc-kabul");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 39. A LOCKED CAPTION LEFT BEHIND, AND CAUGHT UP (TODOS F-04) ----
        //
        // A parcel's edge lengths on a layer that is then locked; a corner is
        // moved. The two captions of the changed edges stay where they were —
        // the lock says so — and now they SAY they are behind: marked on the
        // canvas, GÜNCEL DEĞİL in the panel. Unlocking the layer brings them to
        // their edges in the same step, with their new lengths.
        {
            const auto settled = [this] {
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 20000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                QCoreApplication::processEvents();
            };
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=PARSEL",
                  "ALAN 485300,4310200 485340,4310200 "
                  "485340,4310230 485300,4310230",
                  "UZUNLUKYAZ nesneler=1 katman=OLCU", "KATMAN ad=OLCU kilitli=evet"}) {
                runScriptLine(QString::fromUtf8(line));
                settled();
            }
            canvas_->zoomToBox(core::Box2{485'290'000, 4'310'190'000, 485'360'000, 4'310'240'000});
            transcript_->clear();
            runScriptLine(QStringLiteral("KÖŞETAŞI nesne=1 kose=3 nokta=485348,4310230"));
            endCommand();
            canvas_->update();
            QCoreApplication::processEvents();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("Katmanın kilidi açılınca kaynağına yetişir.")),
                  QStringLiteral("kilitli yazıların geride kaldığı ve kilit açılınca yetişeceği "
                                 "söylendi"));
            check(canvas_->staleFollowerCountForProbe() == 2,
                  QStringLiteral("tuvalde değişen iki kenarın yazısı işaretli (%1)")
                      .arg(canvas_->staleFollowerCountForProbe()));
            // The caption of the moved top edge: the one that no longer says 40,00 m.
            std::int64_t behind_key = 0;
            {
                const core::Document& doc = controller_->document();
                for (const core::Tie& t : core::every_tie(doc))
                    if (t.kind == core::TieKind::Caption && t.state == core::TieState::Behind) {
                        behind_key = static_cast<std::int64_t>(core::raw(doc.key_of(t.dependent)));
                        break;
                    }
            }
            runScriptLine(QStringLiteral("SEÇ nesneler=%1").arg(behind_key));
            attributePanel_->refresh();
            QCoreApplication::processEvents();
            check(attributePanel_->probeRowBadge(QStringLiteral("bag")) ==
                      QStringLiteral("GÜNCEL DEĞİL"),
                  QStringLiteral("panelde yazının bağı GÜNCEL DEĞİL (\"%1\")")
                      .arg(attributePanel_->probeRowValue(QStringLiteral("bag"))));
            if (shooting) {
                attributePanel_->resize(420, 640);
                (void)attributePanel_->grab().save(into +
                                                   QStringLiteral("/kilitli-yazi-panel.png"));
            }
            shoot("kilitli-yazi-geride");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));

            transcript_->clear();
            runScriptLine(QStringLiteral("KATMAN ad=OLCU kilitli=hayır"));
            endCommand();
            canvas_->update();
            QCoreApplication::processEvents();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("Kilidi açılan 2 bağlı nesne kaynağına yetişti.")) &&
                      canvas_->staleFollowerCountForProbe() == 0,
                  QStringLiteral("kilit açılınca iki yazı kaynağına yetişti, işaret kalktı"));
            shoot("kilitli-yazi-yetisti");
        }

        // ---- 40. AN OUT-OF-DATE RESULT COMPUTED AGAIN, IN PLACE (TODOS F-04) ----
        //
        // A well and its protection zone, the zone given a value; the well is
        // moved. The ribbon's Güncelle runs the buffer again round the well where
        // it is now: the same object — key, value — with its new shape, current,
        // and the mark gone. One undo takes the shape back.
        {
            const auto settled = [this] {
                QElapsedTimer waited;
                waited.start();
                while (controller_->session() != nullptr && controller_->session()->working() &&
                       waited.elapsed() < 20000)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                endCommand();
                QCoreApplication::processEvents();
            };
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=KUYU", "SÜTUN kimlik=not tur=metin", "NOKTA 485320,4310220",
                  "TAMPON nesneler=1 mesafe=8 katman=KORUMA",
                  "ÖZNİTELİK ad=not nesne=2 deger=A-bolgesi",
                  "TAŞI nesneler=1 baslangic=485320,4310220 bitis=485330,4310225"}) {
                runScriptLine(QString::fromUtf8(line));
                settled();
            }
            canvas_->zoomToBox(core::Box2{485'300'000, 4'310'200'000, 485'350'000, 4'310'245'000});
            canvas_->update();
            QCoreApplication::processEvents();
            check(canvas_->staleResultCountForProbe() == 1,
                  QStringLiteral("kuyu taşınınca koruma alanı güncel değil"));
            shoot("yeniden-hesaplama-once");

            transcript_->clear();
            actDependencyRefresh_->trigger();
            settled();
            canvas_->update();
            QCoreApplication::processEvents();
            const core::Document& doc = controller_->document();
            const core::EntityId zone = doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{2}));
            const core::Box2 box =
                zone != core::kNoEntity ? doc.entities().box_of(zone) : core::Box2{};
            const core::Point2 middle{(box.min_x + box.max_x) / 2, (box.min_y + box.max_y) / 2};
            const auto note = doc.attribute(doc.attributes().find("not"), zone);
            check(transcript_->toPlainText().contains(QStringLiteral(
                      "1 sonuç kaynaklarının şimdiki hâlinden yeniden hesaplandı (TAMPON).")) &&
                      middle == core::Point2{485'330'000, 4'310'225'000} && note.ok() &&
                      note.value().text == "A-bolgesi" && canvas_->staleResultCountForProbe() == 0,
                  QStringLiteral("Güncelle koruma alanını yeni konumda, aynı nesne ve değerle "
                                 "yeniden çizdi; işaret kalktı (orta %1, %2)")
                      .arg(middle.x)
                      .arg(middle.y));
            runScriptLine(QStringLiteral("SEÇ nesneler=2"));
            attributePanel_->refresh();
            QCoreApplication::processEvents();
            check(attributePanel_->probeRowBadge(QStringLiteral("koken")) ==
                      QStringLiteral("GÜNCEL"),
                  QStringLiteral("panelde koruma alanı GÜNCEL"));
            shoot("yeniden-hesaplama-sonra");
            runScriptLine(QStringLiteral("SEÇ mod=TEMİZLE"));
        }

        // ---- 41. A SHEET TABLE WHOSE COLUMN WENT: SAID, NEVER PRINTED EMPTY (TODOS F-04) ----
        //
        // An area table on a sheet reads the parcels' `ada`. The column is
        // dropped: the drop says which sheet item read it, BAĞIMLILIK counts the
        // broken sheet tie, and the designer draws a dashed refusal where the
        // table stood — where it used to print a header over no rows.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line :
                 {"KATMAN ad=PARSEL", "SÜTUN kimlik=ada tur=tam_sayi",
                  "ALAN 485300,4310200 485340,4310200 485340,4310230 485300,4310230",
                  "ÖZNİTELİK ad=ada nesne=1 deger=101", "ÇIKTIYERLEŞİMİ islem=ekle ad=Pafta",
                  "ÇIKTIÖĞE islem=ekle tur=tablo ad=liste",
                  "ÇIKTIÖĞE islem=ayarla ad=liste metin=PARSEL sutunlar=ada"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            transcript_->clear();
            runScriptLine(QStringLiteral("SÜTUN kimlik=ada sil=evet"));
            endCommand();
            check(transcript_->toPlainText().contains(QStringLiteral(
                      "Silinen 'ada' sütununu 1 pafta öğesi okuyordu ('Pafta' ▸ 'liste')")),
                  QStringLiteral("sütun silinirken onu okuyan pafta tablosu söylendi"));
            transcript_->clear();
            actDependency_->trigger();
            endCommand();
            check(transcript_->toPlainText().contains(QStringLiteral("1 bağı kopuk.")) &&
                      transcript_->toPlainText().contains(QStringLiteral(
                          "bağı kopuk: 'Pafta' ▸ 'liste' — 'ada' sütunu çizimde yok")),
                  QStringLiteral("BAĞIMLILIK kopuk pafta bağını saydı"));
            const core::Layout* sheet = controller_->document().layouts().find("Pafta");
            bool said                 = false;
            if (sheet != nullptr)
                for (const std::string& one : core::layout_trouble(*sheet, controller_->document()))
                    said = said || one.find("'ada' sütununu yazıyor") != std::string::npos;
            check(said, QStringLiteral("ön denetim kopuk tablo sütununu söylüyor"));
            if (shooting) {
                LayoutDesigner designer(*controller_, QStringLiteral("Pafta"), this);
                designer.applyTheme(theme_);
                designer.aimAt(controller_->document().extent());
                designer.resize(1400, 900);
                designer.show();
                for (int i = 0; i < 4; ++i)
                    QCoreApplication::processEvents();
                (void)designer.grab().save(into + QStringLiteral("/pafta-kopuk-tablo.png"));
                designer.close();
            }
            shoot("pafta-kopuk-bag");
        }

        // ---- 42. AN EXPORT THAT COULD NOT MOVE WHOLE SAYS WHICH FILES DID (TODOS F-05) ----
        //
        // A DXF and its `.prj` are written into a staging directory beside the
        // target and moved together. A directory standing where the `.prj` goes
        // refuses its move, the way a file held open by another program does on
        // Windows: the drawing is replaced, the `.prj` is not, and the answer
        // names both instead of saying "Dışa aktarıldı". Nothing is left behind.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            runScriptLine(QStringLiteral("AYAR core.crs.id EPSG:5254"));
            endCommand();
            runScriptLine(QStringLiteral("KATMAN ad=YOL"));
            endCommand();
            for (int i = 0; i < 40; ++i) {
                const int x = 485300 + (i * 2);
                runScriptLine(QStringLiteral("ÇİZGİ %1,4310200 %2,4310230").arg(x).arg(x + 1));
                endCommand();
            }
            canvas_->zoomToBox(core::Box2{485'290'000, 4'310'170'000, 485'390'000, 4'310'240'000});
            const QString dir = QDir::temp().filePath(QStringLiteral("kentos-f05-probe"));
            QDir(dir).removeRecursively();
            QDir().mkpath(dir);
            const QString dxf       = dir + QStringLiteral("/pafta.dxf");
            const QString prj       = dir + QStringLiteral("/pafta.prj");
            const auto staging_left = [&dir] {
                return !QDir(dir)
                            .entryList(QStringList{QStringLiteral(".kentos-*")},
                                       QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)
                            .isEmpty();
            };

            transcript_->clear();
            runScriptLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(dxf));
            endCommand();
            check(QFileInfo(dxf).isFile() && QFileInfo(prj).isFile() && !staging_left(),
                  QStringLiteral("DXF ve .prj birlikte yerine kondu, hazırlık kalmadı"));
            const qint64 first_size = QFileInfo(dxf).size();

            // The `.prj`'s place taken by a directory.
            QFile::remove(prj);
            QDir().mkpath(prj + QStringLiteral("/dolu"));
            runScriptLine(QStringLiteral("ÇİZGİ 485300,4310180 485380,4310180"));
            endCommand();
            transcript_->clear();
            runScriptLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(dxf));
            endCommand();
            const QString said = transcript_->toPlainText();
            check(said.contains(QStringLiteral("yerine tam konamadı")) &&
                      said.contains(QStringLiteral("Yerine konan: ") + dxf) &&
                      said.contains(QStringLiteral("Konamayan: ") + prj +
                                    QStringLiteral(" (yerinde aynı adlı bir klasör var)")),
                  QStringLiteral("yarım taşıma dosya dosya söylendi"));
            check(!said.contains(QStringLiteral("Dışa aktarıldı")),
                  QStringLiteral("yarım taşıma başarı sayılmadı"));
            check(QFileInfo(dxf).size() > first_size && QFileInfo(prj).isDir() && !staging_left(),
                  QStringLiteral("çizim yenilendi, klasöre dokunulmadı, hazırlık kalmadı"));
            shoot("disa-aktarim-yarim");

            // The directory gone, the same export again: the whole set, once.
            QDir(prj).removeRecursively();
            transcript_->clear();
            runScriptLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(dxf));
            endCommand();
            check(transcript_->toPlainText().contains(QStringLiteral("Dışa aktarıldı")) &&
                      QFileInfo(prj).isFile() && !staging_left(),
                  QStringLiteral("yineleyince takım bütün olarak yerine kondu"));
            shoot("disa-aktarim-tamam");
            QDir(dir).removeRecursively();
        }

        // ---- 43. ONE LOGICAL OPERATION: 500 PARCELS, THEIR VALUES AND CAPTIONS (TODOS F-05) ----
        //
        // A script moves 500 parcels and re-values them; their captions follow.
        // It is one step, and the answer says what the one step changed. One
        // GERİAL takes all of it back and says what it did. A script that fails
        // at its 251st command leaves nothing: the drawing, the undo stack and
        // what YİNELE would bring back are exactly as they were.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            const QString dir = QDir::temp().filePath(QStringLiteral("kentos-f05-tek-islem"));
            QDir(dir).removeRecursively();
            QDir().mkpath(dir);
            const auto write = [&dir](const QString& name, const std::string& text) {
                QFile f(dir + QLatin1Char('/') + name);
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    f.write(text.data(), static_cast<qint64>(text.size()));
                return dir + QLatin1Char('/') + name;
            };
            std::string setup  = R"({"ad": "Kurulum", "komutlar": [)";
            std::string moves  = R"({"ad": "Kaydırma", "komutlar": [)";
            std::string broken = R"({"ad": "Yarım", "komutlar": [)";
            for (int i = 0; i < 500; ++i) {
                const std::int64_t x = 485'300'000 + (std::int64_t{i % 25} * 12'000);
                const std::int64_t y = 4'310'200'000 + (std::int64_t{i / 25} * 12'000);
                setup += (i == 0 ? "" : ",");
                setup += R"({"cmd": "core.area", "args": {"noktalar": [[)" + std::to_string(x) +
                         "," + std::to_string(y) + "],[" + std::to_string(x + 10000) + "," +
                         std::to_string(y) + "],[" + std::to_string(x + 10000) + "," +
                         std::to_string(y + 10000) + "],[" + std::to_string(x) + "," +
                         std::to_string(y + 10000) + "]]}}";
                setup += R"(,{"cmd": "core.attribute", "args": {"ad": "ada", "nesne": )" +
                         std::to_string(i + 1) + R"(, "deger": ")" + std::to_string(100 + i) +
                         R"("}})";
                const std::string move =
                    R"({"cmd": "core.move", "args": {"nesneler": [)" + std::to_string(i + 1) +
                    R"(], "baslangic": [0, 0], "bitis": [4000, 2000]}},)" +
                    R"({"cmd": "core.attribute", "args": {"ad": "ada", "nesne": )" +
                    std::to_string(i + 1) + R"(, "deger": ")" + std::to_string(1000 + i) + R"("}})";
                moves += (i == 0 ? "" : ",") + move;
                broken += (i == 0 ? "" : ",") + move;
                if (i == 249) broken += R"(,{"cmd": "core.erase", "args": {"nesneler": [999999]}})";
            }
            setup += "]}";
            moves += "]}";
            broken += "]}";
            runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
            endCommand();
            runScriptLine(QStringLiteral("SÜTUN kimlik=ada tur=tam_sayi"));
            endCommand();
            runScriptLine(
                QStringLiteral("BETİK \"%1\"").arg(write(QStringLiteral("kurulum.json"), setup)));
            endCommand();
            runScriptLine(QStringLiteral("ETİKET katman=PARSEL bicim=\"{ada}\" yukseklik=2500"));
            endCommand();
            canvas_->zoomToBox(core::Box2{485'295'000, 4'310'195'000, 485'610'000, 4'310'450'000});
            const std::uint64_t hash_before = controller_->document().content_hash();
            const std::size_t undo_before   = controller_->bus().undo_stack().undo_depth();

            transcript_->clear();
            runScriptLine(
                QStringLiteral("BETİK \"%1\"").arg(write(QStringLiteral("kaydirma.json"), moves)));
            endCommand();
            const QString said = transcript_->toPlainText();
            check(said.contains(QStringLiteral(
                      "Kaydırma: 1000 komut, tek geri alma adımı — 1000 nesnenin yeri ya da "
                      "biçimi, 500 yazının metni ve 500 nesnenin öznitelik değeri değişti.")),
                  QStringLiteral("betik tek adımının ne değiştirdiğini söyledi"));
            check(controller_->bus().undo_stack().undo_depth() == undo_before + 1,
                  QStringLiteral("500 parsellik betik tek geri alma adımı"));
            const std::uint64_t moved = controller_->document().content_hash();
            shoot("tek-islem-betik");

            transcript_->clear();
            runScriptLine(QStringLiteral("GERİAL"));
            endCommand();
            check(transcript_->toPlainText().contains(QStringLiteral(
                      "Geri almayla 1000 nesnenin yeri ya da biçimi, 500 yazının metni ve 500 "
                      "nesnenin öznitelik değeri değişti.")) &&
                      controller_->document().content_hash() == hash_before,
                  QStringLiteral("tek GERİAL hepsini geri aldı ve ne yaptığını söyledi"));
            shoot("tek-islem-geri");
            runScriptLine(QStringLiteral("YİNELE"));
            endCommand();
            check(controller_->document().content_hash() == moved,
                  QStringLiteral("YİNELE betiğin bıraktığı hâle döndü"));
            runScriptLine(QStringLiteral("GERİAL"));
            endCommand();

            const std::size_t redo_before = controller_->bus().undo_stack().redo_depth();
            transcript_->clear();
            runScriptLine(
                QStringLiteral("BETİK \"%1\"").arg(write(QStringLiteral("yarim.json"), broken)));
            endCommand();
            check(transcript_->toPlainText().contains(
                      QStringLiteral("Betik hatası: Betik satırı 501 (core.erase)")),
                  QStringLiteral("yarıda kalan betik hata olarak söylendi"));
            check(
                controller_->document().content_hash() == hash_before &&
                    controller_->bus().undo_stack().undo_depth() == undo_before &&
                    controller_->bus().undo_stack().redo_depth() == redo_before,
                QStringLiteral("yarıda kalan betik çizimde, geri alma ve yinelemede iz bırakmadı"));
            shoot("tek-islem-yarim");
            QDir(dir).removeRecursively();
        }

        // ---- 44. A SUGGESTION SHOWS WHAT IT WOULD LEAVE BEFORE ANYBODY APPLIES IT (TODOS F-05)
        // ----
        //
        // An outside client proposes buffering a parcel and erasing a line. The
        // card says what applying it would do — counted by running it and taking
        // it back — and the canvas draws the result dashed: the buffer in the
        // accent ink, the line to be erased in the danger ink. The drawing, its
        // revision and its keys are exactly as they were, so the suggestion is
        // still composed against it; rejecting it takes the ghosts away.
        {
            runScriptLine(QStringLiteral("YENİ"));
            endCommand();
            for (const char* line : {"KATMAN ad=PARSEL",
                                     "ALAN 485300,4310200 485340,4310200 485340,4310230 "
                                     "485300,4310230",
                                     "ÇİZGİ 485300,4310250 485340,4310250"}) {
                runScriptLine(QString::fromUtf8(line));
                endCommand();
            }
            canvas_->zoomToBox(core::Box2{485'280'000, 4'310'180'000, 485'360'000, 4'310'265'000});
            const std::uint64_t hash_before     = controller_->document().content_hash();
            const std::uint64_t revision_before = controller_->document().revision();

            ai::Plan plan;
            plan.requester = "Sınama istemcisi";
            plan.revision  = revision_before;
            for (const char* line : {"TAMPON nesneler=1 mesafe=6 katman=BANT", "SİL nesneler=2"}) {
                auto inv = controller_->bus().parse_invocation(line, command::Origin::Ai);
                if (!inv) {
                    check(
                        false,
                        QStringLiteral("öneri satırı okunamadı: %1").arg(QString::fromUtf8(line)));
                    continue;
                }
                ai::PlanStep step;
                step.command_id = inv.value().name;
                step.args       = inv.value().args;
                step.line       = line;
                plan.steps.push_back(std::move(step));
            }
            auto filed = controller_->aiService().propose(std::move(plan));
            check(filed.ok(), QStringLiteral("öneri açıldı"));
            for (int i = 0; i < 6; ++i)
                QCoreApplication::processEvents();
            SuggestionCard* card = nullptr;
            for (SuggestionCard* one : chatPanel_->findChildren<SuggestionCard*>())
                if (filed && one->planId() == QString::fromStdString(filed.value())) card = one;
            check(card != nullptr, QStringLiteral("önerinin kartı açıldı"));
            const QString would = card != nullptr ? card->previewTextForProbe() : QString();
            check(
                would.contains(QStringLiteral("Uygulanırsa: 1 nesne eklenecek; 1 nesne silinecek")),
                QStringLiteral("kart uygulanırsa ne olacağını söyledi: %1").arg(would));
            const auto [made, gone] = canvas_->previewGhostCountsForProbe();
            check(made == 1 && gone == 1,
                  QStringLiteral("tuval sonucu kesikli çizdi (%1 oluşacak, %2 silinecek)")
                      .arg(made)
                      .arg(gone));
            check(controller_->document().content_hash() == hash_before &&
                      controller_->document().revision() == revision_before &&
                      controller_->document().find_layer("BANT") == core::kNoLayer,
                  QStringLiteral("önizleme çizime, sürüme ve katmanlara dokunmadı"));
            shoot("onizleme-oneri");
            // THE CARD WHOLE, as the dock shows it scrolled to its lines.
            if (shooting && card != nullptr)
                (void)card->grab().save(into + QStringLiteral("/onizleme-kart.png"));

            // The same answer on the command line, for any line.
            transcript_->clear();
            runScriptLine(QStringLiteral("ÖNİZLE komut=\"TAMPON nesneler=1 mesafe=6 katman=BANT\" "
                                         "komut=\"SİL nesneler=2\""));
            endCommand();
            check(transcript_->toPlainText().contains(QStringLiteral(
                      "Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek; 1 nesne silinecek. "
                      "Çizim değişmedi.")),
                  QStringLiteral("ÖNİZLE aynı cevabı verdi"));
            shoot("onizleme-komut");

            // Rejected at the card — the one place a decision is made — the
            // ghosts go with it.
            if (card != nullptr)
                check(card->probeReject().ok(), QStringLiteral("öneri reddedildi"));
            for (int i = 0; i < 4; ++i)
                QCoreApplication::processEvents();
            const auto [made_after, gone_after] = canvas_->previewGhostCountsForProbe();
            check(made_after == 0 && gone_after == 0,
                  QStringLiteral("reddedilen önerinin taslakları kalktı"));
        }
    }

    (void)std::fprintf(stdout, "[fare] %d kusur\n", failures);
    return failures;
}

void MainWindow::offerRemedy(const QString& message, const QString& remedy)
{
    if (remedyBanner_ == nullptr || remedy.isEmpty()) return;
    // The button says what it will do: the title of the command it runs.
    const QString word               = remedy.section(QLatin1Char(' '), 0, 0);
    const command::CommandSpec* spec = controller_->bus().registry().resolve(word.toStdString());
    remedyButton_->setText(
        spec != nullptr && !spec->title.empty() ? QString::fromStdString(spec->title) : word);
    remedyBanner_->setText(message);
    remedy_ = remedy;
    remedyBanner_->setVisible(true);
}

QString MainWindow::remedyForProbe() const
{
    return remedyBanner_ != nullptr && remedyBanner_->isVisible() ? remedy_ : QString();
}

bool MainWindow::pressRemedyForProbe()
{
    if (remedyForProbe().isEmpty()) return false;
    remedyButton_->click();
    return true;
}

void MainWindow::onCommandFinished(const QString& id, const QString& report)
{
    // A FILE JUST ATTACHED is shown where it can be managed: the references
    // tab comes up beside the layers its file brought.
    if (id == QLatin1String("core.xref")) {
        auto said              = core::Json::parse(report.toStdString());
        const core::Json* step = said ? said.value().find("islem") : nullptr;
        if (step != nullptr && step->as_string() == "ekle" && layerHeader_ != nullptr) {
            layerHeader_->setCurrent(1);
            if (layerDock_ != nullptr) layerDock_->show();
        }
        return;
    }
    if (id != QLatin1String("core.block_edit")) return;
    auto parsed = core::Json::parse(report.toStdString());
    if (!parsed) return;
    const core::Json& said  = parsed.value();
    const core::Json* step  = said.find("islem");
    const core::Json* block = said.find("blok");
    if (step == nullptr || block == nullptr) return;
    if (step->as_string() != "ac") {
        if (blockEdit_) dormantBlockEdit_ = std::move(blockEdit_);
        blockEdit_.reset();
        refreshBlockEdit();
        return;
    }

    BlockEditSession edit;
    edit.block = QString::fromStdString(block->as_string());
    if (const core::Json* ref = said.find("referans"); ref != nullptr)
        edit.reference = ref->as_int();
    if (const core::Json* made = said.find("parcalar"); made != nullptr)
        for (const core::Json& k : made->as_array())
            edit.opened.push_back(k.as_int());
    // EVERYTHING DRAWN FROM HERE ON BELONGS TO THE EDIT, the way AutoCAD's
    // reference edit takes what is drawn while it is open: keys only grow, so
    // the largest one now is the line.
    const core::Document& doc = controller_->document();
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        edit.watermark = std::max<std::uint64_t>(edit.watermark, core::raw(doc.key_of(e)));
    blockEdit_ = std::move(edit);
    dormantBlockEdit_.reset();
    SARibbonBar* bar = ribbonBar();
    if (bar != nullptr && !beforeBlockEdit_)
        beforeBlockEdit_ = bar->categoryByIndex(bar->currentIndex());
    refreshBlockEdit();
    if (bar != nullptr && blockEditTab_ != nullptr)
        if (SARibbonCategory* page = blockEditTab_->categoryPage(0); page != nullptr)
            bar->raiseCategory(page);
    statusBar()->showMessage(
        tr("'%1' bloğu düzenleniyor — bitirince Bloğu Kaydet ya da Vazgeç").arg(blockEdit_->block),
        8000);
}

void MainWindow::refreshBlockEdit()
{
    const core::Document& doc = controller_->document();
    // AN EDIT IS OPEN while what its open made is on the sheet — and, opened
    // from a reference, while that reference is hidden. An undo past the open
    // ends it; an undo of the save, or a redo of the open, brings it back.
    const auto still_open = [&doc](const BlockEditSession& edit) {
        bool any = false;
        for (const std::int64_t k : edit.opened) {
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(k)));
            if (e != core::kNoEntity && doc.alive(e)) any = true;
        }
        if (edit.reference == 0) return any;
        const core::EntityId r =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(edit.reference)));
        return any && r != core::kNoEntity && doc.alive(r) &&
               (doc.entities().flags[r] & core::FlagHidden) != 0;
    };
    if (blockEdit_ && !still_open(*blockEdit_)) {
        dormantBlockEdit_ = std::move(blockEdit_);
        blockEdit_.reset();
    } else if (!blockEdit_ && dormantBlockEdit_ && still_open(*dormantBlockEdit_)) {
        blockEdit_ = std::move(dormantBlockEdit_);
        dormantBlockEdit_.reset();
    }
    const bool editing = blockEdit_.has_value();
    if (SARibbonBar* bar = ribbonBar(); bar != nullptr && blockEditTab_ != nullptr) {
        if (editing)
            if (SARibbonCategory* page = blockEditTab_->categoryPage(0); page != nullptr)
                page->setCategoryName(tr("Blok: %1").arg(blockEdit_->block));
        if (bar->isContextCategoryVisible(blockEditTab_) != editing) {
            bar->setContextCategoryVisible(blockEditTab_, editing);
            // THE EDIT OVER, the hand goes back to the tab it was on — not
            // to whichever tab the bar falls on when one goes away.
            if (!editing) {
                if (beforeBlockEdit_ != nullptr)
                    bar->raiseCategory(beforeBlockEdit_);
                else
                    bar->setCurrentIndex(0);
                beforeBlockEdit_.clear();
            }
        }
    }
}

QString MainWindow::blockEditLine(bool save) const
{
    if (!blockEdit_) return {};
    const core::Document& doc = controller_->document();
    QString line              = QStringLiteral("BLOKDÜZENLE islem=%1")
                       .arg(save ? QStringLiteral("kaydet") : QStringLiteral("vazgec"));
    if (blockEdit_->reference != 0)
        line += QStringLiteral(" nesne=%1").arg(blockEdit_->reference);
    else
        line += QStringLiteral(" ad=\"%1\"").arg(blockEdit_->block);
    // What the open made and is still there, then what was drawn since.
    std::vector<std::int64_t> objects;
    for (const std::int64_t k : blockEdit_->opened) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(k)));
        if (e != core::kNoEntity && doc.alive(e)) objects.push_back(k);
    }
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.entities().standalone(e)) continue;
        const std::uint64_t k = core::raw(doc.key_of(e));
        if (k <= blockEdit_->watermark) continue;
        if (static_cast<std::int64_t>(k) == blockEdit_->reference) continue;
        objects.push_back(static_cast<std::int64_t>(k));
    }
    for (const std::int64_t k : objects)
        line += QStringLiteral(" nesneler=%1").arg(k);
    return line;
}

bool MainWindow::settleBlockEdit(const QString& why)
{
    refreshBlockEdit();
    if (!blockEdit_) return true;
    // THREE ANSWERS, as `confirmDiscard` gives: save the block, give the edit
    // up, or stay — a hand that reached for the wrong control must be able to
    // say it did not mean it.
    QMessageBox ask(QMessageBox::Question, tr("Blok düzenleniyor"),
                    tr("'%1' bloğunun düzenlemesi açık. %2 bloğu kaydedin ya da düzenlemeden "
                       "vazgeçin.")
                        .arg(blockEdit_->block, why),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    ask.button(QMessageBox::Save)->setText(tr("Bloğu Kaydet"));
    ask.button(QMessageBox::Discard)->setText(tr("Düzenlemeden Vazgeç"));
    ask.button(QMessageBox::Cancel)->setText(tr("İptal"));
    ask.setDefaultButton(QMessageBox::Save);
    const int answer = ask.exec();
    if (answer != QMessageBox::Save && answer != QMessageBox::Discard) return false;
    controller_->runLine(blockEditLine(answer == QMessageBox::Save), command::Origin::Gui);
    refreshBlockEdit();
    return !blockEdit_.has_value();
}

QImage MainWindow::probePicture()
{
    QImage picture = grab().toImage();
    if (const QImage live = canvas_->grabCanvas(); !live.isNull() && !picture.isNull()) {
        QPainter painter(&picture);
        painter.drawImage(QRect(canvas_->mapTo(this, QPoint(0, 0)), canvas_->size()), live);
        // AND WHAT STANDS ON THE CANVAS: its own child widgets — the caption box
        // a double click opens — which the drawn frame above has just covered.
        for (QWidget* child : canvas_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
            if (child->isVisible())
                painter.drawPixmap(canvas_->mapTo(this, child->pos()), child->grab());
    }
    return picture;
}

int MainWindow::probeOsClicks()
{
    const QString into = QString::fromLocal8Bit(qgetenv("KENTOS_OSCLICK_PROBE"));
    QDir().mkpath(into);
    bool given  = false;
    int seconds = qEnvironmentVariableIntValue("KENTOS_OSCLICK_SECONDS", &given);
    if (!given || seconds <= 0) seconds = 60;

    const auto say = [](const QString& line) {
        (void)std::fprintf(stdout, "[os] %s\n", line.toUtf8().constData());
        (void)std::fflush(stdout);
    };
    const auto centre = [](const QWidget* w) {
        return w->mapToGlobal(QPoint(w->width() / 2, w->height() / 2));
    };
    const auto name_of = [](const QAction* action) {
        if (action == nullptr) return QStringLiteral("-");
        const QString word = action->property(kToolCommand).toString();
        return word.isEmpty() ? action->text() : word;
    };

    // WHERE EVERYTHING IS, in the coordinates the window system clicks in: the
    // `Giriş` tab's buttons, the one the window opens on. A family's split button
    // is given twice: its face, which runs the member used last, and its arrow,
    // which opens the list.
    const QRect frame = frameGeometry();
    say(QStringLiteral("pencere %1 %2 %3 %4")
            .arg(frame.x())
            .arg(frame.y())
            .arg(frame.width())
            .arg(frame.height()));
    if (SARibbonBar* bar = ribbonBar(); bar != nullptr) bar->setCurrentIndex(0);
    QCoreApplication::processEvents();
    for (QToolButton* button : ribbonButtons()) {
        if (!button->isVisible()) continue;
        QAction* face   = button->defaultAction();
        const QPoint at = button->mapToGlobal(
            QPoint(button->width() / 2, static_cast<int>(button->height() * 0.3)));
        if (button->popupMode() == QToolButton::MenuButtonPopup) {
            const QPoint arrow =
                button->mapToGlobal(QPoint(button->width() / 2, button->height() - 4));
            say(QStringLiteral("aile %1 %2 %3 ok %4 %5")
                    .arg(name_of(face))
                    .arg(at.x())
                    .arg(at.y())
                    .arg(arrow.x())
                    .arg(arrow.y()));
        } else {
            say(QStringLiteral("dugme %1 %2 %3").arg(name_of(face)).arg(at.x()).arg(at.y()));
        }
    }
    const QPoint canvas_at = canvas_->mapToGlobal(QPoint(0, 0));
    say(QStringLiteral("tuval %1 %2 %3 %4")
            .arg(canvas_at.x())
            .arg(canvas_at.y())
            .arg(canvas_->width())
            .arg(canvas_->height()));
    const QPoint line_at = centre(commandLine_);
    say(QStringLiteral("komut-satiri %1 %2").arg(line_at.x()).arg(line_at.y()));

    // WHAT THE ACTION ITSELF DID. An exclusive `QActionGroup` checks an action
    // when it is triggered, so a lit button does NOT prove the command ran: the
    // light can come from Qt while `runCommand` never fired. The signal is
    // listened to directly, so the two can be told apart in the log.
    const auto action_name = [](const QAction* action) {
        const QString word = action->property(kToolCommand).toString();
        return word.isEmpty() ? action->text() : word;
    };
    for (QAction* action : drawingTools_->actions()) {
        connect(action, &QAction::triggered, this, [say, action_name, action] {
            say(QStringLiteral("tetiklendi %1").arg(action_name(action)));
        });
        // `toggled` fires for `setChecked` as well as for `trigger`, so a check
        // that arrives WITHOUT a trigger is visible as one line and not the other.
        connect(action, &QAction::toggled, this, [say, action_name, action](bool on) {
            if (on) say(QStringLiteral("isaretlendi %1").arg(action_name(action)));
        });
    }

    // AND WHETHER THE BUTTON GOT THE CLICK AT ALL. Everything above watches what
    // the program did; this watches what the window system delivered, which is
    // the half no synthesised probe can be wrong about in the same way.
    struct Watch : QObject
    {
        std::function<void(const QString&)> say;

        bool eventFilter(QObject* o, QEvent* e) override
        {
            const char* what = nullptr;
            switch (e->type()) {
            case QEvent::MouseButtonPress: what = "bas"; break;
            case QEvent::MouseButtonRelease: what = "birak"; break;
            case QEvent::MouseButtonDblClick: what = "cift"; break;
            case QEvent::Enter: what = "gir"; break;
            default: break;
            }
            if (what != nullptr) {
                QString who = QString::fromLatin1(o->metaObject()->className());
                if (const auto* b = qobject_cast<QToolButton*>(o); b != nullptr)
                    if (const QAction* face = b->defaultAction(); face != nullptr) {
                        const QString word = face->property(kToolCommand).toString();
                        who += QLatin1Char('/') + (word.isEmpty() ? face->text() : word);
                    }
                QString where;
                if (e->type() != QEvent::Enter) {
                    const auto* m = static_cast<QMouseEvent*>(e);
                    where         = QStringLiteral(" @%1,%2")
                                .arg(qRound(m->globalPosition().x()))
                                .arg(qRound(m->globalPosition().y()));
                }
                say(QStringLiteral("olay %1 %2%3").arg(QLatin1String(what), who, where));
            }
            return false;
        }
    };

    Watch watch;
    watch.say = say;
    // ON THE APPLICATION, not on the buttons: the question is WHERE a real click
    // lands, and a filter on what we expect it to land on cannot answer that.
    qApp->installEventFilter(&watch);

    QString lit_before;
    QString prompt_before;
    QString state_before;
    std::size_t count_before    = controller_->document().live_entity_count();
    qsizetype transcript_before = transcript_->toPlainText().size();
    const QWidget* card_before  = nullptr;
    say(QStringLiteral("nesne %1").arg(count_before));
    say(QStringLiteral("hazır"));

    // THEN WATCH. Nothing here sends an event; the loop only pumps what the
    // window system delivers and says what changed, so that the driver's log
    // and this one can be read side by side.
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < seconds * 1000 && !QFile::exists(into + QStringLiteral("/dur"))) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        const QString lit_now = name_of(drawingTools_->checkedAction());
        if (lit_now != lit_before) {
            say(QStringLiteral("yanan %1").arg(lit_now));
            lit_before = lit_now;
        }

        const command::Session* live = controller_->session();

        // The session as three facts, because "the button lit" is not one of
        // them: which command, whether it is waiting for an answer, whether a
        // worker holds it.
        const QString state_now = live == nullptr
                                      ? QStringLiteral("-")
                                      : QStringLiteral("%1 bekliyor=%2 calisiyor=%3")
                                            .arg(QString::fromStdString(live->spec().id))
                                            .arg(live->waiting() ? 1 : 0)
                                            .arg(live->working() ? 1 : 0);
        if (state_now != state_before) {
            say(QStringLiteral("oturum %1").arg(state_now));
            state_before = state_now;
        }

        const QString prompt_now = live != nullptr && live->waiting()
                                       ? QString::fromStdString(live->prompt().message)
                                       : QString();
        if (prompt_now != prompt_before) {
            say(prompt_now.isEmpty() ? QStringLiteral("istem -")
                                     : QStringLiteral("istem %1").arg(prompt_now));
            prompt_before = prompt_now;
        }

        const std::size_t count_now = controller_->document().live_entity_count();
        if (count_now != count_before) {
            say(QStringLiteral("nesne %1").arg(count_now));
            count_before = count_now;
        }

        const QString text = transcript_->toPlainText();
        if (text.size() > transcript_before) {
            for (const QString& line : text.mid(transcript_before).split(QLatin1Char('\n')))
                if (!line.trimmed().isEmpty()) say(QStringLiteral("döküm %1").arg(line));
            transcript_before = text.size();
        }

        QWidget* popup = QApplication::activePopupWidget();
        QWidget* card =
            popup != nullptr && popup->property("kentos.rows").isValid() ? popup : nullptr;
        if (card != nullptr && card != card_before) {
            const QPoint first = card->mapToGlobal(card->property("kentos.rowCentre").toPoint());
            say(QStringLiteral("kart %1 satır, ilk %2 %3, adım %4")
                    .arg(card->property("kentos.rows").toInt())
                    .arg(first.x())
                    .arg(first.y())
                    .arg(card->property("kentos.rowPitch").toInt()));
        }
        card_before = card;
    }

    (void)probePicture().save(into + QStringLiteral("/os-son.png"));
    say(QStringLiteral("bitti nesne=%1 yanan=%2").arg(count_before).arg(lit_before));
    return 0;
}

int MainWindow::probeAccessible()
{
    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[erişim] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        (void)std::fflush(stdout);
        if (!ok) ++failures;
    };

    const QString press  = QAccessibleActionInterface::pressAction();
    const QString toggle = QAccessibleActionInterface::toggleAction();

    const auto name_of = [](const QAction* action) {
        const QString word = action->property(kToolCommand).toString();
        return word.isEmpty() ? action->text() : word;
    };

    // NOTHING ARMED, AND NOT THIS ACTION. An exclusive `QActionGroup` refuses to
    // re-trigger the action that is already checked, so pressing a lit tool would
    // report a failure that is Qt's rule rather than this program's defect. The
    // group is parked somewhere else before every press.
    const auto park = [this](const QAction* notThis) {
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        controller_->cancelInteractive();
        QAction* elsewhere = notThis == actSelect_ ? actLine_ : actSelect_;
        elsewhere->setChecked(true);
        QCoreApplication::processEvents();
    };

    // The tools a press ARMS — the draw, edit and query ones and the three that
    // pick and pan. The rest of the ribbon opens windows and dialogs; the tree
    // is asked about them too, but a press that would open a file dialog is not
    // made (it would hold the probe until somebody closed it).
    const auto arms = [this](const QAction* a) {
        if (a == actSelect_ || a == actSelectArea_ || a == actPan_) return true;
        QString line = a->property(kToolCommand).toString();
        if (line.isEmpty() && a->objectName().startsWith(QStringLiteral("toolAction.")))
            line = a->objectName().section(QLatin1Char('.'), 1);
        const command::CommandSpec* spec =
            line.isEmpty() ? nullptr
                           : controller_->registry().resolve(
                                 line.section(QLatin1Char(' '), 0, 0).toStdString());
        return spec != nullptr && (spec->category == command::Category::Draw ||
                                   spec->category == command::Category::Modify ||
                                   spec->category == command::Category::Query);
    };
    // A family's head is pressed for its face.
    const auto faceOf = [this](QAction* carried) -> QAction* {
        for (const RibbonFamily* f : std::as_const(families_))
            if (f->head() == carried) return f->face();
        return carried;
    };

    // ---- 1. WHAT THE TREE SAYS ABOUT EVERY BUTTON --------------------------
    //
    // A screen reader reads the accessibility tree, not the pixels: every
    // ribbon button must be in it with a name, a description (its tip), the
    // press it offers — and, for a tool, whether it is the one lit.
    int buttons = 0;
    for (QToolButton* button : ribbonButtons()) {
        QAction* carried  = button->defaultAction();
        QAction* face     = faceOf(carried);
        const QString who = name_of(face);
        ++buttons;

        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(button);
        check(iface != nullptr, QStringLiteral("%1: erişilebilirlik arayüzü var").arg(who));
        if (iface == nullptr) continue;
        check(!iface->text(QAccessible::Name).trimmed().isEmpty(),
              QStringLiteral("%1: erişilebilir adı var").arg(who));
        check(!iface->text(QAccessible::Description).trimmed().isEmpty() ||
                  !button->toolTip().trimmed().isEmpty(),
              QStringLiteral("%1: erişilebilir açıklaması var").arg(who));

        QAccessibleActionInterface* actions = iface->actionInterface();
        check(actions != nullptr, QStringLiteral("%1: eylem arayüzü var").arg(who));
        if (actions == nullptr) continue;
        check(actions->actionNames().contains(press),
              QStringLiteral("%1: \"%2\" eylemi sunuluyor").arg(who, press));

        if (!arms(face)) continue;
        if (button->isCheckable()) {
            park(face);
            face->setChecked(true);
            QCoreApplication::processEvents();
            check(iface->state().checkable && iface->state().checked,
                  QStringLiteral("%1: yanan düğme ağaçta da işaretli").arg(who));
        }

        // AND THE PRESS IT OFFERS RUNS THE TOOL — the one road a switch user or
        // a screen reader has to it.
        park(face);
        int fired = 0;
        const QMetaObject::Connection on =
            connect(face, &QAction::triggered, this, [&fired] { ++fired; });
        actions->doAction(press);
        QCoreApplication::processEvents();
        disconnect(on);
        check(fired > 0, QStringLiteral("%1: \"%2\" komutu çalıştırdı").arg(who, press));
        if (button->isCheckable() && actions->actionNames().contains(toggle))
            check(true, QStringLiteral("%1: \"%2\" eylemi sunuluyor").arg(who, toggle));
    }
    check(buttons > 0, QStringLiteral("şeritte %1 düğme ağaçta").arg(buttons));

    // ---- 2. THE KEYBOARD'S ROAD ---------------------------------------------
    //
    // Tab reaches a ribbon button, Space presses it, and on a split button the
    // Down arrow opens its list (`.claude/ui.md` R21): a family's other members
    // are not only the mouse's.
    park(nullptr);
    // THE CIRCLE'S FAMILY, a split button on the tab the window opens on.
    QToolButton* line = ribbonButton(actCircle_);
    check(line != nullptr && line->defaultAction()->menu() != nullptr,
          QStringLiteral("Daire ailesinin bölünmüş düğmesi şeritte"));
    if (line != nullptr && line->defaultAction()->menu() == nullptr) line = nullptr;
    if (line != nullptr) {
        // THE WINDOW HAS TO BE THE ACTIVE ONE for anything in it to hold the
        // focus, and an offscreen run activates nothing by itself.
        activateWindow();
        QCoreApplication::processEvents();
        line->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        check(line->hasFocus() && line->focusPolicy() != Qt::NoFocus,
              QStringLiteral("şerit düğmesi Tab ile odak alıyor"));

        const auto key = [line](int code) {
            QKeyEvent down(QEvent::KeyPress, code, Qt::NoModifier);
            QCoreApplication::sendEvent(line, &down);
            QKeyEvent up(QEvent::KeyRelease, code, Qt::NoModifier);
            QCoreApplication::sendEvent(line, &up);
            QCoreApplication::processEvents();
        };

        QAction* face = faceOf(line->defaultAction());
        int fired     = 0;
        const QMetaObject::Connection on =
            connect(face, &QAction::triggered, this, [&fired] { ++fired; });
        key(Qt::Key_Space);
        disconnect(on);
        check(fired > 0,
              QStringLiteral("Boşluk odaktaki şerit düğmesini bastı (%1)").arg(name_of(face)));

        park(nullptr);
        line->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        // Opened asynchronously and closed at once: a menu's `exec` would wait
        // for a hand that is not there.
        QTimer::singleShot(0, this, [] {
            if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) {
                (void)std::fprintf(stdout, "[erişim] açılan liste: %d satır\n",
                                   static_cast<int>(open->actions().size()));
                open->close();
            }
        });
        bool opened                         = false;
        const QMetaObject::Connection shown = connect(
            line->defaultAction()->menu(), &QMenu::aboutToShow, this, [&opened] { opened = true; });
        key(Qt::Key_Down);
        disconnect(shown);
        check(opened, QStringLiteral("aşağı ok bölünmüş düğmenin listesini açtı"));
    }

    controller_->cancelInteractive();
    QCoreApplication::processEvents();
    return failures;
}

int MainWindow::probeFlyouts()
{
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_FLYOUT_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[kart] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        (void)std::fflush(stdout);
        if (!ok) ++failures;
    };

    runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
    runScriptLine(QStringLiteral("SİL"));
    runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
    runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));
    QCoreApplication::processEvents();

    // EVERY FAMILY, EVERY MEMBER. A split button is a list a hand opens with the
    // arrow; each row must start its own tool and become the face, so the next
    // click on the face runs what the hand chose last.
    int families = 0;
    int members  = 0;
    for (const RibbonFamily* family : std::as_const(families_)) {
        ++families;
        QAction* head       = family->head();
        const QString label = QString(family->members().value(0)->text()).remove(QLatin1Char('&'));
        QToolButton* button = ribbonButton(family->members().value(0));
        check(button != nullptr && button->popupMode() == QToolButton::MenuButtonPopup,
              QStringLiteral("%1: şeritte bölünmüş düğme").arg(label));
        QMenu* list = head->menu();
        check(list != nullptr && list->actions().size() > 1,
              QStringLiteral("%1: listede %2 üye var")
                  .arg(label)
                  .arg(list != nullptr ? list->actions().size() : 0));
        if (list == nullptr || button == nullptr) continue;

        if (shooting) {
            list->popup(button->mapToGlobal(QPoint(0, button->height())));
            QCoreApplication::processEvents();
            (void)list->grab().save(into + QStringLiteral("/liste-") + label +
                                    QStringLiteral(".png"));
            list->close();
            QCoreApplication::processEvents();
        }

        for (QAction* member : list->actions()) {
            controller_->cancelInteractive();
            actSelect_->setChecked(true);
            QCoreApplication::processEvents();

            const qsizetype transcript_before = transcript_->toPlainText().size();
            member->trigger(); // what a row of the list does when it is chosen
            QCoreApplication::processEvents();
            ++members;

            const QString word              = member->property(kToolCommand).toString();
            const command::Session* running = controller_->session();
            const bool answered             = transcript_->toPlainText().size() > transcript_before;
            const bool started =
                (running != nullptr && (running->waiting() || running->working())) ||
                member->isChecked() || answered;
            check(!word.isEmpty() && started && family->face() == member,
                  QStringLiteral("%1 > %2 çalıştı ve yüz oldu").arg(label, word));
        }
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
    }

    (void)std::fprintf(stdout, "[kart] %d aile, %d üye, %d kusur\n", families, members, failures);
    return failures;
}

int MainWindow::probeAnswerable()
{
    // A DIRECTORY MEANS PHOTOGRAPH IT. What a user sees when a tool asks for a
    // name is the whole point of this probe, and a pass/fail line does not show
    // it: the frame does.
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_ANSWER_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int failures     = 0;
    const auto check = [&failures](bool ok, const QString& what) {
        (void)std::fprintf(ok ? stdout : stderr, "[cevap] %s: %s\n", ok ? "tamam" : "BAŞARISIZ",
                           what.toUtf8().constData());
        if (!ok) ++failures;
    };

    // A drawing with a block in it, because BLOKEKLE's answer IS the drawing's
    // own blocks and an empty drawing would make "nothing offered" correct.
    runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
    runScriptLine(QStringLiteral("SİL"));
    runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
    runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));
    runScriptLine(QStringLiteral("ÇİZGİ 100,100 110,105"));
    endCommand(); ///< a run is written when it ends (TODOS C-02)
    runScriptLine(QStringLiteral("SEÇ mod=KUTU noktalar=99,99 111,106"));
    runScriptLine(QStringLiteral("BLOK ad=OK taban=100,100"));
    runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
    QCoreApplication::processEvents();

    struct Case
    {
        const char* command; ///< the command a button sends
        const char* wants;   ///< a fragment of the prompt it must reach
        const char* offers;  ///< a word its choices must contain, or nullptr
        const char* answer;  ///< what a hand would type to satisfy it
        bool selection;      ///< it asks which objects FIRST, so give it some
    };

    // Every tool whose typed question is the one that looked dead: the prompt
    // arrived and the keyboard did not follow it. `selection` marks the ones that
    // ask which objects first — a selection is handed over before the trigger so
    // the typed prompt is the one under test, which is how a user works anyway
    // (pick, then reach for the tool).
    const Case cases[] = {
        {"BLOKEKLE", "bloğun adı", "OK", "OK", false},
        {"BLOK", "Bloğun adı", nullptr, "YENİBLOK", false},
        {"KATMAN", "Katman adı", "PARSEL", "PARSEL", false},
        {"KATMANAT", "katmanın adı", "PARSEL", "PARSEL", true},
        {"ETİKET", "Etiketlenecek katman", "PARSEL", "PARSEL", false},
        {"KATMANGÖRÜNÜM", "İşlem", nullptr, "tumu", false},
        {"OFSET", "mesafesi", nullptr, "5", true},
    };

    for (const Case& c : cases) {
        controller_->cancelInteractive();
        runScriptLine(c.selection ? QStringLiteral("SEÇ mod=KUTU noktalar=-1,-1 41,31")
                                  : QStringLiteral("SEÇ TEMİZLE"));
        QCoreApplication::processEvents();

        QAction* action =
            findChild<QAction*>(QStringLiteral("toolAction.") + QString::fromUtf8(c.command));
        // Not every one has a column button; the menu entry is the same road.
        if (action == nullptr)
            for (QAction* candidate : findChildren<QAction*>())
                if (candidate->property(kToolCommand).toString() == QString::fromUtf8(c.command))
                    action = candidate;
        if (action == nullptr) {
            check(false, QStringLiteral("%1 için bir eylem var").arg(QString::fromUtf8(c.command)));
            continue;
        }

        action->trigger();
        QCoreApplication::processEvents();

        const command::Session* live = controller_->session();
        if (live == nullptr || !live->waiting()) {
            check(false, QStringLiteral("%1 bir şey sordu").arg(QString::fromUtf8(c.command)));
            continue;
        }

        const QString asked = QString::fromStdString(live->prompt().message);
        check(asked.contains(QString::fromUtf8(c.wants)),
              QStringLiteral("%1 sordu: \"%2\"").arg(QString::fromUtf8(c.command), asked));

        // THE KEYBOARD IS WHERE THE ANSWER GOES. This is the defect: the prompt
        // appeared and the focus stayed on the canvas, so typing went nowhere.
        check(QApplication::focusWidget() == commandLine_,
              QStringLiteral("%1: odak komut satırında").arg(QString::fromUtf8(c.command)));

        if (shooting) {
            QImage picture = grab().toImage();
            if (!picture.isNull() && canvas_ != nullptr && canvas_->isVisible()) {
                const QImage drawn = canvas_->grabCanvas();
                if (!drawn.isNull()) {
                    QPainter painter(&picture);
                    painter.drawImage(QRect(canvas_->mapTo(this, QPoint(0, 0)), canvas_->size()),
                                      drawn);
                }
            }
            // The completer's own popup is a window of its own, so it is grabbed
            // separately and pasted where it sits — otherwise the one thing the
            // frame exists to show is the one thing missing from it.
            if (QWidget* popup = QApplication::activePopupWidget();
                popup != nullptr && !picture.isNull()) {
                const QImage list = popup->grab().toImage();
                if (!list.isNull()) {
                    QPainter painter(&picture);
                    painter.drawImage(mapFromGlobal(popup->mapToGlobal(QPoint(0, 0))), list);
                }
            }
            (void)picture.save(into + QStringLiteral("/sorar-") + QString::fromUtf8(c.command) +
                               QStringLiteral(".png"));
        }

        if (c.offers != nullptr) {
            QStringList offered;
            for (const std::string& word : live->prompt().choices)
                offered << QString::fromStdString(word);
            check(offered.contains(QString::fromUtf8(c.offers)),
                  QStringLiteral("%1 seçenekleri sunuyor: %2")
                      .arg(QString::fromUtf8(c.command), offered.join(QStringLiteral(", "))));
        }

        // AND THE ANSWER LANDS. Typed into the field a hand would type into,
        // through the road a hand has — not `supplyText`, which would prove the
        // session accepts a value and nothing about whether a user can give one.
        commandLine_->setText(QString::fromUtf8(c.answer));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(commandLine_, &enter);
        QCoreApplication::processEvents();

        const command::Session* after = controller_->session();
        const QString now             = after != nullptr && after->waiting()
                                            ? QString::fromStdString(after->prompt().message)
                                            : QStringLiteral("(bitti)");
        check(now != asked, QStringLiteral("%1: cevap geçti, sırada \"%2\"")
                                .arg(QString::fromUtf8(c.command), now));

        controller_->cancelInteractive();
        QCoreApplication::processEvents();
    }

    // ---- AND THE ROWS THAT DO NOT RUN A COMMAND AT ALL --------------------
    //
    // Two kinds looked dead for two different reasons, and a user reported both
    // as the same thing: "I do not understand why these menu items are there."
    //
    //   * a feature that has not landed — a disabled row answers nothing when it
    //     is clicked, so it reads as broken rather than as not-yet;
    //   * a query command — it answers in the transcript, and the transcript
    //     panel starts closed, so the answer arrived where nobody was looking.
    // WHAT IS STILL A PLACEHOLDER, and the list shrinks as work lands: the three
    // clipboard rows were here until P6 gave them commands, and the assertion
    // below now checks the opposite of what it used to — that they RUN.
    for (const char* named : {"Katman Yöneticisi"}) {
        QAction* row = nullptr;
        for (QAction* candidate : findChildren<QAction*>())
            if (candidate->text() == QString::fromUtf8(named)) row = candidate;
        if (row == nullptr) {
            check(false, QStringLiteral("%1 satırı var").arg(QString::fromUtf8(named)));
            continue;
        }
        check(row->isEnabled(), QStringLiteral("%1 tıklanabilir").arg(QString::fromUtf8(named)));

        // Opened on a timer because `QMessageBox::exec` blocks: the box is read
        // and closed from inside its own event loop.
        QString said;
        QTimer::singleShot(0, this, [&said, named, shooting, into] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box == nullptr) return;
            said = box->text() + QLatin1Char(' ') + box->informativeText();
            if (shooting)
                (void)box->grab().save(into + QStringLiteral("/anlat-") + QString::fromUtf8(named) +
                                       QStringLiteral(".png"));
            box->accept();
        });
        row->trigger();
        QCoreApplication::processEvents();

        check(said.contains(QStringLiteral("gelecek")),
              QStringLiteral("%1 hangi fazda geleceğini söylüyor").arg(QString::fromUtf8(named)));
        check(said.contains(QStringLiteral("Bugün")),
              QStringLiteral("%1 bugün ne kullanılacağını söylüyor").arg(QString::fromUtf8(named)));
    }

    // AND WHAT STOPPED BEING ONE. A row that used to explain itself and now has a
    // command behind it has to RUN that command — the opposite assertion, and the
    // one that catches a placeholder left in place after its command landed.
    for (const auto& [named, word] :
         {std::pair{"Kes", "KES"}, std::pair{"Panoya Kopyala", "PANOYAKOPYALA"},
          std::pair{"Yapıştır", "YAPIŞTIR"}}) {
        QAction* row = nullptr;
        for (QAction* candidate : findChildren<QAction*>())
            if (candidate->text() == QString::fromUtf8(named)) row = candidate;
        check(row != nullptr && row->isEnabled(),
              QStringLiteral("%1 satırı canlı").arg(QString::fromUtf8(named)));
        if (row == nullptr) continue;
        check(row->property(kToolCommand).toString() == QString::fromUtf8(word),
              QStringLiteral("%1 artık %2 komutunu çalıştırıyor")
                  .arg(QString::fromUtf8(named), QString::fromUtf8(word)));
    }

    // A query command run from its menu row brings its own answer into view: the
    // properties dock, on the tab the transcript is on.
    {
        propertyDock_->hide();
        propertyHeader_->setCurrent(0);
        propertyStack_->setCurrentIndex(0);
        QCoreApplication::processEvents();

        QAction* listing = nullptr;
        for (QAction* candidate : findChildren<QAction*>())
            if (candidate->property(kToolCommand).toString() == QLatin1String("KATMANLAR"))
                listing = candidate;
        if (listing == nullptr) {
            check(false, QStringLiteral("Katmanları Listele satırı var"));
        } else {
            listing->trigger();
            QCoreApplication::processEvents();
            check(propertyDock_->isVisible(), QStringLiteral("KATMANLAR paneli açtı"));
            check(propertyStack_->currentWidget() == transcript_,
                  QStringLiteral("KATMANLAR Geçmiş sekmesine geçti"));
            check(transcript_->toPlainText().contains(QStringLiteral("PARSEL")),
                  QStringLiteral("cevap görünür yerde: katman adı yazıldı"));
        }
    }

    (void)std::fprintf(stdout, "[cevap] %d kusur\n", failures);
    return failures;
}

int MainWindow::probeReach()
{
    // Every command any action in this window can start, resolved through the
    // registry so an alias on a button counts as the command it names and a
    // rename cannot silently orphan a button.
    QSet<QString> reachable;
    for (QAction* action : findChildren<QAction*>()) {
        QString word = action->property(kToolCommand).toString();
        if (word.isEmpty() && action->objectName().startsWith(QStringLiteral("toolAction.")))
            word = action->objectName().section(QLatin1Char('.'), 1);
        if (word.isEmpty()) continue;
        if (!action->isEnabled()) continue; ///< a disabled placeholder is not a road
        if (const command::CommandSpec* spec =
                controller_->registry().resolve(word.section(QLatin1Char(' '), 0, 0).toStdString());
            spec != nullptr)
            reachable.insert(QString::fromStdString(spec->id));
    }

    int total   = 0;
    int missing = 0;
    for (const command::CommandSpec& spec : controller_->registry().all()) {
        if (spec.names.empty()) continue;
        ++total;
        if (reachable.contains(QString::fromStdString(spec.id))) continue;
        ++missing;
        (void)std::fprintf(stderr, "[erişim] BAŞARISIZ: %s (%s) düğmesiz ve menüsüz\n",
                           spec.names.front().c_str(), spec.id.c_str());
    }

    (void)std::fprintf(stdout, "[erişim] %d komuttan %d'i fareyle başlatılabiliyor\n", total,
                       total - missing);
    return missing;
}

int MainWindow::probeMenus()
{
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_MENU_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int failures     = 0;
    int index        = 0;
    SARibbonBar* bar = ribbonBar();
    if (bar == nullptr) {
        (void)std::fprintf(stderr, "[menü] BAŞARISIZ: şerit yok\n");
        return 1;
    }
    // THE THEME, when asked for one (`koyu` or `acik`): the window's own state
    // for the length of the probe; the preference file is not written.
    if (const QByteArray asked = qgetenv("KENTOS_PROBE_THEME"); !asked.isEmpty()) {
        theme_ = asked == "acik" ? ThemeMode::Light : ThemeMode::Dark;
        applyTheme();
        QCoreApplication::processEvents();
    }

    const auto walk = [](QMenu* menu, const QString& lead, auto&& self) -> int {
        int leaves = 0;
        for (QAction* action : menu->actions()) {
            if (action->isSeparator() || !action->isVisible()) continue;
            if (action->menu() != nullptr) {
                (void)std::fprintf(stdout, "[menü] %s%s  >\n", lead.toUtf8().constData(),
                                   action->text().toUtf8().constData());
                leaves += self(action->menu(), lead + QStringLiteral("    "), self);
                continue;
            }
            ++leaves;
            const QString word = action->property(kToolCommand).toString();
            (void)std::fprintf(stdout, "[menü] %s%-28s %-14s %s\n", lead.toUtf8().constData(),
                               action->text().toUtf8().constData(), word.toUtf8().constData(),
                               action->isEnabled() ? "" : "(kapalı)");
        }
        return leaves;
    };
    const auto opened = [&](QMenu* menu, const QString& title, QPoint at) {
        menu->popup(at);
        QCoreApplication::processEvents();
        (void)std::fprintf(stdout, "[menü] ==== %s (%d px) ====\n", title.toUtf8().constData(),
                           menu->height());
        const int leaves = walk(menu, QString(), walk);
        // A LIST TALLER THAN A LAPTOP'S SCREEN is a list whose foot nobody sees.
        if (menu->height() > 900) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: %s %d px — ekranı aşıyor\n",
                               title.toUtf8().constData(), menu->height());
            ++failures;
        }
        if (leaves == 0) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: %s boş\n", title.toUtf8().constData());
            ++failures;
        }
        // The title names the tab AND the list (`Giriş / Ölçü`), and the slash
        // in it made the file a path into a folder that is not there: every
        // family list was grabbed and none was ever written.
        if (shooting)
            (void)menu->grab().toImage().save(
                QStringLiteral("%1/menu-%2-%3.png")
                    .arg(into)
                    .arg(index, 2, 10, QLatin1Char('0'))
                    .arg(QString(title).replace(QStringLiteral(" / "), QStringLiteral("-"))));
        menu->close();
        QCoreApplication::processEvents();
    };

    // ---- the application button: KentOS CAD, and quit at its foot ------------
    if (auto* app = qobject_cast<QToolButton*>(bar->applicationButton());
        app != nullptr && appMenu_ != nullptr) {
        ++index;
        const QString title = app->text();
        if (title != QStringLiteral("KentOS CAD")) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: ana menü düğmesi \"%s\" diyor\n",
                               title.toUtf8().constData());
            ++failures;
        }
        openApplicationMenu();
        QCoreApplication::processEvents();
        // THE WAY OUT IS THE LAST THING AT ITS FOOT, on every platform.
        QList<QAbstractButton*> foot;
        for (QAbstractButton* b : appMenu_->findChildren<QAbstractButton*>())
            if (b->parentWidget() != nullptr &&
                b->parentWidget()->objectName() == QStringLiteral("applicationMenuFoot"))
                foot << b;
        std::sort(foot.begin(), foot.end(), [](const QAbstractButton* a, const QAbstractButton* b) {
            return a->x() < b->x();
        });
        if (foot.isEmpty() ||
            foot.back()->text() != QString(actQuit_->text()).remove(QLatin1Char('&'))) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: Çıkış ana menünün en altında değil\n");
            ++failures;
        }
        int verbs = 0;
        for (QAbstractButton* b : appMenu_->findChildren<QAbstractButton*>())
            if (b->objectName() == QStringLiteral("applicationMenuVerb")) {
                ++verbs;
                (void)std::fprintf(stdout, "[ana menü] %s — %s\n", b->text().toUtf8().constData(),
                                   b->accessibleDescription().toUtf8().constData());
            }
        if (verbs == 0) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: ana menü boş\n");
            ++failures;
        }
        if (shooting)
            (void)appMenu_->grab().toImage().save(QStringLiteral("%1/menu-%2-%3.png")
                                                      .arg(into)
                                                      .arg(index, 2, 10, QLatin1Char('0'))
                                                      .arg(title));
        // And with the keyboard on `Yazdır`, whose profiles fill the pane. Found
        // first and pressed after: landing on it rebuilds the pane, and the rows
        // a stale list still held are gone by then.
        QAbstractButton* print = nullptr;
        for (QAbstractButton* b : appMenu_->findChildren<QAbstractButton*>())
            if (b->objectName() == QStringLiteral("applicationMenuVerb") &&
                b->text() == QString(actPrint_->text()).remove(QLatin1Char('&')))
                print = b;
        if (print != nullptr) {
            print->setFocus(Qt::TabFocusReason);
            QCoreApplication::processEvents();
            if (shooting)
                (void)appMenu_->grab().toImage().save(QStringLiteral("%1/menu-%2-%3-yazdir.png")
                                                          .arg(into)
                                                          .arg(index, 2, 10, QLatin1Char('0'))
                                                          .arg(title));
        }
        appMenu_->hide();
        QCoreApplication::processEvents();
    } else {
        (void)std::fprintf(stderr, "[menü] BAŞARISIZ: ana menü düğmesi yok\n");
        ++failures;
    }

    // ---- every tab, every panel, every list ------------------------------------
    for (SARibbonCategory* tab : bar->categoryPages()) {
        ++index;
        // AN EDITOR TAB IS SHOWN FOR ITS PICTURE the way a selection would show
        // it, and put away again after.
        SARibbonContextCategory* context = nullptr;
        for (SARibbonContextCategory* c : bar->contextCategoryList())
            if (c->isHaveCategory(tab)) context = c;
        if (context != nullptr) bar->setContextCategoryVisible(context, true);
        bar->raiseCategory(tab);
        QCoreApplication::processEvents();
        const QString title = tab->categoryName();
        int shown           = 0;
        for (SARibbonToolButton* button : tab->findChildren<SARibbonToolButton*>()) {
            const QAction* face = button->defaultAction();
            if (face == nullptr) continue;
            ++shown;
            // THE PANEL IT SITS IN and the command it runs, so the line is the
            // path a page of the manual names: tab, panel, button, word.
            QString panel;
            for (QWidget* up = button->parentWidget(); up != nullptr; up = up->parentWidget())
                if (auto* p = qobject_cast<SARibbonPanel*>(up); p != nullptr) {
                    panel = p->panelName();
                    break;
                }
            (void)std::fprintf(stdout, "[şerit] %-10s %-22s %-30s %-14s %s\n",
                               title.toUtf8().constData(), panel.toUtf8().constData(),
                               QString(face->text()).remove(QLatin1Char('&')).toUtf8().constData(),
                               face->property(kToolCommand).toString().toUtf8().constData(),
                               face->menu() != nullptr ? "▾" : "");
        }
        (void)std::fprintf(stdout, "[şerit] ==== %s: %d düğme ====\n", title.toUtf8().constData(),
                           shown);
        if (shown == 0) {
            (void)std::fprintf(stderr, "[şerit] BAŞARISIZ: %s sekmesi boş\n",
                               title.toUtf8().constData());
            ++failures;
        }
        if (shooting)
            (void)bar->grab().toImage().save(QStringLiteral("%1/serit-%2-%3.png")
                                                 .arg(into)
                                                 .arg(index, 2, 10, QLatin1Char('0'))
                                                 .arg(title));
        for (SARibbonToolButton* button : tab->findChildren<SARibbonToolButton*>())
            if (QAction* face = button->defaultAction(); face != nullptr && face->menu() != nullptr)
                opened(face->menu(), title + QStringLiteral(" / ") + face->text(),
                       button->mapToGlobal(QPoint(0, button->height())));
        if (context != nullptr) bar->setContextCategoryVisible(context, false);
    }
    bar->setCurrentIndex(0);

    {
        QMenu probe(this);
        const core::Settings& session = controller_->bus().session_settings();
        const auto mask  = static_cast<std::uint32_t>(session.get("core.yakalama.modlar").as_int());
        std::size_t rows = 0;
        for (std::uint32_t bit = 1; bit != 0; bit = static_cast<std::uint32_t>(bit << 1)) {
            if ((core::SnapAllMask & bit) == 0) continue;
            ++rows;
            (void)std::fprintf(stdout, "[yakalama] %-22s %-18s %s\n", core::snap_mode_label(bit),
                               core::snap_mode_id(bit), (mask & bit) != 0 ? "açık" : "");
        }
        std::size_t declared = 0;
        for (const std::uint32_t* bit = core::snap_mode_bits(); *bit != core::SnapNone; ++bit)
            if ((core::SnapAllMask & *bit) != 0) ++declared;
        if (rows != declared) {
            (void)std::fprintf(stderr, "[yakalama] BAŞARISIZ: %zu satır, motorun %zu modu var\n",
                               rows, declared);
            ++failures;
        }
        (void)std::fprintf(stdout, "[yakalama] %zu mod listelenebiliyor\n", rows);
    }

    // ---- one key, one action ----------------------------------------------------
    // TWO ACTIONS ON ONE KEY IS NO KEY AT ALL: Qt reports the press as
    // ambiguous and triggers neither. `Ctrl+Shift+A` once cleared the selection
    // and opened the chat, and so did nothing — every shortcut now lives on the
    // window (`buildRibbon`), which is what made the collision real everywhere.
    {
        QHash<QString, QStringList> owners;
        for (const QAction* action : findChildren<QAction*>())
            for (const QKeySequence& key : action->shortcuts())
                if (!key.isEmpty())
                    owners[key.toString(QKeySequence::PortableText)]
                        << QString(action->text()).remove(QLatin1Char('&'));
        for (auto it = owners.cbegin(); it != owners.cend(); ++it)
            if (it.value().size() > 1) {
                (void)std::fprintf(stderr, "[kısayol] BAŞARISIZ: %s iki eylemde: %s\n",
                                   it.key().toUtf8().constData(),
                                   it.value().join(QStringLiteral(", ")).toUtf8().constData());
                ++failures;
            }
        (void)std::fprintf(stdout, "[kısayol] %lld tuş, her biri tek eylemde\n",
                           static_cast<long long>(owners.size()));
    }

    // ---- a switch says what it switches ------------------------------------------
    // `Görünüm ▸ Pencereler ▸ Komut Satırı` and Ctrl+9 read off while the line was
    // showing, so the first press did nothing a user could see.
    if (actCommandLine_->isChecked() != commandLine_->isVisible()) {
        (void)std::fprintf(stderr, "[pencere] BAŞARISIZ: Komut Satırı düğmesi %s, satır %s\n",
                           actCommandLine_->isChecked() ? "basılı" : "basılı değil",
                           commandLine_->isVisible() ? "görünüyor" : "gizli");
        ++failures;
    } else {
        (void)std::fprintf(stdout, "[pencere] Komut Satırı düğmesi satırla aynı: %s\n",
                           commandLine_->isVisible() ? "görünüyor" : "gizli");
    }

    (void)std::fprintf(stdout, "[menü] %d menü, %d kusur\n", index, failures);
    return failures;
}

void MainWindow::probeDialogs()
{
    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[pencere] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    const QString dir = QString::fromLocal8Bit(qgetenv("KENTOS_DIALOG_PROBE"));
    if (dir.isEmpty() || dir == QLatin1String("1")) {
        say(QStringLiteral("KENTOS_DIALOG_PROBE bir dizin olmalı"));
        return;
    }
    QDir().mkpath(dir);

    // THE THEME THE MOCKUPS ARE DRAWN IN, unless asked otherwise. The window's own
    // state for the length of the probe; the preference file is not written.
    theme_ = qgetenv("KENTOS_PROBE_THEME") == "acik" ? ThemeMode::Light : ThemeMode::Dark;
    applyTheme();

    seedProbeDrawing();
    command::Bus& bus = controller_->bus();
    (void)bus.execute_line("SEÇ TÜMÜ", command::Origin::Gui);
    QCoreApplication::processEvents();

    // One object selected, through the same line the pick list sends.
    const std::vector<core::EntityKey> all = bus.selection().keys();
    if (!all.empty()) {
        command::Args args;
        args.set("mod", command::Value::text("NESNE"));
        args.set("nesneler", command::Value::ids({static_cast<std::int64_t>(
                                 static_cast<std::uint64_t>(all.front()))}));
        controller_->runInvocation(
            command::Invocation{"core.select", std::move(args), command::Origin::Gui});
    }
    QCoreApplication::processEvents();
    say(QStringLiteral("nesne: %1, seçili: %2").arg(all.size()).arg(bus.selection().size()));

    const auto shoot = [&dir, &say](QWidget* w, const char* name) {
        w->show();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        const QString file = dir + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png");
        say(w->grab().save(file) ? QStringLiteral("kare: %1.png").arg(QLatin1String(name))
                                 : QStringLiteral("yazılamadı: %1").arg(file));
    };

    shoot(this, "ana");

    // EVERY PICTURE THE PROGRAM DRAWS, at the size a large ribbon button shows
    // it and twice that, numbered in the order `Glyph` declares them: the one
    // sheet an icon is judged on before it reaches a button (design.md §5).
    {
        constexpr int kCell    = 76;
        constexpr int kColumns = 16;
        const int count        = static_cast<int>(Glyph::Dependency) + 1;
        const int rows         = (count + kColumns - 1) / kColumns;
        const Tokens& t        = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        QImage sheet(kColumns * kCell, rows * kCell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(t.ribbonBody);
        QPainter paint(&sheet);
        paint.setRenderHint(QPainter::Antialiasing, true);
        paint.setPen(t.textDim);
        const GlyphInks inks = actionInks();
        for (int i = 0; i < count; ++i) {
            const int x = (i % kColumns) * kCell;
            const int y = (i / kColumns) * kCell;
            paint.drawPixmap(x + 6, y + 6,
                             colour_icon(static_cast<Glyph>(i), inks, 48).pixmap(48, 48));
            paint.drawPixmap(x + 56, y + 6,
                             colour_icon(static_cast<Glyph>(i), inks, 16).pixmap(16, 16));
            paint.drawText(QRect(x, y + 56, kCell, 18), Qt::AlignCenter, QString::number(i));
        }
        paint.end();
        const QString file = dir + QStringLiteral("/simgeler.png");
        say(sheet.save(file) ? QStringLiteral("kare: simgeler.png")
                             : QStringLiteral("yazılamadı: %1").arg(file));
    }

    if (attributePanel_ != nullptr) shoot(attributePanel_, "nesne-paneli");
    if (layerPanel_ != nullptr) shoot(layerPanel_, "katman-paneli");

    {
        // The About window, and a message box with Qt's own standard buttons —
        // `Kaydet`, `Kaydetme`, `İptal` must read in Turkish, from Qt's catalogue.
        AboutFacts facts;
        facts.version = QStringLiteral(KENTOS_VERSION);
        facts.qt      = QString::fromLatin1(qVersion());
        facts.backend = canvas_->backendName();
        facts.platform =
            tr("%1 · %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
        facts.commands = static_cast<int>(controller_->registry().size());
        facts.tools    = static_cast<int>(processing::processing_tools().size());
        facts.dataRoot = QDir::toNativeSeparators(QString::fromStdString(data_root()));
        AboutDialog about(facts, this);
        about.applyTheme(theme_);
        shoot(&about, "hakkinda");
        QMessageBox box(QMessageBox::Question, tr("Kaydedilsin mi?"),
                        tr("Çizimde kaydedilmemiş değişiklikler var."),
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        applyThemeToChildren(&box, theme_);
        shoot(&box, "mesaj-kutusu");
        say(QStringLiteral("mesaj kutusu düğmeleri: %1 · %2 · %3")
                .arg(box.button(QMessageBox::Save)->text(),
                     box.button(QMessageBox::Discard)->text(),
                     box.button(QMessageBox::Cancel)->text()));
    }

    {
        SettingsDialog d(*controller_, SettingsDialog::Mode::All, this);
        d.applyTheme(theme_);
        d.resize(1180, 740);
        shoot(&d, "secenekler");

        // THE PAGES THAT HOLD A TABLE ARE PHOTOGRAPHED TOO, because a table is
        // where a layout goes wrong and the first page never shows one. The
        // listener page is the live example: it grew a client list and two row
        // actions, and neither is visible in a shot of the page above it.
        for (const auto& [title, name] :
             {std::pair<const char*, const char*>{"MCP Sunucusu", "secenekler-mcp"},
              {"Yapay Zeka Modelleri", "secenekler-modeller"},
              {"Yazdırma", "secenekler-yazdirma"}}) {
            d.showSection(QString::fromUtf8(title));
            QCoreApplication::processEvents();
            shoot(&d, name);
        }

#if KENTOS_HAVE_MCP
        // AND THE LISTENER PAGE AGAIN WITH SOMETHING IN IT. An empty table and
        // a full one are different pictures and both can be wrong; the empty
        // one was — three headings elided to "İstem…", "Çağ…", "Son görül…".
        // The rows are made the honest way, by actually talking to the server,
        // so what is photographed is what a real client leaves behind.
        if (McpService* server = controller_->mcpService(); server != nullptr) {
            if (auto up = server->start(18790); up) {
                for (int i = 0; i < 3; ++i)
                    (void)server->probe();
                d.showSection(QStringLiteral("MCP Sunucusu"));
                QCoreApplication::processEvents();
                shoot(&d, "secenekler-mcp-istemciler");
                server->stop();
            } else {
                say(QStringLiteral("MCP sunucusu açılmadı: %1")
                        .arg(QString::fromStdString(up.error().message)));
            }
        }
#endif
    }
    {
        SettingsDialog d(*controller_, SettingsDialog::Mode::Project, this);
        d.applyTheme(theme_);
        d.resize(1180, 740);
        shoot(&d, "proje-ayarlari");
    }
    {
        StyleDesigner d(*controller_, controller_->activeLayerName(), this);
        d.applyTheme(theme_);
        d.resize(1280, 756);
        shoot(&d, "katman-ozellikleri");
    }
    {
        // The plain state as well — a layer no classification touched — because
        // that is the page most layers open on.
        QString plain;
        const core::Document& doc = controller_->document();
        for (std::size_t slot = 0; slot < doc.layers().size() && plain.isEmpty(); ++slot) {
            const QString name = QString::fromStdString(doc.layers()[slot].name);
            if (name != controller_->activeLayerName()) plain = name;
        }
        if (!plain.isEmpty()) {
            StyleDesigner d(*controller_, plain, this);
            d.applyTheme(theme_);
            d.resize(1280, 756);
            shoot(&d, "katman-ozellikleri-tek");
        }
    }
    {
        AttributeTable d(*controller_, QString(), this);
        d.applyTheme(theme_);
        d.resize(1400, 800);
        shoot(&d, "oznitelik-tablosu");
    }
    {
        DatabaseDialog d(*controller_, this);
        d.applyTheme(theme_);
        shoot(&d, "veritabani");
    }
    {
        ImportWizard d(*controller_, theme_, this);
        shoot(&d, "ice-aktar");

        // With a file named in `KENTOS_PROBE_IMPORT`, the two pages a read fills
        // as well: the layers and the fields. A wizard photographed only on its
        // first page is a wizard nobody checked.
        const QString sample = QString::fromLocal8Bit(qgetenv("KENTOS_PROBE_IMPORT"));
        if (!sample.isEmpty()) {
            d.beginWith(sample);
            if (d.probeSettle(1)) shoot(&d, "ice-aktar-katmanlar");
            if (d.probeSettle(2)) shoot(&d, "ice-aktar-alanlar");
        }
    }
    {
        ColumnDialog d(*controller_, QString(), controller_->activeLayerName(), this);
        d.applyTheme(theme_);
        shoot(&d, "yeni-sutun");
    }
    {
        ExportDialog d(*controller_, ExportSubject::Coordinates, QString(), this);
        d.applyTheme(theme_);
        shoot(&d, "disa-aktar");
    }
}

void MainWindow::openCommandSearch(const QString& focus_on)
{
    if (!palette_) {
        palette_ = new CommandPalette(controller_->registry(), this);
        palette_->applyTheme(theme_);
        connect(palette_, &CommandPalette::chosen, this, [this](const QString& name) {
            // Straight to the prompt rather than straight to the bus: a command
            // with arguments needs them typed, and the command line is where the
            // shell already asks for them (Article 1.2).
            showCommandLine(true);
            commandLine_->setText(name + QLatin1Char(' '));
            commandLine_->setFocus(Qt::ShortcutFocusReason);
        });
    }
    palette_->reveal(focus_on);
}

void MainWindow::resetLayout()
{
    // THE LAYOUT THE PROGRAM STARTS WITH, not a third arrangement of its own.
    //
    // This used to put two of the four docks back, tabify them, and forget the
    // chat and the journal entirely — so the one command a user reaches for
    // when the shell looks wrong LEFT it wrong, with Qt's tab bar over the two
    // it did restore and two panels still missing. It now rebuilds exactly what
    // the constructor builds, in the same order and at the same sizes.
    for (QDockWidget* dock : {propertyDock_, layerDock_, chatDock_})
        if (dock != nullptr) {
            dock->setFloating(false);
            addDockWidget(Qt::RightDockWidgetArea, dock);
            dock->show();
        }
    if (journalDock_ != nullptr) {
        journalDock_->setFloating(false);
        addDockWidget(Qt::BottomDockWidgetArea, journalDock_);
    }
    if (pythonDock_ != nullptr) {
        pythonDock_->setFloating(false);
        addDockWidget(Qt::BottomDockWidgetArea, pythonDock_);
    }

    if (propertyDock_ != nullptr && layerDock_ != nullptr) {
        resizeDocks({propertyDock_, layerDock_}, {312, 312}, Qt::Horizontal);
        resizeDocks({propertyDock_, layerDock_}, {600, 268}, Qt::Vertical);
    }
    syncDockTitles();
}

void MainWindow::chooseCapture(const std::vector<core::EntityId>& candidates)
{
    if (candidates.empty()) {
        canvas_->cancelCapture();
        return;
    }

    // The same window as `choosePick`, and the same care about the selection: the
    // list highlights rows by SELECTING them, so whatever was selected before is
    // put back either way — a field's pick is not a selection (model.md R43).
    const std::vector<core::EntityKey> before = controller_->bus().selection().keys();
    const auto send                           = [this](const std::vector<core::EntityKey>& keys) {
        command::Args args;
        if (keys.empty()) {
            args.set("mod", command::Value::text("TEMİZLE"));
        } else {
            std::vector<std::int64_t> ids;
            ids.reserve(keys.size());
            for (const core::EntityKey key : keys)
                ids.push_back(static_cast<std::int64_t>(static_cast<std::uint64_t>(key)));
            args.set("mod", command::Value::text("NESNE"));
            args.set("nesneler", command::Value::ids(std::move(ids)));
        }
        controller_->runInvocation(
            command::Invocation{"core.select", std::move(args), command::Origin::Gui});
    };

    PickList chooser(*controller_, candidates, this);
    chooser.applyTheme(theme_);
    connect(&chooser, &PickList::highlighted, this, [&send](core::EntityKey key) { send({key}); });
    const bool accepted = chooser.exec() == QDialog::Accepted;
    send(before);
    if (!accepted || chooser.picked() == core::EntityKey::None) {
        canvas_->cancelCapture();
        return;
    }
    canvas_->finishCapture(chooser.picked());
}

void MainWindow::onEcho(const QString& text)
{
    // THE NEWEST LINE HAS TO BE THE VISIBLE ONE. `appendPlainText` puts the line
    // in the document and leaves the viewport where it was, and a tab that was
    // hidden while a hundred lines arrived comes forward showing line forty — so
    // a query ran, answered, and the answer was off-screen in a panel that had
    // just been opened to show it.
    //
    // Followed only when the tail was already in view: a user who scrolled back
    // to read something is reading it, and yanking them to the bottom on the next
    // echo would be the program taking the page away.
    QScrollBar* bar       = transcript_->verticalScrollBar();
    const bool was_at_end = bar->value() >= bar->maximum() - 4;
    transcript_->appendPlainText(text);
    if (was_at_end) bar->setValue(bar->maximum());

    // THE TRANSCRIPT, ON STDOUT, FOR A PROBE RUN. Developer tooling and an
    // environment variable rather than a CLI flag, for the reason the other
    // probes give: a flag is user-facing and would need its own /docs page
    // (CLAUDE.md 5.17). It exists because a script run under `--betik` puts
    // every answer the program gives into a widget nobody is looking at, so a
    // refusal — an export with no CRS, a layer name that matched nothing — was
    // indistinguishable from success from outside the process.
    static const bool echo_out = qEnvironmentVariableIsSet("KENTOS_ECHO_STDOUT");
    if (echo_out) {
        (void)std::fprintf(stdout, "[echo] %s\n", qPrintable(text));
        // Flushed line by line: a probe run ends by being killed, and a block
        // buffer that is never flushed loses exactly the answer being probed for.
        (void)std::fflush(stdout);
    }

    // AND WHERE THE USER IS LOOKING. The transcript is the record; the status
    // line is the answer. A measurement, a coordinate, a count or a refusal that
    // only reached the record read as a command that did nothing at all — which
    // is exactly how ÖLÇ and ALANÖLÇ were experienced.
    //
    // The last line only: a command that echoes several times is reporting a
    // list, and the list belongs in the transcript.
    if (statusStrip_ != nullptr) {
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) statusStrip_->setMessage(lines.back().trimmed());
    }
}

void MainWindow::onDocumentChanged()
{
    canvas_->noteDocumentChange();
    refreshBlockEdit();
    layerPanel_->refresh();
    xrefPanel_->refresh();
    attributePanel_->refresh();
    refreshStatus();
    refreshRibbon();
    // Whichever client opened or saved it — the menu, AÇ at the prompt, a
    // script — the document goes on the recent list.
    noteRecentFile(controller_->currentFile());

    // Mirroring the journal keeps the architecture visible while using the program.
    journalView_->clear();
    for (const auto& e : controller_->journal().entries())
        journalView_->appendPlainText(QString::fromStdString(e.to_json(false).dump()));

    canvas_->update();
}

void MainWindow::showChat()
{
    if (chatDock_ == nullptr) return;
    // PUT IT BACK WHERE IT BELONGS BEFORE SHOWING IT, the repair
    // `showPythonConsole` makes: a saved layout can leave the dock floating
    // somewhere the person never chose, and a suggestion card opened in a
    // stray window is a card nobody sees. A deliberate float is respected —
    // this reaches only a dock that is floating AND hidden.
    if (chatDock_->isFloating() && !chatDock_->isVisible()) {
        chatDock_->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, chatDock_);
    }
    chatDock_->show();
    chatDock_->raise();
}

void MainWindow::showPythonConsole(const QString& source)
{
    if (pythonDock_ == nullptr || pythonConsole_ == nullptr) return;

    // PUT IT BACK WHERE IT BELONGS BEFORE SHOWING IT. A saved layout can leave a
    // dock floating in the middle of the screen or collapsed to nothing, and both
    // states are ones the user never chose — they are what `restoreState` does
    // with a dock it half remembers. Opening a panel that cannot be read is worse
    // than not opening it, so the one gesture that opens it also repairs it.
    //
    // A DELIBERATE float is respected: this only reaches a dock that is floating
    // AND hidden, which is the state nobody asks for.
    if (pythonDock_->isFloating() && !pythonDock_->isVisible()) {
        pythonDock_->setFloating(false);
        addDockWidget(Qt::BottomDockWidgetArea, pythonDock_);
    }

    pythonDock_->show();
    pythonDock_->raise();

    // SIZED AFTER IT IS SHOWN, and that ordering is the whole of it: `resizeDocks`
    // on a hidden dock is a request Qt has nowhere to apply. The console's own
    // `sizeHint` carries the same number, which is what actually holds when the
    // user reopens the panel later.
    // A THIRD OF THE WINDOW AT MOST: 260 px of a 720 px laptop window leaves
    // the drawing less than half the height it had.
    resizeDocks({pythonDock_}, {std::min(260, height() / 3)}, Qt::Vertical);

    pythonConsole_->focusPrompt();
    if (!source.isEmpty()) pythonConsole_->runSource(source);
}

void MainWindow::typeIntoPythonPrompt(const QString& source)
{
    if (pythonConsole_ == nullptr) return;
    pythonConsole_->typeIntoPrompt(source);
}

QWidget* MainWindow::pythonSignatureHint() const
{
    return pythonConsole_ == nullptr ? nullptr : pythonConsole_->promptHint();
}

QWidget* MainWindow::pythonCompletionPopup() const
{
    return pythonConsole_ == nullptr ? nullptr : pythonConsole_->promptPopup();
}

void MainWindow::clearPythonPrompt()
{
    if (pythonConsole_ != nullptr) pythonConsole_->prompt()->clear();
}

void MainWindow::showTranscript()
{
    if (propertyDock_ == nullptr || propertyStack_ == nullptr) return;

    propertyDock_->show();
    propertyDock_->raise();
    // The tab AND the stack: the header draws the tabs and the stack holds the
    // pages, and moving one without the other shows a panel labelled `Geçmiş`
    // with the attribute table in it.
    const int at = propertyStack_->indexOf(transcript_);
    if (at < 0) return;
    propertyStack_->setCurrentIndex(at);
    if (propertyHeader_ != nullptr) propertyHeader_->setCurrent(at);

    // AT THE NEWEST LINE. This is called because a command is about to answer, or
    // has just answered, so the line worth reading is the last one — and a hidden
    // QPlainTextEdit does not lay out, so the tab otherwise comes forward at
    // whatever row it happened to stop at.
    if (QScrollBar* bar = transcript_->verticalScrollBar(); bar != nullptr)
        bar->setValue(bar->maximum());
}

void MainWindow::showAttributes()
{
    if (propertyDock_ == nullptr || propertyStack_ == nullptr || attributePanel_ == nullptr) return;
    propertyDock_->show();
    propertyDock_->raise();
    const int at = propertyStack_->indexOf(attributePanel_);
    if (at < 0) return;
    propertyStack_->setCurrentIndex(at);
    if (propertyHeader_ != nullptr) propertyHeader_->setCurrent(at);
    attributePanel_->refresh();
}

bool MainWindow::confirmErase()
{
    if (!controller_->bus().app_settings().get("core.duzenleme.silme_onayi").as_bool()) return true;

    const std::size_t count = controller_->bus().selection().size();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Silme onayı"));
    box.setText(tr("%n nesne silinecek.", "", static_cast<int>(count)));
    box.setInformativeText(tr("Silmek istediğinize emin misiniz? Bu işlem GERİAL ile "
                              "geri alınabilir."));

    QPushButton* erase  = box.addButton(tr("Sil"), QMessageBox::DestructiveRole);
    QPushButton* cancel = box.addButton(tr("Vazgeç"), QMessageBox::RejectRole);

    // Cancel is the default, because this dialog exists for the user who did NOT
    // mean to press Del: a Return landing on "Sil" would delete exactly the work
    // the confirmation was turned on to protect.
    box.setDefaultButton(cancel);
    box.setEscapeButton(static_cast<QAbstractButton*>(cancel));
    box.exec();

    return box.clickedButton() == static_cast<QAbstractButton*>(erase);
}

void MainWindow::syncToolSelection()
{
    // WHICH command is running, not WHETHER one is. Every draw tool carries the
    // command it sends; the session knows the command it is running; `Registry`
    // is what turns the first into the second. A tool whose command is not the
    // running one is not lit, and that includes ÇİZGİ.
    const command::Session* session     = controller_->session();
    const command::CommandSpec* running = session ? &session->spec() : nullptr;

    QAction* lit = nullptr;
    if (running) {
        // THE LINE THAT ARMED IT FIRST. Five buttons send `core.arc_draw` — YAY
        // and its four methods — so the command id alone cannot say which one the
        // hand pressed, and matching the first id lit the plain YAY while "Yay —
        // üç nokta" ran. The controller remembers the exact line a button sent,
        // and a button whose whole line is that line is the one that is running.
        const QString armed = controller_->armedLine();
        if (!armed.isEmpty())
            for (QAction* action : drawingTools_->actions())
                if (action->property(kToolCommand).toString() == armed) {
                    lit = action;
                    break;
                }

        // THEN THE COMMAND, by its FIRST WORD. A button's property may be a whole
        // line (`YAY yontem=3n`); resolving all of it as a name found nothing, so
        // no method tool ever lit and the select arrow lit instead — which, from
        // the chair, is a tool that gets selected and then dropped.
        if (lit == nullptr)
            for (QAction* action : drawingTools_->actions()) {
                const QVariant carried = action->property(kToolCommand);
                if (!carried.isValid()) continue;
                const QString word = carried.toString().section(QLatin1Char(' '), 0, 0);
                const command::CommandSpec* spec =
                    controller_->registry().resolve(word.toStdString());
                if (spec && spec->id == running->id) {
                    lit = action;
                    break;
                }
            }
    }

    // Nothing drawing — or a command no tool button sends, such as one typed at
    // the command line — leaves the select tool lit, which is what "no modal tool
    // is armed" looks like.
    //
    // No signal guard is needed and none is written: every tool dispatches from
    // `triggered`, which `setChecked` does not emit, so lighting a button here
    // cannot re-run the command it stands for. A tool that ever moves to
    // `toggled` has to revisit this.
    if (lit)
        lit->setChecked(true);
    else
        actSelect_->setChecked(true);
}

void MainWindow::onPromptChanged(const QString& prompt)
{
    commandLine_->setPrompt(prompt);
    // A command that starts asking puts the editor tabs away; one that ends
    // brings them back for what is still selected.
    refreshContextTabs();

    // The first line of a new drawing has nothing on the stack to undo, and a
    // disabled action takes no shortcut: Ctrl+Z has to be live for the point.
    actUndo_->setEnabled(canUndo_ || controller_->canRetract());

    // WHERE THE KEYBOARD GOES WHEN THE MOUSE CANNOT ANSWER.
    //
    // A command waiting for a POINT or for OBJECTS is answered by pointing, and
    // focus belongs on the canvas — that is where Esc, the arrow keys and the
    // rubber band live. A command waiting for a NAME or a NUMBER cannot be
    // answered by pointing at anything, and until now nothing moved: pressing
    // Blok Ekle put "Yerleştirilecek bloğun adı" on the status line with the
    // focus still on the canvas, so typing went nowhere and the button looked
    // dead. That is what the user meant by the block and measuring tools not
    // working.
    //
    // The command says what it wants and the shell decides where the keyboard
    // has to be; no command looks at `InputSource` (command.md P10).
    const command::Session* live = controller_->session();
    if (live != nullptr && live->waiting()) {
        const command::Prompt& asked = live->prompt();
        const bool typed             = asked.kind == command::ParamKind::Text ||
                           asked.kind == command::ParamKind::Number ||
                           asked.kind == command::ParamKind::Integer;
        // UNLESS THE WORDS ARE ALREADY BEING TYPED WHERE THEY GO: the box over
        // a caption opened for this question (`MapCanvas::editTextAt`) keeps
        // the keyboard.
        if (typed && !canvas_->textEditorOpen()) {
            showCommandLine(true);
            commandLine_->setFocus(Qt::OtherFocusReason);
        }

        // AND THE WORDS THAT WOULD ANSWER IT, when the command knows the set:
        // the blocks in the drawing, the layers, a verb list. The shell offers
        // what the command named and nothing it did not (`Prompt::choices`).
        QStringList words;
        for (const std::string& word : asked.choices)
            words << QString::fromStdString(word);
        commandLine_->offerChoices(words);
    } else {
        commandLine_->offerChoices(QStringList());
    }

    syncToolSelection();
    canvas_->update();
}

void MainWindow::onInteractiveFinished(const QString& id, bool mutated, bool dismissed)
{
    // A TOOL IS MODAL. Picking `ALAN`, drawing a parsel and being dropped back on
    // the select tool means reaching for the tool column again before every
    // single parcel, and a cadastral sheet is hundreds of them. So a tool stays
    // armed until the user puts it down: a run that finished on its own or that
    // the right button closed arms the tool again; Esc, the select arrow and
    // picking another tool dismiss it (`Controller::interactiveFinished`).
    //
    // The wizard asked for a zoom once its import lands (see openImportWizard).
    if (zoomAfterImport_ && id == QLatin1String("core.import")) {
        zoomAfterImport_ = false;
        if (mutated) controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
    }
    if (dismissed) return;

    // A GRIP EDIT RE-ARMS NOTHING: a corner clicked and put down is one edit,
    // not a tool picked up, and the selection it was made on stays for the
    // next grip (TODOS C-07). Re-arming KÖŞETAŞI would clear it.
    if (controller_->oneShot()) return;

    // THE SAME TOOL, NOT THE FAMILY'S FIRST ONE. A method tool carries a whole
    // line (`ÇOKGEN yontem=dis`), and resolving all of it as a command NAME finds
    // nothing — so the loop walked on and matched the plain `ÇOKGEN`, whose name
    // does resolve to the same command id. The user picked "Çokgen — dıştan",
    // drew one, and the next one was silently the inscribed one: the tool they
    // chose was replaced by the default. That is what "the default tool gets
    // selected again after drawing" was.
    //
    // `armedLine()` is the line that actually started this run, so a button
    // carrying exactly it is the button that ran (the same test
    // `syncToolSelection` uses to decide which one to light).
    const QString armed = controller_->armedLine();
    if (!armed.isEmpty())
        for (QAction* action : drawingTools_->actions()) {
            if (!action->property(kToolRepeats).toBool() || !action->isEnabled()) continue;
            if (action->property(kToolCommand).toString() != armed) continue;
            rearm(action, id);
            return;
        }

    // A METHOD NO BUTTON CARRIES comes back as the line itself — `YAY
    // yontem=bby yon=sag` typed at the command line has no tool of its own, and
    // walking on to the family's first button put the plain one back (TODOS
    // C-02). Only for a tool that repeats at all: the loop below is what knows.
    const bool carries_method = armed.contains(QLatin1Char(' '));
    if (carries_method)
        for (const QAction* action : drawingTools_->actions()) {
            if (!action->property(kToolRepeats).toBool() || !action->isEnabled()) continue;
            const QString word =
                action->property(kToolCommand).toString().section(QLatin1Char(' '), 0, 0);
            const command::CommandSpec* spec = controller_->registry().resolve(word.toStdString());
            if (spec == nullptr || spec->id != id.toStdString()) continue;
            rearmLine(armed);
            return;
        }

    for (QAction* action : drawingTools_->actions()) {
        if (!action->property(kToolRepeats).toBool()) continue;

        const QVariant carried = action->property(kToolCommand);
        if (!carried.isValid() || !action->isEnabled()) continue;

        // BY THE FIRST WORD, for the same reason: a button may carry a line.
        const QString word               = carried.toString().section(QLatin1Char(' '), 0, 0);
        const command::CommandSpec* spec = controller_->registry().resolve(word.toStdString());
        if (spec == nullptr || spec->id != id.toStdString()) continue;

        rearm(action, id);
        return;
    }
}

void MainWindow::rearm(QAction* action, const QString& id)
{
    // A TOOL THAT STARTS BY ASKING WHICH OBJECTS asks again from nothing: with
    // the selection left standing, a re-armed TAŞI would take the same objects
    // and ask for a base point the instant the move landed, and a re-armed
    // ALANÖLÇ would measure the same parcel for ever.
    //
    // ANY SELECTION PARAMETER, not only the first: TARAMA lists its corners
    // before its objects (so a typed run of points fills them), and a re-armed
    // TARAMA took the parcel it had just hatched and hatched it again — two
    // hatches for one click (TODOS C-11).
    const command::CommandSpec* spec = controller_->registry().resolve(id.toStdString());
    const bool wants_objects =
        spec != nullptr && std::ranges::any_of(spec->params, [](const command::Param& p) {
            return p.kind == command::ParamKind::Selection;
        });
    if (wants_objects && !controller_->bus().selection().empty())
        controller_->runLine(QStringLiteral("SEÇ TEMİZLE"), command::Origin::Gui);

    // Queued, not called: this runs inside the finishing command's own signal,
    // and starting the next session on top of the one being torn down is how a
    // coroutine gets resumed after its frame is gone.
    //
    // AND IT STANDS DOWN IF ANYTHING GOT THERE FIRST. A queued call is not an
    // immediate one: on macOS it was seen to wait for the next input event, 800
    // ms later, by which time the user had typed PAH and given it a corner — and
    // the late re-arm then cancelled PAH and put ÇOKLUÇİZGİ back. A tool that
    // comes back over the command the user reached for since is the tool column
    // overriding the user, so the re-arm happens only if nothing has started
    // since it was queued and nothing is waiting now.
    const std::uint64_t ticket = controller_->sessionsBegun();
    QMetaObject::invokeMethod(
        this,
        [this, action, ticket] {
            if (controller_->sessionsBegun() != ticket || controller_->session() != nullptr) return;
            action->trigger();
        },
        Qt::QueuedConnection);
}

void MainWindow::rearmLine(const QString& line)
{
    // The same queued, stand-down-if-anything-started re-arm as `rearm`, for a
    // line rather than a button.
    const std::uint64_t ticket = controller_->sessionsBegun();
    QMetaObject::invokeMethod(
        this,
        [this, line, ticket] {
            if (controller_->sessionsBegun() != ticket || controller_->session() != nullptr) return;
            controller_->beginInteractive(line);
        },
        Qt::QueuedConnection);
}

void MainWindow::onUndoStateChanged(bool canUndo, bool canRedo)
{
    canUndo_ = canUndo;
    actUndo_->setEnabled(canUndo_ || controller_->canRetract());
    actRedo_->setEnabled(canRedo);
}

void MainWindow::onCursorMoved(core::Point2 world)
{
    // Turkish surveying convention, which EPSG:5254 itself declares: Y is the
    // easting (`sağa değer`) and X is the northing (`yukarı değer`). Storage is
    // unaffected — Point2::x holds the easting either way (.claude/model.md R37a).
    // The strip prints it the way a surveyor reads a coordinate off an
    // instrument: the axis letter, then the number, digit-aligned in mono.
    statusStrip_->setCoordinate(
        tr("Y %1  X %2").arg(format_metres(world.x), format_metres(world.y)));
}

void MainWindow::onViewRequested(const QString& mode, double factor)
{
    if (mode == QLatin1String("KAPSAM") || mode == QLatin1String("EXTENTS"))
        canvas_->zoomToExtents();
    else if (mode == QLatin1String("SIFIRLA") || mode == QLatin1String("RESET"))
        canvas_->resetView();
    else
        canvas_->zoomBy(factor);
    refreshStatus();
}

void MainWindow::onPanRequested(core::Point2 from, core::Point2 to)
{
    // The centre moves by the OPPOSITE of the grab: dragging a corner to the
    // right walks the view to the left, which is what "holding the paper" means.
    const render::ViewTransform& v = canvas_->view();
    canvas_->setCentre(
        core::Point2{v.centre().x - (to.x - from.x), v.centre().y - (to.y - from.y)});
    refreshStatus();
}

void MainWindow::onCommandSubmitted(const QString& line)
{
    onEcho(QStringLiteral("> ") + line);
    controller_->runLine(line, command::Origin::CommandLine);
}

void MainWindow::refreshStatus()
{

    const core::Crs& crs = controller_->document().crs();

    // design.md 7 prints the reading a surveyor checks before they draw: the EPSG
    // code, the realisation and the projection zone. An unresolved CRS says so in
    // words rather than showing a bare id that looks resolved.
    QString crsText;
    if (crs.empty()) {
        crsText = tr("tanımsız");
    } else if (crs.resolved()) {
        crsText = tr("EPSG:%1 · %2").arg(crs.epsg()).arg(QString::fromStdString(crs.id()));
        if (!crs.epoch().empty())
            crsText =
                tr("EPSG:%1 · %2 / %3")
                    .arg(crs.epsg())
                    .arg(QString::fromStdString(crs.epoch()), QString::fromStdString(crs.id()));
    } else if (crs.id() == "YEREL" || crs.id() == "LOCAL") {
        // A LOCAL drawing is not a broken one: the crew called their station 0,0
        // and every distance in it is right. What it is not, yet, is anywhere on
        // the map — and the status line has to say that plainly, because a
        // coordinate read off it means nothing to anyone else until OTURT runs.
        crsText = tr("YEREL · haritaya oturtulmadı");
    } else {
        crsText = tr("%1 · çözümlenmedi").arg(QString::fromStdString(crs.id()));
    }
    statusStrip_->setCrs(crsText);

    // Every chip re-reads its own setting from the store its scope lives in, so
    // the strip agrees with the store whoever wrote it — the F-keys, the command
    // line, a script or the AI. The two mask chips read the snap mask.
    {
        const core::Settings& app     = controller_->bus().app_settings();
        const core::Settings& session = controller_->bus().session_settings();
        for (const char* id :
             {"core.izgara.gorunur", "core.arayuz.dinamik_girdi", "core.harita.kalinlik"})
            statusStrip_->setToggle(QString::fromLatin1(id), app.get(id).as_bool());
        for (const char* id :
             {"core.yakalama.izgara", "core.yakalama.dik_mod", "core.yakalama.yuzey_normali"})
            statusStrip_->setToggle(QString::fromLatin1(id), session.get(id).as_bool());
        const int mask = static_cast<int>(session.get("core.yakalama.modlar").as_int());
        statusStrip_->setToggle(QString::fromLatin1(kChipOsnap),
                                (static_cast<unsigned>(mask) & core::SnapObjectMask) != 0U);
        statusStrip_->setToggle(QString::fromLatin1(kChipPolar),
                                (static_cast<unsigned>(mask) & core::SnapPolar) != 0U);
    }

    const io::DatabaseService& db = controller_->database();
    statusStrip_->setConnection(db.connected()
                                    ? tr("PostGIS · %1").arg(QString::fromStdString(db.target()))
                                : io::DatabaseService::available() ? tr("PostGIS · bağlı değil")
                                                                   : tr("PostGIS · bu yapıda yok"),
                                db.connected());
    statusStrip_->setPerformance(canvas_->backendName());

    // The PLOT scale, not a pixel size: how many ground millimetres one paper
    // millimetre carries. That is the number printed in a layout's title block and
    // the number an engineer means by "ölçek".
    const double ground_mm_per_paper_mm =
        canvas_->view().mm_per_pixel() * canvas_->pixelsPerPaperMm();

    statusStrip_->setScale(ground_mm_per_paper_mm > 0.0
                               ? tr("1 : %1").arg(groupedNumber(qRound(ground_mm_per_paper_mm)))
                               : QStringLiteral("—"));
}

void MainWindow::runScriptLine(const QString& line)
{
    controller_->runLine(line, command::Origin::Gui);
}

void MainWindow::endCommand()
{
    controller_->finishInteractive();
}

void MainWindow::cancelCommand()
{
    controller_->cancelInteractive();
}

void MainWindow::runScriptFile(const QString& path)
{
    if (path.isEmpty()) return;

    controller_->runLine(QStringLiteral("BETİK \"%1\"").arg(path), command::Origin::Gui);

    // Bring the result into view. A second command on the same bus, not a special
    // case reaching into the canvas (CLAUDE.md Article 1).
    controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
}

QString MainWindow::externalFormatFilter(bool for_writing) const
{
    QStringList entries;
    for (const io::VectorFormat& f : io::vector_formats()) {
        if (for_writing ? !f.write : !f.read) continue;
        entries << QStringLiteral("%1 (*%2)")
                       .arg(QString::fromStdString(f.label), QString::fromStdString(f.extension));
    }

    // DWG IS NOT A GDAL DRIVER HERE. It is read by LibreDWG and routed by
    // extension in `FileService::import_into`, so it is absent from the
    // allow-list the loop above walks — and a format the file dialog does not
    // offer is a format the user has no way to know exists. Reading only: io.md
    // P8 forbids a native DWG writer, so it must never appear in a save dialog.
    if (!for_writing && io::dwg_backend_available()) entries << tr("AutoCAD DWG (*.dwg)");

    entries << tr("Tüm dosyalar (*)");
    return entries.join(QStringLiteral(";;"));
}

void MainWindow::refreshWindowTitle()
{
    const QString file = controller_->currentFile();
    const QString name = file.isEmpty() ? tr("adsız") : QFileInfo(file).fileName();
    // `design.md` §7: `<document> — KentOSCad <version>`, in the system's own
    // caption now that the ribbon is the only bar under it.
    setWindowTitle(tr("%1 — KentOSCad %2").arg(name, QStringLiteral(KENTOS_VERSION)));
}

bool MainWindow::confirmDiscard(const QString& question)
{
    if (!controller_->isDirty()) return true;

    // THREE ANSWERS, and the third one is the point: a person who reaches for
    // the wrong control by accident must be able to say "no, I did not mean
    // that". A two-button dialog with Save and Discard makes the accident
    // unrecoverable.
    const QString name = controller_->currentFile().isEmpty()
                             ? tr("Adsız çizim")
                             : QFileInfo(controller_->currentFile()).fileName();

    const auto answer = QMessageBox::question(
        this, tr("Kaydedilsin mi?"),
        tr("%1 üzerinde kaydedilmemiş değişiklikler var.\n\n%2").arg(name, question),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    if (answer == QMessageBox::Cancel) return false;

    if (answer == QMessageBox::Save) {
        // `saveProject` already turns "no file yet" into the Save As dialog, and
        // that dialog can itself be cancelled — a cancelled save must not become
        // a silent discard, which is what the check below is for.
        saveProject();
        if (controller_->isDirty()) return false;
    }
    return true;
}

void MainWindow::newProject()
{
    if (!settleBlockEdit(tr("Yeni çizime geçmeden önce")) ||
        !confirmDiscard(tr("Yeni çizime geçmeden önce kaydedilsin mi?")))
        return;

    // AND THEN THE COMMAND, exactly as typed. The window's whole contribution is
    // the question above it: the drawing, the undo stack, the file the document
    // belonged to and the view are reset by `YENİ` itself, so a script gets the
    // same reset without a dialog it could not answer (Article 1.2).
    controller_->runLine(QStringLiteral("YENİ"), command::Origin::Gui);
    refreshWindowTitle();
}

void MainWindow::openProject()
{
    if (!settleBlockEdit(tr("Başka bir proje açılmadan önce"))) return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Proje aç"), QFileInfo(controller_->currentFile()).absolutePath(),
        tr("KentOSCad projesi (*.pcad);;Tüm dosyalar (*)"));
    if (path.isEmpty()) return;

    controller_->runLine(QStringLiteral("AÇ \"%1\"").arg(path), command::Origin::Gui);
    controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
    refreshWindowTitle();
}

void MainWindow::saveProject()
{
    // AN OPEN BLOCK EDIT IS FINISHED FIRST: saved mid-edit, the file would hold
    // the opened objects on the sheet and the reference hidden, with nothing to
    // say they belong together.
    if (!settleBlockEdit(tr("Proje kaydedilmeden önce"))) return;
    // A drawing with no file yet has nothing to save TO, and the command says so.
    // The window turns that into the dialog a user expects rather than showing
    // them an error they cannot act on from a menu.
    if (controller_->currentFile().isEmpty()) {
        saveProjectAs();
        return;
    }
    controller_->runLine(QStringLiteral("KAYDET"), command::Origin::Gui);
    refreshWindowTitle();
}

void MainWindow::saveProjectAs()
{
    if (!settleBlockEdit(tr("Proje kaydedilmeden önce"))) return;
    QString path = QFileDialog::getSaveFileName(
        this, tr("Farklı kaydet"), controller_->currentFile(), tr("KentOSCad projesi (*.pcad)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".pcad");

    controller_->runLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(path), command::Origin::Gui);
    refreshWindowTitle();
}

ImportWizard* MainWindow::openImportWizard(const QString& path)
{
    auto* wizard = new ImportWizard(*controller_, theme_, this);
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->applyTheme(theme_);

    // Non-blocking, so the caller keeps the event loop. The command line the
    // window builds still runs through the controller — the wizard states the
    // work and this runs it, exactly as the modal path does.
    connect(wizard, &QDialog::accepted, this, [this, wizard] {
        const QString line = wizard->commandLine();
        if (line.isEmpty()) return;
        // The import reads on a worker thread and finishes later, so the zoom
        // to what arrived waits for `interactiveFinished` (below) rather than
        // running now against a drawing the data has not reached yet.
        zoomAfterImport_ = true;
        controller_->runLine(line, command::Origin::Gui);
    });

    wizard->open();
    if (!path.isEmpty()) wizard->beginWith(path);
    return wizard;
}

void MainWindow::importData()
{
    if (!io::vector_backend_available()) {
        onEcho(QString::fromStdString(io::vector_backend_status()));
        return;
    }

    // A FILE DIALOG IS NOT ENOUGH FOR A DRAWING. A cadastral DXF holds forty
    // layers and the user wants six of them; picking the file and then deleting
    // thirty-four layers is not the same job. The wizard reads the file once,
    // shows it, and asks — then runs the one command line it built.
    ImportWizard wizard(*controller_, theme_, this);
    wizard.applyTheme(theme_);
    if (wizard.exec() != QDialog::Accepted) return;

    const QString line = wizard.commandLine();
    if (line.isEmpty()) return;

    controller_->runLine(line, command::Origin::Gui);
    controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
}

void MainWindow::exportData()
{
    if (!io::vector_backend_available()) {
        onEcho(QString::fromStdString(io::vector_backend_status()));
        return;
    }
    // ONE EXPORT WINDOW for everything that leaves the program: the format list
    // is the io layer's, the line it runs is shown, and a user who reads it has
    // learnt the command (export_dialog.hpp).
    ExportDialog window(*controller_, ExportSubject::Drawing, QString(), this);
    window.applyTheme(theme_);
    window.exec();
}

void MainWindow::rebuildLayoutMenu()
{
    if (layoutMenu_ == nullptr) return;
    // EMPTIED BY HAND, not with `clear()`. `clear()` deletes only the actions
    // no other widget shows, and every one of these is shown elsewhere too — in
    // the quick access row's copy of this list — so each rebuild left the last
    // one's actions alive, and with them a second `Ctrl+Shift+P` on the window
    // that made the key trigger neither.
    for (QAction* action : layoutMenu_->actions()) {
        layoutMenu_->removeAction(action);
        if (action == actLayoutManager_) continue;
        if (QMenu* sub = action->menu(); sub != nullptr && sub->parent() == layoutMenu_)
            sub->deleteLater();
        else if (action->parent() == layoutMenu_)
            action->deleteLater();
    }

    QAction* fresh = layoutMenu_->addAction(tr("Yeni Çıktı Yerleşimi…"));
    fresh->setStatusTip(
        tr("ÇIKTIYERLEŞİMİ islem=ekle — başlık, harita, ölçek çubuğu ve kuzey oku ile "
           "gelir"));
    connect(fresh, &QAction::triggered, this, [this] { newLayout(); });

    // THE ONE ENTRY WITH A KEY is made once and kept: a key belongs to one
    // action for the life of the window, not to whichever list was built last.
    if (actLayoutManager_ == nullptr) {
        actLayoutManager_ = new QAction(tr("Çıktı Yerleşimi Yöneticisi…"), this);
        actLayoutManager_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
        actLayoutManager_->setStatusTip(
            tr("Çizimdeki çıktı yerleşimlerini listeler: aç, yeniden adlandır, çoğalt, sil"));
        connect(actLayoutManager_, &QAction::triggered, this, &MainWindow::openLayoutManager);
        addAction(actLayoutManager_);
    }
    layoutMenu_->addAction(actLayoutManager_);

    // ---- the office's templates ---------------------------------------------
    //
    // A SUBMENU RATHER THAN A WINDOW, because the whole interaction is "make a
    // sheet like the one we always use": a list of names, and applying one. The
    // two verbs that are not that — saving the current sheet, throwing a
    // template away — sit under it rather than needing a manager of their own.
    QMenu* templates          = layoutMenu_->addMenu(tr("Şablonlar"));
    const QStringList library = controller_->layoutTemplates().names();
    if (library.isEmpty()) {
        QAction* none = templates->addAction(tr("(kayıtlı şablon yok)"));
        none->setEnabled(false);
    } else {
        for (const QString& one : library) {
            QAction* apply = templates->addAction(one);
            apply->setStatusTip(tr("ÇIKTIŞABLON islem=uygula — bu çizimde bu şablondan bir "
                                   "yerleşim kurar"));
            connect(apply, &QAction::triggered, this, [this, one] { applyLayoutTemplate(one); });
        }
    }
    templates->addSeparator();
    QAction* store = templates->addAction(tr("Yerleşimi Şablon Olarak Kaydet…"));
    store->setEnabled(!controller_->document().layouts().empty());
    connect(store, &QAction::triggered, this, &MainWindow::saveLayoutTemplate);

    const core::LayoutStore& sheets = controller_->document().layouts();
    if (sheets.empty()) {
        layoutMenu_->addSeparator();
        QAction* none = layoutMenu_->addAction(tr("(çizimde çıktı yerleşimi yok)"));
        none->setEnabled(false);
        return;
    }

    // ---- one entry per sheet, and a submenu for each ------------------------
    //
    // TWO WAYS INTO ONE SHEET, because they are two different intentions:
    // `Tasarımcıyı aç` is "let me arrange the page", and `Tuvalden alan seç` is
    // "let me say what the map looks at". The second is the flow the print
    // menu's arrow offers, repeated here so a user who lives in the menu bar is
    // not sent to the toolbar to find it.
    layoutMenu_->addSeparator();
    for (const core::Layout& l : sheets.all()) {
        const QString name = QString::fromStdString(l.name);
        QMenu* one         = layoutMenu_->addMenu(name);
        one->setToolTip(QStringLiteral("%1 %2×%3 mm")
                            .arg(QString::fromStdString(l.paper))
                            .arg(l.pages.front().w / 1000)
                            .arg(l.pages.front().h / 1000));

        QAction* design = one->addAction(tr("Tasarımcıyı Aç"));
        connect(design, &QAction::triggered, this, [this, name] { openLayoutDesigner(name); });

        QAction* aim = one->addAction(tr("Tuvalden Alan Seç…"));
        aim->setStatusTip(tr("Haritanın bakacağı alanı tuvalden çerçeveleyin; sonra tasarımcı "
                             "açılır"));
        connect(aim, &QAction::triggered, this, [this, name] { layoutWithFrame(name); });

        one->addSeparator();
        QAction* pdf = one->addAction(tr("PDF'e Aktar…"));
        connect(pdf, &QAction::triggered, this, [this, name] { exportLayout(name); });
    }
}

/// Every entry of the toolbar's print list, as a menu walk would find it.
///
/// Raises `aboutToShow` for the same reason `probeLayoutMenu` does: a menu
/// nobody has opened is a state a user never sees, so reading it would test the
/// wrong thing — and this list's whole defect was that it was NOT rebuilt when
/// it opened.
QStringList MainWindow::probePrintMenu()
{
    QStringList out;
    if (printMenu_ == nullptr) return {QStringLiteral("yazdırma listesi yok")};
    emit printMenu_->aboutToShow();
    for (QAction* action : printMenu_->actions()) {
        if (action->isSeparator()) continue;
        out << action->text().trimmed();
    }
    return out;
}

QStringList MainWindow::probeLayoutMenu()
{
    QStringList out;
    if (layoutMenu_ == nullptr) return {QStringLiteral("Çıktı Yerleşimleri menüsü yok")};

    // THE MENU IS BUILT ON `aboutToShow`, so the probe raises that signal rather
    // than reading a menu nobody has opened — which is exactly the state a user
    // never sees and therefore the wrong thing to test.
    emit layoutMenu_->aboutToShow();

    for (QAction* action : layoutMenu_->actions()) {
        if (action->isSeparator()) continue;
        out << QStringLiteral("%1%2").arg(
            action->text(), action->isEnabled() ? QString() : QStringLiteral(" [kapalı]"));
        if (QMenu* sub = action->menu(); sub != nullptr)
            for (QAction* inner : sub->actions())
                if (!inner->isSeparator()) out << QStringLiteral("    %1").arg(inner->text());
    }
    return out;
}

void MainWindow::applyLayoutTemplate(const QString& templateName)
{
    bool accepted = false;
    const QString named =
        QInputDialog::getText(this, tr("Şablondan yerleşim"), tr("Kurulacak yerleşimin adı:"),
                              QLineEdit::Normal, templateName, &accepted);
    if (!accepted || named.trimmed().isEmpty()) return;

    QString sheet = named.trimmed();
    sheet.replace('\\', QStringLiteral("\\\\"));
    sheet.replace('"', QStringLiteral("\\\""));
    QString from = templateName;
    from.replace('\\', QStringLiteral("\\\\"));
    from.replace('"', QStringLiteral("\\\""));

    controller_->runLine(
        QStringLiteral("ÇIKTIŞABLON islem=uygula ad=\"%1\" yerleşim=\"%2\"").arg(from, sheet),
        command::Origin::Gui);
    // STRAIGHT INTO THE DESIGNER: a sheet made from a template still needs its
    // map aimed, and that is the next thing the user was going to do.
    if (controller_->document().layouts().find(named.trimmed().toStdString()) != nullptr)
        openLayoutDesigner(named.trimmed());
}

void MainWindow::saveLayoutTemplate()
{
    const core::LayoutStore& sheets = controller_->document().layouts();
    if (sheets.empty()) return;

    QStringList choices;
    for (const core::Layout& l : sheets.all())
        choices << QString::fromStdString(l.name);

    bool accepted = false;
    const QString from =
        choices.size() == 1
            ? choices.front()
            : QInputDialog::getItem(this, tr("Şablon olarak kaydet"), tr("Hangi yerleşim:"),
                                    choices, 0, false, &accepted);
    if (choices.size() > 1 && !accepted) return;

    const QString named = QInputDialog::getText(
        this, tr("Şablon olarak kaydet"), tr("Şablonun adı:"), QLineEdit::Normal, from, &accepted);
    if (!accepted || named.trimmed().isEmpty()) return;

    QString sheet = from;
    sheet.replace('\\', QStringLiteral("\\\\"));
    sheet.replace('"', QStringLiteral("\\\""));
    QString as = named.trimmed();
    as.replace('\\', QStringLiteral("\\\\"));
    as.replace('"', QStringLiteral("\\\""));

    controller_->runLine(
        QStringLiteral("ÇIKTIŞABLON islem=kaydet ad=\"%1\" yerleşim=\"%2\"").arg(as, sheet),
        command::Origin::Gui);
}

void MainWindow::openLayoutManager()
{
    LayoutManager manager(*controller_, this);
    manager.applyTheme(theme_);
    // QUEUED, so the manager is closed before the designer opens: two modal
    // windows stacked on each other is how a user loses track of which one the
    // Escape key answers.
    QString wanted;
    connect(&manager, &LayoutManager::openRequested, this,
            [&wanted](const QString& name) { wanted = name; });
    manager.exec();
    if (!wanted.isEmpty()) openLayoutDesigner(wanted);
}

void MainWindow::exportLayout(const QString& layout)
{
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Yerleşimi PDF olarak kaydet"),
                                     layout + QStringLiteral(".pdf"), tr("PDF (*.pdf)"));
    if (path.isEmpty()) return;

    QString quotedName = layout;
    quotedName.replace('\\', QStringLiteral("\\\\"));
    quotedName.replace('"', QStringLiteral("\\\""));
    QString quotedPath = path;
    quotedPath.replace('\\', QStringLiteral("\\\\"));
    quotedPath.replace('"', QStringLiteral("\\\""));
    controller_->runLine(
        QStringLiteral("YAZDIR yerlesim=\"%1\" dosya=\"%2\"").arg(quotedName, quotedPath),
        command::Origin::Gui);
}

void MainWindow::rebuildPrintMenu()
{
    if (printMenu_ == nullptr) return;
    printMenu_->clear();

    const io::PrintProfiles& store = controller_->printService().profiles();
    for (const io::PrintProfile& p : store.all()) {
        const QString name  = QString::fromStdString(p.name);
        const bool fallback = controller_->printService().profiles().default_name() == p.name;
        // The default is MARKED rather than named in the label: a bullet beside
        // one row of a list is read at a glance, and the word "varsayılan" in
        // the row would make the list read as the profile's name.
        QAction* entry = printMenu_->addAction(fallback ? tr("● %1").arg(name)
                                                        : QStringLiteral("    %1").arg(name));
        entry->setToolTip(QString::fromStdString(io::describe_print_profile(p)));
        connect(entry, &QAction::triggered, this, [this, name] { printWithProfile(name); });
    }
    // ---- and the drawing's own layouts ---------------------------------------
    //
    // THE SHEET SITS BESIDE THE PROFILES because that is where a user looks for
    // "what am I printing onto". A profile is a blank sheet of paper; a layout is
    // a sheet with a title block, a legend and a map frame already on it. Picking
    // either starts the same gesture — drag a rectangle on the drawing — and the
    // difference is only what opens afterwards.
    if (const core::LayoutStore& sheets = controller_->document().layouts(); !sheets.empty()) {
        printMenu_->addSeparator();
        auto* heading = printMenu_->addAction(tr("Çıktı Yerleşimleri"));
        heading->setEnabled(false);
        for (const core::Layout& l : sheets.all()) {
            const QString name = QString::fromStdString(l.name);
            QAction* entry     = printMenu_->addAction(QStringLiteral("    %1").arg(name));
            entry->setToolTip(tr("%1 — %2×%3 mm, %4 öğe. Seçince tuvalden alan seçilir ve "
                                 "tasarımcı açılır.")
                                  .arg(QString::fromStdString(l.paper))
                                  .arg(l.pages.front().w / 1000)
                                  .arg(l.pages.front().h / 1000)
                                  .arg(l.items.size()));
            connect(entry, &QAction::triggered, this, [this, name] { layoutWithFrame(name); });
        }
        QAction* fresh = printMenu_->addAction(tr("    Yeni çıktı yerleşimi…"));
        connect(fresh, &QAction::triggered, this, [this] { newLayout(); });
    } else {
        printMenu_->addSeparator();
        QAction* fresh = printMenu_->addAction(tr("Yeni çıktı yerleşimi…"));
        fresh->setToolTip(
            tr("ÇIKTIYERLEŞİMİ islem=ekle — başlık, harita, ölçek çubuğu ve kuzey oku "
               "ile gelir"));
        connect(fresh, &QAction::triggered, this, [this] { newLayout(); });
    }

    printMenu_->addSeparator();
    QAction* manage = printMenu_->addAction(tr("Profilleri Yönet…"));
    manage->setToolTip(tr("YAZDIRMAPROFİLİ — Seçenekler ▸ Plot ve Çıktı"));
    connect(manage, &QAction::triggered, this,
            [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); });
}

void MainWindow::printWithProfile(const QString& profile)
{
    if (canvas_ == nullptr) return;

    // AN EXPLICIT PROFILE PICK IS A CHANGE OF MIND. Choosing a paper from the
    // menu after having chosen a layout means the user wants the paper; the
    // pending sheet is dropped rather than quietly overriding what they just
    // clicked.
    if (!profile.isEmpty()) pendingLayout_.clear();

    // THE SECOND PRESS CAPTURES. A frame that is already up is the user's aim;
    // taking it is what the button means then, and opening a second frame would
    // throw away the aiming they just did.
    if (canvas_->printFraming()) {
        const core::Box2 window = canvas_->printFrameWindow();
        canvas_->endPrintFrame();

        // AND IT CAPTURES INTO WHATEVER THE FRAME WAS STARTED FOR. A frame begun
        // from a layout belongs to that layout, whichever control the user
        // presses to take it — the toolbar's printer icon is the obvious one to
        // reach for, and it used to open the plain print dialog instead, throwing
        // the sheet away without a word. Pressing the layout's own menu entry
        // again worked, which made the icon look broken rather than different.
        if (!pendingLayout_.isEmpty()) {
            const QString sheet = pendingLayout_;
            pendingLayout_.clear();
            openLayoutDesigner(sheet, window);
            return;
        }
        openPrintDialog(window, printProfile_);
        return;
    }

    const io::PrintProfiles& store = controller_->printService().profiles();
    const io::PrintProfile* p =
        profile.isEmpty() ? store.fallback() : store.find(profile.toStdString());
    if (p == nullptr) {
        onEcho(tr("Yazdırma profili yok: YAZDIRMAPROFİLİ ekle ile bir profil tanımlayın."));
        return;
    }
    printProfile_ = QString::fromStdString(p->name);

    const auto w = static_cast<double>(p->printable_width_mm());
    const auto h = static_cast<double>(p->printable_height_mm());
    if (w <= 0.0 || h <= 0.0) {
        onEcho(tr("'%1' profilinde kâğıtta yazdırılacak alan kalmıyor; kenar boşluğunu "
                  "küçültün.")
                   .arg(printProfile_));
        return;
    }
    canvas_->beginPrintFrame(w / h);
}

void MainWindow::layoutWithFrame(const QString& layout)
{
    if (canvas_ == nullptr) return;

    // THE SECOND PRESS CAPTURES, exactly as it does for a plain print: a frame
    // already up is the user's aim, and starting a new one would throw it away.
    if (canvas_->printFraming() && !pendingLayout_.isEmpty()) {
        const core::Box2 window = canvas_->printFrameWindow();
        const QString sheet     = pendingLayout_;
        pendingLayout_.clear();
        canvas_->endPrintFrame();
        openLayoutDesigner(sheet, window);
        return;
    }

    const core::Layout* l = controller_->document().layouts().find(layout.toStdString());
    if (l == nullptr) {
        onEcho(tr("Çıktı yerleşimi yok: %1").arg(layout));
        return;
    }
    const core::LayoutItem* map = l->first_map();
    if (map == nullptr) {
        // NO MAP FRAME MEANS NOTHING TO AIM. Opening the designer is the useful
        // answer — that is where one is added — rather than a refusal.
        onEcho(tr("'%1' yerleşiminde harita çerçevesi yok; tasarımcıda ekleyebilirsiniz.")
                   .arg(layout));
        openLayoutDesigner(layout);
        return;
    }

    pendingLayout_ = layout;
    // THE FRAME TAKES THE MAP ITEM'S ASPECT, not the paper's: what the user
    // drags is what the MAP will hold, and the paper around it is title block
    // and legend. Framing at the paper's aspect would put ground on the sheet
    // that the map frame never shows.
    const double aspect = static_cast<double>(map->frame.w) /
                          static_cast<double>(std::max<core::Um>(1, map->frame.h));
    canvas_->beginPrintFrame(aspect);
}

void MainWindow::probeBeginLayoutFrame(const QString& layout)
{
    // THE REAL ENTRY POINT, not a shortcut past it: this is the function the
    // menu's layout row is connected to, so the probe exercises the road a user
    // takes rather than a parallel one that could drift from it.
    layoutWithFrame(layout);
}

void MainWindow::probeBeginPlainFrame()
{
    // THE FRAME UP IS DROPPED FIRST. With one still up, `printWithProfile` takes
    // it — that is its job — and opens a MODAL dialog, which a headless probe
    // cannot answer. Ending it here is what a user pressing Escape does, and it
    // leaves the function to do the thing being tested: start a fresh frame for
    // a paper profile.
    if (canvas_ != nullptr && canvas_->printFraming()) canvas_->endPrintFrame();

    const std::string fallback = controller_->printService().profiles().default_name();
    printWithProfile(fallback.empty() ? QString() : QString::fromStdString(fallback));
}

QString MainWindow::probeFrameDestination() const
{
    if (canvas_ == nullptr || !canvas_->printFraming()) return {};
    return pendingLayout_.isEmpty() ? QStringLiteral("yazdir")
                                    : QStringLiteral("yerlesim:") + pendingLayout_;
}

bool MainWindow::isProbeRun()
{
    return probe_run();
}

QStringList MainWindow::probeDockDrag()
{
    QStringList said;
    if (propertyDock_ == nullptr || propertyHeader_ == nullptr) return {QStringLiteral("dock yok")};

    // THE LAYOUT IS PUT BACK EXACTLY AS IT WAS FOUND.
    //
    // This probe floats a panel, drags it across the screen and re-docks it,
    // and the shell saves its dock layout on the way out — so without this the
    // arrangement this probe happened to leave behind became the user's, and
    // the panels it did not touch came back in the wrong places or not at all.
    // Test mode keeps it out of the real profile; this keeps it out of the
    // running program's own state, which the rest of the probe goes on to use.
    const QByteArray before_state = saveState(kLayoutVersion);

    propertyDock_->setFloating(true);
    propertyDock_->move(320, 240);
    QCoreApplication::processEvents();

    // THE HANDLE, ASKED OF THE HEADER rather than guessed. A guessed point that
    // lands on a tab tests the tab — which is exactly how the first version of
    // this probe passed while the panel still could not be dragged.
    PanelHeader* bar  = propertyHeader_;
    const QPoint bare = bar->handlePoint();
    if (bare.x() < 0) return {QStringLiteral("başlıkta tutamak yok")};

    const QPoint before = propertyDock_->pos();
    const auto atScreen = [&](QPoint local) { return bar->mapToGlobal(local); };

    const auto send = [&](QEvent::Type what, QPoint local, Qt::MouseButton button,
                          Qt::MouseButtons held) {
        QMouseEvent event(what, QPointF(local), QPointF(atScreen(local)), button, held,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(bar, &event);
        return event.isAccepted();
    };

    // COUNTED AT THE DOCK. "The press was accepted" does not say WHO accepted
    // it; only a filter on the dock says whether it ever got there.
    struct Counter : QObject
    {
        int presses = 0, moves = 0;

        bool eventFilter(QObject*, QEvent* e) override
        {
            if (e->type() == QEvent::MouseButtonPress) ++presses;
            if (e->type() == QEvent::MouseMove) ++moves;
            return false;
        }
    } seen;

    propertyDock_->installEventFilter(&seen);

    const bool kept = send(QEvent::MouseButtonPress, bare, Qt::LeftButton, Qt::LeftButton);
    said << QStringLiteral("tutamaktaki basışı alan oldu mu: %1")
                .arg(kept ? QStringLiteral("evet") : QStringLiteral("hayır"));

    // Two moves: Qt's dock starts the drag on the first one that passes its
    // threshold and follows the pointer on the rest.
    (void)send(QEvent::MouseMove, bare + QPoint(30, 24), Qt::NoButton, Qt::LeftButton);
    (void)send(QEvent::MouseMove, bare + QPoint(90, 70), Qt::NoButton, Qt::LeftButton);
    QCoreApplication::processEvents();

    said << QStringLiteral("dock'a ulaşan: %1 basış, %2 hareket").arg(seen.presses).arg(seen.moves);
    propertyDock_->removeEventFilter(&seen);

    const QPoint after = propertyDock_->pos();
    said << QStringLiteral("panel %1,%2 → %3,%4")
                .arg(before.x())
                .arg(before.y())
                .arg(after.x())
                .arg(after.y());

    (void)send(QEvent::MouseButtonRelease, bare + QPoint(90, 70), Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();

    // AND A TAB STILL SWITCHES. Handing the bare strip back must not hand back
    // the parts that mean something else.
    // A TAB STILL SWITCHES. Handing the grip back must not hand back the parts
    // that mean something else — and the two overlap, because the marks are
    // painted over the strip the tabs claim.
    //
    // FROM A KNOWN TAB TO A DIFFERENT ONE. Pressing the tab that is already
    // current proves nothing.
    propertyHeader_->setCurrent(0);
    const QPoint onTab(12, bar->height() / 2);
    (void)send(QEvent::MouseButtonPress, onTab, Qt::LeftButton, Qt::LeftButton);
    (void)send(QEvent::MouseButtonRelease, onTab, Qt::LeftButton, Qt::NoButton);
    const int first = propertyHeader_->current();

    propertyHeader_->setCurrent(1);
    (void)send(QEvent::MouseButtonPress, onTab, Qt::LeftButton, Qt::LeftButton);
    (void)send(QEvent::MouseButtonRelease, onTab, Qt::LeftButton, Qt::NoButton);
    said << QStringLiteral("sekme: ilkine basınca %1, ikincideyken ilkine basınca %2")
                .arg(first)
                .arg(propertyHeader_->current());

    propertyDock_->setFloating(false);
    (void)restoreState(before_state, kLayoutVersion);
    QCoreApplication::processEvents();
    return said;
}

void MainWindow::openLayoutDesigner(const QString& layout, core::Box2 window)
{
    LayoutDesigner designer(*controller_, layout, this);
    designer.applyTheme(theme_);
    if (!window.empty()) designer.aimAt(window);
    designer.exec();
}

void MainWindow::newLayout()
{
    // The name is the only thing the command cannot guess, so it is the only
    // thing asked for; paper and orientation are the defaults and the designer
    // changes them.
    bool accepted       = false;
    const QString named = QInputDialog::getText(
        this, tr("Yeni çıktı yerleşimi"), tr("Yerleşim adı:"), QLineEdit::Normal,
        tr("Yerleşim %1").arg(controller_->document().layouts().size() + 1), &accepted);
    if (!accepted || named.trimmed().isEmpty()) return;

    QString quoted = named.trimmed();
    quoted.replace('\\', QStringLiteral("\\\\"));
    quoted.replace('"', QStringLiteral("\\\""));
    controller_->runLine(
        QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=\"%1\" kagit=A3 yon=yatay").arg(quoted),
        command::Origin::Gui);
    openLayoutDesigner(named.trimmed());
}

void MainWindow::openPrintDialog(core::Box2 window, const QString& profile)
{
    if (window.empty()) {
        onEcho(tr("Yazdırılacak alan boş."));
        return;
    }
    PrintDialog dialog(*controller_, window, profile, this);
    dialog.applyTheme(theme_);
    // The two ways out that are not a plot: the profile editor, and aiming
    // again on the canvas. Queued, so the dialog is closed before either opens.
    connect(
        &dialog, &PrintDialog::manageProfilesRequested, this,
        [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); }, Qt::QueuedConnection);
    connect(
        &dialog, &PrintDialog::reaimRequested, this, [this, profile] { printWithProfile(profile); },
        Qt::QueuedConnection);
    dialog.exec();
}

QString MainWindow::probePrintLine(core::Box2 box, const QString& profile, const QString& pdf,
                                   bool round)
{
    PrintDialog dialog(*controller_, box, profile, this);
    // AN OUTPUT, because a preview with nowhere to write says nothing: the line
    // is deliberately empty until the plot has a destination, and this probe is
    // reading the line.
    if (auto* file = dialog.findChild<Field*>(QStringLiteral("printPdfPath"))) file->setValue(pdf);
    if (round) {
        // PRESSED, not called: the probe reaches the button the way a hand does,
        // so a button that is there but unreachable fails here.
        if (auto* offer = dialog.findChild<Button*>(QStringLiteral("printRoundScale")))
            offer->click();
    }
    return dialog.commandLine();
}

void MainWindow::openSettingsSection(const QString& title)
{
    openSettings();
    if (settings_ != nullptr) settings_->showSection(title);
}

void MainWindow::openScript()
{
    runScriptFile(QFileDialog::getOpenFileName(this, tr("Betik seç"),
                                               QStringLiteral("tests/journal"),
                                               tr("KentOSCad betiği (*.json);;Tüm dosyalar (*)")));
}

void MainWindow::openScriptPreview()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Önizlenecek betiği seç"), QStringLiteral("tests/journal"),
        tr("KentOSCad betiği (*.json);;Tüm dosyalar (*)"));
    if (path.isEmpty()) return;
    controller_->runLine(QStringLiteral("BETİK \"%1\" onizle=evet").arg(path),
                         command::Origin::Gui);
}

void MainWindow::showAbout()
{
    // THE FACTS A BUG REPORT NEEDS, read from the running program rather than
    // written down: the version it was built as, the Qt under it, the backend
    // the canvas actually got, and how much of the registry is there.
    AboutFacts facts;
    facts.version = QStringLiteral(KENTOS_VERSION);
    facts.qt      = QString::fromLatin1(qVersion());
    facts.backend = canvas_->backendName();
    facts.platform =
        tr("%1 · %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    facts.commands = static_cast<int>(controller_->registry().size());
    facts.tools    = static_cast<int>(processing::processing_tools().size());
    facts.dataRoot = QDir::toNativeSeparators(QString::fromStdString(data_root()));
    AboutDialog about(facts, this);
    about.applyTheme(theme_);
    about.exec();
}

// =============================================================================
// KENTOS_TOOL_PROBE — the tool column, pressed
// =============================================================================

namespace {

/// How long the credential probe below will wait for the key store to answer an
/// entry that is not there. Generous, because it is a bound rather than an
/// expectation: an absent entry answers in microseconds on all three platforms,
/// and anything approaching this number is the failure the probe exists to catch.
constexpr int kKeyProbeWaitMs = 10'000;

/// How long one pass of the wait loop blocks before looking again.
constexpr int kKeyProbeStepMs = 50;

/// The endings one probe request produced, for the credential probe below.
struct EndingSink : ai::StreamSink
{
    /// Nothing is ever sent in this probe, so no chunk can arrive.
    void on_chunk(std::string_view) override {}

    /// Counts as well as keeps: `ai::StreamSink` promises EXACTLY ONE ending per
    /// request, and the window this probe exercises is one in which two could
    /// plausibly be produced — a cancel and then the key store's own answer.
    void on_finished(int status, std::string_view error) override
    {
        ++endings;
        code = status;
        why.assign(error);
    }

    int endings{0};  ///< how many times the turn has ended; exactly 1 is the rule
    int code{0};     ///< the HTTP status, or 0 when there never was one
    std::string why; ///< the Turkish sentence, empty when nothing went wrong
};

/// Builds a profile naming a key-store entry that cannot exist, for the probe.
///
/// `mark` distinguishes one request from the next, so a `key_ref` settled by an
/// earlier step cannot answer a later one out of memory and make the step look
/// like it passed.
ai::ProviderProfile keyedProbeProfile(const char* mark)
{
    ai::ProviderProfile profile;
    profile.name        = std::string("Sınama (anahtarlı ") + mark + ")";
    profile.dialect     = ai::Dialect::OpenAiChat;
    profile.base_url    = "http://127.0.0.1:1";
    profile.path        = "/chat/completions";
    profile.model       = "sinama-1";
    profile.key_ref     = std::string("kentos-sinama-boyle-bir-kayit-yok-") + mark;
    profile.auth_header = "Authorization";
    profile.auth_scheme = "Bearer";
    return profile;
}

/// Proves that fetching a credential does not stop the GUI thread.
///
/// THE REGRESSION. `AiTransport::send` used to call the platform key store
/// inline, which on macOS sits inside `SecItemCopyMatching` for as long as an
/// authorisation prompt goes unanswered — over two minutes, in the headless run
/// that found it — with the window frozen and not a byte sent (ai.md R18, P8).
///
/// WHAT IT ASSERTS IS THE SHAPE, not a stopwatch: `send` returns a handle while
/// the sink is STILL WAITING, because the answer is coming from another thread
/// through the event loop and cannot have arrived yet. A `send` that resolved the
/// key itself could not possibly satisfy that. The second half asserts the other
/// half of R18: that the window between the handle and the request is
/// CANCELLABLE, and that cancelling it produces one ending rather than two.
///
/// IT NAMES ENTRIES THAT CANNOT EXIST. The key store is asked one question —
/// "is there a record under this name?" — whose answer is no on every machine.
/// No key of the user's is read, and no prompt can be raised, because there is
/// nothing there to authorise. Returns the number of checks that failed.
int probeCredentialOffThread(Controller& controller)
{
    int failures     = 0;
    const auto check = [&failures](bool held, const char* what) {
        if (held) return;
        ++failures;
        (void)std::fprintf(stdout, "[sohbet] BASARISIZ — %s\n", what);
        (void)std::fflush(stdout);
    };
    const auto say = [](const char* what, const QString& detail) {
        (void)std::fprintf(stdout, "[sohbet] %-22s %s\n", what, detail.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    SecretResolver& keys = controller.providerService().secrets();
    AiTransport& wire    = controller.aiTransport();

    /// Hands one request to the transport and returns without waiting for it.
    const auto post = [&wire](const ai::ProviderProfile& profile, EndingSink& sink) {
        // Loopback, so the permit is granted whatever the project's sensitivity
        // is (ai.md R14, R30) — this probe is about the credential, not the
        // policy.
        auto permitted = ai::permit_for(profile, /*sensitive=*/false);
        if (!permitted) return std::shared_ptr<ai::Cancellation>();
        ai::HttpRequest request;
        request.url  = permitted.value().url();
        request.body = "{}";
        wire.useProfile(profile);
        return wire.send(permitted.value(), std::move(request), sink);
    };

    // ---- the answer arrives, and it arrives from the event loop ----
    const ai::ProviderProfile waited = keyedProbeProfile("a");
    const QString waitedRef          = QString::fromStdString(waited.key_ref);
    check(!keys.known(waitedRef).settled, "sınama kaydı çözülmüş görünüyor — probe kirli");

    EndingSink first;
    const std::shared_ptr<ai::Cancellation> flight = post(waited, first);
    check(flight != nullptr, "istek tutamağı verilmedi");
    check(first.endings == 0, "ANAHTAR BU İŞ PARÇACIĞINDA OKUNDU — cevap send() dönmeden geldi");

    const qint64 until = QDateTime::currentMSecsSinceEpoch() + kKeyProbeWaitMs;
    while (first.endings == 0 && QDateTime::currentMSecsSinceEpoch() < until)
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kKeyProbeStepMs);

    check(first.endings == 1, "anahtar deposunun cevabı gelmedi");
    check(first.code == 0, "olmayan anahtar bir HTTP durumu üretti — istek gönderilmiş");
    check(first.why.find("Anahtar bulunamad") != std::string::npos,
          "eksik anahtar açıkça söylenmedi");
    check(keys.known(waitedRef).settled, "cevap oturum belleğine yazılmadı");
    say("credential off-thread", QString::fromStdString(first.why));

    // ---- and the wait is cancellable, with exactly one ending ----
    //
    // Cancelling here is deterministic whatever the key store does: nothing has
    // been pumped, so the lookup cannot have answered and the handle is still in
    // the window between `send` and the socket.
    EndingSink second;
    const std::shared_ptr<ai::Cancellation> pending = post(keyedProbeProfile("b"), second);
    check(pending != nullptr, "bekleyen istek tutamağı verilmedi");
    if (pending) pending->cancel();
    check(second.endings == 1, "anahtar beklerken iptal cevapsız kaldı");
    check(second.why.find("iptal") != std::string::npos, "iptal öyle söylenmedi");

    // The lookup is still out; when it lands it must find a cancelled turn and
    // say nothing, or the sink has been told twice that one request ended.
    const qint64 settle = QDateTime::currentMSecsSinceEpoch() + kKeyProbeWaitMs;
    while (keys.known(QString::fromStdString(keyedProbeProfile("b").key_ref)).settled == false &&
           QDateTime::currentMSecsSinceEpoch() < settle)
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kKeyProbeStepMs);
    check(second.endings == 1, "iptal edilen istek İKİ KEZ bitti");
    say("credential cancel", QString::fromStdString(second.why));

    return failures;
}

} // namespace

int MainWindow::probePython()
{
    int failures     = 0;
    const auto check = [&failures](bool held, const char* what) {
        if (held) return;
        ++failures;
        (void)std::fprintf(stdout, "[python] BASARISIZ — %s\n", what);
        (void)std::fflush(stdout);
    };
    const auto say = [](const char* what, const QString& detail) {
        (void)std::fprintf(stdout, "[python] %-26s %s\n", what, detail.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    check(pythonConsole_ != nullptr, "Python konsolu kurulmadı");
    if (pythonConsole_ == nullptr) return 1;
    showPythonConsole();

    // ---- `cad.` offers the whole surface -----------------------------------
    //
    // THE COUNT IS THE POINT. The old completer answered `cad.` with nothing at
    // all — it wanted three characters and a `cad` prefix — and the way that bug
    // stays fixed is a number that cannot be met by a hand-written list.
    {
        const QStringList shown = pythonConsole_->probeOffered(QStringLiteral("cad."));
        say("cad. ->", QString::number(shown.size()) + QStringLiteral(" aday"));
        check(shown.size() > 80, "cad. bütün komutları önermiyor");
        check(shown.contains(QStringLiteral("line")), "cad. -> line yok");
        check(shown.contains(QStringLiteral("doc")), "cad. -> doc yok");
        check(shown.contains(QStringLiteral("viewport")), "cad. -> viewport yok");
        check(shown.contains(QStringLiteral("Point")), "cad. -> Point yok");
        check(shown.contains(QStringLiteral("run")), "cad. -> run yok");
    }

    // ---- an owner narrows it, rather than widening it -----------------------
    {
        const QStringList shown = pythonConsole_->probeOffered(QStringLiteral("cad.doc."));
        say("cad.doc. ->", shown.join(QStringLiteral(" ")));
        check(shown.contains(QStringLiteral("layers")), "cad.doc. -> layers yok");
        check(shown.contains(QStringLiteral("entity_count")), "cad.doc. -> entity_count yok");
        check(!shown.contains(QStringLiteral("line")), "cad.doc. komut öneriyor");
    }
    {
        const QStringList shown = pythonConsole_->probeOffered(QStringLiteral("cad.viewport."));
        check(shown.contains(QStringLiteral("bbox")), "cad.viewport. -> bbox yok");
        check(!shown.contains(QStringLiteral("layers")), "cad.viewport. -> doc çağrısı öneriyor");
    }

    // ---- inside a call, the keywords ----------------------------------------
    {
        const QStringList shown = pythonConsole_->probeOffered(QStringLiteral("cad.line("));
        say("cad.line( ->", shown.join(QStringLiteral(" ")));
        check(shown.contains(QStringLiteral("points=")), "çağrı içinde points= önerilmiyor");
        check(pythonConsole_->promptHintVisible(), "çağrı içinde imza ipucu yok");
    }
    {
        // ALREADY WRITTEN IS NOT OFFERED AGAIN: a second `points=` is a TypeError.
        const QStringList shown =
            pythonConsole_->probeOffered(QStringLiteral("cad.line(points=[], "));
        check(!shown.contains(QStringLiteral("points=")), "yazılmış anahtar tekrar öneriliyor");
    }

    // ---- outside a call, the script's own names and Python's ----------------
    {
        const QStringList shown =
            pythonConsole_->probeOffered(QStringLiteral("kenar_sayisi = 6\nken"));
        say("yerel ad ->", shown.join(QStringLiteral(" ")));
        check(shown.contains(QStringLiteral("kenar_sayisi")), "betiğin kendi adı önerilmiyor");
    }
    {
        const QStringList shown = pythonConsole_->probeOffered(QStringLiteral("pri"));
        check(shown.contains(QStringLiteral("print")), "yerleşik print önerilmiyor");
    }

    // ---- nothing floats over the line being typed, nor over the other --------
    //
    // WHAT THE USER SAW: the list and the hint on top of each other and both on
    // top of the line, at a prompt that sits on the bottom edge of the window.
    // The pictures never showed it, because each floater was photographed alone.
    {
        (void)pythonConsole_->probeOffered(QStringLiteral("cad.line("));
        const ScriptEditor::Floaters f = pythonConsole_->promptFloaters();
        const auto text                = [](const QRect& r) {
            return QStringLiteral("%1,%2 %3×%4")
                .arg(r.x())
                .arg(r.y())
                .arg(r.width())
                .arg(r.height());
        };
        say("satır", text(f.line));
        say("liste", text(f.popup));
        say("ipucu", text(f.hint));
        check(!f.popup.isEmpty(), "çağrı içinde liste açılmadı");
        check(!f.hint.isEmpty(), "çağrı içinde ipucu açılmadı");
        check(!f.popup.intersects(f.line), "liste yazılan satırı örtüyor");
        check(!f.hint.intersects(f.line), "ipucu yazılan satırı örtüyor");
        check(!f.popup.intersects(f.hint), "liste ile ipucu üst üste");
    }

    // ---- the prompt is a prompt ---------------------------------------------
    {
        ScriptEditor* prompt = pythonConsole_->prompt();
        check(prompt->gutterText(0) == QStringLiteral(">>>"), "istem >>> ile başlamıyor");
        check(prompt->gutterText(1) == QStringLiteral("..."), "devam satırı ... değil");
    }

    // ---- the hint follows the cursor out of the call ------------------------
    {
        (void)pythonConsole_->probeOffered(QStringLiteral("cad.line(points=[]) "));
        check(!pythonConsole_->promptHintVisible(), "çağrı bittikten sonra ipucu duruyor");
    }

    (void)std::fprintf(stdout, "[python] %d hata\n", failures);
    (void)std::fflush(stdout);
    return failures;
}

int MainWindow::probeChat()
{
    int failures     = 0;
    const auto check = [&failures](bool held, const char* what) {
        if (held) return;
        ++failures;
        (void)std::fprintf(stdout, "[sohbet] BASARISIZ — %s\n", what);
        (void)std::fflush(stdout);
    };
    const auto say = [](const char* what, const QString& detail) {
        (void)std::fprintf(stdout, "[sohbet] %-22s %s\n", what, detail.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    check(chatPanel_ != nullptr, "sohbet paneli kurulmadı");
    if (chatPanel_ == nullptr) return 1;
    chatDock_->show();

    // A LOCAL PROFILE WITH NO `key_ref`, AND IT IS THE PANEL'S ONLY ONE. Two
    // separate reasons, both of which outlive the defect that made the second
    // one urgent.
    //
    //   1. The bytes below are recorded, so the endpoint is never reached. The
    //      profile exists so the panel can pick a dialect and name a model on the
    //      message (ai.md P10: no test calls a live provider).
    //
    //   2. It is KEYLESS, so nothing here can reach the user's key store, and a
    //      run on somebody's own machine cannot raise a keychain prompt.
    //
    // IT IS INSTALLED OVER THE PANEL'S PROFILE SOURCE, not merely handed to
    // `probeStream`, and that is the part that was missing. Step 2 below feeds a
    // read tool, and a turn whose tools were all reads CONTINUES — `finishTurn`
    // calls `sendRound` again — through `chosen()`, which is the CHOOSER's
    // profile rather than this one. On a developer's machine that is whatever
    // they configured: the probe was reaching a real cloud profile, asking the
    // real key store for its key, and would have opened a real socket if it
    // found one. That is how this test came to sit inside the macOS Security
    // framework for two minutes. The block itself is gone (secret_resolver.hpp);
    // pointing the panel at a keyless loopback endpoint is what keeps the probe
    // off both roads for good.
    //
    // The transport's own credential path is proved separately, in step 6.
    ai::ProviderProfile profile;
    profile.name     = "Sınama";
    profile.dialect  = ai::Dialect::OpenAiChat;
    profile.base_url = "http://127.0.0.1:1";
    profile.path     = "/chat/completions";
    profile.model    = "sinama-1";

    ai::ProviderProfiles only;
    check(only.upsert(profile).ok(), "sınama profili kurulamadı");
    chatPanel_->setProfileSource([only] { return only; });

    // 1. TEXT AND REASONING. Two deltas of prose and one of thinking, framed the
    //    way `chat/completions` frames them.
    const std::string prose =
        "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"Kuzey cepheyi seçtim.\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"Parselin alanı 3482.64 m². \"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"İki eşit parça 1741.32 m² olur.\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":1200,\"completion_tokens\":64}}\n\n"
        "data: [DONE]\n\n";
    const int before = chatPanel_->transcript()->count();
    chatPanel_->probeStream(profile, prose);
    say("text + reasoning", tr("%1 ileti").arg(chatPanel_->transcript()->count()));
    check(chatPanel_->transcript()->count() == before + 1, "yanıt dökümde görünmedi");
    auto* answer = qobject_cast<MessageBubble*>(chatPanel_->transcript()->last());
    check(answer != nullptr, "son ileti bir ileti balonu değil");
    if (answer != nullptr) {
        check(answer->text().contains(QStringLiteral("3482.64")), "yanıt metni eksik");
        check(answer->speaker() == Speaker::Model, "yanıt model iletisi değil");
    }

    // 2. A READ TOOL RUNS AT ONCE and its result comes back as a tool message.
    const std::string read =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c1\","
        "\"function\":{\"name\":\"katmanlari_listele\",\"arguments\":\"\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"function\":{\"arguments\":\"{}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: [DONE]\n\n";
    chatPanel_->probeStream(profile, read);
    QString told;
    for (MessageBubble* bubble : chatPanel_->transcript()->findChildren<MessageBubble*>())
        if (bubble->speaker() == Speaker::ToolResult) told = bubble->text();
    say("read tool", told.isEmpty() ? tr("araç sonucu YOK") : told);
    check(!told.isEmpty(), "okuma aracı sonucunu dökme yazmadı");
    check(told.contains(QStringLiteral("katman")) || told.contains(QStringLiteral("KATMAN")),
          "okuma aracı katmanları bildirmedi");

    // 3. A WRITE TOOL CHANGES NOTHING and leaves a card that does.
    const std::size_t layersBefore = controller_->document().layers().size();
    const std::string write =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c2\","
        "\"function\":{\"name\":\"core_layer\",\"arguments\":\"\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"function\":{\"arguments\":\"{\\\"ad\\\":\\\"SOHBET\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: [DONE]\n\n";
    chatPanel_->probeStream(profile, write);
    check(controller_->document().find_layer("SOHBET") == core::kNoLayer,
          "YAZMA ARACI UYGULANDI — onay beklemesi gerekirdi");
    check(controller_->document().layers().size() == layersBefore, "çizim değişti");

    auto* card = chatPanel_->findChild<SuggestionCard*>();
    say("write tool",
        card != nullptr ? tr("öneri kartı %1").arg(card->planId()) : tr("öneri kartı YOK"));
    check(card != nullptr, "yazma aracı öneri kartı bırakmadı");

    // 4. AND THE CARD IS WHAT APPLIES IT, in one undo entry.
    if (card != nullptr) {
        const std::size_t depth = controller_->undoStack().undo_depth();
        const auto decided      = card->probeApply();
        check(decided.ok(), "onaylanan öneri uygulanamadı");
        check(controller_->document().find_layer("SOHBET") != core::kNoLayer,
              "onaydan sonra katman yok");
        // KATMAN IS DELIBERATELY NOT UNDOABLE — `ensure_layer` is idempotent and
        // creating a layer leaves no undo entry (`layer.cpp`) — so the depth is
        // REPORTED rather than asserted here. That one approved plan of geometry
        // is one undo entry is proved where it can be proved without a window,
        // in `tests/unit/test_ai_tools.cpp`.
        say("apply", tr("katman geldi; geri alma yığını %1 → %2 (KATMAN geri alınmaz)")
                         .arg(depth)
                         .arg(controller_->undoStack().undo_depth()));

        // 4b. AND THE JOB CARRIES ON (TODOS A-04). The loop used to END at the
        // first card: a request needing a layer, then objects, then a sheet,
        // then a PDF stopped after one suggestion, and the user — having just
        // clicked Uygula — watched nothing happen.
        //
        // WHAT IS ASSERTED IS THE HAND-BACK, not a second model turn: the
        // conversation must now carry what actually happened, so the model can
        // VERIFY rather than claim. Nothing is applied by this: the person
        // already decided.
        const std::span<const ai::Message> said_so_far = chatPanel_->probeMessages();
        bool carried                                   = false;
        for (const ai::Message& one : said_so_far)
            if (one.role == ai::Role::User &&
                one.text().find("Öneri " + card->planId().toStdString()) != std::string::npos)
                carried = true;
        check(carried, "ONAYDAN SONRA İŞ DEVAM ETMEDİ — model sonucu hiç öğrenmedi");
    }

    // 5. A COORDINATE LITERAL IS REFUSED where a handle is declared.
    const std::string literal =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c3\","
        "\"function\":{\"name\":\"core_line\",\"arguments\":\"\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":"
        "{\"arguments\":\"{\\\"noktalar\\\":[[0,0],[1000,0]]}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: [DONE]\n\n";
    const int cardsBefore = static_cast<int>(chatPanel_->findChildren<SuggestionCard*>().size());
    chatPanel_->probeStream(profile, literal);
    const int cardsAfter = static_cast<int>(chatPanel_->findChildren<SuggestionCard*>().size());
    say("coordinate literal", tr("kart sayısı %1 → %2").arg(cardsBefore).arg(cardsAfter));
    check(cardsAfter == cardsBefore, "koordinat literali öneri oldu — reddedilmeliydi");

    // 6. AND THE CREDENTIAL IS NEVER FETCHED ON THIS THREAD. The regression this
    //    guards: `AiTransport::send` used to call the platform key store inline,
    //    which on macOS sits inside `SecItemCopyMatching` for as long as an
    //    authorisation prompt goes unanswered — over two minutes, in the headless
    //    run that found it — with the window frozen and not a byte sent
    //    (ai.md R18, P8).
    //
    //    WHAT IS ASSERTED IS THE SHAPE, not a stopwatch: `send` comes back with
    //    a handle and the sink is STILL WAITING, because the answer cannot have
    //    arrived yet — it is coming from another thread through the event loop.
    //    The `key_ref` names an entry that cannot exist, so no key of the user's
    //    is read and no prompt can be raised: there is nothing there to
    //    authorise.
    failures += probeCredentialOffThread(*controller_);

    (void)std::fprintf(stdout, "[sohbet] %s — %d ileti\n", failures == 0 ? "TAMAM" : "BASARISIZ",
                       chatPanel_->transcript()->count());
    (void)std::fflush(stdout);
    return failures == 0 ? 0 : 1;
}

void MainWindow::probeTools()
{
    // A scene with something of every shape the column's tools act on: one face,
    // two lines that cross inside the box below, and one that touches neither.
    const auto scene = [this] {
        runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
        runScriptLine(QStringLiteral("SİL"));
        runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
        runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));
        runScriptLine(QStringLiteral("ÇİZGİ 60,0 60,40"));
        runScriptLine(QStringLiteral("ÇİZGİ 50,20 80,20"));
        runScriptLine(QStringLiteral("ÇİZGİ 100,0 120,10"));
        endCommand(); ///< a run is written when it ends (TODOS C-02)
    };

    // THE TRANSCRIPT, not the `echoed` signal. Several buttons answer through
    // `onEcho` directly — "Önce nesne seçin" is a refusal the shell writes, not
    // one the bus does — and a probe that watched only the bus would call those
    // buttons silent. Everything a user can read ends up here.
    QString prompt;
    bool armed          = false;
    bool asked          = false;
    const auto onPrompt = connect(controller_, &Controller::promptChanged, this,
                                  [&prompt, &armed, &asked](const QString& p) {
                                      // An EMPTY prompt is the command letting go.
                                      // Kept, not ignored, so the feed below stops
                                      // the moment the tool is finished instead of
                                      // sending its next answer to the bus as a
                                      // command of its own.
                                      armed = !p.isEmpty();
                                      if (armed) {
                                          prompt = p;
                                          asked  = true;
                                      }
                                  });

    // EVERY TOOL A HAND CAN PRESS, found rather than listed: a tool added and
    // forgotten here would be a tool nobody presses. The column first — every
    // member of every family, not only the faces, because the faces were all a
    // probe used to press and the members behind them went untested — and then
    // every menu entry that starts a drawing, editing or reading command. The
    // menus are where PAH, YUVARLA, UZAT, KIR and the corner tools lived, and
    // "the toolbox tools are not wired to the commands" is a complaint about all
    // of them, not only the ones in the column.
    //
    // NOT THE FILE, VIEW, LAYER AND SYSTEM COMMANDS: they open windows, and a
    // modal window stops the probe where it stands. They have their own probes.
    struct Pressable
    {
        QAction* action;
        const char* where;
    };

    QVector<Pressable> pressable;
    QSet<const QAction*> seen;
    int unbuilt = 0; ///< a ribbon slot handed an action before it existed
    // THE RIBBON'S TOOLS, in ribbon order — what the tool column was: the draw,
    // edit and query tools and the three that pick and pan. The rest of the
    // ribbon (save, print, the theme, the panels) opens windows and dialogs, and
    // the menu pass below already reaches every command among them.
    for (QAction* a : std::as_const(ribbonTools_)) {
        if (a == nullptr) {
            ++unbuilt;
            continue;
        }
        const bool idle = a == actSelect_ || a == actSelectArea_ || a == actPan_;
        QString line    = a->property(kToolCommand).toString();
        if (line.isEmpty() && a->objectName().startsWith(QStringLiteral("toolAction.")))
            line = a->objectName().section(QLatin1Char('.'), 1);
        const command::CommandSpec* spec =
            line.isEmpty() ? nullptr
                           : controller_->registry().resolve(
                                 line.section(QLatin1Char(' '), 0, 0).toStdString());
        const bool tool = spec != nullptr && (spec->category == command::Category::Draw ||
                                              spec->category == command::Category::Modify ||
                                              spec->category == command::Category::Query);
        if ((!idle && !tool) || seen.contains(a)) continue;
        seen.insert(a);
        pressable.push_back({a, "şerit"});
    }
    for (QAction* a : findChildren<QAction*>()) {
        if (seen.contains(a) || a->isSeparator() || a->menu() != nullptr) continue;
        QString line = a->property(kToolCommand).toString();
        if (line.isEmpty() && a->objectName().startsWith(QStringLiteral("toolAction.")))
            line = a->objectName().section(QLatin1Char('.'), 1);
        if (line.isEmpty()) continue;
        const command::CommandSpec* spec =
            controller_->registry().resolve(line.section(QLatin1Char(' '), 0, 0).toStdString());
        if (spec == nullptr) continue;
        if (spec->category != command::Category::Draw &&
            spec->category != command::Category::Modify &&
            spec->category != command::Category::Query)
            continue;
        seen.insert(a);
        pressable.push_back({a, "menü"});
    }

    int ok_ran   = 0;
    int ok_armed = 0;
    int dead     = unbuilt;
    QStringList broken;
    if (unbuilt > 0)
        broken << QStringLiteral("şeride %1 araç kurulmadan eklenmiş; düğmeleri yok").arg(unbuilt);

    for (int pass = 0; pass < 2; ++pass) {
        const bool withSelection = pass == 0;

        (void)std::fprintf(stdout, "[araç] ==== %s ====\n",
                           withSelection ? "önce seç, sonra bas"
                                         : "önce bas, sonra seç (BOŞ SEÇİM)");
        for (const Pressable& press : pressable) {
            QAction* action = press.action;

            const QString name = action->text().remove(QLatin1Char('&'));
            QString cmd        = action->property(kToolCommand).toString();
            if (cmd.isEmpty()) cmd = action->objectName().section(QLatin1Char('.'), 1);

            // Reset before each, so one tool's leftovers cannot stand in for the
            // next one's answer. `withSelection` is the pass this probe is on: a tool
            // has to work BOTH ways round — select then press, and press then select —
            // and it was the second that was broken.
            controller_->cancelInteractive();

            // A FRESH SCENE BEFORE EVERY TOOL. Twenty-one buttons trimming, merging,
            // offsetting and placing points in the same drawing leaves each one
            // measuring what the last one did rather than what it does itself.
            scene();
            runScriptLine(withSelection ? QStringLiteral("SEÇ mod=KUTU noktalar=45,-5 85,45")
                                        : QStringLiteral("SEÇ mod=TEMİZLE"));
            QCoreApplication::processEvents();

            const int held = static_cast<int>(controller_->bus().selection().size());

            // THE TRANSCRIPT STARTS EMPTY FOR EACH PRESS. It keeps 2000 lines and
            // drops the oldest, so after a hundred tools a character offset taken
            // before a press pointed into text that had since moved up — and the
            // tools that answered late in the run were reported as silent.
            QCoreApplication::processEvents();
            transcript_->clear();
            const int mark = 0;
            prompt.clear();
            armed = false;
            asked = false;

            // A TOOL WITH A WINDOW answers by opening it: the find-and-replace
            // window's fields are its prompts. What was already on screen is
            // noted first, so the window this press opened is the one found
            // afterwards.
            QSet<const QWidget*> shown_before;
            for (const QWidget* w : QApplication::topLevelWidgets())
                if (w->isVisible()) shown_before.insert(w);

            action->trigger();
            QCoreApplication::processEvents();

            QWidget* opened = nullptr;
            for (QWidget* w : QApplication::topLevelWidgets())
                if (w != this && w->isVisible() && !shown_before.contains(w) &&
                    qobject_cast<QDialog*>(w) != nullptr)
                    opened = w;

            // A modal tool is fed three points, so it either finishes or says what it
            // still wants: an armed tool that cannot finish is as dead as one that
            // never started.
            if (asked) {
                // A TEXT PROMPT IS ANSWERED WITH TEXT. Feeding a point into `yazi`
                // is the probe being wrong, not METİN — and the resulting type error
                // would otherwise be reported as a broken tool.
                for (int step = 0; step < 3 && armed; ++step) {
                    static const core::Point2 clicks[3] = {
                        {5'000, 5'000}, {62'000, 20'000}, {75'000, 25'000}};
                    // THROUGH THE ROADS A USER ACTUALLY HAS. A point arrives from the
                    // canvas; a number or a caption is typed into the command line.
                    // Calling `supplyText` directly would prove the session accepts a
                    // value and prove nothing about whether the user can give it one —
                    // which is exactly how OFSET came to have no answerable prompt.
                    if (controller_->promptKind() == command::ParamKind::Selection) {
                        // What the canvas does: pick through `SEÇ`, then Enter.
                        runScriptLine(QStringLiteral("SEÇ mod=KUTU noktalar=45,-5 85,45"));
                        std::vector<std::int64_t> ids;
                        for (core::EntityKey k : controller_->bus().selection().keys())
                            ids.push_back(static_cast<std::int64_t>(core::raw(k)));
                        controller_->supplyObjects(ids);
                    } else if (prompt.contains(QStringLiteral("Yazılacak"))) {
                        runScriptLine(QStringLiteral("deneme"));
                    } else if (controller_->promptKind() == command::ParamKind::Number ||
                               controller_->promptKind() == command::ParamKind::Integer ||
                               prompt.contains(QStringLiteral("mesafe"))) {
                        runScriptLine(QStringLiteral("5"));
                    } else {
                        controller_->supplyPoint(clicks[step]);
                    }
                    QCoreApplication::processEvents();
                }
                controller_->cancelInteractive();
                QCoreApplication::processEvents();
            }

            QString said = transcript_->toPlainText().mid(mark).trimmed();
            said.replace(QLatin1Char('\n'), QLatin1Char(' '));

            QString verdict;
            // ANYWHERE IN WHAT WAS SAID, not at its start: the scene's own last
            // line can land in the transcript after the mark, and a refusal
            // behind "1 çizgi çizildi." was once counted as a tool that ran.
            const bool refused = said.contains(QStringLiteral("Hata:")) ||
                                 said.contains(QStringLiteral("Bilinmeyen komut"));
            // WHAT A REFUSAL AT THE PRESS IS ABOUT. A tool that never asked and
            // said it had nothing to work on — "belirtilmedi", "zorunlu", "seçim
            // boş", "Seçili: 0" — is the dead button this probe exists to find:
            // it could have asked. Any other refusal is about the DRAWING: two
            // crossing lines are not a parcel to merge, a drawing with no blocks
            // has nothing to place, and each says so and what to do.
            const bool had_nothing = said.contains(QStringLiteral("belirtilmedi")) ||
                                     said.contains(QStringLiteral("zorunlu")) ||
                                     said.contains(QStringLiteral("seçim boş")) ||
                                     said.contains(QStringLiteral("Seçili: 0"));
            if (refused && !asked && had_nothing) {
                // REFUSED AT THE PRESS, before asking anything: the dead button.
                // A refusal AFTER the tool asked is the probe's own three answers
                // being wrong for it, which says nothing about the button.
                ++dead;
                verdict = QStringLiteral("KIRIK   ") + said;
                broken << QStringLiteral("%1 (%2, %3)")
                              .arg(name, cmd,
                                   withSelection ? QStringLiteral("seçiliyken")
                                                 : QStringLiteral("seçim yokken"));
            } else if (refused && !asked) {
                ++ok_ran;
                verdict =
                    (withSelection ? QStringLiteral("UYMADI  ") : QStringLiteral("VERİ    ")) +
                    said;
            } else if (asked) {
                ++ok_armed;
                verdict = QStringLiteral("SORDU   \"") + prompt + QLatin1Char('"');
                verdict += said.isEmpty()
                               ? QStringLiteral("  ->  tamamlandı, söyleyecek bir şeyi yok")
                               : QStringLiteral("  ->  ") + said;
            } else if (said.isEmpty() && cmd.isEmpty()) {
                // The idle arrow. It sends no command by design — its job is to
                // disarm whatever is running — so saying nothing is the right answer.
                ++ok_ran;
                verdict = QStringLiteral("BOŞTA   çalışan komutu iptal eder (Esc)");
            } else if (opened != nullptr) {
                ++ok_ran;
                verdict = QStringLiteral("PENCERE ") + opened->windowTitle();
                opened->hide();
            } else if (said.isEmpty()) {
                ++dead;
                verdict = QStringLiteral("SESSİZ  düğme ne sordu ne de bir şey söyledi");
                broken << QStringLiteral("%1 (%2, sessiz)").arg(name, cmd);
            } else {
                ++ok_ran;
                verdict = QStringLiteral("ÇALIŞTI ") + said;
            }

            (void)std::fprintf(stdout, "[araç] %-5s %-26s %-22s seçili=%d  %s\n", press.where,
                               qPrintable(name.left(26)), qPrintable(cmd.left(22)), held,
                               qPrintable(verdict.left(150)));
            (void)std::fflush(stdout);
        }
    }
    disconnect(onPrompt);

    (void)std::fprintf(stdout,
                       "[araç] ---- %d araç x2 geçiş: %d çalıştı, %d girdi sordu, %d kırık\n",
                       static_cast<int>(pressable.size()), ok_ran, ok_armed, dead);
    for (const QString& b : broken)
        (void)std::fprintf(stdout, "[araç] KIRIK: %s\n", qPrintable(b));

    // ---- WHAT A HAND CANNOT REACH AT ALL -----------------------------------
    //
    // The column tests above press what is there. This asks the other question,
    // which is the one the user asked: how much of the program has no button and
    // no menu entry — how many commands exist only for somebody who already
    // knows their name and can type it.
    //
    // CLAUDE.md 5.15 forbids a feature reachable only by mouse. Its mirror is
    // just as bad and is what shipped: a command reachable only by keyboard is a
    // command the mouse user does not have, and Article 1.2 makes the GUI an
    // equal client rather than a poorer one. The registry is walked because it is
    // the one command list (5.10); the actions are found rather than listed,
    // because a hand-kept list of "what has a button" is the second list that
    // rule exists to prevent.
    {
        // Every command any action in this window can start, however it says so:
        // the tool property the column and the draw menus use, the object name
        // the probes reach buttons by, and the processing submenu's own property.
        QSet<QString> reachable;
        for (QAction* a : findChildren<QAction*>()) {
            const QString by_property = a->property(kToolCommand).toString();
            if (!by_property.isEmpty())
                reachable.insert(by_property.section(QLatin1Char(' '), 0, 0));
            const QString by_name = a->objectName();
            if (by_name.startsWith(QStringLiteral("toolAction.")))
                reachable.insert(by_name.section(QLatin1Char('.'), 1));
        }

        // Resolved through the registry, so an alias on a button counts as the
        // command it resolves to and a rename cannot silently orphan a button.
        QSet<QString> ids;
        for (const QString& word : reachable)
            if (const command::CommandSpec* spec =
                    controller_->registry().resolve(word.toStdString());
                spec != nullptr)
                ids.insert(QString::fromStdString(spec->id));

        int total = 0;
        int have  = 0;
        QMap<QString, QStringList> missing;
        for (const command::CommandSpec& spec : controller_->registry().all()) {
            if (spec.names.empty()) continue;
            // A command a hand has no business starting from a button: the ones
            // whose whole job is to carry a typed argument (AYAR, TERCİH, MOD have
            // their own windows) are still counted, because "it has a window" is
            // a reachability answer and this probe only reports.
            ++total;
            if (ids.contains(QString::fromStdString(spec.id))) {
                ++have;
                continue;
            }
            missing[QString::fromUtf8(command::category_name(spec.category))]
                << QString::fromStdString(spec.names.front());
        }

        (void)std::fprintf(stdout,
                           "[kapsam] %d komuttan %d'i bir düğme ya da menüden başlatılıyor\n",
                           total, have);
        for (auto it = missing.constBegin(); it != missing.constEnd(); ++it)
            (void)std::fprintf(stdout, "[kapsam] %-12s %2d eksik: %s\n",
                               it.key().toUtf8().constData(), static_cast<int>(it.value().size()),
                               it.value().join(QStringLiteral(", ")).toUtf8().constData());
    }
}

// =============================================================================
// KENTOS_HAND_PROBE — the six modify tools, driven by a hand
// =============================================================================

void MainWindow::probeLayerPanel()
{
    if (layerPanel_ == nullptr) return;
    layerPanel_->probeByHand();

    const auto say = [](const QString& text) {
        (void)std::fprintf(stdout, "[katman] %s\n", text.toUtf8().constData());
        (void)std::fflush(stdout);
    };

    // THE TWO CONTEXT-MENU ENTRIES, OPENED AS A USER OPENS THEM. The menu is
    // built by the same function the right-click builds it with, and the entry is
    // found by the text on it — so a renamed entry, a menu that stops being built
    // for a layer row, or a signal that goes nowhere all show up here. Calling
    // `selectAllOn` directly would prove none of that, which is the lesson
    // `probeByHand` above was written for.
    const core::Document& doc = controller_->document();

    QStringList busy; // the layers that actually hold something, in order
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        const core::LayerId slot = doc.entities().layer[e];
        if (slot >= doc.layers().size()) continue;
        const QString name = QString::fromStdString(doc.layers()[slot].name);
        if (!busy.contains(name)) busy << name;
    }
    if (busy.isEmpty()) {
        say(QStringLiteral("çizimde nesne yok; menü denenmedi"));
        return;
    }
    const QString on = busy.front();

    // THE WHOLE MENU, in order. One entry firing says nothing about the two that
    // were taken out of it.
    say(QStringLiteral("menü · %1: %2")
            .arg(on, layerPanel_->contextEntries(on).join(QStringLiteral(" | "))));

    // ---- THE GÖRÜNÜM SUBMENU, AND WHAT IT DOES TO SEVERAL ROWS -------------
    //
    // A submenu shows up in the line above as its own title and nothing else, so
    // its shape is printed on its own line. Then it is USED, on two rows at once,
    // because "birden fazla katman seçilebilmeli" is only true if something acts
    // on the set — and because the set goes out as one command per layer inside
    // one batch, the undo after it is the proof that it was one gesture.
    const QString submenu = tr("Görünüm");
    say(QStringLiteral("görünüm · %1: %2")
            .arg(on, layerPanel_->contextEntries(on, submenu).join(QStringLiteral(" | "))));

    const auto hidden = [&doc] {
        int n = 0;
        for (const core::Layer& l : doc.layers())
            if (!l.visible) ++n;
        return n;
    };

    // From a known state, whatever the eye clicks above left behind.
    if (!layerPanel_->triggerContextEntry(on, tr("Tümünü göster"), submenu))
        say(QStringLiteral("'%1' görünüm menüsünde 'Tümünü göster' yok").arg(on));
    else
        say(QStringLiteral("tümünü göster → %1 katman gizli").arg(hidden()));

    if (busy.size() > 1) {
        const QString other = busy.at(1);
        layerPanel_->probeSelect({on, other});
        const QString many = tr("Seçili %1 katmanı gizle").arg(2);
        if (!layerPanel_->triggerContextEntry(on, many, submenu)) {
            say(QStringLiteral("çoklu seçimde '%1' girişi yok").arg(many));
        } else {
            say(QStringLiteral("çoklu gizle · %1 + %2 → %3 katman gizli")
                    .arg(on, other)
                    .arg(hidden()));
            // ONE GESTURE, ONE STEP BACK (CLAUDE.md 1.5, Bus::begin_batch).
            controller_->runLine(QStringLiteral("GERİAL"), command::Origin::Gui);
            say(QStringLiteral("çoklu gizle geri alındı → %1 katman gizli").arg(hidden()));
        }
        layerPanel_->probeSelect({on});
    }

    if (!layerPanel_->triggerContextEntry(on, tr("Tümünü seç")))
        say(QStringLiteral("'%1' satırında 'Tümünü seç' yok").arg(on));
    else
        say(QStringLiteral("Tümünü seç · %1 → %2 nesne seçili")
                .arg(on)
                .arg(controller_->bus().selection().size()));

    if (!layerPanel_->triggerContextEntry(on, tr("Öznitelik tablosu"))) {
        say(QStringLiteral("'%1' satırında 'Öznitelik tablosu' yok").arg(on));
        return;
    }

    // The window the signal opened, and how many rows it decided to show. The
    // table is scoped to the layer the menu was opened on, so this number is the
    // whole point of the entry: the drawing's total would mean the scope was lost.
    auto* table = findChild<AttributeTable*>();
    if (table == nullptr) {
        say(QStringLiteral("Öznitelik tablosu · %1 → pencere açılmadı").arg(on));
        return;
    }

    auto* grid = table->findChild<QTableView*>();
    say(QStringLiteral("Öznitelik tablosu · %1 → %2 satır")
            .arg(on)
            .arg(grid != nullptr && grid->model() != nullptr ? grid->model()->rowCount() : -1));
    table->close();
}

void MainWindow::probeToolsByHand()
{
    // WHERE THE FRAMES GO. A probe that reads the transcript proves a command
    // ran; it proves nothing about what the user is looking at while it runs, and
    // "ekrana bakarsan görürsün" is exactly the gap between those two.
    const QString into  = QString::fromLocal8Bit(qgetenv("KENTOS_HAND_PROBE"));
    const bool shooting = into.size() > 1;
    if (shooting) QDir().mkpath(into);

    int frame       = 0;
    const auto shot = [&](const QString& what) {
        if (!shooting) return;
        QImage picture = grab().toImage();
        if (!picture.isNull() && canvas_ != nullptr && canvas_->isVisible()) {
            const QImage live = canvas_->grabCanvas();
            if (!live.isNull()) {
                QPainter painter(&picture);
                painter.drawImage(QRect(canvas_->mapTo(this, QPoint(0, 0)), canvas_->size()), live);
            }
        }
        (void)picture.save(QStringLiteral("%1/%2-%3.png")
                               .arg(into)
                               .arg(frame++, 2, 10, QLatin1Char('0'))
                               .arg(what));
    };

    const auto send = [this](QEvent::Type t, const QPointF& at, Qt::MouseButton b,
                             Qt::MouseButtons held) {
        QMouseEvent ev(t, at, canvas_->mapToGlobal(at), b, held, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &ev);
        QCoreApplication::processEvents();
    };

    /// One click on the canvas. The MOVE first, because that is what a hand does
    /// and what the rubber band follows.
    const auto click = [&](const QPointF& p) {
        send(QEvent::MouseMove, p, Qt::NoButton, Qt::NoButton);
        send(QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton);
    };

    /// Enter, delivered where Qt would deliver it: to whatever holds focus.
    const auto enter = [] {
        QWidget* focused = QApplication::focusWidget();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(focused != nullptr ? focused : QApplication::activeWindow(),
                                    &down);
        QCoreApplication::processEvents();
    };

    const auto at = [this](core::Point2 world) {
        const auto p = canvas_->view().to_screen(world);
        return QPointF(p.x, p.y);
    };

    /// WHICH BUTTON IS LIT. The reported defect is that pressing one tool lights
    /// another, so the answer has to be read off the widgets rather than assumed.
    const auto lit = [this] {
        QStringList on;
        for (QAction* a : drawingTools_->actions())
            if (a->isChecked()) on << a->text();
        return on.isEmpty() ? QStringLiteral("(hiçbiri)") : on.join(QStringLiteral("+"));
    };

    const auto scene = [this] {
        runScriptLine(QStringLiteral("SEÇ mod=TÜMÜ"));
        runScriptLine(QStringLiteral("SİL"));
        runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
        runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));
        runScriptLine(QStringLiteral("ÇİZGİ 60,0 60,40"));
        runScriptLine(QStringLiteral("ÇİZGİ 50,20 80,20"));
        endCommand(); ///< a run is written when it ends (TODOS C-02)
        canvas_->zoomToExtents();
        QCoreApplication::processEvents();
    };

    struct Step
    {
        const char* tool;    ///< the button pressed
        core::Point2 pick_a; ///< first object to point at
        core::Point2 pick_b; ///< a second, when the tool needs two (0,0 = none)
        core::Point2 p1;     ///< the points it asks for afterwards
        core::Point2 p2;
        const char* typed; ///< a number it asks for, or nullptr
    };

    const Step steps[] = {
        // Inside the face; then two points to move it by.
        {"TAŞI", {20'000, 15'000}, {}, {5'000, 5'000}, {25'000, 20'000}, nullptr},
        // Inside the face; then a cut line straight through it.
        {"BÖL", {20'000, 15'000}, {}, {20'000, -5'000}, {20'000, 35'000}, nullptr},
        // Nothing chosen, so every line near the one clicked cuts it (the quick
        // trim): the click is the piece to discard, and Enter ends the run.
        {"BUDA", {60'000, 35'000}, {}, {}, {}, nullptr},
        // Inside the face; then a distance typed at the command line.
        {"OFSET", {20'000, 15'000}, {}, {}, {}, "5"},
    };

    for (const Step& step : steps) {
        scene();
        QAction* action =
            findChild<QAction*>(QStringLiteral("toolAction.") + QString::fromUtf8(step.tool));
        if (action == nullptr) {
            (void)std::fprintf(stdout, "[el] %-7s DÜĞME YOK\n", step.tool);
            continue;
        }

        const auto mark = static_cast<int>(transcript_->toPlainText().size());
        const auto say  = [&](const char* when) {
            const command::Session* live = controller_->session();
            (void)std::fprintf(stdout, "[el] %-7s %-12s yanan=%-22s sorulan=\"%s\"\n", step.tool,
                                when, qPrintable(lit()),
                               live != nullptr ? live->prompt().message.c_str() : "(yok)");
        };

        action->trigger();
        QCoreApplication::processEvents();
        shot(QString::fromUtf8(step.tool) + QStringLiteral("-1-basildi"));
        say("bastıktan");

        click(at(step.pick_a));
        if (step.pick_b.x != 0 || step.pick_b.y != 0) {
            // Shift adds, which is how a second object joins a selection.
            QMouseEvent add(QEvent::MouseButtonPress, at(step.pick_b),
                            canvas_->mapToGlobal(at(step.pick_b)), Qt::LeftButton, Qt::LeftButton,
                            Qt::ShiftModifier);
            QCoreApplication::sendEvent(canvas_, &add);
            QMouseEvent up(QEvent::MouseButtonRelease, at(step.pick_b),
                           canvas_->mapToGlobal(at(step.pick_b)), Qt::LeftButton, Qt::NoButton,
                           Qt::ShiftModifier);
            QCoreApplication::sendEvent(canvas_, &up);
            QCoreApplication::processEvents();
        }
        shot(QString::fromUtf8(step.tool) + QStringLiteral("-2-secildi"));
        say("seçtikten");

        enter();
        shot(QString::fromUtf8(step.tool) + QStringLiteral("-3-enter"));
        say("enter'dan");

        if (step.typed != nullptr) {
            runScriptLine(QString::fromUtf8(step.typed));
        } else {
            if (step.p1.x != 0 || step.p1.y != 0) click(at(step.p1));
            if (step.p2.x != 0 || step.p2.y != 0) {
                // The hand hovers before it clicks: the frame here is the GHOST
                // — the objects carried under the cursor — that TAŞI shows.
                send(QEvent::MouseMove, at(step.p2), Qt::NoButton, Qt::NoButton);
                shot(QString::fromUtf8(step.tool) + QStringLiteral("-4-hayalet"));
                click(at(step.p2));
            }
        }
        QCoreApplication::processEvents();
        shot(QString::fromUtf8(step.tool) + QStringLiteral("-5-bitti"));

        QString said = transcript_->toPlainText().mid(mark).trimmed();
        said.replace(QLatin1Char('\n'), QLatin1Char(' '));
        (void)std::fprintf(stdout, "[el] %-7s SONUÇ  yanan=%-22s :: %s\n", step.tool,
                           qPrintable(lit()), qPrintable(said.right(110)));

        controller_->cancelInteractive();
        QCoreApplication::processEvents();
    }

    // ---- THE FENCE, from the family's own entry ----------------------------
    //
    // "Buda — çitle" pressed, two corners clicked across the east piece of the
    // horizontal line, the whole edit photographed BEFORE Enter, then Enter.
    // And "Buda — tıklanan kalsın": one click on the piece that stays.
    {
        struct Mode
        {
            const char* tool;                 ///< the member pressed
            std::vector<core::Point2> clicks; ///< its clicks, the last one hovered first
            const char* frame;                ///< the photograph's name
        };

        const std::array<Mode, 3> modes{{
            {"BUDA yontem=çit", {{62'000, 8'000}, {74'000, 32'000}}, "BUDA-cit"},
            {"BUDA tut=evet", {{70'000, 20'000}}, "BUDA-tut"},
            {"UZAT uzanti=evet", {{52'000, 20'000}}, "UZAT-uzanti"},
        }};
        for (const Mode& mode : modes) {
            scene();
            auto* action =
                findChild<QAction*>(QStringLiteral("toolAction.") + QString::fromUtf8(mode.tool));
            if (action == nullptr) {
                (void)std::fprintf(stdout, "[el] %-16s DÜĞME YOK\n", mode.tool);
                continue;
            }
            const auto mark = static_cast<int>(transcript_->toPlainText().size());
            action->trigger();
            QCoreApplication::processEvents();
            for (std::size_t i = 0; i < mode.clicks.size(); ++i) {
                if (i + 1 == mode.clicks.size()) {
                    send(QEvent::MouseMove, at(mode.clicks[i]), Qt::NoButton, Qt::NoButton);
                    shot(QString::fromUtf8(mode.frame) + QStringLiteral("-onizleme"));
                }
                click(at(mode.clicks[i]));
            }
            enter();
            shot(QString::fromUtf8(mode.frame) + QStringLiteral("-bitti"));
            QString said = transcript_->toPlainText().mid(mark).trimmed();
            said.replace(QLatin1Char('\n'), QLatin1Char(' '));
            (void)std::fprintf(stdout, "[el] %-16s SONUÇ :: %s\n", mode.tool,
                               qPrintable(said.right(110)));
            controller_->cancelInteractive();
            QCoreApplication::processEvents();
        }
    }

    // ---- THE GHOSTS ----------------------------------------------------------
    //
    // Every preview the canvas draws under the cursor, photographed mid-gesture:
    // the frame is what the user sees BEFORE the click that commits. A preview
    // that draws nothing here is a preview that draws nothing for them.
    struct Ghost
    {
        const char* name;                 ///< the frame's name
        const char* line;                 ///< what starts the tool (a command line)
        std::vector<core::Point2> clicks; ///< points given before the hover
        core::Point2 hover;               ///< where the pointer rests for the frame
        bool preselect;                   ///< the face is selected first (TAŞI, KOPYALA)
    };

    const Ghost ghosts[] = {
        {"elips", "ELİPS", {{10'000, 40'000}, {30'000, 40'000}}, {20'000, 47'000}, false},
        {"spline",
         "SPLINE",
         {{0, 35'000}, {10'000, 48'000}, {20'000, 35'000}},
         {30'000, 48'000},
         false},
        {"olcu", "ÖLÇÜ", {{0, 33'000}, {25'000, 33'000}}, {12'000, 42'000}, false},
        {"tarama", "TARAMA noktalar=50,35", {{70'000, 35'000}}, {60'000, 48'000}, false},
        {"kopyala", "KOPYALA", {{20'000, 15'000}}, {60'000, 20'000}, true},
        {"blok", "BLOKEKLE ad=OK", {}, {30'000, 45'000}, false},
    };
    scene();
    // Keys run on across scenes, so the member is found by WHERE it is.
    runScriptLine(QStringLiteral("ÇİZGİ 100,100 110,105"));
    endCommand(); ///< a run is written when it ends (TODOS C-02)
    runScriptLine(QStringLiteral("SEÇ mod=KUTU noktalar=99,99 111,106"));
    runScriptLine(QStringLiteral("BLOK ad=OK taban=100,100"));
    for (const Ghost& g : ghosts) {
        runScriptLine(QStringLiteral("SEÇ TEMİZLE"));
        if (g.preselect) runScriptLine(QStringLiteral("SEÇ mod=KUTU noktalar=-1,-1 41,31"));
        controller_->runCommand(QString::fromUtf8(g.line));
        QCoreApplication::processEvents();
        for (const core::Point2& p : g.clicks)
            click(at(p));
        send(QEvent::MouseMove, at(g.hover), Qt::NoButton, Qt::NoButton);
        shot(QStringLiteral("hayalet-") + QString::fromUtf8(g.name));
        const command::Session* live = controller_->session();
        (void)std::fprintf(stdout, "[el] hayalet %-8s sorulan=\"%s\" kılavuz=%zu köşe\n", g.name,
                           live != nullptr ? live->prompt().message.c_str() : "(yok)",
                           canvas_->guideVertexCountForProbe());
        controller_->cancelInteractive();
        QCoreApplication::processEvents();
    }

    // ---- THE ARAÇLAR PANEL --------------------------------------------------
    //
    // The panel composes a command line from its fields and sends it: the line
    // is printed so a reader can type it, and the frame shows the card.
    scene();
    if (toolsPanel_ != nullptr && propertyHeader_ != nullptr) {
        propertyHeader_->setCurrent(2);
        propertyStack_->setCurrentIndex(2);
        QCoreApplication::processEvents();
        // EVERY tool the registry lists, so a tool added tomorrow is probed today
        // (processing.md, "Adding a tool", step 4).
        for (const processing::ProcessingTool* tool : processing::processing_tools()) {
            const std::string id = tool->spec().id;
            if (!toolsPanel_->selectTool(QString::fromStdString(id))) continue;
            QCoreApplication::processEvents();
            // With `TERCİH araç_penceresi evet` the card opened in its own window,
            // which the main window's frame cannot show: it is photographed itself.
            if (auto* dialog = findChild<ToolDialog*>();
                dialog != nullptr && dialog->isVisible() && shooting) {
                (void)dialog->grab().save(
                    QStringLiteral("%1/%2-pencere-%3.png")
                        .arg(into)
                        .arg(frame++, 2, 10, QLatin1Char('0'))
                        .arg(QString::fromStdString(id).section(QLatin1Char('.'), 1)));
            }
            const auto before = static_cast<int>(transcript_->toPlainText().size());
            runScriptLine(QStringLiteral("SEÇ mod=KUTU noktalar=-1,-1 41,31"));
            QString line = toolsPanel_->commandLine();
            if (id == "islem.alan_duzenle") line += QStringLiteral(" alan=1500 mod=kenar");
            // BAĞLA wants the object its captions are to follow: the box the
            // probe drew is object 1.
            if (id == "islem.bagla") line += QStringLiteral(" kaynak=1");
            controller_->runLine(line, command::Origin::Gui);
            if (id == "islem.alan_duzenle") {
                // The tool asks which edge, then where: the top edge, pulled up,
                // photographed with the ghost following the hand, then Enter.
                click(at(core::Point2{20'000, 30'000}));
                send(QEvent::MouseMove, at(core::Point2{20'000, 36'000}), Qt::NoButton,
                     Qt::NoButton);
                shot(QStringLiteral("alan-hayalet"));
                (void)std::fprintf(stdout, "[el] alan hayalet kılavuz=%zu köşe etiket=\"%s\"\n",
                                   canvas_->guideVertexCountForProbe(),
                                   canvas_->guideLabelForProbe().c_str());
                // Enter ON THE CANVAS, where the hand is: focus is in the tree
                // here, and Enter there would run the tool afresh.
                QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas_, &accept);
                QCoreApplication::processEvents();
            }
            // The work is on a worker thread; the result lands when it is done.
            QElapsedTimer waited;
            waited.start();
            while (controller_->session() != nullptr && controller_->session()->working() &&
                   waited.elapsed() < 5000)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QCoreApplication::processEvents();
            QString said = transcript_->toPlainText().mid(before).trimmed();
            said.replace(QLatin1Char('\n'), QLatin1Char(' '));
            (void)std::fprintf(stdout, "[el] araç %-20s satır=\"%s\" :: %s\n", id.c_str(),
                               qPrintable(toolsPanel_->commandLine()), qPrintable(said.right(120)));
            shot(QStringLiteral("araclar-") +
                 QString::fromStdString(id).section(QLatin1Char('.'), 1));
        }
        propertyHeader_->setCurrent(0);
        propertyStack_->setCurrentIndex(0);
    }

    // ---- AND A TOOL STAYS IN THE HAND ---------------------------------------
    //
    // The right button FINISHES the open-ended line and the tool comes back
    // armed; Esc puts it away. Both halves are printed, because the first
    // without the second is a tool that cannot be left.
    scene();
    if (QAction* line = findChild<QAction*>(QStringLiteral("toolAction.ÇİZGİ"))) {
        line->trigger();
        QCoreApplication::processEvents();
        click(at(core::Point2{5'000, 45'000}));
        click(at(core::Point2{25'000, 45'000}));

        const QPointF here = at(core::Point2{25'000, 45'000});
        send(QEvent::MouseButtonPress, here, Qt::RightButton, Qt::RightButton);
        send(QEvent::MouseButtonRelease, here, Qt::RightButton, Qt::NoButton);
        QCoreApplication::processEvents();
        (void)std::fprintf(stdout, "[el] ÇİZGİ  SAĞ TIK yanan=%s\n", qPrintable(lit()));

        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &esc);
        QCoreApplication::processEvents();
        (void)std::fprintf(stdout, "[el] ÇİZGİ  ESC    yanan=%s\n", qPrintable(lit()));
    }
}

} // namespace kentos::app
