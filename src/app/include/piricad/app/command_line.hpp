// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the command line widget.
//
// piricad.md §3: AutoCAD's command line is forty years of refinement and the users
// are coming from it. It is a separate engineering job, not a text box. What is
// implemented here, and what is still owed, is listed in .claude/ui.md.
#pragma once

#include "piricad/app/theme.hpp"

#include <QLineEdit>
#include <QStringList>

/// Qt widgets this header only holds pointers to.
class QCompleter;
class QStringListModel;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

class CommandLine : public QLineEdit, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds the line over a controller, which outlives it. Completion comes
    /// from the registry, so a new command is completable the moment it exists —
    /// there is no second list of names (CLAUDE.md 5.10).
    explicit CommandLine(Controller& controller, QWidget* parent = nullptr);

    /// Shows what the running command is waiting for. Empty when none is.
    void setPrompt(const QString& prompt);

    /// Offers `words` instead of the command names, for as long as a command is
    /// waiting for one of them.
    ///
    /// A prompt for a name — a block, a layer, a colour, a pattern — is
    /// unanswerable by a mouse, and the field's completer already knows how to
    /// offer a set. The command says which set (`command::Prompt::choices`), in
    /// the order it wants them read; a click on one ANSWERS the prompt, and
    /// typing narrows the list. An empty list puts the command names back, which
    /// is what the field offers when nothing is running.
    ///
    /// AS A LIST, NEVER INLINE. The inline completion the command names use
    /// wrote the first choice into the line, selected: the question — which is
    /// the placeholder of an EMPTY line — disappeared, and an answer nobody gave
    /// sat in front of Enter. RENK's colour prompt read "beyaz", and one Enter
    /// painted the parcel white.
    void offerChoices(const QStringList& words);

    /// True while the field is offering a command's choices rather than the
    /// command names.
    bool offeringChoices() const noexcept { return choosing_; }

    /// COMPOSES A LINE WITH THE SCENE (`.claude/ui.md` R48a): writes `fragment`
    /// and, while composing, takes each click on the canvas as text — the
    /// clicked coordinate, or after `nesne(` the clicked object's key — written
    /// at the cursor with the separator the grammar wants in front of it. After
    /// `clicks` clicks the line is submitted as if Enter were pressed; with none,
    /// Enter submits it. What is submitted is exactly what was typed would be:
    /// the tab that started it has no road of its own (CLAUDE.md 1.2).
    void beginCompose(const QString& fragment, int clicks);

    /// Whether the line is listening to the scene.
    bool composing() const noexcept { return composing_; }

    /// Whether the next click answers an OBJECT: `nesne(` just before the cursor.
    bool composeWantsObject() const;

    /// Writes one click's answer at the cursor. True when the line wants another
    /// click; false once it has submitted itself or stopped listening.
    bool composeWrite(const QString& value);

    /// Stops listening to the scene. The text stays, for the keyboard.
    void endCompose();

    /// Submits the line as Enter does — the `Nokta Girişi` tab's `Gönder`, for a
    /// hand on the mouse.
    void submitLine() { submit(); }

    void applyTheme(ThemeMode mode) override;

signals:
    /// Enter pressed with NOTHING typed.
    ///
    /// Every CAD reads that as "done / go ahead", and it is the gesture that ends
    /// an open-ended step. It matters here because focus lives on this widget
    /// almost all the time — a tool-column button is `NoFocus` — so a command
    /// waiting for the user to finish pointing gets its Enter here and nowhere
    /// else. Swallowing it, which is what `submit()` used to do, left every such
    /// command armed forever.
    void accepted();

    /// Emitted on Enter, with the raw line. The controller parses it — this
    /// widget never interprets a command, because the parser is shared and there
    /// is exactly one (5.11).
    void submitted(const QString& line);

    /// The line started composing and wants the scene's next click — an object
    /// when `object`. Emitted once; the window re-arms after each answer.
    void composeClickWanted(bool object);

    /// The line stopped listening to the scene.
    void composeEnded();

protected:
    /// Draws the permanent `Komut:` prefix `design.md` §7 puts at the left of the
    /// strip, then lets the line edit draw the text after it. The prefix is a
    /// PAINTED label rather than part of the text, because it must not be
    /// selectable, editable or submitted with what the user typed.
    void paintEvent(QPaintEvent* event) override;

    /// Handles history, completion and Esc. Esc cancels the RUNNING COMMAND
    /// rather than clearing the text, which is what a CAD user's hand expects,
    /// and ⌫ on an empty line takes the run's newest point back.
    void keyPressEvent(QKeyEvent* event) override;

    /// Lets Ctrl+Z and its redo reach the window when nothing is typed. The line
    /// edit claims both for its own text history, so with the focus here —
    /// which is almost always — Ctrl+Z put the last command's text back into the
    /// line instead of undoing the drawing.
    bool event(QEvent* event) override;

private:
    void submit();
    void refreshCompletions();
    void historyStep(int direction);

    Controller& controller_;
    QCompleter* completer_{nullptr};
    QStringListModel* model_{nullptr};
    QStringList history_;
    int history_pos_{-1};
    QString prompt_;
    bool choosing_{false};  ///< the list is a prompt's choices, not the command names
    bool composing_{false}; ///< the scene's clicks are written into the line
    int clicksLeft_{0};     ///< clicks before the line submits itself; 0: Enter does
    int prefixWidth_{0};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace piricad::app
