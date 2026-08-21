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
constexpr Mm mm_round(double v) noexcept
{
    return v >= 0.0 ? static_cast<Mm>(v + 0.5) : static_cast<Mm>(v - 0.5);
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
    Mm x{0};
    Mm y{0};

    friend constexpr auto operator<=>(const Point2&, const Point2&) = default;
};

constexpr Point2 operator+(Point2 a, Point2 b) noexcept
{
    return {a.x + b.x, a.y + b.y};
}

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
    Mm min_x{1};
    Mm min_y{1};
    Mm max_x{0};
    Mm max_y{0};

    constexpr bool empty() const noexcept { return min_x > max_x || min_y > max_y; }

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

    constexpr void extend(const Box2& o) noexcept
    {
        if (o.empty()) return;
        extend(Point2{o.min_x, o.min_y});
        extend(Point2{o.max_x, o.max_y});
    }

    constexpr Mm width() const noexcept { return empty() ? 0 : max_x - min_x; }

    constexpr Mm height() const noexcept { return empty() ? 0 : max_y - min_y; }

    constexpr Point2 centre() const noexcept
    {
        return empty() ? Point2{} : Point2{min_x + width() / 2, min_y + height() / 2};
    }

    friend constexpr bool operator==(const Box2&, const Box2&) = default;
};

} // namespace piricad::core
