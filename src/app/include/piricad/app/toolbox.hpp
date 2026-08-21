// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the tool palette.
//
// A narrow, dockable, floatable icon column: left by default, draggable to the
// right edge or out of the window entirely. Every button triggers a QAction that
// dispatches a command — the palette holds no logic of its own and buys the GUI
// no privilege over any other client (CLAUDE.md Article 1).
#pragma once

#include "piricad/app/theme.hpp"

#include <QDockWidget>
#include <QList>
#include <QVector>

class QAction;
class QFrame;
class QToolButton;
class QVBoxLayout;

namespace piricad::app {

class ToolBox : public QDockWidget
{
    Q_OBJECT

public:
    explicit ToolBox(QWidget* parent = nullptr);

    /// Adds a tool button bound to `action`. Checkable actions render as a
    /// sustained mode, the way a CAD tool palette behaves.
    void addTool(QAction* action);
    void addSeparator();

    /// Re-tints every button when the theme changes.
    void applyTheme(ThemeMode mode);

private:
    QVBoxLayout* column_{nullptr};
    QVector<QToolButton*> buttons_;
    QVector<QFrame*> separators_;
};

} // namespace piricad::app
