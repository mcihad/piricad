// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/transform.hpp"

namespace kentos::core {
Point2 rotated_about(Point2 p, Point2 base, SinCos t)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * t.cos - dy * t.sin),
                  base.y + mm_round(dx * t.sin + dy * t.cos)};
}

Point2 scaled_about(Point2 p, Point2 base, double factor)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * factor), base.y + mm_round(dy * factor)};
}

Point2 mirrored_in_line(Point2 p, Point2 a, Point2 b)
{
    const Mm ax = b.x - a.x;
    const Mm ay = b.y - a.y;

    // THE TWO AXES A SURVEYOR ACTUALLY PICKS, handled exactly. A reflection in a
    // horizontal or vertical line is an integer negation; routing it through the
    // general formula would round a coordinate that had an exact answer.
    if (ay == 0 && ax != 0) return Point2{p.x, 2 * a.y - p.y};
    if (ax == 0 && ay != 0) return Point2{2 * a.x - p.x, p.y};

    const auto vx     = static_cast<double>(ax);
    const auto vy     = static_cast<double>(ay);
    const double len2 = vx * vx + vy * vy;
    if (len2 <= 0.0) return p; // a line through one point reflects nothing

    const auto px = static_cast<double>(p.x - a.x);
    const auto py = static_cast<double>(p.y - a.y);

    // The reflection of a vector in a line through the origin, written so the
    // only division is by the axis's squared length.
    const double t  = (px * vx + py * vy) / len2;
    const double rx = 2.0 * t * vx - px;
    const double ry = 2.0 * t * vy - py;

    return Point2{a.x + mm_round(rx), a.y + mm_round(ry)};
}

} // namespace kentos::core
