// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/ties.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/lineage.hpp"
#include "kentos_cad/core/text_fields.hpp"

#include <algorithm>
#include <map>

namespace kentos::core {
namespace {

/// One ring of `slot`, copied out, and whether it closes.
struct RingOf
{
    std::vector<Point2> points;
    bool closed{false};
    bool ok{false};
};

RingOf ring_of(const RingGeometry& geom, std::uint32_t slot, std::uint16_t ring)
{
    RingOf out;
    const RingSpan rs = geom.rings_of(slot);
    if (ring >= rs.count) return out;
    const std::uint32_t r = rs.first + ring;
    const auto xs         = geom.ring_xs(r);
    const auto ys         = geom.ring_ys(r);
    out.points.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i)
        out.points.push_back(Point2{xs[i], ys[i]});
    out.closed = geom.ring_role[r] != RingRole::Open;
    out.ok     = true;
    return out;
}

bool alive_key(const Document& doc, EntityKey k)
{
    const EntityId e = doc.slot_of(k);
    return e != kNoEntity && doc.alive(e);
}

void sort_once(std::vector<EntityKey>& keys)
{
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
}

std::optional<Tie> caption_tie(const Document& doc, EntityId e)
{
    const Attachment* a = doc.attachments().get(e);
    if (a == nullptr) return std::nullopt;
    Tie t;
    t.dependent = e;
    t.kind      = TieKind::Caption;
    t.sources   = {a->source};
    if (!alive_key(doc, a->source)) {
        t.state = TieState::Broken;
        t.gone  = {a->source};
        return t;
    }
    const std::uint32_t slot = doc.entities().slot[e];
    const TextTable& texts   = doc.texts();
    if (!texts.has(slot)) return t; // only captions follow today, and they are current
    const auto should = caption_follow(doc, e, *a);
    if (!should) {
        // The anchor is not on the source any more — a corner removed behind a
        // lock: the tie names a feature the source does not have.
        t.state = TieState::Broken;
        return t;
    }
    const RingGeometry& geom = doc.geometry();
    const RingSpan rs        = geom.rings_of(slot);
    const bool placed        = rs.count == 1 && geom.ring_count[rs.first] == 2 &&
                        geom.ring_role[rs.first] == RingRole::Open &&
                        geom.vertex(rs.first, 0) == should->base[0] &&
                        geom.vertex(rs.first, 1) == should->base[1];
    if (!placed || should->text != texts.text(slot) || should->anchor != texts.anchor(slot)) {
        t.state   = TieState::Behind;
        t.changed = {a->source};
    }
    return t;
}

std::optional<Tie> dimension_tie(const Document& doc, EntityId e)
{
    const std::vector<DimLink>* links = doc.dimension_links().get(e);
    if (links == nullptr) return std::nullopt;
    Tie t;
    t.dependent              = e;
    t.kind                   = TieKind::Dimension;
    const RingGeometry& geom = doc.geometry();
    const RingSpan span      = geom.rings_of(doc.entities().slot[e]);
    bool behind              = false;
    bool broken              = false;
    for (const DimLink& l : *links) {
        t.sources.push_back(l.source);
        if (l.broken || !alive_key(doc, l.source)) {
            broken = true;
            if (!alive_key(doc, l.source)) t.gone.push_back(l.source);
            continue;
        }
        const auto at = dim_anchor_point(doc, l);
        if (!at || span.count < 2 || l.point >= geom.ring_count[span.first + 1]) {
            broken = true;
            continue;
        }
        if (*at != geom.vertex(span.first + 1, l.point)) {
            behind = true;
            t.changed.push_back(l.source);
        }
    }
    // BEHIND BEFORE BROKEN: a dimension with one link broken still follows the
    // other, and one that has fallen behind is the one a user can bring back.
    t.state = behind ? TieState::Behind : broken ? TieState::Broken : TieState::Current;
    sort_once(t.sources);
    sort_once(t.changed);
    sort_once(t.gone);
    return t;
}

std::optional<Tie> hatch_tie(const Document& doc, EntityId e)
{
    const std::vector<HatchSource>* sources = doc.hatch_links().get(e);
    if (sources == nullptr) return std::nullopt;
    Tie t;
    t.dependent = e;
    t.kind      = TieKind::Hatch;
    bool broken = false;
    for (const HatchSource& s : *sources) {
        t.sources.push_back(s.source);
        if (!alive_key(doc, s.source)) {
            t.gone.push_back(s.source);
            broken = true;
        } else if (s.broken || closed_loops_of(doc, doc.slot_of(s.source)).empty()) {
            broken = true;
        }
    }
    sort_once(t.sources);
    sort_once(t.gone);
    if (broken) {
        t.state = TieState::Broken;
        return t;
    }
    if (!hatch_fills(doc, e, *sources)) {
        t.state   = TieState::Behind;
        t.changed = t.sources;
    }
    return t;
}

Tie result_tie(EntityId e, const Lineage& origin, const ResultCheck& check)
{
    Tie t;
    t.dependent = e;
    t.kind      = TieKind::Result;
    t.sources   = origin.sources;
    t.changed   = check.changed;
    t.gone      = check.gone;
    t.state     = check.state == ResultState::Stale        ? TieState::Behind
                  : check.state == ResultState::Sourceless ? TieState::Sourceless
                                                           : TieState::Current;
    return t;
}

} // namespace

const char* tie_kind_id(TieKind k) noexcept
{
    switch (k) {
    case TieKind::Caption: return "yazi";
    case TieKind::Dimension: return "olcu";
    case TieKind::Hatch: return "tarama";
    case TieKind::Result: return "sonuc";
    }
    return "yazi";
}

const char* tie_state_id(TieState s) noexcept
{
    switch (s) {
    case TieState::Current: return "guncel";
    case TieState::Behind: return "guncel_degil";
    case TieState::Broken: return "kopuk";
    case TieState::Sourceless: return "kaynaksiz";
    }
    return "guncel";
}

std::optional<CaptionFollow> caption_follow(const Document& doc, EntityId e, const Attachment& a)
{
    if (e >= doc.entities().size() || !doc.alive(e)) return std::nullopt;
    const EntityId src = doc.slot_of(a.source);
    if (src == kNoEntity || !doc.alive(src)) return std::nullopt;
    const std::uint32_t slot = doc.entities().slot[e];
    const TextTable& texts   = doc.texts();
    if (!texts.has(slot)) return std::nullopt;
    const RingOf now = ring_of(doc.geometry(), doc.entities().slot[src], a.ring);
    if (!now.ok) return std::nullopt;
    CaptionFollow out;
    out.height       = texts.height(slot);
    const auto place = attach_place(now.points, now.closed, a, out.height, true);
    if (!place) return std::nullopt;
    out.text = std::string(texts.text(slot));
    if (const auto derived = attach_text(now.points, now.closed, a); derived) out.text = *derived;
    // A column emptied leaves the caption blank rather than gone: an empty text
    // detaches the words from the entity, and the label would not come back
    // when the column is filled again.
    if (a.derive == AttachDerive::Fields)
        if (auto filled = fill_fields(
                doc, src, a.format,
                FieldFormat{a.precision, a.separator, static_cast<DrawingUnit>(a.unit)});
            filled)
            out.text = filled.value().empty() ? std::string(" ") : std::move(filled.value());
    out.base = dimension_baseline(place->centre, place->dir_x, place->dir_y, out.height, out.text);
    // A landing's caption changes its alignment with the side it stands on;
    // every other caption keeps the one it has.
    out.anchor = place->anchor.value_or(texts.anchor(slot));
    return out;
}

bool hatch_fills(const Document& doc, EntityId hatch, std::span<const HatchSource> sources)
{
    std::vector<EntityId> live;
    for (const HatchSource& s : sources) {
        const EntityId src = doc.slot_of(s.source);
        if (s.broken || src == kNoEntity || !doc.alive(src)) return false;
        live.push_back(src);
    }
    const RingGeometry& g    = doc.geometry();
    const std::uint32_t slot = doc.entities().slot[hatch];
    auto def                 = hatch_of(g, slot);
    if (!def) return false;
    auto boundary = hatch_boundary(doc, live, def.value().style);
    if (!boundary) return false;
    const RingSpan span = g.rings_of(slot);
    if (span.count != boundary.value().loops.size()) return false;
    for (std::uint32_t r = 0; r < span.count; ++r) {
        const auto xs                   = g.ring_xs(span.first + r);
        const auto ys                   = g.ring_ys(span.first + r);
        const std::vector<Point2>& loop = boundary.value().loops[r];
        if (xs.size() != loop.size() || g.ring_role[span.first + r] != boundary.value().roles[r])
            return false;
        for (std::size_t v = 0; v < xs.size(); ++v)
            if (xs[v] != loop[v].x || ys[v] != loop[v].y) return false;
    }
    return true;
}

std::vector<Tie> ties_of(const Document& doc, EntityId e)
{
    std::vector<Tie> out;
    if (e >= doc.entities().size()) return out;
    if (auto t = caption_tie(doc, e)) out.push_back(std::move(*t));
    if (auto t = dimension_tie(doc, e)) out.push_back(std::move(*t));
    if (auto t = hatch_tie(doc, e)) out.push_back(std::move(*t));
    if (const Lineage* origin = doc.lineage().get(e); origin != nullptr && origin->result())
        out.push_back(result_tie(e, *origin, check_origin(doc, *origin)));
    return out;
}

std::vector<Tie> every_tie(const Document& doc)
{
    std::vector<Tie> out;
    for (const EntityId e : doc.attachments().attached())
        if (doc.alive(e))
            if (auto t = caption_tie(doc, e)) out.push_back(std::move(*t));
    for (const EntityId e : doc.dimension_links().linked())
        if (doc.alive(e))
            if (auto t = dimension_tie(doc, e)) out.push_back(std::move(*t));
    for (const EntityId e : doc.hatch_links().linked())
        if (doc.alive(e))
            if (auto t = hatch_tie(doc, e)) out.push_back(std::move(*t));
    // One comparison per shared origin: thirty contours are one question.
    const LineageTable& lineage = doc.lineage();
    std::map<std::uint32_t, ResultCheck> asked;
    for (const EntityId e : lineage.derived()) {
        if (!doc.alive(e)) continue;
        const std::uint32_t at = lineage.origin_of(e);
        if (!lineage.origin(at).result()) continue;
        auto it = asked.find(at);
        if (it == asked.end()) it = asked.emplace(at, check_origin(doc, lineage.origin(at))).first;
        out.push_back(result_tie(e, lineage.origin(at), it->second));
    }
    std::ranges::stable_sort(out, [](const Tie& a, const Tie& b) {
        return a.dependent != b.dependent ? a.dependent < b.dependent : a.kind < b.kind;
    });
    return out;
}

} // namespace kentos::core
