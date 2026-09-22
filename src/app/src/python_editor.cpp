// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/python_editor.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
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

/// THE HAND-WRITTEN HALF OF THE SURFACE, and the only part of it that is a list
/// here. Everything under `cad.<command>` is projected from the registry; these
/// eleven names are written in `python_runner.cpp` by hand, so they are written
/// here by hand too — and a test walks the module to prove the two agree, which
/// is what keeps a list this short from drifting.
const QStringList kHostCalls{QStringLiteral("run"), QStringLiteral("sandbox"),
                             QStringLiteral("read_file"), QStringLiteral("write_file")};

const QStringList kDocCalls{QStringLiteral("layers"),          QStringLiteral("layer_count"),
                            QStringLiteral("active_layer"),    QStringLiteral("entity_count"),
                            QStringLiteral("selection_count"), QStringLiteral("crs"),
                            QStringLiteral("setting")};

const QStringList kViewportCalls{QStringLiteral("exists"),       QStringLiteral("bbox"),
                                 QStringLiteral("center"),       QStringLiteral("scale"),
                                 QStringLiteral("mm_per_pixel"), QStringLiteral("size_px"),
                                 QStringLiteral("crs")};

/// Python's own words, offered where a name with no owner is being typed.
const QStringList kPythonWords{
    QStringLiteral("False"),  QStringLiteral("None"),   QStringLiteral("True"),
    QStringLiteral("and"),    QStringLiteral("as"),     QStringLiteral("assert"),
    QStringLiteral("break"),  QStringLiteral("class"),  QStringLiteral("continue"),
    QStringLiteral("def"),    QStringLiteral("del"),    QStringLiteral("elif"),
    QStringLiteral("else"),   QStringLiteral("except"), QStringLiteral("finally"),
    QStringLiteral("for"),    QStringLiteral("from"),   QStringLiteral("global"),
    QStringLiteral("if"),     QStringLiteral("import"), QStringLiteral("in"),
    QStringLiteral("is"),     QStringLiteral("lambda"), QStringLiteral("not"),
    QStringLiteral("or"),     QStringLiteral("pass"),   QStringLiteral("raise"),
    QStringLiteral("return"), QStringLiteral("try"),    QStringLiteral("while"),
    QStringLiteral("with"),   QStringLiteral("yield")};

const QStringList kPythonBuiltins{
    QStringLiteral("abs"),   QStringLiteral("all"),    QStringLiteral("any"),
    QStringLiteral("bool"),  QStringLiteral("dict"),   QStringLiteral("enumerate"),
    QStringLiteral("float"), QStringLiteral("int"),    QStringLiteral("isinstance"),
    QStringLiteral("len"),   QStringLiteral("list"),   QStringLiteral("max"),
    QStringLiteral("min"),   QStringLiteral("print"),  QStringLiteral("range"),
    QStringLiteral("round"), QStringLiteral("sorted"), QStringLiteral("str"),
    QStringLiteral("sum"),   QStringLiteral("tuple"),  QStringLiteral("zip")};

/// Draws a completion row: the name, then its kind dimmed on the right.
///
/// A DELEGATE AND NOT A STYLESHEET, for the reason the signature hint is painted:
/// the two halves of the row differ in MEANING, and a sheet has no way to say
/// "this column quieter than that one" without an object name per column.
class CompletionRow : public QStyledItemDelegate
{
public:
    CompletionRow(QObject* parent, ThemeMode* theme) : QStyledItemDelegate(parent), theme_(theme) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = tokensOf(*theme_);
        const bool on   = (option.state & QStyle::State_Selected) != 0;

        painter->fillRect(option.rect, on ? t.accentWash : t.bgPanel);

        const QAbstractItemModel* model = index.model();
        const QString name              = model->data(model->index(index.row(), 0)).toString();
        const QString detail            = model->data(model->index(index.row(), 1)).toString();

        QRect box = option.rect.adjusted(8, 0, -8, 0);
        painter->setPen(t.text);
        painter->drawText(box, Qt::AlignLeft | Qt::AlignVCenter, name);

        painter->setPen(t.textFaint);
        painter->drawText(box, Qt::AlignRight | Qt::AlignVCenter, detail);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), 22));
        size.setWidth(size.width() + 28); // the gap between name and kind
        return size;
    }

private:
    ThemeMode* theme_;
};

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

} // namespace

/// The signature hint: one painted strip under the cursor.
///
/// PAINTED AND NOT STYLED, like the status strip and the title bar: it is three
/// runs of text whose colours ARE the meaning — the active parameter in the
/// accent, the rest quiet, the Turkish help under them — and a stylesheet cannot
/// say "this word and not that one". It carries no controls, so CLAUDE.md 5.19
/// has nothing to object to.
///
/// A CHILD WINDOW rather than a tooltip: a tooltip disappears when the user
/// types, which is exactly when a signature is worth reading.
class SignatureHint : public QWidget
{
public:
    explicit SignatureHint(QWidget* parent) : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
    }

    /// `head` is `cad.line(`, `parts` the parameters, `active` the one the cursor
    /// is on (-1 for none), `note` the Turkish help under the line.
    void show(const QString& head, const QStringList& parts, int active, const QString& note,
              const QString& tail, ThemeMode mode)
    {
        head_   = head;
        parts_  = parts;
        active_ = active;
        note_   = note;
        tail_   = tail;
        theme_  = mode;
        adjustSize();
        update();
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const QVector<int> rows = layoutRows(fm);
        int widest              = 0;
        for (const int w : rows)
            widest = qMax(widest, w);
        widest          = qMax(widest, fm.horizontalAdvance(note_) + kPad);
        const int lines = static_cast<int>(rows.size()) + (note_.isEmpty() ? 0 : 1);
        return {qMin(widest, kMaxWidth) + kPad, lines * fm.height() + 2 * kPad};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(t.border, 1));
        p.setBrush(t.bgRaised);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);

        const QFontMetrics fm(font());
        const int indent = kPad + fm.horizontalAdvance(head_);
        int x            = kPad;
        int y            = kPad + fm.ascent();

        p.setPen(t.textDim);
        p.drawText(x, y, head_);
        x = indent;

        for (int i = 0; i < parts_.size(); ++i) {
            // THE COMMA STAYS WITH THE PARAMETER IT FOLLOWS. Drawn before the
            // wrap test rather than after it, or a break lands between a
            // parameter and its own comma and the next line opens with `, radius`
            // — which reads as a typo and is the first thing a reader's eye
            // catches.
            if (i != 0) {
                p.setPen(t.textFaint);
                p.drawText(x, y, QStringLiteral(","));
                x += fm.horizontalAdvance(QStringLiteral(", "));
            }

            // WRAPPED, because a command with nine parameters is two screens wide
            // and a hint that runs off the edge is a hint nobody reads. The break
            // is between parameters, never inside one, and the continuation lines
            // are indented under the opening bracket so the call still reads as
            // one call.
            if (x > indent && x + fm.horizontalAdvance(parts_.at(i)) > kMaxWidth) {
                x = indent;
                y += fm.height();
            }
            // THE ACTIVE PARAMETER IS THE WHOLE REASON THIS EXISTS. A signature
            // with every word the same weight answers "what does it take"; this
            // answers "what does it want NEXT", which is the question the person
            // with their hands on the keyboard actually has.
            p.setPen(i == active_ ? t.accent : t.textFaint);
            p.drawText(x, y, parts_.at(i));
            x += fm.horizontalAdvance(parts_.at(i));
        }

        p.setPen(t.textDim);
        p.drawText(x, y, tail_);

        if (!note_.isEmpty()) {
            p.setPen(t.textFaint);
            p.drawText(kPad, y + fm.height(), note_);
        }
    }

private:
    static constexpr int kPad      = 7;
    static constexpr int kMaxWidth = 860; ///< past this the signature wraps

    /// The width of each wrapped row, for `sizeHint`. Measured the way
    /// `paintEvent` lays it out, because a hint sized one way and drawn another
    /// is a hint with its last parameter cut off.
    QVector<int> layoutRows(const QFontMetrics& fm) const
    {
        QVector<int> rows;
        const int indent = kPad + fm.horizontalAdvance(head_);
        int x            = indent;
        for (int i = 0; i < parts_.size(); ++i) {
            if (i != 0) x += fm.horizontalAdvance(QStringLiteral(", "));
            if (x > indent && x + fm.horizontalAdvance(parts_.at(i)) > kMaxWidth) {
                rows.push_back(x);
                x = indent;
            }
            x += fm.horizontalAdvance(parts_.at(i));
        }
        rows.push_back(x + fm.horizontalAdvance(tail_));
        return rows;
    }

    QString head_;
    QStringList parts_;
    QString tail_;
    QString note_;
    int active_{-1};
    ThemeMode theme_{ThemeMode::Dark};
};

namespace {

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

void PythonHighlighter::setArgumentNames(const QStringList& names)
{
    args_ = names;
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

    // A KEYWORD ARGUMENT IS NOT AN ORDINARY NAME. `points=` names a declared
    // parameter and reads as one only when it is tinted as one — and only when
    // this build really declares it, so a misspelling stays plain.
    if (!args_.isEmpty()) {
        QTextCharFormat argument;
        argument.setForeground(t.syntaxField);
        static const QRegularExpression keyword(QStringLiteral("\\b(\\w+)\\s*="));
        auto it = keyword.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (!args_.contains(m.captured(1))) continue;
            setFormat(static_cast<int>(m.capturedStart(1)), static_cast<int>(m.capturedLength(1)),
                      argument);
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

    hint_ = new SignatureHint(this);

    words_     = new QStandardItemModel(this);
    completer_ = new QCompleter(words_, this);
    completer_->setWidget(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCompletionColumn(0);
    completer_->setCompletionRole(Qt::DisplayRole);

    // CASE-INSENSITIVE, because the API is lower case and a user coming from the
    // Turkish command line types `CAD.` about once a session. Matching by
    // CONTAINS rather than by prefix would offer `circle_draw` for `raw`, which
    // is a different editor's idea and not this one's.
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->popup()->setItemDelegate(new CompletionRow(completer_, &theme_));
    if (auto* list = qobject_cast<QListView*>(completer_->popup()); list != nullptr)
        list->setUniformItemSizes(true);
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

void ScriptEditor::setApi(QVector<PythonCallable> api)
{
    api_ = std::move(api);

    QStringList names;
    QStringList arguments;
    names.reserve(api_.size() + 16);
    for (const PythonCallable& c : api_) {
        names << c.name;
        for (const PythonArg& a : c.args)
            if (!arguments.contains(a.name)) arguments << a.name;
    }
    for (const QString& n : kHostCalls)
        names << n;
    for (const QString& n : kDocCalls)
        names << n;
    for (const QString& n : kViewportCalls)
        names << n;
    names << QStringLiteral("Point") << QStringLiteral("Box") << QStringLiteral("doc")
          << QStringLiteral("viewport");

    highlighter_->setApiNames(names);
    highlighter_->setArgumentNames(arguments);
}

const PythonCallable* ScriptEditor::callable(const QString& name) const
{
    for (const PythonCallable& c : api_)
        if (c.name == name) return &c;
    return nullptr;
}

QStringList ScriptEditor::localNames() const
{
    // WHAT THE BUFFER ITSELF DEFINES. Half of what a person types is a name they
    // wrote three lines up, and a completer that knew only the API would be
    // useless for exactly that half.
    //
    // Four shapes, read off the text: an assignment, a `def`/`class`, a `for`
    // target and an `import ... as`. Not a scope analysis — a name defined in a
    // function is offered outside it, which costs a wrong suggestion and saves
    // writing a Python binder in an editor.
    static const QRegularExpression assigned(
        QStringLiteral("^\\s*([A-Za-z_]\\w*)\\s*(?::[^=]+)?=[^=]"));
    static const QRegularExpression defined(
        QStringLiteral("^\\s*(?:def|class)\\s+([A-Za-z_]\\w*)"));
    static const QRegularExpression looped(
        QStringLiteral("^\\s*for\\s+([A-Za-z_]\\w*(?:\\s*,\\s*[A-Za-z_]\\w*)*)\\s+in\\b"));
    static const QRegularExpression imported(
        QStringLiteral("^\\s*(?:import|from)\\b.*?\\bas\\s+([A-Za-z_]\\w*)"));

    QStringList out;
    const QStringList lines = toPlainText().split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        for (const QRegularExpression* re : {&assigned, &defined, &looped, &imported}) {
            const QRegularExpressionMatch m = re->match(line);
            if (!m.hasMatch()) continue;
            for (const QString& name : m.captured(1).split(QLatin1Char(','))) {
                const QString clean = name.trimmed();
                if (!clean.isEmpty() && !out.contains(clean)) out << clean;
            }
        }
    }
    return out;
}

ScriptEditor::Context ScriptEditor::contextAt() const
{
    Context where;

    const QTextCursor cursor = textCursor();
    const QString source     = toPlainText().left(cursor.position());

    // ---- what is being typed, and what owns it ------------------------------
    int at = static_cast<int>(source.size());
    while (at > 0 &&
           (source.at(at - 1).isLetterOrNumber() || source.at(at - 1) == QLatin1Char('_')))
        --at;
    where.prefix = source.mid(at);

    if (at > 0 && source.at(at - 1) == QLatin1Char('.')) {
        int owner = at - 1;
        while (owner > 0 && (source.at(owner - 1).isLetterOrNumber() ||
                             source.at(owner - 1) == QLatin1Char('_') ||
                             source.at(owner - 1) == QLatin1Char('.')))
            --owner;
        where.owner = source.mid(owner, at - 1 - owner);
    }

    // ---- which call the cursor is inside ------------------------------------
    //
    // Walked backwards, counting brackets and skipping strings, to the first
    // unclosed `(`. Commas at THAT depth are the argument separators; commas
    // inside a nested list are not, which is why a depth counter and not a split.
    int depth      = 0;
    int commas     = 0;
    int open       = -1;
    bool in_string = false;
    QChar quote;
    for (int i = static_cast<int>(source.size()) - 1; i >= 0; --i) {
        const QChar c = source.at(i);
        if (in_string) {
            if (c == quote && (i == 0 || source.at(i - 1) != QLatin1Char('\\'))) in_string = false;
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            in_string = true;
            quote     = c;
            continue;
        }
        if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}'))
            ++depth;
        else if (c == QLatin1Char('[') || c == QLatin1Char('{'))
            --depth;
        else if (c == QLatin1Char('(')) {
            if (depth == 0) {
                open = i;
                break;
            }
            --depth;
        } else if (c == QLatin1Char(',') && depth == 0) {
            ++commas;
        } else if (c == QLatin1Char('\n') && depth == 0) {
            // A newline at depth zero ends the statement, so a call on the line
            // above is not the call we are in.
            break;
        }
    }

    if (open >= 0) {
        int name = open;
        while (name > 0 &&
               (source.at(name - 1).isLetterOrNumber() || source.at(name - 1) == QLatin1Char('_')))
            --name;
        where.call          = source.mid(name, open - name);
        where.argumentIndex = commas;

        static const QRegularExpression written(QStringLiteral("([A-Za-z_]\\w*)\\s*="));
        auto it = written.globalMatch(source.mid(open + 1));
        while (it.hasNext())
            where.used << it.next().captured(1);
    }

    return where;
}

void ScriptEditor::offerCompletions(const Context& where)
{
    struct Item
    {
        QString text;   ///< what gets inserted
        QString detail; ///< what is shown, dimmed, to its right
    };

    QVector<Item> items;

    const auto add = [&items](const QString& text, const QString& detail) {
        items.push_back(Item{text, detail});
    };

    if (where.owner == QStringLiteral("cad")) {
        for (const PythonCallable& c : api_)
            add(c.name, c.turkish.isEmpty() ? c.command : c.turkish);
        for (const QString& n : kHostCalls)
            add(n, tr("konak"));
        add(QStringLiteral("doc"), tr("çizimden okuma"));
        add(QStringLiteral("viewport"), tr("görünüm"));
        add(QStringLiteral("Point"), tr("koordinat tipi"));
        add(QStringLiteral("Box"), tr("dikdörtgen tipi"));
    } else if (where.owner == QStringLiteral("cad.doc")) {
        for (const QString& n : kDocCalls)
            add(n, tr("çizimden okuma"));
    } else if (where.owner == QStringLiteral("cad.viewport")) {
        for (const QString& n : kViewportCalls)
            add(n, tr("görünüm"));
    } else if (where.owner.isEmpty()) {
        // INSIDE A CALL, THE KEYWORDS COME FIRST, because that is what the cursor
        // is actually waiting for. One that is already written is not offered
        // again — a second `points=` is a TypeError, not a suggestion.
        if (const PythonCallable* c = callable(where.call); c != nullptr) {
            for (const PythonArg& a : c->args) {
                if (where.used.contains(a.name)) continue;
                add(a.name + QStringLiteral("="), a.type);
            }
        }
        add(QStringLiteral("cad"), tr("çizim"));
        for (const QString& n : localNames())
            if (n != where.prefix) add(n, tr("bu betikte"));
        for (const QString& n : kPythonWords)
            add(n, tr("anahtar sözcük"));
        for (const QString& n : kPythonBuiltins)
            add(n, tr("yerleşik"));
    }

    words_->clear();
    words_->setColumnCount(2);
    for (const Item& item : items) {
        auto* name   = new QStandardItem(item.text);
        auto* detail = new QStandardItem(item.detail);
        detail->setFlags(Qt::NoItemFlags);
        words_->appendRow({name, detail});
    }

    if (items.isEmpty()) {
        completer_->popup()->hide();
        return;
    }

    completer_->setCompletionPrefix(where.prefix);
    if (completer_->completionCount() == 0) {
        completer_->popup()->hide();
        return;
    }
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));

    QRect box = cursorRect();
    box.setWidth(completer_->popup()->sizeHintForColumn(0) +
                 completer_->popup()->sizeHintForColumn(1) + 36 +
                 completer_->popup()->verticalScrollBar()->sizeHint().width());
    box.translate(-fontMetrics().horizontalAdvance(where.prefix), 0);
    completer_->complete(box);
}

void ScriptEditor::updateSignatureHint(const Context& where)
{
    const PythonCallable* c = callable(where.call);
    if (c == nullptr || !hasFocus()) {
        hint_->hide();
        return;
    }

    QStringList parts;
    parts.reserve(c->args.size());
    for (const PythonArg& a : c->args)
        parts << a.name + QStringLiteral(": ") + a.type;

    // WHICH ARGUMENT IS ACTIVE. A keyword already written wins over the position:
    // `cad.line(points=` is on `points` whatever the comma count says, because
    // the user named it.
    int active = where.argumentIndex;
    if (!where.used.isEmpty()) {
        for (int i = 0; i < c->args.size(); ++i)
            if (c->args.at(i).name == where.used.last()) active = i;
    }
    if (active >= c->args.size()) active = -1;

    QString note = c->summary;
    if (active >= 0) {
        const PythonArg& a = c->args.at(active);
        note               = a.name + QStringLiteral(" — ") + a.help;
        if (!a.unit.isEmpty()) note += QStringLiteral(" [") + a.unit + QStringLiteral("]");
    }

    hint_->setFont(font());
    hint_->show(QStringLiteral("cad.") + c->name + QLatin1Char('('), parts, active, note,
                QStringLiteral(") -> int"), theme_);

    // UNDER THE CURSOR, and pushed back onto the screen when the line is long.
    QPoint at = mapToGlobal(cursorRect().bottomLeft()) + QPoint(0, 6);
    if (const QScreen* screen = this->screen(); screen != nullptr) {
        const int right = screen->availableGeometry().right() - hint_->width() - 8;
        at.setX(qMin(at.x(), right));
    }
    hint_->move(at);
    hint_->QWidget::show();
}

QWidget* ScriptEditor::signatureHint() const
{
    return hint_;
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

void ScriptEditor::insertCompletion(const QString& completion)
{
    // THE WHOLE WORD IS REPLACED, not appended to. Appending the tail worked only
    // while matching was by prefix and case-sensitive: with `CAD.li` matching
    // `line` the tail is `ne`, and a match shorter than what was typed gives a
    // negative length.
    QTextCursor cursor = textCursor();
    const int typed    = static_cast<int>(completer_->completionPrefix().length());
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, typed);
    cursor.insertText(completion);
    setTextCursor(cursor);

    // A CALL OPENS ITS OWN PARENTHESES and the hint comes up with them, because
    // the next thing wanted is an argument and the next thing shown should be
    // which one. A keyword (`points=`) gets nothing: it is already complete.
    if (!completion.endsWith(QLatin1Char('=')) && callable(completion) != nullptr) {
        insertPlainText(QStringLiteral("()"));
        moveCursor(QTextCursor::Left);
    }
    updateSignatureHint(contextAt());
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

    // CTRL+SPACE ASKS, wherever the cursor is. Every editor has this gesture and
    // it is the answer to "the popup is not up and I want it".
    if (event->key() == Qt::Key_Space && (event->modifiers() & Qt::ControlModifier) != 0) {
        offerCompletions(contextAt());
        return;
    }

    // ESCAPE PUTS BOTH AWAY. The popup takes it first (the guard above returns),
    // so this is the hint's Escape and not the popup's.
    if (event->key() == Qt::Key_Escape && hint_->isVisible()) {
        hint_->hide();
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

    const Context where = contextAt();
    updateSignatureHint(where);

    // WHEN TO OFFER, and the rule is the one an editor earns its keep by:
    //
    //   after a `.`      — always, even with nothing typed yet, because that is
    //                      the moment the user is asking "what is in here"
    //   inside a call    — always, because the answer is a short list of keywords
    //                      this command declares and nothing else will do
    //   a bare word      — from two characters, or the popup fights every `i`
    //                      in every `if`
    //
    // The old rule was "three characters and it must start with `cad`", which
    // offered nothing for `cad.` itself, nothing inside a call, and nothing for
    // any name the script defined.
    const bool after_dot   = !where.owner.isEmpty();
    const bool in_call     = !where.call.isEmpty() && callable(where.call) != nullptr;
    const bool long_enough = where.prefix.length() >= 2;

    if (!after_dot && !in_call && !long_enough) {
        completer_->popup()->hide();
        return;
    }
    offerCompletions(where);
}

void ScriptEditor::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    highlighter_->setTheme(mode);
    highlightCurrentLine();
    gutter_->update();
    hint_->update();
    completer_->popup()->update();
}

void ScriptEditor::focusOutEvent(QFocusEvent* event)
{
    // A HINT FLOATING OVER A WINDOW NOBODY IS TYPING IN is a hint in the way. It
    // is a child window, so it does not go away by itself.
    hint_->hide();
    QPlainTextEdit::focusOutEvent(event);
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
    prompt_->setApi(controller_.pythonApi());

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

QWidget* PythonConsole::promptHint() const
{
    return prompt_->signatureHint();
}

void PythonConsole::typeIntoPrompt(const QString& source)
{
    prompt_->setFocus(Qt::OtherFocusReason);
    for (const QChar c : source) {
        QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(c));
        QCoreApplication::sendEvent(prompt_, &press);
    }
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
