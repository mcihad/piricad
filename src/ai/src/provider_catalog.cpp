// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/provider_catalog.hpp"

#include "kentos_cad/core/json.hpp"

#include <algorithm>
#include <array>

namespace kentos::ai {
namespace {

using core::Json;

/// A string field, or empty when it is absent or not a string.
std::string text_of(const Json& parent, const char* key)
{
    const Json* v = parent.find(key);
    return v != nullptr && v->is_string() ? v->as_string() : std::string();
}

/// An integer field, or `fallback`.
std::int64_t int_of(const Json& parent, const char* key, std::int64_t fallback = 0)
{
    const Json* v = parent.find(key);
    return v != nullptr && v->is_number() ? v->as_int() : fallback;
}

/// A boolean field, or `fallback`.
bool bool_of(const Json& parent, const char* key, bool fallback = false)
{
    const Json* v = parent.find(key);
    return v != nullptr && v->is_bool() ? v->as_bool() : fallback;
}

} // namespace

const char* model_list_shape_id(ModelListShape shape)
{
    switch (shape) {
    case ModelListShape::None: return "yok";
    case ModelListShape::OpenAi: return "openai";
    case ModelListShape::Ollama: return "ollama";
    case ModelListShape::Anthropic: return "anthropic";
    }
    return "yok";
}

std::optional<ModelListShape> model_list_shape_from_id(std::string_view id)
{
    constexpr std::array<ModelListShape, 4> kShapes = {
        ModelListShape::None,
        ModelListShape::OpenAi,
        ModelListShape::Ollama,
        ModelListShape::Anthropic,
    };
    for (ModelListShape shape : kShapes)
        if (id == model_list_shape_id(shape)) return shape;
    return std::nullopt;
}

const CatalogModel* CatalogVendor::preferred_model() const
{
    return models.empty() ? nullptr : &models.front();
}

core::Result<ProviderCatalog> ProviderCatalog::from_json(std::string_view text)
{
    auto parsed = Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "Sağlayıcı kataloğu okunamadı: " + parsed.error().message);

    const Json& root = parsed.value();
    if (!root.is_object())
        return core::err(core::ErrorCode::ParseError, "Sağlayıcı kataloğu bir JSON nesnesi değil.");

    // THE SCHEMA VERSION IS CHECKED BEFORE ANYTHING IS READ. A newer file is
    // refused whole rather than read in part: a field this build does not know
    // is a field it would silently drop, and a dropped `adres` is a profile
    // pointing at the wrong host.
    const std::int64_t schema = int_of(root, "schema_version");
    if (schema > kSchemaVersion)
        return core::err(core::ErrorCode::Unsupported,
                         "Sağlayıcı kataloğu bu sürümden yeni (dosya şeması " +
                             std::to_string(schema) + ", bu sürüm " +
                             std::to_string(kSchemaVersion) +
                             "). Programı güncelleyin; katalog olduğu gibi bırakıldı.");

    ProviderCatalog out;
    out.package_version_ = text_of(root, "package_version");
    out.source_          = text_of(root, "source");

    const Json* list = root.find("saglayicilar");
    if (list == nullptr || !list->is_array())
        return core::err(core::ErrorCode::ParseError,
                         "Sağlayıcı kataloğunda 'saglayicilar' dizisi yok.");

    for (const Json& one : list->as_array()) {
        if (!one.is_object()) continue;

        CatalogVendor vendor;
        vendor.id       = text_of(one, "kimlik");
        vendor.name     = text_of(one, "ad");
        vendor.base_url = text_of(one, "adres");
        vendor.path     = text_of(one, "yol");
        vendor.docs_url = text_of(one, "belge");
        vendor.key_ref  = text_of(one, "anahtar_ref");
        vendor.key_env  = text_of(one, "anahtar_ortam");
        vendor.local    = bool_of(one, "yerel");
        vendor.seed     = bool_of(one, "baslangic");

        if (vendor.id.empty() || vendor.name.empty())
            return core::err(core::ErrorCode::ParseError,
                             "Sağlayıcı kataloğunda kimliği ya da adı olmayan bir kayıt var.");

        if (const Json* v = one.find("lehce"); v != nullptr && v->is_string()) {
            const std::optional<Dialect> dialect = dialect_from_id(v->as_string());
            if (!dialect)
                return core::err(core::ErrorCode::Unsupported,
                                 "'" + vendor.name + "' sağlayıcısının lehçesi bu sürümde yok: '" +
                                     v->as_string() + "'.");
            vendor.dialect = *dialect;
        }

        // THE CREDENTIAL HEADER DEFAULTS ONLY WHEN THE FILE IS SILENT, and an
        // EXPLICIT EMPTY STRING IS MEANINGFUL: Anthropic takes the bare key with
        // no scheme, and a local server takes no credential at all. A `find`
        // rather than `text_of` is what tells those two cases apart.
        if (const Json* v = one.find("kimlik_baslik"); v != nullptr && v->is_string())
            vendor.auth_header = v->as_string();
        if (const Json* v = one.find("kimlik_onek"); v != nullptr && v->is_string())
            vendor.auth_scheme = v->as_string();

        if (const Json* v = one.find("ek_basliklar"); v != nullptr && v->is_array())
            for (const Json& header : v->as_array())
                if (header.is_object())
                    vendor.extra_headers.emplace_back(text_of(header, "ad"),
                                                      text_of(header, "deger"));

        vendor.models_path = text_of(one, "model_listesi_yolu");
        if (const Json* v = one.find("model_listesi_bicimi"); v != nullptr && v->is_string()) {
            const std::optional<ModelListShape> shape = model_list_shape_from_id(v->as_string());
            if (!shape)
                return core::err(core::ErrorCode::Unsupported,
                                 "'" + vendor.name + "' için model listesi biçimi bilinmiyor: '" +
                                     v->as_string() + "'.");
            vendor.list_shape = *shape;
        }

        if (const Json* v = one.find("modeller"); v != nullptr && v->is_array())
            for (const Json& model : v->as_array()) {
                if (!model.is_object()) continue;
                CatalogModel m;
                m.id         = text_of(model, "kimlik");
                m.label      = text_of(model, "etiket");
                m.context    = int_of(model, "baglam");
                m.max_output = int_of(model, "azami_cikti"); // ui-label
                m.reasoning  = bool_of(model, "dusunur");
                if (m.id.empty())
                    return core::err(core::ErrorCode::ParseError,
                                     "'" + vendor.name +
                                         "' sağlayıcısında kimliksiz bir model "
                                         "kaydı var.");
                vendor.models.push_back(std::move(m));
            }

        out.vendors_.push_back(std::move(vendor));
    }

    if (out.vendors_.empty())
        return core::err(core::ErrorCode::ParseError, "Sağlayıcı kataloğu boş.");
    return out;
}

const CatalogVendor* ProviderCatalog::find(std::string_view id) const
{
    for (const CatalogVendor& vendor : vendors_)
        if (vendor.id == id) return &vendor;
    return nullptr;
}

const CatalogVendor* ProviderCatalog::for_profile(const ProviderProfile& profile) const
{
    // THE LONGEST MATCHING ADDRESS WINS, because two vendors share a host: a
    // profile on `https://api.openai.com/v1` matches both OpenAI entries, and
    // `openrouter.ai/api/v1` must not be matched by a bare `openrouter.ai`.
    const CatalogVendor* best = nullptr;
    for (const CatalogVendor& vendor : vendors_) {
        if (vendor.base_url.empty()) continue;
        if (profile.base_url.rfind(vendor.base_url, 0) != 0) continue;
        // Of two matching prefixes, prefer the one whose dialect also agrees —
        // that is what tells OpenAI's Responses entry from its Chat entry.
        const bool better = best == nullptr || vendor.base_url.size() > best->base_url.size() ||
                            (vendor.base_url.size() == best->base_url.size() &&
                             vendor.dialect == profile.dialect && best->dialect != profile.dialect);
        if (better) best = &vendor;
    }
    return best;
}

ProviderProfile profile_for(const CatalogVendor& vendor)
{
    ProviderProfile p;
    p.name        = vendor.name;
    p.dialect     = vendor.dialect;
    p.base_url    = vendor.base_url;
    p.path        = vendor.path;
    p.auth_header = vendor.auth_header;
    p.auth_scheme = vendor.auth_scheme;
    p.key_ref     = vendor.key_ref;

    p.extra_headers = vendor.extra_headers;

    if (const CatalogModel* model = vendor.preferred_model(); model != nullptr) {
        p.model      = model->id;
        p.max_tokens = model->max_output;
        p.context    = {model->context, ContextSource::Builtin};

        // A THINKING MODEL GETS ITS DIALECT'S OWN KNOB, because the four dialects
        // ask for reasoning in four different ways and a profile that asked the
        // wrong way would simply not think (`ai/provider.hpp`, `ReasoningMode`).
        if (model->reasoning) {
            switch (vendor.dialect) {
            case Dialect::AnthropicMessages:
                p.reasoning = Reasoning{ReasoningMode::AnthropicBudget, "medium", "auto", 4096,
                                        /*show_text=*/true};
                break;
            case Dialect::OpenAiResponses:
                p.reasoning = Reasoning{ReasoningMode::ResponsesSummary, "medium", "auto", 0,
                                        /*show_text=*/true};
                break;
            case Dialect::OpenAiChat:
            case Dialect::OllamaNative:
                p.reasoning =
                    Reasoning{ReasoningMode::Effort, "medium", "auto", 0, /*show_text=*/true};
                break;
            }
        }
    }
    return p;
}

ProviderProfiles seed_profiles(const ProviderCatalog& catalog)
{
    ProviderProfiles out;
    std::string first_local;

    for (const CatalogVendor& vendor : catalog.vendors()) {
        if (!vendor.seed) continue;
        ProviderProfile p = profile_for(vendor);
        if (vendor.local && first_local.empty()) first_local = p.name;
        // A profile the store refuses is simply ABSENT rather than repaired, for
        // the reason the former `builtin()` gave: a silently mended record would
        // then be written into the user's own file.
        (void)out.upsert(std::move(p));
    }

    // THE DEFAULT IS A LOCAL ENDPOINT WHEREVER THERE IS ONE (ai.md R13: local
    // support is mandatory, and a default that needs a credit card is not
    // support). Only when the catalogue ships none does the first entry stand in.
    if (!first_local.empty()) (void)out.set_default(first_local);
    return out;
}

std::optional<HttpRequest> model_list_request(const ProviderProfile& profile,
                                              const CatalogVendor& vendor,
                                              const EndpointPermit& permit)
{
    if (vendor.models_path.empty() || vendor.list_shape == ModelListShape::None)
        return std::nullopt;

    HttpRequest request;
    request.method = "GET";

    // BUILT FROM THE PROFILE'S OWN ADDRESS, not the vendor's: a user who pointed
    // a profile at a mirror, a proxy or a second port asked for THAT host, and
    // listing models from the catalogue's host instead would show them a list
    // their endpoint does not serve. The permit is still what authorised the
    // host — a caller cannot reach here without one.
    std::string base = profile.base_url;
    if (!base.empty() && base.back() == '/') base.pop_back();
    request.url = base + vendor.models_path;
    (void)permit;

    for (const auto& [name, value] : profile.extra_headers)
        request.headers.emplace_back(name, value);
    request.headers.emplace_back("Accept", "application/json");
    return request;
}

std::vector<std::string> parse_model_list(ModelListShape shape, std::string_view body)
{
    std::vector<std::string> out;
    if (shape == ModelListShape::None) return out;

    auto parsed = Json::parse(body);
    if (!parsed || !parsed.value().is_object()) return out;
    const Json& root = parsed.value();

    const char* const key = shape == ModelListShape::Ollama ? "models" : "data";
    const Json* list      = root.find(key);
    if (list == nullptr || !list->is_array()) return out;

    for (const Json& one : list->as_array()) {
        if (!one.is_object()) continue;
        // Ollama names the field `name` and everyone else names it `id`; both are
        // read, so a gateway that answers in the other shape still works.
        std::string id = text_of(one, shape == ModelListShape::Ollama ? "name" : "id");
        if (id.empty()) id = text_of(one, shape == ModelListShape::Ollama ? "model" : "name");
        if (id.empty()) continue;
        if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(std::move(id));
    }
    return out;
}

} // namespace kentos::ai
