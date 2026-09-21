// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/command_palette.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/text.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

namespace kentos::app {
namespace {

// WIDER AND TALLER THAN IT WAS, because the summaries are sentences. At 560 px
// every one of them ran past the right edge and the list grew a HORIZONTAL
// scrollbar — a list of commands that had to be scrolled sideways to be read.
// A PAGE, NOT A DROPDOWN. The list on the left and what the selected command
// TAKES on the right: `YARDIM` used to answer by echoing ninety-eight lines into
// the transcript, and the answer to "what is there and how do I call it" is a
// page you can read, not a conversation you have to scroll back through.
constexpr int kWidth  = 980;
constexpr int kHeight = 560;
constexpr int kDetail = 340; ///< the right-hand pane
constexpr int kRadius = 7;

/// A heading carries its group's name here; a command row carries nothing.
constexpr int kGroupRole = Qt::UserRole + 1;

/// The command's abbreviations, drawn at the right end of its row.
constexpr int kShortsRole = Qt::UserRole + 2;

/// The one-line summary, drawn under the name.
constexpr int kSummaryRole = Qt::UserRole + 3;

constexpr int kRowHeight   = 40;
constexpr int kGroupHeight = 26;
constexpr int kPadX        = 12;

/// Draws one palette line: a group heading, or a command with its summary and
/// the abbreviations the prompt takes.
///
/// WHY A DELEGATE AND NOT A STRING. Every row used to be one string —
/// `"ÇİZGİ — İki veya daha fazla nokta arasında doğru parçaları çizer."` — set on
/// a plain item, so the name and the sentence describing it had the same weight,
/// the same colour and the same line. Ninety-eight of those is a wall, and the
/// list had no grouping to break it: the user's own words for it were that it
/// "stretches away downwards".
///
/// Three things are drawn instead, and each answers a different question: WHAT it
/// is called (the name, in the weight a reader scans), WHAT it does (the summary,
/// dim and elided so nothing ever reaches the right edge), and WHAT TO TYPE (the
/// abbreviations — `Ç · L` — which is the palette teaching the command line every
/// time it is opened, and the reason this program has abbreviations at all).
class PaletteRow : public QStyledItemDelegate
{
public:
    explicit PaletteRow(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setTheme(ThemeMode mode) { theme_ = mode; }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const int w = QStyledItemDelegate::sizeHint(option, index).width();
        return {w, index.data(kGroupRole).toString().isEmpty() ? kRowHeight : kGroupHeight};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);

        QFont small = option.font;
        small.setPixelSize(11);

        // ---- a group heading ------------------------------------------------
        const QString group = index.data(kGroupRole).toString();
        if (!group.isEmpty()) {
            p->fillRect(option.rect, t.bgHeader);
            p->setFont(small);
            p->setPen(t.textDim);
            p->drawText(option.rect.adjusted(kPadX, 0, -kPadX, -1),
                        Qt::AlignLeft | Qt::AlignVCenter, group);
            p->setPen(QPen(t.lineSoft, 1.0));
            p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
            p->restore();
            return;
        }

        // ---- the row's ground -----------------------------------------------
        const bool picked = (option.state & QStyle::State_Selected) != 0;
        if (picked) {
            p->fillRect(option.rect, t.accentWash);
            p->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                        t.accent);
        } else if ((option.state & QStyle::State_MouseOver) != 0) {
            p->fillRect(option.rect, t.hoverRow);
        }

        // ---- what to type, at the right end ---------------------------------
        //
        // RESERVED FIRST, so a long summary is elided rather than pushing the
        // abbreviations off the panel.
        const QString shorts = index.data(kShortsRole).toString();
        const QFontMetrics tiny(small);
        const int wide = shorts.isEmpty() ? kPadX : tiny.horizontalAdvance(shorts) + kPadX * 2;
        if (!shorts.isEmpty()) {
            p->setFont(small);
            p->setPen(picked ? t.accent : t.textFaint);
            p->drawText(QRect(option.rect.right() - wide, option.rect.top(), wide - kPadX,
                              option.rect.height()),
                        Qt::AlignRight | Qt::AlignVCenter, shorts);
        }

        const QRect words = option.rect.adjusted(kPadX, 4, -wide, -4);

        QFont named = option.font;
        named.setWeight(QFont::DemiBold);
        p->setFont(named);
        p->setPen(t.text);
        const QFontMetrics nm(named);
        p->drawText(
            QRect(words.left(), words.top(), words.width(), nm.height()),
            Qt::AlignLeft | Qt::AlignTop,
            nm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, words.width()));

        // THE SUMMARY IS A SECOND LINE, dim, and always elided. On one line with
        // the name it doubled the row's width and bought the list a sideways
        // scrollbar; under it, the name stays scannable and the sentence is
        // there for the reader who stops.
        p->setFont(small);
        p->setPen(t.textDim);
        p->drawText(
            QRect(words.left(), words.bottom() - tiny.height(), words.width(), tiny.height()),
            Qt::AlignLeft | Qt::AlignBottom,
            tiny.elidedText(index.data(kSummaryRole).toString(), Qt::ElideRight, words.width()));
        p->restore();
    }

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// The short forms a spec declares, as the row shows them.
///
/// `.names` is ordered by command.md R7: Turkish primary, ASCII-folded Turkish,
/// English, then the abbreviations. What a reader wants from this column is what
/// they can type INSTEAD, briefly — so two kinds are dropped.
///
/// THE ASCII FOLD IS NOT A SECOND NAME. `CIZGI` is `ÇİZGİ` spelt for a keyboard
/// without Turkish keys; showing it says nothing a reader did not already see on
/// the line above, and the search folds anyway, so nobody has to be told it
/// works. It is recognised by the registry's own folding rather than by a rule
/// about dotted letters (CLAUDE.md 5.6).
///
/// AND A FULL WORD IS NOT AN ABBREVIATION. Measured in characters, not bytes: at
/// bytes, `CIZGI` is shorter than `ÇİZGİ` because Turkish letters take two, and
/// the column filled up with the very spellings this function exists to drop.
QString short_names(const command::CommandSpec& spec)
{
    constexpr qsizetype kShort = 4;
    const std::string primary  = core::turkish_fold_key(spec.names.front());
    QStringList out;
    for (const std::string& alias : spec.names) {
        if (core::turkish_fold_key(alias) == primary) continue;
        const QString shown = QString::fromStdString(alias);
        if (shown.size() > kShort || out.contains(shown)) continue;
        out << shown;
    }
    return out.join(QStringLiteral(" · "));
}

/// The parameters a command takes, as the detail pane shows them.
///
/// Built once at construction rather than on every selection: a spec never
/// changes while the program runs, and a pane that rebuilt this on each arrow
/// key would do it ninety-eight times to show one.
QString parameter_table(const command::CommandSpec& spec)
{
    if (spec.params.empty()) return CommandPalette::tr("Parametre almaz.");

    QStringList out;
    for (const command::Param& p : spec.params) {
        // ARITY IN WORDS. `0..1` and `1..4294967295` are the numbers the struct
        // holds, not the thing a reader wants to know, which is whether they
        // have to write it and whether they may write it more than once.
        QString how;
        if (p.arity.min == 0 && p.arity.max == 1)
            how = CommandPalette::tr("isteğe bağlı");
        else if (p.arity.min == 1 && p.arity.max == 1)
            how = CommandPalette::tr("gerekli");
        else if (p.arity.max == 0xFFFFFFFFu)
            how = CommandPalette::tr("en az %1, yinelenir").arg(p.arity.min);
        else
            how = CommandPalette::tr("%1–%2 kez").arg(p.arity.min).arg(p.arity.max);

        QString line = QStringLiteral("<b>%1</b> &nbsp;<i>%2</i> · %3")
                           .arg(QString::fromStdString(p.name),
                                QString::fromUtf8(command::param_kind_label(p.kind)), how);
        if (!p.unit.empty())
            line += QStringLiteral(" · %1").arg(QString::fromStdString(p.unit).toHtmlEscaped());
        if (p.bounded) line += QStringLiteral(" · %1…%2").arg(p.low).arg(p.high);
        if (!p.choices.empty()) {
            QStringList words;
            for (const std::string& word : p.choices)
                words << QString::fromStdString(word);
            line += QStringLiteral("<br>&nbsp;&nbsp;%1").arg(words.join(QStringLiteral(" | ")));
        }
        if (!p.help.empty())
            line += QStringLiteral("<br>&nbsp;&nbsp;%1")
                        .arg(QString::fromStdString(p.help).toHtmlEscaped());
        out << line;
    }
    return out.join(QStringLiteral("<br><br>"));
}

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
    query_->setPlaceholderText(tr("Komut ara — ad, kısaltma ya da ne yaptığı"));
    query_->installEventFilter(this);
    column->addWidget(query_);

    auto* body    = new QWidget(this);
    auto* bodyRow = new QHBoxLayout(body);
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(0);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("paletteList"));
    list_->setFrameShape(QFrame::NoFrame);
    list_->setMouseTracking(true);
    // NEITHER UNIFORM NOR SIDEWAYS. A heading is shorter than a command row, so
    // the sizes differ by design; and nothing in this list may ever need a
    // horizontal scrollbar, because the delegate elides instead.
    list_->setUniformItemSizes(false);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rows_delegate_ = new PaletteRow(list_);
    list_->setItemDelegate(rows_delegate_);
    bodyRow->addWidget(list_, 1);

    // ---- the right-hand pane -------------------------------------------------
    auto* pane = new QWidget(body);
    pane->setFixedWidth(kDetail);
    auto* column2 = new QVBoxLayout(pane);
    column2->setContentsMargins(kPadX, 10, kPadX, 10);
    column2->setSpacing(4);

    detailName_  = new QLabel(pane);
    QFont bigger = detailName_->font();
    bigger.setPixelSize(15);
    bigger.setWeight(QFont::DemiBold);
    detailName_->setFont(bigger);
    detailName_->setWordWrap(true);
    column2->addWidget(detailName_);

    detailMeta_ = new QLabel(pane);
    detailMeta_->setObjectName(QStringLiteral("formHelp"));
    detailMeta_->setWordWrap(true);
    column2->addWidget(detailMeta_);
    column2->addSpacing(6);

    detailBody_ = new QLabel(pane);
    detailBody_->setTextFormat(Qt::RichText);
    detailBody_->setWordWrap(true);
    detailBody_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    detailBody_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* scroll = new QScrollArea(pane);
    scroll->setWidget(detailBody_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    column2->addWidget(scroll, 1);

    bodyRow->addWidget(pane);
    column->addWidget(body, 1);

    footer_ = new QLabel(this);
    footer_->setObjectName(QStringLiteral("formHelp"));
    footer_->setContentsMargins(kPadX, 4, kPadX, 6);
    column->addWidget(footer_);

    // Every command the registry knows, folded once at construction. The fold is
    // the parser's own (CLAUDE.md 5.6), so searching for `cizgi` finds `ÇİZGİ`
    // exactly as typing it at the prompt would.
    for (const command::CommandSpec& spec : registry_.all()) {
        if (spec.names.empty()) continue;

        Row row;
        row.name     = QString::fromStdString(spec.names.front());
        row.summary  = QString::fromStdString(spec.summary);
        row.id       = QString::fromStdString(spec.id);
        row.shorts   = short_names(spec);
        row.category = static_cast<int>(spec.category);
        row.group    = QString::fromUtf8(command::category_name(spec.category));
        row.detail   = parameter_table(spec);
        {
            QStringList all;
            for (const std::string& alias : spec.names)
                all << QString::fromStdString(alias);
            row.names = all.join(QStringLiteral(" · "));
        }
        for (const std::string& alias : spec.names)
            row.key += core::turkish_fold_key(alias) + '\x1f';
        row.key += core::turkish_fold_key(spec.id) + '\x1f';
        row.key += core::turkish_fold_key(spec.summary);
        rows_.push_back(std::move(row));
    }

    // GROUPED, AND THE ORDER IS THE CATEGORY'S OWN. Within a group the commands
    // keep the order the registry declared them in, which is the order the docs
    // and the reference already use — a palette that sorted them alphabetically
    // would be a third order for one list.
    std::stable_sort(rows_.begin(), rows_.end(),
                     [](const Row& a, const Row& b) { return a.category < b.category; });

    connect(query_, &QLineEdit::textChanged, this, &CommandPalette::refilter);
    connect(list_, &QListWidget::itemActivated, this, [this] { accept(); });
    connect(list_, &QListWidget::currentItemChanged, this, [this] { showDetail(); });
    refilter();
}

void CommandPalette::reveal(const QString& focus_on)
{
    query_->clear();
    refilter();

    // OPENED ON A COMMAND WHEN ONE WAS NAMED. `YARDIM komut=ÇİZGİ` asks about
    // one command, so the page opens on it rather than on the first row and the
    // reader's first look is the answer.
    if (!focus_on.isEmpty()) {
        const std::string wanted = core::turkish_fold_key(focus_on.toStdString());
        for (int i = 0; i < list_->count(); ++i) {
            const QVariant carried = list_->item(i)->data(Qt::UserRole);
            if (!carried.isValid()) continue;
            if (core::turkish_fold_key(carried.toString().toStdString()) != wanted) continue;
            list_->setCurrentRow(i);
            list_->scrollToItem(list_->item(i), QAbstractItemView::PositionAtCenter);
            break;
        }
    }

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

    QString open;
    int shown = 0;
    for (const Row& row : rows_) {
        if (!needle.empty() && row.key.find(needle) == std::string::npos) continue;

        // The heading is written once, where its run starts. A fact that repeats
        // down a run of rows belongs to the run, not to the row.
        if (row.group != open) {
            open       = row.group;
            auto* head = new QListWidgetItem(list_);
            head->setData(kGroupRole, open);
            // NOT SELECTABLE AND NOT A TAB STOP: a heading is not a command, and
            // arrowing down the list must not stop on one.
            head->setFlags(Qt::NoItemFlags);
        }

        auto* item = new QListWidgetItem(row.name, list_);
        item->setData(Qt::UserRole, row.name);
        item->setData(kShortsRole, row.shorts);
        item->setData(kSummaryRole, row.summary);
        item->setToolTip(row.summary.isEmpty() ? row.id
                                               : row.id + QStringLiteral("\n") + row.summary);
        ++shown;
    }

    // WHAT IS ON SCREEN, AND WHAT TO DO WHEN NOTHING IS. An empty result used to
    // be an empty box: the one moment the palette has to say something, and it
    // said nothing at all.
    if (shown == 0)
        footer_->setText(tr("Eşleşen komut yok. Aramayı kısaltın ya da temizleyin — "
                            "arama adı, kısaltmayı ve ne yaptığını birlikte tarar."));
    else if (needle.empty())
        footer_->setText(
            tr("%1 komut. Yazarak süzün; ↑ ↓ ile gezin, Enter komut satırına yazar.").arg(shown));
    else
        footer_->setText(tr("%1 / %2 komut eşleşti.").arg(shown).arg(rows_.size()));

    selectFirstCommand();
    showDetail();
}

void CommandPalette::selectFirstCommand()
{
    // PAST THE HEADING. Row zero is a group heading whenever anything matched,
    // and a heading cannot be selected — so setting row zero left the palette
    // with no selection and Enter with nothing to run.
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).isValid()) {
            list_->setCurrentRow(i);
            return;
        }
}

void CommandPalette::showDetail()
{
    QListWidgetItem* item  = list_->currentItem();
    const QVariant carried = item ? item->data(Qt::UserRole) : QVariant();
    if (!carried.isValid()) {
        detailName_->clear();
        detailMeta_->clear();
        detailBody_->clear();
        return;
    }

    const QString name = carried.toString();
    for (const Row& row : rows_) {
        if (row.name != name) continue;
        detailName_->setText(row.name);
        // WHAT IT IS AND WHAT ELSE IT ANSWERS TO. The id is what a script and an
        // agent write; the aliases are what a hand may type. Both belong here
        // and neither belongs on a list row, which has one line to be scanned.
        detailMeta_->setText(row.group + QStringLiteral("  ·  ") + row.id + QStringLiteral("\n") +
                             row.names);
        detailBody_->setText(
            row.summary.isEmpty()
                ? row.detail
                : QStringLiteral("%1<br><br>%2").arg(row.summary.toHtmlEscaped(), row.detail));
        return;
    }
}

CommandPalette::Shown CommandPalette::shown() const
{
    Shown out;
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).isValid())
            ++out.commands;
        else
            ++out.headings;

    out.scroll_max = list_->verticalScrollBar()->maximum();
    out.height     = height();
    if (QListWidgetItem* at = list_->currentItem(); at != nullptr)
        out.selected = at->data(Qt::UserRole).toString();
    out.detail = detailName_->text();
    return out;
}

void CommandPalette::accept()
{
    QListWidgetItem* item = list_->currentItem();
    if (!item || !item->data(Qt::UserRole).isValid()) return;

    hide();
    emit chosen(item->data(Qt::UserRole).toString());
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    // The arrow keys steer the list while the caret stays in the field, which is
    // what every palette does and what a user reaches for without being told.
    // Qt skips the headings on its own: they carry no flags, so they are not
    // selectable and arrow navigation passes over them.
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
    // THE DELEGATE TOO. It is not a widget, so the walk over the children never
    // reaches it and it would keep the tokens it was built with — which are the
    // dark ones, drawn on a light panel.
    if (rows_delegate_ != nullptr) static_cast<PaletteRow*>(rows_delegate_)->setTheme(mode);
    if (list_ != nullptr) list_->viewport()->update();
    applyThemeToChildren(this, mode);
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
