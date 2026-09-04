// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/main_window.hpp"

#include "kentos_cad/app/attribute_panel.hpp"
#include "kentos_cad/app/attribute_table.hpp"
#include "kentos_cad/app/command_line.hpp"
#include "kentos_cad/app/command_palette.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/data_root.hpp"
#include "kentos_cad/app/database_dialog.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/map_canvas.hpp"
#include "kentos_cad/app/panels.hpp"
#include "kentos_cad/app/settings_dialog.hpp"
#include "kentos_cad/app/shell_chrome.hpp"
#include "kentos_cad/app/style_designer.hpp"
#include "kentos_cad/app/title_bar.hpp"
#include "kentos_cad/app/toolbox.hpp"
#include "kentos_cad/core/snap.hpp"

#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/render/backend.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/settings.hpp"

#include <QAction>
#include <QToolButton>

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <cstdio>

namespace kentos::app {
namespace {

/// Property name under which a tool button carries the command it sends.
/// `QAction::data()` is taken by the glyph, and a tool that did not say which
/// command it runs would have to be recognised from a hand-written table — the
/// second command list CLAUDE.md 5.10 forbids.
constexpr const char* kToolCommand = "piricad.command";

/// The same swatch the layer panel draws, so the combo and the panel agree.
/// `1 000 000` — thin-space thousands, the way a Turkish pafta prints a scale.
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
constexpr int kLayoutVersion = 4;

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
    connect(titleBar_, &TitleBar::searchRequested, this, &MainWindow::openCommandSearch);

    auto* search = new QAction(this);
    search->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    search->setShortcutContext(Qt::ApplicationShortcut);
    connect(search, &QAction::triggered, this, &MainWindow::openCommandSearch);
    addAction(search);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks |
                   QMainWindow::AllowNestedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    canvas_ = new MapCanvas(*controller_, this);

    commandLine_ = new CommandLine(*controller_, this);
    commandLine_->setObjectName(QStringLiteral("commandLine"));

    // design.md 7: the tool box, the canvas column and the right dock sit side by
    // side inside the body, and the status bar runs under all three. The tool box
    // is part of the body rather than a dock, so the command line starts at its
    // right edge exactly as the reference draws it.
    docTabs_ = new DocumentTabs(this);
    connect(docTabs_, &DocumentTabs::activated, this, [this](int) { refreshWindowTitle(); });
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
    connect(controller_, &Controller::selectionChanged, this,
            [this] { attributePanel_->refresh(); });
    connect(controller_, &Controller::selectionChanged, canvas_,
            QOverload<>::of(&MapCanvas::update));
    connect(canvas_, &MapCanvas::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(canvas_, &MapCanvas::viewChanged, this, &MainWindow::refreshStatus);
    connect(canvas_, &MapCanvas::echoRequested, this, &MainWindow::onEcho);
    connect(commandLine_, &CommandLine::submitted, this, &MainWindow::onCommandSubmitted);
    connect(layerPanel_, &LayerPanel::layerSelected, attributePanel_, &AttributePanel::setLayer);
    connect(layerPanel_, &LayerPanel::styleRequested, this, &MainWindow::openStyleDesigner);

    onEcho(tr("KentOSCad %1 — komut merkezli mimari, GPLv3.").arg(QStringLiteral(KENTOS_VERSION)));
    onEcho(tr("Aynı komut arayüzden, komut satırından ve betikten tıpatıp aynı yolu izler."));
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

QAction* MainWindow::commandAction(Glyph glyph, const QString& text, const QString& line,
                                   const QString& tip, const QKeySequence& shortcut)
{
    auto* action = new QAction(text, this);
    action->setToolTip(tip);
    action->setStatusTip(tip);
    action->setData(static_cast<int>(glyph));
    if (!shortcut.isEmpty()) action->setShortcut(shortcut);

    connect(action, &QAction::triggered, this,
            [this, line] { controller_->runLine(line, command::Origin::Gui); });
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
    drawingTools_->addAction(action);

    // NO SELECTION GUARD. It used to refuse an empty selection with a sentence in
    // the status line and never start the command — so the ordinary order of work,
    // reach for the tool and then point at the thing, did nothing at all. The
    // commands ask for their objects now (`want_objects`), which is the order
    // every CAD trains and the one a script never sees.
    connect(action, &QAction::triggered, this,
            [this, command] { controller_->runCommand(command); });
    return action;
}

QAction* MainWindow::placeholder(Glyph glyph, const QString& text, const QString& command,
                                 const QString& phase)
{
    auto* action = new QAction(text, this);
    action->setEnabled(false);
    action->setData(static_cast<int>(glyph));

    const QString tip = command.isEmpty() ? tr("%1 — %2'de gelecek").arg(text, phase)
                                          : tr("%1 — %2'de gelecek").arg(command, phase);
    action->setToolTip(tip);
    action->setStatusTip(tip);
    return action;
}

void MainWindow::buildActions()
{
    // ---- dosya ----
    actNew_ = placeholder(Glyph::New, tr("Yeni"), QStringLiteral("YENİ"), tr("Faz 1"));

    actOpen_ = new QAction(tr("Aç…"), this);
    actOpen_->setShortcut(QKeySequence::Open);
    actOpen_->setToolTip(tr("AÇ — bir KentOSCad proje dosyası açar"));
    actOpen_->setData(static_cast<int>(Glyph::Open));
    connect(actOpen_, &QAction::triggered, this, &MainWindow::openProject);

    actSave_ = new QAction(tr("Kaydet"), this);
    actSave_->setShortcut(QKeySequence::Save);
    actSave_->setToolTip(tr("KAYDET — çizimi bağlı olduğu dosyaya yazar"));
    actSave_->setData(static_cast<int>(Glyph::Save));
    connect(actSave_, &QAction::triggered, this, &MainWindow::saveProject);

    actSaveAs_ = new QAction(tr("Farklı Kaydet…"), this);
    actSaveAs_->setShortcut(QKeySequence::SaveAs);
    actSaveAs_->setToolTip(tr("FARKLIKAYDET — çizimi yeni bir dosyaya yazar"));
    actSaveAs_->setData(static_cast<int>(Glyph::Save));
    connect(actSaveAs_, &QAction::triggered, this, &MainWindow::saveProjectAs);

    actImport_ = new QAction(tr("İçe Aktar…"), this);
    actImport_->setToolTip(tr("İÇEAKTAR — dış bir veri dosyasını çizime ekler"));
    actImport_->setData(static_cast<int>(Glyph::Open));
    connect(actImport_, &QAction::triggered, this, &MainWindow::importData);

    actExport_ = new QAction(tr("Dışa Aktar…"), this);
    actExport_->setToolTip(tr("DIŞAAKTAR — çizimi dış bir veri biçimine yazar"));
    actExport_->setData(static_cast<int>(Glyph::Export));
    connect(actExport_, &QAction::triggered, this, &MainWindow::exportData);

    actPrint_ = placeholder(Glyph::Print, tr("Yazdır"), QStringLiteral("YAZDIR"), tr("Faz 2"));

    actScript_ = new QAction(tr("Betik Çalıştır…"), this);
    actScript_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actScript_->setToolTip(tr("BETİK — bir JSON betiğini komut veri yolundan çalıştırır"));
    actScript_->setData(static_cast<int>(Glyph::Script));
    connect(actScript_, &QAction::triggered, this, &MainWindow::openScript);

    actDatabase_ = new QAction(tr("Veritabanı…"), this);
    actDatabase_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    actDatabase_->setToolTip(tr("VERİTABANI — PostGIS sunucusuna bağlanır, katmanları tablo, "
                                "projeleri kayıt olarak yazar"));
    actDatabase_->setData(static_cast<int>(Glyph::Open));
    connect(actDatabase_, &QAction::triggered, this, &MainWindow::openDatabase);

    actSettings_ = new QAction(tr("Ayarlar…"), this);
    actSettings_->setData(static_cast<int>(Glyph::Settings));
    actSettings_->setShortcut(QKeySequence::Preferences);
    actSettings_->setToolTip(tr("Bildirilen her ayarı kapsamına göre gösterir; her "
                                "değişiklik AYAR, TERCİH ya da MOD komutu olarak geçer"));
    connect(actSettings_, &QAction::triggered, this, &MainWindow::openSettings);

    actQuit_ = new QAction(tr("Çıkış"), this);
    actQuit_->setShortcut(QKeySequence::Quit);
    connect(actQuit_, &QAction::triggered, qApp, &QApplication::quit);

    // ---- seçim ve çizim ----
    actSelect_ = new QAction(tr("Seç"), this);
    actSelect_->setCheckable(true);
    actSelect_->setChecked(true);
    actSelect_->setToolTip(tr("Seçim aracı — çalışan komutu iptal eder (Esc)"));
    actSelect_->setData(static_cast<int>(Glyph::Select));
    connect(actSelect_, &QAction::triggered, this, [this] { controller_->cancelInteractive(); });

    actLine_ = new QAction(tr("Çizgi"), this);
    actLine_->setCheckable(true);
    actLine_->setToolTip(tr("ÇİZGİ — ardışık doğru parçaları çizer  ·  kısaltma: Ç, L"));
    actLine_->setData(static_cast<int>(Glyph::Line));
    actLine_->setProperty(kToolCommand, QStringLiteral("ÇİZGİ"));
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

        // Named so a test can reach the button a user would press. Nothing in the
        // shell looks an action up by name; this exists for `KENTOS_EDIT_PROBE`,
        // which drives the tool column the way a hand does.
        action->setObjectName(QStringLiteral("toolAction.") + command);

        connect(action, &QAction::triggered, this,
                [this, command] { controller_->runCommand(command); });
        return action;
    };

    actPolygon_ = drawTool(Glyph::Polygon, tr("Poligon"), QStringLiteral("ALAN"),
                           tr("ALAN — kapalı bir alan çizer  ·  kısaltma: POLİGON, AL"));
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

    actPolyline_ = drawTool(Glyph::Polyline, tr("Çoklu Çizgi"), QStringLiteral("ÇOKLUÇİZGİ"),
                            tr("ÇOKLUÇİZGİ — çok köşeli TEK çizgi nesnesi  ·  kısaltma: ÇÇ"));
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
    actSector_ = drawTool(Glyph::Arc, tr("Daire Dilimi"), QStringLiteral("DİLİM"),
                          tr("DİLİM — merkez ve iki kenardan daire dilimi  ·  kısaltma: DL"));
    drawingTools_->addAction(actSector_);
    actEllipse_ = drawTool(Glyph::Circle, tr("Elips"), QStringLiteral("ELİPS"),
                           tr("ELİPS — merkez ve iki eksenden elips; ikinci eksen birincisine "
                              "diktir  ·  kısaltma: EL"));
    drawingTools_->addAction(actEllipse_);
    actAnnulus_ = drawTool(Glyph::Circle, tr("Halka"), QStringLiteral("HALKA"),
                           tr("HALKA — merkez, iç ve dış yarıçaptan delikli halka  ·  "
                              "kısaltma: HLK"));
    drawingTools_->addAction(actAnnulus_);

    actPoint_ = drawTool(Glyph::Point, tr("Nokta"), QStringLiteral("NOKTA"),
                         tr("NOKTA — ölçülmüş nokta: nirengi, poligon noktası, röper  ·  "
                            "kısaltma: NK"));
    drawingTools_->addAction(actPoint_);
    actText_ = drawTool(Glyph::Text, tr("Metin"), QStringLiteral("METİN"),
                        tr("METİN — çizime yazı yazar  ·  kısaltma: MT"));
    drawingTools_->addAction(actText_);

    // ---- düzenleme ----
    actErase_ = new QAction(tr("Sil"), this);
    actErase_->setToolTip(tr("SİL — seçilen nesneleri siler  ·  Del"));
    actErase_->setData(static_cast<int>(Glyph::Erase));

    // Del, the key every drawing program deletes with. A WINDOW shortcut rather
    // than one the canvas handles, so it works with the focus in the layer list
    // or the attribute table too — the selection is the same selection whichever
    // panel the user is looking at.
    actErase_->setShortcut(QKeySequence::Delete);
    actErase_->setShortcutContext(Qt::WindowShortcut);
    addAction(actErase_);

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

    // The clipboard group. `KES`/`YAPIŞTIR` are Phase 2 commands; the buttons
    // exist now so the bar has the shape design.md 7 draws, and each is disabled
    // with the phase named in its tooltip rather than silently absent.
    actCut_      = placeholder(Glyph::Cut, tr("Kes"), QStringLiteral("KES"), tr("Faz 2"));
    actCopyClip_ = placeholder(Glyph::Duplicate, tr("Panoya Kopyala"),
                               QStringLiteral("PANOKOPYALA"), tr("Faz 2"));
    actPaste_ = placeholder(Glyph::Paste, tr("Yapıştır"), QStringLiteral("YAPIŞTIR"), tr("Faz 2"));

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
    actMeasureArea_ = commandAction(Glyph::MeasureArea, tr("Alan Ölç"), QStringLiteral("ALANÖLÇ"),
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
    connect(actUndo_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("GERİAL")); });

    actRedo_ = new QAction(tr("Yinele"), this);
    actRedo_->setShortcut(QKeySequence::Redo);
    actRedo_->setToolTip(tr("YİNELE — geri alınan işlemi yineler"));
    actRedo_->setData(static_cast<int>(Glyph::Redo));
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
    actSelectAll_  = commandAction(Glyph::Select, tr("Tümünü Seç"), QStringLiteral("SEÇ TÜMÜ"),
                                   tr("SEÇ TÜMÜ — görünür bütün nesneleri seçer"),
                                   QKeySequence(Qt::CTRL | Qt::Key_A));
    actSelectNone_ = commandAction(
        Glyph::Select, tr("Seçimi Temizle"), QStringLiteral("SEÇ TEMİZLE"),
        tr("SEÇ TEMİZLE — seçimi boşaltır"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));

    // ---- `katman` ve CBS ----
    actLayer_ = new QAction(tr("Katman"), this);
    actLayer_->setToolTip(tr("KATMAN — katman oluşturur ve aktif yapar"));
    actLayer_->setData(static_cast<int>(Glyph::Layer));
    connect(actLayer_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KATMAN")); });

    actLayerManager_ = placeholder(Glyph::LayerManager, tr("Katman Yöneticisi"),
                                   QStringLiteral("KATMANYÖNETİCİSİ"), tr("Faz 1"));
    // A MODAL TOOL like every other two-click tool. It was a plain action, so it
    // ran but never lit: the user clicked twice on a canvas that gave no sign a
    // measurement was in progress, and the answer went to a hidden tab.
    actMeasure_ = new QAction(tr("Ölç"), this);
    actMeasure_->setCheckable(true);
    actMeasure_->setToolTip(tr("ÖLÇ — iki nokta arası mesafe, koordinat farkı ve açı"));
    actMeasure_->setData(static_cast<int>(Glyph::Measure));
    actMeasure_->setProperty(kToolCommand, QStringLiteral("ÖLÇ"));
    actMeasure_->setObjectName(QStringLiteral("toolAction.ÖLÇ"));
    connect(actMeasure_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("ÖLÇ")); });
    drawingTools_->addAction(actMeasure_);

    actCoordinate_ = new QAction(tr("Koordinat Oku"), this);
    actCoordinate_->setCheckable(true);
    actCoordinate_->setToolTip(tr("KOORDİNAT — tıklanan noktanın sağa/yukarı değerini yazar"));
    actCoordinate_->setData(static_cast<int>(Glyph::Coordinate));
    actCoordinate_->setProperty(kToolCommand, QStringLiteral("KOORDİNAT"));
    actCoordinate_->setObjectName(QStringLiteral("toolAction.KOORDİNAT"));
    connect(actCoordinate_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KOORDİNAT")); });
    drawingTools_->addAction(actCoordinate_);
    actIdentify_ =
        placeholder(Glyph::Identify, tr("Sorgula"), QStringLiteral("SORGULA"), tr("Faz 2"));
    actTable_ = new QAction(tr("Öznitelik Tablosu"), this);
    actTable_->setData(static_cast<int>(Glyph::Table));
    actTable_->setToolTip(tr("Katmanın satırlarını ve sütunlarını aç"));
    actTable_->setShortcut(QKeySequence(Qt::Key_F6));
    connect(actTable_, &QAction::triggered, this, &MainWindow::openAttributeTable);

    actAi_ = placeholder(Glyph::Ai, tr("AI Asistan"), QString(), tr("Faz 3"));
    actAi_->setToolTip(tr("AI komut önerisi — önizleme ve onay ile (Faz 3)"));

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

    // file
    tbMain_->addAction(actNew_);
    tbMain_->addAction(actOpen_);
    tbMain_->addAction(actSave_);
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

    // output
    tbMain_->addAction(actPrint_);
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
    file->addSeparator();
    file->addAction(actDatabase_);
    file->addAction(actScript_);
    file->addSeparator();
    file->addAction(actQuit_);

    auto* edit = bar->addMenu(tr("D&üzen"));
    edit->addAction(actUndo_);
    edit->addAction(actRedo_);
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
    view->addAction(actGridSnap_);
    view->addSeparator();

    if (tbMain_) view->addAction(tbMain_->toggleViewAction());

    auto* panels = view->addMenu(tr("Paneller"));
    for (QDockWidget* dock : {layerDock_, propertyDock_, transcriptDock_, journalDock_}) {
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
    draw->addAction(actPoint_);
    draw->addAction(actText_);

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
    map->addAction(actMeasure_);
    map->addSeparator();
    map->addAction(actDatabase_);

    auto* analyse = bar->addMenu(tr("&Analiz"));
    analyse->addAction(actTable_);
    analyse->addSeparator();
    analyse->addAction(actAi_);

    auto* layer = bar->addMenu(tr("&Katman"));
    layer->addAction(actLayer_);
    layer->addAction(actLayerManager_);

    auto* window = bar->addMenu(tr("&Pencere"));
    for (QDockWidget* dock : {layerDock_, propertyDock_, transcriptDock_, journalDock_}) {
        if (dock) window->addAction(dock->toggleViewAction());
    }
    window->addSeparator();
    auto* reset = window->addAction(tr("Yerleşimi Sıfırla"));
    connect(reset, &QAction::triggered, this, &MainWindow::resetLayout);

    auto* about = bar->addMenu(tr("&Yardım"));
    auto* ref   = about->addAction(tr("Komut Listesi"));
    connect(ref, &QAction::triggered, this, &MainWindow::showCommandReference);
    about->addSeparator();
    auto* info = about->addAction(tr("Hakkında"));
    connect(info, &QAction::triggered, this, &MainWindow::showAbout);
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
    toolBox_->addTool(actLine_);
    toolBox_->addTool(actPolyline_);
    toolBox_->addTool(actPolygon_);
    toolBox_->addTool(actRectangle_);
    toolBox_->addTool(actCircle_);
    // YAY was built as a full draw tool and then left out of the column, so the
    // one curve this program can draw was reachable only by typing its name.
    toolBox_->addTool(actArc_);
    toolBox_->addTool(actPoint_);
    toolBox_->addTool(actText_);
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
    toolBox_->addTool(actOffset_);
    toolBox_->addSeparator();

    // measurement
    toolBox_->addTool(actMeasure_);
    toolBox_->addTool(actMeasureArea_);
    toolBox_->addTool(actCoordinate_);
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

    propertyStack_ = new QStackedWidget(this);
    propertyStack_->addWidget(attributePanel_);
    propertyStack_->addWidget(transcript_);

    propertyHeader_ = new PanelHeader(this);
    propertyHeader_->addTab(tr("Öznitelikler"), static_cast<int>(Glyph::Table));
    propertyHeader_->addTab(tr("Geçmiş"), static_cast<int>(Glyph::History));
    connect(propertyHeader_, &PanelHeader::tabChanged, propertyStack_,
            &QStackedWidget::setCurrentIndex);

    propertyDock_ = makeDock(QStringLiteral("propertyDock"), propertyHeader_, propertyStack_);
    propertyDock_->toggleViewAction()->setText(tr("Öznitelikler"));

    // ---- the layers panel ----
    layerPanel_ = new LayerPanel(*controller_, this);

    layerHeader_ = new PanelHeader(this);
    layerHeader_->addTab(tr("Katmanlar"), static_cast<int>(Glyph::Layer));
    layerHeader_->setButtons(PanelHeader::Grip | PanelHeader::Collapse | PanelHeader::Float);

    layerDock_ = makeDock(QStringLiteral("layerDock"), layerHeader_, layerPanel_);
    layerDock_->toggleViewAction()->setText(tr("Katmanlar"));

    // ---- the command journal, hidden until asked for ----
    journalHeader_ = new PanelHeader(this);
    journalHeader_->addTab(tr("Komut Günlüğü"), static_cast<int>(Glyph::Script));
    journalHeader_->setButtons(PanelHeader::Grip | PanelHeader::Float | PanelHeader::Close);

    journalDock_ = makeDock(QStringLiteral("journalDock"), journalHeader_, journalView_);
    journalDock_->toggleViewAction()->setText(tr("Komut Günlüğü"));

    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    addDockWidget(Qt::RightDockWidgetArea, layerDock_);
    addDockWidget(Qt::BottomDockWidgetArea, journalDock_);

    // The reference has no bottom panel open: the command line carries the
    // conversation and the journal is there when a user asks for it.
    journalDock_->hide();

    // 312 px wide, and the layers panel 268 px tall — both from design.md 7.
    resizeDocks({propertyDock_, layerDock_}, {312, 312}, Qt::Horizontal);
    resizeDocks({propertyDock_, layerDock_}, {600, 268}, Qt::Vertical);

    for (QDockWidget* dock : {propertyDock_, layerDock_, journalDock_}) {
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
        settings_ = new SettingsDialog(*controller_, this);
        settings_->setAttribute(Qt::WA_DeleteOnClose);
        connect(settings_, &QObject::destroyed, this, [this] { settings_ = nullptr; });
    }
    settings_->applyTheme(theme_);
    settings_->show();
    settings_->raise();
    settings_->activateWindow();
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
        connect(database_, &QObject::destroyed, this, [this] { database_ = nullptr; });
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

    // Each chip is the command it names. The mouse gets no private road: clicking
    // IZGARA runs `IZGARA`, exactly as typing it would (Article 1.2).
    statusStrip_->addToggle(tr("IZGARA"), QStringLiteral("core.izgara.gorunur"));
    statusStrip_->addToggle(tr("YAKALAMA"), QStringLiteral("core.izgara.yakalama"));
    statusStrip_->addToggle(tr("DİK"), QStringLiteral("core.yakalama.dik"));
    statusStrip_->addToggle(tr("POLAR"), QStringLiteral("core.yakalama.polar"));
    statusStrip_->addToggle(tr("OSNAP"), QStringLiteral("core.yakalama.acik"));
    statusStrip_->addToggle(tr("DİNAMİK GİRDİ"), QStringLiteral("core.arayuz.dinamik_girdi"));
    statusStrip_->addToggle(tr("KALINLIK"), QStringLiteral("core.harita.kalinlik"));

    connect(statusStrip_, &StatusStrip::configureRequested, this, [this](const QString& id) {
        if (id == QStringLiteral("core.yakalama.acik")) {
            openSnapModes();
            return;
        }
        // Every other chip is a plain on/off, so "configure" means the page of
        // Ayarlar it lives on rather than a list of its own.
        openSettings();
    });

    connect(statusStrip_, &StatusStrip::toggled, this, [this](const QString& id) {
        core::Settings& store     = controller_->bus().app_settings();
        const std::uint32_t index = store.catalogue().find(id.toStdString());
        if (index == core::kNoSetting) {
            onEcho(tr("Bu yardımcı henüz bir ayara bağlı değil: %1").arg(id));
            return;
        }
        const bool now = store.get(id.toStdString()).as_bool();
        controller_->runLine(QStringLiteral("AYAR ad=%1 deger=%2")
                                 .arg(id, now ? QStringLiteral("hayır") : QStringLiteral("evet")),
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
    if (id.startsWith(QLatin1String("core.izgara."))) {
        canvas_->reloadGridSettings();
        canvas_->reloadSnapSettings();
        refreshAidActions();
        canvas_->update();
        return;
    }

    if (id.startsWith(QLatin1String("core.yakalama.")) ||
        id.startsWith(QLatin1String("core.secim."))) {
        canvas_->reloadSnapSettings();
        refreshAidActions();
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
    const auto mask = static_cast<std::uint16_t>(session.get("core.yakalama.modlar").as_int());

    // Writes the whole mask through MOD, which is the only road there is: the
    // command line, a script and this menu all set the same sixteen bits.
    const auto write = [this](std::uint16_t next) {
        controller_->runLine(QStringLiteral("MOD ad=yakalama_modları deger=%1").arg(next),
                             command::Origin::Gui);
    };

    for (std::uint16_t bit = 1; bit != 0; bit = static_cast<std::uint16_t>(bit << 1)) {
        if ((core::SnapAllMask & bit) == 0) continue;

        auto* row = menu.addAction(QString::fromUtf8(core::snap_mode_label(bit)));
        row->setCheckable(true);
        row->setChecked((mask & bit) != 0);

        // The machine name in the tip, because it is what a script writes and what
        // the transcript prints.
        row->setToolTip(QString::fromUtf8(core::snap_mode_id(bit)));
        connect(row, &QAction::triggered, this, [write, mask, bit](bool on) {
            write(static_cast<std::uint16_t>(on ? (mask | bit) : (mask & ~bit)));
        });
    }

    menu.addSeparator();
    connect(menu.addAction(tr("Hepsi")), &QAction::triggered, this,
            [write] { write(static_cast<std::uint16_t>(core::SnapAllMask)); });
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
    const bool objectSnap = (mask & static_cast<int>(core::SnapObjectMask)) != 0;
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

void MainWindow::openAttributeTable()
{
    // Rebuilt each time rather than kept: the window reads the document through
    // the controller and holds no copy, so there is nothing to keep alive, and a
    // stale one is one more thing that can disagree with the drawing.
    auto* table = new AttributeTable(*controller_, controller_->activeLayerName(), this);
    table->setAttribute(Qt::WA_DeleteOnClose, true);
    table->applyTheme(theme_);
    table->show();
}

void MainWindow::openCommandSearch()
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
    palette_->reveal();
}

void MainWindow::resetLayout()
{
    addDockWidget(Qt::RightDockWidgetArea, layerDock_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    tabifyDockWidget(layerDock_, propertyDock_);
    layerDock_->raise();
    syncDockTitles();
}

void MainWindow::onEcho(const QString& text)
{
    transcript_->appendPlainText(text);

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
        for (QAction* action : drawingTools_->actions()) {
            const QVariant carried = action->property(kToolCommand);
            if (!carried.isValid()) continue;

            const command::CommandSpec* spec =
                controller_->registry().resolve(carried.toString().toStdString());
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
    syncToolSelection();
    canvas_->update();
}

void MainWindow::onInteractiveFinished(const QString& id, bool mutated)
{
    // A DRAW TOOL IS MODAL. Picking `ALAN`, drawing a parsel and being dropped
    // back on the select tool means reaching for the tool column again before
    // every single parcel, and a cadastral sheet is hundreds of them.
    //
    // Re-arming only after a run that DREW something is what keeps that from
    // becoming a trap: Esc both finishes an open-ended shape and cancels an empty
    // one, so the first Esc closes the parsel and re-arms the tool, and the
    // second — with nothing drawn — puts it away. Two Escs to leave, which is
    // what a CAD user's hands already expect.
    if (!mutated) return;

    for (QAction* action : drawingTools_->actions()) {
        const QVariant carried = action->property(kToolCommand);
        if (!carried.isValid() || !action->isEnabled()) continue;

        const command::CommandSpec* spec =
            controller_->registry().resolve(carried.toString().toStdString());
        if (spec == nullptr || spec->id != id.toStdString()) continue;

        // Queued, not called: this runs inside the finishing command's own signal,
        // and starting the next session on top of the one being torn down is how a
        // coroutine gets resumed after its frame is gone.
        QMetaObject::invokeMethod(this, [action] { action->trigger(); }, Qt::QueuedConnection);
        return;
    }
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

    // Every chip re-reads its own setting, so the strip agrees with the store
    // whoever wrote it — the F-keys, the command line, a script or the AI.
    const core::Settings& store = controller_->bus().app_settings();
    for (const QString& id :
         {QStringLiteral("core.izgara.gorunur"), QStringLiteral("core.izgara.yakalama"),
          QStringLiteral("core.yakalama.dik"), QStringLiteral("core.yakalama.polar"),
          QStringLiteral("core.yakalama.acik"), QStringLiteral("core.arayuz.dinamik_girdi"),
          QStringLiteral("core.harita.kalinlik")}) {
        if (store.catalogue().find(id.toStdString()) == core::kNoSetting) continue;
        statusStrip_->setToggle(id, store.get(id.toStdString()).as_bool());
    }

    const io::DatabaseService& db = controller_->database();
    statusStrip_->setConnection(db.connected()
                                    ? tr("PostGIS · %1").arg(QString::fromStdString(db.target()))
                                : io::DatabaseService::available() ? tr("PostGIS · bağlı değil")
                                                                   : tr("PostGIS · bu yapıda yok"),
                                db.connected());
    statusStrip_->setPerformance(canvas_->backendName());

    // The PLOT scale, not a pixel size: how many ground millimetres one paper
    // millimetre carries. That is the number printed in a pafta's title block and
    // the number an engineer means by "ölçek".
    const double ground_mm_per_paper_mm =
        canvas_->view().mm_per_pixel() * canvas_->pixelsPerPaperMm();

    readout_->setScale(ground_mm_per_paper_mm > 0.0
                           ? tr("1 : %1").arg(groupedNumber(qRound(ground_mm_per_paper_mm)))
                           : QStringLiteral("—"));
}

void MainWindow::showCommandReference()
{
    // Generated from the registry, never hand-written (kentoscad.md §2.3).
    QMessageBox box(this);
    box.setWindowTitle(tr("Komut Listesi"));
    box.setTextFormat(Qt::MarkdownText);
    box.setText(QString::fromStdString(controller_->registry().markdown_reference()));
    box.exec();
}

void MainWindow::runScriptLine(const QString& line)
{
    controller_->runLine(line, command::Origin::Gui);
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

void MainWindow::importData()
{
    if (!io::vector_backend_available()) {
        onEcho(QString::fromStdString(io::vector_backend_status()));
        return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this, tr("İçe aktar"), QString(), externalFormatFilter(false));
    if (path.isEmpty()) return;

    controller_->runLine(QStringLiteral("İÇEAKTAR \"%1\"").arg(path), command::Origin::Gui);
    controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
}

void MainWindow::exportData()
{
    if (!io::vector_backend_available()) {
        onEcho(QString::fromStdString(io::vector_backend_status()));
        return;
    }
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Dışa aktar"), QString(), externalFormatFilter(true));
    if (path.isEmpty()) return;

    controller_->runLine(QStringLiteral("DIŞAAKTAR \"%1\"").arg(path), command::Origin::Gui);
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

            const int mark = transcript_->toPlainText().size();
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
}

} // namespace kentos::app
