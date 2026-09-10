// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/drawing_catalogs.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace kentos::command {

using core::err;
using core::ErrorCode;

namespace {

core::Result<core::Json> read_json(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return err(ErrorCode::NotFound, "'" + path +
                                            "' açılamadı; katalog dosyası yok ya da "
                                            "okunamıyor. TERCİH ile yolunu düzeltin.");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto json = core::Json::parse(text);
    if (!json)
        return err(ErrorCode::ParseError,
                   "'" + path + "' geçerli JSON değil: " + json.error().message);
    if (!json.value().is_object())
        return err(ErrorCode::ParseError, "'" + path + "' bir katalog nesnesi değil.");
    return json;
}

std::string version_of(const core::Json& root)
{
    const core::Json* v = root.find("package_version");
    return v != nullptr && v->is_string() ? v->as_string() : std::string();
}

std::int64_t int_at(const core::Json& arr, std::size_t i)
{
    if (!arr.is_array() || arr.as_array().size() <= i) return 0;
    return arr.as_array()[i].as_int();
}

} // namespace

const HatchPattern* HatchPatternCatalog::find(std::string_view id) const noexcept
{
    for (const HatchPattern& p : patterns)
        if (core::turkish_key_equals(p.id, id)) return &p;
    return nullptr;
}

const DimensionStyle* DimensionStyleCatalog::find(std::string_view id) const noexcept
{
    for (const DimensionStyle& s : styles)
        if (core::turkish_key_equals(s.id, id)) return &s;
    return nullptr;
}

std::string resolve_catalog_path(std::string_view configured)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string given(configured);
    if (given.empty()) return {};
    if (fs::exists(given, ec)) return given;

    // The manual prints `data/catalogs/...`; the shipped tree is `$KENTOS_DATA`
    // or the source tree's `data`, and either holds the part after `data/`.
    const std::string tail = given.rfind("data/", 0) == 0 ? given.substr(5) : given;
    if (const char* env = std::getenv("KENTOS_DATA"); env != nullptr && *env != '\0') {
        const fs::path candidate = fs::path(env) / tail;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
#ifdef KENTOS_SOURCE_DATA_DIR
    {
        const fs::path candidate = fs::path(KENTOS_SOURCE_DATA_DIR) / tail;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
#endif
    return {};
}

core::Result<HatchPatternCatalog> load_hatch_patterns(const std::string& path)
{
    auto json = read_json(path);
    if (!json) return json.error();
    const core::Json& root = json.value();
    HatchPatternCatalog out;
    out.package_version    = version_of(root);
    const core::Json* list = root.find("desenler");
    if (list == nullptr || !list->is_array())
        return err(ErrorCode::ParseError, "'" + path + "' `desenler` dizisi taşımıyor.");
    for (const core::Json& row : list->as_array()) {
        const core::Json* id = row.find("id");
        if (id == nullptr || !id->is_string() || id->as_string().empty())
            return err(ErrorCode::ParseError, "'" + path + "' içinde adı olmayan bir desen var.");
        HatchPattern p;
        p.id = id->as_string();
        if (const core::Json* d = row.find("aciklama"); d != nullptr && d->is_string())
            p.description = d->as_string();
        if (const core::Json* fams = row.find("aileler"); fams != nullptr && fams->is_array()) {
            for (const core::Json& f : fams->as_array()) {
                core::HatchDef::Family family;
                if (const core::Json* a = f.find("aci_uderece"); a != nullptr)
                    family.angle_udeg = a->as_int();
                if (const core::Json* b = f.find("taban_um"); b != nullptr) {
                    family.base_x_um = int_at(*b, 0);
                    family.base_y_um = int_at(*b, 1);
                }
                if (const core::Json* o = f.find("kayma_um"); o != nullptr) {
                    family.offset_x_um = int_at(*o, 0);
                    family.offset_y_um = int_at(*o, 1);
                }
                if (const core::Json* k = f.find("kesik_um"); k != nullptr && k->is_array())
                    for (const core::Json& d : k->as_array())
                        family.dashes_um.push_back(d.as_int());
                p.families.push_back(std::move(family));
            }
        }
        if (out.find(p.id) != nullptr)
            return err(ErrorCode::ValidationFailed,
                       "'" + path + "' aynı adı iki kez tanımlıyor: " + p.id);
        out.patterns.push_back(std::move(p));
    }
    return out;
}

core::Result<DimensionStyleCatalog> load_dimension_styles(const std::string& path)
{
    auto json = read_json(path);
    if (!json) return json.error();
    const core::Json& root = json.value();
    DimensionStyleCatalog out;
    out.package_version    = version_of(root);
    const core::Json* list = root.find("stiller");
    if (list == nullptr || !list->is_array() || list->as_array().empty())
        return err(ErrorCode::ParseError, "'" + path + "' `stiller` dizisi taşımıyor.");
    for (const core::Json& row : list->as_array()) {
        const core::Json* id = row.find("id");
        if (id == nullptr || !id->is_string() || id->as_string().empty())
            return err(ErrorCode::ParseError, "'" + path + "' içinde adı olmayan bir stil var.");
        DimensionStyle s;
        s.id = id->as_string();
        if (const core::Json* d = row.find("aciklama"); d != nullptr && d->is_string())
            s.description = d->as_string();
        if (const core::Json* a = row.find("ok"); a != nullptr && a->is_string()) {
            const std::string& word = a->as_string();
            if (core::turkish_key_equals(word, "acik"))
                s.arrow = core::ArrowStyle::Open;
            else if (core::turkish_key_equals(word, "centik"))
                s.arrow = core::ArrowStyle::Tick;
            else
                s.arrow = core::ArrowStyle::Closed;
        }
        const auto um = [&row](const char* key, std::int32_t fallback) {
            const core::Json* v = row.find(key);
            if (v == nullptr || !v->is_number()) return fallback;
            const std::int64_t n = v->as_int();
            return n < 0 ? fallback : static_cast<std::int32_t>(n);
        };
        s.arrow_um            = um("ok_boyu_um", s.arrow_um);
        s.extension_beyond_um = um("uzatma_fazlasi_um", s.extension_beyond_um);
        s.extension_offset_um = um("uzatma_boslugu_um", s.extension_offset_um);
        s.text_gap_um         = um("yazi_boslugu_um", s.text_gap_um);
        s.text_height_um      = um("yazi_yuksekligi_um", s.text_height_um);
        if (const core::Json* p = row.find("ondalik"); p != nullptr && p->is_number())
            s.precision = static_cast<std::uint8_t>(
                std::min<std::int64_t>(8, std::max<std::int64_t>(0, p->as_int())));
        if (const core::Json* sep = row.find("ondalik_ayraci"); sep != nullptr && sep->is_string())
            s.decimal_separator = sep->as_string() == "." ? '.' : ',';
        if (out.find(s.id) != nullptr)
            return err(ErrorCode::ValidationFailed,
                       "'" + path + "' aynı adı iki kez tanımlıyor: " + s.id);
        out.styles.push_back(std::move(s));
    }
    return out;
}

core::Symbol hatch_symbol(const core::HatchDef& def, std::uint32_t ink_rgba)
{
    core::Symbol sym;

    // The boundary: one plain stroke in the ink, so the hatch has an edge the
    // way a DXF HATCH is drawn with its loops.
    core::SymbolLayer edge;
    edge.look.rgba       = ink_rgba;
    edge.look.src_colour = core::Source::Explicit;
    edge.type            = core::SymbolLayerType::SimpleLine;
    sym.layers.push_back(edge);

    // A pattern the catalogue does not know draws as its boundary alone: the
    // name and the loops are kept, and the reader said so.
    if (!def.solid && def.families.empty()) return sym;

    if (def.solid) {
        core::SymbolLayer fill;
        fill.type            = core::SymbolLayerType::SimpleFill;
        fill.look.rgba       = ink_rgba;
        fill.look.fill_rgba  = ink_rgba;
        fill.look.src_fill   = core::Source::Explicit;
        fill.look.src_colour = core::Source::Explicit;
        sym.layers.push_back(fill);
        return sym;
    }

    const auto add_family = [&](const core::HatchDef::Family& f, std::int64_t extra_udeg) {
        const core::Mm spacing = core::hatch_family_spacing_mm(def, f);
        if (spacing <= 0) return;
        core::SymbolLayer lines;
        lines.type            = core::SymbolLayerType::LinePatternFill;
        lines.look.rgba       = ink_rgba;
        lines.look.src_colour = core::Source::Explicit;
        lines.look.fill_rgba  = ink_rgba;
        lines.look.src_fill   = core::Source::Explicit;
        std::int64_t angle = (f.angle_udeg + def.angle_udeg + extra_udeg) % core::kUDegFullCircle;
        if (angle < 0) angle += core::kUDegFullCircle;
        lines.angle_udeg = static_cast<std::int32_t>(angle);
        lines.interval   = core::Measure{
            static_cast<std::int32_t>(std::min<core::Mm>(spacing, 2000000000)), core::Unit::Ground};
        sym.layers.push_back(lines);
    };
    for (const core::HatchDef::Family& f : def.families) {
        add_family(f, 0);
        if (def.double_lines) add_family(f, core::kUDegFullCircle / 4);
    }
    return sym;
}

} // namespace kentos::command
