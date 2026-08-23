// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: internal units and primitive geometry types.
//
// Constitution: coordinates are stored as int64 fixed-point millimetres.
// Never double, never float. See piricad.md §10.2 and .claude/core.md.
#pragma once

#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace piricad::core {

/// Internal storage unit: signed 64-bit fixed-point millimetres.
/// Range at 1 mm resolution is ±9.2e15 mm ≈ ±9.2e12 m — nine orders of
/// magnitude beyond any terrestrial coordinate. Exact, deterministic,
/// order-independent under addition.
using Mm = std::int64_t;

inline constexpr Mm kMmPerMetre = 1000;
inline constexpr Mm kMmInvalid  = std::numeric_limits<Mm>::min();

/// THE rounding helper (core.md R20). Deterministic round-half-away-from-zero,
/// identical on every platform: std::llround is not constexpr and std::round's
/// mode is not pinned. Every transient `double` that becomes an `Mm` goes through
/// this one function, so there is exactly one rounding rule in the product.
///
/// It does NOT compute `(v + 0.5)`, because that breaks its own contract: the
/// addition can round before the truncation ever runs. Two cases, measured rather
/// than argued:
///
///   * `(long long)(0x1.fffffffffffffp-2 + 0.5)` is 1. That input is the largest
///     double below one half, so the answer must be 0; the sum lands exactly on a
///     midpoint and ties-to-even carries it to 1.0.
///   * Reachable through mm_from_metres at 4503599627370.497 metres, where the
///     scaled value is already the integer 4503599627370497 and the `+ 0.5` pushes
///     it to ...498 — a whole millimetre invented at an input that needed no
///     rounding at all. Above 2^52 the gap between doubles is 1, so adding a half
///     can only sit on a tie.
///
/// That magnitude is 4.5e12 metres, far past anything terrestrial, so this is a
/// contract the old code broke rather than a pafta it got wrong. It is written
/// this way because a rounding primitive that is right only within the range
/// someone remembered to check is not a rounding primitive — and because R20 makes
/// this the single place every coordinate in the product is rounded.
///
/// Comparing the fraction against one half never adds anything, so nothing can
/// round on the way. The subtraction is exact for every input in range.
constexpr Mm mm_round(double v) noexcept
{
    const auto truncated = static_cast<Mm>(v); // toward zero
    const double frac    = v - static_cast<double>(truncated);

    if (frac >= 0.5) return truncated + 1;
    if (frac <= -0.5) return truncated - 1;
    return truncated;
}

/// Metres to millimetres, rounded by the R20 helper above.
constexpr Mm mm_from_metres(double metres) noexcept
{
    return mm_round(metres * static_cast<double>(kMmPerMetre));
}

// ------------------------------------------------------------- angles ------
//
// model.md R21: no stored field is floating point, and an angle IS a stored field
// the moment a setting carries it (core.yakalama.kutupsal_aci). Angles are
// therefore MICRO-DEGREES, int64: 45 degrees is 45'000'000, and a full circle
// divides exactly by 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 30, 36, 40,
// 45, 60, 72 and 90 with no remainder, so every polar step a surveyor asks for is
// representable exactly.

/// One degree in micro-degrees.
inline constexpr std::int64_t kUDegPerDegree = 1000000;

/// A full turn in micro-degrees.
inline constexpr std::int64_t kUDegFullCircle = 360 * kUDegPerDegree;

/// Micro-degrees to radians. Transient computation type only — never storage.
constexpr double udeg_to_radians(std::int64_t udeg) noexcept
{
    return static_cast<double>(udeg) *
           (3.14159265358979323846 / (180.0 * static_cast<double>(kUDegPerDegree)));
}

/// Millimetres back to metres. Transient computation type only — never storage.
constexpr double mm_to_metres(Mm v) noexcept
{
    return static_cast<double>(v) / static_cast<double>(kMmPerMetre);
}

/// Planar point in the document CRS, fixed-point millimetres.
struct Point2
{
    /// Easting. In Turkish surveying this is the `sağa değer` and it is called
    /// Y on a pafta, not X — EPSG:5254 declares that axis order. The member is
    /// named `x` because it is the first Cartesian axis in code; the labelling is
    /// a presentation concern and lives in /src/app (model.md R37a).
    Mm x{0};

    /// Northing — the `yukarı değer`, labelled X on a pafta. Same reasoning.
    Mm y{0};

    /// Ordering and equality, both exact because both members are integers. A
    /// point is a map key and a sort key in several places, and a floating-point
    /// coordinate could not be either safely.
    friend constexpr auto operator<=>(const Point2&, const Point2&) = default;
};

/// Vector addition and subtraction, exact in fixed point. Overflow is the
/// caller's concern: `RingGeometry` refuses any coordinate beyond
/// `kMmCoordinateLimit` precisely so that these stay in range.
constexpr Point2 operator+(Point2 a, Point2 b) noexcept
{
    return {a.x + b.x, a.y + b.y};
}

/// Vector subtraction; see the addition above.
constexpr Point2 operator-(Point2 a, Point2 b) noexcept
{
    return {a.x - b.x, a.y - b.y};
}

/// Squared distance in mm². Exact for coordinates below ~3e9 mm apart.
constexpr double distance_metres(Point2 a, Point2 b) noexcept
{
    const double dx = mm_to_metres(a.x - b.x);
    const double dy = mm_to_metres(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}

/// Axis-aligned bounding box. Empty is encoded as min > max.
struct Box2
{
    /// Defaults encode EMPTY as min > max, so a freshly constructed box extends to
    /// exactly the first point given to it. A box defaulting to all-zero would
    /// silently include the origin, and in TUREF the origin is a thousand
    /// kilometres from any Turkish parcel — every bounding box would span the
    /// country and every cull test would pass.
    Mm min_x{1};
    Mm min_y{1};
    Mm max_x{0};
    Mm max_y{0};

    /// Whether this box contains nothing at all.
    constexpr bool empty() const noexcept { return min_x > max_x || min_y > max_y; }

    /// Grows the box to include `p`. An empty box becomes exactly that point.
    constexpr void extend(Point2 p) noexcept
    {
        if (empty()) {
            min_x = max_x = p.x;
            min_y = max_y = p.y;
            return;
        }
        if (p.x < min_x) min_x = p.x;
        if (p.y < min_y) min_y = p.y;
        if (p.x > max_x) max_x = p.x;
        if (p.y > max_y) max_y = p.y;
    }

    /// Grows the box to include another. An empty argument is ignored rather than
    /// collapsing this box, so folding over a list with gaps in it works.
    constexpr void extend(const Box2& o) noexcept
    {
        if (o.empty()) return;
        extend(Point2{o.min_x, o.min_y});
        extend(Point2{o.max_x, o.max_y});
    }

    /// Extent along each axis, zero for an empty box — never the negative number
    /// the sentinel defaults would otherwise produce.
    constexpr Mm width() const noexcept { return empty() ? 0 : max_x - min_x; }

    constexpr Mm height() const noexcept { return empty() ? 0 : max_y - min_y; }

    /// The middle, rounded toward the lower corner by integer division. Exact and
    /// identical on every platform, which a floating-point midpoint would not be.
    constexpr Point2 centre() const noexcept
    {
        return empty() ? Point2{} : Point2{min_x + width() / 2, min_y + height() / 2};
    }

    /// Exact equality. Two empty boxes with different sentinel values are NOT
    /// equal, which is deliberate: the encoding is data and comparing it is how a
    /// round-trip test catches a reader that invented its own empty.
    friend constexpr bool operator==(const Box2&, const Box2&) = default;
};

} // namespace piricad::core
