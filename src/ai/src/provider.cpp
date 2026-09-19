// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/provider.hpp"

#include "kentos_cad/ai/redact.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>

namespace kentos::ai {
namespace {

using core::Json;

constexpr double kMinTemperature = 0.0;
constexpr double kMaxTemperature = 2.0;

constexpr std::array<Dialect, 4> kDialects = {
    Dialect::OpenAiChat,
    Dialect::OpenAiResponses,
    Dialect::AnthropicMessages,
    Dialect::OllamaNative,
};

/// The host of a URL, with the scheme, any userinfo, the port and the path
/// removed, and an IPv6 address unbracketed. Empty when there is no host, which
/// `reach_of` reads as "cannot tell".
std::string host_of(std::string_view url)
{
    const std::size_t scheme = url.find("://");
    std::string_view rest    = scheme == std::string_view::npos ? url : url.substr(scheme + 3);

    const std::size_t end = rest.find_first_of("/?#");
    if (end != std::string_view::npos) rest = rest.substr(0, end);

    // Userinfo, which a corporate proxy URL carries and which is not the host.
    const std::size_t at = rest.rfind('@');
    if (at != std::string_view::npos) rest = rest.substr(at + 1);

    if (!rest.empty() && rest.front() == '[') {
        const std::size_t close = rest.find(']');
        if (close != std::string_view::npos) return std::string(rest.substr(1, close - 1));
        return std::string(rest.substr(1));
    }
    const std::size_t colon = rest.find(':');
    if (colon != std::string_view::npos) rest = rest.substr(0, colon);
    return std::string(rest);
}

/// The four octets of a dotted-quad address, or nothing when `host` is not one.
std::optional<std::array<int, 4>> ipv4_of(std::string_view host)
{
    std::array<int, 4> octets{};
    std::size_t at = 0;
    for (int i = 0; i < 4; ++i) {
        std::size_t digits = 0;
        int value          = 0;
        while (at < host.size() && host[at] >= '0' && host[at] <= '9') {
            value = value * 10 + (host[at] - '0');
            if (value > 255) return std::nullopt;
            ++at;
            ++digits;
        }
        if (digits == 0) return std::nullopt;
        octets[static_cast<std::size_t>(i)] = value;
        if (i < 3) {
            if (at >= host.size() || host[at] != '.') return std::nullopt;
            ++at;
        }
    }
    return at == host.size() ? std::optional<std::array<int, 4>>(octets) : std::nullopt;
}

bool ends_with(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/// ASCII lower-casing, and ASCII only because a HOST NAME IS ASCII: a Turkish
/// domain travels as punycode, so there is no dotted-i question here and
/// `core::turkish_fold_key` would fold `ı` into a letter a resolver never sees
/// (CLAUDE.md 5.6 governs Turkish TEXT, which this is not).
std::string ascii_lower(std::string_view text)
{
    std::string out(text);
    for (char& ch : out)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return out;
}

/// The headers object of a profile, as the file stores it.
Json headers_json(const std::vector<std::pair<std::string, std::string>>& headers)
{
    Json out = Json::object({});
    for (const auto& [name, value] : headers)
        out.set(name, Json::string(value));
    return out;
}

Json reasoning_json(const Reasoning& reasoning)
{
    Json out = Json::object({});
    out.set("kip", Json::string(reasoning_mode_id(reasoning.mode)));
    out.set("seviye", Json::string(reasoning.effort));
    out.set("ozet", Json::string(reasoning.summary));
    out.set("butce", Json::integer(reasoning.budget_tokens));
    out.set("metni_goster", Json::boolean(reasoning.show_text));
    return out;
}

Json context_json(const ContextWindow& window)
{
    Json out = Json::object({});
    out.set("jeton", Json::integer(window.tokens));
    out.set("kaynak", Json::string(context_source_id(window.source)));
    return out;
}

} // namespace

const char* dialect_id(Dialect dialect)
{
    switch (dialect) {
    case Dialect::OpenAiChat: return "openai_chat";
    case Dialect::OpenAiResponses: return "openai_responses";
    case Dialect::AnthropicMessages: return "anthropic_messages";
    case Dialect::OllamaNative: return "ollama_native";
    }
    return "openai_chat";
}

DialectCapabilities capabilities_of(Dialect dialect)
{
    DialectCapabilities out;
    switch (dialect) {
    case Dialect::OpenAiChat:
        // The oldest and widest of the four. Reasoning arrives as
        // `delta.reasoning_content` on the providers that have it, and usage only
        // when `stream_options: {"include_usage": true}` was sent — which this
        // program sends, so the number can be trusted when it comes.
        out.vision            = true;
        out.reasoning         = true;
        out.structured_output = true;
        break;

    case Dialect::OpenAiResponses:
        // Typed events with a sequence number, and the one dialect that requires
        // an opaque payload to be replayed byte for byte (`encrypted_content`).
        out.vision            = true;
        out.reasoning         = true;
        out.structured_output = true;
        break;

    case Dialect::AnthropicMessages:
        // `thinking` blocks carry a signature that must be replayed unchanged,
        // which is why a message keeps the provider's opaque payload beside the
        // text it rendered.
        out.vision            = true;
        out.reasoning         = true;
        out.structured_output = false;
        break;

    case Dialect::OllamaNative:
        // NDJSON, one object per line; tool calls arrive in `message.tool_calls`
        // rather than streamed. A LOCAL RUNNER, so what it can do depends far more
        // on the model loaded than on the wire language — `vision` is false here
        // because the dialect has no place for an image, not because no local
        // model can see.
        out.vision            = false;
        out.reasoning         = true;
        out.structured_output = true;
        break;
    }
    return out;
}

std::optional<Dialect> dialect_from_id(std::string_view id)
{
    for (Dialect dialect : kDialects)
        if (id == dialect_id(dialect)) return dialect;
    return std::nullopt;
}

std::span<const Dialect> dialects()
{
    return kDialects;
}

const char* reasoning_mode_id(ReasoningMode mode)
{
    switch (mode) {
    case ReasoningMode::None: return "yok";
    case ReasoningMode::Effort: return "seviye";
    case ReasoningMode::ResponsesSummary: return "ozet";
    case ReasoningMode::AnthropicBudget: return "butce";
    case ReasoningMode::QwenBudget: return "qwen_butce";
    }
    return "yok";
}

std::optional<ReasoningMode> reasoning_mode_from_id(std::string_view id)
{
    constexpr std::array<ReasoningMode, 5> kModes = {
        ReasoningMode::None,
        ReasoningMode::Effort,
        ReasoningMode::ResponsesSummary,
        ReasoningMode::AnthropicBudget,
        ReasoningMode::QwenBudget,
    };
    for (ReasoningMode mode : kModes)
        if (id == reasoning_mode_id(mode)) return mode;
    return std::nullopt;
}

const char* context_source_id(ContextSource source)
{
    switch (source) {
    case ContextSource::Unknown: return "bilinmiyor";
    case ContextSource::Builtin: return "gomulu";
    case ContextSource::User: return "kullanici";
    case ContextSource::Reported: return "sunucu";
    }
    return "bilinmiyor";
}

const char* context_source_label(ContextSource source)
{
    switch (source) {
    case ContextSource::Unknown: return "bilinmiyor";
    case ContextSource::Builtin: return "gömülü";
    case ContextSource::User: return "kullanıcı";
    case ContextSource::Reported: return "sunucu";
    }
    return "bilinmiyor";
}

std::optional<ContextSource> context_source_from_id(std::string_view id)
{
    constexpr std::array<ContextSource, 4> kSources = {
        ContextSource::Unknown,
        ContextSource::Builtin,
        ContextSource::User,
        ContextSource::Reported,
    };
    for (ContextSource source : kSources)
        if (id == context_source_id(source)) return source;
    return std::nullopt;
}

Reach reach_of(std::string_view url)
{
    const std::string host = ascii_lower(host_of(url));
    if (host.empty()) return Reach::Internet;

    if (host == "localhost" || ends_with(host, ".localhost")) return Reach::Loopback;
    if (host == "::1" || host == "0:0:0:0:0:0:0:1") return Reach::Loopback;

    if (const auto octets = ipv4_of(host); octets) {
        const auto& ip = *octets;
        if (ip[0] == 127) return Reach::Loopback;
        // 0.0.0.0 is what a server BINDS to, and a client that was handed it is
        // talking to this machine.
        if (ip[0] == 0) return Reach::Loopback;
        if (ip[0] == 10) return Reach::PrivateNetwork;
        if (ip[0] == 192 && ip[1] == 168) return Reach::PrivateNetwork;
        if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) return Reach::PrivateNetwork;
        // Link-local (RFC 3927) and the carrier-grade NAT block are not the open
        // internet either, and neither is reachable from outside.
        if (ip[0] == 169 && ip[1] == 254) return Reach::PrivateNetwork;
        if (ip[0] == 100 && ip[1] >= 64 && ip[1] <= 127) return Reach::PrivateNetwork;
        return Reach::Internet;
    }

    // RFC 4193 unique-local (fc00::/7) and RFC 4291 link-local (fe80::/10).
    if (host.rfind("fc", 0) == 0 || host.rfind("fd", 0) == 0) {
        if (host.find(':') != std::string::npos) return Reach::PrivateNetwork;
    }
    if (host.rfind("fe8", 0) == 0 && host.find(':') != std::string::npos)
        return Reach::PrivateNetwork;

    // A NAME WITH NO DOT RESOLVES INSIDE THE NETWORK — `vllm`, `sunucu2` — and
    // the mDNS and corporate suffixes below are the same case spelled out. This
    // is the in-institution endpoint of ai.md R13, which is not loopback and is
    // not the internet.
    if (host.find('.') == std::string::npos) return Reach::PrivateNetwork;
    for (const char* suffix : {".local", ".internal", ".intranet", ".lan", ".home.arpa"})
        if (ends_with(host, suffix)) return Reach::PrivateNetwork;

    return Reach::Internet;
}

const char* reach_label(Reach reach)
{
    switch (reach) {
    case Reach::Loopback: return "yerel";
    case Reach::PrivateNetwork: return "kurum ağı";
    case Reach::Internet: return "internet";
    }
    return "internet";
}

bool operator==(const ProviderProfile& a, const ProviderProfile& b)
{
    return a.name == b.name && a.dialect == b.dialect && a.base_url == b.base_url &&
           a.path == b.path && a.model == b.model && a.auth_header == b.auth_header &&
           a.auth_scheme == b.auth_scheme && a.extra_headers == b.extra_headers &&
           a.extra_body.dump() == b.extra_body.dump() && a.stream == b.stream &&
           a.reasoning == b.reasoning && a.max_tokens == b.max_tokens &&
           a.temperature == b.temperature && a.context == b.context && a.tools == b.tools &&
           a.key_ref == b.key_ref;
}

std::string endpoint_url(const ProviderProfile& profile)
{
    std::string base = profile.base_url;
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    if (profile.path.empty()) return base;
    if (profile.path.front() == '/') return base + profile.path;
    return base + "/" + profile.path;
}

std::string describe_provider(const ProviderProfile& profile)
{
    return profile.name + " — " + dialect_id(profile.dialect) + ", " + endpoint_url(profile) +
           ", " + profile.model + ", " + reach_label(reach_of(profile.base_url));
}

core::Result<EndpointPermit> permit_for(const ProviderProfile& profile, bool sensitive)
{
    const std::string url = endpoint_url(profile);
    if (url.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + profile.name + "' profilinin adresi boş.");

    const Reach reach = reach_of(profile.base_url);
    if (sensitive && reach == Reach::Internet)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Proje gizli olarak işaretli: yalnızca yerel ya da kurum ağındaki bir "
                         "model kullanılabilir. '" +
                             profile.name + "' profili " + url +
                             " adresine, yani kurum dışına çıkıyor.");

    EndpointPermit permit;
    permit.url_     = url;
    permit.dialect_ = profile.dialect;
    permit.reach_   = reach;
    return permit;
}

// ---------------------------------------------------------- ProviderProfiles --

const ProviderProfile* ProviderProfiles::find(std::string_view name) const
{
    for (const ProviderProfile& p : profiles_)
        if (core::turkish_key_equals(p.name, name)) return &p;
    return nullptr;
}

const ProviderProfile* ProviderProfiles::fallback() const
{
    if (const ProviderProfile* p = find(default_); p != nullptr) return p;
    return profiles_.empty() ? nullptr : &profiles_.front();
}

core::Status ProviderProfiles::upsert(ProviderProfile p)
{
    if (p.name.empty())
        return core::err(core::ErrorCode::InvalidArgument, "Sağlayıcı adı boş olamaz.");
    if (p.base_url.rfind("http://", 0) != 0 && p.base_url.rfind("https://", 0) != 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name +
                             "' için adres http:// ya da https:// ile başlamalı; "
                             "verilen '" +
                             p.base_url + "'.");
    if (p.path.empty() || p.path.front() != '/')
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için uç nokta yolu '/' ile başlamalı; verilen '" +
                             p.path + "'.");
    if (p.model.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için model adı boş olamaz.");
    if (p.max_tokens < 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için çıktı jeton sınırı negatif olamaz.");
    if (p.reasoning.budget_tokens < 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için düşünme bütçesi negatif olamaz.");
    if (p.context.tokens < 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için bağlam penceresi negatif olamaz.");
    if (p.temperature && (*p.temperature < kMinTemperature || *p.temperature > kMaxTemperature))
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name + "' için sıcaklık 0 ile 2 arasında olmalı.");
    // AI.MD P11, CAUGHT AT THE DOOR. `key_ref` is the NAME of a keychain entry;
    // a user who pasted the key itself into that box would have written the
    // secret into a settings file, and no amount of care further down the line
    // takes it back out of the file the user already saved.
    if (looks_like_secret(p.key_ref))
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + p.name +
                             "' için anahtar adı bir API anahtarına benziyor. Buraya anahtarın "
                             "kendisi değil, anahtar zincirindeki kaydın adı yazılır.");

    for (ProviderProfile& have : profiles_)
        if (core::turkish_key_equals(have.name, p.name)) {
            p.name = have.name; // keep the spelling the user first gave
            have   = std::move(p);
            return core::ok();
        }
    profiles_.push_back(std::move(p));
    if (default_.empty()) default_ = profiles_.back().name;
    return core::ok();
}

core::Status ProviderProfiles::remove(std::string_view name)
{
    const auto at = std::find_if(profiles_.begin(), profiles_.end(), [&](const ProviderProfile& p) {
        return core::turkish_key_equals(p.name, name);
    });
    if (at == profiles_.end())
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir sağlayıcı yok: '" + std::string(name) + "'.");
    if (profiles_.size() == 1)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Son sağlayıcı silinemez; önce başka bir sağlayıcı tanımlayın.");
    const bool was_default = core::turkish_key_equals(at->name, default_);
    profiles_.erase(at);
    if (was_default) default_ = profiles_.front().name;
    return core::ok();
}

core::Status ProviderProfiles::set_default(std::string_view name)
{
    const ProviderProfile* p = find(name);
    if (p == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir sağlayıcı yok: '" + std::string(name) + "'.");
    default_ = p->name;
    return core::ok();
}

std::string ProviderProfiles::listing() const
{
    if (profiles_.empty()) return "Hiç sağlayıcı tanımlı değil.";
    std::string out;
    for (const ProviderProfile& p : profiles_) {
        out += core::turkish_key_equals(p.name, default_) ? "* " : "  ";
        out += describe_provider(p);
        out += '\n';
    }
    out += "(* varsayılan)";
    return out;
}

std::string ProviderProfiles::to_json() const
{
    Json root = Json::object({});
    root.set("surum", Json::integer(kFormatVersion));
    root.set("varsayilan", Json::string(default_));

    Json list = Json::array({});
    for (const ProviderProfile& p : profiles_) {
        Json one = Json::object({});
        one.set("ad", Json::string(p.name));
        one.set("lehce", Json::string(dialect_id(p.dialect)));
        one.set("adres", Json::string(p.base_url));
        one.set("yol", Json::string(p.path));
        one.set("model", Json::string(p.model));
        one.set("yetki_basligi", Json::string(p.auth_header));
        one.set("yetki_onu", Json::string(p.auth_scheme));
        one.set("ek_basliklar", headers_json(p.extra_headers));
        // WRITTEN ONLY WHEN THERE IS ONE. An empty object read back is not the
        // same value as the null a profile with no knobs carries, and a round
        // trip that changed the value would make the settings file disagree with
        // the store it came from.
        if (p.extra_body.is_object() && !p.extra_body.as_object().empty())
            one.set("ek_govde", p.extra_body);
        one.set("akis", Json::boolean(p.stream));
        one.set("dusunme", reasoning_json(p.reasoning));
        one.set("jeton_siniri", Json::integer(p.max_tokens));
        // WRITTEN ONLY WHEN IT IS SET, because absent and 1.0 are different
        // instructions: one sends no temperature at all, and the models that
        // reject the field need exactly that.
        if (p.temperature) one.set("sicaklik", Json::number(*p.temperature));
        one.set("baglam", context_json(p.context));
        one.set("araclar", Json::boolean(p.tools));
        one.set("anahtar_adi", Json::string(p.key_ref));
        list.push(std::move(one));
    }
    root.set("saglayicilar", std::move(list));
    return root.dump_pretty(2);
}

core::Result<ProviderProfiles> ProviderProfiles::from_json(std::string_view text)
{
    auto parsed = core::Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "Sağlayıcı dosyası okunamadı: " + parsed.error().message);
    const Json& root = parsed.value();
    if (!root.is_object())
        return core::err(core::ErrorCode::ParseError, "Sağlayıcı dosyası bir JSON nesnesi değil.");

    const std::int64_t version = root.find("surum") != nullptr ? root.find("surum")->as_int() : 0;
    if (version > kFormatVersion)
        return core::err(core::ErrorCode::Unsupported,
                         "Sağlayıcı dosyası bu sürümden yeni (dosya " + std::to_string(version) +
                             ", bu sürüm " + std::to_string(kFormatVersion) +
                             "). Programı güncelleyin; dosya olduğu gibi bırakıldı.");

    ProviderProfiles out;
    if (const Json* list = root.find("saglayicilar"); list != nullptr && list->is_array())
        for (const Json& one : list->as_array()) {
            ProviderProfile p;
            if (const Json* v = one.find("ad")) p.name = v->as_string();
            if (const Json* v = one.find("lehce")) {
                const std::optional<Dialect> dialect = dialect_from_id(v->as_string());
                if (!dialect)
                    return core::err(core::ErrorCode::Unsupported,
                                     "'" + p.name + "' profilinin lehçesi bu sürümde yok: '" +
                                         v->as_string() + "'.");
                p.dialect = *dialect;
            }
            if (const Json* v = one.find("adres")) p.base_url = v->as_string();
            if (const Json* v = one.find("yol")) p.path = v->as_string();
            if (const Json* v = one.find("model")) p.model = v->as_string();
            if (const Json* v = one.find("yetki_basligi")) p.auth_header = v->as_string();
            if (const Json* v = one.find("yetki_onu")) p.auth_scheme = v->as_string();
            if (const Json* v = one.find("ek_basliklar"); v != nullptr && v->is_object())
                for (const auto& [name, value] : v->as_object())
                    p.extra_headers.emplace_back(name, value.as_string());
            if (const Json* v = one.find("ek_govde");
                v != nullptr && v->is_object() && !v->as_object().empty())
                p.extra_body = *v;
            if (const Json* v = one.find("akis")) p.stream = v->as_bool(true);
            if (const Json* v = one.find("dusunme"); v != nullptr && v->is_object()) {
                if (const Json* w = v->find("kip")) {
                    const std::optional<ReasoningMode> mode =
                        reasoning_mode_from_id(w->as_string());
                    if (!mode)
                        return core::err(core::ErrorCode::Unsupported,
                                         "'" + p.name +
                                             "' profilinin düşünme kipi bu sürümde yok: '" +
                                             w->as_string() + "'.");
                    p.reasoning.mode = *mode;
                }
                if (const Json* w = v->find("seviye")) p.reasoning.effort = w->as_string();
                if (const Json* w = v->find("ozet")) p.reasoning.summary = w->as_string();
                if (const Json* w = v->find("butce")) p.reasoning.budget_tokens = w->as_int();
                if (const Json* w = v->find("metni_goster"))
                    p.reasoning.show_text = w->as_bool(false);
            }
            if (const Json* v = one.find("jeton_siniri")) p.max_tokens = v->as_int();
            if (const Json* v = one.find("sicaklik"); v != nullptr && v->is_number())
                p.temperature = v->as_double();
            if (const Json* v = one.find("baglam"); v != nullptr && v->is_object()) {
                if (const Json* w = v->find("jeton")) p.context.tokens = w->as_int();
                if (const Json* w = v->find("kaynak"))
                    p.context.source =
                        context_source_from_id(w->as_string()).value_or(ContextSource::Unknown);
            }
            if (const Json* v = one.find("araclar")) p.tools = v->as_bool(true);
            if (const Json* v = one.find("anahtar_adi")) p.key_ref = v->as_string();

            if (auto status = out.upsert(std::move(p)); !status)
                return core::err(core::ErrorCode::ParseError,
                                 "Sağlayıcı dosyasında geçersiz kayıt: " + status.error().message);
        }

    if (const Json* d = root.find("varsayilan"); d != nullptr && d->is_string())
        if (const ProviderProfile* p = out.find(d->as_string()); p != nullptr)
            out.default_ = p->name;
    if (out.default_.empty() && !out.profiles_.empty()) out.default_ = out.profiles_.front().name;
    return out;
}

} // namespace kentos::ai
