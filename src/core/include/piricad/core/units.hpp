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

/// Deterministic round-half-away-from-zero. Identical on every platform;
/// std::llround is not constexpr and std::round's mode is not pinned.
///
/// The obvious `(int)(x + 0.5)` breaks its own contract, because the addition can
/// round before the truncation ever runs. Two cases, measured rather than argued:
///
///   * `(long long)(0x1.fffffffffffffp-2 + 0.5)` is 1. That input is the largest
///     double below one half, so the answer must be 0; the sum lands exactly on a
///     midpoint and ties-to-even carries it to 1.0.
///   * Reachable through this function at `mm_from_metres(4503599627370.497)`,
///     where the scaled value is already the integer 4503599627370497 and the
///     `+ 0.5` pushes it to ...498 — a whole millimetre invented at an input that
///     needed no rounding at all. Above 2^52 the gap between doubles is 1, so
///     adding a half can only sit on a tie.
///
/// That magnitude is 4.5e12 metres, far past anything terrestrial, so this is a
/// contract the old code broke rather than a pafta it got wrong. It is fixed
/// because a rounding primitive that is right only within the range someone
/// remembered to check is not a rounding primitive.
///
/// Comparing the fraction against one half never adds anything, so nothing can
/// round on the way. The subtraction is exact for every input in range.
constexpr Mm mm_from_metres(double metres) noexcept
{
    const double scaled  = metres * static_cast<double>(kMmPerMetre);
    const auto truncated = static_cast<Mm>(scaled); // toward zero
    const double frac    = scaled - static_cast<double>(truncated);

    if (frac >= 0.5) return truncated + 1;
    if (frac <= -0.5) return truncated - 1;
    return truncated;
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
