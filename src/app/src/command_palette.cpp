// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/command_palette.hpp"

#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/text.hpp"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kWidth  = 560;
constexpr int kHeight = 420;
constexpr int kRadius = 7;

} // namespace

CommandPalette::CommandPalette(const command::Registry& registry, QWidget* parent)
    : QWidget(parent), registry_(registry)
{
    setObjectName(QStringLiteral("commandPalette"));
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(kWidth, kHeight);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(1, 1, 1, 1);
    column->setSpacing(0);

    query_ = new QLineEdit(this);
    query_->setObjectName(QStringLiteral("paletteQuery"));
    query_->setPlaceholderText(tr("Komut ara…"));
    query_->installEventFilter(this);
    column->addWidget(query_);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("paletteList"));
    list_->setUniformItemSizes(true);
    list_->setFrameShape(QFrame::NoFrame);
    column->addWidget(list_, 1);

    // Every command the registry knows, folded once at construction. The fold is
    // the parser's own (CLAUDE.md 5.6), so searching for `cizgi` finds `ÇİZGİ`
    // exactly as typing it at the prompt would.
    for (const command::CommandSpec& spec : registry_.all()) {
        if (spec.names.empty()) continue;

        Row row;
        row.name    = QString::fromStdString(spec.names.front());
        row.summary = QString::fromStdString(spec.summary);
        row.id      = QString::fromStdString(spec.id);
        for (const std::string& alias : spec.names)
            row.key += core::turkish_fold_key(alias) + '\x1f';
        row.key += core::turkish_fold_key(spec.id) + '\x1f';
        row.key += core::turkish_fold_key(spec.summary);
        rows_.push_back(std::move(row));
    }

    connect(query_, &QLineEdit::textChanged, this, &CommandPalette::refilter);
    connect(list_, &QListWidget::itemActivated, this, [this] { accept(); });
    refilter();
}

void CommandPalette::reveal()
{
    query_->clear();
    refilter();

    QWidget* over = parentWidget() ? parentWidget()->window() : nullptr;
    if (over) {
        const QPoint centre = over->mapToGlobal(over->rect().center());
        move(centre.x() - width() / 2, centre.y() - height() / 2 - 60);
    }
    show();
    query_->setFocus(Qt::ShortcutFocusReason);
}

void CommandPalette::refilter()
{
    const std::string needle = core::turkish_fold_key(query_->text().toStdString());

    list_->clear();
    for (const Row& row : rows_) {
        if (!needle.empty() && row.key.find(needle) == std::string::npos) continue;

        auto* item = new QListWidgetItem(
            row.summary.isEmpty() ? row.name
                                  : QStringLiteral("%1 — %2").arg(row.name, row.summary));
        item->setData(Qt::UserRole, row.name);
        list_->addItem(item);
    }
    if (list_->count() > 0) list_->setCurrentRow(0);
}

void CommandPalette::accept()
{
    QListWidgetItem* item = list_->currentItem();
    if (!item) return;

    hide();
    emit chosen(item->data(Qt::UserRole).toString());
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    // The arrow keys steer the list while the caret stays in the field, which is
    // what every palette does and what a user reaches for without being told.
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp: QCoreApplication::sendEvent(list_, key); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter: accept(); return true;
        default: break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CommandPalette::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void CommandPalette::paintEvent(QPaintEvent*)
{
    const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(t.bgPanel);
    p.setPen(QPen(t.border, 1.0));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
}

} // namespace kentos::app
