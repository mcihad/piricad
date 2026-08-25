// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the left tool box, `design.md` §7.
//
// A 46 px column of modal DRAWING tools, five groups separated by a 24×1 rule.
// It is not a dock: the reference draws it flush against the window edge with no
// title bar, running the full height of the body from the tool bar down to the
// status bar. §6 gives every dock a 29 px title, and this has none — so it is a
// plain widget in the body layout and cannot be dragged out of place.
//
// Every button triggers a QAction that dispatches a command; the palette holds no
// logic of its own and buys the GUI no privilege over any other client
// (CLAUDE.md Article 1).
#pragma once

#include "piricad/app/theme.hpp"

#include <QVector>
#include <QWidget>

class QAction;
class QToolButton;
class QVBoxLayout;

namespace piricad::app {

/// The two colour chips at the foot of the column: draw colour over fill colour,
/// the pair every CAD program has had at the bottom of its tool palette.
class ColourChips : public QWidget
{
    Q_OBJECT

public:
    /// Builds the pair. Colours arrive from the theme until a document sets them.
    explicit ColourChips(QWidget* parent = nullptr);

    /// An invalid `fill` draws the lower chip hollow, which is what "no fill"
    /// has meant on a CAD tool palette since the beginning.
    void setColours(const QColor& stroke, const QColor& fill);

    void applyTheme(ThemeMode mode);

signals:
    /// The user clicked a chip: 0 is the stroke, 1 is the fill.
    void chipActivated(int which);

protected:
    /// Draws the two 22 px chips, 3 px apart.
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QColor stroke_;
    QColor fill_;
    ThemeMode theme_ = ThemeMode::Dark;
};

class ToolBox : public QWidget
{
    Q_OBJECT

public:
    /// Builds an empty column. Tools are added by the main window, each bound to
    /// an action that dispatches a command.
    explicit ToolBox(QWidget* parent = nullptr);

    /// Adds a tool button bound to `action`. Checkable actions render as a
    /// sustained mode, the way a CAD tool palette behaves.
    void addTool(QAction* action);
    void addSeparator();

    ColourChips* chips() const noexcept { return chips_; }

    /// Re-tints every button when the theme changes.
    void applyTheme(ThemeMode mode);

protected:
    /// Fills the column and draws the 1 px rule along its right edge.
    void paintEvent(QPaintEvent* event) override;

private:
    QVBoxLayout* column_{nullptr};
    QVector<QToolButton*> buttons_;
    QVector<QWidget*> separators_;
    ColourChips* chips_{nullptr};
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace piricad::app
