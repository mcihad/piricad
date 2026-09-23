// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"

#include <cmath>
#include <cstring>
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

namespace {

/// Whether start->through->end turns counter-clockwise, in metres so the cross
/// product stays inside the mantissa.
bool turns_ccw(Point2 a, Point2 through, Point2 b) noexcept
{
    const double sx = mm_to_metres(through.x - a.x);
    const double sy = mm_to_metres(through.y - a.y);
    const double ex = mm_to_metres(b.x - a.x);
    const double ey = mm_to_metres(b.y - a.y);
    return (sx * ey - sy * ex) > 0.0;
}

} // namespace

std::vector<std::uint8_t> encode_arc_guide(const ArcGuide& guide)
{
    std::vector<std::uint8_t> bytes(9);
    bytes[0] = static_cast<std::uint8_t>(guide.build);
    std::memcpy(bytes.data() + 1, &guide.radius, sizeof(guide.radius));
    return bytes;
}

std::optional<ArcGuide> decode_arc_guide(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 9) return std::nullopt;
    if (bytes[0] > static_cast<std::uint8_t>(ArcBuild::Radius)) return std::nullopt;

    ArcGuide guide{};
    guide.build = static_cast<ArcBuild>(bytes[0]);
    std::memcpy(&guide.radius, bytes.data() + 1, sizeof(guide.radius));
    return guide;
}

double arc_sweep_toward(Point2 centre, Point2 start, Point2 toward,
                        AngleConvention convention) noexcept
{
    if (toward == centre || start == centre) return 0.0;
    // Turns grow in the convention's own positive sense (`direction_turns`), so
    // the difference is the sweep that sense makes, folded into one turn.
    double by = direction_turns(centre, toward, convention.rule) -
                direction_turns(centre, start, convention.rule);
    if (by < 0.0) by += 1.0;
    if (!(by > 0.0) || by >= 1.0) return 0.0;
    return angle_from_turns(by, convention.unit);
}

bool arc_by_sweep(Point2 centre, Point2 start, double sweep, AngleConvention convention, Mm& radius,
                  Point2& first, Point2& last) noexcept
{
    radius = radius_through(centre, start);
    if (radius <= 0) return false;

    // SIGNED, AND NOT FOLDED INTO ONE TURN. `turns_from_udeg` folds into
    // [0, 1), which is right for a DIRECTION and wrong for a SWEEP: a highway
    // curve of −100 grad turns the other way, and folded it would turn the same
    // way by 300. The sweep is rounded to the micro-degree first, the one
    // rounding a typed angle goes through (`udeg_from_angle`).
    const double by_turns = static_cast<double>(udeg_from_angle(sweep, convention.unit)) /
                            static_cast<double>(kUDegFullCircle);
    if (by_turns == 0.0) return false;

    const double from_turns = direction_turns(centre, start, convention.rule);
    const Point2 computed =
        centre + polar_offset_turns(mm_to_metres(radius), from_turns + by_turns, convention.rule);

    // THE SWEEP IS IN THE CONVENTION'S SENSE; THE ARC IS STORED IN THE MODEL'S.
    // A positive sweep is clockwise under semt — what a surveyor means by one —
    // and counter-clockwise under matematik. An arc is stored counter-clockwise
    // from start to end, so a clockwise sweep is the same arc with its ends the
    // other way round. Getting this wrong does not draw a slightly different
    // arc: it draws the other three quarters of the circle.
    const bool ccw_in_drawing =
        convention.rule == AngleRule::Matematik ? by_turns > 0.0 : by_turns < 0.0;
    first = ccw_in_drawing ? start : computed;
    last  = ccw_in_drawing ? computed : start;
    return true;
}

bool arc_radius_side(Point2 a, Point2 b, Point2 toward) noexcept
{
    // Metres before multiplying, so the cross product stays inside the mantissa.
    const double ex = mm_to_metres(b.x - a.x);
    const double ey = mm_to_metres(b.y - a.y);
    const double tx = mm_to_metres(toward.x - a.x);
    const double ty = mm_to_metres(toward.y - a.y);
    return (ex * ty - ey * tx) > 0.0;
}

bool arc_by_radius(Point2 a, Point2 b, Mm radius, bool to_right, Point2& centre, Mm& out_radius,
                   Point2& start, Point2& end) noexcept
{
    if (radius <= 0) return false;

    Point2 left{};
    Point2 right{};
    const CircleMeet meet = circle_intersection(a, radius, b, radius, left, right);
    if (meet != CircleMeet::Two && meet != CircleMeet::Tangent) return false;

    // The centre on the LEFT of a->b bulges the arc to the RIGHT, and the arc is
    // stored counter-clockwise, so the ends swap with the side.
    centre     = to_right ? right : left;
    out_radius = radius;
    start      = to_right ? b : a;
    end        = to_right ? a : b;
    return true;
}

bool arc_from_guide(const ArcGuide& guide, std::span<const Point2> chain, Point2 cursor,
                    Point2& centre, Mm& radius, Point2& start, Point2& end) noexcept
{
    switch (guide.build) {
    case ArcBuild::ThreePoint: {
        if (chain.size() < 2) return false;
        const Point2 a       = chain[0];
        const Point2 through = chain[1];
        if (!circumcircle(a, through, cursor, centre, radius) || radius <= 0) return false;

        // WHICH WAY ROUND. An arc is stored counter-clockwise from start to end,
        // and the three points say which of the two arcs between the ends was
        // meant: the one the middle point is on.
        const bool ccw = turns_ccw(a, through, cursor);
        start          = ccw ? a : cursor;
        end            = ccw ? cursor : a;
        return true;
    }
    case ArcBuild::Tangent: {
        if (chain.size() < 2) return false;
        const Point2 from   = chain[0];
        const Point2 along  = chain[1];
        const double tx_raw = mm_to_metres(along.x - from.x);
        const double ty_raw = mm_to_metres(along.y - from.y);
        const double t_len  = std::sqrt(tx_raw * tx_raw + ty_raw * ty_raw);
        if (t_len <= 0.0) return false;
        const double tx = tx_raw / t_len;
        const double ty = ty_raw / t_len;

        // THE CENTRE IS WHERE THE PERPENDICULAR AT THE START MEETS THE CHORD'S
        // BISECTOR, and that is one equation: the centre is `from + s*n` for the
        // normal `n`, and equidistant from both ends, so
        //   s = |d|^2 / (2 * d.n)   with d = end - start.
        // `d.n == 0` means the end lies ALONG the tangent — a straight line, not
        // an arc — so nothing is answered rather than dividing by zero.
        const double nx = -ty;
        const double ny = tx;
        const double dx = static_cast<double>(cursor.x - from.x);
        const double dy = static_cast<double>(cursor.y - from.y);
        const double dn = dx * nx + dy * ny;
        if (dn > -1.0 && dn < 1.0) return false;

        const double span_sq = dx * dx + dy * dy;
        const double s       = span_sq / (2.0 * dn);
        centre               = Point2{from.x + mm_round(nx * s), from.y + mm_round(ny * s)};
        // MEASURED FROM THE ROUNDED CENTRE, not from `|s|`: the centre is what
        // gets stored, so the radius has to be the distance to THAT point or the
        // two would disagree by a millimetre.
        radius = segment_length(centre, from);
        if (radius <= 0) return false;

        // The arc leaves `from` along the tangent, so it runs counter-clockwise
        // from `from` when the centre is to its LEFT.
        const bool ccw = s > 0.0;
        start          = ccw ? from : cursor;
        end            = ccw ? cursor : from;
        return true;
    }
    case ArcBuild::Radius:
        if (chain.size() < 2) return false;
        return arc_by_radius(chain[0], chain[1], guide.radius,
                             arc_radius_side(chain[0], chain[1], cursor), centre, radius, start,
                             end);
    }
    return false;
}

} // namespace kentos::core
