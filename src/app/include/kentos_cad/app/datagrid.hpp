// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the one table, `design.md` §9.
//
// WHY A COMPONENT AND NOT A STYLESHEET. `QTableView` under a stylesheet gives a
// table Qt's way: a header that is a row of buttons, a selection that is a flat
// highlight over whatever was under it, gridlines on every side of every cell,
// and a row height that follows the font. `öznitelik_tablosu.png` draws none of
// that. It draws a 30 px header band with mono labels and an accent sort arrow, a
// 46 px row-number column at the left, zebra rows two shades apart, a selected
// row washed in the accent with a 2 px bar at its left edge, a NULL as a faint
// dash, an edited cell in the warn colour with a hairline ring — and it draws
// every one of those the same way in every table of the program.
//
// So the table is a widget of the component set, like the buttons are: the
// header paints its own sections, the delegate paints its own cells, the view
// tracks the row under the pointer, and a window that needs a table asks for a
// `DataGrid` rather than for a `QTableView` it then has to style by hand. The
// attribute table, the category table of the style designer and the object
// chooser are the same table at three sizes (design.md §9; ui.md R29).
//
// WHAT IT DOES NOT DO. It holds no data and reads no document — the model does
// that — and it does not decide what an edited cell IS. The model says so through
// `GridRole::Edited`, because only the model knows which entity a row is, and a
// mark kept by row number would move to the wrong row the next time the filter
// ran.
#pragma once

#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/theme.hpp"

#include <QHeaderView>
#include <QTableView>

class QPainter;

namespace kentos::app {

/// The roles a model may answer beyond Qt's own, so the grid can paint the
/// states §9 names without knowing what the cells mean.
namespace GridRole {
/// `true` for a cell the user wrote in this session and the document has not
/// yet saved to disk: warn ink, warn wash, a hairline warn ring (§9).
constexpr int Edited = Qt::UserRole + 41;
/// `true` for a cell that holds no value. Painted as a faint `—` whatever the
/// display text says, so a model need not choose the dash itself.
constexpr int Null = Qt::UserRole + 42;
} // namespace GridRole

/// The header band of a `DataGrid`, horizontal or vertical.
///
/// Horizontal: 30 px, mono labels in the faint ink, the sorted column in the
/// accent ink with its arrow, a hard rule under the band and a soft one between
/// sections. Vertical: the 46 px row-number column — right-aligned faint digits,
/// and the 2 px accent bar at the left edge of every selected row, which is the
/// mark §9 puts there and the one place a whole-row state can be painted once.
class GridHeader : public QHeaderView, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds a header for one orientation; the grid owns and sizes it.
    explicit GridHeader(Qt::Orientation orientation, QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

    /// The band's fixed height (horizontal) or the column's fixed width (vertical).
    QSize sectionSizeFromContents(int logicalIndex) const override;

protected:
    /// Draws one section: ground, label, sort arrow, rules, selection bar.
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// The cell painter of a `DataGrid`, and its editors.
///
/// It IS a `FieldDelegate`, so a grid that is given a spec function opens the
/// component set's editors in its cells and reports Enter through `advanced`;
/// one that is not is read-only. Painting is entirely its own: Qt's item
/// painting is not called, because everything it would draw — the highlight,
/// the focus rectangle, the gridline — is drawn here the standard's way.
class GridDelegate : public FieldDelegate
{
    Q_OBJECT

public:
    /// `specs` answers what column N holds; an empty function makes every cell
    /// read-only.
    explicit GridDelegate(SpecFor specs, QObject* parent = nullptr);

    /// The palette to paint with. Hides the base's, so the editors and the cells
    /// change theme together.
    void setTheme(ThemeMode mode);

    /// Paints the cell: zebra, hover, selection, the edited ring, the value.
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// The standard's 26 px row.
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// The table.
///
/// A `QTableView` whose header, cells and hover are the component set's. Give
/// it a model; give it `setEditors` when its cells may be opened; connect the
/// canvas to `selectionModel()` as with any view. Rows are 26 px, the header
/// 30, the row-number column 46 — `design.md` §9, measured off the reference.
class DataGrid : public QTableView, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty grid; give it a model.
    explicit DataGrid(QWidget* parent = nullptr);

    /// Installs the component set's editors over the cells: `specs` answers
    /// what column N holds. Until this is called the grid opens nothing.
    void setEditors(FieldDelegate::SpecFor specs);

    /// The delegate, for connecting `FieldDelegate::advanced`.
    GridDelegate* gridDelegate() const noexcept { return delegate_; }

    /// Whether the row-number column at the left is shown. On by default.
    void setRowNumbers(bool on);

    /// The row under the pointer, or -1. The delegate paints it a step lighter.
    int hoverRow() const noexcept { return hoverRow_; }

    void applyTheme(ThemeMode mode) override;

    ThemeMode theme() const noexcept { return theme_; }

protected:
    /// Paints the rows' zebra bands to the viewport's right edge before the cells
    /// go on: a table narrower than its window still reads as rows, not as a
    /// block of cells floating on the window ground (§9).
    void paintEvent(QPaintEvent* event) override;

    /// Tracks the hovered row so the WHOLE row lightens, not the one cell Qt
    /// would report.
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    GridHeader* columns_{nullptr};
    GridHeader* rows_{nullptr};
    GridDelegate* delegate_{nullptr};
    int hoverRow_{-1};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
