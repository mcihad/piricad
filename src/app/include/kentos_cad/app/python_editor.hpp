// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: writing Python, and running it a line at a time.
//
// THREE WIDGETS, ONE CAPABILITY. The editor and the console both end at
// `core.python`, which is a command like any other — so what a user can do here
// they can also do from the command line and from a script, and CLAUDE.md 5.15's
// "never a feature reachable only by mouse" holds by construction rather than by
// promise. Neither widget interprets Python; neither owns an interpreter.
//
// WHY AN EDITOR IN THE PROGRAM AT ALL, when every user has one. Because the API
// is projected at run time: `cad.__all__` is the truth about what this build can
// do, and only a window inside the program can complete against it. An editor
// outside gets the generated stub (`docs/python/kentos_cad.pyi`) and that is the
// right answer for a plugin; a five-line batch job is not worth leaving for.
#pragma once

#include "kentos_cad/app/python_api_info.hpp"
#include "kentos_cad/app/theme.hpp"

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QWidget>

class QCompleter;
class QStandardItemModel;
class QTextDocument;

namespace kentos::app {

/// The signature strip the editor floats under the cursor; defined in the .cpp
/// because it is painted, not assembled, and nothing outside constructs one.
class SignatureHint;

/// The shell's command client; declared rather than included, because only the
/// console's constructor needs the type.
class Controller;

/// Colours Python source.
///
/// A HIGHLIGHTER AND NOT A PARSER, and the distinction keeps this file honest:
/// it decides what a run of characters LOOKS like, never what it means. Python's
/// real grammar lives in CPython and this program has exactly one grammar of its
/// own (CLAUDE.md 5.11) — a second one here, even a partial one, would be a
/// second answer to "what is this text".
///
/// The one piece of state it carries is the triple-quoted string, which is the
/// only Python construct that spans blocks. `QSyntaxHighlighter` keeps that for
/// us: `setCurrentBlockState` says "still inside", and the next block asks.
class PythonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    /// Attaches to a document and colours it from then on.
    explicit PythonHighlighter(QTextDocument* document);

    /// The names this build actually exposes, so `cad.line` is tinted and
    /// `cad.lien` is not. Taken from the live registry rather than from a list
    /// here, which is the same rule the surface itself follows (5.10).
    void setApiNames(const QStringList& names);

    /// The keywords of every callable, so `points=` inside `cad.line(...)` is
    /// tinted as the argument it is rather than as an ordinary name.
    void setArgumentNames(const QStringList& names);

    void setTheme(ThemeMode mode);

protected:
    /// Colours one block. Qt calls it; nothing here does.
    void highlightBlock(const QString& text) override;

private:
    ThemeMode theme_{ThemeMode::Dark};
    QStringList api_;
    QStringList args_;
};

/// A Python source editor: line numbers, indentation, completion, one gutter.
class ScriptEditor : public QPlainTextEdit, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds an editor with its gutter, highlighter and completer. The API names
    /// arrive later, through `setApiNames`, because they come from a registry the
    /// widget must not reach into.
    explicit ScriptEditor(QWidget* parent = nullptr);

    /// The surface this build exposes: what completion offers, what the
    /// highlighter tints, and what the signature hint reads.
    void setApi(QVector<PythonCallable> api);

    /// Makes plain Enter SEND rather than open a line, which is what a console
    /// prompt wants and what a file editor must not do. Shift+Enter still opens a
    /// line either way, so a multi-line statement is typable at the prompt.
    void setSubmitOnEnter(bool on) { submitOnEnter_ = on; }

    /// Makes the gutter a PROMPT: `>>>` on the first line and `...` on the
    /// lines after it, which is what every Python REPL has shown since 1991 and
    /// what tells the user at a glance that this box runs what they type. Line
    /// numbers are for a file; a console line has no number worth reading.
    void setPromptGutter(bool on);

    /// Whether the statement being typed CONTINUES one already sent — a `for`
    /// line whose body is still coming. Then the first line shows `...` too.
    void setContinuing(bool on);

    /// Steps through what has been sent, for Up and Down at a console prompt.
    /// Empty when there is nothing to recall.
    void setHistory(const QStringList& entries) { history_ = entries; }

    /// What the popup is offering right now, for the probe that checks it.
    ///
    /// The COMPLETION MODEL and not the candidate list: what is asked is what the
    /// user can actually see, after the prefix has filtered it.
    QStringList completionsShown() const;

    /// Whether the signature strip is up.
    bool hintVisible() const;

    /// The signature strip, for a probe that photographs it. It is a window of
    /// its own, so a grab of the shell does not contain it.
    QWidget* signatureHint() const;

    /// The completion popup, for the same probe and for the same reason.
    QWidget* completionPopup() const;

    /// Where the line being typed, the popup and the hint are ON THE SCREEN, for
    /// the probe that checks that none of the three covers another. An empty
    /// rectangle for one that is not up.
    struct Floaters
    {
        QRect line;  ///< the cursor's line, across the editor
        QRect popup; ///< the completion list
        QRect hint;  ///< the signature strip
    };

    /// See `Floaters`.
    Floaters floaters() const;

    /// Paints the line-number gutter. Public because the gutter widget is a
    /// plain `QWidget` whose `paintEvent` forwards here — the shape Qt's own
    /// code-editor example uses, and the reason is that a gutter needs the
    /// editor's block geometry and has no business owning any of it.
    void paintGutter(QPaintEvent* event);

    /// How wide the gutter must be for the current line count.
    int gutterWidth() const;

    /// What the gutter shows beside `block`: its number in a file, `>>>` or
    /// `...` at a prompt (`setPromptGutter`).
    QString gutterText(int block) const;

    void applyTheme(ThemeMode mode) override;

signals:
    /// Enter, when `setSubmitOnEnter(true)`. The editor never runs anything.
    void submitRequested();

protected:
    /// Keeps the gutter beside the text when the widget changes size.
    void resizeEvent(QResizeEvent* event) override;

    /// Tab, Enter, Up and Down — the four keys a code editor owes its writer.
    void keyPressEvent(QKeyEvent* event) override;

    /// Puts the signature hint away when the editor stops being typed in.
    void focusOutEvent(QFocusEvent* event) override;

private:
    void updateGutterWidth();
    void highlightCurrentLine();

    /// Inserts the completion the popup is showing, replacing the word under the
    /// cursor rather than appending to it.
    void insertCompletion(const QString& completion);

    /// Where the cursor is, as far as completion is concerned.
    struct Context
    {
        /// What is being typed after the last `.`, or the whole bare word.
        QString prefix;

        /// What owns it: `cad`, `cad.doc`, `cad.viewport`, or empty for a name
        /// with no owner at all.
        QString owner;

        /// The callable whose parentheses the cursor is inside, empty when it is
        /// not inside any. This is what makes `points=` offerable and what the
        /// signature hint reads.
        QString call;

        /// Which argument of `call` the cursor is on, counting the commas at the
        /// call's own bracket depth. -1 when not in a call.
        int argumentIndex{-1};

        /// The keywords already written in this call, so completion does not
        /// offer a second `points=`.
        QStringList used;
    };

    /// Reads the source to the LEFT of the cursor and answers the three questions
    /// completion has: what is being typed, what owns it, and what call it is in.
    ///
    /// A READING, NOT A PARSE. It walks backwards over brackets and strings and
    /// stops at the first thing it understands. Python's real grammar is
    /// CPython's, and a second one here — even a partial one — would be a second
    /// answer to "what is this text" (CLAUDE.md 5.11).
    Context contextAt() const;

    /// Fills the popup for `where`, and hides it when there is nothing to offer.
    void offerCompletions(const Context& where);

    /// Shows or hides the signature hint for `where`.
    void updateSignatureHint(const Context& where);

    /// Puts the hint and the popup where they cover neither the line being typed
    /// nor each other: on the side of the line with room for both, the hint next
    /// to the line and the list beyond it. See the definition for why that order.
    void placeFloaters();

    /// The callable named `name`, or null.
    const PythonCallable* callable(const QString& name) const;

    /// The names the buffer itself defines: assignments, `def`, `class`, `for`
    /// targets and imports. Offered alongside the API, because half of what a
    /// person types is a name they wrote three lines up.
    QStringList localNames() const;

    QWidget* gutter_{nullptr};
    SignatureHint* hint_{nullptr};
    PythonHighlighter* highlighter_{nullptr};
    QCompleter* completer_{nullptr};
    class QStandardItemModel* words_{nullptr};
    QVector<PythonCallable> api_;
    ThemeMode theme_{ThemeMode::Dark};

    bool submitOnEnter_{false};
    bool promptGutter_{false}; ///< `>>>` and `...` instead of line numbers
    bool continuing_{false};   ///< the first line continues a statement: `...`
    QStringList history_;      ///< what a console prompt recalls with Up and Down
    int recall_{-1};           ///< where Up/Down is in `history_`, -1 when not recalling
};

/// The bottom panel: a transcript and a prompt, the command line's sibling.
///
/// IT IS A CLIENT, exactly as the command line is. Enter sends the buffered
/// source to `core.python` through the controller and the panel learns what
/// happened from the transcript the bus already writes — it never calls an
/// interpreter and never sees a `Document`.
class PythonConsole : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the panel over a controller, which outlives it. Completion comes
    /// from the registry through the controller, so a command added today is
    /// completable today (CLAUDE.md 5.10).
    explicit PythonConsole(Controller& controller, QWidget* parent = nullptr);

    /// Puts a line in the transcript. The window feeds it whatever the bus echoed
    /// while the console's own submission was running.
    void appendOutput(const QString& text);

    /// Focuses the prompt, for the menu action and the shortcut.
    void focusPrompt();

    /// Clears the prompt, types `source` and answers what completion offers.
    QStringList probeOffered(const QString& source);

    /// Whether the prompt's signature strip is up.
    bool promptHintVisible() const;

    /// The prompt's signature strip, for the probe.
    QWidget* promptHint() const;

    /// Types `source` into the prompt one KEY AT A TIME, without sending it.
    ///
    /// Real key events and not `setPlainText`, because what is being exercised is
    /// what happens BETWEEN the keys: completion fires on a keystroke and the
    /// signature hint follows the cursor. A probe that set the text would
    /// photograph a box with words in it and prove nothing.
    void typeIntoPrompt(const QString& source);

    /// Types `source` into the prompt and sends it, as if the user had.
    ///
    /// The SAME road a keystroke takes, so a probe photographs the panel a user
    /// would see rather than one the probe assembled.
    void runSource(const QString& source);

    void applyTheme(ThemeMode mode) override;

    /// The height the dock should open at: the prompt plus about a dozen lines of
    /// answer, which is what a person reads back after running something.
    ///
    /// A SIZE HINT AND NOT A `resizeDocks` CALL. Qt lays a dock area out from its
    /// widgets' hints, and a resize asked for before the layout has run is a
    /// request with nowhere to land — which is how this panel first opened as a
    /// 90 px slot with its transcript invisible.
    QSize sizeHint() const override { return {720, 260}; }

    /// The prompt's popup, hint and cursor line on the screen (`ScriptEditor::Floaters`).
    ScriptEditor::Floaters promptFloaters() const { return prompt_->floaters(); }

    /// The prompt's completion popup, for the probe that photographs it.
    QWidget* promptPopup() const { return prompt_->completionPopup(); }

    /// The prompt itself, for the probe that reads its gutter.
    ScriptEditor* prompt() const { return prompt_; }

private:
    /// Sends what is buffered, or keeps buffering when the source is unfinished.
    ///
    /// CONTINUATION IS DECIDED BY SHAPE, not by parsing: a line ending in `:` or
    /// an open bracket, or any line while the buffer is already open. That is
    /// what `>>>` and `...` have meant in every Python REPL since 1991, and it is
    /// a reading of the text rather than a second grammar.
    void submit();

    Controller& controller_;
    QPlainTextEdit* transcript_{nullptr};
    ScriptEditor* prompt_{nullptr};
    QStringList buffer_;  ///< the lines of an unfinished statement
    QStringList history_; ///< what has been sent, newest last
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
