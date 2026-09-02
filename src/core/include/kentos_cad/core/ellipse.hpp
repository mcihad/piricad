// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the ellipse, stored by its definition.
//
// STORED AS WHAT IT IS, like the circle and the arc beside it: a centre and the
// two axis endpoints, five numbers, from which the whole shape follows. The
// many-sided run is only what gets DRAWN. A drawing that stored the run instead
// would lose the shape's identity the moment somebody zoomed in — a hundred and
// twenty-eight chords look like an ellipse at one scale and like a polygon at
// twenty times that.
//
// THE STORAGE IS THREE VERTICES in one open ring: the centre, the end of the
// first axis, and the end of the second. The second is kept as a point rather
// than as a length so a rotated ellipse needs no angle anywhere — the two axis
// VECTORS carry the rotation, and no trigonometry is stored or read back.
//
// DETERMINISM (§7.3). The outline is the unit circle's own table — the same
// deterministic bisection `circle_outline` walks — with each unit direction
// scaled along the two axis vectors. Multiplication and addition only: no `cos`,
// no `sin`, and the same answer on every platform.
#pragma once

#include "kentos_cad/core/geometry.hpp"

#include <cstdint>
#include <vector>

namespace kentos::core {

/// The centre of the ellipse in `slot`.
Point2 ellipse_centre_of(const RingGeometry& geom, std::uint32_t slot);

/// The end of its first (major) axis — a point, not a length.
Point2 ellipse_major_of(const RingGeometry& geom, std::uint32_t slot);

/// The end of its second (minor) axis.
Point2 ellipse_minor_of(const RingGeometry& geom, std::uint32_t slot);

/// Appends the DRAWN form: a closed run of vertices, counter-clockwise from the
/// major axis, WITHOUT a repeated closing vertex.
void ellipse_outline(Point2 centre, Point2 major, Point2 minor, std::vector<Mm>& xs,
                     std::vector<Mm>& ys);

} // namespace kentos::core
