// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: parallel geometry (offset).
//
// The operation a surveyor calls "ofset" and a planner calls "çekme mesafesi":
// a curve parallel to another at a fixed distance. It is the single most-used
// construction in cadastral drafting after the line itself — a road right-of-way
// from a centre line, a building setback from a parcel boundary, a buffer around
// a watercourse.
//
// NOT HAND-ROLLED, and that is CLAUDE.md 2.7 and 5.16 rather than a preference.
// The naive "move every edge sideways and re-intersect" is wrong on reflex
// corners, produces self-intersecting loops on concave input, and spikes without
// bound on a sharp mitre. Clipper2 solves all three and has been solving them in
// production for twenty years; the facade here is what keeps its headers out of
// every translation unit that needs a parallel (Article 9).
//
// INTEGER IN, INTEGER OUT. Clipper2's `Path64` is int64 and `Mm` is int64
// fixed-point millimetres, so nothing is converted on the way in or out and the
// answer is the same on every platform (Article 2.4, §7.3).
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <vector>

namespace kentos::core {

/// How a parallel turns an outside corner.
enum class JoinStyle : std::uint8_t {
    Miter, ///< carried to the meeting point — the CAD default, and what a plan sheet shows
    Round, ///< an arc of the offset radius
    Bevel, ///< the corner cut straight across
};

/// What happens at the two ends of an OPEN run.
enum class EndStyle : std::uint8_t {
    Butt,   ///< stops square at the end, adding nothing
    Round,  ///< a half-disc cap
    Square, ///< a square cap projecting by the offset distance
    Joined, ///< both sides joined into one closed loop around the run
};

/// One offset result: a run of vertices and whether it closes.
struct OffsetRing
{
    std::vector<Point2> points; ///< the parallel's vertices, in order
    bool closed{false};         ///< true when the run joins back to its first vertex
};

/// Offsets `points` by `distance` millimetres.
///
/// A positive distance grows a closed ring outward and offsets an open run to its
/// LEFT looking along it; a negative one does the opposite. Zero is refused
/// rather than returning the input, because a caller asking for a zero offset has
/// almost certainly failed to read a value.
///
/// A closed input may return SEVERAL rings — shrinking a dumbbell-shaped parcel
/// past its waist splits it in two — and may return NONE, when the shape
/// disappears entirely. Both are correct answers and the caller must handle them;
/// that is exactly the case a hand-rolled offset gets wrong.
Result<std::vector<OffsetRing>> offset_ring(const std::vector<Point2>& points, bool closed,
                                            Mm distance, JoinStyle join = JoinStyle::Miter,
                                            EndStyle end = EndStyle::Butt);

/// One polygon: an exterior ring and the holes inside it.
///
/// The shape every boolean below takes and returns. A ring's winding is not the
/// caller's problem — Clipper2's even-odd rule reads a hole as a hole whichever
/// way round it was drawn, which is what lets a parcel read from a DXF work
/// without being rewound first.
struct Polygon
{
    std::vector<Point2> exterior;           ///< the outer boundary
    std::vector<std::vector<Point2>> holes; ///< the voids inside it, if any
};

/// Which boolean to run.
enum class BooleanOp : std::uint8_t {
    Union,        ///< everything either side covers — TEVHİT
    Difference,   ///< what the first covers and the second does not — İFRAZ's remainder
    Intersection, ///< what both cover — the overlap a topology check looks for
};

/// Runs `op` over two sets of polygons.
///
/// NOT HAND-ROLLED, for the reason `offset_ring` is not: polygon boolean on real
/// cadastral input — rings that touch at a point, holes that share an edge with
/// their exterior, coordinates a metre apart over a hundred kilometres — is a
/// problem with twenty years of corrections in it. `.claude/domain.md` and
/// CLAUDE.md 5.16 both point at the library rather than at a rewrite.
///
/// May return SEVERAL polygons, or NONE. A difference can cut one parcel into
/// two, and a union of two parcels that do not touch is still two parcels; both
/// are correct answers a cadastral caller has to handle.
Result<std::vector<Polygon>> polygon_boolean(const std::vector<Polygon>& subject,
                                             const std::vector<Polygon>& clip, BooleanOp op);

/// The signed area a ring encloses, in square millimetres.
///
/// Positive counter-clockwise. Exposed because a topology check compares areas —
/// "does the union cover less than the sum" is how an overlap is found — and
/// doing that through the entity table would mean building an entity first.
Mm2 ring_area(const std::vector<Point2>& ring) noexcept;

} // namespace kentos::core
