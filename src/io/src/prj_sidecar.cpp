// SPDX-License-Identifier: GPL-3.0-or-later
#include "prj_sidecar.hpp"

#ifdef KENTOS_HAVE_GDAL
#include <cpl_conv.h>
#include <cpl_error.h>
#include <ogr_spatialref.h>
#endif

#include <fstream>
#include <iterator>

namespace kentos::io {

using core::err;
using core::ErrorCode;

std::string prj_sidecar_path(const std::string& path)
{
    std::string sidecar = path;
    const auto dot      = sidecar.find_last_of('.');
    const auto slash    = sidecar.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) sidecar.erase(dot);
    sidecar += ".prj";
    return sidecar;
}

#ifdef KENTOS_HAVE_GDAL

namespace {

std::string gdal_reason()
{
    const char* msg = ::CPLGetLastErrorMsg();
    return msg && *msg ? std::string(msg) : std::string("(GDAL sebep bildirmedi)");
}

} // namespace

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
    if (authority && code) return std::string(authority) + ":" + code;
    const char* name = srs.GetName();
    return name ? std::string(name) : std::string("WKT");
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
