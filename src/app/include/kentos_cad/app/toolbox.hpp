// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the left tool box, `design.md` §7.
//
// A 46 px column of modal DRAWING tools, five groups separated by a 24×1 rule.
// It is not a dock: the reference draws it flush against the window edge with no
// title bar, running the full height of the body from the tool bar down to the
// status bar. §6 gives every dock a 29 px title, and this has none — so it is a
// plain widget in the body layout and cannot be dragged out of place.
//
// A BODY TOO SHORT FOR THE COLUMN GETS A SECOND ONE. The reference is drawn on a
// 1033 px tall body; a laptop gives the body far less. A vertical box layout
// made the column's full height the window's MINIMUM, so the window could not be
// shorter than 1131 px and on a 900 px screen its foot — the lower tools, the
// status bar, the console — sat below the edge where nobody could see it. The
// column now lays its tools out itself and carries on in another 45 px column,
// keeping a group whole where it fits, so every tool stays on show at any
// height and the column asks the window for almost nothing (`design.md` §14.4).
//
// Every button triggers a QAction that dispatches a command; the palette holds no
// logic of its own and buys the GUI no privilege over any other client
// (CLAUDE.md Article 1).
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <functional>

#include <QColor>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

class QAction;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QResizeEvent;
class QToolButton;

namespace kentos::app {

/// The dynamic property every tool action carries: the command word it
/// dispatches. Declared here rather than twice, because the button that runs a
/// tool and the flyout that names it must never be able to disagree about which
/// command it is (CLAUDE.md 5.10).
inline constexpr const char* kToolCommandProperty = "piricad.command";

/// The two colour chips at the foot of the column: draw colour over fill colour,
/// the pair every CAD program has had at the bottom of its tool palette.
///
/// They SHOW what is in hand — the first selected object's colours, or with
/// nothing selected the active layer's, which is what a new object draws in —
/// and a press asks for a colour to paint with (`MainWindow::openColourMenu`).
/// Reached by the keyboard too: Tab lands on them, the arrows move between the
/// two and Enter or Space opens the one that has focus.
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

    /// What each chip says under the pointer: the colour it shows, whose it is,
    /// and what a press will do.
    void setDescriptions(const QString& stroke, const QString& fill);

    /// The colour chip `which` (0 stroke, 1 fill) shows; invalid for no fill.
    QColor colour(int which) const { return which == 0 ? stroke_ : fill_; }

    /// Where chip `which` (0 stroke, 1 fill) sits, in this widget's coordinates —
    /// what a menu opened from it is anchored to.
    static QRect chipRect(int which);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user clicked a chip: 0 is the stroke, 1 is the fill.
    void chipActivated(int which);

protected:
    /// Draws the two 22 px chips, 3 px apart.
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private:
    QColor stroke_;
    QColor fill_;
    QString strokeTip_;
    QString fillTip_;
    int focused_     = 0; ///< the chip Enter opens
    ThemeMode theme_ = ThemeMode::Dark;
};

/// One row of colour swatches, the first thing in the chips' menu: a click
/// paints with that colour.
///
/// DRAWN, NOT BUILT FROM BUTTONS. Nine swatches are nine colours and nothing
/// else — no caption, no role, no hierarchy — so they are not controls of the
/// component set (`widgets.hpp`) but one control of their own, painted in the
/// way the chips they belong to are. Each says its name under the pointer and
/// to a screen reader; the arrows move between them and Enter or Space picks.
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

    /// Every button in the column, top to bottom, for `KENTOS_TOOLBOX_PROBE`.
    ///
    /// The reported defect is that pressing a tool does nothing a user can see,
    /// and the only honest way to check that is to press all of them the way a
    /// hand does and read what came back. A test cannot press a button it cannot
    /// reach, and the column is built by the main window rather than declared
    /// anywhere a test could walk.
    const QVector<QToolButton*>& buttons() const noexcept { return buttons_; }

    /// Every tool the column holds, EVERY MEMBER OF EVERY FAMILY included, top to
    /// bottom. A probe that pressed only the buttons pressed only the faces: the
    /// nine members behind the creation buttons, and DÖNDÜR, ÖLÇEKLE, AYNALA and
    /// DİZİ behind TAŞI, were never pressed by anything but a hand.
    const QVector<QAction*>& tools() const noexcept { return tools_; }

    /// The width the tools need at the height in force, and the height one
    /// column of them would take.
    QSize sizeHint() const override;

    /// Almost nothing: the column wraps rather than making the window taller.
    QSize minimumSizeHint() const override;

    /// How many columns the tools take at `height` — 1 when they fit in one.
    int columnsFor(int height) const;

protected:
    /// Fills the column, draws the 1 px rule along its right edge, and rings the
    /// tool the keyboard is on when the keyboard is here.
    void paintEvent(QPaintEvent* event) override;

    /// Lays the tools out for the new height, and widens the column when they
    /// need another one (`place`).
    void resizeEvent(QResizeEvent* event) override;

    /// Puts the ring on the tool that is RUNNING when Tab arrives, so the ring
    /// and the light never say different things.
    void focusInEvent(QFocusEvent* event) override;

    /// Takes the ring away again.
    void focusOutEvent(QFocusEvent* event) override;

    /// The column is ONE tab stop and the arrow keys move inside it: ↑/↓ walk the
    /// tools, Home/End jump, Space and Enter run the one under the ring, → opens
    /// a family's card. Without this the column was mouse-only furniture — every
    /// command in it is reachable from the menus and the command line, so it was
    /// never a CLAUDE.md 5.15 breach, but it did not meet 6.9 either.
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// The next enabled button `by` steps from `start`, wrapping; -1 when the
    /// column holds no enabled button at all.
    int stepFrom(int start, int by) const;

    /// The one card, reused by every family: two cards can never be open at once
    /// and a widget per family would be a widget per family to re-theme.
    ToolFlyout* flyout_{nullptr};

    /// One place down the column, in order: a tool, or the rule between groups.
    struct Entry
    {
        QWidget* widget{nullptr}; ///< the button, or the 1 px rule
        bool rule{false};         ///< a group boundary rather than a tool
    };

    /// Where every entry goes at `height`, in `entries_` order — an empty rect
    /// for a rule that falls at the head of a column and is not drawn — and how
    /// many columns that takes. With `whole`, a group that would not fit below
    /// the last one moves on to a column of its own; without, the tools simply
    /// flow on. `place` takes whichever needs fewer columns, groups whole on a tie.
    QVector<QRect> flow(int height, bool whole, int& columns) const;
    QVector<QRect> place(int height, int& columns) const;

    QVector<Entry> entries_; ///< the column's contents, top to bottom
    int columns_{1};         ///< how many columns the height in force needs
    QVector<QToolButton*> buttons_;
    QVector<QAction*> tools_; ///< every tool, family members included; see `tools()`
    QVector<QWidget*> separators_;
    ColourChips* chips_{nullptr};

    /// The button the keyboard is on, an index into `buttons_`. Only drawn while
    /// the column has the focus; kept across a focus loss so Tab and Shift+Tab
    /// through the window return the user where they were.
    int focused_{-1};
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace kentos::app
