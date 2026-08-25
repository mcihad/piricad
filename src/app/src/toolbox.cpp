// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/toolbox.hpp"

#include "piricad/app/tokens.hpp"

#include <QAction>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace piricad::app {
namespace {

// Measured off the reference, not guessed: the active tool renders as a 32×32
// accent chip at x 7..38 inside a 45 px column, on a 34 px pitch — so a 2 px gap
// — and a group rule is a 24×1 line with 7 px of air either side.
constexpr int kColumn    = 46; ///< 45 px of content plus the 1 px right rule
constexpr int kButton    = 32;
constexpr int kIcon      = 20;
constexpr int kGap       = 2;
constexpr int kTopPad    = 6;
constexpr int kRuleWidth = 24;
constexpr int kRuleAir   = 7;

constexpr int kChip     = 22;
constexpr int kChipGap  = 3;
constexpr int kChipsPad = 10;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

} // namespace

// =============================================================================
// ColourChips
// =============================================================================

ColourChips::ColourChips(QWidget* parent) : QWidget(parent)
{
    setFixedSize(kChip, kChip * 2 + kChipGap);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Çizim ve dolgu rengi"));
}

void ColourChips::setColours(const QColor& stroke, const QColor& fill)
{
    stroke_ = stroke;
    fill_   = fill;
    update();
}

void ColourChips::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    if (!stroke_.isValid()) stroke_ = tokensOf(mode).accent;
    update();
}

void ColourChips::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto chip = [&](int top, const QColor& fill, const QColor& edge) {
        const QRectF box(0.5, top + 0.5, kChip - 1, kChip - 1);
        p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
        p.setPen(QPen(edge, 1.0));
        p.drawRoundedRect(box, 3.0, 3.0);
    };

    chip(0, stroke_.isValid() ? stroke_ : t.accent, t.accentHi);
    chip(kChip + kChipGap, fill_, t.border);
}

void ColourChips::mousePressEvent(QMouseEvent* event)
{
    emit chipActivated(event->position().y() < kChip ? 0 : 1);
}

// =============================================================================
// ToolBox
// =============================================================================

ToolBox::ToolBox(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("toolBox"));
    setFixedWidth(kColumn);

    column_ = new QVBoxLayout(this);
    column_->setContentsMargins(0, kTopPad, 1, kChipsPad);
    column_->setSpacing(kGap);
    column_->setAlignment(Qt::AlignHCenter);
}

void ToolBox::addTool(QAction* action)
{
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("toolBoxButton"));
    button->setDefaultAction(action);
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);

    buttons_.push_back(button);
    column_->addWidget(button, 0, Qt::AlignHCenter);
}

void ToolBox::addSeparator()
{
    auto* rule = new QWidget(this);
    rule->setObjectName(QStringLiteral("toolBoxRule"));
    rule->setFixedSize(kRuleWidth, 1);

    separators_.push_back(rule);

    // The rule carries its own air rather than relying on the layout's spacing,
    // so the 51 px pitch across a group boundary is exactly the reference's.
    column_->addSpacing(kRuleAir - kGap);
    column_->addWidget(rule, 0, Qt::AlignHCenter);
    column_->addSpacing(kRuleAir - kGap);
}

void ToolBox::applyTheme(ThemeMode mode)
{
    theme_ = mode;

    // The chips are built on the first theme pass so they sit under the stretch,
    // which is what pins them to the foot of the column whatever else is in it.
    if (!chips_) {
        column_->addStretch(1);
        chips_ = new ColourChips(this);
        column_->addWidget(chips_, 0, Qt::AlignHCenter);
    }
    chips_->applyTheme(mode);

    const Tokens& t = tokensOf(mode);
    for (QWidget* rule : separators_) {
        QPalette palette = rule->palette();
        palette.setColor(QPalette::Window, t.separator);
        rule->setAutoFillBackground(true);
        rule->setPalette(palette);
    }
    update();
}

void ToolBox::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.fillRect(rect(), t.bgPanel);
    p.fillRect(QRect(width() - 1, 0, 1, height()), t.lineHard);
}

} // namespace piricad::app
