// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/attach.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::core {
namespace {

constexpr std::uint64_t kAttachSeed = fnv1a("piricad.core.attach");

struct Dir
{
    double x{0.0};
    double y{0.0};
};

Dir unit_between(Point2 from, Point2 to)
{
    const auto dx  = static_cast<double>(to.x - from.x);
    const auto dy  = static_cast<double>(to.y - from.y);
    const double l = std::sqrt(dx * dx + dy * dy);
    if (l == 0.0) return Dir{};
    return Dir{dx / l, dy / l};
}

/// `u` turned to read left to right, the way `dimension_baseline` will lay it.
Dir reading(Dir u)
{
    if (u.x < 0.0 || (u.x == 0.0 && u.y < 0.0)) return Dir{-u.x, -u.y};
    return u;
}

/// Twice the signed area of the ring: positive when it turns counter-clockwise.
/// Translated to the first vertex so a TUREF coordinate does not overflow.
double twice_area(std::span<const Point2> v)
{
    if (v.size() < 3) return 0.0;
    double twice = 0.0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const Point2 a = v[i];
        const Point2 b = v[(i + 1) % v.size()];
        twice += static_cast<double>(a.x - v[0].x) * static_cast<double>(b.y - v[0].y) -
                 static_cast<double>(b.x - v[0].x) * static_cast<double>(a.y - v[0].y);
    }
    return twice;
}

/// Exact squared distance between two points, in mm², on 128 bits.
Int128 distance2(Point2 a, Point2 b) noexcept
{
    const Int128 dx = static_cast<Int128>(a.x) - static_cast<Int128>(b.x);
    const Int128 dy = static_cast<Int128>(a.y) - static_cast<Int128>(b.y);
    return dx * dx + dy * dy;
}

std::size_t segment_count(std::size_t n, bool closed) noexcept
{
    if (n < 2) return 0;
    return closed ? n : n - 1;
}

} // namespace

const char* attach_anchor_name(AttachAnchor a) noexcept
{
    switch (a) {
    case AttachAnchor::Vertex: return "kose";
    case AttachAnchor::Edge: return "kenar";
    case AttachAnchor::Centre: return "merkez";
    case AttachAnchor::Landing: return "uc";
    }
    return "kenar";
}

const char* attach_side_name(AttachSide s) noexcept
{
    switch (s) {
    case AttachSide::Outside: return "dis";
    case AttachSide::Inside: return "ic";
    case AttachSide::Left: return "sol";
    case AttachSide::Right: return "sag";
    }
    return "dis";
}

const char* attach_derive_name(AttachDerive d) noexcept
{
    switch (d) {
    case AttachDerive::Keep: return "sabit";
    case AttachDerive::Length: return "uzunluk";
    case AttachDerive::Fields: return "bicim";
    }
    return "sabit";
}

std::optional<AttachSide> attach_side_from_name(std::string_view word) noexcept
{
    if (turkish_key_equals(word, "dis")) return AttachSide::Outside;
    if (turkish_key_equals(word, "ic")) return AttachSide::Inside;
    if (turkish_key_equals(word, "sol")) return AttachSide::Left;
    if (turkish_key_equals(word, "sag")) return AttachSide::Right;
    return std::nullopt;
}

const char* attach_unit_suffix(std::uint8_t unit) noexcept
{
    switch (static_cast<DrawingUnit>(unit)) {
    case DrawingUnit::Centimetre: return " cm";
    case DrawingUnit::Millimetre: return " mm";
    case DrawingUnit::Kilometre: return " km";
    default: return " m";
    }
}

std::string attach_fill(std::string_view format, std::string_view figure)
{
    if (format.empty()) return std::string(figure);
    const auto at = format.find("{}");
    if (at == std::string_view::npos) return std::string(format) + std::string(figure);
    return std::string(format.substr(0, at)) + std::string(figure) +
           std::string(format.substr(at + 2));
}

std::optional<AttachPlacement> attach_place(std::span<const Point2> ring, bool closed,
                                            const Attachment& a, Mm height, bool with_offset)
{
    const std::size_t n = ring.size();
    if (n < 2) return std::nullopt;
    if (height < 0) height = 0;
    const auto offset = static_cast<double>(a.gap) + static_cast<double>(height) / 2.0;

    AttachPlacement out;
    Dir frame_u{1.0, 0.0}; // the reading frame the hand offset is measured in

    if (a.anchor == AttachAnchor::Landing) {
        // A LEADER'S LANDING: its last point, the words beside it on the side
        // the last segment points — right of it for a line coming in from the
        // left, left of it for one coming in from the right — reading along
        // the page, their near edge a gap away. Only an open line has a free
        // end to hang words on.
        if (closed) return std::nullopt;
        const Point2 end  = ring[n - 1];
        const Point2 from = ring[n - 2];
        const bool right  = end.x >= from.x;
        out.centre        = Point2{end.x + (right ? a.gap : -a.gap), end.y};
        out.dir_x         = 1.0;
        out.dir_y         = 0.0;
        out.anchor        = right ? TextAnchor::MiddleLeft : TextAnchor::MiddleRight;
    } else if (a.anchor == AttachAnchor::Centre) {
        // The middle of the ring's box, read along the page — where ETİKET has
        // always put a parcel's number, so a label that follows its parcel sits
        // where the one that did not used to.
        Box2 box;
        for (const Point2 p : ring)
            box.extend(p);
        out.centre = box.centre();
        out.dir_x  = 1.0;
        out.dir_y  = 0.0;
    } else if (a.anchor == AttachAnchor::Edge) {
        if (a.index >= segment_count(n, closed)) return std::nullopt;
        const Point2 p = ring[a.index];
        const Point2 q = ring[(a.index + 1) % n];
        const Dir u    = unit_between(p, q);
        if (u.x == 0.0 && u.y == 0.0) return std::nullopt;

        // Which way is off the edge. Outside/inside ask the ring's turn; left
        // and right ask the reading direction, which is the walk turned to read
        // left to right.
        double nx = -u.y;
        double ny = u.x;
        if (a.side == AttachSide::Outside || a.side == AttachSide::Inside) {
            const bool outward = a.side == AttachSide::Outside;
            const bool ccw     = twice_area(ring) > 0.0;
            if (closed ? (ccw == outward) : !outward) {
                nx = -nx;
                ny = -ny;
            }
        } else {
            const Dir r = reading(u);
            nx          = -r.y;
            ny          = r.x;
            if (a.side == AttachSide::Right) {
                nx = -nx;
                ny = -ny;
            }
        }
        out.centre = Point2{(p.x + q.x) / 2 + mm_round(nx * offset),
                            (p.y + q.y) / 2 + mm_round(ny * offset)};
        out.dir_x  = u.x;
        out.dir_y  = u.y;
        frame_u    = reading(u);
    } else {
        if (a.index >= n) return std::nullopt;
        const std::size_t i = a.index;
        const Point2 corner = ring[i];

        // The outward bisector: away from both neighbours; for a reflex corner
        // that points inside, so the ring is asked.
        const bool has_prev = closed || i > 0;
        const bool has_next = closed || i + 1 < n;
        const Dir to_prev   = has_prev ? unit_between(corner, ring[(i + n - 1) % n]) : Dir{};
        const Dir to_next   = has_next ? unit_between(corner, ring[(i + 1) % n]) : Dir{};
        const Dir d{to_prev.x + to_next.x, to_prev.y + to_next.y};
        const double dl = std::sqrt(d.x * d.x + d.y * d.y);
        Dir away{};
        if (dl > 1e-9) {
            away = Dir{-d.x / dl, -d.y / dl};
            if (closed) {
                std::vector<Mm> xs(n);
                std::vector<Mm> ys(n);
                for (std::size_t k = 0; k < n; ++k) {
                    xs[k] = ring[k].x;
                    ys[k] = ring[k].y;
                }
                const Point2 probe{corner.x + mm_round(away.x * offset * 2.0),
                                   corner.y + mm_round(away.y * offset * 2.0)};
                if (ring_contains(xs, ys, probe)) away = Dir{-away.x, -away.y};
            }
        } else {
            // Straight through, or an end of a line: the right of the walk is
            // outside a counter-clockwise ring; a line's end points away.
            const bool ccw = closed && twice_area(ring) > 0.0;
            const Dir u    = has_next ? to_next : Dir{-to_prev.x, -to_prev.y};
            away           = Dir{u.y, -u.x};
            if (closed && !ccw) away = Dir{-away.x, -away.y};
            if (!closed && !has_next) away = Dir{-to_prev.x, -to_prev.y};
            if (!closed && !has_prev) away = Dir{-to_next.x, -to_next.y};
        }
        out.centre =
            Point2{corner.x + mm_round(away.x * offset), corner.y + mm_round(away.y * offset)};
        out.dir_x = 1.0;
        out.dir_y = 0.0;
    }

    if (with_offset && (a.along != 0 || a.across != 0)) {
        const Dir frame_n{-frame_u.y, frame_u.x};
        out.centre.x += mm_round(frame_u.x * static_cast<double>(a.along) +
                                 frame_n.x * static_cast<double>(a.across));
        out.centre.y += mm_round(frame_u.y * static_cast<double>(a.along) +
                                 frame_n.y * static_cast<double>(a.across));
    }
    return out;
}

std::optional<std::string> attach_text(std::span<const Point2> ring, bool closed,
                                       const Attachment& a)
{
    if (a.derive != AttachDerive::Length) return std::nullopt;
    if (a.anchor != AttachAnchor::Edge) return std::nullopt;
    if (a.index >= segment_count(ring.size(), closed)) return std::nullopt;
    const Point2 p = ring[a.index];
    const Point2 q = ring[(a.index + 1) % ring.size()];
    const Mm len   = segment_length(p, q);
    return attach_fill(
        a.format.empty() ? std::string("{}") + attach_unit_suffix(a.unit) : a.format,
        format_dimension_length(len, static_cast<DrawingUnit>(a.unit), a.precision, a.separator));
}

void attach_measure_offset(const AttachPlacement& rule, Point2 actual, Attachment& a)
{
    Dir u = reading(Dir{rule.dir_x, rule.dir_y});
    if (a.anchor == AttachAnchor::Vertex || (u.x == 0.0 && u.y == 0.0)) u = Dir{1.0, 0.0};
    const Dir n{-u.y, u.x};
    const auto dx = static_cast<double>(actual.x - rule.centre.x);
    const auto dy = static_cast<double>(actual.y - rule.centre.y);
    a.along       = mm_round(dx * u.x + dy * u.y);
    a.across      = mm_round(dx * n.x + dy * n.y);
}

std::uint32_t attach_nearest(std::span<const Point2> ring, bool closed, AttachAnchor anchor,
                             Point2 at)
{
    const std::size_t n = ring.size();
    if (n == 0) return 0;
    std::uint32_t best = 0;
    bool have          = false;
    Int128 best_d      = 0;
    if (anchor == AttachAnchor::Vertex) {
        for (std::size_t i = 0; i < n; ++i) {
            const Int128 d = distance2(ring[i], at);
            if (!have || d < best_d) {
                have   = true;
                best_d = d;
                best   = static_cast<std::uint32_t>(i);
            }
        }
        return best;
    }
    const std::size_t segs = segment_count(n, closed);
    for (std::size_t i = 0; i < segs; ++i) {
        const Point2 foot = closest_point_on_segment(ring[i], ring[(i + 1) % n], at);
        const Int128 d    = distance2(foot, at);
        if (!have || d < best_d) {
            have   = true;
            best_d = d;
            best   = static_cast<std::uint32_t>(i);
        }
    }
    return best;
}

AttachSide attach_side_of(std::span<const Point2> ring, bool closed, std::uint32_t index, Point2 at)
{
    const std::size_t n = ring.size();
    if (index >= segment_count(n, closed)) return closed ? AttachSide::Outside : AttachSide::Left;
    const Point2 p = ring[index];
    const Point2 q = ring[(index + 1) % n];
    // Which side of the walk `at` lies on: the sign of the cross product.
    const Int128 cross = (static_cast<Int128>(q.x) - p.x) * (static_cast<Int128>(at.y) - p.y) -
                         (static_cast<Int128>(q.y) - p.y) * (static_cast<Int128>(at.x) - p.x);
    const bool left_of_walk = cross > 0;
    if (closed) {
        // The interior is on the left of a counter-clockwise walk.
        const bool ccw = twice_area(ring) > 0.0;
        return (left_of_walk == ccw) ? AttachSide::Inside : AttachSide::Outside;
    }
    const Dir u = unit_between(p, q);
    const Dir r = reading(u);
    // Reading left is the walk's left when the walk already reads left to right.
    const bool same = (r.x == u.x && r.y == u.y);
    return (left_of_walk == same) ? AttachSide::Left : AttachSide::Right;
}

Attachment attach_reanchor(std::span<const Point2> was, bool was_closed,
                           std::span<const Point2> now, bool now_closed, const Attachment& a)
{
    if (was.size() == now.size() || was.empty() || now.empty()) return a;
    // The centre names no corner and no edge: there is nothing to re-anchor.
    if (a.anchor == AttachAnchor::Centre) return a;

    // Where the named feature stood: a corner, or an edge's midpoint. A feature
    // that was not on the old ring either falls back to its first vertex.
    Point2 stood = was[0];
    if (a.anchor == AttachAnchor::Vertex) {
        if (a.index < was.size()) stood = was[a.index];
    } else if (a.index < segment_count(was.size(), was_closed)) {
        const Point2 p = was[a.index];
        const Point2 q = was[(a.index + 1) % was.size()];
        stood          = Point2{(p.x + q.x) / 2, (p.y + q.y) / 2};
    }

    Attachment out = a;
    if (a.anchor == AttachAnchor::Vertex) {
        out.index = attach_nearest(now, now_closed, AttachAnchor::Vertex, stood);
        return out;
    }
    // For an edge the nearest MIDPOINT, not the nearest segment: the caption
    // stood beside the middle of its edge, and the half of a split edge whose
    // middle is nearer is the half it was written on.
    const std::size_t segs = segment_count(now.size(), now_closed);
    std::uint32_t best     = 0;
    bool have              = false;
    Int128 best_d          = 0;
    for (std::size_t i = 0; i < segs; ++i) {
        const Point2 p = now[i];
        const Point2 q = now[(i + 1) % now.size()];
        const Int128 d = distance2(Point2{(p.x + q.x) / 2, (p.y + q.y) / 2}, stood);
        if (!have || d < best_d) {
            have   = true;
            best_d = d;
            best   = static_cast<std::uint32_t>(i);
        }
    }
    out.index = best;
    return out;
}

// ------------------------------------------------------------ AttachTable ----

void AttachTable::resize(std::size_t entity_count)
{
    count_ = entity_count;
    if (!ref_.empty() && ref_.size() < count_) ref_.resize(count_, kNoAttach);
}

AttachTable::Tail AttachTable::tail() const
{
    return Tail{count_, !ref_.empty(), records_.size(), free_};
}

void AttachTable::truncate(const Tail& t)
{
    if (t.count > count_ || t.records > records_.size()) return;
    for (std::size_t r = t.records; r < owner_.size(); ++r)
        if (owner_[r] != kNoEntity) return; // still in use: leave everything as it is
    count_ = t.count;
    if (!t.materialised)
        ref_.clear();
    else if (ref_.size() > t.count)
        ref_.resize(t.count);
    records_.resize(t.records);
    owner_.resize(t.records);
    free_ = t.free;
}

void AttachTable::materialise()
{
    if (ref_.empty()) ref_.assign(count_, kNoAttach);
}

bool AttachTable::has(EntityId e) const noexcept
{
    return e < ref_.size() && ref_[e] != kNoAttach;
}

const Attachment* AttachTable::get(EntityId e) const noexcept
{
    if (!has(e)) return nullptr;
    return &records_[ref_[e]];
}

void AttachTable::set(EntityId e, Attachment a)
{
    if (e >= count_) count_ = static_cast<std::size_t>(e) + 1;
    materialise();
    if (ref_.size() < count_) ref_.resize(count_, kNoAttach);
    if (ref_[e] != kNoAttach) {
        records_[ref_[e]] = std::move(a);
        return;
    }
    std::uint32_t r = 0;
    if (!free_.empty()) {
        r = free_.back();
        free_.pop_back();
        records_[r] = std::move(a);
        owner_[r]   = e;
    } else {
        r = static_cast<std::uint32_t>(records_.size());
        records_.push_back(std::move(a));
        owner_.push_back(e);
    }
    ref_[e] = r;
    ++live_;
}

bool AttachTable::clear(EntityId e)
{
    if (!has(e)) return false;
    const std::uint32_t r = ref_[e];
    ref_[e]               = kNoAttach;
    owner_[r]             = kNoEntity;
    records_[r]           = Attachment{};
    free_.push_back(r);
    --live_;
    return true;
}

std::vector<EntityId> AttachTable::attached() const
{
    std::vector<EntityId> out;
    out.reserve(live_);
    for (std::size_t e = 0; e < ref_.size(); ++e)
        if (ref_[e] != kNoAttach) out.push_back(static_cast<EntityId>(e));
    return out;
}

void AttachTable::dependents_of(EntityKey source, std::vector<EntityId>& out) const
{
    out.clear();
    if (live_ == 0 || source == EntityKey::None) return;
    for (std::size_t r = 0; r < records_.size(); ++r)
        if (owner_[r] != kNoEntity && records_[r].source == source) out.push_back(owner_[r]);
    std::sort(out.begin(), out.end());
}

std::uint64_t AttachTable::fold(std::uint64_t seed, std::span<const std::uint32_t> position) const
{
    if (live_ == 0) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(live_), seed ^ kAttachSeed);
    for (const EntityId e : attached()) {
        const Attachment& a     = records_[ref_[e]];
        const std::uint32_t row = e < position.size() ? position[e] : e;
        h                       = fnv1a_int(static_cast<std::int64_t>(row), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(raw(a.source)), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.anchor), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.side), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.derive), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.ring), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.index), h);
        h                       = fnv1a_int(a.gap, h);
        h                       = fnv1a_int(a.along, h);
        h                       = fnv1a_int(a.across, h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.unit), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.precision), h);
        h                       = fnv1a_int(static_cast<std::int64_t>(a.separator), h);
        h                       = fnv1a(a.format, h);
    }
    return h;
}

} // namespace kentos::core
