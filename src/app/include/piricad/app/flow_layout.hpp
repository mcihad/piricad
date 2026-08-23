// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: a reflowing layout for icon palettes.
//
// Qt ships no QFlowLayout — the wrapping layout is documented as an example, not
// as API — so this is the one place the project writes what a library would
// otherwise provide (CLAUDE.md 5.16 records the exception rather than pretending
// a library exists). It is the standard heightForWidth reflow: items are packed
// left to right and wrap to the next row when the row is full, so a tool palette
// docked wide shows a grid and docked narrow shows a column, without the caller
// choosing between the two.
#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>
#include <QStyle>

namespace piricad::app {

class FlowLayout : public QLayout
{
public:
    /// Builds the layout on `parent`. Spacing is in device-independent pixels;
    /// the defaults are what a tool palette wants.
    explicit FlowLayout(QWidget* parent, int margin = 4, int hspacing = 2, int vspacing = 2);
    ~FlowLayout() override;

    /// Takes ownership of an item. Called by Qt through `addWidget`, and by
    /// `addFullWidth` below.
    void addItem(QLayoutItem* item) override;

    /// Adds `widget` on a row of its own, spanning the full width — the shape a
    /// group separator needs in a wrapping palette.
    void addFullWidth(QWidget* widget);

    int horizontalSpacing() const noexcept { return hspace_; }

    int verticalSpacing() const noexcept { return vspace_; }

    Qt::Orientations expandingDirections() const override { return {}; }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

private:
    /// Lays the items out inside `rect`, or measures only when `test` is set.
    int reflow(const QRect& rect, bool test) const;

    QList<QLayoutItem*> items_;
    QList<bool> full_width_; ///< parallel to items_: forces its own row
    int hspace_;
    int vspace_;
};

} // namespace piricad::app
