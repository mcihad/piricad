// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/feature_classes.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/drawing_catalogs.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

namespace piricad::command {

using core::err;
using core::ErrorCode;

namespace {

core::Result<core::Json> read_package(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return err(ErrorCode::NotFound,
                   "'" + path +
                       "' açılamadı; kalem kataloğu yok ya da okunamıyor. TERCİH "
                       "core.kalem.katalog ile yolunu düzeltin.");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto json = core::Json::parse(text);
    if (!json)
        return err(ErrorCode::ParseError,
                   "'" + path + "' geçerli JSON değil: " + json.error().message);
    if (!json.value().is_object())
        return err(ErrorCode::ParseError, "'" + path + "' bir katalog nesnesi değil.");
    return json;
}

std::string text_of(const core::Json& row, const char* key)
{
    const core::Json* v = row.find(key);
    return v != nullptr && v->is_string() ? v->as_string() : std::string();
}

/// "#RRGGBB" as 0xFFRRGGBB and "#RRGGBBAA" as 0xAARRGGBB, or nothing when it is neither.
std::optional<std::uint32_t> colour_of(const std::string& word)
{
    if ((word.size() != 7 && word.size() != 9) || word[0] != '#') return std::nullopt;
    std::uint32_t rgb = 0;
    for (std::size_t i = 1; i < 7; ++i) {
        const char c        = word[i];
        std::uint32_t digit = 0;
        if (c >= '0' && c <= '9')
            digit = static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        else
            return std::nullopt;
        rgb = (rgb << 4U) | digit;
    }
    std::uint32_t alpha = 0xFFU;
    if (word.size() == 9) {
        alpha = 0;
        for (std::size_t i = 7; i < 9; ++i) {
            const char c        = word[i];
            std::uint32_t digit = 0;
            if (c >= '0' && c <= '9')
                digit = static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                digit = static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                digit = static_cast<std::uint32_t>(c - 'A' + 10);
            else
                return std::nullopt;
            alpha = (alpha << 4U) | digit;
        }
    }
    return (alpha << 24U) | rgb;
}

std::optional<ClassGeometry> geometry_from_word(const std::string& word)
{
    if (core::turkish_key_equals(word, "nokta")) return ClassGeometry::Point;
    if (core::turkish_key_equals(word, "cizgi")) return ClassGeometry::Line;
    if (core::turkish_key_equals(word, "alan")) return ClassGeometry::Area;
    return std::nullopt;
}

/// The spec a field makes, for reading its default the way a column would.
core::AttrSpec spec_of(const ClassField& f)
{
    core::AttrSpec spec;
    spec.id      = f.id;
    spec.name_tr = f.name;
    spec.type    = f.type;
    spec.scale   = f.scale;
    return spec;
}

} // namespace

const char* class_geometry_word(ClassGeometry g) noexcept
{
    switch (g) {
    case ClassGeometry::Point: return "nokta";
    case ClassGeometry::Line: return "cizgi";
    case ClassGeometry::Area: return "alan";
    case ClassGeometry::Other: return "diger";
    }
    return "diger";
}

const char* class_geometry_noun(ClassGeometry g) noexcept
{
    switch (g) {
    case ClassGeometry::Point: return "nokta";
    case ClassGeometry::Line: return "açık çizgi";
    case ClassGeometry::Area: return "kapalı alan";
    case ClassGeometry::Other: return "başka bir nesne";
    }
    return "başka bir nesne";
}

const char* class_rule_word(ClassFinding::Rule rule) noexcept
{
    switch (rule) {
    case ClassFinding::Rule::Geometry: return "geometri";
    case ClassFinding::Rule::TooSmall: return "çok küçük";
    case ClassFinding::Rule::TooShort: return "çok kısa";
    case ClassFinding::Rule::MissingValue: return "eksik değer";
    case ClassFinding::Rule::NotAChoice: return "listede yok";
    case ClassFinding::Rule::MissingColumn: return "eksik sütun";
    }
    return "";
}

const FeatureClass* FeatureClassCatalog::find(std::string_view word) const noexcept
{
    for (const FeatureClass& c : classes) {
        if (core::turkish_key_equals(c.id, word) || core::turkish_key_equals(c.name, word))
            return &c;
        for (const std::string& a : c.aliases)
            if (core::turkish_key_equals(a, word)) return &c;
    }
    return nullptr;
}

std::string reference_package(std::string_view reference)
{
    const std::size_t at    = reference.find('@');
    const std::size_t slash = reference.find('/');
    const std::size_t end   = at != std::string_view::npos ? at : slash;
    return std::string(reference.substr(0, end));
}

std::string reference_class(std::string_view reference)
{
    const std::size_t slash = reference.find('/');
    return slash == std::string_view::npos ? std::string()
                                           : std::string(reference.substr(slash + 1));
}

const FeatureClass* FeatureClassCatalog::of_reference(std::string_view reference) const noexcept
{
    if (reference.empty() || reference_package(reference) != package_id) return nullptr;
    const std::string wanted = reference_class(reference);
    for (const FeatureClass& c : classes)
        if (c.id == wanted) return &c;
    return nullptr;
}

std::string FeatureClassCatalog::reference_to(const FeatureClass& cls) const
{
    return package_id + "@" + package_version + "/" + cls.id;
}

core::Result<FeatureClassCatalog> load_feature_classes(const std::string& path)
{
    auto json = read_package(path);
    if (!json) return json.error();
    const core::Json& root = json.value();

    FeatureClassCatalog out;
    out.package_id      = text_of(root, "id");
    out.package_version = text_of(root, "package_version");
    if (out.package_id.empty() || out.package_version.empty())
        return err(ErrorCode::ParseError,
                   "'" + path +
                       "' paket kimliğini (id) ya da sürümünü (package_version) taşımıyor.");
    if (out.package_id.find_first_of("@/") != std::string::npos)
        return err(ErrorCode::ParseError,
                   "'" + path + "' paket kimliği '@' ya da '/' içeremez: " + out.package_id);

    const core::Json* list = root.find("siniflar");
    if (list == nullptr || !list->is_array() || list->as_array().empty())
        return err(ErrorCode::ParseError, "'" + path + "' `siniflar` dizisi taşımıyor.");

    std::set<std::string> ids;
    std::set<std::string> layers;
    for (const core::Json& row : list->as_array()) {
        FeatureClass c;
        c.id = text_of(row, "id");
        if (c.id.empty())
            return err(ErrorCode::ParseError,
                       "'" + path + "' içinde kimliği olmayan bir sınıf var.");
        const std::string where = "'" + path + "' sınıf '" + c.id + "'";
        if (c.id.find_first_of("@/ ") != std::string::npos)
            return err(ErrorCode::ParseError, where + ": kimlikte '@', '/' ya da boşluk olamaz.");
        if (!ids.insert(c.id).second)
            return err(ErrorCode::ValidationFailed, where + ": aynı kimlik iki kez tanımlı.");

        c.name    = text_of(row, "ad");
        c.summary = text_of(row, "ozet");
        c.source  = text_of(row, "kaynak");
        if (c.name.empty()) return err(ErrorCode::ParseError, where + ": `ad` yok.");
        if (const core::Json* names = row.find("adlar"); names != nullptr && names->is_array())
            for (const core::Json& n : names->as_array())
                if (n.is_string() && !n.as_string().empty()) c.aliases.push_back(n.as_string());

        const auto geometry = geometry_from_word(text_of(row, "geometri"));
        if (!geometry)
            return err(ErrorCode::ParseError,
                       where + ": `geometri` nokta, cizgi ya da alan olmalı.");
        c.geometry = *geometry;

        const core::Json* layer = row.find("katman");
        if (layer == nullptr || !layer->is_object() || text_of(*layer, "ad").empty())
            return err(ErrorCode::ParseError, where + ": `katman.ad` yok.");
        c.layer             = text_of(*layer, "ad");
        c.group             = text_of(*layer, "grup");
        c.layer_description = text_of(*layer, "aciklama");
        if (const std::string colour = text_of(*layer, "renk"); !colour.empty()) {
            const auto rgba = colour_of(colour);
            if (!rgba)
                return err(ErrorCode::ParseError,
                           where + ": renk '" + colour + "' #RRGGBB biçiminde olmalı.");
            c.has_colour  = true;
            c.colour_rgba = *rgba;
        }
        if (const std::string fill = text_of(*layer, "dolgu"); !fill.empty()) {
            const auto rgba = colour_of(fill);
            if (!rgba)
                return err(ErrorCode::ParseError,
                           where + ": dolgu '" + fill + "' #RRGGBB ya da #RRGGBBAA olmalı.");
            c.has_fill  = true;
            c.fill_rgba = *rgba;
        }
        if (const core::Json* w = layer->find("kalinlik_mm"); w != nullptr && w->is_number()) {
            if (w->as_double() < 0.0 || w->as_double() > 20.0)
                return err(ErrorCode::ParseError, where + ": `kalinlik_mm` 0–20 arasında olmalı.");
            c.width_um = static_cast<std::int32_t>(std::lround(w->as_double() * 1000.0));
        }
        if (!layers.insert(core::turkish_upper(c.layer)).second)
            return err(ErrorCode::ValidationFailed,
                       where + ": '" + c.layer + "' katmanını başka bir sınıf da kullanıyor.");

        if (const core::Json* fields = row.find("alanlar");
            fields != nullptr && fields->is_array()) {
            std::set<std::string> field_ids;
            for (const core::Json& f : fields->as_array()) {
                ClassField field;
                field.id = text_of(f, "kimlik");
                if (field.id.empty())
                    return err(ErrorCode::ParseError, where + ": kimliği olmayan bir alan var.");
                const std::string here = where + " alanı '" + field.id + "'";
                if (!field_ids.insert(field.id).second)
                    return err(ErrorCode::ValidationFailed,
                               here + ": aynı kimlik iki kez tanımlı.");
                field.name      = text_of(f, "ad").empty() ? field.id : text_of(f, "ad");
                field.summary   = text_of(f, "ozet");
                const auto type = core::attr_type_from_name(text_of(f, "tur"));
                if (!type)
                    return err(ErrorCode::ParseError,
                               here + ": `tur` '" + text_of(f, "tur") + "' bilinmiyor.");
                field.type = *type;
                if (const core::Json* z = f.find("zorunlu"); z != nullptr)
                    field.required = z->as_bool();
                if (const core::Json* b = f.find("basamak"); b != nullptr && b->is_number())
                    field.scale = static_cast<std::uint8_t>(
                        std::clamp<std::int64_t>(b->as_int(), 0, core::kMaxScale));
                if (const core::Json* o = f.find("secenekler"); o != nullptr && o->is_array())
                    for (const core::Json& choice : o->as_array())
                        if (choice.is_string()) field.choices.push_back(choice.as_string());
                field.default_text = text_of(f, "varsayilan");
                if (!field.default_text.empty()) {
                    auto read = core::attr_parse(spec_of(field), field.default_text);
                    if (!read)
                        return err(ErrorCode::ValidationFailed,
                                   here + ": varsayılan '" + field.default_text +
                                       "' okunamıyor — " + read.error().message);
                    if (!field.choices.empty() &&
                        std::find(field.choices.begin(), field.choices.end(), field.default_text) ==
                            field.choices.end())
                        return err(ErrorCode::ValidationFailed,
                                   here + ": varsayılan '" + field.default_text +
                                       "' kendi seçenekleri arasında değil.");
                }
                c.fields.push_back(std::move(field));
            }
        }

        if (const core::Json* checks = row.find("dogrulama");
            checks != nullptr && checks->is_object()) {
            if (const core::Json* v = checks->find("en_az_alan_m2"); v != nullptr && v->is_number())
                c.min_area_m2 = std::max(0.0, v->as_double());
            if (const core::Json* v = checks->find("en_az_uzunluk_m");
                v != nullptr && v->is_number())
                c.min_length_m = std::max(0.0, v->as_double());
        }
        out.classes.push_back(std::move(c));
    }
    return out;
}

ClassGeometry geometry_class_of(const core::Document& doc, core::EntityId e)
{
    const core::EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return ClassGeometry::Other;
    switch (ents.kind[e]) {
    case core::kPointKind: return ClassGeometry::Point;
    case core::kPolylineKind:
    case core::kArcPolylineKind: {
        if (doc.texts().has(ents.slot[e])) return ClassGeometry::Other;
        const core::RingSpan rs = doc.geometry().rings_of(ents.slot[e]);
        if (rs.count == 0) return ClassGeometry::Other;
        return doc.geometry().ring_role[rs.first] == core::RingRole::Open ? ClassGeometry::Line
                                                                          : ClassGeometry::Area;
    }
    case core::kHatchKind:
    case core::kCircleKind:
    case core::kEllipseKind: return ClassGeometry::Area;
    case core::kArcKind:
    case core::kSplineKind: return ClassGeometry::Line;
    default: return ClassGeometry::Other;
    }
}

core::Result<ClassSettle> settle_feature_classes(Transaction& tx, std::size_t mark,
                                                 const FeatureClassCatalog& catalog)
{
    ClassSettle done;
    core::Document& doc = tx.document();

    // NOTHING FOLLOWS A CLASS: the usual case, and it costs one pass over the layer list.
    bool any = false;
    for (core::LayerId l = 0; l < doc.layers().size(); ++l)
        if (const core::Layer* layer = doc.layer(l);
            layer != nullptr && !layer->feature_class.empty()) {
            any = true;
            break;
        }
    if (!any) return done;

    // WHAT THIS COMMAND MADE OR MOVED: an object that came alive, and one that changed layer.
    std::vector<core::EntityId> arrived;
    for (const core::Op& op : tx.ops_since(mark)) {
        const bool created = op.kind == core::Op::Kind::SetEntityAlive && !op.bool_arg;
        const bool moved   = op.kind == core::Op::Kind::SetEntityLayer;
        if ((created || moved) && op.entity != core::kNoEntity) arrived.push_back(op.entity);
    }
    std::ranges::sort(arrived);
    arrived.erase(std::ranges::unique(arrived).begin(), arrived.end());

    for (const core::EntityId e : arrived) {
        if (!doc.alive(e)) continue;
        const core::Layer* layer = doc.layer(doc.entities().layer[e]);
        if (layer == nullptr || layer->feature_class.empty()) continue;
        const FeatureClass* cls = catalog.of_reference(layer->feature_class);
        if (cls == nullptr) continue; // package not here: the objects stay, nothing is checked

        // THE GEOMETRY IS THE CLASS'S, or the whole command is refused: an area class holding a
        // loose line is not a thing the digitiser meant to make.
        ++done.checked;
        const ClassGeometry got = geometry_class_of(doc, e);
        if (got != cls->geometry)
            return err(ErrorCode::ValidationFailed,
                       "'" + cls->name + "' sınıfı " + class_geometry_noun(cls->geometry) +
                           " ister (katman '" + layer->name + "'); çizilen nesne " +
                           class_geometry_noun(got) +
                           ". Şekli sınıfa uydurun ya da başka bir "
                           "sınıfı seçin (KALEM).");

        // THE DEFAULTS, for the cells nobody has filled.
        for (const ClassField& f : cls->fields) {
            if (f.default_text.empty()) continue;
            const core::AttrId col = doc.attributes().find(f.id);
            if (col == core::kNoAttr) continue; // the column is gone: KALEMDENETİM says so
            const auto had = doc.attribute(col, e);
            if (had && had.value().present) continue;
            const core::AttrColumn* column = doc.attributes().column(col);
            auto value                     = core::attr_parse(column->spec(), f.default_text);
            if (!value)
                return err(ErrorCode::ValidationFailed,
                           "'" + cls->name + "' sınıfının '" + f.id +
                               "' varsayılanı yazılamadı: " + value.error().message);
            if (auto st = tx.set_attribute(col, e, value.value()); !st) return st.error();
            ++done.filled;
        }
    }
    return done;
}

std::vector<ClassFinding> check_feature_class(const core::Document& doc, core::LayerId layer,
                                              const FeatureClass& cls)
{
    std::vector<ClassFinding> found;

    // A COLUMN THE CLASS NEEDS AND THE DRAWING LACKS is one finding for the class, not one per
    // object.
    for (const ClassField& f : cls.fields)
        if (doc.attributes().find(f.id) == core::kNoAttr) {
            ClassFinding finding;
            finding.rule  = ClassFinding::Rule::MissingColumn;
            finding.field = f.id;
            finding.detail =
                "'" + f.id + "' sütunu belgede yok; KALEM '" + cls.name + "' onu yeniden tanımlar.";
            found.push_back(std::move(finding));
        }

    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().layer[e] != layer) continue;
        const core::EntityKey key = doc.key_of(e);
        const auto note = [&](ClassFinding::Rule rule, std::string field, std::string detail) {
            ClassFinding finding;
            finding.rule   = rule;
            finding.object = key;
            finding.field  = std::move(field);
            finding.detail = std::move(detail);
            found.push_back(std::move(finding));
        };

        const ClassGeometry got = geometry_class_of(doc, e);
        if (got != cls.geometry) {
            note(ClassFinding::Rule::Geometry, {},
                 std::string("sınıf ") + class_geometry_noun(cls.geometry) + " ister, nesne " +
                     class_geometry_noun(got));
            continue; // a size on the wrong kind of thing is noise
        }
        if (cls.geometry == ClassGeometry::Area && cls.min_area_m2 > 0.0) {
            const double m2 = static_cast<double>(doc.entity_area(e)) / 1.0e6;
            if (m2 < cls.min_area_m2) {
                char text[96];
                (void)std::snprintf(text, sizeof text, "alan %.2f m², en az %.2f m² olmalı", m2,
                                    cls.min_area_m2);
                note(ClassFinding::Rule::TooSmall, {}, text);
            }
        }
        if (cls.geometry == ClassGeometry::Line && cls.min_length_m > 0.0) {
            const double m = static_cast<double>(doc.entity_perimeter(e)) / 1.0e3;
            if (m < cls.min_length_m) {
                char text[96];
                (void)std::snprintf(text, sizeof text, "uzunluk %.2f m, en az %.2f m olmalı", m,
                                    cls.min_length_m);
                note(ClassFinding::Rule::TooShort, {}, text);
            }
        }
        for (const ClassField& f : cls.fields) {
            const core::AttrId col = doc.attributes().find(f.id);
            if (col == core::kNoAttr) continue;
            const auto cell = doc.attribute(col, e);
            const bool set  = cell && cell.value().present;
            if (!set) {
                if (f.required) note(ClassFinding::Rule::MissingValue, f.id, "'" + f.id + "' boş");
                continue;
            }
            if (f.choices.empty()) continue;
            const std::string shown = core::attr_display(cell.value(), core::DecimalMark::Point);
            if (std::find(f.choices.begin(), f.choices.end(), shown) == f.choices.end())
                note(ClassFinding::Rule::NotAChoice, f.id,
                     "'" + f.id + "' = '" + shown + "', izin verilenler arasında değil");
        }
    }
    return found;
}

std::shared_ptr<const FeatureClassCatalog> Bus::feature_classes()
{
    namespace fs = std::filesystem;
    const std::string configured(app_settings_.get("core.kalem.katalog").as_text());
    const std::string path = resolve_catalog_path(configured);
    if (path.empty()) {
        classes_.reset();
        classes_path_.clear();
        classes_error_ = "Kalem kataloğu bulunamadı: '" + configured +
                         "'. Kurulumda eksikse PIRICAD_DATA ile dizini gösterin.";
        return nullptr;
    }

    // READ AGAIN WHEN THE FILE CHANGED, and only then: this runs after every command, and the
    // answer to "did it change" is two numbers from the file system.
    std::error_code ec;
    const auto size           = fs::file_size(path, ec);
    const auto time           = fs::last_write_time(path, ec);
    const std::uint64_t stamp = static_cast<std::uint64_t>(size) * 1000003ULL +
                                static_cast<std::uint64_t>(time.time_since_epoch().count());
    if (classes_ != nullptr && path == classes_path_ && stamp == classes_stamp_) return classes_;
    if (classes_ == nullptr && path == classes_path_ && stamp == classes_stamp_ &&
        !classes_error_.empty())
        return nullptr; // the same unreadable file: say it once, not after every command

    auto loaded    = load_feature_classes(path);
    classes_path_  = path;
    classes_stamp_ = stamp;
    if (!loaded) {
        classes_.reset();
        classes_error_ = loaded.error().message;
        return nullptr;
    }
    classes_error_.clear();
    classes_ = std::make_shared<const FeatureClassCatalog>(std::move(loaded.value()));
    return classes_;
}

} // namespace piricad::command
