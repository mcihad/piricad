// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the small chrome pieces the shell paints for itself.
//
// WHY THESE ARE PAINTED AND NOT ASSEMBLED FROM WIDGETS. Each of them is a strip
// of text and rules at exact pixel offsets that `design.md` §4 fixes to the
// pixel. A `QHBoxLayout` of `QLabel`s reaches those offsets only by accident: the
// label adds its own margin, the style adds its own frame, and the result drifts
// by a pixel or two on every platform — which is the one thing the specification
// forbids. Painted, the offsets are the numbers in this file.
#pragma once

#include "piricad/app/theme.hpp"

#include <QStatusBar>
#include <QString>
#include <QVector>
#include <QWidget>

namespace piricad::app {

/// The two read-only readings at the right end of the tool bar: the plot scale
/// and the coordinate reference system (`design.md` §7).
class ReadoutStrip : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds the strip with both cells showing a dash until asked otherwise.
    explicit ReadoutStrip(QWidget* parent = nullptr);

    /// The PLOT scale, already formatted: `1 : 1 000`.
    void setScale(const QString& text);

    /// The coordinate system, already formatted: `EPSG:5254 · ITRF96 / TM30`.
    void setCrs(const QString& text);

    void applyTheme(ThemeMode mode) override;

    /// As wide as the two cells actually need, so the strip sits flush right.
    QSize sizeHint() const override;

protected:
    /// Draws both cells and the 1x22 rule between them.
    void paintEvent(QPaintEvent* event) override;

private:
    struct Cell
    {
        QString caption; ///< 9.5 px, letter-spaced, dim
        QString value;   ///< 12 px mono, bright
    };

    int cellWidth(const Cell& cell) const;

    Cell scale_;
    Cell crs_;
    ThemeMode theme_ = ThemeMode::Dark;
};

/// The 30 px document tab strip above the canvas (`design.md` §7).
class DocumentTabs : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds an empty strip. `setDocuments` fills it.
    explicit DocumentTabs(QWidget* parent = nullptr);

    /// Replaces the whole strip. `active` indexes `names`.
    void setDocuments(const QStringList& names, int active);

    void applyTheme(ThemeMode mode) override;

    QSize sizeHint() const override;

signals:
    /// A tab was clicked. The shell decides what switching means.
    void activated(int index);

    /// The close mark on the active tab was clicked.
    void closeRequested(int index);

    /// The split-view button at the right end was clicked.
    void splitRequested();

    /// The expand button beside it was clicked.
    void expandRequested();

protected:
    /// Draws every tab, its rule, and the two buttons at the right end.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks which tab, close mark or button the pointer is over.
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Tab
    {
        QString name;
        int left  = 0;
        int width = 0;
    };

    void relayout();
    int tabAt(QPoint at) const;

    QVector<Tab> tabs_;
    int active_      = 0;
    int hot_         = -1;
    int hotClose_    = -1;
    int hotButton_   = -1; ///< 0 = split, 1 = expand
    ThemeMode theme_ = ThemeMode::Dark;
};

/// The 26 px status strip across the foot of the window (`design.md` §7).
///
/// Left: the cursor's own coordinate, in mono, because a coordinate is read
/// digit by digit. Middle: the drawing-aid toggles, each a chip that dispatches
/// its command when clicked — `IZGARA` is the `IZGARA` command and nothing else,
/// so the mouse buys no privilege the keyboard lacks (Article 1.2). Right: the
/// database connection and the frame budget.
/// It IS the status bar rather than a widget inside one. `QStatusBar` lays its
/// items out in `reformat()`, which re-applies the style's own margins on every
/// change and cannot be talked out of them — a child strip ends up three pixels
/// low and clipped at the bottom of the window. With no items there is no
/// reformat, and `setStatusBar()` puts this in the one slot that already spans
/// the full width beneath the docks.
class StatusStrip : public QStatusBar, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds an empty strip. The shell adds its toggles and its readings.
    explicit StatusStrip(QWidget* parent = nullptr);

    /// Declares one toggle chip. `id` is handed back when it is clicked.
    void addToggle(const QString& label, const QString& id);
    void setToggle(const QString& id, bool on);

    void setCoordinate(const QString& text);

    /// The last thing a command SAID, printed between the aid chips and the
    /// right-hand cells.
    ///
    /// Every command reports through `ctx.echo`, and until this existed the only
    /// place that landed was the `Geçmiş` tab of a dock the user has usually
    /// tabbed away from. So ÖLÇ measured, wrote its distance where nobody was
    /// looking, and read as a tool that does nothing — the same for ALANÖLÇ,
    /// KOORDİNAT and every error message a command produces.
    void setMessage(const QString& text);
    void setConnection(const QString& text, bool connected);
    void setPerformance(const QString& text);

    void applyTheme(ThemeMode mode) override;

    QSize sizeHint() const override;

signals:
    /// A chip was clicked; `id` is the setting it stands for. The shell turns
    /// this into an `AYAR` call, so the mouse and the keyboard reach the store by
    /// exactly the same road (Article 1.2).
    void toggled(const QString& id);

    /// A chip was right-clicked: the user wants to CONFIGURE the aid rather than
    /// switch it. `OSNAP` is on/off as a chip and thirteen separate modes
    /// underneath, and a strip with thirteen chips on it would be a strip nobody
    /// can read.
    void configureRequested(const QString& id);

protected:
    /// Draws the coordinate cell, the chips and the two right-hand cells.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks which chip the pointer is over.
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString message_;

    struct Chip
    {
        QString label;
        QString id;
        bool on   = false;
        int left  = 0;
        int width = 0;
    };

    void relayout();
    int cellWidth(const QString& text, bool withIcon) const;

    QVector<Chip> chips_;
    QString coordinate_;
    QString connection_;
    QString performance_;
    bool connected_  = false;
    int hot_         = -1;
    int coordWidth_  = 0;
    ThemeMode theme_ = ThemeMode::Dark;
};

/// The 29 px header every dock panel wears (`design.md` §6).
///
/// ONE ROW, NOT TWO. Qt gives a tabified dock a tab bar AND a title bar, which is
/// 58 px where the reference has 29. So the tabs and the panel buttons are drawn
/// here together and the dock's own title bar is replaced by this widget.
class PanelHeader : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// `buttons` names which of the four §6 marks this header carries, in the
    /// order the reference draws them: grip, collapse, float, close.
    enum Button { Grip = 1 << 0, Collapse = 1 << 1, Float = 1 << 2, Close = 1 << 3 };

    explicit PanelHeader(QWidget* parent = nullptr);

    /// Adds one tab. `glyph` is drawn 14 px before the label.
    void addTab(const QString& label, int glyph);

    void setButtons(int mask);
    void setCurrent(int index);

    int current() const noexcept { return current_; }

    void applyTheme(ThemeMode mode) override;

    QSize sizeHint() const override;

    /// `QDockWidgetLayout` measures its title area with `minimumSizeHint()`, not
    /// `sizeHint()` and not `setFixedHeight()`. Left to the default it returns the
    /// font's height — 19 px against the 29 the specification asks for — so the
    /// height is stated here, where the dock actually reads it.
    QSize minimumSizeHint() const override;

signals:
    /// A different tab was chosen. The shell switches its stack.
    void tabChanged(int index);

    /// One of the four §6 marks was pressed; `button` is a `Button` value.
    void buttonPressed(int button);

protected:
    /// Draws the tabs, the 2 px accent edge on the active one, and the marks.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks which tab or mark the pointer is over.
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Tab
    {
        QString label;
        int glyph = 0;
        int left  = 0;
        int width = 0;
    };

    void relayout();
    QVector<int> buttonList() const;

    QVector<Tab> tabs_;
    int buttons_     = Grip | Collapse | Float;
    int current_     = 0;
    int hotTab_      = -1;
    int hotButton_   = -1;
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace piricad::app
