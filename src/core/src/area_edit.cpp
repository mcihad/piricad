// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/area_edit.hpp"

#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/wire.hpp"

#include <cmath>
#include <cstring>

namespace kentos::core {
namespace {

constexpr std::size_t kPayloadBytes = 8 + 1 + 4 + 8 + 8 + 8;

struct V2
{
    double x{0.0};
    double y{0.0};
};

V2 sub(Point2 a, Point2 b)
{
    return V2{static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y)};
}

double cross(V2 a, V2 b)
{
    return a.x * b.y - a.y * b.x;
}

double dot(V2 a, V2 b)
{
    return a.x * b.x + a.y * b.y;
}

double length(V2 a)
{
    return std::sqrt(a.x * a.x + a.y * a.y);
}

/// The intersection of the line through `p` along `u` with the line through `q`
/// along `w`, or false when they are parallel.
bool intersect(V2 p, V2 u, V2 q, V2 w, V2& out)
{
    const double den = cross(u, w);
    if (std::abs(den) < 1e-9) return false;
    const double t = cross(V2{q.x - p.x, q.y - p.y}, w) / den;
    out            = V2{p.x + u.x * t, p.y + u.y * t};
    return true;
}

Point2 rounded(V2 v)
{
    return Point2{mm_round(v.x), mm_round(v.y)};
}

/// +1 for a counter-clockwise ring, -1 for clockwise.
double turn(const std::vector<Point2>& ring)
{
    return ring_area(ring) < 0 ? -1.0 : 1.0;
}

Mm2 magnitude(Mm2 a)
{
    return a < 0 ? -a : a;
}

/// The unit outward normal of edge `edge`.
bool outward_normal(const std::vector<Point2>& ring, std::size_t edge, V2& n, V2& u)
{
    const std::size_t count = ring.size();
    if (count < 3 || edge >= count) return false;
    const Point2 a = ring[edge];
    const Point2 b = ring[(edge + 1) % count];
    u              = sub(b, a);
    const double l = length(u);
    if (l == 0.0) return false;
    u = V2{u.x / l, u.y / l};
    // Right of the walk is outside a counter-clockwise ring.
    const double s = turn(ring);
    n              = V2{u.y * s, -u.x * s};
    return true;
}

} // namespace

std::vector<std::uint8_t> encode_area_edit(const AreaEditRequest& r)
{
    std::vector<std::uint8_t> out;
    out.reserve(kPayloadBytes);
    put_i64(out, r.key);
    put_u8(out, static_cast<std::uint8_t>(r.mode));
    put_u32(out, r.index);
    put_i64(out, r.target);
    put_i64(out, r.grab.x);
    put_i64(out, r.grab.y);
    return out;
}

std::optional<AreaEditRequest> decode_area_edit(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != kPayloadBytes) return std::nullopt;
    const auto i64 = [&bytes](std::size_t at) {
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(bytes[at + i]) << (8 * i);
        std::int64_t out = 0;
        std::memcpy(&out, &v, sizeof out);
        return out;
    };
    AreaEditRequest r;
    r.key = i64(0);
    if (bytes[8] > 2) return std::nullopt;
    r.mode  = static_cast<AreaEditMode>(bytes[8]);
    r.index = static_cast<std::uint32_t>(bytes[9]) | (static_cast<std::uint32_t>(bytes[10]) << 8) |
              (static_cast<std::uint32_t>(bytes[11]) << 16) |
              (static_cast<std::uint32_t>(bytes[12]) << 24);
    r.target = i64(13);
    r.grab   = Point2{i64(21), i64(29)};
    return r;
}

std::vector<Point2> area_edit_shift_edge(const std::vector<Point2>& ring, std::size_t edge,
                                         double offset)
{
    V2 n{};
    V2 u{};
    if (!outward_normal(ring, edge, n, u)) return {};
    const std::size_t count = ring.size();
    const std::size_t i     = edge;
    const std::size_t j     = (edge + 1) % count;
    const Point2 a          = ring[i];
    const Point2 b          = ring[j];
    const Point2 p          = ring[(i + count - 1) % count];
    const Point2 q          = ring[(j + 1) % count];

    const V2 a2{static_cast<double>(a.x) + n.x * offset, static_cast<double>(a.y) + n.y * offset};
    const V2 prev_dir = sub(a, p);
    const V2 next_dir = sub(q, b);
    V2 na{};
    V2 nb{};
    if (!intersect(V2{static_cast<double>(p.x), static_cast<double>(p.y)}, prev_dir, a2, u, na))
        return {};
    if (!intersect(V2{static_cast<double>(b.x), static_cast<double>(b.y)}, next_dir, a2, u, nb))
        return {};

    std::vector<Point2> out = ring;
    out[i]                  = rounded(na);
    out[j]                  = rounded(nb);
    return out;
}

std::optional<double> area_edit_edge_offset(const std::vector<Point2>& ring, std::size_t edge,
                                            Mm2 target)
{
    if (target <= 0 || ring.size() < 3 || edge >= ring.size()) return std::nullopt;
    const Mm2 have = magnitude(ring_area(ring));
    if (have == target) return 0.0;
    const double sign = target > have ? 1.0 : -1.0;

    // Bracket: grow the offset until the area passes the target or the shape
    // stops being a shape, then bisect. Doubles, then the vertices are rounded
    // to the millimetre once by `area_edit_shift_edge`.
    const auto area_at = [&](double t) -> std::optional<double> {
        const std::vector<Point2> shifted = area_edit_shift_edge(ring, edge, t);
        if (shifted.empty()) return std::nullopt;
        const Mm2 a = ring_area(shifted);
        // A shape that turned over went past its degenerate point.
        if ((a < 0) != (ring_area(ring) < 0)) return std::nullopt;
        return static_cast<double>(magnitude(a));
    };
    const auto want = static_cast<double>(target);
    double lo       = 0.0;
    double hi       = 1000.0; // one metre to start
    for (int i = 0; i < 40; ++i) {
        const auto a = area_at(sign * hi);
        if (!a) return std::nullopt;
        if ((sign > 0 && *a >= want) || (sign < 0 && *a <= want)) break;
        lo = hi;
        hi *= 2.0;
        if (hi > 1e9) return std::nullopt;
    }
    for (int i = 0; i < 64; ++i) {
        const double mid = (lo + hi) / 2.0;
        const auto a     = area_at(sign * mid);
        if (!a) {
            hi = mid;
            continue;
        }
        if ((sign > 0 && *a < want) || (sign < 0 && *a > want))
            lo = mid;
        else
            hi = mid;
    }
    return sign * (lo + hi) / 2.0;
}

std::vector<Point2> area_edit_move_vertex(const std::vector<Point2>& ring, std::size_t index,
                                          Point2 to)
{
    if (index >= ring.size()) return {};
    std::vector<Point2> out = ring;
    out[index]              = to;
    return out;
}

std::optional<Point2> area_edit_vertex_for(const std::vector<Point2>& ring, std::size_t index,
                                           Point2 cursor, Mm2 target)
{
    const std::size_t count = ring.size();
    if (count < 3 || index >= count || target <= 0) return std::nullopt;
    const Point2 v    = ring[index];
    const Point2 prev = ring[(index + count - 1) % count];
    const Point2 next = ring[(index + 1) % count];
    const V2 d        = sub(next, prev);
    const double dl   = length(d);
    if (dl == 0.0) return std::nullopt;

    // Twice the signed area changes by cross(m − v, d) when v moves to m, so the
    // places where the area equals the target form a line parallel to
    // prev→next. The cursor's foot on it is the nearest such place.
    const double s       = turn(ring);
    const auto have      = static_cast<double>(ring_area(ring)); // signed
    const double wanted2 = 2.0 * (s * static_cast<double>(target) - have);
    const V2 n{-d.y / dl, d.x / dl};
    const double k = (cross(sub(cursor, v), d) - wanted2) / dl;
    return rounded(
        V2{static_cast<double>(cursor.x) + n.x * k, static_cast<double>(cursor.y) + n.y * k});
}

Result<std::vector<Point2>> area_edit_uniform(const std::vector<Point2>& ring, Mm2 target)
{
    if (target <= 0) return err(ErrorCode::InvalidArgument, "Hedef alan sıfırdan büyük olmalı.");
    if (ring.size() < 3)
        return err(ErrorCode::InvalidArgument,
                   "Alanı değiştirilecek nesne en az üç köşeli bir alan olmalı.");
    const Mm2 have = magnitude(ring_area(ring));
    if (have == target) return ring;

    // The face offset by `d` on every edge, as one ring; nothing when it split
    // or vanished — past that point the figure cannot be reached this way.
    const auto shape = [&](Mm d) -> std::optional<std::vector<Point2>> {
        if (d == 0) return ring;
        auto rings = offset_ring(ring, true, d, JoinStyle::Miter);
        if (!rings || rings.value().size() != 1) return std::nullopt;
        return rings.value().front().points;
    };
    const Mm sign = target > have ? 1 : -1;
    Mm lo         = 0;
    Mm hi         = 1000;
    for (int i = 0; i < 40; ++i) {
        const auto s = shape(sign * hi);
        if (!s)
            return err(ErrorCode::ValidationFailed,
                       "Alan bu değere her taraftan daraltılarak getirilemiyor: şekil hedefe "
                       "varmadan bozuluyor.");
        const Mm2 a = magnitude(ring_area(*s));
        if ((sign > 0 && a >= target) || (sign < 0 && a <= target)) break;
        lo = hi;
        hi *= 2;
        if (hi > 1000000000) // a thousand kilometres: not a parcel
            return err(ErrorCode::ValidationFailed, "Hedef alan bu şekil için ulaşılamaz.");
    }
    while (hi - lo > 1) {
        const Mm mid = lo + (hi - lo) / 2;
        const auto s = shape(sign * mid);
        if (!s) {
            hi = mid;
            continue;
        }
        const Mm2 a = magnitude(ring_area(*s));
        if ((sign > 0 && a < target) || (sign < 0 && a > target))
            lo = mid;
        else
            hi = mid;
    }
    // Of the two millimetres the answer lies between, the nearer figure.
    const auto at_lo = shape(sign * lo);
    const auto at_hi = shape(sign * hi);
    if (!at_lo && !at_hi)
        return err(ErrorCode::ValidationFailed, "Hedef alan bu şekil için ulaşılamaz.");
    if (!at_hi) return *at_lo;
    if (!at_lo) return *at_hi;
    const Mm2 dlo = magnitude(magnitude(ring_area(*at_lo)) - target);
    const Mm2 dhi = magnitude(magnitude(ring_area(*at_hi)) - target);
    return dlo <= dhi ? *at_lo : *at_hi;
}

AreaGhost area_edit_ghost(const std::vector<Point2>& ring, const AreaEditRequest& request,
                          Point2 cursor, Mm snap)
{
    AreaGhost ghost;
    if (ring.size() < 3) return ghost;
    switch (request.mode) {
    case AreaEditMode::Edge: {
        V2 n{};
        V2 u{};
        if (!outward_normal(ring, request.index, n, u)) return ghost;
        const Point2 a      = ring[request.index];
        const double t_hand = dot(sub(cursor, a), n);
        const auto t_target = area_edit_edge_offset(ring, request.index, request.target);
        double t            = t_hand;
        if (t_target) {
            ghost.commit = rounded(V2{static_cast<double>(a.x) + n.x * *t_target,
                                      static_cast<double>(a.y) + n.y * *t_target});
            if (std::abs(t_hand - *t_target) <= static_cast<double>(snap)) {
                t             = *t_target;
                ghost.snapped = true;
            }
        } else {
            ghost.commit = cursor;
        }
        ghost.points = area_edit_shift_edge(ring, request.index, t);
        break;
    }
    case AreaEditMode::Vertex: {
        const auto on_target = area_edit_vertex_for(ring, request.index, cursor, request.target);
        Point2 to            = cursor;
        if (on_target) {
            ghost.commit   = *on_target;
            const double d = length(sub(*on_target, cursor));
            if (d <= static_cast<double>(snap)) {
                to            = *on_target;
                ghost.snapped = true;
            }
        } else {
            ghost.commit = cursor;
        }
        ghost.points = area_edit_move_vertex(ring, request.index, to);
        break;
    }
    case AreaEditMode::Uniform: {
        auto shaped = area_edit_uniform(ring, request.target);
        if (shaped) {
            ghost.points  = shaped.value();
            ghost.snapped = true;
        }
        ghost.commit = cursor;
        break;
    }
    }
    if (!ghost.points.empty()) ghost.area = magnitude(ring_area(ghost.points));
    return ghost;
}

Result<std::vector<Point2>> area_edit_apply(const std::vector<Point2>& ring,
                                            const AreaEditRequest& request, Point2 at)
{
    if (ring.size() < 3)
        return err(ErrorCode::InvalidArgument,
                   "Alanı değiştirilecek nesne en az üç köşeli bir alan olmalı.");
    switch (request.mode) {
    case AreaEditMode::Uniform: return area_edit_uniform(ring, request.target);
    case AreaEditMode::Edge: {
        V2 n{};
        V2 u{};
        if (!outward_normal(ring, request.index, n, u))
            return err(ErrorCode::InvalidArgument, "Kenar " + std::to_string(request.index + 1) +
                                                       " yok ya da sıfır uzunlukta.");
        const double t                    = dot(sub(at, ring[request.index]), n);
        const std::vector<Point2> shifted = area_edit_shift_edge(ring, request.index, t);
        if (shifted.empty())
            return err(ErrorCode::ValidationFailed,
                       "Kenar bu kadar kaydırılamaz: komşu kenarlar onunla kesişmiyor.");
        return shifted;
    }
    case AreaEditMode::Vertex: {
        if (request.index >= ring.size())
            return err(ErrorCode::InvalidArgument,
                       "Köşe " + std::to_string(request.index + 1) + " yok.");
        return area_edit_move_vertex(ring, request.index, at);
    }
    }
    return err(ErrorCode::InvalidArgument, "Bilinmeyen alan düzenleme kipi.");
}

std::string format_square_metres(Mm2 area)
{
    const bool negative     = area < 0;
    const auto abs_mm2      = static_cast<std::uint64_t>(negative ? -area : area);
    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000;
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac + " m²";
}

} // namespace kentos::core
