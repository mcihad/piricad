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
class QStandardItemModel;
class QVBoxLayout;
class QWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp, fields.hpp and datagrid.hpp.
class Button;
class ComboBox;
class DataGrid;
class Field;
class Segment;

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
    /// Opens the page whose section title is `title`; an unknown title leaves
    /// the window where it is. What the print menu's "Profilleri Yönet…" asks.
    void showSection(const QString& title);

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
        std::string group;        ///< the id's second component: `yakalama`, `izgara`
        QString title;            ///< what the section list and the page heading say
        QWidget* page{nullptr};   ///< the scroll area shown when it is chosen
        std::size_t first_row{0}; ///< `rows_` from here …
        std::size_t end_row{0};   ///< … to here are on this page
    };

    /// Builds the page for one group and returns it. Never null: a group only
    /// exists because a setting declared it.
    QWidget* buildGroup(const std::string& section, const QString& title);

    /// The PRINT PROFILES block, at the top of the `Plot ve Çıktı` page.
    ///
    /// Not a setting and so not generated from the catalogue: a profile is a
    /// NAMED SHEET and `SettingSpec` holds one value of one type (model.md
    /// R38). It is not a second list either — the store is
    /// `PrintService::profiles()` and this draws it — and every edit leaves
    /// through `YAZDIRMAPROFİLİ`, so the window has no road the command line
    /// lacks (Article 1.2, CLAUDE.md 5.10).
    QWidget* buildPrintProfiles();

    /// Refills the profile table from the store. Called when the window opens
    /// and whenever `PrintService::profilesChanged` fires — including for an
    /// edit made at the command line while this window is open.
    void refreshPrintProfiles();

    /// The MODEL PROVIDER block, at the top of the `Yapay Zeka Modelleri` page.
    ///
    /// Not a setting, for the reason the print profiles are not: a provider
    /// profile is a named endpoint and a `SettingSpec` holds one value of one
    /// type (model.md R38) — and its text holds 48 bytes, which is another
    /// reason an API key can never be one (CLAUDE.md 5.21). The store is
    /// `ProviderService::profiles()` and this draws it; every edit leaves
    /// through `YAPAYZEKAMODELİ`, so the window has no road the command line
    /// lacks (Article 1.2, CLAUDE.md 5.10).
    ///
    /// THE ONE EXCEPTION IS THE KEY ITSELF, and it is an exception in the other
    /// direction: it must NOT become a command, because a command's arguments
    /// are journalled. The secret field writes straight to the operating
    /// system's key store (`secret_store.hpp`) and the profile keeps only the
    /// NAME of that entry.
    QWidget* buildProviderProfiles();

    /// Refills the provider table from the store. Called when the window opens
    /// and whenever `ProviderService::profilesChanged` fires — including for a
    /// profile added at the command line while this window is open.
    void refreshProviderProfiles();

    /// Opens `ProviderDialog` on a new profile, or on `edit` when it names one
    /// in the store. The window writes through `YAPAYZEKAMODELİ` like every
    /// other control on this page, so the table refreshes on `profilesChanged`
    /// rather than being told directly.
    void openProviderDialog(const QString& edit);

    /// The LISTENER block, at the top of the `MCP Sunucusu` page.
    ///
    /// WHY THE ADDRESS IS A WIDGET AND NOT A SETTING. What a person has to hand
    /// to an agent is one string — `http://127.0.0.1:8765/mcp/<belirteç>` — and
    /// it is assembled from a setting, a running port and a secret. The secret
    /// is the reason it cannot be a `SettingSpec`: a token in a settings value
    /// would be written to the settings file, read back into a `Value` and
    /// echoed by `AYAR` (CLAUDE.md 5.21). So the token lives in the listener,
    /// this block shows it once for copying, and everything else that talks
    /// about it uses the fingerprint.
    ///
    /// Every button here runs `MCPSUNUCU`, so the window has no road the command
    /// line lacks (Article 1.2).
    QWidget* buildAgentServer();

    /// Redraws the listener block from what the listener is actually doing.
    /// Called when the window opens and on `McpService::stateChanged`.
    void refreshAgentServer();

    /// Redraws the client table from the ledger. Separate from
    /// `refreshAgentServer` because the two change at different times: the
    /// listener's state on a start or a stop, the client list on every request.
    void refreshAgentClients();

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

    /// The print profile table and the fields that add one; null in
    /// `Mode::Project`, which has no such page.
    DataGrid* profiles_table_{nullptr};
    QStandardItemModel* profiles_model_{nullptr};
    Field* profile_name_{nullptr};
    ComboBox* profile_paper_{nullptr};
    Segment* profile_orientation_{nullptr};
    Field* profile_dpi_{nullptr};
    Field* profile_margin_{nullptr};
    Button* profile_default_{nullptr};
    Button* profile_remove_{nullptr};

    /// The model provider table, the fields that add one and the key field;
    /// null in `Mode::Project`, which has no such page.
    DataGrid* providers_table_{nullptr};
    QStandardItemModel* providers_model_{nullptr};

    /// The API key, and the only control in this window whose value never
    /// becomes a command (see `buildProviderProfiles`). `FieldSpec::secret`, so
    /// it shows dots and is never echoed.
    Field* provider_key_{nullptr};
    QLabel* provider_key_note_{nullptr};

    /// The listener block's own widgets; null on the pages that do not have it.
    QLabel* mcp_state_{nullptr};
    Field* mcp_address_{nullptr};
    Button* mcp_toggle_{nullptr};
    Button* mcp_token_{nullptr};
    Button* mcp_copy_{nullptr};
    Button* mcp_probe_{nullptr};
    QLabel* mcp_note_{nullptr};

    // ---- who is using this listener (TODOS M-08) ----------------------------
    //
    // MCP 2026-07-28 IS STATELESS, so "connected clients" cannot be read off a
    // socket table: what the ledger holds, and what this table shows, is who has
    // SPOKEN here and when. Revoking one of them is the sharp instrument beside
    // the blunt one — rotating the token locks out every agent, including the
    // one the person is working with.
    DataGrid* mcp_clients_{nullptr};
    QStandardItemModel* mcp_clients_model_{nullptr};
    Button* mcp_revoke_{nullptr};
    Button* mcp_allow_{nullptr};
    QLabel* mcp_scope_{nullptr};

    /// What stands where the table does when nobody has connected yet, and the
    /// row of actions that goes away with it. A fresh installation has no
    /// clients and will have none until somebody points an agent at the address
    /// above; an empty grid reads as broken.
    QLabel* mcp_empty_{nullptr};
    QWidget* mcp_actions_{nullptr};
    Button* provider_edit_{nullptr};
    Button* provider_default_{nullptr};
    Button* provider_remove_{nullptr};
    Button* provider_test_{nullptr};
    Button* provider_key_save_{nullptr};

    /// Guards the editors while `refresh()` fills them, so a programmatic write
    /// does not read straight back as a user edit.
    bool loading_{false};

    std::vector<Row> rows_;

    /// A topic's caption on the project page and the rows under it: the search
    /// hides a caption whose rows it hid, so a filtered page shows the answers
    /// and not a column of empty headings.
    struct Caption
    {
        QWidget* caption{nullptr};   ///< the heading
        std::vector<QWidget*> lines; ///< the rows it heads
    };

    std::vector<Caption> captions_;
};

} // namespace kentos::app
