// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/offset.hpp"

#include "clipper2/clipper.h"

#include <utility>

namespace piricad::core {
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

} // namespace piricad::core
