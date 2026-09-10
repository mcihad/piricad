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
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
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

/// The sweep of a PARTIAL ellipse — an elliptic arc — as the kind's payload
/// (model.md R9a). Angles are the ellipse's own PARAMETER, in micro-degrees,
/// counter-clockwise from the first axis: the point at parameter t is
/// `centre + cos t · (major − centre) + sin t · (minor − centre)`, which is how
/// DXF, DWG and every CAD program define it. A whole ellipse carries no payload.
struct EllipseArc
{
    std::int64_t start_udeg{0}; ///< where the sweep begins, 0 ≤ start < 360°
    std::int64_t end_udeg{0};   ///< where it ends, counter-clockwise from start; never equal to it

    friend constexpr bool operator==(const EllipseArc&, const EllipseArc&) noexcept = default;
};

/// The payload layout version `encode_ellipse_arc` writes.
inline constexpr std::uint16_t kEllipseArcLayout = 1;

/// The arc as the 24 payload bytes: the R9a header, then the two angles.
std::vector<std::uint8_t> encode_ellipse_arc(const EllipseArc& arc);

/// The arc back from its bytes, refused when they are not exactly what
/// `encode_ellipse_arc` writes or the angles are out of range or equal.
Result<EllipseArc> decode_ellipse_arc(std::span<const std::uint8_t> payload);

/// The sweep of the ellipse in `slot`, or nothing for a whole ellipse. A payload
/// that fails to decode reads as a whole ellipse: the kind's validate refused it
/// on the way in, so this only ever sees bytes it wrote.
std::optional<EllipseArc> ellipse_arc_of(const RingGeometry& geom, std::uint32_t slot);

/// Appends the DRAWN form of a partial ellipse: an open run from the start
/// parameter to the end, counter-clockwise, both ends included exactly, in
/// steps of at most 1/128 of a turn — the whole ellipse's density.
///
/// Deterministic like `ellipse_outline`: the parameter's sine and cosine come
/// from `sin_cos_udeg`, never from libm (§7.3).
void ellipse_arc_outline(Point2 centre, Point2 major, Point2 minor, std::int64_t start_udeg,
                         std::int64_t end_udeg, std::vector<Mm>& xs, std::vector<Mm>& ys);

} // namespace kentos::core
