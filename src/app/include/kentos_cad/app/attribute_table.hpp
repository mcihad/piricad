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
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/identity.hpp"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

class QLabel;
class QLineEdit;
class QTableView;

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

private:
    Controller& controller_;
    QVector<core::EntityKey> rows_; ///< the rows the filter kept, in slot order
    QVector<core::AttrId> columns_; ///< every declared column, in declaration order
    QString filter_;
    QString error_;

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

    Controller& controller_;
    QString layerName_;

    AttributeModel* model_{nullptr};
    QTableView* view_{nullptr};
    QLineEdit* filter_{nullptr};
    QLineEdit* search_{nullptr};
    QLabel* pager_{nullptr};
    QLabel* summary_{nullptr};
    QWidget* statistics_{nullptr};
    QLabel* statsField_{nullptr};
    QLabel* statsBody_{nullptr};
    QWidget* histogram_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
