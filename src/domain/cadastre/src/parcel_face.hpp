// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — cadastre: a parcel as İFRAZ, ALANİFRAZ and TEVHİT cut and join it.
//
// A PARCEL WITH AN ARC EDGE KEEPS ITS ARC (TODOS O-3). The three commands used
// to read a parcel's ring and hand it to Clipper2, and an arc polyline's ring is
// its CORNERS — the arc between two of them was cut and joined as the chord
// between them, and a parcel rounded at a corner came out of an ifraz with its
// corner cut off straight. A parcel that bends goes through the geometry kernel
// instead (core/kernel.hpp, CLAUDE.md 2.11), which takes the arc and hands it
// back with the centre and radius it had. A parcel that does not bend — nearly
// every parcel on a sheet — keeps the Clipper2 road it has always had: fast,
// and exactly what the golden fixtures hold.
//
// INTERNAL TO THE CADASTRE MODULE: the three commands share it, nothing else
// reads it.
#pragma once

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/kernel.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/result.hpp"

#include <optional>
#include <span>

namespace kentos::command::cadastre {

/// A parcel read for cutting: its boundary and holes, arcs as arcs, and whether
/// any edge bends — which decides the road it is cut by.
struct ParcelFace
{
    core::KernelFace face;
    bool curved{false};
};

/// The parcel in `slot` of `doc`, or nothing for an object that is not a
/// closed area.
std::optional<ParcelFace> parcel_face(const core::Document& doc, core::EntityId slot);

/// The face as the polygon Clipper2 takes: for a face that does not bend.
core::Polygon polygon_of(const core::KernelFace& face);

/// A face whose boundary and holes are the polygon's rings.
core::KernelFace face_of(const core::Polygon& polygon);

/// The face's area with its arcs, positive, holes taken out.
core::Mm2 face_area(const core::KernelFace& face);

/// The area inside the face's boundary, positive, holes not taken out — what
/// the commands have always reported for a piece. A straight boundary is
/// measured over its ring exactly as it always was (`core::ring_area`), so a
/// report of a straight-edged ifraz reads to the square millimetre what it
/// read before.
core::Mm2 outer_area(const core::KernelFace& face);

/// `op` over two sets of parcels by the road they need: the kernel when any
/// bends, Clipper2 when none does.
core::Result<std::vector<core::KernelFace>> parcel_boolean(std::span<const core::KernelFace> a,
                                                           std::span<const core::KernelFace> b,
                                                           core::BooleanOp op, bool curved);

/// Writes `face` as a new area on `layer`: a polyline when no edge bends, an
/// arc polyline when one does. Refused, with the sentence a user reads, for a
/// face with an arc edge that also has a hole — an arc polyline has one ring.
core::Result<core::EntityId> add_face(Context& ctx, core::LayerId layer,
                                      const core::KernelFace& face);

} // namespace kentos::command::cadastre
