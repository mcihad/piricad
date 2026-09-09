// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/datagrid.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <QFont>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyleOptionViewItem>

#include <algorithm>

namespace kentos::app {
namespace {

// `design.md` §9, measured off `öznitelik_tablosu.png`.
constexpr int kHeaderBand = 30; ///< the column header's height
constexpr int kRowNumbers = 46; ///< the row-number column's width
constexpr int kRow        = 26; ///< one row
constexpr int kCellPadX   = 8;  ///< text inset from the cell's edges
constexpr int kBar        = 2;  ///< the selected row's accent bar
constexpr int kMonoPx     = 12; ///< a value; §9 says 11.5 and Qt has no half pixel
constexpr int kHeaderPx   = 11; ///< a header label; §9 says 10.5
constexpr int kArrow      = 4;  ///< the sort arrow's half width

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QFont mono(int px, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setPixelSize(px);
    f.setWeight(weight);
    return f;
}

QFont sans(int px)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(px);
    return f;
}

/// Whether a cell reads as a figure: the model says so through its alignment,
/// which is the same signal the reader uses — a column of numbers is a column
/// that lines up on the right.
bool numeric(const QModelIndex& index)
{
    const QVariant align = index.data(Qt::TextAlignmentRole);
    return align.isValid() && (align.toInt() & static_cast<int>(Qt::AlignRight)) != 0;
}

} // namespace

// =============================================================================
// GridHeader
// =============================================================================

GridHeader::GridHeader(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    setObjectName(QStringLiteral("gridHeader"));
    setHighlightSections(false);
    setSectionsClickable(orientation == Qt::Horizontal);
    setSectionsMovable(orientation == Qt::Horizontal);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setStretchLastSection(false);
    if (orientation == Qt::Vertical) {
        setSectionResizeMode(QHeaderView::Fixed);
        setDefaultSectionSize(kRow);
        setFixedWidth(kRowNumbers);
    } else {
        setFixedHeight(kHeaderBand);
        setSectionResizeMode(QHeaderView::Interactive);
    }
}

void GridHeader::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    viewport()->update();
}

QSize GridHeader::sectionSizeFromContents(int logicalIndex) const
{
    const QSize base = QHeaderView::sectionSizeFromContents(logicalIndex);
    return orientation() == Qt::Horizontal ? QSize(base.width() + 2 * kCellPadX, kHeaderBand)
                                           : QSize(kRowNumbers, kRow);
}

void GridHeader::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    const Tokens& t = tokensOf(theme_);
    if (!rect.isValid() || model() == nullptr) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->fillRect(rect, t.bgHeader);

    if (orientation() == Qt::Horizontal) {
        const bool sorted = isSortIndicatorShown() && sortIndicatorSection() == logicalIndex;
        const QString text =
            model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();
        const QVariant align =
            model()->headerData(logicalIndex, Qt::Horizontal, Qt::TextAlignmentRole);
        const bool right =
            align.isValid() && (align.toInt() & static_cast<int>(Qt::AlignRight)) != 0;

        // Mono, tracked, faint — and the sorted column in the accent ink with its
        // arrow after the label, where the eye lands after reading the name.
        QFont face = mono(kHeaderPx, QFont::DemiBold);
        face.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
        painter->setFont(face);
        painter->setPen(sorted ? t.accentHi : t.textFaint);

        QRect box = rect.adjusted(kCellPadX, 0, -kCellPadX, 0);
        if (sorted) box.adjust(0, 0, -(2 * kArrow + 6), 0);
        const int flags =
            static_cast<int>((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        const QString shown = painter->fontMetrics().elidedText(text, Qt::ElideRight, box.width());
        painter->drawText(box, flags, shown);

        if (sorted) {
            const int textW = painter->fontMetrics().horizontalAdvance(shown);
            const int ax    = right ? rect.right() - kCellPadX - kArrow
                                    : rect.left() + kCellPadX + textW + 6 + kArrow;
            const int ay    = rect.center().y();
            const bool up   = sortIndicatorOrder() == Qt::AscendingOrder;
            QPainterPath arrow;
            arrow.moveTo(ax - kArrow, up ? ay + 2 : ay - 2);
            arrow.lineTo(ax + kArrow, up ? ay + 2 : ay - 2);
            arrow.lineTo(ax, up ? ay - 2 : ay + 2);
            arrow.closeSubpath();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->fillPath(arrow, t.accentHi);
            painter->setRenderHint(QPainter::Antialiasing, false);
        }

        // A hard rule under the band, a soft one between sections.
        painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), 1), t.lineHard);
        painter->fillRect(QRect(rect.right(), rect.top() + 7, 1, rect.height() - 14), t.lineSoft);
    } else {
        // The row number, right-aligned and faint; the selected row's accent bar
        // at the far left, 2 px, the height of the row (§9).
        const bool selected = selectionModel() != nullptr &&
                              selectionModel()->isRowSelected(logicalIndex, rootIndex());
        if (selected) {
            painter->fillRect(rect, t.accentWash);
            painter->fillRect(QRect(rect.left(), rect.top(), kBar, rect.height()), t.accent);
        }
        painter->setFont(mono(kHeaderPx));
        painter->setPen(selected ? t.textDim : t.textFaint);
        painter->drawText(rect.adjusted(0, 0, -kCellPadX, 0), Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(logicalIndex + 1));
        painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), 1), t.lineSoft);
        painter->fillRect(QRect(rect.right(), rect.top(), 1, rect.height()), t.lineSoft);
    }
    painter->restore();
}

// =============================================================================
// GridDelegate
// =============================================================================

GridDelegate::GridDelegate(SpecFor specs, QObject* parent) : FieldDelegate(std::move(specs), parent)
{}

void GridDelegate::setTheme(ThemeMode mode)
{
    FieldDelegate::setTheme(mode);
    theme_ = mode;
}

QSize GridDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QSize base = FieldDelegate::sizeHint(option, index);
    return {base.width() + 2 * kCellPadX, kRow};
}

void GridDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
    const Tokens& t     = tokensOf(theme_);
    const auto* grid    = qobject_cast<const DataGrid*>(option.widget);
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered  = grid != nullptr && grid->hoverRow() == index.row();
    const bool edited   = index.data(GridRole::Edited).toBool();
    const bool isNull   = index.data(GridRole::Null).toBool() ||
                        index.data(Qt::DisplayRole).toString() == QStringLiteral("—");
    const bool figure     = numeric(index);
    const bool hasCurrent = (option.state & QStyle::State_HasFocus) != 0 && grid != nullptr &&
                            grid->currentIndex() == index && grid->hasFocus();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // THE GROUND, in §9's order of precedence: selection over hover over zebra.
    // Each is a token, so the light theme gets its own two shades rather than
    // the dark theme's greys inverted.
    QColor ground = index.row() % 2 == 0 ? t.rowEven : t.rowOdd;
    if (hovered) ground = t.hoverRow;
    if (selected) ground = t.accentWash;
    painter->fillRect(option.rect, ground);

    // An edited cell keeps its own wash and a hairline ring INSIDE the cell, so
    // it reads as edited whether or not its row is selected.
    if (edited) {
        painter->fillRect(option.rect, t.warnWash);
        painter->setPen(QPen(t.warn, 1));
        painter->drawRect(option.rect.adjusted(1, 1, -2, -2));
    }

    // The soft rules: one under the row, one at the cell's right edge.
    painter->fillRect(QRect(option.rect.left(), option.rect.bottom(), option.rect.width(), 1),
                      t.lineSoft);
    painter->fillRect(QRect(option.rect.right(), option.rect.top(), 1, option.rect.height()),
                      t.lineSoft);

    // A CHECK BOX, when the model offers a check state: the same 14 px box the
    // component draws, filled when on, a bar when partial.
    const QVariant check = index.data(Qt::CheckStateRole);
    if (check.isValid()) {
        const auto state = static_cast<Qt::CheckState>(check.toInt());
        const QRect box(option.rect.left() + (option.rect.width() - 14) / 2,
                        option.rect.top() + (option.rect.height() - 14) / 2, 14, 14);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(state == Qt::Unchecked ? t.border : t.accent, 1));
        painter->setBrush(state == Qt::Unchecked ? QBrush(t.bgInput) : QBrush(t.accent));
        painter->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);
        painter->setPen(QPen(t.onAccent, 1.8));
        if (state == Qt::Checked)
            painter->drawPolyline(QPolygonF({QPointF(box.left() + 3.2, box.top() + 7.4),
                                             QPointF(box.left() + 6.0, box.top() + 10.2),
                                             QPointF(box.left() + 11.0, box.top() + 4.6)}));
        else if (state == Qt::PartiallyChecked)
            painter->drawLine(QPointF(box.left() + 3.5, box.center().y() + 0.5),
                              QPointF(box.right() - 2.5, box.center().y() + 0.5));
        painter->restore();
        return;
    }

    // A PICTURE, when the model offers one: a symbol swatch in the category
    // table, drawn at its own size and left-aligned like a word.
    const QVariant decoration = index.data(Qt::DecorationRole);
    if (decoration.canConvert<QIcon>() || decoration.canConvert<QPixmap>()) {
        QPixmap picture = decoration.canConvert<QPixmap>()
                              ? decoration.value<QPixmap>()
                              : decoration.value<QIcon>().pixmap(option.decorationSize.isValid()
                                                                     ? option.decorationSize
                                                                     : QSize(58, 17));
        if (!picture.isNull()) {
            const QSize shown = picture.size() / picture.devicePixelRatio();
            const QPoint at(option.rect.left() + kCellPadX,
                            option.rect.top() + (option.rect.height() - shown.height()) / 2);
            painter->drawPixmap(at, picture);
        }
        if (index.data(Qt::DisplayRole).toString().isEmpty()) {
            painter->restore();
            return;
        }
    }

    // THE VALUE. A figure is mono and right-aligned; a word is sans and left; a
    // NULL is a faint dash whatever the model printed; an edited value is in the
    // warn ink so the eye finds it across a thousand rows.
    const QString text = isNull ? QStringLiteral("—") : index.data(Qt::DisplayRole).toString();
    painter->setFont(figure ? mono(kMonoPx) : sans(kMonoPx));
    QColor ink = selected ? t.text : (figure ? t.readout : t.text);
    if (isNull) ink = t.textFaint;
    if (edited) ink = t.warn;
    painter->setPen(ink);

    const QRect box = option.rect.adjusted(kCellPadX, 0, -kCellPadX, 0);
    const int flags =
        static_cast<int>((figure ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    painter->drawText(box, flags,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, box.width()));

    // KEYBOARD FOCUS ONLY: a 1 px accent ring on the current cell while the grid
    // itself has focus, so somebody walking the table with the arrows can see
    // where they are (ui.md R21, R31). A mouse click selects the row and the
    // wash already says so.
    if (hasCurrent && !selected) {
        painter->setPen(QPen(t.accent, 1));
        painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
    }
    painter->restore();
}

// =============================================================================
// DataGrid
// =============================================================================

DataGrid::DataGrid(QWidget* parent) : QTableView(parent)
{
    setObjectName(QStringLiteral("dataGrid"));
    columns_ = new GridHeader(Qt::Horizontal, this);
    rows_    = new GridHeader(Qt::Vertical, this);
    setHorizontalHeader(columns_);
    setVerticalHeader(rows_);

    // Everything Qt would paint itself is painted by the delegate and the header,
    // so the view is stripped to the model and the scrolling.
    setFrameShape(QFrame::NoFrame);
    setShowGrid(false);
    setAlternatingRowColors(false);
    setWordWrap(false);
    setCornerButtonEnabled(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Read-only until `setEditors`: the delegate exists so the cells are painted
    // the standard's way from the first frame, and opens nothing.
    delegate_ = new GridDelegate({}, this);
    setItemDelegate(delegate_);

    // The header repaints its selection bars when the selection moves.
    connect(this, &QAbstractItemView::clicked, rows_, [this] { rows_->viewport()->update(); });
}

void DataGrid::setEditors(FieldDelegate::SpecFor specs)
{
    auto* fresh = new GridDelegate(std::move(specs), this);
    fresh->setTheme(theme_);
    setItemDelegate(fresh);
    delete delegate_;
    delegate_ = fresh;
}

void DataGrid::setRowNumbers(bool on)
{
    rows_->setVisible(on);
}

void DataGrid::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    columns_->applyTheme(mode);
    rows_->applyTheme(mode);
    if (delegate_ != nullptr) delegate_->setTheme(mode);
    viewport()->update();
}

void DataGrid::paintEvent(QPaintEvent* event)
{
    // The bands first, across the whole viewport, then Qt paints the cells over
    // them; where the columns end the band carries on, so the selected row's
    // wash and the hover reach the right edge as they do in the reference.
    if (model() != nullptr && model()->rowCount() > 0) {
        const Tokens& t = tokensOf(theme_);
        QPainter p(viewport());
        const int rows  = model()->rowCount();
        const int first = std::max(0, rowAt(0));
        for (int row = first; row < rows; ++row) {
            const int y = rowViewportPosition(row);
            if (y > viewport()->height()) break;
            const int h   = rowHeight(row);
            QColor ground = row % 2 == 0 ? t.rowEven : t.rowOdd;
            if (row == hoverRow_) ground = t.hoverRow;
            if (selectionModel() != nullptr && selectionModel()->isRowSelected(row, rootIndex()))
                ground = t.accentWash;
            p.fillRect(QRect(0, y, viewport()->width(), h), ground);
            p.fillRect(QRect(0, y + h - 1, viewport()->width(), 1), t.lineSoft);
        }
    }
    QTableView::paintEvent(event);
}

void DataGrid::mouseMoveEvent(QMouseEvent* event)
{
    const int row = indexAt(event->pos()).row();
    if (row != hoverRow_) {
        hoverRow_ = row;
        viewport()->update();
    }
    QTableView::mouseMoveEvent(event);
}

void DataGrid::leaveEvent(QEvent* event)
{
    if (hoverRow_ != -1) {
        hoverRow_ = -1;
        viewport()->update();
    }
    QTableView::leaveEvent(event);
}

} // namespace kentos::app
