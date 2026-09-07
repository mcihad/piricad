// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: hit testing.
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
// (kentoscad.md §10.1).
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

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/units.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// Forward-declared: picking searches a document, and core headers avoid
/// including one another where a declaration will do.
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
/// of a 3-degree TM `dilim`, and eight thousand times the longest segment any
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

/// Whether `probe` lies inside the closed ring, by the even-odd rule.
///
/// EXACT INTEGER ARITHMETIC, in `core::Int128` like `ring_area`: whether a click
/// landed inside a parcel must not depend on the machine, and a crossing test in
/// `double` disagrees with itself along an edge at TUREF's seven digits.
///
/// One implementation for both readers — the pick radius and the kind's own hit
/// test — because two answers to "is the cursor in this parcel" is two chances
/// for the selection and the highlight to disagree (CLAUDE.md 5.16).
bool ring_contains(std::span<const Mm> xs, std::span<const Mm> ys, Point2 probe) noexcept;

/// Intersection of the closed segments [a,b] and [c,d]. False when they are
/// parallel, collinear or do not meet. Collinear overlap is deliberately NOT an
/// intersection: it has no single point, and a snap must produce one point.
bool segment_intersection(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out) noexcept;

/// The point of the INFINITE line through `a` and `b` nearest to `p`, with the
/// parameter that locates it: `t` is 0 at `a`, 1 at `b`, and outside [0,1] beyond
/// the ends. False for a degenerate segment, which names no line.
///
/// The unclamped twin of `closest_point_on_segment`. A boundary being re-
/// established runs PAST the last monument that survived, so the useful point is
/// the one the segment does not contain, and the caller needs `t` to tell the two
/// apart.
bool closest_point_on_line(Point2 a, Point2 b, Point2 p, Point2& out, double& t) noexcept;

/// Intersection of the INFINITE lines through [a,b] and [c,d], with the parameters
/// that locate it on each. False when the lines are parallel or either is
/// degenerate.
///
/// `t` and `u` are the segment parameters: a crossing with both inside [0,1] is a
/// real intersection, and one outside is the corner two boundaries WOULD make —
/// which is the point an ifraz needs when the corner monument is gone.
bool line_intersection(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out, double& t,
                       double& u) noexcept;

/// The four corners of the band a caption occupies, or false when `e` carries no
/// text. Corners run baseline-start, baseline-end, then back along the top.
///
/// A CAPTION IS NOT ITS BASELINE. Text is stored as a two-vertex baseline plus a
/// string (model.md R9), which is what lets the cull, the snap and the hit test
/// treat it like every other entity — but the baseline is a hairline UNDER the
/// letters and nothing is drawn on it. Picked and bounded by that line alone, an
/// imported ada number was a caption you could see, could not click, and whose
/// extent the document reported as zero tall.
///
/// The band runs from half a text height BELOW the baseline to a full height
/// above it: above covers the capitals, below covers the descenders and the
/// `MiddleCentre` anchor, and doing it along the baseline's own normal means a
/// caption laid along a road is bounded along the road rather than by the
/// upright box around it.
bool text_quad(const Document& doc, EntityId e, std::array<Point2, 4>& out);

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

/// EVERY visible entity whose geometry comes within `radius` of `cursor`, nearest
/// first. `out` is cleared first.
///
/// WHY THE WHOLE LIST AND NOT JUST THE WINNER. On a cadastral sheet a click lands
/// on a parcel, its boundary and the ada boundary over it at once, and
/// `pick_nearest` answers with one of the three and no way to say which one was
/// meant. The shell offers the list; `SEÇ mod=NOKTA sira=` names a row of it from
/// the command line and from a script, so choosing the second thing under the
/// cursor is not a mouse-only capability (CLAUDE.md 5.15).
///
/// THE ORDER IS `pick_nearest`'S OWN: sorted by distance, ties on the lower slot,
/// so `out.front()` is exactly what `pick_nearest` would have returned. The two
/// must not be able to disagree about which entity is on top — a test holds them
/// to it.
void pick_all(const Document& doc, Point2 cursor, Mm radius, std::vector<EntityId>& out);

} // namespace kentos::core
