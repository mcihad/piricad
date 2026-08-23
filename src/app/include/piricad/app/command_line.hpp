// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the command line widget.
//
// piricad.md §3: AutoCAD's command line is forty years of refinement and the users
// are coming from it. It is a separate engineering job, not a text box. What is
// implemented here, and what is still owed, is listed in .claude/ui.md.
#pragma once

#include <QLineEdit>
#include <QStringList>

/// Qt widgets this header only holds pointers to.
class QCompleter;
class QStringListModel;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

class CommandLine : public QLineEdit
{
    Q_OBJECT

public:
    /// Builds the line over a controller, which outlives it. Completion comes
    /// from the registry, so a new command is completable the moment it exists —
    /// there is no second list of names (CLAUDE.md 5.10).
    explicit CommandLine(Controller& controller, QWidget* parent = nullptr);

    /// Shows what the running command is waiting for. Empty when none is.
    void setPrompt(const QString& prompt);

signals:
    /// Emitted on Enter, with the raw line. The controller parses it — this
    /// widget never interprets a command, because the parser is shared and there
    /// is exactly one (5.11).
    void submitted(const QString& line);

protected:
    /// Handles history, completion and Esc. Esc cancels the RUNNING COMMAND
    /// rather than clearing the text, which is what a CAD user's hand expects.
    void keyPressEvent(QKeyEvent* event) override;

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
};

} // namespace piricad::app
