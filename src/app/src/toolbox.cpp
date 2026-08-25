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

    // BUILT HERE, not on the first theme pass. They used to be created lazily in
    // `applyTheme`, which runs after the shell has already asked for `chips()` to
    // connect to it — so the connect got a null and the program died on the next
    // line. A widget the caller can ask for must exist as soon as the object does.
    //
    // The stretch goes in first so the chips stay pinned to the foot of the
    // column whatever tools are added above them.
    chipsSpacer_ = column_->count();
    column_->addStretch(1);

    chips_ = new ColourChips(this);
    column_->addWidget(chips_, 0, Qt::AlignHCenter);
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
    column_->insertWidget(chipsSpacer_++, button, 0, Qt::AlignHCenter);
}

void ToolBox::addSeparator()
{
    auto* rule = new QWidget(this);
    rule->setObjectName(QStringLiteral("toolBoxRule"));
    rule->setFixedSize(kRuleWidth, 1);

    separators_.push_back(rule);

    // The rule carries its own air rather than relying on the layout's spacing,
    // so the 51 px pitch across a group boundary is exactly the reference's.
    column_->insertSpacing(chipsSpacer_++, kRuleAir - kGap);
    column_->insertWidget(chipsSpacer_++, rule, 0, Qt::AlignHCenter);
    column_->insertSpacing(chipsSpacer_++, kRuleAir - kGap);
}

void ToolBox::applyTheme(ThemeMode mode)
{
    theme_ = mode;

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
