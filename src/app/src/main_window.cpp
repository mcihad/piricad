// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/main_window.hpp"

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
#include "kentos_cad/app/schema_page.hpp"
#include "kentos_cad/app/settings_dialog.hpp"
#include "kentos_cad/app/shell_chrome.hpp"
#include "kentos_cad/app/style_designer.hpp"
#include "kentos_cad/app/suggestion_card.hpp"
#include "kentos_cad/app/title_bar.hpp"
#include "kentos_cad/app/toolbox.hpp"
#include "kentos_cad/app/tools_panel.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/processing/registry.hpp"

#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/render/backend.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/selection.hpp"
#include "kentos_cad/core/settings.hpp"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QToolButton>

#include <cmath>
#include <limits>
#include <span>

#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <cstdio>

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
constexpr int kLayoutVersion = 5;

/// Whether this process is a probe driving the real shell.
///
/// Named here because the shell is what must not write. See the save path.
bool probe_run()
{
    for (const char* probe :
         {"KENTOS_PRINT_PROBE",    "KENTOS_LAYOUT_PROBE",  "KENTOS_SHOT_DIR",
          "KENTOS_DESIGNER_PROBE", "KENTOS_WIDGETS_PROBE", "KENTOS_DIALOG_PROBE",
          "KENTOS_HAND_PROBE",     "KENTOS_LAYER_PROBE",   "KENTOS_PICK_PROBE",
          "KENTOS_TABLE_PROBE",    "KENTOS_SCHEMA_PROBE",  "KENTOS_CHAT_PROBE",
          "KENTOS_TOOL_PROBE",     "KENTOS_NORMAL_PROBE",  "KENTOS_FAMILY_PROBE",
          "KENTOS_BUDGET_PROBE",   "KENTOS_PROBE_LINE",    "KENTOS_FRAME_DUMP",
          "KENTOS_MCP_PROBE",      "KENTOS_EDIT_PROBE"})
        if (qEnvironmentVariableIsSet(probe)) return true;
    return false;
}

QString format_metres(core::Mm v)
{
    return QString::number(core::mm_to_metres(v), 'f', 3);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    controller_ = new Controller(this);

    setWindowTitle(tr("KentOSCad — Türkiye Odaklı CBS + CAD"));
    resize(1560, 1000);

    // design.md 7: the frame belongs to the window manager. The shell used to be
    // frameless and drew its own buttons; that cost the resize edges, snapping
    // and the window menu the platform already gives away for free.
    titleBar_ = new TitleBar(this);
    setMenuWidget(titleBar_);
    connect(titleBar_, &TitleBar::searchRequested, this, [this] { openCommandSearch(); });

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

    // design.md 7: the tool box, the canvas column and the right dock sit side by
    // side inside the body, and the status bar runs under all three. The tool box
    // is part of the body rather than a dock, so the command line starts at its
    // right edge exactly as the reference draws it.
    docTabs_ = new DocumentTabs(this);
    connect(docTabs_, &DocumentTabs::activated, this, [this](int) { refreshWindowTitle(); });
    connect(docTabs_, &DocumentTabs::newRequested, this, &MainWindow::newProject);
    connect(docTabs_, &DocumentTabs::closeRequested, this, [this](int) {
        onEcho(tr("Birden çok çizim Faz 2'de gelecek; şimdilik tek belge açıktır."));
    });
    connect(docTabs_, &DocumentTabs::splitRequested, this,
            [this] { onEcho(tr("Bölünmüş görünüm Faz 2'de gelecek.")); });
    connect(docTabs_, &DocumentTabs::expandRequested, this,
            [this] { isMaximized() ? showNormal() : showMaximized(); });

    commandLineRule_ = new QFrame(this);
    commandLineRule_->setFrameShape(QFrame::HLine);
    commandLineRule_->setFrameShadow(QFrame::Plain);

    buildActions();
    buildToolBars();
    buildToolBox();

    auto* column = new QWidget(this);
    auto* stack  = new QVBoxLayout(column);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    stack->addWidget(docTabs_);
    stack->addWidget(canvas_, 1);
    stack->addWidget(commandLineRule_);
    stack->addWidget(commandLine_);

    auto* central = new QWidget(this);
    auto* body    = new QHBoxLayout(central);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(toolBox_);
    body->addWidget(column, 1);
    setCentralWidget(central);

    buildPanels();
    buildStatusBar();

    // The strip runs under EVERYTHING — tool box, canvas and right dock alike —
    // and QMainWindow's status bar is the one slot that already does. A bottom
    // DOCK is a pixel shorter than its widget, because the dock area keeps a
    // separator between itself and the body; the status bar has no such gap.
    setStatusBar(statusStrip_);
    buildMenus();

    // The command line is the shell's conversation and design.md 7 draws it as a
    // permanent 28 px strip above the status bar. It was hidden while the tool
    // bars carried the work; the reference puts it back where every CAD user's
    // hand already reaches for it.
    commandLine_->setVisible(true);
    commandLineRule_->setVisible(false);

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

    connect(controller_, &Controller::undoStateChanged, this, &MainWindow::onUndoStateChanged);
    connect(controller_, &Controller::viewRequested, this, &MainWindow::onViewRequested);
    connect(controller_, &Controller::panRequested, this, &MainWindow::onPanRequested);
    connect(controller_, &Controller::settingChanged, this, &MainWindow::onSettingChanged);
    connect(controller_, &Controller::selectionChanged, this, [this] {
        attributePanel_->refresh();
        if (toolsPanel_ != nullptr) toolsPanel_->refresh();

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
    actImport_->setData(static_cast<int>(Glyph::Open));
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

    actDatabase_ = new QAction(tr("Veritabanı…"), this);
    actDatabase_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    actDatabase_->setToolTip(tr("VERİTABANI — PostGIS sunucusuna bağlanır, katmanları tablo, "
                                "projeleri kayıt olarak yazar"));
    actDatabase_->setData(static_cast<int>(Glyph::Open));
    actDatabase_->setProperty(kToolCommand, QStringLiteral("VERİTABANI"));
    connect(actDatabase_, &QAction::triggered, this, &MainWindow::openDatabase);

    // THE PROJECT'S OWN WINDOW, in the Dosya menu because the project IS the
    // file: what it holds travels with the file and nothing on that window is
    // about this computer.
    actProjectSettings_ = new QAction(tr("Proje Ayarları…"), this);
    actProjectSettings_->setData(static_cast<int>(Glyph::Document));
    actProjectSettings_->setToolTip(
        tr("Çizimle birlikte giden ayarlar ve projenin öznitelik sütunları"));
    actProjectSettings_->setProperty(kToolCommand, QStringLiteral("AYAR"));
    connect(actProjectSettings_, &QAction::triggered, this, &MainWindow::openProjectSettings);

    actSettings_ = new QAction(tr("Ayarlar…"), this);
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
    actSpline_ = drawTool(Glyph::Function, tr("Spline"), QStringLiteral("SPLINE"),
                          tr("SPLINE — kontrol noktalarından pürüzsüz eğri  ·  kısaltma: SPL"));
    drawingTools_->addAction(actSpline_);
    // TARAMA and BLOK work on objects, so they go the way BUDA does: the command
    // asks for its objects when nothing is selected (`want_objects`).
    actHatch_  = modifyTool(Glyph::Grid, tr("Tarama"), QStringLiteral("TARAMA"),
                            tr("TARAMA — kapalı nesnelerin içini katalogdaki bir desenle tarar  ·  "
                                "kısaltma: TRM"));
    actBlock_  = modifyTool(Glyph::Duplicate, tr("Blok"), QStringLiteral("BLOK"),
                            tr("BLOK — seçilen nesnelerden adlı blok tanımlar ve yerine bir "
                                "referans koyar  ·  kısaltma: BLK"));
    actInsert_ = drawTool(Glyph::Copy, tr("Blok Ekle"), QStringLiteral("BLOKEKLE"),
                          tr("BLOKEKLE — tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle "
                             "yerleştirir  ·  kısaltma: BE"));
    drawingTools_->addAction(actInsert_);
    actDimension_ = drawTool(Glyph::Ruler, tr("Ölçü"), QStringLiteral("ÖLÇÜ"),
                             tr("ÖLÇÜ — iki nokta arasını, yarıçapı, çapı ya da açıyı ölçüp yazısı "
                                "ve oklarıyla çizer  ·  kısaltma: ÖÇ"));
    drawingTools_->addAction(actDimension_);
    actLeader_ = drawTool(Glyph::Locate, tr("Lider"), QStringLiteral("LİDER"),
                          tr("LİDER — bir noktayı gösteren oklu çizgi, istenirse yanına yazı  ·  "
                             "kısaltma: LD"));
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
    actErase_->setShortcuts({QKeySequence(QKeySequence::Delete), QKeySequence(Qt::Key_Backspace)});
    actErase_->setShortcutContext(Qt::WindowShortcut);
    addAction(actErase_);

    actErase_->setProperty(kToolCommand, QStringLiteral("SİL"));
    connect(actErase_, &QAction::triggered, this, [this] {
        // With a selection the button IS the command, exactly as typing `SİL`
        // would be. With nothing selected there is nothing to name, so the button
        // opens the command line rather than doing something silently.
        if (!controller_->bus().selection().empty()) {
            if (!confirmErase()) return;
            controller_->runCommand(QStringLiteral("SİL"));
            return;
        }
        onEcho(tr("Silinecek nesne seçili değil. Nesneleri seçin ya da "
                  "SİL nesneler=1 yazın."));
        showCommandLine(true);
        commandLine_->setText(QStringLiteral("SİL nesneler="));
        commandLine_->setFocus();
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
                          tr("BUDA — çizgiyi kestiği sınıra kadar kısaltır  ·  kısaltma: BD"));
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
        modifyTool(Glyph::ParcelSplit, tr("Alana Göre İfraz"), QStringLiteral("ALANİFRAZ"),
                   tr("ALANİFRAZ — parselden istenen yüzölçümünde parça ayırır (kadastro)"));
    actMeasureArea_ = modifyTool(Glyph::MeasureArea, tr("Alan Ölç"), QStringLiteral("ALANÖLÇ"),
                                 tr("ALANÖLÇ — seçili nesnelerin alanını ve çevresini yazar"));

    actStyleCopy_ = modifyTool(Glyph::StyleCopy, tr("Stil Kopyala"), QStringLiteral("STİLKOPYALA"),
                               tr("STİLKOPYALA — bir nesnenin stilini seçili nesnelere uygular"));
    // NOT a `modifyTool`: the check runs on the whole drawing when nothing is
    // selected, and refusing an empty selection would refuse its most useful form.
    actTopology_ = new QAction(tr("Topoloji Denetimi"), this);
    actTopology_->setToolTip(tr("TOPOLOJİ — kendini kesen sınır, sıfır alan ve örtüşen "
                                "parselleri raporlar; hiçbir şeyi düzeltmez"));
    actTopology_->setData(static_cast<int>(Glyph::Topology));
    actTopology_->setObjectName(QStringLiteral("toolAction.TOPOLOJİ"));
    connect(actTopology_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("TOPOLOJİ")); });

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
    actScale_  = modifyTool(Glyph::Rotate, tr("Ölçekle"), QStringLiteral("ÖLÇEKLE"),
                            tr("ÖLÇEKLE — seçili nesneleri bir merkeze göre büyütür/küçültür"));
    actMirror_ = modifyTool(Glyph::Rotate, tr("Aynala"), QStringLiteral("AYNALA"),
                            tr("AYNALA — seçili nesneleri bir eksende yansıtır"));
    actArray_ =
        modifyTool(Glyph::Copy, tr("Dizi"), QStringLiteral("DİZİ"),
                   tr("DİZİ — seçili nesneleri satır/sütun ya da merkez etrafında çoğaltır"));
    actExtend_   = modifyTool(Glyph::Trim, tr("Uzat"), QStringLiteral("UZAT"),
                              tr("UZAT — çizgiyi sınır çizgisine kadar uzatır"));
    actSplit_    = modifyTool(Glyph::Split, tr("Böl"), QStringLiteral("BÖL"),
                              tr("BÖL — çizgiyi verilen noktadan ikiye böler  ·  kısaltma: BL"));
    actChamfer_  = modifyTool(Glyph::Trim, tr("Pah"), QStringLiteral("PAH"),
                              tr("PAH — köşeyi düz bir kenarla keser"));
    actFillet_   = modifyTool(Glyph::Trim, tr("Yuvarla"), QStringLiteral("YUVARLA"),
                              tr("YUVARLA — köşeyi verilen yarıçapta yayla yuvarlatır"));
    actSetLayer_ = modifyTool(Glyph::LayerManager, tr("Katmana Taşı"), QStringLiteral("KATMANAT"),
                              tr("KATMANAT — seçili nesneleri başka bir katmana taşır"));
    actOffset_   = modifyTool(Glyph::Offset, tr("Ofset"), QStringLiteral("OFSET"),
                              tr("OFSET — seçili nesnelerin paralelini çizer; eksi mesafe içeri"));

    actUndo_ = new QAction(tr("Geri Al"), this);
    actUndo_->setShortcut(QKeySequence::Undo);
    actUndo_->setToolTip(tr("GERİAL — son işlemi geri alır"));
    actUndo_->setData(static_cast<int>(Glyph::Undo));
    actUndo_->setProperty(kToolCommand, QStringLiteral("GERİAL"));
    connect(actUndo_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("GERİAL")); });

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
    actMeasure_->setToolTip(tr("ÖLÇ — iki nokta arası mesafe, koordinat farkı ve açı"));
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
    actMeasureAngle_ = modifyTool(Glyph::Measure, tr("Açı Ölç"), QStringLiteral("AÇIÖLÇ"),
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
    actAi_->setToolTip(tr("Yapay zeka sohbeti — model komut önerir, uygulayan sizsiniz"));
    actAi_->setStatusTip(actAi_->toolTip());
    actAi_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    actAi_->setProperty(kToolCommand, QStringLiteral("ÖNERİ"));
    connect(actAi_, &QAction::triggered, this, [this] {
        if (chatDock_ == nullptr) return;
        chatDock_->show();
        chatDock_->raise();
        if (chatPanel_ != nullptr) chatPanel_->refreshProfiles();
    });

    // ---- arayüz ----
    actTheme_ = new QAction(tr("Koyu Tema"), this);
    actTheme_->setCheckable(true);
    connect(actTheme_, &QAction::toggled, this, &MainWindow::toggleTheme);

    // Render statistics are a developer overlay, never a user-facing feature
    // (kentoscad.md §6.3, .claude/render.md). Off by default.
    actHud_ = new QAction(tr("Geliştirici Bilgisi"), this);
    actHud_->setCheckable(true);
    actHud_->setShortcut(QKeySequence(Qt::Key_F12));
    connect(actHud_, &QAction::toggled, this, [this](bool on) { canvas_->setDebugHud(on); });

    // The command line is hidden by default in this build. Ctrl+9 matches the
    // shortcut CAD users already have in their fingers.
    actCommandLine_ = new QAction(tr("Komut Satırı"), this);
    actCommandLine_->setCheckable(true);
    actCommandLine_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_9));
    connect(actCommandLine_, &QAction::toggled, this, &MainWindow::showCommandLine);
}

void MainWindow::buildToolBars()
{
    // ONE BAR, SEVEN GROUPS. design.md 7 draws a single 46 px strip with 1 px
    // rules between groups; five draggable QToolBars gave five handles, five
    // wrap points and a different arrangement on every start. The bar is fixed
    // because its layout is part of the specification, not a preference.
    //
    // Two surfaces, two jobs — the left tool box holds the modal DRAWING tools,
    // this bar holds ACTIONS. Every button dispatches a command; none reaches
    // the document directly (CLAUDE.md Article 1).
    tbMain_ = addToolBar(tr("Araçlar"));
    tbMain_->setObjectName(QStringLiteral("tbMain"));
    tbMain_->setIconSize(QSize(20, 20));
    tbMain_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tbMain_->setMovable(false);
    tbMain_->setFloatable(false);

    // file. YAZDIR sits with them, to the right of Kaydet: printing is what a
    // drafter does with a file, and it belongs in the group the file lives in.
    tbMain_->addAction(actNew_);
    tbMain_->addAction(actOpen_);
    tbMain_->addAction(actSave_);
    tbMain_->addAction(actPrint_);
    tbMain_->addAction(actPrintMenu_);
    if (auto* arrow = qobject_cast<QToolButton*>(tbMain_->widgetForAction(actPrintMenu_))) {
        printMenu_ = new QMenu(arrow);
        arrow->setIconSize(QSize(12, 12));
        arrow->setFixedWidth(20);
        // The list drops from the arrow's own bottom-left, so it reads as
        // belonging to the button beside it rather than to the bar.
        connect(actPrintMenu_, &QAction::triggered, this, [this, arrow] {
            printMenu_->popup(arrow->mapToGlobal(QPoint(0, arrow->height())));
        });
        // REBUILT EVERY TIME IT OPENS, like `Dosya ▸ Çıktı Yerleşimleri` (which is
        // why that one was right and this one was not).
        //
        // It used to be rebuilt from three places: once at startup, when a print
        // PROFILE changed, and from the one menu entry that creates a layout.
        // Opening a project was none of those — so a drawing whose layouts came
        // off disk listed none of them here, while the File menu listed them all.
        // So could a layout made on the command line, from a script, from a
        // template or over MCP. Chasing those call sites is a list that is wrong
        // again the next time somebody adds a way to make a layout; asking the
        // document when the menu opens is not.
        connect(printMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildPrintMenu);
        rebuildPrintMenu();
        connect(&controller_->printService(), &PrintService::profilesChanged, this,
                &MainWindow::rebuildPrintMenu);
    }
    tbMain_->addSeparator();

    // undo / redo
    tbMain_->addAction(actUndo_);
    tbMain_->addAction(actRedo_);
    tbMain_->addSeparator();

    // clipboard
    tbMain_->addAction(actCut_);
    tbMain_->addAction(actCopyClip_);
    tbMain_->addAction(actPaste_);
    tbMain_->addSeparator();

    // navigation
    tbMain_->addAction(actSelect_);
    tbMain_->addAction(actPan_);
    tbMain_->addAction(actZoomIn_);
    tbMain_->addAction(actZoomExtents_);
    tbMain_->addSeparator();

    // drawing aids
    tbMain_->addAction(actGridSnap_);
    tbMain_->addAction(actSnap_);
    tbMain_->addAction(actMeasure_);
    tbMain_->addSeparator();

    // windows
    tbMain_->addAction(actLayerManager_);
    tbMain_->addAction(actStyle_);
    tbMain_->addAction(actTable_);
    tbMain_->addSeparator();

    // the conversation, then the settings
    tbMain_->addAction(actAi_);
    tbMain_->addAction(actSettings_);

    // The two readings sit at the far right, so a spacer eats everything between.
    auto* gap = new QWidget(tbMain_);
    gap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    gap->setAttribute(Qt::WA_NoSystemBackground, true);
    tbMain_->addWidget(gap);

    readout_ = new ReadoutStrip(tbMain_);
    tbMain_->addWidget(readout_);
}

void MainWindow::buildMenus()
{
    // design.md 7: ten titles, in this order. The order is part of the
    // specification, not a preference — a user who learned where "Harita" sits
    // finds it in the same place on every platform because the bar is ours.
    QMenuBar* bar = titleBar_->menus();

    auto* file = bar->addMenu(tr("&Dosya"));
    file->addAction(actNew_);
    file->addAction(actOpen_);
    file->addAction(actSave_);
    file->addAction(actSaveAs_);
    file->addSeparator();
    file->addAction(actImport_);
    file->addAction(actExport_);
    file->addAction(actPrint_);

    // ---- ÇIKTIYERLEŞİMİLAR, where a QGIS user looks for layouts -----------------------
    //
    // UNDER `Dosya` AND NOT UNDER `Görünüm`, because a layout belongs to the
    // DOCUMENT: it is saved in the file, it is in the content hash, and it is
    // part of what gets signed. QGIS puts its layouts under `Project` for the
    // same reason. The submenu is rebuilt whenever the document changes, so a
    // sheet added at the command line appears here without anything being told.
    layoutMenu_ = file->addMenu(tr("&Çıktı Yerleşimleri"));
    connect(layoutMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildLayoutMenu);
    rebuildLayoutMenu();
    file->addSeparator();
    file->addAction(actProjectSettings_);
    file->addSeparator();
    file->addAction(actDatabase_);
    file->addAction(actScript_);
    file->addSeparator();
    file->addAction(actQuit_);

    auto* edit = bar->addMenu(tr("D&üzen"));
    edit->addAction(actUndo_);
    edit->addAction(actRedo_);
    edit->addSeparator();
    edit->addAction(actCut_);
    edit->addAction(actCopyClip_);
    edit->addAction(actPaste_);
    edit->addSeparator();
    edit->addAction(actSelectAll_);
    edit->addAction(actSelectNone_);
    edit->addSeparator();
    edit->addAction(actSettings_);

    auto* view = bar->addMenu(tr("&Görünüm"));
    view->addAction(actZoomExtents_);
    view->addAction(actZoomIn_);
    view->addAction(actZoomOut_);
    view->addSeparator();
    view->addAction(actSnap_);

    // The keyboard road to the same list the OSNAP chip's right click opens.
    // ui.md P7: nothing ships reachable only by mouse.
    auto* snapModes = new QAction(tr("Yakalama Modları…"), this);
    snapModes->setToolTip(tr("Hangi nesne yakalama modlarının açık olduğunu seçer"));
    snapModes->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(snapModes, &QAction::triggered, this, &MainWindow::openSnapModes);
    view->addAction(snapModes);
    addAction(snapModes); // so the shortcut works with focus anywhere in the shell
    view->addAction(actOrtho_);
    view->addAction(actNormal_);
    view->addAction(actGridSnap_);
    view->addSeparator();

    if (tbMain_) view->addAction(tbMain_->toggleViewAction());

    auto* panels = view->addMenu(tr("Paneller"));
    for (QDockWidget* dock : {layerDock_, propertyDock_, journalDock_}) {
        if (dock) panels->addAction(dock->toggleViewAction());
    }
    panels->addSeparator();
    panels->addAction(actCommandLine_);

    view->addSeparator();
    view->addAction(actTheme_);
    view->addAction(actHud_);

    auto* draw = bar->addMenu(tr("Çi&zim"));
    draw->addAction(actLine_);
    draw->addAction(actPolyline_);
    draw->addAction(actArc_);
    draw->addAction(actCircle_);
    draw->addAction(actEllipse_);
    draw->addAction(actSector_);
    draw->addAction(actAnnulus_);
    draw->addAction(actRectangle_);
    draw->addAction(actRegular_);
    draw->addAction(actPoint_);
    draw->addAction(actText_);
    draw->addSeparator();
    draw->addAction(actSpline_);
    draw->addAction(actHatch_);
    draw->addAction(actBlock_);
    draw->addAction(actInsert_);
    draw->addAction(actDimension_);
    draw->addAction(actLeader_);
    draw->addSeparator();
    // THE THREE THAT HAD NO ENTRY AT ALL. A guide line, a label read from a
    // layer's own attributes and a contour set are drawing tools a surveyor
    // reaches for, and all three were reachable only by typing their names.
    // Curated here rather than left to the generated tail, because a tool this
    // ordinary belongs on the menu a hand already opens.
    // THE SURVEY ENTRY, where a drawing actually starts for a crew with a tape.
    // Curated rather than left to the generated tail: this is the first tool a
    // Turkish surveyor reaches for, not an occasional one (TODOS-CAD P1b).
    draw->addAction(commandAction(Glyph::Function, tr("Poligon Hesabı"), QStringLiteral("POLİGON"),
                                  tr("POLİGON — kırılma açısı ve kenarlardan poligon "
                                     "koordinatları, kapanma dağıtımı ve mevzuat toleransı  ·  "
                                     "kısaltma: PLG")));
    draw->addAction(commandAction(Glyph::Point, tr("Kesişim Noktası"),
                                  QStringLiteral("KESİŞİMNOKTA"),
                                  tr("KESİŞİMNOKTA — iki doğrultunun, iki uzaklığın ya da iki "
                                     "doğrunun kesişimine nokta koyar  ·  kısaltma: KSN")));
    draw->addAction(commandAction(Glyph::Point, tr("Ara Nokta"), QStringLiteral("ARANOKTA"),
                                  tr("ARANOKTA — doğru üzerinde oran, uzaklık ya da eşit bölmeyle "
                                     "nokta koyar  ·  kısaltma: ARN")));
    draw->addAction(commandAction(Glyph::Locate, tr("Alım"), QStringLiteral("ALIM"),
                                  tr("ALIM — istasyondan okunan açı ve kenarlardan nokta "
                                     "hesaplar  ·  kısaltma: ALM")));
    draw->addAction(commandAction(Glyph::PerpOffset, tr("Dik Ayak"), QStringLiteral("DİKAYAK"),
                                  tr("DİKAYAK — taban çizgisine göre dik ayak ve dik boy vererek "
                                     "nokta yerleştirir  ·  kısaltma: DA")));
    draw->addAction(commandAction(Glyph::Line, tr("Kılavuz"), QStringLiteral("KILAVUZ"),
                                  tr("KILAVUZ — cetvel kılavuzu ekler, listeler ve siler  ·  "
                                     "kısaltma: KLV")));
    // THE ANGLED GUIDE IS A LINE THE RULER CANNOT GIVE. Dragging off the ruler
    // yields horizontal and vertical only; an angled one needs an angle, and an
    // angle is a number — so it has a row of its own that starts the command with
    // one already chosen and then asks for the point it passes through.
    draw->addAction(commandAction(Glyph::Ruler, tr("Açılı Kılavuz"),
                                  QStringLiteral("KILAVUZ yon=45g"),
                                  tr("KILAVUZ yon=<açı> — verilen noktadan geçen açılı kılavuz; "
                                     "açı oturumun birim ve kuralıyla okunur  ·  kısaltma: KLV")));
    draw->addAction(commandAction(Glyph::Text, tr("Etiket"), QStringLiteral("ETİKET"),
                                  tr("ETİKET — katmandaki nesneleri özniteliklerinden okuyarak "
                                     "etiketler  ·  kısaltma: ETK")));
    draw->addAction(
        commandAction(Glyph::Function, tr("Eşyükselti Eğrileri"), QStringLiteral("EŞYÜKSELTİ"),
                      tr("EŞYÜKSELTİ — kotlu noktalardan eş yükselti eğrileri çizer  ·  "
                         "kısaltma: EŞY")));

    auto* modify = bar->addMenu(tr("D&eğiştir"));
    modify->addAction(actErase_);
    modify->addSeparator();
    modify->addAction(actMove_);
    modify->addAction(actCopy_);
    modify->addAction(actRotate_);
    modify->addAction(actScale_);
    modify->addAction(actMirror_);
    modify->addAction(actArray_);
    modify->addSeparator();
    modify->addAction(actTrim_);
    modify->addAction(actExtend_);
    modify->addAction(actSplit_);
    modify->addAction(actCombine_);
    modify->addAction(actChamfer_);
    modify->addAction(actFillet_);
    modify->addSeparator();
    modify->addAction(actSetLayer_);
    modify->addAction(actStyleCopy_);
    modify->addSeparator();
    modify->addAction(actOffset_);
    modify->addSeparator();
    // THE SEVEN VERBS P3 ADDED, on the menu a hand already opens for the others.
    // A generated tail would have put them behind `Diğer komutlar`, which is the
    // right place for a command nobody reaches for and the wrong one for KIR.
    modify->addAction(commandAction(Glyph::Cut, tr("Kır"), QStringLiteral("KIR"),
                                    tr("KIR — iki nokta arasındaki parçayı çıkarır; tek nokta "
                                       "boşluksuz böler  ·  kısaltma: KR")));
    modify->addAction(commandAction(Glyph::Line, tr("Uç Uca Ekle"), QStringLiteral("UÇUCA"),
                                    tr("UÇUCA — uçları değen çizgileri tek çizgiye ekler; "
                                       "BİRLEŞTİR ile karıştırmayın  ·  kısaltma: UÇE")));
    modify->addAction(commandAction(Glyph::Ruler, tr("Uzunluk"), QStringLiteral("UZUNLUK"),
                                    tr("UZUNLUK — bir ucu kendi doğrultusunda hareket ettirir  ·  "
                                       "kısaltma: UZN")));
    modify->addAction(actStretch_);
    modify->addAction(commandAction(Glyph::Duplicate, tr("Patlat"), QStringLiteral("PATLAT"),
                                    tr("PATLAT — çizgiyi kenarlara, alanı sınırına, bloğu "
                                       "bileşenlerine ayırır  ·  kısaltma: PTL")));
    modify->addAction(commandAction(Glyph::Move, tr("Hizala"), QStringLiteral("HİZALA"),
                                    tr("HİZALA — bir ya da iki nokta çiftiyle taşır, döndürür ve "
                                       "istenirse ölçekler  ·  kısaltma: HZL")));
    modify->addAction(commandAction(Glyph::Point, tr("Bölümle"), QStringLiteral("BÖLÜMLE"),
                                    tr("BÖLÜMLE — nesne boyunca eşit parçalara ya da sabit "
                                       "aralıkla nokta koyar  ·  kısaltma: BLM")));
    modify->addAction(commandAction(Glyph::Polyline, tr("Çizgi Düzenle"),
                                    QStringLiteral("ÇİZGİDÜZENLE"),
                                    tr("ÇİZGİDÜZENLE — kapatır, açar, yönünü çevirir ya da "
                                       "sadeleştirir  ·  kısaltma: ÇZD")));

    // A MENU OF THEIR OWN, because they are a different kind of thing. Each of
    // these is an act with a regulation behind it, and grouping them says so; the
    // everyday geometry that resembles them is one menu to the left.
    auto* cadastre = bar->addMenu(tr("K&adastro"));
    cadastre->addAction(actParcelSplit_);
    cadastre->addAction(actAreaSplit_);
    cadastre->addAction(actUnion_);
    cadastre->addSeparator();
    cadastre->addAction(actTopology_);

    auto* map = bar->addMenu(tr("&Harita"));
    map->addAction(actIdentify_);
    map->addAction(actEntityInfo_);
    map->addAction(actMeasure_);
    map->addAction(actMeasureArea_);
    map->addAction(actMeasureAngle_);
    map->addAction(actCoordinate_);
    map->addSeparator();
    // THE SURVEY COMPUTATIONS. `APLİKASYON` is what a crew takes to the field
    // and `HACİM` is what an earthwork report is made of; neither had a way in.
    map->addAction(commandAction(Glyph::Function, tr("Poligon Hesabı"), QStringLiteral("POLİGON"),
                                 tr("POLİGON — poligon güzergâhını hesaplar, kapanma hatalarını "
                                    "dağıtır ve mevzuat toleransına karşı denetler  ·  "
                                    "kısaltma: PLG")));
    map->addAction(commandAction(Glyph::Locate, tr("Aplikasyon"), QStringLiteral("APLİKASYON"),
                                 tr("APLİKASYON — istasyondan hedefe semt açısı ve kenar  ·  "
                                    "kısaltma: APL")));
    map->addAction(commandAction(Glyph::Function, tr("Hacim Hesabı"), QStringLiteral("HACİM"),
                                 tr("HACİM — iki yüzey arasındaki kazı ve dolgu hacmi  ·  "
                                    "kısaltma: HCM")));
    map->addAction(commandAction(Glyph::Function, tr("Oturt (Helmert)"), QStringLiteral("OTURT"),
                                 tr("OTURT — ortak noktalardan Helmert dönüşümüyle çizimi "
                                    "oturtur  ·  kısaltma: OTR")));
    map->addAction(commandAction(Glyph::Layer, tr("Dönüştür"), QStringLiteral("DÖNÜŞTÜR"),
                                 tr("DÖNÜŞTÜR — çizimi başka bir koordinat sistemine "
                                    "dönüştürür  ·  kısaltma: DNS")));
    map->addSeparator();
    map->addAction(actDatabase_);

    auto* analyse = bar->addMenu(tr("&Analiz"));
    analyse->addAction(actTable_);
    analyse->addSeparator();

    // THE PROCESSING TOOLS, from the registry: one entry per tool, and the panel
    // that shows them as a tree. Nothing here is a second list (CLAUDE.md 5.10).
    auto* tools = analyse->addMenu(tr("İşlem Araçları"));
    for (const processing::ProcessingTool* tool : processing::processing_tools()) {
        const auto& spec = tool->spec();
        tools->addAction(commandAction(Glyph::Function, QString::fromStdString(spec.title),
                                       QString::fromStdString(spec.names.front()),
                                       QString::fromStdString(spec.summary)));
    }
    tools->addSeparator();
    auto* showTools = tools->addAction(tr("Araçlar Paneli"));
    showTools->setStatusTip(tr("Sağ paneldeki Araçlar sekmesini açar"));
    connect(showTools, &QAction::triggered, this, [this] {
        propertyDock_->show();
        propertyHeader_->setCurrent(2);
        propertyStack_->setCurrentIndex(2);
    });
    analyse->addSeparator();
    analyse->addAction(actAi_);

    // ---- THE AGENT LISTENER, one entry that toggles ----
    //
    // It runs `MCPSUNUCU`, exactly as the status cell does, so there is one road
    // to the listener and a script can take it too (Article 1.2, 5.15). The
    // wording follows the state rather than describing both: a menu that says
    // `Başlat/Durdur` makes the reader work out which one they are about to do.
    actMcp_ = new QAction(tr("MCP Sunucusunu Başlat"), this);
    actMcp_->setData(static_cast<int>(Glyph::Server));
    actMcp_->setStatusTip(tr("Yapay zeka ajanlarının bağlanacağı yerel sunucuyu açar"));
    actMcp_->setProperty(kToolCommand, QStringLiteral("MCPSUNUCU"));
    connect(actMcp_, &QAction::triggered, this, [this] {
#if KENTOS_HAVE_MCP
        const bool up =
            controller_->mcpService() != nullptr && controller_->mcpService()->listening();
        controller_->runLine(up ? QStringLiteral("MCPSUNUCU islem=durdur")
                                : QStringLiteral("MCPSUNUCU islem=baslat"),
                             command::Origin::Gui);
#else
        onEcho(tr("Bu yapıda MCP sunucusu yok (KENTOS_WITH_MCP kapalı)."));
#endif
    });
    analyse->addAction(actMcp_);
    QAction* mcpToken = analyse->addAction(tr("MCP Belirteci Üret"));
    mcpToken->setStatusTip(tr("Yeni bir erişim belirteci üretir; eskisi geçersiz olur"));
    connect(mcpToken, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("MCPSUNUCU islem=belirtec"), command::Origin::Gui);
    });

    auto* layer = bar->addMenu(tr("&Katman"));
    layer->addAction(actLayer_);
    layer->addAction(actLayerManager_);

    // THE TWO THAT NEED NO ROW. `Tümünü göster` and `Gösterimi ters çevir` are
    // about the whole table, so they belong where a user looks for something that
    // is not about one layer — and a user whose layers are all hidden has no row
    // left to right-click. The per-layer visibility entries stay in the panel's
    // own `Görünüm` submenu, beside the layer they act on.
    layer->addSeparator();
    QAction* showAll = layer->addAction(tr("Tümünü Göster"));
    showAll->setToolTip(tr("KATMANGÖRÜNÜM islem=tumu — gizli bütün katmanları geri getirir"));
    connect(showAll, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=tumu"), command::Origin::Gui);
    });
    QAction* flipAll = layer->addAction(tr("Gösterimi Ters Çevir")); // ui-label
    flipAll->setToolTip(tr("KATMANGÖRÜNÜM islem=tersine — görünenleri gizler, gizlileri gösterir"));
    connect(flipAll, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=tersine"), command::Origin::Gui);
    });

    auto* window = bar->addMenu(tr("&Pencere"));
    for (QDockWidget* dock : {layerDock_, propertyDock_, chatDock_, journalDock_}) {
        if (dock) window->addAction(dock->toggleViewAction());
    }
    window->addSeparator();
    auto* reset = window->addAction(tr("Yerleşimi Sıfırla"));
    connect(reset, &QAction::triggered, this, &MainWindow::resetLayout);

    auto* about = bar->addMenu(tr("&Yardım"));
    auto* ref   = about->addAction(tr("Komut Listesi"));
    // F1 EXPLICITLY, not `QKeySequence::HelpContents`, which is `Cmd+?` on
    // macOS: every CAD program this one sits beside answers F1 with help, and a
    // shortcut the manual prints has to be the shortcut on all three platforms.
    ref->setShortcut(QKeySequence(Qt::Key_F1));
    ref->setShortcutContext(Qt::ApplicationShortcut);
    // THE MENU RUNS THE COMMAND, and the command opens the page. It used to pour
    // the whole generated reference into a `QMessageBox`, which has no scroll
    // area for its text and so grew a window taller than the screen — the
    // user's words were that it does not scroll and is awful. Going through
    // `YARDIM` rather than calling the page directly keeps the menu an ordinary
    // client: the same line, the same transcript, the same page a typed `YARDIM`
    // or `Ctrl+K` gets (Article 1.2).
    ref->setProperty(kToolCommand, QStringLiteral("YARDIM"));
    connect(ref, &QAction::triggered, this,
            [this] { controller_->runLine(QStringLiteral("YARDIM"), command::Origin::Gui); });
    about->addSeparator();
    auto* info = about->addAction(tr("Hakkında"));
    connect(info, &QAction::triggered, this, &MainWindow::showAbout);

    // AND EVERYTHING ELSE THE REGISTRY KNOWS, so the bar is complete rather than
    // remembered. Last, because it reads what the curated entries above already
    // offer.
    completeMenusFromRegistry();
}

namespace {

/// The mark a generated menu entry wears: its category's, because a generated
/// entry has no drawing of its own and a wrong picture is worse than a generic
/// one. A command that deserves its own mark gets a curated entry instead.
Glyph glyph_of(command::Category c)
{
    switch (c) {
    case command::Category::Draw: return Glyph::Line;
    case command::Category::Modify: return Glyph::Move;
    case command::Category::View: return Glyph::ZoomExtents;
    case command::Category::Layer: return Glyph::Layer;
    case command::Category::File: return Glyph::Save;
    case command::Category::Query: return Glyph::Identify;
    case command::Category::Processing: return Glyph::Function;
    case command::Category::Script: return Glyph::Script;
    case command::Category::System: return Glyph::Settings;
    }
    return Glyph::Function;
}

/// `EŞYÜKSELTİ` -> `Eşyükselti`, with Turkish casing.
///
/// `QLocale(QLocale::Turkish)` rather than `<cctype>`: the dotted and dotless i
/// are two letters in this language and `std::tolower('İ')` gets both of them
/// wrong (CLAUDE.md 5.6). `İŞŞABLONU` lower-cases to `işşablonu` and its first
/// letter upper-cases back to `İ`, which is the point.
QString turkish_title(const QString& shouted)
{
    static const QLocale tr_TR(QLocale::Turkish, QLocale::Turkey);
    const QString lower = tr_TR.toLower(shouted);
    if (lower.isEmpty()) return lower;
    return tr_TR.toUpper(lower.left(1)) + lower.mid(1);
}

} // namespace

void MainWindow::completeMenusFromRegistry()
{
    QMenuBar* bar = titleBar_->menus();

    // WHICH MENU A CATEGORY BELONGS TO, declared once. The bar has ten titles and
    // the registry has nine categories, and they are not the same cut: `Kadastro`
    // is a workflow and `Sorgu` is a kind of command. The curated entries above
    // already put the regulated cadastral acts where a surveyor looks for them;
    // this only decides where a command with NO chosen place goes.
    struct Home
    {
        const char* menu;        ///< the menu title, as `addMenu` wrote it
        command::Category takes; ///< the category whose leftovers land there
        QAction* before;         ///< the entry it must stay above; null appends
    };

    // QUIT IS LAST. An entry appended to the file menu lands under it, which is
    // where nothing belongs: Quit is the floor of that menu on every platform,
    // and a generated tail below it reads as a mistake — a user reported exactly
    // that, curious items showing up below the last row. A menu with a terminal
    // entry names it here and the tail goes above.
    const Home homes[] = {
        {"Çi&zim", command::Category::Draw, nullptr},
        {"D&eğiştir", command::Category::Modify, nullptr},
        {"&Görünüm", command::Category::View, actTheme_},
        {"&Katman", command::Category::Layer, nullptr},
        {"&Dosya", command::Category::File, actQuit_},
        {"&Harita", command::Category::Query, nullptr},
        {"&Analiz", command::Category::Processing, nullptr},
        {"D&üzen", command::Category::System, actSettings_},
        {"&Dosya", command::Category::Script, actQuit_},
    };

    // Every command any action in this window can already start, resolved through
    // the registry so an alias on a button counts as the command it names.
    QSet<QString> already;
    for (QAction* action : findChildren<QAction*>()) {
        QString word = action->property(kToolCommand).toString();
        if (word.isEmpty() && action->objectName().startsWith(QStringLiteral("toolAction.")))
            word = action->objectName().section(QLatin1Char('.'), 1);
        if (word.isEmpty()) continue;
        if (const command::CommandSpec* spec =
                controller_->registry().resolve(word.section(QLatin1Char(' '), 0, 0).toStdString());
            spec != nullptr)
            already.insert(QString::fromStdString(spec->id));
    }

    for (const Home& home : homes) {
        QMenu* menu = nullptr;
        for (QAction* action : bar->actions())
            if (action->menu() != nullptr && action->text() == QString::fromUtf8(home.menu))
                menu = action->menu();
        if (menu == nullptr) continue;

        QList<QAction*> added;
        for (const command::CommandSpec& spec : controller_->registry().all()) {
            if (spec.category != home.takes || spec.names.empty()) continue;
            if (already.contains(QString::fromStdString(spec.id))) continue;

            const QString word = QString::fromStdString(spec.names.front());
            // THE SPEC'S OWN LABEL. A name is one word by design (`ÇIKTIYERLEŞİMİ`)
            // and a menu built from it reads as one word, which is not Turkish;
            // `title` is where the spaces live. The fallback is right for a
            // one-word command and the only honest guess for any other.
            const QString label =
                spec.title.empty() ? turkish_title(word) : QString::fromStdString(spec.title);
            QAction* entry =
                commandAction(glyph_of(spec.category), label, word,
                              word + QStringLiteral(" — ") + QString::fromStdString(spec.summary));
            added << entry;
        }
        if (added.isEmpty()) continue;

        // ONE ROW, ALWAYS A SUBMENU. `Düzenleme` alone has thirty-one commands
        // and a menu that runs off the screen is the problem this change is
        // fixing, not a milder version of it — and a tail of loose rows changes
        // the shape of a menu the user has learned. One labelled row does not.
        auto* rest = new QMenu(tr("Diğer komutlar"), menu);
        for (QAction* entry : added)
            rest->addAction(entry);

        if (home.before != nullptr && menu->actions().contains(home.before)) {
            // A rule of its own above the anchor, then the row above that rule:
            // the result is `… | Diğer komutlar | ─── | Çıkış`, with the menu's
            // own foot untouched.
            QAction* rule = menu->insertSeparator(home.before);
            menu->insertMenu(rule, rest);
        } else {
            menu->addSeparator();
            menu->addMenu(rest);
        }
    }
}

void MainWindow::buildToolBox()
{
    // The modal drawing tools only, in the five groups design.md 7 names. File,
    // edit, view, layer and GIS actions live in the horizontal bar, the way
    // AutoCAD and QGIS both arrange them.
    toolBox_ = new ToolBox(this);

    // selection
    toolBox_->addTool(actSelect_);
    toolBox_->addTool(actSelectArea_);
    toolBox_->addTool(actPan_);
    toolBox_->addSeparator();

    // creation
    // ÇİZGİ FIRST, and it was missing entirely. It is the one command in this
    // program that works today end to end — the tool box listed the five that do
    // not and left out the one that does, which is the opposite of useful.
    //
    // FAMILIES, not one button each. Eleven creation tools down a 46 px column is
    // a list nobody reads. The button shows whichever member was used last and
    // holds the rest one press away, which is how every CAD tool palette has
    // answered this since the first one.
    //
    // GROUPED BY WHAT COMES OUT, not by whether the pen moves in a straight line.
    // "Straight edges" put DİKDÖRTGEN and ÇOKGEN under the line button, and a
    // rectangle is not a kind of line — it is a FACE, with an area, a perimeter
    // and a fill, and it belongs beside the other tool that makes one. The same
    // mistake put YAY under DAİRE: a circle is closed and encloses something, an
    // arc is an open run of edge and encloses nothing.
    //
    //   line   — an open run of edges
    //   face   — a closed face
    //   circle — a closed curve
    //   arc    — an open curve, and the sector cut from one
    //
    // The flyout also PRINTS THE COMMAND WORD beside each name, so the mouse
    // teaches the keyboard: a user who found ÇOKLUÇİZGİ under the line button
    // has just been told what to type tomorrow (CLAUDE.md 5.15).
    //   spline — an open run of edges too, drawn smooth; it joins the line family
    //   hatch  — a closed face with a pattern; it joins the face family
    //   block  — placing and defining: the placement first, it is the daily one
    //   note   — a dimension and a leader both annotate: one family
    toolBox_->addFamily({actLine_, actPolyline_, actSpline_});
    toolBox_->addFamily({
        actRectangle_,
        methodTool(Glyph::Rectangle, tr("Dikdörtgen — döndürülmüş"),
                   QStringLiteral("DİKDÖRTGEN yontem=3n"),
                   tr("Bir kenarın iki köşesi ve yüksekliği veren üçüncü nokta")),
        actPolygon_,
        actRegular_,
        methodTool(Glyph::Polygon, tr("Çokgen — dıştan"), QStringLiteral("ÇOKGEN yontem=dis"),
                   tr("Kenarlar çembere teğet; yarıçap iç yarıçaptır")),
        methodTool(Glyph::Polygon, tr("Çokgen — kenardan"), QStringLiteral("ÇOKGEN yontem=kenar"),
                   tr("Kenar uzunluğundan; yarıçap sorulmaz")),
        actHatch_,
    });
    toolBox_->addFamily({
        actCircle_,
        methodTool(Glyph::Circle, tr("Daire — çapın iki ucu"), QStringLiteral("DAİRE yontem=2n"),
                   tr("İki nokta çapı verir; merkez ortalarıdır")),
        methodTool(Glyph::Circle, tr("Daire — üç nokta"), QStringLiteral("DAİRE yontem=3n"),
                   tr("Çevrel çember: üç noktanın hepsi çemberin üzerinde")),
        methodTool(Glyph::Circle, tr("Daire — iki doğruya teğet"),
                   QStringLiteral("DAİRE yontem=ttr"),
                   tr("İki doğru, yarıçap ve dairenin geleceği köşe gösterilir")),
        actEllipse_,
        methodTool(Glyph::Ellipse, tr("Elips — eksenin iki ucu"),
                   QStringLiteral("ELİPS yontem=eksen"),
                   tr("Merkez iki ucun ortasıdır; üçüncü nokta ikinci ekseni verir")),
        actAnnulus_,
    });
    // YAY was built as a full draw tool and then left out of the column, so the
    // one curve this program can draw was reachable only by typing its name.
    // EVERY CLASSICAL METHOD, under the face it belongs to. A family is exactly
    // the shape this wants: one button a hand reaches for, and the variants a
    // press-and-hold reveals.
    toolBox_->addFamily({
        actArc_,
        methodTool(Glyph::Arc, tr("Yay — üç nokta"), QStringLiteral("YAY yontem=3n"),
                   tr("Başlangıç, üzerinden geçtiği nokta ve bitiş")),
        methodTool(Glyph::Arc, tr("Yay — başlangıç, merkez, açı"), QStringLiteral("YAY yontem=bma"),
                   tr("Süpürme açısı oturumun birim ve kuralıyla okunur")),
        methodTool(Glyph::Arc, tr("Yay — başlangıç, bitiş, yarıçap"),
                   QStringLiteral("YAY yontem=bby"),
                   tr("İki çözüm vardır; yon=sol|sag hangisi olduğunu söyler")),
        methodTool(Glyph::Arc, tr("Yay — teğet devam"), QStringLiteral("YAY yontem=devam"),
                   tr("Son çizilen çizginin ya da yayın ucundan teğet devam eder")),
        actSector_,
    });
    // NOKTA AND THE TWO WAYS A MEASURED POINT ARRIVES. A point clicked on the
    // canvas and a point computed from a baseline are the same kind of thing to
    // a surveyor, and the second is what a tape survey produces all day.
    // AND THE METHODS, under the tool they belong to. `KESİŞİMNOKTA` has three
    // and `ARANOKTA` two, and only the default of each was reachable from the
    // card — so the two-distance intersection, which is how a boundary is
    // recovered from two tape measurements, could be run only by typing its
    // method (§2.6a: a tool that arrives brings its UI with it).
    toolBox_->addFamily({
        actPoint_,
        actPerpOffset_,
        actSurvey_,
        actIntersect_,
        methodTool(Glyph::PointIntersect, tr("Kesişim — iki mesafeden"),
                   QStringLiteral("KESİŞİMNOKTA yontem=mesafe"),
                   tr("İki bilinen noktadan ölçülen iki uzaklık; iki çözümden birini "
                      "gösterirsiniz")),
        methodTool(Glyph::PointIntersect, tr("Kesişim — iki doğrudan"),
                   QStringLiteral("KESİŞİMNOKTA yontem=dogru"),
                   tr("İki doğrunun her birinden iki nokta")),
        actAlong_,
        methodTool(Glyph::PointAlong, tr("Ara Nokta — mesafeden"),
                   QStringLiteral("ARANOKTA yontem=mesafe"),
                   tr("Oran değil, ilk noktadan metre cinsinden uzaklık")),
    });
    toolBox_->addTool(actText_);
    toolBox_->addFamily({actInsert_, actBlock_});
    toolBox_->addFamily({actDimension_, actLeader_});
    toolBox_->addSeparator();

    // editing
    //
    // THE GENERIC PAIR, not the cadastral one. `TEVHİT` and `İFRAZ` used to sit
    // here, which put two regulated cadastral acts among the everyday edit tools
    // and left the ordinary "merge these two shapes" with no button at all. They
    // are in the Kadastro menu now; these two are geometry and work on anything.
    toolBox_->addTool(actTrim_);
    toolBox_->addTool(actCombine_);
    toolBox_->addTool(actSplit_);
    toolBox_->addTool(actMove_);
    toolBox_->addTool(actStretch_);
    toolBox_->addTool(actOffset_);
    toolBox_->addSeparator();

    // measurement
    toolBox_->addFamily(
        {actMeasure_, actMeasureArea_, actMeasureAngle_, actCoordinate_, actEntityInfo_});
    toolBox_->addSeparator();

    // helpers
    toolBox_->addTool(actStyleCopy_);
    toolBox_->addTool(actTopology_);

    connect(toolBox_->chips(), &ColourChips::chipActivated, this, [this](int which) {
        onEcho(which == 0 ? tr("Çizim rengi: katmanın rengi geçerlidir. RENK komutu Faz 2.")
                          : tr("Dolgu rengi: katmanın dolgusu geçerlidir. RENK komutu Faz 2."));
    });
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

    layerHeader_ = new PanelHeader(this);
    layerHeader_->addTab(tr("Katmanlar"), static_cast<int>(Glyph::Layer));
    layerHeader_->setButtons(PanelHeader::Add | PanelHeader::Filter | PanelHeader::Grip |
                             PanelHeader::Collapse | PanelHeader::Float);
    connect(layerHeader_, &PanelHeader::buttonPressed, this, [this](int button) {
        // The panel's own two marks; the dock marks are answered where every
        // header's are.
        if (button == PanelHeader::Add) layerPanel_->addLayerInteractively();
        if (button == PanelHeader::Filter) layerPanel_->toggleFilter();
    });

    layerDock_ = makeDock(QStringLiteral("layerDock"), layerHeader_, layerPanel_);
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

    chatHeader_ = new PanelHeader(this);
    chatHeader_->addTab(tr("Yapay Zeka"), static_cast<int>(Glyph::Chat));
    chatHeader_->setButtons(PanelHeader::Grip | PanelHeader::Float | PanelHeader::Close);

    chatDock_ = makeDock(QStringLiteral("chatDock"), chatHeader_, chatPanel_);
    chatDock_->toggleViewAction()->setText(tr("Yapay Zeka"));

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

    // The reference has no bottom panel open: the command line carries the
    // conversation and the journal is there when a user asks for it. The chat is
    // closed for the same reason — an assistant panel nobody opened is an
    // assistant panel taking a third of the screen.
    journalDock_->hide();
    chatDock_->hide();

    // 312 px wide, and the layers panel 268 px tall — both from design.md 7.
    resizeDocks({propertyDock_, layerDock_}, {312, 312}, Qt::Horizontal);
    resizeDocks({propertyDock_, layerDock_}, {600, 268}, Qt::Vertical);

    // These fire during TEARDOWN as well as during use — see the note in
    // `~MainWindow`, which is where the connection is severed.
    for (QDockWidget* dock : {propertyDock_, layerDock_, chatDock_, journalDock_}) {
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
    // of actions to maintain.
    for (QAction* action : findChildren<QAction*>()) {
        const QVariant glyph = action->data();
        if (!glyph.isValid()) continue;
        action->setIcon(icon(static_cast<Glyph>(glyph.toInt()), p.text, p.accent));
    }

    // ONE WALK, not a list of calls. A list is a thing to forget an entry in,
    // and the settings window's sidebar proved it: every painted widget declares
    // `Themed` and this hands the theme to all of them at once.
    applyThemeToChildren(this, theme_);
    if (palette_) palette_->applyTheme(theme_);
    if (settings_) settings_->applyTheme(theme_);
}

void MainWindow::onSettingChanged(const QString& id)
{
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

void MainWindow::refreshLayerCombo()
{
    // The tool bar combo is gone with design.md 7's single 46 px strip, which has
    // no combo in it. The active layer is shown and changed in the Katmanlar
    // panel instead — the same `KATMAN` command either way (Article 1.2), so no
    // capability moved with the widget.
    if (layerPanel_) layerPanel_->refresh();
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

    if (toolBox_ == nullptr) {
        say(QStringLiteral("araç kutusu yok"));
        return;
    }

    // The button whose face is ÇİZGİ — found by what it carries rather than by
    // its position in the column, so re-ordering the palette cannot make this
    // probe silently check a different button.
    QToolButton* line = nullptr;
    for (QToolButton* button : toolBox_->findChildren<QToolButton*>()) {
        const QAction* face = button->defaultAction();
        if (face != nullptr && face->property(kToolCommand).toString() == QStringLiteral("ÇİZGİ"))
            line = button;
    }
    if (line == nullptr) {
        say(QStringLiteral("ÇİZGİ düğmesi yok"));
        return;
    }

    // A RIGHT CLICK, which opens the family at once — the held press takes 280 ms
    // of real time and a probe that slept for it would be a probe that sometimes
    // did not.
    const QPointF centre(line->width() / 2.0, line->height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, centre,
                      QPointF(line->mapToGlobal(centre.toPoint())), Qt::RightButton,
                      Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(line, &press);

    // NO `processEvents` between the press and the check. A `Qt::Popup` grabs the
    // pointer, and the real pointer is not where this synthetic press said it
    // was — so the first turn of the loop delivers a click outside the card and
    // Qt closes it. `sendEvent` is synchronous; by the time it returns the card
    // is up, and everything below drives it directly.
    auto* card = toolBox_->findChild<ToolFlyout*>();
    if (card == nullptr) {
        say(QStringLiteral("kart yok"));
        return;
    }
    if (!card->isVisible()) {
        say(QStringLiteral("kart görünmez"));
        return;
    }
    say(QStringLiteral("kart açıldı"));

    // THE RELEASE THAT ENDS THE HOLD, and it arrives HERE rather than at the
    // button: a `Qt::Popup` grabs the pointer the moment it opens. The pointer is
    // still over the button, so in the card's own coordinates it is off every row
    // — and closing on that made the card vanish in the same motion that opened
    // it. Nobody could hold, look, and then choose.
    const QPointF outside(-20, 10);
    QMouseEvent letGo(QEvent::MouseButtonRelease, outside,
                      QPointF(card->mapToGlobal(QPoint(-20, 10))), Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(card, &letGo);
    say(QStringLiteral("bırakınca: %1")
            .arg(card->isVisible() ? QStringLiteral("açık") : QStringLiteral("kapandı")));
    if (!card->isVisible()) return;

    // WHAT IT LISTS, in order. The card is the only place a user is told that the
    // button they pressed sends `ÇİZGİ` and that `ÇOKLUÇİZGİ` is beside it.
    QStringList members;
    for (const QAction* member : card->members())
        members << member->property(kToolCommand).toString();

    // The second member, because taking the first would prove nothing the button
    // did not already do.
    say(QStringLiteral("üyeler: %1").arg(members.join(QStringLiteral(" · "))));

    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QKeyEvent again(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(card, &down);
    QCoreApplication::sendEvent(card, &again);
    QCoreApplication::sendEvent(card, &enter);

    const QAction* face = line->defaultAction();
    say(QStringLiteral("düğmenin yüzü: %1")
            .arg(face != nullptr ? face->property(kToolCommand).toString() : QString()));

    const command::Session* running = controller_->session();
    say(QStringLiteral("çalışan komut: %1")
            .arg(running != nullptr ? QString::fromStdString(running->spec().id)
                                    : QStringLiteral("yok")));
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
    if (!confirmDiscard(tr("Kapatmadan önce kaydedilsin mi?"))) {
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
    // A family button opens its card on a press held past `kHoldMs`. A human
    // click is not instant, so a slow one runs nothing at all: the card opens,
    // the release lands on the card, and the tool the user reached for never
    // starts. That is what "drawing never works, all three of them" looks like —
    // all three are the members of ONE family button.
    const auto clickButton = [](QToolButton* button, int heldMs) {
        const QPointF centre(button->width() / 2.0, button->height() / 2.0);
        QMouseEvent down(QEvent::MouseButtonPress, centre, button->mapToGlobal(centre),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(button, &down);

        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < heldMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

        QWidget* card     = QApplication::activePopupWidget();
        const bool opened = card != nullptr && card->property("kentos.rows").isValid();

        // RELEASED WHERE IT WAS PRESSED, which is what a click is. A hand that
        // wanted the card moves onto it first; this one did not move at all.
        QMouseEvent up(QEvent::MouseButtonRelease, centre, button->mapToGlobal(centre),
                       Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(button, &up);
        QCoreApplication::processEvents();
        return opened;
    };

    QToolButton* lineButton = nullptr;
    for (QToolButton* button : toolBox_->buttons())
        if (QAction* face = button->defaultAction();
            face != nullptr && face->property(kToolCommand).toString() == QStringLiteral("ÇİZGİ"))
            lineButton = button;
    // The face follows the last member used, so ask for the family by membership.
    if (lineButton == nullptr)
        for (QToolButton* button : toolBox_->buttons())
            if (button->property("kentos.family").toBool() && lineButton == nullptr)
                lineButton = button;

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
        (void)std::fprintf(stdout, "[fare] %3d ms basılı: kart=%s komut=%s\n", heldMs,
                           opened ? "açıldı" : "hayır", running ? "çalıştı" : "HAYIR");
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        QCoreApplication::processEvents();

        // A CLICK IS A CLICK, at any interval a hand produces. Not "something
        // happened": the TOOL has to run. A card opening in place of the tool is
        // the defect — an open Qt popup grabs the mouse, so the next press on the
        // same button only dismisses the card and is swallowed, and a user whose
        // clicks are all slow never reaches the tool at all.
        check(running, QStringLiteral("%1 ms'lik tıklama aracı çalıştırdı").arg(heldMs));
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
        check(actErase_->shortcuts().contains(QKeySequence(Qt::Key_Backspace)),
              QStringLiteral("SİL, ⌫ tuşuna bağlı"));
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

    (void)std::fprintf(stdout, "[fare] %d kusur\n", failures);
    return failures;
}

QImage MainWindow::probePicture()
{
    QImage picture = grab().toImage();
    if (const QImage live = canvas_->grabCanvas(); !live.isNull() && !picture.isNull()) {
        QPainter painter(&picture);
        painter.drawImage(QRect(canvas_->mapTo(this, QPoint(0, 0)), canvas_->size()), live);
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

    // WHERE EVERYTHING IS, in the coordinates the window system clicks in. A
    // family button is given twice: its centre, which runs the face, and the
    // wedge in its corner, which a plain left click opens the card with — the
    // one card gesture a driver without a right button or a hold can make.
    const QRect frame = frameGeometry();
    say(QStringLiteral("pencere %1 %2 %3 %4")
            .arg(frame.x())
            .arg(frame.y())
            .arg(frame.width())
            .arg(frame.height()));
    for (QToolButton* button : toolBox_->buttons()) {
        QAction* face = button->defaultAction();
        if (face == nullptr) continue;
        const QPoint at = centre(button);
        if (button->property("kentos.family").toBool()) {
            const QPoint corner =
                button->mapToGlobal(QPoint(button->width() - 2, button->height() - 2));
            say(QStringLiteral("aile %1 %2 %3 kose %4 %5")
                    .arg(name_of(face))
                    .arg(at.x())
                    .arg(at.y())
                    .arg(corner.x())
                    .arg(corner.y()));
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
    const auto park = [this](QAction* notThis) {
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        controller_->cancelInteractive();
        QAction* elsewhere = notThis == actSelect_ ? actLine_ : actSelect_;
        elsewhere->setChecked(true);
        QCoreApplication::processEvents();
    };

    // ---- 1. WHAT THE TREE SAYS ABOUT EVERY BUTTON --------------------------
    //
    // `ui.md` R22: an interactive widget carries an `accessibleName` and an
    // `accessibleDescription`. Qt copies an action's text and tool tip onto a
    // button but copies neither of those, so a column of buttons can be perfectly
    // labelled on screen and silent to a screen reader.
    for (QToolButton* button : toolBox_->buttons()) {
        QAction* face = button->defaultAction();
        if (face == nullptr) continue;
        const QString who = name_of(face);

        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(button);
        check(iface != nullptr, QStringLiteral("%1: erişilebilirlik arayüzü var").arg(who));
        if (iface == nullptr) continue;

        check(!iface->text(QAccessible::Name).trimmed().isEmpty(),
              QStringLiteral("%1: erişilebilir adı var").arg(who));
        check(!iface->text(QAccessible::Description).trimmed().isEmpty(),
              QStringLiteral("%1: erişilebilir açıklaması var").arg(who));

        QAccessibleActionInterface* actions = iface->actionInterface();
        check(actions != nullptr, QStringLiteral("%1: eylem arayüzü var").arg(who));
        if (actions == nullptr) continue;

        check(actions->actionNames().contains(press),
              QStringLiteral("%1: \"%2\" eylemi sunuluyor").arg(who, press));
        if (button->isCheckable())
            check(actions->actionNames().contains(toggle),
                  QStringLiteral("%1: \"%2\" eylemi sunuluyor").arg(who, toggle));

        // THE LIGHT IS STILL SPOKEN. A screen reader saying "işaretli" is the
        // spoken form of the lit button, and the column's whole job is to say
        // which command is running — so the fix may not buy the press by
        // throwing the checked state away.
        if (button->isCheckable()) {
            park(face);
            face->setChecked(true);
            QCoreApplication::processEvents();
            check(iface->state().checkable && iface->state().checked,
                  QStringLiteral("%1: yanan düğme ağaçta da işaretli").arg(who));
        }
    }

    // ---- 2. THE PRESS RUNS THE COMMAND -------------------------------------
    //
    // THE ASSERTION IS `triggered`, NOT THE LIGHT. That distinction is the whole
    // defect: the exclusive group checks an action when Qt toggles the button, so
    // the column lit up exactly as it does for a mouse while `runCommand` was
    // never called. Measured on this machine with real CGEvent clicks the log
    // read `olay bas QToolButton/ÇİZGİ` then `tetiklendi ÇİZGİ`; with an
    // accessibility press it read `isaretlendi METİN` and stopped there.
    for (QToolButton* button : toolBox_->buttons()) {
        QAction* face = button->defaultAction();
        if (face == nullptr) continue;
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(button);
        if (iface == nullptr || iface->actionInterface() == nullptr) continue;
        const QString who = name_of(face);

        for (const QString& what : iface->actionInterface()->actionNames()) {
            park(face);

            int fired = 0;
            const QMetaObject::Connection on =
                connect(face, &QAction::triggered, this, [&fired] { ++fired; });
            iface->actionInterface()->doAction(what);
            QCoreApplication::processEvents();
            disconnect(on);

            check(fired > 0, QStringLiteral("%1: \"%2\" komutu çalıştırdı").arg(who, what));
        }
    }

    // ---- 3. AND THE KEYBOARD, WITH REAL KEY EVENTS -------------------------
    //
    // The buttons are `Qt::NoFocus` so that clicking a tool does not take the
    // keyboard off the canvas; the column itself is the tab stop and the arrow
    // keys move inside it. A user with no mouse reaches every tool here.
    park(nullptr);
    toolBox_->setFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    check(toolBox_->hasFocus(), QStringLiteral("araç kolonu Tab ile odak alıyor"));

    const auto typeInto = [this](int key) {
        QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier);
        QCoreApplication::sendEvent(toolBox_, &down);
        QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
        QCoreApplication::sendEvent(toolBox_, &up);
        QCoreApplication::processEvents();
    };

    // HOME AND END LAND ON THE ENDS, and Space runs what they landed on. Asked
    // of the ACTION rather than of a highlight, because a ring drawn in the wrong
    // place looks exactly like one drawn in the right place — and an off-by-one
    // here is a key that quietly skips a tool. The first draft of this column had
    // one: End stepped back from the last index and stopped one short of it.
    const auto pressWith = [&](std::initializer_list<int> keys, QToolButton* expected,
                               const QString& what) {
        QAction* face = expected->defaultAction();
        park(face);
        toolBox_->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        int fired = 0;
        const QMetaObject::Connection on =
            connect(face, &QAction::triggered, this, [&fired] { ++fired; });
        for (const int key : keys)
            typeInto(key);
        typeInto(Qt::Key_Space);
        disconnect(on);

        check(fired > 0, QStringLiteral("%1: %2 çalıştı").arg(what, name_of(face)));
    };

    QToolButton* firstTool = nullptr;
    QToolButton* lastTool  = nullptr;
    for (QToolButton* button : toolBox_->buttons()) {
        if (!button->isEnabled() || button->defaultAction() == nullptr) continue;
        if (firstTool == nullptr) firstTool = button;
        lastTool = button;
    }

    if (firstTool == nullptr || lastTool == nullptr) {
        check(false, QStringLiteral("kolonda etkin araç var"));
    } else {
        pressWith({Qt::Key_Home}, firstTool, QStringLiteral("Home ilk araca gidiyor"));
        pressWith({Qt::Key_End}, lastTool, QStringLiteral("End son araca gidiyor"));
        pressWith({Qt::Key_Home, Qt::Key_Down, Qt::Key_Up}, firstTool,
                  QStringLiteral("↓ sonra ↑ başlanan yere dönüyor"));
    }

    // AND THE CARD, which holds ten of the eleven family members. It answers
    // arrow keys and Enter already; what was missing was any way to OPEN it
    // without a mouse.
    {
        park(nullptr);
        toolBox_->setFocus(Qt::TabFocusReason);
        typeInto(Qt::Key_Home);

        bool opened = false;
        for (int step = 0; step < toolBox_->buttons().size() && !opened; ++step) {
            typeInto(Qt::Key_Right);
            QWidget* popup = QApplication::activePopupWidget();
            opened         = popup != nullptr && popup->property("kentos.rows").isValid();
            if (!opened) typeInto(Qt::Key_Down);
        }
        check(opened, QStringLiteral("sağ ok tuşu aile kartını açtı"));
        if (QWidget* open = QApplication::activePopupWidget(); open != nullptr) open->close();
        QCoreApplication::processEvents();
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

    /// One of the three gestures that open a family card, on a real button.
    /// Returns the card, which is ours rather than whatever popup happened to be
    /// up: a completer list is a popup too.
    enum class How { Hold, RightClick, Mark };
    const auto ours = [] {
        QWidget* popup = QApplication::activePopupWidget();
        return popup != nullptr && popup->property("kentos.rows").isValid() ? popup : nullptr;
    };
    const auto open = [&ours](QToolButton* button, How how) {
        const QPointF centre(button->width() / 2.0, button->height() / 2.0);
        // The mark is the wedge in the bottom-right corner, so the press has to
        // land inside it rather than near it.
        const QPointF corner(button->width() - 2.0, button->height() - 2.0);
        const QPointF at            = how == How::Mark ? corner : centre;
        const Qt::MouseButton which = how == How::RightClick ? Qt::RightButton : Qt::LeftButton;

        QMouseEvent down(QEvent::MouseButtonPress, at, button->mapToGlobal(at), which, which,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(button, &down);
        if (how == How::Hold) {
            // The hold timer IS the gesture, so it is waited out rather than
            // faked: a change to its interval shows up here instead of in the
            // field. Bounded, because a card that never opens must not hang.
            QElapsedTimer waited;
            waited.start();
            while (waited.elapsed() < 600 && ours() == nullptr)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        QCoreApplication::processEvents();

        // A HAND REACHING FOR THE CARD MOVES OFF THE BUTTON, and that is what
        // tells the button this was a hold rather than a slow click: released in
        // place, a hold runs the face tool instead (see `FamilyButton`). The move
        // has to happen here, or this probe would be testing a gesture it is not
        // making.
        if (how == How::Hold && ours() != nullptr) {
            const QPointF off(button->width() + 20.0, button->height() / 2.0);
            QMouseEvent away(QEvent::MouseMove, off, button->mapToGlobal(off), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(button, &away);
            QCoreApplication::processEvents();
        }

        QMouseEvent up(QEvent::MouseButtonRelease, at, button->mapToGlobal(at), which, Qt::NoButton,
                       Qt::NoModifier);
        QCoreApplication::sendEvent(button, &up);
        QCoreApplication::processEvents();
        return ours();
    };

    /// Chooses the `index`-th row on an open card, the way a hand does.
    const auto choose = [](QWidget* card, int index) {
        const QPoint first = card->property("kentos.rowCentre").toPoint();
        const int pitch    = card->property("kentos.rowPitch").toInt();
        const QPointF on(first.x(), first.y() + (index * pitch));
        for (const QEvent::Type t :
             {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent ev(t, on, card->mapToGlobal(on),
                           t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                           t == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                           Qt::NoModifier);
            QCoreApplication::sendEvent(card, &ev);
        }
        QCoreApplication::processEvents();
    };

    int families = 0;
    int members  = 0;
    for (QToolButton* button : toolBox_->buttons()) {
        if (!button->property("kentos.family").toBool()) continue;
        ++families;

        QAction* face       = button->defaultAction();
        const QString label = face == nullptr ? QStringLiteral("?") : face->text();

        QWidget* card = open(button, How::Hold);
        check(card != nullptr, QStringLiteral("%1: basılı tutmak kartı açıyor").arg(label));
        if (card == nullptr) continue;

        const int rows = card->property("kentos.rows").toInt();
        check(rows > 1, QStringLiteral("%1: kartta %2 üye var").arg(label).arg(rows));
        if (shooting)
            (void)card->grab().save(into + QStringLiteral("/kart-") + label +
                                    QStringLiteral(".png"));
        card->close();
        QCoreApplication::processEvents();

        // THE OTHER TWO GESTURES OPEN IT TOO. A card that only answers a hold is
        // a card most users never see: the wedge is drawn in the corner, so it
        // has to be a target, and a right click is what a CAD hand tries first.
        for (const auto& [how, named] :
             {std::pair{How::RightClick, "sağ tık"}, std::pair{How::Mark, "köşe işareti"}}) {
            QWidget* again = open(button, how);
            check(again != nullptr,
                  QStringLiteral("%1: %2 kartı açıyor").arg(label, QString::fromUtf8(named)));
            if (again != nullptr) {
                again->close();
                QCoreApplication::processEvents();
            }
        }

        // AND EVERY MEMBER RUNS. Chosen from the card rather than triggered,
        // because choosing is what a hand does and the card's own signal is what
        // carries it — eleven tools live only here and none had been pressed.
        for (int i = 0; i < rows; ++i) {
            controller_->cancelInteractive();
            QCoreApplication::processEvents();

            QWidget* live = open(button, How::Hold);
            if (live == nullptr) {
                check(false,
                      QStringLiteral("%1: %2. üye için kart açılmadı").arg(label).arg(i + 1));
                break;
            }
            const qsizetype transcript_before = transcript_->toPlainText().size();
            choose(live, i);
            ++members;

            // The face follows the choice, and the chosen tool is what started.
            QAction* armed = button->defaultAction();
            const QString word =
                armed == nullptr ? QString() : armed->property(kToolCommand).toString();
            // WHAT "IT RAN" MEANS. A modal draw tool suspends on its first
            // question and the session is waiting; one that works on the
            // selection finishes on the spot and lights its button; and one
            // that has nothing to work on — BLOKEKLE in a drawing with no
            // block — declines out loud, which puts a line in the transcript.
            // Any of the three is the tool having run; none of them happening
            // is the card doing nothing.
            const command::Session* running = controller_->session();
            const bool answered             = transcript_->toPlainText().size() > transcript_before;
            const bool started =
                (running != nullptr && (running->waiting() || running->working())) ||
                (armed != nullptr && armed->isChecked()) || answered;
            check(!word.isEmpty() && started,
                  QStringLiteral("%1 > %2. üye (%3) çalıştı").arg(label).arg(i + 1).arg(word));
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

    int failures  = 0;
    int index     = 0;
    QMenuBar* bar = titleBar_->menus();

    /// Every entry on a menu, one line each, submenus indented under their own.
    const auto walk = [](QMenu* menu, const QString& lead, auto&& self) -> int {
        int leaves = 0;
        for (QAction* action : menu->actions()) {
            if (action->isSeparator()) continue;
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

    for (QAction* title : bar->actions()) {
        QMenu* menu = title->menu();
        if (menu == nullptr) continue;
        ++index;

        // OPENED, not read: a menu that cannot be shown is a menu nobody can use,
        // and its own size is only decided once it lays itself out.
        menu->popup(mapToGlobal(QPoint(10, 40)));
        QCoreApplication::processEvents();

        const QString clean = QString(title->text()).remove(QLatin1Char('&'));
        (void)std::fprintf(stdout, "[menü] ==== %s (%d px) ====\n", clean.toUtf8().constData(),
                           menu->height());
        const int leaves = walk(menu, QString(), walk);

        // A MENU TALLER THAN A SCREEN is the defect this probe exists for. 900 px
        // is a short laptop; a menu past it has rows a user cannot reach.
        if (menu->height() > 900) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: %s menüsü %d px — ekranı aşıyor\n",
                               clean.toUtf8().constData(), menu->height());
            ++failures;
        }
        if (leaves == 0) {
            (void)std::fprintf(stderr, "[menü] BAŞARISIZ: %s menüsü boş\n",
                               clean.toUtf8().constData());
            ++failures;
        }

        if (shooting) {
            const QImage picture = menu->grab().toImage();
            (void)picture.save(QStringLiteral("%1/menu-%2-%3.png")
                                   .arg(into)
                                   .arg(index, 2, 10, QLatin1Char('0'))
                                   .arg(clean));
        }
        menu->close();
        QCoreApplication::processEvents();
    }

    // AND THE SNAP-MODE POPUP, which is not on the menu bar and so is not in the
    // loop above. It is built from `core::SnapAllMask`, so every mode the engine
    // declares has a row — the release list asks for ÇEYREK and TEĞET by name,
    // and their bits were unreachable through the setting until the range was
    // widened. A list generated from the mask cannot drift; a list nobody ever
    // opened can still be empty.
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
    if (attributePanel_ != nullptr) shoot(attributePanel_, "nesne-paneli");
    if (layerPanel_ != nullptr) shoot(layerPanel_, "katman-paneli");

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
    refreshLayerCombo();
    layerPanel_->refresh();
    attributePanel_->refresh();
    refreshStatus();

    // Mirroring the journal keeps the architecture visible while using the program.
    journalView_->clear();
    for (const auto& e : controller_->journal().entries())
        journalView_->appendPlainText(QString::fromStdString(e.to_json(false).dump()));

    canvas_->update();
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
        if (typed) {
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
    const command::CommandSpec* spec = controller_->registry().resolve(id.toStdString());
    const bool wants_objects         = spec != nullptr && !spec->params.empty() &&
                               spec->params.front().kind == command::ParamKind::Selection;
    if (wants_objects && !controller_->bus().selection().empty())
        controller_->runLine(QStringLiteral("SEÇ TEMİZLE"), command::Origin::Gui);

    // Queued, not called: this runs inside the finishing command's own signal,
    // and starting the next session on top of the one being torn down is how a
    // coroutine gets resumed after its frame is gone.
    QMetaObject::invokeMethod(this, [action] { action->trigger(); }, Qt::QueuedConnection);
}

void MainWindow::onUndoStateChanged(bool canUndo, bool canRedo)
{
    actUndo_->setEnabled(canUndo);
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
    readout_->setCrs(crsText);

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

    readout_->setScale(ground_mm_per_paper_mm > 0.0
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
    setWindowTitle(tr("%1 — KentOSCad").arg(name));
    docTabs_->setDocuments({QFileInfo(name).completeBaseName()}, 0);
    titleBar_->setDocumentName(tr("%1 — KentOSCad %2").arg(name, QStringLiteral(KENTOS_VERSION)));
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
    if (!confirmDiscard(tr("Yeni çizime geçmeden önce kaydedilsin mi?"))) return;

    // AND THEN THE COMMAND, exactly as typed. The window's whole contribution is
    // the question above it: the drawing, the undo stack, the file the document
    // belonged to and the view are reset by `YENİ` itself, so a script gets the
    // same reset without a dialog it could not answer (Article 1.2).
    controller_->runLine(QStringLiteral("YENİ"), command::Origin::Gui);
    refreshWindowTitle();
}

void MainWindow::openProject()
{
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
    layoutMenu_->clear();

    QAction* fresh = layoutMenu_->addAction(tr("Yeni Çıktı Yerleşimi…"));
    fresh->setStatusTip(
        tr("ÇIKTIYERLEŞİMİ islem=ekle — başlık, harita, ölçek çubuğu ve kuzey oku ile "
           "gelir"));
    connect(fresh, &QAction::triggered, this, [this] { newLayout(); });

    QAction* manage = layoutMenu_->addAction(tr("Çıktı Yerleşimi Yöneticisi…"));
    manage->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    manage->setStatusTip(
        tr("Çizimdeki çıktı yerleşimlerini listeler: aç, yeniden adlandır, çoğalt, sil"));
    connect(manage, &QAction::triggered, this, &MainWindow::openLayoutManager);

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

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("KentOSCad Hakkında"),
                       tr("<h3>KentOSCad %1</h3>"
                          "<p>Türkiye odaklı CBS + CAD harita yazılımı.</p>"
                          "<p><b>Mimari:</b> Uygulamanın durumunu değiştiren her şey bir komuttur. "
                          "Arayüz, komut veri yolunun yalnızca bir istemcisidir.</p>"
                          "<p><b>Lisans:</b> GPLv3 veya sonrası<br>"
                          "<b>Render:</b> %2<br>"
                          "<b>Komut sayısı:</b> %3</p>")
                           .arg(QStringLiteral(KENTOS_VERSION), canvas_->backendName())
                           .arg(controller_->registry().size()));
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

void MainWindow::probeToolBox()
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

    // Every button on the column, found rather than listed: a tool added and
    // forgotten here would be a tool nobody presses.
    const QList<QToolButton*> buttons = toolBox_->findChildren<QToolButton*>();

    int ok_ran   = 0;
    int ok_armed = 0;
    int dead     = 0;

    for (int pass = 0; pass < 2; ++pass) {
        const bool withSelection = pass == 0;

        (void)std::fprintf(stdout, "[araç] ==== %s ====\n",
                           withSelection ? "önce seç, sonra bas"
                                         : "önce bas, sonra seç (BOŞ SEÇİM)");
        for (QToolButton* button : buttons) {
            QAction* action = button->defaultAction();
            if (action == nullptr) continue;

            const QString name = action->text();
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

            const auto mark = static_cast<int>(transcript_->toPlainText().size());
            prompt.clear();
            armed = false;
            asked = false;

            action->trigger();
            QCoreApplication::processEvents();

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
                    } else if (prompt.contains(QStringLiteral("Yazılacak")))
                        runScriptLine(QStringLiteral("deneme"));
                    else if (prompt.contains(QStringLiteral("mesafe")))
                        runScriptLine(QStringLiteral("5"));
                    else
                        controller_->supplyPoint(clicks[step]);
                    QCoreApplication::processEvents();
                }
                controller_->cancelInteractive();
                QCoreApplication::processEvents();
            }

            QString said = transcript_->toPlainText().mid(mark).trimmed();
            said.replace(QLatin1Char('\n'), QLatin1Char(' '));

            QString verdict;
            const bool refused = said.startsWith(QStringLiteral("Hata:")) ||
                                 said.startsWith(QStringLiteral("Bilinmeyen komut"));
            if (refused) {
                ++dead;
                verdict = QStringLiteral("KIRIK   ") + said;
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
            } else if (said.isEmpty()) {
                ++dead;
                verdict = QStringLiteral("SESSİZ  düğme ne sordu ne de bir şey söyledi");
            } else {
                ++ok_ran;
                verdict = QStringLiteral("ÇALIŞTI ") + said;
            }

            (void)std::fprintf(stdout, "[araç] %-18s %-12s seçili=%d  %s\n", qPrintable(name),
                               qPrintable(cmd), held, qPrintable(verdict.left(140)));
        }
    }
    disconnect(onPrompt);

    (void)std::fprintf(stdout,
                       "[araç] ---- %d araç x2 geçiş: %d çalıştı, %d girdi sordu, %d kırık\n",
                       static_cast<int>(buttons.size()), ok_ran, ok_armed, dead);

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
        // The two crossing lines; then the piece to discard.
        {"BUDA", {60'000, 35'000}, {70'000, 20'000}, {60'000, 38'000}, {}, nullptr},
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
