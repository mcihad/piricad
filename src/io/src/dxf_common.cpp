// SPDX-License-Identifier: GPL-3.0-or-later
#include "dxf_common.hpp"

#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"
#include "kentos_cad/io/dxf.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace kentos::io {

bool dxf_backend_available()
{
#ifdef KENTOS_HAVE_DXFRW
    return true;
#else
    return false;
#endif
}

std::string dxf_backend_status()
{
#ifdef KENTOS_HAVE_DXFRW
    return "DXF libdxfrw ile okunur ve yazılır: daire, yay, elips, blok ve XDATA olduğu gibi.";
#else
    return "DXF GDAL sürücüsüyle okunur ve yazılır (KENTOS_WITH_DXFRW=OFF): eğriler "
           "parçalanır, bloklar açılır, XDATA düşer. Tam okuma için -DKENTOS_WITH_DXFRW=ON ile "
           "yapılandırın (kaynak indirilebilir olmalı).";
#endif
}

std::optional<DxfVersion> dxf_version_from_year(int year) noexcept
{
    switch (year) {
    case 2000: return DxfVersion::R2000;
    case 2004: return DxfVersion::R2004;
    case 2007: return DxfVersion::R2007;
    case 2010: return DxfVersion::R2010;
    case 2013: return DxfVersion::R2013;
    case 2018: return DxfVersion::R2018;
    default: return std::nullopt;
    }
}

int dxf_version_year(DxfVersion v) noexcept
{
    switch (v) {
    case DxfVersion::R2000: return 2000;
    case DxfVersion::R2004: return 2004;
    case DxfVersion::R2007: return 2007;
    case DxfVersion::R2010: return 2010;
    case DxfVersion::R2013: return 2013;
    case DxfVersion::R2018: return 2018;
    }
    return 2007;
}

namespace dxf {

// ------------------------------------------------------------------ colour ----

namespace {

/// The nine primaries and the six greys AutoCAD fixes by hand.
constexpr std::uint32_t kAciFixed[10] = {
    0x000000, // 0: ByBlock — never asked for
    0xFF0000, // 1 red
    0xFFFF00, // 2 yellow
    0x00FF00, // 3 green
    0x00FFFF, // 4 cyan
    0x0000FF, // 5 blue
    0xFF00FF, // 6 magenta
    0xFFFFFF, // 7 white (black on a light ground; the file says 7 either way)
    0x808080, // 8
    0xC0C0C0, // 9
};

constexpr std::uint32_t kAciGrey[6] = {0x333333, 0x505050, 0x696969, 0x828282, 0xBEBEBE, 0xFFFFFF};

/// Entries 10..249: 24 hues fifteen degrees apart, each in five brightness
/// steps, each step full and half saturation — the table AutoCAD has shipped
/// unchanged since R12 and every DXF consumer reproduces from this rule.
std::uint32_t aci_hue_table(int index) noexcept
{
    const int i     = index - 10;
    const int hue   = i / 10;       // 0..23
    const int step  = (i % 10) / 2; // 0..4, darker as it grows
    const bool half = (i % 2) != 0; // the pale variant

    static constexpr double kValue[5] = {1.0, 0.8, 0.6, 0.5, 0.3};
    const double v                    = kValue[step];
    const double s                    = half ? 0.5 : 1.0;

    // HSV → RGB with H = hue·15°; sector arithmetic, no trigonometry.
    const double h    = static_cast<double>(hue) * 15.0 / 60.0; // in sixths
    const int sector  = static_cast<int>(h) % 6;
    const double frac = h - static_cast<double>(static_cast<int>(h));
    const double p    = v * (1.0 - s);
    const double q    = v * (1.0 - s * frac);
    const double t    = v * (1.0 - s * (1.0 - frac));
    double r = 0, g = 0, b = 0;
    switch (sector) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }
    const auto byte = [](double c) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(c, 0.0, 1.0) * 255.0));
    };
    return (byte(r) << 16) | (byte(g) << 8) | byte(b);
}

} // namespace

std::uint32_t aci_rgb(int index) noexcept
{
    if (index >= 1 && index <= 9) return kAciFixed[index];
    if (index >= 10 && index <= 249) return aci_hue_table(index);
    if (index >= 250 && index <= 255) return kAciGrey[index - 250];
    return 0x000000;
}

std::uint32_t aci_ink(int index) noexcept
{
    return index == 7 ? 0x000000u : aci_rgb(index);
}

int aci_for_ink(std::uint32_t rgb) noexcept
{
    return (rgb & 0x00FFFFFFu) == 0 ? 7 : nearest_aci(rgb);
}

int nearest_aci(std::uint32_t rgb) noexcept
{
    const int r = static_cast<int>((rgb >> 16) & 0xFFu);
    const int g = static_cast<int>((rgb >> 8) & 0xFFu);
    const int b = static_cast<int>(rgb & 0xFFu);
    int best    = 7;
    long best_d = 1L << 40;
    for (int i = 1; i <= 255; ++i) {
        const std::uint32_t c = aci_rgb(i);
        const int dr          = r - static_cast<int>((c >> 16) & 0xFFu);
        const int dg          = g - static_cast<int>((c >> 8) & 0xFFu);
        const int db          = b - static_cast<int>(c & 0xFFu);
        const long d =
            static_cast<long>(dr) * dr + static_cast<long>(dg) * dg + static_cast<long>(db) * db;
        if (d < best_d) {
            best_d = d;
            best   = i;
        }
    }
    return best;
}

// -------------------------------------------------------------- lineweight ----

namespace {
constexpr int kLegalLineweights[] = {0,  5,  9,  13, 15, 18,  20,  25,  30,  35,  40,  50,
                                     53, 60, 70, 80, 90, 100, 106, 120, 140, 158, 200, 211};
}

std::int32_t lineweight_um_from_dxf(int code370) noexcept
{
    if (code370 < 0) return 0;
    return static_cast<std::int32_t>(code370) * 10; // hundredths of a mm → µm
}

int dxf_lineweight_from_um(std::int32_t um) noexcept
{
    const int target = static_cast<int>(um / 10);
    int best         = 0;
    for (const int w : kLegalLineweights)
        if (std::abs(w - target) < std::abs(best - target)) best = w;
    return best;
}

// ---------------------------------------------------------------- angles ----

std::int64_t udeg_from_radians(double radians) noexcept
{
    return static_cast<std::int64_t>(std::llround(radians * (180.0 * 1000000.0 / core::kPi)));
}

std::int64_t udeg_from_degrees(double degrees) noexcept
{
    return static_cast<std::int64_t>(std::llround(degrees * 1000000.0));
}

double radians_from_udeg(std::int64_t udeg) noexcept
{
    return static_cast<double>(udeg) * (core::kPi / (180.0 * 1000000.0));
}

double degrees_from_udeg(std::int64_t udeg) noexcept
{
    return static_cast<double>(udeg) / 1000000.0;
}

core::Point2 point_on_circle(core::Point2 centre, core::Mm radius, std::int64_t udeg) noexcept
{
    const core::SinCos t = core::sin_cos_udeg(udeg);
    const auto r         = static_cast<double>(radius);
    return core::Point2{centre.x + core::mm_round(r * t.cos), centre.y + core::mm_round(r * t.sin)};
}

// -------------------------------------------------------------- geometry ----

bool arc_from_bulge(core::Point2 a, core::Point2 b, double bulge, core::Point2& centre,
                    core::Mm& radius, bool& ccw) noexcept
{
    if (bulge == 0.0 || a == b) return false;
    // Translated to `a` before anything is multiplied (core.md R3).
    const double dx    = static_cast<double>(b.x - a.x);
    const double dy    = static_cast<double>(b.y - a.y);
    const double chord = std::sqrt(dx * dx + dy * dy);
    if (chord <= 0.0) return false;
    const double h = chord / 2.0;
    const double s = std::abs(bulge) * h; // sagitta
    if (s <= 1e-9) return false;          // a bulge too small to bend a millimetre
    const double r = (h * h + s * s) / (2.0 * s);
    // The centre sits on the chord's perpendicular bisector, `r - s` from the
    // chord's midpoint, on the side the arc bulges AWAY from.
    const double mx = dx / 2.0;
    const double my = dy / 2.0;
    const double nx = -dy / chord; // left normal of the chord
    const double ny = dx / chord;
    const double d  = r - s;
    ccw             = bulge > 0.0;
    // A counter-clockwise bulge bulges to the right of a→b, so its centre is to
    // the left; clockwise the other way round.
    const double sign = ccw ? 1.0 : -1.0;
    centre            = core::Point2{a.x + core::mm_round(mx + sign * d * nx),
                          a.y + core::mm_round(my + sign * d * ny)};
    radius            = core::mm_round(r);
    return radius > 0;
}

void ellipse_arc_points(core::Point2 centre, core::Point2 major_vec, core::Point2 minor_vec,
                        std::int64_t start_udeg, std::int64_t end_udeg,
                        std::vector<core::Point2>& out)
{
    std::int64_t sweep = end_udeg - start_udeg;
    while (sweep <= 0)
        sweep += core::kUDegFullCircle;
    // Segments at most 360/128 of a turn each, the circle's own density.
    const std::int64_t step_max = core::kUDegFullCircle / 128;
    const auto steps            = std::max(1, static_cast<int>((sweep + step_max - 1) / step_max));
    const auto ax = static_cast<double>(major_vec.x), ay = static_cast<double>(major_vec.y);
    const auto bx = static_cast<double>(minor_vec.x), by = static_cast<double>(minor_vec.y);
    for (int i = 0; i <= steps; ++i) {
        // The last point of a full turn is the first one again and is left out.
        if (i == steps && sweep == core::kUDegFullCircle) break;
        const std::int64_t at = start_udeg + (sweep * i) / steps;
        const core::SinCos t  = core::sin_cos_udeg(at);
        out.push_back(core::Point2{centre.x + core::mm_round(t.cos * ax + t.sin * bx),
                                   centre.y + core::mm_round(t.cos * ay + t.sin * by)});
    }
}

bool nurbs_points(int degree, const std::vector<double>& knots, const std::vector<Pt>& controls,
                  const std::vector<double>& weights, int samples, std::vector<Pt>& out)
{
    const std::size_t n = controls.size();
    const auto p        = static_cast<std::size_t>(degree);
    if (degree < 1 || n < p + 1 || knots.size() != n + p + 1 || samples < 1) return false;
    if (!weights.empty() && weights.size() != n) return false;

    // Homogeneous control points: (w·x, w·y, w). Non-rational is w = 1.
    std::vector<double> hx(n), hy(n), hw(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = weights.empty() ? 1.0 : weights[i];
        hx[i]          = controls[i].x * w;
        hy[i]          = controls[i].y * w;
        hw[i]          = w;
    }

    std::vector<double> dx(p + 1), dy(p + 1), dw(p + 1);
    for (std::size_t span = p; span < n; ++span) {
        const double u0 = knots[span];
        const double u1 = knots[span + 1];
        if (!(u1 > u0)) continue; // a zero-length span (a repeated knot) draws nothing
        const bool last = span == n - 1;
        for (int s = 0; s < samples + (last ? 1 : 0); ++s) {
            const double u =
                u0 + (u1 - u0) * (static_cast<double>(s) / static_cast<double>(samples));
            // de Boor: p rounds of convex combinations over the span's p+1 points.
            for (std::size_t j = 0; j <= p; ++j) {
                dx[j] = hx[span - p + j];
                dy[j] = hy[span - p + j];
                dw[j] = hw[span - p + j];
            }
            for (std::size_t r = 1; r <= p; ++r) {
                for (std::size_t j = p; j >= r; --j) {
                    const std::size_t i = span - p + j;
                    const double den    = knots[i + p - r + 1] - knots[i];
                    const double alpha  = den == 0.0 ? 0.0 : (u - knots[i]) / den;
                    dx[j]               = (1.0 - alpha) * dx[j - 1] + alpha * dx[j];
                    dy[j]               = (1.0 - alpha) * dy[j - 1] + alpha * dy[j];
                    dw[j]               = (1.0 - alpha) * dw[j - 1] + alpha * dw[j];
                }
            }
            if (dw[p] == 0.0) return false;
            out.push_back(Pt{dx[p] / dw[p], dy[p] / dw[p]});
        }
    }
    return !out.empty();
}

// ------------------------------------------------------------------ text ----

std::string expand_text_codes(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '%' && i + 2 < raw.size() && raw[i + 1] == '%') {
            const char c = raw[i + 2];
            if (c == 'd' || c == 'D') {
                out += "\xC2\xB0";
                i += 2;
                continue;
            }
            if (c == 'p' || c == 'P') {
                out += "\xC2\xB1";
                i += 2;
                continue;
            }
            if (c == 'c' || c == 'C') {
                out += "\xC3\x98";
                i += 2;
                continue;
            }
            if (c == '%') {
                out += '%';
                i += 2;
                continue;
            }
        }
        out += raw[i];
    }
    return out;
}

std::string strip_mtext(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '{' || c == '}') continue;
        if (c != '\\') {
            out += c;
            continue;
        }
        if (i + 1 >= raw.size()) break;
        const char k = raw[++i];
        switch (k) {
        case 'P': out += ' '; break; // paragraph
        case '\\':
        case '{':
        case '}': out += k; break;
        case '~': out += ' '; break; // non-breaking space
        case 'S': {                  // stacked: \S a^b; → "a b"
            std::string frac;
            while (i + 1 < raw.size() && raw[i + 1] != ';')
                frac += raw[++i];
            if (i + 1 < raw.size()) ++i; // the ';'
            for (char& f : frac)
                if (f == '^' || f == '/' || f == '#') f = ' ';
            out += frac;
            break;
        }
        case 'A':
        case 'C':
        case 'c':
        case 'f':
        case 'F':
        case 'H':
        case 'L':
        case 'l':
        case 'O':
        case 'o':
        case 'Q':
        case 'T':
        case 'W':
        case 'p':
            // A formatting code with an argument up to ';' — dropped whole.
            while (i + 1 < raw.size() && raw[i] != ';')
                ++i;
            break;
        default: break; // an unknown code: dropped, its letter too
        }
    }
    return expand_text_codes(out);
}

#ifdef KENTOS_HAVE_DXFRW

// ----------------------------------------------------------------- XDATA ----

namespace {
constexpr std::uint8_t kXdString = 1, kXdInteger = 2, kXdDouble = 3, kXdCoord = 4;

void put_double_bits(std::vector<std::uint8_t>& out, double d)
{
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    core::put_u64(out, u);
}

double get_double_bits(core::WireReader& r)
{
    const std::uint64_t u = r.u64();
    double d              = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}
} // namespace

std::vector<std::uint8_t> encode_xdata(const std::vector<std::shared_ptr<DRW_Variant>>& items)
{
    std::vector<std::uint8_t> out;
    for (const auto& v : items) {
        if (!v) continue;
        core::put_u16(out, static_cast<std::uint16_t>(v->code()));
        switch (v->type()) {
        case DRW_Variant::STRING: {
            const std::string s = v->content.s ? *v->content.s : std::string();
            core::put_u8(out, kXdString);
            core::put_u32(out, static_cast<std::uint32_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
            break;
        }
        case DRW_Variant::INTEGER:
            core::put_u8(out, kXdInteger);
            core::put_i64(out, v->content.i);
            break;
        case DRW_Variant::DOUBLE:
            core::put_u8(out, kXdDouble);
            put_double_bits(out, v->content.d);
            break;
        case DRW_Variant::COORD: {
            core::put_u8(out, kXdCoord);
            const DRW_Coord c = v->content.v ? *v->content.v : DRW_Coord{};
            put_double_bits(out, c.x);
            put_double_bits(out, c.y);
            put_double_bits(out, c.z);
            break;
        }
        default: break; // INVALID: nothing to keep
        }
    }
    return out;
}

std::vector<std::shared_ptr<DRW_Variant>> decode_xdata(std::span<const std::uint8_t> bytes)
{
    std::vector<std::shared_ptr<DRW_Variant>> out;
    core::WireReader r(bytes);
    while (r.remaining(3)) {
        const int code         = r.u16();
        const std::uint8_t typ = r.u8();
        auto v                 = std::make_shared<DRW_Variant>();
        switch (typ) {
        case kXdString: {
            if (!r.remaining(4)) return out;
            const std::uint32_t n = r.u32();
            if (!r.remaining(n)) return out;
            std::string s(reinterpret_cast<const char*>(bytes.data() + r.position()), n);
            r.skip(n);
            v->addString(code, s);
            break;
        }
        case kXdInteger:
            if (!r.remaining(8)) return out;
            v->addInt(code, static_cast<int>(r.i64()));
            break;
        case kXdDouble:
            if (!r.remaining(8)) return out;
            v->addDouble(code, get_double_bits(r));
            break;
        case kXdCoord: {
            if (!r.remaining(24)) return out;
            DRW_Coord c;
            c.x = get_double_bits(r);
            c.y = get_double_bits(r);
            c.z = get_double_bits(r);
            v->addCoord(code, c);
            break;
        }
        default: return out; // not ours: stop rather than guess
        }
        out.push_back(std::move(v));
    }
    return out;
}

DRW::Version drw_version_for_year(int year) noexcept
{
    switch (year) {
    case 2000: return DRW::AC1015;
    case 2004: return DRW::AC1018;
    case 2010: return DRW::AC1024;
    case 2013: return DRW::AC1027;
    case 2018: return DRW::AC1032;
    default: return DRW::AC1021;
    }
}

std::string acad_name(DRW::Version v)
{
    // Spelled out rather than searched in the library's map: that map is keyed by
    // pointer and iterates in no fixed order, and a transcript must read the same
    // on every run (core.md P11).
    switch (v) {
    case DRW::AC1009: return "AC1009";
    case DRW::AC1012: return "AC1012";
    case DRW::AC1014: return "AC1014";
    case DRW::AC1015: return "AC1015";
    case DRW::AC1018: return "AC1018";
    case DRW::AC1021: return "AC1021";
    case DRW::AC1024: return "AC1024";
    case DRW::AC1027: return "AC1027";
    case DRW::AC1032: return "AC1032";
    default: return "?";
    }
}

#endif

} // namespace dxf
} // namespace kentos::io
