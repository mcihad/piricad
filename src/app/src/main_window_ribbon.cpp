// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the main window's ribbon (`.claude/ui.md` R46–R50).
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
#include "piricad/app/main_window.hpp"

#include "piricad/app/app_menu.hpp"
#include "piricad/app/command_line.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/app/panels.hpp"
#include "piricad/app/print_service.hpp"
#include "piricad/app/ribbon.hpp"
#include "piricad/app/shell_chrome.hpp"
#include "piricad/app/swatch_row.hpp"
#include "piricad/app/tokens.hpp"
#include "piricad/app/tools_panel.hpp"
#include "piricad/app/widgets.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/colour.hpp"
#include "piricad/command/drawing_catalogs.hpp"
#include "piricad/command/feature_classes.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/select_modes.hpp"
#include "piricad/command/targets.hpp"
#include "piricad/core/dimension.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/entity_kind.hpp"
#include "piricad/core/hatch.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/core/style.hpp"
#include "piricad/core/text.hpp"
#include "piricad/core/text_store.hpp"
#include "piricad/processing/registry.hpp"
#include "piricad/processing/tool.hpp"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>

namespace piricad::app {

namespace {

constexpr const char* kToolCommand = kToolCommandProperty;

/// The catalogue pattern an action stands for, on the hatch galleries.
constexpr const char* kPatternProperty = "piricad.ribbon.pattern";

/// A hatch gallery's cell, the swatch and its name under it.
constexpr int kPatternCell = 58;
/// How many patterns the drawing tab's gallery shows before its arrow.
constexpr int kShownPatterns = 2;
/// The gallery's scroll buttons, the arrow that opens it and the gaps between
/// its cells: with only the buttons' 20 px counted, two cells' width showed one.
constexpr int kGalleryScroll = 44;

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

/// What a ribbon panel shows: a tool, or a family's split button.
using RibbonItem = std::variant<QAction*, RibbonFamily*>;

/// THE RIBBON'S GRAMMAR (`.claude/ui.md` R46): two sizes and one order. A
/// panel's LEADS — the one to three tools it is named for, the ones the hand
/// reaches for most in it — are large buttons, the picture over the word, at
/// the panel's left; everything else in it is a labelled ROW, three to a
/// column. There is no third size: a bare picture made a tool a guessing game,
/// and a button two rows tall was a size no rule explained.
void place_item(SARibbonPanel* panel, const RibbonItem& item, bool lead)
{
    const auto* family = std::get_if<RibbonFamily*>(&item);
    QAction* action    = family != nullptr ? (*family)->head() : std::get<QAction*>(item);
    if (action == nullptr) return;
    // A FAMILY OPENS FROM ITS ARROW, a tool runs where it is pressed.
    const QToolButton::ToolButtonPopupMode mode =
        family != nullptr ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup;
    if (lead)
        panel->addLargeAction(action, mode);
    else
        panel->addSmallAction(action, mode);
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
    // it; a second one in the tab row pushed `PiriCAD` off the left edge.
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
    // TWO SIZES AND ONE ORDER (`place_item`): a panel's leads large at its
    // left, the rest as labelled rows three to a column.
    const auto put = [&](SARibbonPanel* panel, std::initializer_list<RibbonItem> items, bool lead) {
        for (const RibbonItem& item : items) {
            place_item(panel, item, lead);
            if (std::holds_alternative<QAction*>(item)) {
                remember(std::get<QAction*>(item));
                continue;
            }
            for (QAction* m : std::get<RibbonFamily*>(item)->members())
                remember(m);
        }
    };
    const auto leads = [&](SARibbonPanel* panel, std::initializer_list<RibbonItem> items) {
        put(panel, items, true);
    };
    const auto rows = [&](SARibbonPanel* panel, std::initializer_list<RibbonItem> items) {
        put(panel, items, false);
    };
    // ONE FAMILY, ONE BUTTON (`ribbon.hpp`): the face runs the member used last
    // and the arrow lists them all. Made ONCE and placed on every tab that shows
    // it, so the button is the same one, with the same face, wherever the hand
    // meets it. A family is the ways of ONE command — the four ways to a circle of
    // `DAİRE` — and two pairs every CAD shows as one tool: Buda with Uzat
    // (Netcad's Uzat-Kes, AutoCAD's Trim/Extend) and Yuvarla with Pah. Two other
    // commands are two buttons: a ring is not a way to draw an area. `word` is
    // the button's label whichever member is on its face — `Daire`, never
    // "Daire — üç nokta" — unless the member carries a short word of its own.
    const auto makeFamily = [&](const QList<QAction*>& members, const QString& word) {
        auto* f = new RibbonFamily(members, this);
        f->setFixedLabel(word);
        families_ << f;
        return f;
    };
    const auto menuButton = [&](SARibbonPanel* panel, QMenu* menu, Glyph glyph) {
        menu->menuAction()->setData(static_cast<int>(glyph));
        panel->addLargeMenu(menu);
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
    // `PiriCAD`, where Office writes `Dosya`: what is in it is what a drawing
    // is done to as a FILE — new, open, save, import, export, print, the
    // project's settings, the program's — and the one way out. AutoCAD's
    // application menu (`app_menu.hpp`) and not a backstage page over the whole
    // window: a CAD user opens this to save and goes straight back to a drawing
    // that should never have been covered to do it.
    auto* appButton = new RibbonAppButton(tr("PiriCAD"), bar);
    appButton->setObjectName(QStringLiteral("ribbonApplicationButton"));
    appButton->setAccessibleName(tr("PiriCAD ana menüsü"));
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
    about->setObjectName(QStringLiteral("aboutPiricad"));
    about->setToolTip(tr("Sürüm, lisans ve kaynak kodu"));
    about->setData(static_cast<int>(Glyph::Info));
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    auto* appRest = new QMenu(tr("Diğer Komutlar"), this);
    appRest->setObjectName(QStringLiteral("applicationMenuRest"));
    appRest->menuAction()->setData(static_cast<int>(Glyph::More));
    actQuit_->setToolTip(tr("PiriCAD'i kapatır; kaydedilmemiş değişiklik varsa sorar"));
    appMenu_->setFooter(reference, about, actSettings_, actQuit_);

    // ---- quick access and the corner ---------------------------------------
    //
    // AutoCAD's quick access row: the file verbs a hand wants from any tab, and
    // the two that undo a mistake.
    //
    // YAZDIR WITH ITS SHEETS BESIDE IT: the printer prints, the arrow at its
    // right lists the drawing's layouts — a new one, the manager, the office's
    // templates and every sheet the drawing holds — rebuilt as it opens.
    sampleMenu_ = new QMenu(tr("Örnek Projeler"), this);
    sampleMenu_->setObjectName(QStringLiteral("sampleMenu"));
    rebuildSampleMenu();
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
    auto* rectangleRotated =
        methodTool(Glyph::RectangleRotated, tr("Dikdörtgen — döndürülmüş"),
                   QStringLiteral("DİKDÖRTGEN yontem=3n"),
                   tr("Bir kenarın iki köşesi ve yüksekliği veren üçüncü nokta"));
    auto* regularOutside = methodTool(Glyph::PolygonOutside, tr("Çokgen — dıştan"),
                                      QStringLiteral("ÇOKGEN yontem=dis"),
                                      tr("Kenarlar çembere teğet; yarıçap iç yarıçaptır"));
    auto* regularSide    = methodTool(Glyph::PolygonSide, tr("Çokgen — kenardan"),
                                      QStringLiteral("ÇOKGEN yontem=kenar"),
                                      tr("Kenar uzunluğundan; yarıçap sorulmaz"));
    auto* circleTwo      = methodTool(Glyph::CircleTwoPoint, tr("Daire — çapın iki ucu"),
                                      QStringLiteral("DAİRE yontem=2n"),
                                      tr("İki nokta çapı verir; merkez ortalarıdır"));
    auto* circleThree    = methodTool(Glyph::CircleThreePoint, tr("Daire — üç nokta"),
                                      QStringLiteral("DAİRE yontem=3n"),
                                      tr("Çevrel çember: üç noktanın hepsi çemberin üzerinde"));
    auto* circleTangent  = methodTool(Glyph::CircleTangent, tr("Daire — iki doğruya teğet"),
                                      QStringLiteral("DAİRE yontem=ttr"),
                                      tr("İki doğru, yarıçap ve dairenin geleceği köşe gösterilir"));
    auto* ellipseAxis    = methodTool(Glyph::EllipseAxis, tr("Elips — eksenin iki ucu"),
                                      QStringLiteral("ELİPS yontem=eksen"),
                                      tr("Merkez iki ucun ortasıdır; üçüncü nokta ikinci ekseni "
                                            "verir"));
    auto* arcThree =
        methodTool(Glyph::ArcThreePoint, tr("Yay — üç nokta"), QStringLiteral("YAY yontem=3n"),
                   tr("Başlangıç, üzerinden geçtiği nokta ve bitiş"));
    auto* arcAngle  = methodTool(Glyph::ArcCentreAngle, tr("Yay — başlangıç, merkez, açı"),
                                 QStringLiteral("YAY yontem=bma"),
                                 tr("Süpürme açısı oturumun birim ve kuralıyla okunur"));
    auto* arcRadius = methodTool(Glyph::ArcEndsRadius, tr("Yay — başlangıç, bitiş, yarıçap"),
                                 QStringLiteral("YAY yontem=bby"),
                                 tr("İki çözüm vardır; yon=sol|sag hangisi olduğunu söyler"));
    auto* arcOn =
        methodTool(Glyph::ArcContinue, tr("Yay — teğet devam"), QStringLiteral("YAY yontem=devam"),
                   tr("Son çizilen çizginin ya da yayın ucundan teğet devam eder"));
    auto* crossDistances =
        methodTool(Glyph::IntersectDistances, tr("Kesişim — iki mesafeden"),
                   QStringLiteral("KESİŞİMNOKTA yontem=mesafe"),
                   tr("İki bilinen noktadan ölçülen iki uzaklık; iki çözümden birini "
                      "gösterirsiniz"));
    auto* crossLines    = methodTool(Glyph::IntersectLines, tr("Kesişim — iki doğrudan"),
                                     QStringLiteral("KESİŞİMNOKTA yontem=dogru"),
                                     tr("İki doğrunun her birinden iki nokta"));
    auto* alongDistance = methodTool(Glyph::AlongDistance, tr("Ara Nokta — mesafeden"),
                                     QStringLiteral("ARANOKTA yontem=mesafe"),
                                     tr("Oran değil, ilk noktadan metre cinsinden uzaklık"));
    auto* areaByCorners = methodTool(Glyph::MeasureAreaCorners, tr("Alan Ölç — köşelerden"),
                                     QStringLiteral("ALANÖLÇ yontem=nokta"),
                                     tr("Köşelere tıklayın; alan ve çevre imleçle birlikte "
                                        "yazılır, Enter bitirir"));
    // Corners clicked, not objects picked: a selected line does not grey it.
    areaByCorners->setProperty(kIgnoresSelectionProperty, true);
    // NETCAD'S ALAN SEÇİM ARACI (wiki 217387115): the region round a click,
    // found in loose linework the way SINIR finds it.
    auto* areaInside = methodTool(Glyph::MeasureAreaInside, tr("Alan Ölç — içine tıklayarak"),
                                  QStringLiteral("ALANÖLÇ yontem=ic"),
                                  tr("Bölgenin içine tıklayın: çevreleyen çizgilerin kapattığı "
                                     "alan ölçülür, içindeki kapalı çizgiler ada olarak düşülür"));
    areaInside->setProperty(kIgnoresSelectionProperty, true);
    // NETCAD'S İLK NOKTA SABİT (wiki 217385201): every point measured from the
    // first, where Ölç measures each from the one before.
    auto* measureFixed = methodTool(
        Glyph::MeasureFixed, tr("Ölç — ilk nokta sabit"), QStringLiteral("ÖLÇ sabit=evet"),
        tr("Her nokta ilk noktadan ölçülür: bir köşenin çevresindeki yapılara "
           "uzaklıkları okumanın yolu; Enter bitirir"));
    measureFixed->setProperty(kIgnoresSelectionProperty, true);
    // THE GUIDES A HAND PLACES: through the point clicked, across or down —
    // the same guide a drag off the ruler leaves — at an angle, and the list.
    auto* guideAcross =
        methodTool(Glyph::GuideHorizontal, tr("Yatay Kılavuz"), QStringLiteral("KILAVUZ yon=yatay"),
                   tr("Tıkladığınız noktadan geçen yatay kılavuz; cetvelden "
                      "sürüklemek de koyar"));
    auto* guideDown =
        methodTool(Glyph::GuideVertical, tr("Düşey Kılavuz"), QStringLiteral("KILAVUZ yon=düşey"),
                   tr("Tıkladığınız noktadan geçen düşey kılavuz"));
    auto* guideList =
        commandAction(Glyph::GuideList, tr("Kılavuzları Listele"), QStringLiteral("KILAVUZ"),
                      tr("KILAVUZ — çizimdeki kılavuzları listeler  ·  kısaltma: "
                         "KLV"));

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

    // THE PROCESSING TOOLS a tab shows by name: each runs when pressed, on the
    // selection with its defaults, unless it needs a figure first and opens
    // where the figure is typed (`processingAction`).
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

    // NETCAD'S AREA HATCHES through its area tool (wiki 217385786): the region
    // round a click, hatched, found in loose linework the way SINIR finds it.
    auto* hatchInside = methodTool(Glyph::HatchInside, tr("Tarama — içine tıklayarak"),
                                   QStringLiteral("TARAMA yontem=ic"),
                                   tr("Bölgenin içine tıklayın: çevreleyen çizgilerin kapattığı "
                                      "alan taranır, içindeki kapalı çizgiler boş kalır"));
    hatchInside->setProperty(kIgnoresSelectionProperty, true);
    // NETCAD'S "DİĞER OBJELER SEÇ" (plan open question 18): what is selected —
    // a parcel's number, a block, a point — is left free of the pattern, and the
    // region is the one clicked. The line is built from the selection when the
    // button is pressed, so it is the one a user would type, and the journal's.
    auto* hatchExclude = new QAction(tr("Tarama — seçilenler dışarıda"), this);
    hatchExclude->setCheckable(true);
    const QString excludeTip =
        tr("Önce taramadan boş kalacak yazıları, blokları ya da noktaları seçin, sonra bölgenin "
           "içine tıklayın: seçilenler taramada boş kalır (TARAMA yontem=ic disarida=…)");
    hatchExclude->setToolTip(excludeTip);
    hatchExclude->setStatusTip(excludeTip);
    hatchExclude->setData(static_cast<int>(Glyph::HatchExclude));
    hatchExclude->setProperty(kToolCommand, QStringLiteral("TARAMA yontem=ic disarida=…"));
    hatchExclude->setProperty(kIgnoresSelectionProperty, true);
    hatchExclude->setObjectName(QStringLiteral("toolAction.TARAMA yontem=ic disarida"));
    drawingTools_->addAction(hatchExclude);
    connect(hatchExclude, &QAction::triggered, this, [this, hatchExclude] {
        QStringList keys;
        for (const core::EntityKey k : controller_->bus().selection().keys())
            keys << QStringLiteral("disarida=%1").arg(static_cast<qulonglong>(core::raw(k)));
        // NOTHING SELECTED: said where the command line answers, since that is
        // where the eye goes after a press that started nothing.
        if (keys.isEmpty()) {
            hatchExclude->setChecked(false);
            onEcho(tr("Tarama — seçilenler dışarıda: önce taramadan boş kalacak yazıları, "
                      "blokları ya da noktaları seçin, sonra bu düğmeyle bölgenin içine "
                      "tıklayın."));
            return;
        }
        // ONE SHOT: the objects left out belong to this region alone, so the
        // run re-arms nothing — a re-armed line would carry them to the next
        // region — and the selection stays, the way a grip edit leaves it.
        controller_->beginOneShot(QStringLiteral("TARAMA yontem=ic ") +
                                  keys.join(QLatin1Char(' ')));
    });

    // The two ways of `TEMİZLE`: find and mark, or repair.
    QAction* cleanFind =
        commandAction(Glyph::Cleanup, tr("Temizle — bul"), QStringLiteral("TEMİZLE"),
                      tr("TEMİZLE — yinelenen, boş ve tekrarlanan köşeli nesneleri bulur, seçer "
                         "ve işaretler; hiçbir şeyi değiştirmez  ·  kısaltma: TMZ"));
    QAction* cleanRepair =
        commandAction(Glyph::Cleanup, tr("Temizle — onar"), QStringLiteral("TEMİZLE islem=onar"),
                      tr("TEMİZLE islem=onar — yinelenenleri ve boş nesneleri siler, tekrarlanan "
                         "köşeleri çıkarır; değişen alanları önce/sonra söyler, tek adımda geri "
                         "alınır"));

    // ---- the families, made once ------------------------------------------
    RibbonFamily* circles =
        makeFamily({actCircle_, circleTwo, circleThree, circleTangent}, tr("Daire"));
    RibbonFamily* arcs = makeFamily({actArc_, arcThree, arcAngle, arcRadius, arcOn}, tr("Yay"));
    RibbonFamily* rectangles = makeFamily({actRectangle_, rectangleRotated}, tr("Dikdörtgen"));
    RibbonFamily* regulars   = makeFamily({actRegular_, regularOutside, regularSide}, tr("Çokgen"));
    RibbonFamily* ellipses   = makeFamily({actEllipse_, ellipseAxis}, tr("Elips"));
    RibbonFamily* crossings =
        makeFamily({actIntersect_, crossDistances, crossLines}, tr("Kesişim"));
    RibbonFamily* alongs  = makeFamily({actAlong_, alongDistance}, tr("Ara Nokta"));
    RibbonFamily* hatches = makeFamily({actHatch_, hatchInside, hatchExclude}, tr("Tarama"));
    RibbonFamily* guides =
        makeFamily({guideAcross, guideDown, actAngledGuide_, guideList}, tr("Kılavuz"));
    RibbonFamily* aligns   = makeFamily({actAlign_, actAlignScaled_}, tr("Hizala"));
    RibbonFamily* arrays   = makeFamily({actArray_, actArrayPolar_, actArrayPath_}, tr("Dizi"));
    RibbonFamily* measures = makeFamily({actMeasure_, measureFixed, actStationOffset_}, tr("Ölç"));
    RibbonFamily* areaMeasures =
        makeFamily({actMeasureArea_, areaByCorners, areaInside}, tr("Alan Ölç"));
    RibbonFamily* dimensions = makeFamily(dimensionTypes, tr("Ölçü"));
    RibbonFamily* cleanups   = makeFamily({cleanFind, cleanRepair}, tr("Temizle"));
    RibbonFamily* clipboards = makeFamily({actCopyClip_, actCopyBase_}, tr("Panoya Kopyala"));
    RibbonFamilies shared;
    shared.rotate = makeFamily({actRotate_, actRotateRef_}, tr("Döndür"));
    shared.scale  = makeFamily({actScale_, actScaleRef_}, tr("Ölçekle"));
    shared.mirror = makeFamily({actMirror_, actMirrorCopy_}, tr("Aynala"));
    shared.trim   = makeFamily({actTrim_, actTrimFence_, actTrimKeep_, actTrimCarry_, actExtend_,
                                actExtendFence_, actExtendCarry_},
                               tr("Buda"));
    shared.fillet =
        makeFamily({actFillet_, actFilletAll_, actChamfer_, actChamferAll_}, tr("Yuvarla"));
    shared.split = makeFamily(
        {actSplit_, actSplitPoint_, actSplitCross_, actSplitEqual_, actSplitDistance_}, tr("Böl"));
    RibbonFamily* clips =
        makeFamily({actBlockClip_, actBlockClipPolygon_, actBlockClipObject_}, tr("Kırp"));

    // =================================================================== `Giriş`
    //
    // WHAT A DRAFTER DOES ALL DAY, on the tab that opens first — AutoCAD's Home:
    // draw, change, annotate, the layer and the colours in hand, the clipboard.
    // Every tool here has its home on a tab of its own as well; this is the
    // short form, so the edit verbs are a grid of rows rather than leads.
    SARibbonCategory* home = bar->addCategoryPage(tr("Giriş"));
    home->setObjectName(QStringLiteral("ribbonHome"));
    selectFirst(home);

    // THE SHAPES A HAND DRAWS ALL DAY ARE LARGE, the area among them: a parcel
    // is drawn as often as a line, and a building as often as a parcel. Five,
    // which leaves this tab the margin a different machine's font metrics need;
    // the arc, the ellipse and the point are one row each.
    SARibbonPanel* sketch = home->addPanel(tr("Çizim"));
    leads(sketch, {actLine_, actPolyline_, actPolygon_, circles, rectangles});
    rows(sketch, {arcs, ellipses, actPoint_});
    launcher(sketch, tr("Çizim ve yakalama ayarları"),
             [this] { openSettingsSection(QStringLiteral("Çizim ve Yakalama")); });

    // THE EDIT VERBS AS A GRID OF ROWS, read down each column: move and copy,
    // turn and size, cut and round.
    SARibbonPanel* change = home->addPanel(tr("Değiştir"));
    rows(change, {actMove_, actCopy_, shared.rotate, shared.scale, shared.mirror, actOffset_,
                  shared.trim, shared.fillet, actErase_});

    SARibbonPanel* note = home->addPanel(tr("Açıklama"));
    leads(note, {actText_, dimensions});
    rows(note, {actLabel_, actLeader_});
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
    leads(layers, {layersPanel});
    ribbonLive_->layer = new RibbonLayerBox(layers);
    ribbonLive_->layer->setFixedWidth(150);
    ribbonLive_->layer->setToolTip(
        tr("Seçim yokken: yeni nesnelerin çizileceği etkin katman (KATMAN ad=…).\n"
           "Seçim varken: seçilen nesnelerin katmanı; başka bir katman seçmek onları oraya "
           "taşır (KATMANAT)."));
    connect(ribbonLive_->layer, &RibbonLayerBox::layerPicked, this, &MainWindow::pickRibbonLayer);
    layers->addMediumWidget(ribbonLive_->layer);
    // THE LIST'S OWN STRIP, as AutoCAD's layer panel has it: six pictures that
    // belong to the box above them — make active, move to active, hide,
    // isolate, show all, lock. `Görünüm ▸ Katmanlar` has them with their names.
    auto* layerStrip = new SARibbonButtonGroupWidget(layers);
    layerStrip->setObjectName(QStringLiteral("ribbonLayerStrip"));
    const QList<QAction*> layerVerbs = layerActions();
    for (const int i : {0, 1, 2, 3, 4, 6})
        layerStrip->addAction(layerVerbs.at(i));
    layers->addMediumWidget(layerStrip);
    launcher(layers, tr("Katmanlar paneli"), [this] { showLayerPanel(); });

    // THE COLOURS IN HAND AND HOW TO BORROW A LOOK: the stroke and the fill of
    // the selection (or of the active layer), each opening RENK's swatches, and
    // under them STİL KOPYALA, which copies all of a look from one object to
    // others — AutoCAD's Match Properties, under its Properties.
    SARibbonPanel* looks = home->addPanel(tr("Özellikler"));
    ribbonLive_->stroke  = new RibbonColourBox(looks);
    ribbonLive_->stroke->setObjectName(QStringLiteral("ribbonStrokeBox"));
    ribbonLive_->stroke->setAccessibleName(tr("Çizgi rengi"));
    ribbonLive_->stroke->setFixedWidth(118);
    connect(ribbonLive_->stroke, &RibbonColourBox::menuRequested, this,
            [this] { openColourMenu(0); });
    looks->addSmallWidget(captioned(looks, tr("Çizgi"), ribbonLive_->stroke, 34));
    ribbonLive_->fill = new RibbonColourBox(looks);
    ribbonLive_->fill->setObjectName(QStringLiteral("ribbonFillBox"));
    ribbonLive_->fill->setAccessibleName(tr("Dolgu rengi"));
    ribbonLive_->fill->setFixedWidth(118);
    connect(ribbonLive_->fill, &RibbonColourBox::menuRequested, this,
            [this] { openColourMenu(1); });
    looks->addSmallWidget(captioned(looks, tr("Dolgu"), ribbonLive_->fill, 34));
    rows(looks, {actStyleCopy_});
    launcher(looks, tr("Stil Tasarımcısı — etkin katmanın bütün stili"),
             [this] { openStyleDesigner(QString()); });

    SARibbonPanel* clip = home->addPanel(tr("Pano"));
    rows(clip, {actPaste_, actCut_, clipboards});

    // =================================================================== `Çizim`
    //
    // THE WHOLE OF DRAWING, one panel per kind of thing drawn: what a line runs
    // along, what closes, what a crew measures, what fills, what is placed.
    SARibbonCategory* drawTab = bar->addCategoryPage(tr("Çizim"));
    drawTab->setObjectName(QStringLiteral("ribbonDraw"));
    selectFirst(drawTab);

    SARibbonPanel* lines = drawTab->addPanel(tr("Çizgi ve Eğri"));
    leads(lines, {actLine_, actPolyline_, arcs});
    rows(lines, {actSpline_, ellipses});

    SARibbonPanel* shapes = drawTab->addPanel(tr("Kapalı Şekil"));
    leads(shapes, {actPolygon_, rectangles, circles});
    rows(shapes, {regulars, actSector_, actAnnulus_});

    // THE SURVEY ENTRIES, where a drawing actually starts for a crew with a
    // tape: this is the first tool a Turkish surveyor reaches for, not an
    // occasional one (TODOS-CAD P1b). POLİGON is Harita's, with the geodesy.
    SARibbonPanel* points = drawTab->addPanel(tr("Nokta ve Alım"));
    leads(points, {actSurvey_});
    rows(points, {actPoint_, actPerpOffset_, crossings, alongs});

    // THE PATTERNS AS THEY LOOK, from the catalogue: a click starts TARAMA with
    // that pattern and asks for the boundary; the Tarama button itself uses the
    // last one given.
    SARibbonPanel* fills = drawTab->addPanel(tr("Tarama"));
    leads(fills, {hatches});
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
        group->setGridMinimumWidth(kPatternCell);
        group->setGridMaximumWidth(kPatternCell);
        gallery->setCurrentViewGroup(group);
        // TWO PATTERNS SHOW, so the row reads as a choice and the room goes to the
        // two tools beside it; the arrow under the scroll buttons opens them all.
        gallery->setFixedWidth((kShownPatterns * kPatternCell) + kGalleryScroll);
    }
    // THE TWO THINGS DONE WITH A HATCH'S EDGE: edit one that is drawn (a click
    // asks which — it is also the Tarama editor tab's, which comes forward with a
    // picked hatch) and find the boundary a hatch needs.
    rows(fills, {actHatchEdit_, actBoundary_});

    // EXPLODE is the modify tab's and the external reference the map tab's.
    SARibbonPanel* blocks = drawTab->addPanel(tr("Blok"));
    leads(blocks, {actInsert_});
    rows(blocks, {actBlockLibrary_, actBlock_, clips});

    // ================================================================ `Değiştir`
    //
    // THE WHOLE OF CHANGING, by what is changed: where an object is, how many
    // there are, where it ends, its corners, what it is made of, what it covers.
    SARibbonCategory* modifyTab = bar->addCategoryPage(tr("Değiştir"));
    modifyTab->setObjectName(QStringLiteral("ribbonModify"));
    selectFirst(modifyTab);

    SARibbonPanel* moves = modifyTab->addPanel(tr("Dönüştür"));
    leads(moves, {actMove_});
    rows(moves, {shared.rotate, shared.scale, shared.mirror, aligns, actStretch_});

    // WHAT MAKES MORE OF IT: a copy, a parallel, a pattern of copies.
    SARibbonPanel* copies = modifyTab->addPanel(tr("Çoğalt"));
    leads(copies, {actCopy_, actOffset_, arrays});

    SARibbonPanel* cuts = modifyTab->addPanel(tr("Kes ve Uzat"));
    leads(cuts, {shared.trim, shared.split});
    rows(cuts, {actBreak_, actLengthen_, actDivide_});

    SARibbonPanel* corners = modifyTab->addPanel(tr("Köşe ve Kenar"));
    leads(corners, {shared.fillet});
    rows(corners,
         {actVertexMove_, actVertexAdd_, actVertexDelete_, actEdgeKind_, actPolylineEdit_});

    SARibbonPanel* joins = modifyTab->addPanel(tr("Birleştir ve Ayır"));
    leads(joins, {actCombine_});
    rows(joins, {actJoin_, actToArea_, actExplode_});

    // THE FOUR AREA OPERATIONS, each its own button: FARK's order is said by
    // its tip and by the prompt the four share.
    SARibbonPanel* areaBoolean = modifyTab->addPanel(tr("Alan İşlemleri"));
    areaBoolean->setObjectName(QStringLiteral("ribbonModifyAreaBoolean"));
    leads(areaBoolean, {actAreaUnion_});
    rows(areaBoolean, {actAreaIntersection_, actAreaDifference_, actAreaSymdifference_});

    SARibbonPanel* tidy = modifyTab->addPanel(tr("Sil ve Temizle"));
    leads(tidy, {actErase_});
    rows(tidy, {cleanups});

    // =============================================================== `Açıklama`
    SARibbonCategory* annotateTab = bar->addCategoryPage(tr("Açıklama"));
    annotateTab->setObjectName(QStringLiteral("ribbonAnnotate"));
    selectFirst(annotateTab);

    // THE DEFAULTS the annotation commands fall back to, in the ribbon where
    // AutoCAD keeps its text and dimension styles: the height a new METİN gets
    // and the style a new ÖLÇÜ is drawn in. Each is a project setting (AYAR).
    SARibbonPanel* words = annotateTab->addPanel(tr("Yazı"));
    leads(words, {actText_});
    rows(words, {actTextEdit_, actFindReplace_});
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

    SARibbonPanel* measuresPanel = annotateTab->addPanel(tr("Ölçü"));
    leads(measuresPanel, {dimensions});
    rows(measuresPanel, {actDimChain_, actDimBaseline_, actDimensionEdit_});
    ribbonLive_->dimStyleDefault = new ComboBox(measuresPanel);
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
    measuresPanel->addSmallWidget(
        captioned(measuresPanel, tr("Stil"), ribbonLive_->dimStyleDefault, 28));
    rows(
        measuresPanel,
        {commandAction(Glyph::DimStyle, tr("Ölçü Stilleri"), QStringLiteral("ÖLÇÜSTİLİ"),
                       tr("ÖLÇÜSTİLİ — ölçü stillerini kâğıttaki ve bu paftadaki boylarıyla "
                          "listeler; varsayılanı AYAR ölçü_stili değiştirir  ·  kısaltma: ÖST")),
         commandAction(Glyph::DimRefresh, tr("Pafta Ölçeğine Uyarla"), QStringLiteral("ÖLÇÜYENİLE"),
                       tr("ÖLÇÜYENİLE — çizimin bütün ölçülerini plan ölçeğine uyarlar: oklar, "
                          "uzatma çizgileri ve yazılar kâğıtta stilin boyunda kalır; yazılar "
                          "çizimin birimiyle yeniden yazılır  ·  kısaltma: ÖYN"))});
    launcher(measuresPanel, tr("Ölçü stili ve pafta ölçeği ayarları"),
             [this] { openSettingsSection(QStringLiteral("Plot ve Çıktı")); });

    SARibbonPanel* tags = annotateTab->addPanel(tr("Etiket"));
    leads(tags, {actLabel_, actLeader_});
    rows(tags, {bindText, unbindText, writeLengths});

    // ================================================================ `Kadastro`
    //
    // WHAT A PARCEL GOES THROUGH: cut and merged and brought to its deed area,
    // its corners numbered and its edges written, its sheet checked.
    SARibbonCategory* cadastreTab = bar->addCategoryPage(tr("Kadastro"));
    cadastreTab->setObjectName(QStringLiteral("ribbonCadastre"));
    selectFirst(cadastreTab);

    SARibbonPanel* parcels = cadastreTab->addPanel(tr("Parsel"));
    leads(parcels, {actParcelSplit_, actAreaSplit_, actUnion_});
    rows(parcels, {editArea, makeAreas});

    SARibbonPanel* marks = cadastreTab->addPanel(tr("Yazım"));
    leads(marks, {numberCorners, writeLengths, actLabel_});

    SARibbonPanel* checks = cadastreTab->addPanel(tr("Denetim"));
    leads(checks, {actTopology_});
    rows(checks, {areaMeasures, bufferZone});

    // ================================================================== `Harita`
    SARibbonCategory* mapTab = bar->addCategoryPage(tr("Harita"));
    mapTab->setObjectName(QStringLiteral("ribbonMap"));
    selectFirst(mapTab);

    SARibbonPanel* ask = mapTab->addPanel(tr("Sorgu"));
    leads(ask, {actIdentify_});
    rows(ask, {actEntityInfo_, actCoordinate_});

    // ÖLÇ'S FAMILY, as the plan names it: the run, the star from a held first
    // point, and PRİZMA — three ways of reading distances off points.
    SARibbonPanel* tape = mapTab->addPanel(tr("Ölçüm"));
    leads(tape, {measures});
    rows(tape, {areaMeasures, actMeasureAngle_});

    SARibbonPanel* geodesy = mapTab->addPanel(tr("Jeodezi"));
    leads(geodesy, {actTraverse_});
    rows(geodesy, {actStakeout_,
                   commandAction(Glyph::Helmert, tr("Oturt (Helmert)"), QStringLiteral("OTURT"),
                                 tr("OTURT — ortak noktalardan Helmert dönüşümüyle çizimi "
                                    "oturtur  ·  kısaltma: OTR")),
                   commandAction(Glyph::Globe, tr("Dönüştür"), QStringLiteral("DÖNÜŞTÜR"),
                                 tr("DÖNÜŞTÜR — çizimi başka bir koordinat sistemine "
                                    "dönüştürür  ·  kısaltma: DNS"))});
    launcher(geodesy, tr("Koordinat sistemi ayarları"),
             [this] { openSettingsSection(QStringLiteral("Koordinat Sistemleri")); });

    SARibbonPanel* ground = mapTab->addPanel(tr("Arazi"));
    leads(ground, {commandAction(Glyph::Contour, tr("Eşyükselti"), QStringLiteral("EŞYÜKSELTİ"),
                                 tr("EŞYÜKSELTİ — kotlu noktalardan eş yükselti eğrileri çizer  ·  "
                                    "kısaltma: EŞY")),
                   commandAction(Glyph::Volume, tr("Hacim"), QStringLiteral("HACİM"),
                                 tr("HACİM — iki yüzey arasındaki kazı ve dolgu hacmi  ·  "
                                    "kısaltma: HCM"))});

    // THE PEN (TODOS G-04), first on the drawing tab because it is the first thing a digitiser
    // picks — and not on `Giriş`, whose row is already as wide as the narrowest window allows: what
    // is being drawn, as a surveyor says it — a building, a road centreline, a parcel — rather than
    // a layer and a list of columns. Picking one runs `KALEM`, which builds the layer and its
    // fields and makes it the active one; whatever is drawn next starts with the class's defaults.
    // The two verbs beside it bring what is already drawn under the class in hand and ask whether
    // everything on a class layer still is what it says.
    SARibbonPanel* pen = mapTab->addPanel(tr("Kalem"));
    ribbonLive_->pen   = new ComboBox(pen);
    ribbonLive_->pen->setObjectName(QStringLiteral("ribbonPen"));
    ribbonLive_->pen->setControlSize(ControlSize::Compact);
    ribbonLive_->pen->setFixedWidth(150);
    ribbonLive_->pen->setAccessibleName(tr("Sayısallaştırma kalemi"));
    ribbonLive_->pen->setToolTip(
        tr("Sayısallaştırma kalemi: Bina, Yol ekseni, Parsel… Birini seçince katmanı ve alanları "
           "kurulur, etkin olur ve çizdiğiniz nesne sınıfın varsayılanlarıyla başlar (KALEM)."));
    connect(ribbonLive_->pen, &QComboBox::activated, this, [this](int index) {
        const QString id = ribbonLive_->pen->itemData(index).toString();
        if (!id.isEmpty())
            controller_->runLine(QStringLiteral("KALEM \"%1\"").arg(id), command::Origin::Gui);
    });
    pen->addSmallWidget(ribbonLive_->pen);
    auto* penBind = new QAction(tr("Seçimi bağla"), this);
    penBind->setObjectName(QStringLiteral("ribbonPenBind"));
    penBind->setData(static_cast<int>(Glyph::Attach));
    penBind->setToolTip(
        tr("Seçili nesneleri kalemdeki sınıfa bağlar: katmana alır, varsayılanları doldurur; "
           "uymayanları atlar (KALEMBAĞLA)"));
    connect(penBind, &QAction::triggered, this, [this] {
        const QString id = ribbonLive_->pen->currentData().toString();
        if (id.isEmpty()) {
            onEcho(tr("Önce bir kalem seçin; sonra seçilen nesneler o sınıfa bağlanır."));
            return;
        }
        controller_->runLine(QStringLiteral("KALEMBAĞLA ad=\"%1\"").arg(id), command::Origin::Gui);
    });
    auto* penCheck = new QAction(tr("Sınıfı denetle"), this);
    penCheck->setObjectName(QStringLiteral("ribbonPenCheck"));
    penCheck->setData(static_cast<int>(Glyph::Check));
    penCheck->setToolTip(tr("Bir sınıfı izleyen katmanlardaki nesnelerin hâlâ sınıfın dediği gibi "
                            "olup olmadığına bakar (KALEMDENETİM)"));
    connect(penCheck, &QAction::triggered, this,
            [this] { controller_->runLine(QStringLiteral("KALEMDENETİM"), command::Origin::Gui); });
    rows(pen, {penBind, penCheck});

    SARibbonPanel* sources = mapTab->addPanel(tr("Veri"));
    leads(sources, {actDatabase_});
    rows(sources, {actImport_, actExport_});

    SARibbonPanel* references = mapTab->addPanel(tr("Dış Referans"));
    leads(references, {actXref_});
    rows(references, {actXrefReload_, actLocalCopy_, clips});

    // ================================================================== `Analiz`
    SARibbonCategory* analyseTab = bar->addCategoryPage(tr("Analiz"));
    analyseTab->setObjectName(QStringLiteral("ribbonAnalyse"));
    selectFirst(analyseTab);

    SARibbonPanel* tables = analyseTab->addPanel(tr("Tablo"));
    leads(tables, {actTable_});

    // THE PROCESSING TOOLS, one row each from the processing registry — the
    // list `src/processing/src/registry.cpp` declares, never a second one.
    SARibbonPanel* process = analyseTab->addPanel(tr("İşlem araçları"));
    auto* tools            = new QMenu(tr("İşlem Araçları"), this);
    tools->setObjectName(QStringLiteral("processingMenu"));
    for (const processing::ProcessingTool* each : processing::processing_tools()) {
        const auto& spec = each->spec();
        tools->addAction(processingAction(QString::fromStdString(spec.id), QString()));
    }
    menuButton(process, tools, Glyph::Toolbox);
    auto* showTools = new QAction(tr("Araçlar Paneli"), this);
    showTools->setData(static_cast<int>(Glyph::Tune));
    showTools->setStatusTip(tr("Sağ paneldeki Araçlar sekmesini açar"));
    connect(showTools, &QAction::triggered, this, [this] { showToolsPanel(QString()); });
    rows(process, {showTools, bufferZone, makeAreas});

    // WHAT THE DRAWING AND THE TOOLS MADE, asked whether it still holds: the
    // topology of the faces, the outputs that went stale (TODOS F-04), the
    // objects far from all the rest.
    SARibbonPanel* audit = analyseTab->addPanel(tr("Denetim"));
    leads(audit, {actTopology_, actDependency_});
    rows(audit, {actDependencyRefresh_, actExtentCheck_});

    SARibbonPanel* agents = analyseTab->addPanel(tr("Yapay zekâ"));
    leads(agents, {actAi_});
    actMcp_ = new QAction(tr("MCP Sunucusunu Başlat"), this);
    actMcp_->setData(static_cast<int>(Glyph::Server));
    actMcp_->setStatusTip(tr("Yapay zeka ajanlarının bağlanacağı yerel sunucuyu açar"));
    actMcp_->setProperty(kToolCommand, QStringLiteral("MCPSUNUCU"));
    connect(actMcp_, &QAction::triggered, this, [this] {
#if PIRICAD_HAVE_MCP
        const bool up =
            controller_->mcpService() != nullptr && controller_->mcpService()->listening();
        controller_->runLine(up ? QStringLiteral("MCPSUNUCU islem=durdur")
                                : QStringLiteral("MCPSUNUCU islem=baslat"),
                             command::Origin::Gui);
#else
        onEcho(tr("Bu yapıda MCP sunucusu yok (PIRICAD_WITH_MCP kapalı)."));
#endif
    });
    auto* mcpToken = new QAction(tr("MCP Belirteci Üret"), this);
    mcpToken->setData(static_cast<int>(Glyph::Lock));
    mcpToken->setStatusTip(tr("Yeni bir erişim belirteci üretir; eskisi geçersiz olur"));
    connect(mcpToken, &QAction::triggered, this, [this] {
        controller_->runLine(QStringLiteral("MCPSUNUCU islem=belirtec"), command::Origin::Gui);
    });
    rows(agents, {actMcp_, mcpToken});
    launcher(agents, tr("Yapay zeka modelleri"),
             [this] { openSettingsSection(QStringLiteral("Yapay Zeka Modelleri")); });

    // ================================================================= `Görünüm`
    SARibbonCategory* viewTab = bar->addCategoryPage(tr("Görünüm"));
    viewTab->setObjectName(QStringLiteral("ribbonView"));
    selectFirst(viewTab);

    // THE PANEL'S OWN WORDS ON ITS BUTTONS — Pencere, Seçime, Önceki, Sonraki —
    // the plan's names for them; a menu row and a tooltip still say the whole
    // "Önceki Görünüm" (`QAction::iconText`).
    SARibbonPanel* navigate = viewTab->addPanel(tr("Gezinme"));
    actViewWindow_->setIconText(tr("Pencere"));
    actZoomSelection_->setIconText(tr("Seçime"));
    actViewPrevious_->setIconText(tr("Önceki"));
    actViewNext_->setIconText(tr("Sonraki"));
    leads(navigate, {actZoomExtents_});
    rows(navigate, {actViewWindow_, actZoomSelection_, actPan_, actViewPrevious_, actViewNext_,
                    actZoomIn_, actZoomOut_});

    // THE DRAFTING AIDS: what the cursor catches, how it is held, and the
    // guides a hand places to draw against.
    SARibbonPanel* aids = viewTab->addPanel(tr("Yardımcılar"));
    // The keyboard road to the same list the OSNAP chip's right click opens.
    // ui.md P7: nothing ships reachable only by mouse.
    auto* snapModes = new QAction(tr("Yakalama Modları…"), this);
    snapModes->setData(static_cast<int>(Glyph::Snap));
    snapModes->setToolTip(tr("Hangi nesne yakalama modlarının açık olduğunu seçer"));
    snapModes->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(snapModes, &QAction::triggered, this, &MainWindow::openSnapModes);
    addAction(snapModes); // so the shortcut works with focus anywhere in the shell
    leads(aids, {actSnap_});
    rows(aids, {snapModes, actOrtho_, actNormal_, actGridSnap_, guides});
    launcher(aids, tr("Yakalama modları"), [this] { openSnapModes(); });

    // WHAT IS SEEN, not what is edited: the verbs that change which layers
    // show. Making one active, moving objects onto it and locking it are
    // `Giriş`'s, beside the layer list.
    SARibbonPanel* layerView = viewTab->addPanel(tr("Katmanlar"));
    leads(layerView, {layersPanel});
    rows(layerView,
         {layerVerbs.at(2), layerVerbs.at(3), layerVerbs.at(4), layerVerbs.at(5), actStyle_});

    // THE PANELS, as one list: each a switch, ticked while it is open, and the
    // way back to the layout the program starts with.
    SARibbonPanel* windows = viewTab->addPanel(tr("Pencereler"));
    auto* panelsMenu       = new QMenu(tr("Paneller"), this);
    panelsMenu->setObjectName(QStringLiteral("ribbonPanelsMenu"));
    const std::pair<QDockWidget*, Glyph> docks[] = {{layerDock_, Glyph::Layer},
                                                    {propertyDock_, Glyph::Table},
                                                    {chatDock_, Glyph::Chat},
                                                    {journalDock_, Glyph::History},
                                                    {pythonDock_, Glyph::Script}};
    for (const auto& [dock, glyph] : docks)
        if (dock != nullptr) {
            dock->toggleViewAction()->setData(static_cast<int>(glyph));
            panelsMenu->addAction(dock->toggleViewAction());
        }
    panelsMenu->addAction(actCommandLine_);
    panelsMenu->addSeparator();
    auto* reset = new QAction(tr("Yerleşimi Sıfırla"), this);
    reset->setData(static_cast<int>(Glyph::Refresh));
    connect(reset, &QAction::triggered, this, &MainWindow::resetLayout);
    panelsMenu->addAction(reset);
    panelsMenu->menuAction()->setToolTip(
        tr("Panelleri açar ya da kapatır: Katmanlar, Öznitelikler, Yapay Zeka, Komut Günlüğü, "
           "Python Konsolu, Komut Satırı; Yerleşimi Sıfırla başlangıç düzenine döner"));
    menuButton(windows, panelsMenu, Glyph::SplitView);

    SARibbonPanel* look = viewTab->addPanel(tr("Tema"));
    leads(look, {actTheme_});
    rows(look, {actHud_});
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
    // THE SHEETS, AS ONE WORD ON THE BUTTON: "Çıktı Yerleşimleri" broke over the
    // button's two lines as "Çıktı / erleşimle", its first letter lost under the
    // arrow. A menu of its own, filled from the layout menu each time it opens,
    // for the reason the quick access printer has one (the layout menu's own
    // action would be shown twice).
    auto* layoutsHere = new QMenu(this);
    layoutsHere->setObjectName(QStringLiteral("ribbonLayoutMenu"));
    connect(layoutsHere, &QMenu::aboutToShow, this, [this, layoutsHere] {
        rebuildLayoutMenu();
        layoutsHere->clear();
        layoutsHere->addActions(layoutMenu_->actions());
    });
    auto* layoutsHead = new QAction(tr("Yerleşimler"), this);
    layoutsHead->setObjectName(QStringLiteral("ribbonLayouts"));
    layoutsHead->setData(static_cast<int>(Glyph::Layout));
    layoutsHead->setToolTip(tr("Çıktı yerleşimleri: çizimin başlıklı, lejantlı pafta düzenleri, "
                               "yeni bir yerleşim, yönetici ve şablonlar"));
    layoutsHead->setMenu(layoutsHere);
    sheets->addLargeAction(layoutsHead, QToolButton::InstantPopup);
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
    leads(files, {actExport_});
    rows(files, {actSave_, actSaveAs_});

    buildContextTabs(bar, shared);

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
        menuButton(panel, menu, Glyph::ChevronDown);
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
    // HAZIR ÖRNEKLER, beside the verbs that open a drawing: the answer to "where do I
    // start" for someone who has nothing to open (TODOS U-06). The list is the one
    // `ÖRNEKPROJE` reads, rebuilt whenever it is shown.
    sampleMenu_->menuAction()->setData(static_cast<int>(Glyph::Terrain));
    sampleMenu_->menuAction()->setToolTip(
        tr("ÖRNEKPROJE — hazır bir örnekle başlayın: ölçüden harita, parsel düzenleme, plan, "
           "GIS, aplikasyon"));
    verb(sampleMenu_->menuAction(), tr("Hazır bir işle başlar"), false, [this] {
        rebuildSampleMenu();
        return sampleMenu_->actions();
    });
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
    verb(actScriptPreview_, tr("Bir betiğin ne değiştireceğini çalıştırmadan söyler"));
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

    gatherTargetTools();
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

std::optional<RibbonContext> ribbon_context_of(const core::Document& doc, core::EntityId e)
{
    const core::KindId kind   = doc.entities().kind[e];
    const std::uint32_t gslot = doc.entities().slot[e];
    if (kind == core::kDimensionKind) return RibbonContext::Dimension;
    if (kind == core::kHatchKind) return RibbonContext::Hatch;
    if (kind == core::kBlockReferenceKind) return RibbonContext::Block;
    if (doc.texts().has(gslot)) return RibbonContext::Text;
    // THE CLASS THE COMMANDS ARE DECLARED FOR (`command::target_of`), so the tab
    // a parcel brings up holds exactly the tools that are offered for it — a
    // circle is a curve, not an area, and İFRAZ does not take one.
    switch (command::target_of(doc, e)) {
    case command::Targets::Faces: return RibbonContext::Area;
    case command::Targets::Lines: return RibbonContext::Line;
    case command::Targets::Curves: return RibbonContext::Curve;
    default: break;
    }
    return std::nullopt;
}

void MainWindow::buildContextTabs(SARibbonBar* bar, const RibbonFamilies& families)
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
    // THE SAME GRAMMAR AS EVERY OTHER TAB (`place_item`): leads large, the rest
    // as labelled rows three to a column.
    const auto leads = [](SARibbonPanel* panel, std::initializer_list<RibbonItem> items) {
        for (const RibbonItem& item : items)
            place_item(panel, item, true);
    };
    const auto rows = [](SARibbonPanel* panel, std::initializer_list<RibbonItem> items) {
        for (const RibbonItem& item : items)
            place_item(panel, item, false);
    };
    // Straight to the tool, on the selection with its defaults (`processingAction`).
    const auto tool = [this](const char* id, const QString& word) {
        return processingAction(QString::fromLatin1(id), word);
    };

    // EVERY EDITOR TAB HAS ONE SKELETON, so the hand finds the same thing in the
    // same place whichever kind it picked:
    //
    //   Seç | Nesne | what is done to THIS kind | Kapat
    //
    // NESNE COMES SECOND, ON EVERY TAB: move, copy, rotate, scale, mirror and
    // erase are what any selection is also made for, so a tab coming forward
    // takes nothing from the hand — and a caption, a dimension, a hatch and a
    // block are moved far more often than they are restyled.
    //
    // WHAT FOLLOWS ACTS ON THE SELECTED KIND, and nothing else does. A tab holds
    // only commands that declare the kind among their targets (`CommandSpec::
    // targets`, the class `command::target_of` gives the object), and a command
    // whose press does not read the selection has no place on one: `Koordinat Oku`
    // reads a clicked point, `Sınır Bul` a clicked region, `Blok Ekle` places
    // another block, `Bul ve Değiştir` searches the whole drawing. They were here,
    // and the user's word for a tab that offered them was "alakasız".
    const auto context = [&](RibbonContext which, const QString& group, const QString& title,
                             const QColor& colour) {
        SARibbonContextCategory* ctx =
            bar->addContextCategory(group, colour, static_cast<int>(which));
        ribbonLive_->contexts[static_cast<std::size_t>(which)] = ctx;
        SARibbonCategory* page                                 = ctx->addCategoryPage(title);
        page->setObjectName(QStringLiteral("ribbonContext.%1").arg(static_cast<int>(which)));
        if (selectFirst_) selectFirst_(page);
        SARibbonPanel* verbs = page->addPanel(tr("Nesne"));
        verbs->setObjectName(QStringLiteral("ribbonObjectVerbs"));
        rows(verbs,
             {actMove_, actCopy_, families.rotate, families.scale, families.mirror, actErase_});
        return page;
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

    // ---------------------------------------------------------------- `Yazı`
    SARibbonCategory* text =
        context(RibbonContext::Text, tr("Yazı Araçları"), tr("Yazı"), t.accent);
    SARibbonPanel* textEdit = text->addPanel(tr("Düzenle"));
    leads(textEdit, {actTextEdit_});
    rows(textEdit, {actStyleCopy_});
    ribbonLive_->editors[static_cast<std::size_t>(RibbonContext::Text)] = actTextEdit_;

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
            a->setProperty("piricad.anchor.col", col);
            a->setProperty("piricad.anchor.row", row);
            anchors->addAction(a);
            connect(a, &QAction::triggered, this, [onSelection, word] {
                onSelection(QStringLiteral("YAZIDÜZENLE hizalama=%1").arg(word));
            });
            strip->addAction(a);
            ribbonLive_->textAnchors[static_cast<int>(anchor)] = a;
        }
        textAnchor->addSmallWidget(strip);
    }

    // WHAT THE CAPTION IS TIED TO: an edge or a parcel it names, so it follows
    // when that one moves.
    SARibbonPanel* textBind = text->addPanel(tr("Bağ"));
    rows(textBind, {tool("islem.bagla", tr("Bağla")), tool("islem.bag_coz", tr("Bağı Çöz"))});
    closer(text);

    // ---------------------------------------------------------------- `Ölçü`
    SARibbonCategory* dim =
        context(RibbonContext::Dimension, tr("Ölçü Araçları"), tr("Ölçü"), t.accent);
    SARibbonPanel* dimEdit = dim->addPanel(tr("Düzenle"));
    leads(dimEdit, {actDimensionEdit_});
    ribbonLive_->editors[static_cast<std::size_t>(RibbonContext::Dimension)] = actDimensionEdit_;
    auto* dimReset = new QAction(tr("Stile Döndür"), this);
    dimReset->setObjectName(QStringLiteral("ribbonDimReset"));
    dimReset->setData(static_cast<int>(Glyph::Refresh));
    dimReset->setToolTip(tr("ÖLÇÜDÜZENLE sifirla=hepsi — yazı, önek, sonek, tolerans, birim, "
                            "ondalık ve yazı yeri stile döner"));
    dimReset->setProperty(kToolCommandProperty, QStringLiteral("ÖLÇÜDÜZENLE"));
    connect(dimReset, &QAction::triggered, this,
            [onSelection] { onSelection(QStringLiteral("ÖLÇÜDÜZENLE sifirla=hepsi")); });
    auto* dimRefresh = new QAction(tr("Pafta Ölçeğine Uyarla"), this);
    dimRefresh->setObjectName(QStringLiteral("ribbonDimRefresh"));
    dimRefresh->setData(static_cast<int>(Glyph::DimRefresh));
    dimRefresh->setToolTip(tr("ÖLÇÜYENİLE — seçili ölçüleri plan ölçeğine uyarlar"));
    dimRefresh->setProperty(kToolCommandProperty, QStringLiteral("ÖLÇÜYENİLE"));
    connect(dimRefresh, &QAction::triggered, this, [this, onSelection] {
        onSelection(QStringLiteral("ÖLÇÜYENİLE") + selectionArgs(core::kDimensionKind));
    });
    rows(dimEdit, {dimReset, dimRefresh});

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

    // THE NEXT DIMENSION, carried on from the selected one.
    SARibbonPanel* dimMore = dim->addPanel(tr("Devam"));
    rows(dimMore, {actDimChain_, actDimBaseline_});
    closer(dim);

    // -------------------------------------------------------------- `Tarama`
    SARibbonCategory* hatch =
        context(RibbonContext::Hatch, tr("Tarama Araçları"), tr("Tarama"), t.accent);
    ribbonLive_->editors[static_cast<std::size_t>(RibbonContext::Hatch)] = actHatchEdit_;
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
        group->setGridMinimumWidth(kPatternCell);
        group->setGridMaximumWidth(kPatternCell);
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
    rows(hatchLook, {ribbonLive_->hatchCross});

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
        rows(islands, {a});
        ribbonLive_->hatchIslands << a;
    }

    // THE HATCH ITSELF: edited in one go, and its area read — a hatch covers
    // the ground its pattern fills.
    SARibbonPanel* hatchEdit = hatch->addPanel(tr("Düzenle"));
    leads(hatchEdit, {actHatchEdit_});
    rows(hatchEdit, {actMeasureArea_, actEntityInfo_});
    closer(hatch);

    // ---------------------------------------------------------------- `Alan`
    //
    // WHAT IS DONE TO A CLOSED AREA ONCE IT IS PICKED — a parcel, a building, a
    // fenced plot — each on the picked area and in one press: its corners and
    // edges reshaped, cut as a parcel, combined with another, offset or
    // hatched, its area read and its corners and edges written. A tool that
    // needs a figure first (TAMPON's distance, ALANDÜZENLE's target area) says
    // so with `…` and opens where the figure is typed.
    SARibbonCategory* area =
        context(RibbonContext::Area, tr("Alan Araçları"), tr("Alan"), t.accent);
    SARibbonPanel* areaCorners = area->addPanel(tr("Köşe ve Kenar"));
    areaCorners->setObjectName(QStringLiteral("ribbonAreaCorners"));
    leads(areaCorners, {families.fillet});
    rows(areaCorners,
         {actVertexMove_, actVertexAdd_, actVertexDelete_, actEdgeKind_, actPolylineEdit_});
    SARibbonPanel* areaParcel = area->addPanel(tr("Parsel"));
    leads(areaParcel, {actParcelSplit_, families.split});
    rows(areaParcel, {actAreaSplit_, actUnion_, tool("islem.alan_duzenle", tr("Alanı Düzenle"))});
    SARibbonPanel* areaBoolean = area->addPanel(tr("Alan İşlemleri"));
    areaBoolean->setObjectName(QStringLiteral("ribbonContextAreaBoolean"));
    leads(areaBoolean, {actAreaUnion_});
    rows(areaBoolean, {actAreaIntersection_, actAreaDifference_, actAreaSymdifference_});
    SARibbonPanel* areaMake = area->addPanel(tr("Dönüştür"));
    leads(areaMake, {actOffset_});
    rows(areaMake, {tool("islem.tampon", tr("Tampon")), actHatch_, actExplode_});
    // READ, CHECKED AND WRITTEN: its area, what it is, whether it overlaps or
    // leaves a gap against its neighbours — TOPOLOJİ checks the selection when
    // there is one — and its corners and edges written on the sheet.
    SARibbonPanel* areaRead = area->addPanel(tr("Ölç ve Yaz"));
    leads(areaRead, {actMeasureArea_});
    rows(areaRead, {actEntityInfo_, actTopology_, tool("islem.kose_numarala", tr("Köşe Numarala")),
                    tool("islem.uzunluk_yaz", tr("Uzunluk Yaz"))});
    launcher(areaRead, tr("Numaralama ve uzunluk yazma ayarları — Araçlar paneli"),
             [this] { showToolsPanel(QStringLiteral("islem.kose_numarala")); });
    closer(area);

    // --------------------------------------------------------------- `Çizgi`
    //
    // AN OPEN LINE, and what is done to one once it is picked: cut and carried
    // on, its corners and edges reshaped, closed into an area, offset, written.
    // Buda and Uzat take the picked line as the edge to cut or reach to, the way
    // every CAD reads a selection made before them.
    SARibbonCategory* lineTab =
        context(RibbonContext::Line, tr("Çizgi Araçları"), tr("Çizgi"), t.accent);
    SARibbonPanel* lineCut = lineTab->addPanel(tr("Kes ve Uzat"));
    lineCut->setObjectName(QStringLiteral("ribbonLineCut"));
    leads(lineCut, {families.trim, families.split});
    rows(lineCut, {actBreak_, actLengthen_, actDivide_});
    SARibbonPanel* lineCorner = lineTab->addPanel(tr("Köşe ve Kenar"));
    leads(lineCorner, {families.fillet});
    rows(lineCorner,
         {actVertexMove_, actVertexAdd_, actVertexDelete_, actEdgeKind_, actPolylineEdit_});
    SARibbonPanel* lineMake = lineTab->addPanel(tr("Dönüştür"));
    leads(lineMake, {actOffset_, actToArea_});
    rows(lineMake, {actJoin_, tool("islem.tampon", tr("Tampon")), actExplode_});
    SARibbonPanel* lineRead = lineTab->addPanel(tr("Ölç ve Yaz"));
    rows(lineRead, {actEntityInfo_, tool("islem.uzunluk_yaz", tr("Uzunluk Yaz")),
                    tool("islem.kose_numarala", tr("Köşe Numarala"))});
    closer(lineTab);

    // ---------------------------------------------------------------- `Eğri`
    //
    // A CIRCLE, AN ARC, AN ELLIPSE OR A SPLINE: cut and carried on, rounded
    // against what it meets, combined as an area when it closes, offset —
    // never numbered at the corners it does not have.
    SARibbonCategory* curve =
        context(RibbonContext::Curve, tr("Eğri Araçları"), tr("Eğri"), t.accent);
    SARibbonPanel* curveCut = curve->addPanel(tr("Kes ve Uzat"));
    curveCut->setObjectName(QStringLiteral("ribbonCurveCut"));
    leads(curveCut, {families.trim, families.split});
    rows(curveCut, {actBreak_, actLengthen_, actDivide_});
    SARibbonPanel* curveJoin = curve->addPanel(tr("Köşe ve Birleştir"));
    leads(curveJoin, {families.fillet});
    rows(curveJoin, {actJoin_});
    SARibbonPanel* curveBoolean = curve->addPanel(tr("Alan İşlemleri"));
    leads(curveBoolean, {actAreaUnion_});
    rows(curveBoolean, {actAreaIntersection_, actAreaDifference_, actAreaSymdifference_});
    SARibbonPanel* curveMake = curve->addPanel(tr("Dönüştür"));
    leads(curveMake, {actOffset_});
    rows(curveMake, {tool("islem.tampon", tr("Tampon")), actHatch_});
    SARibbonPanel* curveRead = curve->addPanel(tr("Ölç"));
    leads(curveRead, {actMeasureArea_});
    rows(curveRead, {actEntityInfo_});
    closer(curve);

    // ---------------------------------------------------------------- `Blok`
    //
    // A PLACED BLOCK: its definition edited, its base moved, broken back into
    // its parts, and clipped (TODOS C-14) — every way to give the boundary, the
    // boundary drawn out, and the clip taken off.
    SARibbonCategory* block =
        context(RibbonContext::Block, tr("Blok Araçları"), tr("Blok"), t.accent);
    SARibbonPanel* blockEdit = block->addPanel(tr("Blok"));
    leads(blockEdit, {actBlockEdit_});
    ribbonLive_->editors[static_cast<std::size_t>(RibbonContext::Block)] = actBlockEdit_;
    rows(blockEdit, {actBlockBase_, actExplode_, actEntityInfo_});
    // EVERY WAY TO GIVE THE BOUNDARY IS ITS OWN BUTTON here (TODOS C-14), not a
    // family: this tab is about one thing, and a hand on it should not have to
    // remember which member the face last ran. The drawing tab's `Blok` panel
    // keeps them under one split button, where room is short.
    SARibbonPanel* clipping = block->addPanel(tr("Kırpma"));
    clipping->setObjectName(QStringLiteral("ribbonBlockClipPanel"));
    leads(clipping, {actBlockClip_});
    rows(clipping,
         {actBlockClipPolygon_, actBlockClipObject_, actBlockClipBoundary_, actBlockUnclip_});
    closer(block);

    // ------------------------------------------------------- `Blok: <ad>`
    //
    // UP FOR AS LONG AS A BLOCK IS OUT FOR EDITING, whatever is selected: the
    // save that puts the new picture into every reference, and the way back.
    // Not a selection's editor tab — the edit spans commands, and what belongs
    // to it is what the shell tracks (`MainWindow::blockEditLine`).
    blockEditTab_ = bar->addContextCategory(tr("Blok Düzenleme"), t.accent, kBlockEditContextId);
    SARibbonCategory* editing = blockEditTab_->addCategoryPage(tr("Blok"));
    editing->setObjectName(QStringLiteral("ribbonBlockEdit"));
    SARibbonPanel* finish = editing->addPanel(tr("Düzenlemeyi Bitir"));
    finish->addLargeAction(actBlockSave_);
    finish->addLargeAction(actBlockCancel_);
    SARibbonPanel* inside = editing->addPanel(tr("Bloğun İçinde"));
    inside->addLargeAction(actLine_);
    inside->addLargeAction(actCircle_);
    inside->addSmallAction(actMove_);
    inside->addSmallAction(actErase_);

    // ------------------------------------------------------------- `Seçim`
    //
    // UP WHILE A COMMAND ASKS FOR OBJECTS (`.claude/ui.md` R48a) — Netcad's
    // Seçim Süzgeci — and gone when they are handed over. EVERY BUTTON WRITES A
    // LINE: a SEÇ mode that needs no click runs as typed; one that does is
    // started in the command line and the canvas's clicks finish it
    // (`CommandLine::beginCompose`). The modes are SEÇ's own table
    // (`command::select_modes`), so the tab lists nothing SEÇ does not know.
    promptSelectTab_ = bar->addContextCategory(tr("Seçim"), t.accent, kPromptSelectContextId);
    SARibbonCategory* picking = promptSelectTab_->addCategoryPage(tr("Seçim"));
    picking->setObjectName(QStringLiteral("ribbonPromptSelect"));
    SARibbonPanel* gestures = picking->addPanel(tr("Seçim Kipi"));
    SARibbonPanel* sets     = picking->addPanel(tr("Küme"));
    // A PICTURE PER MODE, by its word; a mode added to SEÇ later still gets its
    // button, under the plain selection mark.
    const auto glyphOf = [](const QString& word) {
        static const QHash<QString, Glyph> known{
            {QStringLiteral("PENCERE"), Glyph::SelectWindow},
            {QStringLiteral("KESEN"), Glyph::SelectCrossing},
            {QStringLiteral("ÇOKGEN"), Glyph::SelectPolygon},
            {QStringLiteral("ÇOKGENKESEN"), Glyph::SelectPolygonCross},
            {QStringLiteral("ÇİT"), Glyph::SelectFence},
            {QStringLiteral("DAİRE"), Glyph::SelectCircle},
            {QStringLiteral("DIŞINDA"), Glyph::SelectOutside},
            {QStringLiteral("İÇEREN"), Glyph::SelectContaining},
            {QStringLiteral("GEÇEN"), Glyph::SelectThrough},
            {QStringLiteral("TÜMÜ"), Glyph::SelectEverything},
            {QStringLiteral("ÖNCEKİ"), Glyph::History},
            {QStringLiteral("SON"), Glyph::SelectNewest},
            {QStringLiteral("TEMİZLE"), Glyph::Close},
        };
        return known.value(word, Glyph::Select);
    };
    for (const command::SelectModeInfo& mode : command::select_modes()) {
        const QString word =
            QString::fromUtf8(mode.word.data(), static_cast<qsizetype>(mode.word.size()));
        auto* a = new QAction(
            QString::fromUtf8(mode.label.data(), static_cast<qsizetype>(mode.label.size())), this);
        a->setObjectName(QStringLiteral("promptSelect.%1").arg(word));
        a->setData(static_cast<int>(glyphOf(word)));
        a->setToolTip(tr("SEÇ %1%2 — %3")
                          .arg(word, mode.adds ? QStringLiteral(" islem=EKLE") : QString(),
                               QString::fromUtf8(mode.summary.data(),
                                                 static_cast<qsizetype>(mode.summary.size()))));
        connect(a, &QAction::triggered, this, [this, mode] { startSelectMode(mode); });
        (mode.points != 0 ? gestures : sets)->addSmallAction(a);
    }
    auto* invert = new QAction(tr("Tersine Çevir"), this);
    invert->setObjectName(QStringLiteral("promptSelect.TERSİNE"));
    invert->setData(static_cast<int>(Glyph::Invert));
    invert->setToolTip(tr("SEÇ TÜMÜ islem=TERSİNE — seçili olanlar çıkar, olmayanlar girer"));
    connect(invert, &QAction::triggered, this, [this] {
        QString line = QStringLiteral("SEÇ TÜMÜ islem=TERSİNE");
        if (const QString kind = selectKind_->currentData().toString(); !kind.isEmpty())
            line += QStringLiteral(" tur=") + kind;
        controller_->runLine(line, command::Origin::Gui);
    });
    sets->addSmallAction(invert);
    // THE KIND EVERY LINE OF THIS TAB IS NARROWED TO (`tur=`), from the kind
    // table itself, so a kind a plugin registers is offered the day it is.
    SARibbonPanel* kinds = picking->addPanel(tr("Süzgeç"));
    selectKind_          = new ComboBox(kinds);
    selectKind_->setObjectName(QStringLiteral("promptSelectKind"));
    selectKind_->setControlSize(ControlSize::Compact);
    selectKind_->setAccessibleName(tr("Seçilecek nesne türü"));
    selectKind_->addItem(tr("Her tür"), QString());
    for (const core::KindSpec& kind : core::builtin_kinds().all())
        if (kind.names[0] != nullptr) {
            const QString name = QString::fromUtf8(kind.names[0]);
            selectKind_->addItem(turkish_title(name), name);
        }
    kinds->addSmallWidget(captioned(kinds, tr("Tür"), selectKind_, 34));
    // AND THE WAY TO HAND THEM OVER, for a hand on the mouse: what Enter and the
    // right button do.
    SARibbonPanel* handOver = picking->addPanel(tr("Bitir"));
    auto* give              = new QAction(tr("Seçimi Ver"), this);
    give->setObjectName(QStringLiteral("promptSelect.VER"));
    give->setData(static_cast<int>(Glyph::Check));
    give->setToolTip(tr("Seçilenleri soran komuta verir — Enter ya da sağ tık"));
    connect(give, &QAction::triggered, this, [this] { (void)controller_->supplyPickedObjects(); });
    handOver->addLargeAction(give);

    // ------------------------------------------------------ `Nokta Girişi`
    //
    // UP WHILE A COMMAND ASKS FOR A POINT (R48a) — Netcad's
    // `Nokta Seçim Araçları` and its `Koordinat Hesap Makinası`. The snaps are the engine's
    // own bits (`core::snap_mode_label`), the palette is the grammar's own
    // table (`command::point_functions`): a button writes `dik(` into the
    // command line, the canvas's clicks write the points, the hand types the
    // figures, and Enter — or Gönder — answers with the line as typed.
    promptPointTab_ = bar->addContextCategory(tr("Nokta Girişi"), t.accent, kPromptPointContextId);
    SARibbonCategory* pointing = promptPointTab_->addCategoryPage(tr("Nokta Girişi"));
    pointing->setObjectName(QStringLiteral("ribbonPromptPoint"));

    SARibbonPanel* snaps = pointing->addPanel(tr("Yakalama"));
    promptSnaps_.clear();
    for (const std::uint32_t* bit = core::snap_mode_bits(); *bit != core::SnapNone; ++bit) {
        if ((core::SnapAllMask & *bit) == 0) continue;
        auto* a = new QAction(turkish_title(QString::fromUtf8(core::snap_mode_label(*bit))), this);
        a->setObjectName(
            QStringLiteral("promptPoint.snap.%1").arg(QString::fromUtf8(core::snap_mode_id(*bit))));
        a->setCheckable(true);
        a->setProperty(kSnapBitProperty, *bit);
        a->setToolTip(tr("MOD yakalama_modları — %1 yakalamayı açar ya da kapatır")
                          .arg(QString::fromUtf8(core::snap_mode_label(*bit))));
        const std::uint32_t mine = *bit;
        connect(a, &QAction::triggered, this, [this, mine](bool on) {
            const auto mask = static_cast<std::uint32_t>(
                controller_->bus().session_settings().get("core.yakalama.modlar").as_int());
            controller_->runLine(QStringLiteral("MOD ad=yakalama_modları deger=%1")
                                     .arg(on ? (mask | mine) : (mask & ~mine)),
                                 command::Origin::Gui);
        });
        promptSnaps_ << a;
        snaps->addSmallAction(a);
    }

    SARibbonPanel* palette = pointing->addPanel(tr("Hesap"));
    // A PICTURE PER FUNCTION, by its name — the construction it does, drawn; a
    // function added to the grammar later still gets its button, under ƒ.
    const auto fnGlyph = [](const QString& name) {
        static const QHash<QString, Glyph> known{
            {QStringLiteral("son"), Glyph::FnLast},
            {QStringLiteral("n"), Glyph::FnNumbered},
            {QStringLiteral("orta"), Glyph::FnMid},
            {QStringLiteral("ile"), Glyph::FnRelative},
            {QStringLiteral("dik"), Glyph::PerpOffset},
            {QStringLiteral("semt"), Glyph::Survey},
            {QStringLiteral("kes"), Glyph::PointIntersect},
            {QStringLiteral("ara"), Glyph::PointAlong},
            {QStringLiteral("uzanti"), Glyph::FnBeyond},
            {QStringLiteral("xy"), Glyph::FnXY},
            {QStringLiteral("boyunca"), Glyph::FnAlong},
        };
        return known.value(name, Glyph::Function);
    };
    for (const command::PointFunctionInfo& fn : command::point_functions()) {
        const QString name =
            QString::fromUtf8(fn.name.data(), static_cast<qsizetype>(fn.name.size()));
        auto* a = new QAction(
            QString::fromUtf8(fn.label.data(), static_cast<qsizetype>(fn.label.size())), this);
        a->setObjectName(QStringLiteral("promptPoint.fn.%1").arg(name));
        a->setData(static_cast<int>(fnGlyph(name)));
        a->setToolTip(tr("%1 — noktaları tuvalde tıklayın, sayıları yazın, Enter")
                          .arg(QString::fromStdString(fn.syntax)));
        const bool whole = !fn.takes_arguments;
        connect(a, &QAction::triggered, this, [this, name, whole] {
            // A FUNCTION OF NOTHING is the answer as it stands: `son()`.
            if (whole) {
                controller_->runLine(name + QStringLiteral("()"), command::Origin::Gui);
                return;
            }
            commandLine_->beginCompose(name + QLatin1Char('('), 0);
        });
        palette->addSmallAction(a);
    }

    // THE LAYER OF WHAT IS CLICKED, for what is being drawn (plan open question
    // 19): Netcad's drawing onto the layer of the object pointed at, as a
    // `katman=` the line carries — the active layer stays where it is.
    SARibbonPanel* layerPanel = pointing->addPanel(tr("Katman"));
    promptLayerPick_          = new QAction(tr("Katmanı nesneden al"), this);
    promptLayerPick_->setObjectName(QStringLiteral("promptPoint.KATMAN"));
    promptLayerPick_->setData(static_cast<int>(Glyph::LayerFromObject));
    promptLayerPick_->setToolTip(
        tr("Bir nesneye tıklayın: bu komutun çizdikleri onun katmanına gider, etkin katman "
           "değişmez (katman=…)"));
    connect(promptLayerPick_, &QAction::triggered, this, &MainWindow::pickLayerFromObject);
    layerPanel->addLargeAction(promptLayerPick_);

    // THE PROMPT'S OWN OPTIONS, as buttons (TODOS U-01): taking the newest point back and the
    // words a prompt takes beside a point (`command::Prompt::words`: Kapat). They are the
    // keyboard's answers — ⌫ and `K` — with a hand to press them; both reach `Session::retract`
    // and `Session::choose`, so the mouse has no road the line does not (CLAUDE.md 1.2, 5.15).
    //
    // A FIXED POOL, NOT A LIST WE KEEP: the words come from the running command, so the panel
    // holds a few buttons that are relabelled for each prompt and hidden when it has fewer
    // (`refreshPointTab`). Which words exist is the command's to say (5.10).
    SARibbonPanel* options = pointing->addPanel(tr("Seçenekler"));
    promptRetract_         = new QAction(tr("Geri Al"), this);
    promptRetract_->setObjectName(QStringLiteral("promptPoint.GERI"));
    promptRetract_->setData(static_cast<int>(Glyph::Undo));
    promptRetract_->setToolTip(tr("Son noktayı geri alır, çizimin geri kalanı durur — ⌫, G"));
    connect(promptRetract_, &QAction::triggered, this,
            [this] { (void)controller_->retractPoint(); });
    options->addLargeAction(promptRetract_);
    promptWords_.clear();
    for (int i = 0; i < 3; ++i) {
        auto* word = new QAction(tr("Seçenek"), this);
        word->setObjectName(QStringLiteral("promptPoint.word.%1").arg(i));
        word->setData(static_cast<int>(Glyph::Check));
        word->setToolTip(tr("Soran komutun seçeneği; komut satırına sözcüğü yazmakla aynı"));
        connect(word, &QAction::triggered, this, [this, word] {
            (void)controller_->chooseWord(word->property("piricad.word").toString());
        });
        promptWords_ << word;
        options->addLargeAction(word);
    }

    SARibbonPanel* send = pointing->addPanel(tr("Satır"));
    auto* go            = new QAction(tr("Gönder"), this);
    go->setObjectName(QStringLiteral("promptPoint.GONDER"));
    go->setData(static_cast<int>(Glyph::Check));
    go->setToolTip(tr("Kurulan satırı soran komuta verir — Enter; açık parantezler kapanır"));
    connect(go, &QAction::triggered, this, [this] { commandLine_->submitLine(); });
    send->addLargeAction(go);
    auto* drop = new QAction(tr("Vazgeç"), this);
    drop->setObjectName(QStringLiteral("promptPoint.VAZGEC"));
    drop->setData(static_cast<int>(Glyph::Close));
    drop->setToolTip(tr("Kurulan satırı siler, soru sürer — Esc"));
    connect(drop, &QAction::triggered, this, [this] {
        commandLine_->clear();
        commandLine_->endCompose();
    });
    send->addSmallAction(drop);
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
    // A TOOL THAT NEEDS A FIGURE FIRST — TAMPON's distance, ALANDÜZENLE's
    // target area: a number with no default — opens where the figure is typed
    // and says so with `…`. EVERY OTHER TOOL RUNS WHEN PRESSED, on the
    // selection or on the objects it then asks for, with its defaults; its
    // settings are one ↘ away on the panel. It used to open the panel always,
    // which from the chair was a button that did nothing on the drawing — the
    // user's "çalışmayan araçlar".
    const bool needs_figure = std::ranges::any_of(spec.params, [](const processing::ToolParam& p) {
        return p.kind == command::ParamKind::Number && p.fallback.empty();
    });
    // THE TOOL'S OWN TITLE, unless the ribbon row is too short for it: a menu
    // row may say "Kenar uzunluklarını yaz", a ribbon row says "Uzunluk Yaz".
    QString label = word.isEmpty() ? turkish_title(QString::fromStdString(spec.title)) : word;
    if (needs_figure && !label.endsWith(QStringLiteral("…"))) label += QStringLiteral("…");
    action->setText(label);
    // THE BUTTON SAYS IT TOO: Qt drops a trailing ellipsis from the words a
    // tool button shows (`QAction::iconText`) unless they are set outright, and
    // then the ribbon could not tell a tool that opens its form from one that
    // runs.
    if (needs_figure) action->setIconText(label);
    action->setToolTip(QString::fromStdString(spec.summary));
    action->setStatusTip(QString::fromStdString(spec.summary));
    const QString line = QString::fromStdString(spec.names.front());
    action->setProperty(kToolCommand, line);
    if (needs_figure)
        connect(action, &QAction::triggered, this, [this, id] { showToolsPanel(id); });
    else
        connect(action, &QAction::triggered, this,
                [this, line] { controller_->runLine(line, command::Origin::Gui); });
    return action;
}

void MainWindow::showToolsPanel(const QString& id)
{
    propertyDock_->show();
    propertyDock_->raise();
    propertyHeader_->setCurrent(2);
    propertyStack_->setCurrentIndex(2);
    if (!id.isEmpty() && toolsPanel_ != nullptr && toolsPanel_->selectTool(id)) {
        // A TOOL OPENED FOR ITS FIGURE (`Tampon…`, `Alanı Düzenle…`) is waiting
        // for that figure, so the keyboard goes where it is typed — after the
        // button's own click has let go of the focus it took.
        QTimer::singleShot(0, this, [this] {
            if (toolsPanel_ != nullptr) (void)toolsPanel_->focusFirstField();
        });
    }
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
    refreshPenBox();
    refreshColourBoxes();
    refreshRibbonDefaults();
    refreshContextTabs();
    refreshToolAvailability();
    refreshPointTab();
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

void MainWindow::refreshPenBox()
{
    ComboBox* box = ribbonLive_->pen;
    if (box == nullptr) return;
    const auto catalog = controller_->bus().feature_classes();
    const QSignalBlocker quiet(box);

    // FILLED ONLY WHEN THE PACKAGE CHANGED: this runs after every command, and the list is the
    // package's, not the drawing's.
    if (catalog.get() != ribbonLive_->penPackage) {
        ribbonLive_->penPackage = catalog.get();
        box->clear();
        box->addItem(catalog != nullptr ? tr("Kalem seçin…") : tr("Kalem kataloğu yok"), QString());
        if (catalog != nullptr)
            for (const command::FeatureClass& c : catalog->classes) {
                box->addItem(
                    tr("%1 — %2").arg(QString::fromStdString(c.name),
                                      QString::fromUtf8(command::class_geometry_noun(c.geometry))),
                    QString::fromStdString(c.id));
                box->setItemData(box->count() - 1,
                                 QString::fromStdString(c.summary) + QStringLiteral("\n") +
                                     tr("Katman: %1").arg(QString::fromStdString(c.layer)),
                                 Qt::ToolTipRole);
            }
        else
            box->setToolTip(QString::fromStdString(controller_->bus().feature_classes_error()));
    }
    box->setEnabled(catalog != nullptr);

    // THE CLASS THE ACTIVE LAYER FOLLOWS, or none.
    int shown = 0;
    if (catalog != nullptr) {
        const core::Layer* active =
            controller_->document().layer(controller_->bus().active_layer());
        if (active != nullptr)
            if (const command::FeatureClass* c = catalog->of_reference(active->feature_class);
                c != nullptr)
                shown = std::max(0, box->findData(QString::fromStdString(c->id)));
    }
    box->setCurrentIndex(shown);
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
        const std::optional<RibbonContext> which = ribbon_context_of(doc, e);
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
        // hatch picked to change its pattern, a parcel picked to cut it, a line
        // picked to round its corner — the way AutoCAD's Hatch and Text Editor
        // tabs do. Every such tab carries the move, copy and erase a selection
        // is also made for (`objectVerbs`), so coming forward takes nothing
        // away from the hand; and it goes back when the selection goes.
        if (want && count[i] == total) raise = i;
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

// =============================================================================
// The tools the selection holds nothing for
// =============================================================================

namespace {

/// Why a greyed tool is greyed, as the tip under it says it.
constexpr const char* kUnavailable = "piricad.unavailable";

} // namespace

void MainWindow::gatherTargetTools()
{
    // EVERY ACTION THAT STARTS SUCH A COMMAND, wherever it is shown — a ribbon
    // button, a family's arrow, an editor tab, the Araçlar tree's buttons — so
    // one declaration greys all of them at once (`CommandSpec::targets`).
    targetTools_.clear();
    for (QAction* action : findChildren<QAction*>()) {
        const QString word =
            action->property(kToolCommand).toString().section(QLatin1Char(' '), 0, 0);
        if (word.isEmpty() || action->property(kIgnoresSelectionProperty).toBool()) continue;
        const command::CommandSpec* spec = controller_->registry().resolve(word.toStdString());
        if (spec == nullptr) continue;
        // THE METHOD THE BUTTON SENDS narrows what it takes (`verb_targets`):
        // "Böl — eşit parçaya" cuts a line, never an area.
        command::Args given;
        const QString line = action->property(kToolCommand).toString();
        for (const QString& token : line.split(QLatin1Char(' '), Qt::SkipEmptyParts).mid(1))
            if (const qsizetype eq = token.indexOf(QLatin1Char('=')); eq > 0)
                given.set(token.left(eq).toStdString(),
                          command::Value::text(token.mid(eq + 1).toStdString()));
        TargetTool one{.action = action, .targets = command::targets_of(*spec, given)};
        if (const processing::ProcessingTool* tool = processing::find_tool(spec->id);
            tool != nullptr)
            one.applies = static_cast<std::uint8_t>(tool->spec().applies);
        if (one.applies == 0 && spec->targets == command::Targets::Any) continue;
        targetTools_.push_back(one);
    }
}

void MainWindow::refreshToolAvailability()
{
    if (controller_->awaitingInput()) return;
    const core::Document& doc                = controller_->document();
    const std::vector<core::EntityKey>& keys = controller_->bus().selection().keys();
    const command::Held held                 = command::held_by(doc, keys);
    // The classes the Araçlar runner sorts the same objects into, which is
    // what it skips by (`processing::classify`).
    std::uint8_t sorted = 0;
    for (const core::EntityKey k : keys)
        if (const core::EntityId e = doc.slot_of(k); e != core::kNoEntity && doc.alive(e))
            sorted |= static_cast<std::uint8_t>(processing::classify(doc, e));

    for (TargetTool& one : targetTools_) {
        QAction* action = one.action.data();
        if (action == nullptr) continue;
        const bool offered = one.applies != 0 ? held.count == 0 || (sorted & one.applies) != 0
                                              : command::acts_on_all(one.targets, held);
        if (!offered) {
            if (!action->isEnabled() && !one.greyed) continue; // another rule's, left alone
            const QString word =
                action->property(kToolCommand).toString().section(QLatin1Char(' '), 0, 0);
            action->setProperty(
                kUnavailable,
                one.applies != 0
                    ? tr("Seçimde bu aracın işlediği nesne yok: %1 yalnız %2 üzerinde çalışır.")
                          .arg(word, QString::fromStdString(command::target_names(one.targets)))
                    : tr("Seçimde bu komutun işlemediği nesne var: %1 yalnız %2 üzerinde "
                         "çalışır.")
                          .arg(word, QString::fromStdString(command::target_names(one.targets))));
            action->setEnabled(false);
            one.greyed = true;
        } else if (one.greyed) {
            action->setProperty(kUnavailable, QVariant());
            action->setEnabled(true);
            one.greyed = false;
        }
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
            a->setIcon(anchor_icon(a->property("piricad.anchor.col").toInt(),
                                   a->property("piricad.anchor.row").toInt(), t.iconInk,
                                   t.iconNote));
    // THE SNAP SWITCHES WEAR THE CANVAS'S OWN MARKERS, in the note ink.
    for (QAction* a : std::as_const(promptSnaps_))
        a->setIcon(snap_icon(a->property(kSnapBitProperty).toUInt(), t.iconNote));
    // And the prompt tabs wear the accent cap, as the editor tabs do.
    for (SARibbonContextCategory* tab : {promptSelectTab_, promptPointTab_})
        if (tab != nullptr) tab->setContextColor(t.accent);
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
    // WHY IT IS GREY, first, where the eye lands: a tool the selection holds
    // nothing for says what it does take (`refreshToolAvailability`).
    const QString unavailable = face->property(kUnavailable).toString();
    if (!keys.isEmpty())
        html += QStringLiteral("&nbsp;&nbsp;<span style='color:%1'>%2</span>")
                    .arg(faint, keys.toHtmlEscaped());
    html += QStringLiteral("</p>");
    if (!unavailable.isEmpty() && !face->isEnabled())
        html += QStringLiteral("<p style='margin:4px 0 0 0; color:%1'>%2</p>")
                    .arg(t.warn.name(), unavailable.toHtmlEscaped());
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

QToolButton* MainWindow::ribbonButton(const QAction* action, bool raise, bool own)
{
    SARibbonBar* bar = ribbonBar();
    if (bar == nullptr || action == nullptr) return nullptr;
    // A member is pressed through its family's face, unless its own button
    // elsewhere is asked for.
    const QAction* carried = action;
    if (!own)
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
// The ribbon, photographed
// =============================================================================

int MainWindow::probeRibbonSheet()
{
    const QString into = QString::fromLocal8Bit(qgetenv("PIRICAD_RIBBON_SHEET"));
    QDir().mkpath(into);
    SARibbonBar* bar = ribbonBar();
    if (bar == nullptr) return 1;
    const auto settle = [] {
        for (int i = 0; i < 4; ++i)
            QCoreApplication::processEvents();
    };
    const auto save = [&into](const QImage& image, const QString& name) {
        const QString path = into + QLatin1Char('/') + name + QStringLiteral(".png");
        (void)std::fprintf(image.save(path) ? stdout : stderr, "[şerit] %s\n", qPrintable(path));
    };
    const auto file_word = [](QString word) {
        word = word.toLower();
        for (QChar& c : word)
            if (!c.isLetterOrNumber()) c = QLatin1Char('-');
        return word;
    };

    // Photograph the laptop width the layout is measured against, so a row
    // outside the viewport is visible as a defect in the captured sheet.
    resize(1440, 860);
    settle();

    // ONE OF EACH KIND, so every editor tab can be brought up.
    runScriptLine(QStringLiteral("YENİ"));
    endCommand();
    for (const char* line : {
             "KATMAN ad=PARSEL",
             "ALAN 0,0 30,0 30,20 0,20",
             "ÇOKLUÇİZGİ 40,0 55,0 55,15",
             "DAİRE 70,10 76,10",
             "METİN 5,10 \"1234/7\" 2000",
             "TARAMA nesneler=1",
             "ÖLÇÜ 0,-5 30,-5 15,-9",
             // A BLOCK, defined from a circle and placed once, for the Blok tab.
             "DAİRE 90,10 92,10",
             "BLOK ad=KAPAK taban=90,10 nesneler=7",
             "BLOKEKLE ad=KAPAK nokta=100,10",
         }) {
        runScriptLine(QString::fromUtf8(line));
        endCommand();
    }
    settle();
    // The placed block's key, as the drawing gave it.
    QString blockKey;
    {
        const core::Document& doc = controller_->document();
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kBlockReferenceKind) {
                blockKey = keyText(doc.key_of(e));
                break;
            }
    }

    // THE WIDTH A TAB ASKS FOR, panel by panel: what decides whether it fits a
    // 1440 px window without scrolling — the laptop the ribbon is laid out for
    // (docs/baslangic/arayuz.md). A wider one is a finding, editor tabs too.
    constexpr int kFits = 1440;
    QStringList too_wide;
    // AND THE GRAMMAR (`place_item`): a button is large, picture over word, or a
    // labelled row of one row's height. A bare picture, or a row two rows tall,
    // is the third size the user read as "some big, some small, all jumbled".
    QStringList off_grammar;
    // AND THE QUICK ACCESS PRINTER'S ARROW (`theme.cpp`, the ::menu-button rule):
    // Fusion paints the arrow half of a split button in a button group from a
    // transparent `Button` role and, on Linux, drew a slab of black beside the
    // printer that the Mac never showed.
    QStringList black_arrows;
    // AND THE DRAWING FACE (`.claude/ui.md` R55): the desktop's platform theme
    // names its own font for some widget classes — Plasma gives QToolButton, QMenu
    // and QLabel Noto Sans 10 pt — and a ribbon that came out in it was 5% wider
    // than the design's IBM Plex. A widget of each of those classes, polished the
    // way a real one is, must come out in the program's face on this platform.
    QStringList wrong_faces;
    {
        const auto face = [&wrong_faces](const char* who, QWidget* w) {
            w->ensurePolished();
            if (!w->font().family().startsWith(QStringLiteral("IBM Plex Sans")))
                wrong_faces << QStringLiteral("%1: %2").arg(QString::fromLatin1(who),
                                                            w->font().family());
        };
        QToolButton button(this);
        QLabel label(this);
        QMenu menu(this);
        face("QToolButton", &button);
        face("QLabel", &label);
        face("QMenu", &menu);
        for (const SARibbonToolButton* b : bar->findChildren<SARibbonToolButton*>())
            if (b->defaultAction() != nullptr) {
                face("şerit düğmesi", const_cast<SARibbonToolButton*>(b));
                break;
            }
    }
    const auto measure = [&too_wide, &off_grammar](const SARibbonCategory* tab) {
        int wanted = 0;
        QStringList widths;
        for (const SARibbonPanel* panel : tab->panelList()) {
            const int w = panel->sizeHint().width();
            wanted += w;
            widths << QStringLiteral("%1 %2").arg(panel->panelName()).arg(w);
            // THE MAP, as a reader of the manual meets it: `Tab ▸ Panel`, then
            // each button with ▣ for a lead and · for a row. What the pages
            // under /docs cite is checked against these lines.
            QStringList items;
            int rowHeight = 0;
            for (const SARibbonToolButton* b : panel->ribbonToolButtons())
                if (b->buttonType() != SARibbonToolButton::LargeButton && b->isVisible())
                    rowHeight = rowHeight == 0 ? b->height() : std::min(rowHeight, b->height());
            for (const SARibbonToolButton* b : panel->ribbonToolButtons()) {
                const QAction* a = b->defaultAction();
                if (a == nullptr || !b->isVisible()) continue;
                const bool lead    = b->buttonType() == SARibbonToolButton::LargeButton;
                const QString word = QString(a->iconText()).remove(QLatin1Char('&'));
                items << (lead ? QStringLiteral("▣ ") : QStringLiteral("· ")) + word;
                const bool bare = b->toolButtonStyle() == Qt::ToolButtonIconOnly;
                const bool tall = !lead && rowHeight > 0 && b->height() > (rowHeight * 3) / 2;
                if (bare || tall)
                    off_grammar << QStringLiteral("%1 ▸ %2 ▸ %3 (%4)")
                                       .arg(tab->categoryName(), panel->panelName(), word,
                                            bare ? QStringLiteral("yalnız resim")
                                                 : QStringLiteral("iki satır boyu"));
            }
            (void)std::fprintf(stdout, "[şerit] yol: %s ▸ %s: %s\n",
                               qPrintable(tab->categoryName()), qPrintable(panel->panelName()),
                               qPrintable(items.join(QStringLiteral(", "))));
        }
        // WHAT THE TAB NEEDS WITH THE GAPS BETWEEN ITS PANELS, against the room
        // the window gives it: the panels' own widths summed passed at 1417 px
        // while the last panel was already cut off at the window's edge.
        // AGAINST THE DESIGN'S WIDTH, not against whatever the window manager
        // granted: a 1366 px laptop clamps the 1440 px window, and every tab would
        // then "not fit" for a reason that is the screen's and not the ribbon's.
        constexpr int kRoom = kFits - 4; // the bar's width inside a 1440 px window
        const int need      = std::max(wanted, tab->sizeHint().width());
        const int room      = kRoom;
        (void)std::fprintf(stdout, "[şerit] genişlik %s: %d, gereken %d, yer %d (%s)\n",
                           qPrintable(tab->categoryName()), wanted, need, room,
                           qPrintable(widths.join(QStringLiteral(", "))));
        if (need > room)
            too_wide << QStringLiteral("%1 (%2 > %3)").arg(tab->categoryName()).arg(need).arg(room);
    };

    // ---- every tab, in the order the bar has them ----
    int n = 0;
    for (SARibbonCategory* tab : bar->categoryPages(false)) {
        if (tab == nullptr || tab->isContextCategory()) continue;
        bar->raiseCategory(tab);
        settle();
        measure(tab);
        save(bar->grab().toImage(), QStringLiteral("%1-%2")
                                        .arg(++n, 2, 10, QLatin1Char('0'))
                                        .arg(file_word(tab->categoryName())));
    }

    // ---- every editor tab, raised by an object of its kind ----
    const std::array<std::pair<QString, const char*>, 7> picks{{
        {QStringLiteral("1"), "alan"},
        {QStringLiteral("2"), "cizgi"},
        {QStringLiteral("3"), "egri"},
        {QStringLiteral("4"), "yazi"},
        {QStringLiteral("5"), "tarama"},
        {QStringLiteral("6"), "olcu"},
        {blockKey, "blok"},
    }};
    for (const auto& [key, word] : picks) {
        if (key.isEmpty()) {
            (void)std::fprintf(stderr, "[şerit] %s sekmesi için nesne yok\n", word);
            continue;
        }
        runScriptLine(QStringLiteral("SEÇ mod=NESNE nesneler=") + key);
        endCommand();
        settle();
        if (const SARibbonCategory* raised = bar->categoryByIndex(bar->currentIndex());
            raised != nullptr && raised->isContextCategory())
            measure(raised);
        save(bar->grab().toImage(), QStringLiteral("baglam-") + QString::fromLatin1(word));
    }
    // Selected combo rows must remain readable under the ribbon's stylesheet,
    // in either theme. Photograph the actual popup, including editable lists.
    const ThemeMode originalTheme = theme_;
    const auto popupShot          = [&settle, &save](ComboBox* box, const char* name,
                                            const QString& themeName) {
        box->showPopup();
        settle();
        save(box->view()->window()->grab().toImage(),
                      QStringLiteral("combo-%1-%2").arg(themeName, QString::fromLatin1(name)));
        box->hidePopup();
    };
    for (const ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
        runScriptLine(QStringLiteral("SEÇ mod=NESNE nesneler=6"));
        endCommand();
        theme_ = mode;
        applyTheme();
        settle();
        const QString themeName =
            mode == ThemeMode::Light ? QStringLiteral("acik") : QStringLiteral("koyu");
        // THE ARROW HALF OF THE PRINTER BUTTON, on the light ground, where black
        // cannot hide: its right 14 logical pixels may hold the small grey
        // triangle and nothing darker over more than a fifth of them.
        if (mode == ThemeMode::Light)
            if (const auto* quick = bar->quickAccessBar(); quick != nullptr)
                if (auto* printer = findChild<QAction*>(QStringLiteral("quickPrint"));
                    printer != nullptr)
                    if (QWidget* button = quick->widgetForAction(printer); button != nullptr) {
                        // CROPPED OUT OF THE WHOLE BAR'S PICTURE: the button alone
                        // grabs on a transparent ground, and transparent reads as
                        // black.
                        const QImage whole = bar->grab().toImage();
                        const qreal dpr    = whole.devicePixelRatio();
                        const QRect box = QRect(button->mapTo(bar, QPoint(0, 0)), button->size());
                        const int arrow = static_cast<int>(14.0 * dpr);
                        qint64 dark     = 0;
                        qint64 all      = 0;
                        const int right = static_cast<int>(box.right() * dpr);
                        for (int y = static_cast<int>(box.top() * dpr);
                             y <= static_cast<int>(box.bottom() * dpr) && y < whole.height(); ++y)
                            for (int x = std::max(0, right - arrow);
                                 x <= right && x < whole.width(); ++x) {
                                ++all;
                                if (qGray(whole.pixel(x, y)) < 70) ++dark;
                            }
                        const double share =
                            all > 0 ? static_cast<double>(dark) / static_cast<double>(all) : 0.0;
                        (void)std::fprintf(stdout, "[şerit] yazdır oku: koyu piksel payı %.0f%%\n",
                                           share * 100.0);
                        if (share > 0.2)
                            black_arrows << QStringLiteral("%1%").arg(share * 100.0, 0, 'f', 0);
                    }
        for (const auto& [box, name] : {std::pair{ribbonLive_->dimStyle, "stil"},
                                        std::pair{ribbonLive_->dimPrecision, "hassasiyet"},
                                        std::pair{ribbonLive_->dimUnit, "birim"}}) {
            popupShot(box, name, themeName);
        }
        runScriptLine(QStringLiteral("SEÇ mod=NESNE nesneler=5"));
        endCommand();
        settle();
        save(bar->grab().toImage(), QStringLiteral("baglam-tarama-%1").arg(themeName));
        popupShot(ribbonLive_->hatchAngle, "aci", themeName);
        popupShot(ribbonLive_->hatchScale, "olcek", themeName);
    }
    theme_ = originalTheme;
    applyTheme();
    controller_->clearSelection();
    settle();

    // ---- every picture, with its name and its command ----
    struct Entry
    {
        const QAction* action{nullptr};
        QString label;
        QString command;
        int glyph{-1};
    };

    std::vector<Entry> entries;
    QSet<const QAction*> seen;
    const auto take = [&](const QAction* a) {
        if (a == nullptr || seen.contains(a) || a->text().isEmpty()) return;
        seen.insert(a);
        entries.push_back(Entry{.action  = a,
                                .label   = QString(a->text()).remove(QLatin1Char('&')),
                                .command = a->property(kToolCommand).toString(),
                                .glyph   = a->data().isValid() ? a->data().toInt() : -1});
    };
    for (const QToolButton* button : ribbonButtons()) {
        const QAction* shown = button->defaultAction();
        bool member          = false;
        for (const RibbonFamily* f : std::as_const(families_))
            if (f->head() == shown) {
                for (const QAction* m : f->members())
                    take(m);
                member = true;
            }
        if (!member) take(shown);
    }

    const Tokens& t      = tokensFor(theme_);
    const GlyphInks k    = actionInks();
    constexpr int kCols  = 10;
    constexpr int kCellW = 168;
    constexpr int kCellH = 104;
    const int rows       = static_cast<int>((entries.size() + kCols - 1) / kCols);
    QImage sheet(kCols * kCellW, (rows * kCellH) + 8, QImage::Format_ARGB32);
    sheet.fill(t.bgPanel);
    {
        QPainter p(&sheet);
        p.setRenderHint(QPainter::Antialiasing);
        QFont name = font();
        name.setPixelSize(12);
        QFont small = font();
        small.setPixelSize(10);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const int col = static_cast<int>(i % kCols);
            const int row = static_cast<int>(i / kCols);
            const QRect cell(col * kCellW, row * kCellH, kCellW, kCellH);
            p.setPen(t.lineSoft);
            p.drawRect(cell.adjusted(0, 0, -1, -1));
            const Entry& e = entries[i];
            const QIcon picture =
                e.glyph >= 0 ? colour_icon(static_cast<Glyph>(e.glyph), k, 32) : e.action->icon();
            picture.paint(&p, QRect(cell.center().x() - 16, cell.top() + 8, 32, 32));
            p.setFont(name);
            p.setPen(t.text);
            p.drawText(QRect(cell.left() + 4, cell.top() + 44, kCellW - 8, 34),
                       Qt::AlignHCenter | Qt::TextWordWrap, e.label);
            p.setFont(small);
            p.setPen(t.textFaint);
            p.drawText(QRect(cell.left() + 4, cell.top() + 80, kCellW - 8, 20), Qt::AlignHCenter,
                       e.command.left(28));
        }
    }
    save(sheet, QStringLiteral("ikonlar"));

    // ---- what the reviewer should look at ----
    int findings = 0;
    QHash<int, QStringList> by_glyph;
    for (const Entry& e : entries) {
        if (e.glyph < 0 && e.action->icon().isNull()) {
            ++findings;
            (void)std::fprintf(stdout, "[şerit] resimsiz: %s (%s)\n", qPrintable(e.label),
                               qPrintable(e.command));
        }
        if (e.glyph == static_cast<int>(Glyph::Function)) {
            ++findings;
            (void)std::fprintf(stdout, "[şerit] genel işaret: %s (%s)\n", qPrintable(e.label),
                               qPrintable(e.command));
        }
        const QString word = e.command.section(QLatin1Char(' '), 0, 0);
        if (e.glyph >= 0 && !word.isEmpty() && !by_glyph[e.glyph].contains(word))
            by_glyph[e.glyph] << word;
    }
    for (auto it = by_glyph.cbegin(); it != by_glyph.cend(); ++it)
        if (it.value().size() > 1)
            (void)std::fprintf(stdout, "[şerit] bir resim birçok komut: %s\n",
                               qPrintable(it.value().join(QStringLiteral(", "))));
    for (const QString& tab : std::as_const(too_wide)) {
        ++findings;
        (void)std::fprintf(stdout, "[şerit] pencereye sığmıyor (%d piksel): %s\n", kFits,
                           qPrintable(tab));
    }
    for (const QString& button : std::as_const(off_grammar)) {
        ++findings;
        (void)std::fprintf(stdout, "[şerit] boy kuralı dışında: %s\n", qPrintable(button));
    }
    for (const QString& face : std::as_const(wrong_faces)) {
        ++findings;
        (void)std::fprintf(stdout, "[şerit] yazı tipi IBM Plex Sans değil — %s\n",
                           qPrintable(face));
    }
    for (const QString& share : std::as_const(black_arrows)) {
        ++findings;
        (void)std::fprintf(stdout, "[şerit] yazdır okunun yanı siyah (açık temada %s koyu)\n",
                           qPrintable(share));
    }
    (void)std::fprintf(stdout, "[şerit] %zu düğme, %d bulgu\n", entries.size(), findings);
    return 0;
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

} // namespace piricad::app
