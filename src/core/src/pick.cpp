// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/pick.hpp"

#include "kentos_cad/core/entity_kind.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/spatial_index.hpp"

#include <algorithm>
#include <cstdlib>

namespace kentos::core {
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
/// The runs to measure against, whichever kind the entity is.
///
/// A curve's stored vertices are its DEFINITION, not its shape: a circle holds a
/// centre and a radius handle, and measuring to those measures to a line pointing
/// east. That is exactly what made a circle unpickable anywhere except along its
/// own radius (core/entity_kind.hpp `curve_outline`).
struct Runs
{
    EmitBuffer curve;
    bool is_curve{false};
    std::uint32_t count{0};

    void build(const Document& doc, EntityId e)
    {
        const std::uint32_t slot = doc.entities().slot[e];
        is_curve = curve_outline(doc.entities().kind[e], doc.geometry(), slot, curve);
        count    = is_curve ? static_cast<std::uint32_t>(curve.run_total())
                            : doc.geometry().rings_of(slot).count;
    }

    /// `i` is a run index, 0-based within the entity.
    std::span<const Mm> xs(const Document& doc, EntityId e, std::uint32_t i) const
    {
        if (is_curve)
            return std::span<const Mm>(curve.xs.data() + curve.run_start[i], curve.run_count[i]);
        return doc.geometry().ring_xs(doc.geometry().rings_of(doc.entities().slot[e]).first + i);
    }

    std::span<const Mm> ys(const Document& doc, EntityId e, std::uint32_t i) const
    {
        if (is_curve)
            return std::span<const Mm>(curve.ys.data() + curve.run_start[i], curve.run_count[i]);
        return doc.geometry().ring_ys(doc.geometry().rings_of(doc.entities().slot[e]).first + i);
    }

    bool closed(const Document& doc, EntityId e, std::uint32_t i) const
    {
        if (is_curve) return curve.run_closed[i] != 0;
        const RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
        return doc.geometry().ring_role[rs.first + i] != RingRole::Open;
    }

    /// Whether this run is a VOID in the entity rather than its outline. A curve
    /// has none: a circle's single run is its own boundary.
    bool hole(const Document& doc, EntityId e, std::uint32_t i) const
    {
        if (is_curve) return false;
        const RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
        return doc.geometry().ring_role[rs.first + i] == RingRole::Interior;
    }
};

double min_distance_squared(const Document& doc, EntityId e, Point2 p)
{
    Runs runs;
    runs.build(doc, e);

    double best = -1.0;
    for (std::uint32_t r = 0; r < runs.count; ++r) {
        const auto xs = runs.xs(doc, e, r);
        const auto ys = runs.ys(doc, e, r);
        if (xs.empty()) continue;

        if (xs.size() == 1) {
            const double d = distance_squared(Point2{xs[0], ys[0]}, p);
            if (best < 0.0 || d < best) best = d;
            continue;
        }

        const bool closed          = runs.closed(doc, e, r);
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

    // A CURSOR INSIDE A FACE IS ON IT, at distance zero.
    //
    // Measuring only to the edges is the CAD answer and the wrong one for a map:
    // a user reaching for a parcel points AT the parcel, not at the hairline
    // around it, and on a sheet of adjoining parcels the interior is nearly all
    // there is to point at. Because the pick radius is a few pixels, a click one
    // metre inside a parcel was simply not a click on anything — which made every
    // tool that starts by asking WHICH objects unusable: the click found nothing
    // and the command waited for a selection that could not be made.
    //
    // A hole vetoes: the court cut out of a building is not the building.
    if (best != 0.0) {
        bool in_exterior = false;
        bool in_hole     = false;
        for (std::uint32_t r = 0; r < runs.count; ++r) {
            if (!runs.closed(doc, e, r)) continue;
            const auto xs = runs.xs(doc, e, r);
            const auto ys = runs.ys(doc, e, r);
            if (!ring_contains(xs, ys, p)) continue;
            if (runs.hole(doc, e, r))
                in_hole = true;
            else
                in_exterior = true;
        }
        if (in_exterior && !in_hole) return 0.0;
    }
    return best;
}

} // namespace

bool ring_contains(std::span<const Mm> xs, std::span<const Mm> ys, Point2 probe) noexcept
{
    const std::size_t n = xs.size();
    if (n < 3 || ys.size() != n) return false;

    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const bool straddles = (ys[i] > probe.y) != (ys[j] > probe.y);
        if (!straddles) continue;

        // Where the edge crosses the probe's row, compared against the probe
        // WITHOUT dividing: (x_j - x_i)(y_p - y_i) against (x_p - x_i)(y_j - y_i),
        // with the sign of (y_j - y_i) deciding which way the comparison runs.
        const auto dx = static_cast<Int128>(xs[j]) - xs[i];
        const auto dy = static_cast<Int128>(ys[j]) - ys[i];
        const auto px = static_cast<Int128>(probe.x) - xs[i];
        const auto py = static_cast<Int128>(probe.y) - ys[i];

        if (dy > 0 ? dx * py > px * dy : dx * py < px * dy) inside = !inside;
    }
    return inside;
}

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

    const EntityTable& entities = doc.entities();
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
        // parcel's box covers ground the parcel does not, and a circle's box has
        // four corners the circle never reaches.
        Runs runs;
        runs.build(doc, e);
        for (std::uint32_t r = 0; r < runs.count; ++r) {
            const auto xs = runs.xs(doc, e, r);
            const auto ys = runs.ys(doc, e, r);
            if (xs.empty()) continue;

            if (xs.size() == 1) {
                if (box_contains_point(box, Point2{xs[0], ys[0]})) {
                    out.push_back(e);
                    return;
                }
                continue;
            }

            const bool closed          = runs.closed(doc, e, r);
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

void pick_all(const Document& doc, Point2 cursor, Mm radius, std::vector<EntityId>& out)
{
    out.clear();
    if (radius < 0) return;

    const Box2 box{cursor.x - radius, cursor.y - radius, cursor.x + radius, cursor.y + radius};
    const double limit = static_cast<double>(radius) * static_cast<double>(radius);

    const EntityTable& entities = doc.entities();
    std::vector<EntityId> scratch;

    // The distance rides ALONGSIDE the id rather than being recomputed in the
    // comparator: `min_distance_squared` walks the entity's rings, and a sort
    // that called it would walk them O(n log n) times for a list a user is about
    // to read four rows of.
    std::vector<std::pair<double, EntityId>> found;

    for_each_candidate(doc, box, scratch, [&](EntityId e) {
        if (!entities.visible(e)) return;
        if (!boxes_overlap(entities.box_of(e), box)) return;

        const double d = min_distance_squared(doc, e, cursor);
        if (d < 0.0 || d > limit) return;
        found.emplace_back(d, e);
    });

    // STABLE, and on the distance alone. Candidates arrive in ascending slot
    // order, so a stable sort leaves two equally close entities in slot order —
    // which is the tie `pick_nearest` breaks the same way, and is what lets
    // `out.front()` be its answer.
    std::stable_sort(found.begin(), found.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    out.reserve(found.size());
    for (const auto& [distance, entity] : found)
        out.push_back(entity);
}

} // namespace kentos::core
