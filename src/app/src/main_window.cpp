// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/main_window.hpp"

#include "piricad/app/command_line.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/app/icons.hpp"
#include "piricad/app/map_canvas.hpp"
#include "piricad/app/panels.hpp"
#include "piricad/app/toolbox.hpp"
#include "piricad/render/backend.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/core/settings.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace piricad::app {
namespace {

/// The same swatch the layer panel draws, so the combo and the panel agree.
QIcon swatchIcon(std::uint32_t rgba)
{
    QPixmap pm(24, 12);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor::fromRgba(static_cast<QRgb>(rgba)));
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawRoundedRect(QRectF(0.5, 0.5, 23.0, 11.0), 2, 2);
    return QIcon(pm);
}

QString format_metres(core::Mm v)
{
    return QString::number(core::mm_to_metres(v), 'f', 3);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    controller_ = new Controller(this);

    setWindowTitle(tr("PiriCAD — Türkiye Odaklı CBS + CAD"));
    resize(1560, 960);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks |
                   QMainWindow::AllowNestedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    canvas_ = new MapCanvas(*controller_, this);

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    commandLine_ = new CommandLine(*controller_, central);

    commandLineRule_ = new QFrame(central);
    commandLineRule_->setFrameShape(QFrame::HLine);
    commandLineRule_->setFrameShadow(QFrame::Plain);

    layout->addWidget(canvas_, 1);
    layout->addWidget(commandLineRule_);
    layout->addWidget(commandLine_);
    setCentralWidget(central);

    buildActions();
    buildToolBars();
    buildToolBox();
    buildPanels();
    buildMenus();
    buildStatusBar();

    // The command line is hidden in this build; the tool bars carry the work.
    // Nothing was removed — Ctrl+9 or Görünüm > Paneller brings it back, and every
    // command it accepts is still reachable from a script and from the AI.
    commandLine_->setVisible(false);

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

    connect(controller_, &Controller::echoed, this, &MainWindow::onEcho);
    connect(controller_, &Controller::documentChanged, this, &MainWindow::onDocumentChanged);
    connect(controller_, &Controller::promptChanged, this, &MainWindow::onPromptChanged);
    connect(controller_, &Controller::undoStateChanged, this, &MainWindow::onUndoStateChanged);
    connect(controller_, &Controller::viewRequested, this, &MainWindow::onViewRequested);
    connect(controller_, &Controller::settingChanged, this, &MainWindow::onSettingChanged);
    connect(controller_, &Controller::selectionChanged, canvas_,
            QOverload<>::of(&MapCanvas::update));
    connect(canvas_, &MapCanvas::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(canvas_, &MapCanvas::viewChanged, this, &MainWindow::refreshStatus);
    connect(commandLine_, &CommandLine::submitted, this, &MainWindow::onCommandSubmitted);
    connect(layerPanel_, &LayerPanel::layerSelected, propertyPanel_, &PropertyPanel::setLayer);

    onEcho(tr("PiriCAD %1 — komut merkezli mimari, GPLv3.").arg(QStringLiteral(PIRICAD_VERSION)));
    onEcho(tr("Aynı komut arayüzden, komut satırından ve betikten tıpatıp aynı yolu izler."));
    if (const std::string status = render::gpu_backend_status(); !status.empty())
        onEcho(tr("Not: %1").arg(QString::fromStdString(status)));
    onEcho(tr("Başlamak için: ÇİZGİ  ·  ÇİZGİ 485320,4310220 @50,30 @100<45  ·  YARDIM"));

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
    settings.setValue(QStringLiteral("ui/state"), saveState());
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
    actNew_  = placeholder(Glyph::New, tr("Yeni"), QStringLiteral("YENİ"), tr("Faz 1"));
    actOpen_ = placeholder(Glyph::Open, tr("Aç"), QStringLiteral("AÇ"), tr("Faz 1"));
    actSave_ = placeholder(Glyph::Save, tr("Kaydet"), QStringLiteral("KAYDET"), tr("Faz 1"));
    actExport_ =
        placeholder(Glyph::Export, tr("Dışa Aktar"), QStringLiteral("DIŞAAKTAR"), tr("Faz 2"));
    actPrint_ = placeholder(Glyph::Print, tr("Yazdır"), QStringLiteral("YAZDIR"), tr("Faz 2"));

    actScript_ = new QAction(tr("Betik Çalıştır…"), this);
    actScript_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actScript_->setToolTip(tr("BETİK — bir JSON betiğini komut veri yolundan çalıştırır"));
    actScript_->setData(static_cast<int>(Glyph::Script));
    connect(actScript_, &QAction::triggered, this, &MainWindow::openScript);

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
    actLine_->setToolTip(tr("ÇİZGİ — ardışık doğru parçaları çizer  ·  kısaltma: Ç, L"));
    actLine_->setData(static_cast<int>(Glyph::Line));
    connect(actLine_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("ÇİZGİ")); });

    actPolyline_ =
        placeholder(Glyph::Polyline, tr("Çoklu Çizgi"), QStringLiteral("ÇOKLUÇİZGİ"), tr("Faz 2"));
    actArc_    = placeholder(Glyph::Arc, tr("Yay"), QStringLiteral("YAY"), tr("Faz 2"));
    actCircle_ = placeholder(Glyph::Circle, tr("Daire"), QStringLiteral("DAİRE"), tr("Faz 2"));
    actRectangle_ =
        placeholder(Glyph::Rectangle, tr("Dikdörtgen"), QStringLiteral("DİKDÖRTGEN"), tr("Faz 2"));
    actPoint_ = placeholder(Glyph::Point, tr("Nokta"), QStringLiteral("NOKTA"), tr("Faz 2"));
    actText_  = placeholder(Glyph::Text, tr("Metin"), QStringLiteral("METİN"), tr("Faz 2"));

    // ---- düzenleme ----
    actErase_ = new QAction(tr("Sil"), this);
    actErase_->setToolTip(tr("SİL — seçilen nesneleri siler"));
    actErase_->setData(static_cast<int>(Glyph::Erase));
    connect(actErase_, &QAction::triggered, this, [this] {
        // With a selection the button IS the command, exactly as typing `SİL`
        // would be. With nothing selected there is nothing to name, so the button
        // opens the command line rather than doing something silently.
        if (!controller_->bus().selection().empty()) {
            controller_->runCommand(QStringLiteral("SİL"));
            return;
        }
        onEcho(tr("Silinecek nesne seçili değil. Nesneleri seçin ya da "
                  "SİL nesneler=1 yazın."));
        showCommandLine(true);
        commandLine_->setText(QStringLiteral("SİL nesneler="));
        commandLine_->setFocus();
    });

    actMove_   = placeholder(Glyph::Move, tr("Taşı"), QStringLiteral("TAŞI"), tr("Faz 2"));
    actCopy_   = placeholder(Glyph::Copy, tr("Kopyala"), QStringLiteral("KOPYALA"), tr("Faz 2"));
    actRotate_ = placeholder(Glyph::Rotate, tr("Döndür"), QStringLiteral("DÖNDÜR"), tr("Faz 2"));
    actOffset_ = placeholder(Glyph::Offset, tr("Ofset"), QStringLiteral("OFSET"), tr("Faz 2"));

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

    actPan_ = placeholder(Glyph::Pan, tr("Kaydır"), QStringLiteral("KAYDIR"), tr("Faz 2"));
    actPan_->setToolTip(tr("Kaydır — orta fare tuşu basılı sürükleme her zaman çalışır"));

    // ---- girdi yardımları ----
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

    // ---- katman ve CBS ----
    actLayer_ = new QAction(tr("Katman"), this);
    actLayer_->setToolTip(tr("KATMAN — katman oluşturur ve aktif yapar"));
    actLayer_->setData(static_cast<int>(Glyph::Layer));
    connect(actLayer_, &QAction::triggered, this,
            [this] { controller_->runCommand(QStringLiteral("KATMAN")); });

    actLayerManager_ = placeholder(Glyph::LayerManager, tr("Katman Yöneticisi"),
                                   QStringLiteral("KATMANYÖNETİCİSİ"), tr("Faz 1"));
    actMeasure_      = placeholder(Glyph::Measure, tr("Ölç"), QStringLiteral("ÖLÇ"), tr("Faz 2"));
    actIdentify_ =
        placeholder(Glyph::Identify, tr("Sorgula"), QStringLiteral("SORGULA"), tr("Faz 2"));
    actTable_ = placeholder(Glyph::Table, tr("Öznitelik Tablosu"),
                            QStringLiteral("ÖZNİTELİKTABLOSU"), tr("Faz 2"));
    actAi_    = placeholder(Glyph::Ai, tr("AI Asistan"), QString(), tr("Faz 3"));
    actAi_->setToolTip(tr("AI komut önerisi — önizleme ve onay ile (Faz 3)"));

    // ---- arayüz ----
    actTheme_ = new QAction(tr("Koyu Tema"), this);
    actTheme_->setCheckable(true);
    connect(actTheme_, &QAction::toggled, this, &MainWindow::toggleTheme);

    // Render statistics are a developer overlay, never a user-facing feature
    // (piricad.md §6.3, .claude/render.md). Off by default.
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
    // Two surfaces, two jobs — the layout professional CAD and GIS users expect.
    // The left tool box holds the modal DRAWING tools; these horizontal bars hold
    // ACTIONS. Every one of them dispatches a command; none reaches the document
    // directly (CLAUDE.md Article 1).
    const auto makeBar = [this](const QString& title, const QString& name) {
        auto* bar = addToolBar(title);
        bar->setObjectName(name);
        bar->setIconSize(QSize(20, 20));
        bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        bar->setFloatable(true);
        bar->setMovable(true);
        return bar;
    };

    tbFile_ = makeBar(tr("Dosya"), QStringLiteral("tbFile"));
    tbFile_->addAction(actNew_);
    tbFile_->addAction(actOpen_);
    tbFile_->addAction(actSave_);
    tbFile_->addSeparator();
    tbFile_->addAction(actExport_);
    tbFile_->addAction(actPrint_);
    tbFile_->addSeparator();
    tbFile_->addAction(actScript_);

    tbEdit_ = makeBar(tr("Düzen"), QStringLiteral("tbEdit"));
    tbEdit_->addAction(actUndo_);
    tbEdit_->addAction(actRedo_);
    tbEdit_->addSeparator();
    tbEdit_->addAction(actErase_);
    tbEdit_->addAction(actMove_);
    tbEdit_->addAction(actCopy_);
    tbEdit_->addAction(actRotate_);
    tbEdit_->addAction(actOffset_);

    tbView_ = makeBar(tr("Görünüm"), QStringLiteral("tbView"));
    tbView_->addAction(actPan_);
    tbView_->addAction(actZoomExtents_);
    tbView_->addAction(actZoomIn_);
    tbView_->addAction(actZoomOut_);
    tbView_->addSeparator();
    tbView_->addAction(actSnap_);

    // The layer combo is the signature CAD control: it shows the current layer and
    // switching it is a KATMAN command, exactly as if it had been typed.
    tbLayer_ = makeBar(tr("Katman"), QStringLiteral("tbLayer"));
    tbLayer_->addAction(actLayerManager_);
    tbLayer_->addAction(actLayer_);

    layerCombo_ = new QComboBox(tbLayer_);
    layerCombo_->setMinimumWidth(190);
    layerCombo_->setToolTip(tr("Aktif katman — değiştirmek KATMAN komutunu gönderir"));
    layerCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    tbLayer_->addWidget(layerCombo_);

    connect(layerCombo_, &QComboBox::activated, this, [this](int index) {
        const QString name = layerCombo_->itemText(index);
        if (name.isEmpty()) return;
        controller_->runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(name), command::Origin::Gui);
    });

    tbGis_ = makeBar(tr("CBS"), QStringLiteral("tbGis"));
    tbGis_->addAction(actIdentify_);
    tbGis_->addAction(actTable_);
    tbGis_->addAction(actMeasure_);
    tbGis_->addSeparator();
    tbGis_->addAction(actAi_);
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(tr("&Dosya"));
    file->addAction(actNew_);
    file->addAction(actOpen_);
    file->addAction(actSave_);
    file->addSeparator();
    file->addAction(actExport_);
    file->addAction(actPrint_);
    file->addSeparator();
    file->addAction(actScript_);
    file->addSeparator();
    file->addAction(actQuit_);

    auto* edit = menuBar()->addMenu(tr("&Düzen"));
    edit->addAction(actUndo_);
    edit->addAction(actRedo_);
    edit->addSeparator();
    edit->addAction(actSelectAll_);
    edit->addAction(actSelectNone_);
    edit->addSeparator();
    edit->addAction(actErase_);
    edit->addAction(actMove_);
    edit->addAction(actCopy_);
    edit->addAction(actRotate_);
    edit->addAction(actOffset_);

    auto* draw = menuBar()->addMenu(tr("Çi&zim"));
    draw->addAction(actLine_);
    draw->addAction(actPolyline_);
    draw->addAction(actArc_);
    draw->addAction(actCircle_);
    draw->addAction(actRectangle_);
    draw->addAction(actPoint_);
    draw->addAction(actText_);
    draw->addSeparator();
    draw->addAction(actLayer_);
    draw->addAction(actLayerManager_);

    auto* gis = menuBar()->addMenu(tr("&CBS"));
    gis->addAction(actIdentify_);
    gis->addAction(actTable_);
    gis->addAction(actMeasure_);
    gis->addSeparator();
    gis->addAction(actAi_);

    auto* view = menuBar()->addMenu(tr("&Görünüm"));
    view->addAction(actZoomExtents_);
    view->addAction(actZoomIn_);
    view->addAction(actZoomOut_);
    view->addSeparator();
    view->addAction(actSnap_);
    view->addAction(actOrtho_);
    view->addAction(actGridSnap_);
    view->addSeparator();

    auto* bars = view->addMenu(tr("Araç Çubukları"));
    for (QToolBar* bar : {tbFile_, tbEdit_, tbView_, tbLayer_, tbGis_})
        if (bar) bars->addAction(bar->toggleViewAction());

    auto* panels = view->addMenu(tr("Paneller"));
    for (QDockWidget* dock : {static_cast<QDockWidget*>(toolBox_), layerDock_, propertyDock_,
                              transcriptDock_, journalDock_}) {
        if (dock) panels->addAction(dock->toggleViewAction());
    }
    panels->addSeparator();
    panels->addAction(actCommandLine_);
    panels->addSeparator();
    auto* reset = panels->addAction(tr("Düzeni Sıfırla"));
    connect(reset, &QAction::triggered, this, &MainWindow::resetLayout);

    view->addSeparator();
    view->addAction(actTheme_);
    view->addAction(actHud_);

    auto* about = menuBar()->addMenu(tr("&Yardım"));
    auto* ref   = about->addAction(tr("Komut Listesi"));
    connect(ref, &QAction::triggered, this, &MainWindow::showCommandReference);
    about->addSeparator();
    auto* info = about->addAction(tr("Hakkında"));
    connect(info, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::buildToolBox()
{
    // The modal drawing tools only. File, edit, view, layer and GIS actions live
    // in the horizontal tool bars, the way AutoCAD and QGIS both arrange them.
    toolBox_ = new ToolBox(this);

    toolBox_->addTool(actSelect_);
    toolBox_->addSeparator();
    toolBox_->addTool(actLine_);
    toolBox_->addTool(actPolyline_);
    toolBox_->addTool(actArc_);
    toolBox_->addTool(actCircle_);
    toolBox_->addTool(actRectangle_);
    toolBox_->addTool(actPoint_);
    toolBox_->addTool(actText_);
    toolBox_->addSeparator();
    toolBox_->addTool(actErase_);
    toolBox_->addTool(actMove_);
    toolBox_->addTool(actCopy_);
    toolBox_->addTool(actRotate_);
    toolBox_->addTool(actOffset_);
    toolBox_->addSeparator();
    toolBox_->addTool(actMeasure_);
    toolBox_->addTool(actIdentify_);
    toolBox_->addTool(actSnap_);

    addDockWidget(Qt::LeftDockWidgetArea, toolBox_);
    // Two columns to start with. The palette reflows, so this is a starting shape
    // and not a constraint: dragging the splitter turns it into three, four or one.
    resizeDocks({toolBox_}, {74}, Qt::Horizontal);
}

void MainWindow::buildPanels()
{
    const auto makeDock = [this](const QString& title, const QString& name, QWidget* body) {
        auto* dock = new QDockWidget(title, this);
        dock->setObjectName(name);
        dock->setWidget(body);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
                              Qt::BottomDockWidgetArea);
        return dock;
    };

    layerPanel_    = new LayerPanel(*controller_, this);
    propertyPanel_ = new PropertyPanel(*controller_, this);

    layerDock_    = makeDock(tr("Katmanlar"), QStringLiteral("layerDock"), layerPanel_);
    propertyDock_ = makeDock(tr("Öznitelikler"), QStringLiteral("propertyDock"), propertyPanel_);

    addDockWidget(Qt::RightDockWidgetArea, layerDock_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    tabifyDockWidget(layerDock_, propertyDock_);
    layerDock_->raise();

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

    transcriptDock_ = makeDock(tr("Transkript"), QStringLiteral("transcriptDock"), transcript_);
    journalDock_    = makeDock(tr("Komut Günlüğü"), QStringLiteral("journalDock"), journalView_);

    addDockWidget(Qt::BottomDockWidgetArea, transcriptDock_);
    addDockWidget(Qt::BottomDockWidgetArea, journalDock_);
    tabifyDockWidget(transcriptDock_, journalDock_);
    transcriptDock_->raise();

    resizeDocks({layerDock_}, {330}, Qt::Horizontal);
    resizeDocks({transcriptDock_}, {170}, Qt::Vertical);

    for (QDockWidget* dock : {layerDock_, propertyDock_, transcriptDock_, journalDock_}) {
        connect(dock, &QDockWidget::topLevelChanged, this, [this] { syncDockTitles(); });
        connect(dock, &QDockWidget::visibilityChanged, this, [this] { syncDockTitles(); });
    }

    QSettings settings;
    if (settings.contains(QStringLiteral("ui/state"))) {
        restoreGeometry(settings.value(QStringLiteral("ui/geometry")).toByteArray());
        restoreState(settings.value(QStringLiteral("ui/state")).toByteArray());
    }
}

void MainWindow::syncDockTitles()
{
    for (QDockWidget* dock : {layerDock_, propertyDock_, transcriptDock_, journalDock_}) {
        if (!dock) continue;

        const bool tabbed = !dock->isFloating() && !tabifiedDockWidgets(dock).isEmpty();
        const bool hidden = dock->titleBarWidget() != nullptr;
        if (tabbed == hidden) continue;

        if (tabbed) {
            dock->setTitleBarWidget(new QWidget(dock));
        } else {
            QWidget* old = dock->titleBarWidget();
            dock->setTitleBarWidget(nullptr);
            delete old;
        }
    }
}

void MainWindow::buildStatusBar()
{
    statusPrompt_ = new QLabel(tr("Hazır"), this);
    statusCoords_ = new QLabel(QStringLiteral("—"), this);
    statusScale_  = new QLabel(QStringLiteral("—"), this);
    statusLayer_  = new QLabel(QStringLiteral("0"), this);
    statusCrs_    = new QLabel(QStringLiteral("TUREF/TM30"), this);

    statusCoords_->setMinimumWidth(330);
    statusScale_->setMinimumWidth(140);
    statusLayer_->setMinimumWidth(110);

    statusBar()->addWidget(statusPrompt_, 1);
    statusBar()->addPermanentWidget(new QLabel(tr("Katman:"), this));
    statusBar()->addPermanentWidget(statusLayer_);
    statusBar()->addPermanentWidget(statusCoords_);
    statusBar()->addPermanentWidget(statusScale_);
    statusBar()->addPermanentWidget(statusCrs_);
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

    toolBox_->applyTheme(theme_);
    canvas_->applyTheme(theme_);
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
    if (!layerCombo_) return;

    const auto& doc      = controller_->document();
    const QString active = controller_->activeLayerName();

    QSignalBlocker block(layerCombo_);
    layerCombo_->clear();

    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& l = doc.layers()[i];
        layerCombo_->addItem(swatchIcon(l.appearance.rgba), QString::fromStdString(l.name));
        if (!l.visible) layerCombo_->setItemData(static_cast<int>(i), tr("gizli"), Qt::ToolTipRole);
    }

    const int index = layerCombo_->findText(active);
    if (index >= 0) layerCombo_->setCurrentIndex(index);
}

void MainWindow::resetLayout()
{
    addDockWidget(Qt::LeftDockWidgetArea, toolBox_);
    addDockWidget(Qt::RightDockWidgetArea, layerDock_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    tabifyDockWidget(layerDock_, propertyDock_);
    layerDock_->raise();
    syncDockTitles();
}

void MainWindow::onEcho(const QString& text)
{
    transcript_->appendPlainText(text);
}

void MainWindow::onDocumentChanged()
{
    refreshLayerCombo();
    layerPanel_->refresh();
    propertyPanel_->refresh();
    refreshStatus();

    // Mirroring the journal keeps the architecture visible while using the program.
    journalView_->clear();
    for (const auto& e : controller_->journal().entries())
        journalView_->appendPlainText(QString::fromStdString(e.to_json(false).dump()));

    canvas_->update();
}

void MainWindow::onPromptChanged(const QString& prompt)
{
    commandLine_->setPrompt(prompt);
    statusPrompt_->setText(prompt.isEmpty() ? tr("Hazır") : prompt);
    actSelect_->setChecked(prompt.isEmpty());
    actLine_->setChecked(!prompt.isEmpty());
    canvas_->update();
}

void MainWindow::onUndoStateChanged(bool canUndo, bool canRedo)
{
    actUndo_->setEnabled(canUndo);
    actRedo_->setEnabled(canRedo);
}

void MainWindow::onCursorMoved(core::Point2 world)
{
    // Turkish surveying convention, which EPSG:5254 itself declares: Y is the
    // easting (sağa değer) and X is the northing (yukarı değer). Storage is
    // unaffected — Point2::x holds the easting either way (.claude/model.md R37a).
    statusCoords_->setText(
        tr("Sağa (Y) %1   Yukarı (X) %2").arg(format_metres(world.x), format_metres(world.y)));
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

void MainWindow::onCommandSubmitted(const QString& line)
{
    onEcho(QStringLiteral("> ") + line);
    controller_->runLine(line, command::Origin::CommandLine);
}

void MainWindow::refreshStatus()
{
    statusLayer_->setText(controller_->activeLayerName());
    statusCrs_->setText(QString::fromStdString(controller_->document().crs().id()));

    const double metres_per_pixel =
        canvas_->view().mm_per_pixel() / static_cast<double>(core::kMmPerMetre);
    statusScale_->setText(tr("1 px = %1 m").arg(metres_per_pixel, 0, 'f', 4));
}

void MainWindow::showCommandReference()
{
    // Generated from the registry, never hand-written (piricad.md §2.3).
    QMessageBox box(this);
    box.setWindowTitle(tr("Komut Listesi"));
    box.setTextFormat(Qt::MarkdownText);
    box.setText(QString::fromStdString(controller_->registry().markdown_reference()));
    box.exec();
}

void MainWindow::runScriptFile(const QString& path)
{
    if (path.isEmpty()) return;

    controller_->runLine(QStringLiteral("BETİK \"%1\"").arg(path), command::Origin::Gui);

    // Bring the result into view. A second command on the same bus, not a special
    // case reaching into the canvas (CLAUDE.md Article 1).
    controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
}

void MainWindow::openScript()
{
    runScriptFile(QFileDialog::getOpenFileName(this, tr("Betik seç"),
                                               QStringLiteral("tests/journal"),
                                               tr("PiriCAD betiği (*.json);;Tüm dosyalar (*)")));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("PiriCAD Hakkında"),
                       tr("<h3>PiriCAD %1</h3>"
                          "<p>Türkiye odaklı CBS + CAD harita yazılımı.</p>"
                          "<p><b>Mimari:</b> Uygulamanın durumunu değiştiren her şey bir komuttur. "
                          "Arayüz, komut veri yolunun yalnızca bir istemcisidir.</p>"
                          "<p><b>Lisans:</b> GPLv3 veya sonrası<br>"
                          "<b>Render:</b> %2<br>"
                          "<b>Komut sayısı:</b> %3</p>")
                           .arg(QStringLiteral(PIRICAD_VERSION), canvas_->backendName())
                           .arg(controller_->registry().size()));
}

} // namespace piricad::app
