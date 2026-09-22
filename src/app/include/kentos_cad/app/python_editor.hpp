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

#include "kentos_cad/app/theme.hpp"

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QWidget>

class QCompleter;
class QStringListModel;
class QTextDocument;

namespace kentos::app {

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

    void setTheme(ThemeMode mode);

protected:
    /// Colours one block. Qt calls it; nothing here does.
    void highlightBlock(const QString& text) override;

private:
    ThemeMode theme_{ThemeMode::Dark};
    QStringList api_;
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

    /// The names offered by completion and tinted by the highlighter.
    void setApiNames(const QStringList& names);

    /// Makes plain Enter SEND rather than open a line, which is what a console
    /// prompt wants and what a file editor must not do. Shift+Enter still opens a
    /// line either way, so a multi-line statement is typable at the prompt.
    void setSubmitOnEnter(bool on) { submitOnEnter_ = on; }

    /// Steps through what has been sent, for Up and Down at a console prompt.
    /// Empty when there is nothing to recall.
    void setHistory(const QStringList& entries) { history_ = entries; }

    /// Paints the line-number gutter. Public because the gutter widget is a
    /// plain `QWidget` whose `paintEvent` forwards here — the shape Qt's own
    /// code-editor example uses, and the reason is that a gutter needs the
    /// editor's block geometry and has no business owning any of it.
    void paintGutter(QPaintEvent* event);

    /// How wide the gutter must be for the current line count.
    int gutterWidth() const;

    void applyTheme(ThemeMode mode) override;

signals:
    /// Enter, when `setSubmitOnEnter(true)`. The editor never runs anything.
    void submitRequested();

protected:
    /// Keeps the gutter beside the text when the widget changes size.
    void resizeEvent(QResizeEvent* event) override;

    /// Tab, Enter, Up and Down — the four keys a code editor owes its writer.
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateGutterWidth();
    void highlightCurrentLine();

    /// Inserts the completion the popup is showing, replacing the word under the
    /// cursor rather than appending to it.
    void insertCompletion(const QString& completion);

    /// The word being typed, for the completer. Stops at a `.` only when the dot
    /// is not part of `cad.` — completing `cad.li` must offer `cad.line`.
    QString wordUnderCursor() const;

    QWidget* gutter_{nullptr};
    PythonHighlighter* highlighter_{nullptr};
    QCompleter* completer_{nullptr};
    QStringListModel* words_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};

    bool submitOnEnter_{false};
    QStringList history_; ///< what a console prompt recalls with Up and Down
    int recall_{-1};      ///< where Up/Down is in `history_`, -1 when not recalling
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
