// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the corners a regular polygon has, a rectangle built on an
// edge or by its measures, and the fourth corner three measured ones fix.
//
// WHY THIS IS IN `core` AND NOT IN THE COMMAND THAT DRAWS IT.
//
// Two callers need the same corners: the command, which writes them into the
// document, and the canvas, which draws the guide that promises what the next
// click will make. A guide computed a second way is a guide that is eventually
// wrong — it agrees with the command for the easy cases and diverges exactly
// where the arithmetic is interesting (a circumscribed polygon's flat, a
// rectangle whose third point is off the edge's normal). The user then sees one
// shape and gets another, which is worse than having no guide at all.
//
// So the corners are computed ONCE, here, and both callers ask for them. This is
// the rule `circle.hpp` already follows for `circle_outline` (map_canvas.cpp
// draws the circle guide with the very function the document is drawn with).
//
// THE WINDING IS GEOMETRY, NOT A READING CONVENTION (model.md R11). A ring is
// stored counter-clockwise whichever angle rule the session is in, so under semt
// — where an increasing angle turns clockwise — the step walks the other way.
// `AngleRule` is therefore an argument here: the SHAPE does not depend on how a
// user likes to read angles, but the direction the parameter sweeps does.
#pragma once

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// The fewest and the most sides a regular polygon may have.
///
/// Three because two "sides" enclose nothing; 1024 because past that the ring is
/// a circle with a thousand vertices in it, and a user who wants a circle has
/// DAİRE, which stores two.
inline constexpr std::int64_t kPolygonMinSides = 3;
inline constexpr std::int64_t kPolygonMaxSides = 1024;

/// Which measurement of a regular polygon the user has in hand.
///
/// All three reduce to the circumradius — the distance from the centre to a
/// VERTEX — but a plan sheet gives whichever one was convenient to measure, and
/// asking for a radius the user does not have is asking them to do arithmetic
/// the program can do exactly.
enum class PolygonFit : std::uint8_t {
    Inscribed,     ///< `ic`: the vertices sit ON a circle of the given radius
    Circumscribed, ///< `dis`: the EDGES touch a circle of the given radius
    Side           ///< `kenar`: the length of one side
};

/// The circumradius implied by `given` under `fit`, in metres.
///
/// Inscribed is it directly; circumscribed divides by cos(pi/n), because an edge
/// midpoint is that much closer to the centre than a vertex; a side length s
/// gives R = s / (2·sin(pi/n)).
///
/// Returns 0 for a side count outside [kPolygonMinSides, kPolygonMaxSides] or a
/// non-positive measurement, so a caller that forgot to check gets a shape with
/// no size rather than a division by a cosine of something meaningless.
double polygon_circumradius(double given_metres, std::int64_t sides, PolygonFit fit) noexcept;

/// The reverse: what `fit`'s own measurement is, given the circumradius.
///
/// Needed because the interactive gesture hands over a DISTANCE — how far the
/// cursor is from the centre — and what the command records has to be the
/// measurement its own parameter is named after, or a replay of that line would
/// draw a different polygon (Article 1.4).
double polygon_measurement(double circumradius_metres, std::int64_t sides, PolygonFit fit) noexcept;

/// Half of one side's share of the turn: `0.5 / sides`.
///
/// This is the offset between "a vertex is in that direction" and "an edge
/// midpoint is in that direction", and it is what makes a circumscribed
/// polygon's flat pass under the cursor instead of a corner.
double polygon_half_step_turns(std::int64_t sides) noexcept;

/// Appends the corners of a regular polygon, counter-clockwise in the drawing.
///
/// `start_turns` is the direction of the FIRST VERTEX from the centre, in turns
/// under `rule`. Nothing is appended when the side count is out of range or the
/// circumradius is not positive.
void regular_polygon_corners(Point2 centre, std::int64_t sides, double circumradius_metres,
                             double start_turns, AngleRule rule, std::vector<Point2>& out);

/// The same, returned rather than appended, for a caller that wants the vector.
std::vector<Point2> regular_polygon_corners(Point2 centre, std::int64_t sides,
                                            double circumradius_metres, double start_turns,
                                            AngleRule rule);

/// The four corners of the rectangle that has `first`–`second` as one edge and
/// passes through `across`.
///
/// `across` gives the DEPTH, not a corner: it is projected onto the edge's own
/// normal, so a hand a few millimetres off the perpendicular still gets a
/// rectangle rather than a parallelogram. False when the edge is degenerate or
/// `across` lies on it, which are the two cases that enclose nothing.
bool edge_rectangle_corners(Point2 first, Point2 second, Point2 across,
                            std::array<Point2, 4>& out) noexcept;

/// The fourth corner of the parallelogram whose corners run `a`, `b`, `c`, and a
/// fourth: opposite `b`, so `a + c − b`. Exact integer arithmetic.
///
/// A FIELD SKETCH GIVES THREE CORNERS OF A BUILDING (`DÖRDÜNCÜKÖŞE`, the tool
/// Netcad calls `4.Köşeyi Oluştur`) — the fourth is behind a fence, under a tree
/// or was never reached — and a parallelogram is what three corners fix. `b` is
/// the corner BETWEEN the other two, the one a right angle would sit at. Three
/// points in a line give a fourth on the same line and enclose nothing; that is
/// the caller's refusal to make, because a point is a fine answer to a question
/// this function is not asked.
constexpr Point2 fourth_corner(Point2 a, Point2 b, Point2 c) noexcept
{
    return {a.x + c.x - b.x, a.y + c.y - b.y};
}

/// The four corners of the rectangle that stands on the edge `first`–`second`
/// and reaches `depth_mm` out from it: `first`, `second`, then the two the depth
/// puts across. `depth_mm` is measured to the RIGHT of first→second — right is
/// positive, left negative, the one sign every offset from a baseline in this
/// program uses (`perpendicular_offset`, `dik()`, `DİKAYAK`, and the building
/// tool Netcad calls `Bina Oluştur`). False when the edge is degenerate or the
/// depth rounds to nothing, the two cases that enclose nothing.
///
/// THE OTHER TWO CORNERS ARE THE EDGE'S ENDS MOVED BY ONE VECTOR, worked out once
/// (`perpendicular_offset` from `first` at a foot of zero) and added to both — so
/// the far side is exactly as long as the edge to the millimetre, which is what a
/// rectangle sold to a plan sheet has to be. Each end moved by a rounding of its
/// own could differ from the other by one.
bool depth_rectangle_corners(Point2 first, Point2 second, Mm depth_mm,
                             std::array<Point2, 4>& out) noexcept;

/// The four corners of a rectangle `width_mm` along its first side and
/// `length_mm` along the one after it, standing on `origin`, then turned by
/// `turns` of a full turn in the direction `rule` counts angles.
///
/// UNTURNED, the first side runs EAST and the second NORTH: the box a plan sheet
/// or a building footprint is before anyone turns it. `turns` then swings it
/// about `origin` in the direction the session's angles grow: clockwise under
/// semt, so the second side's bearing is `turns` itself, and counter-clockwise
/// under matematik. Only the sense of the rule matters here, never where its
/// zero is — a box has no direction to read, only a turn to make.
///
/// The ring runs counter-clockwise from `origin` whichever rule is in force. The
/// two sides are each rounded once from the deterministic sine and cosine
/// (`polar_offset_turns`) and the far corner is their SUM, so opposite sides are
/// equal to the millimetre. False for a side that is not positive.
bool box_corners(Point2 origin, Mm width_mm, Mm length_mm, double turns, AngleRule rule,
                 std::array<Point2, 4>& out) noexcept;

/// What a polygon guide needs beyond the points it is handed.
///
/// The canvas is given a centre and a cursor; the side count and the fit are the
/// two facts it cannot see. Under `kenar` the size is already fixed by a typed
/// length, so the cursor sets only the rotation; with `aci` given the rotation
/// is fixed, so the cursor sets only the size. Carried through
/// `Prompt::rubber_payload`, which exists for exactly this (`command/input.hpp`).
struct PolygonGuide
{
    std::int64_t sides{kPolygonMinSides};
    PolygonFit fit{PolygonFit::Inscribed};

    /// The fit's own measurement in metres — what `yaricap` or
    /// `kenar_uzunlugu` holds — when it is already settled, 0 when the cursor's
    /// distance from the centre sets it. The MEASUREMENT and not the
    /// circumradius, because the command derives its corners from what it
    /// records, and a millimetre-rounded circumradius is not what it records.
    double measured{0.0};

    /// Whether the first vertex's direction was given (`aci`) rather than
    /// pointed at, and that direction in micro-degrees under the session's
    /// convention — the form a typed `aci` takes on its way in.
    bool angle_given{false};
    std::int64_t angle_udeg{0};

    friend constexpr bool operator==(const PolygonGuide&, const PolygonGuide&) = default;
};

/// Fixed 26-byte layout: sides (int64, little end first), fit (uint8), measured
/// (the IEEE-754 bits of a double), angle_given (uint8), angle_udeg (int64).
/// Fixed rather than versioned because a guide lives for the length of one
/// prompt and is never written to a file.
std::vector<std::uint8_t> encode_polygon_guide(const PolygonGuide& guide);
std::optional<PolygonGuide> decode_polygon_guide(std::span<const std::uint8_t> bytes);

/// What pointing at a place makes of a polygon guide: the numbers ÇOKGEN
/// records for it, and the corners those numbers draw.
struct PolygonPick
{
    double measured{0.0}; ///< the fit's measurement, metres — what the size parameter records
    bool angle_pointed{false};   ///< the rotation came from the point, and `aci` records it
    double angle{0.0};           ///< that rotation, in the convention's unit
    std::vector<Point2> corners; ///< counter-clockwise in the drawing, as ÇOKGEN writes them
};

/// The polygon that pointing at `at` makes, for the command and its guide alike.
///
/// THE CORNERS COME FROM WHAT IS RECORDED. The measurement is the one the size
/// parameter records, and a pointed rotation is quantised to the micro-degree
/// the way `aci` is read back (`angle_from_turns`, `udeg_from_angle`) — so the
/// polygon the guide shows, the polygon the click writes and the polygon a
/// replay of its journal line draws are one polygon, to the millimetre. False
/// when `at` is the centre or the guide's side count is out of range.
bool polygon_from_guide(const PolygonGuide& guide, Point2 centre, Point2 at,
                        AngleConvention convention, PolygonPick& out);

} // namespace kentos::core
