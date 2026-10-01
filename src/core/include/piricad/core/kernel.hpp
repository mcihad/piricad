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

/// `op` over two sets of faces — union, intersection, difference — every arc
/// kept an arc. Collinear edges and arcs of one circle that the operation cut
/// and joined again come back as one (a parcel fused back with the piece cut
/// from it is the parcel it was). Refused, with the sentence a user reads, for
/// a path that is not closed, one with an ellipse or a spline piece (the kernel
/// takes those in a later stage), and when the kernel fails.
Result<std::vector<KernelFace>> kernel_boolean(std::span<const KernelFace> a,
                                               std::span<const KernelFace> b, BooleanOp op);

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
