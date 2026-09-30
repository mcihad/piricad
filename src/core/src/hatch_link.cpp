// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/hatch_link.hpp"

#include "piricad/core/curve_path.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/core/outline.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace piricad::core {
namespace {

// A NEW seed, carrying the program's present name (CLAUDE.md 0.5a froze only
// the seeds already folded into fixtures).
constexpr std::uint64_t kHatchLinkSeed = fnv1a("piricad.core.hatchlink");

// The record as the undo op carries it: the key and the broken flag.
constexpr std::size_t kSourceBytes = 8 + 1;

} // namespace

// ---------------------------------------------------------------- table ----

const std::vector<HatchSource>* HatchLinkTable::get(EntityId hatch) const
{
    const auto it = rows_.find(hatch);
    return it == rows_.end() ? nullptr : &it->second;
}

void HatchLinkTable::set(EntityId hatch, std::vector<HatchSource> sources)
{
    if (sources.empty()) {
        rows_.erase(hatch);
        return;
    }
    rows_[hatch] = std::move(sources);
}

std::vector<EntityId> HatchLinkTable::linked() const
{
    std::vector<EntityId> out;
    out.reserve(rows_.size());
    for (const auto& [hatch, sources] : rows_)
        out.push_back(hatch);
    return out;
}

std::uint64_t HatchLinkTable::fold(std::uint64_t seed,
                                   std::span<const std::uint32_t> position) const
{
    if (rows_.empty()) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_.size()), seed ^ kHatchLinkSeed);
    for (const auto& [hatch, sources] : rows_) {
        h = fnv1a_int(static_cast<std::int64_t>(hatch < position.size() ? position[hatch] : hatch),
                      h);
        h = fnv1a_int(static_cast<std::int64_t>(sources.size()), h);
        for (const HatchSource& s : sources) {
            h = fnv1a_int(static_cast<std::int64_t>(raw(s.source)), h);
            h = fnv1a_int(s.broken ? 1 : 0, h);
        }
    }
    return h;
}

std::vector<std::uint8_t> encode_hatch_links(std::span<const HatchSource> sources)
{
    std::vector<std::uint8_t> out(1 + sources.size() * kSourceBytes);
    out[0]             = 1; // layout version
    std::size_t offset = 1;
    for (const HatchSource& s : sources) {
        const std::uint64_t key = raw(s.source);
        std::memcpy(out.data() + offset, &key, 8);
        out[offset + 8] = s.broken ? 1 : 0;
        offset += kSourceBytes;
    }
    return out;
}

Result<std::vector<HatchSource>> decode_hatch_links(std::span<const std::uint8_t> bytes)
{
    std::vector<HatchSource> out;
    if (bytes.empty()) return out;
    if (bytes[0] != 1 || (bytes.size() - 1) % kSourceBytes != 0)
        return err(ErrorCode::InvalidArgument, "Tarama bağlarının baytları tanınmıyor.");
    for (std::size_t offset = 1; offset < bytes.size(); offset += kSourceBytes) {
        std::uint64_t key = 0;
        std::memcpy(&key, bytes.data() + offset, 8);
        out.push_back(HatchSource{static_cast<EntityKey>(key), bytes[offset + 8] != 0});
    }
    return out;
}

// ------------------------------------------------------------- boundary ----

std::vector<RingGeometry::RingInput> HatchBoundary::rings() const
{
    std::vector<RingGeometry::RingInput> out;
    out.reserve(loops.size());
    for (std::size_t i = 0; i < loops.size(); ++i)
        out.push_back(RingGeometry::RingInput{loops[i], roles[i], parts[i]});
    return out;
}

std::vector<std::vector<Point2>> closed_loops_of(const Document& doc, EntityId e)
{
    std::vector<std::vector<Point2>> out;
    const RingGeometry& g = doc.geometry();
    EmitBuffer runs;
    if (!entity_outline(doc, e, runs)) {
        // A face or a polyline: its closed rings as they are stored.
        const RingSpan span = g.rings_of(doc.entities().slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            if (g.ring_role[r] == RingRole::Open) continue;
            const auto xs = g.ring_xs(r);
            const auto ys = g.ring_ys(r);
            std::vector<Point2> pts;
            pts.reserve(xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v)
                pts.push_back(Point2{xs[v], ys[v]});
            if (pts.size() >= 3) out.push_back(std::move(pts));
        }
        return out;
    }
    // A curve: every closed run of what it draws.
    for (std::size_t r = 0; r < runs.run_total(); ++r) {
        if (runs.run_closed[r] == 0) continue;
        const auto xs = runs.run_xs(r);
        const auto ys = runs.run_ys(r);
        std::vector<Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(Point2{xs[v], ys[v]});
        if (pts.size() > 1 && pts.front() == pts.back()) pts.pop_back();
        if (pts.size() >= 3) out.push_back(std::move(pts));
    }
    return out;
}

namespace {

double area_of(const std::vector<Point2>& loop)
{
    double a2 = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point2& p = loop[i];
        const Point2& q = loop[(i + 1) % loop.size()];
        a2 += static_cast<double>(p.x - loop[0].x) * static_cast<double>(q.y - loop[0].y) -
              static_cast<double>(q.x - loop[0].x) * static_cast<double>(p.y - loop[0].y);
    }
    return std::fabs(a2) / 2.0;
}

} // namespace

Result<HatchBoundary> hatch_boundary(const Document& doc, std::span<const EntityId> sources,
                                     std::uint16_t style)
{
    std::vector<std::vector<Point2>> all;
    for (const EntityId e : sources) {
        if (e >= doc.entities().size() || !doc.alive(e)) continue;
        for (std::vector<Point2>& loop : closed_loops_of(doc, e))
            all.push_back(std::move(loop));
    }
    if (all.empty())
        return err(ErrorCode::InvalidArgument,
                   "Tarama sınırı kapalı bir şey çevrelemiyor: kapalı bir alan, daire, elips ya "
                   "da kapalı çoklu çizgi gerekir.");
    return nest_loops(std::move(all), style);
}

namespace {

/// A closed path of segments through `ring`.
CurvePath ring_path(std::span<const Point2> ring)
{
    CurvePath p;
    p.closed = true;
    p.pieces.reserve(ring.size());
    for (std::size_t i = 0; i < ring.size(); ++i)
        p.pieces.push_back(
            PathPiece{.from = ring[i], .to = ring[i + 1 == ring.size() ? 0 : i + 1]});
    return p;
}

/// A path as the points a hatch holds: segments as they are, arcs by the
/// routine a YAY is drawn with, the closing vertex not repeated.
std::vector<Point2> loop_points(const CurvePath& path)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    path_outline(path, xs, ys);
    std::vector<Point2> out;
    out.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i)
        out.push_back(Point2{xs[i], ys[i]});
    if (out.size() > 1 && out.front() == out.back()) out.pop_back();
    return out;
}

/// The OPEN runs an object draws, as paths of segments — what a strip is grown
/// from when the object has no path of its own: an ellipse's arc, a spline, a
/// dimension's lines.
std::vector<CurvePath> open_runs_of(const Document& doc, EntityId e)
{
    std::vector<CurvePath> out;
    const auto take = [&out](std::span<const Mm> xs, std::span<const Mm> ys) {
        if (xs.size() < 2) return;
        CurvePath p;
        p.pieces.reserve(xs.size() - 1);
        for (std::size_t v = 0; v + 1 < xs.size(); ++v)
            p.pieces.push_back(
                PathPiece{.from = Point2{xs[v], ys[v]}, .to = Point2{xs[v + 1], ys[v + 1]}});
        out.push_back(std::move(p));
    };
    const RingGeometry& g = doc.geometry();
    EmitBuffer runs;
    if (!entity_outline(doc, e, runs)) {
        const RingSpan span = g.rings_of(doc.entities().slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            if (g.ring_role[r] == RingRole::Open) take(g.ring_xs(r), g.ring_ys(r));
        return out;
    }
    for (std::size_t r = 0; r < runs.run_total(); ++r)
        if (runs.run_closed[r] == 0) take(runs.run_xs(r), runs.run_ys(r));
    return out;
}

/// `paths` moved `margin` off themselves — a closed one outward, an open one to
/// a band round it — and made faces again. Square corners: what is left free
/// round a caption is a box, as the caption's own is.
Result<std::vector<KernelFace>> grown(std::span<const CurvePath> paths, Mm margin, bool open)
{
    std::vector<CurvePath> rings;
    for (const CurvePath& p : paths) {
        auto moved = kernel_offset(p, margin, OffsetCorner::Sharp, open);
        if (!moved) return moved.error();
        for (CurvePath& q : moved.value())
            rings.push_back(std::move(q));
    }
    return kernel_faces_of(std::move(rings));
}

} // namespace

Result<std::vector<KernelFace>> hatch_cutout(const Document& doc, EntityId e, Mm margin)
{
    if (e >= doc.entities().size() || !doc.alive(e) || margin < 0) return std::vector<KernelFace>{};
    if (!kernel_available())
        return err(ErrorCode::Unsupported,
                   "disarida= geometri çekirdeğini ister; bu yapıda OpenCASCADE yok (" +
                       kernel_version() + ").");
    const KindId kind = doc.entities().kind[e];

    // A BLOCK OR A POINT KEEPS ITS BOX: the mask a symbol is left in, grown by
    // the margin in whole millimetres. A point's box is a point, so only the
    // margin gives it any ground.
    if (kind == kBlockReferenceKind || kind == kPointKind) {
        const Box2 b = doc.entities().box_of(e);
        if (b.max_x - b.min_x + (2 * margin) <= 0 || b.max_y - b.min_y + (2 * margin) <= 0)
            return std::vector<KernelFace>{};
        const std::array<Point2, 4> box{
            Point2{b.min_x - margin, b.min_y - margin}, Point2{b.max_x + margin, b.min_y - margin},
            Point2{b.max_x + margin, b.max_y + margin}, Point2{b.min_x - margin, b.max_y + margin}};
        return std::vector<KernelFace>{KernelFace{ring_path(box), {}}};
    }

    // A CAPTION: its letters, not its hairline.
    if (std::array<Point2, 4> quad{}; text_quad(doc, e, quad)) {
        const CurvePath path = ring_path(quad);
        if (margin == 0) return std::vector<KernelFace>{KernelFace{path, {}}};
        return grown(std::span<const CurvePath>(&path, 1), margin, false);
    }

    // A CLOSED OBJECT: its face, a circle's a circle.
    if (const auto path = path_of(doc, e, PathScope::Circular); path && path->closed) {
        if (margin == 0) return std::vector<KernelFace>{KernelFace{*path, {}}};
        return grown(std::span<const CurvePath>(&*path, 1), margin, false);
    }
    if (const auto loops = closed_loops_of(doc, e); !loops.empty()) {
        // One with holes: its rings nested into faces, and with a margin each
        // boundary grown and each hole shrunk by it.
        std::vector<CurvePath> rings;
        rings.reserve(loops.size());
        for (const auto& loop : loops)
            rings.push_back(ring_path(loop));
        std::vector<KernelFace> faces = kernel_faces_of(std::move(rings));
        if (margin == 0) return faces;
        std::vector<CurvePath> moved;
        for (const KernelFace& face : faces) {
            auto out = kernel_offset(face.outer, margin, OffsetCorner::Sharp, false);
            if (!out) return out.error();
            for (CurvePath& q : out.value())
                moved.push_back(std::move(q));
            for (const CurvePath& hole : face.holes) {
                auto in = kernel_offset(hole, -margin, OffsetCorner::Sharp, false);
                if (!in) return in.error();
                for (CurvePath& q : in.value())
                    moved.push_back(std::move(q));
            }
        }
        return kernel_faces_of(std::move(moved));
    }

    // AN OPEN LINE keeps ground only with a margin: a band that wide on either
    // side, round at its ends.
    if (margin == 0) return std::vector<KernelFace>{};
    std::vector<CurvePath> runs;
    if (auto path = path_of(doc, e, PathScope::Circular); path)
        runs.push_back(std::move(*path));
    else
        runs = open_runs_of(doc, e);
    return grown(runs, margin, true);
}

Result<HatchCut> hatch_without(const HatchBoundary& boundary,
                               std::span<const std::vector<KernelFace>> cutouts)
{
    std::vector<CurvePath> rings;
    rings.reserve(boundary.loops.size());
    for (const auto& loop : boundary.loops)
        rings.push_back(ring_path(loop));
    const std::vector<KernelFace> subject = kernel_faces_of(std::move(rings));

    HatchCut out;
    std::vector<KernelFace> clip;
    for (std::size_t i = 0; i < cutouts.size(); ++i) {
        // IDLE: a cutout that shares no ground with the hatch. Counted, so the
        // sentence can say which of the objects shown changed nothing.
        auto common = kernel_boolean(subject, cutouts[i], BooleanOp::Intersection);
        if (!common) return common.error();
        if (common.value().empty()) {
            out.idle.push_back(i);
            continue;
        }
        clip.insert(clip.end(), cutouts[i].begin(), cutouts[i].end());
    }
    if (clip.empty()) {
        out.boundary = boundary; // nothing touched: the hatch it would have been
        return out;
    }
    // Every cutout a tool of one cut, so two that overlap take one hole out.
    auto left = kernel_boolean(subject, clip, BooleanOp::Difference);
    if (!left) return left.error();
    if (left.value().empty())
        return err(ErrorCode::InvalidArgument,
                   "disarida= nesneleri taranacak yerin tamamını kaplıyor; taranacak yer kalmadı.");
    for (std::size_t part = 0; part < left.value().size(); ++part) {
        const KernelFace& face = left.value()[part];
        out.boundary.loops.push_back(loop_points(face.outer));
        out.boundary.roles.push_back(RingRole::Exterior);
        out.boundary.parts.push_back(static_cast<std::uint16_t>(part));
        for (const CurvePath& hole : face.holes) {
            out.boundary.loops.push_back(loop_points(hole));
            out.boundary.roles.push_back(RingRole::Interior);
            out.boundary.parts.push_back(static_cast<std::uint16_t>(part));
        }
    }
    return out;
}

HatchBoundary nest_loops(std::vector<std::vector<Point2>> all, std::uint16_t style)
{
    // HOW DEEP EACH LOOP LIES: how many of the others contain it, and which of
    // those is the smallest — its immediate container. Only a larger loop can
    // hold a smaller one, so two copies of one loop never hold each other.
    const std::size_t n = all.size();
    std::vector<double> area(n);
    std::vector<std::vector<Mm>> xs(n);
    std::vector<std::vector<Mm>> ys(n);
    for (std::size_t i = 0; i < n; ++i) {
        area[i] = area_of(all[i]);
        xs[i].reserve(all[i].size());
        ys[i].reserve(all[i].size());
        for (const Point2 p : all[i]) {
            xs[i].push_back(p.x);
            ys[i].push_back(p.y);
        }
    }
    constexpr auto kNone = static_cast<std::size_t>(-1);
    std::vector<std::size_t> depth(n, 0);
    std::vector<std::size_t> parent(n, kNone);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j || !(area[j] > area[i])) continue;
            if (!ring_contains(xs[j], ys[j], all[i].front())) continue;
            ++depth[i];
            if (parent[i] == kNone || area[j] < area[parent[i]]) parent[i] = j;
        }

    // THE STYLE DECIDES WHICH DEPTHS ARE KEPT, and nesting decides the rest: an
    // even depth is a face of its own, an odd one a hole in its container.
    std::size_t deepest = kNone; // normal: every depth
    if (style == 1) deepest = 1; // outermost: faces and their first holes
    if (style == 2) deepest = 0; // ignore: the outer faces alone
    HatchBoundary out;
    std::vector<std::size_t> part_of(n, kNone);
    std::uint16_t next_part = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (depth[i] > deepest || depth[i] % 2 != 0) continue;
        part_of[i] = next_part++;
        out.loops.push_back(all[i]);
        out.roles.push_back(RingRole::Exterior);
        out.parts.push_back(static_cast<std::uint16_t>(part_of[i]));
        for (std::size_t k = 0; k < n; ++k) {
            if (parent[k] != i || depth[k] > deepest || depth[k] % 2 == 0) continue;
            out.loops.push_back(all[k]);
            out.roles.push_back(RingRole::Interior);
            out.parts.push_back(static_cast<std::uint16_t>(part_of[i]));
        }
    }
    return out;
}

} // namespace piricad::core
