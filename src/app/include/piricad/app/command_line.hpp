// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the command line widget.
//
// piricad.md §3: AutoCAD's command line is forty years of refinement and the users
// are coming from it. It is a separate engineering job, not a text box. What is
// implemented here, and what is still owed, is listed in .claude/ui.md.
#pragma once

#include <QLineEdit>
#include <QStringList>

class QCompleter;
class QStringListModel;

namespace piricad::app {

class Controller;

class CommandLine : public QLineEdit
{
    Q_OBJECT

public:
    explicit CommandLine(Controller& controller, QWidget* parent = nullptr);

    void setPrompt(const QString& prompt);

signals:
    void submitted(const QString& line);

protected:
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
