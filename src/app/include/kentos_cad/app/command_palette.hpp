// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the command search, `design.md` §7 (`Ctrl+K`).
//
// GENERATED FROM `Registry`, NEVER A LIST. CLAUDE.md 5.10 forbids a second
// command list, and a search palette is exactly the shape that temptation takes:
// a hand-kept array of "the commands worth searching". This one enumerates
// `Registry::all()` and matches on the same Turkish folding the parser uses, so a
// command is searchable the moment it is declared and never a day later.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <QString>
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QStyledItemDelegate;

namespace kentos::command {
/// The one command list; the palette is generated from it.
class Registry;
} // namespace kentos::command

namespace kentos::app {

/// A centred overlay listing every command, filtered as the user types.
class CommandPalette : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the palette over a registry, which outlives it.
    CommandPalette(const command::Registry& registry, QWidget* parent);

    /// Clears the query and shows the palette centred over its parent.
    ///
    /// `focus_on` is a command name to select instead of the first row — what
    /// `YARDIM komut=ÇİZGİ` opens the page on. Empty selects the first.
    void reveal(const QString& focus_on = {});

    void applyTheme(ThemeMode mode) override;

    /// What the page is showing, for `KENTOS_HELP_PROBE`: how many command rows,
    /// how many group headings, the selected command's name, and whether the
    /// list can be scrolled rather than running off the bottom of the screen.
    ///
    /// The two halves of this are exactly the two complaints the message-box
    /// version earned: a window that grew past the screen, and no way to scroll
    /// it. `/tests` links no Qt, so nothing but a probe can see either.
    struct Shown
    {
        int commands{0};   ///< rows that are a command
        int headings{0};   ///< category headings between them
        int scroll_max{0}; ///< the list's scrollbar range; 0 means nothing to scroll
        int height{0};     ///< the page's own height in pixels
        QString selected;  ///< the row the cursor is on
        QString detail;    ///< the right-hand pane's title
    };

    /// What the page is showing right now.
    Shown shown() const;

signals:
    /// The chosen command's primary name, ready for the command line.
    void chosen(const QString& name);

protected:
    /// Esc closes the palette without running anything.
    void keyPressEvent(QKeyEvent* event) override;

    /// Draws the rounded panel behind the field and the list.
    void paintEvent(QPaintEvent* event) override;

    /// Steers the list from the query field, so the caret never has to leave it.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row
    {
        QString name;    ///< the Turkish primary name, as typed at the prompt
        QString summary; ///< the one-line description from the spec
        QString id;      ///< the stable id, shown mono at the right
        QString shorts;  ///< the abbreviations, e.g. `Ç · L` — what the prompt takes
        QString group;   ///< the category's Turkish name, the heading it sits under
        QString names;   ///< every accepted spelling, for the detail pane
        QString detail;  ///< the parameter table, built once from the spec
        int category{0}; ///< `command::Category`, for ordering the groups
        std::string key; ///< every name folded, for matching
    };

    /// Fills the right-hand pane from the row the cursor is on.
    void showDetail();

    void refilter();
    void accept();

    /// The row the cursor should land on when the list is refilled: the first
    /// command, never a heading.
    void selectFirstCommand();

    const command::Registry& registry_;
    std::vector<Row> rows_;
    QLineEdit* query_  = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* footer_    = nullptr;

    // ---- the right-hand pane: what the selected command takes ---------------
    QLabel* detailName_ = nullptr;
    QLabel* detailMeta_ = nullptr;
    QLabel* detailBody_ = nullptr;
    ThemeMode theme_    = ThemeMode::Dark;

    /// Draws the headings and the rows; see `PaletteRow` in the source.
    QStyledItemDelegate* rows_delegate_ = nullptr;
};

} // namespace kentos::app
