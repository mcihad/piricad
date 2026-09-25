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

/// The one-sided PARALLEL of an open run: the curve `distance` beside it — to
/// its LEFT looking along it when positive, to its right when negative — OPEN
/// like its source and running the same way (TODOS C-03).
///
/// This is what a road edge is to its centre line, and it is NOT what
/// `offset_ring` returns for an open run: that is the band around the run, a
/// two-sided BUFFER and a face. `(0,0)→(10,0)` at +2 m is `(0,2)→(10,2)`.
///
/// Clipper2 still does the offsetting. The band it computes already has every
/// corner joined the asked way and every loop of a tight concave turn trimmed
/// away — the part of a parallel that is hard, and what a hand-rolled "move each
/// edge and re-intersect" gets wrong. What is done here is only to take the side
/// of that band the caller asked for: every vertex of the band lies at the
/// distance from the run on one side of it, the two butt caps join the sides, so
/// the parallel is the stretch of the band's boundary on the wanted side. GEOS's
/// `OffsetCurve` is the same selection over its own buffer; linking GEOS for it
/// would bring a floating-point engine into the answer of the one module whose
/// answers must be the same bits on every platform (CLAUDE.md 2.4, 2.7, 9).
///
/// A parallel may come back in SEVERAL runs — the inward side of a U narrower
/// than twice the distance breaks in two — and in NONE, when nothing of that
/// side survives. A run marked `closed` met itself: the source doubled back
/// onto its own start. Consecutive repeated points of the source are ignored.
Result<std::vector<OffsetRing>> parallel_run(const std::vector<Point2>& run, Mm distance,
                                             JoinStyle join = JoinStyle::Miter);

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

/// The offset of FACES: every ring moved together, outward for a positive
/// distance and inward for a negative one, so that a HOLE STAYS A HOLE of the
/// face it was in — growing the face shrinks its holes, and a hole that shrinks
/// away is gone rather than left behind as a face of its own.
///
/// Offsetting each ring alone, which is what OFSET did, turned a holed parcel's
/// courtyard into a second parcel. The rings are wound here (exteriors
/// counter-clockwise, holes clockwise) because that is how Clipper2 tells one
/// from the other; the caller's winding does not matter.
///
/// May return several polygons (shrinking a dumbbell past its waist) or none
/// (shrinking past half the width). An island inside a hole comes back as a
/// polygon of its own.
Result<std::vector<Polygon>> offset_faces(const std::vector<Polygon>& faces, Mm distance,
                                          JoinStyle join = JoinStyle::Miter);

/// What a two-sided BUFFER is taken of.
struct BufferSource
{
    std::vector<std::vector<Point2>> runs; ///< open runs: the band on both sides
    std::vector<Point2> points;            ///< single points: a disc each
    std::vector<Polygon> faces;            ///< faces: grown outward, holes kept
};

/// The two-sided BUFFER of `source` at `distance`: the GIS operation, a face
/// covering everything within the distance — a protection band along a stream,
/// a disc round a well (TODOS C-03, G-10). Everything in `source` is dissolved
/// into one answer; a caller that wants one buffer per object calls once per
/// object.
///
/// `end` shapes the two ends of an open run; a point is a disc whatever it says,
/// because a square or flat end has no direction to take at a point. A negative
/// distance only means something for a face — it erodes it — and is refused for
/// a source with runs or points in it.
Result<std::vector<Polygon>> buffer(const BufferSource& source, Mm distance,
                                    JoinStyle join = JoinStyle::Round,
                                    EndStyle end   = EndStyle::Round);

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

/// A rectangle covering one side of the line `a`->`b`, big enough to contain
/// `box` whatever angle the line is at.
///
/// The shape a CUT is made with: intersect a face with this and you get the half
/// on one side of the line. It is extended and widened well past the face on
/// purpose, so every vertex of the result is either a vertex of the face or a
/// point ON the cut, and none of them came from this rectangle's own corners —
/// which is what makes the two halves meet exactly along the line the user drew.
///
/// In `core` rather than in either caller because both an ifraz and an ordinary
/// BÖL are the same cut, and two copies of a geometric trick this particular is
/// two chances to get it subtly different (CLAUDE.md 5.16).
/// Drops the vertices of `ring` that carry no shape, keeping every point within
/// `tolerance` millimetres of the simplified line.
///
/// CLIPPER2'S OWN, not a loop written here. `ÇİZGİDÜZENLE islem=sadelestir` used
/// to walk the run comparing each vertex against the line from the last KEPT one
/// to the next — a perpendicular-distance filter, which is not what simplifying
/// means: whether a vertex survives then depends on which of its neighbours
/// happened to survive before it, so the same shape thins differently depending
/// on where the walk started. Clipper2 has solved this and every degenerate case
/// around it (collinear runs, spikes, a closed ring's seam), and CLAUDE.md 5.16
/// says a solved problem is not re-solved here.
///
/// `closed` tells it the run is a ring, because a ring's first and last vertex
/// are neighbours and an open run's are ends — an end is never dropped, it is
/// where the run meets whatever it meets.
///
/// A tolerance of zero or less returns the ring unchanged: nothing carries less
/// than no information. A ring too short to thin (under three points open, four
/// closed) comes back as it went in.
std::vector<Point2> simplify_ring(const std::vector<Point2>& ring, Mm tolerance, bool closed);

/// The pieces of the run `path` that lie inside the closed ring `window`, each
/// in the order the run goes: an open path clipped by a face (Clipper2's
/// open-subject intersection, CLAUDE.md 5.16). A closed run is passed with its
/// first vertex repeated at the end, and one wholly inside comes back as one
/// piece holding every vertex it went in with. `window` may be wound either
/// way (non-zero fill); what a block clip crops a drawn line with
/// (`BlockReference::clip`).
std::vector<std::vector<Point2>> clip_path_to(const std::vector<Point2>& path,
                                              const std::vector<Point2>& window);

/// The faces of the closed ring `ring` that lie inside the closed ring
/// `window` — the intersection of two faces, non-zero filled, so either may be
/// wound either way. What a block clip leaves of a filled shape it cuts.
std::vector<std::vector<Point2>> clip_ring_to(const std::vector<Point2>& ring,
                                              const std::vector<Point2>& window);

Polygon half_plane(Point2 a, Point2 b, const Box2& box, bool left);

/// The signed area a ring encloses, in square millimetres.
///
/// Positive counter-clockwise. Exposed because a topology check compares areas —
/// "does the union cover less than the sum" is how an overlap is found — and
/// doing that through the entity table would mean building an entity first.
Mm2 ring_area(const std::vector<Point2>& ring) noexcept;

} // namespace kentos::core
