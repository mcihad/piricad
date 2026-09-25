// SPDX-License-Identifier: GPL-3.0-or-later
#include "prj_sidecar.hpp"

#ifdef KENTOS_HAVE_GDAL
#include <cpl_conv.h>
#include <cpl_error.h>
#include <ogr_spatialref.h>
#endif

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace kentos::io {

using core::err;
using core::ErrorCode;

std::string dxf_prj_withheld(core::DrawingUnit unit)
{
    return std::string("DXF'in yanına .prj yazılmadı: sayıları ") + core::drawing_unit_name(unit) +
           " (AYAR çizim_birimi), .prj'nin bildirdiği sistem ise metre sayar — bir CBS programı "
           "bu dosyayı metre okuyup yanlış yere ve yanlış ölçekte koyardı. Koordinat sistemini "
           "taşıyan bir DXF için AYAR çizim_birimi metre ile yeniden dışa aktarın.";
}

core::Status file_crs_holds_metres(const core::Crs& crs, const std::string& where)
{
    const std::string problem = core::crs_unit_problem(crs);
    if (problem.empty()) return {};
    // THE WAY IN, for the tools a Turkish GIS office already has open: the
    // conversion is PROJ's in both, and a later import reads metres. Not a
    // conversion of our own on the way in — that is a datum question (which
    // transformation, how accurate) and it gets its own answer rather than a
    // silent ballpark one.
    return err(ErrorCode::ValidationFailed,
               where + " içe alınmadı. " + problem +
                   " Dosyayı önce metre birimli bir sisteme dönüştürüp öyle alın: QGIS'te "
                   "Farklı Kaydet ▸ KRS olarak ör. EPSG:5256 (TUREF/TM36), ya da komutla "
                   "ogr2ogr -t_srs EPSG:5256 yeni.gpkg eski.gpkg. İçe alırken dönüştürme "
                   "Faz 1'de gelecek.");
}

std::string prj_sidecar_path(const std::string& path)
{
    std::string sidecar = path;
    const auto dot      = sidecar.find_last_of('.');
    const auto slash    = sidecar.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) sidecar.erase(dot);
    sidecar += ".prj";
    return sidecar;
}

std::string remove_stale_prj(const std::string& path)
{
    const std::string stale = prj_sidecar_path(path);
    std::error_code ec;
    if (!std::filesystem::exists(stale, ec) || !std::filesystem::remove(stale, ec)) return {};
    return "'" + stale +
           "' kaldırıldı: önceki bir metre dışa aktarımından kalmıştı ve bu çizimin sayılarını "
           "metre diye etiketlerdi.";
}

#ifdef KENTOS_HAVE_GDAL

namespace {

std::string gdal_reason()
{
    const char* msg = ::CPLGetLastErrorMsg();
    return msg && *msg ? std::string(msg) : std::string("(GDAL sebep bildirmedi)");
}

} // namespace

core::Crs crs_with_unit(const OGRSpatialReference& srs, std::string id)
{
    core::Crs crs(std::move(id));
    // GDAL's own classification, which reads a compound system by its
    // horizontal half and a bound one (a WKT with TOWGS84) by the system it
    // binds — the same reading the geodesy module asks PROJ for.
    if (srs.IsGeographic() != 0) {
        const char* name = nullptr;
        (void)srs.GetAngularUnits(&name);
        crs.set_unit(core::CrsUnit::Degree, name != nullptr ? name : "degree");
    } else if (srs.IsGeocentric() != 0) {
        crs.set_unit(core::CrsUnit::Other, "yer merkezli metre (X/Y/Z)");
    } else if (srs.IsProjected() != 0 || srs.IsLocal() != 0) {
        const char* name    = nullptr;
        const double factor = srs.GetLinearUnits(&name);
        // EXACTLY one, as in the geodesy module: a US survey foot is
        // 0,304800609601… and a system in it must not pass for metres.
        if (factor > 0.0)
            crs.set_unit(factor == 1.0 ? core::CrsUnit::Metre : core::CrsUnit::Other,
                         name != nullptr ? name : "");
    }
    return crs;
}

core::Result<std::string> prj_sidecar_crs(const std::string& path)
{
    const std::string sidecar = prj_sidecar_path(path);
    std::ifstream in(sidecar, std::ios::binary);
    if (!in) return err(ErrorCode::NotFound, sidecar);

    // A .prj is one WKT string. A hostile one is still only text handed to PROJ's
    // parser, and the cap keeps a multi-gigabyte "sidecar" out of memory (R18).
    std::string wkt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (wkt.empty() || wkt.size() > (1u << 20))
        return err(ErrorCode::ParseError,
                   "'" + sidecar + "' boş ya da akla yatkın bir koordinat sistemi tanımı değil.");

    OGRSpatialReference srs;
    ::CPLErrorReset();
    if (srs.SetFromUserInput(wkt.c_str()) != OGRERR_NONE)
        return err(ErrorCode::ParseError,
                   "'" + sidecar + "' içindeki koordinat sistemi çözülemedi: " + gdal_reason());

    const char* authority = srs.GetAuthorityName(nullptr);
    const char* code      = srs.GetAuthorityCode(nullptr);
    const char* name      = srs.GetName();
    std::string id        = authority && code ? std::string(authority) + ":" + code
                                              : (name ? std::string(name) : std::string("WKT"));

    // A `.prj` beside a DXF says what its numbers ARE, and a DXF of degrees is
    // refused here for the reason every other file is (TODOS F-03).
    if (auto st = file_crs_holds_metres(crs_with_unit(srs, id), "'" + path + "'"); !st)
        return st.error();
    return id;
}

core::Result<std::string> write_prj_sidecar(const std::string& path, const std::string& crs_id)
{
    OGRSpatialReference srs;
    ::CPLErrorReset();
    if (srs.SetFromUserInput(crs_id.c_str()) != OGRERR_NONE)
        return err(ErrorCode::ValidationFailed, "Çizimin koordinat sistemi '" + crs_id +
                                                    "' .prj için çözülemedi: " + gdal_reason());

    // WKT1 morphed to the ESRI dialect, because that is what a .prj is and what
    // every GIS in the country reads back.
    srs.morphToESRI();
    char* wkt = nullptr;
    if (srs.exportToWkt(&wkt) != OGRERR_NONE || !wkt) {
        if (wkt) ::CPLFree(wkt);
        return err(ErrorCode::Internal, "Koordinat sistemi WKT'ye çevrilemedi: " + gdal_reason());
    }

    const std::string sidecar = prj_sidecar_path(path);
    std::ofstream out(sidecar, std::ios::binary | std::ios::trunc);
    out << wkt;
    ::CPLFree(wkt);
    if (!out)
        return err(ErrorCode::IoFailure,
                   "'" + sidecar +
                       "' yazılamadı. Koordinat sistemi olmayan bir dışa aktarım eksik veridir; "
                       "dizin izinlerini denetleyin.");
    return sidecar;
}

#else

core::Result<std::string> prj_sidecar_crs(const std::string& path)
{
    (void)path;
    return err(ErrorCode::Unsupported,
               ".prj dosyası bu yapıda çözülemez (KENTOS_WITH_GDAL=OFF); çizimin kendi "
               "sistemi kullanıldı.");
}

core::Result<std::string> write_prj_sidecar(const std::string& path, const std::string& crs_id)
{
    (void)path;
    (void)crs_id;
    return err(ErrorCode::Unsupported, ".prj dosyası bu yapıda yazılamaz (KENTOS_WITH_GDAL=OFF).");
}

#endif

} // namespace kentos::io
