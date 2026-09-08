// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the attribute table window, `design.md` §9.
//
// A layer's rows and columns, with a filter bar over them and field statistics
// beside them. It is a READER of the document with exactly one way to write:
// every edited cell dispatches `ÖZNİTELİK` through the bus, so the journal
// cannot tell a cell edited here from the same value typed at the prompt
// (CLAUDE.md 1.2, 5.9).
//
// THE FILTER IS THE ONE GRAMMAR. `"alan_m2" > 2000 AND "plan_fonksiyon" =
// 'Konut'` is read by `command::evaluate_predicate`, which lives in
// `parser.cpp` beside the arithmetic evaluator — 5.11 allows exactly one
// grammar in this product and an attribute filter is not an exception to it.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/identity.hpp"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

class QLabel;
class QLineEdit;
class QTableView;
class QToolButton;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The document's attribute columns, as rows and columns.
///
/// It holds no copy of the data: every `data()` reads the document through the
/// controller, because a table that cached would disagree with a value written
/// from the command line while it was open.
class AttributeModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// Builds the model over a controller, which outlives it, showing `layerName`
    /// alone or the whole drawing when it is empty.
    explicit AttributeModel(Controller& controller, QString layerName = QString(),
                            QObject* parent = nullptr);

    /// Re-reads the document and re-applies the filter.
    void refresh();

    /// Keeps only the rows the predicate accepts. An empty filter keeps them
    /// all; a broken one is reported through `filterError` and changes nothing.
    void setFilter(const QString& expression);

    /// The last filter's complaint, or empty when it parsed.
    QString filterError() const noexcept { return error_; }

    /// The entity behind a row, for selecting it on the canvas.
    core::EntityKey keyAt(int row) const;

    /// Every value of one column, as text, for the statistics panel.
    QVector<QString> columnValues(int column) const;

    /// Whether cells may be opened at all.
    ///
    /// OFF UNTIL SOMEBODY ASKS. A grid of a thousand parcels is read far more
    /// often than it is written, and a double click that starts editing a
    /// cadastral value the reader only meant to look at is a change nobody
    /// intended and nobody notices. The mode is a deliberate act, and while it is
    /// off `flags()` does not mark a single cell editable — the refusal is in the
    /// model, not in the view's triggers, so it holds however the cell is reached.
    void setEditing(bool on);

    bool editing() const noexcept { return editing_; }

    /// What the column at `column` holds, for the delegate that opens its editor.
    /// Column 0 is `fid` and has no editor.
    FieldSpec fieldFor(int column) const;

    /// Checks one row against the schema: required cells present, codes known.
    /// Empty when it is sound, otherwise what is wrong with it, in Turkish.
    QString validateRow(int row) const;

    /// The same over every row the filter kept. Empty when the whole table is
    /// sound.
    QStringList validateAll() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

signals:
    /// The filter was re-applied: how many rows survived, out of how many.
    void filtered(int shown, int total);

    /// A cell was refused before it was ever sent, with the reason. The window
    /// shows it; the document was not touched.
    void rejected(const QString& reason);

    /// A cell was written and the row it belongs to was checked afterwards.
    /// `complaint` is empty when the row is sound.
    void rowChecked(int row, const QString& complaint);

private:
    Controller& controller_;
    QVector<core::EntityKey> rows_; ///< the rows the filter kept, in slot order
    QVector<core::AttrId> columns_; ///< every declared column, in declaration order
    QString filter_;
    QString error_;
    bool editing_{false};

    /// The layer the table is scoped to, or empty for the whole drawing.
    ///
    /// HELD BY NAME AND RESOLVED ON EVERY REFRESH, not resolved once into a
    /// `LayerId`: a layer id is a SLOT and slots move when layers are added or
    /// removed (model.md R1/R5), so a cached one would quietly start showing a
    /// different layer's rows.
    QString layer_;
};

/// The window `design.md` §9 draws around that model.
class AttributeTable : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on `layerName`, or on the whole document when it is empty.
    AttributeTable(Controller& controller, QString layerName, QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

    /// Drives the grid the way a hand does, for `KENTOS_TABLE_PROBE`: turn the
    /// mode on or off, type into the current cell, and read back where the
    /// cursor ended up.
    ///
    /// It exists because none of what was just built can be checked from a
    /// transcript: "a cell does not open while the mode is off" and "Enter lands
    /// on the next column" are both statements about a grid under a hand.
    QString probeGrid(const QString& action, const QString& value);

private:
    /// Builds the 44 px tool row above the filter bar.
    QWidget* buildToolRow();

    /// Builds the `fx` filter bar.
    QWidget* buildFilterBar();

    /// Builds the right-hand field statistics panel.
    QWidget* buildStatistics();

    /// Re-reads the chosen column and redraws the histogram and the summary.
    void refreshStatistics();

    /// Re-reads the counts in the title bar and the footer.
    void refreshCounts();

    /// Turns the edit mode on or off.
    ///
    /// TURNING IT OFF VALIDATES FIRST. Leaving the mode is the moment the user
    /// says "I am done with this table", and it is the last one at which a
    /// missing required value is still theirs to fix rather than a surprise in
    /// somebody else's save.
    void setEditing(bool on);

    /// Moves to the cell after `from` and opens it.
    ///
    /// ACROSS, THEN DOWN, THEN STOP — the way a ledger is filled in. The last
    /// column wraps to the next row's first editable column, which is column 1:
    /// `fid` is identity and never opens.
    void advanceFrom(const QModelIndex& from);

    /// Says something in the footer, in the warn colour, until the next entry.
    void complain(const QString& text);

    Controller& controller_;
    QString layerName_;

    AttributeModel* model_{nullptr};
    QTableView* view_{nullptr};
    QLineEdit* filter_{nullptr};
    QLineEdit* search_{nullptr};
    QLabel* pager_{nullptr};
    QLabel* summary_{nullptr};
    QLabel* complaint_{nullptr};
    QToolButton* editToggle_{nullptr};
    FieldDelegate* delegate_{nullptr};
    QWidget* statistics_{nullptr};
    QLabel* statsField_{nullptr};
    QLabel* statsBody_{nullptr};
    QWidget* histogram_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
