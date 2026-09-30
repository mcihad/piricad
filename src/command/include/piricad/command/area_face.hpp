// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: an area as a face for cutting and joining, and back.
//
// AN AREA WITH AN ARC EDGE KEEPS ITS ARC (TODOS O-3). Cutting and joining
// areas used to read their rings and hand them to Clipper2, and an arc
// polyline's ring is its CORNERS — the arc between two of them was cut and
// joined as the chord between them, and an area rounded at a corner came out
// with its corner cut off straight. An area that bends goes through the
// geometry kernel instead (core/kernel.hpp, CLAUDE.md 2.11), which takes the arc
// and hands it back with the centre and radius it had. An area that does not
// bend — nearly every parcel on a sheet — keeps the Clipper2 road it has always
// had: fast, and exactly what the golden fixtures hold.
//
// ONE ANSWER FOR EVERY COMMAND THAT CUTS OR JOINS AREAS: BİRLEŞTİR reads it
// here, and İFRAZ, ALANİFRAZ and TEVHİT through the cadastre module's
// `parcel_face.hpp`, so a generic merge and a tevhit cannot disagree about what
// an arc edge is.
#pragma once

#include "piricad/command/context.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/core/offset.hpp"
#include "piricad/core/result.hpp"

#include <optional>
#include <span>
#include <vector>

namespace piricad::command {

/// An area read for cutting and joining: its boundary and holes, arcs as arcs,
/// and whether any edge bends — which decides the road it is cut by.
struct AreaFace
{
    core::KernelFace face; ///< the boundary and its holes
    bool curved{false};    ///< an edge bends: the kernel's road
};

/// The area in `slot` of `doc`, or nothing for an object that is not a closed
/// area.
std::optional<AreaFace> area_face(const core::Document& doc, core::EntityId slot);

/// The face as the polygon Clipper2 takes: for a face that does not bend.
core::Polygon face_polygon(const core::KernelFace& face);

/// A face whose boundary and holes are the polygon's rings.
core::KernelFace polygon_face(const core::Polygon& polygon);

/// The face's area with its arcs, positive, holes taken out.
core::Mm2 face_area(const core::KernelFace& face);

/// The area inside the face's boundary, positive, holes not taken out — what
/// the commands have always reported for a piece. A straight boundary is
/// measured over its ring exactly as it always was (`core::ring_area`), so a
/// report of a straight-edged ifraz reads to the square millimetre what it
/// read before.
core::Mm2 outer_area(const core::KernelFace& face);

/// `op` over two sets of areas by the road they need: the kernel when any
/// bends, Clipper2 when none does.
core::Result<std::vector<core::KernelFace>> area_boolean(std::span<const core::KernelFace> a,
                                                         std::span<const core::KernelFace> b,
                                                         core::BooleanOp op, bool curved);

/// Writes `face` as a new area on `layer`: a polyline when no edge bends, an
/// arc polyline when one does. Refused, with the sentence a user reads, for a
/// face with an arc edge that also has a hole — an arc polyline has one ring.
core::Result<core::EntityId> add_face(Context& ctx, core::LayerId layer,
                                      const core::KernelFace& face);

} // namespace piricad::command
