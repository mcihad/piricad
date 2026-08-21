// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/flow_layout.hpp"

#include <QWidget>

namespace piricad::app {

FlowLayout::FlowLayout(QWidget* parent, int margin, int hspacing, int vspacing)
    : QLayout(parent), hspace_(hspacing), vspace_(vspacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* item = takeAt(0))
        delete item;
}

void FlowLayout::addItem(QLayoutItem* item)
{
    items_.append(item);
    full_width_.append(false);
}

void FlowLayout::addFullWidth(QWidget* widget)
{
    addWidget(widget); // routes through addItem, appending `false`
    if (!full_width_.isEmpty()) full_width_.last() = true;
}

int FlowLayout::count() const
{
    return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= items_.size()) return nullptr;
    full_width_.removeAt(index);
    return items_.takeAt(index);
}

int FlowLayout::heightForWidth(int width) const
{
    return reflow(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    reflow(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    // One item wide is the true minimum: the palette must survive being dragged
    // down to a single column, which is the shape it had before it could wrap.
    QSize size;
    for (const QLayoutItem* item : items_)
        size = size.expandedTo(item->minimumSize());

    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::reflow(const QRect& rect, bool test) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());

    int x          = area.x();
    int y          = area.y();
    int row_height = 0;

    for (int i = 0; i < items_.size(); ++i) {
        QLayoutItem* item = items_.at(i);
        const bool spans  = full_width_.at(i);
        const QSize hint  = item->sizeHint();
        const int w       = spans ? area.width() : hint.width();

        // A full-width item always starts a row, and the item after it starts a
        // row too — otherwise a separator would sit beside the group it separates.
        const bool wrap = x != area.x() && (spans || x + w > area.right() + 1);
        if (wrap) {
            x          = area.x();
            y          = y + row_height + vspace_;
            row_height = 0;
        }

        if (!test) item->setGeometry(QRect(QPoint(x, y), QSize(w, hint.height())));

        x          = spans ? area.right() + 1 : x + w + hspace_;
        row_height = qMax(row_height, hint.height());
    }

    return y + row_height - rect.y() + m.bottom();
}

} // namespace piricad::app
