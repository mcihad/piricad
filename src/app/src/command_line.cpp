// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/command_line.hpp"

#include "kentos_cad/app/controller.hpp"

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/colour.hpp"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPixmap>
#include <QStringListModel>

namespace kentos::app {

namespace {

constexpr int kSwatchPx = 12; ///< the square beside a colour word

/// The words in the field's completer, with a swatch beside each one that
/// names a colour.
///
/// RENK offers `kırmızı`, `mavi`, `siyah`; the list shows the colour next to
/// the word, so the choice is made by eye. Read with `command::parse_colour`,
/// the same reader the command uses, so the square is the colour the word
/// paints. Only while a prompt's choices are shown: a command name is never a
/// colour, and the inline completion that offers them draws no icons.
class WordModel : public QStringListModel
{
public:
    using QStringListModel::QStringListModel;

    void show_swatches(bool on)
    {
        swatches_ = on;
        any_      = false;
        if (!on) return;
        for (const QString& word : stringList())
            any_ = any_ || command::parse_colour(word.toStdString()).has_value();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DecorationRole && swatches_ && any_) {
            const QString word = QStringListModel::data(index, Qt::DisplayRole).toString();
            QPixmap square(kSwatchPx, kSwatchPx);
            square.fill(Qt::transparent);
            // A word that names no colour gets an empty square of the same size,
            // so every word in the list starts at the same column.
            if (const auto rgba = command::parse_colour(word.toStdString())) {
                QPainter p(&square);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
                p.setBrush(QColor::fromRgba(*rgba));
                p.drawRoundedRect(QRectF(0.5, 0.5, kSwatchPx - 1.0, kSwatchPx - 1.0), 2.0, 2.0);
            }
            return QIcon(square);
        }
        return QStringListModel::data(index, role);
    }

private:
    bool swatches_ = false;
    bool any_      = false;
};

} // namespace

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

    model_     = new WordModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::InlineCompletion);
    setCompleter(completer_);

    // A CLICK ON AN OFFERED WORD ANSWERS THE PROMPT. Read from the row that was
    // clicked rather than from the line, so it does not depend on whether the
    // completer has written the word in yet. The keyboard needs nothing: Enter
    // on a highlighted row completes it and reaches `returnPressed` as usual.
    connect(completer_->popup(), &QAbstractItemView::clicked, this, [this](const QModelIndex& row) {
        if (!choosing_ || !row.isValid()) return;
        // The list was clicked, so this program is the one in front: the line
        // is given the keyboard back before the answer goes, so the question
        // that follows can be typed into (see `offerChoices`).
        window()->activateWindow();
        setFocus(Qt::PopupFocusReason);
        setText(row.data().toString());
        submit();
    });

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

void CommandLine::offerChoices(const QStringList& words)
{
    if (words.isEmpty()) {
        if (choosing_) {
            choosing_               = false;
            QAbstractItemView* list = completer_->popup();
            // THE KEYBOARD COMES BACK HERE when the list goes. A window system
            // that made the list the active window — the offscreen one does —
            // activates nothing when it closes, and the next prompt's typing
            // went nowhere. Only when the list WAS the active window: with no
            // active window at all the program is in the background, and taking
            // the focus back would take it from whatever the user went to.
            const bool held = list->isVisible() && QApplication::activeWindow() == list;
            list->hide();
            if (held) {
                window()->activateWindow();
                setFocus(Qt::PopupFocusReason);
            }
            completer_->setCompletionMode(QCompleter::InlineCompletion);
            static_cast<WordModel*>(model_)->show_swatches(false);
        }
        refreshCompletions();
        return;
    }

    // IN THE COMMAND'S ORDER. RENK offers `katman` and then its colours in
    // palette order; sorted, the list opened on `beyaz`.
    choosing_ = true;
    model_->setStringList(words);
    static_cast<WordModel*>(model_)->show_swatches(true);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCompletionPrefix(QString());
    completer_->setMaxVisibleItems(12);

    // THE SHELL'S LIST, in the field's own face: the words are what would be
    // typed, so they are set in the type the line uses.
    QAbstractItemView* list = completer_->popup();
    list->setObjectName(QStringLiteral("commandChoices"));
    list->setFont(font());
    list->setIconSize(QSize(kSwatchPx, kSwatchPx));

    // AS WIDE AS ITS LONGEST WORD, under the words of the line rather than the
    // whole strip: a list the width of the window reads as a panel, and the
    // words it holds sat at its far left edge a screen away from the cursor.
    const QFontMetrics metrics(font());
    int widest = 0;
    for (const QString& word : words)
        widest = std::max(widest, metrics.horizontalAdvance(word));
    const int width = std::max(180, widest + kSwatchPx + 64);

    // SHOWN, not merely available. The point is that a user who does not know
    // the names can see them; a completer that waits to be typed into first has
    // not answered the question.
    completer_->complete(QRect(prefixWidth_, 0, width, height()));
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
    case Qt::Key_Backspace:
        // NOTHING TO ERASE HERE, SO THE RUN'S LAST POINT: a user who typed
        // `@10,0` and Enter has the focus in this line, and ⌫ is the key their
        // hand reaches for when the corner went wrong (TODOS C-02).
        if (text().isEmpty() && controller_.retractPoint()) return;
        break;
    default: break;
    }
    QLineEdit::keyPressEvent(event);
}

bool CommandLine::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride && text().isEmpty()) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)) {
            event->ignore(); ///< not ours: the window's GERİAL and YİNELE take it
            return false;
        }
    }
    return QLineEdit::event(event);
}

} // namespace kentos::app
