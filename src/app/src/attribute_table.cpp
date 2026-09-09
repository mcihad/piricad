// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/attribute_table.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/datagrid.hpp"
#include "kentos_cad/app/export_dialog.hpp"
#include "kentos_cad/app/expression_edit.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/measure_text.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/command/selection.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kentos::app {
namespace {

// `design.md` §9, measured off `öznitelik_tablosu.png`.
constexpr int kToolRow    = 42;
constexpr int kFilterRow  = 44;
constexpr int kStatsWidth = 268;
constexpr int kFooterRow  = 30;
constexpr int kToolMark   = 28; ///< one tool button
constexpr int kToolIcon   = 18;
constexpr int kRuleHeight = 22; ///< the 1 px separator between tool groups
constexpr int kBuckets    = 14; ///< the histogram's bars, §9

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

/// Whether a declared column holds figures. Decided by TYPE, not by parsing the
/// text: a parcel number is a figure even when it reads `0`, and a code is a
/// word even when every code happens to be digits.
bool numericType(core::AttrType type)
{
    switch (type) {
    case core::AttrType::Int64:
    case core::AttrType::Length:
    case core::AttrType::Decimal: return true;
    case core::AttrType::Bool:
    case core::AttrType::Text:
    case core::AttrType::CodeRef:
    case core::AttrType::Date: return false;
    }
    return false;
}

/// The Turkish fold of `text`, for a search that finds `İZMİR` when `izmir` is
/// typed. Core's table, never `<cctype>` (CLAUDE.md 5.6).
QString folded(const QString& text)
{
    return QString::fromStdString(core::turkish_fold_key(text.toStdString()));
}

} // namespace

// =============================================================================
// AttributeModel
// =============================================================================

AttributeModel::AttributeModel(Controller& controller, QString layerName, QObject* parent)
    : QAbstractTableModel(parent), controller_(controller), layer_(std::move(layerName))
{
    refresh();
}

QString AttributeModel::rawText(core::EntityId slot, int column) const
{
    const core::Document& doc = controller_.document();
    if (column == 0) return QString::number(static_cast<qulonglong>(doc.entities().key[slot]));
    if (column < 1 || column > columns_.size()) return {};

    const core::AttrColumn* held = doc.attributes().column(columns_[column - 1]);
    if (!held || slot >= held->rows() || !held->present(slot)) return {};
    const auto cell = held->get(slot);
    if (!cell) return {};
    return QString::fromStdString(core::attr_display(cell.value(), core::DecimalMark::Point));
}

void AttributeModel::refresh()
{
    beginResetModel();

    const core::Document& doc    = controller_.document();
    const core::AttrTable& table = doc.attributes();

    // ONLY THE COLUMNS THIS TABLE'S SCOPE CARRIES. A table opened on a layer
    // shows the project's columns and that layer's own; the whole-drawing table
    // shows every one, because there is no single layer it could filter by and a
    // column hidden there would be a column with no table at all.
    columns_.clear();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* held = table.column(static_cast<core::AttrId>(c));
        if (held == nullptr) continue;
        if (!layer_.isEmpty() && !core::attr_applies_to(held->spec(), layer_.toStdString()))
            continue;
        columns_.push_back(static_cast<core::AttrId>(c));
    }

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

    const command::Selection& picked = controller_.bus().selection();
    const QString needle             = folded(search_);

    // The predicate reads THIS row's cells. Nothing about the grammar knows what
    // an attribute is, which is what lets the same expression filter a PostGIS
    // result set the day that lands.
    std::size_t total = 0;
    for (core::EntityId slot = 0; slot < doc.entities().size(); ++slot) {
        if (!doc.alive(slot)) continue;
        if (scoped && doc.entities().layer[slot] != only) continue;
        ++total;

        if (onlySelected_ && !picked.contains(doc.entities().key[slot])) continue;

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
        if (!matched.value()) continue;

        // THE QUICK SEARCH, over every cell of the row, folded the Turkish way.
        if (!needle.isEmpty()) {
            bool found = false;
            for (int c = 0; c <= columns_.size() && !found; ++c)
                found = folded(rawText(slot, c)).contains(needle);
            if (!found) continue;
        }

        rows_.push_back(doc.entities().key[slot]);
    }

    applySort();
    endResetModel();
    emit filtered(static_cast<int>(rows_.size()), static_cast<int>(total));
}

void AttributeModel::setFilter(const QString& expression)
{
    filter_ = expression;
    refresh();
}

void AttributeModel::setSearch(const QString& text)
{
    search_ = text.trimmed();
    refresh();
}

void AttributeModel::setOnlySelected(bool on)
{
    if (onlySelected_ == on) return;
    onlySelected_ = on;
    refresh();
}

core::EntityKey AttributeModel::keyAt(int row) const
{
    return row >= 0 && row < rows_.size() ? rows_[row] : core::EntityKey::None;
}

int AttributeModel::rowOf(core::EntityKey key) const
{
    return static_cast<int>(rows_.indexOf(key));
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

bool AttributeModel::numericColumn(int column) const
{
    if (column == 0) return true;
    if (column < 1 || column > columns_.size()) return false;
    const core::AttrColumn* held = controller_.document().attributes().column(columns_[column - 1]);
    return held != nullptr && numericType(held->spec().type);
}

QVariant AttributeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return role == Qt::DisplayRole ? QVariant(section + 1) : QVariant();

    // The header lines up with its column: a column of figures carries its
    // name at the right, where the figures end.
    if (role == Qt::TextAlignmentRole)
        return QVariant(static_cast<int>((numericColumn(section) ? Qt::AlignRight : Qt::AlignLeft) |
                                         Qt::AlignVCenter));
    if (role != Qt::DisplayRole) return {};
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

    // Numbers right, words left — the only way a column of areas can be scanned
    // for an outlier without reading every digit. Decided by the column's TYPE.
    if (role == Qt::TextAlignmentRole)
        return QVariant(static_cast<int>(
            (numericColumn(index.column()) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter));

    if (role == GridRole::Null) {
        if (index.column() == 0) return false;
        const core::AttrColumn* column = doc.attributes().column(columns_[index.column() - 1]);
        return !column || slot >= column->rows() || !column->present(slot);
    }

    if (role == GridRole::Edited) {
        if (index.column() == 0) return false;
        return edited_.contains(QStringLiteral("%1:%2")
                                    .arg(static_cast<qulonglong>(key))
                                    .arg(static_cast<int>(columns_[index.column() - 1])));
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
    return held != nullptr ? as_cell(field_for(held->spec())) : FieldSpec{};
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

    const core::EntityKey key = keyAt(index.row());
    const QString mark        = QStringLiteral("%1:%2")
                             .arg(static_cast<qulonglong>(key))
                             .arg(static_cast<int>(columns_[index.column() - 1]));

    // THROUGH THE BUS, like every other client. The table has no path into the
    // entity store and must not: a value edited here and the same value typed at
    // the prompt have to produce the same journal line (Article 1.2, 5.9).
    // AND THE ANSWER IS READ. `runLine` discards the result, so a command that
    // refused — a locked layer, an object the key no longer names — left the cell
    // showing the old value with nothing said about why. A write nobody checks is
    // a write that sometimes does not happen.
    auto written = controller_.runLineResult(QStringLiteral("ÖZNİTELİK ad=%1 nesne=%2 deger=\"%3\"")
                                                 .arg(QString::fromStdString(column->spec().id))
                                                 .arg(static_cast<qulonglong>(key))
                                                 .arg(asTyped),
                                             command::Origin::Gui);
    if (!written) {
        emit rejected(QString::fromStdString(written.error().message));
        return false;
    }

    // Remembered by entity and column, so the warn ink stays on the cell when
    // the filter reorders the rows — and so the title can count what was done.
    edited_.insert(mark);

    // The document already changed under the row; `refresh()` will have run from
    // the controller's signal, so the index in hand may point at a new row set.
    const int row = rowOf(key);
    if (row >= 0) {
        const QModelIndex at = this->index(row, index.column());
        emit dataChanged(at, at);

        // AND THE ROW IS CHECKED AFTER EVERY ENTRY, not only when the mode
        // closes. A required cell left empty three hundred rows ago is a
        // complaint nobody will connect to what they were doing; the same
        // complaint at the moment it happens is one keystroke from being fixed.
        emit rowChecked(row, validateRow(row));
    }
    return true;
}

void AttributeModel::sort(int column, Qt::SortOrder order)
{
    sortColumn_ = column;
    sortOrder_  = order;
    beginResetModel();
    applySort();
    endResetModel();
}

void AttributeModel::applySort()
{
    if (sortColumn_ < 0 || sortColumn_ >= columnCount()) return;

    const core::Document& doc = controller_.document();
    const bool figures        = numericColumn(sortColumn_);
    const int column          = sortColumn_;

    // Figures as figures, words as words with the locale's collation, and an
    // empty cell after everything either way — a NULL that sorted as zero would
    // put "unknown" among the smallest, which is a claim the data does not make.
    const auto textOf = [&](core::EntityKey key) { return rawText(doc.slot_of(key), column); };
    std::stable_sort(rows_.begin(), rows_.end(), [&](core::EntityKey a, core::EntityKey b) {
        const QString ta = textOf(a);
        const QString tb = textOf(b);
        if (ta.isEmpty() != tb.isEmpty()) return tb.isEmpty(); // empties last, both orders
        bool less = false;
        if (figures) {
            less = ta.toDouble() < tb.toDouble();
        } else {
            less = QString::localeAwareCompare(ta, tb) < 0;
        }
        return sortOrder_ == Qt::AscendingOrder ? less : (!less && ta != tb);
    });
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
/// charted, because a chart library for fourteen rectangles is a dependency that
/// buys nothing (Article 2.7's test runs both ways).
class Histogram : public QWidget
{
public:
    explicit Histogram(QWidget* parent) : QWidget(parent) { setFixedHeight(76); }

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

        QVector<int> counts(kBuckets, 0);
        for (double v : values_) {
            const int bucket = std::clamp(
                static_cast<int>((v - *lo) / (*hi - *lo) * (kBuckets - 1)), 0, kBuckets - 1);
            ++counts[bucket];
        }
        const int tallest = *std::max_element(counts.begin(), counts.end());
        if (tallest <= 0) return;

        // The bars in the accent, lit towards the top — §9's "mavi gradyan".
        constexpr int kAxis = 14;
        const qreal step    = static_cast<qreal>(width()) / kBuckets;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < kBuckets; ++i) {
            const qreal h = static_cast<qreal>(counts[i]) / tallest * (height() - kAxis - 2);
            const QRectF bar(i * step + 1.0, height() - kAxis - h, step - 2.0, h);
            QLinearGradient shade(bar.topLeft(), bar.bottomLeft());
            shade.setColorAt(0.0, t.accentHi);
            shade.setColorAt(1.0, t.accent);
            p.setBrush(shade);
            p.drawRect(bar);
        }

        QFont face(QStringLiteral("IBM Plex Mono"));
        face.setPixelSize(9);
        p.setFont(face);
        p.setPen(t.textFaint);
        const QRect axis(0, height() - 11, width(), 11);
        p.drawText(axis, Qt::AlignLeft, grouped(*lo, 0));
        p.drawText(axis, Qt::AlignHCenter, grouped((*lo + *hi) / 2.0, 0));
        p.drawText(axis, Qt::AlignRight, grouped(*hi, 0));
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

    // THE ONE TABLE. Header band, row numbers, zebra, selection bar, edited
    // cells — all the grid's, none of them styled here (design.md §9).
    view_ = new DataGrid(this);
    view_->setModel(model_);
    view_->setSortingEnabled(true);
    view_->horizontalHeader()->setSortIndicatorShown(true);
    view_->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    view_->setEditors([this](int column) { return model_->fieldFor(column); });
    view_->gridDelegate()->setTheme(theme_);
    connect(view_->gridDelegate(), &FieldDelegate::advanced, this, &AttributeTable::advanceFrom);

    // Sized by type, then left alone: a column the user widened must stay
    // widened, so this runs once per reset rather than on every repaint. The
    // widths are §9's — a figure needs less room than a word, a date a fixed
    // amount, `fid` the least.
    const auto fitColumns = [this] {
        for (int c = 0; c < model_->columnCount(); ++c) {
            const FieldSpec spec = model_->fieldFor(c);
            int width            = 160;
            if (c == 0)
                width = 72;
            else if (spec.kind == FieldKind::Number)
                width = 100;
            else if (spec.kind == FieldKind::Decimal)
                width = 120;
            else if (spec.kind == FieldKind::Date)
                width = 116;
            else if (spec.kind == FieldKind::Bool)
                width = 90;
            view_->setColumnWidth(c, width);
        }
    };
    connect(model_, &QAbstractItemModel::modelReset, this, fitColumns);
    fitColumns();

    // Picking a row on the table picks the entity on the canvas, because they
    // are the same object seen two ways. It leaves as `SEÇ`, like every other
    // selection in the program.
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        pushSelection();
        refreshStatistics();
        refreshCounts();
    });
    connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { refreshStatistics(); });
    connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this] { refreshCounts(); });

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
    buildFooter();

    connect(model_, &AttributeModel::filtered, this, [this](int, int) { refreshCounts(); });
    connect(&controller_, &Controller::documentChanged, this, [this] { model_->refresh(); });
    connect(&controller_, &Controller::selectionChanged, this, &AttributeTable::followCanvas);

    // ---- what the grid says back ----
    connect(model_, &AttributeModel::rejected, this, &AttributeTable::complain);
    connect(model_, &AttributeModel::rowChecked, this, [this](int at, const QString& complaint) {
        if (complaint.isEmpty())
            complain(QString());
        else
            complain(tr("%1. satır: %2").arg(at + 1).arg(complaint));
    });

    // READ-ONLY UNTIL ASKED. The model refuses to mark a cell editable while the
    // mode is off; this stops the view from even trying, so a double click on a
    // value somebody meant to read does nothing at all.
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshCounts();
    refreshStatistics();
    followCanvas();
}

QToolButton* AttributeTable::toolMark(Glyph glyph, const QString& tip, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("tableTool"));
    button->setFixedSize(kToolMark, kToolMark);
    button->setIconSize(QSize(kToolIcon, kToolIcon));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setProperty("glyph", static_cast<int>(glyph));
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

QWidget* AttributeTable::buildToolRow()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("tableToolRow"));
    bar->setFixedHeight(kToolRow);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 0, 8, 0);
    row->setSpacing(2);

    // A 1 px rule between groups, §9's five groups: editing · rows · selection ·
    // analysis · output.
    const auto rule = [bar, row] {
        auto* line = new QFrame(bar);
        line->setObjectName(QStringLiteral("toolRule"));
        line->setFixedSize(1, kRuleHeight);
        row->addSpacing(4);
        row->addWidget(line);
        row->addSpacing(4);
    };

    // Every mark is a command, and the ones that are not implemented yet say so
    // rather than doing nothing quietly (§11.8).
    const auto line = [this](const QString& command) {
        return [this, command] { controller_.runLine(command, command::Origin::Gui); };
    };
    const auto pending = [](QToolButton* b, const QString& what) {
        b->setEnabled(false);
        b->setToolTip(tr("%1 — Faz 2'de gelecek").arg(what));
    };

    // ---- editing ----
    editToggle_ =
        toolMark(Glyph::Pencil, tr("Düzenleme kipi — kapalıyken hiçbir hücre açılmaz"), bar);
    editToggle_->setCheckable(true);
    connect(editToggle_, &QToolButton::toggled, this, &AttributeTable::setEditing);
    row->addWidget(editToggle_);

    auto* save = toolMark(Glyph::Save, tr("Kaydet — KAYDET"), bar);
    connect(save, &QToolButton::clicked, this, line(QStringLiteral("KAYDET")));
    row->addWidget(save);
    auto* undo = toolMark(Glyph::Undo, tr("Geri al — GERİAL"), bar);
    connect(undo, &QToolButton::clicked, this, line(QStringLiteral("GERİAL")));
    row->addWidget(undo);
    auto* redo = toolMark(Glyph::Redo, tr("Yinele — YİNELE"), bar);
    connect(redo, &QToolButton::clicked, this, line(QStringLiteral("YİNELE")));
    row->addWidget(redo);
    rule();

    // ---- rows ----
    auto* add = toolMark(Glyph::Plus, QString(), bar);
    pending(add, tr("Satır ekle"));
    row->addWidget(add);
    deleteRows_ = toolMark(Glyph::Trash, tr("Seçili satırları sil — SİL"), bar);
    connect(deleteRows_, &QToolButton::clicked, this, &AttributeTable::deleteSelectedRows);
    row->addWidget(deleteRows_);
    auto* duplicate = toolMark(Glyph::Duplicate, QString(), bar);
    pending(duplicate, tr("Satırı çoğalt"));
    row->addWidget(duplicate);
    rule();

    // ---- selection ----
    auto* all = toolMark(Glyph::SelectArea, tr("Tümünü seç"), bar);
    connect(all, &QToolButton::clicked, this, [this] { view_->selectAll(); });
    row->addWidget(all);
    auto* none = toolMark(Glyph::Select, tr("Seçimi kaldır — SEÇ TEMİZLE"), bar);
    connect(none, &QToolButton::clicked, this, [this] {
        view_->clearSelection();
        controller_.runLine(QStringLiteral("SEÇ TEMİZLE"), command::Origin::Gui);
    });
    row->addWidget(none);
    auto* invert = toolMark(Glyph::Invert, tr("Seçimi tersine çevir"), bar);
    connect(invert, &QToolButton::clicked, this, &AttributeTable::invertSelection);
    row->addWidget(invert);
    auto* zoom = toolMark(Glyph::Fit, QString(), bar);
    pending(zoom, tr("Seçiliye yakınlaş"));
    row->addWidget(zoom);
    rule();

    // ---- analysis ----
    auto* filter = toolMark(Glyph::Filter, tr("Süz — ifade çubuğuna gider"), bar);
    connect(filter, &QToolButton::clicked, this, [this] { filter_->setFocus(); });
    row->addWidget(filter);
    auto* calculator = toolMark(Glyph::Function, QString(), bar);
    pending(calculator, tr("Alan hesaplayıcı"));
    row->addWidget(calculator);
    statsToggle_ = toolMark(Glyph::Sigma, tr("Alan istatistikleri panelini gösterir"), bar);
    statsToggle_->setCheckable(true);
    statsToggle_->setChecked(true);
    connect(statsToggle_, &QToolButton::toggled, this, [this](bool on) {
        statistics_->setVisible(on);
        if (on) refreshStatistics();
    });
    row->addWidget(statsToggle_);
    auto* columns = toolMark(Glyph::Grid, tr("Sütunlar — göster, gizle"), bar);
    connect(columns, &QToolButton::clicked, this, &AttributeTable::showColumnsMenu);
    row->addWidget(columns);
    rule();

    // ---- output ----
    auto* exportMark = toolMark(Glyph::Export, tr("Dışa aktar — DIŞAAKTAR"), bar);
    connect(exportMark, &QToolButton::clicked, this, [this] {
        // The rows of a table travel with their geometry: GeoPackage carries the
        // attributes, and the export window says so beside the format.
        ExportDialog window(controller_, ExportSubject::Drawing, QString(), this);
        window.applyTheme(theme_);
        window.exec();
    });
    row->addWidget(exportMark);
    auto* print = toolMark(Glyph::Print, tr("Yazdır — YAZDIR"), bar);
    connect(print, &QToolButton::clicked, this, line(QStringLiteral("YAZDIR")));
    row->addWidget(print);

    row->addStretch(1);

    // The `Tablo | Form` pair: the same rows, all at once or one at a time. ONE
    // control, because it is one choice — two loose buttons that happened to
    // touch rounded a corner each and read as two things.
    auto* view = new Segment(bar);
    view->addOption(tr("Tablo"), tr("Satırlar bir ızgarada"));
    view->addOption(tr("Form"), tr("Bir kayıt, alan alan — Faz 2'de gelecek"));
    view->setControlSize(ControlSize::Regular);
    connect(view, &Segment::currentChanged, this, [this, view](int index) {
        // Said, not silently ignored: the form view is Phase 2 and the control
        // goes back to the table rather than pretending it changed something.
        if (index != 1) return;
        complain(tr("Form görünümü Faz 2'de gelecek; şimdilik tablo."));
        view->setCurrent(0);
    });
    row->addWidget(view);

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
    mark->setObjectName(QStringLiteral("filterMark"));
    mark->setPixmap(
        glyph_pixmap(Glyph::Function, tokensOf(theme_).accent, 16, devicePixelRatioF()));
    row->addWidget(mark);

    filter_ = new ExpressionEdit(bar);
    filter_->setPlaceholderText(
        tr("Süzme ifadesi — \"alan_m2\" > 2000 AND \"plan_fonksiyon\" = 'Konut'"));
    connect(filter_, &ExpressionEdit::applied, this, &AttributeTable::applyFilter);
    row->addWidget(filter_, 1);

    auto* apply = new Button(ButtonRole::Primary, tr("Filtrele"), Glyph::Filter, bar);
    connect(apply, &QPushButton::clicked, this, &AttributeTable::applyFilter);
    row->addWidget(apply);

    // A REAL MENU ARROW, drawn by the style, instead of a `▾` typed into the
    // label: the glyph in the text was a picture of a menu on a button that had
    // none. It still opens nothing — the saved filters are Phase 2 — and says so.
    auto* save  = new Button(ButtonRole::Secondary, tr("Kaydet"), Glyph::Save, bar);
    auto* saved = new QMenu(save);
    saved->addAction(tr("Kayıtlı süzgeçler — Faz 2'de gelecek"))->setEnabled(false);
    save->setMenuArrow(saved);
    save->setToolTip(tr("Kayıtlı süzgeçler Faz 2'de gelecek."));
    save->setEnabled(false);
    row->addWidget(save);

    search_ = new QLineEdit(bar);
    search_->setObjectName(QStringLiteral("tableSearch"));
    search_->setPlaceholderText(tr("Tabloda ara…")); // ui-label
    search_->setClearButtonEnabled(true);
    search_->setFixedWidth(200);
    search_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    search_->setAccessibleName(tr("Tabloda ara"));
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        model_->setSearch(text);
        refreshStatistics();
    });
    row->addWidget(search_);

    return bar;
}

QWidget* AttributeTable::buildStatistics()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("statsPanel"));
    panel->setFixedWidth(kStatsWidth);

    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(14, 10, 14, 12);
    column->setSpacing(10);

    // The head: the sign, the name, and a grip at the far end — §9 draws the
    // panel dockable, and the grip says so before anyone drags it.
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    auto* sign = new QLabel(panel);
    sign->setObjectName(QStringLiteral("statsSign"));
    head->addWidget(sign);
    auto* title = new QLabel(tr("Alan İstatistikleri"), panel);
    title->setObjectName(QStringLiteral("statsTitle"));
    head->addWidget(title);
    head->addStretch(1);
    auto* grip = new QLabel(panel);
    grip->setObjectName(QStringLiteral("statsGrip"));
    head->addWidget(grip);
    column->addLayout(head);

    statsField_ = new QLabel(panel);
    statsField_->setObjectName(QStringLiteral("statsField"));
    column->addWidget(statsField_);

    histogram_ = new Histogram(panel);
    column->addWidget(histogram_);

    // Nine measures, key at the left in the dim ink, figure at the right in mono.
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(9);
    const char* const kKeys[] = {"Nesne sayısı", "Geçerli değer", "NULL",
                                 "Minimum",      "Maksimum",      "Ortalama",
                                 "Ortanca",      "Std. sapma",    "Toplam"};
    int at                    = 0;
    for (const char* key : kKeys) {
        auto* name = new QLabel(tr(key), panel);
        name->setObjectName(QStringLiteral("statsKey"));
        auto* value = new QLabel(QStringLiteral("—"), panel);
        value->setObjectName(QStringLiteral("statsValue"));
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(name, at, 0);
        grid->addWidget(value, at, 1);
        statValues_.push_back(value);
        ++at;
    }
    grid->setColumnStretch(1, 1);
    column->addLayout(grid);
    column->addStretch(1);

    return panel;
}

void AttributeTable::buildFooter()
{
    // ---- the pager, at the left: first · previous · range · next · last ----
    auto* pagerBox = new QWidget(this);
    auto* pager    = new QHBoxLayout(pagerBox);
    pager->setContentsMargins(4, 0, 0, 0);
    pager->setSpacing(2);
    const auto mark = [this, pagerBox, pager](Glyph glyph, const QString& tip, int direction) {
        auto* b = new QToolButton(pagerBox);
        b->setObjectName(QStringLiteral("pagerMark"));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setProperty("glyph", static_cast<int>(glyph));
        connect(b, &QToolButton::clicked, this, [this, direction] { page(direction); });
        pager->addWidget(b);
    };
    mark(Glyph::PageFirst, tr("İlk sayfa"), -2);
    mark(Glyph::ChevronLeft, tr("Önceki sayfa"), -1);
    pager_ = new QLabel(pagerBox);
    pager_->setObjectName(QStringLiteral("mono"));
    pager_->setMinimumWidth(120);
    pager_->setAlignment(Qt::AlignCenter);
    pager->addWidget(pager_);
    mark(Glyph::ChevronRight, tr("Sonraki sayfa"), 1);
    mark(Glyph::PageLast, tr("Son sayfa"), 2);
    footer()->insertWidget(0, pagerBox);

    // ---- the two scope switches ----
    onlySelected_ = new CheckBox(tr("Yalnızca seçiliyi göster"), this);
    connect(onlySelected_, &QAbstractButton::toggled, this, [this](bool on) {
        model_->setOnlySelected(on);
        refreshStatistics();
    });
    footer()->insertWidget(1, onlySelected_);

    followMap_ = new CheckBox(tr("Haritayla eşitle"), this);
    followMap_->setChecked(true);
    followMap_->setToolTip(tr("Tuvalde seçilen nesnenin satırı tabloda da seçilir"));
    connect(followMap_, &QAbstractButton::toggled, this, [this](bool on) {
        if (on) followCanvas();
    });
    footer()->insertWidget(2, followMap_);

    // What the last entry was refused for, or what the row it landed in is still
    // missing. In the warn colour, because it is about something the user just
    // did rather than about something that is broken.
    complaint_ = new QLabel(this);
    complaint_->setObjectName(QStringLiteral("warning"));
    complaint_->setVisible(false);
    footer()->insertWidget(3, complaint_);

    // ---- the column summary, at the right ----
    summary_ = new QLabel(this);
    summary_->setObjectName(QStringLiteral("mono"));
    footer()->addWidget(summary_);
}

void AttributeTable::applyFilter()
{
    model_->setFilter(filter_->expression());
    if (!model_->filterError().isEmpty()) {
        filter_->setToolTip(model_->filterError());
        complain(model_->filterError());
    } else {
        filter_->setToolTip(QString());
        complain(QString());
    }
    refreshCounts();
    refreshStatistics();
}

void AttributeTable::pushSelection()
{
    if (pushing_) return;
    QStringList keys;
    for (const QModelIndex& index : view_->selectionModel()->selectedRows())
        keys << QString::number(static_cast<qulonglong>(model_->keyAt(index.row())));
    if (keys.isEmpty()) return;

    pushing_ = true;
    controller_.runLine(QStringLiteral("SEÇ nesneler=%1").arg(keys.join(QLatin1Char(','))),
                        command::Origin::Gui);
    pushing_ = false;
}

void AttributeTable::followCanvas()
{
    if (pushing_ || followMap_ == nullptr || !followMap_->isChecked()) return;

    // The canvas's selection, as rows. Nothing is sent back: `pushing_` is not
    // needed because the selection model is changed under a blocker, and the
    // blocker is what keeps the grid from answering the canvas with a `SEÇ`.
    QSignalBlocker quiet(view_->selectionModel());
    view_->selectionModel()->clearSelection();
    const command::Selection& picked = controller_.bus().selection();
    QItemSelection rows;
    for (const core::EntityKey key : picked.keys()) {
        const int row = model_->rowOf(key);
        if (row < 0) continue;
        rows.select(model_->index(row, 0), model_->index(row, model_->columnCount() - 1));
    }
    view_->selectionModel()->select(rows, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    view_->viewport()->update();
    view_->verticalHeader()->viewport()->update();
    refreshCounts();
    refreshStatistics();
}

void AttributeTable::deleteSelectedRows()
{
    QStringList keys;
    for (const QModelIndex& index : view_->selectionModel()->selectedRows())
        keys << QString::number(static_cast<qulonglong>(model_->keyAt(index.row())));
    if (keys.isEmpty()) {
        complain(tr("Silinecek satır seçili değil."));
        return;
    }
    // `SİL` asks its own confirmation when the preference says so, and undoes in
    // one step — the table adds nothing to that and takes nothing from it.
    controller_.runLine(QStringLiteral("SİL nesneler=%1").arg(keys.join(QLatin1Char(','))),
                        command::Origin::Gui);
}

void AttributeTable::invertSelection()
{
    QItemSelection inverted;
    for (int row = 0; row < model_->rowCount(); ++row) {
        if (view_->selectionModel()->isRowSelected(row, QModelIndex())) continue;
        inverted.select(model_->index(row, 0), model_->index(row, model_->columnCount() - 1));
    }
    view_->selectionModel()->select(inverted, QItemSelectionModel::ClearAndSelect |
                                                  QItemSelectionModel::Rows);
    if (inverted.isEmpty())
        controller_.runLine(QStringLiteral("SEÇ TEMİZLE"), command::Origin::Gui);
}

void AttributeTable::showColumnsMenu()
{
    QMenu menu(this);
    for (int c = 1; c < model_->columnCount(); ++c) {
        QAction* entry =
            menu.addAction(model_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
        entry->setCheckable(true);
        entry->setChecked(!view_->isColumnHidden(c));
        connect(entry, &QAction::toggled, this,
                [this, c](bool on) { view_->setColumnHidden(c, !on); });
    }
    menu.exec(QCursor::pos());
}

void AttributeTable::page(int direction)
{
    QScrollBar* bar = view_->verticalScrollBar();
    switch (direction) {
    case -2: bar->setValue(bar->minimum()); break;
    case -1: bar->setValue(bar->value() - bar->pageStep()); break;
    case 1: bar->setValue(bar->value() + bar->pageStep()); break;
    case 2: bar->setValue(bar->maximum()); break;
    default: break;
    }
    refreshCounts();
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
    while (column < model_->columnCount() && view_->isColumnHidden(column))
        ++column;
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

void AttributeTable::refreshCounts()
{
    const int shown    = model_->rowCount();
    const int selected = view_->selectionModel() != nullptr
                             ? static_cast<int>(view_->selectionModel()->selectedRows().size())
                             : 0;

    // THE VISIBLE RANGE, read off the viewport: the grid is virtual and scrolls,
    // so the pager reports where the eye is rather than turning pages of its
    // own. `1 – 18 / 1 482`, §9.
    if (pager_ != nullptr) {
        const int first = shown == 0 ? 0 : std::max(0, view_->rowAt(0)) + 1;
        int last        = view_->rowAt(view_->viewport()->height() - 1);
        if (last < 0) last = shown - 1;
        pager_->setText(shown == 0 ? tr("0 / 0")
                                   : tr("%1 – %2 / %3")
                                         .arg(first)
                                         .arg(std::min(last + 1, shown))
                                         .arg(grouped(shown, 0)));
    }

    // The title says the whole state in one mono line, §9: objects, selected,
    // edited — each only when it has something to say.
    QString summary = tr("%1 nesne").arg(grouped(shown, 0));
    if (selected > 0) summary += tr(" · %1 seçili").arg(selected);
    if (model_->editedCount() > 0) summary += tr(" · %1 düzenlendi").arg(model_->editedCount());
    setHeading(Glyph::Table, tr("Öznitelik Tablosu"),
               tr("— %1   %2").arg(layerName_.isEmpty() ? tr("tüm çizim") : layerName_, summary));

    if (deleteRows_ != nullptr) deleteRows_->setEnabled(selected > 0);
}

void AttributeTable::refreshStatistics()
{
    if (statistics_ == nullptr || !statistics_->isVisibleTo(this)) return;

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

    const auto put = [this](int at, const QString& value) {
        if (at < statValues_.size()) statValues_[at]->setText(value);
    };

    if (numbers.isEmpty()) {
        put(0, grouped(model_->rowCount(), 0));
        put(1, grouped(0, 0));
        put(2, grouped(nulls, 0));
        for (int at = 3; at < statValues_.size(); ++at)
            put(at, QStringLiteral("—"));
        summary_->setText(QString());
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

    put(0, grouped(model_->rowCount(), 0));
    put(1, grouped(static_cast<double>(numbers.size()), 0));
    put(2, grouped(nulls, 0));
    put(3, grouped(numbers.front()));
    put(4, grouped(numbers.back()));
    put(5, grouped(mean));
    put(6, grouped(median));
    put(7, grouped(std::sqrt(variance)));
    put(8, grouped(total));

    summary_->setText(tr("Σ %1 = %2      x̄ = %3").arg(name, grouped(total), grouped(mean)));
}

void AttributeTable::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    DialogFrame::applyTheme(mode);
    if (view_ != nullptr) view_->applyTheme(mode);

    // Every drawn mark re-tints with the theme; each carries its glyph in a
    // property, which keeps this from being a second list of buttons.
    const Tokens& t = tokensOf(mode);
    for (QToolButton* button : findChildren<QToolButton*>()) {
        const QVariant glyph = button->property("glyph");
        if (!glyph.isValid()) continue;
        const int px = button->objectName() == QLatin1String("pagerMark") ? 16 : kToolIcon;
        button->setIcon(icon(static_cast<Glyph>(glyph.toInt()), t.textDim, t.accent, px));
    }
    if (auto* mark = findChild<QLabel*>(QStringLiteral("filterMark")); mark != nullptr)
        mark->setPixmap(glyph_pixmap(Glyph::Function, t.accent, 16, devicePixelRatioF()));
    if (auto* sign = findChild<QLabel*>(QStringLiteral("statsSign")); sign != nullptr)
        sign->setPixmap(glyph_pixmap(Glyph::Sigma, t.accent, 16, devicePixelRatioF()));
    if (auto* grip = findChild<QLabel*>(QStringLiteral("statsGrip")); grip != nullptr)
        grip->setPixmap(glyph_pixmap(Glyph::Grip, t.textFaint, 14, devicePixelRatioF()));
    refreshStatistics();
}

} // namespace kentos::app
