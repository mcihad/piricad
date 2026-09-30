// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — cadastre: a parcel as İFRAZ, ALANİFRAZ and TEVHİT cut and join it.
//
// A PARCEL WITH AN ARC EDGE KEEPS ITS ARC (TODOS O-3), by the one answer every
// command that cuts or joins areas shares: `command/area_face.hpp`, where
// BİRLEŞTİR reads it too. The names below are the ones the three commands have
// always called; they are that header's functions and nothing more, so a
// generic merge and a tevhit cannot disagree about what an arc edge is.
//
// INTERNAL TO THE CADASTRE MODULE: the three commands share it, nothing else
// reads it.
#pragma once

#include "piricad/command/area_face.hpp"

#include <optional>
#include <span>
#include <vector>

namespace piricad::command::cadastre {

/// A parcel read for cutting: its boundary and holes, arcs as arcs, and whether
/// any edge bends — `command::AreaFace`.
using ParcelFace = command::AreaFace;

/// The parcel in `slot` of `doc`, or nothing for an object that is not a
/// closed area.
inline std::optional<ParcelFace> parcel_face(const core::Document& doc, core::EntityId slot)
{
    return command::area_face(doc, slot);
}

/// The face as the polygon Clipper2 takes: for a face that does not bend.
inline core::Polygon polygon_of(const core::KernelFace& face)
{
    return command::face_polygon(face);
}

/// A face whose boundary and holes are the polygon's rings.
inline core::KernelFace face_of(const core::Polygon& polygon)
{
    return command::polygon_face(polygon);
}

using command::add_face;
using command::face_area;
using command::outer_area;

/// `op` over two sets of parcels by the road they need: the kernel when any
/// bends, Clipper2 when none does.
inline core::Result<std::vector<core::KernelFace>>
parcel_boolean(std::span<const core::KernelFace> a, std::span<const core::KernelFace> b,
               core::BooleanOp op, bool curved)
{
    return command::area_boolean(a, b, op, curved);
}

} // namespace piricad::command::cadastre
