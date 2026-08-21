// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: hit testing.
//
// What "picking" is, and what it is NOT. Picking answers "which entities did the
// user aim at?". Its answer is a SELECTION, and .claude/model.md R43 puts
// selection outside document state: nothing here mutates a Document, nothing here
// touches content_hash(), and nothing here is journalled.
//
// It reads the same two things the frame path reads — the spatial index and the
// four bbox arrays — and only then looks at ring geometry, for the handful of
// entities the index returned (model.md R6). A pick on a five-million-parcel
// layer must cost what a frame costs, because it runs on every mouse move
// (piricad.md §10.1).
//
// ARITHMETIC. Coordinates are int64 millimetres, so the classic reason to reach
// for Shewchuk's adaptive predicates — floating-point input whose sign cannot be
// trusted — does not apply: an orientation over int64 inputs is EXACT in integers
// as long as the products do not overflow. The tests below therefore translate
// every coordinate to a local origin first and work in int64, and refuse to lie
// when the local extent is large enough to overflow: past `kPickExactLimit` the
// box test degrades to the bounding-box answer, which is conservative (it can
// return an entity the user did not touch, never miss one they did).
//
// Where a RESULT is a point rather than a yes/no — the foot of a perpendicular,
// a segment intersection — the ratio is computed in `double` and rounded back
// through `mm_round` (core.md R20). IEEE-754 division and multiplication are
// correctly rounded and `-ffp-contract=off` forbids the fused product, so those
// values are bit-identical on x86 and Apple Silicon (CLAUDE.md 2.5).
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace piricad::core {

class Document;

/// How a box selection treats an entity it only partly contains. The two names
/// are the CAD convention a surveyor already has in their hands: dragging left to
/// right selects what is wholly inside, right to left selects whatever the box
/// touches.
enum class PickMode : std::uint8_t {
    Window,  ///< PENCERE — every vertex of the entity is inside the box
    Crossing ///< KESEN — the entity touches the box at all
};

/// The local extent past which the integer box test would overflow int64 and
/// degrades to the bounding-box answer. 2^29 mm is 536 km: four times the width
/// of a 3-degree TM dilim, and eight thousand times the longest segment any
/// cadastral or zoning drawing contains.
inline constexpr Mm kPickExactLimit = Mm{1} << 29;

/// Squared distance in mm², as a transient double. `Mm2` would overflow for
/// points more than ~3e9 mm apart, and a hit test must not have a range limit.
double distance_squared(Point2 a, Point2 b) noexcept;

/// The point of the closed segment [a,b] nearest to `p`. Returns `a` when the
/// segment is degenerate.
Point2 closest_point_on_segment(Point2 a, Point2 b, Point2 p) noexcept;

/// True when the closed segment [a,b] shares at least one point with `box`.
/// Exact within `kPickExactLimit`; conservative (true) beyond it.
bool segment_touches_box(Point2 a, Point2 b, const Box2& box) noexcept;

/// Intersection of the closed segments [a,b] and [c,d]. False when they are
/// parallel, collinear or do not meet. Collinear overlap is deliberately NOT an
/// intersection: it has no single point, and a snap must produce one point.
bool segment_intersection(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out) noexcept;

/// Appends every visible entity whose bounding box overlaps `box`, in ascending
/// slot order. This is the shared narrowing step: the index for what it has
/// packed, then the short unindexed tail, exactly as the frame path does. `out`
/// is cleared first, because every caller wants only this query's answer.
void pick_candidates(const Document& doc, const Box2& box, std::vector<EntityId>& out);

/// Appends every visible entity matching `box` under `mode`, in ascending slot
/// order so two runs over the same document agree (core.md P11). `out` is not
/// cleared, so a caller may reuse its capacity across picks (§10.4).
void pick_in_box(const Document& doc, const Box2& box, PickMode mode, std::vector<EntityId>& out);

/// The visible entity nearest `cursor` whose geometry comes within `radius`, or
/// `kNoEntity`. Ties break on the lower slot, which makes the answer stable.
EntityId pick_nearest(const Document& doc, Point2 cursor, Mm radius);

} // namespace piricad::core
