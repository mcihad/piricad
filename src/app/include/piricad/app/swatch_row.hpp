// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the row of colour swatches in the colour menu.
//
// `RENK`'s named colours, drawn as one row at the head of the menu the ribbon's
// colour boxes open (`MainWindow::openColourMenu`). Every swatch is one of the
// command's own words, so a colour picked here is the colour the command paints.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <QColor>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace kentos::app {

/// One row of colour swatches, the first thing in the colour menu the ribbon's
/// colour boxes open: a click paints with that colour.
///
/// DRAWN, NOT BUILT FROM BUTTONS. Nine swatches are nine colours and nothing
/// else — no caption, no role, no hierarchy — so they are not controls of the
/// component set (`widgets.hpp`) but one control of their own. Each says its
/// name under the pointer and to a screen reader; the arrows move between them
/// and Enter or Space picks.
class SwatchRow : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// One colour in the row.
    struct Swatch
    {
        QColor colour; ///< what it paints
        QString name;  ///< what it is called — the word `RENK` reads
    };

    /// Builds the row, left to right in the order given.
    explicit SwatchRow(QVector<Swatch> swatches, QWidget* parent = nullptr);

    /// Every swatch on one line, with the air a menu row has round it.
    QSize sizeHint() const override;

    /// Repaints the edges and the rings in the theme's tokens.
    void applyTheme(ThemeMode mode) override;

    /// The swatch under the keyboard, for a screen reader.
    int current() const noexcept { return focus_; }

signals:
    /// Swatch `index` was clicked, or chosen with Enter or Space.
    void picked(int index);

protected:
    /// Draws the swatches and the ring round the hovered or focused one.
    void paintEvent(QPaintEvent* event) override;
    /// Follows the pointer, for the hover ring.
    void mouseMoveEvent(QMouseEvent* event) override;
    /// A left release over a swatch picks it.
    void mouseReleaseEvent(QMouseEvent* event) override;
    /// Drops the hover ring when the pointer leaves the row.
    void leaveEvent(QEvent* event) override;
    /// Left/Right, Home/End move; Enter or Space picks; Up and Down go to the menu.
    void keyPressEvent(QKeyEvent* event) override;
    /// Names the swatch under the pointer in a tooltip.
    bool event(QEvent* event) override;

private:
    /// The swatch at `at`, or -1 between and around them.
    int swatchAt(QPointF at) const;
    static QRectF swatchRect(int index);
    void moveTo(int index);

    QVector<Swatch> swatches_;
    int hover_       = -1;
    int focus_       = 0;
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace kentos::app
