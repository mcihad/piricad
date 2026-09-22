// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/python_editor.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kIndent         = 4;   ///< PEP 8, and the only indentation this offers
constexpr int kGutterPad      = 10;  ///< breathing room either side of the numbers
constexpr int kPromptHeight   = 64;  ///< three lines: a statement, not an essay
constexpr int kConsoleMinimum = 200; ///< the prompt plus enough answer to be worth reading

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// Python's reserved words, exactly as CPython 3.14 defines them.
///
/// LISTED AND NOT DERIVED, because there is nothing to derive them from: they are
/// the language's own, they change about once a decade, and `keyword.kwlist`
/// lives inside an interpreter this widget must run without — the editor works in
/// a build with no Python at all, which is the build most users will open first.
const QRegularExpression& keywords()
{
    static const QRegularExpression re(QStringLiteral(
        "\\b(False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|else"
        "|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise"
        "|return|try|while|with|yield)\\b"));
    return re;
}

/// The builtins a script here actually reaches for. A short list on purpose: a
/// complete one would tint half the file and say nothing.
const QRegularExpression& builtins()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(abs|all|any|bool|dict|enumerate|filter|float|format|int|isinstance"
                       "|len|list|map|max|min|open|print|range|round|set|sorted|str|sum|tuple"
                       "|zip|Exception|ValueError|TypeError|RuntimeError)\\b"));
    return re;
}

const QRegularExpression& numbers()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(0[xX][0-9a-fA-F_]+|\\d[\\d_]*\\.?[\\d_]*([eE][+-]?\\d+)?)\\b"));
    return re;
}

const QRegularExpression& comment()
{
    static const QRegularExpression re(QStringLiteral("#[^\\n]*"));
    return re;
}

/// A single-line string, either quote, with escapes. Triple quotes are handled
/// separately because they are the one construct that crosses a block.
const QRegularExpression& shortString()
{
    static const QRegularExpression re(QStringLiteral(R"((\"([^\"\\]|\\.)*\"|'([^'\\]|\\.)*'))"));
    return re;
}

const QRegularExpression& tripleDelimiter()
{
    static const QRegularExpression re(QStringLiteral("(\"\"\"|''')"));
    return re;
}

/// The name a `def` or `class` is introducing.
const QRegularExpression& definition()
{
    static const QRegularExpression re(QStringLiteral("\\b(?:def|class)\\s+([A-Za-z_]\\w*)"));
    return re;
}

/// True when the source so far cannot be a complete statement.
///
/// SHAPE, NOT GRAMMAR: an unbalanced bracket, or a line ending in a backslash.
/// Everything else is left to CPython, which is the only thing here that
/// actually knows.
///
/// WHAT IT DELIBERATELY DOES NOT DO is keep a block open after a line ending in
/// `:`. A one-line REPL has to — it has nowhere to put the body — and asks for a
/// blank line to close it. This prompt is FOUR LINES TALL: a `for` and its body
/// are typed together with Shift+Enter and submitted as one thing, so treating
/// the colon as "wait for more" would make every loop need an extra Enter and
/// leave the user pressing it to find out. The bracket rule stays because a
/// half-typed call really is incomplete however tall the box is.
bool wantsMore(const QStringList& lines)
{
    if (lines.isEmpty()) return false;
    if (lines.last().endsWith(QLatin1Char('\\'))) return true;

    int depth = 0;
    for (const QString& line : lines) {
        bool in_string = false;
        QChar quote;
        for (int i = 0; i < line.size(); ++i) {
            const QChar c = line.at(i);
            if (in_string) {
                if (c == QLatin1Char('\\')) {
                    ++i;
                    continue;
                }
                if (c == quote) in_string = false;
                continue;
            }
            if (c == QLatin1Char('#')) break;
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                in_string = true;
                quote     = c;
                continue;
            }
            if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) ++depth;
            if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) --depth;
        }
    }
    return depth > 0;
}

/// The gutter. A bare `QWidget` whose paint and width forward to the editor,
/// which is Qt's own code-editor shape: the numbers need the editor's block
/// geometry and have no business owning any of it.
class Gutter : public QWidget
{
public:
    explicit Gutter(ScriptEditor* editor) : QWidget(editor), editor_(editor) {}

    QSize sizeHint() const override { return {editor_->gutterWidth(), 0}; }

protected:
    void paintEvent(QPaintEvent* event) override { editor_->paintGutter(event); }

private:
    ScriptEditor* editor_;
};

} // namespace

// =============================================================================
// PythonHighlighter
// =============================================================================

PythonHighlighter::PythonHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {}

void PythonHighlighter::setApiNames(const QStringList& names)
{
    api_ = names;
    rehighlight();
}

void PythonHighlighter::setTheme(ThemeMode mode)
{
    theme_ = mode;
    rehighlight();
}

void PythonHighlighter::highlightBlock(const QString& text)
{
    const Tokens& t = tokensOf(theme_);

    const auto tint = [this, &text](const QRegularExpression& re, const QColor& ink,
                                    bool bold = false, int group = 0) {
        QTextCharFormat format;
        format.setForeground(ink);
        if (bold) format.setFontWeight(QFont::DemiBold);
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(static_cast<int>(m.capturedStart(group)),
                      static_cast<int>(m.capturedLength(group)), format);
        }
    };

    // ORDER IS THE WHOLE ALGORITHM. Words first, then literals over them, then
    // the comment over everything — so `# if x` is a comment and `"def"` is a
    // string, which is what a reader expects and what a naive pass gets wrong.
    tint(numbers(), t.syntaxNumber);
    tint(keywords(), t.syntaxLogical, /*bold=*/true);
    tint(builtins(), t.syntaxOperator);
    tint(definition(), t.syntaxDef, /*bold=*/true, /*group=*/1);

    // THE PROGRAM'S OWN NAMES, from the live registry. `cad` itself is tinted
    // whatever follows it; a member is tinted only when this build really has it,
    // so a typo looks wrong before it is run.
    if (!api_.isEmpty()) {
        QTextCharFormat api;
        api.setForeground(t.syntaxField);
        static const QRegularExpression member(QStringLiteral("\\bcad(?:\\.doc)?\\.(\\w+)"));
        auto it = member.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (!api_.contains(m.captured(1))) continue;
            setFormat(static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()),
                      api);
        }
    }

    tint(shortString(), t.syntaxString);

    // THE ONE CONSTRUCT THAT CROSSES A BLOCK. `QSyntaxHighlighter` keeps one int
    // per block for exactly this: 1 means "this block ended inside a triple
    // quote", and the next block starts by asking.
    QTextCharFormat string;
    string.setForeground(t.syntaxString);

    int start = 0;
    setCurrentBlockState(0);
    if (previousBlockState() != 1) {
        const QRegularExpressionMatch open = tripleDelimiter().match(text);
        start = open.hasMatch() ? static_cast<int>(open.capturedStart()) : -1;
    }
    while (start >= 0) {
        const QRegularExpressionMatch close =
            tripleDelimiter().match(text, start + (previousBlockState() == 1 ? 0 : 3));
        int length = 0;
        if (close.hasMatch()) {
            length = static_cast<int>(close.capturedEnd()) - start;
        } else {
            setCurrentBlockState(1);
            length = static_cast<int>(text.length()) - start;
        }
        setFormat(start, length, string);

        const QRegularExpressionMatch next = tripleDelimiter().match(text, start + length);
        start                              = (currentBlockState() == 1 || !next.hasMatch())
                                                 ? -1
                                                 : static_cast<int>(next.capturedStart());
    }

    tint(comment(), t.syntaxComment);
}

// =============================================================================
// ScriptEditor
// =============================================================================

ScriptEditor::ScriptEditor(QWidget* parent) : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("scriptEditor"));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabChangesFocus(false);
    setAccessibleName(tr("Python kaynağı"));

    QFont face(QStringLiteral("IBM Plex Mono"));
    face.setPixelSize(12);
    setFont(face);
    setTabStopDistance(QFontMetricsF(face).horizontalAdvance(QLatin1Char(' ')) * kIndent);
    document()->setDocumentMargin(4);

    highlighter_ = new PythonHighlighter(document());
    gutter_      = new Gutter(this);

    words_     = new QStringListModel(this);
    completer_ = new QCompleter(words_, this);
    completer_->setWidget(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseSensitive);
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            &ScriptEditor::insertCompletion);

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) { updateGutterWidth(); });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy) {
        if (dy != 0)
            gutter_->scroll(0, dy);
        else
            gutter_->update(0, rect.y(), gutter_->width(), rect.height());
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this,
            &ScriptEditor::highlightCurrentLine);

    updateGutterWidth();
    highlightCurrentLine();
}

void ScriptEditor::setApiNames(const QStringList& names)
{
    highlighter_->setApiNames(names);

    // OFFERED WITH THE `cad.` IN FRONT, because that is what the user is typing.
    // A completer over bare names would offer `line` in the middle of a comment.
    QStringList offered;
    offered.reserve(names.size() * 2 + 8);
    for (const QString& n : names)
        offered << QStringLiteral("cad.") + n;
    offered << QStringLiteral("cad.run") << QStringLiteral("cad.sandbox")
            << QStringLiteral("cad.read_file") << QStringLiteral("cad.write_file")
            << QStringLiteral("cad.doc.layers") << QStringLiteral("cad.doc.layer_count")
            << QStringLiteral("cad.doc.active_layer") << QStringLiteral("cad.doc.entity_count")
            << QStringLiteral("cad.doc.selection_count") << QStringLiteral("cad.doc.crs")
            << QStringLiteral("cad.doc.setting");
    offered.sort();
    words_->setStringList(offered);
}

int ScriptEditor::gutterWidth() const
{
    int digits = 1;
    for (int lines = qMax(1, blockCount()); lines >= 10; lines /= 10)
        ++digits;
    return kGutterPad + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + kGutterPad;
}

void ScriptEditor::updateGutterWidth()
{
    setViewportMargins(gutterWidth(), 0, 0, 0);
    gutter_->update();
}

void ScriptEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect box = contentsRect();
    gutter_->setGeometry(QRect(box.left(), box.top(), gutterWidth(), box.height()));
}

void ScriptEditor::paintGutter(QPaintEvent* event)
{
    const Tokens& t = tokensOf(theme_);

    QPainter painter(gutter_);
    painter.fillRect(event->rect(), t.bgPanel);

    QTextBlock block = firstVisibleBlock();
    int number       = block.blockNumber();
    int top    = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    const int current = textCursor().blockNumber();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            // THE CURRENT LINE'S NUMBER IS THE ONLY BRIGHT ONE. A gutter is a
            // reference, not a column of content, and 200 equally dark numbers
            // beside the code compete with it.
            painter.setPen(number == current ? t.text : t.textFaint);
            painter.drawText(0, top, gutter_->width() - kGutterPad, fontMetrics().height(),
                             Qt::AlignRight, QString::number(number + 1));
        }
        block  = block.next();
        top    = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++number;
    }
}

void ScriptEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> lines;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection here;
        here.format.setBackground(tokensOf(theme_).bgRaised);
        here.format.setProperty(QTextFormat::FullWidthSelection, true);
        here.cursor = textCursor();
        here.cursor.clearSelection();
        lines.append(here);
    }
    setExtraSelections(lines);
}

QString ScriptEditor::wordUnderCursor() const
{
    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text().left(cursor.positionInBlock());

    // Walk back over what may be part of a dotted name. Stopping at the dot would
    // make `cad.li` complete as `li`, which matches nothing offered.
    int at = static_cast<int>(line.size());
    while (at > 0) {
        const QChar c = line.at(at - 1);
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.'))
            --at;
        else
            break;
    }
    return line.mid(at);
}

void ScriptEditor::insertCompletion(const QString& completion)
{
    QTextCursor cursor = textCursor();
    const int extra =
        static_cast<int>(completion.length() - completer_->completionPrefix().length());
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 0);
    cursor.insertText(completion.right(extra));
    setTextCursor(cursor);
}

void ScriptEditor::keyPressEvent(QKeyEvent* event)
{
    // The popup owns Enter, Tab and the arrows while it is up; letting them
    // through would run the line the user is still choosing a word for.
    if (completer_->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab: event->ignore(); return;
        default: break;
        }
    }

    // TAB IS FOUR SPACES, always. A file that mixes tabs and spaces is a file
    // CPython refuses to run, and an editor that produced one would be handing
    // the user a `TabError` it could simply not cause.
    if (event->key() == Qt::Key_Tab && !completer_->popup()->isVisible()) {
        insertPlainText(QString(kIndent, QLatin1Char(' ')));
        return;
    }
    if (event->key() == Qt::Key_Backtab) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, kIndent);
        if (cursor.selectedText() == QString(kIndent, QLatin1Char(' ')))
            cursor.removeSelectedText();
        return;
    }

    // AT A CONSOLE PROMPT, ENTER SENDS. Shift+Enter still opens a line, so a
    // `for` loop is typable; the reverse binding would make the panel's one
    // gesture need a modifier.
    if (submitOnEnter_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        (event->modifiers() & Qt::ShiftModifier) == 0) {
        emit submitRequested();
        return;
    }

    // UP AND DOWN RECALL, but only from the first and last line — inside a
    // multi-line statement they must still move the cursor, which is what they
    // are for.
    if (submitOnEnter_ && !history_.isEmpty() &&
        (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        const QTextCursor cursor = textCursor();
        const bool at_top        = cursor.blockNumber() == 0;
        const bool at_bottom     = cursor.blockNumber() == blockCount() - 1;
        if ((event->key() == Qt::Key_Up && at_top) || (event->key() == Qt::Key_Down && at_bottom)) {
            const int depth = static_cast<int>(history_.size());
            if (recall_ < 0) recall_ = depth;
            recall_ += (event->key() == Qt::Key_Up) ? -1 : 1;
            recall_ = qBound(0, recall_, depth);
            setPlainText(recall_ < depth ? history_.at(recall_) : QString());
            moveCursor(QTextCursor::End);
            return;
        }
    }

    // ENTER KEEPS THE INDENT, and adds one after a `:`. This is the whole of what
    // an editor owes a Python writer: the language's blocks ARE the indentation,
    // so re-typing it every line is re-typing the syntax.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) == 0) {
            const QString line = textCursor().block().text();
            int lead           = 0;
            while (lead < line.size() && line.at(lead) == QLatin1Char(' '))
                ++lead;
            if (line.trimmed().endsWith(QLatin1Char(':'))) lead += kIndent;

            QPlainTextEdit::keyPressEvent(event);
            insertPlainText(QString(lead, QLatin1Char(' ')));
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    const QString prefix = wordUnderCursor();
    if (prefix.length() < 3 || !prefix.startsWith(QStringLiteral("cad"))) {
        completer_->popup()->hide();
        return;
    }
    if (prefix != completer_->completionPrefix()) {
        completer_->setCompletionPrefix(prefix);
        completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
    }
    if (completer_->completionCount() == 0) {
        completer_->popup()->hide();
        return;
    }
    QRect box = cursorRect();
    box.setWidth(completer_->popup()->sizeHintForColumn(0) +
                 completer_->popup()->verticalScrollBar()->sizeHint().width());
    completer_->complete(box);
}

void ScriptEditor::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    highlighter_->setTheme(mode);
    highlightCurrentLine();
    gutter_->update();
}

// =============================================================================
// PythonConsole
// =============================================================================

PythonConsole::PythonConsole(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("pythonConsole"));

    transcript_ = new QPlainTextEdit(this);
    transcript_->setObjectName(QStringLiteral("pythonTranscript"));
    transcript_->setReadOnly(true);
    transcript_->setMaximumBlockCount(2000);
    transcript_->setFrameShape(QFrame::NoFrame);
    transcript_->setAccessibleName(tr("Python çıktısı"));
    {
        QFont face(QStringLiteral("IBM Plex Mono"));
        face.setPixelSize(12);
        transcript_->setFont(face);
    }

    prompt_ = new ScriptEditor(this);
    prompt_->setObjectName(QStringLiteral("pythonPrompt"));
    prompt_->setFixedHeight(kPromptHeight);
    prompt_->setAccessibleName(tr("Python istemi"));
    prompt_->setApiNames(controller_.pythonApiNames());

    // A FLOOR AND NOT A HINT. A dock area divides the height it has among the
    // docks in it, and a `sizeHint` is only a preference — the panel first opened
    // as a 90 px slot where the 64 px prompt left six pixels of transcript, which
    // read as "the console printed nothing". A minimum is the one thing Qt cannot
    // negotiate away, and a console with no room to answer is not a console.
    setMinimumHeight(kConsoleMinimum);

    auto* stack = new QVBoxLayout(this);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    stack->addWidget(transcript_, 1);
    stack->addWidget(prompt_);

    // ENTER SENDS, Shift+Enter opens a line. `wantsMore` keeps a block open on
    // its own, so a `for` loop still gets its body without a modifier.
    prompt_->setSubmitOnEnter(true);
    connect(prompt_, &ScriptEditor::submitRequested, this, &PythonConsole::submit);

    appendOutput(tr("Python konsolu. `cad.run(\"ÇİZGİ 0,0 10,10\")` ya da `cad.line(points=…)`. "
                    "Yardım için `help(cad.line)`."));
}

void PythonConsole::appendOutput(const QString& text)
{
    if (text.isEmpty()) return;
    transcript_->appendPlainText(text);
    transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
}

void PythonConsole::focusPrompt()
{
    prompt_->setFocus(Qt::OtherFocusReason);
}

void PythonConsole::runSource(const QString& source)
{
    prompt_->setPlainText(source);
    submit();
}

void PythonConsole::submit()
{
    const QString typed = prompt_->toPlainText();
    if (typed.trimmed().isEmpty() && buffer_.isEmpty()) return;

    for (const QString& line : typed.split(QLatin1Char('\n')))
        buffer_ << line;
    appendOutput((buffer_.size() > 1 ? QStringLiteral("... ") : QStringLiteral(">>> ")) +
                 typed.split(QLatin1Char('\n')).join(QStringLiteral("\n... ")));
    prompt_->clear();

    if (wantsMore(buffer_)) return;

    const QString source = buffer_.join(QLatin1Char('\n'));
    buffer_.clear();
    history_ << source;
    prompt_->setHistory(history_);

    // THE COMMAND, not the interpreter. `core.python` is what a user could have
    // typed at the command line, so this panel is a client and not a private
    // entry point (Article 1.2, CLAUDE.md 5.15).
    //
    // WHAT THE BUS SAYS WHILE THIS RUNS IS THIS CONSOLE'S ANSWER, and only while
    // it runs. A permanent connection would fill the console with "1 çizgi
    // çizildi" every time the user drew with the mouse — the panel would stop
    // being a transcript of a conversation and become a second command log, which
    // the window already has.
    const QMetaObject::Connection listening =
        connect(&controller_, &Controller::echoed, this, &PythonConsole::appendOutput);
    controller_.runPython(source);
    disconnect(listening);
}

void PythonConsole::applyTheme(ThemeMode mode)
{
    theme_ = mode;

    // THE TRANSCRIPT IS STYLED BY THE ONE SHEET, by its object name
    // (`pythonTranscript` in theme.cpp). A private stylesheet here would be a
    // second place the panel's colours are decided, and the first theme change
    // that touched only one of them would show it (`ui.md` R1, design.md §12).
    prompt_->applyTheme(mode);
}

} // namespace kentos::app
