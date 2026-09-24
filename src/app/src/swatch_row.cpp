// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/swatch_row.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <algorithm>
#include <utility>

#include <QAccessible>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace kentos::app {
namespace {

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

} // namespace

// =============================================================================
// SwatchRow
// =============================================================================

namespace {
constexpr int kSwatch    = 20; ///< one swatch, square
constexpr int kSwatchGap = 5;
constexpr int kSwatchPad = 10; ///< air round the row, matching a menu row's inset
} // namespace

SwatchRow::SwatchRow(QVector<Swatch> swatches, QWidget* parent)
    : QWidget(parent), swatches_(std::move(swatches))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(tr("Renkler"));
    setAccessibleDescription(tr("Sol/sağ ok renkler arasında gezer; Enter ya da Boşluk seçer"));
}

QSize SwatchRow::sizeHint() const
{
    const int n = static_cast<int>(swatches_.size());
    return {kSwatchPad * 2 + n * kSwatch + std::max(0, n - 1) * kSwatchGap,
            kSwatchPad + kSwatch + kSwatchPad / 2};
}

void SwatchRow::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QRectF SwatchRow::swatchRect(int index)
{
    return {static_cast<qreal>(kSwatchPad + index * (kSwatch + kSwatchGap)),
            static_cast<qreal>(kSwatchPad) / 2.0, static_cast<qreal>(kSwatch),
            static_cast<qreal>(kSwatch)};
}

int SwatchRow::swatchAt(QPointF at) const
{
    for (int i = 0; i < swatches_.size(); ++i)
        if (swatchRect(i).contains(at)) return i;
    return -1;
}

void SwatchRow::moveTo(int index)
{
    if (swatches_.isEmpty()) return;
    focus_ = std::clamp(index, 0, static_cast<int>(swatches_.size()) - 1);
    update();

    // A SCREEN READER HEARS THE COLOUR, not "Renkler" nine times over.
    setAccessibleName(swatches_[focus_].name);
    QAccessibleEvent moved(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&moved);
}

void SwatchRow::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < swatches_.size(); ++i) {
        const QRectF box = swatchRect(i).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(swatches_[i].colour);
        p.setPen(QPen(t.border, 1.0));
        p.drawRoundedRect(box, 3.0, 3.0);

        // Under the pointer, and where the keyboard is: a ring OUTSIDE the
        // swatch, so the colour itself is never covered by the mark.
        const bool keyed = hasFocus() && i == focus_;
        if (i == hover_ || keyed) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(keyed ? t.accentEdge : t.text, 1.5));
            p.drawRoundedRect(box.adjusted(-2.5, -2.5, 2.5, 2.5), 4.5, 4.5);
        }
    }
}

void SwatchRow::mouseMoveEvent(QMouseEvent* event)
{
    const int at = swatchAt(event->position());
    if (at != hover_) {
        hover_ = at;
        update();
    }
}

void SwatchRow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (const int at = swatchAt(event->position()); at >= 0) emit picked(at);
}

void SwatchRow::leaveEvent(QEvent* /*event*/)
{
    hover_ = -1;
    update();
}

void SwatchRow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left: moveTo(focus_ - 1); return;
    case Qt::Key_Right: moveTo(focus_ + 1); return;
    case Qt::Key_Home: moveTo(0); return;
    case Qt::Key_End: moveTo(static_cast<int>(swatches_.size()) - 1); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (!swatches_.isEmpty()) emit picked(focus_);
        return;
    default:
        // Up and Down belong to the menu the row sits in.
        event->ignore();
    }
}

bool SwatchRow::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        if (const int at = swatchAt(help->pos()); at >= 0) {
            QToolTip::showText(help->globalPos(), swatches_[at].name, this,
                               swatchRect(at).toAlignedRect());
            return true;
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

} // namespace kentos::app
