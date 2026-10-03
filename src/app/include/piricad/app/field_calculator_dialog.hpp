// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the field calculator window (TODOS G-03).
//
// THE WINDOW IS A FACE ON ÖZNİTELİKHESAPLA. It never evaluates an expression and never writes a
// cell: "Önizle" sends the command with `onizle=evet` and fills its table from the command's
// structured answer (`Context::report`), "Uygula" sends the same line without it. So the preview a
// hand sees here is the preview a script is told, row for row, and the calculation is one journal
// line and one undo step (Article 1.2, 5.9).
//
// THE PREVIEW IS THE GATE, as in the find-and-replace window: "Uygula" is enabled only while the
// fields say what the table shows. Change the column, the expression, the filter or the scope and
// it goes grey until Önizle is pressed again.
//
// THE LISTS ARE READ, NOT KEPT. The columns come from the document's schema and the functions from
// `command::expression_functions()`, the very table the parser reads its function names from — a
// function the window lists is a function the language has.
#pragma once

#include "piricad/app/dialog_chrome.hpp"

#include <QString>

#include <cstdint>

class QLabel;
class QListWidget;
class QTableWidget;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp and fields.hpp.
class Button;
class Field;
class FormRow;
class FormSection;

/// The expression bar; see expression_edit.hpp.
class ExpressionEdit;

/// Modeless: the table it was opened from stays in reach and shows the new values the moment the
/// command has written them.
class FieldCalculatorDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on `layerName` (empty: the whole drawing), with `filter` as the starting row filter.
    FieldCalculatorDialog(Controller& controller, QString layerName, QString filter,
                          QWidget* parent = nullptr);

    /// What a button asks of the command.
    enum class Step : std::uint8_t {
        Preview, ///< what would be written, nothing written
        Apply,   ///< write it
    };

    /// Replaces the row filter, as when the window is opened again over a table whose bar changed.
    void setFilter(const QString& filter);

    /// Repaints the window and its controls in `mode`.
    void applyTheme(ThemeMode mode) override;

    /// Fills the fields and clicks the button of `step`, as a hand would, for the probe that proves
    /// the window end to end. A disabled button stays unclicked, exactly as under the mouse.
    void runForProbe(const QString& column, const QString& expression, Step step);

    /// How many example rows the table shows.
    int previewRows() const;

    /// Whether "Uygula" can be pressed now.
    bool applyEnabled() const;

    /// The sentence under the fields.
    QString summaryText() const;

    /// How many columns the window offers as targets.
    int columnChoices() const;

    /// How many functions the window lists.
    int functionCount() const;

    /// Double-clicks the function at `row` as a hand would, and returns what the expression bar
    /// holds.
    QString insertFunctionForProbe(int row);

signals:
    /// The calculation was written; the table behind this window re-reads.
    void applied();

private:
    /// The command line the fields say for `step`.
    QString commandLine(Step step) const;

    /// Runs the line and puts its answer in the table and the summary.
    void run(Step step);

    /// Greys "Uygula" out unless the fields still say what was previewed.
    void refreshApply();

    /// Puts `text` into the expression bar at the cursor.
    void insertText(const QString& text);

    Controller& controller_;
    QString layer_;
    Field* column_{nullptr};
    Field* scope_{nullptr};
    Field* filter_{nullptr};
    ExpressionEdit* expression_{nullptr};
    FormRow* expressionRow_{nullptr};
    QListWidget* columns_{nullptr};
    QListWidget* functions_{nullptr};
    QLabel* summary_{nullptr};
    QTableWidget* table_{nullptr};
    Button* preview_{nullptr};
    Button* apply_{nullptr};
    QString previewed_; ///< the preview line the table shows, empty when none
};

} // namespace piricad::app
