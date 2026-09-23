// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/planar.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/spatial_index.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <queue>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <set>
#include <utility>

#ifdef KENTOS_HAVE_CGAL
#include <CGAL/Arr_circle_segment_traits_2.h>
#include <CGAL/Arr_consolidated_curve_data_traits_2.h>
#include <CGAL/Arr_naive_point_location.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/version.h>
#endif

namespace kentos::core {

bool network_available() noexcept
{
#ifdef KENTOS_HAVE_CGAL
    return true;
#else
    return false;
#endif
}

namespace {

/// One piece as the network holds it, after the node tolerance has had its say.
struct Prepared
{
    PathPiece piece;
    std::uint32_t source{0};
    bool approximate{false};
    std::int32_t bridge{-1}; ///< the bridge this piece is, or -1 for linework
};

// ---- everything below, to `gather`, serves the arrangement alone: a build
// without CGAL has no network to prepare pieces for.
#ifdef KENTOS_HAVE_CGAL

bool full_turn(const PathPiece& p) noexcept
{
    return p.kind == PathPiece::Kind::Arc &&
           (p.sweep_udeg >= kUDegFullCircle || p.sweep_udeg <= -kUDegFullCircle);
}

/// Chebyshev distance: the node test compares in integers, so two machines can
/// never disagree about whether two ends are one node (the rule ALANAÇEVİR
/// already uses).
Mm chebyshev(Point2 a, Point2 b) noexcept
{
    const Mm dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const Mm dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

/// The point halfway along an arc piece, whichever way it is walked.
Point2 arc_middle(const PathPiece& p)
{
    return p.sweep_udeg >= 0 ? arc_midpoint(p.centre, p.radius, p.from, p.to)
                             : arc_midpoint(p.centre, p.radius, p.to, p.from);
}

/// Whether the direction of `q` from an arc's centre lies within its sweep.
bool on_sweep(const PathPiece& p, Point2 q)
{
    if (full_turn(p)) return true;
    const Point2 a           = p.sweep_udeg >= 0 ? p.from : p.to;
    const Point2 b           = p.sweep_udeg >= 0 ? p.to : p.from;
    const std::int64_t span  = arc_sweep_udeg(p.centre, a, b);
    const std::int64_t where = arc_sweep_udeg(p.centre, a, q);
    return where <= span;
}

/// The point of a piece nearest to `q`.
struct Nearest
{
    Point2 at{};        ///< that point, rounded to the millimetre
    double gap{0.0};    ///< how far it is, in millimetres, to the exact point
    bool inside{false}; ///< strictly between the piece's two ends
};

Nearest nearest_on(const PathPiece& p, Point2 q)
{
    Nearest n;
    const auto qx     = static_cast<double>(q.x);
    const auto qy     = static_cast<double>(q.y);
    const auto gap_to = [&](Point2 e) {
        const double dx = static_cast<double>(e.x) - qx;
        const double dy = static_cast<double>(e.y) - qy;
        return std::sqrt(dx * dx + dy * dy);
    };
    if (p.kind == PathPiece::Kind::Segment) {
        const auto ax   = static_cast<double>(p.from.x);
        const auto ay   = static_cast<double>(p.from.y);
        const double dx = static_cast<double>(p.to.x) - ax;
        const double dy = static_cast<double>(p.to.y) - ay;
        const double l2 = dx * dx + dy * dy;
        const double t  = l2 > 0.0 ? ((qx - ax) * dx + (qy - ay) * dy) / l2 : 0.0;
        if (t <= 0.0) {
            n.at  = p.from;
            n.gap = gap_to(p.from);
        } else if (t >= 1.0) {
            n.at  = p.to;
            n.gap = gap_to(p.to);
        } else {
            const double px = ax + t * dx;
            const double py = ay + t * dy;
            n.at            = Point2{mm_round(px), mm_round(py)};
            n.gap           = std::sqrt((px - qx) * (px - qx) + (py - qy) * (py - qy));
            n.inside        = true;
        }
        return n;
    }
    const double vx  = qx - static_cast<double>(p.centre.x);
    const double vy  = qy - static_cast<double>(p.centre.y);
    const double len = std::sqrt(vx * vx + vy * vy);
    const auto r     = static_cast<double>(p.radius);
    if (len > 0.0 && on_sweep(p, q)) {
        n.at     = Point2{p.centre.x + mm_round(vx / len * r), p.centre.y + mm_round(vy / len * r)};
        n.gap    = std::fabs(len - r);
        n.inside = full_turn(p) || (n.at != p.from && n.at != p.to);
        return n;
    }
    const double gf = gap_to(p.from);
    const double gt = gap_to(p.to);
    n.at            = gf <= gt ? p.from : p.to;
    n.gap           = std::min(gf, gt);
    return n;
}

/// How far along a piece `q` lies, for ordering the points a piece is split at:
/// the projection for a segment, the angle walked from its start for an arc.
double along(const PathPiece& p, Point2 q)
{
    if (p.kind == PathPiece::Kind::Segment) {
        const auto dx = static_cast<double>(p.to.x - p.from.x);
        const auto dy = static_cast<double>(p.to.y - p.from.y);
        return static_cast<double>(q.x - p.from.x) * dx + static_cast<double>(q.y - p.from.y) * dy;
    }
    const std::int64_t turned = p.sweep_udeg >= 0 ? arc_sweep_udeg(p.centre, p.from, q)
                                                  : arc_sweep_udeg(p.centre, q, p.from);
    return static_cast<double>(turned % kUDegFullCircle);
}

/// The part of `p` from `a` to `b`, walked the way `p` is.
PathPiece sub_piece(const PathPiece& p, Point2 a, Point2 b)
{
    if (p.kind == PathPiece::Kind::Segment) {
        PathPiece s;
        s.from = a;
        s.to   = b;
        return s;
    }
    return arc_piece(p.centre, p.radius, a, b, p.sweep_udeg >= 0);
}

/// `p` cut at every point of `at` that is not one of its ends.
std::vector<PathPiece> split_piece(const PathPiece& p, std::vector<Point2> at)
{
    std::erase_if(at, [&](Point2 q) { return !full_turn(p) && (q == p.from || q == p.to); });
    if (at.empty()) return {p};
    const auto order = [&](Point2 a, Point2 b) {
        const double ta = along(p, a);
        const double tb = along(p, b);
        return ta != tb ? ta < tb : a < b;
    };
    std::ranges::sort(at, order);
    at.erase(std::ranges::unique(at).begin(), at.end());

    std::vector<PathPiece> out;
    if (full_turn(p)) {
        // A WHOLE CIRCLE CUT ONCE is still one closed curve; the far side is cut
        // too, so it becomes two arcs meeting at the point that joins it.
        if (at.size() == 1) {
            at.push_back(Point2{2 * p.centre.x - at.front().x, 2 * p.centre.y - at.front().y});
            std::ranges::sort(at, order);
        }
        for (std::size_t i = 0; i < at.size(); ++i)
            out.push_back(
                arc_piece(p.centre, p.radius, at[i], at[(i + 1) % at.size()], p.sweep_udeg > 0));
        return out;
    }
    Point2 prev = p.from;
    for (const Point2 q : at) {
        out.push_back(sub_piece(p, prev, q));
        prev = q;
    }
    out.push_back(sub_piece(p, prev, p.to));
    return out;
}

/// A piece's bounds: a segment's ends, an arc's whole circle (generous, and a
/// prefilter only).
Box2 piece_box(const PathPiece& p)
{
    Box2 b;
    if (p.kind == PathPiece::Kind::Segment) {
        b.extend(p.from);
        b.extend(p.to);
    } else {
        b.extend(Point2{p.centre.x - p.radius, p.centre.y - p.radius});
        b.extend(Point2{p.centre.x + p.radius, p.centre.y + p.radius});
    }
    return b;
}

bool near_box(const Box2& b, Point2 q, Mm reach) noexcept
{
    return q.x >= b.min_x - reach && q.x <= b.max_x + reach && q.y >= b.min_y - reach &&
           q.y <= b.max_y + reach;
}

/// A piece that no longer spans anything after its ends moved.
bool degenerate(const PathPiece& p) noexcept
{
    if (p.kind == PathPiece::Kind::Segment) return p.from == p.to;
    return p.radius <= 0 || (!full_turn(p) && p.from == p.to);
}

/// ENDS WITHIN THE TOLERANCE ARE ONE NODE. Clustered transitively, in integers,
/// and moved onto the point most of them already share — the corner the drawing
/// meant — with ties broken by position so the choice is the same everywhere.
void cluster_ends(std::vector<Prepared>& pieces, Mm tol, NodeSnaps& snaps)
{
    if (tol <= 0) return;

    struct End
    {
        Point2 at;
        std::size_t piece;
        bool to;
    };

    std::vector<End> ends;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (full_turn(pieces[i].piece)) continue;
        ends.push_back(End{pieces[i].piece.from, i, false});
        ends.push_back(End{pieces[i].piece.to, i, true});
    }
    std::vector<std::size_t> order(ends.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) { return ends[a].at < ends[b].at; });

    std::vector<std::size_t> parent(ends.size());
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    const auto find = [&](std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i         = parent[i];
        }
        return i;
    };
    for (std::size_t a = 0; a < order.size(); ++a) {
        for (std::size_t b = a + 1; b < order.size(); ++b) {
            const End& ea = ends[order[a]];
            const End& eb = ends[order[b]];
            if (eb.at.x - ea.at.x > tol) break;
            if (chebyshev(ea.at, eb.at) > tol) continue;
            const std::size_t ra = find(order[a]);
            const std::size_t rb = find(order[b]);
            if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
        }
    }

    std::map<std::size_t, std::map<Point2, std::size_t>> clusters;
    for (std::size_t i = 0; i < ends.size(); ++i)
        ++clusters[find(i)][ends[i].at];
    std::map<std::size_t, Point2> chosen;
    for (const auto& [root, points] : clusters) {
        Point2 best            = points.begin()->first;
        std::size_t best_count = 0;
        for (const auto& [p, count] : points) {
            if (count > best_count) {
                best       = p;
                best_count = count;
            }
        }
        chosen[root] = best;
    }
    for (std::size_t i = 0; i < ends.size(); ++i) {
        const Point2 to = chosen[find(i)];
        if (to == ends[i].at) continue;
        ++snaps.moved;
        snaps.largest    = std::max(snaps.largest, segment_length(ends[i].at, to));
        PathPiece& piece = pieces[ends[i].piece].piece;
        if (ends[i].to)
            piece.to = to;
        else
            piece.from = to;
    }
    for (Prepared& p : pieces) {
        if (p.piece.kind == PathPiece::Kind::Arc && !full_turn(p.piece) &&
            p.piece.from != p.piece.to)
            p.piece = arc_piece(p.piece.centre, p.piece.radius, p.piece.from, p.piece.to,
                                p.piece.sweep_udeg >= 0);
    }
    std::erase_if(pieces, [](const Prepared& p) { return degenerate(p.piece); });
}

/// How many piece ends meet at each point.
std::map<Point2, std::uint32_t> end_counts(const std::vector<Prepared>& pieces)
{
    std::map<Point2, std::uint32_t> counts;
    for (const Prepared& p : pieces) {
        if (full_turn(p.piece)) continue;
        ++counts[p.piece.from];
        ++counts[p.piece.to];
    }
    return counts;
}

/// Replaces every piece that has cut points with its parts.
void apply_splits(std::vector<Prepared>& pieces, std::vector<std::vector<Point2>>& splits)
{
    std::vector<Prepared> out;
    out.reserve(pieces.size());
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (i >= splits.size() || splits[i].empty()) {
            out.push_back(pieces[i]);
            continue;
        }
        for (const PathPiece& part : split_piece(pieces[i].piece, splits[i])) {
            if (degenerate(part)) continue;
            Prepared q = pieces[i];
            q.piece    = part;
            out.push_back(q);
        }
    }
    pieces = std::move(out);
}

/// Whether piece `p` has an end at `q`.
bool ends_at(const PathPiece& p, Point2 q) noexcept
{
    return !full_turn(p) && (p.from == q || p.to == q);
}

/// AN END WITHIN THE TOLERANCE OF ANOTHER LINE IS ON THAT LINE. A T-junction
/// drawn to the millimetre almost never lands exactly on the line it meets, and
/// an exact arrangement would read every one of them as a gap. The END moves —
/// onto the nearest point of the line, rounded — and the line is cut there, so
/// the line it met stays where it was measured to within half a millimetre and
/// the stray end is the one corrected.
void snap_ends_to_lines(std::vector<Prepared>& pieces, Mm tol, NodeSnaps& snaps)
{
    // Half a millimetre is "on" at any tolerance: it is the storage resolution,
    // and the nearest integer point to a line is on it.
    const double reach = std::max(static_cast<double>(tol), 0.5);
    std::vector<Box2> boxes;
    boxes.reserve(pieces.size());
    for (const Prepared& p : pieces)
        boxes.push_back(piece_box(p.piece));
    std::vector<std::vector<Point2>> splits(pieces.size());
    bool any = false;
    for (const auto& [at, count] : end_counts(pieces)) {
        if (count != 1) continue;
        std::size_t best = pieces.size();
        Nearest found;
        for (std::size_t j = 0; j < pieces.size(); ++j) {
            if (ends_at(pieces[j].piece, at)) continue;
            if (!near_box(boxes[j], at, tol + 1)) continue;
            const Nearest n = nearest_on(pieces[j].piece, at);
            if (!n.inside || n.gap > reach) continue;
            if (best == pieces.size() || n.gap < found.gap) {
                best  = j;
                found = n;
            }
        }
        if (best == pieces.size()) continue;
        splits[best].push_back(found.at);
        any = true;
        if (found.at == at) continue;
        ++snaps.moved;
        snaps.largest = std::max(snaps.largest, segment_length(at, found.at));
        for (Prepared& owner : pieces) {
            if (!ends_at(owner.piece, at)) continue;
            PathPiece& piece = owner.piece;
            if (piece.from == at) piece.from = found.at;
            if (piece.to == at) piece.to = found.at;
            if (piece.kind == PathPiece::Kind::Arc && piece.from != piece.to)
                piece = arc_piece(piece.centre, piece.radius, piece.from, piece.to,
                                  piece.sweep_udeg >= 0);
            break;
        }
    }
    if (any) {
        apply_splits(pieces, splits);
        std::erase_if(pieces, [](const Prepared& p) { return degenerate(p.piece); });
    }
}

/// The length of a piece along itself.
double piece_length(const PathPiece& p)
{
    if (p.kind == PathPiece::Kind::Segment) {
        const auto dx = static_cast<double>(p.to.x - p.from.x);
        const auto dy = static_cast<double>(p.to.y - p.from.y);
        return std::sqrt(dx * dx + dy * dy);
    }
    const auto sweep = static_cast<double>(p.sweep_udeg < 0 ? -p.sweep_udeg : p.sweep_udeg);
    return static_cast<double>(p.radius) * sweep * (kPi / (180.0 * 1000000.0));
}

/// How far along a piece from its start `q` lies, in millimetres.
double length_to(const PathPiece& p, Point2 q)
{
    if (p.kind == PathPiece::Kind::Segment) {
        const auto dx = static_cast<double>(q.x - p.from.x);
        const auto dy = static_cast<double>(q.y - p.from.y);
        return std::sqrt(dx * dx + dy * dy);
    }
    return static_cast<double>(p.radius) * along(p, q) * (kPi / (180.0 * 1000000.0));
}

/// Distances along the linework from one point, settled outward only as far as
/// a question needs.
class Walk
{
public:
    explicit Walk(const std::vector<Prepared>& pieces)
    {
        for (const Prepared& p : pieces) {
            if (full_turn(p.piece)) continue;
            const std::size_t a = node(p.piece.from);
            const std::size_t b = node(p.piece.to);
            const double len    = piece_length(p.piece);
            links_[a].emplace_back(b, len);
            links_[b].emplace_back(a, len);
        }
        best_.assign(links_.size(), kFar);
    }

    /// Starts again from `from`.
    void restart(Point2 from)
    {
        for (const std::size_t n : touched_)
            best_[n] = kFar;
        touched_.clear();
        queue_ = {};
        if (const auto it = nodes_.find(from); it != nodes_.end()) reach(it->second, 0.0);
    }

    /// The distance along the linework to the node at `q`, or `kFar` when it is
    /// not within `limit`.
    double to(Point2 q, double limit)
    {
        const auto it = nodes_.find(q);
        if (it == nodes_.end()) return kFar;
        settle(limit);
        return best_[it->second] <= limit ? best_[it->second] : kFar;
    }

    static constexpr double kFar = 1e300;

private:
    std::size_t node(Point2 p)
    {
        const auto [it, fresh] = nodes_.try_emplace(p, links_.size());
        if (fresh) links_.emplace_back();
        return it->second;
    }

    void reach(std::size_t n, double d)
    {
        if (d >= best_[n]) return;
        if (best_[n] == kFar) touched_.push_back(n);
        best_[n] = d;
        queue_.emplace(d, n);
    }

    void settle(double limit)
    {
        while (!queue_.empty() && queue_.top().first <= limit) {
            const auto [d, n] = queue_.top();
            queue_.pop();
            if (d > best_[n]) continue;
            for (const auto& [m, len] : links_[n])
                reach(m, d + len);
        }
    }

    std::map<Point2, std::size_t> nodes_;
    std::vector<std::vector<std::pair<std::size_t, double>>> links_;
    std::vector<double> best_;
    std::vector<std::size_t> touched_;
    std::priority_queue<std::pair<double, std::size_t>, std::vector<std::pair<double, std::size_t>>,
                        std::greater<>>
        queue_;
};

/// Every end the arrangement found meeting nothing, and the nearest linework to
/// it that is a GAP away rather than a walk away.
///
/// THE LINE AN END BELONGS TO IS NOT ITS GAP. The nearest point to a dangling
/// end is very often on its own line — the corner it turned a moment before, the
/// next piece of a zig-zag — and bridging to that would close a sliver, not a
/// gap. So a point counts only when walking there along the linework is more
/// than twice as far as the straight line to it: the far end of a parcel's
/// boundary that stopped 5 cm short is sixty metres round and five centimetres
/// across, while the corner the same line just turned is as near one way as the
/// other. Linework the end is not connected to at all always counts.
std::vector<OpenEnd> measure_open_ends(const std::vector<Prepared>& pieces,
                                       const std::vector<Point2>& free_points)
{
    std::vector<OpenEnd> out;
    out.reserve(free_points.size());
    Walk walk(pieces);

    struct Candidate
    {
        double gap;
        Point2 at;
        std::size_t piece;
    };

    std::vector<Candidate> candidates;
    for (const Point2 at : free_points) {
        OpenEnd end;
        end.at = at;
        candidates.clear();
        for (std::size_t j = 0; j < pieces.size(); ++j) {
            const Prepared& p = pieces[j];
            if (ends_at(p.piece, at)) {
                if (p.bridge < 0) end.source = p.source;
                continue;
            }
            const Nearest n = nearest_on(p.piece, at);
            candidates.push_back(Candidate{n.gap, n.at, j});
        }
        std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
            return a.gap != b.gap ? a.gap < b.gap : a.at < b.at;
        });
        walk.restart(at);
        for (const Candidate& c : candidates) {
            const PathPiece& piece = pieces[c.piece].piece;
            const double limit     = 2.0 * c.gap;
            double walked          = Walk::kFar;
            if (!full_turn(piece)) {
                const double from_start = walk.to(piece.from, limit);
                const double from_end   = walk.to(piece.to, limit);
                const double into       = length_to(piece, c.at);
                if (from_start < Walk::kFar) walked = std::min(walked, from_start + into);
                if (from_end < Walk::kFar)
                    walked = std::min(walked, from_end + piece_length(piece) - into);
            }
            if (walked <= limit) continue;
            end.has_nearest = true;
            end.nearest     = c.at;
            end.distance    = mm_round(c.gap);
            break;
        }
        out.push_back(end);
    }
    return out;
}

/// BRIDGES, WHEN ASKED FOR: every open end whose nearest linework is within
/// `reach` is joined to it by a segment, the far piece cut where the segment
/// lands. Each is recorded, because a bridge is a line the drawing did not have.
void add_bridges(std::vector<Prepared>& pieces, const std::vector<OpenEnd>& open, Mm reach,
                 std::vector<Bridge>& bridges)
{
    std::set<std::pair<Point2, Point2>> made;
    std::vector<std::vector<Point2>> splits(pieces.size());
    std::vector<Prepared> added;
    for (const OpenEnd& end : open) {
        if (!end.has_nearest || end.distance <= 0 || end.distance > reach) continue;
        const Point2 a = std::min(end.at, end.nearest);
        const Point2 b = std::max(end.at, end.nearest);
        if (!made.insert({a, b}).second) continue;
        for (std::size_t j = 0; j < pieces.size(); ++j) {
            if (ends_at(pieces[j].piece, end.nearest)) continue;
            const Nearest n = nearest_on(pieces[j].piece, end.nearest);
            if (n.inside && n.gap < 1.0) splits[j].push_back(end.nearest);
        }
        Prepared bridge;
        bridge.piece.from = end.at;
        bridge.piece.to   = end.nearest;
        bridge.bridge     = static_cast<std::int32_t>(bridges.size());
        added.push_back(bridge);
        bridges.push_back(Bridge{end.at, end.nearest, end.distance});
    }
    apply_splits(pieces, splits);
    pieces.insert(pieces.end(), added.begin(), added.end());
}

#endif // KENTOS_HAVE_CGAL

/// Every entity whose box meets `box`, in slot order: the index and the tail it
/// has not taken in yet, the way a pick gathers (pick.cpp).
std::vector<EntityId> gather(const Document& doc, const Box2& box)
{
    const EntityTable& ents   = doc.entities();
    const SpatialIndex& index = doc.spatial_index();
    std::vector<EntityId> out;
    index.query(box, out);
    for (EntityId e = doc.indexed_upto(); e < ents.size(); ++e) {
        const Box2 b = ents.box_of(e);
        if (!b.empty() && b.min_x <= box.max_x && b.max_x >= box.min_x && b.min_y <= box.max_y &&
            b.max_y >= box.min_y)
            out.push_back(e);
    }
    std::ranges::sort(out);
    out.erase(std::ranges::unique(out).begin(), out.end());
    return out;
}

/// The bounds of everything drawn: the index's, and its untaken tail's.
Box2 drawing_bounds(const Document& doc)
{
    const EntityTable& ents = doc.entities();
    Box2 b                  = doc.spatial_index().bounds();
    for (EntityId e = doc.indexed_upto(); e < ents.size(); ++e)
        if (ents.alive(e)) b.extend(ents.box_of(e));
    return b;
}

/// The bounds of a closed path as it is drawn.
Box2 path_box(const CurvePath& path)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    path_outline(path, xs, ys);
    Box2 b;
    for (std::size_t i = 0; i < xs.size(); ++i)
        b.extend(Point2{xs[i], ys[i]});
    return b;
}

#ifdef KENTOS_HAVE_CGAL
/// Turns a closed path so it starts at its lowest-left vertex: the same face
/// reads the same whichever edge the arrangement happened to start it at.
void start_lowest(FaceRing& ring)
{
    std::vector<PathPiece>& pieces = ring.path.pieces;
    if (pieces.size() < 2) return;
    std::size_t best = 0;
    for (std::size_t i = 1; i < pieces.size(); ++i)
        if (pieces[i].from < pieces[best].from) best = i;
    std::ranges::rotate(pieces, pieces.begin() + static_cast<std::ptrdiff_t>(best));
    std::ranges::rotate(ring.sources, ring.sources.begin() + static_cast<std::ptrdiff_t>(best));
}
#endif // KENTOS_HAVE_CGAL

} // namespace

// ---------------------------------------------------------------- areas ----

Mm2 path_area(const CurvePath& path)

{
    if (path.pieces.empty()) return 0;
    // THE POLYGON OF THE ENDS, IN 128 BITS, then each arc's segment added or
    // taken away by the sign of its sweep. A counter-clockwise arc on a
    // counter-clockwise ring bulges outward and adds.
    __int128 twice = 0;
    for (const PathPiece& p : path.pieces)
        twice +=
            static_cast<__int128>(p.from.x) * p.to.y - static_cast<__int128>(p.to.x) * p.from.y;
    auto area = static_cast<Mm2>(twice / 2);
    for (const PathPiece& p : path.pieces) {
        if (p.kind != PathPiece::Kind::Arc) continue;
        area += p.sweep_udeg >= 0 ? circular_segment_area(p.radius, p.sweep_udeg)
                                  : -circular_segment_area(p.radius, -p.sweep_udeg);
    }
    return area;
}

// --------------------------------------------------------------- pieces ----

std::vector<NetworkPiece> network_pieces(const Document& doc, EntityId e, std::uint32_t source)
{
    std::vector<NetworkPiece> out;
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return out;
    if ((ents.flags[e] & FlagInBlock) != 0) return out;
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];
    const KindId kind        = ents.kind[e];
    const auto segment       = [&](Point2 a, Point2 b, bool approximate) {
        if (a == b) return;
        NetworkPiece n;
        n.piece.from  = a;
        n.piece.to    = b;
        n.source      = source;
        n.approximate = approximate;
        out.push_back(n);
    };

    if (kind == kPolylineKind) {
        if (doc.texts().has(slot)) return out; // a caption's baseline bounds nothing
        const RingSpan span = geom.rings_of(slot);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs       = geom.ring_xs(r);
            const auto ys       = geom.ring_ys(r);
            const std::size_t n = xs.size();
            for (std::size_t i = 0; i + 1 < n; ++i)
                segment(Point2{xs[i], ys[i]}, Point2{xs[i + 1], ys[i + 1]}, false);
            if (geom.ring_role[r] != RingRole::Open && n >= 3)
                segment(Point2{xs[n - 1], ys[n - 1]}, Point2{xs[0], ys[0]}, false);
        }
        return out;
    }
    if (kind == kArcKind || kind == kCircleKind || kind == kArcPolylineKind) {
        if (auto path = path_of(doc, e)) {
            for (const PathPiece& piece : path->pieces) {
                NetworkPiece n;
                n.piece  = piece;
                n.source = source;
                out.push_back(n);
            }
        }
        return out;
    }
    if (kind == kEllipseKind || kind == kSplineKind) {
        // AS DRAWN, AND SAID. An ellipse or a spline is not a path yet
        // (curve_path.hpp); the chords it is drawn with stand in for it, and a
        // face they bound carries the mark so the caller can report how far the
        // drawing is from the curve.
        EmitBuffer drawn;
        if (!curve_outline(kind, geom, slot, drawn)) return out;
        for (std::size_t r = 0; r < drawn.run_total(); ++r) {
            const std::uint32_t first = drawn.run_start[r];
            const std::uint32_t count = drawn.run_count[r];
            for (std::uint32_t v = 0; v + 1 < count; ++v)
                segment(Point2{drawn.xs[first + v], drawn.ys[first + v]},
                        Point2{drawn.xs[first + v + 1], drawn.ys[first + v + 1]}, true);
            if (drawn.run_closed[r] != 0 && count >= 3)
                segment(Point2{drawn.xs[first + count - 1], drawn.ys[first + count - 1]},
                        Point2{drawn.xs[first], drawn.ys[first]}, true);
        }
        return out;
    }
    return out; // a dimension, a hatch, a block, a point: they bound no ground
}

// -------------------------------------------------------------- network ----

#ifdef KENTOS_HAVE_CGAL
namespace {

using Kernel      = CGAL::Exact_predicates_exact_constructions_kernel;
using BaseTraits  = CGAL::Arr_circle_segment_traits_2<Kernel>;
using Traits      = CGAL::Arr_consolidated_curve_data_traits_2<BaseTraits, std::uint32_t>;
using Arrangement = CGAL::Arrangement_2<Traits>;
using Halfedge    = Arrangement::Halfedge_const_handle;
using FaceHandle  = Arrangement::Face_const_handle;
using VertexH     = Arrangement::Vertex_const_handle;

Kernel::Point_2 kpoint(Point2 p)
{
    // Exact: a coordinate is an integer well inside a double's 53 bits.
    return {Kernel::FT(static_cast<double>(p.x)), Kernel::FT(static_cast<double>(p.y))};
}

BaseTraits::Curve_2 base_curve(const PathPiece& p)
{
    if (p.kind == PathPiece::Kind::Segment)
        return {Kernel::Segment_2(kpoint(p.from), kpoint(p.to))};
    if (full_turn(p)) {
        const Kernel::FT r(static_cast<double>(p.radius));
        return {Kernel::Circle_2(kpoint(p.centre), r * r, CGAL::COUNTERCLOCKWISE)};
    }
    // THROUGH ITS OWN ENDS. The arc is the one through the two stored end points
    // and the point halfway along it, so it meets the segments that share those
    // points exactly — a stored arc's ends are rounded to the millimetre and lie
    // on its circle only to within that.
    return {kpoint(p.from), kpoint(arc_middle(p)), kpoint(p.to)};
}

Point2 rounded(const BaseTraits::Point_2& p)
{
    return Point2{mm_round(CGAL::to_double(p.x())), mm_round(CGAL::to_double(p.y()))};
}

/// The face a point location landed in, or null. CGAL 6 answers with a
/// `std::variant`, 5.x with a `boost::variant`; Ubuntu 24.04 ships 5.6, so both
/// are read (CLAUDE.md 2.7: the distribution's package, not a vendored copy).
template<typename Located> const FaceHandle* located_face(const Located& found)
{
#if CGAL_VERSION_NR >= 1060000000
    return std::get_if<FaceHandle>(&found);
#else
    return boost::get<FaceHandle>(&found);
#endif
}

} // namespace
#endif

struct Network::Impl
{
    std::vector<Prepared> pieces;
    std::vector<Bridge> bridges;
    NodeSnaps snaps;
    mutable std::size_t collapsed{0};
#ifdef KENTOS_HAVE_CGAL
    Traits traits;
    Arrangement arr{&traits};

    void insert_all()
    {
        std::vector<Traits::Curve_2> curves;
        curves.reserve(pieces.size());
        for (std::size_t i = 0; i < pieces.size(); ++i)
            curves.emplace_back(base_curve(pieces[i].piece), static_cast<std::uint32_t>(i));
        arr.clear();
        CGAL::insert(arr, curves.begin(), curves.end());
    }

    /// The ends of the arrangement that meet nothing: its degree-one vertices,
    /// which are always stored ends and so exact integers.
    std::vector<Point2> free_points() const
    {
        std::vector<Point2> out;
        for (auto v = arr.vertices_begin(); v != arr.vertices_end(); ++v)
            if (v->degree() == 1) out.push_back(rounded(v->point()));
        std::ranges::sort(out);
        out.erase(std::ranges::unique(out).begin(), out.end());
        return out;
    }

    /// Whether a circular half-edge is walked counter-clockwise round its circle.
    bool ccw(Halfedge h) const
    {
        const auto& xc         = h->curve();
        const bool along       = traits.equal_2_object()(h->source()->point(), xc.source());
        const bool curve_turns = xc.orientation() == CGAL::COUNTERCLOCKWISE;
        return along == curve_turns;
    }

    /// The arc piece a circular half-edge lies on: its centre and radius are the
    /// stored ones, never the exact circle the arrangement worked with.
    const PathPiece* arc_source(Halfedge h) const
    {
        for (const std::uint32_t i : h->curve().data())
            if (pieces[i].piece.kind == PathPiece::Kind::Arc) return &pieces[i].piece;
        return nullptr;
    }

    /// A CCB as rings: the edges with the face on BOTH sides — a dangling line,
    /// or a bridge joining an island to the boundary — are left out, and what
    /// remains is linked end to start, so an island reached by a bridge becomes
    /// a ring of its own.
    static std::vector<std::vector<Halfedge>>
    cycles_of(Arrangement::Ccb_halfedge_const_circulator start, FaceHandle f)
    {
        std::vector<Halfedge> ccb;
        auto c = start;
        do {
            ccb.push_back(c);
        } while (++c != start);
        const std::size_t n = ccb.size();
        std::vector<char> keep(n, 0);
        for (std::size_t i = 0; i < n; ++i)
            keep[i] = ccb[i]->twin()->face() != f ? 1 : 0;
        constexpr auto kNone = static_cast<std::size_t>(-1);
        std::vector<std::size_t> next(n, kNone);
        for (std::size_t i = 0; i < n; ++i) {
            if (keep[i] == 0) continue;
            const VertexH v = ccb[i]->target();
            for (std::size_t k = 1; k <= n; ++k) {
                const std::size_t j = (i + k) % n;
                if (keep[j] != 0 && ccb[j]->source() == v) {
                    next[i] = j;
                    break;
                }
            }
        }
        std::vector<std::vector<Halfedge>> cycles;
        std::vector<char> seen(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            if (keep[i] == 0 || seen[i] != 0) continue;
            std::vector<Halfedge> cycle;
            for (std::size_t j = i; j != kNone && seen[j] == 0; j = next[j]) {
                seen[j] = 1;
                cycle.push_back(ccb[j]);
            }
            // A ring that touches itself at a vertex is two rings.
            std::vector<Halfedge> path;
            for (const Halfedge h : cycle) {
                path.push_back(h);
                const VertexH v = h->target();
                for (std::size_t k = 0; k < path.size(); ++k) {
                    if (path[k]->source() != v) continue;
                    cycles.emplace_back(path.begin() + static_cast<std::ptrdiff_t>(k), path.end());
                    path.resize(k);
                    break;
                }
            }
            if (!path.empty()) cycles.push_back(path);
        }
        return cycles;
    }

    /// One cycle of half-edges as a ring of pieces. Arcs of one circle that meet
    /// at a vertex nothing else touches are one arc again — the arrangement cuts
    /// every arc where it turns back in x, and those cuts are its, not the
    /// drawing's.
    FaceRing ring_of(const std::vector<Halfedge>& cycle) const
    {
        FaceRing ring;
        ring.path.closed    = true;
        const std::size_t n = cycle.size();
        if (n == 0) return ring;
        std::vector<char> join(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            const Halfedge a = cycle[i];
            const Halfedge b = cycle[(i + 1) % n];
            if (a->curve().is_circular() && b->curve().is_circular() &&
                a->target()->degree() == 2 && a->curve().has_same_supporting_curve(b->curve()) &&
                ccw(a) == ccw(b))
                join[i] = 1;
        }
        const auto credit = [&](Halfedge h, std::vector<std::uint32_t>& into) {
            for (const std::uint32_t i : h->curve().data()) {
                const Prepared& p = pieces[i];
                if (p.approximate) ring.approximate = true;
                if (p.bridge >= 0) {
                    ring.bridges.push_back(static_cast<std::size_t>(p.bridge));
                    continue;
                }
                into.push_back(p.source);
            }
        };
        const auto finish = [](std::vector<std::uint32_t>& s) {
            std::ranges::sort(s);
            s.erase(std::ranges::unique(s).begin(), s.end());
        };

        std::size_t start = n;
        for (std::size_t i = 0; i < n; ++i) {
            if (join[(i + n - 1) % n] == 0) {
                start = i;
                break;
            }
        }
        if (start == n) {
            // One whole circle, made of the pieces the arrangement cut it into.
            const PathPiece* src = arc_source(cycle.front());
            if (src == nullptr) return ring;
            PathPiece whole;
            whole.kind       = PathPiece::Kind::Arc;
            whole.centre     = src->centre;
            whole.radius     = src->radius;
            whole.from       = rounded(cycle.front()->source()->point());
            whole.to         = whole.from;
            whole.sweep_udeg = ccw(cycle.front()) ? kUDegFullCircle : -kUDegFullCircle;
            std::vector<std::uint32_t> sources;
            for (const Halfedge h : cycle)
                credit(h, sources);
            finish(sources);
            ring.path.pieces.push_back(whole);
            ring.sources.push_back(std::move(sources));
            return ring;
        }

        std::size_t i = start;
        for (std::size_t done = 0; done < n;) {
            std::size_t j   = i;
            std::size_t run = 1;
            while (join[j] != 0 && done + run < n) {
                j = (j + 1) % n;
                ++run;
            }
            const Halfedge first = cycle[i];
            const Halfedge last  = cycle[j];
            const Point2 from    = rounded(first->source()->point());
            const Point2 to      = rounded(last->target()->point());
            std::vector<std::uint32_t> sources;
            for (std::size_t k = 0; k < run; ++k)
                credit(cycle[(i + k) % n], sources);
            finish(sources);
            if (from != to) {
                PathPiece piece;
                if (first->curve().is_circular()) {
                    const PathPiece* src = arc_source(first);
                    if (src != nullptr)
                        piece = arc_piece(src->centre, src->radius, from, to, ccw(first));
                } else {
                    piece.from = from;
                    piece.to   = to;
                }
                if (piece.kind == PathPiece::Kind::Arc || piece.from != piece.to) {
                    ring.path.pieces.push_back(piece);
                    ring.sources.push_back(std::move(sources));
                }
            }
            done += run;
            i = (j + 1) % n;
        }
        std::ranges::sort(ring.bridges);
        ring.bridges.erase(std::ranges::unique(ring.bridges).begin(), ring.bridges.end());
        start_lowest(ring);
        return ring;
    }

    /// Whether a rounded ring still encloses anything.
    static bool usable(const FaceRing& ring)
    {
        const std::vector<PathPiece>& p = ring.path.pieces;
        if (p.empty()) return false;
        bool arcs = false;
        for (const PathPiece& piece : p)
            arcs = arcs || piece.kind == PathPiece::Kind::Arc;
        if (p.size() == 1) return full_turn(p.front());
        return (arcs || p.size() >= 3) && path_area(ring.path) != 0;
    }

    std::optional<NetworkFace> face_of(FaceHandle f, bool islands) const
    {
        NetworkFace face;
        bool have_outer = false;
        std::vector<FaceRing> holes;
        for (const std::vector<Halfedge>& cycle : cycles_of(f->outer_ccb(), f)) {
            FaceRing ring = ring_of(cycle);
            if (!usable(ring)) continue;
            const Mm2 area = path_area(ring.path);
            if (area > 0 && !have_outer) {
                face.outer      = std::move(ring);
                face.outer_area = area;
                have_outer      = true;
            } else if (area < 0) {
                holes.push_back(std::move(ring));
            }
        }
        if (!have_outer) {
            ++collapsed;
            return std::nullopt;
        }
        for (auto inner = f->inner_ccbs_begin(); inner != f->inner_ccbs_end(); ++inner) {
            for (const std::vector<Halfedge>& cycle : cycles_of(*inner, f)) {
                FaceRing ring = ring_of(cycle);
                if (usable(ring) && path_area(ring.path) < 0) holes.push_back(std::move(ring));
            }
        }
        face.area = face.outer_area;
        if (islands) {
            for (const FaceRing& hole : holes)
                face.area += path_area(hole.path);
            face.holes = std::move(holes);
        }
        return face;
    }

    using Located = decltype(std::declval<CGAL::Arr_naive_point_location<Arrangement>>().locate(
        std::declval<BaseTraits::Point_2>()));

    Located locate(Point2 p) const
    {
        const CGAL::Arr_naive_point_location<Arrangement> where(arr);
        return where.locate(BaseTraits::Point_2(Kernel::FT(static_cast<double>(p.x)),
                                                Kernel::FT(static_cast<double>(p.y))));
    }
#endif
};

Network::Network(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Network::Network(Network&&) noexcept            = default;
Network& Network::operator=(Network&&) noexcept = default;
Network::~Network()                             = default;

Result<Network> Network::build(std::span<const NetworkPiece> pieces, Mm node_tolerance, Mm bridge)
{
#ifndef KENTOS_HAVE_CGAL
    (void)pieces;
    (void)node_tolerance;
    (void)bridge;
    return err(ErrorCode::Unsupported,
               "Bu derleme CGAL olmadan yapıldı; kapalı bölge bulunamıyor. CGAL'ı kurup "
               "KENTOS_WITH_CGAL=ON ile yeniden derleyin (brew install cgal · apt install "
               "libcgal-dev · vcpkg install cgal).");
#else
    auto impl = std::make_unique<Impl>();
    impl->pieces.reserve(pieces.size());
    for (const NetworkPiece& n : pieces) {
        if (degenerate(n.piece)) continue;
        Prepared p;
        p.piece       = n.piece;
        p.source      = n.source;
        p.approximate = n.approximate;
        impl->pieces.push_back(p);
    }
    cluster_ends(impl->pieces, node_tolerance, impl->snaps);
    snap_ends_to_lines(impl->pieces, node_tolerance, impl->snaps);
    impl->insert_all();
    if (bridge > 0) {
        const std::vector<OpenEnd> open = measure_open_ends(impl->pieces, impl->free_points());
        const std::size_t before        = impl->bridges.size();
        add_bridges(impl->pieces, open, bridge, impl->bridges);
        if (impl->bridges.size() != before) impl->insert_all();
    }
    return Network(std::move(impl));
#endif
}

std::optional<NetworkFace> Network::face_at(Point2 p, bool islands) const
{
    impl_->collapsed = 0;
#ifdef KENTOS_HAVE_CGAL
    const auto found = impl_->locate(p);
    if (const FaceHandle* f = located_face(found)) {
        if ((*f)->is_unbounded()) return std::nullopt;
        return impl_->face_of(*f, islands);
    }
#else
    (void)p;
    (void)islands;
#endif
    return std::nullopt;
}

bool Network::on_linework(Point2 p) const
{
#ifdef KENTOS_HAVE_CGAL
    const auto found = impl_->locate(p);
    return located_face(found) == nullptr;
#else
    (void)p;
    return false;
#endif
}

std::vector<NetworkFace> Network::faces(bool islands) const
{
    impl_->collapsed = 0;
    std::vector<NetworkFace> out;
#ifdef KENTOS_HAVE_CGAL
    for (auto f = impl_->arr.faces_begin(); f != impl_->arr.faces_end(); ++f) {
        if (f->is_unbounded()) continue;
        if (auto face = impl_->face_of(f, islands)) out.push_back(std::move(*face));
    }
    // By position, so a network lists its faces the same way on every machine
    // whatever order the arrangement stored them in.
    std::ranges::sort(out, [](const NetworkFace& a, const NetworkFace& b) {
        return a.outer.path.pieces.front().from < b.outer.path.pieces.front().from;
    });
#else
    (void)islands;
#endif
    return out;
}

std::size_t Network::collapsed() const noexcept
{
    return impl_->collapsed;
}

std::vector<OpenEnd> Network::open_ends() const
{
#ifdef KENTOS_HAVE_CGAL
    return measure_open_ends(impl_->pieces, impl_->free_points());
#else
    return {};
#endif
}

std::vector<std::vector<std::uint32_t>> Network::overlaps() const
{
    std::set<std::vector<std::uint32_t>> groups;
#ifdef KENTOS_HAVE_CGAL
    for (auto e = impl_->arr.edges_begin(); e != impl_->arr.edges_end(); ++e) {
        std::vector<std::uint32_t> sources;
        for (const std::uint32_t i : e->curve().data())
            if (impl_->pieces[i].bridge < 0) sources.push_back(impl_->pieces[i].source);
        std::ranges::sort(sources);
        sources.erase(std::ranges::unique(sources).begin(), sources.end());
        if (sources.size() >= 2) groups.insert(std::move(sources));
    }
#endif
    return {groups.begin(), groups.end()};
}

const std::vector<Bridge>& Network::bridges() const noexcept
{
    return impl_->bridges;
}

NodeSnaps Network::snaps() const noexcept
{
    return impl_->snaps;
}

// -------------------------------------------------------------- preview ----

std::vector<std::uint8_t> encode_region_preview(const RegionPreview& preview)
{
    // version, islands, tolerance, bridge, count, keys — little-endian as the
    // machine writes it, because the bytes never leave the process.
    const auto count = static_cast<std::uint32_t>(preview.keys.size());
    std::vector<std::uint8_t> bytes(2 + 2 * sizeof(Mm) + sizeof(count) +
                                    preview.keys.size() * sizeof(std::int64_t));
    bytes[0]           = 1;
    bytes[1]           = preview.islands ? 1 : 0;
    std::size_t offset = 2;
    std::memcpy(bytes.data() + offset, &preview.node_tolerance, sizeof(Mm));
    offset += sizeof(Mm);
    std::memcpy(bytes.data() + offset, &preview.bridge, sizeof(Mm));
    offset += sizeof(Mm);
    std::memcpy(bytes.data() + offset, &count, sizeof(count));
    offset += sizeof(count);
    if (count != 0)
        std::memcpy(bytes.data() + offset, preview.keys.data(), count * sizeof(std::int64_t));
    return bytes;
}

Result<RegionPreview> decode_region_preview(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t kHead = 2 + 2 * sizeof(Mm) + sizeof(std::uint32_t);
    if (bytes.size() < kHead || bytes[0] != 1 || bytes[1] > 1)
        return err(ErrorCode::InvalidArgument, "Sınır önizlemesinin baytları tanınmıyor.");
    RegionPreview preview;
    preview.islands    = bytes[1] != 0;
    std::size_t offset = 2;
    std::memcpy(&preview.node_tolerance, bytes.data() + offset, sizeof(Mm));
    offset += sizeof(Mm);
    std::memcpy(&preview.bridge, bytes.data() + offset, sizeof(Mm));
    offset += sizeof(Mm);
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + offset, sizeof(count));
    offset += sizeof(count);
    if (bytes.size() != offset + static_cast<std::size_t>(count) * sizeof(std::int64_t))
        return err(ErrorCode::InvalidArgument, "Sınır önizlemesinin baytları tanınmıyor.");
    preview.keys.resize(count);
    if (count != 0)
        std::memcpy(preview.keys.data(), bytes.data() + offset, count * sizeof(std::int64_t));
    return preview;
}

// --------------------------------------------------------------- region ----

Result<Region> region_at(const Document& doc, const RegionQuery& query)
{
    if (!network_available())
        return err(ErrorCode::Unsupported,
                   "Bu derleme CGAL olmadan yapıldı; kapalı bölge bulunamıyor. CGAL'ı kurup "
                   "KENTOS_WITH_CGAL=ON ile yeniden derleyin.");
    const EntityTable& ents = doc.entities();
    std::vector<char> allowed;
    if (!query.only.empty()) {
        allowed.assign(ents.size(), 0);
        for (const EntityId e : query.only)
            if (e < ents.size()) allowed[e] = 1;
    }
    Region region;
    const Box2 extent = drawing_bounds(doc);
    if (extent.empty()) return region;

    // OUTWARD FROM THE POINT, until the face found no longer reaches the edge of
    // what was gathered. Linework that crosses a face must cross its bounding
    // box, so once that box sits strictly inside the gathered one nothing left
    // out could change the answer — which therefore depends on the drawing and
    // never on the view, the way every client needs it to (Article 1.2).
    constexpr Mm kFirstReach = 16'000;
    for (Mm reach = kFirstReach;; reach *= 2) {
        const Box2 box{query.at.x - reach, query.at.y - reach, query.at.x + reach,
                       query.at.y + reach};
        const bool whole = box.min_x <= extent.min_x && box.min_y <= extent.min_y &&
                           box.max_x >= extent.max_x && box.max_y >= extent.max_y;
        const bool last = whole || (query.max_reach > 0 && reach * 2 > query.max_reach);
        std::vector<NetworkPiece> pieces;
        for (const EntityId e : gather(doc, box)) {
            if (!ents.visible(e)) continue;
            if (!allowed.empty() && (e >= allowed.size() || allowed[e] == 0)) continue;
            std::vector<NetworkPiece> own = network_pieces(doc, e, e);
            pieces.insert(pieces.end(), own.begin(), own.end());
        }
        auto net = Network::build(pieces, query.node_tolerance, query.bridge);
        if (!net) return net.error();
        const Network& network = net.value();
        region.snaps           = network.snaps();
        if (network.on_linework(query.at)) {
            region.on_linework = true;
            return region;
        }
        std::optional<NetworkFace> face = network.face_at(query.at, query.islands);
        if (face) {
            const Box2 fb     = path_box(face->outer.path);
            const bool inside = fb.min_x > box.min_x && fb.min_y > box.min_y &&
                                fb.max_x < box.max_x && fb.max_y < box.max_y;
            if (inside || last) {
                std::vector<const FaceRing*> rings{&face->outer};
                for (const FaceRing& hole : face->holes)
                    rings.push_back(&hole);
                std::set<std::size_t> used;
                for (const FaceRing* ring : rings) {
                    region.approximate = region.approximate || ring->approximate;
                    for (const std::vector<std::uint32_t>& sources : ring->sources)
                        region.sources.insert(region.sources.end(), sources.begin(), sources.end());
                    used.insert(ring->bridges.begin(), ring->bridges.end());
                }
                std::ranges::sort(region.sources);
                region.sources.erase(std::ranges::unique(region.sources).begin(),
                                     region.sources.end());
                for (const std::size_t b : used)
                    region.bridges.push_back(network.bridges()[b]);
                if (region.approximate)
                    for (const EntityId e : region.sources)
                        region.deviation = std::max(region.deviation, drawn_deviation(doc, e));
                region.face = std::move(face);
                return region;
            }
        } else if (last) {
            // NOT CLOSED: the open ends say why. Nearest the point first, since
            // the gap that matters is the one in the boundary around the click.
            region.open = network.open_ends();
            std::ranges::sort(region.open, [&](const OpenEnd& a, const OpenEnd& b) {
                const Mm da = segment_length(a.at, query.at);
                const Mm db = segment_length(b.at, query.at);
                return da != db ? da < db : a.at < b.at;
            });
            return region;
        }
        if (last) return region;
    }
}

} // namespace kentos::core
