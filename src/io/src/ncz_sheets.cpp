// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): a Netcad map sheet (`pafta`) drawn as the sheet it is.
//
// See ncz_sheets.hpp for what a MapSheet record holds and why its box is not
// its frame.
#include "ncz_sheets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#ifdef KENTOS_HAVE_GDAL
#include <ogr_spatialref.h>
#endif

namespace kentos::io::ncz {

namespace {

/// The ellipsoid a datum byte names in PROJ's words; none for one not known.
/// ITRF is TUREF's datum, whose ellipsoid is GRS80; ED50's is International 1924.
const char* ellipsoid_of(std::uint8_t datum) noexcept
{
    switch (datum) {
    case 0: return "WGS84";
    case 1: return "GRS80";
    case 4:
    case 254: return "intl";
    default: return nullptr;
    }
}

#ifdef KENTOS_HAVE_GDAL
struct SrsRelease
{
    void operator()(OGRSpatialReference* srs) const noexcept
    {
        OGRSpatialReference::DestroySpatialReference(srs);
    }
};

struct CtRelease
{
    void operator()(OGRCoordinateTransformation* ct) const noexcept
    {
        OGRCoordinateTransformation::DestroyCT(ct);
    }
};

/// Solves the 4 × 4 system `a · x = b` by Gaussian elimination with partial
/// pivoting; false when it is singular.
bool solve4(std::array<std::array<double, 4>, 4> a, std::array<double, 4> b,
            std::array<double, 4>& x) noexcept
{
    for (std::size_t c = 0; c < 4; ++c) {
        std::size_t pivot = c;
        for (std::size_t r = c + 1; r < 4; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
        if (std::fabs(a[pivot][c]) < 1e-12) return false;
        std::swap(a[pivot], a[c]);
        std::swap(b[pivot], b[c]);
        for (std::size_t r = c + 1; r < 4; ++r) {
            const double f = a[r][c] / a[c][c];
            for (std::size_t k = c; k < 4; ++k)
                a[r][k] -= f * a[c][k];
            b[r] -= f * b[c];
        }
    }
    for (std::size_t c = 4; c-- > 0;) {
        double s = b[c];
        for (std::size_t k = c + 1; k < 4; ++k)
            s -= a[c][k] * x[k];
        x[c] = s / a[c][c];
    }
    return true;
}
#endif

} // namespace

struct SheetFrames::Impl
{
    std::string why;
    double central_meridian{0.0};
#ifdef KENTOS_HAVE_GDAL
    std::unique_ptr<OGRCoordinateTransformation, CtRelease> forward; ///< degrees → metres
    std::unique_ptr<OGRCoordinateTransformation, CtRelease> inverse; ///< metres → degrees
#endif
};

SheetFrames::SheetFrames(const Header& header) : impl_(std::make_unique<Impl>())
{
    Impl& d = *impl_;
    if (!header.mproj) {
        d.why = "dosya koordinat sistemi bildirmiyor";
        return;
    }
    // THE ZONE THE FILE ITSELF DECLARES — never the drawing's: the box was worked
    // out in the file's projection, and the numbers are read untransformed.
    int meridian   = 0;
    const char* k0 = nullptr;
    if (header.projection == 3) { // a Turkish 3° zone: the byte is the meridian, scale 1
        meridian = header.zone;
        k0       = "1";
    } else if (header.projection == 2) { // a 6° UTM zone: the byte is the zone number
        meridian = 6 * static_cast<int>(header.zone) - 183;
        k0       = "0.9996";
    } else {
        d.why = "dosya bir TM ya da UTM dilimi bildirmiyor";
        return;
    }
    const char* ellipsoid = ellipsoid_of(header.datum);
    if (ellipsoid == nullptr) {
        d.why = "dosyanın datumu tanınmıyor";
        return;
    }
    if (meridian < -180 || meridian > 180) {
        d.why = "dosyanın dilim bilgisi bir meridyen vermiyor";
        return;
    }
    d.central_meridian = meridian;
#ifdef KENTOS_HAVE_GDAL
    // A projection and its own geographic system: the one conversion the box was
    // made with, so no datum shift and no grid enters it.
    const std::string definition = "+proj=tmerc +lat_0=0 +lon_0=" + std::to_string(meridian) +
                                   " +k=" + k0 + " +x_0=500000 +y_0=0 +ellps=" + ellipsoid +
                                   " +units=m +no_defs";
    OGRSpatialReference projected;
    if (projected.importFromProj4(definition.c_str()) != OGRERR_NONE) {
        d.why = "dosyanın dilimi bir projeksiyona çevrilemedi";
        return;
    }
    projected.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    std::unique_ptr<OGRSpatialReference, SrsRelease> geographic(projected.CloneGeogCS());
    if (!geographic) {
        d.why = "dosyanın dilimi bir projeksiyona çevrilemedi";
        return;
    }
    geographic->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    d.forward.reset(OGRCreateCoordinateTransformation(geographic.get(), &projected));
    d.inverse.reset(OGRCreateCoordinateTransformation(&projected, geographic.get()));
    if (!d.forward || !d.inverse) {
        d.forward.reset();
        d.inverse.reset();
        d.why = "dosyanın dilimi bir projeksiyona çevrilemedi";
    }
#else
    (void)k0;
    d.why = "bu yapıda GDAL yok (KENTOS_WITH_GDAL=OFF)";
#endif
}

SheetFrames::~SheetFrames() = default;

bool SheetFrames::usable() const noexcept
{
    return impl_->why.empty();
}

const std::string& SheetFrames::why_not() const noexcept
{
    return impl_->why;
}

std::optional<std::array<Coord, 4>> SheetFrames::frame(double min_easting, double min_northing,
                                                       double max_easting,
                                                       double max_northing) const
{
    if (!usable()) return std::nullopt;
#ifdef KENTOS_HAVE_GDAL
    const Impl& d = *impl_;
    // The unknowns: the cell's south and north latitude, west and east longitude.
    using Four = std::array<double, 4>;
    const Four target{min_easting, min_northing, max_easting, max_northing};

    // The box of the cell `v` projected, and its corners. A parallel's northing
    // grows away from the central meridian, so where the cell straddles it the
    // least northing is the south parallel's crossing, not a corner; a
    // meridian's easting is monotonic along it, so the eastings come from
    // corners always.
    auto box = [&](const Four& v, Four& out, std::array<Coord, 4>* corners) -> bool {
        double lon[6]        = {v[2], v[3], v[3], v[2], d.central_meridian, d.central_meridian};
        double lat[6]        = {v[0], v[0], v[1], v[1], v[0], v[1]};
        const bool straddles = v[2] < d.central_meridian && d.central_meridian < v[3];
        const std::size_t n  = straddles ? 6 : 4;
        int ok[6]            = {};
        if (!d.forward->Transform(n, lon, lat, nullptr, ok)) return false;
        for (std::size_t i = 0; i < n; ++i)
            if (!ok[i] || !std::isfinite(lon[i]) || !std::isfinite(lat[i])) return false;
        out = {lon[0], lat[0], lon[0], lat[0]};
        for (std::size_t i = 1; i < n; ++i) {
            out[0] = std::min(out[0], lon[i]);
            out[1] = std::min(out[1], lat[i]);
            out[2] = std::max(out[2], lon[i]);
            out[3] = std::max(out[3], lat[i]);
        }
        if (corners != nullptr)
            for (std::size_t i = 0; i < 4; ++i)
                (*corners)[i] = Coord{lon[i], lat[i], 0.0};
        return true;
    };

    // The start: the box's own corners in degrees — within a fraction of a
    // second of the answer, which Newton's method closes in two or three steps.
    double west = min_easting, south = min_northing;
    double east = max_easting, north = max_northing;
    int ok[1] = {};
    if (!d.inverse->Transform(1, &west, &south, nullptr, ok) || !ok[0]) return std::nullopt;
    if (!d.inverse->Transform(1, &east, &north, nullptr, ok) || !ok[0]) return std::nullopt;
    Four u{south, north, west, east};

    constexpr double kStep = 1e-7; // degrees: about a centimetre on the ground
    for (int step = 0; step < 12; ++step) {
        Four f{};
        if (!box(u, f, nullptr)) return std::nullopt;
        Four r{};
        double worst = 0.0;
        for (std::size_t i = 0; i < 4; ++i) {
            r[i]  = f[i] - target[i];
            worst = std::max(worst, std::fabs(r[i]));
        }
        if (worst < 1e-6) break; // a micrometre
        std::array<std::array<double, 4>, 4> jacobian{};
        for (std::size_t j = 0; j < 4; ++j) {
            Four v = u;
            v[j] += kStep;
            Four g{};
            if (!box(v, g, nullptr)) return std::nullopt;
            for (std::size_t i = 0; i < 4; ++i)
                jacobian[i][j] = (g[i] - f[i]) / kStep;
        }
        Four delta{};
        if (!solve4(jacobian, r, delta)) return std::nullopt;
        for (std::size_t j = 0; j < 4; ++j)
            u[j] -= delta[j];
    }

    Four f{};
    std::array<Coord, 4> corners{};
    if (!box(u, f, &corners)) return std::nullopt;
    for (std::size_t i = 0; i < 4; ++i)
        if (std::fabs(f[i] - target[i]) > 0.01) return std::nullopt;

    // A SHEET, NOT ANY RECTANGLE. Four equations always have a cell that fits
    // them, so fitting proves nothing; what proves a sheet is that a sheet lies
    // on its scale's graticule: its south edge and its west edge fall on whole
    // multiples of its own size (1:1000: 40°11′15″ is the 6 430th 22,5″ step
    // from the equator). A local sheet — a rectangle in the projection itself —
    // fails this and keeps its box.
    const double height = u[1] - u[0];
    const double width  = u[3] - u[2];
    if (!(height > 0.0) || !(width > 0.0)) return std::nullopt;
    const double rows = u[0] / height;
    const double cols = u[2] / width;
    if (std::fabs(rows - std::round(rows)) > 1e-3 || std::fabs(cols - std::round(cols)) > 1e-3)
        return std::nullopt;
    return corners;
#else
    (void)min_easting;
    (void)min_northing;
    (void)max_easting;
    (void)max_northing;
    return std::nullopt;
#endif
}

} // namespace kentos::io::ncz
