// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the attribute schema, as a page of Katman Özellikleri.
//
// WHAT THIS IS. The columns a drawing's objects can carry — `ada`, `parsel`,
// `taks`, `onay_tarihi` — listed, added, edited and deleted. It is the schema
// half of the attribute story; the VALUES are entered in the object inspector
// and in the attribute table, and both of those only offer a row for a column
// that was declared here first.
//
// THE SCHEMA IS THE DOCUMENT'S, NOT THE LAYER'S, and this page says so rather
// than pretending otherwise. `model.md` R27 puts the schema on the COLLECTION —
// which is what OGRFeatureDefn and Blender's CustomData both do — and this
// document has one collection. A column declared here is offered on every
// object in the drawing, and the layer whose properties are open is where the
// user happened to be standing when they declared it. A per-layer schema is a
// model change and not a panel change.
//
// EVERY EDIT LEAVES AS A COMMAND. `SÜTUN` declares, amends and drops; this page
// builds those lines and hands them to the bus (CLAUDE.md 1.1, 1.2, 5.9), so a
// script can do everything this page can.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/theme.hpp"

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// One inline editor; see fields.hpp.
class Field;

/// Declares or edits one column. Modal, and it writes nothing itself: the caller
/// reads `line()` and sends it.
class ColumnDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// `existing` empty declares a new column; otherwise it edits that one, with
    /// the id and the type locked — see `AttrColumn::amend` for why.
    ColumnDialog(Controller& controller, QString existing, QWidget* parent = nullptr);

    /// The `SÜTUN` line the dialog was left describing, or empty when it holds
    /// nothing that could be sent.
    QString line() const;

    void applyTheme(ThemeMode mode) override;

private:
    /// Shows or hides the rows that only one type reads — `basamak` for a
    /// decimal, `katalog` for a code.
    void syncTypeRows();

    Controller& controller_;
    QString existing_;

    Field* id_{nullptr};
    Field* label_{nullptr};
    Field* type_{nullptr};
    Field* digits_{nullptr};
    Field* catalog_{nullptr};
    Field* required_{nullptr};
    Field* about_{nullptr};

    QLabel* digitsLabel_{nullptr};
    QLabel* catalogLabel_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

/// The list of columns, with the three buttons that change it.
class SchemaPage : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the page over a controller, which outlives it.
    explicit SchemaPage(Controller& controller, QWidget* parent = nullptr);

    /// Re-reads the schema from the document. Called when the page is shown and
    /// after every change, because the document is the only copy of it.
    void refresh();

    void applyTheme(ThemeMode mode) override;

    /// Runs one of the three actions by name, for `KENTOS_SCHEMA_PROBE`.
    /// `row` picks the column the action applies to. Returns false when the
    /// action or the row is not there.
    bool probeAction(const QString& action, int row, const QString& line);

    /// What the table shows, one row per line, for the same probe.
    QStringList probeRows() const;

private:
    /// Opens `ColumnDialog` and sends what it was left holding.
    void declareOrEdit(const QString& existing);

    /// Asks first, then sends `SÜTUN kimlik=… sil=evet`.
    void dropSelected();

    /// The column id on the current row, or empty when there is no current row.
    QString currentId() const;

    Controller& controller_;
    QTableWidget* table_{nullptr};
    QPushButton* add_{nullptr};
    QPushButton* edit_{nullptr};
    QPushButton* drop_{nullptr};
    QLabel* note_{nullptr};
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
