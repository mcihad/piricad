// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: what a circle is, and how one is drawn.
//
// A CIRCLE IS ITS DEFINITION, NOT ITS PICTURE. `RingGeometry` holds one Open ring
// of exactly two vertices:
//
//     vertex 0 = the centre
//     vertex 1 = {centre.x + radius, centre.y}
//
// so the radius reads back as `xs[1] - xs[0]` — an exact integer subtraction, no
// square root, no rounding. A circle stored as the polygon it looks like would
// have a circumference that is not 2·pi·r and an area that is not pi·r², and on a
// cadastral sheet those are the numbers that reach the tapu (§12).
//
// Those two vertices are geometry the arena already understands, which is what
// lets a circle be saved, loaded, culled and bounded by the same columns a line
// uses. What tells a circle from a two-point line is `entities.kind` — that column
// is exactly why it exists (model.md R22–R26).
//
// This header, rather than `entity_kind.hpp`, is where a caller asks "where is
// this circle and how big is it". That is not tidiness: `KindSpec` has a member
// called `emit`, `emit` is a Qt macro, and including that header from a Qt
// translation unit fails with `expected unqualified-id` on a line that looks
// perfectly good.
#pragma once

#include "piricad/core/geometry.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace piricad::core {

/// The radius of the circle in `slot`, in millimetres. Never negative.
Mm circle_radius_of(const RingGeometry& geom, std::uint32_t slot);

/// The centre of the circle in `slot`.
Point2 circle_centre_of(const RingGeometry& geom, std::uint32_t slot);

/// How many vertices `circle_outline` produces. Fixed, because `emit` is handed
/// no view and a kind must not need one to describe itself.
inline constexpr std::size_t kCircleSegments = 128;

/// Appends the DRAWN form of a circle — a 128-gon, without a repeated closing
/// vertex, counter-clockwise from due east.
///
/// A 128-gon's greatest departure from the true circle is r·(1 − cos(pi/128)),
/// about 0.03 % of the radius: under a tenth of a millimetre on a 300 m curve.
/// This is the picture only; the area and the radius come from the definition.
/// Precomputed LOD replaces the fixed count when `render.md` R4 lands.
void circle_outline(Point2 centre, Mm radius, std::vector<Mm>& xs, std::vector<Mm>& ys);

} // namespace piricad::core
