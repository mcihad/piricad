// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: internal units and primitive geometry types.
//
// Constitution: coordinates are stored as int64 fixed-point millimetres.
// Never double, never float. See kentoscad.md §10.2 and .claude/core.md.
#pragma once

#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace kentos::core {

/// Internal storage unit: signed 64-bit fixed-point millimetres.
/// Range at 1 mm resolution is ±9.2e15 mm ≈ ±9.2e12 m — nine orders of
/// magnitude beyond any terrestrial coordinate. Exact, deterministic,
/// order-independent under addition.
using Mm = std::int64_t;

inline constexpr Mm kMmPerMetre = 1000;
inline constexpr Mm kMmInvalid  = std::numeric_limits<Mm>::min();

/// The width every exact product of two `Mm` needs, and the ONE place this
/// project names a compiler extension.
///
/// WHY 128 BITS ARE NOT OPTIONAL. A shoelace term, a cross product and a
/// point-in-ring test all multiply two coordinate DIFFERENCES, and a difference
/// is an `Mm` — 64 bits. Their product is 128. Doing it in `double` would be the
/// `-ffast-math` mistake in another costume: a parcel's area is a legal figure
/// (§12) and Article 2.5 pins the arithmetic precisely so two machines agree
/// bit for bit.
///
/// WHY IT IS SPELLED ONCE. `__int128` is not ISO C++, so `-Wpedantic` reports
/// every declaration that spells it — twelve of them across four files — and
/// CLAUDE.md 5.14 forbids answering that with `-Wno-*`. Naming the extension in
/// one declaration, with `__extension__` where the compiler expects to be told
/// "yes, deliberately", leaves every call site written in ordinary C++ and the
/// warning class fully armed everywhere else.
///
#ifdef __SIZEOF_INT128__
__extension__ using Int128 = __int128; ///< exact 128-bit integer; see the note above
#else
#error "Exact geometry needs a 128-bit integer; this compiler has none (core.md R3)."
#endif

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

// ------------------------------------------------------- drawing units ----

/// The unit a drawing FILE writes its numbers in.
///
/// NOT A STORAGE UNIT. The document stores millimetres and nothing else (R2); a
/// `DrawingUnit` exists only at the file boundary, where a DXF's `$INSUNITS` or
/// the project setting `core.cizim.birim` says what the numbers in the file mean.
/// A millimetre-authored DXF read as metres arrives a thousand times too large,
/// which is exactly the silent failure this type is here to name.
///
/// The first three values ARE the indices of the `core.cizim.birim` enum list —
/// `milimetre, santimetre, metre` (settings.cpp) — and MUST NOT be reordered: a
/// saved setting is an index.
enum class DrawingUnit : std::uint8_t {
    Millimetre = 0,
    Centimetre = 1,
    Metre      = 2,
    Inch       = 3,
    Foot       = 4,
    Kilometre  = 5,
    Micron     = 6,
    Decimetre  = 7,
    Decametre  = 8,
    Hectometre = 9,
};

/// Millimetres per drawing unit as an exact rational, so a unit that is not a
/// whole number of millimetres (an inch is 25.4) is still stated exactly.
struct UnitRatio
{
    std::int64_t num{1}; ///< millimetres …
    std::int64_t den{1}; ///< … per `den` units
};

/// The exact ratio of one drawing unit to a millimetre.
constexpr UnitRatio drawing_unit_ratio(DrawingUnit unit) noexcept
{
    switch (unit) {
    case DrawingUnit::Millimetre: return {1, 1};
    case DrawingUnit::Centimetre: return {10, 1};
    case DrawingUnit::Metre: return {1000, 1};
    case DrawingUnit::Inch: return {254, 10};
    case DrawingUnit::Foot: return {3048, 10};
    case DrawingUnit::Kilometre: return {1000000, 1};
    case DrawingUnit::Micron: return {1, 1000};
    case DrawingUnit::Decimetre: return {100, 1};
    case DrawingUnit::Decametre: return {10000, 1};
    case DrawingUnit::Hectometre: return {100000, 1};
    }
    return {1000, 1};
}

/// A file value in `unit` to millimetres: one multiply, one division and the ONE
/// rounding helper (R20). For metres this is `value * 1000.0 / 1.0`, which is
/// bit-identical to `mm_from_metres`, so a geodetic format read through this
/// function rounds exactly as it always did.
constexpr Mm mm_from_drawing_units(double value, DrawingUnit unit) noexcept
{
    const UnitRatio r = drawing_unit_ratio(unit);
    return mm_round(value * static_cast<double>(r.num) / static_cast<double>(r.den));
}

/// Millimetres to a file value in `unit`. A transient `double` for a writer;
/// never stored (R3).
constexpr double drawing_units_from_mm(Mm mm, DrawingUnit unit) noexcept
{
    const UnitRatio r = drawing_unit_ratio(unit);
    return static_cast<double>(mm) * static_cast<double>(r.den) / static_cast<double>(r.num);
}

/// The unit a `core.cizim.birim` setting index names. Anything outside the three
/// the setting declares is read as metres, which is the setting's own default.
constexpr DrawingUnit drawing_unit_from_setting(std::uint16_t index) noexcept
{
    return index <= 2 ? static_cast<DrawingUnit>(index) : DrawingUnit::Metre;
}

/// The Turkish name of a unit, for a note: `milimetre`, `santimetre`, `metre`, …
constexpr const char* drawing_unit_name(DrawingUnit unit) noexcept
{
    switch (unit) {
    case DrawingUnit::Millimetre: return "milimetre";
    case DrawingUnit::Centimetre: return "santimetre";
    case DrawingUnit::Metre: return "metre";
    case DrawingUnit::Inch: return "inç";
    case DrawingUnit::Foot: return "fit";
    case DrawingUnit::Kilometre: return "kilometre";
    case DrawingUnit::Micron: return "mikrometre";
    case DrawingUnit::Decimetre: return "desimetre";
    case DrawingUnit::Decametre: return "dekametre";
    case DrawingUnit::Hectometre: return "hektometre";
    }
    return "metre";
}

/// A rational scale — a block reference's `sx`, a hatch pattern's scale, a paper
/// scale — stored as two integers because no stored field is floating point
/// (model.md R21). `den` is never zero.
struct Ratio
{
    std::int64_t num{1}; ///< numerator
    std::int64_t den{1}; ///< denominator, positive

    /// Equal when both terms are equal; 1/2 and 2/4 are two ratios.
    friend constexpr bool operator==(const Ratio&, const Ratio&) noexcept = default;
};

/// `v * num / den`, rounded half away from zero, with the product carried in 128
/// bits so a TUREF coordinate times a scale cannot wrap (§7.3). `den` must be
/// positive; the sign lives in `num` and `v`. This is the one multiply a stored
/// coordinate goes through when it is scaled, and it is exact where a `double`
/// product of two large integers is not.
constexpr std::int64_t mul_div_round(std::int64_t v, std::int64_t num, std::int64_t den) noexcept
{
    const Int128 product = static_cast<Int128>(v) * static_cast<Int128>(num);
    const Int128 d       = static_cast<Int128>(den);
    // Truncating division, then the half step towards the sign of the product.
    const Int128 q   = product / d;
    const Int128 r   = product - q * d;
    const Int128 two = 2;
    if (r >= 0) return static_cast<std::int64_t>(two * r >= d ? q + 1 : q);
    return static_cast<std::int64_t>(two * (-r) >= d ? q - 1 : q);
}

} // namespace kentos::core
