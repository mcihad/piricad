// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: external vector formats through GDAL/OGR.
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
// WHEN KENTOS_WITH_GDAL IS OFF everything below still compiles and every entry
// point returns an Error naming the option, the package and the install command.
// data.md Enforcement: a gated capability reports itself, it never quietly
// succeeds and it never quietly reports nothing.
#include "kentos_cad/io/vector.hpp"

#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/format.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef KENTOS_HAVE_GDAL
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#endif

namespace kentos::io {
namespace {

using core::err;
using core::ErrorCode;

/// The allow-list, as /cmake/KentOSCadGdalDrivers.cmake declared it. Format:
///   DRIVER:.ext:modes:Türkçe etiket|DRIVER:...
/// where modes is any of "r", "w", "rw". The separator is a vertical bar because
/// a semicolon is a CMake list separator and would not survive the definition.
#ifndef KENTOS_GDAL_DRIVERS
#define KENTOS_GDAL_DRIVERS ""
#endif

std::vector<VectorFormat> parse_allow_list()
{
    std::vector<VectorFormat> out;
    const std::string spec = KENTOS_GDAL_DRIVERS;

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

#ifdef KENTOS_HAVE_GDAL

/// GDAL's virtual filesystem prefixes reach the network, an archive or another
/// process's memory. io.md P14: a path that arrived in a command argument, a
/// script or an AI suggestion never gets to be one of those.
///
/// Inside the guard because it guards GDAL and nothing else: without the backend
/// there is no path to protect, and a function nobody calls is a warning in a
/// build that treats warnings as defects (CLAUDE.md 6.3).
bool is_virtual_path(const std::string& path)
{
    return path.rfind("/vsi", 0) == 0;
}

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

/// The character height an OGR LABEL style declares, in ground millimetres.
///
/// GDAL's DXF driver does not expose a text height FIELD — the fields are Layer,
/// PaperSpace, SubClasses, Linetype, EntityHandle and Text — so the only place
/// the size survives is the style string:
///
///     LABEL(f:"Arial",t:"Parsel 12",p:1,s:2.5g,c:#000000)
///
/// `g` is ground units, `p` points, `mm` millimetres on paper. Only ground is
/// read: a cadastral sheet's parcel numbers are a GROUND height — that is what
/// makes them grow and shrink with the plot scale the way the regulation expects
/// — and a paper height imported as a ground one would put a four-metre number
/// on a parcel. Anything else falls back, and the fallback is stated rather than
/// guessed at the call site.
core::Mm label_height_mm(const char* style)
{
    if (style == nullptr) return 0;

    const std::string_view text(style);
    const std::size_t at = text.find("s:");
    if (at == std::string_view::npos) return 0;

    // Compared against the digit range rather than asked of `<cctype>`: the
    // classifiers are banned outright in this tree (CLAUDE.md 5.6) because they
    // are wrong on Turkish text, and a number needs no classifier anyway.
    std::size_t end = at + 2;
    while (end < text.size() &&
           ((text[end] >= '0' && text[end] <= '9') || text[end] == '.' || text[end] == '-'))
        ++end;

    // GROUND ONLY. `s:2.5g` is 2,5 m of ground; `s:10pt` is ten points on paper
    // and means nothing to a document that stores ground millimetres.
    if (end >= text.size() || text[end] != 'g') return 0;

    double value = 0.0;
    const std::string number(text.substr(at + 2, end - at - 2));
    try {
        value = std::stod(number);
    } catch (...) {
        return 0;
    }
    return value > 0.0 ? core::mm_from_metres(value) : 0;
}

/// The CRS in the `.prj` companion beside `path`, when there is one.
///
/// GDAL's DXF driver does NOT read a `.prj` — verified against GDAL 3.12, where a
/// DXF with a correct sidecar still reports `Layer SRS WKT: (unknown)`. DXF has no
/// slot for a coordinate system at all, so without this every DXF would be an
/// unlabelled dataset and io.md R20 would make the format unimportable. KentOSCad
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

#endif // KENTOS_HAVE_GDAL

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
#ifdef KENTOS_HAVE_GDAL
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

#ifdef KENTOS_HAVE_GDAL
    return "Dış biçim desteği açık (GDAL). İzin verilen sürücüler: " + names + ".";
#else
    return "Dış biçim desteği KAPALI. Bu yapı KENTOS_WITH_GDAL=OFF ile derlendi, bu yüzden "
           "İÇEAKTAR ve DIŞAAKTAR hata döndürür. Açmak için GDAL'ı kurun (Debian/Ubuntu: "
           "sudo apt install libgdal-dev, macOS: brew install gdal, vcpkg: 'gdal' özelliği) ve "
           "-DKENTOS_WITH_GDAL=ON ile yapılandırın. İzin listesindeki sürücüler: " +
           names + ".";
#endif
}

// ---------------------------------------------------------------- import ----

command::Task<core::Result<VectorReport>> import_vector(command::Transaction& tx, std::string path,
                                                        std::string driver, std::string project_crs,
                                                        std::vector<std::string> only,
                                                        std::stop_token stop)
{
#ifndef KENTOS_HAVE_GDAL
    (void)tx;
    (void)path;
    (void)driver;
    (void)project_crs;
    (void)only;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  std::string(kErrNoDriver) + ": " + vector_backend_status());
#else
    ensure_registered();

    if (is_virtual_path(path))
        co_return err(ErrorCode::InvalidArgument,
                      "'" + path +
                          "' sanal dosya sistemi yolu. KentOSCad bir veri dosyasının ağdan ya da "
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

    // A SHAPEFILE IS FOUR FILES, and saying so is this program's job rather than
    // GDAL's. Without the index GDAL refuses with
    //
    //     Unable to open parsel.shx ... Set SHAPE_RESTORE_SHX config option to YES
    //
    // which tells a surveyor to set an environment variable they have never heard
    // of, in English, about a file they did not know existed. What actually
    // happened is that the set arrived incomplete — someone e-mailed the `.shp`
    // alone, or a zip lost a member — and that is what the message should say.
    if (format->driver == "ESRI Shapefile") {
        const std::string base = path.substr(0, path.size() - 4);
        for (const auto& [suffix, why] :
             {std::pair<const char*, const char*>{".shx", "geometri dizini"},
              std::pair<const char*, const char*>{".dbf", "öznitelik tablosu"}}) {
            std::error_code ec;
            if (std::filesystem::exists(base + suffix, ec)) continue;

            const std::string upper = base + core::turkish_upper(suffix);
            if (std::filesystem::exists(upper, ec)) continue;

            co_return err(ErrorCode::IoFailure,
                          "Shapefile eksik: '" + base + suffix + "' (" + why +
                              ") yok. Bir shapefile TEK dosya değildir — .shp, .shx, .dbf ve "
                              ".prj birlikte taşınır. Dosyayı gönderene dördünü de isteyin.");
        }
    }

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
    std::set<std::string> seen_layers; // so report.layers counts names, not features
    bool unlabelled = false;           // the "no CRS in the file" note, said once

    // The wizard's tick boxes, and nothing else in this file knows they exist:
    // an empty list is "everything", which is what a bare İÇEAKTAR sends. Folded
    // per comparison rather than pre-folded once because the list is a handful of
    // names and the comparison happens once per layer, not once per feature —
    // except on DXF, where the layer is a FIELD, so the per-feature answer is
    // memoised in `decided` below.
    const auto wanted = [&only](const std::string& name) {
        if (only.empty()) return true;
        for (const std::string& pick : only)
            if (core::turkish_iequals(pick, name)) return true;
        return false;
    };
    std::map<std::string, bool> decided;

    // Every layer the file holds, whether or not it was read. The wizard's list
    // is built from exactly this on the probe pass.
    const auto tally = [&report](const std::string& name, std::uint64_t made) {
        for (auto& row : report.layer_names)
            if (row.first == name) {
                row.second += made;
                return;
            }
        report.layer_names.emplace_back(name, made);
    };

    for (int li = 0; li < data.ptr->GetLayerCount(); ++li) {
        OGRLayer* layer = data.ptr->GetLayer(li);
        if (!layer) continue;

        const std::string layer_name =
            layer->GetName() && *layer->GetName() ? layer->GetName() : "AKTARILAN";

        auto crs = crs_of(layer->GetSpatialRef(), layer_name);
        if (!crs && !sidecar.empty()) crs = sidecar; // the .prj companion, read above

        // A DXF HAS NOWHERE TO PUT A COORDINATE SYSTEM, and no surveying office
        // ships a `.prj` beside one. Refusing every such file is not R20's rule —
        // which is that an unlabelled coordinate must never be read SILENTLY —
        // it is a refusal of the format itself, and it made real cadastral
        // drawings unopenable.
        //
        // So the drawing's own system stands, and it is announced rather than
        // assumed quietly: the note goes into the report, which the command puts
        // in front of the user. The danger R20 guards against is a TM30 parcel
        // plotted as a TM33 one, and that danger is in the SILENCE, not in the
        // fallback. Nothing is inferred from the coordinates — an easting of
        // 583 000 fits several Turkish zones and guessing between them is exactly
        // the blunder the rule exists for.
        //
        // To state it explicitly, set the drawing's system before importing:
        //     AYAR koordinat_sistemi EPSG:5256
        if (!crs && !project_crs.empty()) {
            crs = project_crs;
            if (!unlabelled) {
                unlabelled = true;
                report.notes.push_back(
                    "Dosya koordinat sistemi bildirmiyor (DXF taşıyamaz). Çizimin kendi "
                    "sistemi varsayıldı: " +
                    project_crs +
                    ". Yanlışsa GERİAL ile geri alın, AYAR koordinat_sistemi ile doğrusunu "
                    "kurun ve yeniden aktarın.");
            }
        }

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

        // The other half of the DXF single-layer story. On the way out we write
        // every drawing layer into the one OGR layer DXF allows and carry the name
        // in a `Layer` attribute; on the way back in, the OGR layer is called
        // `entities` and the names are on the features. Reading the OGR layer name
        // alone would land a whole cadastral drawing in one layer called
        // `entities` — the export would look fixed and the import would still lose
        // the drawing's structure.
        const int layer_field = layer->GetLayerDefn()->GetFieldIndex("Layer");

        // A DXF TEXT arrives as a POINT carrying its string in this field. Read
        // once per layer, like the one above: the lookup is a linear scan of the
        // definition and doing it per feature is the sort of thing that turns a
        // fast import into a slow one on a sheet with a hundred thousand parcel
        // numbers on it.
        const int text_field = layer->GetLayerDefn()->GetFieldIndex("Text");

        core::LayerId slot = core::kNoLayer;
        if (layer_field < 0) {
            // Registered before the tick box is consulted: the wizard's list has
            // to show a layer in order for anyone to be able to tick it.
            tally(layer_name, 0);
            if (!wanted(layer_name)) continue;

            slot = tx.ensure_layer(layer_name);
            if (slot == core::kNoLayer)
                co_return err(ErrorCode::ValidationFailed,
                              "'" + layer_name + "' katmanı oluşturulamadı.");
            ++report.layers;
        }

        // Where the per-layer entity counts come from. `report.entities` only
        // grows, so the run of features belonging to one layer is the delta
        // between two marks — and a DXF interleaves its layers freely, which is
        // why this flushes on every change rather than once at the end.
        std::string open_layer  = layer_field < 0 ? layer_name : std::string();
        std::uint64_t open_mark = report.entities;

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

            core::LayerId target = slot;
            if (layer_field >= 0) {
                const char* named = feature->IsFieldSetAndNotNull(layer_field)
                                        ? feature->GetFieldAsString(layer_field)
                                        : nullptr;
                const std::string want =
                    named && *named ? std::string(named) : std::string(layer_name);

                if (want != open_layer) {
                    if (!open_layer.empty()) tally(open_layer, report.entities - open_mark);
                    open_layer = want;
                    open_mark  = report.entities;
                    tally(want, 0);
                }

                // Memoised: this runs once per FEATURE, and a sheet has a hundred
                // thousand of them against a checklist of forty names.
                auto seen = decided.find(want);
                if (seen == decided.end()) seen = decided.emplace(want, wanted(want)).first;
                if (!seen->second) continue;

                target = tx.ensure_layer(want);
                if (target == core::kNoLayer)
                    co_return err(ErrorCode::ValidationFailed,
                                  "'" + want + "' katmanı oluşturulamadı.");
                if (!seen_layers.contains(want)) {
                    seen_layers.insert(want);
                    ++report.layers;
                }
            }

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

            // A POINT IS A POINT, AND A POINT WITH A STRING IS A CAPTION.
            //
            // Both used to fall through to the note below, so a cadastral DXF came
            // in with its parcels and without its nirengi, its röpers or a single
            // parsel number — which is most of what makes the sheet readable.
            if (type == wkbPoint) {
                const OGRPoint* p = geometry->toPoint();
                const core::Point2 where{core::mm_from_metres(p->getX()),
                                         core::mm_from_metres(p->getY())};

                const char* label = text_field >= 0 && feature->IsFieldSetAndNotNull(text_field)
                                        ? feature->GetFieldAsString(text_field)
                                        : nullptr;

                if (label != nullptr && *label != 0) {
                    // TEXT IS A BASELINE PLUS A STRING, exactly as the `METİN`
                    // command builds one — two real vertices, so the cull, the
                    // snap and the hit test need to know nothing about text.
                    core::Mm height = label_height_mm(feature->GetStyleString());
                    if (height <= 0) {
                        // The file did not say. A metre is the height a 1/1000
                        // cadastral sheet prints a parcel number at, and it is
                        // REPORTED rather than left for the user to discover by
                        // measuring one.
                        height = core::mm_from_metres(1.0);
                        if (report.notes.size() < 8)
                            report.notes.push_back("Yazı yüksekliği dosyada yok; 1 m varsayıldı.");
                    }

                    // A rough advance of 0.6 em per character, which decides the
                    // BOUNDING BOX and not where a glyph lands. Same approximation
                    // `METİN` makes, and for the same reason.
                    core::Point2 end = where;
                    end.x += (height * 6 * static_cast<core::Mm>(std::strlen(label))) / 10;

                    const std::array<core::Point2, 2> baseline{where, end};
                    auto made = tx.add_polyline(target, baseline);
                    if (!made)
                        co_return err(made.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". öğe okunamadı: " + made.error().message);

                    if (auto st = tx.set_text(made.value(), label, height,
                                              core::TextAnchor::BaselineLeft);
                        !st)
                        co_return err(st.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". yazı yazılamadı: " + st.error().message);
                } else {
                    auto made = tx.add_point(target, where);
                    if (!made)
                        co_return err(made.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". öğe okunamadı: " + made.error().message);
                }
                ++report.entities;
                continue;
            }

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

            // A CLOSED LINESTRING IS A FACE, and this is the single biggest thing
            // a cadastral DXF loses without it.
            //
            // DXF has no polygon: a parcel is an LWPOLYLINE with its closed flag
            // set, and OGR hands that over as a `LINESTRING` whose last vertex
            // repeats its first. Read as an open run — which is what happened —
            // every parcel in the file came in as a LINE: no fill, no area to
            // measure, nothing for `İFRAZ` or `TEVHİT` to work on, and a topology
            // check that saw no faces at all.
            //
            // The test is the geometry's own: first vertex equal to last, and at
            // least three distinct corners left after the duplicate is dropped.
            // Nothing is inferred from the layer's name or the file's extension.
            if (rings.size() == 1 && rings.front().role == core::RingRole::Open &&
                ring_store.front().size() >= 4 &&
                ring_store.front().front() == ring_store.front().back()) {
                // AND AT LEAST THREE DISTINCT CORNERS. The vertex COUNT is not the
                // test: a real drawing carries runs of repeated points — one file
                // holds a fifty-five vertex LINESTRING whose vertices are all the
                // same coordinate — and such a run passes "first equals last"
                // trivially. Calling it a face hands `add_area` a ring that
                // collapses to a single vertex, which it rightly refuses.
                std::size_t corners = 0;
                for (std::size_t v = 0; v + 1 < ring_store.front().size(); ++v)
                    if (ring_store.front()[v] != ring_store.front()[v + 1]) ++corners;

                if (corners >= 3) {
                    // The closing vertex is IMPLIED, never stored (model.md R10) —
                    // the same rule `ring_to_mm` applies to a polygon's rings.
                    ring_store.front().pop_back();
                    rings.front().role = core::RingRole::Exterior;
                }
            }

            const bool polyline = rings.size() == 1 && rings.front().role == core::RingRole::Open;
            auto added          = polyline ? tx.add_polyline(target, rings.front().points)
                                           : tx.add_area(target, rings);
            if (!added) {
                // SKIPPED, COUNTED AND NAMED — not thrown, and this is the whole
                // difference between a reader and a validator. One unusable
                // feature in a 48 MB drawing used to abort the import and roll
                // back 18 497 sound entities; the user was told the file could not
                // be read, which was true of one line of it.
                //
                // The DWG reader already answers this way for an entity type it
                // has no translation for, and io.md P11/P13 asks for exactly this:
                // a loss is reported, never silent.
                ++report.skipped;
                if (report.skipped_reason.empty()) report.skipped_reason = added.error().message;
                continue;
            }
            ++report.entities;
        }

        if (!open_layer.empty()) tally(open_layer, report.entities - open_mark);
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
#ifndef KENTOS_HAVE_GDAL
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
                          "' sanal dosya sistemi yolu. KentOSCad ağa ya da arşivin içine yazmaz.");

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

    // DXF holds exactly ONE OGR layer, named `entities`; a drawing's layers live
    // there as a `Layer` attribute on each feature. Asking OGR for a second layer
    // fails with "Unable to have more than one OGR entities layer in a DXF file",
    // and before this the export wrote the first KentOSCad layer and then stopped —
    // a cadastral DXF with one layer in it is not a cadastral DXF.
    //
    // This is a fact about a GDAL driver, not about a regulation, so it lives in
    // /src/io next to the code it governs. If a second single-layer driver is ever
    // allow-listed, this moves into cmake/KentOSCadGdalDrivers.cmake as a field —
    // that file is where per-driver facts belong.
    const bool one_layer_only = format->driver == "DXF";

    OGRLayer* shared = nullptr;
    if (one_layer_only) {
        shared = data.ptr->CreateLayer("entities", &srs, wkbUnknown, nullptr);
        if (!shared)
            co_return err(ErrorCode::IoFailure,
                          "'" + path + "' içinde katman oluşturulamadı: " + gdal_reason());

        OGRFieldDefn field("Layer", OFTString);
        field.SetWidth(255);
        if (shared->CreateField(&field) != OGRERR_NONE)
            co_return err(ErrorCode::IoFailure,
                          "'" + path +
                              "' içinde katman adı alanı oluşturulamadı: " + gdal_reason());
    }

    for (core::LayerId l = 0; l < doc.layers().size(); ++l) {
        const core::Layer& layer = doc.layers()[l];
        if (doc.layer_entity_count(l) == 0) continue;

        OGRLayer* out = shared;
        if (out == nullptr) {
            out = data.ptr->CreateLayer(layer.name.c_str(), &srs, wkbUnknown, nullptr);
            if (!out)
                co_return err(ErrorCode::IoFailure,
                              "'" + layer.name + "' katmanı yazılamadı: " + gdal_reason());
        }
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
            if (one_layer_only) feature->SetField("Layer", layer.name.c_str());

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

} // namespace kentos::io
