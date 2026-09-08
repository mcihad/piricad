// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/attribute_table.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/measure_text.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/core/document.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kentos::app {
namespace {

// `design.md` §9, measured off `öznitelik_tablosu.png`.
constexpr int kToolRow    = 44;
constexpr int kFilterRow  = 44;
constexpr int kHeaderRow  = 30;
constexpr int kTableRow   = 28;
constexpr int kStatsWidth = 280;
constexpr int kFooterRow  = 34;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// `5 128 402.16` — thin-space thousands, two places. What a Turkish map sheet
/// prints, and what the reference's statistics column shows.
QString grouped(double value, int places = 2)
{
    QString text        = QString::number(value, 'f', places);
    const qsizetype dot = text.indexOf(QLatin1Char('.'));
    qsizetype at        = dot < 0 ? text.size() : dot;
    for (at -= 3; at > 0; at -= 3)
        text.insert(at, QLatin1Char(' '));
    return text;
}

// Grouping lives in `measure_text.hpp` now: the pick chooser prints figures into
// the same kind of column and two rules for one column is one too many.
using measure::spacedThousands;

} // namespace

// =============================================================================
// AttributeModel
// =============================================================================

AttributeModel::AttributeModel(Controller& controller, QString layerName, QObject* parent)
    : QAbstractTableModel(parent), controller_(controller), layer_(std::move(layerName))
{
    refresh();
}

void AttributeModel::refresh()
{
    beginResetModel();

    const core::Document& doc    = controller_.document();
    const core::AttrTable& table = doc.attributes();

    columns_.clear();
    for (std::size_t c = 0; c < table.columns(); ++c)
        columns_.push_back(static_cast<core::AttrId>(c));

    rows_.clear();
    error_.clear();

    // THE LAYER SCOPE, resolved here and not cached. The window's title already
    // said which layer it was opened on; every row in the drawing was in it
    // anyway, so the title was the only thing that knew. A layer's attribute
    // table has to be that layer's rows.
    //
    // A name that no longer resolves shows nothing rather than everything: the
    // layer was renamed or deleted under the open window, and the honest answer
    // to "the rows of a layer that is not there" is none of them.
    const bool scoped        = !layer_.isEmpty();
    const core::LayerId only = scoped ? doc.find_layer(layer_.toStdString()) : core::kNoLayer;
    if (scoped && only == core::kNoLayer) {
        endResetModel();
        emit filtered(0, 0);
        return;
    }

    // The predicate reads THIS row's cells. Nothing about the grammar knows what
    // an attribute is, which is what lets the same expression filter a PostGIS
    // result set the day that lands.
    std::size_t total = 0;
    for (core::EntityId slot = 0; slot < doc.entities().size(); ++slot) {
        if (!doc.alive(slot)) continue;
        if (scoped && doc.entities().layer[slot] != only) continue;
        ++total;

        const command::FieldReader field =
            [&table, slot](std::string_view name) -> std::optional<std::string> {
            const core::AttrId col = table.find(name);
            if (col == core::kNoAttr) return std::nullopt;

            const core::AttrColumn* column = table.column(col);
            if (!column || slot >= column->rows() || !column->present(slot)) return std::nullopt;

            // The shared formatter with the POINT mark and NO thousands
            // grouping: the predicate compares numbers, and `2 940.12` is not a
            // number to anything that has to parse it back.
            const auto cell = column->get(slot);
            if (!cell) return std::nullopt;
            return core::attr_display(cell.value(), core::DecimalMark::Point);
        };

        auto matched = command::evaluate_predicate(filter_.toStdString(), field);
        if (!matched) {
            // One complaint, not one per row: a broken filter is a typing
            // mistake and repeating it a thousand times helps nobody.
            error_ = QString::fromStdString(matched.error().message);
            rows_.clear();
            for (core::EntityId all = 0; all < doc.entities().size(); ++all) {
                if (!doc.alive(all)) continue;
                if (scoped && doc.entities().layer[all] != only) continue;
                rows_.push_back(doc.entities().key[all]);
            }
            break;
        }
        if (matched.value()) rows_.push_back(doc.entities().key[slot]);
    }

    endResetModel();
    emit filtered(static_cast<int>(rows_.size()), static_cast<int>(total));
}

void AttributeModel::setFilter(const QString& expression)
{
    filter_ = expression;
    refresh();
}

core::EntityKey AttributeModel::keyAt(int row) const
{
    return row >= 0 && row < rows_.size() ? rows_[row] : core::EntityKey::None;
}

int AttributeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AttributeModel::columnCount(const QModelIndex& parent) const
{
    // One more than the declared columns: `fid` is the entity's own key, which
    // is not an attribute and must never become one — it is identity, and R44
    // keeps identity out of the attribute table.
    return parent.isValid() ? 0 : static_cast<int>(columns_.size()) + 1;
}

QVariant AttributeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Vertical) return section + 1;
    if (section == 0) return QStringLiteral("fid");

    const core::AttrTable& table   = controller_.document().attributes();
    const core::AttrColumn* column = table.column(columns_[section - 1]);
    return column ? QString::fromStdString(column->spec().id) : QVariant{};
}

QVariant AttributeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};

    const core::Document& doc = controller_.document();
    const core::EntityKey key = keyAt(index.row());
    const core::EntityId slot = doc.slot_of(key);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 0) return static_cast<qulonglong>(key);

        const core::AttrColumn* column = doc.attributes().column(columns_[index.column() - 1]);
        if (!column || slot >= column->rows() || !column->present(slot))
            return role == Qt::EditRole ? QVariant{} : QStringLiteral("—");

        // `AttrColumn::text` is empty for a NUMERIC column — it returns the
        // interned string and a number has none — so an `ada_no` declared
        // `tam_sayi` came out blank in every row. The shared formatter renders
        // every type, and with the POINT mark, because the filter grammar, the
        // sort and every export read a point (`core::DecimalMark`).
        const auto cell = column->get(slot);
        if (!cell) return QStringLiteral("—");

        const QString text =
            QString::fromStdString(core::attr_display(cell.value(), core::DecimalMark::Point));

        // Editing gets the raw value; READING gets it grouped, because a column
        // of areas is scanned for magnitude and `18 904.36` says its size at a
        // glance where `18904.36` has to be counted.
        return role == Qt::EditRole ? text : spacedThousands(text);
    }

    if (role == Qt::TextAlignmentRole) {
        // Numbers right, words left — the only way a column of areas can be
        // scanned for an outlier without reading every digit.
        const QVariant shown = data(index, Qt::DisplayRole);
        bool numeric         = false;
        (void)shown.toString().toDouble(&numeric);
        return QVariant(
            static_cast<int>((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter));
    }

    return {};
}

void AttributeModel::setEditing(bool on)
{
    if (editing_ == on) return;
    editing_ = on;

    // The whole grid's flags changed, and Qt has no narrower way to say so.
    beginResetModel();
    endResetModel();
}

FieldSpec AttributeModel::fieldFor(int column) const
{
    if (column <= 0 || column > columns_.size()) return {};
    const core::AttrColumn* held = controller_.document().attributes().column(columns_[column - 1]);
    return held != nullptr ? field_for(held->spec()) : FieldSpec{};
}

QString AttributeModel::validateRow(int row) const
{
    if (row < 0 || row >= rows_.size()) return {};

    const core::Document& doc = controller_.document();
    const core::EntityId slot = doc.slot_of(rows_[row]);
    if (slot == core::kNoEntity || !doc.alive(slot)) return {};

    // THE DOCUMENT'S OWN CHECK, not a second one written here. `validate_row`
    // is what a command runs before it commits, and a table that judged by a
    // different rule would call a row good that the next save refuses.
    const auto checked = doc.attributes().validate_row(doc.entities().slot[slot], doc.catalogues());
    return checked.ok() ? QString() : QString::fromStdString(checked.error().message);
}

QStringList AttributeModel::validateAll() const
{
    QStringList out;
    for (int row = 0; row < rows_.size(); ++row) {
        const QString complaint = validateRow(row);
        if (!complaint.isEmpty()) out << tr("%1. satır: %2").arg(row + 1).arg(complaint);
    }
    return out;
}

Qt::ItemFlags AttributeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    // `fid` is identity and never editable; everything else goes out as a
    // command when it changes — but ONLY while the edit mode is on.
    //
    // THE REFUSAL IS HERE AND NOT IN THE VIEW'S TRIGGERS, because a trigger only
    // covers the ways it was told about: a double click, a key press, `edit()`
    // called from code, a paste. A cell that is not marked editable cannot be
    // opened by any of them.
    const Qt::ItemFlags base = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (!editing_ || index.column() == 0) return base;
    return base | Qt::ItemIsEditable;
}

bool AttributeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.column() == 0) return false;
    if (!editing_) return false;

    const core::AttrColumn* column =
        controller_.document().attributes().column(columns_[index.column() - 1]);
    if (!column) return false;

    const QString text    = value.toString();
    const QString asTyped = text.isEmpty() ? QStringLiteral("yok") : text;

    // CHECKED BEFORE IT IS SENT, against the column's own declaration and with
    // the parser the command itself uses (`core::attr_parse`). The command would
    // refuse it anyway — but a refusal that arrives after the dispatch is a
    // failed line in the transcript and a cell the user has already left. This
    // one keeps the value where they can still see and fix it.
    const auto parsed = core::attr_parse(column->spec(), asTyped.toStdString());
    if (!parsed) {
        emit rejected(QString::fromStdString(parsed.error().message));
        return false;
    }

    // THROUGH THE BUS, like every other client. The table has no path into the
    // entity store and must not: a value edited here and the same value typed at
    // the prompt have to produce the same journal line (Article 1.2, 5.9).
    // AND THE ANSWER IS READ. `runLine` discards the result, so a command that
    // refused — a locked layer, an object the key no longer names — left the cell
    // showing the old value with nothing said about why. A write nobody checks is
    // a write that sometimes does not happen.
    auto written = controller_.runLineResult(QStringLiteral("ÖZNİTELİK ad=%1 nesne=%2 deger=\"%3\"")
                                                 .arg(QString::fromStdString(column->spec().id))
                                                 .arg(static_cast<qulonglong>(keyAt(index.row())))
                                                 .arg(asTyped),
                                             command::Origin::Gui);
    if (!written) {
        emit rejected(QString::fromStdString(written.error().message));
        return false;
    }

    emit dataChanged(index, index);

    // AND THE ROW IS CHECKED AFTER EVERY ENTRY, not only when the mode closes. A
    // required cell left empty three hundred rows ago is a complaint nobody will
    // connect to what they were doing; the same complaint at the moment it
    // happens is one keystroke from being fixed.
    emit rowChecked(index.row(), validateRow(index.row()));
    return true;
}

QVector<QString> AttributeModel::columnValues(int column) const
{
    // The EDIT role, not the display one: display groups thousands with a space
    // and `2 940.12` is not a number to `toDouble`, so the statistics counted
    // only the values small enough to have no grouping — two out of eighteen.
    QVector<QString> out;
    out.reserve(rows_.size());
    for (int row = 0; row < rows_.size(); ++row)
        out.push_back(data(this->index(row, column), Qt::EditRole).toString());
    return out;
}

// =============================================================================
// AttributeTable
// =============================================================================

/// The histogram in the statistics panel: one bar per bucket, drawn rather than
/// charted, because a chart library for twenty rectangles is a dependency that
/// buys nothing (Article 2.7's test runs both ways).
class Histogram : public QWidget
{
public:
    explicit Histogram(QWidget* parent) : QWidget(parent) { setFixedHeight(72); }

    void setValues(QVector<double> values, ThemeMode mode)
    {
        values_ = std::move(values);
        theme_  = mode;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Tokens& t = tokensOf(theme_);
        QPainter p(this);
        p.fillRect(rect(), t.bgPanel);
        if (values_.size() < 2) return;

        const auto [lo, hi] = std::minmax_element(values_.begin(), values_.end());
        if (*hi <= *lo) return;

        constexpr int kBuckets = 20;
        QVector<int> counts(kBuckets, 0);
        for (double v : values_) {
            const int bucket = std::clamp(
                static_cast<int>((v - *lo) / (*hi - *lo) * (kBuckets - 1)), 0, kBuckets - 1);
            ++counts[bucket];
        }
        const int tallest = *std::max_element(counts.begin(), counts.end());
        if (tallest <= 0) return;

        const qreal step = static_cast<qreal>(width()) / kBuckets;
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        for (int i = 0; i < kBuckets; ++i) {
            const qreal h = static_cast<qreal>(counts[i]) / tallest * (height() - 14);
            p.drawRect(QRectF(i * step + 1.0, height() - 14 - h, step - 2.0, h));
        }

        QFont face(QStringLiteral("IBM Plex Mono"));
        face.setPixelSize(9);
        p.setFont(face);
        p.setPen(t.textFaint);
        p.drawText(QRect(0, height() - 12, width() / 2, 12), Qt::AlignLeft, grouped(*lo, 0));
        p.drawText(QRect(width() / 2, height() - 12, width() / 2, 12), Qt::AlignRight,
                   grouped(*hi, 0));
    }

private:
    QVector<double> values_;
    ThemeMode theme_ = ThemeMode::Dark;
};

AttributeTable::AttributeTable(Controller& controller, QString layerName, QWidget* parent)
    : DialogFrame(parent), controller_(controller), layerName_(std::move(layerName))
{
    setHeading(Glyph::Table, tr("Öznitelik Tablosu"),
               layerName_.isEmpty() ? tr("— tüm çizim") : tr("— %1").arg(layerName_));
    setFooterHeight(kFooterRow);
    resize(1900, 1030);

    model_ = new AttributeModel(controller_, layerName_, this);

    view_ = new QTableView(this);
    view_->setObjectName(QStringLiteral("attributeGrid"));
    view_->setModel(model_);
    view_->setAlternatingRowColors(false);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setShowGrid(true);
    view_->setSortingEnabled(true);
    view_->setFrameShape(QFrame::NoFrame);
    view_->verticalHeader()->setDefaultSectionSize(kTableRow);
    view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view_->horizontalHeader()->setFixedHeight(kHeaderRow);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->horizontalHeader()->setSectionsMovable(true);

    // Sized to what is in them, then left alone: a column the user widened must
    // stay widened, so this runs once per reset rather than on every repaint.
    const auto fitColumns = [this] {
        view_->resizeColumnsToContents();
        for (int c = 0; c < model_->columnCount(); ++c)
            view_->setColumnWidth(c, std::max(view_->columnWidth(c) + 16, 76));
    };

    // Sized to what is in them, then left alone: a column the user widened must
    // stay widened, so this runs once per reset rather than on every repaint.
    connect(model_, &QAbstractItemModel::modelReset, this, fitColumns);
    fitColumns();

    // Picking a row on the table picks the entity on the canvas, because they
    // are the same object seen two ways. It leaves as `SEÇ`, like every other
    // selection in the program.
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        QStringList keys;
        for (const QModelIndex& index : view_->selectionModel()->selectedRows())
            keys << QString::number(static_cast<qulonglong>(model_->keyAt(index.row())));
        if (!keys.isEmpty())
            controller_.runLine(QStringLiteral("SEÇ nesneler=%1").arg(keys.join(QLatin1Char(','))),
                                command::Origin::Gui);
        refreshStatistics();
    });

    auto* body  = new QWidget(this);
    auto* stack = new QVBoxLayout(body);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    stack->addWidget(buildToolRow());
    stack->addWidget(buildFilterBar());

    auto* middle = new QWidget(body);
    auto* row    = new QHBoxLayout(middle);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(view_, 1);

    statistics_ = buildStatistics();
    row->addWidget(statistics_);
    stack->addWidget(middle, 1);

    setBody(body);

    // ---- the footer: the pager on the left, the column summary on the right ----
    pager_ = new QLabel(this);
    pager_->setObjectName(QStringLiteral("quiet"));

    summary_ = new QLabel(this);
    summary_->setObjectName(QStringLiteral("mono"));

    // What the last entry was refused for, or what the row it landed in is still
    // missing. In the warn colour, because it is about something the user just
    // did rather than about something that is broken.
    complaint_ = new QLabel(this);
    complaint_->setObjectName(QStringLiteral("warning"));
    complaint_->setVisible(false);

    footer()->insertWidget(0, pager_);
    footer()->insertWidget(1, complaint_);
    footer()->addWidget(summary_);

    connect(model_, &AttributeModel::filtered, this, [this](int, int) { refreshCounts(); });
    connect(&controller_, &Controller::documentChanged, this, [this] { model_->refresh(); });

    // ---- what the grid says back ----
    connect(model_, &AttributeModel::rejected, this, &AttributeTable::complain);
    connect(model_, &AttributeModel::rowChecked, this, [this](int at, const QString& complaint) {
        if (complaint.isEmpty())
            complain(QString());
        else
            complain(tr("%1. satır: %2").arg(at + 1).arg(complaint));
    });

    // ---- our editors, and Enter walking the grid ----
    delegate_ = new FieldDelegate([this](int column) { return model_->fieldFor(column); }, this);
    delegate_->setTheme(theme_);
    view_->setItemDelegate(delegate_);
    connect(delegate_, &FieldDelegate::advanced, this, &AttributeTable::advanceFrom);

    // READ-ONLY UNTIL ASKED. The model refuses to mark a cell editable while the
    // mode is off; this stops the view from even trying, so a double click on a
    // value somebody meant to read does nothing at all.
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshCounts();
    refreshStatistics();
}

void AttributeTable::setEditing(bool on)
{
    if (!on && model_->editing()) {
        // BEFORE THE MODE CLOSES, because this is the last moment the person who
        // typed the values is still the person looking at them.
        const QStringList complaints = model_->validateAll();
        if (!complaints.isEmpty()) {
            const QString head = complaints.mid(0, 8).join(QStringLiteral("\n"));
            const QString more = complaints.size() > 8
                                     ? tr("\n\n…ve %1 satır daha.").arg(complaints.size() - 8)
                                     : QString();
            const auto answer  = QMessageBox::warning(
                this, tr("Doğrulanmayan satırlar"),
                tr("Tablo şemaya uymuyor:\n\n%1%2\n\nYine de düzenleme kipinden çıkılsın mı?")
                    .arg(head, more),
                QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) {
                QSignalBlocker block(editToggle_);
                editToggle_->setChecked(true);
                return;
            }
        }
    }

    model_->setEditing(on);
    view_->setEditTriggers(on ? (QAbstractItemView::DoubleClicked |
                                 QAbstractItemView::EditKeyPressed |
                                 QAbstractItemView::AnyKeyPressed)
                              : QAbstractItemView::NoEditTriggers);
    complain(QString());

    if (on && view_->currentIndex().isValid() && view_->currentIndex().column() == 0)
        view_->setCurrentIndex(model_->index(view_->currentIndex().row(), 1));
}

void AttributeTable::advanceFrom(const QModelIndex& from)
{
    if (!from.isValid() || !model_->editing()) return;

    // ACROSS, THEN DOWN, THEN STOP. Column 0 is `fid` and never opens, so a wrap
    // lands on column 1 — the first cell of the next row that a person can type
    // into. The last cell of the last row stays put rather than wrapping to the
    // top, because "I have reached the end" is information and a silent jump to
    // row one is a value entered in the wrong place.
    int at     = from.row();
    int column = from.column() + 1;
    if (column >= model_->columnCount()) {
        column = 1;
        ++at;
    }
    if (at >= model_->rowCount() || column >= model_->columnCount()) return;

    const QModelIndex next = model_->index(at, column);
    view_->setCurrentIndex(next);
    view_->scrollTo(next);
    view_->edit(next);
}

QString AttributeTable::probeGrid(const QString& action, const QString& value)
{
    // THE EDITOR THAT IS OPEN, not the first one Qt still owns. A closed editor
    // is destroyed with `deleteLater`, so for one turn of the loop the view has
    // two `Field` children and `findChild` hands back the dead one — which is how
    // a probe ends up typing into the cell it just left.
    const auto openEditor = [this]() -> Field* {
        const QList<Field*> all = view_->findChildren<Field*>();
        for (auto it = all.crbegin(); it != all.crend(); ++it)
            if ((*it)->isVisible()) return *it;
        return nullptr;
    };

    const auto where = [this] {
        const QModelIndex at = view_->currentIndex();
        return at.isValid() ? QStringLiteral("%1,%2").arg(at.row()).arg(at.column())
                            : QStringLiteral("yok");
    };

    if (action == QStringLiteral("kip")) {
        editToggle_->setChecked(value == QStringLiteral("evet"));
        return model_->editing() ? QStringLiteral("açık") : QStringLiteral("kapalı");
    }

    if (action == QStringLiteral("git")) {
        const QStringList parts = value.split(QLatin1Char(','));
        if (parts.size() == 2)
            view_->setCurrentIndex(model_->index(parts[0].toInt(), parts[1].toInt()));
        return where();
    }

    if (action == QStringLiteral("ac")) {
        // Through `edit()`, which is what a double click and the F2 key both
        // reach — so a mode that refuses here refuses them too.
        view_->edit(view_->currentIndex());
        return openEditor() != nullptr ? QStringLiteral("açıldı") : QStringLiteral("açılmadı");
    }

    if (action == QStringLiteral("yaz")) {
        auto* editor = openEditor();
        if (editor == nullptr) return QStringLiteral("düzenleyici yok");
        editor->setValue(value);

        // The Enter a person presses, not a call to `commitData`: the whole point
        // is that the key travels from the editor to the delegate to this window.
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(editor, &enter);
        QCoreApplication::processEvents();
        return where();
    }

    if (action == QStringLiteral("takvim")) {
        auto* editor = openEditor();
        if (editor == nullptr) return QStringLiteral("düzenleyici yok");
        auto* button = editor->findChild<QToolButton*>();
        if (button == nullptr) return QStringLiteral("düğme yok");
        button->click();
        QCoreApplication::processEvents();
        auto* card = editor->findChild<DatePopup*>();
        return (card != nullptr && card->isVisible()) ? QStringLiteral("açıldı")
                                                      : QStringLiteral("açılmadı");
    }

    if (action == QStringLiteral("takvimgun")) {
        auto* editor = openEditor();
        return editor != nullptr ? editor->value() : QStringLiteral("düzenleyici yok");
    }

    if (action == QStringLiteral("koy")) {
        auto* editor = openEditor();
        if (editor == nullptr) return QStringLiteral("düzenleyici yok");
        editor->setValue(value);
        return QStringLiteral("kondu");
    }

    if (action == QStringLiteral("hucre")) {
        const QStringList parts = value.split(QLatin1Char(','));
        if (parts.size() != 2) return QStringLiteral("?");
        return model_->index(parts[0].toInt(), parts[1].toInt()).data(Qt::EditRole).toString();
    }

    if (action == QStringLiteral("oku")) {
        const QModelIndex at = view_->currentIndex();
        return at.isValid() ? at.data(Qt::EditRole).toString() : QString();
    }

    if (action == QStringLiteral("sikayet")) return complaint_->text();

    return QStringLiteral("bilinmeyen eylem");
}

void AttributeTable::complain(const QString& text)
{
    if (complaint_ == nullptr) return;
    complaint_->setText(text);
    complaint_->setVisible(!text.isEmpty());
}

QWidget* AttributeTable::buildToolRow()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("tableToolRow"));
    bar->setFixedHeight(kToolRow);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 0, 8, 0);
    row->setSpacing(4);

    // Every mark is a command, and the ones that are not implemented yet say so
    // rather than doing nothing quietly (§11.8).
    struct Mark
    {
        // Widest first. Declared glyph-tip-line-flag this cost eleven bytes of
        // padding per entry where three is the best possible.
        const char* tip;
        const char* line;
        Glyph glyph;
        bool checkable;
    };

    static const Mark kMarks[] = {
        {.tip = "Düzenleme kipi", .line = nullptr, .glyph = Glyph::StyleCopy, .checkable = true},
        {.tip = "Kaydet", .line = "KAYDET", .glyph = Glyph::Save, .checkable = false},
        {.tip = "Geri al", .line = "GERİAL", .glyph = Glyph::Undo, .checkable = false},
        {.tip = "Yinele", .line = "YİNELE", .glyph = Glyph::Redo, .checkable = false},
        {.tip = "Satır ekle", .line = nullptr, .glyph = Glyph::Plus, .checkable = false},
        {.tip = "Satır sil", .line = nullptr, .glyph = Glyph::Erase, .checkable = false},
        {.tip = "Satırı çoğalt", .line = nullptr, .glyph = Glyph::Duplicate, .checkable = false},
        {.tip       = "Tümünü seç",
         .line      = "SEÇ tümü=evet",
         .glyph     = Glyph::SelectArea,
         .checkable = false},
        {.tip       = "Seçimi tersine çevir",
         .line      = nullptr,
         .glyph     = Glyph::Select,
         .checkable = false},
        {.tip = "Süz", .line = nullptr, .glyph = Glyph::Filter, .checkable = false},
        {.tip = "Alan hesapla", .line = nullptr, .glyph = Glyph::Function, .checkable = false},
        {.tip = "Alan istatistikleri", .line = nullptr, .glyph = Glyph::Table, .checkable = true},
        {.tip = "Sütunlar", .line = nullptr, .glyph = Glyph::Grid, .checkable = false},
        {.tip = "Dışa aktar", .line = "DIŞAAKTAR", .glyph = Glyph::Export, .checkable = false},
        {.tip = "Yazdır", .line = "YAZDIR", .glyph = Glyph::Print, .checkable = false},
    };

    for (const Mark& mark : kMarks) {
        auto* button = new QToolButton(bar);
        button->setObjectName(QStringLiteral("tableTool"));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(tr(mark.tip));
        button->setCheckable(mark.checkable);
        button->setProperty("glyph", static_cast<int>(mark.glyph));
        if (mark.line) {
            const QString line = QString::fromUtf8(mark.line);
            connect(button, &QToolButton::clicked, this,
                    [this, line] { controller_.runLine(line, command::Origin::Gui); });
        } else if (std::strcmp(mark.tip, "Düzenleme kipi") == 0) {
            // THE ONE MARK THAT WAS CHECKABLE AND DID NOTHING. It has been in
            // this row since the window was drawn, ticking and untying itself
            // while every cell stayed editable underneath it — which is worse
            // than not having it, because it said a thing about the table that
            // was not true.
            editToggle_ = button;
            button->setToolTip(tr("Düzenleme kipi — kapalıyken hiçbir hücre açılmaz"));
            connect(button, &QToolButton::toggled, this, &AttributeTable::setEditing);
        } else {
            button->setEnabled(mark.checkable);
        }
        row->addWidget(button);
    }

    row->addStretch(1);

    // The `Tablo | Form` pair: the same rows, one at a time or all at once.
    for (const char* name : {"Tablo", "Form"}) {
        auto* button = new QPushButton(tr(name), bar);
        button->setObjectName(QStringLiteral("segment"));
        button->setCheckable(true);
        button->setChecked(std::strcmp(name, "Tablo") == 0);
        row->addWidget(button);
    }

    return bar;
}

QWidget* AttributeTable::buildFilterBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("tableFilterBar"));
    bar->setFixedHeight(kFilterRow);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 0, 12, 0);
    row->setSpacing(8);

    auto* mark = new QLabel(bar);
    mark->setPixmap(
        glyph_pixmap(Glyph::Function, tokensOf(theme_).accent, 16, devicePixelRatioF()));
    row->addWidget(mark);

    filter_ = new QLineEdit(bar);
    filter_->setObjectName(QStringLiteral("expressionBar"));
    filter_->setPlaceholderText(
        tr("Süzme ifadesi — \"alan_m2\" > 2000 AND \"plan_fonksiyon\" = 'Konut'"));
    row->addWidget(filter_, 1);

    auto* apply = new QPushButton(tr("Filtrele"), bar);
    apply->setObjectName(QStringLiteral("primary"));
    connect(apply, &QPushButton::clicked, this, [this] {
        model_->setFilter(filter_->text());
        if (!model_->filterError().isEmpty())
            filter_->setToolTip(model_->filterError());
        else
            filter_->setToolTip(QString());
        refreshCounts();
        refreshStatistics();
    });
    connect(filter_, &QLineEdit::returnPressed, apply, &QPushButton::click);
    row->addWidget(apply);

    auto* save = new QPushButton(tr("Kaydet ▾"), bar);
    save->setToolTip(tr("Kayıtlı süzgeçler Faz 2'de gelecek."));
    save->setEnabled(false);
    row->addWidget(save);

    search_ = new QLineEdit(bar);
    search_->setObjectName(QStringLiteral("tableSearch"));
    search_->setPlaceholderText(tr("Tabloda ara…")); // ui-label
    search_->setFixedWidth(200);
    row->addWidget(search_);

    return bar;
}

QWidget* AttributeTable::buildStatistics()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("statsPanel"));
    panel->setFixedWidth(kStatsWidth);

    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(14, 12, 14, 12);
    column->setSpacing(8);

    auto* title = new QLabel(tr("Alan İstatistikleri"), panel);
    title->setObjectName(QStringLiteral("sectionTitle"));
    column->addWidget(title);

    statsField_ = new QLabel(panel);
    statsField_->setObjectName(QStringLiteral("mono"));
    column->addWidget(statsField_);

    histogram_ = new Histogram(panel);
    column->addWidget(histogram_);

    statsBody_ = new QLabel(panel);
    statsBody_->setObjectName(QStringLiteral("mono"));
    statsBody_->setTextFormat(Qt::RichText);
    statsBody_->setAlignment(Qt::AlignTop);
    column->addWidget(statsBody_, 1);

    return panel;
}

void AttributeTable::refreshCounts()
{
    const int shown = model_->rowCount();
    pager_->setText(tr("1 – %1  /  %2").arg(shown).arg(shown));

    setHeading(
        Glyph::Table, tr("Öznitelik Tablosu"),
        tr("— %1   %2 nesne").arg(layerName_.isEmpty() ? tr("tüm çizim") : layerName_).arg(shown));
}

void AttributeTable::refreshStatistics()
{
    const QModelIndex current = view_->currentIndex();
    const int column          = current.isValid() ? current.column() : 1;
    if (column < 0 || column >= model_->columnCount()) return;

    const QString name = model_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();

    QVector<double> numbers;
    int nulls = 0;
    for (const QString& text : model_->columnValues(column)) {
        bool ok        = false;
        const double v = text.toDouble(&ok);
        if (ok)
            numbers.push_back(v);
        else if (text.isEmpty())
            ++nulls; // the edit role hands an absent cell back as nothing
    }

    statsField_->setText(tr("%1 · %2").arg(name, numbers.isEmpty() ? tr("metin") : tr("gerçek")));
    static_cast<Histogram*>(histogram_)->setValues(numbers, theme_);

    if (numbers.isEmpty()) {
        statsBody_->setText(tr("Bu sütun sayı taşımıyor; sayısal özet çıkarılamaz."));
        return;
    }

    std::sort(numbers.begin(), numbers.end());
    const double total = std::accumulate(numbers.begin(), numbers.end(), 0.0);
    const double mean  = total / static_cast<double>(numbers.size());
    const double median =
        numbers.size() % 2 ? numbers[numbers.size() / 2]
                           : (numbers[numbers.size() / 2 - 1] + numbers[numbers.size() / 2]) / 2.0;

    double variance = 0.0;
    for (double v : numbers)
        variance += (v - mean) * (v - mean);
    variance /= static_cast<double>(numbers.size());

    const auto line = [](const QString& key, const QString& value) {
        return QStringLiteral("<tr><td>%1</td><td align=right>%2</td></tr>").arg(key, value);
    };

    statsBody_->setText(QStringLiteral("<table width='100%' cellspacing=6>") +
                        line(tr("Nesne sayısı"), grouped(model_->rowCount(), 0)) +
                        line(tr("Geçerli değer"), grouped(static_cast<double>(numbers.size()), 0)) +
                        line(tr("NULL"), grouped(nulls, 0)) +
                        line(tr("Minimum"), grouped(numbers.front())) +
                        line(tr("Maksimum"), grouped(numbers.back())) +
                        line(tr("Ortalama"), grouped(mean)) + line(tr("Ortanca"), grouped(median)) +
                        line(tr("Std. sapma"), grouped(std::sqrt(variance))) +
                        line(tr("Toplam"), grouped(total)) + QStringLiteral("</table>"));

    summary_->setText(tr("Σ %1 = %2      x̄ = %3").arg(name, grouped(total), grouped(mean)));
}

void AttributeTable::applyTheme(ThemeMode mode)
{
    if (delegate_ != nullptr) delegate_->setTheme(mode);
    theme_ = mode;
    DialogFrame::applyTheme(mode);

    // Every drawn mark re-tints with the theme; each carries its glyph in a
    // property, which keeps this from being a second list of buttons.
    const Tokens& t = tokensOf(mode);
    for (QToolButton* button : findChildren<QToolButton*>()) {
        const QVariant glyph = button->property("glyph");
        if (!glyph.isValid()) continue;
        button->setIcon(icon(static_cast<Glyph>(glyph.toInt()), t.textDim, t.accent, 18));
    }
    refreshStatistics();
}

} // namespace kentos::app
