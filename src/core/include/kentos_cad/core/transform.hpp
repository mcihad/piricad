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
#include <optional>
#include <span>
#include <vector>

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

// ---------------------------------------------------------------------------
// One transform, described rather than applied, and the ghost that previews it.
//
// WHY THE DESCRIPTION AND NOT A `Point2 -> Point2`. A curve has to ask questions
// a bare map cannot answer: how its radius changes, and whether its sweep is
// reversed. So the edit verbs carry a DESCRIPTION of what they are about to do
// and hand it to whoever needs it.
//
// WHY IT IS IN `core`. Two callers need it: the verb, which rewrites the
// document, and the canvas, which draws the ghost that promises what the click
// will do. The ghost used to be a TRANSLATION whatever the verb was — so
// DÖNDÜR, ÖLÇEKLE and AYNALA either had none at all or would have shown the
// objects sliding sideways while the command turned them. A preview that
// promises the wrong transform is worse than no preview: the user aims with it.
// Now both sides call `transformed`, and the ghost cannot disagree with the
// result because it is the same function.
// ---------------------------------------------------------------------------

/// What is being done to the coordinates.
struct Xform
{
    /// Which of the four transforms this is.
    enum class Kind : std::uint8_t { Translate, Rotate, Scale, Mirror };

    Kind kind{Kind::Translate}; ///< which transform
    Mm dx{0};                   ///< Translate: east component
    Mm dy{0};                   ///< Translate: north component
    Point2 base{};              ///< Rotate/Scale: the centre · Mirror: the axis's first point
    Point2 axis_b{};            ///< Mirror: the axis's second point
    SinCos turn{};              ///< Rotate: the turn
    double factor{1.0};         ///< Scale: the multiplier

    friend bool operator==(const Xform&, const Xform&) = default;
};

/// `p` under `x`.
Point2 transformed(const Xform& x, Point2 p);

/// How the cursor completes a ghost: which transform is being previewed.
enum class GhostKind : std::uint8_t {
    Translate, ///< the cursor is where the base point goes: TAŞI, KOPYALA
    Rotate,    ///< the cursor's direction from the base is the turn: DÖNDÜR
    Scale,     ///< the cursor's distance from the base, in METRES, is the factor: ÖLÇEKLE
    Mirror     ///< the cursor is the axis's second point: AYNALA
};

/// What a ghost needs beyond the points it is handed.
struct GhostSpec
{
    /// Which transform the cursor is completing.
    GhostKind kind{GhostKind::Translate};

    /// How many copies the ghost draws, the first being the original moved once.
    /// More than one only for a command that repeats a step.
    std::int64_t copies{1};

    friend constexpr bool operator==(const GhostSpec&, const GhostSpec&) = default;
};

/// Fixed 9-byte layout: kind (uint8), copies (int64). Fixed rather than
/// versioned because a ghost lives for the length of one prompt and is never
/// written to a file.
std::vector<std::uint8_t> encode_ghost_spec(const GhostSpec& spec);
std::optional<GhostSpec> decode_ghost_spec(std::span<const std::uint8_t> bytes);

/// The transform `cursor` implies for a ghost whose base point is `base`.
///
/// THE ONE PLACE THE CURSOR BECOMES A TRANSFORM, called by the verb when it
/// takes the pointed answer and by the canvas on every mouse move. That is what
/// makes the ghost exact rather than nearly right.
Xform ghost_xform(GhostKind kind, Point2 base, Point2 cursor);

/// The turn `base`->`cursor` makes, in whole micro-degrees counter-clockwise
/// from east — the unit `DÖNDÜR`'s own `aci` is written in, times
/// `kUDegPerDegree`. Split out because the verb records the ANGLE while the
/// ghost needs the `SinCos`, and both have to come from the same integer.
UDeg ghost_turn_udeg(Point2 base, Point2 cursor) noexcept;

/// The factor `base`->`cursor` implies: the distance between them in METRES,
/// which is how every CAD reads a dragged scale. Never negative; zero when the
/// cursor is on the base point, which the verb refuses.
double ghost_factor(Point2 base, Point2 cursor) noexcept;

} // namespace kentos::core
