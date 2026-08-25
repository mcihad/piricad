// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the shell title bar, `design.md` §7.
//
// WHY THE WINDOW IS FRAMELESS. The specification's first requirement is that the
// application look the same on every platform, and a native title bar is the one
// piece of a window that never does: it is a different height, a different font
// and a different button order on Windows, macOS and each Linux desktop. §7 says
// the window buttons are drawn "aynı ölçüde" off macOS, which is only possible
// if the bar itself belongs to the application. So it does.
//
// The bar is 34 px and carries five things in one row, left to right: the window
// buttons, the ten menus, the document name, the command search and the user
// chip. `QMenuBar` cannot host arbitrary widgets, which is the other reason this
// is a plain widget with a `QMenuBar` inside it rather than the menu bar itself.
#pragma once

#include "piricad/app/theme.hpp"

#include <QWidget>

class QLabel;
class QMenuBar;

namespace piricad::app {

/// The command-search chip at the right end of the bar.
class SearchChip;

/// The round initials chip beside it.
class UserChip;

/// The three window buttons at the left end.
class WindowButtons;

/// The 34 px bar across the top of the shell.
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    /// Builds an empty bar. The menus are hung on it by the main window.
    explicit TitleBar(QWidget* parent = nullptr);

    /// The menu bar to hang the ten menus on. Owned by this widget.
    QMenuBar* menus() const noexcept { return menus_; }

    /// The middle reading: `<document> — PiriCAD <version>`.
    void setDocumentName(const QString& name);

    /// The initials in the round chip at the right end.
    void setUserInitials(const QString& initials);

    void applyTheme(ThemeMode mode);

    QSize sizeHint() const override;

signals:
    /// The search chip was clicked, or Ctrl+K was pressed somewhere in the shell.
    void searchRequested();

protected:
    /// Paints the two-stop gradient and the rule beneath it.
    void paintEvent(QPaintEvent* event) override;

    /// Drags the window. Qt's own system move is preferred so the window manager
    /// keeps charge of snapping and multi-monitor edges.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Maximises and restores, the way a native title bar does.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QMenuBar* menus_        = nullptr;
    QLabel* title_          = nullptr;
    SearchChip* search_     = nullptr;
    UserChip* user_         = nullptr;
    WindowButtons* buttons_ = nullptr;
    ThemeMode theme_        = ThemeMode::Dark;
    QPoint dragFrom_;
    bool dragging_ = false;
};

} // namespace piricad::app
