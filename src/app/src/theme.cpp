// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/theme.hpp"

#include "piricad/app/data_root.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QProxyStyle>
#include <QStyleFactory>

#include "piricad/app/tokens.hpp"

namespace piricad::app {
namespace {

/// A `Palette` built from a token set.
///
/// The palette exists because the canvas and the widgets want different names for
/// the same colours — `canvas`, `grid`, `snapMarker` mean something to a renderer
/// and nothing to a QSS rule. What it must NOT be is a second set of values, which
/// is what it was: two tables of hand-written hex, drifting apart from the design
/// and from each other. It is now a projection of `tokens.hpp` and holds no colour
/// of its own.
Palette palette_of(const Tokens& t)
{
    Palette p;
    p.window     = t.bgWindow;
    p.panel      = t.bgPanel;
    p.field      = t.bgInput;
    p.border     = t.border;
    p.text       = t.text;
    p.textMuted  = t.textDim;
    p.accent     = t.accent;
    p.accentSoft = t.accentWash;
    p.hover      = t.hoverIcon;
    p.alternate  = t.rowOdd;

    p.canvas     = t.bgCanvas;
    p.grid       = t.gridMinor;
    p.gridMajor  = t.gridMajor;
    p.crosshair  = t.crosshair;
    p.rubberBand = t.rubberBand;
    p.hud        = t.hud;

    p.selection    = t.accent;
    p.selectWindow = t.selectWindow;
    p.selectCross  = t.selectCross;
    p.snapMarker   = t.warn;
    return p;
}

/// `rgba(r,g,b,a)` as QSS wants it, with alpha as a fraction.
QString rgba(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

} // namespace

const Palette& themePalette(ThemeMode mode)
{
    static const Palette light = palette_of(lightTokens());
    static const Palette dark  = palette_of(darkTokens());
    return mode == ThemeMode::Light ? light : dark;
}

namespace {

/// `Fusion`, with the shortcut underline made conditional on Alt.
class ShellStyle : public QProxyStyle
{
public:
    ShellStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
        if (hint == SH_UnderlineShortcut)
            return QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier) ? 1 : 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    /// The dock metrics `design.md` §6 fixes, which a stylesheet cannot reach.
    ///
    /// A dock's title area is `titleWidget->sizeHint() + 2 * titleMargin + 2 *
    /// frameWidth`, and none of those three is a stylesheet property. Fusion's
    /// defaults add two pixels, so a 29 px `PanelHeader` occupies 31 and every
    /// panel below it starts two pixels low — which is exactly the drift the
    /// specification forbids. Zero here means the header is the header.
    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override
    {
        switch (metric) {
        case PM_DockWidgetTitleMargin:
        case PM_DockWidgetFrameWidth:
        case PM_DockWidgetTitleBarButtonMargin: return 0;
        case PM_DockWidgetSeparatorExtent: return 1;

        // The menu bar's own leading and inter-item spacing, likewise unreachable
        // from a stylesheet. Fusion adds six pixels before the first title and
        // six between each pair, which moved `Dosya` eight pixels right of where
        // the reference has it and stretched the whole row. The 2 px gap the
        // design asks for comes from `margin: 0 1px` on ::item instead.
        case PM_MenuBarHMargin:
        case PM_MenuBarVMargin:
        case PM_MenuBarPanelWidth:
        case PM_MenuBarItemSpacing: return 0;

        // The tool bar's own frame and item margin, for the same reason. Fusion
        // insets a widget added to a tool bar by eight pixels, so the ÖLÇEK /
        // KOORDİNAT SİSTEMİ reading started eight pixels down and ran eight
        // pixels past the bottom of the bar — the value ended one pixel above
        // the rule with no air under it at all. The bar's padding is the
        // stylesheet's business; this makes sure the style adds nothing to it.
        case PM_ToolBarFrameWidth:
        case PM_ToolBarItemMargin: return 0;
        default: return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }
};

} // namespace

void applyThemeToChildren(QWidget* root, ThemeMode mode)
{
    if (root == nullptr) return;

    // CHILDREN ONLY — never `root` itself, and the name is the contract.
    //
    // This walked `root` too for about ten minutes, and the result was a stack
    // overflow: `DialogFrame::applyTheme` calls this with `this`, so the walk
    // called `applyTheme` on the dialog, which called the walk, forever. Anything
    // that IS themed and calls this is calling it from inside its own
    // `applyTheme`; applying to itself again can only recurse.
    //
    // QObject, not QWidget: an item delegate paints rows from tokens and is not a
    // widget, so a widget-only walk would leave every layer row in one theme.
    for (QObject* child : root->findChildren<QObject*>())
        if (auto* themed = dynamic_cast<Themed*>(child)) themed->applyTheme(mode);
}

bool loadShellFonts(QString* whereLooked)
{
    const QString dir = QString::fromStdString(data_root()) + QStringLiteral("/fonts");
    if (whereLooked) *whereLooked = dir;

    // Every face the sheet and the painted chrome ask for. A missing one is a
    // silent substitution at draw time, so all five are required together.
    static const char* const kFaces[] = {
        "IBMPlexSans-Regular.ttf", "IBMPlexSans-Medium.ttf", "IBMPlexSans-SemiBold.ttf",
        "IBMPlexMono-Regular.ttf", "IBMPlexMono-Medium.ttf",
    };

    bool all = true;
    for (const char* face : kFaces)
        all &= QFontDatabase::addApplicationFont(dir + QLatin1Char('/') + QLatin1String(face)) >= 0;

    if (!all) return false;

    // Pixel sizes, never points: `design.md` §4 is written in pixels and a point
    // size would rescale with the screen's reported DPI, which is the one thing
    // that must not move between platforms.
    QFont body(QStringLiteral("IBM Plex Sans"));
    body.setPixelSize(12);
    QApplication::setFont(body);
    return true;
}

void installShellStyle()
{
    QApplication::setStyle(new ShellStyle);
}

QString themeStyleSheet(ThemeMode mode)
{
    const Tokens& t = mode == ThemeMode::Light ? lightTokens() : darkTokens();

    // ONE STYLESHEET FOR THE WHOLE APPLICATION, and every measure in it comes from
    // `design.md` §4. A widget this sheet does not name is a widget Fusion draws
    // its own way, which on three platforms is three ways — so the sheet covers
    // everything the program shows rather than the parts that looked wrong.
    //
    // A DIALOG MAY NOT CARRY ITS OWN. Both the settings window and the style
    // designer used to call `setStyleSheet` with their own greys, which is how
    // they ended up matching neither each other nor the shell. Those are gone and
    // this is the only sheet there is (`ci-gate-theme.sh` keeps it that way).
    return QStringLiteral(R"(
        /* ---- ground ------------------------------------------------------- */
        QWidget                          { background: %(window)s; color: %(text)s;
                                           font-size: 12px; }
        QMainWindow::separator           { background: %(lineHard)s; width: 1px; height: 1px; }
        QToolTip                         { background: %(raised)s; color: %(text)s;
                                           border: 1px solid %(border)s; padding: 4px 7px; }

        /* ---- title bar, §7 ------------------------------------------------- */
        /*
         * The bar paints its own gradient, so the menu bar inside it is
         * TRANSPARENT: a background here would draw a flat rectangle over the two
         * stops and the seam would show. The 12.5 px label, the 5x9 padding and
         * the 4 px radius are the mockup's own numbers, and `margin: 0 1px` is
         * how a stylesheet says the 2 px gap between titles.
         */
        QMenuBar#shellMenuBar            { background: transparent; color: %(menuText)s;
                                           border: none; font-size: 12.5px; padding: 0; }
        QMenuBar#shellMenuBar::item      { padding: 5px 9px; margin: 0px;
                                           background: transparent; border-radius: 4px; }
        QMenuBar#shellMenuBar::item:selected,
        QMenuBar#shellMenuBar::item:pressed { background: %(hoverChip)s; color: %(onHover)s; }
        QLabel#shellDocTitle             { background: transparent; color: %(titleText)s;
                                           font-size: 12px; font-weight: 500; }
        /*
         * TRANSPARENT, and this is not cosmetic. `QWidget { background: … }`
         * above sets WA_StyledBackground on every widget in the program, so a
         * child of the title bar paints a flat `bgWindow` rectangle over the
         * gradient before its own paintEvent runs — a pale slab across the two
         * stops, sitting on top of the first menu title.
         */
        #titleDots, #titleSearch, #titleUser { background: transparent; }

        /*
         * NO `min-height` HERE. A minimum on the bar becomes a minimum on each
         * ::item, the items grow past the 34 px bar and every title is clipped
         * away — the menu titles vanish entirely and the bar looks empty. The
         * height belongs to `TitleBar`, which fixes it.
         */
        QMenuBar                         { background: %(titlebar)s; color: %(menuText)s;
                                           border-bottom: 1px solid %(lineHard)s;
                                           font-size: 12.5px; }
        QMenuBar::item                   { padding: 5px 9px; background: transparent;
                                           border-radius: 4px; }
        QMenuBar::item:selected          { background: %(hoverChip)s; color: %(onHover)s; }
        QMenu                            { background: %(panel)s; color: %(text)s;
                                           border: 1px solid %(border)s; padding: 4px; }
        QMenu::item                      { padding: 6px 24px 6px 12px; border-radius: 4px; }
        QMenu::item:selected             { background: %(wash)s; color: %(text)s; }
        QMenu::item:disabled             { color: %(textFaint)s; }
        QMenu::separator                 { height: 1px; background: %(lineSoft)s; margin: 4px 8px; }

        /* ---- tool bar, §7 --------------------------------------------------- */
        /*
         * Measured off the reference, not guessed: 30x30 buttons on a 34 px
         * pitch (so a 4 px gap), a 1 px rule with 5 px either side between
         * groups, 8 px of padding at each end, and 45 px of content under a
         * 1 px rule, which is the 46 px band the reference measures. Qt puts a
         * border OUTSIDE min-height, so 45 here is 46 on screen — asking for 46
         * gives 47 and pushes every band below it down a pixel. Every one of
         * these numbers is load-bearing: the button pitch is what
         * button pitch is what puts the seventh group where the screenshot has
         * it, 300 px along the bar.
         */
        QToolBar                         { background: %(raised)s; border: none;
                                           border-bottom: 1px solid %(lineHard)s;
                                           min-height: 45px; max-height: 45px;
                                           padding: 0px 8px; spacing: 4px; }
        QToolBar::separator              { background: %(separator)s; width: 1px;
                                           margin: 12px 5px; }
        QToolButton                      { background: transparent; border: 1px solid transparent;
                                           border-radius: 4px; padding: 0px; margin: 0px;
                                           min-width: 30px; max-width: 30px;
                                           min-height: 30px; max-height: 30px;
                                           color: %(textDim)s; }
        QToolButton:hover                { background: %(hoverIcon)s; color: %(onHover)s; }
        QToolButton:checked,
        QToolButton:pressed              { background: %(wash)s; color: %(accentHi)s;
                                           border: 1px solid %(accentEdge)s; }
        QToolButton:disabled             { color: %(textFaint)s; }

        /* ---- docks, §6: one 29 px header, drawn by PanelHeader -------------- */
        /*
         * NO `QDockWidget::title` RULE. Every dock in this shell carries a
         * `PanelHeader` as its title bar widget, and a stylesheet rule for
         * ::title makes QStyleSheetStyle compute the title area from the RULE's
         * padding instead of from the widget — which silently adds pixels the
         * header does not know about and pushes the panel below it down.
         */
        QDockWidget                      { color: %(textDim)s; font-size: 11.5px;
                                           border: none; }
        QDockWidget > QWidget            { background: %(panel)s; }
        QMainWindow::separator:hover     { background: %(accent)s; }

        /* ---- dialogs, §8–§10 ----------------------------------------------- */
        QDialog#dialogFrame              { background: %(window)s; }
        QWidget#dialogFooter             { background: %(raised)s;
                                           border-top: 1px solid %(lineHard)s; }
        QWidget#settingsSidebar,
        QWidget#designerSidebar          { background: %(strip)s;
                                           border-right: 1px solid %(lineHard)s; }
        QWidget#symbolColumn             { background: %(panel)s;
                                           border-left: 1px solid %(lineHard)s; }
        QLineEdit#settingsSearch         { background: %(input)s; border: 1px solid %(border)s;
                                           border-radius: 4px; padding: 4px 8px;
                                           min-height: 20px; font-size: 11.5px; }
        QLabel#settingsProfile           { background: transparent; color: %(textFaint)s;
                                           font-size: 11px;
                                           border-top: 1px solid %(lineSoft)s; }
        QLabel#settingsHeading           { background: transparent; color: %(text)s;
                                           font-size: 16px; font-weight: 600; }
        QLabel#rowName                   { background: transparent; color: %(text)s;
                                           font-size: 12px; }
        QLabel#rowHelp                   { background: transparent; color: %(textFaint)s;
                                           font-size: 11px; }

        /* ---- attribute table, §9 -------------------------------------------- */
        QWidget#rendererRow              { background: %(raised)s;
                                           border-bottom: 1px solid %(lineHard)s; }
        QWidget#tableToolRow             { background: %(raised)s;
                                           border-bottom: 1px solid %(lineHard)s; }
        QWidget#tableFilterBar           { background: %(window)s;
                                           border-bottom: 1px solid %(lineHard)s; }
        QToolButton#tableTool            { background: transparent; border-radius: 4px;
                                           border: 1px solid transparent; padding: 0px;
                                           min-width: 30px; max-width: 30px;
                                           min-height: 30px; max-height: 30px; }
        QToolButton#tableTool:hover      { background: %(hoverIcon)s; }
        QToolButton#tableTool:checked    { background: %(wash)s;
                                           border: 1px solid %(accentEdge)s; }
        QLineEdit#expressionBar          { background: %(input)s; border: 1px solid %(border)s;
                                           border-radius: 4px; padding: 5px 10px;
                                           font-family: "IBM Plex Mono"; font-size: 12px;
                                           min-height: 22px; }
        QLineEdit#tableSearch            { background: %(input)s; border: 1px solid %(border)s;
                                           border-radius: 4px; padding: 5px 10px;
                                           font-size: 11.5px; min-height: 22px; }
        QWidget#statsPanel               { background: %(panel)s;
                                           border-left: 1px solid %(lineHard)s; }
        QTableView#attributeGrid         { background: %(window)s; border: none;
                                           gridline-color: %(lineSoft)s;
                                           font-family: "IBM Plex Mono"; font-size: 11.5px;
                                           selection-background-color: %(wash)s;
                                           selection-color: %(text)s; }
        QTableView#attributeGrid::item   { padding: 0px 8px; }
        QPushButton#segment              { background: %(input)s; color: %(textDim)s;
                                           border: 1px solid %(border)s; border-radius: 4px;
                                           padding: 4px 14px; min-height: 22px;
                                           font-size: 11.5px; }
        QPushButton#segment:checked      { background: %(wash)s; color: %(accentHi)s;
                                           border: 1px solid %(accentEdge)s; }

        /* ---- layers panel, §7 ---------------------------------------------- */
        /* The row is painted by LayerRowDelegate; the view must add nothing. */
        QTreeWidget#layerTree            { background: %(panel)s; border: none;
                                           outline: none; }
        QTreeWidget#layerTree::item      { border: none; padding: 0px; }
        QLabel#layerFooter               { background: %(panel)s; color: %(textFaint)s;
                                           border-top: 1px solid %(lineSoft)s;
                                           font-size: 11px; }

        /* ---- left tool box, §7: 46 px, 32x32 buttons ----------------------- */
        QToolButton#toolBoxButton        { background: transparent; border-radius: 4px;
                                           border: 1px solid transparent; padding: 0px;
                                           color: %(textDim)s; }
        QToolButton#toolBoxButton:hover  { background: %(hoverIcon)s; color: %(onHover)s; }
        QToolButton#toolBoxButton:checked,
        QToolButton#toolBoxButton:pressed { background: %(wash)s; color: %(accentHi)s;
                                           border: 1px solid %(accentEdge)s; }
        QToolButton#toolBoxButton:disabled { color: %(textFaint)s; }

        /* ---- tabs, §4: 30 px, active carries a 2 px accent edge ------------ */
        QTabWidget::pane                 { background: %(panel)s; border: 1px solid %(lineHard)s;
                                           top: -1px; }
        QTabBar                          { background: %(window)s; }
        QTabBar::tab                     { background: %(window)s; color: %(textDim)s;
                                           border: none; border-top: 2px solid transparent;
                                           padding: 7px 14px; min-height: 30px;
                                           font-size: 12.5px; }
        QTabBar::tab:hover:!selected     { background: %(hoverIcon)s; color: %(text)s; }
        QTabBar::tab:selected            { background: %(panel)s; color: %(text)s;
                                           border-top: 2px solid %(accent)s; }

        /* ---- lists and trees, §11: selection and hover never look alike ---- */
        QTreeView, QTreeWidget, QListView, QListWidget, QTableView, QTableWidget {
            background: %(window)s; alternate-background-color: %(rowOdd)s;
            color: %(text)s; border: 1px solid %(lineHard)s;
            selection-background-color: transparent; outline: none;
        }
        QTreeView::item, QListWidget::item, QTableView::item {
            padding: 5px 6px; min-height: 26px; border: none;
        }
        QTreeView::item:hover, QListWidget::item:hover, QTableView::item:hover {
            background: %(hoverRow)s;
        }
        QTreeView::item:selected, QListWidget::item:selected, QTableView::item:selected {
            background: %(wash)s; color: %(text)s;
            border-left: 2px solid %(accent)s;
        }
        QHeaderView::section             { background: %(header)s; color: %(textFaint)s;
                                           border: none; border-bottom: 1px solid %(lineHard)s;
                                           border-right: 1px solid %(lineSoft)s;
                                           padding: 7px 8px; min-height: 30px;
                                           font-size: 10.5px; font-weight: 600;
                                           letter-spacing: 0.7px; }

        /* ---- inputs, §4: 3-4 px radius and nothing wider ------------------- */
        /* ---- command line, §7: a 28 px strip, not a boxed field ------------ */
        /*
         * No border and no radius: the reference draws the prompt straight onto
         * a sunken strip that spans the canvas column, because the command line
         * IS the bottom edge of the drawing area rather than a control sitting
         * on it. The 1 px rule above it is the strip's own top border.
         */
        /* The strip paints itself; the bar around it must add nothing. */
        QStatusBar#statusHost            { background: %(strip)s; border: none;
                                           padding: 0px; margin: 0px;
                                           min-height: 26px; max-height: 26px; }
        QStatusBar#statusHost::item      { border: none; }

        QLineEdit#commandLine            { background: %(sunken)s; color: %(text)s;
                                           border: none; border-top: 1px solid %(lineHard)s;
                                           border-radius: 0px; padding: 0px 12px;
                                           min-height: 27px; max-height: 27px;
                                           font-family: "IBM Plex Mono"; font-size: 12px;
                                           selection-background-color: %(accent)s;
                                           selection-color: %(onAccent)s; }

        /* ---- inputs, `bileşen_standardı.png` -------------------------------- */
        /*
         * 30 px tall, 4 px radius, 1 px `--border`. Seven states, and each of
         * them says a DIFFERENT thing:
         *
         *   default    the value is what it was
         *   focus      the caret is here — 1 px accent plus a 2 px ring
         *   changed    edited and not yet saved — warn
         *   invalid    the value is not acceptable — danger
         *   read-only  it can be read and not written
         *   disabled   it is not available at all
         *   derived    it is computed; editing it is meaningless
         *
         * `changed` and `invalid` are the pair worth stating plainly: warn means
         * "you changed this", danger means "this is wrong". A dialog that used
         * one colour for both would tell a user their own edit was invalid.
         */
        QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: %(input)s; color: %(text)s;
            border: 1px solid %(border)s; border-radius: 4px;
            padding: 4px 9px; min-height: 22px; max-height: 22px;
            selection-background-color: %(accent)s; selection-color: %(onAccent)s;
        }
        QPlainTextEdit, QTextEdit         { max-height: 16777215px; }

        QLineEdit:hover, QSpinBox:hover,
        QDoubleSpinBox:hover, QComboBox:hover { border: 1px solid %(separator)s; }

        QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus,
        QDoubleSpinBox:focus, QComboBox:focus,
        QLineEdit[state="focus"]          { border: 1px solid %(accent)s;
                                            background: %(input)s; }

        QLineEdit[state="changed"], QComboBox[state="changed"],
        QSpinBox[state="changed"], QDoubleSpinBox[state="changed"] {
                                            border: 1px solid %(warn)s; color: %(warn)s;
                                            background: %(warnWash)s; }

        QLineEdit[state="invalid"], QComboBox[state="invalid"],
        QSpinBox[state="invalid"], QDoubleSpinBox[state="invalid"] {
                                            border: 1px solid %(dangerEdge)s; color: %(danger)s;
                                            background: %(dangerWash)s; }

        QLineEdit[state="derived"], QSpinBox[state="derived"],
        QDoubleSpinBox[state="derived"]   { border: 1px solid %(accentEdge)s;
                                            color: %(accentHi)s; }

        QLineEdit[readOnly="true"]        { background: transparent; color: %(textDim)s;
                                            border: 1px solid transparent; }

        QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled,
        QComboBox:disabled                { color: %(textFaint)s; background: transparent;
                                            border: 1px solid transparent; }

        QComboBox::drop-down              { border: none; width: 22px; }
        QComboBox QAbstractItemView       { background: %(panel)s; color: %(text)s;
                                            border: 1px solid %(border)s;
                                            selection-background-color: %(wash)s; }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 15px; border: none;
                                            background: transparent; }

        /* ---- buttons: six kinds, one hierarchy ----------------------------- */
        /*
         * `bileşen_standardı.png` measures 32 px and names the rule that matters
         * more than any measurement: ONE PRIMARY BUTTON PER SCREEN. A screen with
         * two primaries has told the user nothing about which one to press.
         *
         *   primary      the single action that closes the dialog — accent fill
         *   secondary    a non-destructive second action — outline
         *   ghost        low priority; toolbars and inline actions — no chrome
         *   destructive  cannot be undone — danger outline, always confirms
         *   mode         carries an on/off state — reads as pressed
         *   icon         32x32, 16 px glyph, tooltip only
         */
        QPushButton                       { background: transparent; color: %(textDim)s;
                                            border: 1px solid %(border)s; border-radius: 4px;
                                            padding: 0px 16px; min-height: 30px; max-height: 30px;
                                            font-size: 12px; }
        QPushButton:hover                 { background: %(hoverIcon)s; color: %(text)s; }
        QPushButton:pressed               { background: %(header)s; }
        QPushButton:disabled              { color: %(textFaint)s; border-color: %(lineSoft)s; }

        QPushButton#primary,
        QPushButton[primary="true"]       { background: %(accent)s; color: %(onAccent)s;
                                            border: 1px solid %(accentLift)s;
                                            font-weight: 500; }
        QPushButton#primary:hover,
        QPushButton[primary="true"]:hover { background: %(accentHi)s; }
        QPushButton#primary:disabled,
        QPushButton[primary="true"]:disabled { background: %(wash)s; color: %(textFaint)s;
                                            border: 1px solid %(accentEdge)s; }

        QPushButton#ghost                 { border: 1px solid transparent;
                                            background: transparent; }
        QPushButton#ghost:hover           { background: %(hoverIcon)s; }

        QPushButton#danger                { border: 1px solid %(dangerEdge)s; color: %(danger)s; }
        QPushButton#danger:hover          { background: %(dangerWash)s; }
        QPushButton#danger:disabled       { color: %(textFaint)s;
                                            border-color: %(lineSoft)s; }

        QPushButton#mode:checked          { background: %(wash)s; color: %(accentHi)s;
                                            border: 1px solid %(accentEdge)s; }

        QPushButton#iconButton            { min-width: 32px; max-width: 32px;
                                            min-height: 32px; max-height: 32px;
                                            padding: 0px; }

        /* ---- selection controls -------------------------------------------- */
        /*
         * 14 px, and the same accent everywhere. §13 forbids stating anything
         * with colour alone, so each of these carries a SHAPE as well: the tick,
         * the dash, the dot, the knob's side.
         */
        QCheckBox, QRadioButton           { color: %(text)s; spacing: 8px;
                                            font-size: 12px; }
        QCheckBox::indicator,
        QRadioButton::indicator           { width: 14px; height: 14px;
                                            border: 1px solid %(border)s; border-radius: 3px;
                                            background: %(input)s; }
        QCheckBox::indicator:hover,
        QRadioButton::indicator:hover     { border: 1px solid %(separator)s; }
        QCheckBox::indicator:checked      { background: %(accent)s;
                                            border: 1px solid %(accentLift)s; }
        QCheckBox::indicator:indeterminate { background: %(accent)s;
                                            border: 1px solid %(accentLift)s; }
        QCheckBox::indicator:disabled,
        QRadioButton::indicator:disabled  { background: transparent;
                                            border: 1px solid %(lineSoft)s; }
        QCheckBox:disabled, QRadioButton:disabled { color: %(textFaint)s; }
        QRadioButton::indicator           { border-radius: 7px; }
        QRadioButton::indicator:checked   { background: %(input)s;
                                            border: 4px solid %(accent)s; }

        /* ---- segment, slider ------------------------------------------------ */
        QPushButton#segment               { background: transparent; color: %(textDim)s;
                                            border: 1px solid %(border)s; border-radius: 0px;
                                            padding: 0px 14px; min-height: 24px;
                                            max-height: 24px; font-size: 11.5px; }
        QPushButton#segment:checked       { background: %(wash)s; color: %(accentHi)s;
                                            border: 1px solid %(accentEdge)s; }
        QPushButton#segment:hover         { background: %(hoverIcon)s; }

        QSlider::groove:horizontal        { height: 4px; background: %(lineSoft)s;
                                            border-radius: 2px; }
        QSlider::sub-page:horizontal      { background: %(accent)s; border-radius: 2px; }
        QSlider::handle:horizontal        { width: 12px; height: 12px; margin: -5px 0px;
                                            border-radius: 6px; background: %(readout)s; }

        QGroupBox                         { background: transparent; border: 1px solid %(lineSoft)s;
                                            border-radius: 4px; margin-top: 10px;
                                            padding: 10px 10px 9px 10px;
                                            color: %(textFaint)s; font-size: 10.5px;
                                            font-weight: 600; }
        QGroupBox::title                  { subcontrol-origin: margin;
                                            subcontrol-position: top left;
                                            left: 10px; padding: 0 5px;
                                            letter-spacing: 0.7px; }

        /* ---- bars, §4: command line 28 px, status bar 26 px ---------------- */
        QStatusBar                        { background: %(raised)s; color: %(textDim)s;
                                            border-top: 1px solid %(lineHard)s;
                                            min-height: 26px; }
        QStatusBar QLabel                 { color: %(textDim)s; padding: 0 6px; }
        QStatusBar::item                  { border: none; }

        /* ---- scrollbars: the mockup's own 10 px ---------------------------- */
        QScrollBar:vertical               { background: %(window)s; width: 10px; margin: 0; }
        QScrollBar:horizontal             { background: %(window)s; height: 10px; margin: 0; }
        QScrollBar::handle                { background: %(scroll)s; border-radius: 5px; }
        QScrollBar::handle:vertical       { min-height: 26px; }
        QScrollBar::handle:horizontal     { min-width: 26px; }
        QScrollBar::handle:hover          { background: %(scrollHi)s; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        /* ---- splitters, §6: 5 px, accent on hover -------------------------- */
        QSplitter::handle                 { background: %(lineHard)s; }
        QSplitter::handle:horizontal      { width: 5px; }
        QSplitter::handle:vertical        { height: 5px; }
        QSplitter::handle:hover           { background: %(accent)s; }

        QFrame[frameShape="4"],
        QFrame[frameShape="5"]            { color: %(lineSoft)s; }

        /* ---- named roles the shell asks for by object name ----------------- */
        QLabel#sectionTitle               { color: %(text)s; font-size: 16px; font-weight: 600; }
        QLabel#quiet                      { color: %(textFaint)s; }

        /* design.md 16.1: the field caption sits ABOVE its editor and stays out
           of the way — a form column is read down the values, not down the
           labels. */
        QLabel#formCaption                { background: transparent; color: %(textFaint)s;
                                            font-size: 11px; font-weight: 500; }

        /* A colour is a FIELD of the form, so it is the height of one. Its face
           and its ink are set per value in `show_colour`. */
        QToolButton#colourField           { border-radius: 4px; padding: 0px;
                                            min-height: 22px; max-height: 22px;
                                            /* The base QToolButton rule clamps every tool
                                               button to a 30 px square, which is right for
                                               an icon button and wrong for a field: it left
                                               the colour a chip with its hex elided to
                                               "#...6". A field is as wide as its column. */
                                            min-width: 0px; max-width: 16777215px;
                                            font-family: "IBM Plex Mono", monospace;
                                            font-size: 11px; }
        QLabel#mono                       { font-family: "IBM Plex Mono", monospace; }
        QWidget#toolBoxBody               { background: %(panel)s;
                                            border-right: 1px solid %(lineHard)s; }
        QFrame#toolBoxRule                { background: %(lineSoft)s; border: none; }
    )")
        .replace(QStringLiteral("%(window)s"), t.bgWindow.name())
        .replace(QStringLiteral("%(panel)s"), t.bgPanel.name())
        .replace(QStringLiteral("%(raised)s"), t.bgRaised.name())
        .replace(QStringLiteral("%(header)s"), t.bgHeader.name())
        .replace(QStringLiteral("%(titlebar)s"), t.bgTitlebar.name())
        .replace(QStringLiteral("%(input)s"), t.bgInput.name())
        .replace(QStringLiteral("%(lineHard)s"), t.lineHard.name())
        .replace(QStringLiteral("%(lineSoft)s"), t.lineSoft.name())
        .replace(QStringLiteral("%(border)s"), t.border.name())
        .replace(QStringLiteral("%(text)s"), t.text.name())
        .replace(QStringLiteral("%(textDim)s"), t.textDim.name())
        .replace(QStringLiteral("%(textFaint)s"), t.textFaint.name())
        .replace(QStringLiteral("%(accent)s"), t.accent.name())
        .replace(QStringLiteral("%(accentHi)s"), t.accentHi.name())
        .replace(QStringLiteral("%(accentEdge)s"), t.accentEdge.name())
        .replace(QStringLiteral("%(wash)s"), rgba(t.accentWash))
        .replace(QStringLiteral("%(onAccent)s"), t.onAccent.name())
        .replace(QStringLiteral("%(onHover)s"), t.onHover.name())
        .replace(QStringLiteral("%(hoverIcon)s"), t.hoverIcon.name())
        .replace(QStringLiteral("%(hoverChip)s"), t.hoverChip.name())
        .replace(QStringLiteral("%(menuText)s"), t.menuText.name())
        .replace(QStringLiteral("%(titleText)s"), t.titleText.name())
        .replace(QStringLiteral("%(strip)s"), t.bgStrip.name())
        .replace(QStringLiteral("%(separator)s"), t.separator.name())
        .replace(QStringLiteral("%(readout)s"), t.readout.name())
        .replace(QStringLiteral("%(hint)s"), t.hint.name())
        .replace(QStringLiteral("%(warnWash)s"), rgba(t.warnWash))
        .replace(QStringLiteral("%(warn)s"), t.warn.name())
        .replace(QStringLiteral("%(ok)s"), t.ok.name())
        .replace(QStringLiteral("%(danger)s"), t.danger.name())
        .replace(QStringLiteral("%(dangerEdge)s"), t.dangerEdge.name())
        .replace(QStringLiteral("%(dangerWash)s"), rgba(t.dangerWash))
        .replace(QStringLiteral("%(accentLift)s"), t.accentLift.name())
        .replace(QStringLiteral("%(sunken)s"), t.bgSunken.name())
        .replace(QStringLiteral("%(hoverRow)s"), t.hoverRow.name())
        .replace(QStringLiteral("%(rowOdd)s"), t.rowOdd.name())
        .replace(QStringLiteral("%(scroll)s"), t.border.name())
        .replace(QStringLiteral("%(scrollHi)s"), t.textFaint.name());
}

} // namespace piricad::app
