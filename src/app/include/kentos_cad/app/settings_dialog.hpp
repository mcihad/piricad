// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the settings window.
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

#include "kentos_cad/core/settings.hpp"

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QDialog>
#include <QString>

#include <cstdint>
#include <vector>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QVBoxLayout;
class QWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Shows declared settings, grouped by topic, and writes changes through the
/// command bus.
///
/// TWO WINDOWS, ONE CLASS, and that is deliberate. `Seçenekler` and `Proje
/// Ayarları` answer different questions — "how do I want this program to behave"
/// and "what travels inside this file" — and they are separate windows because a
/// person asking the second one is usually about to hand the file to somebody.
/// But every row in both is the same row: built from the same catalogue, written
/// through the same command, marked and reset the same way. Two classes would be
/// two copies of that (CLAUDE.md 5.10), and the second copy is the one that
/// stops offering a setting the first one gained.
class SettingsDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Which of the two windows this instance is.
    enum class Mode : std::uint8_t {
        All,     ///< `Seçenekler`: every declared setting, by topic
        Project, ///< `Proje Ayarları`: what the .pcad file carries, and its schema
    };

    /// Opens on the first section, with the settings `mode` selects already
    /// listed.
    explicit SettingsDialog(Controller& controller, Mode mode = Mode::All,
                            QWidget* parent = nullptr);

    /// The sidebar's section titles, in order, for `KENTOS_SETTINGS_PROBE`.
    QStringList probeSections() const;

    /// The ids of the settings the `Proje Ayarları` page carries, in order.
    ///
    /// Read from the catalogue the page is built from rather than from the
    /// widgets, because what is being checked is the RULE — every project-scoped
    /// setting, and nothing else — and a page that built itself from a hand-kept
    /// list would pass a widget count while breaking exactly that.
    QStringList probeProjectSettings() const;

private:
    /// One editable setting: the widgets that show it and the spec behind them.
    struct Row
    {
        const core::SettingSpec* spec{nullptr};
        QWidget* line{nullptr};   ///< the whole row, hidden when the search filters it
        QLabel* state{nullptr};   ///< "ayarlanmış" / "varsayılan"
        QWidget* editor{nullptr}; ///< the type's own widget
    };

    /// One section of the left-hand list: a group of the catalogue, its page,
    /// and whether anything in it has been changed since the window opened.
    struct Section
    {
        std::string group;      ///< the id's second component: `yakalama`, `izgara`
        QString title;          ///< what the section list and the page heading say
        QWidget* page{nullptr}; ///< the scroll area shown when it is chosen
    };

    /// Builds the page for one group and returns it. Never null: a group only
    /// exists because a setting declared it.
    QWidget* buildGroup(const std::string& section, const QString& title);

    /// The page that gathers every PROJECT-scoped setting, whatever topic it was
    /// declared under.
    ///
    /// WHY A SECOND VIEW OF THE SAME SETTINGS. The pages above are cut by topic,
    /// which is how a person looks for one setting: `çizim birimi` is found under
    /// `Genel`, beside the other general things. But "what travels with this
    /// file" is a different question and a real one — it is the answer somebody
    /// needs before handing a `.pcad` to a colleague, and topic pages scatter it
    /// across five of them.
    ///
    /// It is NOT a second list (CLAUDE.md 5.10): both views are generated from
    /// the one settings catalogue, and a row here writes the same setting through
    /// the same command as the row on its topic page. Adding a setting adds it to
    /// both with no edit here.
    QWidget* buildProjectPage();

    /// Builds the page of a section that has no settings yet: the phase it
    /// arrives in and one line saying what will be on it.
    QWidget* buildPending(const QString& phase, const QString& note);

    /// Adds one setting's row to `into`, and records it for the search.
    ///
    /// The row is `design.md` §10's: the name and its one-line help on the left,
    /// the control right-aligned, and the reset mark after it. A `Bool` gets a
    /// `ToggleSwitch` rather than a tick box, because §10 draws a switch.
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

    /// Which window this is; see `Mode`.
    Mode mode_{Mode::All};
    /// §10's left column: a search field over a list of sections, with the
    /// pages themselves in a stack the list switches.
    SectionList* sections_{nullptr};
    QStackedWidget* pages_{nullptr};
    QLineEdit* search_{nullptr};
    QLabel* heading_{nullptr};
    QLabel* summary_{nullptr};
    QLabel* profile_{nullptr};
    std::vector<Section> order_;

    /// Guards the editors while `refresh()` fills them, so a programmatic write
    /// does not read straight back as a user edit.
    bool loading_{false};

    std::vector<Row> rows_;
};

} // namespace kentos::app
