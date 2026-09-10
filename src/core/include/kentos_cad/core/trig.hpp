// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: deterministic sine and cosine over micro-degrees.
//
// WHY THIS EXISTS, because "we wrote our own trigonometry" needs a reason.
//
// kentoscad.md §7.3 and core.md R9 require bit-identical results across Linux,
// Windows and macOS. `std::sin`, `std::cos` and `std::atan2` cannot give that:
// IEEE-754 does not specify them, every libm implements them differently, and two
// conforming platforms may return results one ulp apart. That is invisible in a
// screen pixel and visible in a golden fixture — and a golden fixture that
// disagrees across platforms is, in this product, a legal defect.
//
// This is NOT a claim that the hand-rolled version is better than libm. It is not;
// libm is faster and at least as accurate. It is a claim about REPRODUCIBILITY:
// what this file computes is the same everywhere, because it uses nothing but the
// four operations IEEE-754 DOES specify exactly — add, subtract, multiply,
// divide — with `-ffp-contract=off` mandated by CLAUDE.md 2.5 so no fused
// multiply-add can round an intermediate differently on one machine.
//
// CLAUDE.md 5.16 says not to hand-roll what a mature library does well, and the
// exception is recorded in /NOTICE with this reasoning: no cross-platform library
// provides a correctly-rounded or even bit-reproducible sin/cos. (crlibm and
// core-math do, and neither is packaged for all three platforms today; when one
// is, this file is deleted rather than kept.)
//
// ACCURACY, measured against glibc rather than asserted: the worst disagreement
// over the whole circle is 1.4e-15, a few ulps of a double. At a ten-kilometre
// radius that is 1.4e-8 mm — seven orders below the stored millimetre — so the
// rounded coordinate is exact for every distance this product stores.
//
// Getting there took two corrections that a written claim would have hidden. The
// first version had a mistyped cosine coefficient and was wrong by 5e-12; the
// second had the right coefficients and too few of them, and was wrong by 4e-13,
// which is exactly the truncation error of the series at π/4. Both were found by
// comparing against libm, which is why that comparison is a test and not a
// one-off script.
#pragma once

#include "kentos_cad/core/units.hpp"

#include <cstdint>

namespace kentos::core {

/// π to the precision a double can hold. Written out rather than computed so the
/// constant is the same token on every platform and in every build.
inline constexpr double kPi = 3.14159265358979323846;

/// Sine and cosine of one angle, computed together because the argument
/// reduction below produces both and computing them separately would repeat it.
struct SinCos
{
    double sin{0.0}; ///< sine of the angle
    double cos{1.0}; ///< cosine of the angle; 1 for the default zero angle
};

namespace detail {

/// sin(x) on [-π/4, π/4] by its Taylor series, truncated where the next term is
/// already below a double's precision at this argument.
///
/// Horner order is FIXED and load-bearing: floating-point addition is not
/// associative, so evaluating the same polynomial in another order gives another
/// answer. Reordering these lines changes the output of the program.
constexpr double sin_poly(double x) noexcept
{
    const double z = x * x;
    return x + x * z *
                   (-1.6666666666666666e-01 +                                   // -1/3!
                    z * (8.3333333333333332e-03 +                               //  1/5!
                         z * (-1.9841269841269841e-04 +                         // -1/7!
                              z * (2.7557319223985893e-06 +                     //  1/9!
                                   z * (-2.5052108385441720e-08 +               // -1/11!
                                        z * (1.6059043836821613e-10 +           //  1/13!
                                             z * -7.6471637318198164e-13)))))); // -1/15!
}

/// cos(x) on [-π/4, π/4]. Same construction and the same fixed order.
constexpr double cos_poly(double x) noexcept
{
    const double z = x * x;
    return 1.0 - z * (5.0000000000000000e-01 +                                   //  1/2!
                      z * (-4.1666666666666664e-02 +                             // -1/4!
                           z * (1.3888888888888889e-03 +                         //  1/6!
                                z * (-2.4801587301587302e-05 +                   // -1/8!
                                     z * (2.7557319223985893e-07 +               //  1/10!
                                          z * (-2.0876756987868100e-09 +         // -1/12!
                                               z * 1.1470745597729725e-11)))))); // 1/14!
}

} // namespace detail

/// Sine and cosine of an angle given in MICRO-DEGREES.
///
/// Micro-degrees rather than radians on purpose (model.md R21): the angle is a
/// stored value, stored values are never floating point, and a full turn divides
/// exactly by every step a surveyor asks for. The conversion to radians happens
/// once, here, on an integer that is exact.
///
/// Quadrant reduction is INTEGER arithmetic: exact by construction, where reducing
/// a large radian argument in floating point is the classic place a libm loses
/// digits. Only the final octant — at most 45° — ever reaches a polynomial.
constexpr SinCos sin_cos_udeg(std::int64_t udeg) noexcept
{
    // Into [0, 360°) exactly, whatever the caller passed.
    std::int64_t a = udeg % kUDegFullCircle;
    if (a < 0) a += kUDegFullCircle;

    // Quadrant, then the offset inside it. Both exact.
    const std::int64_t quarter = kUDegFullCircle / 4; // 90°
    const std::int64_t q       = a / quarter;
    const std::int64_t rest    = a - q * quarter;

    // Fold the second half of a quadrant onto the first, so the polynomial only
    // ever sees [0, 45°] where it is at its most accurate.
    const std::int64_t eighth = quarter / 2; // 45°
    const bool folded         = rest > eighth;
    const std::int64_t inner  = folded ? quarter - rest : rest;

    // One multiplication, correctly rounded, on an exact integer.
    const double radians = static_cast<double>(inner) * (kPi / (180.0 * 1000000.0));

    const double s = detail::sin_poly(radians);
    const double c = detail::cos_poly(radians);

    // Undo the fold, then place into the quadrant. Sign flips and swaps only —
    // no arithmetic, so nothing rounds here.
    const double fs = folded ? c : s;
    const double fc = folded ? s : c;

    switch (q) {
    case 0: return SinCos{fs, fc};
    case 1: return SinCos{fc, -fs};
    case 2: return SinCos{-fs, -fc};
    default: return SinCos{-fc, fs};
    }
}

namespace detail {

/// atan(t) on [0, tan(π/8)] by its Taylor series, twelve terms in fixed Horner
/// order. At the fold point the next term is below 1e-16, a few ulps.
constexpr double atan_poly(double t) noexcept
{
    const double z = t * t;
    return t *
           (1.0 +
            z * (-3.3333333333333333e-01 +
                 z * (2.0000000000000000e-01 +
                      z * (-1.4285714285714285e-01 +
                           z * (1.1111111111111111e-01 +
                                z * (-9.0909090909090912e-02 +
                                     z * (7.6923076923076927e-02 +
                                          z * (-6.6666666666666666e-02 +
                                               z * (5.8823529411764705e-02 +
                                                    z * (-5.2631578947368418e-02 +
                                                         z * (4.7619047619047616e-02 +
                                                              z * -4.3478260869565216e-02)))))))))));
}

/// tan(π/8) = √2 − 1, the fold point of `atan_unit`.
inline constexpr double kTanEighth = 0.41421356237309503;

/// atan(t) on [0, 1], in radians. Arguments past tan(π/8) are folded through
/// atan(t) = π/8 + atan((t − c)/(1 + t·c)) with c = tan(π/8), so the series only
/// ever sees [0, tan(π/16)] where twelve terms carry it to the last bit.
constexpr double atan_unit(double t) noexcept
{
    if (t <= kTanEighth) return atan_poly(t);
    return kPi / 8.0 + atan_poly((t - kTanEighth) / (1.0 + t * kTanEighth));
}

} // namespace detail

/// The direction of the vector (dx, dy) in MICRO-DEGREES, counter-clockwise from
/// due east, in [0, 360°). The zero vector answers 0.
///
/// Deterministic on every platform where `std::atan2` is not: the octant is
/// found by INTEGER comparisons, so the four axes and the four diagonals come
/// out exact — 90 000 000, never 89 999 999 — and only the residue inside one
/// octant reaches a polynomial, in one fixed evaluation order. The rounding to a
/// whole micro-degree happens once, in the first octant, before the exact
/// integer reflections place the result. Half a micro-degree at ten kilometres
/// is a tenth of a millimetre, under the storage unit.
constexpr std::int64_t atan2_udeg(std::int64_t dy, std::int64_t dx) noexcept
{
    if (dx == 0 && dy == 0) return 0;

    // Magnitudes as unsigned so INT64_MIN cannot overflow the negation.
    const std::uint64_t ax =
        dx < 0 ? 0u - static_cast<std::uint64_t>(dx) : static_cast<std::uint64_t>(dx);
    const std::uint64_t ay =
        dy < 0 ? 0u - static_cast<std::uint64_t>(dy) : static_cast<std::uint64_t>(dy);

    // Into the first octant: the smaller over the larger is at most one.
    const bool steep = ay > ax;
    const double t   = steep ? static_cast<double>(ax) / static_cast<double>(ay)
                             : static_cast<double>(ay) / static_cast<double>(ax);
    const double rad = detail::atan_unit(t);

    // Round ONCE, here, where the angle is at most 45° and never negative. Spelled
    // out rather than `std::llround`, which is not constexpr.
    const double udeg_d  = rad * (180.0 * 1000000.0 / kPi);
    const auto truncated = static_cast<std::int64_t>(udeg_d);
    const std::int64_t whole =
        (udeg_d - static_cast<double>(truncated)) >= 0.5 ? truncated + 1 : truncated;
    const std::int64_t quarter = kUDegFullCircle / 4;

    // Exact reflections back into place.
    std::int64_t a = steep ? quarter - whole : whole;
    if (dx < 0) a = 2 * quarter - a;
    if (dy < 0) a = kUDegFullCircle - a;
    if (a >= kUDegFullCircle) a -= kUDegFullCircle;
    return a;
}

/// `p` turned about `base` by `udeg` counter-clockwise.
///
/// A multiple of a quarter turn is an integer swap and negation — exact, so a
/// block inserted at 90° lands on the millimetre it was drawn on. Anything else
/// goes through `sin_cos_udeg` and one `mm_round` per coordinate.
constexpr Point2 rotate_udeg(Point2 p, Point2 base, std::int64_t udeg) noexcept
{
    std::int64_t a = udeg % kUDegFullCircle;
    if (a < 0) a += kUDegFullCircle;
    const std::int64_t quarter = kUDegFullCircle / 4;
    const Mm dx                = p.x - base.x;
    const Mm dy                = p.y - base.y;
    if (a % quarter == 0) {
        switch (a / quarter) {
        case 0: return p;
        case 1: return Point2{base.x - dy, base.y + dx};
        case 2: return Point2{base.x - dx, base.y - dy};
        default: return Point2{base.x + dy, base.y - dx};
        }
    }
    const SinCos t = sin_cos_udeg(a);
    const auto fx  = static_cast<double>(dx);
    const auto fy  = static_cast<double>(dy);
    return Point2{base.x + mm_round(fx * t.cos - fy * t.sin),
                  base.y + mm_round(fx * t.sin + fy * t.cos)};
}

/// Square millimetres, spelled here as geometry.hpp spells it, so this header
/// stays below the geometry it serves.
using Mm2 = std::int64_t;

/// The area of the circular segment cut from a circle of radius `radius` by a
/// chord subtending `sweep_udeg` (0 < sweep < 360°), in square millimetres:
/// r²(θ − sin θ)/2. What an arc-polyline's bulged edge adds to, or takes from,
/// the polygon of its vertices (`alan hesabı`, model.md R12).
constexpr Mm2 circular_segment_area(Mm radius, std::int64_t sweep_udeg) noexcept
{
    const double r     = static_cast<double>(radius);
    const double theta = static_cast<double>(sweep_udeg) * (kPi / (180.0 * 1000000.0));
    const double s     = sin_cos_udeg(sweep_udeg).sin;
    const double area  = r * r * (theta - s) / 2.0;
    // Half away from zero, spelled out: `std::llround` is not constexpr.
    const auto truncated = static_cast<Mm2>(area);
    const double frac    = area - static_cast<double>(truncated);
    if (frac >= 0.5) return truncated + 1;
    if (frac <= -0.5) return truncated - 1;
    return truncated;
}

} // namespace kentos::core
