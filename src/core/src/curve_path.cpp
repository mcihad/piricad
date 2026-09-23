// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a curve walked piece by piece. See curve_path.hpp.
#include "kentos_cad/core/curve_path.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::core {
namespace {

constexpr std::int64_t kTurn = kUDegFullCircle;

/// Parameters this close are one place: a millionth of a piece.
constexpr double kSame = 1e-9;

/// Half a millimetre, in metres: nearer than this a line touches a circle.
constexpr double kTouch = 0.0005;

std::int64_t wrap(std::int64_t a) noexcept
{
    a %= kTurn;
    return a < 0 ? a + kTurn : a;
}

std::int64_t angle_of(Point2 centre, Point2 p) noexcept
{
    return atan2_udeg(p.y - centre.y, p.x - centre.x);
}

Point2 on_circle(Point2 centre, Mm radius, std::int64_t udeg) noexcept
{
    const SinCos sc = sin_cos_udeg(wrap(udeg));
    return Point2{centre.x + mm_round(static_cast<double>(radius) * sc.cos),
                  centre.y + mm_round(static_cast<double>(radius) * sc.sin)};
}

Point2 piece_point(const PathPiece& p, double t) noexcept
{
    if (t <= 0.0) return p.from;
    if (t >= 1.0) return p.to;
    if (p.kind == PathPiece::Kind::Segment)
        return Point2{p.from.x + mm_round(static_cast<double>(p.to.x - p.from.x) * t),
                      p.from.y + mm_round(static_cast<double>(p.to.y - p.from.y) * t)};
    const auto turn =
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.sweep_udeg) * t));
    return on_circle(p.centre, p.radius, angle_of(p.centre, p.from) + turn);
}

/// How far along an arc piece the direction of `q` lies, or a value outside
/// [0, 1] when it is off the sweep (then: below 0 nearer the start, above 1
/// nearer the end).
double arc_fraction(const PathPiece& p, Point2 q) noexcept
{
    const std::int64_t d = wrap(angle_of(p.centre, q) - angle_of(p.centre, p.from));
    if (p.sweep_udeg >= kTurn) return static_cast<double>(d) / static_cast<double>(kTurn);
    if (d <= p.sweep_udeg) return static_cast<double>(d) / static_cast<double>(p.sweep_udeg);
    // Off the sweep: which end is nearer round the circle.
    const std::int64_t past = d - p.sweep_udeg; // beyond the end
    const std::int64_t back = kTurn - d;        // before the start
    return past <= back ? 1.0 + static_cast<double>(past) / static_cast<double>(p.sweep_udeg)
                        : -static_cast<double>(back) / static_cast<double>(p.sweep_udeg);
}

/// The fraction of a segment piece nearest `q`, unclamped.
double segment_fraction(const PathPiece& p, Point2 q) noexcept
{
    const double ex   = mm_to_metres(p.to.x - p.from.x);
    const double ey   = mm_to_metres(p.to.y - p.from.y);
    const double len2 = ex * ex + ey * ey;
    if (len2 <= 0.0) return 0.0;
    return (mm_to_metres(q.x - p.from.x) * ex + mm_to_metres(q.y - p.from.y) * ey) / len2;
}

/// Whether a point already known to be on the circle lies on the piece's arc.
bool on_piece_arc(const PathPiece& p, Point2 q) noexcept
{
    if (p.sweep_udeg >= kTurn) return true;
    const double f = arc_fraction(p, q);
    return f >= -kSame && f <= 1.0 + kSame;
}

/// A meet found between two pieces: the point, and whether it only touches.
struct Meet
{
    Point2 point{};
    bool touching{false};
};

/// Where the infinite line through `a`-`b` meets the circle of the arc piece
/// `c`, each with its parameter along the line — whether or not on the arc; the
/// caller keeps what lies on it.
void line_circle(Point2 a0, Point2 b0, const PathPiece& c, std::vector<LineMeet>& out)
{
    // In metres about the circle's centre, so the squares stay small.
    const double ax = mm_to_metres(a0.x - c.centre.x);
    const double ay = mm_to_metres(a0.y - c.centre.y);
    const double dx = mm_to_metres(b0.x - a0.x);
    const double dy = mm_to_metres(b0.y - a0.y);
    const double r  = mm_to_metres(c.radius);
    const double a  = dx * dx + dy * dy;
    if (a <= 0.0) return;

    const auto push = [&](double t, bool touching) {
        out.push_back(LineMeet{t,
                               Point2{a0.x + mm_round(static_cast<double>(b0.x - a0.x) * t),
                                      a0.y + mm_round(static_cast<double>(b0.y - a0.y) * t)},
                               touching});
    };
    // The distance from the centre to the line decides: past the radius the
    // line misses, at it (to half a millimetre) it touches, inside it crosses.
    const double h = std::abs(ax * dy - ay * dx) / std::sqrt(a);
    const double b = 2.0 * (ax * dx + ay * dy);
    if (h > r + kTouch) return;
    if (std::abs(h - r) <= kTouch) {
        push(-b / (2.0 * a), true);
        return;
    }
    const double cc   = ax * ax + ay * ay - r * r;
    const double disc = b * b - 4.0 * a * cc;
    if (disc <= 0.0) {
        push(-b / (2.0 * a), true);
        return;
    }
    // The stable form of the two roots: no cancellation between `b` and the root.
    const double s_disc = std::sqrt(disc);
    const double q      = -0.5 * (b + (b >= 0.0 ? s_disc : -s_disc));
    const double t1     = q / a;
    const double t2     = q != 0.0 ? cc / q : t1;
    push(std::min(t1, t2), false);
    push(std::max(t1, t2), false);
}

/// Where the segment `s` meets the circle of the arc piece `c` — on the segment,
/// whether or not on the arc.
void segment_circle(const PathPiece& s, const PathPiece& c, std::vector<Meet>& out)
{
    std::vector<LineMeet> line;
    line_circle(s.from, s.to, c, line);
    for (const LineMeet& m : line) {
        if (m.t < -kSame || m.t > 1.0 + kSame) continue;
        const double ct = std::clamp(m.t, 0.0, 1.0);
        out.push_back(Meet{Point2{s.from.x + mm_round(static_cast<double>(s.to.x - s.from.x) * ct),
                                  s.from.y + mm_round(static_cast<double>(s.to.y - s.from.y) * ct)},
                           m.touching});
    }
}

/// Every meet between two pieces, each point on both.
void piece_meets(const PathPiece& p, const PathPiece& q, std::vector<Meet>& out)
{
    using K = PathPiece::Kind;
    std::vector<Meet> found;
    if (p.kind == K::Segment && q.kind == K::Segment) {
        Point2 at{};
        double t = 0.0;
        double u = 0.0;
        if (!line_intersection(p.from, p.to, q.from, q.to, at, t, u)) return;
        if (t < -kSame || t > 1.0 + kSame || u < -kSame || u > 1.0 + kSame) return;
        found.push_back(Meet{at, false});
    } else if (p.kind == K::Segment) {
        segment_circle(p, q, found);
    } else if (q.kind == K::Segment) {
        segment_circle(q, p, found);
    } else {
        Point2 left{};
        Point2 right{};
        const CircleMeet m =
            circle_intersection(p.centre, p.radius, q.centre, q.radius, left, right);
        if (m == CircleMeet::Two) {
            found.push_back(Meet{left, false});
            found.push_back(Meet{right, false});
        } else if (m == CircleMeet::Tangent) {
            found.push_back(Meet{left, true});
        }
    }
    for (const Meet& m : found) {
        if (p.kind == K::Arc && !on_piece_arc(p, m.point)) continue;
        if (q.kind == K::Arc && !on_piece_arc(q, m.point)) continue;
        out.push_back(m);
    }
}

/// Merges two arcs of one circle meeting end to start — the two halves of a
/// closed path's sub-path across its seam.
void merge_arcs(std::vector<PathPiece>& pieces)
{
    std::vector<PathPiece> out;
    for (const PathPiece& p : pieces) {
        if (!out.empty()) {
            PathPiece& last = out.back();
            if (last.kind == PathPiece::Kind::Arc && p.kind == PathPiece::Kind::Arc &&
                last.centre == p.centre && last.radius == p.radius && last.to == p.from &&
                last.sweep_udeg + p.sweep_udeg <= kTurn) {
                last.to = p.to;
                last.sweep_udeg += p.sweep_udeg;
                continue;
            }
        }
        out.push_back(p);
    }
    pieces = std::move(out);
}

} // namespace

bool comes_before(PathPlace a, PathPlace b) noexcept
{
    if (a.piece != b.piece) return a.piece < b.piece;
    return a.t < b.t - kSame;
}

std::optional<CurvePath> path_of(const Document& doc, EntityId e)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return std::nullopt;
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];

    CurvePath path;
    switch (ents.kind[e]) {
    case kPolylineKind: {
        if (doc.texts().has(slot)) return std::nullopt;
        const RingSpan span = geom.rings_of(slot);
        if (span.count != 1) return std::nullopt;
        const auto xs = geom.ring_xs(span.first);
        const auto ys = geom.ring_ys(span.first);
        if (xs.size() < 2) return std::nullopt;
        path.closed         = geom.ring_role[span.first] != RingRole::Open;
        const std::size_t n = xs.size();
        for (std::size_t i = 0; i + 1 < n; ++i) {
            PathPiece p;
            p.from = Point2{xs[i], ys[i]};
            p.to   = Point2{xs[i + 1], ys[i + 1]};
            if (p.from != p.to) path.pieces.push_back(p);
        }
        if (path.closed) {
            PathPiece p;
            p.from = Point2{xs[n - 1], ys[n - 1]};
            p.to   = Point2{xs[0], ys[0]};
            if (p.from != p.to) path.pieces.push_back(p);
        }
        break;
    }
    case kArcKind: {
        PathPiece p;
        p.kind       = PathPiece::Kind::Arc;
        p.centre     = arc_centre_of(geom, slot);
        p.radius     = arc_radius_of(geom, slot);
        p.from       = arc_start_of(geom, slot);
        p.to         = arc_end_of(geom, slot);
        p.sweep_udeg = arc_sweep_udeg(p.centre, p.from, p.to);
        if (p.radius <= 0) return std::nullopt;
        path.pieces.push_back(p);
        break;
    }
    case kCircleKind: {
        PathPiece p;
        p.kind       = PathPiece::Kind::Arc;
        p.centre     = circle_centre_of(geom, slot);
        p.radius     = circle_radius_of(geom, slot);
        p.from       = Point2{p.centre.x + p.radius, p.centre.y};
        p.to         = p.from;
        p.sweep_udeg = kTurn;
        if (p.radius <= 0) return std::nullopt;
        path.pieces.push_back(p);
        path.closed = true;
        break;
    }
    default: return std::nullopt;
    }
    if (path.pieces.empty()) return std::nullopt;
    return path;
}

Point2 point_at(const CurvePath& path, PathPlace at)
{
    if (path.pieces.empty()) return Point2{};
    const std::size_t i = std::min(at.piece, path.pieces.size() - 1);
    return piece_point(path.pieces[i], at.t);
}

PathPlace place_of(const CurvePath& path, Point2 probe)
{
    PathPlace best{};
    double nearest = -1.0;
    for (std::size_t i = 0; i < path.pieces.size(); ++i) {
        const PathPiece& p = path.pieces[i];
        const double t = std::clamp(p.kind == PathPiece::Kind::Segment ? segment_fraction(p, probe)
                                                                       : arc_fraction(p, probe),
                                    0.0, 1.0);
        const double d = distance_squared(piece_point(p, t), probe);
        if (nearest < 0.0 || d < nearest) {
            nearest = d;
            best    = PathPlace{i, t};
        }
    }
    return best;
}

PathPlace path_start(const CurvePath& /*path*/) noexcept
{
    return PathPlace{0, 0.0};
}

PathPlace path_end(const CurvePath& path) noexcept
{
    return PathPlace{path.pieces.empty() ? 0 : path.pieces.size() - 1, 1.0};
}

Mm path_length(const CurvePath& path)
{
    double metres = 0.0;
    for (const PathPiece& p : path.pieces) {
        if (p.kind == PathPiece::Kind::Segment) {
            metres += mm_to_metres(segment_length(p.from, p.to));
        } else {
            // r · θ, the sweep in radians from whole micro-degrees.
            metres += mm_to_metres(p.radius) * static_cast<double>(p.sweep_udeg) *
                      (kPi / 180.0 / 1000000.0);
        }
    }
    return mm_round(metres * static_cast<double>(kMmPerMetre));
}

std::vector<PathCrossing> path_crossings(const CurvePath& path, const CurvePath& other)
{
    std::vector<PathCrossing> out;
    std::vector<Meet> meets;
    for (std::size_t i = 0; i < path.pieces.size(); ++i) {
        const PathPiece& p = path.pieces[i];
        for (const PathPiece& q : other.pieces) {
            meets.clear();
            piece_meets(p, q, meets);
            for (const Meet& m : meets) {
                const double t =
                    std::clamp(p.kind == PathPiece::Kind::Segment ? segment_fraction(p, m.point)
                                                                  : arc_fraction(p, m.point),
                               0.0, 1.0);
                out.push_back(PathCrossing{PathPlace{i, t}, m.point, m.touching});
            }
        }
    }
    std::ranges::sort(
        out, [](const PathCrossing& a, const PathCrossing& b) { return comes_before(a.at, b.at); });
    // ONE MEET, ONE CROSSING: a vertex two pieces share — on either curve —
    // finds the same point twice.
    std::vector<PathCrossing> unique;
    for (const PathCrossing& c : out) {
        if (!unique.empty() && distance_squared(unique.back().point, c.point) <= 1.0) {
            unique.back().touching = unique.back().touching && c.touching;
            continue;
        }
        unique.push_back(c);
    }
    // And on a closed path the seam is one place: a meet at the start and one at
    // the end are the same meet.
    if (path.closed && unique.size() >= 2 &&
        distance_squared(unique.front().point, unique.back().point) <= 1.0)
        unique.pop_back();
    return unique;
}

std::vector<LineMeet> line_meets(Point2 a, Point2 b, const PathPiece& piece)
{
    std::vector<LineMeet> out;
    if (piece.kind == PathPiece::Kind::Segment) {
        Point2 at{};
        double t = 0.0;
        double u = 0.0;
        if (line_intersection(a, b, piece.from, piece.to, at, t, u) && u >= -kSame &&
            u <= 1.0 + kSame)
            out.push_back(LineMeet{t, at, false});
        return out;
    }
    std::vector<LineMeet> raw;
    line_circle(a, b, piece, raw);
    for (const LineMeet& m : raw)
        if (on_piece_arc(piece, m.point)) out.push_back(m);
    return out;
}

std::vector<Point2> circle_meets(const PathPiece& arc, const PathPiece& piece)
{
    PathPiece whole  = arc;
    whole.to         = whole.from;
    whole.sweep_udeg = kTurn;
    std::vector<Meet> meets;
    piece_meets(whole, piece, meets);
    std::vector<Point2> out;
    out.reserve(meets.size());
    for (const Meet& m : meets)
        out.push_back(m.point);
    return out;
}

CurvePath sub_path(const CurvePath& path, PathPlace a, PathPlace b)
{
    CurvePath out;
    if (path.pieces.empty()) return out;
    const std::size_t last = path.pieces.size() - 1;
    a.piece                = std::min(a.piece, last);
    b.piece                = std::min(b.piece, last);

    const auto take = [&out, &path](std::size_t i, double t0, double t1) {
        if (t1 - t0 <= kSame) return;
        const PathPiece& p = path.pieces[i];
        PathPiece q        = p;
        q.from             = piece_point(p, t0);
        q.to               = piece_point(p, t1);
        if (p.kind == PathPiece::Kind::Arc) {
            q.sweep_udeg = static_cast<std::int64_t>(
                std::llround(static_cast<double>(p.sweep_udeg) * (t1 - t0)));
            if (q.sweep_udeg <= 0) return;
        } else if (q.from == q.to) {
            return;
        }
        out.pieces.push_back(q);
    };
    const auto run = [&take](std::size_t from_piece, double from_t, std::size_t to_piece,
                             double to_t) {
        if (from_piece == to_piece) {
            take(from_piece, from_t, to_t);
            return;
        }
        take(from_piece, from_t, 1.0);
        for (std::size_t i = from_piece + 1; i < to_piece; ++i)
            take(i, 0.0, 1.0);
        take(to_piece, 0.0, to_t);
    };

    if (!comes_before(b, a)) {
        run(a.piece, a.t, b.piece, b.t);
    } else if (path.closed) {
        run(a.piece, a.t, last, 1.0);
        run(0, 0.0, b.piece, b.t);
    }
    merge_arcs(out.pieces);
    return out;
}

std::vector<Point2> path_vertices(const CurvePath& path)
{
    std::vector<Point2> out;
    for (const PathPiece& p : path.pieces) {
        if (out.empty()) out.push_back(p.from);
        out.push_back(p.to);
    }
    return out;
}

void path_outline(const CurvePath& path, std::vector<Mm>& xs, std::vector<Mm>& ys)
{
    for (const PathPiece& p : path.pieces) {
        if (p.kind == PathPiece::Kind::Segment) {
            if (xs.empty() || xs.back() != p.from.x || ys.back() != p.from.y) {
                xs.push_back(p.from.x);
                ys.push_back(p.from.y);
            }
            xs.push_back(p.to.x);
            ys.push_back(p.to.y);
            continue;
        }
        std::vector<Mm> ax;
        std::vector<Mm> ay;
        if (p.sweep_udeg >= kTurn) {
            // A whole circle: two half turns, because an arc's two ends coincide
            // there and `arc_outline` would read them as nothing.
            const Point2 far =
                on_circle(p.centre, p.radius, angle_of(p.centre, p.from) + kTurn / 2);
            arc_outline(p.centre, p.radius, p.from, far, ax, ay);
            std::vector<Mm> bx;
            std::vector<Mm> by;
            arc_outline(p.centre, p.radius, far, p.to, bx, by);
            ax.insert(ax.end(), bx.begin() + (bx.empty() ? 0 : 1), bx.end());
            ay.insert(ay.end(), by.begin() + (by.empty() ? 0 : 1), by.end());
        } else {
            arc_outline(p.centre, p.radius, p.from, p.to, ax, ay);
        }
        for (std::size_t i = 0; i < ax.size(); ++i) {
            if (i == 0 && !xs.empty() && xs.back() == ax[0] && ys.back() == ay[0]) continue;
            xs.push_back(ax[i]);
            ys.push_back(ay[i]);
        }
    }
}

} // namespace kentos::core
