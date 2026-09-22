// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: what an arc is, and how one is drawn.
//
// Like a circle, AN ARC IS ITS DEFINITION, NOT ITS PICTURE. `RingGeometry` holds
// one Open ring of exactly four vertices:
//
//     vertex 0 = the centre
//     vertex 1 = {centre.x + radius, centre.y}   — the radius, exact
//     vertex 2 = where the arc starts
//     vertex 3 = where the arc ends
//
// The sweep is always COUNTER-CLOCKWISE from vertex 2 to vertex 3. A clockwise arc
// is the same arc with its two ends given the other way round, so there is no
// direction flag to get wrong and no arc that two different records could mean.
//
// Vertices 2 and 3 are real coordinates — the points a surveyor measured — which
// is why they are stored rather than a pair of angles. The radius is stored
// separately and exactly, so the drawn arc passes through the radius the record
// declares even when the measured ends round to a millimetre either side of it.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// The radius of the arc in `slot`, in millimetres. Never negative.
Mm arc_radius_of(const RingGeometry& geom, std::uint32_t slot);

Point2 arc_centre_of(const RingGeometry& geom, std::uint32_t slot);

/// Where the sweep begins and ends. These are the stored points, which lie on the
/// arc's radius only to within the rounding of one millimetre.
Point2 arc_start_of(const RingGeometry& geom, std::uint32_t slot);
Point2 arc_end_of(const RingGeometry& geom, std::uint32_t slot);

/// Appends the DRAWN form of an arc: a run of vertices from the start to the end,
/// counter-clockwise, WITHOUT a closing segment.
///
/// Deterministic on every platform, and that is a requirement rather than a
/// nicety (§7.3). The points come from repeated bisection of the chord between
/// two unit vectors — nothing but +, * and `sqrt`, all correctly rounded by
/// IEEE-754 — never from `std::cos`/`std::sin`, which are not required to agree
/// between platforms. The sweep is first cut into pieces of at most a quarter
/// turn using exact perpendicular rotations, because the chord midpoint of two
/// directions more than half a turn apart bisects the arc the OTHER way round.
void arc_outline(Point2 centre, Mm radius, Point2 start, Point2 end, std::vector<Mm>& xs,
                 std::vector<Mm>& ys);

/// The point halfway ALONG the arc — not the midpoint of the chord between its
/// ends, which is inside the curve and on nothing. Ends that coincide are a full
/// turn (as `arc_perimeter` reads them), whose midpoint is the far side.
///
/// Deterministic for the same reason `arc_outline` is: the direction is the
/// normalised sum of the two end directions (or its negation past a half turn),
/// so nothing but +, *, / and `sqrt` touches it — never `std::cos`/`std::sin`.
Point2 arc_midpoint(Point2 centre, Mm radius, Point2 start, Point2 end);

/// Whether `p`'s direction from `centre` falls within the counter-clockwise
/// sweep from `start` to `end` — on the arc that is DRAWN, not merely on the
/// circle it was cut from. Decided from cross and dot products, never from an
/// angle.
bool on_arc(Point2 centre, Point2 start, Point2 end, Point2 p);

/// The counter-clockwise sweep from `from` to `to` about `centre`, in whole
/// micro-degrees, from `atan2_udeg` — never `std::atan2` (§7.3). Coincident
/// ends are a full turn, as `arc_perimeter` reads them.
std::int64_t arc_sweep_udeg(Point2 centre, Point2 from, Point2 to) noexcept;

/// The arc a DXF BULGE describes between `a` and `b`. A bulge is tan(sweep/4),
/// positive for a counter-clockwise sweep from a to b; 1 is a half circle. Only
/// +, *, / and one square root touch it, so the centre and radius round the same
/// on every platform. False when the bulge bends nothing a millimetre can show,
/// or the two ends coincide.
bool arc_from_bulge(Point2 a, Point2 b, double bulge, Point2& centre, Mm& radius,
                    bool& ccw) noexcept;

/// The bulge of the arc from `a` to `b` about `centre` with `radius`, sweeping
/// counter-clockwise when `ccw`: what a DXF writer puts on the vertex. The
/// inverse of `arc_from_bulge` to within the rounding of the stored centre.
double bulge_from_arc(Point2 a, Point2 b, Point2 centre, Mm radius, bool ccw) noexcept;

// ---------------------------------------------------------------------------
// The constructions an arc arrives by, and the guide that previews them.
//
// SAME RULE AS THE CIRCLE AND THE POLYGON: one answer, computed once here, so
// the guide the canvas draws and the arc the command commits cannot drift apart.
// Three of `YAY`'s five methods had no guide worth the name — the third point of
// `3n` and the end of a tangent continuation were previewed as straight LINES,
// which say nothing about the curve — and `bby` never asked which side the curve
// goes at all.
// ---------------------------------------------------------------------------

/// How the points in hand are to be read.
enum class ArcBuild : std::uint8_t {
    ThreePoint, ///< `3n`: the chain holds the start and a point it passes through; the cursor is
                ///< the end
    Tangent,    ///< `devam`: the chain holds the start and a point ALONG the tangent there; the
                ///< cursor is the end
    Radius      ///< `bby`: the chain holds the two ends and `radius` is given; the cursor says
                ///< which of the two arcs was meant
};

/// What an arc guide needs beyond the points it is handed.
struct ArcGuide
{
    /// Which of the constructions the points are to be read as.
    ArcBuild build{ArcBuild::ThreePoint};

    /// The radius, for `Radius` only, where it is typed rather than pointed at.
    Mm radius{0};

    friend constexpr bool operator==(const ArcGuide&, const ArcGuide&) = default;
};

/// Fixed 9-byte layout: build (uint8), radius (int64). Fixed rather than
/// versioned because a guide lives for the length of one prompt and is never
/// written to a file.
std::vector<std::uint8_t> encode_arc_guide(const ArcGuide& guide);
std::optional<ArcGuide> decode_arc_guide(std::span<const std::uint8_t> bytes);

/// Which of the two arcs of a given radius joining `a` and `b` the point
/// `toward` asks for: true for the one `yon=sag` names.
///
/// DECIDED BY WHICH SIDE OF THE CHORD `toward` LIES ON, and that is the whole
/// reason this is a function of its own. The obvious test — whichever of the two
/// centres is nearer — fails in exactly the case a fillet is most often drawn
/// in: when the radius is half the span the two centres COINCIDE, and the two
/// arcs are still different arcs (they differ by which end is the start, which
/// is what the model stores). The chord has two sides whatever the radius is.
bool arc_radius_side(Point2 a, Point2 b, Point2 toward) noexcept;

/// The arc of `radius` joining `a` and `b` on the named side, in stored form.
/// False when the radius is shorter than half the span, which joins nothing.
bool arc_by_radius(Point2 a, Point2 b, Mm radius, bool to_right, Point2& centre, Mm& out_radius,
                   Point2& start, Point2& end) noexcept;

/// The arc a construction makes, in the form the document stores: a centre, a
/// radius and the two ends COUNTER-CLOCKWISE from `start` to `end` (the winding
/// this file's own note explains).
///
/// False when the points do not determine an arc: three points in a line, an end
/// that lies along the tangent (that is a straight line, not an arc), a radius
/// shorter than half the span, a chain that is too short. The canvas then draws
/// nothing rather than drawing rubbish.
bool arc_from_guide(const ArcGuide& guide, std::span<const Point2> chain, Point2 cursor,
                    Point2& centre, Mm& radius, Point2& start, Point2& end) noexcept;

} // namespace kentos::core
