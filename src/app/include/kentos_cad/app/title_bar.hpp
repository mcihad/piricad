// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the shell title bar, `design.md` §7.
//
// WHY THE WINDOW FRAME IS THE SYSTEM'S. The shell used to be frameless and drew
// its own three window buttons, so that the chrome would look the same on every
// platform. It cost more than it bought: a drawn frame has no resize edges the
// window manager knows about, no snapping, no shade, no window menu, and it
// reads as foreign next to every other window on the user's desktop. §7 now says
// the frame is the system's — the platform draws the border, the caption and the
// buttons, and this bar carries only what belongs to the application.
//
// The bar is 34 px and carries four things in one row, left to right: the ten
// menus, the document name, the command search and the user chip. `QMenuBar`
// cannot host arbitrary widgets, which is why this is a plain widget with a
// `QMenuBar` inside it rather than the menu bar itself.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <QWidget>

class QLabel;
class QMenuBar;

namespace kentos::app {

/// The command-search chip at the right end of the bar.
class SearchChip;

/// The round initials chip beside it.
class UserChip;

/// The 34 px bar across the top of the shell.
class TitleBar : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty bar. The menus are hung on it by the main window.
    explicit TitleBar(QWidget* parent = nullptr);

    /// The menu bar to hang the ten menus on. Owned by this widget.
    QMenuBar* menus() const noexcept { return menus_; }

    /// The middle reading: `<document> — KentOSCad <version>`.
    void setDocumentName(const QString& name);

    /// The initials in the round chip at the right end.
    void setUserInitials(const QString& initials);

    void applyTheme(ThemeMode mode) override;

    QSize sizeHint() const override;

signals:
    /// The search chip was clicked, or Ctrl+K was pressed somewhere in the shell.
    void searchRequested();

protected:
    /// Paints the two-stop gradient and the rule beneath it.
    void paintEvent(QPaintEvent* event) override;

private:
    QMenuBar* menus_    = nullptr;
    QLabel* title_      = nullptr;
    SearchChip* search_ = nullptr;
    UserChip* user_     = nullptr;
    ThemeMode theme_    = ThemeMode::Dark;
};

} // namespace kentos::app
