// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/dimension_link.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/spatial_index.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace kentos::core {
namespace {

// A NEW seed, so it carries the program's present name (CLAUDE.md 0.5a froze the
// seeds that were already folded into fixtures, not the ones still to come).
constexpr std::uint64_t kDimLinkSeed = fnv1a("kentos.core.dimlink");

// The record as the undo op carries it: every field, fixed width.
constexpr std::size_t kLinkBytes = 1 + 1 + 2 + 4 + 8 + 1;

} // namespace

// ---------------------------------------------------------------- table ----

const std::vector<DimLink>* DimLinkTable::get(EntityId dim) const
{
    const auto it = rows_.find(dim);
    return it == rows_.end() ? nullptr : &it->second;
}

void DimLinkTable::set(EntityId dim, std::vector<DimLink> links)
{
    if (links.empty()) {
        rows_.erase(dim);
        return;
    }
    rows_[dim] = std::move(links);
}

std::vector<EntityId> DimLinkTable::linked() const
{
    std::vector<EntityId> out;
    out.reserve(rows_.size());
    for (const auto& [dim, links] : rows_)
        out.push_back(dim);
    return out;
}

std::uint64_t DimLinkTable::fold(std::uint64_t seed) const
{
    if (rows_.empty()) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_.size()), seed ^ kDimLinkSeed);
    for (const auto& [dim, links] : rows_) {
        h = fnv1a_int(static_cast<std::int64_t>(dim), h);
        h = fnv1a_int(static_cast<std::int64_t>(links.size()), h);
        for (const DimLink& l : links) {
            h = fnv1a_int(static_cast<std::int64_t>(l.point), h);
            h = fnv1a_int(static_cast<std::int64_t>(l.anchor), h);
            h = fnv1a_int(static_cast<std::int64_t>(l.ring), h);
            h = fnv1a_int(static_cast<std::int64_t>(l.index), h);
            h = fnv1a_int(static_cast<std::int64_t>(raw(l.source)), h);
            h = fnv1a_int(l.broken ? 1 : 0, h);
        }
    }
    return h;
}

std::vector<std::uint8_t> encode_dim_links(std::span<const DimLink> links)
{
    std::vector<std::uint8_t> out(1 + links.size() * kLinkBytes);
    out[0]             = 1; // layout version
    std::size_t offset = 1;
    for (const DimLink& l : links) {
        const std::uint64_t key = raw(l.source);
        out[offset]             = l.point;
        out[offset + 1]         = static_cast<std::uint8_t>(l.anchor);
        std::memcpy(out.data() + offset + 2, &l.ring, 2);
        std::memcpy(out.data() + offset + 4, &l.index, 4);
        std::memcpy(out.data() + offset + 8, &key, 8);
        out[offset + 16] = l.broken ? 1 : 0;
        offset += kLinkBytes;
    }
    return out;
}

Result<std::vector<DimLink>> decode_dim_links(std::span<const std::uint8_t> bytes)
{
    std::vector<DimLink> out;
    if (bytes.empty()) return out;
    if (bytes[0] != 1 || (bytes.size() - 1) % kLinkBytes != 0)
        return err(ErrorCode::InvalidArgument, "Ölçü bağlarının baytları tanınmıyor.");
    for (std::size_t offset = 1; offset < bytes.size(); offset += kLinkBytes) {
        DimLink l;
        l.point = bytes[offset];
        if (bytes[offset + 1] > static_cast<std::uint8_t>(DimAnchor::OnCircle))
            return err(ErrorCode::InvalidArgument, "Ölçü bağlarının baytları tanınmıyor.");
        l.anchor          = static_cast<DimAnchor>(bytes[offset + 1]);
        std::uint64_t key = 0;
        std::memcpy(&l.ring, bytes.data() + offset + 2, 2);
        std::memcpy(&l.index, bytes.data() + offset + 4, 4);
        std::memcpy(&key, bytes.data() + offset + 8, 8);
        l.source = static_cast<EntityKey>(key);
        l.broken = bytes[offset + 16] != 0;
        out.push_back(l);
    }
    return out;
}

// ---------------------------------------------------------------- roles ----

std::vector<std::optional<DimRole>> dim_roles(DimensionType type)
{
    using R = std::optional<DimRole>;
    switch (type) {
    case DimensionType::Linear:
    case DimensionType::Aligned: return {R{DimRole::Point}, R{DimRole::Point}, std::nullopt};
    case DimensionType::Angular:
        return {R{DimRole::Point}, R{DimRole::Point}, R{DimRole::Point}, R{DimRole::Point},
                std::nullopt};
    case DimensionType::Diametric: return {R{DimRole::OnCircle}, R{DimRole::OnCircle}};
    case DimensionType::Radial: return {R{DimRole::Centre}, R{DimRole::OnCircle}};
    case DimensionType::Angular3P:
        return {R{DimRole::Point}, R{DimRole::Point}, R{DimRole::Point}, std::nullopt};
    case DimensionType::Ordinate: return {R{DimRole::Point}, R{DimRole::Point}, std::nullopt};
    case DimensionType::ArcLength:
        return {R{DimRole::Centre}, R{DimRole::ArcStart}, R{DimRole::ArcEnd}, std::nullopt};
    }
    return {};
}

// --------------------------------------------------------------- lookup ----

namespace {

/// Every visible standalone entity whose box meets `box`, oldest first: the
/// index and the tail it has not taken in yet, the way a pick gathers.
std::vector<EntityId> near(const Document& doc, const Box2& box)
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
    std::erase_if(out, [&ents](EntityId e) { return !ents.visible(e); });
    return out;
}

/// The circle a circle or an arc lies on: its centre and radius.
bool circle_of(const Document& doc, EntityId e, Point2& centre, Mm& radius)
{
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = doc.entities().slot[e];
    const KindId kind        = doc.entities().kind[e];
    if (kind == kCircleKind) {
        centre = circle_centre_of(geom, slot);
        radius = circle_radius_of(geom, slot);
        return radius > 0;
    }
    if (kind == kArcKind) {
        centre = arc_centre_of(geom, slot);
        radius = arc_radius_of(geom, slot);
        return radius > 0;
    }
    return false;
}

std::uint32_t angle_of(Point2 centre, Point2 p)
{
    std::int64_t a = atan2_udeg(p.y - centre.y, p.x - centre.x);
    a %= kUDegFullCircle;
    if (a < 0) a += kUDegFullCircle;
    return static_cast<std::uint32_t>(a);
}

} // namespace

std::optional<DimLink> dim_anchor_at(const Document& doc, Point2 p, DimRole role, EntityId exclude,
                                     std::span<const EntityId> among)
{
    const EntityTable& ents  = doc.entities();
    const RingGeometry& geom = doc.geometry();
    const Box2 box{p.x - 1, p.y - 1, p.x + 1, p.y + 1};
    for (const EntityId e : near(doc, box)) {
        if (e == exclude) continue;
        if (!among.empty() && !std::ranges::binary_search(among, e)) continue;
        const KindId kind        = ents.kind[e];
        const std::uint32_t slot = ents.slot[e];
        DimLink link;
        link.source = ents.key[e];

        if (role == DimRole::Point &&
            (kind == kPolylineKind || kind == kArcPolylineKind || kind == kPointKind) &&
            !doc.texts().has(slot)) {
            const RingSpan span = geom.rings_of(slot);
            for (std::uint32_t r = 0; r < span.count; ++r) {
                const auto xs = geom.ring_xs(span.first + r);
                const auto ys = geom.ring_ys(span.first + r);
                for (std::size_t v = 0; v < xs.size(); ++v) {
                    if (xs[v] != p.x || ys[v] != p.y) continue;
                    link.anchor = DimAnchor::Vertex;
                    link.ring   = static_cast<std::uint16_t>(r);
                    link.index  = static_cast<std::uint32_t>(v);
                    return link;
                }
            }
            continue;
        }
        if (kind == kArcKind && (role == DimRole::Point || role == DimRole::ArcStart) &&
            arc_start_of(geom, slot) == p) {
            link.anchor = DimAnchor::ArcStart;
            return link;
        }
        if (kind == kArcKind && (role == DimRole::Point || role == DimRole::ArcEnd) &&
            arc_end_of(geom, slot) == p) {
            link.anchor = DimAnchor::ArcEnd;
            return link;
        }
        Point2 centre{};
        Mm radius = 0;
        if (!circle_of(doc, e, centre, radius)) continue;
        if ((role == DimRole::Point || role == DimRole::Centre) && centre == p) {
            link.anchor = DimAnchor::Centre;
            return link;
        }
        if (role == DimRole::OnCircle) {
            // ON the circle to the millimetre the drawing was rounded to: a point
            // taken on a circle is rounded once and lies within half of one.
            const auto dx = static_cast<double>(p.x - centre.x);
            const auto dy = static_cast<double>(p.y - centre.y);
            if (std::fabs(std::sqrt(dx * dx + dy * dy) - static_cast<double>(radius)) > 1.0)
                continue;
            link.anchor = DimAnchor::OnCircle;
            link.index  = angle_of(centre, p);
            return link;
        }
    }
    return std::nullopt;
}

std::optional<Point2> dim_anchor_point(const Document& doc, const DimLink& link)
{
    const EntityId e = doc.slot_of(link.source);
    if (e == kNoEntity || !doc.alive(e)) return std::nullopt;
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = doc.entities().slot[e];
    const KindId kind        = doc.entities().kind[e];
    switch (link.anchor) {
    case DimAnchor::Vertex: {
        const RingSpan span = geom.rings_of(slot);
        if (link.ring >= span.count) return std::nullopt;
        const std::uint32_t r = span.first + link.ring;
        if (link.index >= geom.ring_count[r]) return std::nullopt;
        return geom.vertex(r, link.index);
    }
    case DimAnchor::ArcStart:
        if (kind != kArcKind) return std::nullopt;
        return arc_start_of(geom, slot);
    case DimAnchor::ArcEnd:
        if (kind != kArcKind) return std::nullopt;
        return arc_end_of(geom, slot);
    case DimAnchor::Centre:
    case DimAnchor::OnCircle: {
        Point2 centre{};
        Mm radius = 0;
        if (!circle_of(doc, e, centre, radius)) return std::nullopt;
        if (link.anchor == DimAnchor::Centre) return centre;
        const SinCos t = sin_cos_udeg(static_cast<std::int64_t>(link.index));
        const auto r   = static_cast<double>(radius);
        return Point2{centre.x + mm_round(r * t.cos), centre.y + mm_round(r * t.sin)};
    }
    }
    return std::nullopt;
}

std::optional<DimCurve> dim_curve_at(const Document& doc, Point2 p, Mm reach, bool arcs_only)
{
    if (reach < 0) reach = 0;
    const RingGeometry& geom = doc.geometry();
    const EntityTable& ents  = doc.entities();
    std::optional<DimCurve> best;
    double best_off = 0.0;
    for (const EntityId e : near(doc, Box2{p.x - reach, p.y - reach, p.x + reach, p.y + reach})) {
        const KindId kind = ents.kind[e];
        if (kind != kArcKind && (arcs_only || kind != kCircleKind)) continue;
        const std::uint32_t slot = ents.slot[e];
        DimCurve curve;
        curve.entity = e;
        curve.arc    = kind == kArcKind;
        if (!circle_of(doc, e, curve.centre, curve.radius)) continue;
        if (curve.arc) {
            curve.start = arc_start_of(geom, slot);
            curve.end   = arc_end_of(geom, slot);
            // ON THE ARC, not on the rest of its circle: a click on the gap an
            // arc leaves names nothing.
            if (!on_arc(curve.centre, curve.start, curve.end, p)) continue;
        }
        const auto dx = static_cast<double>(p.x - curve.centre.x);
        const auto dy = static_cast<double>(p.y - curve.centre.y);
        const double off =
            std::abs(std::sqrt((dx * dx) + (dy * dy)) - static_cast<double>(curve.radius));
        if (off > static_cast<double>(reach)) continue;
        if (!best || off < best_off) {
            best     = curve;
            best_off = off;
        }
    }
    return best;
}

} // namespace kentos::core
