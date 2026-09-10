// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: moving, turning, mirroring and scaling coordinates.
//
// Every function here is DETERMINISTIC ACROSS PLATFORMS, and that is a
// requirement rather than a nicety: a rotated parcel's corners are stored
// coordinates, and §7.3 promises the same drawing comes out bit-identical on
// Linux, Windows and macOS. A transform that disagreed in the last bit would
// produce a different tapu on a different machine.
//
// That is why `sin_cos_udeg` exists instead of a call to libm. `std::sin` and
// `std::cos` are NOT correctly rounded and are not required to agree between
// platforms or even between versions of the same platform's libm. The
// implementation here uses argument reduction that is exact (a quadrant is an
// integer count of 90°) followed by a polynomial in +, * and / only — every one
// of which IEEE-754 pins exactly.
#pragma once

#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>

namespace kentos::core {

/// Angles are whole MICRO-DEGREES — `kUDegPerDegree` in units.hpp, the unit the
/// polar snap already uses. An integer angle keeps a right angle exactly a right
/// angle: 90° is 90 000 000, not a float that is nearly it.
using UDeg = std::int64_t;

// `SinCos` and `sin_cos_udeg` live in core/trig.hpp: ONE trigonometry, shared
// with the snap engine, the arc's perimeter and every kind that turns a point.
// This header once carried a second copy with a different series, and two
// deterministic functions that disagree in the last bit are one bug more than
// none.

/// `p` moved by `dx`, `dy`. Exact: integers added to integers.
constexpr Point2 translated(Point2 p, Mm dx, Mm dy) noexcept
{
    return Point2{p.x + dx, p.y + dy};
}

/// `p` turned about `base` by the angle whose sine and cosine are `t`.
///
/// The offset from the base is taken FIRST and the rotation applied to it, so the
/// magnitudes multiplied are the size of the object rather than the size of a
/// TUREF coordinate — the same reason every distance in this program is measured
/// after a translation to a local origin (core.md R3).
Point2 rotated_about(Point2 p, Point2 base, SinCos t);

/// `p` scaled about `base` by `factor`, which must be greater than zero.
///
/// A negative factor is refused by the commands rather than silently turning into
/// a rotation by half a turn: "scale by minus one" and "mirror" are different
/// intentions, and a user who typed the wrong sign should be told.
Point2 scaled_about(Point2 p, Point2 base, double factor);

/// `p` reflected in the line through `a` and `b`.
///
/// Exact when the axis is horizontal or vertical — which is the axis a surveyor
/// picks most of the time — because those two cases are integer negation and are
/// taken before any arithmetic that could round.
Point2 mirrored_in_line(Point2 p, Point2 a, Point2 b);

} // namespace kentos::core
