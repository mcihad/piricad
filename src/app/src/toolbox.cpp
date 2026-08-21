// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/toolbox.hpp"

#include <QAction>
#include <QFrame>
#include <QResizeEvent>
#include <QToolButton>

namespace piricad::app {
namespace {

/// Sized like a tool palette rather than a toolbar.
constexpr int kButton = 30;
constexpr int kIcon   = 20;
constexpr int kMargin = 4;
constexpr int kGap    = 2;

} // namespace

ToolBox::ToolBox(QWidget* parent) : QDockWidget(parent)
{
    setObjectName(QStringLiteral("toolBox"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // The title stays empty so the dock is only as wide as the buttons: a title
    // string would set the minimum width and leave a column of dead space. The
    // panel menu still needs a readable name, so the toggle action carries it.
    setWindowTitle(QString());
    toggleViewAction()->setText(tr("Araçlar"));

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("toolBoxBody"));

    flow_ = new FlowLayout(body, kMargin, kGap, kGap);

    // A minimum of one button wide, and no maximum: the palette starts as a
    // column and becomes a grid as the user drags the splitter, which is how a
    // CAD tool palette behaves. Fixing the width here is what left the wide dock
    // with one file of icons and a band of empty panel beside it.
    body->setMinimumWidth(kButton + 2 * kMargin);
    setWidget(body);
}

void ToolBox::addTool(QAction* action)
{
    auto* button = new QToolButton(widget());
    button->setDefaultAction(action);
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);

    // Every operation must also be reachable from the keyboard (piricad.md §13),
    // so the shortcut is always visible in the tooltip.
    const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
    button->setToolTip(shortcut.isEmpty()
                           ? action->toolTip()
                           : QStringLiteral("%1  (%2)").arg(action->toolTip(), shortcut));

    flow_->addWidget(button);
    buttons_.push_back(button);
}

void ToolBox::addSeparator()
{
    auto* line = new QFrame(widget());
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(1);

    // A separator spans the row: in a wrapping palette a short rule would drift
    // into the middle of a line and stop reading as a group boundary.
    flow_->addFullWidth(line);
    separators_.push_back(line);
}

void ToolBox::applyTheme(ThemeMode mode)
{
    const Palette& p = themePalette(mode);

    widget()->setStyleSheet(QStringLiteral("#toolBoxBody { background: %1; border: 1px solid %2; }")
                                .arg(p.panel.name(), p.border.name()));

    for (QFrame* line : separators_)
        line->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(p.border.name()));
}

} // namespace piricad::app
