// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/command_line.hpp"

#include "kentos_cad/app/controller.hpp"

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <QCompleter>
#include <QKeyEvent>
#include <QPainter>
#include <QStringListModel>

namespace kentos::app {

CommandLine::CommandLine(Controller& controller, QWidget* parent)
    : QLineEdit(parent), controller_(controller)
{
    setObjectName(QStringLiteral("commandLine"));
    setPlaceholderText(tr("Komut girin — ÇİZGİ, KATMAN, YARDIM …"));

    // No clear button: the reference's strip carries a prompt and a caret and
    // nothing else, and Esc already cancels — the running command, which is what
    // a CAD user's hand expects, rather than the text.
    setFrame(false);

    QFont face(QStringLiteral("IBM Plex Mono"));
    face.setPixelSize(12);
    face.setStyleHint(QFont::Monospace);
    setFont(face);

    prefixWidth_ = QFontMetrics(face).horizontalAdvance(tr("Komut:")) + 8;
    setTextMargins(prefixWidth_, 0, 0, 0);

    model_     = new QStringListModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::InlineCompletion);
    setCompleter(completer_);

    refreshCompletions();

    connect(this, &QLineEdit::returnPressed, this, &CommandLine::submit);
}

void CommandLine::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void CommandLine::paintEvent(QPaintEvent* event)
{
    QLineEdit::paintEvent(event);

    QPainter p(this);
    p.setFont(font());
    p.setPen((theme_ == ThemeMode::Dark ? darkTokens() : lightTokens()).textDim);
    p.drawText(QRect(12, 0, prefixWidth_, height()), Qt::AlignVCenter | Qt::AlignLeft,
               tr("Komut:"));
}

void CommandLine::refreshCompletions()
{
    // Completions come from the command registry — the single source of truth.
    // There is no second command list anywhere (kentoscad.md §2.3).
    QStringList names;
    for (const auto& spec : controller_.registry().all())
        for (const auto& n : spec.names)
            names << QString::fromStdString(n);

    names.sort();
    model_->setStringList(names);
}

void CommandLine::setPrompt(const QString& prompt)
{
    prompt_ = prompt;
    setPlaceholderText(prompt.isEmpty() ? tr("Komut girin — ÇİZGİ, KATMAN, YARDIM …")
                                        : prompt + QStringLiteral(":"));
}

void CommandLine::submit()
{
    const QString line = text().trimmed();
    if (line.isEmpty()) {
        emit accepted();
        return;
    }

    history_.removeAll(line);
    history_.append(line);
    history_pos_ = static_cast<int>(history_.size());

    clear();
    emit submitted(line);
}

void CommandLine::historyStep(int direction)
{
    if (history_.isEmpty()) return;

    history_pos_ = qBound(0, history_pos_ + direction, static_cast<int>(history_.size()));
    setText(history_pos_ < history_.size() ? history_.at(history_pos_) : QString());
}

void CommandLine::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up: historyStep(-1); return;
    case Qt::Key_Down: historyStep(+1); return;
    case Qt::Key_Escape:
        if (text().isEmpty())
            controller_.cancelInteractive();
        else
            clear();
        return;
    default: break;
    }
    QLineEdit::keyPressEvent(event);
}

} // namespace kentos::app
