// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/transform.hpp"

namespace piricad::core {
namespace {

/// pi/4 in radians, and the factor that turns a micro-degree into a radian.
/// Written as literals so no libm constant and no run-time division decides them.
constexpr double kRadPerUDeg = 3.14159265358979323846 / (180.0 * 1'000'000.0);

/// sin(x) for x in [0, pi/4], from its Taylor series in u = x².
///
/// sin x / x = 1 - u/3! + u²/5! - u³/7! + ... , evaluated in Horner form so the
/// whole thing is multiplications and additions and every rounding is pinned by
/// IEEE-754 — never libm, which is not required to agree between platforms
/// (§7.3). Nine terms: at the top of the range the first term left out is under
/// 1e-19, which is far below the last bit of a double, so the series is
/// exhausted rather than truncated.
double sin_small(double x)
{
    const double u = x * x;
    double s       = 1.0 / 355687428096000.0; // u^8 / 17!
    s              = s * u - 1.0 / 1307674368000.0;
    s              = s * u + 1.0 / 6227020800.0;
    s              = s * u - 1.0 / 39916800.0;
    s              = s * u + 1.0 / 362880.0;
    s              = s * u - 1.0 / 5040.0;
    s              = s * u + 1.0 / 120.0;
    s              = s * u - 1.0 / 6.0;
    s              = s * u + 1.0;
    return x * s;
}

/// cos(x) for x in [0, pi/4]: 1 - u/2! + u²/4! - u³/6! + ... , to the same depth.
double cos_small(double x)
{
    const double u = x * x;
    double c       = -1.0 / 6402373705728000.0; // u^9 / 18!
    c              = c * u + 1.0 / 20922789888000.0;
    c              = c * u - 1.0 / 87178291200.0;
    c              = c * u + 1.0 / 479001600.0;
    c              = c * u - 1.0 / 3628800.0;
    c              = c * u + 1.0 / 40320.0;
    c              = c * u - 1.0 / 720.0;
    c              = c * u + 1.0 / 24.0;
    c              = c * u - 1.0 / 2.0;
    c              = c * u + 1.0;
    return c;
}

} // namespace

SinCos sin_cos_udeg(UDeg angle)
{
    // Fold into one turn. INTEGER arithmetic, so a right angle stays exactly a
    // right angle however many turns away it was written.
    UDeg a = angle % kUDegFullCircle;
    if (a < 0) a += kUDegFullCircle;

    const UDeg quarter = 90 * kUDegPerDegree;
    const int quadrant = static_cast<int>(a / quarter);
    UDeg within        = a % quarter;

    // Into the first OCTANT, where the series converges fastest, by the identity
    // sin(90° - t) = cos(t). The reduction is exact: it is a subtraction of
    // integers.
    bool swap = false;
    if (within > quarter / 2) {
        within = quarter - within;
        swap   = true;
    }

    const double x = static_cast<double>(within) * kRadPerUDeg;
    double s       = sin_small(x);
    double c       = cos_small(x);
    if (swap) {
        const double t = s;
        s              = c;
        c              = t;
    }

    // The quadrant decides the signs and the swap, and both are exact: an axis
    // angle comes out as exactly 0 and exactly 1, never 6.1e-17 and 0.99999.
    switch (quadrant) {
    case 0: return SinCos{s, c};
    case 1: return SinCos{c, -s};
    case 2: return SinCos{-s, -c};
    default: return SinCos{-c, s};
    }
}

Point2 rotated_about(Point2 p, Point2 base, SinCos t)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * t.cos - dy * t.sin),
                  base.y + mm_round(dx * t.sin + dy * t.cos)};
}

Point2 scaled_about(Point2 p, Point2 base, double factor)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * factor), base.y + mm_round(dy * factor)};
}

Point2 mirrored_in_line(Point2 p, Point2 a, Point2 b)
{
    const Mm ax = b.x - a.x;
    const Mm ay = b.y - a.y;

    // THE TWO AXES A SURVEYOR ACTUALLY PICKS, handled exactly. A reflection in a
    // horizontal or vertical line is an integer negation; routing it through the
    // general formula would round a coordinate that had an exact answer.
    if (ay == 0 && ax != 0) return Point2{p.x, 2 * a.y - p.y};
    if (ax == 0 && ay != 0) return Point2{2 * a.x - p.x, p.y};

    const auto vx = static_cast<double>(ax);
    const auto vy = static_cast<double>(ay);
    const double len2 = vx * vx + vy * vy;
    if (len2 <= 0.0) return p; // a line through one point reflects nothing

    const auto px = static_cast<double>(p.x - a.x);
    const auto py = static_cast<double>(p.y - a.y);

    // The reflection of a vector in a line through the origin, written so the
    // only division is by the axis's squared length.
    const double t  = (px * vx + py * vy) / len2;
    const double rx = 2.0 * t * vx - px;
    const double ry = 2.0 * t * vy - py;

    return Point2{a.x + mm_round(rx), a.y + mm_round(ry)};
}

} // namespace piricad::core
