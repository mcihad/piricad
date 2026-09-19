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

class QListWidget;
class QScrollArea;
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

    const QString& sheet() const noexcept { return sheet_; }

    /// Which item is picked, or empty.
    const QString& selected() const noexcept { return selected_; }

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

    /// The page's rectangle inside this widget, fitted and centred.
    QRectF pageRect() const;

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

    /// Aims the sheet's first map frame at `window` and redraws — what picking a
    /// frame on the canvas does before this window opens.
    void aimAt(core::Box2 window);

    /// Drives the window the way a hand would, for `KENTOS_LAYOUT_PROBE`: picks
    /// an item, drags it, retypes a property and reports what the document ended
    /// up with.
    QStringList probeDrive();

private:
    QWidget* buildBody();
    QWidget* buildItemList();
    QWidget* buildProperties();

    /// Refills the list and the property panel from the document.
    void refresh();

    /// Runs one `ÇIKTIÖĞE` line for the selected item and refreshes.
    void edit(const QString& arguments);

    /// Adds an item of `kind` and selects it.
    void addItem(const QString& kind);

    /// Exports the sheet: `Dışa aktar` asks for a path and runs `YAZDIR`.
    void exportSheet();

    const core::Layout* layout() const;

    Controller& controller_;
    QString name_;

    LayoutCanvas* canvas_{nullptr};
    QListWidget* items_{nullptr};
    QWidget* properties_{nullptr};
    QVBoxLayout* propertyColumn_{nullptr};
    QLabel* status_{nullptr};

    /// True while `refresh()` is filling the widgets, so a `valueChanged` from
    /// setting a field does not run a command and refresh again.
    bool filling_{false};
};

} // namespace kentos::app
