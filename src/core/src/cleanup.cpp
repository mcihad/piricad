// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/cleanup.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/text_store.hpp"

#include <algorithm>
#include <map>

namespace kentos::core {
namespace {

Mm chebyshev(Point2 a, Point2 b) noexcept
{
    const Mm dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const Mm dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

std::vector<Point2> ring_points(const RingGeometry& geom, std::uint32_t r)
{
    const auto xs = geom.ring_xs(r);
    const auto ys = geom.ring_ys(r);
    std::vector<Point2> out;
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(Point2{xs[v], ys[v]});
    return out;
}

/// A polyline that is only its vertices: no kind payload, no caption. Its
/// rings may be read in either direction and from any corner and still be the
/// same object; anything else is compared exactly as stored.
bool plain_polyline(const Document& doc, EntityId e)
{
    const std::uint32_t slot = doc.entities().slot[e];
    return doc.entities().kind[e] == kPolylineKind && !doc.texts().has(slot) &&
           doc.geometry().payload_of(slot).empty();
}

/// The ring written the same way whichever way round and from whichever corner
/// it was drawn: an open run from its lesser end, a closed ring
/// counter-clockwise from its lowest-left corner.
std::vector<Point2> canonical(std::vector<Point2> ring, bool closed)
{
    if (ring.size() < 2) return ring;
    if (!closed) {
        const std::vector<Point2> back(ring.rbegin(), ring.rend());
        return back < ring ? back : ring;
    }
    if (ring_area(ring) < 0) std::ranges::reverse(ring);
    std::ranges::rotate(ring, std::ranges::min_element(ring));
    return ring;
}

/// What two copies share: kind, layer, rings — canonical for a plain polyline,
/// as stored otherwise — payload and caption.
std::vector<std::int64_t> signature(const Document& doc, EntityId e)
{
    const EntityTable& ents  = doc.entities();
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];
    const bool plain         = plain_polyline(doc, e);
    std::vector<std::int64_t> key{static_cast<std::int64_t>(ents.kind[e]),
                                  static_cast<std::int64_t>(ents.layer[e])};

    const RingSpan span = geom.rings_of(slot);
    std::vector<std::vector<std::int64_t>> rings;
    rings.reserve(span.count);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        std::vector<Point2> pts = ring_points(geom, r);
        const RingRole role     = geom.ring_role[r];
        if (plain) pts = canonical(std::move(pts), role != RingRole::Open);
        std::vector<std::int64_t> one{static_cast<std::int64_t>(role),
                                      static_cast<std::int64_t>(geom.ring_part[r]),
                                      static_cast<std::int64_t>(pts.size())};
        one.reserve(3 + 2 * pts.size());
        for (const Point2 p : pts) {
            one.push_back(p.x);
            one.push_back(p.y);
        }
        rings.push_back(std::move(one));
    }
    // A plain face's rings in any order are the same face.
    if (plain) std::ranges::sort(rings);
    for (const std::vector<std::int64_t>& ring : rings)
        key.insert(key.end(), ring.begin(), ring.end());

    key.push_back(-1);
    for (const std::uint8_t b : geom.payload_of(slot))
        key.push_back(b);
    key.push_back(-2);
    if (doc.texts().has(slot)) {
        for (const char c : doc.texts().text(slot))
            key.push_back(static_cast<unsigned char>(c));
        key.push_back(doc.texts().height(slot));
        key.push_back(static_cast<std::int64_t>(doc.texts().anchor(slot)));
    }
    return key;
}

/// Whether `e` draws nothing: a polyline whose every vertex is within the
/// tolerance of its first, or a face of no area. A caption never is — its
/// words are what it is — and neither is a point, a curve or anything with a
/// payload, whose vertices are a definition and not a drawing.
bool draws_nothing(const Document& doc, EntityId e, Mm tolerance)
{
    if (!plain_polyline(doc, e)) return false;
    const RingGeometry& geom = doc.geometry();
    const RingSpan span      = geom.rings_of(doc.entities().slot[e]);
    if (span.count == 0) return true;
    bool closed = false;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
        closed = closed || geom.ring_role[r] != RingRole::Open;
    if (closed) return doc.entity_area(e) == 0;
    const std::vector<Point2> first = ring_points(geom, span.first);
    if (first.empty()) return true;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
        for (const Point2 p : ring_points(geom, r))
            if (chebyshev(p, first.front()) > tolerance) return false;
    return true;
}

Point2 first_vertex(const Document& doc, EntityId e)
{
    const RingGeometry& geom = doc.geometry();
    const RingSpan span      = geom.rings_of(doc.entities().slot[e]);
    if (span.count == 0 || geom.ring_count[span.first] == 0) return Point2{};
    return Point2{geom.ring_xs(span.first)[0], geom.ring_ys(span.first)[0]};
}

} // namespace

std::optional<std::vector<RepairedRing>> rings_without_repeats(const Document& doc, EntityId e,
                                                               Mm tolerance)
{
    if (e >= doc.entities().size() || !doc.alive(e) || !plain_polyline(doc, e)) return std::nullopt;
    const RingGeometry& geom = doc.geometry();
    const RingSpan span      = geom.rings_of(doc.entities().slot[e]);
    std::vector<RepairedRing> out;
    bool removed = false;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const std::vector<Point2> pts = ring_points(geom, r);
        RepairedRing ring;
        ring.role = geom.ring_role[r];
        ring.part = geom.ring_part[r];
        for (const Point2 p : pts) {
            if (!ring.points.empty() && chebyshev(p, ring.points.back()) <= tolerance) continue;
            ring.points.push_back(p);
        }
        const bool closed = ring.role != RingRole::Open;
        // A closed ring's last corner is next to its first.
        if (closed && ring.points.size() > 1 &&
            chebyshev(ring.points.back(), ring.points.front()) <= tolerance)
            ring.points.pop_back();
        if (ring.points.size() < (closed ? 3U : 2U)) return std::nullopt;
        removed = removed || ring.points.size() != pts.size();
        out.push_back(std::move(ring));
    }
    if (!removed) return std::nullopt;
    return out;
}

// ROWS ARE GEOMETRY SLOTS, not entity ids (Document::carry_attributes): a
// geometry edit moves an entity to a new slot and its row moves with it.

bool has_attributes(const Document& doc, EntityId e)
{
    const AttrTable& table = doc.attributes();
    if (e >= doc.entities().size()) return false;
    const std::uint32_t row = doc.entities().slot[e];
    if (row >= table.rows()) return false;
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto v = table.get(static_cast<AttrId>(c), row);
        if (v && v.value().present) return true;
    }
    return false;
}

bool same_attributes(const Document& doc, EntityId a, EntityId b)
{
    const AttrTable& table = doc.attributes();
    const auto cell        = [&](AttrId c, EntityId e) {
        if (e >= doc.entities().size()) return AttrValue{};
        const std::uint32_t row = doc.entities().slot[e];
        if (row >= table.rows()) return AttrValue{};
        const auto v = table.get(c, row);
        return v ? v.value() : AttrValue{};
    };
    for (std::size_t c = 0; c < table.columns(); ++c)
        if (!(cell(static_cast<AttrId>(c), a) == cell(static_cast<AttrId>(c), b))) return false;
    return true;
}

std::vector<Redundancy> find_redundant(const Document& doc, std::span<const EntityId> slots,
                                       Mm tolerance)
{
    const EntityTable& ents = doc.entities();
    // THE SCOPE SAYS WHAT IS REPORTED, NOT WHAT A COPY IS COMPARED WITH. The
    // twin of a selected copy may be anywhere in the drawing — and the find
    // mode of TEMİZLE selects the copies and not their originals, so a scope
    // that limited the comparison would find nothing the second time.
    std::vector<char> in_scope;
    if (!slots.empty()) {
        in_scope.assign(ents.size(), 0);
        for (const EntityId e : slots)
            if (e < ents.size()) in_scope[e] = 1;
    }

    std::vector<Redundancy> out;
    std::map<std::vector<std::int64_t>, EntityId> seen;
    for (EntityId e = 0; e < ents.size(); ++e) {
        if (!ents.standalone(e)) continue;
        const bool reported = in_scope.empty() || in_scope[e] != 0;
        // An empty object is not also a duplicate, nor anybody's original: it
        // goes for being empty.
        if (draws_nothing(doc, e, tolerance)) {
            if (reported)
                out.push_back(
                    Redundancy{RedundancyKind::Empty, e, kNoEntity, 0, first_vertex(doc, e)});
            continue;
        }
        const auto [it, fresh] = seen.try_emplace(signature(doc, e), e);
        if (!reported) continue;
        if (!fresh) {
            out.push_back(
                Redundancy{RedundancyKind::Duplicate, e, it->second, 0, first_vertex(doc, e)});
            continue;
        }
        if (auto fixed = rings_without_repeats(doc, e, tolerance)) {
            std::size_t before  = 0;
            const RingSpan span = doc.geometry().rings_of(ents.slot[e]);
            for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
                before += doc.geometry().ring_count[r];
            std::size_t after = 0;
            for (const RepairedRing& ring : *fixed)
                after += ring.points.size();
            out.push_back(Redundancy{RedundancyKind::RepeatedVertex, e, kNoEntity, before - after,
                                     first_vertex(doc, e)});
        }
    }
    return out;
}

} // namespace kentos::core
