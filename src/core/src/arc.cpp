// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/trig.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace kentos::core {
namespace {

struct Unit
{
    double x{1.0};
    double y{0.0};
};

/// A direction as a unit vector. `sqrt` is correctly rounded by IEEE-754, so this
/// is the same vector on every platform (see the header note).
Unit unit_of(double dx, double dy)
{
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return Unit{1.0, 0.0};
    return Unit{dx / len, dy / len};
}

/// A quarter turn counter-clockwise. EXACT: it swaps and negates, so it introduces
/// no rounding at all and the quadrant boundaries stay perfectly on axis.
Unit perp(Unit u)
{
    return Unit{-u.y, u.x};
}

/// The bisector of the SHORT arc between two unit vectors. Correct only while
/// they are less than half a turn apart, which is what the quadrant split below
/// guarantees.
Unit bisect(Unit a, Unit b)
{
    return unit_of(a.x + b.x, a.y + b.y);
}

/// Whether `d` lies on the counter-clockwise sweep from `a` to `b`.
///
/// Both tests are cross products, so the answer is a sign comparison over
/// multiplications and never an angle — no `atan2`, and nothing that could
/// disagree between platforms.
bool within_ccw(Unit a, Unit b, Unit d)
{
    const double ab = a.x * b.y - a.y * b.x; ///< > 0 when the sweep is under half a turn
    const double ad = a.x * d.y - a.y * d.x;
    const double db = d.x * b.y - d.y * b.x;

    if (ab > 0.0) return ad > 0.0 && db > 0.0; // minor sweep: inside both
    return ad > 0.0 || db > 0.0;               // major sweep: outside the short way
}

} // namespace

Mm arc_radius_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return 0;
    const auto xs = geom.ring_xs(rs.first);
    if (xs.size() < 4) return 0;
    const Mm r = xs[1] - xs[0];
    return r < 0 ? -r : r;
}

Point2 arc_centre_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.size() < 4) return Point2{};
    return Point2{xs[0], ys[0]};
}

Point2 arc_start_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.size() < 4) return Point2{};
    return Point2{xs[2], ys[2]};
}

Point2 arc_end_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.size() < 4) return Point2{};
    return Point2{xs[3], ys[3]};
}

void arc_outline(Point2 centre, Mm radius, Point2 start, Point2 end, std::vector<Mm>& xs,
                 std::vector<Mm>& ys)
{
    if (radius <= 0) return;

    const Unit from =
        unit_of(static_cast<double>(start.x - centre.x), static_cast<double>(start.y - centre.y));
    const Unit to =
        unit_of(static_cast<double>(end.x - centre.x), static_cast<double>(end.y - centre.y));

    // CUT AT THE QUADRANTS FIRST. Chord bisection finds the short way round, so a
    // sweep of more than half a turn would come out as its own complement — the
    // arc drawn on the wrong side of the circle. Every piece below is at most a
    // quarter turn, and the cuts are exact because `perp` only swaps and negates.
    std::vector<Unit> corners{from};
    Unit q = perp(from);
    for (int i = 0; i < 3; ++i) {
        if (within_ccw(from, to, q)) corners.push_back(q);
        q = perp(q);
    }
    corners.push_back(to);

    // Four bisections per piece: 17 points across a quarter turn, which matches
    // the 128-gon a full circle is drawn with. The picture only — the radius and
    // the ends are what the record holds.
    constexpr int kDepth = 4;

    std::vector<Unit> run;
    run.reserve(corners.size() * 17);
    run.push_back(corners.front());

    for (std::size_t c = 0; c + 1 < corners.size(); ++c) {
        std::vector<Unit> piece{corners[c], corners[c + 1]};
        for (int d = 0; d < kDepth; ++d) {
            std::vector<Unit> next;
            next.reserve(piece.size() * 2 - 1);
            for (std::size_t i = 0; i + 1 < piece.size(); ++i) {
                next.push_back(piece[i]);
                next.push_back(bisect(piece[i], piece[i + 1]));
            }
            next.push_back(piece.back());
            piece = std::move(next);
        }
        // The first point of this piece is the last of the one before it.
        for (std::size_t i = 1; i < piece.size(); ++i)
            run.push_back(piece[i]);
    }

    const auto r = static_cast<double>(radius);
    xs.reserve(xs.size() + run.size());
    ys.reserve(ys.size() + run.size());
    for (const Unit& u : run) {
        xs.push_back(centre.x + mm_round(r * u.x));
        ys.push_back(centre.y + mm_round(r * u.y));
    }
}

namespace {

/// `v` scaled to unit length, or (0, 0) for the zero vector.
std::pair<double, double> unit(Point2 centre, Point2 p)
{
    const double dx  = static_cast<double>(p.x - centre.x);
    const double dy  = static_cast<double>(p.y - centre.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return {0.0, 0.0};
    return {dx / len, dy / len};
}

} // namespace

Point2 arc_midpoint(Point2 centre, Mm radius, Point2 start, Point2 end)
{
    const auto [ax, ay] = unit(centre, start);
    const auto [bx, by] = unit(centre, end);
    const double cross  = ax * by - ay * bx;
    const double dot    = ax * bx + ay * by;

    double mx = ax + bx;
    double my = ay + by;
    if (cross == 0.0 && dot < 0.0) {
        // Exactly a half turn: the sum vanishes, and the midpoint is the
        // perpendicular a quarter turn counter-clockwise from the start.
        mx = -ay;
        my = ax;
    } else if (cross == 0.0 && dot >= 0.0) {
        // Ends coincide: a full turn, whose midpoint is the far side.
        mx = -ax;
        my = -ay;
    } else {
        // The sum bisects the SHORTER way round; past a half turn the
        // counter-clockwise sweep is the longer way, so it is the negation.
        if (cross < 0.0) {
            mx = -mx;
            my = -my;
        }
        const double len = std::sqrt(mx * mx + my * my);
        mx /= len;
        my /= len;
    }
    const auto r = static_cast<double>(radius);
    return Point2{centre.x + mm_round(r * mx), centre.y + mm_round(r * my)};
}

bool on_arc(Point2 centre, Point2 start, Point2 end, Point2 p)
{
    const auto [ax, ay] = unit(centre, start);
    const auto [bx, by] = unit(centre, end);
    const auto [px, py] = unit(centre, p);
    if (px == 0.0 && py == 0.0) return false;

    const double ab     = ax * by - ay * bx; // sign of the sweep's turn
    const double ap     = ax * py - ay * px; // start -> p
    const double pb     = px * by - py * bx; // p -> end
    const double dot_ab = ax * bx + ay * by;

    // Coincident ends are a full turn: everything on the circle is on it.
    if (ab == 0.0 && dot_ab >= 0.0) return true;

    // Sweep of at most a half turn: p sits between the two, turning the same way.
    if (ab > 0.0 || (ab == 0.0 && dot_ab < 0.0)) return ap >= 0.0 && pb >= 0.0;

    // Sweep past a half turn: p is on it unless it sits strictly inside the
    // SHORT way round from end back to start.
    return !(ap < 0.0 && pb < 0.0);
}

std::int64_t arc_sweep_udeg(Point2 centre, Point2 from, Point2 to) noexcept
{
    const std::int64_t a0 = atan2_udeg(from.y - centre.y, from.x - centre.x);
    const std::int64_t a1 = atan2_udeg(to.y - centre.y, to.x - centre.x);
    std::int64_t sweep    = a1 - a0;
    if (sweep <= 0) sweep += kUDegFullCircle;
    return sweep;
}

bool arc_from_bulge(Point2 a, Point2 b, double bulge, Point2& centre, Mm& radius,
                    bool& ccw) noexcept
{
    if (bulge == 0.0 || a == b) return false;
    // Translated to `a` before anything is multiplied (core.md R3).
    const double dx    = static_cast<double>(b.x - a.x);
    const double dy    = static_cast<double>(b.y - a.y);
    const double chord = std::sqrt(dx * dx + dy * dy);
    if (chord <= 0.0) return false;
    const double h = chord / 2.0;
    const double s = std::abs(bulge) * h; // sagitta
    if (s <= 1e-9) return false;          // a bulge too small to bend a millimetre
    const double r = (h * h + s * s) / (2.0 * s);
    // The centre sits on the chord's perpendicular bisector, `r - s` from the
    // chord's midpoint, on the side the arc bulges AWAY from.
    const double mx = dx / 2.0;
    const double my = dy / 2.0;
    const double nx = -dy / chord; // left normal of the chord
    const double ny = dx / chord;
    const double d  = r - s;
    ccw             = bulge > 0.0;
    // A counter-clockwise bulge bulges to the right of a→b, so its centre is to
    // the left; clockwise the other way round.
    const double sign = ccw ? 1.0 : -1.0;
    centre = Point2{a.x + mm_round(mx + sign * d * nx), a.y + mm_round(my + sign * d * ny)};
    radius = mm_round(r);
    return radius > 0;
}

double bulge_from_arc(Point2 a, Point2 b, Point2 centre, Mm radius, bool ccw) noexcept
{
    const double dx    = static_cast<double>(b.x - a.x);
    const double dy    = static_cast<double>(b.y - a.y);
    const double chord = std::sqrt(dx * dx + dy * dy);
    if (chord <= 0.0 || radius <= 0) return 0.0;
    const double h       = chord / 2.0;
    const double r       = static_cast<double>(radius);
    const double under   = r * r - h * h;
    const double leg     = under > 0.0 ? std::sqrt(under) : 0.0;
    const std::int64_t s = ccw ? arc_sweep_udeg(centre, a, b) : arc_sweep_udeg(centre, b, a);
    // Past a half turn the sagitta is on the far side of the centre.
    const double sagitta = s > kUDegFullCircle / 2 ? r + leg : r - leg;
    const double bulge   = sagitta / h;
    return ccw ? bulge : -bulge;
}

} // namespace kentos::core
