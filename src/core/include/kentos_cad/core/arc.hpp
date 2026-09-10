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

} // namespace kentos::core
