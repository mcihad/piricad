// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the window that defines one model endpoint.
//
// WHY A WINDOW AND NOT A ROW. The settings page used to add a profile from five
// fields on one line at the foot of the table, which could express a name, a
// dialect, an address, a model id and a key name — and nothing else. A profile
// has fourteen fields, and the ones the line left out are the ones that decide
// whether the endpoint answers at all: the output cap, the temperature, the
// context window, the thinking knob, the vendor's mandatory extra headers. A row
// that silently drops them is a row that produces a profile the user then has to
// repair by hand in a JSON file.
//
// AND IT MAKES THE MODEL A CHOICE RATHER THAN A SPELLING. A model id is typed
// exactly or it is a 404, and nobody remembers `meta-llama/Llama-3.3-70B-Instruct`.
// The dialog fills the model list from the vendor catalogue
// (`ai/provider_catalog.hpp`) for the chosen endpoint, and `Modelleri getir`
// replaces that list with what the endpoint itself reports — the list a key
// actually has access to, which is the only authoritative one. The combo stays
// editable, because a private deployment serves a name no catalogue knows.
//
// IT STILL LEAVES AS A COMMAND. Everything but the API key goes out as one
// `YAPAYZEKAMODELİ islem=ekle` line, so the window has no road the command line
// lacks (Article 1.2). The key is the deliberate exception and goes straight to
// the key store, because a command's arguments are journalled (CLAUDE.md 5.21).
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/provider_catalog.hpp"

#include <QString>

#include <memory>

namespace kentos::app {

/// The AI layer's door to the document; see ai_service.hpp.
class AiService;
/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Defines one model endpoint: the vendor, the address, the model and the knobs.
class ProviderDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on a NEW profile seeded from the catalogue's first vendor, or on an
    /// existing one when `edit` names a profile in the store. Editing an existing
    /// profile keeps its name locked, because a rename is a remove and an add and
    /// the store would otherwise end up holding both.
    ProviderDialog(Controller& controller, const QString& edit = QString(),
                   QWidget* parent = nullptr);
    ~ProviderDialog() override;

    ProviderDialog(const ProviderDialog&)            = delete;
    ProviderDialog& operator=(const ProviderDialog&) = delete;

    /// The profile as the fields currently read it. Used by `Kaydet` and by the
    /// probe; it is a value, so nothing has been written anywhere yet.
    ai::ProviderProfile profile() const;

    /// Fills every field from `vendor` — what picking a template does.
    void applyVendor(const ai::CatalogVendor& vendor);

    /// Asks the endpoint what models it serves and replaces the model list with
    /// the answer. Does nothing when the vendor has no list endpoint, and says so.
    void fetchModels();

    /// Presses `Kaydet` as a person would. FOR THE PROBE AND THE TESTS.
    bool probeSave();

    /// Every model the chooser is currently offering, as it shows them. FOR THE
    /// PROBE: a list that stayed on one vendor's names is the defect this
    /// window exists to end, and a closed combo cannot be photographed.
    QStringList probeModels() const;

private:
    /// Fills every field from an existing profile.
    void load(const ai::ProviderProfile& profile);

    /// Redraws the line that lists the vendor's mandatory extra headers.
    void refreshHeaderNote();

    /// The model id the combo is on: the selected row's data, or what was typed
    /// with the caption's decoration cut off.
    QString chosenModelId() const;

    /// Writes the key field's value to the key store and clears the field.
    void saveKey();

    /// The listing came back. Separate from the sink so the Qt-facing half is
    /// one readable function.
    void finishListing(int status, const QString& trouble, const std::string& body);

    /// Builds the form. One place, so the new and the edit case cannot differ.
    QWidget* buildBody();

    /// Writes the profile out as one `YAPAYZEKAMODELİ` line and closes.
    void save();

    /// Refills the model combo from the catalogue for the address now in the
    /// address field, keeping whatever the user had typed.
    void refreshModelsFromCatalog();

    /// Turns a model id into the label the combo shows: the id, and the context
    /// window after it when the catalogue knows one.
    QString modelCaption(const ai::CatalogModel& model) const;

    /// Says `text` in the dialog's own note line, in `tone`.
    void note(const QString& text, Tone tone = Tone::Neutral);

    Controller& controller_;
    QString editing_; ///< empty for a new profile

    ComboBox* vendor_{nullptr};
    Field* name_{nullptr};
    ComboBox* dialect_{nullptr};
    Field* url_{nullptr};
    Field* path_{nullptr};
    ComboBox* model_{nullptr};
    Button* fetch_{nullptr};
    Field* keyRef_{nullptr};
    Field* key_{nullptr};
    Button* keySave_{nullptr};
    Field* context_{nullptr};
    Field* maxTokens_{nullptr};
    Field* temperature_{nullptr};
    ToggleSwitch* stream_{nullptr};
    ToggleSwitch* tools_{nullptr};
    ComboBox* reasoning_{nullptr};
    QLabel* note_{nullptr};
    QLabel* headers_{nullptr};

    /// The extra headers the chosen vendor requires. Not editable in this window
    /// — they are protocol, not preference — but shown, because a user debugging
    /// a 400 needs to know what is being sent.
    std::vector<std::pair<std::string, std::string>> extraHeaders_;

    /// The in-flight model listing, so a second press cancels the first.
    struct Listing;
    std::unique_ptr<Listing> listing_;
};

} // namespace kentos::app
