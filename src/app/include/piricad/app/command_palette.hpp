// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the command search, `design.md` §7 (`Ctrl+K`).
//
// GENERATED FROM `Registry`, NEVER A LIST. CLAUDE.md 5.10 forbids a second
// command list, and a search palette is exactly the shape that temptation takes:
// a hand-kept array of "the commands worth searching". This one enumerates
// `Registry::all()` and matches on the same Turkish folding the parser uses, so a
// command is searchable the moment it is declared and never a day later.
#pragma once

#include "piricad/app/theme.hpp"

#include <QString>
#include <QWidget>
#include <vector>

class QLineEdit;
class QListWidget;

namespace piricad::command {
/// The one command list; the palette is generated from it.
class Registry;
} // namespace piricad::command

namespace piricad::app {

/// A centred overlay listing every command, filtered as the user types.
class CommandPalette : public QWidget
{
    Q_OBJECT

public:
    /// Builds the palette over a registry, which outlives it.
    CommandPalette(const command::Registry& registry, QWidget* parent);

    /// Clears the query and shows the palette centred over its parent.
    void reveal();

    void applyTheme(ThemeMode mode);

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
        std::string key; ///< every name folded, for matching
    };

    void refilter();
    void accept();

    const command::Registry& registry_;
    std::vector<Row> rows_;
    QLineEdit* query_  = nullptr;
    QListWidget* list_ = nullptr;
    ThemeMode theme_   = ThemeMode::Dark;
};

} // namespace piricad::app
