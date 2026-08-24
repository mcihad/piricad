// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/pick.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/spatial_index.hpp"

#include <algorithm>
#include <cstdlib>

namespace piricad::core {
namespace {

bool boxes_overlap(const Box2& a, const Box2& b) noexcept
{
    if (a.empty() || b.empty()) return false;
    return !(a.max_x < b.min_x || a.min_x > b.max_x || a.max_y < b.min_y || a.min_y > b.max_y);
}

bool box_contains_point(const Box2& b, Point2 p) noexcept
{
    return !b.empty() && p.x >= b.min_x && p.x <= b.max_x && p.y >= b.min_y && p.y <= b.max_y;
}

bool box_contains_box(const Box2& outer, const Box2& inner) noexcept
{
    if (outer.empty() || inner.empty()) return false;
    return outer.min_x <= inner.min_x && outer.min_y <= inner.min_y && outer.max_x >= inner.max_x &&
           outer.max_y >= inner.max_y;
}

Mm abs_mm(Mm v) noexcept
{
    return v < 0 ? -v : v;
}

/// Walks the candidates for `box`: the index for everything it has packed, then
/// the short unindexed tail. Exactly the split render::build_scene uses, for
/// exactly the same reason — drawing one line must not repack the layer (§10.5).
template<class Fn>
void for_each_candidate(const Document& doc, const Box2& box, std::vector<EntityId>& scratch,
                        Fn&& visit)
{
    const EntityTable& entities = doc.entities();
    const SpatialIndex& index   = doc.spatial_index();

    if (index.empty() || box_contains_box(box, index.bounds())) {
        for (EntityId e = 0; e < entities.size(); ++e)
            visit(e);
        return;
    }

    scratch.clear();
    index.query(box, scratch);
    // The index hands back leaf-mates in packing order; a pick answer must not
    // depend on how the tree happened to be packed (core.md P11).
    std::sort(scratch.begin(), scratch.end());
    for (EntityId e : scratch)
        visit(e);

    for (EntityId e = doc.indexed_upto(); e < entities.size(); ++e)
        visit(e);
}

/// Smallest squared distance from `p` to any segment of entity `e`, or a negative
/// value when the entity carries no segment.
double min_distance_squared(const Document& doc, EntityId e, Point2 p)
{
    const RingGeometry& geometry = doc.geometry();
    const RingSpan span          = geometry.rings_of(doc.entities().slot[e]);

    double best = -1.0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = geometry.ring_xs(r);
        const auto ys = geometry.ring_ys(r);
        if (xs.empty()) continue;

        if (xs.size() == 1) {
            const double d = distance_squared(Point2{xs[0], ys[0]}, p);
            if (best < 0.0 || d < best) best = d;
            continue;
        }

        const bool closed          = geometry.ring_role[r] != RingRole::Open;
        const std::size_t n        = xs.size();
        const std::size_t segments = closed ? n : n - 1;

        for (std::size_t v = 0; v < segments; ++v) {
            const std::size_t w = (v + 1) % n;
            const Point2 a{xs[v], ys[v]};
            const Point2 b{xs[w], ys[w]};
            const double d = distance_squared(closest_point_on_segment(a, b, p), p);
            if (best < 0.0 || d < best) best = d;
        }
    }
    return best;
}

} // namespace

double distance_squared(Point2 a, Point2 b) noexcept
{
    const double dx = static_cast<double>(a.x - b.x);
    const double dy = static_cast<double>(a.y - b.y);
    return dx * dx + dy * dy;
}

Point2 closest_point_on_segment(Point2 a, Point2 b, Point2 p) noexcept
{
    // Translate to `a` before multiplying. A raw dot product on 1e9-magnitude TM3
    // coordinates loses every digit that matters (geometry.hpp says the same
    // thing about the shoelace formula).
    const double dx   = static_cast<double>(b.x - a.x);
    const double dy   = static_cast<double>(b.y - a.y);
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) return a;

    const double px = static_cast<double>(p.x - a.x);
    const double py = static_cast<double>(p.y - a.y);

    double t = (px * dx + py * dy) / len2;
    if (t <= 0.0) return a;
    if (t >= 1.0) return b;

    return Point2{a.x + mm_round(t * dx), a.y + mm_round(t * dy)};
}

bool segment_touches_box(Point2 a, Point2 b, const Box2& box) noexcept
{
    if (box.empty()) return false;

    // Cheap rejections first: most segments fail here on a real drawing.
    Box2 sb{};
    sb.extend(a);
    sb.extend(b);
    if (!boxes_overlap(sb, box)) return false;

    if (box_contains_point(box, a) || box_contains_point(box, b)) return true;

    // Separating-axis test, doubled so the box centre and the segment midpoint
    // stay integral. Everything is expressed relative to the box's minimum
    // corner, which keeps the magnitudes local rather than seven-digit TM3.
    const Mm ax = a.x - box.min_x, ay = a.y - box.min_y;
    const Mm bx = b.x - box.min_x, by = b.y - box.min_y;
    const Mm hx = box.max_x - box.min_x, hy = box.max_y - box.min_y;

    const Mm limit = kPickExactLimit;
    if (abs_mm(ax) >= limit || abs_mm(ay) >= limit || abs_mm(bx) >= limit || abs_mm(by) >= limit ||
        hx >= limit || hy >= limit) {
        // Beyond the exact range the honest answer is the bounding-box answer:
        // conservative, never a missed hit. See the header.
        return true;
    }

    const Mm mx = ax + bx - hx; // 2 * (segment midpoint - box centre)
    const Mm my = ay + by - hy;
    const Mm dx = bx - ax; // 2 * half direction
    const Mm dy = by - ay;

    if (abs_mm(mx) > hx + abs_mm(dx)) return false;
    if (abs_mm(my) > hy + abs_mm(dy)) return false;

    const Mm cross = mx * dy - my * dx;
    return abs_mm(cross) <= hx * abs_mm(dy) + hy * abs_mm(dx);
}

bool line_intersection(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out, double& t,
                       double& u) noexcept
{
    // Translated to `a` before multiplying, for the reason
    // `closest_point_on_segment` gives: a raw cross product on TM3 coordinates
    // loses every digit that matters.
    const double r_x = static_cast<double>(b.x - a.x);
    const double r_y = static_cast<double>(b.y - a.y);
    const double s_x = static_cast<double>(d.x - c.x);
    const double s_y = static_cast<double>(d.y - c.y);

    const double denom = r_x * s_y - r_y * s_x;
    if (denom == 0.0) return false; // parallel or collinear: no single point

    const double q_x = static_cast<double>(c.x - a.x);
    const double q_y = static_cast<double>(c.y - a.y);

    t = (q_x * s_y - q_y * s_x) / denom;
    u = (q_x * r_y - q_y * r_x) / denom;

    out = Point2{a.x + mm_round(t * r_x), a.y + mm_round(t * r_y)};
    return true;
}

bool segment_intersection(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out) noexcept
{
    // The segment case IS the line case with both parameters inside their span.
    // Written once so the two can never drift apart on a degenerate input.
    double t = 0.0;
    double u = 0.0;
    if (!line_intersection(a, b, c, d, out, t, u)) return false;
    return t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0;
}

bool closest_point_on_line(Point2 a, Point2 b, Point2 p, Point2& out, double& t) noexcept
{
    const double dx   = static_cast<double>(b.x - a.x);
    const double dy   = static_cast<double>(b.y - a.y);
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) return false;

    const double px = static_cast<double>(p.x - a.x);
    const double py = static_cast<double>(p.y - a.y);

    t   = (px * dx + py * dy) / len2;
    out = Point2{a.x + mm_round(t * dx), a.y + mm_round(t * dy)};
    return true;
}

void pick_candidates(const Document& doc, const Box2& box, std::vector<EntityId>& out)
{
    out.clear();
    if (box.empty()) return;

    const EntityTable& entities = doc.entities();
    std::vector<EntityId> scratch;

    for_each_candidate(doc, box, scratch, [&](EntityId e) {
        if (!entities.visible(e)) return;
        if (!boxes_overlap(entities.box_of(e), box)) return;
        out.push_back(e);
    });
}

void pick_in_box(const Document& doc, const Box2& box, PickMode mode, std::vector<EntityId>& out)
{
    if (box.empty()) return;

    const EntityTable& entities  = doc.entities();
    const RingGeometry& geometry = doc.geometry();
    std::vector<EntityId> scratch;

    for_each_candidate(doc, box, scratch, [&](EntityId e) {
        if (!entities.visible(e)) return;

        const Box2 eb = entities.box_of(e);
        if (!boxes_overlap(eb, box)) return;

        if (mode == PickMode::Window) {
            // Every vertex inside is equivalent to the entity's own box being
            // inside, and the box is already in the cull block.
            if (box_contains_box(box, eb)) out.push_back(e);
            return;
        }

        // Crossing: the bounding box overlapping is not enough — an L-shaped
        // parcel's box covers ground the parcel does not.
        const RingSpan span = geometry.rings_of(entities.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geometry.ring_xs(r);
            const auto ys = geometry.ring_ys(r);
            if (xs.empty()) continue;

            if (xs.size() == 1) {
                if (box_contains_point(box, Point2{xs[0], ys[0]})) {
                    out.push_back(e);
                    return;
                }
                continue;
            }

            const bool closed          = geometry.ring_role[r] != RingRole::Open;
            const std::size_t n        = xs.size();
            const std::size_t segments = closed ? n : n - 1;

            for (std::size_t v = 0; v < segments; ++v) {
                const std::size_t w = (v + 1) % n;
                if (segment_touches_box(Point2{xs[v], ys[v]}, Point2{xs[w], ys[w]}, box)) {
                    out.push_back(e);
                    return;
                }
            }
        }
    });
}

EntityId pick_nearest(const Document& doc, Point2 cursor, Mm radius)
{
    if (radius < 0) return kNoEntity;

    const Box2 box{cursor.x - radius, cursor.y - radius, cursor.x + radius, cursor.y + radius};
    const double limit = static_cast<double>(radius) * static_cast<double>(radius);

    const EntityTable& entities = doc.entities();
    std::vector<EntityId> scratch;

    EntityId best = kNoEntity;
    double best_d = 0.0;

    for_each_candidate(doc, box, scratch, [&](EntityId e) {
        if (!entities.visible(e)) return;
        if (!boxes_overlap(entities.box_of(e), box)) return;

        const double d = min_distance_squared(doc, e, cursor);
        if (d < 0.0 || d > limit) return;

        // Strictly less: candidates arrive in ascending slot order, so the first
        // of two equally close entities wins and the answer is stable.
        if (best == kNoEntity || d < best_d) {
            best   = e;
            best_d = d;
        }
    });

    return best;
}

} // namespace piricad::core
