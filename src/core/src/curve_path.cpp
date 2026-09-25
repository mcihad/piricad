// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a curve walked piece by piece. See curve_path.hpp.
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/precision.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include "curve_eval.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>

namespace kentos::core {
namespace {

constexpr std::int64_t kTurn = kUDegFullCircle;

/// Parameters this close are one place: a millionth of a piece.
constexpr double kSame = 1e-9;

/// Half a millimetre, in metres: nearer than this a line touches a circle
/// (`kOnCurveMm`, precision.hpp).
constexpr double kTouch = kOnCurveMm / 1000.0;

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

/// Whether a piece is one of the kinds only `PathScope::Curves` walks.
bool curved(const PathPiece& p) noexcept
{
    return p.kind == PathPiece::Kind::Ellipse || p.kind == PathPiece::Kind::Spline;
}

/// The point of an ellipse piece's ellipse at parameter `udeg`, as the
/// ellipse kind draws it (`sin_cos_udeg`, one rounding).
Point2 ellipse_at(const PathPiece& e, std::int64_t udeg) noexcept
{
    const SinCos sc = sin_cos_udeg(wrap(udeg));
    const auto ux   = static_cast<double>(e.major.x - e.centre.x);
    const auto uy   = static_cast<double>(e.major.y - e.centre.y);
    const auto vx   = static_cast<double>(e.minor.x - e.centre.x);
    const auto vy   = static_cast<double>(e.minor.y - e.centre.y);
    return Point2{e.centre.x + mm_round((sc.cos * ux) + (sc.sin * vx)),
                  e.centre.y + mm_round((sc.cos * uy) + (sc.sin * vy))};
}

Point2 piece_point(const PathPiece& p, double t)
{
    if (t <= 0.0) return p.from;
    if (t >= 1.0) return p.to;
    if (p.kind == PathPiece::Kind::Segment)
        return Point2{p.from.x + mm_round(static_cast<double>(p.to.x - p.from.x) * t),
                      p.from.y + mm_round(static_cast<double>(p.to.y - p.from.y) * t)};
    if (curved(p)) return curve::Eval(p, p.from).world(t);
    const auto turn =
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.sweep_udeg) * t));
    return on_circle(p.centre, p.radius, angle_of(p.centre, p.from) + turn);
}

/// How much an arc piece turns, whichever way it is walked.
std::int64_t turn_of(const PathPiece& p) noexcept
{
    return p.sweep_udeg < 0 ? -p.sweep_udeg : p.sweep_udeg;
}

/// How far along an arc piece the direction of `q` lies, or a value outside
/// [0, 1] when it is off the sweep (then: below 0 nearer the start, above 1
/// nearer the end). Measured the way the piece is walked.
double arc_fraction(const PathPiece& p, Point2 q) noexcept
{
    const std::int64_t s     = turn_of(p);
    const std::int64_t delta = angle_of(p.centre, q) - angle_of(p.centre, p.from);
    const std::int64_t d     = wrap(p.sweep_udeg < 0 ? -delta : delta);
    if (s >= kTurn) return static_cast<double>(d) / static_cast<double>(kTurn);
    if (d <= s) return static_cast<double>(d) / static_cast<double>(s);
    // Off the sweep: which end is nearer round the circle.
    const std::int64_t past = d - s;     // beyond the end
    const std::int64_t back = kTurn - d; // before the start
    return past <= back ? 1.0 + static_cast<double>(past) / static_cast<double>(s)
                        : -static_cast<double>(back) / static_cast<double>(s);
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

/// The fraction of a piece nearest `q`: closed forms for a segment and an arc
/// (unclamped), the numerical nearest for an ellipse and a spline.
double fraction_of(const PathPiece& p, Point2 q)
{
    switch (p.kind) {
    case PathPiece::Kind::Segment: return segment_fraction(p, q);
    case PathPiece::Kind::Arc: return arc_fraction(p, q);
    case PathPiece::Kind::Ellipse:
    case PathPiece::Kind::Spline: return curve::nearest_t(p, q);
    }
    return 0.0;
}

/// How long a piece is, in metres.
double piece_metres(const PathPiece& p)
{
    switch (p.kind) {
    case PathPiece::Kind::Segment: return mm_to_metres(segment_length(p.from, p.to));
    case PathPiece::Kind::Arc:
        // r · θ, the sweep in radians from whole micro-degrees.
        return mm_to_metres(p.radius) * static_cast<double>(turn_of(p)) * (kPi / 180.0 / 1000000.0);
    case PathPiece::Kind::Ellipse:
    case PathPiece::Kind::Spline: return curve::length(p, 0.0, 1.0);
    }
    return 0.0;
}

/// Whether a point already known to be on the circle lies on the piece's arc.
bool on_piece_arc(const PathPiece& p, Point2 q) noexcept
{
    if (turn_of(p) >= kTurn) return true;
    const double f = arc_fraction(p, q);
    return f >= -kSame && f <= 1.0 + kSame;
}

/// A meet found between two pieces: the point, whether it only touches, and
/// — when the solve that found it knows — where it lies on the first piece.
struct Meet
{
    Point2 point{};
    bool touching{false};
    double s{-1.0}; ///< on the first piece, or negative when not known
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

/// An arc piece as the ellipse piece it is: the same circle, its axes due east
/// and due north — so two arcs of one circle share their stretch through the
/// ellipse's overlap test.
PathPiece as_ellipse(const PathPiece& arc)
{
    PathPiece e  = arc;
    e.kind       = PathPiece::Kind::Ellipse;
    e.major      = Point2{arc.centre.x + arc.radius, arc.centre.y};
    e.minor      = Point2{arc.centre.x, arc.centre.y + arc.radius};
    e.start_udeg = wrap(angle_of(arc.centre, arc.from));
    return e;
}

/// Every meet between two pieces, each point on both.
void piece_meets(const PathPiece& p, const PathPiece& q, std::vector<Meet>& out)
{
    using K = PathPiece::Kind;
    if (curved(p) || curved(q)) {
        const curve::Meets m = curve::meets(p, q);
        for (const curve::Hit& h : m.hits)
            out.push_back(Meet{h.point, h.touching, h.s});
        return;
    }
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

/// Two spline pieces of one degree, the first ending where the second
/// begins, as the one spline they make: both clamped, the knots run on with
/// the joint a knot `degree` times over, so each keeps its shape exactly.
std::optional<PathPiece> joined_splines(const PathPiece& a, const PathPiece& b)
{
    if (a.spline.degree != b.spline.degree || a.to != b.from) return std::nullopt;
    curve::SplineParts first  = curve::clamped({a.controls, a.spline});
    curve::SplineParts second = curve::clamped({b.controls, b.spline});
    const auto p              = static_cast<std::size_t>(a.spline.degree);
    if (first.controls.size() < p + 1 || second.controls.size() < p + 1) return std::nullopt;
    const std::vector<std::int64_t>& ka = first.def.knots_nano;
    const std::vector<std::int64_t>& kb = second.def.knots_nano;
    const std::int64_t end              = ka.back();
    const std::int64_t shift            = end - kb.front();

    PathPiece out;
    out.kind            = PathPiece::Kind::Spline;
    out.from            = a.from;
    out.to              = b.to;
    out.spline          = first.def;
    out.spline.closed   = false;
    out.controls        = first.controls;
    out.controls.back() = a.to;
    out.controls.insert(out.controls.end(), second.controls.begin() + 1, second.controls.end());
    std::vector<std::int64_t> knots(
        ka.begin(), ka.begin() + static_cast<std::ptrdiff_t>(first.controls.size()));
    for (std::size_t i = 0; i < p; ++i)
        knots.push_back(end);
    for (std::size_t i = p + 1; i < kb.size(); ++i)
        knots.push_back(kb[i] + shift);
    out.spline.knots_nano = std::move(knots);
    const bool rational   = !first.def.weights_nano.empty() || !second.def.weights_nano.empty();
    out.spline.weights_nano.clear();
    if (rational) {
        // A NURBS curve does not change when all its weights scale alike, so
        // the second's are scaled to meet the first's at the joint.
        const auto weights = [](const curve::SplineParts& s) {
            return s.def.weights_nano.empty() ? std::vector<std::int64_t>(s.controls.size(), kNano)
                                              : s.def.weights_nano;
        };
        std::vector<std::int64_t> wa = weights(first);
        std::vector<std::int64_t> wb = weights(second);
        const double scale =
            wb.front() > 0 ? static_cast<double>(wa.back()) / static_cast<double>(wb.front()) : 1.0;
        for (std::size_t i = 1; i < wb.size(); ++i)
            wa.push_back(std::llround(static_cast<double>(wb[i]) * scale));
        out.spline.rational     = true;
        out.spline.weights_nano = std::move(wa);
    }
    return out;
}

/// Merges neighbours that are one curve: two arcs of one circle, two arcs of
/// one ellipse, walked the same way and meeting end to start — the two halves
/// of a closed path's sub-path across its seam — and two splines.
void merge_pieces(std::vector<PathPiece>& pieces)
{
    std::vector<PathPiece> out;
    for (const PathPiece& p : pieces) {
        if (!out.empty()) {
            PathPiece& last = out.back();
            // One circle, walked the same way, not past a whole turn.
            if (last.kind == PathPiece::Kind::Arc && p.kind == PathPiece::Kind::Arc &&
                last.centre == p.centre && last.radius == p.radius && last.to == p.from &&
                (last.sweep_udeg < 0) == (p.sweep_udeg < 0) &&
                turn_of(last) + turn_of(p) <= kTurn) {
                last.to = p.to;
                last.sweep_udeg += p.sweep_udeg;
                continue;
            }
            // One ellipse likewise, the second starting where the first ends
            // (to the micro-degree or two a cut rounds to).
            if (last.kind == PathPiece::Kind::Ellipse && p.kind == PathPiece::Kind::Ellipse &&
                last.centre == p.centre && last.major == p.major && last.minor == p.minor &&
                last.to == p.from && (last.sweep_udeg < 0) == (p.sweep_udeg < 0) &&
                turn_of(last) + turn_of(p) <= kTurn) {
                std::int64_t gap = wrap(last.start_udeg + last.sweep_udeg) - wrap(p.start_udeg);
                if (gap > kTurn / 2) gap -= kTurn;
                if (gap < -kTurn / 2) gap += kTurn;
                if (gap >= -2 && gap <= 2) {
                    last.to = p.to;
                    last.sweep_udeg += p.sweep_udeg;
                    continue;
                }
            }
            if (last.kind == PathPiece::Kind::Spline && p.kind == PathPiece::Kind::Spline)
                if (auto joined = joined_splines(last, p)) {
                    last = std::move(*joined);
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

PathPiece arc_piece(Point2 centre, Mm radius, Point2 from, Point2 to, bool ccw) noexcept
{
    PathPiece p;
    p.kind   = PathPiece::Kind::Arc;
    p.centre = centre;
    p.radius = radius;
    p.from   = from;
    p.to     = to;
    // `arc_sweep_udeg` is counter-clockwise from its second argument to its
    // third; walked clockwise the same arc is that sweep from the other end.
    p.sweep_udeg = ccw ? arc_sweep_udeg(centre, from, to) : -arc_sweep_udeg(centre, to, from);
    return p;
}

std::optional<CurvePath> path_of(const Document& doc, EntityId e, PathScope scope)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return std::nullopt;
    const std::uint32_t slot = ents.slot[e];
    // A caption is a polyline slot with words on it, and it is not a curve.
    if (ents.kind[e] == kPolylineKind && doc.texts().has(slot)) return std::nullopt;
    return path_of_slot(ents.kind[e], doc.geometry(), slot, scope);
}

Mm2 path_area(const CurvePath& path)
{
    if (path.pieces.empty()) return 0;

    // THE POLYGON OF THE ENDS, EXACTLY: in 128 bits about the first end, then
    // halved half away from zero — the store's own rule (`signed_ring_area`),
    // where this used to multiply absolute coordinates and halve towards zero.
    const Point2 o = path.pieces.front().from;
    Int128 twice   = 0;
    for (const PathPiece& p : path.pieces)
        twice += (static_cast<Int128>(p.from.x - o.x) * (p.to.y - o.y)) -
                 (static_cast<Int128>(p.to.x - o.x) * (p.from.y - o.y));
    const Int128 half = twice >= 0 ? (twice + 1) / 2 : -((-twice + 1) / 2);
    Mm2 area          = saturate_int64(half);

    // AND WHAT EACH CURVED PIECE ADDS TO ITS CHORD. An arc's circular segment
    // is exact (`circular_segment_area`), added by the sign of its sweep: a
    // counter-clockwise arc on a counter-clockwise ring bulges outward. An
    // ellipse's or a spline's is the curve's own share of ½∮(x dy − y dx) less
    // its chord's, by the fixed Gauss rule — from the curve, never from the
    // picture's chords (TODOS F-03).
    for (const PathPiece& p : path.pieces) {
        if (p.kind == PathPiece::Kind::Arc) {
            area += p.sweep_udeg >= 0 ? circular_segment_area(p.radius, p.sweep_udeg)
                                      : -circular_segment_area(p.radius, -p.sweep_udeg);
        } else if (p.kind != PathPiece::Kind::Segment) {
            const double ax    = mm_to_metres(p.from.x - o.x);
            const double ay    = mm_to_metres(p.from.y - o.y);
            const double bx    = mm_to_metres(p.to.x - o.x);
            const double by    = mm_to_metres(p.to.y - o.y);
            const double chord = (ax * by) - (bx * ay);
            const double bulge = (curve::twice_area(p, o) - chord) / 2.0 *
                                 static_cast<double>(kMmPerMetre * kMmPerMetre);
            if (std::isfinite(bulge)) area += mm_round(bulge);
        }
    }
    return area;
}

std::optional<CurvePath> path_of_slot(KindId kind, const RingGeometry& geom, std::uint32_t slot,
                                      PathScope scope)
{
    CurvePath path;
    switch (kind) {
    case kPolylineKind: {
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
    case kArcPolylineKind: {
        // THE VERTICES ARE THE RING, the bends are the payload: each edge is a
        // segment unless an arc names it, and then it is that arc — walked the
        // way the edge runs, which is clockwise when the arc says so.
        const RingSpan span = geom.rings_of(slot);
        if (span.count != 1) return std::nullopt;
        auto def = arc_polyline_of(geom, slot);
        if (!def) return std::nullopt;
        const auto xs = geom.ring_xs(span.first);
        const auto ys = geom.ring_ys(span.first);
        if (xs.size() < 2) return std::nullopt;
        path.closed          = geom.ring_role[span.first] != RingRole::Open;
        const std::size_t n  = xs.size();
        const std::size_t ns = path.closed ? n : n - 1;
        std::size_t next_arc = 0;
        const auto& arcs     = def.value().arcs;
        for (std::size_t i = 0; i < ns; ++i) {
            const Point2 a{xs[i], ys[i]};
            const Point2 b{xs[(i + 1) % n], ys[(i + 1) % n]};
            while (next_arc < arcs.size() && arcs[next_arc].segment < i)
                ++next_arc;
            if (next_arc < arcs.size() && arcs[next_arc].segment == i) {
                const ArcPolyline::Arc& arc = arcs[next_arc];
                const PathPiece p           = arc_piece(arc.centre, arc.radius, a, b, arc.ccw);
                if (p.sweep_udeg != 0) path.pieces.push_back(p);
                continue;
            }
            PathPiece p;
            p.from = a;
            p.to   = b;
            if (p.from != p.to) path.pieces.push_back(p);
        }
        break;
    }
    case kEllipseKind: {
        // THE ELLIPSE'S OWN PARAMETER, as the kind stores it: a partial one
        // sweeps counter-clockwise from its start to its end, a whole one a
        // full turn from its first axis.
        if (scope != PathScope::Curves) return std::nullopt;
        PathPiece p;
        p.kind   = PathPiece::Kind::Ellipse;
        p.centre = ellipse_centre_of(geom, slot);
        p.major  = ellipse_major_of(geom, slot);
        p.minor  = ellipse_minor_of(geom, slot);
        if (p.major == p.centre || p.minor == p.centre) return std::nullopt;
        if (const auto arc = ellipse_arc_of(geom, slot); arc.has_value()) {
            std::int64_t sweep = arc->end_udeg - arc->start_udeg;
            if (sweep <= 0) sweep += kTurn;
            p.start_udeg = wrap(arc->start_udeg);
            p.sweep_udeg = sweep;
        } else {
            p.start_udeg = 0;
            p.sweep_udeg = kTurn;
            path.closed  = true;
        }
        p.from = ellipse_at(p, p.start_udeg);
        p.to   = path.closed ? p.from : ellipse_at(p, p.start_udeg + p.sweep_udeg);
        path.pieces.push_back(p);
        break;
    }
    case kSplineKind: {
        // THE CURVE, whole: its control points and its knots written out. A
        // closed spline is a closed path only when the curve itself returns to
        // its start; one the drawing closes with a chord is not a path.
        if (scope != PathScope::Curves) return std::nullopt;
        auto def = spline_of(geom, slot);
        if (!def) return std::nullopt;
        const RingSpan span = geom.rings_of(slot);
        if (span.count == 0) return std::nullopt;
        const auto xs = geom.ring_xs(span.first);
        const auto ys = geom.ring_ys(span.first);
        PathPiece p;
        p.kind   = PathPiece::Kind::Spline;
        p.spline = def.value();
        for (std::size_t i = 0; i < xs.size(); ++i)
            p.controls.push_back(Point2{xs[i], ys[i]});
        const auto degree = static_cast<std::size_t>(p.spline.degree);
        if (p.spline.knots_nano.empty())
            p.spline.knots_nano = uniform_clamped_knots(p.controls.size(), p.spline.degree);
        if (p.spline.degree < 1 || p.controls.size() < degree + 1 ||
            p.spline.knots_nano.size() != p.controls.size() + degree + 1 ||
            p.spline.knots_nano[p.controls.size()] <= p.spline.knots_nano[degree])
            return std::nullopt;
        const curve::Eval eval(p, p.controls.front());
        p.from = eval.world(0.0);
        p.to   = eval.world(1.0);
        if (p.spline.closed) {
            if (p.from != p.to) return std::nullopt;
            path.closed = true;
        }
        path.pieces.push_back(p);
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
        const double t     = std::clamp(fraction_of(p, probe), 0.0, 1.0);
        const double d     = distance_squared(piece_point(p, t), probe);
        if (nearest < 0.0 || d < nearest) {
            nearest = d;
            best    = PathPlace{i, t};
        }
    }
    return best;
}

std::int64_t direction_at(const CurvePath& path, PathPlace at)
{
    if (path.pieces.empty()) return 0;
    const PathPiece& p = path.pieces[std::min(at.piece, path.pieces.size() - 1)];
    if (p.kind == PathPiece::Kind::Segment) return atan2_udeg(p.to.y - p.from.y, p.to.x - p.from.x);
    if (curved(p)) {
        // The exact derivative, the way the piece is walked, as a unit vector
        // scaled into integers `atan2_udeg` reads without libm.
        const curve::Vec v = curve::Eval(p, p.from).tangent(std::clamp(at.t, 0.0, 1.0));
        const double n     = std::sqrt((v.x * v.x) + (v.y * v.y));
        if (n <= 0.0) return 0;
        return atan2_udeg(std::llround(v.y / n * 1e12), std::llround(v.x / n * 1e12));
    }
    // An arc's tangent is its radius turned a quarter, forward the way the
    // arc is walked.
    const Point2 q          = point_at(path, at);
    const std::int64_t out  = atan2_udeg(q.y - p.centre.y, q.x - p.centre.x);
    const std::int64_t turn = kUDegFullCircle / 4;
    std::int64_t d          = p.sweep_udeg >= 0 ? out + turn : out - turn;
    d %= kUDegFullCircle;
    return d < 0 ? d + kUDegFullCircle : d;
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
    for (const PathPiece& p : path.pieces)
        metres += piece_metres(p);
    return mm_round(metres * static_cast<double>(kMmPerMetre));
}

Box2 path_bounds(const CurvePath& path)
{
    Box2 box;
    for (const PathPiece& p : path.pieces) {
        if (p.kind == PathPiece::Kind::Segment) {
            box.extend(p.from);
            box.extend(p.to);
            continue;
        }
        const Box2 piece = curve::bounds(p);
        box.extend(Point2{piece.min_x, piece.min_y});
        box.extend(Point2{piece.max_x, piece.max_y});
    }
    return box;
}

PathMeets path_meets(const CurvePath& path, const CurvePath& other)
{
    PathMeets result;
    std::vector<PathCrossing> out;
    std::vector<Meet> meets;
    for (std::size_t i = 0; i < path.pieces.size(); ++i) {
        const PathPiece& p = path.pieces[i];
        for (const PathPiece& q : other.pieces) {
            // A SHARED STRETCH is a stretch, not a point: two segments on one
            // line, two arcs of one circle, one ellipse twice, one spline twice.
            if (p.kind == PathPiece::Kind::Segment && q.kind == PathPiece::Kind::Segment) {
                const auto side = [&p](Point2 r) {
                    return (static_cast<Int128>(p.to.x - p.from.x) * (r.y - p.from.y)) -
                           (static_cast<Int128>(p.to.y - p.from.y) * (r.x - p.from.x));
                };
                if (p.from != p.to && side(q.from) == 0 && side(q.to) == 0) {
                    const double a = std::clamp(segment_fraction(p, q.from), 0.0, 1.0);
                    const double b = std::clamp(segment_fraction(p, q.to), 0.0, 1.0);
                    if (std::abs(b - a) > kSame)
                        result.overlaps.push_back(PathOverlap{PathPlace{i, std::min(a, b)},
                                                              PathPlace{i, std::max(a, b)}});
                    continue;
                }
            }
            if (curved(p) || curved(q) ||
                (p.kind == PathPiece::Kind::Arc && q.kind == PathPiece::Kind::Arc &&
                 p.centre == q.centre && p.radius == q.radius)) {
                const PathPiece a    = p.kind == PathPiece::Kind::Arc ? as_ellipse(p) : p;
                const PathPiece b    = q.kind == PathPiece::Kind::Arc ? as_ellipse(q) : q;
                const curve::Meets m = curve::meets(a, b);
                for (const curve::Hit& h : m.hits)
                    out.push_back(
                        PathCrossing{PathPlace{i, std::clamp(h.s, 0.0, 1.0)}, h.point, h.touching});
                for (const curve::Span& o : m.overlaps)
                    result.overlaps.push_back(PathOverlap{PathPlace{i, o.s0}, PathPlace{i, o.s1}});
                result.unresolved = result.unresolved || m.unresolved;
                continue;
            }
            meets.clear();
            piece_meets(p, q, meets);
            for (const Meet& m : meets) {
                const double t = std::clamp(fraction_of(p, m.point), 0.0, 1.0);
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
    result.crossings = std::move(unique);
    std::ranges::sort(result.overlaps, [](const PathOverlap& a, const PathOverlap& b) {
        return comes_before(a.from, b.from);
    });
    return result;
}

std::vector<PathCrossing> path_crossings(const CurvePath& path, const CurvePath& other)
{
    return path_meets(path, other).crossings;
}

std::vector<LineMeet> line_meets(Point2 a, Point2 b, const PathPiece& piece)
{
    std::vector<LineMeet> out;
    if (curved(piece)) {
        // THE LINE WHERE IT CAN MEET THE CURVE: clipped to the curve's box and
        // a metre round it, then met as a segment, each meet put back on the
        // infinite line by where it falls along it.
        Box2 box = curve::bounds(piece);
        box      = Box2{box.min_x - kMmPerMetre, box.min_y - kMmPerMetre, box.max_x + kMmPerMetre,
                   box.max_y + kMmPerMetre};
        const auto dx   = static_cast<double>(b.x - a.x);
        const auto dy   = static_cast<double>(b.y - a.y);
        double lo       = -1e300;
        double hi       = 1e300;
        const auto slab = [&lo, &hi](double from, double d, double min, double max) {
            if (d == 0.0) return from >= min && from <= max;
            const double t1 = (min - from) / d;
            const double t2 = (max - from) / d;
            lo              = std::max(lo, std::min(t1, t2));
            hi              = std::min(hi, std::max(t1, t2));
            return true;
        };
        if (!slab(static_cast<double>(a.x), dx, static_cast<double>(box.min_x),
                  static_cast<double>(box.max_x)) ||
            !slab(static_cast<double>(a.y), dy, static_cast<double>(box.min_y),
                  static_cast<double>(box.max_y)) ||
            lo >= hi)
            return out;
        const PathPiece reach{.from = Point2{a.x + mm_round(dx * lo), a.y + mm_round(dy * lo)},
                              .to   = Point2{a.x + mm_round(dx * hi), a.y + mm_round(dy * hi)}};
        if (reach.from == reach.to) return out;
        for (const curve::Hit& h : curve::meets(reach, piece).hits)
            out.push_back(LineMeet{lo + ((hi - lo) * h.s), h.point, h.touching});
        std::ranges::sort(out, [](const LineMeet& x, const LineMeet& y) { return x.t < y.t; });
        return out;
    }
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

std::int64_t ellipse_parameter_of(const PathPiece& e, Point2 q) noexcept
{
    // q − c = cos t · U + sin t · V: the 2×2 system solved for (cos t, sin t),
    // and the angle read back without libm.
    const auto ux  = static_cast<double>(e.major.x - e.centre.x);
    const auto uy  = static_cast<double>(e.major.y - e.centre.y);
    const auto vx  = static_cast<double>(e.minor.x - e.centre.x);
    const auto vy  = static_cast<double>(e.minor.y - e.centre.y);
    const auto dx  = static_cast<double>(q.x - e.centre.x);
    const auto dy  = static_cast<double>(q.y - e.centre.y);
    const double d = (ux * vy) - (vx * uy);
    if (d == 0.0) return 0;
    const double c = ((dx * vy) - (vx * dy)) / d;
    const double s = ((ux * dy) - (dx * uy)) / d;
    const double n = std::sqrt((c * c) + (s * s));
    if (n <= 0.0) return 0;
    return wrap(atan2_udeg(std::llround(s / n * 1e12), std::llround(c / n * 1e12)));
}

std::vector<Point2> ellipse_meets(const PathPiece& arc, const PathPiece& piece)
{
    PathPiece whole  = arc;
    whole.sweep_udeg = kTurn;
    whole.from       = ellipse_at(arc, arc.start_udeg);
    whole.to         = whole.from;
    std::vector<Point2> out;
    for (const curve::Hit& h : curve::meets(whole, piece).hits)
        out.push_back(h.point);
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
        switch (p.kind) {
        case PathPiece::Kind::Arc:
            q.sweep_udeg = static_cast<std::int64_t>(
                std::llround(static_cast<double>(p.sweep_udeg) * (t1 - t0)));
            if (q.sweep_udeg == 0) return;
            break;
        case PathPiece::Kind::Ellipse:
            q.start_udeg = wrap(p.start_udeg + static_cast<std::int64_t>(std::llround(
                                                   static_cast<double>(p.sweep_udeg) * t0)));
            q.sweep_udeg = static_cast<std::int64_t>(
                std::llround(static_cast<double>(p.sweep_udeg) * (t1 - t0)));
            if (q.sweep_udeg == 0) return;
            break;
        case PathPiece::Kind::Spline: {
            // THE SHORTER CURVE IT IS, re-made by knot insertion, its ends the
            // cut points exactly.
            curve::SplineParts parts{p.controls, p.spline};
            const std::size_t n   = p.controls.size();
            const auto deg        = static_cast<std::size_t>(p.spline.degree);
            const std::int64_t ua = p.spline.knots_nano[deg];
            const std::int64_t ub = p.spline.knots_nano[n];
            const auto u_at       = [ua, ub](double t) {
                return ua +
                       static_cast<std::int64_t>(std::llround(static_cast<double>(ub - ua) * t));
            };
            if (t1 < 1.0) {
                const std::int64_t u = u_at(t1);
                if (u > ua && u < ub) parts = curve::split_spline(parts, u).first;
            }
            if (t0 > 0.0) {
                const std::int64_t u = u_at(t0);
                if (u > ua && u < u_at(t1)) parts = curve::split_spline(parts, u).second;
            }
            if (parts.controls.size() < deg + 1) return;
            if (t0 > 0.0) parts.controls.front() = q.from;
            if (t1 < 1.0) parts.controls.back() = q.to;
            q.controls = std::move(parts.controls);
            q.spline   = std::move(parts.def);
            if (q.from == q.to) return;
            break;
        }
        case PathPiece::Kind::Segment:
            if (q.from == q.to) return;
            break;
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
    merge_pieces(out.pieces);
    return out;
}

CurvePath reversed(const CurvePath& path)
{
    CurvePath out;
    out.closed = path.closed;
    out.pieces.reserve(path.pieces.size());
    for (PathPiece q : std::views::reverse(path.pieces)) {
        std::swap(q.from, q.to);
        if (q.kind == PathPiece::Kind::Ellipse) q.start_udeg = wrap(q.start_udeg + q.sweep_udeg);
        if (q.kind == PathPiece::Kind::Spline) {
            curve::SplineParts turned = curve::reversed_spline({q.controls, q.spline});
            q.controls                = std::move(turned.controls);
            q.spline                  = std::move(turned.def);
        } else {
            q.sweep_udeg = -q.sweep_udeg;
        }
        out.pieces.push_back(q);
    }
    return out;
}

PathPlace place_at_length(const CurvePath& path, Mm length)
{
    if (path.pieces.empty() || length <= 0) return path_start(path);
    double left = mm_to_metres(length);
    for (std::size_t i = 0; i < path.pieces.size(); ++i) {
        const PathPiece& p  = path.pieces[i];
        const double metres = piece_metres(p);
        if (metres <= 0.0) continue;
        if (left <= metres)
            return PathPlace{i, curved(p) ? curve::t_at_length(p, left) : left / metres};
        left -= metres;
    }
    return path_end(path);
}

std::vector<CurvePath> split_path(const CurvePath& path, std::vector<PathPlace> cuts)
{
    std::vector<CurvePath> out;
    if (path.pieces.empty()) return out;

    // IN ORDER, ONCE, AND NOT AT AN END OF AN OPEN PATH: a cut at its start or
    // its end cuts off nothing.
    std::ranges::sort(cuts, [](PathPlace a, PathPlace b) { return comes_before(a, b); });
    std::vector<PathPlace> at;
    for (const PathPlace& c : cuts) {
        if (!at.empty() && !comes_before(at.back(), c)) continue;
        if (!path.closed &&
            (!comes_before(path_start(path), c) || !comes_before(c, path_end(path))))
            continue;
        at.push_back(c);
    }

    const auto keep = [&out](CurvePath piece) {
        if (piece.pieces.empty() || path_length(piece) <= 0) return;
        piece.closed = false;
        out.push_back(std::move(piece));
    };
    if (!path.closed) {
        PathPlace from = path_start(path);
        for (const PathPlace& c : at) {
            keep(sub_path(path, from, c));
            from = c;
        }
        keep(sub_path(path, from, path_end(path)));
        return out;
    }
    if (at.empty()) {
        out.push_back(path);
        return out;
    }
    if (at.size() == 1) {
        // One cut opens a closed path and cuts nothing off: the whole run,
        // from the cut round to the cut.
        CurvePath opened     = sub_path(path, at.front(), path_end(path));
        const CurvePath rest = sub_path(path, path_start(path), at.front());
        opened.pieces.insert(opened.pieces.end(), rest.pieces.begin(), rest.pieces.end());
        keep(std::move(opened));
        return out;
    }
    for (std::size_t i = 0; i < at.size(); ++i)
        keep(sub_path(path, at[i], at[(i + 1) % at.size()]));
    return out;
}

PathJoin join_paths(std::span<const CurvePath> paths, Mm tolerance)
{
    PathJoin out;
    if (paths.empty() || paths.front().closed || paths.front().pieces.empty()) return out;
    const double reach = static_cast<double>(tolerance) * static_cast<double>(tolerance);
    std::vector<bool> used(paths.size(), false);
    used[0] = true;
    out.joined.push_back(0);
    std::vector<PathPiece> chain = paths.front().pieces;

    // A gap bridged by a straight piece, counted; none when the ends coincide.
    const auto bridge = [&out](Point2 from, Point2 to, std::vector<PathPiece>& into) {
        if (from == to) return;
        into.push_back(PathPiece{.from = from, .to = to});
        ++out.bridged;
        out.widest = std::max(out.widest, segment_length(from, to));
    };

    bool grew = true;
    while (grew) {
        grew = false;
        for (std::size_t j = 1; j < paths.size(); ++j) {
            if (used[j] || paths[j].closed || paths[j].pieces.empty()) continue;
            const Point2 head   = chain.front().from;
            const Point2 tail   = chain.back().to;
            const Point2 first  = paths[j].pieces.front().from;
            const Point2 last   = paths[j].pieces.back().to;
            const auto touching = [reach](Point2 a, Point2 b) {
                return distance_squared(a, b) <= reach;
            };
            std::vector<PathPiece> added;
            if (touching(tail, first)) {
                bridge(tail, first, chain);
                chain.insert(chain.end(), paths[j].pieces.begin(), paths[j].pieces.end());
            } else if (touching(tail, last)) {
                bridge(tail, last, chain);
                const CurvePath turned = reversed(paths[j]);
                chain.insert(chain.end(), turned.pieces.begin(), turned.pieces.end());
            } else if (touching(head, last)) {
                added = paths[j].pieces;
                bridge(last, head, added);
                chain.insert(chain.begin(), added.begin(), added.end());
            } else if (touching(head, first)) {
                added = reversed(paths[j]).pieces;
                bridge(first, head, added);
                chain.insert(chain.begin(), added.begin(), added.end());
            } else {
                continue;
            }
            used[j] = true;
            out.joined.push_back(j);
            grew = true;
        }
    }
    merge_pieces(chain);
    out.chain.pieces = std::move(chain);
    out.ends_meet    = out.joined.size() >= 2 && distance_squared(out.chain.pieces.front().from,
                                                                  out.chain.pieces.back().to) <= reach;
    return out;
}

PathRecord path_record(const CurvePath& input)
{
    PathRecord out;
    CurvePath path = input;
    if (std::ranges::any_of(path.pieces, curved)) {
        // ONE CURVE, ONCE: an ellipse or a spline cut into pieces at a seam
        // comes back as the pieces of one curve, which are that curve. Only
        // here: arcs keep the vertices they were given, which an arc-polyline
        // needs three of to close.
        merge_pieces(path.pieces);
        if (path.pieces.size() == 1 && path.pieces.front().kind == PathPiece::Kind::Ellipse) {
            // AN ELLIPSE, whole or partial; a partial one stored counter-
            // clockwise, the way the kind defines its sweep.
            const PathPiece& e      = path.pieces.front();
            out.kind                = kEllipseKind;
            out.ring                = {e.centre, e.major, e.minor};
            out.role                = RingRole::Open;
            const std::int64_t span = turn_of(e);
            if (span < kTurn) {
                EllipseArc arc;
                arc.start_udeg =
                    wrap(e.sweep_udeg >= 0 ? e.start_udeg : e.start_udeg + e.sweep_udeg);
                arc.end_udeg = wrap(arc.start_udeg + span);
                out.payload  = encode_ellipse_arc(arc);
            }
            return out;
        }
        if (path.pieces.size() == 1 && path.pieces.front().kind == PathPiece::Kind::Spline) {
            const PathPiece& c = path.pieces.front();
            out.kind           = kSplineKind;
            out.ring           = c.controls;
            out.role           = RingRole::Open;
            SplineDef def      = c.spline;
            def.closed         = path.closed;
            def.has_fit        = false;
            out.payload        = encode_spline(def);
            return out;
        }
        // CURVES OF DIFFERENT KINDS in one run have no kind of their own; they
        // are kept as the line they are drawn with. No command of this program
        // makes such a run — UÇUCA does not join an ellipse or a spline — so
        // this is the honest answer for one that arrives, not a road anybody
        // walks.
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        path_outline(path, xs, ys);
        out.kind = kPolylineKind;
        for (std::size_t i = 0; i < xs.size(); ++i)
            out.ring.push_back(Point2{xs[i], ys[i]});
        if (path.closed && out.ring.size() >= 2 && out.ring.front() == out.ring.back())
            out.ring.pop_back();
        out.role = path.closed ? RingRole::Exterior : RingRole::Open;
        return out;
    }
    const bool straight = std::ranges::all_of(
        path.pieces, [](const PathPiece& p) { return p.kind == PathPiece::Kind::Segment; });
    if (straight) {
        out.kind = kPolylineKind;
        out.ring = path_vertices(path);
        // A closed polyline stores its closing vertex once, as its first.
        if (path.closed && out.ring.size() >= 2 && out.ring.front() == out.ring.back())
            out.ring.pop_back();
        out.role = path.closed ? RingRole::Exterior : RingRole::Open;
        return out;
    }
    if (path.pieces.size() == 1) {
        const PathPiece& arc = path.pieces.front();
        const Point2 handle{arc.centre.x + arc.radius, arc.centre.y};
        if (turn_of(arc) >= kTurn) {
            out.kind = kCircleKind;
            out.ring = {arc.centre, handle};
            return out;
        }
        // AN ARC IS STORED COUNTER-CLOCKWISE: walked clockwise, the same arc
        // is kept from its end to its start.
        out.kind = kArcKind;
        out.ring = arc.sweep_udeg >= 0 ? std::vector<Point2>{arc.centre, handle, arc.from, arc.to}
                                       : std::vector<Point2>{arc.centre, handle, arc.to, arc.from};
        return out;
    }

    // SEGMENTS AND ARCS TOGETHER: an arc-polyline, each bent edge its arc.
    out.kind = kArcPolylineKind;
    ArcPolyline def;
    for (std::size_t i = 0; i < path.pieces.size(); ++i) {
        const PathPiece& p = path.pieces[i];
        if (out.ring.empty()) out.ring.push_back(p.from);
        const bool last_closing = path.closed && i + 1 == path.pieces.size();
        if (!last_closing) out.ring.push_back(p.to);
        if (p.kind == PathPiece::Kind::Arc)
            def.arcs.push_back(ArcPolyline::Arc{static_cast<std::uint32_t>(i), p.centre, p.radius,
                                                p.sweep_udeg >= 0});
    }
    out.role    = path.closed ? RingRole::Exterior : RingRole::Open;
    out.payload = encode_arc_polyline(def);
    return out;
}

namespace {

/// The one piece two neighbours make when the vertex between them goes: an
/// arc when both are arcs of one circle turning the same way, else straight.
PathPiece merged(const PathPiece& a, const PathPiece& b)
{
    if (a.kind == PathPiece::Kind::Arc && b.kind == PathPiece::Kind::Arc && a.centre == b.centre &&
        a.radius == b.radius && (a.sweep_udeg > 0) == (b.sweep_udeg > 0))
        return arc_piece(a.centre, a.radius, a.from, b.to, a.sweep_udeg > 0);
    return PathPiece{.from = a.from, .to = b.to};
}

} // namespace

Result<CurvePath> path_without_vertex(const CurvePath& path, std::size_t index)
{
    const std::size_t n       = path.pieces.size();
    const std::size_t corners = path.closed ? n : n + 1;
    if (index >= corners)
        return err(ErrorCode::InvalidArgument, "Bu nesnenin " + std::to_string(index + 1) +
                                                   ". köşesi yok; " + std::to_string(corners) +
                                                   " köşesi var.");
    if (path.closed ? corners <= 3 : corners <= 2)
        return err(ErrorCode::ValidationFailed,
                   path.closed ? "Kapalı bir şekil en az üç köşeyle kalır; bu köşe silinemez."
                               : "Bir çizgi en az iki köşeyle kalır; bu köşe silinemez.");
    CurvePath out;
    out.closed = path.closed;
    if (!path.closed) {
        if (index == 0) {
            out.pieces.assign(path.pieces.begin() + 1, path.pieces.end());
        } else if (index == n) {
            out.pieces.assign(path.pieces.begin(), path.pieces.end() - 1);
        } else {
            out.pieces.assign(path.pieces.begin(),
                              path.pieces.begin() + static_cast<std::ptrdiff_t>(index) - 1);
            out.pieces.push_back(merged(path.pieces[index - 1], path.pieces[index]));
            out.pieces.insert(out.pieces.end(),
                              path.pieces.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                              path.pieces.end());
        }
        return out;
    }
    // A CLOSED PATH round its seam: the vertex before the first piece is the
    // last piece's end, and the merged piece closes the ring from there.
    if (index == 0) {
        out.pieces.assign(path.pieces.begin() + 1, path.pieces.end() - 1);
        out.pieces.push_back(merged(path.pieces[n - 1], path.pieces[0]));
        return out;
    }
    out.pieces.assign(path.pieces.begin(),
                      path.pieces.begin() + static_cast<std::ptrdiff_t>(index) - 1);
    out.pieces.push_back(merged(path.pieces[index - 1], path.pieces[index]));
    out.pieces.insert(out.pieces.end(),
                      path.pieces.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                      path.pieces.end());
    return out;
}

Result<CurvePath> path_with_arc_edge(const CurvePath& path, std::size_t edge, Point2 through)
{
    if (edge >= path.pieces.size())
        return err(ErrorCode::InvalidArgument,
                   "Bu nesnenin " + std::to_string(edge + 1) + ". kenarı yok; " +
                       std::to_string(path.pieces.size()) + " kenarı var.");
    const Point2 a = path.pieces[edge].from;
    const Point2 b = path.pieces[edge].to;
    Point2 centre{};
    Mm radius = 0;
    if (a == b || !circumcircle(a, through, b, centre, radius) || radius <= 0)
        return err(ErrorCode::ValidationFailed,
                   "Yayın geçeceği nokta kenarın doğrultusunda; kenar düz kalır. Noktayı kenarın "
                   "bir yanında gösterin.");
    // The arc runs from `a` through `through` to `b`: counter-clockwise when
    // the three turn left.
    const double turn = static_cast<double>(through.x - a.x) * static_cast<double>(b.y - a.y) -
                        static_cast<double>(through.y - a.y) * static_cast<double>(b.x - a.x);
    CurvePath out    = path;
    out.pieces[edge] = arc_piece(centre, radius, a, b, turn > 0.0);
    return out;
}

Result<CurvePath> path_with_straight_edge(const CurvePath& path, std::size_t edge)
{
    if (edge >= path.pieces.size())
        return err(ErrorCode::InvalidArgument,
                   "Bu nesnenin " + std::to_string(edge + 1) + ". kenarı yok; " +
                       std::to_string(path.pieces.size()) + " kenarı var.");
    if (path.pieces[edge].kind == PathPiece::Kind::Segment)
        return err(ErrorCode::InvalidArgument, "Bu kenar zaten düz.");
    if (path.pieces[edge].from == path.pieces[edge].to)
        return err(ErrorCode::ValidationFailed,
                   "Tam bir çember düzleştirilemez: iki ucu aynı noktada.");
    CurvePath out    = path;
    out.pieces[edge] = PathPiece{.from = path.pieces[edge].from, .to = path.pieces[edge].to};
    return out;
}

std::vector<std::uint8_t> encode_edge_guide(const EdgeGuide& guide)
{
    // version, edge, key — little-endian as the machine writes it, because the
    // bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(1 + sizeof(guide.edge) + sizeof(guide.key));
    bytes[0] = 1;
    std::memcpy(bytes.data() + 1, &guide.edge, sizeof(guide.edge));
    std::memcpy(bytes.data() + 1 + sizeof(guide.edge), &guide.key, sizeof(guide.key));
    return bytes;
}

Result<EdgeGuide> decode_edge_guide(std::span<const std::uint8_t> bytes)
{
    EdgeGuide guide;
    if (bytes.size() != 1 + sizeof(guide.edge) + sizeof(guide.key) || bytes[0] != 1)
        return err(ErrorCode::InvalidArgument, "Kenar önizlemesinin baytları tanınmıyor.");
    std::memcpy(&guide.edge, bytes.data() + 1, sizeof(guide.edge));
    std::memcpy(&guide.key, bytes.data() + 1 + sizeof(guide.edge), sizeof(guide.key));
    return guide;
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
        if (curved(p)) {
            // THE KIND'S OWN DRAWING: `ellipse_arc_outline` counter-clockwise
            // (read backwards when walked clockwise; a whole turn in two
            // halves), `spline_points` for a spline.
            std::vector<Mm> ax;
            std::vector<Mm> ay;
            if (p.kind == PathPiece::Kind::Spline) {
                spline_points(p.controls, p.spline, kSplineSamplesPerSpan, ax, ay);
            } else {
                const std::int64_t span = turn_of(p);
                const std::int64_t low =
                    wrap(p.sweep_udeg >= 0 ? p.start_udeg : p.start_udeg + p.sweep_udeg);
                if (span >= kTurn) {
                    ellipse_arc_outline(p.centre, p.major, p.minor, low, wrap(low + (kTurn / 2)),
                                        ax, ay);
                    std::vector<Mm> bx;
                    std::vector<Mm> by;
                    ellipse_arc_outline(p.centre, p.major, p.minor, wrap(low + (kTurn / 2)), low,
                                        bx, by);
                    ax.insert(ax.end(), bx.begin() + (bx.empty() ? 0 : 1), bx.end());
                    ay.insert(ay.end(), by.begin() + (by.empty() ? 0 : 1), by.end());
                } else {
                    ellipse_arc_outline(p.centre, p.major, p.minor, low, wrap(low + span), ax, ay);
                }
                if (p.sweep_udeg < 0) {
                    std::ranges::reverse(ax);
                    std::ranges::reverse(ay);
                }
            }
            for (std::size_t i = 0; i < ax.size(); ++i) {
                if (i == 0 && !xs.empty() && xs.back() == ax[0] && ys.back() == ay[0]) continue;
                xs.push_back(ax[i]);
                ys.push_back(ay[i]);
            }
            continue;
        }
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
        const bool ccw = p.sweep_udeg >= 0;
        if (turn_of(p) >= kTurn) {
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
            if (!ccw) { // the same circle from the same start, the other way round
                std::ranges::reverse(ax);
                std::ranges::reverse(ay);
            }
        } else if (ccw) {
            arc_outline(p.centre, p.radius, p.from, p.to, ax, ay);
        } else {
            // WALKED CLOCKWISE: the arc drawn counter-clockwise from its end to
            // its start, which is the same points, and then read backwards.
            arc_outline(p.centre, p.radius, p.to, p.from, ax, ay);
            std::ranges::reverse(ax);
            std::ranges::reverse(ay);
        }
        for (std::size_t i = 0; i < ax.size(); ++i) {
            if (i == 0 && !xs.empty() && xs.back() == ax[0] && ys.back() == ay[0]) continue;
            xs.push_back(ax[i]);
            ys.push_back(ay[i]);
        }
    }
}

} // namespace kentos::core
