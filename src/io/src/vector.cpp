// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: external vector formats through GDAL/OGR.
//
// THE FOUR RULES THAT SHAPE THIS FILE
//
//   io.md P2   No `gdal*.h`, `ogr*.h` or `cpl_*.h` leaves this translation unit.
//   io.md P7   The driver set is an ALLOW-LIST from /cmake, handed in as a
//              compile definition and passed to GDAL as `papszAllowedDrivers`.
//              GDAL is never asked to guess from its full driver table.
//   io.md P14  Nothing embedded in an input file is executed or resolved: the
//              /vsicurl, /vsis3 and /vsizip prefixes are refused before GDAL sees
//              the path, so a "dataset" cannot become a network fetch.
//   io.md R20  A dataset with no CRS is an ERROR. Never a silent TUREF/TM30.
//
// WHEN PIRICAD_WITH_GDAL IS OFF everything below still compiles and every entry
// point returns an Error naming the option, the package and the install command.
// data.md Enforcement: a gated capability reports itself, it never quietly
// succeeds and it never quietly reports nothing.
#include "piricad/io/vector.hpp"

#include "piricad/core/text.hpp"
#include "piricad/io/format.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef PIRICAD_HAVE_GDAL
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#endif

namespace piricad::io {
namespace {

using core::err;
using core::ErrorCode;

/// The allow-list, as /cmake/PiriCADGdalDrivers.cmake declared it. Format:
///   DRIVER:.ext:modes:Türkçe etiket|DRIVER:...
/// where modes is any of "r", "w", "rw". The separator is a vertical bar because
/// a semicolon is a CMake list separator and would not survive the definition.
#ifndef PIRICAD_GDAL_DRIVERS
#define PIRICAD_GDAL_DRIVERS ""
#endif

std::vector<VectorFormat> parse_allow_list()
{
    std::vector<VectorFormat> out;
    const std::string spec = PIRICAD_GDAL_DRIVERS;

    std::size_t begin = 0;
    while (begin < spec.size()) {
        std::size_t end = spec.find('|', begin);
        if (end == std::string::npos) end = spec.size();

        const std::string row = spec.substr(begin, end - begin);
        begin                 = end + 1;
        if (row.empty()) continue;

        std::string field[4];
        std::size_t f = 0, cursor = 0;
        while (f < 4 && cursor <= row.size()) {
            std::size_t sep = row.find(':', cursor);
            if (sep == std::string::npos || f == 3) sep = row.size();
            field[f++] = row.substr(cursor, sep - cursor);
            cursor     = sep + 1;
        }
        if (field[0].empty()) continue;

        VectorFormat vf;
        vf.driver    = field[0];
        vf.extension = field[1];
        vf.read      = field[2].find('r') != std::string::npos;
        vf.write     = field[2].find('w') != std::string::npos;
        vf.label     = field[3].empty() ? field[0] : field[3];
        out.push_back(std::move(vf));
    }
    return out;
}

/// GDAL's virtual filesystem prefixes reach the network, an archive or another
/// process's memory. io.md P14: a path that arrived in a command argument, a
/// script or an AI suggestion never gets to be one of those.
bool is_virtual_path(const std::string& path)
{
    return path.rfind("/vsi", 0) == 0;
}

#ifdef PIRICAD_HAVE_GDAL

/// GDAL's last error, so a failure says what GDAL said rather than "olmadı".
std::string gdal_reason()
{
    const char* msg = ::CPLGetLastErrorMsg();
    return (msg && *msg) ? std::string(msg) : std::string("ayrıntı yok");
}

/// One-time, idempotent driver registration.
///
/// core.md P8 bans a mutable global in /src/core; this is /src/io, and GDAL's
/// driver manager is a process-wide table GDAL itself owns — there is no version
/// of `GDALAllRegister` that is not global. The flag only keeps us from paying
/// for it twice.
void ensure_registered()
{
    static const bool once = [] {
        ::GDALAllRegister();
        // A driver that needs to fetch something is a driver that can hang a save.
        ::CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "10");
        ::CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", "");
        return true;
    }();
    (void)once;
}

/// The allow-list as GDAL wants it: a NULL-terminated char* array. Built once,
/// from the same parsed list the rest of the module uses, so the UI and GDAL can
/// never disagree about which drivers exist.
char** allowed_driver_argv()
{
    static char** argv = [] {
        char** list = nullptr;
        for (const VectorFormat& f : vector_formats())
            list = ::CSLAddString(list, f.driver.c_str());
        return list;
    }();
    return argv;
}

/// The CRS a dataset declares, as an "AUTHORITY:CODE" string.
///
/// io.md R20 and model.md R36: an unlabelled coordinate is an error. A TM30/TM33
/// mix-up is the classic field blunder and it is silent, so the only safe answer
/// to "this file does not say" is to refuse.
core::Result<std::string> crs_of(const OGRSpatialReference* srs, const std::string& layer)
{
    if (!srs)
        return err(ErrorCode::ValidationFailed,
                   "'" + layer +
                       "' katmanı hiçbir koordinat sistemi bildirmiyor. Etiketsiz koordinat "
                       "kabul edilmez: TM30 ile TM33 karışması sessizce yanlış bir tapu üretir. "
                       "Kaynak dosyaya projeksiyon bilgisini ekleyip yeniden deneyin.");

    const char* authority = srs->GetAuthorityName(nullptr);
    const char* code      = srs->GetAuthorityCode(nullptr);
    if (authority && code) return std::string(authority) + ":" + code;

    char* wkt = nullptr;
    if (srs->exportToWkt(&wkt) == OGRERR_NONE && wkt) {
        // No authority code, but a named system is still a labelled one.
        const char* name = srs->GetName();
        std::string out  = name ? std::string(name) : std::string("WKT");
        ::CPLFree(wkt);
        return out;
    }
    return err(ErrorCode::ValidationFailed,
               "'" + layer + "' katmanının koordinat sistemi çözülemedi: " + gdal_reason());
}

/// A dataset handle that closes itself. GDAL predates RAII and every early return
/// below would otherwise leak a file handle.
struct DatasetHandle
{
    GDALDataset* ptr{nullptr};

    ~DatasetHandle()
    {
        if (ptr) ::GDALClose(ptr);
    }

    DatasetHandle() = default;

    explicit DatasetHandle(GDALDataset* p) : ptr(p) {}

    DatasetHandle(const DatasetHandle&)            = delete;
    DatasetHandle& operator=(const DatasetHandle&) = delete;
};

/// One OGR ring to `Mm`, dropping the repeated closing vertex the way core does.
///
/// `mm_from_metres` is the ONE rounding helper (core.md R20). A raw
/// `static_cast<Mm>` here would truncate towards zero and put a southern or
/// western coordinate one millimetre off from its northern twin.
void ring_to_mm(const OGRLinearRing* ring, std::vector<core::Point2>& out)
{
    out.clear();
    const int n = ring->getNumPoints();
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(
            core::Point2{core::mm_from_metres(ring->getX(i)), core::mm_from_metres(ring->getY(i))});

    // RingGeometry stores the corners and implies the closing segment, so the
    // duplicate OGR always writes is dropped here rather than argued about there.
    while (out.size() >= 2 && out.back() == out.front())
        out.pop_back();
}

void line_to_mm(const OGRLineString* line, std::vector<core::Point2>& out)
{
    out.clear();
    const int n = line->getNumPoints();
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(
            core::Point2{core::mm_from_metres(line->getX(i)), core::mm_from_metres(line->getY(i))});
}

/// The CRS in the `.prj` companion beside `path`, when there is one.
///
/// GDAL's DXF driver does NOT read a `.prj` — verified against GDAL 3.12, where a
/// DXF with a correct sidecar still reports `Layer SRS WKT: (unknown)`. DXF has no
/// slot for a coordinate system at all, so without this every DXF would be an
/// unlabelled dataset and io.md R20 would make the format unimportable. PiriCAD
/// therefore reads the sidecar itself, which is the convention every GIS in the
/// country already follows, and writes it on export so its own output round-trips.
core::Result<std::string> sidecar_crs(const std::string& path, OGRSpatialReference& out)
{
    std::string sidecar = path;
    const auto dot      = sidecar.find_last_of('.');
    const auto slash    = sidecar.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) sidecar.erase(dot);
    sidecar += ".prj";

    std::ifstream in(sidecar, std::ios::binary);
    if (!in) return err(ErrorCode::NotFound, sidecar);

    // A .prj is one WKT string. A hostile one is still only text handed to PROJ's
    // parser, and the cap keeps a multi-gigabyte "sidecar" out of memory (R18).
    std::string wkt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (wkt.empty() || wkt.size() > (1u << 20))
        return err(ErrorCode::ParseError,
                   "'" + sidecar + "' boş ya da akla yatkın bir koordinat sistemi tanımı değil.");

    if (out.SetFromUserInput(wkt.c_str()) != OGRERR_NONE)
        return err(ErrorCode::ParseError,
                   "'" + sidecar + "' içindeki koordinat sistemi çözülemedi: " + gdal_reason());

    const char* authority = out.GetAuthorityName(nullptr);
    const char* code      = out.GetAuthorityCode(nullptr);
    if (authority && code) return std::string(authority) + ":" + code;
    const char* name = out.GetName();
    return name ? std::string(name) : std::string("WKT");
}

/// The CRS of a file as it now stands on disk, or an error when it carries none.
core::Result<std::string> reopen_crs(const std::string& path)
{
    ::CPLErrorReset();
    DatasetHandle probe(static_cast<GDALDataset*>(::GDALOpenEx(
        path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, allowed_driver_argv(), nullptr, nullptr)));
    if (!probe.ptr || probe.ptr->GetLayerCount() == 0)
        return err(ErrorCode::IoFailure, "yazılan dosya geri okunamadı: " + gdal_reason());

    const OGRSpatialReference* srs = probe.ptr->GetLayer(0)->GetSpatialRef();
    if (!srs) return err(ErrorCode::ValidationFailed, "yazılan dosya koordinat sistemi taşımıyor");

    const char* name = srs->GetName();
    return name ? std::string(name) : std::string("(adsız)");
}

/// Writes the ESRI-style `.prj` companion next to `path` and returns its name.
core::Result<std::string> write_prj_sidecar(const std::string& path, const OGRSpatialReference& srs)
{
    std::string sidecar = path;
    const auto dot      = sidecar.find_last_of('.');
    const auto slash    = sidecar.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) sidecar.erase(dot);
    sidecar += ".prj";

    // WKT1 morphed to the ESRI dialect, because that is what a .prj is and what
    // every GIS in the country reads back.
    OGRSpatialReference copy(srs);
    copy.morphToESRI();

    char* wkt = nullptr;
    if (copy.exportToWkt(&wkt) != OGRERR_NONE || !wkt) {
        if (wkt) ::CPLFree(wkt);
        return err(ErrorCode::Internal,
                   "Koordinat sistemi '" + sidecar + "' için WKT'ye çevrilemedi: " + gdal_reason());
    }

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

#endif // PIRICAD_HAVE_GDAL

} // namespace

// ------------------------------------------------------------ allow-list ----

const std::vector<VectorFormat>& vector_formats()
{
    static const std::vector<VectorFormat> list = parse_allow_list();
    return list;
}

const VectorFormat* vector_format_for_path(const std::string& path)
{
    for (const VectorFormat& f : vector_formats()) {
        if (f.extension.empty() || path.size() < f.extension.size()) continue;
        const std::string tail = path.substr(path.size() - f.extension.size());
        if (core::turkish_upper(tail) == core::turkish_upper(f.extension)) return &f;
    }
    return nullptr;
}

const VectorFormat* vector_format_by_id(const std::string& id)
{
    for (const VectorFormat& f : vector_formats())
        if (core::turkish_iequals(f.driver, id)) return &f;
    return nullptr;
}

bool vector_backend_available()
{
#ifdef PIRICAD_HAVE_GDAL
    return true;
#else
    return false;
#endif
}

std::string vector_backend_status()
{
    std::string names;
    for (const VectorFormat& f : vector_formats()) {
        if (!names.empty()) names += ", ";
        names += f.driver;
    }
    if (names.empty()) names = "(izin listesi boş)";

#ifdef PIRICAD_HAVE_GDAL
    return "Dış biçim desteği açık (GDAL). İzin verilen sürücüler: " + names + ".";
#else
    return "Dış biçim desteği KAPALI. Bu yapı PIRICAD_WITH_GDAL=OFF ile derlendi, bu yüzden "
           "İÇEAKTAR ve DIŞAAKTAR hata döndürür. Açmak için GDAL'ı kurun (Debian/Ubuntu: "
           "sudo apt install libgdal-dev, macOS: brew install gdal, vcpkg: 'gdal' özelliği) ve "
           "-DPIRICAD_WITH_GDAL=ON ile yapılandırın. İzin listesindeki sürücüler: " +
           names + ".";
#endif
}

// ---------------------------------------------------------------- import ----

command::Task<core::Result<VectorReport>> import_vector(command::Transaction& tx, std::string path,
                                                        std::string driver, std::string project_crs,
                                                        std::stop_token stop)
{
#ifndef PIRICAD_HAVE_GDAL
    (void)tx;
    (void)path;
    (void)driver;
    (void)project_crs;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  std::string(kErrNoDriver) + ": " + vector_backend_status());
#else
    ensure_registered();

    if (is_virtual_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' sanal dosya sistemi yolu. PiriCAD bir veri dosyasının ağdan ya da "
                          "arşivin içinden okunmasına izin vermez; dosyayı diske alıp yeniden "
                          "deneyin.");

    const VectorFormat* format =
        driver.empty() ? vector_format_for_path(path) : vector_format_by_id(driver);
    if (!format)
        co_return err(ErrorCode::Unsupported, std::string(kErrNoDriver) + ": '" + path +
                                                  "' için sürücü bulunamadı. " +
                                                  vector_backend_status());
    if (!format->read)
        co_return err(ErrorCode::Unsupported, std::string(kErrNoDriver) + ": " + format->driver +
                                                  " sürücüsü okuma için açık değil.");

    ::CPLErrorReset();
    DatasetHandle data(static_cast<GDALDataset*>(::GDALOpenEx(
        path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, allowed_driver_argv(), nullptr, nullptr)));
    if (!data.ptr)
        co_return err(ErrorCode::IoFailure,
                      "'" + path + "' açılamadı: " + gdal_reason() +
                          ". Dosyanın var olduğunu ve biçiminin izin listesinde olduğunu "
                          "denetleyin.");

    VectorReport report;
    report.driver = format->driver;

    // Read once, before the layer loop: a DXF carries no CRS of its own and every
    // layer in it would otherwise re-read the same sidecar.
    OGRSpatialReference sidecar_srs;
    std::string sidecar;
    if (auto found = sidecar_crs(path, sidecar_srs); found) sidecar = found.value();

    std::vector<core::Point2> points;
    std::vector<core::RingGeometry::RingInput> rings;
    std::vector<std::vector<core::Point2>> ring_store;

    for (int li = 0; li < data.ptr->GetLayerCount(); ++li) {
        OGRLayer* layer = data.ptr->GetLayer(li);
        if (!layer) continue;

        const std::string layer_name =
            layer->GetName() && *layer->GetName() ? layer->GetName() : "AKTARILAN";

        auto crs = crs_of(layer->GetSpatialRef(), layer_name);
        if (!crs && !sidecar.empty()) crs = sidecar; // the .prj companion, read above
        if (!crs)
            co_return err(
                crs.error().code,
                crs.error().message +
                    " (Aynı adlı bir .prj dosyasını yanına koyarak da bildirebilirsiniz.)");
        if (report.crs.empty())
            report.crs = crs.value();
        else if (report.crs != crs.value())
            co_return err(ErrorCode::ValidationFailed,
                          "'" + path +
                              "' içindeki katmanlar farklı koordinat sistemleri "
                              "bildiriyor (" +
                              report.crs + " ve " + crs.value() +
                              "). Tek bir sisteme dönüştürüp yeniden deneyin.");

        const core::LayerId slot = tx.ensure_layer(layer_name);
        if (slot == core::kNoLayer)
            co_return err(ErrorCode::ValidationFailed,
                          "'" + layer_name + "' katmanı oluşturulamadı.");
        ++report.layers;

        layer->ResetReading();
        while (OGRFeature* raw_feature = layer->GetNextFeature()) {
            // GetNextFeature hands over ownership, and every `continue` below
            // would leak it.
            const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> feature(
                raw_feature, [](OGRFeature* f) { OGRFeature::DestroyFeature(f); });

            if ((report.features % 4096) == 0 && stop.stop_requested())
                co_return err(ErrorCode::Cancelled, "İçe aktarma iptal edildi; çizim değişmedi.");
            ++report.features;

            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) continue;

            const OGRwkbGeometryType type = wkbFlatten(geometry->getGeometryType());
            rings.clear();
            ring_store.clear();

            const auto push_polygon = [&](const OGRPolygon* polygon, std::uint16_t part) {
                if (const OGRLinearRing* outer = polygon->getExteriorRing()) {
                    ring_store.emplace_back();
                    ring_to_mm(outer, ring_store.back());
                    rings.push_back(
                        core::RingGeometry::RingInput{{}, core::RingRole::Exterior, part});
                }
                for (int h = 0; h < polygon->getNumInteriorRings(); ++h) {
                    ring_store.emplace_back();
                    ring_to_mm(polygon->getInteriorRing(h), ring_store.back());
                    rings.push_back(
                        core::RingGeometry::RingInput{{}, core::RingRole::Interior, part});
                }
            };

            switch (type) {
            case wkbLineString:
                ring_store.emplace_back();
                line_to_mm(geometry->toLineString(), ring_store.back());
                rings.push_back(core::RingGeometry::RingInput{{}, core::RingRole::Open, 0});
                break;

            case wkbMultiLineString: {
                const OGRMultiLineString* multi = geometry->toMultiLineString();
                for (int g = 0; g < multi->getNumGeometries(); ++g) {
                    ring_store.emplace_back();
                    line_to_mm(multi->getGeometryRef(g), ring_store.back());
                    rings.push_back(core::RingGeometry::RingInput{
                        {}, core::RingRole::Open, static_cast<std::uint16_t>(g)});
                }
                break;
            }

            case wkbPolygon: push_polygon(geometry->toPolygon(), 0); break;

            case wkbMultiPolygon: {
                const OGRMultiPolygon* multi = geometry->toMultiPolygon();
                for (int g = 0; g < multi->getNumGeometries(); ++g)
                    push_polygon(multi->getGeometryRef(g), static_cast<std::uint16_t>(g));
                break;
            }

            default:
                // io.md P11/P13: not silently dropped, and not silently repaired
                // either. The note reaches the user through the command.
                if (report.notes.size() < 8)
                    report.notes.push_back(std::string("Desteklenmeyen geometri türü atlandı: ") +
                                           OGRGeometryTypeToName(type) +
                                           ". Bu sürüm çizgi ve alan okur.");
                continue;
            }

            // The spans are filled only now: `ring_store` reallocates while the
            // rings are being collected, and a span taken before the last push
            // would point at freed memory.
            for (std::size_t r = 0; r < rings.size(); ++r)
                rings[r].points = ring_store[r];

            const bool polyline = rings.size() == 1 && rings.front().role == core::RingRole::Open;
            auto added =
                polyline ? tx.add_polyline(slot, rings.front().points) : tx.add_area(slot, rings);
            if (!added)
                co_return err(added.error().code, "'" + path + "' içindeki " +
                                                      std::to_string(report.features) +
                                                      ". öğe okunamadı: " + added.error().message);
            ++report.entities;
        }
    }

    if (report.entities == 0)
        co_return err(ErrorCode::ValidationFailed,
                      "'" + path +
                          "' okunabilir çizgi ya da alan içermiyor; çizime hiçbir şey "
                          "eklenmedi.");

    // The drawing's own CRS is never changed by an import and the coordinates are
    // never reprojected: doing either silently is how a TM30 parcel ends up
    // plotted as a TM33 one. The mismatch is REPORTED and left for the user
    // (io.md R20, model.md R37a).
    if (!project_crs.empty() && project_crs != report.crs)
        report.notes.push_back("Dosyanın koordinat sistemi " + report.crs + ", çizimin ki " +
                               project_crs +
                               ". Koordinatlar dönüştürülmedi; AYAR koordinat_sistemi ile "
                               "denetleyin.");

    // model.md R27/R28: attributes belong in typed columns declared from /data,
    // and the Document has no attribute store yet. Saying so is the honest
    // answer; dropping them without a word would not be.
    report.notes.push_back("Öznitelikler bu sürümde okunmadı; belge modeli öznitelik "
                           "sütunlarını Faz 1'de kazanacak.");

    co_return report;
#endif
}

// ---------------------------------------------------------------- export ----

command::Task<core::Result<VectorReport>> export_vector(const core::Document& doc, std::string path,
                                                        std::string driver, std::string crs,
                                                        std::stop_token stop)
{
#ifndef PIRICAD_HAVE_GDAL
    (void)doc;
    (void)path;
    (void)driver;
    (void)crs;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  std::string(kErrNoDriver) + ": " + vector_backend_status());
#else
    ensure_registered();

    if (is_virtual_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' sanal dosya sistemi yolu. PiriCAD ağa ya da arşivin içine yazmaz.");

    const VectorFormat* format =
        driver.empty() ? vector_format_for_path(path) : vector_format_by_id(driver);
    if (!format)
        co_return err(ErrorCode::Unsupported, std::string(kErrNoDriver) + ": '" + path +
                                                  "' için sürücü bulunamadı. " +
                                                  vector_backend_status());
    if (!format->write)
        co_return err(ErrorCode::Unsupported,
                      std::string(kErrNoDriver) + ": " + format->driver +
                          " sürücüsü yazma için açık değil. DWG için DXF dışa aktarıp "
                          "dönüştürün (io.md R13).");

    GDALDriver* gdal_driver = ::GetGDALDriverManager()->GetDriverByName(format->driver.c_str());
    if (!gdal_driver)
        co_return err(ErrorCode::Unsupported,
                      std::string(kErrNoDriver) + ": GDAL '" + format->driver +
                          "' sürücüsünü tanımıyor. GDAL kurulumu bu sürücü olmadan "
                          "derlenmiş olabilir.");

    ::CPLErrorReset();
    ::VSIUnlink(path.c_str()); // a stale target makes GPKG refuse to create
    DatasetHandle data(gdal_driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    if (!data.ptr)
        co_return err(ErrorCode::IoFailure, "'" + path + "' oluşturulamadı: " + gdal_reason() +
                                                ". Dizin izinlerini ve boş alanı denetleyin.");

    if (crs.empty())
        co_return err(ErrorCode::ValidationFailed,
                      "Çizimin koordinat sistemi belirsiz. Etiketsiz koordinat dışa "
                      "aktarılamaz: AYAR koordinat_sistemi EPSG:5254 ile bildirin.");

    OGRSpatialReference srs;
    const std::string crs_id = crs;
    if (srs.SetFromUserInput(crs_id.c_str()) != OGRERR_NONE) {
        // io.md R20 again, from the writing side: an export with no usable CRS
        // produces a file whose coordinates mean nothing to the receiver.
        co_return err(ErrorCode::ValidationFailed,
                      "Çizimin koordinat sistemi '" + crs_id +
                          "' dışa aktarım için çözülemedi. AYAR koordinat_sistemi ile bir EPSG "
                          "kodu verin (örnek: EPSG:5254).");
    }
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    VectorReport report;
    report.driver = format->driver;
    report.crs    = crs_id;

    const core::EntityTable& ents = doc.entities();
    const core::RingGeometry& geo = doc.geometry();

    for (core::LayerId l = 0; l < doc.layers().size(); ++l) {
        const core::Layer& layer = doc.layers()[l];
        if (doc.layer_entity_count(l) == 0) continue;

        OGRLayer* out = data.ptr->CreateLayer(layer.name.c_str(), &srs, wkbUnknown, nullptr);
        if (!out)
            co_return err(ErrorCode::IoFailure,
                          "'" + layer.name + "' katmanı yazılamadı: " + gdal_reason());
        ++report.layers;

        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if ((e % 4096) == 0 && stop.stop_requested())
                co_return err(ErrorCode::Cancelled, "Dışa aktarma iptal edildi.");
            if (ents.layer[e] != l || !ents.alive(e)) continue;

            const core::RingSpan span = geo.rings_of(ents.slot[e]);
            if (span.count == 0) continue;

            std::unique_ptr<OGRGeometry> geometry;
            if (span.count == 1 && geo.ring_role[span.first] == core::RingRole::Open) {
                auto line     = std::make_unique<OGRLineString>();
                const auto xs = geo.ring_xs(span.first);
                const auto ys = geo.ring_ys(span.first);
                for (std::size_t v = 0; v < xs.size(); ++v)
                    line->addPoint(core::mm_to_metres(xs[v]), core::mm_to_metres(ys[v]));
                geometry = std::move(line);
            } else {
                auto polygon = std::make_unique<OGRPolygon>();
                for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                    if (geo.ring_role[r] == core::RingRole::Open) continue;
                    OGRLinearRing ring;
                    const auto xs = geo.ring_xs(r);
                    const auto ys = geo.ring_ys(r);
                    for (std::size_t v = 0; v < xs.size(); ++v)
                        ring.addPoint(core::mm_to_metres(xs[v]), core::mm_to_metres(ys[v]));
                    ring.closeRings(); // OGR wants the repeated closing vertex back
                    polygon->addRing(&ring);
                }
                if (polygon->getExteriorRing() == nullptr) continue;
                geometry = std::move(polygon);
            }

            const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> feature(
                OGRFeature::CreateFeature(out->GetLayerDefn()),
                [](OGRFeature* f) { OGRFeature::DestroyFeature(f); });
            feature->SetGeometry(geometry.get());

            if (out->CreateFeature(feature.get()) != OGRERR_NONE)
                co_return err(ErrorCode::IoFailure,
                              "'" + layer.name + "' katmanına öğe yazılamadı: " + gdal_reason());
            ++report.features;
            ++report.entities;
        }
    }

    if (report.features == 0)
        co_return err(ErrorCode::ValidationFailed,
                      "Çizimde dışa aktarılacak nesne yok; '" + path + "' yazılmadı.");

    // Flush and close before the check below: nothing is on disk until GDAL says
    // so, and a driver that buffers would otherwise be verified against a file
    // that does not exist yet.
    ::GDALClose(data.ptr);
    data.ptr = nullptr;

    // io.md R20, enforced on what was ACTUALLY WRITTEN rather than on what we
    // asked for. DXF has no slot for a coordinate system at all, so a bare .dxf
    // is an unlabelled dataset — and this module refuses to READ one of those,
    // which would make its own export unreadable. The answer is the sidecar every
    // GIS in the country already understands: <ad>.prj beside the drawing.
    //
    // The check is a read-back rather than a list of driver names, so a driver
    // that gains CRS support stops getting a sidecar without anyone editing a
    // table here.
    if (auto reopened = reopen_crs(path); !reopened) {
        auto sidecar = write_prj_sidecar(path, srs);
        if (!sidecar) co_return sidecar.error();
        report.notes.push_back(format->driver + " biçimi koordinat sistemi taşımaz; sistem '" +
                               sidecar.value() +
                               "' dosyasına yazıldı. Çizimi taşırken bu dosyayı da götürün, "
                               "yoksa koordinatlar etiketsiz kalır.");
    }

    report.notes.push_back("Öznitelik ve stil bilgisi bu sürümde yazılmadı; yalnız geometri "
                           "ve katman adı aktarıldı.");
    co_return report;
#endif
}

} // namespace piricad::io
