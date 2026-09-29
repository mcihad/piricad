// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: a Netcad NCZ drawing, read into the document.
//
// Copyright (C) 2026 Erdinç Örsan ÜNAL
//     The reading of an NCZ this file follows: `ncz_reader.py` and `ncz_pure.py`
//     of his QGIS plugin "NCZ Reader", version 1.4.3,
//     https://github.com/erdincunal/Jeomatik-NCZ-Reader — licensed GPL-2.0-or-later.
// Copyright (C) 2026 KentOSCad contributors
//     This reader, 28 September 2026 (GPLv3 §5a: a modified version).
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. See /NOTICE for the attribution and the trademark note.
//
// The parser (ncz_format.cpp) hands over the plugin's entities; this file turns
// each into what this program has, through the transaction (Article 5.9):
//
//   Point, Symbol, Block        a point — a Point's name into `nokta_no`
//   Line, Polyline              a polyline
//   Polygon, MapSheet,          an area: the closed run, the closing vertex
//   SmartObject, Triangle       implied (model.md R10)
//   Circle                      a circle, centre and radius — not 72 chords
//   Arc                         an arc, its ends from the plugin's own reading
//                               of the angles — not 48 chords
//   Text                        a caption on its baseline, height and rotation
//
// The plugin's seventeen attribute fields are offered as columns (`alanlar=`),
// a field written on an entity only where the parser read it for that kind.
#include "kentos_cad/io/ncz.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include "dxf_common.hpp"
#include "mapped_file.hpp"
#include "ncz_format.hpp"
#include "ncz_symbols.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace kentos::io {
namespace {

using core::ErrorCode;
using core::Mm;
using core::Point2;

/// The reference's validity bound on a coordinate, in metres: ±100 000 km.
constexpr double kWorld = 100000000.0;

/// The decimals a numeric field keeps as a column: a micrometre of a length,
/// a millionth of a degree.
constexpr std::uint8_t kDecimals = 6;

/// One of the plugin's attribute fields: its id — the plugin's field name, so a
/// user comparing with the plugin's layers reads the same word — the Turkish
/// label a column shows, its type, and the parser field it needs. A zero `bit`
/// is a field every entity carries.
struct FieldDef
{
    const char* id;
    const char* label;
    core::AttrType type;
    std::uint16_t bit;
};

/// `NCZReaderPlugin.FIELD_DEFINITIONS`, in its order.
constexpr std::array<FieldDef, 17> kFields{{
    {"source_file", "kaynak dosya", core::AttrType::Text, 0},
    {"layer_code", "katman kodu", core::AttrType::Int64, 0},
    {"layer_name", "katman adı", core::AttrType::Text, 0},
    {"entity_type", "nesne türü", core::AttrType::Text, 0},
    {"name", "ad", core::AttrType::Text, ncz::kName},
    {"label", "etiket", core::AttrType::Text, ncz::kLabel},
    {"color_argb", "renk (ARGB)", core::AttrType::Text, 0},
    {"radius", "yarıçap", core::AttrType::Decimal, ncz::kRadius},
    {"start_ang", "başlangıç açısı", core::AttrType::Decimal, ncz::kStartAngle},
    {"end_ang", "bitiş açısı", core::AttrType::Decimal, ncz::kEndAngle},
    {"text_h", "yazı yüksekliği", core::AttrType::Decimal, ncz::kTextHeight},
    {"rotation", "dönüklük", core::AttrType::Decimal, ncz::kRotation},
    {"box_width", "kutu genişliği", core::AttrType::Decimal, ncz::kBoxWidth},
    {"box_height", "kutu yüksekliği", core::AttrType::Decimal, ncz::kBoxHeight},
    {"scale", "ölçek", core::AttrType::Decimal, ncz::kScale},
    {"grid_x", "ızgara x", core::AttrType::Decimal, ncz::kGridX},
    {"grid_y", "ızgara y", core::AttrType::Decimal, ncz::kGridY},
}};

/// The value of field `k` on `e`, when `e` carries it: text for a text field,
/// a double for a numeric one. `source` is the file's stem.
std::optional<std::string> text_value(std::size_t k, const ncz::Entity& e,
                                      const std::string& source)
{
    switch (k) {
    case 0: return source;
    case 2:
        if (e.layer_name.empty()) return std::nullopt;
        return e.layer_name;
    case 3: return std::string(ncz::kind_name(e.kind));
    case 4:
        if ((e.set & ncz::kName) == 0 || e.name.empty()) return std::nullopt;
        return e.name;
    case 5:
        if ((e.set & ncz::kLabel) == 0 || e.label.empty()) return std::nullopt;
        return e.label;
    case 6:
        // `str(entity.color_argb)`: the unsigned ARGB word in decimal.
        if (!e.color) return std::nullopt;
        return std::to_string(*e.color);
    default: return std::nullopt;
    }
}

std::optional<double> number_value(std::size_t k, const ncz::Entity& e) noexcept
{
    if ((e.set & kFields[k].bit) == 0) return std::nullopt;
    switch (k) {
    case 7: return e.radius;
    case 8: return e.start_angle;
    case 9: return e.end_angle;
    case 10: return e.text_height;
    case 11: return e.rotation;
    case 12: return e.box_width;
    case 13: return e.box_height;
    case 14: return e.scale;
    case 15: return e.grid_x;
    case 16: return e.grid_y;
    default: return std::nullopt;
    }
}

/// A sample for the wizard: the shortest decimal that reads back as `v`.
std::string sample_of(double v)
{
    std::array<char, 64> buf{};
    const auto wrote = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (wrote.ec != std::errc{}) return {};
    std::string out(buf.data(), wrote.ptr);
    for (char& ch : out)
        if (ch == '.') ch = ',';
    return out;
}

/// What the MPROJ bytes say, in Turkish.
std::string declared_of(const ncz::Header& h)
{
    std::string out;
    if (h.mproj) {
        const char* datum = h.datum == 0     ? "WGS-84"
                            : h.datum == 1   ? "ITRF"
                            : h.datum == 4   ? "ED50"
                            : h.datum == 254 ? "ED50 (HGK)"
                                             : "tanımsız datum";
        out += datum;
        if (h.projection == 1) {
            out += ", coğrafi (enlem, boylam)";
        } else if (h.projection == 3) {
            out += ", 3° dilim, orta meridyen " + std::to_string(h.zone) + "°";
        } else if (h.projection == 2) {
            out += ", 6° dilim, dilim bilgisi " + std::to_string(h.zone);
        } else {
            out += ", tanımsız projeksiyon";
        }
    }
    if (!h.epsg.empty()) {
        if (!out.empty()) out += " ";
        out += "(TILED_XML: " + h.epsg + ")";
    }
    return out;
}

/// The geometry type byte's reader, for a record the reference drops or does
/// not know: its entity kind, or the type number.
std::string type_name(std::size_t type)
{
    switch (type) {
    case 1: return "Point";
    case 2: return "Line";
    case 3: return "Circle";
    case 4: return "Arc";
    case 5: return "Text";
    case 6: return "Symbol";
    case 7:
    case 9: return "Polyline";
    case 10: return "Polygon";
    case 11: return "MapSheet";
    case 12: return "Triangle";
    case 13: return "Block";
    case 15: return "SmartObject";
    default: return "NCZ türü " + std::to_string(type);
    }
}

/// Each NCZ entity, into the transaction.
class Mapper final : public ncz::Sink
{
public:
    Mapper(command::Transaction& tx, const ImportOptions& options, NczReport& report,
           std::string source, std::stop_token stop)
        : tx_(tx), options_(options), report_(report), diag_(report.diagnostics),
          source_(std::move(source)), stop_(std::move(stop))
    {
        everything_       = options_.fields.size() == 1 && options_.fields.front() == "*";
        diag_.unit        = core::DrawingUnit::Metre;
        diag_.unit_source = UnitSource::Crs;
    }

    bool begin(const ncz::Header& final_header) override
    {
        final_ = final_header;
        return true;
    }

    bool entity(const ncz::Entity& e) override
    {
        if ((++seen_ % 4096) == 0 && stop_.stop_requested()) {
            cancelled_ = true;
            return false;
        }
        const auto kind = static_cast<std::size_t>(e.kind);
        sample(e);

        const std::string name = layer_of(e);
        census_.try_emplace(name, 0);
        if (!wanted(name)) return true;
        const Layer* layer = layer_for(name, e.layer_code);
        if (layer == nullptr) return false;

        auto made = place(e, layer->slot);
        if (!made) {
            ++skipped_[kind];
            ++diag_.skipped;
            if (diag_.skipped_reason.empty())
                diag_.skipped_reason =
                    std::string(ncz::kind_name(e.kind)) + ": " + made.error().message;
            return true;
        }
        const command::EntityId id = made.value();

        // ITS OWN COLOUR AND ITS OWN PEN: a colour only where it is not the
        // layer's, a width only where the file gives one.
        const bool own_colour       = e.color && (!layer->colour || *e.color != *layer->colour);
        const std::int32_t width_um = pen_width_um(e.line_width);
        if (width_um > 0) {
            ++widths_;
            widest_um_   = std::max(widest_um_, width_um);
            thinnest_um_ = thinnest_um_ == 0 ? width_um : std::min(thinnest_um_, width_um);
        }
        if (own_colour || width_um > 0)
            if (!style(id, own_colour ? e.color : std::nullopt, width_um)) return false;
        if (e.kind == ncz::Kind::Point && !e.name.empty())
            if (!point_number(id, e.name)) return false;
        if (e.smart != ncz::SmartClass::None)
            if (!planet_properties(e, id)) return false;
        if (!stamp(e, id)) return false;

        ++report_.entities;
        ++census_[name];
        ++read_[kind];
        if (!e.coords.empty())
            places_.push_back(Place{e.coords.front().x, e.coords.front().y, layer});
        return true;
    }

    bool cancelled() const noexcept { return cancelled_; }

    const std::optional<core::Error>& failure() const noexcept { return failure_; }

    /// Everything said once the file has been read: the census, the losses, the
    /// system the file declares. An error when the file turns out to hold
    /// degrees, or nothing.
    core::Status finish(const ncz::Header& h, const std::vector<ncz::AttributeTable>& tables,
                        const std::string& path)
    {
        // What the file calls its version, when it reads as one: Netcad writes a
        // build number (`5.2.0.1035N`, `8.5.6.1095`), and the plugin takes the
        // first block that CLAIMS the version's type, so a file with none — or a
        // damaged one — hands over whatever bytes follow. A transcript is not the
        // place for them: anything but a digit first and digits, letters and dots
        // after it is no version, and the report says none.
        const std::string& v   = h.version_name;
        const bool versionlike = !v.empty() && v.size() <= 32 && v.front() >= '0' &&
                                 v.front() <= '9' && std::all_of(v.begin(), v.end(), [](char ch) {
                                     return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                                            (ch >= 'a' && ch <= 'z') || ch == '.';
                                 });
        if (versionlike) report_.version = v;
        report_.declared = declared_of(h);
        report_.layer_names.assign(census_.begin(), census_.end());

        // THE SYSTEM, FIRST: the line a user most needs to see.
        if (auto st = say_system(h); !st) return st;

        for (std::size_t k = 0; k < ncz::kKindCount; ++k)
            if (read_[k] != 0 || skipped_[k] != 0 || degraded_[k] != 0)
                diag_.tally(ncz::kind_name(static_cast<ncz::Kind>(k)), read_[k], skipped_[k],
                            degraded_[k]);

        // WHAT THE PLUGIN LETS FALL IN SILENCE, counted and named (io.md P11).
        std::uint64_t dropped = 0;
        std::uint64_t unknown = 0;
        std::string unknown_types;
        for (std::size_t t = 0; t < h.dropped.size(); ++t) {
            if (h.dropped[t] != 0) {
                diag_.tally(type_name(t), 0, h.dropped[t], 0);
                dropped += h.dropped[t];
            }
            if (h.unsupported[t] != 0) {
                diag_.tally(type_name(t), 0, h.unsupported[t], 0);
                unknown += h.unsupported[t];
                if (!unknown_types.empty()) unknown_types += ", ";
                unknown_types += std::to_string(t);
            }
        }
        if (dropped != 0)
            diag_.note(Severity::Skipped,
                       std::to_string(dropped) +
                           " kayıt okunamadı: türü için kısa, koordinatı ±100 000 km dışında, "
                           "metni ya da yüksekliği olmayan, alanı sıfır ya da tek noktalı.");
        if (unknown != 0)
            diag_.note(Severity::Skipped, std::to_string(unknown) +
                                              " kayıt bu okuyucunun tanımadığı NCZ geometri "
                                              "türlerinde (" +
                                              unknown_types + "); okunmadı.");
        if (h.swept_entities != 0)
            diag_.note(Severity::Info,
                       std::to_string(h.swept_entities) +
                           " nesne, eski NCZ okuyucularının atladığı " +
                           std::to_string(h.swept_blocks) +
                           " bölümden okundu (Netcad 8 düzeni: ayarlar geometrinin arasında).");
        if (!planet_.empty()) {
            std::uint64_t all = 0;
            std::string which;
            for (const auto& [cls, n] : planet_) {
                all += n;
                if (!which.empty()) which += ", ";
                which += std::string(ncz::smart_class_name(cls)) + " " + std::to_string(n);
            }
            diag_.note(Severity::Info,
                       std::to_string(all) + " Netcad akıllı nesnesi (" + which +
                           ") sembol olarak çizildi: " + std::to_string(blocks_.size()) +
                           " farklı sembol blok olarak tanımlandı, her nesne bir blok başvurusu. "
                           "Değerleri (nizam, kat, TAKS, KAKS, genişlik…) nesnenin sütunlarında.");
        }
        if (!planet_undrawn_.empty()) {
            std::uint64_t all = 0;
            std::string which;
            for (const auto& [cls, n] : planet_undrawn_) {
                all += n;
                if (!which.empty()) which += ", ";
                which += std::string(ncz::smart_class_name(cls)) + " " + std::to_string(n);
            }
            diag_.note(Severity::Degraded,
                       std::to_string(all) + " akıllı nesnenin (" + which +
                           ") sembolü çizilemedi: sınıfı bu okuyucunun tanımadığı bir sınıf ya "
                           "da gösterecek değeri yok. Yerinde nokta olarak okundu, değerleri "
                           "sütunlarında.");
        }
        if (planet_unsized_ != 0)
            diag_.note(Severity::Degraded, std::to_string(planet_unsized_) +
                                               " akıllı nesnenin boyutu okunamadı; Netcad'in "
                                               "varsayılanı olan 1 ile çizildi.");
        if (h.point_smart_objects != 0)
            diag_.note(Severity::Info, std::to_string(h.point_smart_objects) +
                                           " akıllı nesnenin okunabilir bir dikdörtgeni yok; plan "
                                           "notasyonu olarak yerinde nokta okundu, etiketi `label` "
                                           "alanında.");
        if (widths_ != 0) {
            const auto mm = [](std::int32_t um) {
                std::string out         = std::to_string(um / 1000) + ",";
                const std::int32_t rest = (um % 1000) / 10;
                if (rest < 10) out += "0";
                out += std::to_string(rest);
                return out;
            };
            diag_.note(Severity::Info, std::to_string(widths_) +
                                           " nesnenin çizgi kalınlığı okundu (" + mm(thinnest_um_) +
                                           "–" + mm(widest_um_) +
                                           " mm); ekranda görmek için durum çubuğunda KALINLIK "
                                           "açık olmalı.");
        }
        if (h.smart_marks != 0)
            diag_.note(Severity::Info, std::to_string(h.smart_marks) +
                                           " ızgara işareti (katman 0'daki S0 sembolü) akıllı "
                                           "nesnenin kendisi çizdiği için ayrıca okunmadı.");
        if (full_turns_ != 0)
            diag_.note(Severity::Degraded, std::to_string(full_turns_) +
                                               " yay tam turdan geniş olduğu için daire olarak "
                                               "okundu.");
        if (flat_rotation_ != 0)
            diag_.note(Severity::Degraded, std::to_string(flat_rotation_) +
                                               " yazının dönüklüğü sayı değildi; yatay yazıldı.");
        if (!renamed_.empty())
            diag_.note(Severity::Degraded, std::to_string(renamed_.size()) +
                                               " katman adındaki denetim karakterleri atıldı.");
        if (unwritable_cells_ != 0)
            diag_.note(Severity::Degraded, std::to_string(unwritable_cells_) +
                                               " sayı değeri sütuna sığmadığı ya da sayı "
                                               "olmadığı için yazılmadı.");

        // `@TAB` records: read, counted, and not yet attached to anything.
        std::uint64_t rows = 0;
        std::string listed;
        for (const ncz::AttributeTable& t : tables) {
            rows += t.rows.size();
            if (listed.size() < 120) {
                if (!listed.empty()) listed += ", ";
                listed += t.table_ref + ": " + std::to_string(t.rows.size()) + " satır";
            }
        }
        report_.attribute_tables = tables.size();
        report_.attribute_rows   = rows;
        if (!tables.empty())
            diag_.note(Severity::Skipped,
                       "Dosyada " + std::to_string(tables.size()) + " öznitelik tablosu var (" +
                           listed +
                           "); satırları bir nesneye bağlanmadığı için çizime aktarılmadı.");

        say_strays();
        fields_report();

        if (report_.entities == 0)
            return core::err(ErrorCode::ValidationFailed,
                             "'" + path +
                                 "' okunabilir geometri içermiyor; çizime hiçbir şey eklenmedi.");
        return core::ok();
    }

private:
    struct Layer
    {
        core::LayerId slot{core::kNoLayer};
        std::optional<std::uint32_t> colour;
    };

    // ---- layers ---------------------------------------------------------------

    /// The layer an entity lands on: its name in the file, without control
    /// characters, or `KATMAN_<code>` when no table names its code.
    std::string layer_of(const ncz::Entity& e)
    {
        std::string clean;
        clean.reserve(e.layer_name.size());
        bool dropped = false;
        for (const char ch : e.layer_name) {
            const auto u = static_cast<unsigned char>(ch);
            if (u < 0x20 || u == 0x7F) {
                dropped = true;
                continue;
            }
            clean.push_back(ch);
        }
        if (dropped) renamed_.insert(e.layer_name);
        if (clean.empty()) clean = "KATMAN_" + std::to_string(e.layer_code);
        return clean;
    }

    /// The wizard's tick boxes: empty means every layer; the match is
    /// Turkish-folded (CLAUDE.md 5.6).
    bool wanted(const std::string& name) const
    {
        if (options_.only.empty()) return true;
        return std::any_of(
            options_.only.begin(), options_.only.end(),
            [&](const std::string& pick) { return core::turkish_iequals(pick, name); });
    }

    const Layer* layer_for(const std::string& name, std::uint8_t code)
    {
        if (const auto found = layers_.find(name); found != layers_.end()) return &found->second;
        Layer made;
        made.slot = tx_.ensure_layer(name);
        if (made.slot == core::kNoLayer) {
            failure_ =
                core::err(ErrorCode::ValidationFailed, "'" + name + "' katmanı oluşturulamadı.");
            return nullptr;
        }
        // The layer's OWN colour — what an entity of colour code 0 on it takes —
        // from the tables as they stand at the end of the file.
        made.colour = ncz::layer_color(final_, code);
        if (made.colour) {
            core::Appearance a{};
            a.rgba       = *made.colour;
            a.src_colour = core::Source::Explicit;
            if (auto st = tx_.set_layer_appearance(made.slot, a); !st) {
                failure_ = st.error();
                return nullptr;
            }
        }
        ++report_.layers;
        return &layers_.emplace(name, made).first->second;
    }

    /// The file's pen width — tenths of a millimetre — in micrometres; 0 for a
    /// width that is not one: zero, negative (Netcad's own DXF export writes
    /// those as the thinnest line), or past 100 mm.
    static std::int32_t pen_width_um(double tenths) noexcept
    {
        if (!std::isfinite(tenths) || tenths <= 0.0 || tenths > 1000.0) return 0;
        return static_cast<std::int32_t>(std::llround(tenths * 100.0));
    }

    bool style(command::EntityId id, std::optional<std::uint32_t> argb, std::int32_t width_um)
    {
        const std::uint64_t key = (argb ? (std::uint64_t{1} << 63) | *argb : 0) ^
                                  (static_cast<std::uint64_t>(width_um) << 32);
        auto found = styles_.find(key);
        if (found == styles_.end()) {
            core::Appearance a{};
            if (argb) {
                a.rgba       = *argb;
                a.src_colour = core::Source::Explicit;
            }
            if (width_um > 0) {
                a.width_um  = width_um;
                a.src_width = core::Source::Explicit;
            }
            found = styles_.emplace(key, tx_.intern_style(a)).first;
        }
        if (auto st = tx_.set_entity_style(id, found->second); !st) {
            failure_ = st.error();
            return false;
        }
        return true;
    }

    // ---- geometry ---------------------------------------------------------------

    /// One coordinate to millimetres by THE rounding, with what the rounding
    /// took counted (TODOS F-03); none for a value outside the world.
    std::optional<Point2> to_mm(const ncz::Coord& c)
    {
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || std::fabs(c.x) > kWorld ||
            std::fabs(c.y) > kWorld)
            return std::nullopt;
        const double ex = core::drawing_units_to_mm_exact(c.x, core::DrawingUnit::Metre);
        const double ey = core::drawing_units_to_mm_exact(c.y, core::DrawingUnit::Metre);
        diag_.rounded(ex);
        diag_.rounded(ey);
        extent_min_x_ = std::min(extent_min_x_, c.x);
        extent_max_x_ = std::max(extent_max_x_, c.x);
        extent_min_y_ = std::min(extent_min_y_, c.y);
        extent_max_y_ = std::max(extent_max_y_, c.y);
        return Point2{core::mm_round(ex), core::mm_round(ey)};
    }

    bool run_to_mm(const std::vector<ncz::Coord>& coords)
    {
        points_.clear();
        points_.reserve(coords.size());
        for (const ncz::Coord& c : coords) {
            const auto p = to_mm(c);
            if (!p) return false;
            points_.push_back(*p);
        }
        return true;
    }

    static core::Error invalid()
    {
        return core::err(ErrorCode::ValidationFailed,
                         "koordinatı sayı değil ya da ±100 000 km dışında");
    }

    core::Result<command::EntityId> place(const ncz::Entity& e, core::LayerId slot)
    {
        switch (e.kind) {
        case ncz::Kind::Point:
        case ncz::Kind::Symbol:
        case ncz::Kind::Block: {
            if (e.coords.empty()) return invalid();
            const auto at = to_mm(e.coords.front());
            if (!at) return invalid();
            return tx_.add_point(slot, *at);
        }
        case ncz::Kind::Line:
        case ncz::Kind::Polyline:
            if (!run_to_mm(e.coords)) return invalid();
            return tx_.add_polyline(slot, points_);
        case ncz::Kind::SmartObject:
            // A Netcad 8 "akıllı nesne": drawn as the symbol it is.
            if (e.smart != ncz::SmartClass::None) return place_planet(e, slot);
            // A plan notation anchored at a point (ncz_format.hpp): a point.
            if (e.coords.size() == 1) {
                const auto at = to_mm(e.coords.front());
                if (!at) return invalid();
                return tx_.add_point(slot, *at);
            }
            [[fallthrough]];
        case ncz::Kind::Polygon:
        case ncz::Kind::MapSheet:
        case ncz::Kind::Triangle: {
            if (!run_to_mm(e.coords)) return invalid();
            // The closing vertex is implied (model.md R10).
            while (points_.size() >= 2 && points_.back() == points_.front())
                points_.pop_back();
            if (points_.size() < 3)
                return core::err(ErrorCode::ValidationFailed,
                                 "kapalı şekil milimetrede üç köşeye ulaşmıyor");
            const core::RingGeometry::RingInput ring{points_, core::RingRole::Exterior, 0};
            return tx_.add_area(slot, {&ring, 1});
        }
        case ncz::Kind::Circle: return place_circle(e, slot);
        case ncz::Kind::Arc: return place_arc(e, slot);
        case ncz::Kind::Text: return place_text(e, slot);
        }
        return core::err(ErrorCode::Internal, "bilinmeyen NCZ türü");
    }

    core::Result<command::EntityId> place_circle(const ncz::Entity& e, core::LayerId slot)
    {
        if (e.coords.empty()) return invalid();
        const auto centre = to_mm(e.coords.front());
        if (!centre) return invalid();
        if (!std::isfinite(e.radius) || e.radius <= 0.0 || e.radius > kWorld)
            return core::err(ErrorCode::ValidationFailed, "yarıçapı sayı değil ya da sıfır");
        const Mm radius = core::mm_from_metres(e.radius);
        if (radius <= 0)
            return core::err(ErrorCode::ValidationFailed, "yarıçap bir milimetrenin altında");
        return tx_.add_circle(slot, *centre, radius);
    }

    /// The arc the plugin draws (`_approximate_arc`): the angles in degrees
    /// when either is past a full turn in radians, in radians otherwise; the
    /// end brought past the start a turn at a time; nothing when the sweep is
    /// empty, over ten turns, or ends where it began.
    core::Result<command::EntityId> place_arc(const ncz::Entity& e, core::LayerId slot)
    {
        if (e.coords.empty()) return invalid();
        const auto centre = to_mm(e.coords.front());
        if (!centre) return invalid();
        double start = e.start_angle;
        double end   = e.end_angle;
        if (!std::isfinite(e.radius) || e.radius <= 0.0 || e.radius > kWorld ||
            !std::isfinite(start) || !std::isfinite(end))
            return core::err(ErrorCode::ValidationFailed, "yarıçapı ya da açıları sayı değil");

        constexpr double kFullTurnRad = 2.0 * core::kPi + 0.001;
        if (std::fabs(start) <= kFullTurnRad && std::fabs(end) <= kFullTurnRad) {
            start *= 180.0 / core::kPi; // `math.degrees`
            end *= 180.0 / core::kPi;
        }
        // `while end < start: end += 360.0`, one turn at a time as the plugin
        // adds it. A pair ten thousand turns apart is not an arc; the plugin
        // would spin on it, this reader refuses it.
        for (int turns = 0; end < start; ++turns) {
            if (turns == 10000)
                return core::err(ErrorCode::ValidationFailed, "açıları birbirinden çok uzak");
            end += 360.0;
        }
        const double sweep = end - start;
        if (sweep <= 0.0 || sweep > 3600.0)
            return core::err(ErrorCode::ValidationFailed, "yay açıklığı boş ya da on turdan fazla");

        const Mm radius = core::mm_from_metres(e.radius);
        if (radius <= 0)
            return core::err(ErrorCode::ValidationFailed, "yarıçap bir milimetrenin altında");

        const auto wrap = [](std::int64_t udeg) {
            udeg %= core::kUDegFullCircle;
            return udeg < 0 ? udeg + core::kUDegFullCircle : udeg;
        };
        const std::int64_t from = wrap(dxf::udeg_from_degrees(start));
        const std::int64_t to   = wrap(dxf::udeg_from_degrees(end));
        if (sweep >= 360.0) {
            // More than a whole turn: what the plugin draws covers the circle,
            // unless it closes exactly — then its chord check leaves nothing.
            if (from == to)
                return core::err(ErrorCode::ValidationFailed, "yay tam tur; başı ile sonu aynı");
            ++full_turns_;
            ++degraded_[static_cast<std::size_t>(ncz::Kind::Arc)];
            return tx_.add_circle(slot, *centre, radius);
        }
        const Point2 a = dxf::point_on_circle(*centre, radius, from);
        const Point2 b = dxf::point_on_circle(*centre, radius, to);
        if (a == b)
            return core::err(ErrorCode::ValidationFailed,
                             "yayın iki ucu milimetrede aynı noktaya düşüyor");
        return tx_.add_arc(slot, *centre, radius, a, b);
    }

    // ---- Netcad 8 SmartObjects -------------------------------------------------

    /// The scale a block reference is written with, to a millionth.
    static core::Ratio ratio_of(double v)
    {
        const auto num       = static_cast<std::int64_t>(std::llround(v * 1000000.0));
        const std::int64_t d = 1000000;
        const std::int64_t g = std::gcd(num < 0 ? -num : num, d);
        return g > 1 ? core::Ratio{num / g, d / g} : core::Ratio{num, d};
    }

    /// The block that draws `symbol`, made the first time it is asked for: one
    /// definition for every object that reads the same, on layer `0` and
    /// ByBlock, so each reference draws it in its own layer's colour.
    core::Result<core::BlockId> block_for(const ncz::Symbol& symbol)
    {
        if (const auto found = blocks_.find(symbol.key); found != blocks_.end())
            return found->second;
        auto made = tx_.add_block(symbol.key, symbol.summary, Point2{0, 0});
        if (!made) return made.error();
        const core::BlockId block = made.value();
        if (member_layer_ == core::kNoLayer) {
            member_layer_ = tx_.ensure_layer("0");
            core::Appearance a{};
            a.src_colour  = core::Source::ByBlock;
            member_style_ = tx_.intern_style(a);
        }
        const auto mm = [](double metres) { return core::mm_from_metres(metres); };
        const auto at = [&](std::pair<double, double> xy) {
            return Point2{mm(xy.first), mm(xy.second)};
        };
        for (const ncz::Stroke& st : symbol.strokes) {
            core::Result<command::EntityId> member = core::err(ErrorCode::Internal, "");
            switch (st.type) {
            case ncz::Stroke::Type::Circle: {
                const Point2 c = at(st.centre);
                const Point2 pts[2]{c, Point2{c.x + mm(st.radius), c.y}};
                const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 2),
                                                         core::RingRole::Open, 0};
                member = tx_.add_kind(member_layer_, core::kCircleKind, {&ring, 1}, {}, block);
                break;
            }
            case ncz::Stroke::Type::Polyline: {
                std::vector<Point2> pts;
                for (const auto& xy : st.points)
                    pts.push_back(at(xy));
                const core::RingGeometry::RingInput ring{
                    pts, st.closed ? core::RingRole::Exterior : core::RingRole::Open, 0};
                member = tx_.add_kind(member_layer_, core::kPolylineKind, {&ring, 1}, {}, block);
                break;
            }
            case ncz::Stroke::Type::Text: {
                const Mm height  = std::max<Mm>(1, mm(st.height));
                const Point2 p   = at(st.centre);
                const Mm advance = st.wrap_width > 0.0
                                       ? mm(st.wrap_width)
                                       : std::max<Mm>(1, core::text_width(st.text, height));
                const Point2 pts[2]{p, Point2{p.x + advance, p.y}};
                const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 2),
                                                         core::RingRole::Open, 0};
                member = tx_.add_kind(member_layer_, core::kPolylineKind, {&ring, 1}, {}, block);
                if (member) {
                    core::TextLines lines;
                    lines.wrap = st.wrap_width > 0.0;
                    if (auto t = tx_.set_text(member.value(), st.text, height, st.anchor, lines);
                        !t)
                        return t.error();
                }
                break;
            }
            }
            if (!member) return member.error();
            if (auto styled = tx_.set_entity_style(member.value(), member_style_); !styled)
                return styled.error();
        }
        blocks_.emplace(symbol.key, block);
        return block;
    }

    /// A Netcad 8 SmartObject as a reference to the block of its symbol,
    /// scaled by its object size and turned by its rotation; a class this
    /// reader does not draw is a point, its properties on it all the same.
    core::Result<command::EntityId> place_planet(const ncz::Entity& e, core::LayerId slot)
    {
        if (e.coords.empty()) return invalid();
        const auto at = to_mm(e.coords.front());
        if (!at) return invalid();
        const auto symbol = ncz::planet_symbol(e);
        if (!symbol) {
            ++planet_undrawn_[e.smart];
            return tx_.add_point(slot, *at);
        }
        ++planet_[e.smart];
        auto block = block_for(*symbol);
        if (!block) return block.error();

        double size = e.scale;
        if (!std::isfinite(size) || size <= 0.0 || size > 1000.0) {
            size = 1.0; // Netcad's own default object size
            ++planet_unsized_;
        }
        core::BlockReference ref;
        ref.block         = block.value();
        ref.sx            = ratio_of(size);
        ref.sy            = ref.sx;
        double turn       = std::isfinite(e.rotation) ? e.rotation : 0.0;
        std::int64_t udeg = dxf::udeg_from_degrees(turn) % core::kUDegFullCircle;
        if (udeg < 0) udeg += core::kUDegFullCircle;
        ref.rotation_udeg = udeg;
        ref.bounds        = core::block_reference_bounds(tx_.document(), *at, ref);
        const Point2 pts[1]{*at};
        const core::RingGeometry::RingInput ring{std::span<const Point2>(pts, 1),
                                                 core::RingRole::Open, 0};
        return tx_.add_kind(slot, core::kBlockReferenceKind, {&ring, 1},
                            core::encode_block_reference(ref), core::kNoBlock);
    }

    /// What the symbol says, as columns of the object: its class in
    /// `akilli_nesne`, and every value the user entered and did not switch
    /// off — `nizam`, `kat`, `taks`, `genislik`… — under the label Netcad gives
    /// it. A settlement symbol's three distances say which garden they are.
    bool planet_properties(const ncz::Entity& e, command::EntityId id)
    {
        if (!class_column_) {
            class_column_ = column("akilli_nesne", "akıllı nesne", core::AttrType::Text);
            if (failure_) return false;
            if (!class_column_) class_column_ = core::kNoAttr;
        }
        if (*class_column_ != core::kNoAttr)
            if (auto st = tx_.set_attribute(*class_column_, id,
                                            core::attr_text(ncz::smart_class_name(e.smart)));
                !st) {
                failure_ = st.error();
                return false;
            }
        for (const ncz::SmartProperty& p : e.properties) {
            if (!p.user || p.null || p.value.empty()) continue;
            std::string label = p.display.empty() ? p.name : p.display;
            if (e.smart == ncz::SmartClass::Settlement &&
                (p.name == "txtOn" || p.name == "txtArka" || p.name == "txtYan"))
                label += " bahçe";
            std::string cid = core::turkish_fold_key(label);
            for (char& ch : cid) {
                if (ch >= 'A' && ch <= 'Z')
                    ch = static_cast<char>(ch - 'A' + 'a');
                else if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')))
                    ch = '_';
            }
            if (cid.empty() || p.name == "rtfData") continue;
            auto found = planet_columns_.find(cid);
            if (found == planet_columns_.end()) {
                auto made = column(cid.c_str(), label.c_str(), core::AttrType::Text);
                if (failure_) return false;
                found = planet_columns_.emplace(cid, made).first;
            }
            if (!found->second) continue;
            if (auto st = tx_.set_attribute(*found->second, id, core::attr_text(p.value)); !st) {
                failure_ = st.error();
                return false;
            }
        }
        return true;
    }

    /// A caption on a baseline from its insertion point, as long as the text is
    /// wide (`core::text_width`, the rule METİN follows), turned by the file's
    /// rotation — anchored at the baseline's left, the CAD default.
    core::Result<command::EntityId> place_text(const ncz::Entity& e, core::LayerId slot)
    {
        if (e.coords.empty()) return invalid();
        const auto at = to_mm(e.coords.front());
        if (!at) return invalid();
        const Mm height = std::max<Mm>(1, core::mm_from_metres(e.text_height));
        double rotation = e.rotation;
        if (!std::isfinite(rotation)) {
            rotation = 0.0;
            ++flat_rotation_;
            ++degraded_[static_cast<std::size_t>(ncz::Kind::Text)];
        }
        std::int64_t udeg = dxf::udeg_from_degrees(rotation) % core::kUDegFullCircle;
        if (udeg < 0) udeg += core::kUDegFullCircle;
        const Mm advance = std::max<Mm>(1, core::text_width(e.label, height));
        const std::array<Point2, 2> baseline{*at, dxf::point_on_circle(*at, advance, udeg)};
        auto made = tx_.add_polyline(slot, baseline);
        if (!made) return made;
        if (auto st = tx_.set_text(made.value(), e.label, height, core::TextAnchor::BaselineLeft);
            !st)
            return st.error();
        return made;
    }

    // ---- attributes ---------------------------------------------------------------

    /// The column `id`, declared on first use and reused when the document
    /// already has it; none, and said, when it is there with another type.
    std::optional<core::AttrId> column(const char* id, const char* label, core::AttrType type)
    {
        const core::AttrTable& table = tx_.document().attributes();
        if (const core::AttrId found = table.find(id); found != core::kNoAttr) {
            const core::AttrColumn* held = table.column(found);
            if (held != nullptr && held->spec().type == type) return found;
            diag_.note(Severity::Degraded,
                       "'" + std::string(id) +
                           "' sütunu belgede başka türde tanımlı; alan okunmadı.");
            return std::nullopt;
        }
        core::AttrSpec spec;
        spec.id      = id;
        spec.name_tr = label;
        spec.type    = type;
        if (type == core::AttrType::Decimal) spec.scale = kDecimals;
        auto made = tx_.declare_attribute(std::move(spec));
        if (!made) {
            failure_ = made.error();
            return std::nullopt;
        }
        return made.value();
    }

    bool point_number(command::EntityId id, const std::string& number)
    {
        if (!number_column_) {
            number_column_ = column("nokta_no", "nokta no", core::AttrType::Text);
            if (failure_) return false;
            if (!number_column_) number_column_ = core::kNoAttr;
        }
        if (*number_column_ == core::kNoAttr) return true;
        if (auto st = tx_.set_attribute(*number_column_, id, core::attr_text(number)); !st) {
            failure_ = st.error();
            return false;
        }
        return true;
    }

    bool asked(std::size_t k) const
    {
        if (everything_) return true;
        return std::any_of(
            options_.fields.begin(), options_.fields.end(),
            [&](const std::string& f) { return core::turkish_iequals(f, kFields[k].id); });
    }

    /// The asked fields of `e` onto `id`.
    bool stamp(const ncz::Entity& e, command::EntityId id)
    {
        if (options_.fields.empty()) return true;
        if (!columns_ready_) {
            columns_ready_ = true;
            for (std::size_t k = 0; k < kFields.size(); ++k) {
                if (!asked(k)) continue;
                columns_[k] = column(kFields[k].id, kFields[k].label, kFields[k].type);
                if (failure_) return false;
            }
        }
        for (std::size_t k = 0; k < kFields.size(); ++k) {
            if (!columns_[k]) continue;
            core::AttrValue value;
            if (kFields[k].type == core::AttrType::Text) {
                const auto v = text_value(k, e, source_);
                if (!v) continue;
                value = core::attr_text(*v);
            } else if (kFields[k].type == core::AttrType::Int64) {
                value = core::attr_int64(e.layer_code);
            } else {
                const auto v = number_value(k, e);
                if (!v) continue;
                // A micrometre of 9 × 10¹² m is the most a 64-bit cell holds.
                if (!std::isfinite(*v) || std::fabs(*v) >= 9.0e12) {
                    ++unwritable_cells_;
                    continue;
                }
                value = core::attr_decimal(std::llround(*v * 1.0e6), kDecimals);
            }
            if (auto st = tx_.set_attribute(*columns_[k], id, value); !st) {
                failure_ = st.error();
                return false;
            }
        }
        return true;
    }

    /// The first value each field takes in the file, for the wizard.
    void sample(const ncz::Entity& e)
    {
        for (std::size_t k = 0; k < kFields.size(); ++k) {
            if (!samples_[k].empty()) continue;
            if (kFields[k].type == core::AttrType::Int64) {
                samples_[k] = std::to_string(e.layer_code);
            } else if (kFields[k].type == core::AttrType::Text) {
                if (const auto v = text_value(k, e, source_); v && !v->empty())
                    samples_[k] = v->substr(0, 40);
            } else if (const auto v = number_value(k, e); v && std::isfinite(*v) && *v != 0.0) {
                samples_[k] = sample_of(*v);
            }
        }
    }

    void fields_report()
    {
        report_.fields.clear();
        for (std::size_t k = 0; k < kFields.size(); ++k) {
            VectorField f;
            f.layer    = "tüm katmanlar";
            f.name     = kFields[k].id;
            f.id       = kFields[k].id;
            f.type     = kFields[k].type;
            f.scale    = kFields[k].type == core::AttrType::Decimal ? kDecimals : 0;
            f.sample   = samples_[k];
            f.imported = columns_[k].has_value();
            report_.fields.push_back(std::move(f));
        }
    }

    // ---- strays ---------------------------------------------------------------

    /// Objects hundreds of kilometres from the rest of the drawing, said.
    ///
    /// A real UİP holds two copies of a 2,6 m transformer outline drawn at
    /// (0, 0) — a digitising slip in the source, and Netcad shows it too. It is
    /// the drawing's data and is read as such; but `YAKINLAŞ KAPSAM` then fits
    /// a 4 400 km box and the city is a dot in its corner, and a user looking at
    /// that dot deserves to be told why. The bulk is where 96 % of the objects
    /// are; a stray is outside that box grown by twenty times its size and at
    /// least 100 km on every side — far enough to shrink the fitted view to a
    /// twentieth, and never the long tail of a real town (the `Suşehri` plan's
    /// runs 10 km south of its centre and is not one).
    void say_strays()
    {
        if (places_.size() < 100) return;
        const auto percentile = [this](bool east, double q) {
            std::vector<double> v;
            v.reserve(places_.size());
            for (const Place& pl : places_)
                v.push_back(east ? pl.x : pl.y);
            const auto at = static_cast<std::ptrdiff_t>(q * static_cast<double>(v.size() - 1));
            std::nth_element(v.begin(), v.begin() + at, v.end());
            return v[static_cast<std::size_t>(at)];
        };
        const double x0   = percentile(true, 0.02);
        const double x1   = percentile(true, 0.98);
        const double y0   = percentile(false, 0.02);
        const double y1   = percentile(false, 0.98);
        const double grow = std::max(20.0 * std::max(x1 - x0, y1 - y0), 100000.0);

        std::size_t strays = 0;
        std::map<std::string, std::size_t> where;
        std::optional<Place> first;
        for (const Place& pl : places_) {
            if (pl.x >= x0 - grow && pl.x <= x1 + grow && pl.y >= y0 - grow && pl.y <= y1 + grow)
                continue;
            ++strays;
            if (!first) first = pl;
            for (const auto& [name, info] : layers_)
                if (&info == pl.layer) {
                    ++where[name];
                    break;
                }
        }
        if (strays == 0 || strays * 100 > places_.size()) return;

        std::string layers;
        std::size_t named = 0;
        for (const auto& [name, n] : where) {
            if (named++ == 3) {
                layers += ", …";
                break;
            }
            if (!layers.empty()) layers += ", ";
            layers += name + " " + std::to_string(n);
        }
        const auto metres = [](double v) {
            return core::metres_fixed(core::mm_from_metres(v), 0, ',');
        };
        diag_.note(Severity::Warning,
                   std::to_string(strays) + " nesne çizimin geri kalanından çok uzakta (" + layers +
                       "; ilki Y " + metres(first->x) + ", X " + metres(first->y) +
                       " yakınında). YAKINLAŞ KAPSAM bu yüzden çizimi küçük gösterir; kaynakta "
                       "yanlış yere düşmüşlerse silin.");
    }

    // ---- the coordinate system ---------------------------------------------------

    core::Status say_system(const ncz::Header& h)
    {
        const std::string& crs = report_.crs;

        // A GEOGRAPHIC declaration over numbers that ARE degrees: refused, because
        // the store counts millimetres and a degree read as a metre collapses a
        // parcel onto a point (io.md R20a). Over numbers the size of metres the
        // declaration is what is wrong, and the numbers are read.
        if (h.mproj && h.projection == 1) {
            const bool degrees = report_.entities != 0 && extent_min_x_ >= -180.0 &&
                                 extent_max_x_ <= 180.0 && extent_min_y_ >= -90.0 &&
                                 extent_max_y_ <= 90.0;
            if (degrees)
                return core::err(ErrorCode::ValidationFailed,
                                 "Dosya coğrafi koordinatlarda (" + report_.declared +
                                     ") ve bütün koordinatları derece aralığında. KentOSCad "
                                     "metre sayan bir sistemde milimetre saklar; çizimi Netcad'de "
                                     "bir TM ya da UTM dilimine dönüştürüp yeniden aktarın.");
            diag_.note(Severity::Warning,
                       "Dosya coğrafi sistem bildiriyor (" + report_.declared +
                           ") ama koordinatları metre büyüklüğünde; bildirim yanlış görünüyor. "
                           "Koordinatlar çizimin sistemi " +
                           crs + " içinde metre olarak okundu.");
            return core::ok();
        }

        if (report_.declared.empty()) {
            diag_.note(
                Severity::Warning,
                "Dosya koordinat sistemi bildirmiyor. Çizimin kendi sistemi varsayıldı: " + crs +
                    ". Yanlışsa GERİAL ile geri alın, AYAR koordinat_sistemi ile doğrusunu "
                    "kurun ve yeniden aktarın.");
            return core::ok();
        }

        // THE TM30/TM33 BLUNDER, made visible (model.md R36): a three-degree zone
        // whose meridian is not the drawing's.
        if (h.mproj && h.projection == 3 && options_.project_meridian != 0 &&
            static_cast<int>(h.zone) != options_.project_meridian) {
            diag_.note(Severity::Warning,
                       "Dosya " + report_.declared + " bildiriyor; çizimin sistemi (" + crs + ") " +
                           std::to_string(options_.project_meridian) +
                           "° orta meridyenli. Koordinatlar dönüştürülmedi: dilimler farklıysa "
                           "çizim yanlış yere düşer. GERİAL ile geri alın, AYAR "
                           "koordinat_sistemi ile doğru dilimi kurun ve yeniden aktarın.");
            return core::ok();
        }

        diag_.note(Severity::Info, "Dosyanın bildirdiği sistem: " + report_.declared +
                                       ". Koordinatlar dönüştürülmeden çizimin sistemi " + crs +
                                       " içinde okundu.");
        return core::ok();
    }

    command::Transaction& tx_;
    const ImportOptions& options_;
    NczReport& report_;
    ImportDiagnostics& diag_;
    std::string source_;
    std::stop_token stop_;
    ncz::Header final_;

    bool everything_{false};
    bool cancelled_{false};
    std::optional<core::Error> failure_;

    std::unordered_map<std::string, Layer> layers_;
    std::map<std::string, std::size_t> census_;
    std::unordered_map<std::uint64_t, core::StyleId> styles_;
    std::set<std::string> renamed_;
    std::vector<Point2> points_;

    /// Where each object read begins, metres, and the layer it went to: what
    /// `say_strays` looks for strays in.
    struct Place
    {
        double x;
        double y;
        const Layer* layer;
    };

    std::vector<Place> places_;

    std::optional<core::AttrId> number_column_;
    std::optional<core::AttrId> class_column_;
    std::map<std::string, std::optional<core::AttrId>> planet_columns_;
    std::unordered_map<std::string, core::BlockId> blocks_;
    core::LayerId member_layer_{core::kNoLayer};
    core::StyleId member_style_{};
    std::map<ncz::SmartClass, std::uint64_t> planet_;         ///< drawn as a symbol, by class
    std::map<ncz::SmartClass, std::uint64_t> planet_undrawn_; ///< read as a point, by class
    std::uint64_t planet_unsized_{0};
    bool columns_ready_{false};
    std::array<std::optional<core::AttrId>, kFields.size()> columns_{};
    std::array<std::string, kFields.size()> samples_{};

    std::array<std::uint64_t, ncz::kKindCount> read_{};
    std::array<std::uint64_t, ncz::kKindCount> skipped_{};
    std::array<std::uint64_t, ncz::kKindCount> degraded_{};
    std::uint64_t seen_{0};
    std::uint64_t widths_{0};
    std::int32_t thinnest_um_{0};
    std::int32_t widest_um_{0};
    std::uint64_t full_turns_{0};
    std::uint64_t flat_rotation_{0};
    std::uint64_t unwritable_cells_{0};

    double extent_min_x_{HUGE_VAL};
    double extent_max_x_{-HUGE_VAL};
    double extent_min_y_{HUGE_VAL};
    double extent_max_y_{-HUGE_VAL};
};

} // namespace

command::Task<core::Result<NczReport>> import_ncz(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop)
{
    // io.md P14: no network path, no archive path — the rule every reader keeps.
    if (path.rfind("/vsi", 0) == 0)
        co_return core::err(ErrorCode::InvalidArgument,
                            "'" + path +
                                "' sanal dosya sistemi yolu. KentOSCad bir veri dosyasının ağdan "
                                "ya da arşivin içinden okunmasına izin vermez; dosyayı diske alıp "
                                "yeniden deneyin.");
    if (stop.stop_requested())
        co_return core::err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");

    // io.md R20: the numbers are read in the drawing's system, so there has to be
    // one. A fresh drawing has TUREF/TM36; only a drawing whose system was
    // cleared arrives here without.
    if (options.project_crs.empty())
        co_return core::err(
            ErrorCode::ValidationFailed,
            "'" + path +
                "' koordinatları çizimin sisteminde okunur ama çizimin koordinat "
                "sistemi yok. AYAR koordinat_sistemi ile kurun ve yeniden aktarın.");

    auto mapped = MappedFile::open(path);
    if (!mapped)
        co_return core::err(ErrorCode::IoFailure,
                            "'" + path + "' okunamadı: " + mapped.error().message);
    const std::span<const std::byte> raw = mapped.value().bytes();
    const std::span<const std::uint8_t> data(reinterpret_cast<const std::uint8_t*>(raw.data()),
                                             raw.size());

    NczReport report;
    report.crs = options.project_crs;
    Mapper mapper(tx, options, report, std::filesystem::path(path).stem().string(), stop);

    ncz::Header header;
    const ncz::Outcome read = ncz::read(data, mapper, header, stop);
    if (read == ncz::Outcome::Cancelled || mapper.cancelled())
        co_return core::err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");
    if (const auto& failed = mapper.failure(); failed) co_return *failed;

    auto tables = ncz::attribute_tables(data, stop);
    if (!tables)
        co_return core::err(ErrorCode::Cancelled, "İçe aktarma durduruldu; çizim değişmedi.");

    if (auto st = mapper.finish(header, *tables, path); !st) co_return st.error();
    co_return report;
}

} // namespace kentos::io
