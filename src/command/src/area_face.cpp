// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: an area as a face for cutting and joining. See
// area_face.hpp.
#include "piricad/command/area_face.hpp"

#include "piricad/command/transaction.hpp"
#include "piricad/core/curve_path.hpp"
#include "piricad/core/entity_kind.hpp"
#include "piricad/core/geometry.hpp"

#include <algorithm>
#include <vector>

namespace piricad::command {
namespace {

/// A closed path of segments through `ring`.
core::CurvePath ring_path(std::span<const core::Point2> ring)
{
    core::CurvePath p;
    p.closed = true;
    for (std::size_t i = 0; i < ring.size(); ++i)
        p.pieces.push_back(
            core::PathPiece{.from = ring[i], .to = ring[i + 1 == ring.size() ? 0 : i + 1]});
    return p;
}

bool bends(const core::CurvePath& path)
{
    return std::ranges::any_of(path.pieces, [](const core::PathPiece& p) {
        return p.kind != core::PathPiece::Kind::Segment;
    });
}

core::Mm2 magnitude(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

} // namespace

std::optional<std::vector<AreaFace>> area_faces(const core::Document& doc, core::EntityId slot)
{
    const core::KindId kind = doc.entities().kind[slot];
    if (kind == core::kArcPolylineKind) {
        // ITS PATH, THE ARCS AS ARCS: an arc polyline has one ring, closed for
        // an area.
        auto path = core::path_of(doc, slot);
        if (!path || !path->closed) return std::nullopt;
        AreaFace out;
        out.curved     = bends(*path);
        out.face.outer = std::move(*path);
        return std::vector<AreaFace>{std::move(out)};
    }
    if (kind != core::kPolylineKind || doc.texts().has(doc.entities().slot[slot]))
        return std::nullopt;

    // THE RINGS, the exterior first and the holes after it, as the ring order
    // stores them (model.md R11).
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);
    std::vector<AreaFace> out;
    std::uint32_t part = 0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});
        if (geom.ring_role[r] == core::RingRole::Exterior) {
            if (ring.size() < 3) return std::nullopt;
            if (!out.empty() && geom.ring_part[r] == part) return std::nullopt;
            part = geom.ring_part[r];
            out.push_back(AreaFace{core::KernelFace{ring_path(ring), {}}, false});
        } else {
            if (out.empty() || geom.ring_part[r] != part || ring.size() < 3) return std::nullopt;
            out.back().face.holes.push_back(ring_path(ring));
        }
    }
    if (out.empty()) return std::nullopt;
    return out;
}

std::optional<AreaFace> area_face(const core::Document& doc, core::EntityId slot)
{
    auto faces = area_faces(doc, slot);
    if (!faces || faces->size() != 1) return std::nullopt;
    return std::move(faces->front());
}

core::Polygon face_polygon(const core::KernelFace& face)
{
    core::Polygon out;
    out.exterior = core::path_vertices(face.outer);
    if (out.exterior.size() > 1 && out.exterior.front() == out.exterior.back())
        out.exterior.pop_back();
    for (const core::CurvePath& hole : face.holes) {
        std::vector<core::Point2> ring = core::path_vertices(hole);
        if (ring.size() > 1 && ring.front() == ring.back()) ring.pop_back();
        out.holes.push_back(std::move(ring));
    }
    return out;
}

core::KernelFace polygon_face(const core::Polygon& polygon)
{
    core::KernelFace out;
    out.outer = ring_path(polygon.exterior);
    for (const std::vector<core::Point2>& hole : polygon.holes)
        out.holes.push_back(ring_path(hole));
    return out;
}

core::Mm2 face_area(const core::KernelFace& face)
{
    core::Mm2 area = magnitude(core::path_area(face.outer));
    for (const core::CurvePath& hole : face.holes)
        area -= magnitude(core::path_area(hole));
    return area;
}

core::Mm2 outer_area(const core::KernelFace& face)
{
    if (!bends(face.outer)) return magnitude(core::ring_area(face_polygon(face).exterior));
    return magnitude(core::path_area(face.outer));
}

core::Result<std::vector<core::KernelFace>> area_boolean(std::span<const core::KernelFace> a,
                                                         std::span<const core::KernelFace> b,
                                                         core::BooleanOp op, bool curved)
{
    if (curved) return core::kernel_boolean(a, b, op);
    std::vector<core::Polygon> pa;
    std::vector<core::Polygon> pb;
    for (const core::KernelFace& f : a)
        pa.push_back(face_polygon(f));
    for (const core::KernelFace& f : b)
        pb.push_back(face_polygon(f));
    auto done = core::polygon_boolean(pa, pb, op);
    if (!done) return done.error();
    std::vector<core::KernelFace> out;
    out.reserve(done.value().size());
    for (const core::Polygon& p : done.value())
        out.push_back(polygon_face(p));
    return out;
}

core::Result<core::EntityId> add_face(Context& ctx, core::LayerId layer,
                                      const core::KernelFace& face)
{
    if (!face.holes.empty() && (bends(face.outer) || std::ranges::any_of(face.holes, bends)))
        return core::err(core::ErrorCode::Unsupported,
                         "Sonuç eğrili bir sınır ve delik içeriyor; bu alan biçimi henüz "
                         "kaydedilemiyor. Kaynaklar değiştirilmedi.");
    if (!bends(face.outer)) {
        // A STRAIGHT-EDGED PIECE is the polyline it always was, written the way
        // it always was.
        const core::Polygon polygon = face_polygon(face);
        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(
            core::RingGeometry::RingInput{polygon.exterior, core::RingRole::Exterior, 0});
        for (const std::vector<core::Point2>& hole : polygon.holes)
            rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});
        return ctx.transaction().add_area(layer, rings);
    }
    const core::PathRecord rec = core::area_record(face.outer);
    const core::RingGeometry::RingInput ring{rec.ring, rec.role, 0};
    return ctx.transaction().add_kind(layer, rec.kind, {&ring, 1}, rec.payload);
}

} // namespace piricad::command
