// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/expression_edit.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <QKeyEvent>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextCharFormat>

namespace kentos::app {
namespace {

constexpr int kBarHeight = 30; ///< the standard's regular control

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// The logical words — exactly the ones `parser.hpp` reads, which are ASCII by
/// its own construction (AND, OR, NOT, IS, NULL). Tinting a word the parser
/// would refuse would be the bar promising what the grammar does not keep.
const QRegularExpression& logicalWords()
{
    static const QRegularExpression re(QStringLiteral("\\b(AND|OR|NOT|IS|NULL)\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression& operators()
{
    static const QRegularExpression re(QStringLiteral("(<>|<=|>=|!=|[=<>+\\-*/%()])"));
    return re;
}

const QRegularExpression& quotedField()
{
    static const QRegularExpression re(QStringLiteral("\"[^\"]*\""));
    return re;
}

const QRegularExpression& quotedString()
{
    static const QRegularExpression re(QStringLiteral("'[^']*'"));
    return re;
}

} // namespace

// =============================================================================
// ExpressionHighlighter
// =============================================================================

ExpressionHighlighter::ExpressionHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
{}

void ExpressionHighlighter::setTheme(ThemeMode mode)
{
    theme_ = mode;
    rehighlight();
}

void ExpressionHighlighter::highlightBlock(const QString& text)
{
    const Tokens& t = tokensOf(theme_);

    const auto tint = [this, &text](const QRegularExpression& re, const QColor& ink,
                                    bool bold = false) {
        QTextCharFormat format;
        format.setForeground(ink);
        if (bold) format.setFontWeight(QFont::DemiBold);
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()),
                      format);
        }
    };

    // Operators and words first, then the quoted spans over them, so a `>` or an
    // AND inside a constant keeps the constant's ink.
    tint(operators(), t.syntaxOperator);
    tint(logicalWords(), t.syntaxLogical, true);
    tint(quotedField(), t.syntaxField);
    tint(quotedString(), t.syntaxString);
}

// =============================================================================
// ExpressionEdit
// =============================================================================

ExpressionEdit::ExpressionEdit(QWidget* parent) : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("expressionEdit"));
    setFixedHeight(kBarHeight);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(true);
    setUndoRedoEnabled(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("Süzme ifadesi"));

    QFont face(QStringLiteral("IBM Plex Mono"));
    face.setPixelSize(12);
    setFont(face);
    document()->setDocumentMargin(2);

    highlighter_ = new ExpressionHighlighter(document());
}

QString ExpressionEdit::expression() const
{
    return toPlainText().simplified();
}

void ExpressionEdit::setExpression(const QString& text)
{
    setPlainText(text.simplified());
}

void ExpressionEdit::applyTheme(ThemeMode mode)
{
    highlighter_->setTheme(mode);
}

void ExpressionEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit applied();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ExpressionEdit::insertFromMimeData(const QMimeData* source)
{
    insertPlainText(source->text().simplified());
}

} // namespace kentos::app
