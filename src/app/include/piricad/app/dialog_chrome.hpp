// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the chrome every dialog in `design.md` wears.
//
// Screens 2, 3 and 4 are the same window with different contents: a 1 px outline,
// a 38 px gradient title bar carrying an icon, a name and a dim subtitle, a body,
// and a footer of buttons. Writing that three times is how three windows end up
// three heights, and the shell already learned that lesson once — the dialogs
// used to carry their own stylesheets and matched neither each other nor the
// shell (see `tokens.hpp`).
//
// FRAMELESS, for the reason `title_bar.hpp` gives: a native dialog frame is a
// different height and a different button order on every platform, and §12 asks
// for one appearance everywhere.
#pragma once

#include "piricad/app/icons.hpp"
#include "piricad/app/theme.hpp"

#include <QAbstractButton>
#include <QDialog>
#include <QString>
#include <QVector>

class QHBoxLayout;
class QVBoxLayout;

namespace piricad::app {

/// A dialog with the §7–§10 chrome: outline, title bar, body, footer.
class DialogFrame : public QDialog, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

    /// The title bar hands the drag back to the frame; it is part of it.
    friend class DialogTitleBar;

public:
    /// Builds an empty frame: title bar, nothing between, and a footer holding
    /// one stretch. The caller sets the heading and the body.
    explicit DialogFrame(QWidget* parent = nullptr);

    /// The icon, the name, and the dim qualifier after it — `Katman Özellikleri`
    /// then `— Kadastro Parselleri`.
    void setHeading(Glyph glyph, const QString& title, const QString& subtitle = QString());

    /// Whether the title bar carries a help mark before the close mark.
    void setHelpVisible(bool on);

    /// Puts `body` between the title bar and the footer. Takes ownership.
    void setBody(QWidget* body);

    /// The footer's layout, for the caller to add buttons to. Left-hand buttons
    /// go before the stretch this already holds, right-hand ones after it.
    QHBoxLayout* footer() const noexcept { return footer_; }

    /// The footer is 48 px in the style designer and 52 px in the settings
    /// window; both are measured off the reference rather than chosen.
    void setFooterHeight(int px);

    void applyTheme(ThemeMode mode) override;

signals:
    /// The help mark in the title bar was pressed.
    void helpRequested();

protected:
    /// Draws the outline and the title bar's two-stop gradient.
    void paintEvent(QPaintEvent* event) override;

    /// Drags the dialog by its title bar.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QVBoxLayout* stack_  = nullptr;
    QHBoxLayout* footer_ = nullptr;
    QWidget* footerBar_  = nullptr;
    QWidget* titleBar_   = nullptr;
    QWidget* body_       = nullptr;
    ThemeMode theme_     = ThemeMode::Dark;
};

/// The left-hand section list of the style designer and the settings window.
///
/// One column of icon-and-label rows, 35 px each. The chosen row carries the
/// §2 selected pattern — accent wash plus a 2 px accent edge — and may carry a
/// warn dot at its right end, which is how the settings window says "you changed
/// something in here and have not applied it yet".
class SectionList : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    explicit SectionList(QWidget* parent = nullptr);

    /// Appends a row and returns its index.
    int addSection(Glyph glyph, const QString& label);

    /// Marks or clears the warn dot on one row.
    void setMarked(int index, bool marked);

    void setCurrent(int index);

    int current() const noexcept { return current_; }

    /// Hides every row whose label does not contain `needle`, Turkish-folded.
    /// An empty needle shows them all.
    void setFilter(const QString& needle);

    void applyTheme(ThemeMode mode) override;

    QSize sizeHint() const override;

signals:
    /// A different row was chosen.
    void currentChanged(int index);

protected:
    /// Draws every visible row.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks and answers the pointer.
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Section
    {
        Glyph glyph = Glyph::Settings;
        QString label;
        std::string key; ///< the label, Turkish-folded, for the filter
        bool marked = false;
        bool shown  = true;
    };

    int rowAt(int y) const;

    QVector<Section> sections_;
    int current_     = 0;
    int hot_         = -1;
    ThemeMode theme_ = ThemeMode::Dark;
};

/// The pill switch `design.md` §10 uses for a yes/no setting: 38 × 20, accent
/// when on, with the knob at the end it is on.
///
/// NOT A `QCheckBox`. §13 forbids stating anything with colour alone, and this
/// states it twice — the fill AND the knob's side — which a tick box cannot do.
class ToggleSwitch : public QAbstractButton, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds an off switch, checkable and keyboard-reachable.
    explicit ToggleSwitch(QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

    /// Always 38 x 20; the size is part of the specification, not of the text.
    QSize sizeHint() const override;

protected:
    /// Draws the track and the knob.
    void paintEvent(QPaintEvent* event) override;

private:
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace piricad::app
