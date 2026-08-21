// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/main_window.hpp"

#include "piricad/app/command_line.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/app/icons.hpp"
#include "piricad/app/map_canvas.hpp"
#include "piricad/app/panels.hpp"
#include "piricad/app/toolbox.hpp"
#include "piricad/render/backend.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>

namespace piricad::app {
namespace {

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

    auto* rule = new QFrame(central);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Plain);

    layout->addWidget(canvas_, 1);
    layout->addWidget(rule);
    layout->addWidget(commandLine_);
    setCentralWidget(central);

    buildActions();
    buildToolBox();
    buildPanels();
    buildMenus();
    buildStatusBar();

    QSettings settings;
    theme_ = settings.value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString() ==
                     QLatin1String("dark")
                 ? ThemeMode::Dark
                 : ThemeMode::Light;
    actTheme_->setChecked(theme_ == ThemeMode::Dark);
    applyTheme();

    connect(controller_, &Controller::echoed, this, &MainWindow::onEcho);
    connect(controller_, &Controller::documentChanged, this, &MainWindow::onDocumentChanged);
    connect(controller_, &Controller::promptChanged, this, &MainWindow::onPromptChanged);
    connect(controller_, &Controller::undoStateChanged, this, &MainWindow::onUndoStateChanged);
    connect(controller_, &Controller::viewRequested, this, &MainWindow::onViewRequested);
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
    onDocumentChanged();
    commandLine_->setFocus();
}

MainWindow::~MainWindow()
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/theme"),
                      theme_ == ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/state"), saveState());
}

void MainWindow::buildActions()
{
    const auto command = [this](const QString& name) {
        return [this, name] { controller_->runCommand(name); };
    };
    const auto line = [this](const QString& text) {
        return [this, text] { controller_->runLine(text, command::Origin::Gui); };
    };

    actSelect_ = new QAction(tr("Seç"), this);
    actSelect_->setCheckable(true);
    actSelect_->setChecked(true);
    actSelect_->setToolTip(tr("Seçim aracı — çalışan komutu iptal eder (Esc)"));
    connect(actSelect_, &QAction::triggered, this, [this] { controller_->cancelInteractive(); });

    actLine_ = new QAction(tr("Çizgi"), this);
    actLine_->setToolTip(tr("ÇİZGİ — ardışık doğru parçaları çizer  ·  kısaltma: Ç, L"));
    connect(actLine_, &QAction::triggered, this, command(QStringLiteral("ÇİZGİ")));

    actErase_ = new QAction(tr("Sil"), this);
    actErase_->setToolTip(tr("SİL — seçilen nesneleri siler"));
    connect(actErase_, &QAction::triggered, this, [this] {
        onEcho(tr("SİL komutu nesne kimliği ister. Örnek:  SİL nesneler=0"));
        commandLine_->setText(QStringLiteral("SİL nesneler="));
        commandLine_->setFocus();
    });

    actLayer_ = new QAction(tr("Katman"), this);
    actLayer_->setToolTip(tr("KATMAN — katman oluşturur ve aktif yapar"));
    connect(actLayer_, &QAction::triggered, this, command(QStringLiteral("KATMAN")));

    actMeasure_ = new QAction(tr("Ölç"), this);
    actMeasure_->setEnabled(false);
    actMeasure_->setToolTip(tr("ÖLÇ — mesafe ve alan ölçümü (Faz 2)"));

    actPan_ = new QAction(tr("Kaydır"), this);
    actPan_->setToolTip(tr("Görünümü kaydır — orta fare tuşuyla her zaman çalışır"));
    actPan_->setEnabled(false);

    actZoomExtents_ = new QAction(tr("Kapsama Yakınlaş"), this);
    actZoomExtents_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    actZoomExtents_->setToolTip(tr("YAKINLAŞ KAPSAM — çizimin tamamını göster"));
    connect(actZoomExtents_, &QAction::triggered, this, line(QStringLiteral("YAKINLAŞ KAPSAM")));

    actZoomIn_ = new QAction(tr("Yakınlaştır"), this);
    actZoomIn_->setShortcut(QKeySequence::ZoomIn);
    actZoomIn_->setToolTip(tr("YAKINLAŞ ÇARPAN carpan=1.25"));
    connect(actZoomIn_, &QAction::triggered, this,
            line(QStringLiteral("YAKINLAŞ ÇARPAN carpan=1.25")));

    actZoomOut_ = new QAction(tr("Uzaklaştır"), this);
    actZoomOut_->setShortcut(QKeySequence::ZoomOut);
    actZoomOut_->setToolTip(tr("YAKINLAŞ ÇARPAN carpan=0.8"));
    connect(actZoomOut_, &QAction::triggered, this,
            line(QStringLiteral("YAKINLAŞ ÇARPAN carpan=0.8")));

    actUndo_ = new QAction(tr("Geri Al"), this);
    actUndo_->setShortcut(QKeySequence::Undo);
    actUndo_->setToolTip(tr("GERİAL — son işlemi geri alır"));
    connect(actUndo_, &QAction::triggered, this, command(QStringLiteral("GERİAL")));

    actRedo_ = new QAction(tr("Yinele"), this);
    actRedo_->setShortcut(QKeySequence::Redo);
    actRedo_->setToolTip(tr("YİNELE — geri alınan işlemi yineler"));
    connect(actRedo_, &QAction::triggered, this, command(QStringLiteral("YİNELE")));

    actScript_ = new QAction(tr("Betik Çalıştır…"), this);
    actScript_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actScript_->setToolTip(tr("BETİK — bir JSON betiğini komut veri yolundan çalıştırır"));
    connect(actScript_, &QAction::triggered, this, &MainWindow::openScript);

    actAi_ = new QAction(tr("AI Asistan"), this);
    actAi_->setEnabled(false);
    actAi_->setToolTip(tr("AI komut önerisi — önizleme ve onay ile (Faz 3)"));

    actTheme_ = new QAction(tr("Koyu Tema"), this);
    actTheme_->setCheckable(true);
    connect(actTheme_, &QAction::toggled, this, &MainWindow::toggleTheme);

    // Render statistics are a developer overlay, never a user-facing feature
    // (piricad.md §6.3, .claude/render.md). Off by default.
    actHud_ = new QAction(tr("Geliştirici Bilgisi"), this);
    actHud_->setCheckable(true);
    actHud_->setShortcut(QKeySequence(Qt::Key_F12));
    connect(actHud_, &QAction::toggled, this, [this](bool on) { canvas_->setDebugHud(on); });

    actQuit_ = new QAction(tr("Çıkış"), this);
    actQuit_->setShortcut(QKeySequence::Quit);
    connect(actQuit_, &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(tr("&Dosya"));
    file->addAction(actScript_);
    file->addSeparator();
    file->addAction(actQuit_);

    auto* edit = menuBar()->addMenu(tr("&Düzen"));
    edit->addAction(actUndo_);
    edit->addAction(actRedo_);
    edit->addSeparator();
    edit->addAction(actErase_);

    auto* draw = menuBar()->addMenu(tr("Çi&zim"));
    draw->addAction(actLine_);
    draw->addAction(actLayer_);
    draw->addAction(actMeasure_);

    auto* view = menuBar()->addMenu(tr("&Görünüm"));
    view->addAction(actZoomExtents_);
    view->addAction(actZoomIn_);
    view->addAction(actZoomOut_);
    view->addSeparator();

    auto* panels = view->addMenu(tr("Paneller"));
    for (QDockWidget* dock : {static_cast<QDockWidget*>(toolBox_), layerDock_, propertyDock_,
                              transcriptDock_, journalDock_}) {
        if (dock) panels->addAction(dock->toggleViewAction());
    }
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
    toolBox_ = new ToolBox(this);

    toolBox_->addTool(actSelect_);
    toolBox_->addSeparator();
    toolBox_->addTool(actLine_);
    toolBox_->addTool(actLayer_);
    toolBox_->addTool(actMeasure_);
    toolBox_->addSeparator();
    toolBox_->addTool(actErase_);
    toolBox_->addTool(actUndo_);
    toolBox_->addTool(actRedo_);
    toolBox_->addSeparator();
    toolBox_->addTool(actPan_);
    toolBox_->addTool(actZoomExtents_);
    toolBox_->addTool(actZoomIn_);
    toolBox_->addTool(actZoomOut_);
    toolBox_->addSeparator();
    toolBox_->addTool(actScript_);
    toolBox_->addTool(actAi_);

    addDockWidget(Qt::LeftDockWidgetArea, toolBox_);
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

    statusCoords_->setMinimumWidth(240);
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

    // Icons are drawn, not loaded, so they re-tint with the palette.
    actSelect_->setIcon(icon(Glyph::Select, p.text, p.accent));
    actLine_->setIcon(icon(Glyph::Line, p.text, p.accent));
    actLayer_->setIcon(icon(Glyph::Layer, p.text, p.accent));
    actMeasure_->setIcon(icon(Glyph::Measure, p.text, p.accent));
    actErase_->setIcon(icon(Glyph::Erase, p.text, p.accent));
    actUndo_->setIcon(icon(Glyph::Undo, p.text, p.accent));
    actRedo_->setIcon(icon(Glyph::Redo, p.text, p.accent));
    actPan_->setIcon(icon(Glyph::Pan, p.text, p.accent));
    actZoomExtents_->setIcon(icon(Glyph::ZoomExtents, p.text, p.accent));
    actZoomIn_->setIcon(icon(Glyph::ZoomIn, p.text, p.accent));
    actZoomOut_->setIcon(icon(Glyph::ZoomOut, p.text, p.accent));
    actScript_->setIcon(icon(Glyph::Script, p.text, p.accent));
    actAi_->setIcon(icon(Glyph::Ai, p.text, p.accent));

    toolBox_->applyTheme(theme_);
    canvas_->applyTheme(theme_);
}

void MainWindow::toggleTheme(bool dark)
{
    theme_ = dark ? ThemeMode::Dark : ThemeMode::Light;
    applyTheme();
    onEcho(dark ? tr("Koyu tema.") : tr("Gündüz teması."));
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
    statusCoords_->setText(tr("X %1   Y %2").arg(format_metres(world.x), format_metres(world.y)));
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
