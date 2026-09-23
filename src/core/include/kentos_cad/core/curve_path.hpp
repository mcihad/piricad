// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a curve walked piece by piece, and where two curves meet.
//
// ONE WAY TO WALK A LINE, AN ARC AND A CIRCLE (TODOS C-01). Trimming, extending
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
// strengths. Ellipses and splines are NOT paths yet: their crossings need an
// iterative solve, which is where a library earns its place, and they arrive
// with it.
//
// TANGENCY AND OVERLAP ARE SAID, NOT GUESSED. A line that touches a circle meets
// it at one point marked `touching`; two collinear segments share a stretch
// rather than a point, and that stretch is not reported as an arbitrary point of
// itself.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kentos::core {

/// One piece of a path: a straight segment, or an arc of a circle swept
/// counter-clockwise from `from` to `to`.
struct PathPiece
{
    /// Which of the two a piece is.
    enum class Kind : std::uint8_t { Segment, Arc };

    Kind kind{Kind::Segment}; ///< which it is
    Point2 from{};            ///< where the piece starts along the path
    Point2 to{};              ///< where it ends
    Point2 centre{};          ///< Arc: the centre
    Mm radius{0};             ///< Arc: the radius
    std::int64_t sweep_udeg{
        0}; ///< Arc: the sweep from `from`, whole micro-degrees; 360° is a circle

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

/// The path `e` is drawn along: a polyline's single ring, open or closed; an
/// arc; a circle. Nothing for every other kind, for a polyline of several rings
/// (a face with holes), and for a caption's baseline.
std::optional<CurvePath> path_of(const Document& doc, EntityId e);

/// The point at `at`, rounded to the millimetre; a piece's own ends exactly.
Point2 point_at(const CurvePath& path, PathPlace at);

/// The place on `path` nearest `probe`.
PathPlace place_of(const CurvePath& path, Point2 probe);

/// Where `path` starts and where it ends.
PathPlace path_start(const CurvePath& path) noexcept;
PathPlace path_end(const CurvePath& path) noexcept;

/// The length of `path`, in millimetres.
Mm path_length(const CurvePath& path);

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
