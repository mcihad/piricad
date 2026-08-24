// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the settings window.
//
// GENERATED FROM THE CATALOGUE, not written out. Every row in this window comes
// from `core::builtin_settings()`: its label is the setting's own primary name,
// its editor is chosen from the declared type, its limits are the declared range
// and its help is the declared summary. A setting added to `settings.cpp` appears
// here with no edit to this file, and one removed disappears — which is the same
// rule CLAUDE.md 5.10 puts on the command list, for the same reason. A dialog
// carrying its own copy of the list is a second list, and a second list is a list
// that will disagree.
//
// EVERY EDIT LEAVES AS A COMMAND. A checkbox does not write a store; it dispatches
// `TERCİH <ad> <değer>`, `AYAR <ad> <değer>` or `MOD <ad> <değer>` through the
// controller, exactly as if the user had typed it. The scope decides which of the
// three, because the scope is what says who owns the value (model.md R39, R40).
// So the journal cannot tell this window from the command line, and neither can a
// replay (Article 1.1, 1.2).
#pragma once

#include "piricad/core/settings.hpp"

#include <QDialog>
#include <QString>

#include <vector>

class QLabel;
class QLineEdit;
class QTabWidget;
class QVBoxLayout;
class QWidget;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Shows every declared setting, grouped by who owns it, and writes changes
/// through the command bus.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// Opens on the Project page, with every declared setting already listed.
    explicit SettingsDialog(Controller& controller, QWidget* parent = nullptr);

private:
    /// One editable setting: the widgets that show it and the spec behind them.
    struct Row
    {
        const core::SettingSpec* spec{nullptr};
        QWidget* line{nullptr};   ///< the whole row, hidden when the search filters it
        QLabel* state{nullptr};   ///< "ayarlanmış" / "varsayılan"
        QWidget* editor{nullptr}; ///< the type's own widget
    };

    /// Builds the page for one scope and returns it, or null when the scope
    /// declares nothing.
    QWidget* buildScope(core::SettingScope scope);

    /// Adds one setting's row to `into`, and records it for the search.
    void addRow(QVBoxLayout* into, const core::SettingSpec& spec);

    /// Reads the store for `spec`'s scope. The dialog never caches a value: a
    /// setting changed from the command line while this window is open must show
    /// through, and the store is the only place that knows.
    const core::Settings& storeOf(core::SettingScope scope) const;

    /// The command word that writes `scope` — TERCİH, AYAR or MOD.
    static QString commandFor(core::SettingScope scope);

    /// Dispatches one write and refreshes what the window shows.
    void write(const core::SettingSpec& spec, const QString& value);

    /// Puts every editor back in step with its store.
    void refresh();

    /// Hides the rows that do not match what is typed in the search box.
    void applyFilter();

    Controller& controller_;
    QTabWidget* tabs_{nullptr};
    QLineEdit* search_{nullptr};

    /// Guards the editors while `refresh()` fills them, so a programmatic write
    /// does not read straight back as a user edit.
    bool loading_{false};

    std::vector<Row> rows_;
};

} // namespace piricad::app
