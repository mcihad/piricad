// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the left tool box, `design.md` §7.
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

#include "kentos_cad/app/theme.hpp"

#include <functional>

#include <QRectF>
#include <QVector>
#include <QWidget>

class QAction;
class QToolButton;
class QVBoxLayout;

namespace kentos::app {

/// The dynamic property every tool action carries: the command word it
/// dispatches. Declared here rather than twice, because the button that runs a
/// tool and the flyout that names it must never be able to disagree about which
/// command it is (CLAUDE.md 5.10).
inline constexpr const char* kToolCommandProperty = "piricad.command";

/// The two colour chips at the foot of the column: draw colour over fill colour,
/// the pair every CAD program has had at the bottom of its tool palette.
class ColourChips : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the pair. Colours arrive from the theme until a document sets them.
    explicit ColourChips(QWidget* parent = nullptr);

    /// An invalid `fill` draws the lower chip hollow, which is what "no fill"
    /// has meant on a CAD tool palette since the beginning.
    void setColours(const QColor& stroke, const QColor& fill);

    void applyTheme(ThemeMode mode) override;

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

/// The card that opens beside a tool button holding a FAMILY of tools.
///
/// WHY A FAMILY AND NOT TWELVE BUTTONS. The column is 46 px wide and every CAD
/// program that ever grew past a dozen tools solved the same problem the same
/// way: the button shows the one you last used, and the rest of its family is one
/// press away. ÇİZGİ, ÇOKLUÇİZGİ, DİKDÖRTGEN and ÇOKGEN are four ways of putting
/// down straight edges — they belong under one finger, not spread down a column
/// the user has to read every time.
///
/// It is drawn rather than assembled from a QMenu because a QMenu cannot show the
/// command word beside the name, and that word is the point: the flyout is where
/// a user learns that the button they are pressing is `ÇOKLUÇİZGİ`, which is what
/// they will type tomorrow (CLAUDE.md 5.15 — the mouse teaches the keyboard).
class ToolFlyout : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the card. It is created once per column and reused by every family.
    explicit ToolFlyout(QWidget* parent = nullptr);

    /// Points the card at `family`, marking `current` as the one in force, and
    /// opens it so its nub touches `anchor` — the right edge of the button, in
    /// global coordinates.
    void reveal(const QVector<QAction*>& family, QAction* current, const QPoint& anchor);

    void applyTheme(ThemeMode mode) override;

    /// The family the card is currently showing. Public for the same reason the
    /// probe entry points are: a card nothing can read is a card nothing checks.
    const QVector<QAction*>& members() const noexcept { return family_; }

signals:
    /// The member the user settled on. Emitted once, as the card closes.
    void chosen(QAction* action);

protected:
    /// Draws the shadow, the card, its nub and one row per member.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks which row the pointer is over.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Dismisses the card when the press lands outside it. A `Qt::Popup` grabs
    /// the pointer but does not close itself, so this is what "click away"
    /// means here.
    void mousePressEvent(QMouseEvent* event) override;

    /// Settles on the row under the pointer.
    ///
    /// A release OFF the rows does nothing, and that is the whole fix: the press
    /// that opened this card is still down, and its release arrives here through
    /// the popup grab with the pointer still over the BUTTON. Closing on it made
    /// the card vanish the instant the user let go — so holding could never be
    /// followed by looking.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// Arrow keys move, Enter settles, Esc closes — the card is answerable
    /// without a mouse even though it opened under one (CLAUDE.md 5.15).
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// The card itself, inside the translucent margin the shadow is drawn in.
    QRectF cardRect() const;

    /// The row under `where`, or -1 when the point is off the rows.
    int rowAt(const QPoint& where) const;

    /// Closes the card and emits `chosen` for row `row`, if it is a real row.
    void settle(int row);

    QVector<QAction*> family_;
    int current_{0};
    int hover_{-1};
    int nub_{0}; ///< where the pointer touches the card, in local y
    ThemeMode theme_ = ThemeMode::Dark;
};

class ToolBox : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty column. Tools are added by the main window, each bound to
    /// an action that dispatches a command.
    explicit ToolBox(QWidget* parent = nullptr);

    /// Adds a tool button bound to `action`. Checkable actions render as a
    /// sustained mode, the way a CAD tool palette behaves.
    void addTool(QAction* action);

    /// Adds ONE button holding `family`, showing whichever member was last used.
    ///
    /// A short press runs the member on the face; press-and-hold, a right click,
    /// or a click on the corner mark opens the flyout. Every member keeps its own
    /// menu entry and its own command name, so nothing here is the only road to a
    /// tool (CLAUDE.md 5.15).
    void addFamily(const QVector<QAction*>& family);

    void addSeparator();

    ColourChips* chips() const noexcept { return chips_; }

    /// Re-tints every button when the theme changes.
    void applyTheme(ThemeMode mode) override;

protected:
    /// Fills the column and draws the 1 px rule along its right edge.
    void paintEvent(QPaintEvent* event) override;

private:
    /// The one card, reused by every family: two cards can never be open at once
    /// and a widget per family would be a widget per family to re-theme.
    ToolFlyout* flyout_{nullptr};

    QVBoxLayout* column_{nullptr};
    QVector<QToolButton*> buttons_;
    QVector<QWidget*> separators_;
    ColourChips* chips_{nullptr};

    /// Where the next tool goes: the index the stretch and the chips sit at, so
    /// everything added later lands ABOVE them and they stay at the foot.
    int chipsSpacer_{0};
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace kentos::app
