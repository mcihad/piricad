// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the main window's ribbon (`.claude/ui.md` R46–R50).
//
// EVERYTHING THE SHELL LAYS OUT ON THE RIBBON, in one file: the application
// menu, the quick access row, the tabs and their panels, the boxes that read
// the document, the editor tabs a selection brings up, and the tip every button
// shows. Nothing here changes the document: a button, a box, a gallery item or a
// launcher runs a command line through the controller, the road every other
// client takes (CLAUDE.md Article 1.2, 5.9).
//
// THE GİRİŞ TAB IS AUTOCAD'S HOME, laid out for a surveyor: four large drawing
// tools and a grid of the rest, the edit verbs in a 3 × 4 grid, text and
// dimension, the layer list and the colours in hand. Every other tab is the long
// form of one of its panels.
#include "kentos_cad/app/main_window.hpp"

#include "kentos_cad/app/app_menu.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/panels.hpp"
#include "kentos_cad/app/print_service.hpp"
#include "kentos_cad/app/ribbon.hpp"
#include "kentos_cad/app/shell_chrome.hpp"
#include "kentos_cad/app/swatch_row.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/tools_panel.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/colour.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>
#include <optional>

namespace kentos::app {

namespace {

constexpr const char* kToolCommand = kToolCommandProperty;

/// The catalogue pattern an action stands for, on the hatch galleries.
constexpr const char* kPatternProperty = "kentos.ribbon.pattern";

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
    QString lower = tr_TR.toLower(shouted);
    if (lower.isEmpty()) return lower;
    return tr_TR.toUpper(lower.left(1)) + lower.mid(1);
}

const Tokens& tokensFor(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// A metre figure the way a Turkish sheet prints it: `2,50`.
QString metresText(double metres, int decimals = 2)
{
    return QLocale(QLocale::Turkish, QLocale::Turkey).toString(metres, 'f', decimals);
}

/// A number typed into a ribbon box: `2,5`, `2.5`, `2,5 m` — the unit is the
/// box's and may be written or left off. Empty when it is not a number.
std::optional<double> typedNumber(QString text, const QString& unit)
{
    text = text.trimmed();
    if (!unit.isEmpty() && text.endsWith(unit, Qt::CaseInsensitive)) text.chop(unit.size());
    text            = text.trimmed().replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok         = false;
    const double at = text.toDouble(&ok);
    if (!ok || !std::isfinite(at)) return std::nullopt;
    return at;
}

/// `1:1000`, `1/1000` or `1000` — a plan scale's denominator.
std::optional<long long> typedScale(QString text)
{
    text = text.trimmed().remove(QLatin1Char(' '));
    for (const QString& lead : {QStringLiteral("1:"), QStringLiteral("1/")})
        if (text.startsWith(lead)) text = text.mid(lead.size());
    bool ok             = false;
    const long long den = text.toLongLong(&ok);
    return ok && den > 0 ? std::optional<long long>(den) : std::nullopt;
}

/// The id a command line names an object by.
QString keyText(core::EntityKey key)
{
    return QString::number(static_cast<qulonglong>(static_cast<std::uint64_t>(key)));
}

/// A caption and a box on one ribbon row, the caption set to `captionWidth` so
/// the rows of a panel line up.
QWidget* captioned(QWidget* parent, const QString& caption, QWidget* box, int captionWidth)
{
    auto* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("ribbonRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(6);
    auto* label = new QLabel(caption, row);
    label->setObjectName(QStringLiteral("ribbonCaption"));
    label->setFixedWidth(captionWidth);
    label->setBuddy(box);
    layout->addWidget(label);
    box->setParent(row);
    layout->addWidget(box);
    return row;
}

} // namespace

// =============================================================================
// The ribbon
// =============================================================================

void MainWindow::buildRibbon()
{
    // THE RIBBON AS SARIBBON LAYS IT OUT: the compact three-row Office layout its
    // native-frame examples use — large buttons with the label under the icon,
    // three rows of small ones beside them, a caption under every panel.
    SARibbonBar* bar = ribbonBar();
    bar->setObjectName(QStringLiteral("shellRibbon"));
    bar->setRibbonStyle(SARibbonBar::RibbonStyleCompactThreeRow);
    // The window's icon belongs to the system's caption, which already shows
    // it; a second one in the tab row pushed `KentOS CAD` off the left edge.
    bar->setTitleIconVisible(false);
    // Office's own sizes: a 32 px picture on a large button, 16 px on a small one.
    constexpr int kLargeIcon = 32;
    constexpr int kSmallIcon = 16;
    bar->setPanelToolButtonIconSize(QSize(kSmallIcon, kSmallIcon), QSize(kLargeIcon, kLargeIcon));
    // THE TAB ROW HAS ROOM FOR ITS WORDS: a folder tab stands 5 px off the top
    // of the row and needs its own height under that (`ribbonStyleSheet`). The
    // tab bar also reserved an icon's width in every tab although no tab has an
    // icon, which is what spread nine short words across the whole row.
    // In the compact layout the tabs share the row with the application button
    // and the quick access row, and are capped at that row's height less two.
    constexpr int kTitleRow = 40;
    constexpr int kTabRow   = kTitleRow - 3;
    bar->setTitleBarHeight(kTitleRow);
    bar->setTabBarHeight(kTabRow);
    if (SARibbonTabBar* tabs = bar->ribbonTabBar(); tabs != nullptr) tabs->setIconSize(QSize(0, 0));
    // THE MAP CAN HAVE THE ROOM BACK: a double click on a tab folds the ribbon to
    // its tab row, another unfolds it.
    bar->setTabDoubleClickToMinimumMode(true);
    // The fold button is ours (below, beside the search): the library's wears
    // the platform's window-shade triangle, which reads as nothing here.
    bar->showMinimumModeButton(false);

    // Every button's tip is composed from its action and the registry.
    RibbonElementFactory::setTipSource([this](const QAction* a) { return ribbonTip(a); });

    // Every tool the ribbon shows, family members included, in ribbon order.
    ribbonTools_.clear();
    const auto remember = [this](QAction* action) {
        if (action != nullptr && !ribbonTools_.contains(action)) ribbonTools_ << action;
    };
    // THREE SIZES, the ribbon's whole grammar: a large button for what a hand
    // does all day, a labelled row for the rest, and a bare picture for a tool
    // everybody knows by its mark — the tip still says its name.
    enum class Size : std::uint8_t { Large, Small, Icon };
    const auto place = [&](SARibbonPanel* panel, QAction* action, Size size,
                           QToolButton::ToolButtonPopupMode mode) {
        if (size == Size::Large)
            panel->addLargeAction(action, mode);
        else
            panel->addSmallAction(action, mode);
        if (size == Size::Icon)
            if (SARibbonToolButton* b = panel->lastAddActionButton(); b != nullptr)
                b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    };
    const auto large = [&](SARibbonPanel* panel, QAction* action) {
        place(panel, action, Size::Large, QToolButton::DelayedPopup);
        remember(action);
    };
    const auto small = [&](SARibbonPanel* panel, QAction* action) {
        place(panel, action, Size::Small, QToolButton::DelayedPopup);
        remember(action);
    };
    const auto icon = [&](SARibbonPanel* panel, QAction* action) {
        place(panel, action, Size::Icon, QToolButton::DelayedPopup);
        remember(action);
    };
    // A FAMILY IS ONE SPLIT BUTTON (`ribbon.hpp`): the face runs the member used
    // last and the arrow lists them all. `word`, when given, is the button's
    // label whichever member is on its face — `Daire`, never "Daire — üç nokta".
    const auto family = [&](SARibbonPanel* panel, const QList<QAction*>& members, Size size,
                            const QString& word = QString()) {
        auto* f = new RibbonFamily(members, this);
        if (!word.isEmpty()) f->setFixedLabel(word);
        families_ << f;
        place(panel, f->head(), size, QToolButton::MenuButtonPopup);
        for (QAction* m : members)
            remember(m);
        return f;
    };
    const auto menuButton = [&](SARibbonPanel* panel, QMenu* menu, Glyph glyph, bool big) {
        menu->menuAction()->setData(static_cast<int>(glyph));
        if (big)
            panel->addLargeMenu(menu);
        else
            panel->addSmallMenu(menu);
    };
    // THE ↘ IN A PANEL'S CAPTION opens the whole window for what the panel
    // shows the everyday part of, the way every ribbon's dialog launcher does.
    const auto launcher = [this](SARibbonPanel* panel, const QString& tip, auto open) {
        auto* action = new QAction(tip, this);
        action->setObjectName(QStringLiteral("ribbonLauncher.") + panel->panelName());
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, open);
        panel->setOptionAction(action);
    };

    // THE SELECT TOOL COMES FIRST ON EVERY TAB, editor tabs included: the hand
    // puts a tool down there, on whatever tab it is, without going back to
    // `Giriş` for it. The arrow holds the other ways to pick — a window, all,
    // none.
    auto* selectMenu = new QMenu(this);
    selectMenu->setObjectName(QStringLiteral("selectMenu"));
    selectMenu->addAction(actSelectArea_);
    selectMenu->addAction(actSelectAll_);
    selectMenu->addAction(actSelectNone_);
    actSelect_->setMenu(selectMenu);
    const auto selectFirst = [this](SARibbonCategory* tab) {
        SARibbonPanel* panel = tab->addPanel(tr("Seçim"));
        panel->setObjectName(QStringLiteral("ribbonSelectPanel"));
        panel->addLargeAction(actSelect_, QToolButton::MenuButtonPopup);
    };
    remember(actSelect_);
    remember(actSelectArea_);
    selectFirst_ = selectFirst;

    // ---- the application button and its menu ------------------------------
    //
    // `KentOS CAD`, where Office writes `Dosya`: what is in it is what a drawing
    // is done to as a FILE — new, open, save, import, export, print, the
    // project's settings, the program's — and the one way out. AutoCAD's
    // application menu (`app_menu.hpp`) and not a backstage page over the whole
    // window: a CAD user opens this to save and goes straight back to a drawing
    // that should never have been covered to do it.
    auto* appButton = new RibbonAppButton(tr("KentOS CAD"), bar);
    appButton->setObjectName(QStringLiteral("ribbonApplicationButton"));
    appButton->setAccessibleName(tr("KentOS CAD ana menüsü"));
    appButton->setAccessibleDescription(
        tr("Yeni, aç, kaydet, içe ve dışa aktar, yazdır, ayarlar ve çıkış"));
    appButton->setToolTip(tr("Ana menü — dosya, yazdırma, son belgeler, ayarlar ve çıkış"));
    appButton_ = appButton;
    bar->setApplicationButton(appButton);
    appMenu_ = new ApplicationMenu(this);
    connect(appButton, &QAbstractButton::clicked, this, &MainWindow::openApplicationMenu);
    connect(appMenu_, &ApplicationMenu::searchRequested, this, [this] { openCommandSearch(); });
    connect(appMenu_, &ApplicationMenu::recentChosen, this, [this](const QString& path) {
        controller_->runLine(QStringLiteral("AÇ \"%1\"").arg(path), command::Origin::Gui);
        controller_->runLine(QStringLiteral("YAKINLAŞ KAPSAM"), command::Origin::Gui);
        refreshWindowTitle();
    });
    // THE FILE VERBS' SHORTCUTS BELONG TO THE WINDOW: the menu is a popup that
    // is closed nearly all the time, and a shortcut on a closed popup is dead.
    addActions({actNew_, actOpen_, actSave_, actSaveAs_, actImport_, actExport_, actPrint_,
                actProjectSettings_, actDatabase_, actScript_, actSettings_, actQuit_});

    auto* reference = new QAction(tr("Komut Listesi"), this);
    reference->setObjectName(QStringLiteral("commandReference"));
    reference->setShortcut(QKeySequence(Qt::Key_F1));
    reference->setShortcutContext(Qt::ApplicationShortcut);
    reference->setToolTip(tr("YARDIM — bütün komutlar, adları ve kısaltmalarıyla"));
    reference->setProperty(kToolCommand, QStringLiteral("YARDIM"));
    reference->setData(static_cast<int>(Glyph::Help));
    connect(reference, &QAction::triggered, this,
            [this] { controller_->runLine(QStringLiteral("YARDIM"), command::Origin::Gui); });
    addAction(reference); // F1 works with the menu closed
    auto* about = new QAction(tr("Hakkında"), this);
    about->setObjectName(QStringLiteral("aboutKentos"));
    about->setToolTip(tr("Sürüm, lisans ve kaynak kodu"));
    about->setData(static_cast<int>(Glyph::Info));
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    auto* appRest = new QMenu(tr("Diğer Komutlar"), this);
    appRest->setObjectName(QStringLiteral("applicationMenuRest"));
    appRest->menuAction()->setData(static_cast<int>(Glyph::More));
    actQuit_->setToolTip(tr("KentOS CAD'i kapatır; kaydedilmemiş değişiklik varsa sorar"));
    appMenu_->setFooter(reference, about, actSettings_, actQuit_);

    // ---- quick access and the corner ---------------------------------------
    //
    // AutoCAD's quick access row: the file verbs a hand wants from any tab, and
    // the two that undo a mistake.
    //
    // YAZDIR WITH ITS SHEETS BESIDE IT: the printer prints, the arrow at its
    // right lists the drawing's layouts — a new one, the manager, the office's
    // templates and every sheet the drawing holds — rebuilt as it opens.
    layoutMenu_ = new QMenu(tr("Çıktı Yerleşimleri"), this);
    layoutMenu_->setObjectName(QStringLiteral("layoutMenu"));
    connect(layoutMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildLayoutMenu);
    rebuildLayoutMenu();
    if (SARibbonQuickAccessBar* quick = bar->quickAccessBar()) {
        quick->addAction(actNew_);
        quick->addAction(actOpen_);
        quick->addAction(actSave_);
        auto* quickPrint = new QAction(actPrint_->text(), this);
        quickPrint->setObjectName(QStringLiteral("quickPrint"));
        quickPrint->setData(actPrint_->data());
        quickPrint->setToolTip(tr("Yazdır (%1) — oktan çıktı yerleşimleri")
                                   .arg(actPrint_->shortcut().toString(QKeySequence::NativeText)));
        quickPrint->setProperty(kToolCommand, QStringLiteral("YAZDIR"));
        // A MENU OF ITS OWN, filled from the layout menu each time it opens. Not
        // the layout menu itself: `QAction::setMenu` makes the action the menu's
        // own `menuAction`, and the `Çıktı` tab's button and the application menu
        // would then both have been showing a second `Yazdır`.
        auto* quickLayouts = new QMenu(this);
        quickLayouts->setObjectName(QStringLiteral("quickLayoutMenu"));
        const auto fillLayouts = [this, quickLayouts] {
            rebuildLayoutMenu();
            quickLayouts->clear();
            quickLayouts->addActions(layoutMenu_->actions());
        };
        connect(quickLayouts, &QMenu::aboutToShow, this, fillLayouts);
        fillLayouts();
        quickPrint->setMenu(quickLayouts);
        connect(quickPrint, &QAction::triggered, actPrint_, &QAction::trigger);
        quick->addAction(quickPrint);
        if (auto* button = qobject_cast<QToolButton*>(quick->widgetForAction(quickPrint));
            button != nullptr) {
            button->setPopupMode(QToolButton::MenuButtonPopup);
            button->setAccessibleName(tr("Yazdır"));
            button->setAccessibleDescription(tr("Oka basın: çıktı yerleşimleri"));
        }
        quick->addSeparator();
        quick->addAction(actUndo_);
        quick->addAction(actRedo_);
    }
    // FOLD AND UNFOLD, the same thing a double click on a tab does, said by a
    // chevron that points the way the ribbon will go.
    auto* fold = new QAction(tr("Şeridi daralt"), this);
    fold->setObjectName(QStringLiteral("ribbonFold"));
    fold->setData(static_cast<int>(Glyph::ChevronUp));
    fold->setToolTip(tr("Şeridi sekme satırına daraltır; bir sekmeye çift tıklamak da aynısını "
                        "yapar"));
    connect(fold, &QAction::triggered, this, [bar] { bar->setMinimumMode(!bar->isMinimumMode()); });
    connect(bar, &SARibbonBar::ribbonModeChanged, this, [this, fold, bar] {
        const bool folded = bar->isMinimumMode();
        fold->setText(folded ? tr("Şeridi aç") : tr("Şeridi daralt"));
        fold->setData(static_cast<int>(folded ? Glyph::ChevronDown : Glyph::ChevronUp));
        fold->setIcon(colour_icon(static_cast<Glyph>(fold->data().toInt()), actionInks()));
    });

    corner_ = new ShellCorner(bar);
    connect(corner_, &ShellCorner::searchRequested, this, [this] { openCommandSearch(); });
    if (SARibbonButtonGroupWidget* right = bar->rightButtonGroup()) {
        right->addAction(fold);
        right->addWidget(corner_);
    }

    // ---- the methods, made once and shown wherever their family is ----------
    //
    // A method a hand needs twice a year is under its family's arrow, where it
    // costs no room and is still one click away.
    auto* rectangleRotated = methodTool(
        Glyph::Rectangle, tr("Dikdörtgen — döndürülmüş"), QStringLiteral("DİKDÖRTGEN yontem=3n"),
        tr("Bir kenarın iki köşesi ve yüksekliği veren üçüncü nokta"));
    auto* regularOutside =
        methodTool(Glyph::Polygon, tr("Çokgen — dıştan"), QStringLiteral("ÇOKGEN yontem=dis"),
                   tr("Kenarlar çembere teğet; yarıçap iç yarıçaptır"));
    auto* regularSide =
        methodTool(Glyph::Polygon, tr("Çokgen — kenardan"), QStringLiteral("ÇOKGEN yontem=kenar"),
                   tr("Kenar uzunluğundan; yarıçap sorulmaz"));
    auto* circleTwo =
        methodTool(Glyph::Circle, tr("Daire — çapın iki ucu"), QStringLiteral("DAİRE yontem=2n"),
                   tr("İki nokta çapı verir; merkez ortalarıdır"));
    auto* circleThree =
        methodTool(Glyph::Circle, tr("Daire — üç nokta"), QStringLiteral("DAİRE yontem=3n"),
                   tr("Çevrel çember: üç noktanın hepsi çemberin üzerinde"));
    auto* circleTangent = methodTool(Glyph::Circle, tr("Daire — iki doğruya teğet"),
                                     QStringLiteral("DAİRE yontem=ttr"),
                                     tr("İki doğru, yarıçap ve dairenin geleceği köşe gösterilir"));
    auto* ellipseAxis   = methodTool(Glyph::Ellipse, tr("Elips — eksenin iki ucu"),
                                     QStringLiteral("ELİPS yontem=eksen"),
                                     tr("Merkez iki ucun ortasıdır; üçüncü nokta ikinci ekseni "
                                          "verir"));
    auto* arcThree = methodTool(Glyph::Arc, tr("Yay — üç nokta"), QStringLiteral("YAY yontem=3n"),
                                tr("Başlangıç, üzerinden geçtiği nokta ve bitiş"));
    auto* arcAngle =
        methodTool(Glyph::Arc, tr("Yay — başlangıç, merkez, açı"), QStringLiteral("YAY yontem=bma"),
                   tr("Süpürme açısı oturumun birim ve kuralıyla okunur"));
    auto* arcRadius = methodTool(Glyph::Arc, tr("Yay — başlangıç, bitiş, yarıçap"),
                                 QStringLiteral("YAY yontem=bby"),
                                 tr("İki çözüm vardır; yon=sol|sag hangisi olduğunu söyler"));
    auto* arcOn =
        methodTool(Glyph::Arc, tr("Yay — teğet devam"), QStringLiteral("YAY yontem=devam"),
                   tr("Son çizilen çizginin ya da yayın ucundan teğet devam eder"));
    auto* crossDistances =
        methodTool(Glyph::PointIntersect, tr("Kesişim — iki mesafeden"),
                   QStringLiteral("KESİŞİMNOKTA yontem=mesafe"),
                   tr("İki bilinen noktadan ölçülen iki uzaklık; iki çözümden birini "
                      "gösterirsiniz"));
    auto* crossLines    = methodTool(Glyph::PointIntersect, tr("Kesişim — iki doğrudan"),
                                     QStringLiteral("KESİŞİMNOKTA yontem=dogru"),
                                     tr("İki doğrunun her birinden iki nokta"));
    auto* alongDistance = methodTool(Glyph::PointAlong, tr("Ara Nokta — mesafeden"),
                                     QStringLiteral("ARANOKTA yontem=mesafe"),
                                     tr("Oran değil, ilk noktadan metre cinsinden uzaklık"));
    auto* areaByCorners = methodTool(Glyph::MeasureArea, tr("Alan Ölç — köşelerden"),
                                     QStringLiteral("ALANÖLÇ yontem=nokta"),
                                     tr("Köşelere tıklayın; alan ve çevre imleçle birlikte "
                                        "yazılır, Enter bitirir"));
    auto* guide = commandAction(Glyph::Guide, tr("Cetvel Kılavuzu"), QStringLiteral("KILAVUZ"),
                                tr("KILAVUZ — cetvel kılavuzu ekler, listeler ve siler  ·  "
                                   "kısaltma: KLV"));

    // EVERY DIMENSION TYPE ITS OWN TOOL (TODOS C-17). The one Ölçü button drew
    // an aligned dimension and nothing else: a radius, an angle or an ordinate
    // needed `tur=` typed at the command line, which is a capability the mouse
    // did not have (CLAUDE.md 5.15). The family is AutoCAD's Home ▸ Dimension
    // list; the face runs whichever type was drawn last.
    const QList<QAction*> dimensionTypes{
        actDimension_,
        methodTool(Glyph::DimLinear, tr("Doğrusal Ölçü"), QStringLiteral("ÖLÇÜ tur=dogrusal"),
                   tr("Yatay ya da düşey ölçü: çizgiyi üste çekmek yatay, yana çekmek düşey "
                      "ölçer")),
        methodTool(Glyph::DimAngular, tr("Açı Ölçüsü"), QStringLiteral("ÖLÇÜ tur=acisal"),
                   tr("Önce tepe, sonra iki kolun ucu; yay hangi açının içinden geçerse o "
                      "ölçülür")),
        methodTool(Glyph::DimArcLength, tr("Yay Uzunluğu Ölçüsü"), QStringLiteral("ÖLÇÜ tur=yay"),
                   tr("Yaya tıklayın: yayın boyu, kirişi değil")),
        methodTool(Glyph::DimRadius, tr("Yarıçap Ölçüsü"), QStringLiteral("ÖLÇÜ tur=yaricap"),
                   tr("Daireye ya da yaya tıklayın; çizgi yazıya doğru uzanır")),
        methodTool(Glyph::DimDiameter, tr("Çap Ölçüsü"), QStringLiteral("ÖLÇÜ tur=cap"),
                   tr("Daireye ya da yaya tıklayın; çap yazıya doğru döner")),
        methodTool(Glyph::DimOrdinate, tr("Koordinat Ölçüsü"), QStringLiteral("ÖLÇÜ tur=koordinat"),
                   tr("Başlangıç, ölçülecek nokta ve yazının yeri; yana çekmek sağa, yukarı "
                      "çekmek yukarı değerini okur")),
    };

    // THE SHORT WORDS a family's button wears while one of these is on its face.
    for (QAction* a : {actExtend_, actExtendFence_, actExtendCarry_})
        a->setProperty(kRibbonShortLabel, tr("Uzat"));
    for (QAction* a : {actChamfer_, actChamferAll_})
        a->setProperty(kRibbonShortLabel, tr("Pah"));

    // THE PROCESSING TOOLS a tab shows by name open in the Araçlar panel, where
    // their parameters are — a button that ran one bare would run it with every
    // default and no chance to say otherwise.
    const auto tool = [this](const char* id, const QString& word) {
        return processingAction(QString::fromLatin1(id), word);
    };
    QAction* numberCorners = tool("islem.kose_numarala", tr("Köşe Numarala"));
    QAction* writeLengths  = tool("islem.uzunluk_yaz", tr("Uzunluk Yaz"));
    QAction* bufferZone    = tool("islem.tampon", tr("Tampon"));
    QAction* makeAreas     = tool("islem.alan_uret", tr("Alan Üret"));
    QAction* editArea      = tool("islem.alan_duzenle", tr("Alanı Düzenle"));
    QAction* bindText      = tool("islem.bagla", tr("Bağla"));
    QAction* unbindText    = tool("islem.bag_coz", tr("Bağı Çöz"));

    loadRibbonCatalogues();

    // =================================================================== `Giriş`
    //
    // WHAT A DRAFTER DOES ALL DAY, on the tab that opens first — AutoCAD's Home:
    // draw, change, annotate, the layer and the colours in hand, the clipboard.
    SARibbonCategory* home = bar->addCategoryPage(tr("Giriş"));
    home->setObjectName(QStringLiteral("ribbonHome"));
    selectFirst(home);

    SARibbonPanel* sketch = home->addPanel(tr("Çizim"));
    large(sketch, actLine_);
    large(sketch, actPolyline_);
    family(sketch, {actCircle_, circleTwo, circleThree, circleTangent}, Size::Large, tr("Daire"));
    family(sketch, {actArc_, arcThree, arcAngle, arcRadius, arcOn}, Size::Large, tr("Yay"));
    family(sketch, {actRectangle_, rectangleRotated, actRegular_, regularOutside, regularSide},
           Size::Icon);
    family(sketch, {actPolygon_, actAnnulus_, actSector_}, Size::Icon);
    family(sketch, {actEllipse_, ellipseAxis}, Size::Icon);
    family(sketch, {actPoint_, actPerpOffset_, actSurvey_, actIntersect_, actAlong_}, Size::Icon);
    icon(sketch, actSpline_);
    family(sketch, {actHatch_, actHatchEdit_, actBoundary_}, Size::Icon);
    launcher(sketch, tr("Çizim ve yakalama ayarları"),
             [this] { openSettingsSection(QStringLiteral("Çizim ve Yakalama")); });

    // THE EDIT VERBS IN AUTOCAD'S GRID: three labelled columns and a fourth of
    // bare pictures, read down each column.
    SARibbonPanel* change = home->addPanel(tr("Değiştir"));
    small(change, actMove_);
    small(change, actCopy_);
    small(change, actStretch_);
    family(change, {actRotate_, actRotateRef_}, Size::Small, tr("Döndür"));
    family(change, {actMirror_, actMirrorCopy_}, Size::Small, tr("Aynala"));
    family(change, {actScale_, actScaleRef_}, Size::Small, tr("Ölçekle"));
    family(change,
           {actTrim_, actTrimFence_, actTrimKeep_, actTrimCarry_, actExtend_, actExtendFence_,
            actExtendCarry_},
           Size::Small, tr("Buda"));
    family(change, {actFillet_, actFilletAll_, actChamfer_, actChamferAll_}, Size::Small,
           tr("Yuvarla"));
    family(change, {actArray_, actArrayPolar_, actArrayPath_}, Size::Small, tr("Dizi"));
    icon(change, actErase_);
    icon(change, actExplode_);
    icon(change, actOffset_);

    SARibbonPanel* note = home->addPanel(tr("Açıklama"));
    family(note, {actText_, actTextEdit_}, Size::Large, tr("Metin"));
    family(note, dimensionTypes, Size::Large, tr("Ölçü"));
    small(note, actLeader_);
    small(note, actLabel_);
    small(note, actFindReplace_);
    launcher(note, tr("Ölçü stili, pafta ölçeği ve yazdırma ayarları"),
             [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); });

    // THE LAYER THE HAND IS ON, and the six things done to a layer from where
    // the drawing is: make the selection's layer the active one, move the
    // selection onto the active one, hide it, isolate it, show them all, lock it.
    SARibbonPanel* layers = home->addPanel(tr("Katmanlar"));
    auto* layersPanel     = new QAction(tr("Katmanlar"), this);
    layersPanel->setObjectName(QStringLiteral("ribbonLayersPanel"));
    layersPanel->setData(static_cast<int>(Glyph::LayerManager));
    layersPanel->setToolTip(tr("Katmanlar panelini açar: her katmanın görünürlüğü, kilidi, "
                               "rengi ve stili"));
    connect(layersPanel, &QAction::triggered, this, &MainWindow::showLayerPanel);
    large(layers, layersPanel);
    ribbonLive_->layer = new RibbonLayerBox(layers);
    ribbonLive_->layer->setFixedWidth(188);
    ribbonLive_->layer->setToolTip(
        tr("Seçim yokken: yeni nesnelerin çizileceği etkin katman (KATMAN ad=…).\n"
           "Seçim varken: seçilen nesnelerin katmanı; başka bir katman seçmek onları oraya "
           "taşır (KATMANAT)."));
    connect(ribbonLive_->layer, &RibbonLayerBox::layerPicked, this, &MainWindow::pickRibbonLayer);
    layers->addMediumWidget(ribbonLive_->layer);
    auto* layerStrip = new SARibbonButtonGroupWidget(layers);
    layerStrip->setObjectName(QStringLiteral("ribbonLayerStrip"));
    // The six done most, as pictures — make active, move to active, hide,
    // isolate, show all, lock; Görünüm has all eight with their names.
    const QList<QAction*> layerVerbs = layerActions();
    for (const int i : {0, 1, 2, 3, 4, 6})
        layerStrip->addAction(layerVerbs.at(i));
    layers->addMediumWidget(layerStrip);
    launcher(layers, tr("Katmanlar paneli"), [this] { showLayerPanel(); });

    // THE COLOURS IN HAND AND HOW TO BORROW A LOOK: the stroke and the fill of
    // the selection (or of the active layer), each opening RENK's swatches.
    SARibbonPanel* looks = home->addPanel(tr("Özellikler"));
    large(looks, actStyleCopy_);
    ribbonLive_->stroke = new RibbonColourBox(looks);
    ribbonLive_->stroke->setObjectName(QStringLiteral("ribbonStrokeBox"));
    ribbonLive_->stroke->setAccessibleName(tr("Çizgi rengi"));
    ribbonLive_->stroke->setFixedWidth(138);
    connect(ribbonLive_->stroke, &RibbonColourBox::menuRequested, this,
            [this] { openColourMenu(0); });
    looks->addMediumWidget(captioned(looks, tr("Çizgi"), ribbonLive_->stroke, 34));
    ribbonLive_->fill = new RibbonColourBox(looks);
    ribbonLive_->fill->setObjectName(QStringLiteral("ribbonFillBox"));
    ribbonLive_->fill->setAccessibleName(tr("Dolgu rengi"));
    ribbonLive_->fill->setFixedWidth(138);
    connect(ribbonLive_->fill, &RibbonColourBox::menuRequested, this,
            [this] { openColourMenu(1); });
    looks->addMediumWidget(captioned(looks, tr("Dolgu"), ribbonLive_->fill, 34));
    launcher(looks, tr("Stil Tasarımcısı — etkin katmanın bütün stili"),
             [this] { openStyleDesigner(QString()); });

    SARibbonPanel* clip = home->addPanel(tr("Pano"));
    large(clip, actPaste_);
    icon(clip, actCut_);
    icon(clip, actCopyClip_);
    icon(clip, actCopyBase_);

    // =================================================================== `Çizim`
    //
    // THE WHOLE OF DRAWING, one panel per kind of thing drawn — sized so the tab
    // fits a 1440 px window without scrolling, which is why the guides share a
    // button and the methods live under their shapes' arrows.
    SARibbonCategory* drawTab = bar->addCategoryPage(tr("Çizim"));
    drawTab->setObjectName(QStringLiteral("ribbonDraw"));
    selectFirst(drawTab);

    SARibbonPanel* lines = drawTab->addPanel(tr("Çizgi"));
    large(lines, actLine_);
    large(lines, actPolyline_);
    small(lines, actSpline_);
    family(lines, {guide, actAngledGuide_}, Size::Small, tr("Kılavuz"));

    SARibbonPanel* shapes = drawTab->addPanel(tr("Şekil"));
    family(shapes, {actCircle_, circleTwo, circleThree, circleTangent}, Size::Large, tr("Daire"));
    family(shapes, {actArc_, arcThree, arcAngle, arcRadius, arcOn}, Size::Large, tr("Yay"));
    large(shapes, actPolygon_);
    family(shapes, {actRectangle_, rectangleRotated}, Size::Small, tr("Dikdörtgen"));
    family(shapes, {actRegular_, regularOutside, regularSide}, Size::Small, tr("Çokgen"));
    family(shapes, {actEllipse_, ellipseAxis}, Size::Small, tr("Elips"));
    small(shapes, actSector_);
    small(shapes, actAnnulus_);

    // THE SURVEY ENTRIES, where a drawing actually starts for a crew with a
    // tape: this is the first tool a Turkish surveyor reaches for, not an
    // occasional one (TODOS-CAD P1b). POLİGON is Harita's, with the geodesy.
    SARibbonPanel* points = drawTab->addPanel(tr("Nokta ve Alım"));
    large(points, actPoint_);
    large(points, actSurvey_);
    small(points, actPerpOffset_);
    family(points, {actIntersect_, crossDistances, crossLines}, Size::Small, tr("Kesişim"));
    family(points, {actAlong_, alongDistance}, Size::Small, tr("Ara Nokta"));

    // THE PATTERNS AS THEY LOOK, from the catalogue: a click starts TARAMA with
    // that pattern and asks for the boundary; the Tarama button itself uses the
    // last one given.
    SARibbonPanel* fills = drawTab->addPanel(tr("Tarama"));
    large(fills, actHatch_);
    if (!ribbonLive_->patterns.empty()) {
        SARibbonGallery* gallery = fills->addGallery(false);
        gallery->setObjectName(QStringLiteral("ribbonHatchGallery"));
        for (const command::HatchPattern& pattern : ribbonLive_->patterns) {
            const QString id = QString::fromStdString(pattern.id);
            auto* a          = new QAction(id, this);
            a->setProperty(kPatternProperty, id);
            a->setToolTip(tr("%1 — %2").arg(id, QString::fromStdString(pattern.description)));
            connect(a, &QAction::triggered, this, [this, id] {
                controller_->runCommand(QStringLiteral("TARAMA desen=%1").arg(id));
            });
            ribbonLive_->drawPatterns << a;
        }
        SARibbonGalleryGroup* group =
            gallery->addCategoryActions(tr("Desenler"), ribbonLive_->drawPatterns);
        group->setGalleryGroupStyle(SARibbonGalleryGroup::IconWithText);
        group->setDisplayRow(SARibbonGalleryGroup::DisplayOneRow);
        group->setGridMinimumWidth(58);
        group->setGridMaximumWidth(58);
        gallery->setCurrentViewGroup(group);
        // Four patterns show; the arrow under the scroll buttons opens them all.
        gallery->setFixedWidth(4 * 58 + 20);
    }
    small(fills, actHatchEdit_);
    small(fills, actBoundary_);

    SARibbonPanel* blocks = drawTab->addPanel(tr("Blok"));
    large(blocks, actInsert_);
    small(blocks, actBlock_);
    small(blocks, actExplode_);

    // ================================================================ `Değiştir`
    SARibbonCategory* modifyTab = bar->addCategoryPage(tr("Değiştir"));
    modifyTab->setObjectName(QStringLiteral("ribbonModify"));
    selectFirst(modifyTab);

    SARibbonPanel* moves = modifyTab->addPanel(tr("Dönüştür"));
    large(moves, actMove_);
    large(moves, actCopy_);
    family(moves, {actRotate_, actRotateRef_}, Size::Small, tr("Döndür"));
    family(moves, {actScale_, actScaleRef_}, Size::Small, tr("Ölçekle"));
    family(moves, {actMirror_, actMirrorCopy_}, Size::Small, tr("Aynala"));
    family(moves, {actAlign_, actAlignScaled_}, Size::Small, tr("Hizala"));
    small(moves, actStretch_);

    SARibbonPanel* arrays = modifyTab->addPanel(tr("Dizi ve Ofset"));
    family(arrays, {actArray_, actArrayPolar_, actArrayPath_}, Size::Large, tr("Dizi"));
    large(arrays, actOffset_);

    SARibbonPanel* cuts = modifyTab->addPanel(tr("Kes ve Uzat"));
    family(cuts, {actTrim_, actTrimFence_, actTrimKeep_, actTrimCarry_}, Size::Large, tr("Buda"));
    family(cuts, {actExtend_, actExtendFence_, actExtendCarry_}, Size::Large, tr("Uzat"));
    small(cuts, actBreak_);
    small(cuts, actLengthen_);
    family(cuts, {actSplit_, actSplitPoint_, actSplitCross_, actSplitEqual_, actSplitDistance_},
           Size::Small, tr("Böl"));
    small(cuts, actDivide_);

    SARibbonPanel* corners = modifyTab->addPanel(tr("Köşe"));
    family(corners, {actFillet_, actFilletAll_}, Size::Large, tr("Yuvarla"));
    family(corners, {actChamfer_, actChamferAll_}, Size::Large, tr("Pah"));
    small(corners, actVertexMove_);
    small(corners, actVertexAdd_);
    small(corners, actVertexDelete_);
    small(corners, actEdgeKind_);
    small(corners, actPolylineEdit_);
    small(corners, editArea);

    SARibbonPanel* joins = modifyTab->addPanel(tr("Birleştir"));
    large(joins, actCombine_);
    small(joins, actJoin_);
    small(joins, actToArea_);
    small(joins, actExplode_);

    SARibbonPanel* tidy = modifyTab->addPanel(tr("Sil ve Temizle"));
    large(tidy, actErase_);
    small(tidy, commandAction(Glyph::Cleanup, tr("Temizle — bul"), QStringLiteral("TEMİZLE"),
                              tr("TEMİZLE — yinelenen, boş ve tekrarlanan köşeli nesneleri "
                                 "bulur, seçer ve işaretler; hiçbir şeyi değiştirmez  ·  "
                                 "kısaltma: TMZ")));
    small(tidy,
          commandAction(Glyph::Cleanup, tr("Temizle — onar"), QStringLiteral("TEMİZLE islem=onar"),
                        tr("TEMİZLE islem=onar — yinelenenleri ve boş nesneleri siler, "
                           "tekrarlanan köşeleri çıkarır; değişen alanları önce/sonra "
                           "söyler, tek adımda geri alınır")));

    // =============================================================== `Açıklama`
    SARibbonCategory* annotateTab = bar->addCategoryPage(tr("Açıklama"));
    annotateTab->setObjectName(QStringLiteral("ribbonAnnotate"));
    selectFirst(annotateTab);

    // THE DEFAULTS the annotation commands fall back to, in the ribbon where
    // AutoCAD keeps its text and dimension styles: the height a new METİN gets
    // and the style a new ÖLÇÜ is drawn in. Each is a project setting (AYAR).
    SARibbonPanel* words = annotateTab->addPanel(tr("Yazı"));
    large(words, actText_);
    small(words, actTextEdit_);
    small(words, actFindReplace_);
    ribbonLive_->textHeightDefault = new ComboBox(words);
    ribbonLive_->textHeightDefault->setObjectName(QStringLiteral("ribbonTextHeightDefault"));
    ribbonLive_->textHeightDefault->setControlSize(ControlSize::Compact);
    ribbonLive_->textHeightDefault->setEditable(true);
    ribbonLive_->textHeightDefault->setInsertPolicy(QComboBox::NoInsert);
    ribbonLive_->textHeightDefault->setFixedWidth(96);
    ribbonLive_->textHeightDefault->setAccessibleName(tr("Varsayılan yazı yüksekliği"));
    ribbonLive_->textHeightDefault->setToolTip(
        tr("Yeni bir METİN'in yüksekliği, zeminde metre (AYAR metin_yüksekliği)"));
    for (const double m : {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 5.0, 7.0, 10.0})
        ribbonLive_->textHeightDefault->addItem(tr("%1 m").arg(metresText(m)), m);
    connectMetres(ribbonLive_->textHeightDefault, [this](double metres) {
        controller_->runLine(
            QStringLiteral("AYAR ad=metin_yüksekliği deger=%1").arg(std::llround(metres * 1000.0)),
            command::Origin::Gui);
    });
    words->addSmallWidget(captioned(words, tr("Yükseklik"), ribbonLive_->textHeightDefault, 58));

    SARibbonPanel* measures = annotateTab->addPanel(tr("Ölçü"));
    family(measures, dimensionTypes, Size::Large, tr("Ölçü"));
    small(measures, actDimChain_);
    small(measures, actDimBaseline_);
    small(measures, actDimensionEdit_);
    ribbonLive_->dimStyleDefault = new ComboBox(measures);
    ribbonLive_->dimStyleDefault->setObjectName(QStringLiteral("ribbonDimStyleDefault"));
    ribbonLive_->dimStyleDefault->setControlSize(ControlSize::Compact);
    ribbonLive_->dimStyleDefault->setFixedWidth(128);
    ribbonLive_->dimStyleDefault->setAccessibleName(tr("Varsayılan ölçü stili"));
    ribbonLive_->dimStyleDefault->setToolTip(
        tr("ÖLÇÜ, ZİNCİRÖLÇÜ, BAZÖLÇÜ ve LİDER'in stil= verilmediğinde kullandığı stil "
           "(AYAR ölçü_stili)"));
    for (const command::DimensionStyle& style : ribbonLive_->dimStyles) {
        ribbonLive_->dimStyleDefault->addItem(QString::fromStdString(style.id));
        ribbonLive_->dimStyleDefault->setItemData(ribbonLive_->dimStyleDefault->count() - 1,
                                                  QString::fromStdString(style.description),
                                                  Qt::ToolTipRole);
    }
    connect(ribbonLive_->dimStyleDefault, &QComboBox::activated, this, [this](int index) {
        controller_->runLine(QStringLiteral("AYAR ad=ölçü_stili deger=%1")
                                 .arg(ribbonLive_->dimStyleDefault->itemText(index)),
                             command::Origin::Gui);
    });
    measures->addSmallWidget(captioned(measures, tr("Stil"), ribbonLive_->dimStyleDefault, 28));
    small(measures, commandAction(Glyph::DimStyle, tr("Ölçü Stilleri"), QStringLiteral("ÖLÇÜSTİLİ"),
                                  tr("ÖLÇÜSTİLİ — ölçü stillerini kâğıttaki ve bu paftadaki "
                                     "boylarıyla listeler; varsayılanı AYAR ölçü_stili "
                                     "değiştirir  ·  kısaltma: ÖST")));
    small(measures, commandAction(Glyph::DimRefresh, tr("Pafta Ölçeğine Uyarla"),
                                  QStringLiteral("ÖLÇÜYENİLE"),
                                  tr("ÖLÇÜYENİLE — çizimin bütün ölçülerini plan ölçeğine "
                                     "uyarlar: oklar, uzatma çizgileri ve yazılar kâğıtta "
                                     "stilin boyunda kalır; yazılar çizimin birimiyle yeniden "
                                     "yazılır  ·  kısaltma: ÖYN")));
    launcher(measures, tr("Ölçü stili ve pafta ölçeği ayarları"),
             [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); });

    SARibbonPanel* tags = annotateTab->addPanel(tr("Etiket"));
    large(tags, actLabel_);
    large(tags, actLeader_);
    small(tags, bindText);
    small(tags, unbindText);
    small(tags, writeLengths);

    // ================================================================ `Kadastro`
    SARibbonCategory* cadastreTab = bar->addCategoryPage(tr("Kadastro"));
    cadastreTab->setObjectName(QStringLiteral("ribbonCadastre"));
    selectFirst(cadastreTab);

    SARibbonPanel* parcels = cadastreTab->addPanel(tr("Parsel"));
    large(parcels, actParcelSplit_);
    large(parcels, actAreaSplit_);
    large(parcels, actUnion_);

    SARibbonPanel* marks = cadastreTab->addPanel(tr("Yazım"));
    large(marks, actLabel_);
    small(marks, numberCorners);
    small(marks, writeLengths);
    small(marks, makeAreas);

    SARibbonPanel* checks = cadastreTab->addPanel(tr("Denetim"));
    large(checks, actTopology_);
    small(checks, actMeasureArea_);
    small(checks, bufferZone);

    // ================================================================== `Harita`
    SARibbonCategory* mapTab = bar->addCategoryPage(tr("Harita"));
    mapTab->setObjectName(QStringLiteral("ribbonMap"));
    selectFirst(mapTab);

    SARibbonPanel* ask = mapTab->addPanel(tr("Sorgu"));
    large(ask, actIdentify_);
    small(ask, actEntityInfo_);
    small(ask, actCoordinate_);

    SARibbonPanel* tape = mapTab->addPanel(tr("Ölçüm"));
    large(tape, actMeasure_);
    family(tape, {actMeasureArea_, areaByCorners}, Size::Small, tr("Alan Ölç"));
    small(tape, actMeasureAngle_);

    SARibbonPanel* geodesy = mapTab->addPanel(tr("Jeodezi"));
    large(geodesy, actTraverse_);
    small(geodesy, actStakeout_);
    small(geodesy, commandAction(Glyph::Helmert, tr("Oturt (Helmert)"), QStringLiteral("OTURT"),
                                 tr("OTURT — ortak noktalardan Helmert dönüşümüyle çizimi "
                                    "oturtur  ·  kısaltma: OTR")));
    small(geodesy, commandAction(Glyph::Globe, tr("Dönüştür"), QStringLiteral("DÖNÜŞTÜR"),
                                 tr("DÖNÜŞTÜR — çizimi başka bir koordinat sistemine "
                                    "dönüştürür  ·  kısaltma: DNS")));
    launcher(geodesy, tr("Koordinat sistemi ayarları"),
             [this] { openSettingsSection(QStringLiteral("Koordinat Sistemleri")); });

    SARibbonPanel* ground = mapTab->addPanel(tr("Arazi"));
    large(ground, commandAction(Glyph::Contour, tr("Eşyükselti"), QStringLiteral("EŞYÜKSELTİ"),
                                tr("EŞYÜKSELTİ — kotlu noktalardan eş yükselti eğrileri çizer  ·  "
                                   "kısaltma: EŞY")));
    large(ground, commandAction(Glyph::Volume, tr("Hacim"), QStringLiteral("HACİM"),
                                tr("HACİM — iki yüzey arasındaki kazı ve dolgu hacmi  ·  "
                                   "kısaltma: HCM")));

    SARibbonPanel* sources = mapTab->addPanel(tr("Veri"));
    large(sources, actDatabase_);
    small(sources, actImport_);
    small(sources, actExport_);

    // ================================================================== `Analiz`
    SARibbonCategory* analyseTab = bar->addCategoryPage(tr("Analiz"));
    analyseTab->setObjectName(QStringLiteral("ribbonAnalyse"));
    selectFirst(analyseTab);

    SARibbonPanel* tables = analyseTab->addPanel(tr("Tablo"));
    large(tables, actTable_);

    // THE PROCESSING TOOLS, one row each from the processing registry — the
    // list `src/processing/src/registry.cpp` declares, never a second one.
    SARibbonPanel* process = analyseTab->addPanel(tr("İşlem araçları"));
    auto* tools            = new QMenu(tr("İşlem Araçları"), this);
    tools->setObjectName(QStringLiteral("processingMenu"));
    for (const processing::ProcessingTool* each : processing::processing_tools()) {
        const auto& spec = each->spec();
        tools->addAction(processingAction(QString::fromStdString(spec.id), QString()));
    }
    menuButton(process, tools, Glyph::Function, true);
    auto* showTools = new QAction(tr("Araçlar Paneli"), this);
    showTools->setData(static_cast<int>(Glyph::Tune));
    showTools->setStatusTip(tr("Sağ paneldeki Araçlar sekmesini açar"));
    connect(showTools, &QAction::triggered, this, [this] { showToolsPanel(QString()); });
    process->addSmallAction(showTools);
    small(process, bufferZone);
    small(process, makeAreas);

    SARibbonPanel* agents = analyseTab->addPanel(tr("Yapay zekâ"));
    large(agents, actAi_);
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
    small(agents, actMcp_);
    auto* mcpToken = new QAction(tr("MCP Belirteci Üret"), this);
    mcpToken->setData(static_cast<int>(Glyph::Lock));
    mcpToken->setStatusTip(tr("Yeni bir erişim belirteci üretir; eskisi geçersiz olur"));
    connect(mcpToken, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("MCPSUNUCU islem=belirtec"), command::Origin::Gui);
    });
    agents->addSmallAction(mcpToken);
    launcher(agents, tr("Yapay zeka modelleri"),
             [this] { openSettingsSection(QStringLiteral("Yapay Zeka Modelleri")); });

    // ================================================================= `Görünüm`
    SARibbonCategory* viewTab = bar->addCategoryPage(tr("Görünüm"));
    viewTab->setObjectName(QStringLiteral("ribbonView"));
    selectFirst(viewTab);

    SARibbonPanel* navigate = viewTab->addPanel(tr("Gezinme"));
    large(navigate, actZoomExtents_);
    small(navigate, actZoomIn_);
    small(navigate, actZoomOut_);
    small(navigate, actPan_);

    SARibbonPanel* aids = viewTab->addPanel(tr("Yardımcılar"));
    large(aids, actSnap_);
    // The keyboard road to the same list the OSNAP chip's right click opens.
    // ui.md P7: nothing ships reachable only by mouse.
    auto* snapModes = new QAction(tr("Yakalama Modları…"), this);
    snapModes->setData(static_cast<int>(Glyph::Snap));
    snapModes->setToolTip(tr("Hangi nesne yakalama modlarının açık olduğunu seçer"));
    snapModes->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(snapModes, &QAction::triggered, this, &MainWindow::openSnapModes);
    addAction(snapModes); // so the shortcut works with focus anywhere in the shell
    aids->addSmallAction(snapModes);
    small(aids, actOrtho_);
    small(aids, actNormal_);
    small(aids, actGridSnap_);
    launcher(aids, tr("Yakalama modları"), [this] { openSnapModes(); });

    // WHAT IS SEEN, not what is edited: the verbs that change which layers
    // show. Making one active, moving objects onto it and locking it are
    // `Giriş`'s, beside the layer list.
    SARibbonPanel* layerView = viewTab->addPanel(tr("Katmanlar"));
    large(layerView, layersPanel);
    for (const int i : {2, 3, 4, 5})
        small(layerView, layerVerbs.at(i));
    small(layerView, actStyle_);

    SARibbonPanel* windows                       = viewTab->addPanel(tr("Pencereler"));
    const std::pair<QDockWidget*, Glyph> docks[] = {{layerDock_, Glyph::Layer},
                                                    {propertyDock_, Glyph::Table},
                                                    {chatDock_, Glyph::Chat},
                                                    {journalDock_, Glyph::History},
                                                    {pythonDock_, Glyph::Script}};
    for (const auto& [dock, glyph] : docks)
        if (dock != nullptr) {
            dock->toggleViewAction()->setData(static_cast<int>(glyph));
            windows->addSmallAction(dock->toggleViewAction());
        }
    windows->addSmallAction(actCommandLine_);
    auto* reset = new QAction(tr("Yerleşimi Sıfırla"), this);
    reset->setData(static_cast<int>(Glyph::Refresh));
    connect(reset, &QAction::triggered, this, &MainWindow::resetLayout);
    windows->addSmallAction(reset);

    SARibbonPanel* look = viewTab->addPanel(tr("Tema"));
    large(look, actTheme_);
    small(look, actHud_);
    launcher(look, tr("Görünüm ve tema ayarları"),
             [this] { openSettingsSection(QStringLiteral("Görünüm ve Tema")); });

    // =================================================================== `Çıktı`
    SARibbonCategory* outputTab = bar->addCategoryPage(tr("Çıktı"));
    outputTab->setObjectName(QStringLiteral("ribbonOutput"));
    selectFirst(outputTab);

    // YAZDIR AND ITS PROFILES, one split button: the face prints with the
    // profile in use, the arrow lists the profiles and the sheets. The list is
    // REBUILT EVERY TIME IT OPENS — a layout made on the command line, from a
    // script, from a template or over MCP is in it without anybody being told.
    SARibbonPanel* sheets = outputTab->addPanel(tr("Yazdır"));
    printMenu_            = new QMenu(this);
    printMenu_->setObjectName(QStringLiteral("printMenu"));
    connect(printMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildPrintMenu);
    rebuildPrintMenu();
    connect(&controller_->printService(), &PrintService::profilesChanged, this,
            &MainWindow::rebuildPrintMenu);
    auto* printHead = new QAction(actPrint_->text(), this);
    printHead->setObjectName(QStringLiteral("ribbonPrint"));
    printHead->setData(actPrint_->data());
    printHead->setToolTip(actPrint_->toolTip());
    printHead->setProperty(kToolCommand, QStringLiteral("YAZDIR"));
    printHead->setMenu(printMenu_);
    connect(printHead, &QAction::triggered, actPrint_, &QAction::trigger);
    sheets->addLargeAction(printHead, QToolButton::MenuButtonPopup);
    menuButton(sheets, layoutMenu_, Glyph::Layout, true);
    // THE PLOT SCALE, where a sheet is set up: the denominator every paper
    // measure is multiplied by (`AYAR plan_ölçeği`).
    ribbonLive_->plotScale = new ComboBox(sheets);
    ribbonLive_->plotScale->setObjectName(QStringLiteral("ribbonPlotScale"));
    ribbonLive_->plotScale->setControlSize(ControlSize::Compact);
    ribbonLive_->plotScale->setEditable(true);
    ribbonLive_->plotScale->setInsertPolicy(QComboBox::NoInsert);
    ribbonLive_->plotScale->setFixedWidth(104);
    ribbonLive_->plotScale->setAccessibleName(tr("Pafta ölçeği"));
    ribbonLive_->plotScale->setToolTip(
        tr("Paftanın ölçeği: kâğıtta bildirilen her boyun zeminde ne kadar yer tuttuğu "
           "(AYAR plan_ölçeği)"));
    for (const int den : {500, 1000, 2000, 5000, 10000, 25000})
        ribbonLive_->plotScale->addItem(QStringLiteral("1:%1").arg(den), den);
    const auto takeScale = [this] {
        if (const auto den = typedScale(ribbonLive_->plotScale->currentText()); den)
            controller_->runLine(QStringLiteral("AYAR ad=plan_ölçeği deger=%1").arg(*den),
                                 command::Origin::Gui);
        else
            refreshRibbon();
    };
    connect(ribbonLive_->plotScale, &QComboBox::activated, this, takeScale);
    connect(ribbonLive_->plotScale->lineEdit(), &QLineEdit::returnPressed, this, takeScale);
    sheets->addSmallWidget(captioned(sheets, tr("Ölçek"), ribbonLive_->plotScale, 40));
    launcher(sheets, tr("Yazdırma profilleri"),
             [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); });

    SARibbonPanel* files = outputTab->addPanel(tr("Dosya"));
    large(files, actExport_);
    small(files, actSave_);
    small(files, actSaveAs_);

    buildContextTabs(bar);

    // ---- AND EVERYTHING ELSE THE REGISTRY KNOWS ----------------------------
    //
    // Last, because it reads what every curated button above already offers.
    // A domain's commands go to the tab of their workflow before a category can
    // claim them: an ifraz is a Kadastro act whatever category it declares.
    const auto rest = [&](SARibbonCategory* tab, const QList<QAction*>& leftovers) {
        if (leftovers.isEmpty()) return;
        auto* menu = new QMenu(tr("Diğer"), this);
        menu->setObjectName(QStringLiteral("ribbonLeftovers.") + tab->objectName());
        menu->addActions(leftovers);
        SARibbonPanel* panel = tab->addPanel(tr("Diğer komutlar"));
        menuButton(panel, menu, Glyph::ChevronDown, true);
    };
    rest(cadastreTab, ribbonLeftovers({}, {"cadastre.", "planning."}));
    rest(mapTab, ribbonLeftovers({}, {"geodesy.", "surface."}));
    rest(drawTab, ribbonLeftovers({command::Category::Draw}, {}));
    rest(modifyTab, ribbonLeftovers({command::Category::Modify}, {}));
    rest(mapTab, ribbonLeftovers({command::Category::Query}, {}));
    rest(analyseTab, ribbonLeftovers({command::Category::Processing}, {}));
    rest(viewTab, ribbonLeftovers({command::Category::View, command::Category::Layer}, {}));
    appRest->addActions(ribbonLeftovers(
        {command::Category::File, command::Category::Script, command::Category::System}, {}));

    // THE APPLICATION MENU'S VERBS, now that every action exists. Each line under
    // a name is what the verb does, in the words its own tip says it with.
    // The line is the menu's own, short enough to read at a glance: a verb's
    // tip is written for the pointer's pause and runs to three clauses.
    QVector<ApplicationMenu::Entry> verbs;
    const auto verb = [&](QAction* action, const QString& line, bool group = false,
                          std::function<QList<QAction*>()> choices = {}) {
        verbs.push_back({action, line, std::move(choices), group});
    };
    verb(actNew_, tr("Boş bir çizim açar"));
    verb(actOpen_, tr("Bir proje dosyası açar"));
    verb(actSave_, tr("Çizimi dosyasına yazar"));
    verb(actSaveAs_, tr("Çizimi yeni bir dosyaya yazar"));
    verb(actImport_, tr("Dış bir veri dosyasını çizime ekler"), true);
    verb(actExport_, tr("Çizimi başka bir biçime yazar"));
    verb(actPrint_, tr("Paftayı bir yazdırma profiliyle basar"), false, [this] {
        rebuildPrintMenu();
        return printMenu_->actions();
    });
    // ÇIKTI YERLEŞİMLERİ, where a QGIS user looks for layouts: UNDER THE FILE
    // COMMANDS, because a layout belongs to the DOCUMENT — it is saved in the
    // file, it is in the content hash, it is part of what gets signed. QGIS puts
    // its layouts under `Project` for the same reason. The list is rebuilt
    // whenever it is shown, so a sheet added at the command line is in it.
    layoutMenu_->menuAction()->setData(static_cast<int>(Glyph::Layout));
    layoutMenu_->menuAction()->setToolTip(tr("Çizimin başlıklı, lejantlı pafta düzenleri"));
    verb(layoutMenu_->menuAction(), tr("Çizimin pafta düzenleri"), false, [this] {
        rebuildLayoutMenu();
        return layoutMenu_->actions();
    });
    verb(actProjectSettings_, tr("Çizimle birlikte giden ayarlar"), true);
    verb(actDatabase_, tr("PostGIS sunucusuna bağlanır"));
    verb(actScript_, tr("Bir betiği komut yolundan çalıştırır"));
    if (!appRest->isEmpty()) {
        appRest->menuAction()->setToolTip(tr("Şeritte yeri olmayan komutlar"));
        verb(appRest->menuAction(), tr("Şeritte yeri olmayan komutlar"), true,
             [appRest] { return appRest->actions(); });
    }
    appMenu_->setEntries(verbs);

    // EVERY SHORTCUT BELONGS TO THE WINDOW. A shortcut lives only while a widget
    // carrying its action is visible, and a ribbon button is visible only while
    // its tab is up: Ctrl+H (⌥⌘F on a Mac) died the moment another tab was
    // chosen. On the window, each works from every tab, the canvas included.
    for (QAction* action : findChildren<QAction*>())
        if (!action->shortcut().isEmpty()) addAction(action);

    bar->setCurrentIndex(0);
}

void MainWindow::openApplicationMenu()
{
    if (appMenu_ == nullptr || appButton_ == nullptr) return;
    appMenu_->applyTheme(theme_);
    appMenu_->setRecent(recentDocuments());
    appMenu_->popupUnder(appButton_);
}

QVector<ApplicationMenu::Recent> MainWindow::recentDocuments() const
{
    QVector<ApplicationMenu::Recent> out;
    const QSettings file;
    const QStringList paths = file.value(QStringLiteral("files/recent")).toStringList();
    const QStringList times = file.value(QStringLiteral("files/recentTimes")).toStringList();
    const QDateTime now     = QDateTime::currentDateTime();
    const QString home      = QDir::homePath();
    for (qsizetype i = 0; i < paths.size(); ++i) {
        const QFileInfo info(paths[i]);
        if (!info.exists()) continue;
        QString folder = QDir::toNativeSeparators(info.absolutePath());
        if (folder.startsWith(QDir::toNativeSeparators(home)))
            folder = QStringLiteral("~") + folder.mid(QDir::toNativeSeparators(home).size());
        QString when;
        if (const QDateTime at = QDateTime::fromString(times.value(i), Qt::ISODate); at.isValid()) {
            const qint64 minutes = at.secsTo(now) / 60;
            if (minutes < 1)
                when = tr("şimdi");
            else if (minutes < 60)
                when = tr("%n dk önce", nullptr, static_cast<int>(minutes));
            else if (minutes < qint64{24} * 60)
                when = tr("%n saat önce", nullptr, static_cast<int>(minutes / 60));
            else if (at.daysTo(now) == 1)
                when = tr("dün");
            else
                when = QLocale(QLocale::Turkish, QLocale::Turkey)
                           .toString(at.date(), QLocale::ShortFormat);
        }
        out.push_back({paths[i], info.fileName(), folder, when});
    }
    return out;
}

void MainWindow::noteRecentFile(const QString& path)
{
    // A PROBE MUST NOT WRITE the person's own list (the reason `closeEvent`
    // does not save the layout during one).
    if (path.isEmpty() || path == lastRecent_ || isProbeRun()) return;
    lastRecent_ = QFileInfo(path).absoluteFilePath();
    QSettings file;
    QStringList paths = file.value(QStringLiteral("files/recent")).toStringList();
    QStringList times = file.value(QStringLiteral("files/recentTimes")).toStringList();
    while (times.size() < paths.size())
        times << QString();
    if (const qsizetype at = paths.indexOf(lastRecent_); at >= 0) {
        paths.removeAt(at);
        times.removeAt(at);
    }
    paths.prepend(lastRecent_);
    times.prepend(QDateTime::currentDateTime().toString(Qt::ISODate));
    // As many as `core.dosya.son_dosya_sayisi` keeps.
    const auto keep = static_cast<qsizetype>(
        controller_->bus().app_settings().get("core.dosya.son_dosya_sayisi").as_int());
    while (paths.size() > keep) {
        paths.removeLast();
        times.removeLast();
    }
    file.setValue(QStringLiteral("files/recent"), paths);
    file.setValue(QStringLiteral("files/recentTimes"), times);
}

// =============================================================================
// The editor tabs a selection brings up
// =============================================================================

void MainWindow::buildContextTabs(SARibbonBar* bar)
{
    const Tokens& t = tokensFor(theme_);
    // Whatever these run, they run on THE SELECTION — the commands' own default
    // when `nesneler=` is not given — so a box here is the line a user would type.
    const auto onSelection = [this](const QString& line) {
        controller_->runLine(line, command::Origin::Gui);
    };
    const auto launcher = [this](SARibbonPanel* panel, const QString& tip, auto open) {
        auto* action = new QAction(tip, this);
        action->setObjectName(QStringLiteral("ribbonLauncher.") + panel->panelName());
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, open);
        panel->setOptionAction(action);
    };
    const auto closer = [this](SARibbonCategory* page) {
        SARibbonPanel* panel = page->addPanel(tr("Kapat"));
        auto* close          = new QAction(tr("Seçimi Bırak"), this);
        close->setObjectName(QStringLiteral("ribbonContextClose"));
        close->setData(static_cast<int>(Glyph::Close));
        close->setToolTip(tr("Seçimi boşaltır; bu sekme de kapanır (Esc)"));
        close->setProperty(kToolCommandProperty, QStringLiteral("SEÇ"));
        connect(close, &QAction::triggered, actSelectNone_, &QAction::trigger);
        panel->addLargeAction(close);
    };
    const auto context = [&](RibbonContext which, const QString& group, const QString& title,
                             const QColor& colour) {
        SARibbonContextCategory* ctx =
            bar->addContextCategory(group, colour, static_cast<int>(which));
        ribbonLive_->contexts[static_cast<std::size_t>(which)] = ctx;
        SARibbonCategory* page                                 = ctx->addCategoryPage(title);
        page->setObjectName(QStringLiteral("ribbonContext.%1").arg(static_cast<int>(which)));
        if (selectFirst_) selectFirst_(page);
        return page;
    };

    // ---------------------------------------------------------------- `Yazı`
    SARibbonCategory* text =
        context(RibbonContext::Text, tr("Yazı Araçları"), tr("Yazı"), t.accent);
    SARibbonPanel* textEdit = text->addPanel(tr("Düzenle"));
    textEdit->addLargeAction(actTextEdit_);
    textEdit->addSmallAction(actFindReplace_);
    textEdit->addSmallAction(actStyleCopy_);

    SARibbonPanel* textLook = text->addPanel(tr("Biçim"));
    ribbonLive_->textHeight = new ComboBox(textLook);
    ribbonLive_->textHeight->setObjectName(QStringLiteral("ribbonTextHeight"));
    ribbonLive_->textHeight->setControlSize(ControlSize::Compact);
    ribbonLive_->textHeight->setEditable(true);
    ribbonLive_->textHeight->setInsertPolicy(QComboBox::NoInsert);
    ribbonLive_->textHeight->setFixedWidth(96);
    ribbonLive_->textHeight->setAccessibleName(tr("Yazı yüksekliği"));
    for (const double m : {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 5.0, 7.0, 10.0})
        ribbonLive_->textHeight->addItem(tr("%1 m").arg(metresText(m)), m);
    connectMetres(ribbonLive_->textHeight, [onSelection](double metres) {
        onSelection(QStringLiteral("YAZIDÜZENLE yukseklik=%1").arg(std::llround(metres * 1000.0)));
    });
    textLook->addSmallWidget(captioned(textLook, tr("Yükseklik"), ribbonLive_->textHeight, 58));
    ribbonLive_->textSpacing = new ComboBox(textLook);
    ribbonLive_->textSpacing->setObjectName(QStringLiteral("ribbonTextSpacing"));
    ribbonLive_->textSpacing->setControlSize(ControlSize::Compact);
    ribbonLive_->textSpacing->setFixedWidth(96);
    ribbonLive_->textSpacing->setAccessibleName(tr("Satır aralığı"));
    for (const double k : {1.0, 1.15, 1.5, 2.0, 2.5, 3.0})
        ribbonLive_->textSpacing->addItem(metresText(k), k);
    connect(ribbonLive_->textSpacing, &QComboBox::activated, this, [this, onSelection](int i) {
        onSelection(QStringLiteral("YAZIDÜZENLE satir_araligi=%1")
                        .arg(ribbonLive_->textSpacing->itemData(i).toDouble(), 0, 'f', 2));
    });
    textLook->addSmallWidget(captioned(textLook, tr("Aralık"), ribbonLive_->textSpacing, 58));

    // THE NINE ANCHORS AS A 3 × 3 GRID of pictures, the way a word processor
    // shows a cell's alignment: where the point sits on the text.
    SARibbonPanel* textAnchor = text->addPanel(tr("Hizalama"));
    auto* anchors             = new QActionGroup(this);
    anchors->setExclusive(true);
    // Rows top to bottom, each left to right, in `core::TextAnchor` values.
    constexpr std::array<std::array<core::TextAnchor, 3>, 3> kGrid = {{
        {core::TextAnchor::TopLeft, core::TextAnchor::TopCentre, core::TextAnchor::TopRight},
        {core::TextAnchor::MiddleLeft, core::TextAnchor::MiddleCentre,
         core::TextAnchor::MiddleRight},
        {core::TextAnchor::BaselineLeft, core::TextAnchor::BaselineCentre,
         core::TextAnchor::BaselineRight},
    }};
    ribbonLive_->textAnchors.clear();
    for (std::uint8_t a = 0; a < core::kTextAnchorCount; ++a)
        ribbonLive_->textAnchors << nullptr;
    static const char* const kAnchorNames[3][3] = {
        {"Üst sol", "Üst orta", "Üst sağ"},
        {"Orta sol", "Merkez", "Orta sağ"},
        {"Taban sol", "Taban orta", "Taban sağ"},
    };
    for (int row = 0; row < 3; ++row) {
        auto* strip = new SARibbonButtonGroupWidget(textAnchor);
        strip->setObjectName(QStringLiteral("ribbonAnchorRow%1").arg(row));
        for (int col = 0; col < 3; ++col) {
            const core::TextAnchor anchor =
                kGrid[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            const QString word = QString::fromLatin1(core::text_anchor_name(anchor));
            auto* a            = new QAction(tr(kAnchorNames[row][col]), this);
            a->setCheckable(true);
            a->setObjectName(QStringLiteral("ribbonAnchor.") + word);
            a->setToolTip(tr("%1 — YAZIDÜZENLE hizalama=%2").arg(tr(kAnchorNames[row][col]), word));
            a->setProperty("kentos.anchor.col", col);
            a->setProperty("kentos.anchor.row", row);
            anchors->addAction(a);
            connect(a, &QAction::triggered, this, [onSelection, word] {
                onSelection(QStringLiteral("YAZIDÜZENLE hizalama=%1").arg(word));
            });
            strip->addAction(a);
            ribbonLive_->textAnchors[static_cast<int>(anchor)] = a;
        }
        textAnchor->addSmallWidget(strip);
    }

    SARibbonPanel* textBind = text->addPanel(tr("Bağ"));
    textBind->addSmallAction(processingAction(QStringLiteral("islem.bagla"), tr("Bağla")));
    textBind->addSmallAction(processingAction(QStringLiteral("islem.bag_coz"), tr("Bağı Çöz")));
    closer(text);

    // ---------------------------------------------------------------- `Ölçü`
    SARibbonCategory* dim =
        context(RibbonContext::Dimension, tr("Ölçü Araçları"), tr("Ölçü"), t.accent);
    SARibbonPanel* dimEdit = dim->addPanel(tr("Düzenle"));
    dimEdit->addLargeAction(actDimensionEdit_);
    auto* dimReset = new QAction(tr("Stile Döndür"), this);
    dimReset->setObjectName(QStringLiteral("ribbonDimReset"));
    dimReset->setData(static_cast<int>(Glyph::Refresh));
    dimReset->setToolTip(tr("ÖLÇÜDÜZENLE sifirla=hepsi — yazı, önek, sonek, tolerans, birim, "
                            "ondalık ve yazı yeri stile döner"));
    dimReset->setProperty(kToolCommandProperty, QStringLiteral("ÖLÇÜDÜZENLE"));
    connect(dimReset, &QAction::triggered, this,
            [onSelection] { onSelection(QStringLiteral("ÖLÇÜDÜZENLE sifirla=hepsi")); });
    dimEdit->addSmallAction(dimReset);
    auto* dimRefresh = new QAction(tr("Pafta Ölçeğine Uyarla"), this);
    dimRefresh->setObjectName(QStringLiteral("ribbonDimRefresh"));
    dimRefresh->setData(static_cast<int>(Glyph::DimRefresh));
    dimRefresh->setToolTip(tr("ÖLÇÜYENİLE — seçili ölçüleri plan ölçeğine uyarlar"));
    dimRefresh->setProperty(kToolCommandProperty, QStringLiteral("ÖLÇÜYENİLE"));
    connect(dimRefresh, &QAction::triggered, this, [this, onSelection] {
        onSelection(QStringLiteral("ÖLÇÜYENİLE") + selectionArgs(core::kDimensionKind));
    });
    dimEdit->addSmallAction(dimRefresh);

    SARibbonPanel* dimLook = dim->addPanel(tr("Stil ve Değer"));
    ribbonLive_->dimStyle  = new ComboBox(dimLook);
    ribbonLive_->dimStyle->setObjectName(QStringLiteral("ribbonDimStyle"));
    ribbonLive_->dimStyle->setControlSize(ControlSize::Compact);
    ribbonLive_->dimStyle->setFixedWidth(118);
    ribbonLive_->dimStyle->setAccessibleName(tr("Ölçü stili"));
    for (const command::DimensionStyle& style : ribbonLive_->dimStyles) {
        ribbonLive_->dimStyle->addItem(QString::fromStdString(style.id));
        ribbonLive_->dimStyle->setItemData(ribbonLive_->dimStyle->count() - 1,
                                           QString::fromStdString(style.description),
                                           Qt::ToolTipRole);
    }
    connect(ribbonLive_->dimStyle, &QComboBox::activated, this, [this, onSelection](int i) {
        onSelection(QStringLiteral("ÖLÇÜDÜZENLE stil=%1").arg(ribbonLive_->dimStyle->itemText(i)));
    });
    dimLook->addSmallWidget(captioned(dimLook, tr("Stil"), ribbonLive_->dimStyle, 52));
    ribbonLive_->dimPrecision = new ComboBox(dimLook);
    ribbonLive_->dimPrecision->setObjectName(QStringLiteral("ribbonDimPrecision"));
    ribbonLive_->dimPrecision->setControlSize(ControlSize::Compact);
    ribbonLive_->dimPrecision->setFixedWidth(118);
    ribbonLive_->dimPrecision->setAccessibleName(tr("Ondalık"));
    for (int d = 0; d <= 4; ++d)
        ribbonLive_->dimPrecision->addItem(
            d == 0 ? QStringLiteral("0") : QStringLiteral("0,") + QString(d, QLatin1Char('0')), d);
    connect(ribbonLive_->dimPrecision, &QComboBox::activated, this, [this, onSelection](int i) {
        onSelection(QStringLiteral("ÖLÇÜDÜZENLE hassasiyet=%1")
                        .arg(ribbonLive_->dimPrecision->itemData(i).toInt()));
    });
    dimLook->addSmallWidget(captioned(dimLook, tr("Ondalık"), ribbonLive_->dimPrecision, 52));
    ribbonLive_->dimUnit = new ComboBox(dimLook);
    ribbonLive_->dimUnit->setObjectName(QStringLiteral("ribbonDimUnit"));
    ribbonLive_->dimUnit->setControlSize(ControlSize::Compact);
    ribbonLive_->dimUnit->setFixedWidth(118);
    ribbonLive_->dimUnit->setAccessibleName(tr("Birim"));
    const std::pair<const char*, QString> units[] = {{"cizim", tr("çizimin")},
                                                     {"mm", tr("mm")},
                                                     {"cm", tr("cm")},
                                                     {"m", tr("m")},
                                                     {"km", tr("km")}};
    for (const auto& [word, label] : units)
        ribbonLive_->dimUnit->addItem(label, QString::fromLatin1(word));
    connect(ribbonLive_->dimUnit, &QComboBox::activated, this, [this, onSelection](int i) {
        onSelection(QStringLiteral("ÖLÇÜDÜZENLE birim=%1")
                        .arg(ribbonLive_->dimUnit->itemData(i).toString()));
    });
    dimLook->addSmallWidget(captioned(dimLook, tr("Birim"), ribbonLive_->dimUnit, 52));

    SARibbonPanel* dimMore = dim->addPanel(tr("Devam"));
    dimMore->addSmallAction(actDimChain_);
    dimMore->addSmallAction(actDimBaseline_);
    closer(dim);

    // -------------------------------------------------------------- `Tarama`
    SARibbonCategory* hatch =
        context(RibbonContext::Hatch, tr("Tarama Araçları"), tr("Tarama"), t.accent);
    SARibbonPanel* hatchPattern = hatch->addPanel(tr("Desen"));
    if (!ribbonLive_->patterns.empty()) {
        SARibbonGallery* gallery = hatchPattern->addGallery();
        gallery->setObjectName(QStringLiteral("ribbonHatchEditGallery"));
        for (const command::HatchPattern& pattern : ribbonLive_->patterns) {
            const QString id = QString::fromStdString(pattern.id);
            auto* a          = new QAction(id, this);
            a->setCheckable(true);
            a->setProperty(kPatternProperty, id);
            a->setToolTip(tr("%1 — %2").arg(id, QString::fromStdString(pattern.description)));
            connect(a, &QAction::triggered, this, [onSelection, id] {
                onSelection(QStringLiteral("TARAMADÜZENLE desen=%1").arg(id));
            });
            ribbonLive_->hatchPatterns << a;
        }
        SARibbonGalleryGroup* group =
            gallery->addCategoryActions(tr("Desenler"), ribbonLive_->hatchPatterns);
        group->setGalleryGroupStyle(SARibbonGalleryGroup::IconWithText);
        group->setDisplayRow(SARibbonGalleryGroup::DisplayOneRow);
        group->setGridMinimumWidth(58);
        group->setGridMaximumWidth(58);
        gallery->setCurrentViewGroup(group);
    }

    SARibbonPanel* hatchLook = hatch->addPanel(tr("Özellikler"));
    ribbonLive_->hatchAngle  = new ComboBox(hatchLook);
    ribbonLive_->hatchAngle->setObjectName(QStringLiteral("ribbonHatchAngle"));
    ribbonLive_->hatchAngle->setControlSize(ControlSize::Compact);
    ribbonLive_->hatchAngle->setEditable(true);
    ribbonLive_->hatchAngle->setInsertPolicy(QComboBox::NoInsert);
    ribbonLive_->hatchAngle->setFixedWidth(90);
    ribbonLive_->hatchAngle->setAccessibleName(tr("Desen açısı"));
    for (const int deg : {0, 15, 30, 45, 60, 90, 135})
        ribbonLive_->hatchAngle->addItem(QStringLiteral("%1°").arg(deg), deg);
    connectMetres(
        ribbonLive_->hatchAngle,
        [onSelection](double deg) {
            onSelection(QStringLiteral("TARAMADÜZENLE aci=%1").arg(deg, 0, 'f', 2));
        },
        QStringLiteral("°"));
    hatchLook->addSmallWidget(captioned(hatchLook, tr("Açı"), ribbonLive_->hatchAngle, 40));
    ribbonLive_->hatchScale = new ComboBox(hatchLook);
    ribbonLive_->hatchScale->setObjectName(QStringLiteral("ribbonHatchScale"));
    ribbonLive_->hatchScale->setControlSize(ControlSize::Compact);
    ribbonLive_->hatchScale->setEditable(true);
    ribbonLive_->hatchScale->setInsertPolicy(QComboBox::NoInsert);
    ribbonLive_->hatchScale->setFixedWidth(90);
    ribbonLive_->hatchScale->setAccessibleName(tr("Desen ölçeği"));
    for (const double k : {0.5, 1.0, 2.0, 5.0, 10.0, 100.0, 500.0, 1000.0})
        ribbonLive_->hatchScale->addItem(metresText(k, k < 1.0 ? 1 : 0), k);
    connectMetres(ribbonLive_->hatchScale, [onSelection](double k) {
        onSelection(QStringLiteral("TARAMADÜZENLE olcek=%1").arg(k, 0, 'g', 6));
    });
    hatchLook->addSmallWidget(captioned(hatchLook, tr("Ölçek"), ribbonLive_->hatchScale, 40));
    ribbonLive_->hatchCross = new QAction(tr("Çapraz"), this);
    ribbonLive_->hatchCross->setObjectName(QStringLiteral("ribbonHatchCross"));
    ribbonLive_->hatchCross->setCheckable(true);
    ribbonLive_->hatchCross->setData(static_cast<int>(Glyph::Grid));
    ribbonLive_->hatchCross->setToolTip(
        tr("TARAMADÜZENLE cift= — desen bir de dik açıyla çizilir (çapraz tarama)"));
    connect(ribbonLive_->hatchCross, &QAction::triggered, this, [onSelection](bool on) {
        onSelection(QStringLiteral("TARAMADÜZENLE cift=%1")
                        .arg(on ? QStringLiteral("evet") : QStringLiteral("hayır")));
    });
    hatchLook->addSmallAction(ribbonLive_->hatchCross);

    // THE ISLAND RULE as three pictures of the same nested squares.
    SARibbonPanel* islands = hatch->addPanel(tr("Adalar"));
    auto* islandGroup      = new QActionGroup(this);
    islandGroup->setExclusive(true);
    const std::tuple<const char*, QString, Glyph, QString> rules[] = {
        {"normal", tr("Normal"), Glyph::IslandNormal,
         tr("İç içe sırayla delik ve dolu: adanın içi boş, onun içindeki ada yine taralı")},
        {"dis", tr("Yalnız dış"), Glyph::IslandOuter,
         tr("Yalnız en dıştaki alan taranır; ilk ada ve içindekiler boş kalır")},
        {"yoksay", tr("Adasız"), Glyph::IslandIgnore,
         tr("Adalar yok sayılır; dış sınırın bütün içi taranır")},
    };
    ribbonLive_->hatchIslands.clear();
    for (const auto& [word, label, glyph, tip] : rules) {
        auto* a = new QAction(label, this);
        a->setCheckable(true);
        a->setObjectName(QStringLiteral("ribbonIsland.") + QString::fromLatin1(word));
        a->setData(static_cast<int>(glyph));
        a->setToolTip(tr("TARAMADÜZENLE stil=%1 — %2").arg(QString::fromLatin1(word), tip));
        islandGroup->addAction(a);
        const QString w = QString::fromLatin1(word);
        connect(a, &QAction::triggered, this,
                [onSelection, w] { onSelection(QStringLiteral("TARAMADÜZENLE stil=%1").arg(w)); });
        islands->addSmallAction(a);
        ribbonLive_->hatchIslands << a;
    }

    SARibbonPanel* hatchBounds = hatch->addPanel(tr("Sınır"));
    hatchBounds->addLargeAction(actBoundary_);
    hatchBounds->addSmallAction(actHatchEdit_);
    closer(hatch);

    // ---------------------------------------------------------------- `Alan`
    //
    // WHAT IS DONE TO A PARCEL ONCE IT IS PICKED, each on the picked parcel and
    // in one press: its area read, its corners numbered, its edges written, cut,
    // merged, hatched. A tool that needs a figure first (TAMPON's distance,
    // ALANDÜZENLE's target area) says so with `…` and opens where the figure is
    // typed.
    SARibbonCategory* area =
        context(RibbonContext::Area, tr("Alan Araçları"), tr("Alan"), t.accent);
    // Straight to the command, on the selection with the tool's defaults.
    const auto direct = [this](const char* id, const QString& word) {
        QAction* a = processingAction(QString::fromLatin1(id), word);
        disconnect(a, &QAction::triggered, nullptr, nullptr);
        const QString line = a->property(kToolCommand).toString();
        connect(a, &QAction::triggered, this,
                [this, line] { controller_->runLine(line, command::Origin::Gui); });
        return a;
    };
    SARibbonPanel* areaRead = area->addPanel(tr("Ölç"));
    areaRead->addLargeAction(actMeasureArea_);
    areaRead->addSmallAction(actEntityInfo_);
    areaRead->addSmallAction(actCoordinate_);
    SARibbonPanel* areaWrite = area->addPanel(tr("Yaz"));
    areaWrite->addLargeAction(direct("islem.kose_numarala", tr("Köşe Numarala")));
    areaWrite->addLargeAction(direct("islem.uzunluk_yaz", tr("Uzunluk Yaz")));
    launcher(areaWrite, tr("Numaralama ve uzunluk yazma ayarları — Araçlar paneli"),
             [this] { showToolsPanel(QStringLiteral("islem.kose_numarala")); });
    SARibbonPanel* areaCadastre = area->addPanel(tr("Kadastro"));
    areaCadastre->addLargeAction(actParcelSplit_);
    areaCadastre->addLargeAction(actAreaSplit_);
    areaCadastre->addLargeAction(actUnion_);
    areaCadastre->addSmallAction(actTopology_);
    SARibbonPanel* areaShape = area->addPanel(tr("Düzenle"));
    areaShape->addLargeAction(actHatch_);
    areaShape->addSmallAction(actOffset_);
    areaShape->addSmallAction(processingAction(QStringLiteral("islem.tampon"), tr("Tampon…")));
    areaShape->addSmallAction(
        processingAction(QStringLiteral("islem.alan_duzenle"), tr("Alanı Düzenle…")));
    closer(area);

    // ---------------------------------------------------------------- `Blok`
    SARibbonCategory* block =
        context(RibbonContext::Block, tr("Blok Araçları"), tr("Blok"), t.accent);
    SARibbonPanel* blockEdit = block->addPanel(tr("Blok"));
    blockEdit->addLargeAction(actExplode_);
    blockEdit->addLargeAction(actInsert_);
    blockEdit->addSmallAction(actBlock_);
    blockEdit->addSmallAction(actEntityInfo_);
    closer(block);
}

void MainWindow::loadRibbonCatalogues()
{
    // THE CATALOGUES THE GALLERIES SHOW, read where the commands read them: the
    // App settings name the files, `resolve_catalog_path` finds them. A missing
    // catalogue leaves its gallery out rather than inventing one.
    const core::Settings& app = controller_->bus().app_settings();
    if (auto patterns = command::load_hatch_patterns(
            command::resolve_catalog_path(app.get("core.tarama.desen_katalogu").as_text()));
        patterns)
        ribbonLive_->patterns = std::move(patterns.value().patterns);
    if (auto styles = command::load_dimension_styles(
            command::resolve_catalog_path(app.get("core.olcu.stil_katalogu").as_text()));
        styles)
        ribbonLive_->dimStyles = std::move(styles.value().styles);
}

void MainWindow::connectMetres(ComboBox* box, const std::function<void(double)>& take,
                               const QString& unit)
{
    // A PICK FROM THE LIST TAKES ITS VALUE; A FIGURE TYPED AND ENTERED IS READ
    // in the box's unit, and one that is not a number puts the box back.
    connect(box, &QComboBox::activated, this, [this, box, take, unit](int index) {
        const QVariant held = box->itemData(index);
        if (held.isValid() && box->itemText(index) == box->currentText()) {
            take(held.toDouble());
            return;
        }
        if (const auto typed = typedNumber(box->currentText(), unit); typed && *typed > 0.0)
            take(*typed);
        else
            refreshRibbon();
    });
    if (QLineEdit* edit = box->lineEdit(); edit != nullptr)
        connect(edit, &QLineEdit::returnPressed, this, [this, box, take, unit] {
            if (const auto typed = typedNumber(box->currentText(), unit); typed && *typed > 0.0)
                take(*typed);
            else
                refreshRibbon();
        });
}

QAction* MainWindow::processingAction(const QString& id, const QString& word)
{
    const processing::ProcessingTool* found = nullptr;
    for (const processing::ProcessingTool* each : processing::processing_tools())
        if (QString::fromStdString(each->spec().id) == id) found = each;
    auto* action = new QAction(this);
    action->setData(
        static_cast<int>(found != nullptr ? glyph_named(found->spec().icon) : Glyph::Function));
    if (found == nullptr) {
        action->setText(id);
        action->setEnabled(false);
        return action;
    }
    const auto& spec = found->spec();
    // THE TOOL'S OWN TITLE, unless the ribbon row is too short for it: a menu
    // row may say "Kenar uzunluklarını yaz", a ribbon row says "Uzunluk Yaz".
    action->setText(word.isEmpty() ? turkish_title(QString::fromStdString(spec.title)) : word);
    action->setToolTip(QString::fromStdString(spec.summary));
    action->setStatusTip(QString::fromStdString(spec.summary));
    action->setProperty(kToolCommand, QString::fromStdString(spec.names.front()));
    connect(action, &QAction::triggered, this, [this, id] { showToolsPanel(id); });
    return action;
}

void MainWindow::showToolsPanel(const QString& id)
{
    propertyDock_->show();
    propertyDock_->raise();
    propertyHeader_->setCurrent(2);
    propertyStack_->setCurrentIndex(2);
    if (!id.isEmpty() && toolsPanel_ != nullptr) toolsPanel_->selectTool(id);
}

void MainWindow::showLayerPanel()
{
    if (layerDock_ == nullptr) return;
    layerDock_->show();
    layerDock_->raise();
}

QList<QAction*> MainWindow::layerActions()
{
    // MADE ONCE, shown on `Giriş` and on `Görünüm` alike. Each reads the layer it
    // means at the moment it is pressed — the selection's, else the active one —
    // and runs the one line that changes it.
    if (!layerActions_.isEmpty()) return layerActions_;
    const auto make = [this](Glyph glyph, const QString& label, const QString& tip, auto run) {
        auto* a = new QAction(label, this);
        a->setData(static_cast<int>(glyph));
        a->setToolTip(tip);
        a->setObjectName(QStringLiteral("ribbonLayer.") + label);
        connect(a, &QAction::triggered, this, run);
        layerActions_ << a;
        return a;
    };
    make(Glyph::LayerCurrent, tr("Etkin Yap"),
         tr("Seçili nesnenin katmanını etkin katman yapar (KATMAN ad=…)"), [this] {
             const QString on = ribbonLayerInHand(true);
             if (on.isEmpty()) {
                 onEcho(tr("Önce katmanı etkin yapılacak bir nesne seçin."));
                 return;
             }
             controller_->runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(on), command::Origin::Gui);
         });
    make(Glyph::LayerMove, tr("Etkin Katmana Taşı"),
         tr("Seçili nesneleri etkin katmana taşır (KATMANAT)"), [this] {
             controller_->runCommand(
                 QStringLiteral("KATMANAT katman=\"%1\"").arg(controller_->activeLayerName()));
         });
    make(Glyph::EyeOff, tr("Gizle"),
         tr("Seçili nesnenin katmanını — seçim yoksa etkin katmanı — gizler"), [this] {
             controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=gizle katman=\"%1\"")
                                      .arg(ribbonLayerInHand(false)),
                                  command::Origin::Gui);
         });
    make(Glyph::LayerIsolate, tr("Yalnız Bu"),
         tr("Yalnız seçili nesnenin katmanı — seçim yoksa etkin katman — görünür kalır"), [this] {
             controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=yalniz katman=\"%1\"")
                                      .arg(ribbonLayerInHand(false)),
                                  command::Origin::Gui);
         });
    make(Glyph::Eye, tr("Tümünü Göster"),
         tr("KATMANGÖRÜNÜM islem=tumu — gizli bütün katmanları geri getirir"), [this] {
             controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=tumu"), command::Origin::Gui);
         });
    make(Glyph::Invert, tr("Gösterimi Ters Çevir"), // ui-label
         tr("KATMANGÖRÜNÜM islem=tersine — görünenleri gizler, gizlileri gösterir"), [this] {
             controller_->runLine(QStringLiteral("KATMANGÖRÜNÜM islem=tersine"),
                                  command::Origin::Gui);
         });
    make(Glyph::Lock, tr("Kilitle / Aç"),
         tr("Seçili nesnenin katmanını — seçim yoksa etkin katmanı — kilitler ya da kilidini "
            "açar; kilitli katmanın nesneleri görünür ama seçilemez"),
         [this] {
             const QString on          = ribbonLayerInHand(false);
             const core::Document& doc = controller_->document();
             const core::LayerId id    = doc.find_layer(on.toStdString());
             const core::Layer* layer  = doc.layer(id);
             const bool locked         = layer != nullptr && layer->locked;
             controller_->runLine(
                 QStringLiteral("KATMAN ad=\"%1\" kilitli=%2")
                     .arg(on, locked ? QStringLiteral("hayır") : QStringLiteral("evet")),
                 command::Origin::Gui);
         });
    make(Glyph::LayerAdd, tr("Yeni Katman"),
         tr("KATMAN — adını yazdığınız katmanı oluşturur ve etkin yapar"),
         [this] { controller_->runCommand(QStringLiteral("KATMAN")); });
    return layerActions_;
}

QString MainWindow::ribbonLayerInHand(bool selectionOnly) const
{
    const core::Document& doc = controller_->document();
    for (const core::EntityKey k : controller_->bus().selection().keys()) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        if (const core::Layer* l = doc.layer(doc.entities().layer[e]); l != nullptr)
            return QString::fromStdString(l->name);
    }
    return selectionOnly ? QString() : controller_->activeLayerName();
}

void MainWindow::pickRibbonLayer(const QString& name)
{
    // AUTOCAD'S RULE: with a selection the list MOVES it, without one it makes
    // the layer active. Both are one line, the same line the Katmanlar panel and
    // the command line run.
    int held = 0;
    for (const core::EntityKey k : controller_->bus().selection().keys())
        if (const core::EntityId e = controller_->document().slot_of(k);
            e != core::kNoEntity && controller_->document().alive(e))
            ++held;
    if (held > 0)
        controller_->runLine(QStringLiteral("KATMANAT katman=\"%1\"").arg(name),
                             command::Origin::Gui);
    else
        controller_->runLine(QStringLiteral("KATMAN ad=\"%1\"").arg(name), command::Origin::Gui);
}

QString MainWindow::selectionArgs(core::KindId kind) const
{
    QString args;
    const core::Document& doc = controller_->document();
    for (const core::EntityKey k : controller_->bus().selection().keys()) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        if (kind != core::kNoKind && doc.entities().kind[e] != kind) continue;
        args += QStringLiteral(" nesneler=") + keyText(k);
    }
    return args;
}

// =============================================================================
// Reading the document into the ribbon
// =============================================================================

void MainWindow::refreshRibbon()
{
    if (ribbonLive_->layer == nullptr) return;
    refreshLayerBox();
    refreshColourBoxes();
    refreshRibbonDefaults();
    refreshContextTabs();
}

void MainWindow::refreshLayerBox()
{
    RibbonLayerBox* box = ribbonLive_->layer;
    if (box == nullptr) return;
    const core::Document& doc = controller_->document();

    // THE SELECTION'S LAYER, when there is one; else the active layer.
    core::LayerId held = core::kNoLayer;
    bool mixed         = false;
    bool any           = false;
    for (const core::EntityKey k : controller_->bus().selection().keys()) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        const core::LayerId l = doc.entities().layer[e];
        if (!any)
            held = l;
        else if (l != held)
            mixed = true;
        any = true;
    }
    if (!any) held = controller_->bus().active_layer();

    QVector<RibbonLayerRow> rows;
    int shown = -1;
    for (core::LayerId id = 0; id < static_cast<core::LayerId>(doc.layers().size()); ++id) {
        const core::Layer* l = doc.layer(id);
        if (l == nullptr) continue;
        const core::Symbol sym = core::layer_symbol(doc, id);
        if (id == held && !mixed) shown = static_cast<int>(rows.size());
        rows.push_back({QString::fromStdString(l->name), QColor::fromRgba(sym.primary().rgba),
                        l->visible, l->locked});
    }
    box->setRows(rows, shown, tr("farklı katmanlar"));
    box->setProperty("state", any ? QStringLiteral("derived") : QString());
}

void MainWindow::refreshColourBoxes()
{
    if (ribbonLive_->stroke == nullptr) return;
    const core::Document& doc = controller_->document();

    core::EntityId first = core::kNoEntity;
    int held             = 0;
    for (const core::EntityKey k : controller_->bus().selection().keys()) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        if (first == core::kNoEntity) first = e;
        ++held;
    }

    // WHAT IS DRAWN, the way the scene draws it: an object's own style, or its
    // layer's when it inherits (`core::drawn_symbol`).
    const core::Symbol sym = first != core::kNoEntity
                                 ? core::drawn_symbol(doc, first)
                                 : core::layer_symbol(doc, controller_->bus().active_layer());
    // THE FILL SHOWS WHAT IS PAINTED. The scene fills a face only from a fill
    // layer, so the fill colour a plain line carries is drawn nowhere and showing
    // it would put a colour in the box that is not on the sheet.
    const std::uint32_t stroke = sym.primary().rgba;
    std::uint32_t fill         = 0;
    for (const core::SymbolLayer& l : sym.layers)
        if (l.enabled && core::draws_fill(l.type)) {
            fill = l.look.fill_rgba;
            break;
        }
    const bool inherited =
        first == core::kNoEntity || doc.entities().style[first] == core::kByLayerStyle;

    const auto named = [](std::uint32_t rgba) {
        const std::string_view word = command::colour_word(rgba);
        return word.empty() ? QString::fromStdString(command::colour_hex(rgba))
                            : turkish_title(QString::fromUtf8(word.data(),
                                                              static_cast<qsizetype>(word.size())));
    };
    ribbonLive_->stroke->setShown(QColor::fromRgba(stroke),
                                  inherited ? tr("Katmandan") : named(stroke));
    ribbonLive_->fill->setShown(fill != 0 ? QColor::fromRgba(fill) : QColor(),
                                fill == 0   ? tr("Dolgu yok")
                                : inherited ? tr("Katmandan")
                                            : named(fill));
    const QString whose = held > 0 ? tr("seçili %n nesnenin", nullptr, held) : tr("etkin katmanın");
    ribbonLive_->stroke->setToolTip(
        tr("Çizgi rengi: %1 (%2). Açın: %3")
            .arg(named(stroke), whose,
                 held > 0 ? tr("seçilenlerin çizgi rengini değiştirin")
                          : tr("bir renk seçin, sonra boyanacak nesnelere tıklayın")));
    ribbonLive_->fill->setToolTip(
        tr("Dolgu rengi: %1 (%2). Açın: %3")
            .arg(fill != 0 ? named(fill) : tr("yok"), whose,
                 held > 0 ? tr("seçilenlerin dolgu rengini değiştirin")
                          : tr("bir renk seçin, sonra boyanacak nesnelere tıklayın")));
}

void MainWindow::refreshRibbonDefaults()
{
    const core::Settings& project = controller_->bus().project_settings();
    if (ComboBox* box = ribbonLive_->textHeightDefault; box != nullptr && !box->hasFocus()) {
        const QSignalBlocker quiet(box);
        const double metres =
            static_cast<double>(project.get("core.cizim.metin_yuksekligi").as_int()) / 1000.0;
        box->setEditText(tr("%1 m").arg(metresText(metres)));
    }
    if (ComboBox* box = ribbonLive_->dimStyleDefault; box != nullptr) {
        const QSignalBlocker quiet(box);
        const std::string_view style = project.get("core.olcu.stil").as_text();
        box->setCurrentIndex(
            box->findText(QString::fromUtf8(style.data(), static_cast<qsizetype>(style.size())),
                          Qt::MatchFixedString));
    }
    if (ComboBox* box = ribbonLive_->plotScale; box != nullptr && !box->hasFocus()) {
        const QSignalBlocker quiet(box);
        box->setEditText(QStringLiteral("1:%1").arg(project.get("core.plan.olcek").as_int()));
    }
}

void MainWindow::refreshContextTabs()
{
    SARibbonBar* bar = ribbonBar();
    if (bar == nullptr || ribbonLive_->contexts[0] == nullptr) return;
    const core::Document& doc = controller_->document();

    // WHAT THE SELECTION HOLDS, by the tab that edits it, and the first object of
    // each so a tab's boxes can read its values.
    std::array<int, kRibbonContextCount> count{};
    std::array<core::EntityId, kRibbonContextCount> first{};
    first.fill(core::kNoEntity);
    int total = 0;
    for (const core::EntityKey k : controller_->bus().selection().keys()) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        ++total;
        const core::KindId kind   = doc.entities().kind[e];
        const std::uint32_t gslot = doc.entities().slot[e];
        std::optional<RibbonContext> which;
        if (kind == core::kDimensionKind)
            which = RibbonContext::Dimension;
        else if (kind == core::kHatchKind)
            which = RibbonContext::Hatch;
        else if (kind == core::kBlockReferenceKind)
            which = RibbonContext::Block;
        else if (doc.texts().has(gslot))
            which = RibbonContext::Text;
        else if (doc.entity_area(e) != 0)
            which = RibbonContext::Area;
        if (!which) continue;
        const auto i = static_cast<std::size_t>(*which);
        if (count[i]++ == 0) first[i] = e;
    }

    // NOT WHILE A COMMAND ASKS: a selection made to answer `TAŞI`'s question is an
    // answer, not an object to be edited, and a tab jumping up under the prompt
    // would be in the way of the next one.
    const bool idle           = !controller_->awaitingInput();
    SARibbonCategory* current = bar->categoryByIndex(bar->currentIndex());
    // Which editor tab the hand is on, if it is on one.
    std::optional<std::size_t> onContext;
    for (std::size_t i = 0; i < kRibbonContextCount; ++i)
        if (current != nullptr && ribbonLive_->contexts[i]->isHaveCategory(current)) onContext = i;
    std::optional<std::size_t> raise;
    for (std::size_t i = 0; i < kRibbonContextCount; ++i) {
        SARibbonContextCategory* ctx = ribbonLive_->contexts[i];
        const bool want              = idle && count[i] > 0;
        if (want == ribbonLive_->showing[i]) continue;
        ribbonLive_->showing[i] = want;
        bar->setContextCategoryVisible(ctx, want);
        // A NEW EDITOR COMES FORWARD when the selection is only its kind — a
        // hatch picked to change its pattern, a caption picked to change its
        // height — the way AutoCAD's Hatch and Text Editor tabs do. Areas and
        // blocks only appear: a parcel is selected for a hundred other reasons.
        const bool editor = i == static_cast<std::size_t>(RibbonContext::Text) ||
                            i == static_cast<std::size_t>(RibbonContext::Dimension) ||
                            i == static_cast<std::size_t>(RibbonContext::Hatch);
        if (want && editor && count[i] == total) raise = i;
    }
    if (raise) {
        if (!onContext) ribbonLive_->before = current;
        if (SARibbonCategory* page = ribbonLive_->contexts[*raise]->categoryPage(0);
            page != nullptr)
            bar->raiseCategory(page);
    } else if (onContext && !ribbonLive_->showing[*onContext]) {
        // The editor that was up went with its selection: back to the tab the
        // hand was on before it came.
        if (ribbonLive_->before != nullptr)
            bar->raiseCategory(ribbonLive_->before);
        else
            bar->setCurrentIndex(0);
    }

    // ---- the values on the tabs that show ---------------------------------
    const auto quiet = [](QWidget* w) { return QSignalBlocker(w); };
    if (const core::EntityId e = first[static_cast<std::size_t>(RibbonContext::Text)];
        e != core::kNoEntity) {
        const std::uint32_t g = doc.entities().slot[e];
        if (!ribbonLive_->textHeight->hasFocus()) {
            const QSignalBlocker hold = quiet(ribbonLive_->textHeight);
            ribbonLive_->textHeight->setEditText(
                tr("%1 m").arg(metresText(static_cast<double>(doc.texts().height(g)) / 1000.0)));
        }
        {
            const QSignalBlocker hold = quiet(ribbonLive_->textSpacing);
            const double k            = static_cast<double>(doc.texts().lines(g).spacing) / 1000.0;
            int best                  = 0;
            for (int i = 1; i < ribbonLive_->textSpacing->count(); ++i)
                if (std::abs(ribbonLive_->textSpacing->itemData(i).toDouble() - k) <
                    std::abs(ribbonLive_->textSpacing->itemData(best).toDouble() - k))
                    best = i;
            ribbonLive_->textSpacing->setCurrentIndex(best);
        }
        const auto anchor = static_cast<std::size_t>(doc.texts().anchor(g));
        for (std::size_t i = 0; i < static_cast<std::size_t>(ribbonLive_->textAnchors.size()); ++i)
            if (QAction* a = ribbonLive_->textAnchors[static_cast<int>(i)]; a != nullptr)
                a->setChecked(i == anchor);
    }
    if (const core::EntityId e = first[static_cast<std::size_t>(RibbonContext::Dimension)];
        e != core::kNoEntity)
        if (auto def = core::dimension_of(doc.geometry(), doc.entities().slot[e]); def) {
            const core::DimensionDef& d = def.value();
            {
                const QSignalBlocker hold = quiet(ribbonLive_->dimStyle);
                ribbonLive_->dimStyle->setCurrentIndex(ribbonLive_->dimStyle->findText(
                    QString::fromStdString(d.style), Qt::MatchFixedString));
            }
            {
                const QSignalBlocker hold = quiet(ribbonLive_->dimPrecision);
                ribbonLive_->dimPrecision->setCurrentIndex(
                    ribbonLive_->dimPrecision->findData(static_cast<int>(d.precision)));
            }
            {
                const QSignalBlocker hold = quiet(ribbonLive_->dimUnit);
                ribbonLive_->dimUnit->setCurrentIndex(
                    std::clamp(static_cast<int>(d.unit), 0, ribbonLive_->dimUnit->count() - 1));
            }
        }
    if (const core::EntityId e = first[static_cast<std::size_t>(RibbonContext::Hatch)];
        e != core::kNoEntity)
        if (auto def = core::hatch_of(doc.geometry(), doc.entities().slot[e]); def) {
            const core::HatchDef& h = def.value();
            const QString name      = QString::fromStdString(h.name);
            for (QAction* a : std::as_const(ribbonLive_->hatchPatterns))
                a->setChecked(
                    a->property(kPatternProperty).toString().compare(name, Qt::CaseInsensitive) ==
                    0);
            if (!ribbonLive_->hatchAngle->hasFocus()) {
                const QSignalBlocker hold = quiet(ribbonLive_->hatchAngle);
                ribbonLive_->hatchAngle->setEditText(QStringLiteral("%1°").arg(
                    metresText(static_cast<double>(h.angle_udeg) * 1e-6, 0)));
            }
            if (!ribbonLive_->hatchScale->hasFocus()) {
                const QSignalBlocker hold = quiet(ribbonLive_->hatchScale);
                const double k            = h.scale.den != 0 ? static_cast<double>(h.scale.num) /
                                                        static_cast<double>(h.scale.den)
                                                             : 1.0;
                ribbonLive_->hatchScale->setEditText(metresText(k, k < 10.0 ? 2 : 0));
            }
            ribbonLive_->hatchCross->setChecked(h.double_lines);
            for (int i = 0; i < ribbonLive_->hatchIslands.size(); ++i)
                ribbonLive_->hatchIslands[i]->setChecked(static_cast<int>(h.style) == i);
        }
}

void MainWindow::refreshRibbonPictures()
{
    // THE PICTURES THAT ARE DATA — a pattern from the catalogue, an anchor — are
    // not glyphs, so the theme's walk over `data()` does not reach them.
    const Tokens& t = tokensFor(theme_);

    // THE EDITOR TABS' CAP IS THE ACCENT, for every one of them: blue means the
    // selection in this program (`design.md` §1.2), and an editor tab is the
    // selection's. Set again on every theme, because SARibbon's own theme pass
    // hands out a palette of its own.
    if (SARibbonBar* bar = ribbonBar(); bar != nullptr && ribbonLive_->contexts[0] != nullptr) {
        for (std::size_t i = 0; i < kRibbonContextCount; ++i)
            ribbonLive_->contexts[i]->setContextColor(t.accent);
        bar->setContextCategoryColorHighLight([](const QColor& c) { return c; });
        bar->update();
    }
    for (const QList<QAction*>* list : {&ribbonLive_->drawPatterns, &ribbonLive_->hatchPatterns})
        for (QAction* a : *list) {
            const QString id = a->property(kPatternProperty).toString();
            for (const command::HatchPattern& pattern : ribbonLive_->patterns)
                if (QString::fromStdString(pattern.id) == id)
                    a->setIcon(hatch_swatch(pattern, t.iconInk, t.bgInput, QSize(44, 30)));
        }
    for (QAction* a : std::as_const(ribbonLive_->textAnchors))
        if (a != nullptr)
            a->setIcon(anchor_icon(a->property("kentos.anchor.col").toInt(),
                                   a->property("kentos.anchor.row").toInt(), t.iconInk,
                                   t.iconNote));
}

// =============================================================================
// The tip every ribbon button shows
// =============================================================================

QString MainWindow::ribbonTip(const QAction* action) const
{
    if (action == nullptr) return {};
    const Tokens& t = tokensFor(theme_);

    // A FAMILY'S BUTTON SPEAKS FOR THE MEMBER ON ITS FACE, and lists the others.
    const QAction* face        = action;
    const RibbonFamily* family = nullptr;
    for (const RibbonFamily* f : families_)
        if (f->head() == action) {
            family = f;
            if (f->face() != nullptr) face = f->face();
            break;
        }

    const QString title = QString(face->text()).remove(QLatin1Char('&'));
    const QString line  = face->property(kToolCommand).toString();
    const QString word  = line.section(QLatin1Char(' '), 0, 0);
    const command::CommandSpec* spec =
        word.isEmpty() ? nullptr : controller_->registry().resolve(word.toStdString());

    // WHAT IT DOES, in the words the action already says it with — its own tip
    // without the leading `WORD — ` and the trailing abbreviation, which the last
    // line says properly — or else the registry's summary.
    QString body = face->toolTip();
    if (body == title || body == face->text()) body.clear();
    if (const qsizetype dash = body.indexOf(QStringLiteral(" — ")); dash > 0 && dash < 48) {
        const QString lead = body.left(dash);
        if (lead == lead.toUpper() || lead.startsWith(word)) body = body.mid(dash + 3);
    }
    if (const qsizetype dot = body.indexOf(QStringLiteral("  ·  ")); dot > 0) body = body.left(dot);
    if (body.isEmpty() && spec != nullptr) body = QString::fromStdString(spec->summary);
    if (!body.isEmpty()) body[0] = body[0].toUpper();

    // HOW IT IS TYPED: the whole line when the button sends one (`YAY yontem=3n`),
    // then every other name the registry knows it by.
    QStringList typed;
    if (spec != nullptr) {
        const std::string primary = core::turkish_fold_key(spec->names.front());
        typed << (line.contains(QLatin1Char(' ')) ? line
                                                  : QString::fromStdString(spec->names.front()));
        for (std::size_t i = 1; i < spec->names.size(); ++i)
            if (core::turkish_fold_key(spec->names[i]) != primary)
                typed << QString::fromStdString(spec->names[i]);
    }
    const QString keys = face->shortcut().toString(QKeySequence::NativeText);

    const QString faint = t.textFaint.name();
    QString html =
        QStringLiteral("<p style='margin:0; white-space:pre'><b>%1</b>").arg(title.toHtmlEscaped());
    if (!keys.isEmpty())
        html += QStringLiteral("&nbsp;&nbsp;<span style='color:%1'>%2</span>")
                    .arg(faint, keys.toHtmlEscaped());
    html += QStringLiteral("</p>");
    if (!body.isEmpty())
        html += QStringLiteral("<p style='margin:4px 0 0 0'>%1</p>").arg(body.toHtmlEscaped());
    if (!typed.isEmpty())
        html += QStringLiteral("<p style='margin:6px 0 0 0; color:%1; font-family:\"IBM Plex "
                               "Mono\"'>%2</p>")
                    .arg(faint, typed.join(QStringLiteral("  ·  ")).toHtmlEscaped());
    if (family != nullptr && family->members().size() > 1) {
        QStringList others;
        for (const QAction* m : family->members())
            if (m != face) others << QString(m->text()).remove(QLatin1Char('&'));
        html +=
            QStringLiteral("<p style='margin:6px 0 0 0; color:%1'>%2</p>")
                .arg(faint, tr("Oktan: %1").arg(others.join(QStringLiteral(", "))).toHtmlEscaped());
    }
    return QStringLiteral("<div style='max-width:340px'>%1</div>").arg(html);
}

// =============================================================================
// The registry's leftovers and the ribbon's buttons
// =============================================================================

QList<QAction*> MainWindow::ribbonLeftovers(std::initializer_list<command::Category> categories,
                                            std::initializer_list<const char*> prefixes)
{
    // Every command any action in this window can already start, resolved through
    // the registry so an alias on a button counts as the command it names. Read
    // afresh on every call: the actions an earlier call made are children of the
    // window too, so a command is offered by exactly one "Diğer" menu.
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

    QList<QAction*> added;
    for (const command::CommandSpec& spec : controller_->registry().all()) {
        if (spec.names.empty() || already.contains(QString::fromStdString(spec.id))) continue;
        const bool byPrefix = std::any_of(prefixes.begin(), prefixes.end(), [&spec](const char* p) {
            return spec.id.starts_with(p);
        });
        const bool byCategory =
            std::find(categories.begin(), categories.end(), spec.category) != categories.end();
        if (!byPrefix && !byCategory) continue;

        const QString word = QString::fromStdString(spec.names.front());
        // THE SPEC'S OWN LABEL. A name is one word by design (`ÇIKTIYERLEŞİMİ`)
        // and a button built from it reads as one word, which is not Turkish;
        // `title` is where the spaces live. The fallback is right for a
        // one-word command and the only honest guess for any other.
        const QString label =
            spec.title.empty() ? turkish_title(word) : QString::fromStdString(spec.title);
        added << commandAction(glyph_of(spec.category), label, word,
                               word + QStringLiteral(" — ") + QString::fromStdString(spec.summary));
    }
    return added;
}

QToolButton* MainWindow::ribbonButton(const QAction* action, bool raise)
{
    SARibbonBar* bar = ribbonBar();
    if (bar == nullptr || action == nullptr) return nullptr;
    // A member is pressed through its family's face.
    const QAction* carried = action;
    for (const RibbonFamily* f : std::as_const(families_))
        if (f->members().contains(const_cast<QAction*>(action))) {
            carried = f->head();
            break;
        }
    for (SARibbonToolButton* button : bar->findChildren<SARibbonToolButton*>()) {
        if (button->defaultAction() != carried) continue;
        if (raise) {
            for (QWidget* w = button->parentWidget(); w != nullptr; w = w->parentWidget())
                if (auto* tab = qobject_cast<SARibbonCategory*>(w); tab != nullptr) {
                    bar->raiseCategory(tab);
                    break;
                }
            QCoreApplication::processEvents();
        }
        return button;
    }
    return nullptr;
}

QList<QToolButton*> MainWindow::ribbonButtons() const
{
    QList<QToolButton*> out;
    const SARibbonBar* bar = ribbonBar();
    if (bar == nullptr) return out;
    for (SARibbonCategory* tab : bar->categoryPages())
        for (SARibbonToolButton* button : tab->findChildren<SARibbonToolButton*>())
            if (button->defaultAction() != nullptr) out << button;
    return out;
}

// =============================================================================
// The colour menu
// =============================================================================

void MainWindow::openColourMenu(int which)
{
    const bool fill = which == 1;
    int held        = 0;
    for (const core::EntityKey k : controller_->bus().selection().keys())
        if (const core::EntityId e = controller_->document().slot_of(k);
            e != core::kNoEntity && controller_->document().alive(e))
            ++held;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setObjectName(fill ? QStringLiteral("colourMenu.fill")
                             : QStringLiteral("colourMenu.stroke"));
    menu->setAccessibleName(fill ? tr("Dolgu rengi") : tr("Çizgi rengi"));

    // WHAT A PICK WILL PAINT, said before it is made: with a selection it is
    // the selection, and without one the next thing RENK does is ask.
    QString heading;
    if (held > 0)
        heading = fill ? tr("Seçili %n nesnenin dolgu rengi", nullptr, held)
                       : tr("Seçili %n nesnenin çizgi rengi", nullptr, held);
    else
        heading = fill ? tr("Dolgu rengi — seçin, sonra nesnelere tıklayın")
                       : tr("Çizgi rengi — seçin, sonra nesnelere tıklayın");
    menu->addAction(heading)->setEnabled(false);

    // THE SWATCHES ARE RENK'S OWN WORDS (`command::named_colours`): a colour
    // picked here is the colour `RENK renk=kırmızı` paints, by construction.
    QVector<SwatchRow::Swatch> swatches;
    for (const command::NamedColour& n : command::named_colours())
        swatches.push_back(
            {QColor::fromRgba(n.rgba),
             QString::fromUtf8(n.word.data(), static_cast<qsizetype>(n.word.size()))});
    auto* row = new SwatchRow(swatches, menu);
    row->applyTheme(theme_);
    auto* host = new QWidgetAction(menu);
    host->setDefaultWidget(row);
    menu->addAction(host);
    connect(row, &SwatchRow::picked, this, [this, menu, fill, swatches](int i) {
        menu->close();
        applyColour(fill, swatches[i].name);
    });

    menu->addSeparator();
    // THE DIALOG OPENS ON THE COLOUR THE BOX SHOWS, so "a little darker" is a
    // nudge and not a search from black.
    RibbonColourBox* box = fill ? ribbonLive_->fill : ribbonLive_->stroke;
    const QColor shown   = box != nullptr ? box->shownColour() : QColor();
    const QAction* other = menu->addAction(tr("Başka bir renk…"));
    connect(other, &QAction::triggered, this, [this, fill, shown] {
        const QColor picked =
            QColorDialog::getColor(shown, this, fill ? tr("Dolgu rengi") : tr("Çizgi rengi"),
                                   QColorDialog::ShowAlphaChannel);
        if (!picked.isValid()) return;
        applyColour(fill, QString::fromStdString(command::colour_hex(picked.rgba())));
    });
    const QAction* layer = menu->addAction(fill ? tr("Katmanın dolgusu") : tr("Katmanın rengi"));
    connect(layer, &QAction::triggered, this,
            [this, fill] { applyColour(fill, QStringLiteral("katman")); });
    if (fill) {
        const QAction* none = menu->addAction(tr("Dolgu yok"));
        connect(none, &QAction::triggered, this,
                [this] { applyColour(true, QStringLiteral("yok")); });
    }

    // UNDER THE BOX, the way every ribbon list drops from its button.
    if (box != nullptr)
        menu->popup(box->mapToGlobal(box->rect().bottomLeft() + QPoint(0, 2)));
    else
        menu->popup(QCursor::pos());
}

void MainWindow::applyColour(bool fill, const QString& word)
{
    controller_->runCommand(
        QStringLiteral("RENK %1=%2")
            .arg(fill ? QStringLiteral("dolgu") : QStringLiteral("renk"), word));
}

} // namespace kentos::app
