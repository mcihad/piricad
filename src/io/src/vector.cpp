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

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/format.hpp"

#include "dxf_units.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
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
void ring_to_mm(const OGRLinearRing* ring, std::vector<core::Point2>& out, core::DrawingUnit unit)
{
    out.clear();
    const int n = ring->getNumPoints();
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(core::Point2{core::mm_from_drawing_units(ring->getX(i), unit),
                                   core::mm_from_drawing_units(ring->getY(i), unit)});

    // RingGeometry stores the corners and implies the closing segment, so the
    // duplicate OGR always writes is dropped here rather than argued about there.
    while (out.size() >= 2 && out.back() == out.front())
        out.pop_back();
}

void line_to_mm(const OGRLineString* line, std::vector<core::Point2>& out, core::DrawingUnit unit)
{
    out.clear();
    const int n = line->getNumPoints();
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(core::Point2{core::mm_from_drawing_units(line->getX(i), unit),
                                   core::mm_from_drawing_units(line->getY(i), unit)});
}

/// Whether any vertex of `g` sits off the ground plane. A DXF is three-
/// dimensional and this document is not: a height that arrives is DROPPED, and
/// the drop is said (io.md P11) rather than discovered when a surveyed kot is
/// missing from every point.
bool carries_height(const OGRGeometry* g)
{
    if (g == nullptr || !g->Is3D()) return false;
    switch (wkbFlatten(g->getGeometryType())) {
    case wkbPoint: return g->toPoint()->getZ() != 0.0;
    case wkbLineString: {
        const OGRLineString* line = g->toLineString();
        for (int i = 0; i < line->getNumPoints(); ++i)
            if (line->getZ(i) != 0.0) return true;
        return false;
    }
    case wkbPolygon: {
        const OGRPolygon* polygon = g->toPolygon();
        if (const OGRLinearRing* outer = polygon->getExteriorRing(); carries_height(outer))
            return true;
        for (int h = 0; h < polygon->getNumInteriorRings(); ++h)
            if (carries_height(polygon->getInteriorRing(h))) return true;
        return false;
    }
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbMultiPoint:
    case wkbGeometryCollection: {
        const OGRGeometryCollection* many = g->toGeometryCollection();
        for (int i = 0; i < many->getNumGeometries(); ++i)
            if (carries_height(many->getGeometryRef(i))) return true;
        return false;
    }
    default: return false;
    }
}

/// The fields this program writes beside a GIS feature and reads back from its
/// own output: what the entity IS, its persistent key, and a caption's four
/// facts. A GeoPackage has no notion of a circle or a text; these are how one
/// written here comes back as one. Never written to a DXF, which has no fields.
constexpr const char* kFieldKind   = "tur";
constexpr const char* kFieldKey    = "anahtar";
constexpr const char* kFieldText   = "yazi";
constexpr const char* kFieldHeight = "yukseklik_mm";
constexpr const char* kFieldAngle  = "aci";
constexpr const char* kFieldAnchor = "hizalama";

bool is_own_field(std::string_view name)
{
    return name == kFieldKind || name == kFieldKey || name == kFieldText || name == kFieldHeight ||
           name == kFieldAngle || name == kFieldAnchor;
}

/// The anchor a `hizalama` field names, or the baseline-left default.
core::TextAnchor anchor_from_name(std::string_view name)
{
    for (const core::TextAnchor a :
         {core::TextAnchor::BaselineLeft, core::TextAnchor::BaselineCentre,
          core::TextAnchor::BaselineRight, core::TextAnchor::MiddleCentre})
        if (name == core::text_anchor_name(a)) return a;
    return core::TextAnchor::BaselineLeft;
}

/// The DXF's own name for an entity, from the class chain OGR hands over in the
/// `SubClasses` field — `AcDbEntity:AcDbCircle:AcDbArc` is an ARC. The census the
/// import prints is in these names, because they are the names a CAD user knows
/// and the names R14's coverage report counts in.
std::string dxf_type_name(std::string_view chain)
{
    const std::size_t colon = chain.rfind(':');
    std::string_view tail   = colon == std::string_view::npos ? chain : chain.substr(colon + 1);
    if (tail.rfind("AcDb", 0) == 0) tail.remove_prefix(4);

    if (tail == "Polyline") return "LWPOLYLINE";
    if (tail == "2dPolyline" || tail == "3dPolyline") return "POLYLINE";
    if (tail == "BlockReference") return "INSERT";
    if (tail == "Face") return "3DFACE";
    if (tail.find("Dimension") != std::string_view::npos) return "DIMENSION";
    if (tail.empty()) return "?";

    // LINE, CIRCLE, ARC, ELLIPSE, SPLINE, HATCH, TEXT, MTEXT, POINT, LEADER…
    std::string out;
    out.reserve(tail.size());
    for (const char ch : tail)
        out.push_back(ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch);
    return out;
}

/// One parameter's value out of an OGR style string, or empty when it is absent.
///
/// GDAL hands a DXF's text over as a style string and nowhere else:
///
///     LABEL(f:"Arial",t:"Yol boyu",p:1,a:37.5,s:2.5g,c:#000000)
///
/// SCANNED RATHER THAN SEARCHED FOR, and the difference is a real defect and not
/// a nicety: the caption's own text is one of the parameters, so a plain
/// `find("a:")` matches inside `t:"Ada: 12"` and reads the parcel's own label as
/// an angle. The scan walks parameter by parameter from the opening bracket and
/// steps over a quoted value whole, so the only `a:` it can see is a parameter
/// name.
std::string_view style_value(const char* style, std::string_view key)
{
    if (style == nullptr) return {};

    const std::string_view text(style);
    std::size_t at = text.find('(');
    if (at == std::string_view::npos) return {};
    ++at;

    while (at < text.size() && text[at] != ')') {
        const std::size_t colon = text.find(':', at);
        if (colon == std::string_view::npos) return {};

        const std::string_view name = text.substr(at, colon - at);

        // The value, to the next top-level comma. A quoted one is stepped over
        // whole, backslash escapes included, so a comma or a bracket inside a
        // caption ends nothing.
        std::size_t end = colon + 1;
        if (end < text.size() && text[end] == '"') {
            ++end;
            while (end < text.size() && text[end] != '"') {
                if (text[end] == '\\' && end + 1 < text.size()) ++end;
                ++end;
            }
            if (end < text.size()) ++end; // the closing quote
        } else {
            while (end < text.size() && text[end] != ',' && text[end] != ')')
                ++end;
        }

        if (name == key) {
            std::string_view value = text.substr(colon + 1, end - colon - 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            return value;
        }

        at = end;
        while (at < text.size() && (text[at] == ',' || text[at] == ' '))
            ++at;
    }

    return {};
}

/// A leading decimal number, or nothing. `std::stod` on a scanned parameter.
///
/// Compared against the digit range rather than asked of `<cctype>`: the
/// classifiers are banned outright in this tree (CLAUDE.md 5.6) because they are
/// wrong on Turkish text, and a number needs no classifier anyway.
std::optional<double> leading_number(std::string_view value, std::string_view unit)
{
    std::size_t end = 0;
    if (end < value.size() && (value[end] == '-' || value[end] == '+')) ++end;
    while (end < value.size() && ((value[end] >= '0' && value[end] <= '9') || value[end] == '.' ||
                                  value[end] == 'e' || value[end] == 'E' ||
                                  ((value[end] == '-' || value[end] == '+') && end > 0 &&
                                   (value[end - 1] == 'e' || value[end - 1] == 'E'))))
        ++end;
    if (end == 0) return std::nullopt;

    // The unit the caller insists on, exactly: `s:2.5g` is ground, `s:10pt` is
    // paper, and the two are not interchangeable.
    if (value.substr(end) != unit) return std::nullopt;

    try {
        return std::stod(std::string(value.substr(0, end)));
    } catch (...) {
        return std::nullopt;
    }
}

/// The character height an OGR LABEL style declares, in ground millimetres.
///
/// GDAL's DXF driver does not expose a text height FIELD — the fields are Layer,
/// PaperSpace, SubClasses, Linetype, EntityHandle and Text — so the only place
/// the size survives is the style string.
///
/// `g` is ground units, `p` points, `mm` millimetres on paper. Only ground is
/// read: a cadastral sheet's parcel numbers are a GROUND height — that is what
/// makes them grow and shrink with the plot scale the way the regulation expects
/// — and a paper height imported as a ground one would put a four-metre number
/// on a parcel. Anything else falls back, and the fallback is stated rather than
/// guessed at the call site.
core::Mm label_height_mm(const char* style, core::DrawingUnit unit)
{
    const std::optional<double> value = leading_number(style_value(style, "s"), "g");
    return value && *value > 0.0 ? core::mm_from_drawing_units(*value, unit) : 0;
}

/// The rotation an OGR LABEL style declares, in degrees counter-clockwise.
///
/// WHY IT MATTERS ON A PLAN. A DXF caption carries an angle (group code 50) and a
/// planner uses it: street names run along the street, parcel numbers along the
/// parcel, ada numbers along the block. Dropped, every one of those comes in
/// horizontal — 1 412 of the 13 112 captions in a 48 MB zoning DXF — and a label
/// that was laid along a road crosses it instead. Nothing else in the file says
/// which way a caption faces.
double label_angle_deg(const char* style)
{
    const std::optional<double> value = leading_number(style_value(style, "a"), "");
    return value ? *value : 0.0;
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

namespace {

/// The circle a run of points lies on, or nothing.
///
/// OGR hands a DXF CIRCLE and ARC over already tessellated, so the centre and the
/// radius have to be recovered from points that sit ON the curve. Three of them
/// determine it exactly — the circumcentre of the triangle they form — and the
/// three are taken a third of the way apart so a nearly straight arc does not
/// produce a nearly degenerate triangle.
///
/// AND THEN IT IS CHECKED. Every vertex is measured against the fitted circle and
/// the fit is refused if any of them is off by more than a millimetre, which is
/// the storage unit: a drawing's numbers are not the place for a shape that
/// almost fits. A refused fit falls back to the polyline the tessellation already
/// is — worse fidelity, never a wrong figure.
/// pi as a literal, for the reason `core/entity_kind.cpp` gives: no libm call, so
/// every platform multiplies the same doubles in the same order (Article 2.5).
constexpr double kPi = 3.14159265358979323846;

struct FittedCircle
{
    core::Point2 centre{};
    core::Mm radius{0};
};

/// How far a vertex may sit off the fitted circle. The storage unit is a
/// millimetre, so every vertex OGR strokes carries up to √2/2 mm of rounding
/// noise against the true circle; the fitted centre carries a fraction of that
/// again, worst on a short arc where the points span a small angle. One and a
/// half millimetres admits that noise and nothing else: a stroked ellipse or a
/// polyline that merely resembles a circle misses by centimetres.
constexpr double kFitTolerance = 1.5;

std::optional<FittedCircle> fit_circle(const std::vector<core::Point2>& pts)
{
    if (pts.size() < 5) return std::nullopt;

    // TRANSLATED TO THE FIRST VERTEX BEFORE ANY MULTIPLY, and this is the whole
    // difference between a circle and a sixty-four-sided polygon. A TUREF
    // northing is 4 448 000 000 millimetres; the circumcentre determinant squares
    // it, which lands at 2e19 — past the 9e15 where a double still counts by ones.
    // The cancellation that follows is total, and the residual check then refused
    // perfectly good circles while letting others through by luck: two adjacent
    // eight-metre circles in the same file came out one as a circle and one as a
    // polygon. `ring_area` translates for exactly this reason.
    const core::Point2 origin = pts.front();
    const auto at             = [&](std::size_t i) {
        return std::pair<double, double>{static_cast<double>(pts[i].x - origin.x),
                                         static_cast<double>(pts[i].y - origin.y)};
    };

    // EVERY VERTEX VOTES, not three of them. A circumcentre through three
    // vertices is exact for three exact points and nothing else: each stroked
    // vertex arrives rounded to the millimetre, and on a short arc — a 90° kerb
    // return of five metres — three rounded points span so little of the circle
    // that their circumcentre wanders a millimetre or more, and the residual
    // check then refused the arc that a whole town's kerbs are made of. The
    // algebraic least-squares fit (Kåsa) averages that noise away over all the
    // vertices, in one fixed summation order, with no library call but a square
    // root — so every platform agrees on the millimetre it lands on.
    const auto n = static_cast<double>(pts.size());
    double mx = 0.0, my = 0.0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto [px, py] = at(i);
        mx += px;
        my += py;
    }
    mx /= n;
    my /= n;

    double suu = 0.0, suv = 0.0, svv = 0.0, suuu = 0.0, svvv = 0.0, suvv = 0.0, svuu = 0.0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto [px, py] = at(i);
        const double u      = px - mx;
        const double v      = py - my;
        suu += u * u;
        suv += u * v;
        svv += v * v;
        suuu += u * u * u;
        svvv += v * v * v;
        suvv += u * v * v;
        svuu += v * u * u;
    }

    // The normal equations; the determinant vanishes exactly when the vertices
    // are collinear, which is the one case this cannot answer.
    const double det = suu * svv - suv * suv;
    if (std::abs(det) < 1e-9) return std::nullopt;
    const double rhs_u = (suuu + suvv) / 2.0;
    const double rhs_v = (svvv + svuu) / 2.0;
    const double uc    = (rhs_u * svv - rhs_v * suv) / det;
    const double vc    = (suu * rhs_v - suv * rhs_u) / det;
    const double ux    = uc + mx;
    const double uy    = vc + my;
    const double r     = std::sqrt(uc * uc + vc * vc + (suu + svv) / n);

    if (!std::isfinite(ux) || !std::isfinite(uy) || !std::isfinite(r) || r <= 0.0)
        return std::nullopt;

    // The check, in the same translated frame.
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto [px, py] = at(i);
        const double dx     = px - ux;
        const double dy     = py - uy;
        const double off    = std::abs(std::sqrt(dx * dx + dy * dy) - r);
        if (off > kFitTolerance) return std::nullopt;
    }

    FittedCircle out;
    out.centre = core::Point2{origin.x + static_cast<core::Mm>(std::llround(ux)),
                              origin.y + static_cast<core::Mm>(std::llround(uy))};
    out.radius = static_cast<core::Mm>(std::llround(r));
    return out.radius > 0 ? std::optional<FittedCircle>{out} : std::nullopt;
}

} // namespace

command::Task<core::Result<VectorReport>> import_vector(command::Transaction& tx, std::string path,
                                                        ImportOptions options, std::stop_token stop)
{
#ifndef KENTOS_HAVE_GDAL
    (void)tx;
    (void)path;
    (void)options;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  std::string(kErrNoDriver) + ": " + vector_backend_status());
#else
    ensure_registered();

    const std::string& driver              = options.driver;
    const std::string& project_crs         = options.project_crs;
    const std::vector<std::string>& only   = options.only;
    const std::vector<std::string>& fields = options.fields;

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
    report.driver           = format->driver;
    ImportDiagnostics& diag = report.diagnostics;

    // THE UNIT THE NUMBERS ARE IN. A geodetic format's unit is its coordinate
    // system's metre and there is nothing to decide. A DXF is read in the
    // PROJECT'S unit (`core.cizim.birim`, handed over in `options`), and what its
    // `$INSUNITS` header says is compared to that and REPORTED, never obeyed. The
    // header is set by whoever last saved the file, and Turkish cadastral
    // drawings routinely say "millimetres" (code 4) over numbers that are plainly
    // metres: obeying one shrank a whole town to eight metres across and rounded
    // every circle and arc in it to a one-millimetre blob. The user owns the
    // setting; the file only gets to disagree out loud.
    core::DrawingUnit unit = core::DrawingUnit::Metre;
    if (format->driver == "DXF") {
        unit                 = options.drawing_unit;
        diag.unit_source     = UnitSource::Setting;
        const char* declared = data.ptr->GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES");
        const int code =
            declared != nullptr ? static_cast<int>(std::strtol(declared, nullptr, 10)) : 0;
        if (const auto known = drawing_unit_from_insunits(code); known.has_value())
            diag.declared_unit = known;
        else if (code != 0)
            diag.note(Severity::Warning,
                      "$INSUNITS=" + std::to_string(code) +
                          (insunits_code_known(code) ? " bu sürümde çevrilmiyor"
                                                     : " DXF'in tanımadığı bir kod") +
                          "; çizim " + core::drawing_unit_name(unit) +
                          " olarak okundu (AYAR çizim_birimi).");
    } else {
        diag.unit_source = UnitSource::Crs;
    }
    diag.unit = unit;

    // The losses this path cannot avoid, counted here and said once at the end.
    std::uint64_t hatch_faces = 0, ellipses_stroked = 0, splines_stroked = 0,
                  dimensions_exploded = 0, heights_dropped = 0, styles_ignored = 0;

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
                diag.note(Severity::Info,
                          "Dosya koordinat sistemi bildirmiyor (DXF taşıyamaz). Çizimin kendi "
                          "sistemi varsayıldı: " +
                              project_crs +
                              ". Yanlışsa GERİAL ile geri alın, AYAR koordinat_sistemi ile "
                              "doğrusunu kurun ve yeniden aktarın.");
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

        // WHAT THE FILE SAYS THE ENTITY IS. OGR's DXF driver tessellates a CIRCLE
        // and an ARC into a LINESTRING before this module ever sees them, so a
        // reader that trusted the geometry alone would store 3 874 circles and
        // 3 523 arcs as many-cornered polygons — no centre, no radius, an area
        // that is the polygon's rather than pi r squared, and no snap to a centre
        // or a quadrant. AutoCAD and FreeCAD keep them as curves and so must we.
        //
        // The class is READ, not guessed: `SubClasses` is the entity's own DXF
        // class chain. What is recovered from the tessellation is only the
        // NUMBERS, and only when they check out — see `fit_circle`.
        const int class_field = layer->GetLayerDefn()->GetFieldIndex("SubClasses");

        // OUR OWN OUTPUT, RECOGNISED. A GeoPackage this program wrote carries a
        // `tur` field naming what each feature was — a circle, an arc, a caption
        // — because the format itself cannot say. It is trusted only when the
        // first feature's value carries this program's prefix: a municipal table
        // may legitimately have a column called `tur`.
        const int kind_field = layer->GetLayerDefn()->GetFieldIndex(kFieldKind);
        bool authored        = false;
        if (kind_field >= 0) {
            layer->ResetReading();
            if (OGRFeature* peek = layer->GetNextFeature()) {
                if (peek->IsFieldSetAndNotNull(kind_field)) {
                    const std::string_view first = peek->GetFieldAsString(kind_field);
                    authored                     = first.rfind("core.", 0) == 0;
                }
                OGRFeature::DestroyFeature(peek);
            }
        }
        const int own_text   = authored ? layer->GetLayerDefn()->GetFieldIndex(kFieldText) : -1;
        const int own_height = authored ? layer->GetLayerDefn()->GetFieldIndex(kFieldHeight) : -1;
        const int own_angle  = authored ? layer->GetLayerDefn()->GetFieldIndex(kFieldAngle) : -1;
        const int own_anchor = authored ? layer->GetLayerDefn()->GetFieldIndex(kFieldAnchor) : -1;

        // PAPER SPACE IS NOT THE DRAWING. A DXF's layouts hold the sheet frame,
        // the title block and the viewports; GDAL hands them over with this
        // field set, and importing them puts a paper border through the middle
        // of the parcels — far from the TUREF coordinates, where zoom-to-extents
        // (`YAKINLAŞ KAPSAM`) then zooms out to include it. The DWG reader has always dropped
        // them (`entmode == 1`); this path does the same, and counts.
        const int paper_field = layer->GetLayerDefn()->GetFieldIndex("PaperSpace");

        // Whether the file styled the entity itself. Neither is read into the
        // model yet — the layer's appearance stands — and that is a loss the user
        // is told about once, by count, rather than left to notice on a plot.
        const int linetype_field = layer->GetLayerDefn()->GetFieldIndex("Linetype");

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
        // THE FIELD PLAN, once per OGR layer: every attribute field the layer
        // carries, the column it would become, and whether this run writes it.
        // A DXF's fields are OGR's own bookkeeping — Layer, SubClasses, Linetype,
        // EntityHandle — and are never offered; a Shapefile's or a GeoPackage's
        // are the parcel's ada and parsel numbers, which is the whole point.
        struct FieldPlan
        {
            int index{0};
            std::string name;
            std::string id;
            core::AttrType type{core::AttrType::Text};
            std::uint8_t scale{0};
            core::AttrId column{core::kNoAttr};
            std::string sample;
        };

        std::vector<FieldPlan> plan;
        if (layer_field < 0) {
            const bool everything = fields.size() == 1 && fields.front() == "*";
            const auto asked      = [&](const std::string& name) {
                if (everything) return true;
                for (const std::string& f : fields)
                    if (core::turkish_iequals(f, name)) return true;
                return false;
            };
            OGRFeatureDefn* defn = layer->GetLayerDefn();
            for (int i = 0; i < defn->GetFieldCount(); ++i) {
                const OGRFieldDefn* f = defn->GetFieldDefn(i);
                // This program's own bookkeeping is not an attribute of the parcel.
                if (authored && is_own_field(f->GetNameRef())) continue;
                FieldPlan entry;
                entry.index = i;
                entry.name  = f->GetNameRef();
                switch (f->GetType()) {
                case OFTInteger:
                case OFTInteger64: entry.type = core::AttrType::Int64; break;
                case OFTReal:
                    entry.type  = core::AttrType::Decimal;
                    entry.scale = static_cast<std::uint8_t>(
                        f->GetPrecision() > 0 && f->GetPrecision() <= 6 ? f->GetPrecision() : 2);
                    break;
                case OFTDate:
                case OFTDateTime: entry.type = core::AttrType::Date; break;
                default: entry.type = core::AttrType::Text; break;
                }
                // The column id: the field's name folded to what `SÜTUN kimlik=`
                // takes, so `ADA_NO` and `ada_no` in two files land in one column.
                // `turkish_fold_key` folds case AND alphabet — to ASCII capitals —
                // and a column id is lower case, so the capitals come down here.
                // ASCII only by then, which is why no locale is involved.
                std::string id = core::turkish_fold_key(entry.name);
                for (char& ch : id) {
                    if (ch >= 'A' && ch <= 'Z')
                        ch = static_cast<char>(ch - 'A' + 'a');
                    else if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_'))
                        ch = '_';
                }
                if (id.empty()) id = "alan_" + std::to_string(i);
                entry.id = id;

                if (asked(entry.name)) {
                    // Declared once and reused when it is already there: a
                    // declaration is not undoable (a schema is not a drawing
                    // edit), so a second import of the same file adds no second
                    // column. A column of another type under the same id is a
                    // conflict the report states rather than resolves.
                    const core::AttrTable& table = tx.document().attributes();
                    if (const core::AttrId found = table.find(entry.id); found != core::kNoAttr) {
                        const core::AttrColumn* held = table.column(found);
                        if (held != nullptr && held->spec().type == entry.type)
                            entry.column = found;
                        else
                            diag.note(Severity::Degraded,
                                      "'" + entry.id +
                                          "' sütunu belgede başka türde tanımlı; alan okunmadı.");
                    } else {
                        core::AttrSpec spec;
                        spec.id       = entry.id;
                        spec.name_tr  = entry.name;
                        spec.type     = entry.type;
                        spec.scale    = entry.scale;
                        spec.layer    = layer_name;
                        auto declared = tx.declare_attribute(std::move(spec));
                        if (!declared) co_return declared.error();
                        entry.column = declared.value();
                    }
                }
                plan.push_back(std::move(entry));
            }
        }
        bool sampled = false;

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

            if (paper_field >= 0 && feature->IsFieldSetAndNotNull(paper_field) &&
                feature->GetFieldAsInteger(paper_field) != 0) {
                ++diag.paper_space_skipped;
                continue;
            }

            // The first feature lends every field its sample, imported or not.
            if (!sampled) {
                sampled = true;
                for (FieldPlan& entry : plan)
                    if (feature->IsFieldSetAndNotNull(entry.index))
                        entry.sample =
                            std::string(feature->GetFieldAsString(entry.index)).substr(0, 40);
            }

            // Writes the requested fields of THIS feature onto the entity it became.
            const auto stamp = [&](command::EntityId made) -> core::Status {
                for (const FieldPlan& entry : plan) {
                    if (entry.column == core::kNoAttr) continue;
                    if (!feature->IsFieldSetAndNotNull(entry.index)) continue;
                    core::AttrValue value;
                    switch (entry.type) {
                    case core::AttrType::Int64:
                        value.type    = core::AttrType::Int64;
                        value.present = true;
                        value.number  = feature->GetFieldAsInteger64(entry.index);
                        break;
                    case core::AttrType::Decimal: {
                        double scaled = feature->GetFieldAsDouble(entry.index);
                        for (std::uint8_t d = 0; d < entry.scale; ++d)
                            scaled *= 10.0;
                        value = core::attr_decimal(static_cast<std::int64_t>(std::llround(scaled)),
                                                   entry.scale);
                        break;
                    }
                    case core::AttrType::Date: {
                        int y = 0, mo = 0, d = 0, h = 0, mi = 0, tz = 0;
                        float sec = 0.0F;
                        if (!feature->GetFieldAsDateTime(entry.index, &y, &mo, &d, &h, &mi, &sec,
                                                         &tz))
                            continue;
                        // Written through the one date parser rather than computed
                        // here, so a 30 February in a file is refused the way a
                        // typed one is. A year outside four digits does not fit
                        // the buffer and is skipped like any other unreadable value.
                        char iso[16];
                        const int wrote =
                            std::snprintf(iso, sizeof iso, "%04d-%02d-%02d", y, mo, d);
                        if (wrote <= 0 || wrote >= static_cast<int>(sizeof iso)) continue;
                        const auto days = core::date_from_text(iso);
                        if (!days) continue;
                        value = core::attr_date(*days);
                        break;
                    }
                    default: value = core::attr_text(feature->GetFieldAsString(entry.index)); break;
                    }
                    if (auto st = tx.set_attribute(entry.column, made, value); !st) return st;
                }
                return core::ok();
            };

            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) continue;

            // WHAT THE FILE CALLS IT, for the census and for the losses named at
            // the end. The class chain is the DXF's own; a geodetic format has
            // none and is counted by its geometry type.
            const std::string_view chain =
                class_field >= 0 && feature->IsFieldSetAndNotNull(class_field)
                    ? std::string_view(feature->GetFieldAsString(class_field))
                    : std::string_view();
            const std::string own_kind = authored && feature->IsFieldSetAndNotNull(kind_field)
                                             ? std::string(feature->GetFieldAsString(kind_field))
                                             : std::string();
            const std::string type_name =
                class_field >= 0 ? dxf_type_name(chain)
                : !own_kind.empty()
                    ? own_kind
                    : std::string(OGRGeometryTypeToName(wkbFlatten(geometry->getGeometryType())));
            const bool is_hatch = chain.find("AcDbHatch") != std::string_view::npos;
            const bool is_ellipse =
                chain.find("AcDbEllipse") != std::string_view::npos || own_kind == "core.ellipse";
            const bool is_spline    = chain.find("AcDbSpline") != std::string_view::npos;
            const bool is_dimension = chain.find("Dimension") != std::string_view::npos;
            const bool degraded     = is_hatch || is_ellipse || is_spline || is_dimension;

            // One line of bookkeeping per outcome, so every `continue` below
            // leaves a trace in the census.
            const auto count_read = [&] {
                diag.tally(type_name, 1, 0, degraded ? 1 : 0);
                if (is_hatch) ++hatch_faces;
                if (is_ellipse) ++ellipses_stroked;
                if (is_spline) ++splines_stroked;
                if (is_dimension) ++dimensions_exploded;
            };
            const auto count_skipped = [&] { diag.tally(type_name, 0, 1, 0); };

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

            // Counted AFTER the layer filter: a loss on a layer the user did not
            // ask for is not a loss of this import, and "71 818 objects kept
            // their own colour" over an import of 2 221 was a number about a
            // different drawing.
            if (carries_height(geometry)) ++heights_dropped;

            {
                const char* named_type =
                    linetype_field >= 0 && feature->IsFieldSetAndNotNull(linetype_field)
                        ? feature->GetFieldAsString(linetype_field)
                        : nullptr;
                const bool own_linetype = named_type != nullptr && *named_type != 0 &&
                                          !core::turkish_iequals(named_type, "BYLAYER");
                const std::string_view pen = style_value(feature->GetStyleString(), "c");
                const bool own_colour      = !pen.empty() && pen != "#000000" && pen != "#000000FF";
                if (own_linetype || own_colour) ++styles_ignored;
            }

            const OGRwkbGeometryType type = wkbFlatten(geometry->getGeometryType());
            rings.clear();
            ring_store.clear();

            const auto push_polygon = [&](const OGRPolygon* polygon, std::uint16_t part) {
                if (const OGRLinearRing* outer = polygon->getExteriorRing()) {
                    ring_store.emplace_back();
                    ring_to_mm(outer, ring_store.back(), unit);
                    rings.push_back(
                        core::RingGeometry::RingInput{{}, core::RingRole::Exterior, part});
                }
                for (int h = 0; h < polygon->getNumInteriorRings(); ++h) {
                    ring_store.emplace_back();
                    ring_to_mm(polygon->getInteriorRing(h), ring_store.back(), unit);
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
                const core::Point2 where{core::mm_from_drawing_units(p->getX(), unit),
                                         core::mm_from_drawing_units(p->getY(), unit)};

                const char* label = text_field >= 0 && feature->IsFieldSetAndNotNull(text_field)
                                        ? feature->GetFieldAsString(text_field)
                                        : nullptr;

                if (own_kind == "core.text" && own_text >= 0 &&
                    feature->IsFieldSetAndNotNull(own_text)) {
                    // A CAPTION THIS PROGRAM WROTE, with its four facts in fields
                    // rather than in a style string: the height in millimetres,
                    // the angle in degrees, the anchor by name.
                    const std::string words = feature->GetFieldAsString(own_text);
                    core::Mm height =
                        own_height >= 0 && feature->IsFieldSetAndNotNull(own_height)
                            ? static_cast<core::Mm>(feature->GetFieldAsInteger64(own_height))
                            : 0;
                    if (height <= 0) height = core::mm_from_metres(1.0);
                    const double angle = own_angle >= 0 && feature->IsFieldSetAndNotNull(own_angle)
                                             ? feature->GetFieldAsDouble(own_angle)
                                             : 0.0;
                    const core::TextAnchor anchor =
                        own_anchor >= 0 && feature->IsFieldSetAndNotNull(own_anchor)
                            ? anchor_from_name(feature->GetFieldAsString(own_anchor))
                            : core::TextAnchor::BaselineLeft;

                    std::size_t glyphs = 0;
                    for (const char c : words)
                        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) ++glyphs;
                    const core::Mm advance =
                        std::max<core::Mm>(1, (height * 6 * static_cast<core::Mm>(glyphs)) / 10);
                    const double turn = angle * kPi / 180.0;
                    const core::Point2 end{
                        where.x + core::mm_round(static_cast<double>(advance) * std::cos(turn)),
                        where.y + core::mm_round(static_cast<double>(advance) * std::sin(turn))};

                    const std::array<core::Point2, 2> baseline{where, end};
                    auto made = tx.add_polyline(target, baseline);
                    if (!made)
                        co_return err(made.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". öğe okunamadı: " + made.error().message);
                    if (auto st = tx.set_text(made.value(), words, height, anchor); !st)
                        co_return err(st.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". yazı yazılamadı: " + st.error().message);
                    if (auto st = stamp(made.value()); !st) co_return st.error();
                } else if (label != nullptr && *label != 0) {
                    // TEXT IS A BASELINE PLUS A STRING, exactly as the `METİN`
                    // command builds one — two real vertices, so the cull, the
                    // snap and the hit test need to know nothing about text.
                    const char* style = feature->GetStyleString();
                    core::Mm height   = label_height_mm(style, unit);
                    if (height <= 0) {
                        // The file did not say. A metre is the height a 1/1000
                        // cadastral sheet prints a parcel number at, and it is
                        // REPORTED rather than left for the user to discover by
                        // measuring one.
                        height = core::mm_from_metres(1.0);
                        diag.note(Severity::Info, "Yazı yüksekliği dosyada yok; 1 m varsayıldı.");
                    }

                    // A rough advance of 0.6 em per character, which decides the
                    // BOUNDING BOX and not where a glyph lands. Same approximation
                    // `METİN` makes, and for the same reason. At least one
                    // millimetre of it, because the baseline is also the
                    // DIRECTION and a zero-length one has none.
                    const core::Mm advance = std::max<core::Mm>(
                        1, (height * 6 * static_cast<core::Mm>(std::strlen(label))) / 10);

                    // THE BASELINE IS THE ROTATION. `render/src/scene.cpp` reads a
                    // caption's facing from its first vertex to its last and
                    // nothing else, so the angle DXF stores is carried by turning
                    // the run rather than by a field: no new column, and every
                    // client that already draws a caption draws a turned one.
                    const double turn = label_angle_deg(style) * kPi / 180.0;
                    const core::Point2 end{
                        where.x + core::mm_round(static_cast<double>(advance) * std::cos(turn)),
                        where.y + core::mm_round(static_cast<double>(advance) * std::sin(turn))};

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
                    if (auto st = stamp(made.value()); !st) co_return st.error();
                } else {
                    auto made = tx.add_point(target, where);
                    if (!made)
                        co_return err(made.error().code,
                                      "'" + path + "' içindeki " + std::to_string(report.features) +
                                          ". öğe okunamadı: " + made.error().message);
                    if (auto st = stamp(made.value()); !st) co_return st.error();
                }
                ++report.entities;
                count_read();
                continue;
            }

            switch (type) {
            case wkbLineString:
                ring_store.emplace_back();
                line_to_mm(geometry->toLineString(), ring_store.back(), unit);
                rings.push_back(core::RingGeometry::RingInput{{}, core::RingRole::Open, 0});
                break;

            case wkbMultiLineString: {
                const OGRMultiLineString* multi = geometry->toMultiLineString();
                for (int g = 0; g < multi->getNumGeometries(); ++g) {
                    ring_store.emplace_back();
                    line_to_mm(multi->getGeometryRef(g), ring_store.back(), unit);
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
                diag.note(Severity::Skipped, std::string("Desteklenmeyen geometri türü atlandı: ") +
                                                 OGRGeometryTypeToName(type) +
                                                 ". Bu sürüm çizgi ve alan okur.");
                count_skipped();
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
            // A CIRCLE OR AN ARC, WHEN THE FILE SAYS SO. Checked before the
            // closed-polygon heuristic, because a tessellated circle IS a closed
            // ring and would otherwise be stored as a face — with the polygon's
            // area instead of pi r squared, and nothing to snap a centre to.
            if ((class_field >= 0 || authored) && rings.size() == 1 && !ring_store.empty()) {

                // `AcDbEntity:AcDbCircle:AcDbArc` for an arc, and the same chain
                // without the tail for a circle — an arc IS a circle in DXF's own
                // class hierarchy, so the more specific name is tested first.
                const bool is_arc =
                    chain.find("AcDbArc") != std::string_view::npos || own_kind == "core.arc";
                const bool is_circle =
                    !is_arc && (chain.find("AcDbCircle") != std::string_view::npos ||
                                own_kind == "core.circle");

                if ((is_arc || is_circle) && !ring_store.front().empty()) {
                    if (const auto fit = fit_circle(ring_store.front()); fit) {
                        // WHICH WAY ROUND, read from the tessellation rather than
                        // assumed. `add_arc` sweeps counter-clockwise from start to
                        // end, and OGR hands this file's arcs over CLOCKWISE — 150°
                        // to 30° for an arc DXF declares as 30° to 150°. Taking the
                        // ends in the order they arrive therefore stored the
                        // COMPLEMENT: a 120° arc came out as the 240° one on the
                        // other side of the circle, which draws and measures wrong.
                        //
                        // The signed turning of the run says it without a
                        // convention: each step's angle change is wrapped into
                        // (-pi, pi] and summed, so the total is the sweep with its
                        // sign, whatever order the points came in.
                        const std::vector<core::Point2>& run = ring_store.front();
                        double turning                       = 0.0;
                        for (std::size_t v = 0; v + 1 < run.size(); ++v) {
                            const double a0 =
                                std::atan2(static_cast<double>(run[v].y - fit->centre.y),
                                           static_cast<double>(run[v].x - fit->centre.x));
                            const double a1 =
                                std::atan2(static_cast<double>(run[v + 1].y - fit->centre.y),
                                           static_cast<double>(run[v + 1].x - fit->centre.x));
                            double step = a1 - a0;
                            while (step > kPi)
                                step -= 2.0 * kPi;
                            while (step <= -kPi)
                                step += 2.0 * kPi;
                            turning += step;
                        }

                        const core::Point2 from = turning >= 0.0 ? run.front() : run.back();
                        const core::Point2 to   = turning >= 0.0 ? run.back() : run.front();

                        auto made = is_circle
                                        ? tx.add_circle(target, fit->centre, fit->radius)
                                        : tx.add_arc(target, fit->centre, fit->radius, from, to);
                        if (made) {
                            if (auto st = stamp(made.value()); !st) co_return st.error();
                            ++report.entities;
                            count_read();
                            continue;
                        }
                        // A refused add falls through to the polyline below: the
                        // tessellation is still a true picture of the curve, and
                        // a drawn approximation beats a dropped entity.
                    }
                }
            }

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
                ++diag.skipped;
                if (diag.skipped_reason.empty()) diag.skipped_reason = added.error().message;
                count_skipped();
                continue;
            }
            if (auto st = stamp(added.value()); !st) co_return st.error();
            ++report.entities;
            count_read();
        }

        if (!open_layer.empty()) tally(open_layer, report.entities - open_mark);

        for (const FieldPlan& entry : plan) {
            VectorField seen;
            seen.layer    = layer_name;
            seen.name     = entry.name;
            seen.id       = entry.id;
            seen.type     = entry.type;
            seen.scale    = entry.scale;
            seen.sample   = entry.sample;
            seen.imported = entry.column != core::kNoAttr;
            report.fields.push_back(std::move(seen));
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
        diag.note(Severity::Warning, "Dosyanın koordinat sistemi " + report.crs + ", çizimin ki " +
                                         project_crs +
                                         ". Koordinatlar dönüştürülmedi; AYAR koordinat_sistemi "
                                         "ile denetleyin.");

    // THE LOSSES THIS PATH CANNOT AVOID, said by count. OGR's DXF driver hands
    // every curve over stroked and every hatch over as a polygon; the file's own
    // colours, line types and heights are not read into the model yet. None of
    // this may stay silent (io.md P11/P13), and the wording says what the next
    // reader will do about it (docs.md R13: future tense, phase named).
    if (hatch_faces != 0)
        diag.note(Severity::Info, std::to_string(hatch_faces) +
                                      " tarama (HATCH) sınırı alan olarak okundu; dolgu deseni "
                                      "okunmadı.");
    if (ellipses_stroked != 0)
        diag.note(Severity::Warning, std::to_string(ellipses_stroked) +
                                         " elips çizgi parçalarına bölünerek okundu; gerçek elips "
                                         "okuma libdxfrw okuyucusuyla gelecek (Faz B).");
    if (splines_stroked != 0)
        diag.note(Severity::Warning,
                  std::to_string(splines_stroked) + " spline çizgi parçalarına bölünerek okundu.");
    if (dimensions_exploded != 0)
        diag.note(Severity::Info, std::to_string(dimensions_exploded) +
                                      " ölçülendirme (DIMENSION) parçası çizgi ve yazı olarak "
                                      "okundu; ölçü nesnesi bu sürümde yok.");
    if (heights_dropped != 0)
        diag.note(Severity::Warning, std::to_string(heights_dropped) +
                                         " öğede yükseklik (Z) vardı; çizim iki boyutludur, Z "
                                         "atıldı.");
    if (styles_ignored != 0)
        diag.note(Severity::Info, std::to_string(styles_ignored) +
                                      " öğenin kendi rengi ya da çizgi tipi bu sürümde okunmadı; "
                                      "katman görünümü kullanıldı.");

    // WHAT HAPPENED TO THE TABLE. Fields the file had and this run did not read
    // are said, by count, with the argument that reads them; fields it did read
    // are counted too. Dropping a parcel's ada number without a word is the one
    // thing an import must never do (io.md P11).
    {
        std::size_t offered = 0, taken = 0;
        for (const VectorField& f : report.fields) {
            ++offered;
            if (f.imported) ++taken;
        }
        if (offered > 0 && taken == 0)
            diag.note(Severity::Info, "Dosyada " + std::to_string(offered) +
                                          " öznitelik alanı var; sütun olarak okumak için "
                                          "alanlar=* ya da alanlar=\"ad,ad\" verin.");
        else if (taken > 0)
            diag.note(Severity::Info, std::to_string(taken) + " alan sütun olarak okundu.");
    }

    co_return report;
#endif
}

// ---------------------------------------------------------------- export ----

command::Task<core::Result<VectorReport>> export_vector(const core::Document& doc, std::string path,
                                                        ExportOptions options, std::stop_token stop)
{
#ifndef KENTOS_HAVE_GDAL
    (void)doc;
    (void)path;
    (void)options;
    (void)stop;
    co_return err(ErrorCode::Unsupported,
                  std::string(kErrNoDriver) + ": " + vector_backend_status());
#else
    ensure_registered();

    const std::string& driver = options.driver;
    const std::string& crs    = options.crs;

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

    // A DXF's numbers are in the drawing unit and its header says which; a
    // geodetic format's are the CRS's metres and take no option. GDAL's writer
    // also turns every polygon into a HATCH unless told otherwise, and a parcel
    // written as a solid hatch comes back into this program — and into every
    // CAD program — as a filled picture rather than a boundary.
    const bool dxf_out = format->driver == "DXF";
    const auto to_out  = [&](core::Mm v) {
        return dxf_out ? core::drawing_units_from_mm(v, options.unit) : core::mm_to_metres(v);
    };
    char** create_options = nullptr;
    if (dxf_out)
        create_options = ::CSLSetNameValue(nullptr, "INSUNITS", gdal_insunits_name(options.unit));
    const std::unique_ptr<char*, void (*)(char**)> create_options_guard(
        create_options, [](char** list) { ::CSLDestroy(list); });

    const char* hatch_before = ::CPLGetThreadLocalConfigOption("DXF_WRITE_HATCH", nullptr);
    const std::optional<std::string> hatch_saved =
        hatch_before != nullptr ? std::optional<std::string>(hatch_before) : std::nullopt;
    if (dxf_out) ::CPLSetThreadLocalConfigOption("DXF_WRITE_HATCH", "FALSE");

    struct RestoreHatchOption
    {
        bool armed;
        const std::optional<std::string>& saved;

        ~RestoreHatchOption()
        {
            if (armed)
                ::CPLSetThreadLocalConfigOption("DXF_WRITE_HATCH",
                                                saved ? saved->c_str() : nullptr);
        }
    } restore_hatch{dxf_out, hatch_saved};

    ::CPLErrorReset();
    ::VSIUnlink(path.c_str()); // a stale target makes GPKG refuse to create
    DatasetHandle data(gdal_driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, create_options));
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

    // The rings of one entity as OGR geometry: one Open ring is a line, anything
    // else is a polygon whose Open rings are dropped. What a polyline IS.
    const auto rings_geometry = [&](const core::RingSpan& span) -> std::unique_ptr<OGRGeometry> {
        if (span.count == 1 && geo.ring_role[span.first] == core::RingRole::Open) {
            auto line     = std::make_unique<OGRLineString>();
            const auto xs = geo.ring_xs(span.first);
            const auto ys = geo.ring_ys(span.first);
            for (std::size_t v = 0; v < xs.size(); ++v)
                line->addPoint(to_out(xs[v]), to_out(ys[v]));
            return line;
        }
        auto polygon = std::make_unique<OGRPolygon>();
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            if (geo.ring_role[r] == core::RingRole::Open) continue;
            OGRLinearRing ring;
            const auto xs = geo.ring_xs(r);
            const auto ys = geo.ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                ring.addPoint(to_out(xs[v]), to_out(ys[v]));
            ring.closeRings(); // OGR wants the repeated closing vertex back
            polygon->addRing(&ring);
        }
        if (polygon->getExteriorRing() == nullptr) return nullptr;
        return polygon;
    };

    // A CURVE IS ITS OUTLINE, NOT ITS DEFINITION. A circle is stored as two
    // vertices — its centre and a radius handle — and writing those as a line
    // exported every circle in the drawing as a short stroke pointing east. The
    // one tessellator the renderer and the pick test use answers here too
    // (core/entity_kind.hpp); a GIS gets the polygon the screen shows, a DXF a
    // closed polyline, and the importer's own fit recovers the circle exactly.
    core::EmitBuffer curve;
    const auto curve_geometry = [&](core::KindId kind,
                                    std::uint32_t slot) -> std::unique_ptr<OGRGeometry> {
        curve.clear();
        if (!core::curve_outline(kind, geo, slot, curve) || curve.run_total() == 0) return nullptr;
        const std::uint32_t first = curve.run_start[0];
        const std::uint32_t count = curve.run_count[0];
        const bool closed         = curve.run_closed[0] != 0;
        if (closed && !dxf_out) {
            auto polygon = std::make_unique<OGRPolygon>();
            OGRLinearRing ring;
            for (std::uint32_t v = first; v < first + count; ++v)
                ring.addPoint(to_out(curve.xs[v]), to_out(curve.ys[v]));
            ring.closeRings();
            polygon->addRing(&ring);
            return polygon;
        }
        auto line = std::make_unique<OGRLineString>();
        for (std::uint32_t v = first; v < first + count; ++v)
            line->addPoint(to_out(curve.xs[v]), to_out(curve.ys[v]));
        if (closed) line->addPoint(to_out(curve.xs[first]), to_out(curve.ys[first]));
        return line;
    };

    // The fields a GIS layer carries beside its geometry: what the entity is,
    // its persistent key, a caption's four facts, then every attribute column
    // that applies to the drawing layer. A DXF has no fields but `Layer`.
    struct OutColumn
    {
        core::AttrId column;
        int field;
        core::AttrType type;
        std::uint8_t scale;
    };

    std::vector<OutColumn> out_columns;
    int f_kind = -1, f_key = -1, f_text = -1, f_height = -1, f_angle = -1, f_anchor = -1;

    const auto make_field = [&](OGRLayer* out, const char* name, OGRFieldType type,
                                OGRFieldSubType sub = OFSTNone, int precision = 0) -> int {
        OGRFieldDefn field(name, type);
        if (sub != OFSTNone) field.SetSubType(sub);
        if (precision > 0) {
            field.SetWidth(24);
            field.SetPrecision(precision);
        }
        if (out->CreateField(&field) != OGRERR_NONE) return -1;
        return out->GetLayerDefn()->GetFieldIndex(name);
    };

    std::uint64_t unknown_kinds = 0;
    std::set<std::string> columns_written;

    for (core::LayerId l = 0; l < doc.layers().size(); ++l) {
        const core::Layer& layer = doc.layers()[l];
        if (doc.layer_entity_count(l) == 0) continue;

        OGRLayer* out = shared;
        if (out == nullptr) {
            out = data.ptr->CreateLayer(layer.name.c_str(), &srs, wkbUnknown, nullptr);
            if (!out)
                co_return err(ErrorCode::IoFailure,
                              "'" + layer.name + "' katmanı yazılamadı: " + gdal_reason());

            f_kind   = make_field(out, kFieldKind, OFTString);
            f_key    = make_field(out, kFieldKey, OFTInteger64);
            f_text   = make_field(out, kFieldText, OFTString);
            f_height = make_field(out, kFieldHeight, OFTInteger64);
            f_angle  = make_field(out, kFieldAngle, OFTReal);
            f_anchor = make_field(out, kFieldAnchor, OFTString);
            if (f_kind < 0 || f_key < 0 || f_text < 0 || f_height < 0 || f_angle < 0 ||
                f_anchor < 0)
                co_return err(ErrorCode::IoFailure,
                              "'" + layer.name +
                                  "' katmanının alanları oluşturulamadı: " + gdal_reason());

            // THE TABLE TRAVELS WITH THE GEOMETRY. Every declared column that
            // applies to this drawing layer becomes a field of the OGR layer, in
            // the type the schema declares — an `ada` number written as text
            // would sort as text on the other side.
            out_columns.clear();
            const core::AttrTable& attrs = doc.attributes();
            for (std::size_t i = 0; i < attrs.columns(); ++i) {
                const auto c                 = static_cast<core::AttrId>(i);
                const core::AttrColumn* held = attrs.column(c);
                if (held == nullptr || !core::attr_applies_to(held->spec(), layer.name)) continue;
                const core::AttrSpec& spec = held->spec();
                // A user column that collides with this program's own field
                // keeps its data under a prefix rather than losing it.
                const std::string name = is_own_field(spec.id) ? "oz_" + spec.id : spec.id;

                int field = -1;
                switch (spec.type) {
                case core::AttrType::Int64:
                    field = make_field(out, name.c_str(), OFTInteger64);
                    break;
                case core::AttrType::Length: field = make_field(out, name.c_str(), OFTReal); break;
                case core::AttrType::Bool:
                    field = make_field(out, name.c_str(), OFTInteger, OFSTBoolean);
                    break;
                case core::AttrType::Decimal:
                    field = make_field(out, name.c_str(), OFTReal, OFSTNone,
                                       std::max<int>(1, spec.scale));
                    break;
                case core::AttrType::Date: field = make_field(out, name.c_str(), OFTDate); break;
                case core::AttrType::Text:
                case core::AttrType::CodeRef:
                    field = make_field(out, name.c_str(), OFTString);
                    break;
                }
                if (field < 0)
                    co_return err(ErrorCode::IoFailure, "'" + layer.name + "' katmanına '" +
                                                            spec.id +
                                                            "' alanı yazılamadı: " + gdal_reason());
                out_columns.push_back(OutColumn{c, field, spec.type, spec.scale});
                columns_written.insert(spec.id);
            }
        }
        ++report.layers;

        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if ((e % 4096) == 0 && stop.stop_requested())
                co_return err(ErrorCode::Cancelled, "Dışa aktarma iptal edildi.");
            if (ents.layer[e] != l || !ents.alive(e)) continue;

            const std::uint32_t slot  = ents.slot[e];
            const core::RingSpan span = geo.rings_of(slot);
            if (span.count == 0) continue;

            // WHAT KIND OF THING THIS IS decides what its rings mean — the same
            // question the renderer asks (model.md R22). A caption is a point
            // with words; a curve is its outline; a kind this build does not
            // know is counted and left out rather than written as its raw
            // definition vertices.
            const core::KindId kind    = ents.kind[e];
            const core::KindSpec* spec = core::builtin_kinds().find(kind);
            const bool caption         = doc.texts().has(slot);
            if (spec == nullptr && !caption && kind != 0) {
                ++unknown_kinds;
                continue;
            }
            const std::string type_id = caption ? "core.text"
                                        : spec  ? spec->stable_id
                                                : "core.polyline";

            std::unique_ptr<OGRGeometry> geometry;
            double angle_deg = 0.0;
            if (caption) {
                const auto xs = geo.ring_xs(span.first);
                const auto ys = geo.ring_ys(span.first);
                geometry      = std::make_unique<OGRPoint>(to_out(xs[0]), to_out(ys[0]));
                if (xs.size() >= 2) {
                    angle_deg = std::atan2(static_cast<double>(ys[1] - ys[0]),
                                           static_cast<double>(xs[1] - xs[0])) *
                                180.0 / kPi;
                    if (angle_deg < 0.0) angle_deg += 360.0;
                }
            } else if (kind == core::kPointKind) {
                const core::Point2 at = core::point_position_of(geo, slot);
                geometry              = std::make_unique<OGRPoint>(to_out(at.x), to_out(at.y));
            } else if (kind == core::kCircleKind || kind == core::kArcKind ||
                       kind == core::kEllipseKind) {
                geometry = curve_geometry(kind, slot);
                if (!geometry) geometry = rings_geometry(span);
            } else {
                geometry = rings_geometry(span);
            }
            if (!geometry) continue;

            const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> feature(
                OGRFeature::CreateFeature(out->GetLayerDefn()),
                [](OGRFeature* f) { OGRFeature::DestroyFeature(f); });
            feature->SetGeometry(geometry.get());

            if (one_layer_only) {
                feature->SetField("Layer", layer.name.c_str());
                if (caption) {
                    // The words, their height and their angle ride in the one
                    // place GDAL's DXF writer reads them from: the style string.
                    std::string words(doc.texts().text(slot));
                    std::string escaped;
                    for (const char ch : words) {
                        if (ch == '"' || ch == '\\') escaped.push_back('\\');
                        escaped.push_back(ch);
                    }
                    char label[96];
                    (void)std::snprintf(
                        label, sizeof label, ",s:%.6gg,a:%.6g)",
                        drawing_units_from_mm(doc.texts().height(slot), options.unit), angle_deg);
                    const std::string style = "LABEL(t:\"" + escaped + "\"" + label;
                    feature->SetStyleString(style.c_str());
                }
            } else {
                feature->SetField(f_kind, type_id.c_str());
                feature->SetField(f_key, static_cast<GIntBig>(core::raw(ents.key[e])));
                if (caption) {
                    feature->SetField(f_text, std::string(doc.texts().text(slot)).c_str());
                    feature->SetField(f_height, static_cast<GIntBig>(doc.texts().height(slot)));
                    feature->SetField(f_angle, angle_deg);
                    feature->SetField(f_anchor, core::text_anchor_name(doc.texts().anchor(slot)));
                }

                for (const OutColumn& oc : out_columns) {
                    auto cell = doc.attribute(oc.column, e);
                    if (!cell || !cell.value().present) {
                        feature->SetFieldNull(oc.field);
                        continue;
                    }
                    const core::AttrValue& v = cell.value();
                    switch (oc.type) {
                    case core::AttrType::Int64:
                        feature->SetField(oc.field, static_cast<GIntBig>(v.number));
                        break;
                    case core::AttrType::Length:
                        feature->SetField(oc.field, core::mm_to_metres(v.number));
                        break;
                    case core::AttrType::Bool:
                        feature->SetField(oc.field, v.number != 0 ? 1 : 0);
                        break;
                    case core::AttrType::Decimal: {
                        double shown = static_cast<double>(v.number);
                        for (std::uint8_t d = 0; d < oc.scale; ++d)
                            shown /= 10.0;
                        feature->SetField(oc.field, shown);
                        break;
                    }
                    case core::AttrType::Date: {
                        // Through the one date formatter, then split: the field
                        // wants three integers and the model holds a day count.
                        const std::string iso = core::date_to_text(v.number);
                        int y = 0, m = 0, d = 0;
                        const auto field = [&iso](std::size_t from, std::size_t to, int& into) {
                            const auto r =
                                std::from_chars(iso.data() + from, iso.data() + to, into);
                            return r.ec == std::errc{} && r.ptr == iso.data() + to;
                        };
                        const std::size_t dash1 = iso.find('-');
                        const std::size_t dash2 = dash1 == std::string::npos
                                                      ? std::string::npos
                                                      : iso.find('-', dash1 + 1);
                        if (dash1 != std::string::npos && dash2 != std::string::npos &&
                            field(0, dash1, y) && field(dash1 + 1, dash2, m) &&
                            field(dash2 + 1, iso.size(), d))
                            feature->SetField(oc.field, y, m, d, 0, 0, 0.0F, 0);
                        else
                            feature->SetFieldNull(oc.field);
                        break;
                    }
                    case core::AttrType::Text:
                    case core::AttrType::CodeRef:
                        feature->SetField(oc.field, v.text.c_str());
                        break;
                    }
                }
            }

            if (out->CreateFeature(feature.get()) != OGRERR_NONE)
                co_return err(ErrorCode::IoFailure,
                              "'" + layer.name + "' katmanına öğe yazılamadı: " + gdal_reason());
            ++report.features;
            ++report.entities;
            if (caption) ++report.texts;
        }
    }
    report.columns = static_cast<std::uint64_t>(columns_written.size());

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

    if (dxf_out && options.unit != core::DrawingUnit::Metre)
        report.notes.push_back(std::string("DXF koordinatları ") +
                               core::drawing_unit_name(options.unit) +
                               " olarak yazıldı ve $INSUNITS başlığa işlendi (AYAR çizim_birimi).");

    if (unknown_kinds != 0)
        report.notes.push_back(std::to_string(unknown_kinds) +
                               " nesne bu yapının tanımadığı türde; dışa aktarılmadı.");

    // WHAT THE FILE CARRIES AND WHAT IT DOES NOT, said either way (io.md P11): a
    // silent lossy export is how a wrong pafta gets delivered.
    if (dxf_out)
        report.notes.push_back("Öznitelik sütunları DXF'e yazılmadı (biçim taşımaz); "
                               "GeoPackage kullanın. Daire, yay ve elips kapalı çokgen olarak "
                               "yazıldı; gerçek eğri yazımı libdxfrw ile gelecek (Faz B). Stil "
                               "bilgisi yazılmadı.");
    else if (report.columns != 0)
        report.notes.push_back(std::to_string(report.columns) +
                               " öznitelik sütunu alan olarak yazıldı; stil bilgisi yazılmadı.");
    else
        report.notes.push_back("Stil bilgisi bu sürümde yazılmadı; geometri, katman adı ve nesne "
                               "türü aktarıldı.");
    co_return report;
#endif
}

} // namespace kentos::io
