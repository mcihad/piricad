// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the layout designer.
//
// WHAT IT IS. A drafting table for one sheet: a tool strip along the top, the
// pages and the items on the left, the paper in the middle — zoomed and panned
// like a drawing, with rulers, smart guides and a marquee — the settings of what
// is picked on the right, and a status strip under it all. It is QGIS's print
// layout in this program's own components (design.md §15–§16), because that is
// the shape a cartographer already knows.
//
// AND EVERY ONE OF ITS GESTURES LEAVES AS A COMMAND. Dragging a title block
// emits `ÇIKTIÖĞE islem=tasi ad=baslik x=… y=…`; drawing a box with the Harita
// tool emits `ÇIKTIÖĞE islem=ekle tur=harita x=… y=… genislik=… yukseklik=…`;
// typing in a property field emits `islem=ayarla`. Nothing here writes to the
// document directly, which is not ceremony: it is what makes the designer
// undoable with the same Ctrl+Z as the rest of the program, journalled,
// scriptable and reachable by a model (CLAUDE.md 1.1, 1.2, 5.15). A designer
// that owned its own edits would be a second editing system with a second undo
// stack, which is exactly what this program refuses to have.
//
// ONE COMMAND PER GESTURE, NOT PER PIXEL. A drag writes when the mouse is
// RELEASED, with the boxes it ended at; the motion in between is drawn but not
// recorded. A gesture on several items at once — a group drag, an alignment —
// is ONE batch (`Controller::runLines`), so it is one Ctrl+Z (ui.md R40).
#pragma once

#include "piricad/app/dialog_chrome.hpp"
#include "piricad/app/widgets.hpp"

#include "piricad/core/layout.hpp"

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QButtonGroup;
class QListWidget;
class QMenu;
class QPainter;
class QScrollArea;
class QStyledItemDelegate;
class QVBoxLayout;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;
/// The inline editor; see fields.hpp.
class Field;

/// One item's box, named: what a group gesture ends with.
using ItemFrame = std::pair<QString, core::PaperRect>;

/// The page itself: draws the sheet and turns mouse gestures into geometry.
///
/// IT KNOWS THE LAYOUT AND NOT THE DOCUMENT'S WRITE PATH. It reports what the
/// user did through signals; the window above it decides which command that is.
class LayoutCanvas : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds an empty canvas; `setSheet` gives it something to draw.
    explicit LayoutCanvas(Controller& controller, QWidget* parent = nullptr);

    /// Draws `layout`'s page `page`. Held by NAME rather than by pointer: the
    /// document is rewritten whole on every edit (`Transaction::set_layouts`),
    /// so a pointer into the old list dangles the moment a command runs.
    void setSheet(const QString& layout, int page);

    /// Which page it is showing, counted from zero.
    int page() const noexcept { return page_; }

    /// The page it is showing, clamped into range; null when no layout is set.
    const core::LayoutPage* activePage() const;

    const QString& sheet() const noexcept { return sheet_; }

    /// The item the panel describes — the last one picked — or empty.
    const QString& selected() const noexcept { return primary_; }

    /// Every picked item, the primary one last.
    const QStringList& selection() const noexcept { return selection_; }

    /// The sheet's rectangle inside this widget, in device pixels.
    ///
    /// Public because a test needs to look at the PAPER and not at the chrome
    /// around it: the rulers and the pasteboard are this window's, and a check
    /// that what the preview draws matches what the printer draws has to
    /// compare the same area.
    QRect sheetRect() const;

    /// Picks `id` alone; empty clears the pick.
    void select(const QString& id);

    /// Picks exactly these, the last one the primary.
    void setSelection(const QStringList& ids);

    /// Re-reads the document. Called after any command touches the layout.
    void refresh();

    // ---- the view --------------------------------------------------------------

    /// Zooms by `factor` about a widget point, which stays where it is.
    void zoomBy(double factor, QPointF about);

    /// Zooms by `factor` about the middle of the view.
    void zoomBy(double factor);

    /// The whole page, fitted inside the view — the state it opens in, and the
    /// state it keeps across a resize until the user zooms.
    void zoomToFit();

    /// The paper at its own size on this screen: a 10 mm box is 10 mm here.
    void zoomToRealSize();

    /// How big the paper is drawn against its own size, in per cent.
    double zoomPercent() const;

    // ---- the tools -------------------------------------------------------------

    /// Arms drawing a new item of `kind` (`harita`, `metin`, … — `tur=`'s own
    /// words); empty goes back to picking.
    void setDrawKind(const QString& kind);

    const QString& drawKind() const noexcept { return drawKind_; }

    /// Whether a drag snaps to the page, its margin and the other items' edges
    /// and centres. On by default; the strip's `Yakala` switches it.
    void setSnapping(bool on);

    bool snapping() const noexcept { return snapping_; }

    void applyTheme(ThemeMode mode) override;

signals:
    /// The primary pick changed; empty when nothing is picked.
    void selectionChanged(const QString& id);

    /// One item's box changed — a resize, or the probe's drag.
    void itemMoved(const QString& id, core::PaperRect frame);

    /// Several boxes changed in one gesture: a group drag or a nudge.
    void itemsMoved(const QVector<piricad::app::ItemFrame>& frames);

    /// A box was drawn with a tool. An EMPTY frame is a click: the window
    /// places the kind's own default size about `frame.x`, `frame.y`.
    void itemDrawn(const QString& kind, core::PaperRect frame);

    /// The drawing tool is put down — drawn, or cancelled with Esc.
    void drawFinished();

    /// An item was double-clicked; the window puts the focus in its properties.
    void itemActivated(const QString& id);

    /// Delete or Backspace with something picked.
    void deleteRequested();

    /// The pointer's place on the paper, in millimetres; `onPage` false off it.
    void cursorAt(double x_mm, double y_mm, bool onPage);

    /// The zoom changed; `percent` against the paper's own size.
    void zoomChanged(double percent);

    /// The context menu was asked for at `global`, over the current pick.
    void contextRequested(const QPoint& global);

protected:
    /// The pasteboard, the sheet as the printer draws it, the guides, the picks
    /// and their handles, and the two millimetre rulers.
    void paintEvent(QPaintEvent* event) override;

    /// Picks, takes hold of a handle, starts a marquee, a drawn box or a pan.
    void mousePressEvent(QMouseEvent* event) override;

    /// Carries the gesture, or shows what the pointer is over.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Ends the gesture and reports it — ONE command per gesture.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// Asks the window to put the focus in the item's properties.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// The wheel zooms about the pointer, as it does on the map.
    void wheelEvent(QWheelEvent* event) override;

    /// Arrow keys nudge the pick by a millimetre, Shift by ten; Delete deletes,
    /// Esc puts the tool down or clears the pick, Ctrl+A picks the page, `+`,
    /// `-` and `0` zoom — the keyboard reaches every gesture (ui.md R21).
    void keyPressEvent(QKeyEvent* event) override;

    /// Space held is a hand on the paper.
    void keyReleaseEvent(QKeyEvent* event) override;

    /// A fitted view stays fitted as the window is resized.
    void resizeEvent(QResizeEvent* event) override;

    /// Picks what is under the pointer, then asks for the menu.
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// The pointer left: no hover outline, no coordinate.
    void leaveEvent(QEvent* event) override;

private:
    /// Which handle of the picked item is under `at`, or `None`.
    ///
    /// EIGHT HANDLES AND A BODY, the shape every page editor has had since
    /// PageMaker: four corners resize both ways, four edges resize one way, and
    /// the inside moves. A user who has used one has used this.
    enum class Grip : std::uint8_t {
        None,
        Body,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
    };

    /// What the held button is doing.
    enum class Gesture : std::uint8_t { None, Move, Resize, Marquee, Draw, Pan };

    /// A guide the last drag snapped to: a line across the page.
    struct SnapLine
    {
        bool vertical{true}; ///< a vertical line at `at` across the page, or a horizontal one
        core::Um at{0};      ///< where, in paper micrometres
    };

    /// The layout as it stands, or null when the document has no such sheet.
    const core::Layout* layout() const;

    /// The page's rectangle inside this widget: fitted, or placed by the zoom.
    QRectF pageRect() const;

    /// Device pixels per paper millimetre, as drawn now.
    double pixelsPerMm() const;

    /// Paper micrometres from a widget point, and a widget rectangle from a box.
    QPointF paperAt(QPointF widget) const;
    QRectF deviceFrom(const core::PaperRect& paper) const;

    /// The item at `at` drawn on top, or null.
    const core::LayoutItem* itemAt(QPoint at) const;

    Grip gripAt(const QPoint& at) const;
    static Qt::CursorShape cursorFor(Grip grip);

    /// The x and y a moving box can snap to on this page: its edges and centre,
    /// its margin, and every item's that is not moving.
    void snapTargets(std::vector<core::Um>& xs, std::vector<core::Um>& ys) const;

    /// The smallest move within reach that puts one of `edges` on one of
    /// `targets`; zero and no line when none is near. Records the line.
    core::Um snapAxis(std::initializer_list<core::Um> edges, const std::vector<core::Um>& targets,
                      bool vertical);

    /// The frames a drag from the press to `at` produces.
    void dragTo(const QPoint& at, Qt::KeyboardModifiers modifiers);

    /// Draws the two millimetre scales and lights the pick's span on them.
    void paintRulers(QPainter& p, const QRectF& box, const QRectF* lit) const;

    /// The sheet as the printer draws it, cached until the document, the zoom
    /// or the page changes: a drag repaints on every mouse move, and a map
    /// frame redrawn at sixty frames a second is a drag that stutters.
    void paintSheet(QPainter& p, const QRectF& box);

    Controller& controller_;
    QString sheet_;
    int page_{0};
    QStringList selection_;
    QString primary_;
    QString hover_;
    QString drawKind_;
    bool snapping_{true};

    // The view: fitted, or a scale and the widget point of the paper's corner.
    bool fit_{true};
    double scale_{1.0}; ///< device pixels per paper micrometre when not fitted
    QPointF corner_{};  ///< the paper's top-left in widget pixels when not fitted

    Gesture gesture_{Gesture::None};
    Grip grip_{Grip::None};
    QPoint press_{};
    QPointF panFrom_{};
    bool spaceHeld_{false};
    std::vector<ItemFrame> starts_;
    std::vector<ItemFrame> live_;
    QRectF marquee_{};
    core::PaperRect drawn_{};
    std::vector<SnapLine> guides_;

    QImage cache_;
    std::uint64_t cacheRevision_{0};
    QSize cacheSize_{};
    QString cacheSheet_;
    int cachePage_{-1};

    ThemeMode theme_{ThemeMode::Dark};
};

/// The window: the tool strip, the pages and items, the paper, the settings and
/// the status strip.
class LayoutDesigner : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on the layout of that name. A name the document does not have is
    /// reported rather than silently creating one.
    LayoutDesigner(Controller& controller, QString layout, QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

    /// Picks the item of that id and opens its settings, as clicking it does.
    ///
    /// A WINDOW THAT CAN BE OPENED ON SOMETHING. The manager and the menu both
    /// know which box a user came here for; without this they could only hand
    /// over a sheet and leave the user to find it again.
    void showItem(const QString& id);

    /// Aims the sheet's first map frame at `window` and redraws — what picking a
    /// frame on the canvas does before this window opens.
    void aimAt(core::Box2 window);

    /// What the main window's map shows, so a map frame can be aimed at it from
    /// here (`Ana pencereden al`). Empty hides the button.
    void setViewWindow(core::Box2 window);

    /// Drives the window the way a hand would, for `PIRICAD_LAYOUT_PROBE`: picks
    /// an item, drags it, retypes a property and reports what the document ended
    /// up with.
    QStringList probeDrive();

    /// What share of the sheet `probeDrive` last found still untouched paper,
    /// as a percentage. See `probeDrive` for the defect this measures; -1 until
    /// it has run.
    int probeBlankPaper() const noexcept { return blankPaperPercent_; }

private:
    /// Which of the two the inspector shows.
    enum class Pane : std::uint8_t { Item, Sheet };

    QWidget* buildBody();

    /// The row over the sheet: history, arranging the pick, the page and the
    /// view — every one a bare 32 px icon with its name in the tooltip.
    QWidget* buildToolRow();

    /// The column of tools down the sheet's left edge: pick, and the nine kinds
    /// of item a sheet is made of.
    QWidget* buildToolRail();

    /// The items on the page, and under them the inspector for the pick.
    QWidget* buildInspector();

    QWidget* buildStatusStrip();

    /// Refills the list, the inspector, the rows and the strips from the document.
    void refresh();

    void refreshItems(const core::Layout& l);

    /// What the tool row can do for the pick, and which page is showing.
    void refreshToolRow(const core::Layout& l);

    /// The status strip: the pick, the checks and the hint.
    void refreshStatus(const core::Layout& l);

    /// Refills the inspector for what is picked, or for the sheet.
    void buildProperties();

    /// The sheet's own settings — paper, orientation, margin, resolution,
    /// pages, name — and what the checks found.
    void buildSheetProperties(const core::Layout& l);

    /// Every setting `ÇIKTIÖĞE` takes for `item`, grouped the way a person
    /// decides them: where it sits, what it shows, how it looks.
    void buildItemProperties(const core::Layout& l, const core::LayoutItem& item);

    /// What several picks share: the list of them and the gestures for all.
    void buildGroupProperties(const core::Layout& l);

    /// Runs one `ÇIKTIÖĞE` line for the primary pick and refreshes.
    ///
    /// `verb` is `ayarla` for everything except moving an item to another page,
    /// which is a move and goes through `tasi` like every other one.
    void edit(const QString& arguments, const QString& verb = QStringLiteral("ayarla"));

    /// Runs one `ÇIKTIÖĞE` line for EVERY picked item, as one batch.
    void editAll(const QString& arguments, const QString& label);

    /// Runs one `ÇIKTIYERLEŞİMİ islem=sayfa` line for the page on screen, with
    /// `change` overriding the sheet as it stands.
    ///
    /// THE WHOLE PAGE IS RESTATED, not just what moved: the command defaults
    /// every argument it is not given, so a line saying only `kenar=15` would
    /// also resize the sheet to A4. See the source for the rest.
    void sheetEdit(const QString& change);

    /// Runs one of the page verbs of `core.layout` on the active page.
    void pageVerb(const char* verb);

    /// Shows page `index` (0-based) of the sheet.
    void showPage(int index);

    /// Adds an item of `kind` at its default place and picks it.
    void addItem(const QString& kind);

    /// Adds an item of `kind` in `frame`, drawn with a tool — or about the click
    /// when `frame` is empty — and picks it.
    void placeItem(const QString& kind, core::PaperRect frame);

    /// Moves the picked items so their edges or centres line up.
    void alignPicked(int how);

    /// Spreads the picked items so the gaps between them are equal.
    void spreadPicked(bool across);

    /// Raises or lowers the picked items: `to` is `on`, `up`, `down` or `back`.
    void restackPicked(const QString& to);

    /// Copies the picked items beside themselves and picks the copies.
    void duplicatePicked();

    /// Locks the pick, or unlocks it when every picked item is locked already.
    void toggleLockPicked();

    /// Deletes the picked items, as one batch.
    void deletePicked();

    /// The menu the items' rows and the canvas share.
    QMenu* itemMenu();

    /// Exports the sheet: asks for a path and runs `YAZDIR`.
    void exportSheet();

    const core::Layout* layout() const;

    // ---- the inspector's rows: design.md §8's `110px | 1fr` grid ----------
    //
    // A CAPTION BESIDE ITS VALUE, not above it. §16.1 stacks them for the
    // four-column record form; §8 is a property column, read down the values,
    // and stacked there every property took two lines and the column ran out of
    // width and height at once (see `form_cell` in `style_designer.cpp`).

    /// A group heading, with an optional note at its far end.
    void group(const QString& title, const QString& note = QString());

    /// One row: `caption` in the fixed column, `editor` in the rest.
    QWidget* row(const QString& caption, QWidget* editor);

    /// A dim line under the last row, in the editor column.
    void help(const QString& text);

    /// Where an editor sends what it writes: `edit` on the pick when empty; a
    /// table column's editors send it through `sutunayarla` instead.
    using Writer = std::function<void(const QString&)>;

    /// A paper-millimetre field that writes `name=` on the pick.
    QWidget* mmEditor(core::Um value, const char* name, const QString& spoken, int decimals = 1,
                      const Writer& write = {});

    /// A whole-number field that writes `name=` on the pick.
    QWidget* countEditor(long long value, const char* name, const QString& spoken, int least,
                         int most, const QString& unit = QString(), const Writer& write = {});

    /// A free-text field that writes a quoted `name=` on the pick.
    QWidget* textEditor(const QString& value, const char* name, const QString& spoken,
                        const QString& hint, const Writer& write = {});

    /// A colour field that writes `name=#RRGGBB` on the pick.
    QWidget* colourEditor(std::uint32_t value, const char* name, const QString& spoken,
                          const Writer& write = {});

    /// A switch that writes `name=evet|hayir` on the pick.
    QWidget* switchEditor(bool on, const char* name, const QString& spoken,
                          const Writer& write = {});

    /// Several words, one written: a segment that writes `name=<word>`.
    QWidget* wordsEditor(const QStringList& shown, const QStringList& words, int current,
                         const char* name, const QString& spoken, const Writer& write = {});

    /// A table's own section of the inspector: its rows, its columns — the
    /// list, and the one picked in it — its head, its lines and its type.
    void buildTableProperties(const core::Layout& l, const core::LayoutItem& item);

    /// Which map frame an item belongs to.
    QWidget* mapEditor(const core::Layout& l, const core::LayoutItem& item);

    Controller& controller_;
    QString name_;
    core::Box2 viewWindow_{};

    LayoutCanvas* canvas_{nullptr};
    QListWidget* items_{nullptr};

    /// Draws the item rows; see `ItemRow` in the source.
    QStyledItemDelegate* rows_{nullptr};

    /// The rail's tools: 0 picks, 1… are `kKinds` in order.
    QButtonGroup* tools_{nullptr};

    /// The tool row's buttons that act on the pick, enabled by what is picked.
    std::vector<Button*> needOne_;   ///< anything picked
    std::vector<Button*> needThree_; ///< three or more picked
    Button* lockButton_{nullptr};
    Button* snapSwitch_{nullptr};
    Button* pagePrev_{nullptr};
    Button* pageNext_{nullptr};
    Button* pageMenu_{nullptr};
    Button* zoomReadout_{nullptr};

    /// The inspector's heading: the name of what is being inspected, and under
    /// it the kind, the id and the size.
    QLabel* headGlyph_{nullptr};
    QLabel* headName_{nullptr};
    QLabel* headKind_{nullptr};

    /// `Öğe | Sayfa`, over the inspector.
    Segment* paneSwitch_{nullptr};
    Pane pane_{Pane::Sheet};

    QLabel* itemsCount_{nullptr};

    QScrollArea* scroll_{nullptr};
    QWidget* properties_{nullptr};
    QVBoxLayout* propertyColumn_{nullptr};

    /// What the inspector was last built for — the pane and the pick — so a
    /// rebuild for the SAME subject keeps its scroll position and a rebuild
    /// for another one starts at the top.
    QString shownFor_;

    QLabel* cursorReadout_{nullptr};
    QLabel* pickReadout_{nullptr};
    Button* troubleReadout_{nullptr};
    QLabel* hint_{nullptr};

    /// Which of a table's columns the inspector is editing, from 0.
    int tableColumn_{0};

    /// See `probeBlankPaper`.
    int blankPaperPercent_{-1};

    /// True while `refresh()` is filling the widgets, so a `valueChanged` from
    /// setting a field does not run a command and refresh again.
    bool filling_{false};
};

} // namespace piricad::app
