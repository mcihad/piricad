// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a curve walked piece by piece, and where two curves meet.
//
// ONE WAY TO WALK A LINE, AN ARC, A CIRCLE AND A POLYLINE WHOSE EDGES BEND
// (TODOS C-01, C-05). Trimming, extending
// and breaking all ask the same three questions — where does this curve cross
// that one, how far along it is a point, what lies between two places on it —
// and the answer must not depend on the kind asking: an arc trimmed to a line is
// still an arc, and a line trimmed to an arc stops ON the arc, not on the chord
// the arc happens to be drawn with.
//
// CLOSED FORMS, EACH WITH ONE SQUARE ROOT. Segment–segment, segment–circle and
// circle–circle meets are solved analytically, in metres about a local origin so
// the squares stay inside the mantissa (core.md R3), and every point is rounded
// to the millimetre once (§7.3). Angles are whole micro-degrees from
// `atan2_udeg`, and a point is put back on its circle by `sin_cos_udeg` — never
// libm's `atan2`, `sin` or `cos`, which platforms do not agree on.
//
// A LIBRARY WAS WEIGHED, as Article 2.7 requires. CGAL's circular kernel answers
// these three questions exactly, and linking it for three closed forms — one of
// which, the circle–circle meet, this core already has (`circle_intersection`) —
// would bring the kernel and GMP into every build for none of CGAL's real
// strengths.
//
// ELLIPSES AND SPLINES ARE PATHS TOO, for the tools that can take them
// (`PathScope::Curves`). Their crossings have no closed form a solver can
// trust, so they are found the way every CAD kernel finds them: candidates
// where the drawn chords of the two curves cross or pass within the chords'
// own deviation, each refined by Newton's method on the exact curves
// (`core/src/curve_eval.hpp`). A library was weighed here too and the
// hand-rolled solve is the stated exception (CLAUDE.md Article 9): OpenCASCADE
// answers it and brings a CAD kernel of its own into every build; SISL is
// AGPL; CGAL's Bézier arrangement is exact but takes no rational curve and
// needs CORE; and none of them pins its operation order, which is what makes
// an answer the same on three platforms (§7.3) — the reason `spline.hpp` draws
// with its own de Boor. The solve SAYS when it could not decide: a candidate
// the refinement cannot settle is reported, never silently dropped
// (`PathMeets::unresolved`).
//
// TANGENCY AND OVERLAP ARE SAID, NOT GUESSED. A line that touches a circle meets
// it at one point marked `touching`; two collinear segments share a stretch
// rather than a point, and that stretch is not reported as an arbitrary point of
// itself.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/spline.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// One piece of a path: a straight segment, or an arc of a circle swept from
/// `from` to `to` — counter-clockwise when `sweep_udeg` is positive, clockwise
/// when it is negative. A path is WALKED, and an arc-polyline edge that bends
/// the other way, or a chain turned round to join another, walks its arcs
/// clockwise; a sign keeps the direction and every arc still its definition.
///
/// Under `PathScope::Curves` a piece may also be an ELLIPSE — an arc of one,
/// swept in the ellipse's own parameter (ellipse.hpp: the point at t is
/// `centre + cos t · (major − centre) + sin t · (minor − centre)`) from
/// `start_udeg`, the sign of `sweep_udeg` again the way it is walked — or a
/// SPLINE: a whole NURBS curve (`controls`, `spline` with its knots written
/// out), walked from the first point of its domain to the last. A spline piece
/// cut short is re-made as the shorter curve it is (knot insertion), so every
/// spline piece is walked over its whole domain.
struct PathPiece
{
    /// Which of the four a piece is.
    enum class Kind : std::uint8_t { Segment, Arc, Ellipse, Spline };

    Kind kind{Kind::Segment};     ///< which it is
    Point2 from{};                ///< where the piece starts along the path
    Point2 to{};                  ///< where it ends
    Point2 centre{};              ///< Arc and Ellipse: the centre
    Mm radius{0};                 ///< Arc: the radius
    std::int64_t sweep_udeg{0};   ///< Arc: the signed sweep from `from`, whole micro-degrees;
                                  ///< ±360° is a circle. Ellipse: the signed sweep of its
                                  ///< parameter; ±360° is the whole ellipse
    Point2 major{};               ///< Ellipse: the end of its first axis
    Point2 minor{};               ///< Ellipse: the end of its second axis
    std::int64_t start_udeg{0};   ///< Ellipse: its parameter at `from`, in [0, 360°)
    std::vector<Point2> controls; ///< Spline: its control points, first to last
    SplineDef spline{};           ///< Spline: degree, knots (always written out), weights

    friend bool operator==(const PathPiece&, const PathPiece&) = default;
};

/// A curve as the pieces it is drawn along, in order.
struct CurvePath
{
    std::vector<PathPiece> pieces; ///< in order along the curve
    bool closed{false};            ///< the last piece ends where the first begins

    friend bool operator==(const CurvePath&, const CurvePath&) = default;
};

/// A place on a path: which piece, and how far along it — 0 at the piece's start
/// and 1 at its end (for an arc, the fraction of its sweep).
struct PathPlace
{
    std::size_t piece{0}; ///< the piece
    double t{0.0};        ///< how far along it
};

/// Whether `a` comes before `b` along the path.
bool comes_before(PathPlace a, PathPlace b) noexcept;

/// The arc piece from `from` to `to` round `centre`, walked counter-clockwise
/// or clockwise — the one place a direction becomes a signed sweep.
PathPiece arc_piece(Point2 centre, Mm radius, Point2 from, Point2 to, bool ccw) noexcept;

/// Which kinds `path_of` walks.
enum class PathScope : std::uint8_t {
    /// Lines, arcs, circles and arc-polylines — every piece a segment or an
    /// arc: what a tool that bends, joins or edits vertices can take.
    Circular,
    /// And ellipses and splines, their pieces `Kind::Ellipse` and
    /// `Kind::Spline`: what BUDA, UZAT, BÖL and KIR take (TODOS C-01).
    Curves,
};

/// The path `e` is drawn along: a polyline's single ring, open or closed; an
/// arc; a circle; an arc-polyline, its bent edges as arcs; under
/// `PathScope::Curves` an ellipse, whole or partial, and a spline — a closed
/// one only when its curve returns to its start. Nothing for every other kind,
/// for a polyline of several rings (a face with holes), and for a caption's
/// baseline.
std::optional<CurvePath> path_of(const Document& doc, EntityId e,
                                 PathScope scope = PathScope::Circular);

/// The same path walked the other way: the pieces in reverse order, each from
/// its end to its start, every arc's sweep negated.
CurvePath reversed(const CurvePath& path);

/// The point at `at`, rounded to the millimetre; a piece's own ends exactly.
Point2 point_at(const CurvePath& path, PathPlace at);

/// The direction the path runs at `at`, whole micro-degrees counter-clockwise
/// from east: a segment's own direction, an arc's tangent the way it is walked
/// (TODOS C-08, the copies of a path array turned to follow it).
std::int64_t direction_at(const CurvePath& path, PathPlace at);

/// The place on `path` nearest `probe`.
PathPlace place_of(const CurvePath& path, Point2 probe);

/// Where `path` starts and where it ends.
PathPlace path_start(const CurvePath& path) noexcept;
PathPlace path_end(const CurvePath& path) noexcept;

/// The length of `path`, in millimetres — an ellipse's and a spline's along the
/// curve itself (Gauss–Legendre quadrature over the exact derivative), not
/// along the chords it is drawn with.
Mm path_length(const CurvePath& path);

/// The box `path` occupies — the curve's own, not its control points'.
Box2 path_bounds(const CurvePath& path);

/// The place `length` millimetres along `path` from its start, clamped to the
/// path — what a split at a distance and a split into equal parts walk to.
PathPlace place_at_length(const CurvePath& path, Mm length);

/// `path` cut at `cuts`, every piece kept, in order: an open path from its
/// start to the first cut and on to its end; a closed one from each cut to the
/// next, round the seam. Cuts are sorted and a repeated or an end cut is
/// dropped; a piece that shrinks to nothing is not a piece. A closed path
/// needs two cuts to come apart — with one it comes back whole, opened there.
std::vector<CurvePath> split_path(const CurvePath& path, std::vector<PathPlace> cuts);

/// What joining paths end to end makes (`join_paths`).
struct PathJoin
{
    CurvePath chain;                 ///< the joined run, in the first path's direction
    std::vector<std::size_t> joined; ///< which of the paths it holds, the first first
    std::size_t bridged{0};          ///< gaps closed with a straight piece
    Mm widest{0};                    ///< the widest of them, millimetres
    bool ends_meet{false};           ///< the chain's two ends are within the tolerance
};

/// Joins open `paths` end to end, starting from the first IN ITS OWN DIRECTION
/// and taking, again and again, a path one of whose ends lies within
/// `tolerance` of either end of the chain — turned round when it has to be.
///
/// NOTHING IS MOVED TO MAKE A JOIN: two ends that touch exactly share their
/// point, and a gap inside the tolerance is closed with a straight piece and
/// counted, so the geometry the user drew is kept and what was added is said.
/// Two arcs of one circle meeting end to start become one arc. A closed path
/// has no ends and is never taken.
PathJoin join_paths(std::span<const CurvePath> paths, Mm tolerance);

/// `path` with vertex `index` taken out — numbered along the path from its
/// start, a closed path's vertices one per piece. The two pieces that met there
/// become one: an arc when both were arcs of one circle turning the same way,
/// else the straight edge between their far ends. An open path's end takes its
/// end piece with it. Refused when too few would remain: two vertices for an
/// open path, three for a closed one (TODOS C-07).
Result<CurvePath> path_without_vertex(const CurvePath& path, std::size_t index);

/// `path` with piece `edge` bent into the arc that runs from its start through
/// `through` to its end — the straight edge made a curve (TODOS C-07).
/// Refused when the three points lie in a line.
Result<CurvePath> path_with_arc_edge(const CurvePath& path, std::size_t edge, Point2 through);

/// `path` with piece `edge` made straight: its chord. Refused when it already is.
Result<CurvePath> path_with_straight_edge(const CurvePath& path, std::size_t edge);

/// The payload the edge-bend preview carries (`command::RubberShape::EdgeArc`):
/// the object by persistent key and the edge being bent, so the canvas can call
/// `path_with_arc_edge` with the cursor — the call the click makes.
struct EdgeGuide
{
    std::int64_t key{0};   ///< the object, by persistent key
    std::uint32_t edge{0}; ///< the edge, its piece index along the path
};

/// The guide as bytes.
std::vector<std::uint8_t> encode_edge_guide(const EdgeGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<EdgeGuide> decode_edge_guide(std::span<const std::uint8_t> bytes);

/// How a path is stored: the kind that holds it, its one ring and the kind's
/// payload. Straight pieces only are a polyline; one arc is an arc and one
/// whole turn a circle; anything else an arc-polyline, whose arcs stay arcs.
struct PathRecord
{
    KindId kind{kPolylineKind};        ///< the kind that holds the path
    std::vector<Point2> ring;          ///< that kind's one ring
    RingRole role{RingRole::Open};     ///< open, or exterior for a closed polyline
    std::vector<std::uint8_t> payload; ///< the kind's payload; empty for a polyline
};

/// The record `path` is written as. False-free: every path has one.
PathRecord path_record(const CurvePath& path);

/// One place where a path meets another.
struct PathCrossing
{
    PathPlace at{};       ///< where on the path walked
    Point2 point{};       ///< the point itself
    bool touching{false}; ///< a tangent: the two meet without crossing
};

/// Every point where `path` meets `other`, ordered along `path`; a meet at a
/// vertex two pieces share is counted once. A collinear overlap is not a point
/// and is not reported.
std::vector<PathCrossing> path_crossings(const CurvePath& path, const CurvePath& other);

/// A stretch two paths share: the same curve for a while, not a point.
struct PathOverlap
{
    PathPlace from{}; ///< where it begins on the path walked
    PathPlace to{};   ///< where it ends
};

/// Everything two paths have in common, told apart (TODOS C-01): the points
/// where they cross, each `touching` when they meet without crossing; the
/// stretches they share; and whether the solve could not decide somewhere —
/// a candidate the refinement did not settle, which is NOT the same answer as
/// "they do not meet", and a tool must not treat it as one.
struct PathMeets
{
    std::vector<PathCrossing> crossings; ///< ordered along the path walked
    std::vector<PathOverlap> overlaps;   ///< shared stretches, ordered likewise
    bool unresolved{false};              ///< a candidate the solve could not settle
};

/// `path_crossings` with the overlaps and the solver's own verdict.
PathMeets path_meets(const CurvePath& path, const CurvePath& other);

/// One place an infinite line meets a piece.
struct LineMeet
{
    double t{0.0};        ///< along the line: 0 at its first point, 1 at its second
    Point2 point{};       ///< the point itself
    bool touching{false}; ///< a tangent
};

/// Where the INFINITE line through `a` and `b` meets `piece` — on the piece,
/// anywhere along the line — sorted along the line. What an extension walks.
std::vector<LineMeet> line_meets(Point2 a, Point2 b, const PathPiece& piece);

/// Where the WHOLE circle of the arc piece `arc` meets `piece`, on `piece`: what
/// an arc's end is carried round to.
std::vector<Point2> circle_meets(const PathPiece& arc, const PathPiece& piece);

/// Where the WHOLE ellipse of the ellipse piece `arc` meets `piece`, on
/// `piece`: what an elliptic arc's end is carried round to.
std::vector<Point2> ellipse_meets(const PathPiece& arc, const PathPiece& piece);

/// The part of `path` from `a` to `b`, along it. On a closed path, `b` before `a`
/// wraps past the seam; on an open one it is empty. Pieces that shrink to nothing
/// are dropped, and two arcs of one circle meeting at the seam become one.
CurvePath sub_path(const CurvePath& path, PathPlace a, PathPlace b);

/// The vertices of a path made only of segments — what a polyline stores.
std::vector<Point2> path_vertices(const CurvePath& path);

/// Draws `path` into `xs`/`ys`: segments as they are, arcs by `arc_outline`, the
/// routine a YAY is drawn with.
void path_outline(const CurvePath& path, std::vector<Mm>& xs, std::vector<Mm>& ys);

} // namespace kentos::core
