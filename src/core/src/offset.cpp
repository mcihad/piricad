// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/offset.hpp"

#include "clipper2/clipper.h"

#include <cmath>
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

} // namespace kentos::core
