// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/command_palette.hpp"

#include "piricad/app/command_usage.hpp"
#include "piricad/app/icons.hpp"
#include "piricad/app/tokens.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/text.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace piricad::app {
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

/// Whether the command is starred; drawn as the star in the row's left gutter.
constexpr int kFavouriteRole = Qt::UserRole + 4;

/// The left gutter every command row keeps for its star, starred or not, so the names
/// stay in one column.
constexpr int kStarGutter = 26;

constexpr int kRowHeight   = 40;
constexpr int kGroupHeight = 26;
constexpr int kPadX        = 12;

/// A five-pointed star in `box`, outlined or filled.
void draw_star(QPainter* p, const QRectF& box, bool filled, const QColor& colour)
{
    constexpr double kPi = 3.14159265358979323846;
    const QPointF c      = box.center();
    const double outer   = std::min(box.width(), box.height()) / 2.0;
    const double inner   = outer * 0.42;
    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        const double angle  = -kPi / 2.0 + i * kPi / 5.0;
        const double radius = (i % 2 == 0) ? outer : inner;
        const QPointF point(c.x() + radius * std::cos(angle), c.y() + radius * std::sin(angle));
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    path.closeSubpath();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (filled) {
        p->setPen(Qt::NoPen);
        p->setBrush(colour);
    } else {
        p->setPen(QPen(colour, 1.2));
        p->setBrush(Qt::NoBrush);
    }
    p->drawPath(path);
}

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

        // THE STAR GUTTER. Starred: a filled star in the accent. Not starred: an outline
        // that shows only under the cursor or on the selected row, so a list of
        // ninety-eight commands is not ninety-eight faint stars, and the way to star
        // one is still where the eye already is.
        const bool starred = index.data(kFavouriteRole).toBool();
        if (starred || picked || (option.state & QStyle::State_MouseOver) != 0) {
            const QRectF box(option.rect.left() + kPadX - 2, option.rect.center().y() - 6.0, 12.0,
                             12.0);
            draw_star(p, box, starred, starred ? t.accent : t.textFaint);
            p->setRenderHint(QPainter::Antialiasing, false);
        }

        const QRect words = option.rect.adjusted(kPadX + kStarGutter - 8, 4, -wide, -4);

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

CommandPalette::CommandPalette(const command::Registry& registry, CommandUsage* usage,
                               QWidget* parent)
    : QWidget(parent), registry_(registry), usage_(usage)
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
    // The star is clicked where it is drawn, in the list's own viewport.
    list_->viewport()->installEventFilter(this);
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
        {
            QStringList all;
            for (const command::KnownName& known : spec.known_as)
                all << tr("%1 adı: %2 — aramada bulunur, komut satırında bu komutu başlatmaz")
                           .arg(QString::fromStdString(known.program),
                                QString::fromStdString(known.name));
            row.known = all.join(QLatin1Char('\n'));
        }
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
    const std::string typed = query_->text().trimmed().toStdString();

    list_->clear();
    topRows_ = 0;
    topEnd_  = 0;

    int shown = 0;
    if (typed.empty()) {
        // WHAT A PERSON REACHES FOR, FIRST (TODOS U-01): the commands they starred,
        // then the ones they ran last that are not starred — a command is listed once
        // in this top part, and again under its category below, because the list
        // underneath stays the whole reference and a palette that hid what it had
        // already shown would have a hole in it.
        const auto section = [this](const QString& title, const std::vector<std::string>& ids,
                                    const std::vector<std::string>& skip) {
            std::vector<const Row*> found;
            for (const std::string& id : ids) {
                if (std::ranges::find(skip, id) != skip.end()) continue;
                const auto at = std::ranges::find_if(
                    rows_, [&id](const Row& r) { return r.id.toStdString() == id; });
                if (at != rows_.end()) found.push_back(&*at);
            }
            if (found.empty()) return;
            auto* head = new QListWidgetItem(list_);
            head->setData(kGroupRole, title);
            head->setFlags(Qt::NoItemFlags);
            for (const Row* row : found) {
                addRow(*row, row->summary);
                ++topRows_;
            }
        };
        if (usage_ != nullptr) {
            section(tr("Favoriler"), usage_->favourites(), {});
            section(tr("Son kullanılanlar"), usage_->recents(), usage_->favourites());
        }
        topEnd_ = list_->count();

        // BROWSING: every command under its category, in the order the
        // registry declared them.
        QString open;
        for (const Row& row : rows_) {
            // The heading is written once, where its run starts. A fact that
            // repeats down a run of rows belongs to the run, not to the row.
            if (row.group != open) {
                open       = row.group;
                auto* head = new QListWidgetItem(list_);
                head->setData(kGroupRole, open);
                // NOT SELECTABLE AND NOT A TAB STOP: a heading is not a command,
                // and arrowing down the list must not stop on one.
                head->setFlags(Qt::NoItemFlags);
            }
            addRow(row, row.summary);
            ++shown;
        }
    } else {
        // SEARCHING: the answers in the order they answer, and no headings —
        // a group heading over a ranked list would put TAŞI above KAYDIR for
        // `kaydır` because Düzenleme comes before Görünüm. The ranking is the
        // command layer's (`command::search_match`), where it is tested; ties
        // keep the browsing order, category first.
        struct Hit
        {
            const Row* row;
            command::SearchMatch match;
        };

        std::vector<Hit> hits;
        for (const Row& row : rows_) {
            const command::CommandSpec* spec = registry_.by_id(row.id.toStdString());
            if (spec == nullptr) continue;
            const command::SearchMatch match = command::search_query(*spec, typed);
            if (match.tier != command::SearchMatch::kNone) hits.push_back({&row, match});
        }
        std::stable_sort(hits.begin(), hits.end(),
                         [](const Hit& a, const Hit& b) { return a.match.tier < b.match.tier; });
        for (const Hit& hit : hits) {
            // FOUND BY ANOTHER PROGRAM'S WORD, AND THE ROW SAYS WHICH: the name on
            // it is not the word that was typed, and without this line a Netcad
            // hand who typed `kutu` would see DİKDÖRTGEN and not know why.
            const QString summary =
                hit.match.known == nullptr
                    ? hit.row->summary
                    : tr("%1 adı: %2 · %3")
                          .arg(QString::fromStdString(hit.match.known->program),
                               QString::fromStdString(hit.match.known->name), hit.row->summary);
            addRow(*hit.row, summary);
            ++shown;
        }
    }

    // WHAT IS ON SCREEN, AND WHAT TO DO WHEN NOTHING IS. An empty result used to
    // be an empty box: the one moment the palette has to say something, and it
    // said nothing at all.
    if (shown == 0)
        footer_->setText(
            tr("Eşleşen komut yok. Başka sözcüklerle deneyin ya da aramayı temizleyin — "
               "arama adı, kısaltmayı ve ne yaptığını birlikte tarar."));
    else if (typed.empty())
        footer_->setText(tr("%1 komut. Yazarak süzün ya da ne yapmak istediğinizi yazın "
                            "(“köşeyi yuvarla”); ↑ ↓ gezin, Enter komut satırına yazar; "
                            "Ctrl+D favoriye ekler, Alt+↑ ↓ favoriyi taşır.")
                             .arg(rows_.size()));
    else
        footer_->setText(
            tr("%1 / %2 komut eşleşti; en iyi eşleşen üstte.").arg(shown).arg(rows_.size()));

    selectFirstCommand();
    showDetail();
}

void CommandPalette::addRow(const Row& row, const QString& summary)
{
    auto* item = new QListWidgetItem(row.name, list_);
    item->setData(Qt::UserRole, row.name);
    item->setData(kShortsRole, row.shorts);
    item->setData(kSummaryRole, summary);
    item->setData(kFavouriteRole, usage_ != nullptr && usage_->isFavourite(row.id.toStdString()));
    item->setToolTip(summary.isEmpty() ? row.id : row.id + QStringLiteral("\n") + summary);
}

void CommandPalette::selectFirstCommand()
{
    // PAST THE HEADING. Row zero is a group heading whenever anything matched,
    // and a heading cannot be selected — so setting row zero left the palette
    // with no selection and Enter with nothing to run.
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).isValid()) {
            list_->setCurrentRow(i);
            // BACK TO THE TOP. The list scrolls a row at a time, and bringing row one into
            // view scrolled the heading above it out of it: the palette opened with
            // "Favoriler" and "Son kullanılanlar" — the whole point of opening on them —
            // hidden over the first row.
            list_->scrollToTop();
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
                             row.names +
                             (row.known.isEmpty() ? QString() : QStringLiteral("\n") + row.known));
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

    out.top        = topRows_;
    out.scroll_max = list_->verticalScrollBar()->maximum();
    out.height     = height();
    if (QListWidgetItem* at = list_->currentItem(); at != nullptr)
        out.selected = at->data(Qt::UserRole).toString();
    for (int i = 0; i < list_->count() && out.first.size() < 5; ++i)
        if (const QVariant carried = list_->item(i)->data(Qt::UserRole); carried.isValid())
            out.first << carried.toString();
    out.detail = detailName_->text();
    for (int i = 0; i < list_->count(); ++i) {
        const QListWidgetItem* item = list_->item(i);
        if (const QString head = item->data(kGroupRole).toString(); !head.isEmpty())
            out.headingNames << head;
        else if (item->data(kFavouriteRole).toBool())
            out.starred << item->data(Qt::UserRole).toString();
    }
    return out;
}

void CommandPalette::accept()
{
    QListWidgetItem* item = list_->currentItem();
    if (!item || !item->data(Qt::UserRole).isValid()) return;

    hide();
    emit chosen(item->data(Qt::UserRole).toString());
}

void CommandPalette::toggleFavourite()
{
    QListWidgetItem* item = list_->currentItem();
    if (usage_ == nullptr || item == nullptr || !item->data(Qt::UserRole).isValid()) return;

    const QString name = item->data(Qt::UserRole).toString();
    const auto row = std::ranges::find_if(rows_, [&name](const Row& r) { return r.name == name; });
    if (row == rows_.end()) return;
    const int scrolled = list_->verticalScrollBar()->value();

    (void)usage_->toggleFavourite(row->id.toStdString());

    // Redrawn with the same query, and the cursor put back on the command it was on:
    // a toggle that threw the selection to the top would make starring ten commands
    // in a row ten trips back down the list.
    refilter();
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).toString() == name) {
            list_->setCurrentRow(i);
            break;
        }
    list_->verticalScrollBar()->setValue(scrolled);
}

void CommandPalette::moveFavourite(int by)
{
    QListWidgetItem* item = list_->currentItem();
    if (usage_ == nullptr || item == nullptr || !item->data(Qt::UserRole).isValid()) return;
    if (!query_->text().trimmed().isEmpty()) return; ///< an answer's rank is not the person's

    const QString name = item->data(Qt::UserRole).toString();
    const auto row = std::ranges::find_if(rows_, [&name](const Row& r) { return r.name == name; });
    if (row == rows_.end() || !usage_->isFavourite(row->id.toStdString())) return;

    // THE CURSOR STAYS ON THE COPY IT WAS ON: a starred command is listed under Favoriler and
    // again under its category, and the move is about the first.
    const bool inTop   = list_->currentRow() < topEnd_;
    const int scrolled = list_->verticalScrollBar()->value();
    if (!usage_->moveFavourite(row->id.toStdString(), by)) return;

    refilter();
    const int from = inTop ? 0 : topEnd_;
    const int to   = inTop ? topEnd_ : list_->count();
    for (int i = from; i < to; ++i)
        if (list_->item(i)->data(Qt::UserRole).toString() == name) {
            list_->setCurrentRow(i);
            break;
        }
    list_->verticalScrollBar()->setValue(scrolled);
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    // THE STAR, CLICKED: a press in a command row's left gutter stars it rather than
    // choosing it. The mouse is not the only way (Ctrl+D below, Article 1.2's spirit:
    // nothing here is reachable by mouse alone).
    if (watched == list_->viewport() && event->type() == QEvent::MouseButtonPress) {
        const auto* mouse   = static_cast<QMouseEvent*>(event);
        QListWidgetItem* at = list_->itemAt(mouse->position().toPoint());
        if (at != nullptr && at->data(Qt::UserRole).isValid() &&
            mouse->position().x() < kPadX + kStarGutter - 4) {
            list_->setCurrentItem(at);
            toggleFavourite();
            return true;
        }
    }

    // Ctrl+D stars the row under the cursor without the caret leaving the field.
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_D && (key->modifiers() & Qt::ControlModifier) != 0) {
            toggleFavourite();
            return true;
        }
        // Alt+↑ / Alt+↓ arranges the starred commands, the caret staying in the field. Before
        // the arrow keys below, which would otherwise move the cursor and nothing else.
        if ((key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) &&
            (key->modifiers() & Qt::AltModifier) != 0) {
            moveFavourite(key->key() == Qt::Key_Up ? -1 : 1);
            return true;
        }
    }

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

} // namespace piricad::app
