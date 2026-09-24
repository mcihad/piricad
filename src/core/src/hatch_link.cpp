// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/hatch_link.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace kentos::core {
namespace {

// A NEW seed, carrying the program's present name (CLAUDE.md 0.5a froze only
// the seeds already folded into fixtures).
constexpr std::uint64_t kHatchLinkSeed = fnv1a("kentos.core.hatchlink");

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

std::uint64_t HatchLinkTable::fold(std::uint64_t seed) const
{
    if (rows_.empty()) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_.size()), seed ^ kHatchLinkSeed);
    for (const auto& [hatch, sources] : rows_) {
        h = fnv1a_int(static_cast<std::int64_t>(hatch), h);
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

} // namespace kentos::core
