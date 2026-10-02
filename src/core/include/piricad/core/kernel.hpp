// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the geometry kernel, OpenCASCADE (CLAUDE.md 2.11).
//
// WHERE OPENCASCADE DOES A THING BETTER THAN THIS PROGRAM DID, IT DOES IT HERE:
// a boolean that keeps an arc an arc, an offset whose corners are true arcs, and
// — stage by stage — fillets between curves, the crossings of ellipses and
// splines. Clipper2 answers a boolean over polygons, so a parcel with a rounded
// corner went into İFRAZ as its chords and came out without its arc; OCCT's
// boolean takes the arc's circle and hands the arc back (TODOS O-1).
//
// THE DOCUMENT DOES NOT CHANGE (Article 2.4). It keeps its millimetres; a
// shape goes into the kernel for one operation and comes back as `CurvePath`s
// — segments and arcs — rounded to the millimetre once, which is what holds
// §7.3 for work the kernel does in floating point: three platforms must agree
// on the millimetres, and the golden fixtures say so.
//
// DETERMINISTIC ON THE WAY BACK, NOT ONLY ROUNDED. The kernel names its faces
// and starts its wires wherever its own maps put them; every face here comes
// back with its boundary counter-clockwise and its holes clockwise, each
// starting at its lowest, then leftmost, vertex, and the faces sorted by that
// vertex — so the same input always makes the same pieces. An arc's sweep is
// worked out again from the rounded points (`arc_piece`), never read off the
// kernel's parameters.
//
// NO OPENCASCADE TYPE CROSSES THIS HEADER: the kernel lives in
// core/src/kernel.cpp alone (Article 9, "keep its headers inside the owning
// module's .cpp files").
#pragma once

#include "piricad/core/curve_path.hpp"
#include "piricad/core/offset.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace piricad::core {

/// Whether this build carries the kernel (`PIRICAD_WITH_OCCT`).
bool kernel_available() noexcept;

/// `OpenCASCADE 7.9.3`, or the sentence that says the build has none.
std::string kernel_version();

/// Crossings and shared stretches of two exact curve pieces, through OCCT's
/// 2D intersector. Parameters are fractions of the first piece's walk, including
/// a reversed ellipse or spline. Points return in whole millimetres, ordered
/// along that walk; a failed solve is an error, never an empty success.
Result<PathMeets> kernel_meets(const PathPiece& a, const PathPiece& b);

/// One circle of a given radius tangent to two curve pieces, as OCCT's 2D
/// fillet (`ChFi2d_FilletAPI`) finds it.
struct KernelFillet
{
    Point2 centre{}; ///< the circle's centre, whole millimetres
    Point2 on_a{};   ///< where it touches the first piece
    Point2 on_b{};   ///< where it touches the second
    double t_a{0.0}; ///< `on_a` as a fraction of the first piece's walk, in [0, 1]
    double t_b{0.0}; ///< `on_b` as a fraction of the second piece's walk

    friend bool operator==(const KernelFillet&, const KernelFillet&) = default;
};

/// EVERY circle of `radius` tangent to both pieces, on either side of each,
/// that OCCT's fillet finds — any of segment, arc, ellipse and rational
/// B-spline, with or without a common point. The tangent points lie on the
/// pieces as given (a caller that wants a line carried on passes it longer).
/// Choosing which one the user meant is the caller's: the kernel only says
/// what exists. `near` is the corner the fillet is sought at — where the two
/// cross, or between the picks. Sorted by centre, then tangent points, so the
/// same input gives the same list; empty when no such circle exists, an error
/// when the kernel fails.
Result<std::vector<KernelFillet>> kernel_fillets(const PathPiece& a, const PathPiece& b, Mm radius,
                                                 Point2 near);

/// A face as the kernel takes and gives it: a boundary and its holes, each a
/// closed path whose arcs are arcs. Handed back with the boundary
/// counter-clockwise, the holes clockwise, each starting at its lowest, then
/// leftmost, vertex.
struct KernelFace
{
    CurvePath outer;              ///< the boundary
    std::vector<CurvePath> holes; ///< the holes, none for most parcels

    friend bool operator==(const KernelFace&, const KernelFace&) = default;
};

/// OCCT's diagnosis of a closed face, including its holes.
enum class KernelFaceIssue : std::uint8_t {
    None,             ///< a usable closed face
    SelfIntersection, ///< a boundary crosses itself
    InvalidBoundary,  ///< disconnected edges, misplaced holes or invalid nesting
};

/// Checks the actual curves with OCCT's BRepCheck, without repairing them.
/// Segments, circles, ellipse arcs and rational B-splines are supported. An
/// invalid face is a diagnosis; a failed computation is an error.
Result<KernelFaceIssue> kernel_face_issue(const KernelFace& face);

/// A conservative whole-millimetre box of the actual curves, through OCCT.
/// Unlike a vertex box, this includes a circle's and an arc's bulging boundary.
Result<Box2> kernel_bounds(const CurvePath& path);

/// A read-only intersection diagnosis; outlines are for display, never storage.
struct KernelOverlap
{
    Mm2 area{0};                   ///< total common area, holes removed, rounded once
    bool exceeds_tolerance{false}; ///< a piece's effective width 2*area/perimeter exceeds tolerance
    std::vector<Point2> region;    ///< largest piece's outer outline, 1 mm display deflection
};

/// Intersects exact faces in OCCT. `tolerance` is a non-negative length in mm:
/// each common piece is significant when 2*area/perimeter exceeds it. Zero
/// reports every positive intersection. The reported area is the entire common
/// area, not an area reduced by tolerance. Kernel failures are never empty success.
/// Input faces must be valid; diagnose untrusted faces with `kernel_face_issue` first.
/// `stop` is passed to OCCT's progress indicator; cancellation returns Cancelled
/// and no partial diagnosis, including during the boolean operation itself.
Result<KernelOverlap> kernel_overlap(const KernelFace& a, const KernelFace& b, Mm tolerance,
                                     std::stop_token stop = {});

/// One enclosed, uncovered piece of a parcel coverage. Display outlines alone
/// are sampled; area and the effective width are computed on exact OCCT faces.
struct KernelCoverageGap
{
    Mm2 area{0};                ///< independent uncovered piece's net area, rounded once
    std::vector<Point2> region; ///< outer display contour at 1 mm deflection
    std::vector<std::vector<Point2>> holes; ///< covered islands, excluded from area and display
};

/// Unites valid faces in OCCT and diagnoses the bounded holes of their union.
/// Explicit input holes are intentional exclusions; they are subtracted from
/// findings, as are covered islands. The unbounded exterior is never a gap.
/// Each returned piece has positive rounded net area and effective width
/// 2*area/perimeter > `tolerance`. Zero reports every positive piece. Output is
/// sorted by outline coordinates; input order does not define finding order.
/// This is a coverage rule, not an assumption about arbitrary CAD objects.
/// Cancellation enters native booleans and returns no partial result.
Result<std::vector<KernelCoverageGap>>
kernel_coverage_gaps(std::span<const KernelFace> faces, Mm tolerance, std::stop_token stop = {});

/// `op` over two sets of faces — union, intersection, difference or symmetric
/// difference — every arc
/// kept an arc. Collinear edges and arcs of one circle that the operation cut
/// and joined again come back as one (a parcel fused back with the piece cut
/// from it is the parcel it was). Refused, with the sentence a user reads, for
/// a path that is not closed, one with an ellipse or a spline piece (the kernel
/// takes those in a later stage), and when the kernel fails.
/// Symmetric difference uses native cuts and union without rounding intermediate
/// shapes. `stop` enters each native boolean; cancellation returns no result.
Result<std::vector<KernelFace>> kernel_boolean(std::span<const KernelFace> a,
                                               std::span<const KernelFace> b, BooleanOp op,
                                               std::stop_token stop = {});

/// FACES FROM RINGS: closed paths that do not cross, each told apart by what
/// holds it — a ring inside none, or inside an even number, is a boundary; one
/// inside an odd number is a hole of the ring just round it. What an offset
/// hands back as loose rings (`kernel_offset`) — a band round a line that
/// closes on itself has a hole, and a face grown round a courtyard keeps one —
/// becomes the faces a boolean and a writer take, boundaries counter-clockwise
/// and holes clockwise. Open paths are left out.
std::vector<KernelFace> kernel_faces_of(std::vector<CurvePath> rings);

/// How an offset turns a corner it moves away from.
enum class OffsetCorner : std::uint8_t {
    Round, ///< a true arc about the corner — a buffer, a road's outer edge
    Sharp, ///< the two offset edges run on until they meet
};

/// The paths `distance` millimetres off `path`, every arc of it and every
/// rounded corner a true arc.
///
/// A CLOSED path grows outward for a positive distance and shrinks for a
/// negative one; shrunk past itself it may come apart into several, or vanish
/// (no paths). An OPEN path gives, with `both_sides`, the closed band around it
/// with round ends — a buffer — and otherwise the one path on the right of
/// travel (the left, for a negative distance).
Result<std::vector<CurvePath>> kernel_offset(const CurvePath& path, Mm distance,
                                             OffsetCorner corner, bool both_sides);

} // namespace piricad::core
