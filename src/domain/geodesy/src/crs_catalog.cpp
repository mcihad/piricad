// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/domain/geodesy/crs_catalog.hpp"

#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace piricad::domain::geodesy {
namespace {

using core::ErrorCode;
using core::Json;

core::Result<Json> read_json(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return core::err(ErrorCode::IoFailure, "CRS veri dosyası açılamadı: " + path);

    std::ostringstream buf;
    buf << in.rdbuf();
    return Json::parse(buf.str());
}

core::Error missing(const std::string& field)
{
    return core::err(ErrorCode::ParseError,
                     "tm3-dilimleri.json içinde '" + field + "' alanı eksik veya yanlış türde");
}

} // namespace

core::Result<CrsCatalog> CrsCatalog::load(const std::string& crs_dir)
{
    const std::string path = crs_dir + "/tm3-dilimleri.json";

    auto parsed = read_json(path);
    if (!parsed) return parsed.error();

    const Json& doc = parsed.value();
    CrsCatalog  out;

    // Provenance is not optional: domain.md R23 requires an error message to cite
    // the catalogue version, and a regulatory claim without its source and date is
    // a liability rather than documentation (.claude/docs.md R12).
    const Json* source = doc.find("source");
    const Json* published = doc.find("published");
    const Json* version = doc.find("package_version");
    if (!source || !source->is_string()) return missing("source");
    if (!published || !published->is_string()) return missing("published");
    if (!version || !version->is_string()) return missing("package_version");

    out.source_          = source->as_string();
    out.published_       = published->as_string();
    out.package_version_ = version->as_string();

    const Json* proj = doc.find("projeksiyon");
    if (!proj || !proj->is_object()) return missing("projeksiyon");

    if (const Json* v = proj->find("olcek_katsayisi")) out.params_.scale_factor = v->as_double(1.0);
    if (const Json* v = proj->find("yalanci_dogu_m"))
        out.params_.false_easting_m = static_cast<long>(v->as_int(500000));
    if (const Json* v = proj->find("yalanci_kuzey_m"))
        out.params_.false_northing_m = static_cast<long>(v->as_int(0));
    if (const Json* v = proj->find("datum")) out.params_.datum = v->as_string();

    const Json* zones = doc.find("dilimler");
    if (!zones || !zones->is_array() || zones->as_array().empty()) return missing("dilimler");

    for (const Json& z : zones->as_array()) {
        Tm3Zone zone;
        if (const Json* v = z.find("orta_meridyen_derece"))
            zone.central_meridian = static_cast<int>(v->as_int(0));
        if (const Json* v = z.find("ad")) zone.name = v->as_string();
        if (const Json* v = z.find("epsg")) zone.epsg = static_cast<int>(v->as_int(0));

        if (zone.name.empty() || zone.epsg == 0)
            return core::err(ErrorCode::ParseError,
                             "tm3-dilimleri.json: dilim kaydında 'ad' veya 'epsg' eksik");

        out.zones_.push_back(std::move(zone));
    }
    return out;
}

const Tm3Zone* CrsCatalog::zone_for_longitude(double longitude_deg) const
{
    const Tm3Zone* best = nullptr;
    double         best_gap = 0.0;

    for (const auto& z : zones_) {
        const double gap = std::abs(longitude_deg - static_cast<double>(z.central_meridian));
        if (!best || gap < best_gap) {
            best     = &z;
            best_gap = gap;
        }
    }

    // A 3-degree zone reaches 1.5 degrees either side of its central meridian.
    // Beyond that the point belongs to a zone Turkey does not define, and
    // guessing would hand back a coordinate with an unstated error.
    return (best && best_gap <= 1.5) ? best : nullptr;
}

const Tm3Zone* CrsCatalog::zone_by_epsg(int epsg) const
{
    for (const auto& z : zones_)
        if (z.epsg == epsg) return &z;
    return nullptr;
}

const Tm3Zone* CrsCatalog::zone_by_name(std::string_view name) const
{
    const std::string folded = core::turkish_upper(name);
    for (const auto& z : zones_)
        if (core::turkish_upper(z.name) == folded) return &z;
    return nullptr;
}

} // namespace piricad::domain::geodesy
