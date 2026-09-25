// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/stroke.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::core {
namespace {

constexpr std::int64_t kQuarterTurn = kUDegFullCircle / 4;
constexpr std::int64_t kEighthTurn  = kUDegFullCircle / 8;

/// `at` folded into [0, 360°).
constexpr std::int64_t wrap_udeg(std::int64_t at) noexcept
{
    const std::int64_t r = at % kUDegFullCircle;
    return r < 0 ? r + kUDegFullCircle : r;
}

/// The largest step, in whole micro-degrees, that keeps a chord of a circle of
/// `radius` within `chord` of it.
///
/// A chord across θ stands r(1 − cos θ/2) off its arc, and that is at most
/// r·θ²/8 — so θ = √(8·chord/r) is always within the tolerance, with no `acos`
/// whose last bit could differ between two libms (§7.3). An eighth of a turn at
/// most, so a curve smaller than its tolerance is still drawn as one.
std::int64_t step_for(double radius, double chord) noexcept
{
    if (radius <= chord) return kEighthTurn;
    const double theta = std::sqrt(8.0 * chord / radius);
    const double udeg  = std::floor(theta * (180.0 * static_cast<double>(kUDegPerDegree)) / kPi);
    return std::clamp<std::int64_t>(mm_round(udeg), 1, kEighthTurn);
}

/// How many steps a sweep takes at most `step` apiece: at least one per quarter
/// turn begun, at most what `kStrokeMaxVertices` allows.
std::int64_t steps_for(std::int64_t sweep, std::int64_t step, std::int64_t at_least) noexcept
{
    const std::int64_t wanted = (sweep + step - 1) / step;
    return std::clamp<std::int64_t>(std::max(wanted, at_least), 1, kStrokeMaxVertices);
}

/// The chord deviation a sweep of `sweep` micro-degrees split into `steps` leaves
/// on a circle of `radius`, by the same bound the step was chosen with.
double deviation_of(double radius, std::int64_t sweep, std::int64_t steps) noexcept
{
    const double theta = static_cast<double>(sweep) / static_cast<double>(steps) * kPi /
                         (180.0 * static_cast<double>(kUDegPerDegree));
    return radius * theta * theta / 8.0;
}

/// The point at micro-degree `at` on a circle of `radius` about `centre`.
Point2 on_circle(Point2 centre, double radius, std::int64_t at) noexcept
{
    const SinCos t = sin_cos_udeg(wrap_udeg(at));
    return Point2{centre.x + mm_round(radius * t.cos), centre.y + mm_round(radius * t.sin)};
}

/// Appends the arc from `start` to `end` about `centre` — counter-clockwise
/// when `ccw` — WITHOUT its first point, which the caller already holds, and
/// with its last exactly as stored. Coincident ends are a full turn. Answers the
/// deviation it left.
double append_arc(std::vector<Point2>& pts, Point2 centre, Mm radius, Point2 start, Point2 end,
                  bool ccw, double chord)
{
    const std::int64_t from = atan2_udeg(start.y - centre.y, start.x - centre.x);
    const std::int64_t to   = atan2_udeg(end.y - centre.y, end.x - centre.x);
    std::int64_t sweep      = ccw ? to - from : from - to;
    while (sweep <= 0)
        sweep += kUDegFullCircle;

    const auto r = static_cast<double>(radius);
    const std::int64_t steps =
        steps_for(sweep, step_for(r, chord), (sweep + kQuarterTurn - 1) / kQuarterTurn);
    const std::int64_t sign = ccw ? 1 : -1;
    for (std::int64_t i = 1; i < steps; ++i)
        pts.push_back(on_circle(centre, r, from + sign * ((sweep * i) / steps)));
    pts.push_back(end);
    return deviation_of(r, sweep, steps);
}

/// The distance from `p` to the segment `a`–`b`, in millimetres.
double to_segment(Point2 p, Point2 a, Point2 b) noexcept
{
    const auto dx     = static_cast<double>(b.x - a.x);
    const auto dy     = static_cast<double>(b.y - a.y);
    const auto px     = static_cast<double>(p.x - a.x);
    const auto py     = static_cast<double>(p.y - a.y);
    const double len2 = dx * dx + dy * dy;
    double t          = len2 > 0.0 ? (px * dx + py * dy) / len2 : 0.0;
    t                 = std::clamp(t, 0.0, 1.0);
    const double ex   = px - t * dx;
    const double ey   = py - t * dy;
    return std::sqrt(ex * ex + ey * ey);
}

bool stroke_circle(const RingGeometry& geom, std::uint32_t slot, double chord, Stroked& out)
{
    const Point2 centre = circle_centre_of(geom, slot);
    const Mm radius     = circle_radius_of(geom, slot);
    if (radius <= 0) return false;

    const auto r             = static_cast<double>(radius);
    const std::int64_t steps = steps_for(kUDegFullCircle, step_for(r, chord), 8);
    StrokedRun run;
    run.role = RingRole::Exterior;
    run.points.reserve(static_cast<std::size_t>(steps));
    // From due east, where the radius handle is stored, counter-clockwise.
    run.points.push_back(Point2{centre.x + radius, centre.y});
    for (std::int64_t i = 1; i < steps; ++i)
        run.points.push_back(on_circle(centre, r, (kUDegFullCircle * i) / steps));
    out.deviation = std::max(out.deviation, deviation_of(r, kUDegFullCircle, steps));
    out.runs.push_back(std::move(run));
    return true;
}

bool stroke_arc(const RingGeometry& geom, std::uint32_t slot, double chord, Stroked& out)
{
    const Mm radius = arc_radius_of(geom, slot);
    if (radius <= 0) return false;
    const Point2 start = arc_start_of(geom, slot);
    StrokedRun run;
    run.points.push_back(start);
    const double left = append_arc(run.points, arc_centre_of(geom, slot), radius, start,
                                   arc_end_of(geom, slot), /*ccw=*/true, chord);
    out.deviation     = std::max(out.deviation, left);
    out.runs.push_back(std::move(run));
    return true;
}

bool stroke_ellipse(const RingGeometry& geom, std::uint32_t slot, double chord, Stroked& out)
{
    const Point2 c = ellipse_centre_of(geom, slot);
    const Point2 m = ellipse_major_of(geom, slot);
    const Point2 n = ellipse_minor_of(geom, slot);
    const auto ax  = static_cast<double>(m.x - c.x);
    const auto ay  = static_cast<double>(m.y - c.y);
    const auto bx  = static_cast<double>(n.x - c.x);
    const auto by  = static_cast<double>(n.y - c.y);
    // The ellipse is the circle of its larger semi-axis squashed along the
    // other, and squashing only shortens how far a chord stands off: the
    // circle's step bound holds for it.
    const double reach = std::max(std::sqrt(ax * ax + ay * ay), std::sqrt(bx * bx + by * by));
    if (reach <= 0.0) return false;

    const auto at = [&](std::int64_t t) {
        const SinCos s = sin_cos_udeg(wrap_udeg(t));
        return Point2{c.x + mm_round(s.cos * ax + s.sin * bx),
                      c.y + mm_round(s.cos * ay + s.sin * by)};
    };

    StrokedRun run;
    const std::optional<EllipseArc> part = ellipse_arc_of(geom, slot);
    std::int64_t from                    = 0;
    std::int64_t sweep                   = kUDegFullCircle;
    if (part) {
        from  = part->start_udeg;
        sweep = part->end_udeg - part->start_udeg;
        while (sweep <= 0)
            sweep += kUDegFullCircle;
    }
    const std::int64_t steps = steps_for(sweep, step_for(reach, chord),
                                         part ? (sweep + kQuarterTurn - 1) / kQuarterTurn : 8);
    if (part) {
        for (std::int64_t i = 0; i <= steps; ++i)
            run.points.push_back(at(from + (sweep * i) / steps));
    } else {
        run.role = RingRole::Exterior;
        for (std::int64_t i = 0; i < steps; ++i)
            run.points.push_back(at((kUDegFullCircle * i) / steps));
    }
    out.deviation = std::max(out.deviation, deviation_of(reach, sweep, steps));
    out.runs.push_back(std::move(run));
    return true;
}

bool stroke_arc_polyline(const RingGeometry& geom, std::uint32_t slot, double chord, Stroked& out)
{
    const RingSpan span = geom.rings_of(slot);
    if (span.count == 0) return false;
    auto def = arc_polyline_of(geom, slot);
    if (!def) return false;

    const auto xs       = geom.ring_xs(span.first);
    const auto ys       = geom.ring_ys(span.first);
    const std::size_t n = xs.size();
    if (n == 0) return false;
    const bool closed = geom.ring_role[span.first] != RingRole::Open;

    StrokedRun run;
    run.role = closed ? RingRole::Exterior : RingRole::Open;
    run.points.push_back(Point2{xs[0], ys[0]});
    const std::size_t edges = closed ? n : n - 1;
    std::size_t next_arc    = 0;
    for (std::size_t i = 0; i < edges; ++i) {
        const Point2 from{xs[i], ys[i]};
        const Point2 to{xs[(i + 1) % n], ys[(i + 1) % n]};
        while (next_arc < def.value().arcs.size() && def.value().arcs[next_arc].segment < i)
            ++next_arc;
        if (next_arc < def.value().arcs.size() && def.value().arcs[next_arc].segment == i) {
            const ArcPolyline::Arc& bend = def.value().arcs[next_arc];
            out.deviation = std::max(out.deviation, append_arc(run.points, bend.centre, bend.radius,
                                                               from, to, bend.ccw, chord));
        } else {
            run.points.push_back(to);
        }
    }
    // A closed run comes back to its first vertex, which it already holds.
    if (closed && run.points.size() > 1) run.points.pop_back();
    out.runs.push_back(std::move(run));
    return true;
}

bool stroke_spline(const RingGeometry& geom, std::uint32_t slot, double chord, Stroked& out)
{
    const RingSpan span = geom.rings_of(slot);
    if (span.count == 0) return false;
    auto def = spline_of(geom, slot);
    if (!def) return false;

    const auto cx = geom.ring_xs(span.first);
    const auto cy = geom.ring_ys(span.first);
    std::vector<Point2> controls;
    controls.reserve(cx.size());
    for (std::size_t i = 0; i < cx.size(); ++i)
        controls.emplace_back(cx[i], cy[i]);

    const auto sample = [&](int per_span) {
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        spline_points(controls, def.value(), per_span, xs, ys);
        std::vector<Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t i = 0; i < xs.size(); ++i)
            pts.emplace_back(xs[i], ys[i]);
        return pts;
    };

    // REFINED UNTIL THE CURVE STOPS MOVING. A spline has no radius to size a
    // step by, so the curve is sampled at a density and again at twice it; the
    // finer run's new points sit between the coarse run's, and how far they
    // stand off the coarse chords is how far the coarse run is off the curve.
    // Doubled until that is within the tolerance. Deterministic: the sampler is.
    int per_span               = kSplineSamplesPerSpan;
    std::vector<Point2> coarse = sample(per_span);
    double left                = 0.0;
    while (coarse.size() >= 2) {
        const std::vector<Point2> fine = sample(per_span * 2);
        if (fine.size() != 2 * coarse.size() - 1)
            break; // not the sampler's pattern: keep what we have
        left = 0.0;
        for (std::size_t k = 0; k + 1 < coarse.size(); ++k)
            left = std::max(left, to_segment(fine[2 * k + 1], coarse[k], coarse[k + 1]));
        if (left <= chord || static_cast<std::int64_t>(fine.size()) > kStrokeMaxVertices / 2) break;
        per_span *= 2;
        coarse = fine;
    }
    if (coarse.empty()) return false;

    StrokedRun run;
    run.role   = def.value().closed ? RingRole::Exterior : RingRole::Open;
    run.points = std::move(coarse);
    if (def.value().closed && run.points.size() > 1 && run.points.back() == run.points.front())
        run.points.pop_back();
    out.deviation = std::max(out.deviation, left);
    out.runs.push_back(std::move(run));
    return true;
}

} // namespace

bool strokes_as_curve(KindId kind) noexcept
{
    return kind == kCircleKind || kind == kArcKind || kind == kEllipseKind ||
           kind == kArcPolylineKind || kind == kSplineKind;
}

bool stroke_curve(KindId kind, const RingGeometry& geom, std::uint32_t slot, Mm chord, Stroked& out)
{
    out          = Stroked{};
    const auto c = static_cast<double>(std::max(chord, kStrokeMinChord));
    if (kind == kCircleKind) return stroke_circle(geom, slot, c, out);
    if (kind == kArcKind) return stroke_arc(geom, slot, c, out);
    if (kind == kEllipseKind) return stroke_ellipse(geom, slot, c, out);
    if (kind == kArcPolylineKind) return stroke_arc_polyline(geom, slot, c, out);
    if (kind == kSplineKind) return stroke_spline(geom, slot, c, out);
    return false;
}

} // namespace kentos::core
