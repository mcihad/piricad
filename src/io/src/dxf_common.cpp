// SPDX-License-Identifier: GPL-3.0-or-later
#include "dxf_common.hpp"

#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"
#include "kentos_cad/io/dxf.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

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

// -------------------------------------------------------------- patterns ----

namespace {

/// Drawing units in a millimetre, and millimetres in a drawing unit.
double units_per_mm(core::DrawingUnit unit) noexcept
{
    const core::UnitRatio r = core::drawing_unit_ratio(unit);
    return static_cast<double>(r.den) / static_cast<double>(r.num);
}

double mm_per_unit(core::DrawingUnit unit) noexcept
{
    const core::UnitRatio r = core::drawing_unit_ratio(unit);
    return static_cast<double>(r.num) / static_cast<double>(r.den);
}

double value_of(core::Ratio r) noexcept
{
    return r.den <= 0 || r.num <= 0 ? 1.0 : static_cast<double>(r.num) / static_cast<double>(r.den);
}

std::int64_t whole_turn(std::int64_t udeg) noexcept
{
    std::int64_t a = udeg % core::kUDegFullCircle;
    if (a < 0) a += core::kUDegFullCircle;
    return a;
}

/// Whether two angles are one, give or take the micro-degree a text file's
/// last digit is worth.
bool same_turn(std::int64_t a, std::int64_t b) noexcept
{
    const std::int64_t d = whole_turn(a - b);
    return d <= 2 || d >= core::kUDegFullCircle - 2;
}

/// A pattern of the user's own: DXF type 0, one family of lines.
bool user_pattern(const core::HatchDef& def) noexcept
{
    return def.pattern_type == 0 && def.families.size() == 1;
}

/// The largest pattern number held, in micrometres: a line a thousand
/// kilometres from the next is not a pattern, and the bound keeps every
/// rounding below inside `int64`.
constexpr double kPatternLimitUm = 1e15;

bool held(double v) noexcept
{
    return std::isfinite(v) && std::fabs(v) <= kPatternLimitUm;
}

/// A positive scale as a ratio in millionths, like every other scale the
/// reader keeps.
core::Ratio scale_ratio(double v) noexcept
{
    if (!std::isfinite(v) || !(v > 0.0)) return core::Ratio{1, 1};
    const auto num       = std::llround(std::min(v, 1e9) * 1'000'000.0);
    const std::int64_t g = std::gcd(num, std::int64_t{1'000'000});
    return num == 0 ? core::Ratio{1, 1'000'000} : core::Ratio{num / g, 1'000'000 / g};
}

/// Whether two numbers of a pattern are one: the text file's rounding, and
/// `slack` micrometres more.
bool near(std::int64_t a, std::int64_t b, double slack) noexcept
{
    const double d = std::fabs(static_cast<double>(a) - static_cast<double>(b));
    return d <= 2.0 + slack + 1e-9 * std::fabs(static_cast<double>(a));
}

} // namespace

double dxf_pattern_scale(const core::HatchDef& def, core::DrawingUnit unit) noexcept
{
    const double s = value_of(def.scale);
    // A user pattern's group 41 is its spacing, in drawing units.
    if (user_pattern(def))
        return std::fabs(static_cast<double>(def.families.front().offset_y_um)) * s / 1000.0 *
               units_per_mm(unit);
    // The catalogue's numbers are millimetres on paper; read as drawing units
    // and multiplied by this, they are the spacing on the ground.
    return s * units_per_mm(unit);
}

std::vector<PatternLine> pattern_lines(const core::HatchDef& def, core::DrawingUnit unit)
{
    std::vector<PatternLine> out;
    if (def.solid) return out;
    const double du = units_per_mm(unit);
    const double k  = value_of(def.scale) / 1000.0 * du; // pattern micrometres -> drawing units
    const core::SinCos turn = core::sin_cos_udeg(def.angle_udeg);
    const double ox         = static_cast<double>(def.origin.x) * du;
    const double oy         = static_cast<double>(def.origin.y) * du;
    const auto add          = [&](const core::HatchDef::Family& f, std::int64_t extra) {
        const std::int64_t phi  = whole_turn(f.angle_udeg + def.angle_udeg + extra);
        const core::SinCos line = core::sin_cos_udeg(phi);
        PatternLine l;
        l.angle_deg = degrees_from_udeg(phi);
        // The base point turns with the pattern; the offset with its line.
        const double bx = static_cast<double>(f.base_x_um) * k;
        const double by = static_cast<double>(f.base_y_um) * k;
        l.base_x        = ox + (bx * turn.cos - by * turn.sin);
        l.base_y        = oy + (bx * turn.sin + by * turn.cos);
        const double ax = static_cast<double>(f.offset_x_um) * k;
        const double ay = static_cast<double>(f.offset_y_um) * k;
        l.offset_x      = ax * line.cos - ay * line.sin;
        l.offset_y      = ax * line.sin + ay * line.cos;
        l.dashes.reserve(f.dashes_um.size());
        for (const std::int64_t d : f.dashes_um)
            l.dashes.push_back(static_cast<double>(d) * k);
        out.push_back(std::move(l));
    };
    for (const core::HatchDef::Family& f : def.families) {
        add(f, 0);
        if (def.double_lines) add(f, core::kUDegFullCircle / 4);
    }
    return out;
}

void apply_dxf_pattern_scale(core::HatchDef& def, double group41, core::DrawingUnit unit)
{
    const double g = std::isfinite(group41) && group41 > 0.0 ? std::min(group41, 1e9) : 1.0;
    if (def.pattern_type == 0) {
        // A user pattern: one family of lines, this far apart on the ground.
        core::HatchDef::Family lines;
        lines.offset_y_um = std::llround(std::min(g * mm_per_unit(unit) * 1000.0, kPatternLimitUm));
        def.scale         = core::Ratio{1, 1};
        def.families      = {lines};
        if (def.name.empty()) def.name = "_USER";
        return;
    }
    def.scale = scale_ratio(g * mm_per_unit(unit));
}

bool families_from_lines(core::HatchDef& def, std::span<const PatternLine> lines,
                         core::DrawingUnit unit, std::span<const core::HatchDef::Family> known)
{
    if (lines.empty() || def.solid) return false;
    const double s     = value_of(def.scale);
    const double mm    = mm_per_unit(unit);
    const double to_um = mm * 1000.0 / s; // drawing units -> pattern micrometres
    for (const PatternLine& l : lines) {
        if (!std::isfinite(l.angle_deg) || std::fabs(l.angle_deg) > 1e6) return false;
        for (const double v : {l.base_x, l.base_y})
            if (!held(v * mm * 1000.0)) return false;
        for (const double v : {l.offset_x, l.offset_y})
            if (!held(v * to_um)) return false;
        for (const double d : l.dashes)
            if (!held(d * to_um)) return false;
    }

    // THE DOUBLED HALF. A doubled hatch writes each family and then the same
    // family a quarter turn on; the model holds the first and the flag. Lines
    // that do not pair up are drawn as the file has them, and not doubled.
    std::vector<PatternLine> kept(lines.begin(), lines.end());
    if (def.double_lines) {
        bool paired = kept.size() % 2 == 0;
        for (std::size_t i = 0; paired && i < kept.size(); i += 2)
            paired = same_turn(udeg_from_degrees(kept[i + 1].angle_deg),
                               udeg_from_degrees(kept[i].angle_deg) + core::kUDegFullCircle / 4);
        if (paired) {
            std::vector<PatternLine> first;
            first.reserve(kept.size() / 2);
            for (std::size_t i = 0; i < kept.size(); i += 2)
                first.push_back(kept[i]);
            kept = std::move(first);
        } else {
            def.double_lines = false;
        }
    }

    const core::SinCos turn = core::sin_cos_udeg(def.angle_udeg);
    std::vector<core::HatchDef::Family> got;
    got.reserve(kept.size());
    for (const PatternLine& l : kept) {
        const std::int64_t phi  = whole_turn(udeg_from_degrees(l.angle_deg));
        const core::SinCos line = core::sin_cos_udeg(phi);
        core::HatchDef::Family f;
        f.angle_udeg = whole_turn(phi - def.angle_udeg);
        // The offset back in the line's own frame: along it, and across it.
        f.offset_x_um = std::llround((l.offset_x * line.cos + l.offset_y * line.sin) * to_um);
        f.offset_y_um = std::llround((-l.offset_x * line.sin + l.offset_y * line.cos) * to_um);
        f.dashes_um.reserve(l.dashes.size());
        for (const double d : l.dashes)
            f.dashes_um.push_back(std::llround(d * to_um));
        got.push_back(std::move(f));
    }

    // THE SAME PATTERN AS THE ONE KNOWN BY THIS NAME, line for line?
    bool same = known.size() == got.size();
    for (std::size_t i = 0; same && i < got.size(); ++i) {
        const core::HatchDef::Family& a = got[i];
        const core::HatchDef::Family& b = known[i];
        same = same_turn(a.angle_udeg, b.angle_udeg) && near(a.offset_x_um, b.offset_x_um, 0.0) &&
               near(a.offset_y_um, b.offset_y_um, 0.0) && a.dashes_um.size() == b.dashes_um.size();
        for (std::size_t d = 0; same && d < a.dashes_um.size(); ++d)
            same = near(a.dashes_um[d], b.dashes_um[d], 0.0);
    }

    // THE ORIGIN: where the pattern's own base point was set. For the known
    // pattern, the first line's base less that family's own, turned and scaled
    // — the point the other program was given; otherwise the first line's base.
    double wx = kept.front().base_x * mm;
    double wy = kept.front().base_y * mm;
    if (same) {
        const double bx = static_cast<double>(known.front().base_x_um) * s / 1000.0;
        const double by = static_cast<double>(known.front().base_y_um) * s / 1000.0;
        wx -= bx * turn.cos - by * turn.sin;
        wy -= bx * turn.sin + by * turn.cos;
    }
    def.origin = core::Point2{core::mm_round(wx), core::mm_round(wy)};

    // Every family's base from the origin, back in the pattern's frame.
    for (std::size_t i = 0; i < kept.size(); ++i) {
        const double dx  = kept[i].base_x * mm - static_cast<double>(def.origin.x);
        const double dy  = kept[i].base_y * mm - static_cast<double>(def.origin.y);
        got[i].base_x_um = std::llround((dx * turn.cos + dy * turn.sin) * 1000.0 / s);
        got[i].base_y_um = std::llround((-dx * turn.sin + dy * turn.cos) * 1000.0 / s);
    }

    // The known numbers themselves when the file's are those, to the rounding
    // of a text file and of the origin's millimetre: an equal pattern is an
    // equal payload, and the pattern keeps its name's meaning.
    const double origin_slack = 600.0 / s; // half a ground millimetre, in pattern micrometres
    for (std::size_t i = 0; same && i < got.size(); ++i)
        same = near(got[i].base_x_um, known[i].base_x_um, origin_slack) &&
               near(got[i].base_y_um, known[i].base_y_um, origin_slack);
    if (same)
        def.families.assign(known.begin(), known.end());
    else
        def.families = std::move(got);
    return true;
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
        // A PARAGRAPH IS A LINE (TODOS C-12): the text keeps its breaks, so a
        // three-line plan note arrives as three lines rather than one run.
        case 'P': out += '\n'; break;
        case '\\':
        case '{':
        case '}': out += k; break;
        case '~': out += ' '; break; // non-breaking space
        case 'S': {                  // stacked: \S a/b; → "a/b", \S a^b; → "a b"
            std::string frac;
            while (i + 1 < raw.size() && raw[i + 1] != ';')
                frac += raw[++i];
            if (i + 1 < raw.size()) ++i; // the ';'
            for (char& f : frac)
                if (f == '^')
                    f = ' ';
                else if (f == '#')
                    f = '/';
            out += frac;
            break;
        }
        // Underline, overline and strike-through switched on or off: no
        // argument, so nothing after them is theirs. Reading them as codes
        // with one up to ';' swallowed the words that followed.
        case 'L':
        case 'l':
        case 'O':
        case 'o':
        case 'K':
        case 'k': break;
        case 'A':
        case 'C':
        case 'c':
        case 'f':
        case 'F':
        case 'H':
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

std::string escape_mtext(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '\n': out += "\\P"; break;
        case '\\': out += "\\\\"; break;
        case '{': out += "\\{"; break;
        case '}': out += "\\}"; break;
        default: out += c; break;
        }
    }
    return out;
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
