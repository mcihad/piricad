// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: what the vendors offer, as DATA rather than as C++ literals.
//
// WHY THIS FILE EXISTS. The endpoints and model ids of two dozen model vendors
// are the fastest-rotting facts in this program: a model id is renamed in a
// month, a base URL grows a version prefix, a vendor retires a family. They were
// written as C++ literals in `ProviderProfiles::builtin()`, which meant every
// such change was a rebuild and a release — and that a user whose vendor had
// shipped a new model could not reach it from the settings page at all, because
// the page offered exactly the one id the binary was compiled with.
//
// So the vendor list is a CATALOGUE under `/data/catalogs/ai`, versioned and
// schema-checked like every other catalogue in this program (CLAUDE.md 3.5,
// Article 9 "To change a regulatory rule" — the same mechanism, for facts that
// are not regulatory but rot just as fast). Updating it is a data release.
//
// IT IS ALSO THE ONE LIST. `ProviderProfiles::from_catalog` derives the starting
// profiles from it, the dialog's template chooser is drawn from it, and the model
// combo for a given endpoint is filled from it — so the shipped defaults, the
// templates and the model names are three views of one file rather than three
// lists that drift (CLAUDE.md 5.10).
//
// AND IT IS THE FALLBACK, NOT THE TRUTH. What a vendor actually serves today is
// what its own list-models endpoint says, and `models_path` here is how to ask.
// The catalogue is what the program knows before anyone has asked and what it
// falls back to when the endpoint cannot be reached — which is the normal state
// of a provider whose key has not been entered yet.
#pragma once

#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/transport.hpp"

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::ai {

/// One model a vendor serves.
struct CatalogModel
{
    std::string id;    ///< the EXACT id sent on the wire; the only field that must be right
    std::string label; ///< a short human name, or empty when the id is already readable

    std::int64_t context{0};    ///< context window in tokens, 0 when the vendor does not say
    std::int64_t max_output{0}; ///< output cap in tokens, 0 when unstated

    /// Whether it is a thinking model. It decides what the profile's `Reasoning`
    /// is seeded with, and whether the panel expects a reasoning stream at all.
    bool reasoning{false};

    friend bool operator==(const CatalogModel&, const CatalogModel&) = default;
};

/// How a vendor's list-models response is shaped.
///
/// THREE SHAPES COVER EVERY ENDPOINT THIS PROGRAM TALKS TO, which is the same
/// finding that gave `Dialect` its four members: the differences between vendors
/// are in the envelope, not in the idea.
enum class ModelListShape : std::uint8_t {
    None,      ///< the vendor has no list endpoint; the catalogue is all there is
    OpenAi,    ///< `{"data":[{"id":…}]}` — almost everyone
    Ollama,    ///< `{"models":[{"name":…}]}` — Ollama's own `/api/tags`
    Anthropic, ///< `{"data":[{"id":…,"display_name":…}]}`
};

/// The shape's wire word, as the catalogue file spells it.
const char* model_list_shape_id(ModelListShape shape);

/// The shape of that name, or nothing when this build does not know it.
std::optional<ModelListShape> model_list_shape_from_id(std::string_view id);

/// One vendor: everything needed to build a profile for it, and the models it is
/// known to serve.
struct CatalogVendor
{
    std::string id;   ///< lowercase ascii slug: `deepseek`, `groq`, `ollama`
    std::string name; ///< what the vendor calls itself, shown in the chooser

    Dialect dialect{Dialect::OpenAiChat}; ///< which wire language this endpoint speaks
    std::string base_url;                 ///< no trailing slash
    std::string path;                     ///< the chat endpoint under it

    /// Which header carries the credential, and what precedes it. An EMPTY
    /// header means the endpoint wants none, which is the ordinary case for a
    /// local server; an empty SCHEME means the bare key, which is Anthropic's.
    std::string auth_header{"Authorization"};
    std::string auth_scheme{"Bearer"};

    /// Headers the vendor requires beyond the credential — Anthropic's
    /// `anthropic-version`, an aggregator's attribution pair.
    std::vector<std::pair<std::string, std::string>> extra_headers;

    /// The key-store entry a profile for this vendor points at, and the
    /// environment variable the vendor's own documentation tells people to set —
    /// which `SecretStore::read` tries after the key store (secret_store.hpp).
    std::string key_ref;
    std::string key_env;

    /// Where the live model list is, under `base_url`, and how it is shaped.
    std::string models_path;
    ModelListShape list_shape{ModelListShape::None};

    std::string docs_url; ///< the vendor's own API reference, for the dialog's link text

    /// Whether it runs on the operator's own machine or inside the institution.
    /// A local vendor needs no credential and is permitted in a sensitive project
    /// (ai.md R14) — `permit_for` judges that from the URL, and this is what the
    /// chooser shows beside the name.
    bool local{false};

    /// WHETHER A FRESH INSTALLATION STARTS WITH A PROFILE FOR IT. The catalogue
    /// carries every vendor this program knows how to reach; a chat chooser
    /// listing forty of them would be a chooser nobody can use. The seeded few
    /// are what a new user sees, and the rest are one dialog away.
    bool seed{false};

    std::vector<CatalogModel> models;

    /// The model a profile for this vendor starts on: the first listed, because
    /// the catalogue lists a vendor's own recommended model first.
    const CatalogModel* preferred_model() const;

    friend bool operator==(const CatalogVendor&, const CatalogVendor&) = default;
};

/// The shipped vendor list.
class ProviderCatalog
{
public:
    /// The schema this build reads. A file declaring a NEWER one is refused with
    /// the version in the message rather than half-read (`.claude/io.md` R9's
    /// rule, which binds every format this program reads).
    static constexpr std::int64_t kSchemaVersion = 1;

    /// Parses the catalogue's JSON text. The application reads the file; this
    /// module does no I/O (`.claude/ai.md` P10).
    static core::Result<ProviderCatalog> from_json(std::string_view text);

    /// Every vendor, in the order the file lists them.
    std::span<const CatalogVendor> vendors() const noexcept { return vendors_; }

    bool empty() const noexcept { return vendors_.empty(); }

    /// The vendor of that id, or null. Matched exactly: an id is ascii and is
    /// what the file and the profile both carry.
    const CatalogVendor* find(std::string_view id) const;

    /// The vendor whose `base_url` a profile's URL starts with, or null.
    ///
    /// THIS IS HOW A PROFILE FINDS ITS MODEL LIST. A profile stores an address,
    /// not a vendor id — it always has, and a user may point one at a gateway
    /// nobody catalogued — so the page matches by address and simply offers no
    /// list when nothing matches, rather than guessing.
    const CatalogVendor* for_profile(const ProviderProfile& profile) const;

    /// The package's own version, for the settings page's note and `make doctor`.
    const std::string& package_version() const noexcept { return package_version_; }

    /// Where the facts came from and when, as the file states it.
    const std::string& source() const noexcept { return source_; }

private:
    std::vector<CatalogVendor> vendors_;
    std::string package_version_;
    std::string source_;
};

/// One profile built from a catalogue vendor, on its preferred model.
ProviderProfile profile_for(const CatalogVendor& vendor);

/// The starting profiles of a fresh installation: one per vendor marked `seed`.
///
/// REPLACES THE FORMER `ProviderProfiles::builtin()`, whose fifteen profiles were
/// C++ literals. Same idea, one source, and a model id that changes is now a data
/// release rather than a rebuild.
ProviderProfiles seed_profiles(const ProviderCatalog& catalog);

/// The request that asks an endpoint what models it serves.
///
/// A `GET`, and the only one this program makes. Returns nothing when the vendor
/// has no list endpoint, so a caller cannot send a request to a path that does
/// not exist.
std::optional<HttpRequest> model_list_request(const ProviderProfile& profile,
                                              const CatalogVendor& vendor,
                                              const EndpointPermit& permit);

/// The model ids in a list-models response body, in the order the endpoint gave
/// them. An unparseable body is an empty list rather than an error: a chooser
/// that cannot be filled falls back to the catalogue, and the caller says so.
std::vector<std::string> parse_model_list(ModelListShape shape, std::string_view body);

} // namespace kentos::ai
