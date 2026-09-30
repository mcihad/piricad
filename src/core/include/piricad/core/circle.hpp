// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: what a circle is, and how one is drawn.
//
// A CIRCLE IS ITS DEFINITION, NOT ITS PICTURE. `RingGeometry` holds one Open ring
// of exactly two vertices:
//
//     vertex 0 = the centre
//     vertex 1 = {centre.x + radius, centre.y}
//
// so the radius reads back as `xs[1] - xs[0]` — an exact integer subtraction, no
// square root, no rounding. A circle stored as the polygon it looks like would
// have a circumference that is not 2·pi·r and an area that is not pi·r², and on a
// cadastral sheet those are the numbers that reach the tapu (§12).
//
// Those two vertices are geometry the arena already understands, which is what
// lets a circle be saved, loaded, culled and bounded by the same columns a line
// uses. What tells a circle from a two-point line is `entities.kind` — that column
// is exactly why it exists (model.md R22–R26).
//
// This header, rather than `entity_kind.hpp`, is where a caller asks "where is
// this circle and how big is it". That is not tidiness: `KindSpec` has a member
// called `emit`, `emit` is a Qt macro, and including that header from a Qt
// translation unit fails with `expected unqualified-id` on a line that looks
// perfectly good.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace kentos::core {

/// The radius of the circle in `slot`, in millimetres. Never negative.
Mm circle_radius_of(const RingGeometry& geom, std::uint32_t slot);

/// The centre of the circle in `slot`.
Point2 circle_centre_of(const RingGeometry& geom, std::uint32_t slot);

/// How many vertices `circle_outline` produces. Fixed, because `emit` is handed
/// no view and a kind must not need one to describe itself.
inline constexpr std::size_t kCircleSegments = 128;

/// The radius `centre`->`p` implies, rounded to the millimetre a record stores.
///
/// ONE ARITHMETIC FOR EVERY RADIUS THAT IS POINTED AT — DAİRE's rim, YAY's
/// first end, a sector's edge, and the canvas guides that preview them. There
/// were five copies of it; a guide computed a second way is a guide that is
/// eventually wrong. Metres before squaring: the square of a TM3 coordinate
/// difference in millimetres leaves the 53-bit mantissa long before it leaves
/// int64 (core.md R3).
Mm radius_through(Point2 centre, Point2 p) noexcept;

/// Appends the DRAWN form of a circle — a 128-gon, without a repeated closing
/// vertex, counter-clockwise from due east.
///
/// A 128-gon's greatest departure from the true circle is r·(1 − cos(pi/128)),
/// about 0,03 % of the radius: 1,5 mm at 5 m, 9 cm on a 300 m curve. That is
/// fine for a picture — sub-pixel at any zoom that shows the whole circle —
/// and it is the PICTURE only: the area and the radius come from the
/// definition, and a file that cannot hold a circle receives it within the
/// project's chord tolerance instead (`stroke_curve`, TODOS F-03). Precomputed
/// LOD replaces the fixed count when `render.md` R4 lands.
void circle_outline(Point2 centre, Mm radius, std::vector<Mm>& xs, std::vector<Mm>& ys);

/// The `(cos t, sin t)` table `circle_outline` walks: `kCircleSegments` unit
/// directions, built by repeated EXACT bisection of the four axis directions.
///
/// Exposed because the ellipse is the same table scaled along two axis vectors,
/// and building a second table would be building a second answer — the two
/// curves would then disagree about where a quadrant is (§7.3).
const std::vector<std::pair<double, double>>& unit_circle();

// ---------------------------------------------------------------------------
// The constructions a circle arrives by, and the guide that previews them.
//
// WHY THESE ARE HERE AND NOT IN THE COMMAND. The canvas has to draw the guide
// that promises what the next click will make, and it can only do that by
// computing the very circle the command will compute. `DAİRE yontem=ttr` is the
// case that forces it: there are FOUR circles of a given radius tangent to two
// crossing lines, the user picks one by pointing at a corner, and a preview that
// guessed differently from the command would put the fillet on the wrong corner
// of the junction while showing the right one.
// ---------------------------------------------------------------------------

/// The centre of the circle of `radius` tangent to both lines and nearest to
/// `near`, which is how the wanted one of the four is chosen.
///
/// The four centres are the crossings of the two lines offset by the radius,
/// each line offset to both sides. False when either line is degenerate or the
/// two are parallel — in which case no such circle exists and the caller says so
/// rather than drawing one.
bool tangent_circle_centre(Point2 a1, Point2 a2, Point2 b1, Point2 b2, Mm radius, Point2 near,
                           Point2& centre) noexcept;

/// Which construction made the circle. All four methods of `DAİRE` are here so
/// that the arithmetic has ONE home: three of them are also what a guide
/// previews, and `Centre` is here because the command would otherwise keep its
/// own copy of the same distance.
enum class CircleBuild : std::uint8_t {
    Diameter,   ///< `2n`: the two ends of a diameter — the chain holds the first
    ThreePoint, ///< `3n`: three points on the rim — the chain holds the first two
    Tangent,    ///< `ttr`: the chain holds the two lines' four points, and `radius` is given
    Centre      ///< `merkez`: the chain holds the centre, the cursor is on the rim. Previewed by
                ///< `RubberShape::Circle`, which the arc shares, so no guide names this one.
};

/// What a circle guide needs beyond the points it is handed.
struct CircleGuide
{
    /// Which of the constructions the points are to be read as.
    CircleBuild build{CircleBuild::Diameter};

    /// The radius, for `Tangent` only, where it is typed rather than pointed at.
    Mm radius{0};

    friend constexpr bool operator==(const CircleGuide&, const CircleGuide&) = default;
};

/// Fixed 9-byte layout: build (uint8), radius (int64). Fixed rather than
/// versioned because a guide lives for the length of one prompt and is never
/// written to a file.
std::vector<std::uint8_t> encode_circle_guide(const CircleGuide& guide);
std::optional<CircleGuide> decode_circle_guide(std::span<const std::uint8_t> bytes);

/// The circle a `CircleBuild` makes from the points fixed so far (`chain`) and
/// the one the cursor is at. False when those points do not determine a circle:
/// a zero diameter, three points in a line, parallel tangents, a chain that is
/// too short. The canvas then draws nothing rather than drawing rubbish.
bool circle_from_guide(const CircleGuide& guide, std::span<const Point2> chain, Point2 cursor,
                       Point2& centre, Mm& radius) noexcept;

} // namespace kentos::core
