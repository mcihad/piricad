// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the expression bar, `design.md` §9.
//
// A filter is READ as much as it is typed. `"alan_m2" > 2000 AND
// "plan_fonksiyon" = 'Konut'` has four parts of speech — a field, an operator, a
// constant, a logical word — and §9 gives each its own ink so the eye parses the
// line before the grammar does. A `QLineEdit` cannot colour its own text, so the
// bar is a one-line `QPlainTextEdit` with a highlighter: the same 30 px box as
// every other input, mono, no scrollbars, Enter applying rather than wrapping.
//
// THE GRAMMAR IS NOT HERE. The words the highlighter tints are the words the one
// parser accepts (`kentos_cad/command/parser.hpp`, CLAUDE.md 5.11); tinting is a
// reading aid and decides nothing. A word this file colours and the parser
// refuses is refused all the same, and says so in the bar's tooltip.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>

namespace kentos::app {

/// Tints the four parts of speech of a filter expression.
class ExpressionHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    /// Attaches to the bar's document.
    explicit ExpressionHighlighter(QTextDocument* document);

    /// The palette to tint with; repaints.
    void setTheme(ThemeMode mode);

protected:
    /// One line, four rules: quoted fields, quoted strings, operators, logical
    /// words. Numbers keep the readout ink.
    void highlightBlock(const QString& text) override;

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// The one-line expression input with syntax colouring.
class ExpressionEdit : public QPlainTextEdit, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an empty bar at the standard's regular height.
    explicit ExpressionEdit(QWidget* parent = nullptr);

    /// The expression as typed, one line, trimmed.
    QString expression() const;

    /// Replaces the expression without emitting `applied`.
    void setExpression(const QString& text);

    void applyTheme(ThemeMode mode) override;

signals:
    /// Enter was pressed: the expression is to be applied.
    void applied();

protected:
    /// Enter applies, Tab leaves, everything else edits. A newline never enters
    /// the text: the bar is one line by definition.
    void keyPressEvent(QKeyEvent* event) override;

    /// Paste of a multi-line clipboard lands as one line.
    void insertFromMimeData(const QMimeData* source) override;

private:
    ExpressionHighlighter* highlighter_{nullptr};
};

} // namespace kentos::app
