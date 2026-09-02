// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: settings.
#include "piricad/core/settings.hpp"

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

void SettingCatalog::add_section(SettingSection s)
{
    sections_.push_back(std::move(s));
}

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
        std::string f = turkish_fold_key(a);
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
    const std::string f = turkish_fold_key(id_or_name);
    for (std::size_t i = 0; i < folded_.size(); ++i)
        if (folded_[i] == f) return owner_[i];
    return kNoSetting;
}

std::string SettingCatalog::suggest(std::string_view typed) const
{
    if (typed.empty()) return {};
    const std::string f = turkish_fold_key(typed);

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
// The angle constants are the ones units.hpp declares: the snap engine reads the
// same values, and a second definition of "one degree" is a second list.
constexpr std::int64_t kPerMille   = 1000; // 1.000 as a per-mille ratio
constexpr std::int64_t kMicroDeg   = kUDegPerDegree;
constexpr std::int64_t kFullCircle = kUDegFullCircle;

} // namespace

PIRICAD_SETTING(vektor_kutuphanesi)
{
    return SettingSpec{
        .id       = "core.stil.vektor",
        .names    = {"vektör_paketi", "vektor_paketi", "vector_library", "vektör"},
        .type     = SettingType::Text,
        .scope    = SettingScope::App,
        .fallback = text_value(
            "data/catalogs/mpyy-vektor/plan-gosterim.json"), // catalog-key: a path into
                                                             // /data/catalogs, not a value
        .range   = SettingRange::unbounded(),
        .values  = {},
        .unit    = "",
        .summary = "Resimli paketin ÜSTÜNE yüklenecek vektör gösterim paketi. Raf her " // ui-label
                   "kimlikten bir satır tutar ve aynı kimliği yeniden bildiren paket "
                   "öncekinin yerine geçer: burada yeniden çizilmiş bir satır "
                   "ekin resmini değiştirir, çizilmemiş olan resmiyle kalır ve eksilmez. "
                   "Bir resim yeniden renklendirilemez, keskin ölçeklenemez, DWG'ye "
                   "çizgi tipi olarak yazılamaz ve köşe dönemez — vektörü bunların "
                   "hepsini yapar. Ayrı bir ayardır çünkü bir metin ayarı 48 bayt alır "
                   "ve iki yol birlikte sığmaz. Boş bırakılırsa yalnız resimli paket "
                   "yüklenir. Hangi paketin kurulu olduğu makineye ait olduğu için "
                   "uygulama kapsamındadır.",
        .section = "Veri Kaynakları", // ui-label
    };
}

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
PIRICAD_SETTING(kosegen_kilidi);
PIRICAD_SETTING(kutupsal_aci);
PIRICAD_SETTING(sembol_kutuphanesi);
PIRICAD_SETTING(vektor_kutuphanesi);
PIRICAD_SETTING(veritabani_sunucu);
PIRICAD_SETTING(veritabani_port);
PIRICAD_SETTING(veritabani_ad);
PIRICAD_SETTING(veritabani_kullanici);
PIRICAD_SETTING(yakalama_uzanti);
PIRICAD_SETTING(yakalama_isaret_boyu);
PIRICAD_SETTING(yakalama_isaret_rengi);
PIRICAD_SETTING(yakalama_ipucu);
PIRICAD_SETTING(yakalama_adimi);
PIRICAD_SETTING(dinamik_girdi);
PIRICAD_SETTING(izgara_rengi);
PIRICAD_SETTING(izgara_ana_rengi);
PIRICAD_SETTING(izgara_adimi_y);
PIRICAD_SETTING(cetvel_gorunur);
PIRICAD_SETTING(cetvel_kalinligi);
PIRICAD_SETTING(cetvel_birimi);
PIRICAD_SETTING(harita_olcek_cubugu);
PIRICAD_SETTING(harita_kuzey_oku);
PIRICAD_SETTING(harita_koordinat);
PIRICAD_SETTING(harita_imlec);
PIRICAD_SETTING(harita_imlec_boyu);
PIRICAD_SETTING(harita_yakinlastirma);
PIRICAD_SETTING(harita_tekerlek_ters);
PIRICAD_SETTING(secim_rengi);
PIRICAD_SETTING(silme_onayi);
PIRICAD_SETTING(secim_vurgu_rengi);
PIRICAD_SETTING(plan_olcegi);
PIRICAD_SETTING(aci_birimi);
PIRICAD_SETTING(alan_birimi);

#define PIRICAD_BUILTIN_SETTINGS(X)                                                                \
    X(koordinat_sistemi)                                                                           \
    X(koordinat_hassasiyeti)                                                                       \
    X(cizim_birimi)                                                                                \
    X(cizgi_tipi_olcegi)                                                                           \
    X(metin_yuksekligi)                                                                            \
    X(veri_paketi_surumu)                                                                          \
    X(dugum_toleransi)                                                                             \
    X(en_kucuk_alan)                                                                               \
    X(tema)                                                                                        \
    X(dil)                                                                                         \
    X(otomatik_kayit)                                                                              \
    X(son_dosya_sayisi)                                                                            \
    X(tuval_arkaplani)                                                                             \
    X(izgara_gorunur)                                                                              \
    X(izgara_modu)                                                                                 \
    X(izgara_adimi)                                                                                \
    X(izgara_ana_cizgi)                                                                            \
    X(yakalama_toleransi)                                                                          \
    X(secim_toleransi)                                                                             \
    X(yakalama_modlari)                                                                            \
    X(dik_mod)                                                                                     \
    X(kosegen_kilidi)                                                                              \
    X(sembol_kutuphanesi)                                                                          \
    X(vektor_kutuphanesi)                                                                          \
    X(kutupsal_aci)                                                                                \
    X(izgaraya_yakala)                                                                             \
    X(veritabani_sunucu)                                                                           \
    X(veritabani_port)                                                                             \
    X(veritabani_ad)                                                                               \
    X(veritabani_kullanici)                                                                        \
    X(yakalama_uzanti)                                                                             \
    X(yakalama_isaret_boyu)                                                                        \
    X(yakalama_isaret_rengi)                                                                       \
    X(yakalama_ipucu)                                                                              \
    X(yakalama_adimi)                                                                              \
    X(dinamik_girdi)                                                                               \
    X(izgara_rengi)                                                                                \
    X(izgara_ana_rengi)                                                                            \
    X(izgara_adimi_y)                                                                              \
    X(cetvel_gorunur)                                                                              \
    X(cetvel_kalinligi)                                                                            \
    X(cetvel_birimi)                                                                               \
    X(harita_olcek_cubugu)                                                                         \
    X(harita_kuzey_oku)                                                                            \
    X(harita_koordinat)                                                                            \
    X(harita_imlec)                                                                                \
    X(harita_imlec_boyu)                                                                           \
    X(harita_yakinlastirma)                                                                        \
    X(harita_tekerlek_ters)                                                                        \
    X(secim_rengi)                                                                                 \
    X(silme_onayi)                                                                                 \
    X(secim_vurgu_rengi)                                                                           \
    X(plan_olcegi)                                                                                 \
    X(aci_birimi)                                                                                  \
    X(alan_birimi)

// ---- SNAP: what the aid layer looks for, and what the canvas draws when it ----

PIRICAD_SETTING(yakalama_adimi)
{
    return SettingSpec{
        .id       = "core.yakalama.adim",
        .names    = {"adım", "adim", "yakalama_adımı", "yakalama_adimi", "snapstep"},
        .type     = SettingType::Length,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::length(0),
        .range    = SettingRange::between(0, 1000000000),
        .values   = {},
        .unit     = "mm",
        .summary  = "İmlecin bir önceki noktaya olan UZAKLIĞININ yuvarlanacağı adım. "
                    "0 kapatır. 120 verilirse çizgi 12 cm, 24 cm, 36 cm... uzunluklarda "
                    "durur; kutupsal izleme açıkken kutupsal bir ızgara olur. Zemin "
                    "milimetresidir ve çizme biçimini etkilediği için oturum kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(dinamik_girdi)
{
    return SettingSpec{
        .id       = "core.arayuz.dinamik_girdi",
        .names    = {"dinamik_girdi", "dynamicinput", "dyn"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Sürüklenen kılavuzun üzerinde uzunluğu ve azimutu yazar. Ele ve "
                    "ekrana ait bir tercih olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(yakalama_uzanti)
{
    return SettingSpec{
        .id       = "core.yakalama.uzanti_carpani",
        .names    = {"uzantı_çarpanı", "uzanti_carpani", "extensionreach", "uzantı"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(10),
        .range    = SettingRange::between(0, 200),
        .values   = {},
        .unit     = "× açıklık",
        .summary  = "UZANTI, PARALEL ve UZATILMIŞ KESİŞİM modlarının, yakalama açıklığının "
                    "kaç katı ötesindeki kenarlardan nokta kurabileceği. Bu üç mod "
                    "imlecin altında olmayan bir kenardan nokta üretir, açıklık tek "
                    "başına o kenarı hiç bulamaz. 0 yazılırsa üç mod da maskede açık "
                    "olsa bile çalışmaz. Görüşe bağlı bir tercih olduğu için uygulama "
                    "kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(yakalama_isaret_boyu)
{
    return SettingSpec{
        .id       = "core.yakalama.isaret_boyu",
        .names    = {"yakalama_işareti_boyu", "yakalama_isareti_boyu", "snapmarkersize"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(12),
        .range    = SettingRange::between(4, 48),
        .values   = {},
        .unit     = "piksel",
        .summary  = "Yakalama işaretinin kenar uzunluğu, ekran pikseli. Ekrana ait bir "
                    "ölçü olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(yakalama_isaret_rengi)
{
    return SettingSpec{
        .id       = "core.yakalama.isaret_rengi",
        .names    = {"yakalama_işareti_rengi", "yakalama_isareti_rengi", "snapmarkercolour"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 0xFFFFFFFFLL),
        .values   = {},
        .unit     = "0xAARRGGBB",
        .summary  = "Yakalama işaretinin rengi. 0 yazılırsa temanın kendi rengi kullanılır. "
                    "Ekrana ait olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(yakalama_ipucu)
{
    return SettingSpec{
        .id       = "core.yakalama.ipucu",
        .names    = {"yakalama_ipucu", "snaptip"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Yakalama işaretinin yanında hangi modun tuttuğunu yazar ('uç nokta', "
                    "'uzantı'). Kapatılırsa yalnızca işaret çizilir. Ekrana ait olduğu "
                    "için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

// ---- GRID: the lattice's own appearance --------------------------------------

PIRICAD_SETTING(izgara_rengi)
{
    return SettingSpec{
        .id       = "core.izgara.renk",
        .names    = {"ızgara_rengi", "izgara_rengi", "gridcolour"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 0xFFFFFFFFLL),
        .values   = {},
        .unit     = "0xAARRGGBB",
        .summary  = "Ara ızgara çizgilerinin rengi. 0 yazılırsa temanın kendi rengi "
                    "kullanılır. Çizime girmediği için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgara_ana_rengi)
{
    return SettingSpec{
        .id       = "core.izgara.ana_renk",
        .names    = {"ızgara_ana_rengi", "izgara_ana_rengi", "gridmajorcolour"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 0xFFFFFFFFLL),
        .values   = {},
        .unit     = "0xAARRGGBB",
        .summary  = "Ana ızgara çizgilerinin rengi. 0 yazılırsa temanın kendi rengi "
                    "kullanılır. Çizime girmediği için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgara_adimi_y)
{
    return SettingSpec{
        .id       = "core.izgara.adim_y",
        .names    = {"ızgara_dikey_adımı", "izgara_dikey_adimi", "ızgara_adımı_y", "izgara_adimi_y",
                     "gridunity"},
        .type     = SettingType::Length,
        .scope    = SettingScope::App,
        .fallback = SettingValue::length(0),
        .range    = SettingRange::between(0, 1000000000),
        .values   = {},
        .unit     = "mm",
        .summary  = "İkinci eksende ızgara adımı, zeminde milimetre. 0 yazılırsa ızgara "
                    "karedir ve 'ızgara_adımı' iki eksende de geçerlidir. Çizime "
                    "girmediği için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

// ---- RULER ------------------------------------------------------------------

PIRICAD_SETTING(cetvel_gorunur)
{
    return SettingSpec{
        .id       = "core.cetvel.gorunur",
        .names    = {"cetvel_görünür", "cetvel_gorunur", "ruler"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Tuvalin üstünde ve solunda cetvel şeridi çizilir. Çizime girmediği "
                    "için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(cetvel_kalinligi)
{
    return SettingSpec{
        .id    = "core.cetvel.kalinlik",
        .names = {"cetvel_kalınlığı", "cetvel_kalinligi", "rulersize"},
        .type  = SettingType::Int,
        .scope = SettingScope::App,
        // 20, which is what design.md 7 draws and what the reference measures.
        .fallback = SettingValue::integer(20),
        .range    = SettingRange::between(12, 64),
        .values   = {},
        .unit     = "piksel",
        .summary  = "Cetvel şeridinin kalınlığı, ekran pikseli. Ekrana ait bir ölçü "
                    "olduğu için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(cetvel_birimi)
{
    return SettingSpec{
        .id       = "core.cetvel.birim",
        .names    = {"cetvel_birimi", "rulerunit"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::unbounded(),
        .values   = {"metre", "santimetre", "kilometre"},
        .unit     = "",
        .summary  = "Cetvelin rakamlarının birimi. Zemin ölçüsünü nasıl okuduğunuzla "
                    "ilgilidir, çizimin kendi birimini değiştirmez; bu yüzden uygulama "
                    "kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

// ---- MAP: what sits on top of the drawing ------------------------------------

PIRICAD_SETTING(harita_olcek_cubugu)
{
    return SettingSpec{
        .id       = "core.harita.olcek_cubugu",
        .names    = {"ölçek_çubuğu", "olcek_cubugu", "scalebar"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Tuvalin köşesinde, o anki yakınlaştırmaya göre bir ölçek çubuğu "
                    "çizilir. Çizime girmediği için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_kuzey_oku)
{
    return SettingSpec{
        .id       = "core.harita.kuzey_oku",
        .names    = {"kuzey_oku", "northarrow"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Tuvalin köşesinde kuzey oku çizilir. Çizime girmediği için uygulama "
                    "kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_koordinat)
{
    return SettingSpec{
        .id       = "core.harita.koordinat_gostergesi",
        .names    = {"koordinat_göstergesi", "koordinat_gostergesi", "coordreadout"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "İmlecin bulunduğu noktanın sağa/yukarı değeri tuvalde gösterilir. "
                    "Çizime girmediği için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_imlec)
{
    return SettingSpec{
        .id       = "core.harita.imlec",
        .names    = {"imleç", "imlec", "cursor"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::unbounded(),
        .values   = {"tam_ekran", "kisa", "yok"},
        .unit     = "",
        .summary  = "Nişan imlecinin biçimi: tuvali baştan başa geçen çizgiler, kısa bir "
                    "artı, ya da hiç. Ekrana ait olduğu için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_imlec_boyu)
{
    return SettingSpec{
        .id       = "core.harita.imlec_boyu",
        .names    = {"imleç_boyu", "imlec_boyu", "cursorsize"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(30),
        .range    = SettingRange::between(4, 400),
        .values   = {},
        .unit     = "piksel",
        .summary  = "Kısa imlecin kol uzunluğu, ekran pikseli. 'imleç' ayarı 'kısa' iken "
                    "kullanılır. Ekrana ait olduğu için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_yakinlastirma)
{
    return SettingSpec{
        .id       = "core.harita.yakinlastirma_adimi",
        .names    = {"yakınlaştırma_adımı", "yakinlastirma_adimi", "zoomstep"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(20),
        .range    = SettingRange::between(2, 100),
        .values   = {},
        .unit     = "%",
        .summary  = "Farenin her tekerlek çentiğinde ölçeğin yüzde kaç değişeceği. "
                    "Ekrana ait bir tercih olduğu için uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(harita_tekerlek_ters)
{
    return SettingSpec{
        .id       = "core.harita.tekerlek_ters",
        .names    = {"tekerlek_ters", "invertwheel"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Tekerleği ileri itmek uzaklaştırır. Ekrana ait bir tercih olduğu için "
                    "uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

PIRICAD_SETTING(silme_onayi)
{
    return SettingSpec{
        .id       = "core.duzenleme.silme_onayi",
        .names    = {"silme_onayı", "silme_onayi", "confirmdelete"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        // The CONFIRMATION is the interface's, never the command's: `SİL` has to
        // run in a headless replay and in a batch, and a command that stopped to
        // ask a question could do neither. So this setting is read by the shell
        // before it sends the command, which is why it is an application-scope
        // preference and not project state.
        .summary  = "Seçili nesneleri silmeden önce onay ister. Çizilen hiçbir baytı "
                    "değiştirmez — yalnız arayüzün soru sorup sormadığını belirler, "
                    "betik ve komut satırı hiçbir zaman sormaz — bu yüzden kullanıcının "
                    "kendi makinesine ait bir uygulama tercihidir.",
        .section  = "Genel", // ui-label
    };
}

PIRICAD_SETTING(secim_rengi)
{
    return SettingSpec{
        .id       = "core.secim.renk",
        .names    = {"seçim_rengi", "secim_rengi", "selectioncolour"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 0xFFFFFFFFLL),
        .values   = {},
        .unit     = "0xAARRGGBB",
        .summary  = "Seçili nesnelerin vurgulanma rengi. 0 yazılırsa temanın kendi rengi "
                    "kullanılır. Çizime girmediği için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(secim_vurgu_rengi)
{
    return SettingSpec{
        .id       = "core.secim.vurgu_renk",
        .names    = {"vurgu_rengi", "vurgu_renk", "highlightcolour"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 0xFFFFFFFFLL),
        .values   = {},
        .unit     = "0xAARRGGBB",
        .summary  = "İmlecin üzerinde durduğu nesnenin vurgulanma rengi. 0 yazılırsa "
                    "temanın kendi rengi kullanılır. Çizime girmediği için uygulama "
                    "kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

// ---- PROJECT scope: anything that can change a byte of the exported document --

PIRICAD_SETTING(plan_olcegi)
{
    return SettingSpec{
        .id       = "core.plan.olcek",
        .names    = {"plan_ölçeği", "plan_olcegi", "pafta_ölçeği", "plotscale"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(1000),
        .range    = SettingRange::between(1, 1000000),
        .values   = {},
        .unit     = "1:N",
        .summary  = "Paftanın ölçek paydası (1000 = 1/1000). Kâğıt biriminde bildirilen "
                    "her ölçünün zeminde ne kadar yer kapladığını bu belirler: 0,5 mm'lik "
                    "bir sınır 1/1000'de 0,5 m, 1/5000'de 2,5 m'dir. Çizimin kendi "
                    "özelliğidir ve dosyayla birlikte gider, bu yüzden proje "
                    "kapsamındadır.",
        .section  = "Plot ve Çıktı", // ui-label
    };
}

PIRICAD_SETTING(aci_birimi)
{
    return SettingSpec{
        .id       = "core.aci.birim",
        .names    = {"açı_birimi", "aci_birimi", "angleunit"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::unbounded(),
        .values   = {"grad", "derece", "radyan"},
        .unit     = "",
        .summary  = "Açıların yazıldığı ve okunduğu birim. Varsayılan GRAD'dır: Türkiye'de "
                    "nirengi, poligon ve aplikasyon hesapları grad ile yürür ve tam daire "
                    "400'dür. Belgenin sayılarının nasıl okunacağını söylediği için proje "
                    "kapsamındadır.",
        .section  = "Genel", // ui-label
    };
}

PIRICAD_SETTING(alan_birimi)
{
    return SettingSpec{
        .id       = "core.alan.birim",
        .names    = {"alan_birimi", "areaunit"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::unbounded(),
        .values   = {"metrekare", "dekar", "hektar"},
        .unit     = "",
        .summary  = "Alanların yazıldığı birim. Tapu ve kadastro metrekare yazar, imar "
                    "uygulamaları dekar ile konuşur (1 dekar = 1000 m²). Belgenin "
                    "sayılarının nasıl okunacağını söylediği için proje kapsamındadır.",
        .section  = "Genel", // ui-label
    };
}

PIRICAD_SETTING(sembol_kutuphanesi)
{
    return SettingSpec{
        .id       = "core.stil.kutuphane",
        .names    = {"sembol_kütüphanesi", "sembol_kutuphanesi", "style_library", "kütüphane"},
        .type     = SettingType::Text,
        .scope    = SettingScope::App,
        .fallback = text_value(
            "data/catalogs/mpyy/plan-gosterim.json"), // catalog-key: a path into /data/catalogs,
                                                      // not a value out of it
        .range   = SettingRange::unbounded(),
        .values  = {},
        .unit    = "",
        .summary = "Açılışta sembol rafına yüklenecek gösterim paketinin yolu. " // ui-label
                   "Yönetmeliğin kendi paketidir ve satırlarını ekin BASTIĞI resimlerle "
                   "taşır; vektör hâli 'vektör_paketi' ayarıyla bunun üstüne yazılır. "
                   "Kapsamı uygulama, çünkü hangi paketin kurulu olduğu makineye aittir, "
                   "çizime değil: bir çizim kullandığı sembolleri kendi içinde taşır ve "
                   "rafı boş bir makinede de aynı açılır. Boş bırakılırsa raf boş başlar.",
        .section = "Veri Kaynakları", // ui-label
    };
}

PIRICAD_SETTING(koordinat_sistemi)
{
    return SettingSpec{
        .id       = "core.crs.id",
        .names    = {"koordinat_sistemi", "crs", "ks"},
        .type     = SettingType::Text,
        .scope    = SettingScope::Project,
        .fallback = text_value("TUREF/TM36"),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Projenin koordinat sistemi. Dışa aktarılan her koordinat bu sisteme "
                    "göre yazıldığı için proje kapsamındadır. Varsayılan TUREF/TM36'dır: "
                    "36 derecelik dilim Ankara'yı ve Orta Anadolu'yu kapsar ve ülkedeki "
                    "işlerin en büyük kısmı orada yürür. Haritaya henüz oturtulmamış bir "
                    "iş için YEREL kullanın.",
        .section  = "Koordinat Sistemleri", // ui-label
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
        .section  = "Koordinat Sistemleri", // ui-label
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
        .section  = "Genel", // ui-label
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
        .section  = "Genel", // ui-label
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
        .section  = "Genel", // ui-label
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
        .section  = "Veri Kaynakları", // ui-label
    };
}

// Tolerances are PROJECT scope, and that is model.md R40 applied literally rather
// than a preference: a node tolerance decides that two corners are one point, which
// changes the coordinate an ifraz or a tevhit produces, which changes a byte of the
// tapu document that gets exported. Were it a per-machine preference, the same
// drawing would report two different parcel areas on two computers.

PIRICAD_SETTING(dugum_toleransi)
{
    return SettingSpec{
        .id       = "core.topoloji.dugum_toleransi",
        .names    = {"düğüm_toleransı", "dugum_toleransi", "tolerans", "tolerance"},
        .type     = SettingType::Length,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::length(10),
        .range    = SettingRange::between(0, 10000),
        .values   = {},
        .unit     = "mm",
        .summary  = "Topoloji düğüm toleransı, zeminde milimetre. Birbirine bu mesafeden "
                    "yakın iki köşe aynı nokta sayılır; ifraz, tevhit ve topoloji "
                    "denetiminin sonucunu değiştirdiği için proje kapsamındadır. "
                    "Varsayılan 10 mm = 1 cm.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(en_kucuk_alan)
{
    return SettingSpec{
        .id       = "core.topoloji.en_kucuk_alan",
        .names    = {"en_küçük_alan", "en_kucuk_alan", "kirpinti", "sliver"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(500000),
        .range    = SettingRange::between(0, 1000000000),
        .values   = {},
        .unit     = "mm²",
        .summary  = "Kırpıntı poligon eşiği, milimetrekare (500000 = 0,5 m²). Bu alandan "
                    "küçük artık yüzeyler topoloji denetiminde kırpıntı olarak raporlanır. "
                    "Denetim çıktısını değiştirdiği için proje kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

// ---- APP scope: per user and machine, and never written into the document ----

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
        .section  = "Görünüm ve Tema", // ui-label
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
        .section  = "Görünüm ve Tema", // ui-label
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
        .section  = "Genel", // ui-label
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
        .section  = "Genel", // ui-label
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
        .unit     = "0xAARRGGBB",
        .summary  = "Tuval arka plan rengi, 0xAARRGGBB düzeninde tam sayı (onaltılık de "
                    "yazılabilir). Ekranda görünür, paftaya basılmaz; uygulama kapsamındadır.",
        .section  = "Görünüm ve Tema", // ui-label
    };
}

// The grid is a display preference; snapping is an input aid. The line between
// them: something you SEE but which does not change where your click lands is
// persistent and App scope, like the theme and the canvas colour; something that
// MOVES the cursor is transient and Session scope.

PIRICAD_SETTING(izgara_gorunur)
{
    return SettingSpec{
        .id       = "core.izgara.gorunur",
        .names    = {"ızgara_görünür", "izgara_gorunur", "ızgara", "izgara", "gridmode"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::between(0, 1),
        .values   = {},
        .unit     = "",
        .summary  = "Izgaranın çizilip çizilmeyeceği. Ekranda görünür, paftaya basılmaz; "
                    "kullanıcıya ait bir görünüm tercihi olduğu için uygulama "
                    "kapsamındadır. Kısayol: F7.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgara_modu)
{
    return SettingSpec{
        .id       = "core.izgara.mod",
        .names    = {"ızgara_modu", "izgara_modu", "gridmod"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::between(0, 1),
        .values   = {"uyarlanır", "sabit"},
        .unit     = "",
        .summary  = "Izgara adımının seçilme biçimi. 'uyarlanır' ölçeğe göre 1/2/5×10ⁿ "
                    "adımlarından okunabilir olanı seçer; 'sabit' her ölçekte "
                    "ızgara_adımı değerini kullanır. Paftaya basılmayan bir görünüm "
                    "tercihi olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgara_adimi)
{
    return SettingSpec{
        .id       = "core.izgara.adim",
        .names    = {"ızgara_adımı", "izgara_adimi", "gridunit"},
        .type     = SettingType::Length,
        .scope    = SettingScope::App,
        .fallback = SettingValue::length(10000),
        .range    = SettingRange::between(1, 1000000000),
        .values   = {},
        .unit     = "mm",
        .summary  = "Sabit ızgara adımı, zeminde milimetre (10000 = 10 m). Yalnızca "
                    "ızgara_modu 'sabit' iken kullanılır. Ekrandaki aralık 2 pikselin "
                    "altına düşerse ızgara o ölçekte çizilmez. Çizime girmediği için "
                    "uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgara_ana_cizgi)
{
    return SettingSpec{
        .id       = "core.izgara.ana_cizgi",
        .names    = {"ana_çizgi", "ana_cizgi", "ızgara_ana_çizgi", "gridmajor"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(5),
        .range    = SettingRange::between(1, 100),
        .values   = {},
        .unit     = "adet",
        .summary  = "Kaç ara çizgide bir koyu ana çizgi çizileceği (5 = her beşinci). "
                    "1 verilirse bütün çizgiler ana çizgi olur. Yalnızca ekranı "
                    "ilgilendirdiği için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(yakalama_toleransi)
{
    return SettingSpec{
        .id       = "core.yakalama.tolerans",
        .names    = {"yakalama_toleransı", "yakalama_toleransi", "aperture"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(12),
        .range    = SettingRange::between(1, 100),
        .values   = {},
        .unit     = "piksel",
        .summary  = "Nesne yakalama arama yarıçapı, ekran pikseli. Zemin metresi değil "
                    "pikseldir: kullanıcı ekrana bakarak nişan alır, bu yüzden tolerans "
                    "yakınlaştırma ile birlikte değişmelidir. Ele ve ekrana ait bir "
                    "büyüklük olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(secim_toleransi)
{
    return SettingSpec{
        .id       = "core.secim.tolerans",
        .names    = {"seçim_toleransı", "secim_toleransi", "pickbox"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(6),
        .range    = SettingRange::between(1, 100),
        .values   = {},
        .unit     = "piksel",
        .summary  = "Seçme kutusunun yarı boyu, ekran pikseli. İmlecin bu kadar "
                    "yakınındaki nesne tıklamayla seçilir. Yakalama toleransından ayrı "
                    "tutulur: nişan almak seçmekten daha geniş bir alan ister. Ele ve "
                    "ekrana ait bir büyüklük olduğu için uygulama kapsamındadır.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

// ---- SESSION scope: transient, never persisted, never hashed ----------------

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
        .section  = "Çizim ve Yakalama", // ui-label
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
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(kosegen_kilidi)
{
    return SettingSpec{
        .id       = "core.yakalama.kosegen",
        .names    = {"köşegen", "kosegen", "diagonal", "kare"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::between(0, 1),
        .values   = {},
        .unit     = "",
        // WHY THIS IS A MODE AND NOT A MOUSE GESTURE. Holding Ctrl while drawing
        // a rectangle to get a square is what every drawing program does, and a
        // modifier key is exactly the kind of capability Article 5.15 refuses to
        // let exist only for the mouse: a script and the AI must be able to ask
        // for the same thing. So Ctrl HOLDS THIS MODE DOWN, and
        // `MOD köşegen=evet` is the same switch typed instead of pressed.
        //
        // It is polar tracking at 45°, not a new constraint: a second corner
        // locked to a 45° diagonal from the first is a square, and the engine
        // that already locks to a polar step needs nothing new to say so.
        .summary = "Köşegen kilidi: imleci öncekinden 45°'nin katlarına kilitler. " // ui-label
                   "DİKDÖRTGEN'in ikinci köşesi böyle kilitlenince kare çıkar. "
                   "Çizerken Ctrl basılı tutmak da bunu açar. Girdi yardımıdır, "
                   "kaydedilmez; oturum kapsamındadır.",
        .section = "Çizim ve Yakalama", // ui-label
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
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

PIRICAD_SETTING(izgaraya_yakala)
{
    return SettingSpec{
        .id       = "core.yakalama.izgara",
        .names    = {"ızgaraya_yakala", "izgaraya_yakala", "snapmode"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::between(0, 1),
        .values   = {},
        .unit     = "",
        .summary  = "Izgaraya yakalama: girilen noktayı en yakın ızgara kesişimine "
                    "oturtur. Izgaranın görünür olması şart değildir. Girdi yardımıdır, "
                    "dik mod ve kutupsal izleme ile aynı sırada oturum kapsamındadır. "
                    "Kısayol: F9.",
        .section  = "Çizim ve Yakalama", // ui-label
    };
}

void SettingCatalog::record_failure(std::string message)
{
    failures_.push_back(std::move(message));
}

// ---- The PostGIS connection, WITHOUT its password -------------------------
//
// Four settings and not one connection string, because a person types four
// things and a program should ask for what a person has. `VERİTABANI baglan`
// still takes a whole libpq string for a script that needs `sslmode` or a
// service file; these are what the window remembers between sessions.
//
// THERE IS NO PASSWORD SETTING AND THERE WILL NOT BE ONE. A settings file is
// plain text in the user's profile, it gets copied into backups and pasted into
// bug reports, and a stored database password is a credential leak with a
// convenience story attached. libpq already solves this properly: `~/.pgpass`
// (`%APPDATA%\postgresql\pgpass.conf` on Windows) is the file it reads, and
// `PGPASSWORD` is the environment variable. `docs/komutlar/veritabani.md` says
// so where a user will read it.
//
// APP scope on all four: which server this machine talks to is a property of the
// machine, not of the drawing. A project mailed to a colleague must not carry a
// pointer at a database they cannot reach — and must not carry a pointer at one
// they CAN.

PIRICAD_SETTING(veritabani_sunucu)
{
    return SettingSpec{
        .id       = "core.veritabani.sunucu",
        .names    = {"veritabanı_sunucu", "veritabani_sunucu", "db_host"},
        .type     = SettingType::Text,
        .scope    = SettingScope::App,
        .fallback = text_value("localhost"),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "PostGIS sunucusunun adresi. Hangi sunucuya bağlanıldığı makineye ait "
                    "bir bilgidir, çizime değil; bu yüzden uygulama kapsamındadır.",
        .section  = "Veri Kaynakları", // ui-label
    };
}

PIRICAD_SETTING(veritabani_port)
{
    return SettingSpec{
        .id       = "core.veritabani.port",
        .names    = {"veritabanı_port", "veritabani_port", "db_port"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::integer(5432),
        .range    = SettingRange::between(1, 65535),
        .values   = {},
        .unit     = "",
        .summary  = "PostgreSQL sunucusunun portu. Öntanımlı 5432, PostgreSQL'in kendi "
                    "öntanımlı portudur. Sunucu adresiyle birlikte makineye ait olduğu "
                    "için uygulama kapsamındadır.",
        .section  = "Veri Kaynakları", // ui-label
    };
}

PIRICAD_SETTING(veritabani_ad)
{
    return SettingSpec{
        .id       = "core.veritabani.ad",
        .names    = {"veritabanı_adı", "veritabani_adi", "db_name"},
        .type     = SettingType::Text,
        .scope    = SettingScope::App,
        .fallback = text_value(""),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Bağlanılacak veritabanının adı. Boş bırakılırsa kullanıcı adıyla aynı "
                    "kabul edilir; libpq'nun kendi kuralıdır. Hangi veritabanına "
                    "bağlanıldığı çizimin verisi değil kurulumun bilgisidir, bu yüzden "
                    "uygulama kapsamındadır.",
        .section  = "Veri Kaynakları", // ui-label
    };
}

PIRICAD_SETTING(veritabani_kullanici)
{
    return SettingSpec{
        .id       = "core.veritabani.kullanici",
        .names    = {"veritabanı_kullanıcı", "veritabani_kullanici", "db_user"},
        .type     = SettingType::Text,
        .scope    = SettingScope::App,
        .fallback = text_value(""),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Veritabanı kullanıcı adı. Kim olarak bağlanıldığı kullanıcıya ve "
                    "makineye aittir, çizime değil; bu yüzden uygulama kapsamındadır. "
                    "Parola BURADA TUTULMAZ: ayar dosyası düz metindir. Parolayı "
                    "~/.pgpass dosyasına ya da PGPASSWORD ortam değişkenine koyun.",
        .section  = "Veri Kaynakları", // ui-label
    };
}

// ---- the pages of the settings window, design.md 10 --------------------------
//
// Declared HERE, beside the settings, because the window is generated from the
// catalogue and a page list kept in the dialog would be a second list (5.10).
// A page with no settings yet is still declared: 11.8 forbids pretending, so it
// names its phase and says on its own page what will be on it.
void register_sections(SettingCatalog& into)
{
    into.add_section({"Genel", "", ""});
    into.add_section({"Görünüm ve Tema", "", ""});
    into.add_section({"Çizim ve Yakalama", "", ""});
    into.add_section({"Koordinat Sistemleri", "", ""});
    into.add_section({"Veri Kaynakları", "", ""});
    into.add_section({"Plot ve Çıktı", "", ""});
    into.add_section({"Etiketleme", "Faz 2",
                      "Etiket yerleşimi, çakışma çözümü ve ölçek aralıkları buraya gelecek. "
                      "Bugün etiketler ETİKET komutuyla yazılır."});
    into.add_section({"Kısayollar", "Faz 2",
                      "Her komuta klavye kısayolu atama buraya gelecek. Bugün kısayollar "
                      "menülerde yazılıdır ve komut satırı her komuta zaten adıyla erişir."});
    into.add_section({"Eklentiler", "Faz 3",
                      "C99 ABI eklenti yöneticisi buraya gelecek; imza doğrulama ve yetenek "
                      "kısıtları dahil (plugin-api.md)."});
    into.add_section({"Performans ve GPU", "Faz 1",
                      "QRhi arka ucu, kare bütçesi ve LOD eşikleri buraya gelecek. Bugün çizim "
                      "QPainter ile yapılıyor (CLAUDE.md 8.1)."});
    into.add_section({"Klasörler ve Şablonlar", "Faz 2",
                      "Varsayılan proje klasörü, şablon ve pafta çerçevesi yolları buraya "
                      "gelecek."});
    into.add_section({"Ağ ve Kimlik", "Faz 3",
                      "Kurumsal servis kimlikleri ve vekil sunucu ayarları buraya gelecek. "
                      "Hangi servislerin destekleneceği veri paketlerinde bildirilir, burada "
                      "değil."});
}

const SettingCatalog& builtin_settings()
{
    // Built once and never mutated. It describes the product, not the state of a
    // document, so it is not the mutable registry core.md P8 bans.
    static const SettingCatalog catalogue = [] {
        SettingCatalog c;
#define PIRICAD_REGISTER(sym)                                                                      \
    if (auto st = c.add(piricad_setting_##sym()); !st) c.record_failure(st.error().message);
        PIRICAD_BUILTIN_SETTINGS(PIRICAD_REGISTER)
#undef PIRICAD_REGISTER
        register_sections(c);
        return c;
    }();
    return catalogue;
}

std::span<const std::string> builtin_setting_failures()
{
    return builtin_settings().failures();
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

    // One lookup, not two. The pair of calls was not only a wasted search: the
    // second one is what an analyser has to assume might return null after the
    // first said otherwise, and it is right to — nothing in the signature promises
    // the two calls agree.
    const SettingValue* current = find_explicit(index);

    SettingChange change;
    change.id           = spec.id;
    change.was_explicit = current != nullptr;
    change.before       = current != nullptr ? *current : spec.fallback;

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

    ++revision_;
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
    ++revision_;
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
    ++revision_;
    return ok();
}

void Settings::clear()
{
    values_.clear();
    ++revision_;
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
