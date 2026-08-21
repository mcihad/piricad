// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: settings.
#include "piricad/core/settings.hpp"

#include "piricad/core/log.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <cstring>

namespace piricad::core {
namespace {

using std::int64_t;
using std::uint64_t;

/// A default that does not fit the fixed text buffer becomes a type mismatch, which
/// SettingCatalog::add rejects. A declared default is never silently truncated.
SettingValue text_value(std::string_view s)
{
    auto v = SettingValue::text(s);
    return v ? v.value() : SettingValue{};
}

/// Scalar conversion, not a grammar: digits, an optional sign, an optional 0x prefix
/// and an optional all-zero fraction. `<cctype>` classifiers are banned project-wide
/// on text that may be Turkish (CLAUDE.md 5.6), so the digit test is explicit.
bool digit_of(char c, int base, int& out)
{
    int v = -1;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (base == 16 && c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (base == 16 && c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    if (v < 0 || v >= base) return false;
    out = v;
    return true;
}

bool parse_scalar(std::string_view s, int64_t& out)
{
    if (s.empty()) return false;

    bool negative = false;
    std::size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        negative = s[0] == '-';
        i        = 1;
    }

    int base = 10;
    if (s.size() >= i + 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        base = 16;
        i += 2;
    }
    if (i >= s.size()) return false;

    uint64_t acc           = 0;
    const uint64_t ceiling = negative ? uint64_t{1} << 63 : (uint64_t{1} << 63) - 1;
    std::size_t digits     = 0;

    for (; i < s.size(); ++i) {
        int d = 0;
        if (!digit_of(s[i], base, d)) break;
        const uint64_t b = static_cast<uint64_t>(base);
        if (acc > (ceiling - static_cast<uint64_t>(d)) / b) return false; // overflow
        acc = acc * b + static_cast<uint64_t>(d);
        ++digits;
    }
    if (digits == 0) return false;

    // The command line renders a number token as "3.000000", so an all-zero
    // fraction is the same integer. A non-zero fraction is a real error: a
    // fixed-point setting has no half unit (model.md R21).
    if (i < s.size()) {
        if (s[i] != '.') return false;
        for (++i; i < s.size(); ++i)
            if (s[i] != '0') return false;
    }

    out = negative ? -static_cast<int64_t>(acc) : static_cast<int64_t>(acc);
    return true;
}

std::string quote(std::string_view s)
{
    return "'" + std::string(s) + "'";
}

std::string join(const std::vector<std::string>& items)
{
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ", ";
        out += items[i];
    }
    return out;
}

std::string scope_list(SettingScopeMask m)
{
    std::vector<std::string> names;
    for (auto s : {SettingScope::App, SettingScope::Project, SettingScope::Session})
        if (accepts(m, s)) names.emplace_back(setting_scope_label(s));
    return names.empty() ? std::string("hiçbir") : join(names);
}

} // namespace

// ---------------------------------------------------------------- scope -----

const char* setting_scope_name(SettingScope s)
{
    switch (s) {
    case SettingScope::App: return "app";
    case SettingScope::Project: return "project";
    case SettingScope::Session: return "session";
    }
    return "app";
}

const char* setting_scope_label(SettingScope s)
{
    switch (s) {
    case SettingScope::App: return "uygulama";
    case SettingScope::Project: return "proje";
    case SettingScope::Session: return "oturum";
    }
    return "uygulama";
}

const char* setting_type_name(SettingType t)
{
    switch (t) {
    case SettingType::Bool: return "bool";
    case SettingType::Int: return "int";
    case SettingType::Length: return "length";
    case SettingType::Text: return "text";
    case SettingType::Enum: return "enum";
    }
    return "bool";
}

const char* setting_type_label(SettingType t)
{
    switch (t) {
    case SettingType::Bool: return "evet/hayır";
    case SettingType::Int: return "tam sayı";
    case SettingType::Length: return "uzunluk (mm)";
    case SettingType::Text: return "metin";
    case SettingType::Enum: return "seçenek";
    }
    return "evet/hayır";
}

// ---------------------------------------------------------------- value -----

SettingValue SettingValue::boolean(bool v)
{
    SettingValue s;
    s.type_ = SettingType::Bool;
    s.num_  = v ? 1 : 0;
    return s;
}

SettingValue SettingValue::integer(std::int64_t v)
{
    SettingValue s;
    s.type_ = SettingType::Int;
    s.num_  = v;
    return s;
}

SettingValue SettingValue::length(Mm v)
{
    SettingValue s;
    s.type_ = SettingType::Length;
    s.num_  = v;
    return s;
}

SettingValue SettingValue::enumerated(std::uint16_t index)
{
    SettingValue s;
    s.type_ = SettingType::Enum;
    s.num_  = static_cast<std::int64_t>(index);
    return s;
}

Result<SettingValue> SettingValue::text(std::string_view v)
{
    if (v.size() >= kSettingTextCapacity)
        return err(ErrorCode::InvalidArgument,
                   "Metin ayarı en çok " + std::to_string(kSettingTextCapacity - 1) +
                       " bayt alır. Girilen: " + std::to_string(v.size()) + " bayt.");

    SettingValue s;
    s.type_ = SettingType::Text;
    if (!v.empty()) std::memcpy(s.text_.data(), v.data(), v.size());
    return s;
}

std::string_view SettingValue::as_text() const noexcept
{
    if (type_ != SettingType::Text) return {};
    const auto end = std::find(text_.begin(), text_.end(), '\0');
    return std::string_view(text_.data(), static_cast<std::size_t>(end - text_.begin()));
}

void SettingValue::set_scalar(std::int64_t v) noexcept
{
    if (type_ == SettingType::Text) return;
    num_ = type_ == SettingType::Bool ? (v != 0 ? 1 : 0) : v;
}

std::uint64_t SettingValue::fold(std::uint64_t seed) const
{
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(type_), seed);
    h               = fnv1a_int(num_, h);
    return fnv1a(as_text(), h);
}

// -------------------------------------------------------------- catalog -----

Status SettingCatalog::add(SettingSpec s)
{
    if (s.id.empty()) return err(ErrorCode::InvalidArgument, "Ayar kimliği boş olamaz.");
    if (s.names.empty())
        return err(ErrorCode::InvalidArgument, quote(s.id) + " ayarı hiç ad tanımlamıyor.");
    if (s.summary.empty())
        return err(ErrorCode::InvalidArgument, quote(s.id) + " ayarının özeti yok.");
    if (find(s.id) != kNoSetting)
        return err(ErrorCode::InvalidArgument, "Yinelenen ayar kimliği: " + quote(s.id));
    if (s.fallback.type() != s.type)
        return err(ErrorCode::InvalidArgument,
                   quote(s.id) + " ayarının varsayılanı " + setting_type_label(s.fallback.type()) +
                       ", oysa tür " + setting_type_label(s.type) + " olarak bildirilmiş.");
    if (s.type == SettingType::Enum) {
        if (s.values.empty())
            return err(ErrorCode::InvalidArgument,
                       quote(s.id) + " seçenek ayarı hiç seçenek tanımlamıyor.");
        if (s.fallback.as_enum() >= s.values.size())
            return err(ErrorCode::InvalidArgument,
                       quote(s.id) + " ayarının varsayılan seçenek indisi listenin dışında.");
    }
    if (s.type != SettingType::Text && !s.range.contains(s.fallback.scalar()))
        return err(ErrorCode::InvalidArgument,
                   quote(s.id) + " ayarının varsayılanı bildirilen aralığın dışında: " +
                       std::to_string(s.fallback.scalar()) + " ∉ [" + std::to_string(s.range.min) +
                       ", " + std::to_string(s.range.max) + "]");

    // The id always resolves, so a script, a file and the AI can use it directly
    // without knowing the Turkish alias — the same rule the command registry follows.
    std::vector<std::string> aliases;
    aliases.push_back(s.id);
    for (const auto& n : s.names)
        aliases.push_back(n);

    std::vector<std::string> kept;
    std::vector<std::string> folded;
    for (const auto& a : aliases) {
        std::string f = turkish_upper(a);
        // A Turkish name that is already ASCII has no separate folded twin; listing
        // it twice is not an error, it is just the same alias.
        if (std::find(folded.begin(), folded.end(), f) != folded.end()) continue;
        if (const std::uint32_t owner = find(a); owner != kNoSetting)
            return err(ErrorCode::InvalidArgument,
                       quote(a) + " ayar adı zaten " + quote(specs_[owner].id) + " ayarına ait.");
        kept.push_back(a);
        folded.push_back(std::move(f));
    }

    const auto index = static_cast<std::uint32_t>(specs_.size());
    for (std::size_t i = 0; i < kept.size(); ++i) {
        alias_.push_back(std::move(kept[i]));
        folded_.push_back(std::move(folded[i]));
        owner_.push_back(index);
    }
    specs_.push_back(std::move(s));
    return ok();
}

std::uint32_t SettingCatalog::find(std::string_view id_or_name) const
{
    if (id_or_name.empty()) return kNoSetting;
    const std::string f = turkish_upper(id_or_name);
    for (std::size_t i = 0; i < folded_.size(); ++i)
        if (folded_[i] == f) return owner_[i];
    return kNoSetting;
}

std::string SettingCatalog::suggest(std::string_view typed) const
{
    if (typed.empty()) return {};
    const std::string f = turkish_upper(typed);

    for (std::size_t i = 0; i < folded_.size(); ++i)
        if (folded_[i].starts_with(f)) return alias_[i];
    for (std::size_t i = 0; i < folded_.size(); ++i)
        if (folded_[i].find(f) != std::string::npos) return alias_[i];
    return {};
}

// ------------------------------------------------------ the declarations ----
//
// One factory per setting, one X-macro list, generated documentation — the same
// idiom as commands/builtin.cpp and for the same reason (model.md R25).
//
// Every summary states WHY the scope is what it is, because R40 is a test that has
// to be applied to each new setting and the answer must survive in the source.

namespace {

// Fixed-point conventions, so that no setting ever needs a double (R21, P8).
constexpr std::int64_t kPerMille   = 1000;    // 1.000 as a per-mille ratio
constexpr std::int64_t kMicroDeg   = 1000000; // one degree in micro-degrees
constexpr std::int64_t kFullCircle = 360 * kMicroDeg;

} // namespace

PIRICAD_SETTING(koordinat_sistemi);
PIRICAD_SETTING(koordinat_hassasiyeti);
PIRICAD_SETTING(cizim_birimi);
PIRICAD_SETTING(cizgi_tipi_olcegi);
PIRICAD_SETTING(metin_yuksekligi);
PIRICAD_SETTING(veri_paketi_surumu);
PIRICAD_SETTING(tema);
PIRICAD_SETTING(dil);
PIRICAD_SETTING(otomatik_kayit);
PIRICAD_SETTING(son_dosya_sayisi);
PIRICAD_SETTING(tuval_arkaplani);
PIRICAD_SETTING(yakalama_modlari);
PIRICAD_SETTING(dik_mod);
PIRICAD_SETTING(kutupsal_aci);

#define PIRICAD_BUILTIN_SETTINGS(X)                                                                \
    X(koordinat_sistemi)                                                                           \
    X(koordinat_hassasiyeti)                                                                       \
    X(cizim_birimi)                                                                                \
    X(cizgi_tipi_olcegi)                                                                           \
    X(metin_yuksekligi)                                                                            \
    X(veri_paketi_surumu)                                                                          \
    X(tema)                                                                                        \
    X(dil)                                                                                         \
    X(otomatik_kayit)                                                                              \
    X(son_dosya_sayisi)                                                                            \
    X(tuval_arkaplani)                                                                             \
    X(yakalama_modlari)                                                                            \
    X(dik_mod)                                                                                     \
    X(kutupsal_aci)

// ---- proje kapsamı: dışa aktarılan belgenin baytını değiştirebilenler --------

PIRICAD_SETTING(koordinat_sistemi)
{
    return SettingSpec{
        .id       = "core.crs.id",
        .names    = {"koordinat_sistemi", "crs", "ks"},
        .type     = SettingType::Text,
        .scope    = SettingScope::Project,
        .fallback = text_value("TUREF/TM30"),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Projenin koordinat sistemi. Dışa aktarılan her koordinat bu sisteme "
                    "göre yazıldığı için proje kapsamındadır.",
    };
}

PIRICAD_SETTING(koordinat_hassasiyeti)
{
    return SettingSpec{
        .id       = "core.crs.hassasiyet",
        .names    = {"koordinat_hassasiyeti", "hassasiyet", "precision"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(3),
        .range    = SettingRange::between(0, 6),
        .values   = {},
        .unit     = "hane",
        .summary  = "Koordinat cetvellerinde yazılan ondalık hane sayısı. Görüntüleme gibi "
                    "durur ama imzalanan cetvelin baytını değiştirir, bu yüzden projededir.",
    };
}

PIRICAD_SETTING(cizim_birimi)
{
    return SettingSpec{
        .id       = "core.cizim.birim",
        .names    = {"çizim_birimi", "cizim_birimi", "birim", "units"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::enumerated(2),
        .range    = SettingRange::between(0, 2),
        .values   = {"milimetre", "santimetre", "metre"},
        .unit     = "",
        .summary  = "Uzunlukların yazıldığı birim. Dışa aktarılan dosyadaki sayıların "
                    "anlamını belirlediği için proje kapsamındadır.",
    };
}

PIRICAD_SETTING(cizgi_tipi_olcegi)
{
    return SettingSpec{
        .id       = "core.cizim.cizgi_tipi_olcegi",
        .names    = {"çizgi_tipi_ölçeği", "cizgi_tipi_olcegi", "ltscale"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(kPerMille),
        .range    = SettingRange::between(1, 1000 * kPerMille),
        .values   = {},
        .unit     = "‰",
        .summary  = "Kesikli çizgi deseninin ölçeği, binde cinsinden (1000 = 1,000 kat). "
                    "Paftadaki çizgi görünümünü değiştirdiği için proje kapsamındadır.",
    };
}

PIRICAD_SETTING(metin_yuksekligi)
{
    return SettingSpec{
        .id       = "core.cizim.metin_yuksekligi",
        .names    = {"metin_yüksekliği", "metin_yuksekligi", "textsize"},
        .type     = SettingType::Length,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::length(2500),
        .range    = SettingRange::between(1, 1000000000),
        .values   = {},
        .unit     = "mm",
        .summary  = "Varsayılan yazı yüksekliği, zeminde milimetre. Paftaya basılan her "
                    "yazının boyu olduğu için proje kapsamındadır.",
    };
}

PIRICAD_SETTING(veri_paketi_surumu)
{
    return SettingSpec{
        .id    = "core.katalog.paket_surumu",
        .names = {"paket_sürümü", "paket_surumu", "katalog", "package"},
        .type  = SettingType::Text,
        .scope = SettingScope::Project,
        // R35: the package version stamp is a CRITICAL part of the document, and a
        // file whose regulatory basis is unknown MUST NOT open silently. A usable
        // default made "this document declares 0.1.0" and "this document declares
        // nothing" read identically through get(), and only is_explicit() could
        // tell them apart — which no caller checked. The default is therefore
        // EMPTY: an empty package version is the visible statement "unknown",
        // which the loader can refuse, rather than a plausible answer it cannot
        // distinguish from a real one. attribute.hpp says the same thing about
        // Catalogue: never defaulted.
        .fallback = text_value(""),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Nesne referanslarının okunduğu veri paketi sürümü. Çizimin hangi "
                    "mevzuat sürümüne dayandığı belgenin kendi bilgisidir, bu yüzden "
                    "proje kapsamındadır; varsayılanı yoktur, boş bir değer "
                    "'dayanağı bilinmiyor' demektir.",
    };
}

// ---- uygulama kapsamı: kullanıcıya ve makineye ait, belgeye girmeyenler ------

PIRICAD_SETTING(tema)
{
    return SettingSpec{
        .id       = "core.arayuz.tema",
        .names    = {"tema", "theme"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::between(0, 2),
        .values   = {"sistem", "acik", "koyu"},
        .unit     = "",
        .summary  = "Arayüz teması. Yalnızca ekranı etkiler, dışa aktarılan hiçbir baytı "
                    "değiştirmez; bu yüzden uygulama kapsamındadır.",
    };
}

PIRICAD_SETTING(dil)
{
    return SettingSpec{
        .id       = "core.arayuz.dil",
        .names    = {"dil", "language"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::between(0, 1),
        .values   = {"tr", "en"},
        .unit     = "",
        .summary  = "Arayüz dili. Kullanıcıya ait bir tercihtir; çizimin verisiyle ilgisi "
                    "olmadığı için uygulama kapsamındadır.",
    };
}

PIRICAD_SETTING(otomatik_kayit)
{
    return SettingSpec{
        .id       = "core.dosya.otomatik_kayit",
        .names    = {"otomatik_kayıt", "otomatik_kayit", "autosave"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(300),
        .range    = SettingRange::between(0, 86400),
        .values   = {},
        .unit     = "sn",
        .summary  = "Otomatik kayıt aralığı, saniye; 0 kapatır. Ne zaman kaydedildiği "
                    "belgenin içeriğini değiştirmediği için uygulama kapsamındadır.",
    };
}

PIRICAD_SETTING(son_dosya_sayisi)
{
    return SettingSpec{
        .id       = "core.dosya.son_dosya_sayisi",
        .names    = {"son_dosya_sayısı", "son_dosya_sayisi", "recent"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(10),
        .range    = SettingRange::between(0, 50),
        .values   = {},
        .unit     = "adet",
        .summary  = "Dosya menüsünde tutulan son dosya sayısı. Makineye ait bir listedir, "
                    "bu yüzden uygulama kapsamındadır.",
    };
}

PIRICAD_SETTING(tuval_arkaplani)
{
    return SettingSpec{
        .id       = "core.tuval.arkaplan",
        .names    = {"tuval_arkaplanı", "tuval_arkaplani", "arkaplan", "background"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0xFF1E2128),
        .range    = SettingRange::between(0, 0xFFFFFFFF),
        .values   = {},
        .unit     = "",
        .summary  = "Tuval arka plan rengi, 0xAARRGGBB düzeninde tam sayı (onaltılık de "
                    "yazılabilir). Ekranda görünür, paftaya basılmaz; uygulama kapsamındadır.",
    };
}

// ---- oturum kapsamı: geçici, kaydedilmez, özete girmez ----------------------

PIRICAD_SETTING(yakalama_modlari)
{
    return SettingSpec{
        .id       = "core.yakalama.modlar",
        .names    = {"yakalama_modları", "yakalama_modlari", "yakalama", "osmode"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::integer(0x7),
        .range    = SettingRange::between(0, 0xFFFF),
        .values   = {},
        .unit     = "bit maskesi",
        .summary  = "Etkin nesne yakalama modları, bit maskesi. Yakalama çizimi değil "
                    "çizme biçimini etkilediği için oturum kapsamındadır.",
    };
}

PIRICAD_SETTING(dik_mod)
{
    return SettingSpec{
        .id       = "core.yakalama.dik_mod",
        .names    = {"dik_mod", "ortho"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::between(0, 1),
        .values   = {},
        .unit     = "",
        .summary  = "Dik mod: imleci yatay ve düşey eksene kilitler. Girdi yardımıdır, "
                    "kaydedilmez; oturum kapsamındadır.",
    };
}

PIRICAD_SETTING(kutupsal_aci)
{
    return SettingSpec{
        .id       = "core.yakalama.kutupsal_aci",
        .names    = {"kutupsal_açı", "kutupsal_aci", "polarang"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::integer(45 * kMicroDeg),
        .range    = SettingRange::between(1000, kFullCircle),
        .values   = {},
        .unit     = "µderece",
        .summary  = "Kutupsal izleme açı adımı, mikro derece (45000000 = 45°). Girdi "
                    "yardımıdır, kaydedilmez; oturum kapsamındadır.",
    };
}

const SettingCatalog& builtin_settings()
{
    // Built once and never mutated. It describes the product, not the state of a
    // document, so it is not the mutable registry core.md P8 bans.
    static const SettingCatalog catalogue = [] {
        SettingCatalog c;
#define PIRICAD_REGISTER(sym)                                                                      \
    if (auto st = c.add(piricad_setting_##sym()); !st)                                             \
        log_error("setting registration failed: " + st.error().message);
        PIRICAD_BUILTIN_SETTINGS(PIRICAD_REGISTER)
#undef PIRICAD_REGISTER
        return c;
    }();
    return catalogue;
}

#undef PIRICAD_BUILTIN_SETTINGS

// ---------------------------------------------------------- conversion ------

std::string format_setting(const SettingSpec& spec, const SettingValue& v)
{
    switch (v.type()) {
    case SettingType::Bool: return v.as_bool() ? "evet" : "hayır";
    case SettingType::Int:
    case SettingType::Length: return std::to_string(v.as_int());
    case SettingType::Text: return std::string(v.as_text());
    case SettingType::Enum:
        return v.as_enum() < spec.values.size() ? spec.values[v.as_enum()]
                                                : std::to_string(v.as_enum());
    }
    return {};
}

Result<SettingValue> parse_setting(const SettingSpec& spec, std::string_view text)
{
    const auto reject = [&](std::string_view expected) {
        return err(ErrorCode::InvalidArgument, quote(spec.id) + " ayarı " + std::string(expected) +
                                                   " bekliyor. Girilen: " + quote(text));
    };

    switch (spec.type) {
    case SettingType::Bool: {
        for (const char* yes : {"EVET", "YES", "TRUE", "1", "AÇIK"})
            if (turkish_iequals(text, yes)) return SettingValue::boolean(true);
        for (const char* no : {"HAYIR", "NO", "FALSE", "0", "KAPALI"})
            if (turkish_iequals(text, no)) return SettingValue::boolean(false);
        return reject("evet/hayır");
    }
    case SettingType::Int: {
        std::int64_t n = 0;
        if (!parse_scalar(text, n)) return reject("tam sayı");
        return SettingValue::integer(n);
    }
    case SettingType::Length: {
        std::int64_t n = 0;
        if (!parse_scalar(text, n)) return reject("uzunluk (tam sayı milimetre)");
        return SettingValue::length(n);
    }
    case SettingType::Text: return SettingValue::text(text);
    case SettingType::Enum: {
        for (std::size_t i = 0; i < spec.values.size(); ++i)
            if (turkish_iequals(text, spec.values[i]))
                return SettingValue::enumerated(static_cast<std::uint16_t>(i));

        // An index is accepted too, because that is what a file and a journal store.
        std::int64_t n = 0;
        if (parse_scalar(text, n) && n >= 0 && n < static_cast<std::int64_t>(spec.values.size()))
            return SettingValue::enumerated(static_cast<std::uint16_t>(n));

        return err(ErrorCode::InvalidArgument, quote(spec.id) +
                                                   " ayarı şu seçeneklerden birini bekliyor: " +
                                                   join(spec.values) + ". Girilen: " + quote(text));
    }
    }
    return reject("değer");
}

// ---------------------------------------------------------------- store -----

Settings::Settings(const SettingCatalog& catalogue, SettingScopeMask accepted)
    : cat_(&catalogue), accepted_(accepted)
{}

const SettingValue* Settings::find_explicit(std::uint32_t index) const
{
    const auto it = std::lower_bound(values_.begin(), values_.end(), index,
                                     [](const auto& e, std::uint32_t k) { return e.first < k; });
    return it != values_.end() && it->first == index ? &it->second : nullptr;
}

SettingValue Settings::get(std::string_view id) const
{
    const std::uint32_t index = cat_->find(id);
    if (index == kNoSetting) return {};
    if (const SettingValue* v = find_explicit(index)) return *v;
    return cat_->at(index).fallback;
}

Result<SettingValue> Settings::lookup(std::string_view id) const
{
    const std::uint32_t index = cat_->find(id);
    if (index == kNoSetting) {
        std::string message = "Bilinmeyen ayar: " + quote(id) + ". Beklenen: tanımlı bir ayar " +
                              "kimliği veya adı (" + std::to_string(cat_->size()) +
                              " tanımlı ayar).";
        if (const std::string near = cat_->suggest(id); !near.empty())
            message += " Bunu mu demek istediniz: " + quote(near) + "?";
        return err(ErrorCode::NotFound, std::move(message));
    }
    if (const SettingValue* v = find_explicit(index)) return *v;
    return cat_->at(index).fallback;
}

bool Settings::is_explicit(std::string_view id) const
{
    const std::uint32_t index = cat_->find(id);
    return index != kNoSetting && find_explicit(index) != nullptr;
}

bool Settings::is_default(std::string_view id) const
{
    const std::uint32_t index = cat_->find(id);
    if (index == kNoSetting) return false;
    const SettingValue* v = find_explicit(index);
    return v == nullptr || *v == cat_->at(index).fallback;
}

std::vector<std::string> Settings::explicit_ids() const
{
    std::vector<std::string> out;
    out.reserve(values_.size());
    for (const auto& entry : values_)
        out.push_back(cat_->at(entry.first).id);
    std::sort(out.begin(), out.end());
    return out;
}

Result<SettingChange> Settings::set(std::string_view id, SettingValue v)
{
    const std::uint32_t index = cat_->find(id);
    if (index == kNoSetting) {
        auto probe = lookup(id); // one message, written once
        return probe.error();
    }

    const SettingSpec& spec = cat_->at(index);

    if (!accepts(accepted_, spec.scope))
        return err(ErrorCode::InvalidArgument,
                   quote(spec.id) + " ayarı " + setting_scope_label(spec.scope) +
                       " kapsamındadır; bu kutu yalnızca " + scope_list(accepted_) +
                       " kapsamını kabul eder.");

    if (v.type() != spec.type)
        return err(ErrorCode::InvalidArgument,
                   quote(spec.id) + " ayarı " + setting_type_label(spec.type) +
                       " bekliyor. Verilen tür: " + setting_type_label(v.type()) + ".");

    SettingChange change;
    change.id           = spec.id;
    change.was_explicit = find_explicit(index) != nullptr;
    change.before       = change.was_explicit ? *find_explicit(index) : spec.fallback;

    // R42: out of range is CLAMPED with a recorded warning. Not silently accepted,
    // because a document must never carry a value the build cannot honour; and not
    // rejected, because that would make a file written by another version unopenable.
    if (v.type() != SettingType::Text) {
        SettingRange effective = spec.range;
        if (spec.type == SettingType::Enum)
            effective.max = std::min<std::int64_t>(
                effective.max, static_cast<std::int64_t>(spec.values.size()) - 1);

        if (!effective.contains(v.scalar())) {
            const std::int64_t clamped = effective.clamp(v.scalar());
            warnings_.push_back(SettingWarning{
                spec.id, quote(spec.id) + " için " + std::to_string(v.scalar()) + " değeri [" +
                             std::to_string(effective.min) + ", " + std::to_string(effective.max) +
                             "] aralığının dışında; " + std::to_string(clamped) +
                             " değerine kırpıldı."});
            v.set_scalar(clamped);
            change.clamped = true;
        }
    }

    change.after = v;

    const auto it = std::lower_bound(values_.begin(), values_.end(), index,
                                     [](const auto& e, std::uint32_t k) { return e.first < k; });
    if (it != values_.end() && it->first == index)
        it->second = v;
    else
        values_.insert(it, {index, v});

    return change;
}

Status Settings::revert(const SettingChange& change)
{
    const std::uint32_t index = cat_->find(change.id);
    if (index == kNoSetting)
        return err(ErrorCode::NotFound, "Bilinmeyen ayar: " + quote(change.id));

    const auto it      = std::lower_bound(values_.begin(), values_.end(), index,
                                          [](const auto& e, std::uint32_t k) { return e.first < k; });
    const bool present = it != values_.end() && it->first == index;

    // The value came from this store, so it is not re-validated: undo restores what
    // was there, including a value an older package version clamped differently.
    if (change.was_explicit) {
        if (present)
            it->second = change.before;
        else
            values_.insert(it, {index, change.before});
    } else if (present) {
        values_.erase(it);
    }
    return ok();
}

Status Settings::reset(std::string_view id)
{
    const std::uint32_t index = cat_->find(id);
    if (index == kNoSetting) {
        auto probe = lookup(id);
        return probe.error();
    }

    const auto it = std::lower_bound(values_.begin(), values_.end(), index,
                                     [](const auto& e, std::uint32_t k) { return e.first < k; });
    if (it != values_.end() && it->first == index) values_.erase(it);
    return ok();
}

void Settings::clear()
{
    values_.clear();
}

std::uint64_t Settings::fold(std::uint64_t seed) const
{
    // Sorted by id, not by registration order: inserting a setting into the middle
    // of the declaration list must not rewrite every stored golden hash.
    std::vector<const std::pair<std::uint32_t, SettingValue>*> project;
    for (const auto& entry : values_)
        if (cat_->at(entry.first).scope == SettingScope::Project) project.push_back(&entry);

    std::sort(project.begin(), project.end(), [this](const auto* a, const auto* b) {
        return cat_->at(a->first).id < cat_->at(b->first).id;
    });

    std::uint64_t h = seed;
    for (const auto* entry : project) {
        h = fnv1a(cat_->at(entry->first).id, h);
        h = entry->second.fold(h);
    }
    return h;
}

} // namespace piricad::core
