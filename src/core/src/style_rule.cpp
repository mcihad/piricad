// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/style_rule.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <limits>

namespace piricad::core {
namespace {

// Every key this file reads is a STRUCTURAL key of the package format, never a
// regulatory identifier: the rows, their codes and their colours arrive as data
// (CLAUDE.md 5.13). Collecting them here keeps that claim checkable at a glance.
constexpr const char* kKeyId         = "id";
constexpr const char* kKeyVersion    = "package_version";
constexpr const char* kKeySource     = "source";
constexpr const char* kKeyPublished  = "published";
constexpr const char* kKeyLicence    = "licence";
constexpr const char* kKeyEntries    = "stiller";
constexpr const char* kKeyRules      = "kurallar";
constexpr const char* kKeyDashTable  = "cizgi_desenleri";
constexpr const char* kKeyHatchTable = "tarama_desenleri";
constexpr const char* kKeyIndex      = "indeks";
constexpr const char* kKeyLabel      = "ad";
constexpr const char* kKeyRef        = "kaynak";
constexpr const char* kKeyRetired    = "deprecated";
constexpr const char* kKeyStroke     = "cizgi";
constexpr const char* kKeyFill       = "dolgu";
constexpr const char* kKeyColour     = "renk";
constexpr const char* kKeyWidth      = "kalinlik_um";
constexpr const char* kKeyDash       = "desen";
constexpr const char* kKeyHatch      = "tarama";
constexpr const char* kKeySymbol     = "simge";
constexpr const char* kKeyAnnex      = "ek";
constexpr const char* kKeySection    = "bolum";
constexpr const char* kKeyPlanTypes  = "plan_turleri";
constexpr const char* kKeyGroup      = "grup";
constexpr const char* kKeyImages     = "gorseller";
constexpr const char* kKeyImage      = "gorsel";
constexpr const char* kKeyImageFile  = "dosya";
constexpr const char* kKeyImageLine  = "cizgi_tipi";
constexpr const char* kKeyImageHatch = "tarama";
constexpr const char* kKeyImageGlyph = "sembol";

/// The declared symbol-layer stack, and the names inside one.
constexpr const char* kKeyLayers      = "katmanlar";
constexpr const char* kKeyLayerType   = "tip";
constexpr const char* kKeyLayerShape  = "sekil";
constexpr const char* kKeyLayerPlace  = "yerlesim";
constexpr const char* kKeyLayerUnit   = "birim";
constexpr const char* kKeyLayerSize   = "boyut";
constexpr const char* kKeyLayerGap    = "aralik";
constexpr const char* kKeyLayerGapY   = "aralik_y";
constexpr const char* kKeyLayerOffset = "kaydirma";
constexpr const char* kKeyLayerAngle  = "aci";
constexpr const char* kKeyLayerInk    = "renk";
constexpr const char* kKeyLayerFill   = "dolgu_renk";
constexpr const char* kKeyLayerWidth  = "kalinlik";
constexpr const char* kKeyLayerDash   = "desen";
constexpr const char* kKeyLayerText   = "yazi";
constexpr const char* kKeyLayerLock   = "renk_kilidi";
constexpr const char* kKeyClear       = "seffaf";
constexpr const char* kKeyOpacityPct  = "saydamlik_yuzde";
constexpr const char* kKeyGlyphColour = "simge_renk";
constexpr const char* kKeyUncertain   = "belirsiz";
constexpr const char* kKeyWhy         = "belirsiz_nedeni";
constexpr const char* kKeyOrder       = "sira";
constexpr const char* kKeyScale       = "olcek";
constexpr const char* kKeyScaleLow    = "en_kucuk_payda";
constexpr const char* kKeyScaleHigh   = "en_buyuk_payda";
constexpr const char* kKeyEntryRef    = "stil";
constexpr const char* kKeyConditions  = "kosullar";
constexpr const char* kKeyField       = "alan";
constexpr const char* kKeyEquals      = "esittir";
constexpr const char* kKeyOneOf       = "biri";
constexpr const char* kKeyRange       = "aralik";
constexpr const char* kKeyRangeLow    = "en_az";
constexpr const char* kKeyRangeHigh   = "en_cok";
constexpr const char* kKeyPresent     = "var";

/// A named table of symbolic indices — the dash table and the hatch table have
/// the same shape, and both exist so that a catalogue row can say `"surekli"`
/// instead of `0`. A reviewer signing off a legend reads names, not indices.
class IndexTable
{
public:
    Status load(const Json& parent, const char* key)
    {
        key_             = key;
        const Json* node = parent.find(key);
        if (node == nullptr) return ok();
        if (!node->is_array())
            return err(ErrorCode::ParseError,
                       std::string("Stil kataloğu: '") + key + "' bir dizi olmalı.");

        for (const Json& item : node->as_array()) {
            if (!item.is_object())
                return err(ErrorCode::ParseError,
                           std::string("Stil kataloğu: '") + key + "' öğeleri nesne olmalı.");
            const Json* id = item.find(kKeyId);
            const Json* ix = item.find(kKeyIndex);
            if (id == nullptr || !id->is_string() || id->as_string().empty())
                return err(ErrorCode::ParseError,
                           std::string("Stil kataloğu: '") + key + "' öğesinde 'id' eksik.");
            if (ix == nullptr || !ix->is_int())
                return err(ErrorCode::ParseError, std::string("Stil kataloğu: '") + key + "/" +
                                                      id->as_string() +
                                                      "' öğesinde tam sayı 'indeks' eksik.");
            const std::int64_t v = ix->as_int();
            if (v < 0 || v > 0xFFFF)
                return err(
                    ErrorCode::InvalidArgument,
                    std::string("Stil kataloğu: '") + key + "/" + id->as_string() +
                        "' indeksi 0–65535 aralığında olmalı. Girilen: " + std::to_string(v));
            items_.emplace_back(id->as_string(), static_cast<std::uint16_t>(v));
        }
        return ok();
    }

    /// Accepts either a plain index or a symbolic name. An unknown name is an
    /// error, never index zero (domain.md P11).
    Result<std::uint16_t> resolve(const Json& value, std::string_view where) const
    {
        if (value.is_int()) {
            const std::int64_t v = value.as_int();
            if (v < 0 || v > 0xFFFF)
                return err(ErrorCode::InvalidArgument,
                           std::string(where) +
                               ": indeks 0–65535 aralığında olmalı. Girilen: " + std::to_string(v));
            return static_cast<std::uint16_t>(v);
        }
        if (!value.is_string())
            return err(ErrorCode::ParseError, std::string(where) + ": tam sayı indeks veya '" +
                                                  key_ + "' tablosundan bir ad bekleniyor.");

        const std::string& name = value.as_string();
        for (const auto& [id, index] : items_)
            if (id == name) return index;

        std::string known;
        for (const auto& [id, index] : items_) {
            (void)index;
            if (!known.empty()) known += ", ";
            known += id;
        }
        return err(ErrorCode::NotFound,
                   std::string(where) + ": '" + name + "' adı '" + key_ +
                       "' tablosunda yok. Tanımlı adlar: " + (known.empty() ? "(yok)" : known));
    }

private:
    std::string key_;
    std::vector<std::pair<std::string, std::uint16_t>> items_;
};

Result<std::string> required_string(const Json& j, const char* key, std::string_view where)
{
    const Json* node = j.find(key);
    if (node == nullptr || !node->is_string() || node->as_string().empty())
        return err(ErrorCode::ParseError, std::string(where) + ": zorunlu '" + key +
                                              "' alanı eksik veya boş. Beklenen: metin.");
    return node->as_string();
}

std::string optional_string(const Json& j, const char* key)
{
    const Json* node = j.find(key);
    return (node != nullptr && node->is_string()) ? node->as_string() : std::string{};
}

Result<ScaleWindow> parse_scale(const Json& j, std::string_view where)
{
    ScaleWindow window;
    const Json* node = j.find(kKeyScale);
    if (node == nullptr) return window;
    if (!node->is_object())
        return err(ErrorCode::ParseError, std::string(where) + ": '" + kKeyScale +
                                              "' bir nesne olmalı ('" + kKeyScaleLow + "', '" +
                                              kKeyScaleHigh + "').");

    const auto read = [&](const char* key, ScaleDenominator& out) -> Status {
        const Json* v = node->find(key);
        if (v == nullptr) return ok();
        if (!v->is_int() || v->as_int() < 0 ||
            v->as_int() > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
            return err(ErrorCode::InvalidArgument,
                       std::string(where) + ": '" + key +
                           "' negatif olmayan bir tam sayı olmalı (0 = sınırsız).");
        out = static_cast<ScaleDenominator>(v->as_int());
        return ok();
    };

    if (auto st = read(kKeyScaleLow, window.low); !st) return st.error();
    if (auto st = read(kKeyScaleHigh, window.high); !st) return st.error();

    if (window.low != 0 && window.high != 0 && window.low > window.high)
        return err(ErrorCode::InvalidArgument,
                   std::string(where) + ": ölçek aralığı ters — '" + kKeyScaleLow + "' (" +
                       std::to_string(window.low) + ") '" + kKeyScaleHigh + "' (" +
                       std::to_string(window.high) + ") değerinden büyük olamaz.");
    return window;
}

Result<std::int64_t> bounded_int(const Json& j, const char* key, std::int64_t low,
                                 std::int64_t high, std::int64_t fallback, std::string_view where)
{
    const Json* node = j.find(key);
    if (node == nullptr) return fallback;
    if (!node->is_int())
        return err(ErrorCode::ParseError, std::string(where) + ": '" + key + "' tam sayı olmalı.");
    const std::int64_t v = node->as_int();
    if (v < low || v > high)
        return err(ErrorCode::InvalidArgument,
                   std::string(where) + ": '" + key + "' " + std::to_string(low) + "–" +
                       std::to_string(high) + " aralığında olmalı. Girilen: " + std::to_string(v));
    return v;
}

/// The text a condition compares against. A text-shaped cell compares by its
/// text; a numeric cell compares by its decimal rendering, which is exact and
/// locale-independent for an integer.
std::string comparable_text(const AttrValue& v)
{
    switch (v.type) {
    case AttrType::Text:
    case AttrType::CodeRef: return v.text;
    case AttrType::Int64:
    case AttrType::Length:
    case AttrType::Bool: return std::to_string(v.number);
    }
    return {};
}

bool numeric(const AttrValue& v) noexcept
{
    return v.type == AttrType::Int64 || v.type == AttrType::Length || v.type == AttrType::Bool;
}

} // namespace

// ------------------------------------------------------------ FeatureView -----

void FeatureView::set(std::string field, AttrValue value)
{
    for (auto& [name, held] : fields_) {
        if (name == field) {
            held = std::move(value);
            return;
        }
    }
    fields_.emplace_back(std::move(field), std::move(value));
}

void FeatureView::set_text(std::string field, std::string value)
{
    set(std::move(field), attr_text(std::move(value)));
}

void FeatureView::set_number(std::string field, std::int64_t value)
{
    set(std::move(field), attr_int64(value));
}

const AttrValue* FeatureView::find(std::string_view field) const
{
    for (const auto& [name, held] : fields_)
        if (name == field) return &held;
    return nullptr;
}

FeatureView FeatureView::from_row(const AttrTable& table, std::size_t row)
{
    FeatureView view;
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const AttrColumn* column = table.column(static_cast<AttrId>(c));
        if (column == nullptr) continue;
        auto cell = table.get(static_cast<AttrId>(c), row);
        if (!cell) continue;
        if (!cell.value().present) continue;
        view.set(column->spec().id, cell.value());
    }
    return view;
}

// -------------------------------------------------------- StyleCondition -----

const char* style_test_name(StyleCondition::Test t) noexcept
{
    switch (t) {
    case StyleCondition::Test::Equals: return "esittir";
    case StyleCondition::Test::OneOf: return "biri";
    case StyleCondition::Test::Range: return "aralik";
    case StyleCondition::Test::Present: return "var";
    }
    return "?";
}

bool StyleCondition::matches(const FeatureView& feature) const
{
    const AttrValue* cell = feature.find(field);
    if (cell == nullptr || !cell->present) return false;

    switch (test) {
    case Test::Present: return true;

    case Test::Equals:
    case Test::OneOf: {
        const std::string text = comparable_text(*cell);
        return std::find(values.begin(), values.end(), text) != values.end();
    }

    case Test::Range: {
        if (!numeric(*cell)) return false;
        if (has_low && cell->number < low) return false;
        if (has_high && cell->number > high) return false;
        return true;
    }
    }
    return false;
}

bool StyleRule::matches(const FeatureView& feature, ScaleDenominator denominator) const
{
    if (!scale.covers(denominator)) return false;
    for (const auto& condition : conditions)
        if (!condition.matches(feature)) return false;
    return true;
}

// ----------------------------------------------------------- StyleCatalog -----

namespace {

Result<StyleCondition> parse_condition(const Json& j, std::string_view where)
{
    if (!j.is_object())
        return err(ErrorCode::ParseError, std::string(where) + ": koşul bir nesne olmalı.");

    StyleCondition condition;
    auto field = required_string(j, kKeyField, where);
    if (!field) return field.error();
    condition.field = std::move(field.value());

    // Exactly one test per condition. Two tests on one field would need an
    // operator precedence to combine them, and that is the expression evaluator
    // CLAUDE.md 5.11 refuses the project.
    int declared = 0;

    if (const Json* v = j.find(kKeyEquals); v != nullptr) {
        ++declared;
        if (!v->is_string())
            return err(ErrorCode::ParseError,
                       std::string(where) + ": '" + kKeyEquals + "' metin olmalı.");
        condition.test = StyleCondition::Test::Equals;
        condition.values.push_back(v->as_string());
    }

    if (const Json* v = j.find(kKeyOneOf); v != nullptr) {
        ++declared;
        if (!v->is_array() || v->as_array().empty())
            return err(ErrorCode::ParseError, std::string(where) + ": '" + kKeyOneOf +
                                                  "' boş olmayan bir metin dizisi olmalı.");
        condition.test = StyleCondition::Test::OneOf;
        for (const Json& item : v->as_array()) {
            if (!item.is_string())
                return err(ErrorCode::ParseError,
                           std::string(where) + ": '" + kKeyOneOf + "' yalnız metin içerebilir.");
            condition.values.push_back(item.as_string());
        }
    }

    if (const Json* v = j.find(kKeyRange); v != nullptr) {
        ++declared;
        if (!v->is_object())
            return err(ErrorCode::ParseError, std::string(where) + ": '" + kKeyRange +
                                                  "' bir nesne olmalı ('" + kKeyRangeLow + "', '" +
                                                  kKeyRangeHigh + "').");
        condition.test = StyleCondition::Test::Range;
        if (const Json* lo = v->find(kKeyRangeLow); lo != nullptr) {
            if (!lo->is_int())
                return err(ErrorCode::ParseError,
                           std::string(where) + ": '" + kKeyRangeLow +
                               "' tam sayı olmalı — kayıtlı hiçbir alan kayan noktalı "
                               "değildir.");
            condition.low     = lo->as_int();
            condition.has_low = true;
        }
        if (const Json* hi = v->find(kKeyRangeHigh); hi != nullptr) {
            if (!hi->is_int())
                return err(ErrorCode::ParseError,
                           std::string(where) + ": '" + kKeyRangeHigh +
                               "' tam sayı olmalı — kayıtlı hiçbir alan kayan noktalı "
                               "değildir.");
            condition.high     = hi->as_int();
            condition.has_high = true;
        }
        if (!condition.has_low && !condition.has_high)
            return err(ErrorCode::ParseError, std::string(where) + ": '" + kKeyRange +
                                                  "' en az bir uç bildirmeli ('" + kKeyRangeLow +
                                                  "' veya '" + kKeyRangeHigh + "').");
        if (condition.has_low && condition.has_high && condition.low > condition.high)
            return err(ErrorCode::InvalidArgument,
                       std::string(where) + ": aralık ters — '" + kKeyRangeLow + "' (" +
                           std::to_string(condition.low) + ") '" + kKeyRangeHigh + "' (" +
                           std::to_string(condition.high) + ") değerinden büyük olamaz.");
    }

    if (const Json* v = j.find(kKeyPresent); v != nullptr) {
        ++declared;
        if (!v->is_bool() || !v->as_bool())
            return err(ErrorCode::ParseError,
                       std::string(where) + ": '" + kKeyPresent + "' yalnız true olabilir.");
        condition.test = StyleCondition::Test::Present;
    }

    if (declared == 0)
        return err(ErrorCode::ParseError, std::string(where) +
                                              ": koşul hiçbir sınama bildirmiyor. Beklenen: '" +
                                              kKeyEquals + "', '" + kKeyOneOf + "', '" + kKeyRange +
                                              "' veya '" + kKeyPresent + "'.");
    if (declared > 1)
        return err(ErrorCode::ParseError,
                   std::string(where) +
                       ": bir koşul yalnız tek bir sınama bildirebilir; birden fazlası "
                       "bildirilmiş. Ayrı koşullar yazın — koşullar zaten 've' ile bağlanır.");

    return condition;
}

/// Annex code to the plan type it names, e.g. `EK-1a` -> `ORTAK GÖSTERİMLER`.
///
/// Read from the package's own `plan_turleri` block. Without it the tree's top
/// level would be a bare `EK-1a`, which is the annex's file name rather than what
/// a planner calls it.
using AnnexNames = std::vector<std::pair<std::string, std::string>>;

/// Image id to the package-relative file it names.
using ImageFiles = std::vector<std::pair<std::string, std::string>>;

/// The file an image id names, or empty when the package does not list it.
///
/// Empty rather than an error: a row referring to a picture the package left out
/// still has its colour, its identity and its place in the tree, and refusing the
/// whole catalogue over one missing file would take the other 475 rows with it.
std::string image_file(const ImageFiles& files, const std::string& id)
{
    for (const auto& [key, file] : files)
        if (key == id) return file;
    return {};
}

/// The first picture of one role, resolved to its file.
std::string first_image(const Json& images, const ImageFiles& files, const char* role)
{
    const Json* list = images.find(role);
    if (list == nullptr || !list->is_array()) return {};

    for (const Json& item : list->as_array())
        if (item.is_string()) {
            std::string file = image_file(files, item.as_string());
            if (!file.empty()) return file;
        }
    return {};
}

/// The declared name of an annex, or the annex code itself when the package does
/// not name it. Never an error: a package with no `plan_turleri` block still has
/// a usable tree, one level of which is spelled the way the annex is.
std::string annex_label(const AnnexNames& names, const std::string& annex)
{
    for (const auto& [code, label] : names)
        if (code == annex) return annex + " — " + label;
    return annex;
}

Result<StyleEntry> parse_entry(const Json& j, const AnnexNames& annexes, const ImageFiles& images,
                               const IndexTable& dashes, const IndexTable& hatches,
                               std::size_t index)
{
    const std::string where =
        "Stil kataloğu: " + std::string(kKeyEntries) + "[" + std::to_string(index) + "]";
    if (!j.is_object()) return err(ErrorCode::ParseError, where + ": satır bir nesne olmalı.");

    StyleEntry entry;
    auto id = required_string(j, kKeyId, where);
    if (!id) return id.error();
    entry.id         = std::move(id.value());
    entry.label      = optional_string(j, kKeyLabel);
    entry.source_ref = optional_string(j, kKeyRef);

    if (const Json* v = j.find(kKeyRetired); v != nullptr) {
        if (!v->is_bool())
            return err(ErrorCode::ParseError,
                       where + " '" + entry.id + "': '" + kKeyRetired + "' true/false olmalı.");
        entry.deprecated = v->as_bool();
    }

    // ---- the group path and the tags ----
    //
    // Both come from the package. `ek` names the annex and `bolum` the section
    // path inside it, which together are exactly how MPYY EK-1 is printed; a row
    // that declares neither is filed nowhere and shows up at the tree's root.
    std::string annex;
    if (const Json* v = j.find(kKeyAnnex); v != nullptr && v->is_string()) {
        annex = v->as_string();
        entry.group.push_back(annex_label(annexes, annex));
        entry.tags.push_back(annex);
    }

    if (const Json* v = j.find(kKeySection); v != nullptr) {
        if (v->is_string()) {
            entry.group.push_back(v->as_string());
        } else if (v->is_array()) {
            for (const Json& part : v->as_array())
                if (part.is_string()) entry.group.push_back(part.as_string());
        } else {
            return err(ErrorCode::ParseError, where + " '" + entry.id + "': '" + kKeySection +
                                                  "' metin ya da metin dizisi olmalı.");
        }
    }

    // The package's own grouping key, kept as a TAG rather than as a tree level:
    // it is an identifier for rows that belong together, not a name anybody reads.
    if (const Json* v = j.find(kKeyGroup); v != nullptr && v->is_string())
        entry.tags.push_back(v->as_string());

    // The colour the annex prints its GLYPH in, which may differ from the area's.
    // Kept as the stroke colour, because a glyph is drawn with the stroke.
    if (const Json* glyph = j.find(kKeyGlyphColour); glyph != nullptr && glyph->is_string()) {
        auto rgba = parse_rgba(glyph->as_string());
        if (!rgba) return rgba.error();
        entry.appearance.rgba       = rgba.value();
        entry.appearance.src_colour = Source::Explicit;
    }

    // Carried, never smoothed over. The package flags a row whose gösterim columns
    // it could not read; offering it as certain would be this program asserting
    // something the regulation did not print.
    if (const Json* flag = j.find(kKeyUncertain); flag != nullptr && flag->is_bool())
        entry.uncertain = flag->as_bool();
    if (const Json* why = j.find(kKeyWhy); why != nullptr && why->is_array())
        for (const Json& reason : why->as_array())
            if (reason.is_string()) entry.uncertain_reasons.push_back(reason.as_string());

    // ---- the symbol this row DRAWS, when it declares one ----
    //
    // Every name below is the one the `STİL` command takes, letter for letter.
    // That is deliberate and it is the same rule CLAUDE.md 5.10 puts on the
    // command list: a catalogue that invented its own spelling for `isaretci-
    // cizgi` would be a second vocabulary for one idea, and the day the two
    // disagree a published gösterim silently becomes a different one.
    if (const Json* layers = j.find(kKeyLayers); layers != nullptr) {
        if (!layers->is_array())
            return err(ErrorCode::ParseError,
                       where + " '" + entry.id + "': '" + kKeyLayers + "' bir dizi olmalı.");

        for (const Json& declared : layers->as_array()) {
            const std::string at = where + " '" + entry.id + "' katmanı";
            if (!declared.is_object()) return err(ErrorCode::ParseError, at + " bir nesne olmalı.");

            DeclaredLayer declared_layer;
            SymbolLayer& layer = declared_layer.layer;

            const Json* type = declared.find(kKeyLayerType);
            if (type == nullptr || !type->is_string())
                return err(ErrorCode::ParseError,
                           at + ": zorunlu '" + kKeyLayerType + "' alanı eksik veya metin değil.");

            const auto kind = symbol_layer_type_from_name(type->as_string());
            if (!kind)
                return err(ErrorCode::ValidationFailed,
                           at + ": bilinmeyen tip '" + type->as_string() +
                               "'. Geçerli olanlar: " + symbol_layer_type_names() + ".");
            layer.type = *kind;

            Unit unit = Unit::Paper;
            if (const Json* u = declared.find(kKeyLayerUnit); u != nullptr) {
                if (!u->is_string())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerUnit + "' metin olmalı.");
                const auto parsed = unit_from_name(u->as_string());
                if (!parsed)
                    return err(ErrorCode::ValidationFailed,
                               at + ": bilinmeyen birim '" + u->as_string() +
                                   "'. Geçerli olanlar: " + unit_names() + ".");
                unit = *parsed;
            }

            if (const Json* shape = declared.find(kKeyLayerShape); shape != nullptr) {
                if (!shape->is_string())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerShape + "' metin olmalı.");
                const auto parsed = marker_shape_from_name(shape->as_string());
                if (!parsed)
                    return err(ErrorCode::ValidationFailed,
                               at + ": bilinmeyen şekil '" + shape->as_string() +
                                   "'. Geçerli olanlar: " + marker_shape_names() + ".");
                layer.shape = *parsed;
            }

            if (const Json* place = declared.find(kKeyLayerPlace); place != nullptr) {
                if (!place->is_string())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerPlace + "' metin olmalı.");
                const auto parsed = marker_placement_from_name(place->as_string());
                if (!parsed)
                    return err(ErrorCode::ValidationFailed,
                               at + ": bilinmeyen yerleşim '" + place->as_string() +
                                   "'. Geçerli olanlar: " + marker_placement_names() + ".");
                layer.placement = *parsed;
            }

            const auto measure = [&](const char* key, Measure& out) -> Status {
                const Json* v = declared.find(key);
                if (v == nullptr) return ok();
                if (!v->is_number())
                    return err(ErrorCode::ParseError, at + ": '" + key + "' sayı olmalı.");
                out = Measure{static_cast<std::int32_t>(v->as_double()), unit};
                return ok();
            };
            if (auto st = measure(kKeyLayerSize, layer.size); !st) return st.error();
            if (auto st = measure(kKeyLayerGap, layer.interval); !st) return st.error();
            if (auto st = measure(kKeyLayerGapY, layer.spacing_y); !st) return st.error();
            if (auto st = measure(kKeyLayerOffset, layer.offset); !st) return st.error();

            if (const Json* a = declared.find(kKeyLayerAngle); a != nullptr) {
                if (!a->is_number())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerAngle + "' sayı olmalı.");
                layer.angle_udeg = static_cast<std::int32_t>(a->as_double());
            }
            if (const Json* w = declared.find(kKeyLayerWidth); w != nullptr) {
                if (!w->is_number())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerWidth + "' sayı olmalı.");
                layer.look.width_um  = static_cast<std::int32_t>(w->as_double());
                layer.look.src_width = Source::Explicit;
            }
            if (const Json* t = declared.find(kKeyLayerText); t != nullptr) {
                if (!t->is_string())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerText + "' metin olmalı.");
                layer.text = t->as_string();
            }
            if (const Json* l = declared.find(kKeyLayerLock); l != nullptr) {
                if (!l->is_bool())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerLock + "' evet/hayır olmalı.");
                layer.colour_locked = l->as_bool();
            }

            const auto colour = [&](const char* key, std::uint32_t& out, Source& src) -> Status {
                const Json* v = declared.find(key);
                if (v == nullptr) return ok();
                if (!v->is_string())
                    return err(ErrorCode::ParseError,
                               at + ": '" + key + "' '#AARRGGBB' biçiminde metin olmalı.");
                auto parsed = parse_rgba(v->as_string());
                if (!parsed) return parsed.error();
                out = parsed.value();
                src = Source::Explicit;
                return ok();
            };
            if (auto st = colour(kKeyLayerInk, layer.look.rgba, layer.look.src_colour); !st)
                return st.error();
            if (auto st = colour(kKeyLayerFill, layer.look.fill_rgba, layer.look.src_fill); !st)
                return st.error();

            // The dash pattern, in multiples of the stroke's own width, mark
            // first. Read here and INTERNED by whoever applies the row: a
            // catalogue holds no document and a dash id belongs to one.
            if (const Json* dash = declared.find(kKeyLayerDash); dash != nullptr) {
                if (!dash->is_array())
                    return err(ErrorCode::ParseError,
                               at + ": '" + kKeyLayerDash + "' dizi olmalı.");

                const auto& parts = dash->as_array();
                if (parts.size() > kMaxDashSegments || parts.size() % 2 != 0)
                    return err(ErrorCode::ValidationFailed,
                               at + ": '" + std::string(kKeyLayerDash) +
                                   "' çizgi ve boşluk çiftlerinden oluşur ve en çok " +
                                   std::to_string(kMaxDashSegments) + " parça taşır.");

                declared_layer.dash.count = static_cast<std::uint8_t>(parts.size());
                for (std::size_t k = 0; k < parts.size(); ++k) {
                    if (!parts[k].is_number())
                        return err(ErrorCode::ParseError,
                                   at + ": '" + kKeyLayerDash + "' yalnız sayı taşır.");
                    declared_layer.dash.lengths[k] =
                        static_cast<std::uint16_t>(parts[k].as_double() * 100.0);
                }
            }

            entry.layers.push_back(declared_layer);
        }
    }

    // ---- the pictures this row was published with ----
    if (const Json* pictures = j.find(kKeyImage); pictures != nullptr && pictures->is_object()) {
        entry.image_line   = first_image(*pictures, images, kKeyImageLine);
        entry.image_hatch  = first_image(*pictures, images, kKeyImageHatch);
        entry.image_symbol = first_image(*pictures, images, kKeyImageGlyph);
    }

    const std::string row = where + " '" + entry.id + "'";

    if (const Json* stroke = j.find(kKeyStroke); stroke != nullptr) {
        if (!stroke->is_object())
            return err(ErrorCode::ParseError, row + ": '" + kKeyStroke + "' bir nesne olmalı.");
        if (const Json* colour = stroke->find(kKeyColour); colour != nullptr) {
            if (!colour->is_string())
                return err(ErrorCode::ParseError, row + ": '" + kKeyStroke + "/" + kKeyColour +
                                                      "' '#AARRGGBB' biçiminde metin olmalı.");
            auto rgba = parse_rgba(colour->as_string());
            if (!rgba) return rgba.error();
            entry.appearance.rgba       = rgba.value();
            entry.appearance.src_colour = Source::Explicit;
        }
        auto width =
            bounded_int(*stroke, kKeyWidth, 0, std::numeric_limits<std::int32_t>::max(), -1, row);
        if (!width) return width.error();
        if (width.value() >= 0) {
            entry.appearance.width_um  = static_cast<std::int32_t>(width.value());
            entry.appearance.src_width = Source::Explicit;
        }
        if (const Json* dash = stroke->find(kKeyDash); dash != nullptr) {
            auto resolved = dashes.resolve(*dash, row + " '" + kKeyDash + "'");
            if (!resolved) return resolved.error();
            entry.appearance.dash     = resolved.value();
            entry.appearance.src_dash = Source::Explicit;
        }
    }

    if (const Json* fill = j.find(kKeyFill); fill != nullptr) {
        if (!fill->is_object())
            return err(ErrorCode::ParseError, row + ": '" + kKeyFill + "' bir nesne olmalı.");
        if (const Json* colour = fill->find(kKeyColour); colour != nullptr) {
            if (!colour->is_string())
                return err(ErrorCode::ParseError, row + ": '" + kKeyFill + "/" + kKeyColour +
                                                      "' '#AARRGGBB' biçiminde metin olmalı.");
            auto rgba = parse_rgba(colour->as_string());
            if (!rgba) return rgba.error();
            entry.appearance.fill_rgba = rgba.value();
            entry.appearance.src_fill  = Source::Explicit;
        }
        // A row the annex prints WITHOUT a fill is transparent, not white: white is
        // a colour a plan sheet uses and would hide what is under the area.
        if (const Json* clear = fill->find(kKeyClear);
            clear != nullptr && clear->is_bool() && clear->as_bool()) {
            entry.appearance.fill_rgba = 0;
            entry.appearance.src_fill  = Source::Explicit;
        }

        // A declared percentage multiplies into the fill's alpha. The package
        // states it as a whole percent, which is how the annex writes it.
        if (const Json* percent = fill->find(kKeyOpacityPct);
            percent != nullptr && percent->is_number() && entry.appearance.fill_rgba != 0) {
            const double value = percent->as_double(100.0);
            const auto scaled  = static_cast<std::uint32_t>((entry.appearance.fill_rgba >> 24) *
                                                            (value < 0     ? 0
                                                             : value > 100 ? 100
                                                                           : value) /
                                                            100.0);
            entry.appearance.fill_rgba =
                (entry.appearance.fill_rgba & 0x00FFFFFFu) | (scaled << 24);
        }

        if (const Json* hatch = fill->find(kKeyHatch); hatch != nullptr) {
            auto resolved = hatches.resolve(*hatch, row + " '" + kKeyHatch + "'");
            if (!resolved) return resolved.error();
            entry.appearance.hatch    = resolved.value();
            entry.appearance.src_fill = Source::Explicit;
        }
    }

    auto symbol = bounded_int(j, kKeySymbol, 0, 0xFFFF, 0, row);
    if (!symbol) return symbol.error();
    entry.appearance.symbol = static_cast<std::uint16_t>(symbol.value());

    auto order = bounded_int(j, kKeyOrder, std::numeric_limits<std::int16_t>::min(),
                             std::numeric_limits<std::int16_t>::max(), 0, row);
    if (!order) return order.error();
    entry.appearance.z_order = static_cast<std::int16_t>(order.value());

    auto scale = parse_scale(j, row);
    if (!scale) return scale.error();
    entry.scale = scale.value();

    return entry;
}

Result<StyleRule> parse_rule(const Json& j, std::size_t index)
{
    const std::string where =
        "Stil kataloğu: " + std::string(kKeyRules) + "[" + std::to_string(index) + "]";
    if (!j.is_object()) return err(ErrorCode::ParseError, where + ": kural bir nesne olmalı.");

    StyleRule rule;
    auto id = required_string(j, kKeyId, where);
    if (!id) return id.error();
    rule.id = std::move(id.value());

    const std::string named = where + " '" + rule.id + "'";

    auto entry = required_string(j, kKeyEntryRef, named);
    if (!entry) return entry.error();
    rule.entry = std::move(entry.value());

    if (const Json* conditions = j.find(kKeyConditions); conditions != nullptr) {
        if (!conditions->is_array())
            return err(ErrorCode::ParseError,
                       named + ": '" + kKeyConditions + "' bir dizi olmalı.");
        std::size_t n = 0;
        for (const Json& item : conditions->as_array()) {
            auto condition =
                parse_condition(item, named + " " + kKeyConditions + "[" + std::to_string(n) + "]");
            if (!condition) return condition.error();
            rule.conditions.push_back(std::move(condition.value()));
            ++n;
        }
    }

    auto scale = parse_scale(j, named);
    if (!scale) return scale.error();
    rule.scale = scale.value();

    return rule;
}

} // namespace

Result<StyleCatalog> StyleCatalog::from_json(const Json& j)
{
    if (!j.is_object())
        return err(ErrorCode::ParseError, "Stil kataloğu: kök öğe bir JSON nesnesi olmalı.");

    StyleCatalog catalog;

    // data.md R2: the header block is mandatory and complete. A package that
    // cannot name its regulation, version, publication date and licence is
    // refused here rather than believed downstream (model.md R35).
    const std::string where = "Stil kataloğu";
    for (const auto& [key, slot] : std::initializer_list<std::pair<const char*, std::string*>>{
             {kKeyId, &catalog.id_},
             {kKeyVersion, &catalog.package_version_},
             {kKeySource, &catalog.source_},
             {kKeyPublished, &catalog.published_},
             {kKeyLicence, &catalog.licence_}}) {
        auto value = required_string(j, key, where);
        if (!value) return value.error();
        *slot = std::move(value.value());
    }

    // The annex names, read before the rows so every row can be filed under the
    // name its annex actually has.
    AnnexNames annexes;
    if (const Json* types = j.find(kKeyPlanTypes); types != nullptr && types->is_array()) {
        for (const Json& item : types->as_array()) {
            if (!item.is_object()) continue;
            const Json* code  = item.find(kKeyAnnex);
            const Json* label = item.find(kKeyLabel);
            if (code != nullptr && code->is_string() && label != nullptr && label->is_string())
                annexes.emplace_back(code->as_string(), label->as_string());
        }
    }

    // The picture table, read before the rows so every row can resolve the ids it
    // names. The package lists each picture once with its file, its size and its
    // sha256; only the file is needed here, and the digest is the data package's
    // own integrity check (data.md).
    ImageFiles images;
    if (const Json* table = j.find(kKeyImages); table != nullptr && table->is_array()) {
        for (const Json& item : table->as_array()) {
            if (!item.is_object()) continue;
            const Json* id   = item.find(kKeyId);
            const Json* file = item.find(kKeyImageFile);
            if (id != nullptr && id->is_string() && file != nullptr && file->is_string())
                images.emplace_back(id->as_string(), file->as_string());
        }
    }

    IndexTable dashes;
    if (auto st = dashes.load(j, kKeyDashTable); !st) return st.error();
    IndexTable hatches;
    if (auto st = hatches.load(j, kKeyHatchTable); !st) return st.error();

    if (const Json* entries = j.find(kKeyEntries); entries != nullptr) {
        if (!entries->is_array())
            return err(ErrorCode::ParseError, where + ": '" + kKeyEntries + "' bir dizi olmalı.");
        std::size_t index = 0;
        for (const Json& item : entries->as_array()) {
            auto entry = parse_entry(item, annexes, images, dashes, hatches, index);
            if (!entry) return entry.error();
            for (const auto& existing : catalog.entries_) {
                if (existing.id == entry.value().id)
                    return err(ErrorCode::InvalidArgument,
                               where + ": '" + entry.value().id +
                                   "' satır kimliği iki kez geçiyor. Katalog kimlikleri "
                                   "kalıcıdır ve yeniden kullanılamaz.");
            }
            catalog.entries_.push_back(std::move(entry.value()));
            ++index;
        }
    }

    if (const Json* rules = j.find(kKeyRules); rules != nullptr) {
        if (!rules->is_array())
            return err(ErrorCode::ParseError, where + ": '" + kKeyRules + "' bir dizi olmalı.");
        std::size_t index = 0;
        for (const Json& item : rules->as_array()) {
            auto rule = parse_rule(item, index);
            if (!rule) return rule.error();
            for (const auto& existing : catalog.rules_) {
                if (existing.id == rule.value().id)
                    return err(ErrorCode::InvalidArgument, where + ": '" + rule.value().id +
                                                               "' kural kimliği iki kez geçiyor.");
            }
            // A rule pointing at a row that does not exist would fail only for the
            // feature that finally matched it — possibly years later, in a signed
            // document. It fails at load instead.
            const bool known =
                std::any_of(catalog.entries_.begin(), catalog.entries_.end(),
                            [&](const StyleEntry& e) { return e.id == rule.value().entry; });
            if (!known)
                return err(ErrorCode::NotFound,
                           where + ": '" + rule.value().id + "' kuralı '" + rule.value().entry +
                               "' satırını gösteriyor, ama katalogda böyle bir satır yok.");
            catalog.rules_.push_back(std::move(rule.value()));
            ++index;
        }
    }

    return catalog;
}

Result<const StyleEntry*> StyleCatalog::entry(std::string_view id) const
{
    for (const auto& e : entries_)
        if (e.id == id) return &e;

    return err(ErrorCode::NotFound, "Stil kataloğunda '" + std::string(id) +
                                        "' kimlikli satır yok. Katalog: " + id_ + " " +
                                        package_version_ + ".");
}

Result<const StyleEntry*> StyleCatalog::classify(const FeatureView& feature,
                                                 ScaleDenominator denominator) const
{
    for (const auto& rule : rules_) {
        if (!rule.matches(feature, denominator)) continue;
        // Resolved at load time, so this cannot fail; kept as a lookup rather than
        // a stored pointer so that the catalogue stays copyable and relocatable.
        return entry(rule.entry);
    }

    return err(ErrorCode::NotFound, "Stil kataloğunda bu nesneye uyan kural yok. Katalog: " + id_ +
                                        " " + package_version_ + ", ölçek paydası " +
                                        std::to_string(denominator) + ", sınanan alan sayısı " +
                                        std::to_string(feature.size()) + ".");
}

std::uint64_t StyleCatalog::content_hash() const
{
    std::uint64_t h = fnv1a(id_);
    h               = fnv1a(package_version_, h);
    h               = fnv1a(source_, h);
    h               = fnv1a(published_, h);
    h               = fnv1a(licence_, h);

    for (const auto& e : entries_) {
        h = fnv1a(e.id, h);
        h = fnv1a(e.label, h);
        h = fnv1a(e.source_ref, h);
        h = fnv1a_int(static_cast<std::int64_t>(e.appearance.rgba), h);
        h = fnv1a_int(e.appearance.width_um, h);
        h = fnv1a_int(e.appearance.dash, h);
        h = fnv1a_int(e.appearance.symbol, h);
        for (const std::string& g : e.group)
            h = fnv1a(g, h);
        for (const std::string& t : e.tags)
            h = fnv1a(t, h);
        h = fnv1a(e.image_line, h);
        h = fnv1a(e.image_hatch, h);
        h = fnv1a(e.image_symbol, h);
        // Folded through the one symbol fold there is, so a declared layer and
        // the same layer in a document hash the same way.
        for (const DeclaredLayer& d : e.layers) {
            Symbol one;
            one.layers.push_back(d.layer);
            h = fold_symbol(one, h);
            h = fnv1a(std::string_view(reinterpret_cast<const char*>(d.dash.lengths),
                                       sizeof(d.dash.lengths)),
                      h);
        }
        h = fnv1a_int(e.uncertain ? 1 : 0, h);
        for (const std::string& why : e.uncertain_reasons)
            h = fnv1a(why, h);
        h = fnv1a_int(static_cast<std::int64_t>(e.appearance.fill_rgba), h);
        h = fnv1a_int(e.appearance.hatch, h);
        h = fnv1a_int(e.appearance.z_order, h);
        h = fnv1a_int(e.scale.low, h);
        h = fnv1a_int(e.scale.high, h);
        h = fnv1a_int(e.deprecated ? 1 : 0, h);
    }

    // Rule ORDER decides which row wins, so it is part of the identity of the
    // package: two catalogues with the same rules in a different order are two
    // different catalogues.
    for (const auto& r : rules_) {
        h = fnv1a(r.id, h);
        h = fnv1a(r.entry, h);
        h = fnv1a_int(r.scale.low, h);
        h = fnv1a_int(r.scale.high, h);
        for (const auto& c : r.conditions) {
            h = fnv1a(c.field, h);
            h = fnv1a(style_test_name(c.test), h);
            for (const auto& v : c.values)
                h = fnv1a(v, h);
            h = fnv1a_int(c.has_low ? c.low : 0, h);
            h = fnv1a_int(c.has_high ? c.high : 0, h);
        }
    }
    return h;
}

// ---------------------------------------------------------------- helpers -----

Appearance apply_entry(const StyleEntry& entry, const Appearance& base)
{
    Appearance out = base;

    if (entry.appearance.src_colour == Source::Explicit) {
        out.rgba       = entry.appearance.rgba;
        out.src_colour = Source::Explicit;
    }
    if (entry.appearance.src_width == Source::Explicit) {
        out.width_um  = entry.appearance.width_um;
        out.src_width = Source::Explicit;
    }
    if (entry.appearance.src_dash == Source::Explicit) {
        out.dash     = entry.appearance.dash;
        out.src_dash = Source::Explicit;
    }
    if (entry.appearance.src_fill == Source::Explicit) {
        out.fill_rgba = entry.appearance.fill_rgba;
        out.hatch     = entry.appearance.hatch;
        out.src_fill  = Source::Explicit;
    }

    // Symbol and draw order carry no cascade source of their own; a row that
    // declares neither leaves the base value alone.
    if (entry.appearance.symbol != 0) out.symbol = entry.appearance.symbol;
    if (entry.appearance.z_order != 0) out.z_order = entry.appearance.z_order;

    return out;
}

Result<std::uint32_t> parse_rgba(std::string_view text)
{
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::string_view body = text;
    if (!body.empty() && body.front() == '#') body.remove_prefix(1);

    if (body.size() != 6 && body.size() != 8)
        return err(ErrorCode::ParseError,
                   "Renk '#RRGGBB' veya '#AARRGGBB' biçiminde olmalı. Girilen: '" +
                       std::string(text) + "'");

    std::uint32_t value = 0;
    for (char c : body) {
        const int d = digit(c);
        if (d < 0)
            return err(ErrorCode::ParseError,
                       "Renk yalnız onaltılık basamak içerebilir. Girilen: '" + std::string(text) +
                           "'");
        value = (value << 4) | static_cast<std::uint32_t>(d);
    }

    if (body.size() == 6) value |= 0xFF000000u; // no alpha given = opaque
    return value;
}

} // namespace piricad::core
