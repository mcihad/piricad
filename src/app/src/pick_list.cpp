// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/pick_list.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/measure_text.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <algorithm>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPushButton>
#include <QTableWidget>

namespace kentos::app {
namespace {

/// The four things a person needs to tell two overlapping objects apart, in the
/// order they help: what it is, what it belongs to, how big it is, and its id.
///
/// THE ID IS LAST AND IT IS STILL THERE. It is the one column that is never
/// ambiguous — two parcels can share a layer, a type and an area to the square
/// centimetre — and it is what the user types into `SEÇ NESNE` afterwards if they
/// want the same object again tomorrow.
enum Column : int { Type = 0, Layer, Size, Fid, ColumnCount };

/// The attribute table's own metrics, because this IS that table at dialog size.
constexpr int kHeaderRow = 30;
constexpr int kTableRow  = 28;
constexpr int kFooterRow = 34;

/// FLOORS, not widths. `ResizeToContents` measures the longest string in a column
/// and stops there, which on a four-row list means `Tür` is as wide as the word
/// `ALAN` and `Kimlik` as wide as `2` — three cramped columns beside one that
/// takes the rest. A column has to be wide enough for what it will hold on the
/// NEXT drawing too, not just this one.
/// How tall the window is: enough for its rows, never a field of empty ground.
constexpr int kRowsMin      = 3;
constexpr int kRowsMax      = 10;
constexpr int kWindowWidth  = 660;
constexpr int kChromeHeight = 58; ///< the frame, the heading strip and the margins

constexpr int kTypeFloor  = 128;
constexpr int kSizeFloor  = 132;
constexpr int kFidFloor   = 78;
constexpr int kCellMargin = 24;

/// Grouping is the attribute table's own, shared: the two print figures into the
/// same kind of column and a chooser that grouped differently would look like a
/// different program.
using measure::spacedThousands;

} // namespace

PickList::PickList(Controller& controller, const std::vector<core::EntityId>& candidates,
                   QWidget* parent)
    : DialogFrame(parent), controller_(controller)
{
    setHeading(Glyph::Select, tr("Hangisi?"),
               tr("— imlecin altında %1 nesne").arg(candidates.size()));
    setFooterHeight(kFooterRow + 14);

    build(candidates);

    auto* pick = new Button(ButtonRole::Primary, tr("Seç"), Glyph::Check, this);
    pick->setDefault(true);
    footer()->addWidget(pick);
    connect(pick, &QPushButton::clicked, this, &QDialog::accept);

    auto* cancel = new Button(ButtonRole::Secondary, tr("Vazgeç"), std::nullopt, this);
    footer()->addWidget(cancel);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    // LIVE, not on confirm. The list describes the candidates in words and the
    // canvas shows them in ink; a chooser that only revealed its answer after the
    // window closed would make the user open it again to check.
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0 || row >= static_cast<int>(keys_.size())) return;
        emit highlighted(keys_[static_cast<std::size_t>(row)]);
    });

    // A double click means "this one" — the same gesture that opens a file in
    // every list a user has ever met.
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { accept(); });

    connect(this, &QDialog::accepted, this, [this] { picked_ = currentKey(); });

    // AS TALL AS IT HAS ROWS. A chooser sized for ten and holding three is a
    // window that is mostly empty ground, and the emptiness reads as the list
    // having failed to fill rather than as there being three answers. Floored at
    // three so one candidate is not a slot, capped at ten so a click in a very
    // busy drawing scrolls instead of covering the canvas it is asking about.
    const int shown = std::clamp(table_->rowCount(), kRowsMin, kRowsMax);
    resize(kWindowWidth, kHeaderRow + shown * kTableRow + kFooterRow + kChromeHeight);

    // The first row is the one `pick_nearest` would have chosen on its own, so
    // pressing Enter straight away is exactly the old behaviour.
    if (table_->rowCount() > 0) table_->setCurrentCell(0, Column::Type);
    table_->setFocus(Qt::OtherFocusReason);
}

void PickList::build(const std::vector<core::EntityId>& candidates)
{
    const core::Document& doc = controller_.document();

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("pickList"));
    table_->setColumnCount(Column::ColumnCount);
    table_->setHorizontalHeaderLabels({tr("Tür"), tr("Katman"), tr("Ölçü"), tr("Kimlik")});

    // The heading sits over the column it names: a centred `Ölçü` above a column
    // of right-aligned figures points at nothing in particular.
    for (const int column : {Column::Size, Column::Fid})
        table_->horizontalHeaderItem(column)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kTableRow);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->horizontalHeader()->setFixedHeight(kHeaderRow);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setShowGrid(true);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);

    // ROW SELECTION, ONE ROW AT A TIME. The window answers one question and the
    // answer is one object; a multi-select here would be a second selection
    // model beside the document's own.
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    table_->setRowCount(static_cast<int>(candidates.size()));
    keys_.reserve(candidates.size());

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const core::EntityId e    = candidates[i];
        const auto row            = static_cast<int>(i);
        const std::uint32_t slot  = doc.entities().slot[e];
        const core::EntityKey key = doc.key_of(e);
        keys_.push_back(key);

        QString layer;
        if (const core::LayerId on = doc.entities().layer[e]; on < doc.layers().size())
            layer = QString::fromStdString(doc.layers()[on].name);

        // WORDS LEFT IN THE UI FACE, NUMBERS RIGHT IN MONO — the object
        // inspector's own split, and it is what makes a column of areas
        // readable: the digits line up under each other and the decimal point
        // is in one place down the column.
        const auto cell = [this, row](int column, const QString& text, bool numeric) {
            auto* item = new QTableWidgetItem(numeric ? spacedThousands(text) : text);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setTextAlignment(numeric ? (Qt::AlignRight | Qt::AlignVCenter)
                                           : (Qt::AlignLeft | Qt::AlignVCenter));
            if (numeric) {
                QFont face(QStringLiteral("IBM Plex Mono"));
                face.setStyleHint(QFont::Monospace);
                face.setPixelSize(12);
                item->setFont(face);
            }
            table_->setItem(row, column, item);
        };

        cell(Column::Type, measure::shapeName(doc, doc.entities().kind[e], slot), false);
        cell(Column::Layer, layer, false);
        cell(Column::Size, measure::sizeSummary(doc, e), true);
        cell(Column::Fid, QString::number(static_cast<std::uint64_t>(key)), true);
    }

    // MEASURED, THEN GIVEN ROOM, THEN HELD ABOVE A FLOOR. Contents alone leaves
    // three narrow columns beside one wide one, and the selected row draws
    // wider than an unselected one — which is how `3600.00 m²` came out as
    // `3600.00 …` on the only row the user was looking at.
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(Column::Layer, QHeaderView::Stretch);

    const auto atLeast = [this](int column, int floor) {
        table_->setColumnWidth(column, std::max(table_->columnWidth(column) + kCellMargin, floor));
    };
    atLeast(Column::Type, kTypeFloor);
    atLeast(Column::Size, kSizeFloor);
    atLeast(Column::Fid, kFidFloor);

    setBody(table_);
}

core::EntityKey PickList::currentKey() const
{
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(keys_.size())) return core::EntityKey::None;
    return keys_[static_cast<std::size_t>(row)];
}

void PickList::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    DialogFrame::applyTheme(mode);
}

} // namespace kentos::app
