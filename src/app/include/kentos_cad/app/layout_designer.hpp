// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the layout designer.
//
// WHAT IT IS. A window onto one sheet: the page in the middle, the items listed
// on the left, the selected item's properties on the right, and a bar that adds
// a new item. A box is picked by clicking it, moved by dragging it and resized
// by its corner handles.
//
// AND EVERY ONE OF THOSE GESTURES LEAVES AS A COMMAND. Dragging a title block
// emits `ÇIKTIÖĞE islem=tasi ad=baslik x=… y=…`; typing in a property field
// emits `islem=ayarla`. Nothing here writes to the document directly, which is
// not ceremony: it is what makes the designer undoable with the same Ctrl+Z as
// the rest of the program, journalled, scriptable and reachable by a model
// (CLAUDE.md 1.1, 1.2, 5.15). A designer that owned its own edits would be a
// second editing system with a second undo stack, which is exactly what this
// program refuses to have.
//
// ONE COMMAND PER GESTURE, NOT PER PIXEL. A drag writes one line when the mouse
// is RELEASED, with the box it ended at; the motion in between is drawn but not
// recorded. Otherwise a single drag across a page would be four hundred undo
// entries and four hundred journal lines.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/core/layout.hpp"

#include <QString>
#include <QWidget>

class QPainter;
class QListWidget;
class QScrollArea;
class QStyledItemDelegate;
class QVBoxLayout;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;
/// The inline editor; see fields.hpp.
class Field;

/// The page itself: draws the sheet and turns mouse gestures into geometry.
///
/// IT KNOWS THE LAYOUT AND NOT THE DOCUMENT'S WRITE PATH. It reports what the
/// user did through signals; the window above it decides which command that is.
class LayoutCanvas : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

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

    /// Which item is picked, or empty.
    const QString& selected() const noexcept { return selected_; }

    /// The sheet's rectangle inside this widget, in device pixels.
    ///
    /// Public because a test needs to look at the PAPER and not at the chrome
    /// around it: the rulers and the surround are this window's, and a check
    /// that what the preview draws matches what the printer draws has to
    /// compare the same area.
    QRect sheetRect() const;

    void select(const QString& id);

    /// Re-reads the document. Called after any command touches the layout.
    void refresh();

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user picked an item, or cleared the pick with an empty string.
    void selectionChanged(const QString& id);

    /// A drag ended. The window turns this into `ÇIKTIÖĞE islem=tasi`.
    void itemMoved(const QString& id, core::PaperRect frame);

    /// An item was double-clicked; the window puts the focus in its properties.
    void itemActivated(const QString& id);

protected:
    /// Draws the sheet through `paint_layout_page`, then the selection's frame
    /// and its eight handles on top.
    void paintEvent(QPaintEvent* event) override;

    /// Picks an item, or takes hold of the selected one's handle.
    void mousePressEvent(QMouseEvent* event) override;

    /// Drags the held item, or just shows which handle is under the pointer.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Ends the drag and reports the frame it ended at — ONE command per
    /// gesture, not per pixel.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// Asks the window to put the focus in the item's properties.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// Arrow keys nudge the selection by a millimetre, Shift by ten, so the
    /// keyboard reaches every gesture the mouse does (ui.md R21).
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Which handle of the selected item is under `at`, or `None`.
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

    /// The layout as it stands, or null when the document has no such sheet.
    const core::Layout* layout() const;

    /// The page's rectangle inside this widget, fitted and centred inside the
    /// rulers' gutter.
    QRectF pageRect() const;

    /// Draws the two millimetre scales and lights `item`'s span on them.
    ///
    /// THE SPAN IS THE POINT, not the ticks: it says where on the paper the
    /// selection sits and how wide it is, as one picture rather than as four
    /// numbers read one at a time — and it follows a drag, so the gesture is
    /// measured while it happens.
    void paintRulers(QPainter& p, const QRectF& box, const core::LayoutItem* item) const;

    /// A paper point from a widget point, and back.
    core::PaperRect paperFrom(const QRectF& device) const;
    QRectF deviceFrom(const core::PaperRect& paper) const;

    Grip gripAt(const QPoint& at) const;
    static Qt::CursorShape cursorFor(Grip grip);

    /// The frame a drag from `press_` to `at` produces, snapped to the page's
    /// millimetre grid and kept at or above a minimum size.
    core::PaperRect dragged(const QPoint& at) const;

    Controller& controller_;
    QString sheet_;
    int page_{0};
    QString selected_;

    Grip grip_{Grip::None};
    bool dragging_{false};
    QPoint press_{};
    core::PaperRect start_{};
    core::PaperRect live_{};

    ThemeMode theme_{ThemeMode::Dark};
};

/// The window: the page, the item list, the properties and the export bar.
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

    /// Drives the window the way a hand would, for `KENTOS_LAYOUT_PROBE`: picks
    /// an item, drags it, retypes a property and reports what the document ended
    /// up with.
    QStringList probeDrive();

    /// What share of the sheet `probeDrive` last found still untouched paper,
    /// as a percentage. See `probeDrive` for the defect this measures; -1 until
    /// it has run.
    int probeBlankPaper() const noexcept { return blankPaperPercent_; }

private:
    /// Which page the canvas shows, as a number the user types. Pages are counted
    /// from one here and from zero in the array.
    Field* pageField_{nullptr};

    /// Runs one of the page verbs of `core.layout` on the active page.
    void pageVerb(const char* verb);

    QWidget* buildBody();
    QWidget* buildItemList();
    QWidget* buildProperties();

    /// Refills the list and the property panel from the document.
    void refresh();

    /// Runs one `ÇIKTIÖĞE` line for the selected item and refreshes.
    ///
    /// `verb` is `ayarla` for everything except moving an item to another page,
    /// which is a move and goes through `tasi` like every other one.
    void edit(const QString& arguments, const QString& verb = QStringLiteral("ayarla"));

    /// Runs one `ÇIKTIYERLEŞİMİ islem=sayfa` line for the page on screen, with
    /// `change` overriding the sheet as it stands.
    ///
    /// THE WHOLE PAGE IS RESTATED, not just what moved: the command defaults
    /// every argument it is not given, so a line saying only `kenar=15` would
    /// also resize the sheet to A4. See the source for the rest.
    void sheetEdit(const QString& change);

    /// Fills the inspector with the sheet's own settings — paper, orientation,
    /// margin, resolution and name. Shown whenever no item is picked, because a
    /// sheet always exists and "select an item" is not worth a column.
    void buildSheetProperties(const core::Layout& l);

    /// Fills the inspector with every setting `ÇIKTIÖĞE` takes for `item`.
    void buildItemProperties(const core::Layout& l, const core::LayoutItem& item);

    /// Two form rows on one line — position is a pair, size is a pair.
    QWidget* pairOf(QWidget* left, QWidget* right);

    /// A paper-millimetre row that writes `name=` on the selected item.
    FormRow* mmRow(const QString& label, core::Um value, const char* name);

    /// A whole-number row that writes `name=` on the selected item.
    FormRow* countRow(const QString& label, long long value, const char* name, int most);

    /// A text row that writes a quoted `name=` on the selected item.
    FormRow* textRow(const QString& label, const QString& value, const char* name,
                     const QString& hint);

    /// Adds an item of `kind` and selects it.
    void addItem(const QString& kind);

    /// Exports the sheet: `Dışa aktar` asks for a path and runs `YAZDIR`.
    void exportSheet();

    const core::Layout* layout() const;

    Controller& controller_;
    QString name_;

    LayoutCanvas* canvas_{nullptr};
    QListWidget* items_{nullptr};

    /// Draws the item rows; see `ItemRow` in the source.
    QStyledItemDelegate* rows_{nullptr};

    /// The inspector's heading: the name of what is being inspected, one step
    /// larger than the body — the only type in this window that is.
    QLabel* headName_{nullptr};

    /// The dim line under it: the kind, the id and the size.
    QLabel* headKind_{nullptr};

    /// The `ÖĞELER` heading, whose note carries the count.
    FormSection* itemsHead_{nullptr};

    /// `2 / 7`, beside the page number rather than in a help line under it.
    QLabel* pageCount_{nullptr};

    /// Lit only while something is picked.
    Button* remove_{nullptr};
    QWidget* properties_{nullptr};
    QVBoxLayout* propertyColumn_{nullptr};
    QLabel* status_{nullptr};

    /// See `probeBlankPaper`.
    int blankPaperPercent_{-1};

    /// True while `refresh()` is filling the widgets, so a `valueChanged` from
    /// setting a field does not run a command and refresh again.
    bool filling_{false};
};

} // namespace kentos::app
