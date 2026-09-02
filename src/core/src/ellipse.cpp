// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/ellipse.hpp"

#include "piricad/core/circle.hpp"
#include "piricad/core/units.hpp"

namespace piricad::core {
namespace {

Point2 vertex_at(const RingGeometry& geom, std::uint32_t slot, std::size_t i)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};

    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (i >= xs.size()) return Point2{};
    return Point2{xs[i], ys[i]};
}

} // namespace

Point2 ellipse_centre_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 0);
}

Point2 ellipse_major_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 1);
}

Point2 ellipse_minor_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 2);
}

void ellipse_outline(Point2 centre, Point2 major, Point2 minor, std::vector<Mm>& xs,
                     std::vector<Mm>& ys)
{
    // The two axis VECTORS. Together they carry the lengths and the rotation, so
    // nothing here needs an angle — which is what keeps the run deterministic.
    const auto ax = static_cast<double>(major.x - centre.x);
    const auto ay = static_cast<double>(major.y - centre.y);
    const auto bx = static_cast<double>(minor.x - centre.x);
    const auto by = static_cast<double>(minor.y - centre.y);

    // The circle's own table: `(cos t, sin t)` for `kCircleSegments` values of t,
    // built by repeated exact bisection of the four axis directions. Scaling it
    // along the two axes is exactly an ellipse, and it costs four multiplies.
    const std::vector<std::pair<double, double>>& unit = unit_circle();

    xs.reserve(xs.size() + unit.size());
    ys.reserve(ys.size() + unit.size());

    for (const auto& u : unit) {
        xs.push_back(centre.x + mm_round(ax * u.first + bx * u.second));
        ys.push_back(centre.y + mm_round(ay * u.first + by * u.second));
    }
}

} // namespace piricad::core
