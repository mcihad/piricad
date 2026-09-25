// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/offset.hpp"

#include "clipper2/clipper.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace kentos::core {
namespace {

Clipper2Lib::JoinType join_of(JoinStyle j)
{
    switch (j) {
    case JoinStyle::Round: return Clipper2Lib::JoinType::Round;
    case JoinStyle::Bevel: return Clipper2Lib::JoinType::Bevel;
    case JoinStyle::Miter: break;
    }
    return Clipper2Lib::JoinType::Miter;
}

Clipper2Lib::EndType end_of(EndStyle e, bool closed)
{
    // A closed ring has no ends, and Clipper2 says so with its own end type
    // rather than with a flag — passing anything else silently treats the ring as
    // an open run and returns a loop around it instead of a parallel of it.
    if (closed) return Clipper2Lib::EndType::Polygon;

    switch (e) {
    case EndStyle::Round: return Clipper2Lib::EndType::Round;
    case EndStyle::Square: return Clipper2Lib::EndType::Square;
    case EndStyle::Joined: return Clipper2Lib::EndType::Joined;
    case EndStyle::Butt: break;
    }
    return Clipper2Lib::EndType::Butt;
}

} // namespace

Result<std::vector<OffsetRing>> offset_ring(const std::vector<Point2>& points, bool closed,
                                            Mm distance, JoinStyle join, EndStyle end)
{
    if (distance == 0)
        return err(ErrorCode::InvalidArgument,
                   "Ofset mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.");

    if (points.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Ofset en az iki nokta ister. Verilen: " + std::to_string(points.size()));

    if (closed && points.size() < 3)
        return err(ErrorCode::InvalidArgument,
                   "Kapalı bir halkanın ofseti en az üç köşe ister. Verilen: " +
                       std::to_string(points.size()));

    Clipper2Lib::Path64 path;
    path.reserve(points.size());
    for (const Point2& p : points)
        path.emplace_back(p.x, p.y);

    Clipper2Lib::ClipperOffset offsetter;

    // THE MITRE LIMIT, and it is not a style choice. Without a bound, a parallel
    // of a nearly-doubled-back edge runs off toward infinity; Clipper2's default
    // of 2 clamps it at twice the offset distance, which is what a plan sheet
    // shows and what every CAD does.
    offsetter.MiterLimit(2.0);
    offsetter.AddPath(path, join_of(join), end_of(end, closed));

    Clipper2Lib::Paths64 out;
    offsetter.Execute(static_cast<double>(distance), out);

    std::vector<OffsetRing> rings;
    rings.reserve(out.size());
    for (const Clipper2Lib::Path64& produced : out) {
        OffsetRing ring;
        // An offset of an open run comes back CLOSED — it is the outline of the
        // band around the run — and an offset of a ring is a ring. Either way
        // what Clipper2 returns is a closed path, and saying otherwise would make
        // the caller draw a gap where the shape meets itself.
        ring.closed = true;
        ring.points.reserve(produced.size());
        for (const Clipper2Lib::Point64& p : produced)
            ring.points.push_back(Point2{static_cast<Mm>(p.x), static_cast<Mm>(p.y)});

        if (ring.points.size() >= 3) rings.push_back(std::move(ring));
    }

    return rings;
}

namespace {

Clipper2Lib::Path64 to_path(const std::vector<Point2>& ring)
{
    Clipper2Lib::Path64 path;
    path.reserve(ring.size());
    for (const Point2& p : ring)
        path.emplace_back(p.x, p.y);
    return path;
}

std::vector<Point2> from_path(const Clipper2Lib::Path64& path)
{
    std::vector<Point2> out;
    out.reserve(path.size());
    for (const Clipper2Lib::Point64& p : path)
        out.push_back(Point2{static_cast<Mm>(p.x), static_cast<Mm>(p.y)});
    return out;
}

/// Every polygon of a Clipper2 tree, holes with their exterior, and an island
/// inside a hole as a polygon of its own — at any depth, which is what a
/// two-level walk loses.
void collect_polygons(const Clipper2Lib::PolyPath64& node, std::vector<Polygon>& out)
{
    for (const auto& child : node) {
        if (child->IsHole()) continue;
        Polygon poly;
        poly.exterior = from_path(child->Polygon());
        for (const auto& hole : *child) {
            if (hole->Polygon().size() >= 3) poly.holes.push_back(from_path(hole->Polygon()));
            collect_polygons(*hole, out); // the islands inside this hole
        }
        if (poly.exterior.size() >= 3) out.push_back(std::move(poly));
    }
}

/// The ring wound the way Clipper2 reads an exterior (counter-clockwise) or a
/// hole (clockwise), whichever way the caller drew it.
Clipper2Lib::Path64 wound(const std::vector<Point2>& ring, bool exterior)
{
    Clipper2Lib::Path64 path = to_path(ring);
    const bool ccw           = Clipper2Lib::Area(path) > 0.0;
    if (ccw != exterior) std::reverse(path.begin(), path.end());
    return path;
}

Clipper2Lib::Paths64 wound_faces(const std::vector<Polygon>& faces)
{
    Clipper2Lib::Paths64 paths;
    for (const Polygon& face : faces) {
        if (face.exterior.size() < 3) continue;
        paths.push_back(wound(face.exterior, true));
        for (const std::vector<Point2>& hole : face.holes)
            if (hole.size() >= 3) paths.push_back(wound(hole, false));
    }
    return paths;
}

/// Twice the signed area of the triangle `a b p`, exactly: positive when `p` is
/// to the left of `a`→`b`.
Int128 cross(Point2 a, Point2 b, Point2 p) noexcept
{
    return static_cast<Int128>(b.x - a.x) * (p.y - a.y) -
           static_cast<Int128>(b.y - a.y) * (p.x - a.x);
}

/// Squared distance from `p` to the segment `a`→`b`, and where along the run
/// the nearest point falls (`along`, millimetres from the run's start given the
/// segment starts at `start`).
double segment_distance2(Point2 a, Point2 b, Point2 p, double start, double& along) noexcept
{
    const double dx  = static_cast<double>(b.x - a.x);
    const double dy  = static_cast<double>(b.y - a.y);
    const double len = dx * dx + dy * dy;
    double t         = 0.0;
    if (len > 0.0)
        t = std::clamp((static_cast<double>(p.x - a.x) * dx + static_cast<double>(p.y - a.y) * dy) /
                           len,
                       0.0, 1.0);
    const double qx = static_cast<double>(a.x) + t * dx - static_cast<double>(p.x);
    const double qy = static_cast<double>(a.y) + t * dy - static_cast<double>(p.y);
    along           = start + t * std::sqrt(len);
    return qx * qx + qy * qy;
}

void push_polygons(const std::vector<Polygon>& in, Clipper2Lib::Paths64& out)
{
    for (const Polygon& poly : in) {
        if (poly.exterior.size() >= 3) out.push_back(to_path(poly.exterior));
        for (const std::vector<Point2>& hole : poly.holes)
            if (hole.size() >= 3) out.push_back(to_path(hole));
    }
}

} // namespace

Mm2 ring_area(const std::vector<Point2>& ring) noexcept
{
    if (ring.size() < 3) return 0;

    // The shoelace sum, in 128-bit intermediates so a TUREF-scale ring cannot
    // overflow: two eastings multiplied are already 6·10^17 and a hundred of them
    // would leave int64 (core.md R3).
    Int128 twice = 0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point2& a = ring[i];
        const Point2& b = ring[(i + 1) % ring.size()];
        twice += static_cast<Int128>(a.x) * b.y - static_cast<Int128>(b.x) * a.y;
    }
    return static_cast<Mm2>(twice / 2);
}

std::vector<std::vector<Point2>> clip_path_to(const std::vector<Point2>& path,
                                              const std::vector<Point2>& window)
{
    std::vector<std::vector<Point2>> out;
    if (path.size() < 2 || window.size() < 3) return out;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(Clipper2Lib::Paths64{to_path(path)});
    clipper.AddClip(Clipper2Lib::Paths64{to_path(window)});
    Clipper2Lib::Paths64 closed;
    Clipper2Lib::Paths64 open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed,
                    open);
    out.reserve(open.size());
    for (const Clipper2Lib::Path64& piece : open)
        out.push_back(from_path(piece));
    return out;
}

std::vector<std::vector<Point2>> clip_ring_to(const std::vector<Point2>& ring,
                                              const std::vector<Point2>& window)
{
    std::vector<std::vector<Point2>> out;
    if (ring.size() < 3 || window.size() < 3) return out;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(Clipper2Lib::Paths64{to_path(ring)});
    clipper.AddClip(Clipper2Lib::Paths64{to_path(window)});
    Clipper2Lib::Paths64 faces;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, faces);
    out.reserve(faces.size());
    for (const Clipper2Lib::Path64& face : faces)
        out.push_back(from_path(face));
    return out;
}

std::vector<Point2> simplify_ring(const std::vector<Point2>& ring, Mm tolerance, bool closed)
{
    // NOTHING CARRIES LESS THAN NO INFORMATION, and a run too short to thin has
    // nothing to give up: an open run needs an interior vertex to drop, a closed
    // one needs a fourth corner or it stops being a face.
    if (tolerance <= 0) return ring;
    if (ring.size() < (closed ? 4u : 3u)) return ring;

    // CLIPPER2 TAKES THE EPSILON AS A SQUARED DISTANCE, in the same units the
    // path is in — millimetres here, so the square of the tolerance. Written out
    // as a double because the square of a metre-scale tolerance overflows nothing
    // but reads badly as an integer expression.
    const auto epsilon = static_cast<double>(tolerance) * static_cast<double>(tolerance);
    const Clipper2Lib::Path64 thinned = Clipper2Lib::SimplifyPath(to_path(ring), epsilon, closed);

    // A SIMPLIFICATION THAT LEFT NOTHING USABLE IS NOT AN ANSWER. Clipper2 can
    // reduce a ring to two points when every vertex is within the tolerance of
    // one line, and two points are not a face — the caller asked to thin a shape,
    // not to delete it, so the original comes back and the caller's count of
    // dropped vertices is honestly zero.
    if (thinned.size() < (closed ? 3u : 2u)) return ring;

    std::vector<Point2> out;
    out.reserve(thinned.size());
    for (const Clipper2Lib::Point64& p : thinned)
        out.push_back(Point2{p.x, p.y});
    return out;
}

Polygon half_plane(Point2 a, Point2 b, const Box2& box, bool left)
{
    // Reach: the box's diagonal, doubled. Anything at least that long puts this
    // rectangle's own corners outside the face whatever angle the cut is at.
    const double dx  = static_cast<double>(b.x - a.x);
    const double dy  = static_cast<double>(b.y - a.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) return {};

    const double wx    = static_cast<double>(box.max_x - box.min_x);
    const double wy    = static_cast<double>(box.max_y - box.min_y);
    const double reach = 2.0 * (std::sqrt(wx * wx + wy * wy) + 1000.0);

    const double ux = dx / len; // along the cut
    const double uy = dy / len;
    const double nx = left ? -uy : uy; // and away from it, to one side
    const double ny = left ? ux : -ux;

    const auto at = [&](double along, double across) {
        return Point2{a.x + mm_round(ux * along + nx * across),
                      a.y + mm_round(uy * along + ny * across)};
    };

    Polygon poly;
    poly.exterior = {at(-reach, 0.0), at(len + reach, 0.0), at(len + reach, reach),
                     at(-reach, reach)};
    return poly;
}

Result<std::vector<Polygon>> polygon_boolean(const std::vector<Polygon>& subject,
                                             const std::vector<Polygon>& clip, BooleanOp op)
{
    if (subject.empty())
        return err(ErrorCode::InvalidArgument, "Boolean işlemi için birinci şekil boş.");

    Clipper2Lib::Paths64 subjects, clips;
    push_polygons(subject, subjects);
    push_polygons(clip, clips);

    if (subjects.empty())
        return err(ErrorCode::InvalidArgument,
                   "Birinci şeklin kapalı bir halkası yok: bir alan en az üç köşe ister.");

    Clipper2Lib::ClipType type = Clipper2Lib::ClipType::Union;
    if (op == BooleanOp::Difference) type = Clipper2Lib::ClipType::Difference;
    if (op == BooleanOp::Intersection) type = Clipper2Lib::ClipType::Intersection;

    // EVEN-ODD, not non-zero. A hole read from a DXF or a GML may be wound the
    // same way as its exterior — the format does not promise otherwise — and
    // even-odd reads it as a hole regardless, which is what makes a parcel with a
    // void inside it survive a round trip through somebody else's software.
    Clipper2Lib::PolyTree64 tree;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(subjects);
    if (!clips.empty()) clipper.AddClip(clips);
    clipper.Execute(type, Clipper2Lib::FillRule::EvenOdd, tree);

    // THE TREE, not a flat path list, because that is what carries which ring is
    // a hole in which. A flat result would leave the caller to work that out from
    // winding, and winding is exactly what even-odd made irrelevant.
    std::vector<Polygon> out;
    for (const auto& child : tree) {
        if (child->IsHole()) continue;

        Polygon poly;
        poly.exterior.reserve(child->Polygon().size());
        for (const Clipper2Lib::Point64& p : child->Polygon())
            poly.exterior.push_back(Point2{static_cast<Mm>(p.x), static_cast<Mm>(p.y)});

        for (const auto& grandchild : *child) {
            if (!grandchild->IsHole()) continue;
            std::vector<Point2> hole;
            hole.reserve(grandchild->Polygon().size());
            for (const Clipper2Lib::Point64& p : grandchild->Polygon())
                hole.push_back(Point2{static_cast<Mm>(p.x), static_cast<Mm>(p.y)});
            if (hole.size() >= 3) poly.holes.push_back(std::move(hole));
        }

        if (poly.exterior.size() >= 3) out.push_back(std::move(poly));
    }

    return out;
}

Result<std::vector<OffsetRing>> parallel_run(const std::vector<Point2>& run, Mm distance,
                                             JoinStyle join)
{
    if (distance == 0)
        return err(ErrorCode::InvalidArgument,
                   "Paralel mesafesi sıfır olamaz. Kaç metre yana istediğinizi yazın.");

    // The source without consecutive repeats: a repeated point has no direction,
    // and a zero-length edge would leave its side to be guessed.
    std::vector<Point2> src;
    src.reserve(run.size());
    for (const Point2& p : run)
        if (src.empty() || !(src.back() == p)) src.push_back(p);
    if (src.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Paralel için çizginin en az iki ayrı noktası olmalı. Verilen: " +
                       std::to_string(src.size()));

    // THE BAND, both sides, butt-ended: its caps are exactly the two ends of each
    // side, which is what lets the sides be told apart below.
    Clipper2Lib::ClipperOffset offsetter;
    offsetter.MiterLimit(2.0); // see `offset_ring`
    offsetter.AddPath(to_path(src), join_of(join), Clipper2Lib::EndType::Butt);
    Clipper2Lib::PolyTree64 tree;
    offsetter.Execute(static_cast<double>(distance < 0 ? -distance : distance), tree);

    // Every ring of the band, outer and hole alike: a run that nearly closes on
    // itself leaves one of its sides as the band's hole.
    std::vector<const Clipper2Lib::Path64*> rings;
    const auto gather = [&rings](const Clipper2Lib::PolyPath64& node, const auto& self) -> void {
        for (const auto& child : node) {
            rings.push_back(&child->Polygon());
            self(*child, self);
        }
    };
    gather(tree, gather);

    // Where each edge of the source starts, measured along it, so a piece of the
    // parallel can be turned to run the way its source runs.
    std::vector<double> starts(src.size(), 0.0);
    for (std::size_t s = 1; s < src.size(); ++s) {
        const double dx = static_cast<double>(src[s].x - src[s - 1].x);
        const double dy = static_cast<double>(src[s].y - src[s - 1].y);
        starts[s]       = starts[s - 1] + std::sqrt(dx * dx + dy * dy);
    }

    const bool left = distance > 0;
    std::vector<OffsetRing> out;
    for (const Clipper2Lib::Path64* ring : rings) {
        const std::size_t n = ring->size();
        if (n < 2) continue;

        // WHICH SIDE EACH VERTEX IS ON: the side of the source edge nearest to it,
        // by the exact sign of a cross product. A band vertex is never ON the
        // source (it is the distance away), so the sign is never zero for one.
        std::vector<char> on(n, 0);
        std::vector<double> along(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            const Point2 v{static_cast<Mm>((*ring)[i].x), static_cast<Mm>((*ring)[i].y)};
            double best      = std::numeric_limits<double>::max();
            std::size_t near = 0;
            for (std::size_t s = 0; s + 1 < src.size(); ++s) {
                double at        = 0.0;
                const double gap = segment_distance2(src[s], src[s + 1], v, starts[s], at);
                if (gap < best) {
                    best     = gap;
                    near     = s;
                    along[i] = at;
                }
            }
            const Int128 side = cross(src[near], src[near + 1], v);
            on[i]             = left ? side > 0 : side < 0;
        }

        // The whole ring on the wanted side: a parallel that met itself.
        if (std::all_of(on.begin(), on.end(), [](char c) { return c != 0; })) {
            OffsetRing closed;
            closed.closed = true;
            closed.points = from_path(*ring);
            out.push_back(std::move(closed));
            continue;
        }

        // The stretches on the wanted side, read from just after a vertex that is
        // not, so no stretch is cut in two where the ring's numbering wraps.
        std::size_t seam = 0;
        while (on[seam] != 0)
            ++seam;
        std::vector<std::size_t> stretch;
        const auto flush = [&] {
            if (stretch.size() >= 2) {
                OffsetRing piece;
                piece.points.reserve(stretch.size());
                for (std::size_t i : stretch)
                    piece.points.push_back(
                        Point2{static_cast<Mm>((*ring)[i].x), static_cast<Mm>((*ring)[i].y)});
                // THE SOURCE'S DIRECTION. Clipper2 winds a face one way, so one
                // side of the band always comes out running backwards.
                if (along[stretch.front()] > along[stretch.back()])
                    std::reverse(piece.points.begin(), piece.points.end());
                out.push_back(std::move(piece));
            }
            stretch.clear();
        };
        for (std::size_t k = 1; k <= n; ++k) {
            const std::size_t i = (seam + k) % n;
            if (on[i] != 0)
                stretch.push_back(i);
            else
                flush();
        }
        flush();
    }

    // A broken parallel reads from the source's start to its end.
    std::stable_sort(out.begin(), out.end(), [&](const OffsetRing& a, const OffsetRing& b) {
        const auto first_along = [&](const OffsetRing& r) {
            double best = std::numeric_limits<double>::max();
            double at   = 0.0;
            for (std::size_t s = 0; s + 1 < src.size(); ++s) {
                double here = 0.0;
                const double gap =
                    segment_distance2(src[s], src[s + 1], r.points.front(), starts[s], here);
                if (gap < best) {
                    best = gap;
                    at   = here;
                }
            }
            return at;
        };
        return first_along(a) < first_along(b);
    });
    return out;
}

Result<std::vector<Polygon>> offset_faces(const std::vector<Polygon>& faces, Mm distance,
                                          JoinStyle join)
{
    if (distance == 0)
        return err(ErrorCode::InvalidArgument,
                   "Ofset mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.");

    const Clipper2Lib::Paths64 paths = wound_faces(faces);
    if (paths.empty())
        return err(ErrorCode::InvalidArgument,
                   "Ofseti alınacak kapalı bir halka yok: bir alan en az üç köşe ister.");

    // ONE GROUP, every ring of every face in it: that is what makes Clipper2 move
    // a hole against its exterior instead of treating it as a face of its own.
    Clipper2Lib::ClipperOffset offsetter;
    offsetter.MiterLimit(2.0);
    offsetter.AddPaths(paths, join_of(join), Clipper2Lib::EndType::Polygon);
    Clipper2Lib::PolyTree64 tree;
    offsetter.Execute(static_cast<double>(distance), tree);

    std::vector<Polygon> out;
    collect_polygons(tree, out);
    return out;
}

Result<std::vector<Polygon>> buffer(const BufferSource& source, Mm distance, JoinStyle join,
                                    EndStyle end)
{
    if (distance == 0)
        return err(ErrorCode::InvalidArgument,
                   "Tampon mesafesi sıfır olamaz. Kaç metre genişlik istediğinizi yazın.");
    if (distance < 0 && (!source.runs.empty() || !source.points.empty()))
        return err(ErrorCode::InvalidArgument,
                   "Eksi tampon yalnız alanlar içindir: bir çizginin ya da noktanın içi yoktur.");

    Clipper2Lib::ClipperOffset offsetter;
    offsetter.MiterLimit(2.0);
    bool any = false;

    for (const std::vector<Point2>& run : source.runs) {
        Clipper2Lib::Path64 path;
        for (const Point2& p : run)
            if (path.empty() || path.back().x != p.x || path.back().y != p.y)
                path.emplace_back(p.x, p.y);
        if (path.empty()) continue;
        // A run that is one point after its repeats are gone is a point.
        offsetter.AddPath(path, path.size() == 1 ? Clipper2Lib::JoinType::Round : join_of(join),
                          path.size() == 1 ? Clipper2Lib::EndType::Round : end_of(end, false));
        any = true;
    }
    for (const Point2& p : source.points) {
        // A DISC, whatever `end` says: a flat or square end has no direction to
        // face at a point.
        offsetter.AddPath(Clipper2Lib::Path64{Clipper2Lib::Point64(p.x, p.y)},
                          Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Round);
        any = true;
    }
    if (const Clipper2Lib::Paths64 faces = wound_faces(source.faces); !faces.empty()) {
        offsetter.AddPaths(faces, join_of(join), Clipper2Lib::EndType::Polygon);
        any = true;
    }
    if (!any)
        return err(ErrorCode::InvalidArgument,
                   "Tamponu alınacak bir şey yok: çizgi, nokta ya da alan verin.");

    Clipper2Lib::PolyTree64 tree;
    offsetter.Execute(static_cast<double>(distance), tree);

    std::vector<Polygon> out;
    collect_polygons(tree, out);
    return out;
}

} // namespace kentos::core
