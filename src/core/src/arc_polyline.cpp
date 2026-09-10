// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/arc_polyline.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace kentos::core {
namespace {

constexpr std::uint16_t kFlagConstantWidth = 1u << 0;
constexpr std::size_t kArcBytes            = 4 + 8 + 8 + 8 + 1;

/// The vertex ring of a slot: the one ring an arc polyline has.
struct Verts
{
    std::span<const Mm> xs;
    std::span<const Mm> ys;
    bool closed{false};
};

Verts verts_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return {};
    return Verts{geom.ring_xs(rs.first), geom.ring_ys(rs.first),
                 geom.ring_role[rs.first] != RingRole::Open};
}

/// Distance between two points, in a double, translated first.
double distance(Point2 a, Point2 b)
{
    const auto dx = static_cast<double>(b.x - a.x);
    const auto dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

// ---------------------------------------------------------- kind functions ---

void ap_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (const std::uint32_t slot : slots) {
        const bool closed = arc_polyline_outline(geom, slot, xs, ys);
        into.begin_run(closed);
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
}

void ap_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        // From the drawn form: a bent edge reaches past the box of its two ends.
        arc_polyline_outline(geom, slots[i], xs, ys);
        out[i] = kind::box_of_points(xs, ys);
    }
}

void ap_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        const std::uint32_t one[1]{slots[i]};
        ap_outline(geom, SlotSpan(one, 1), runs);
        // Inside counts for a closed one, the polyline rule: a click in the
        // parcel is a click on the parcel.
        out[i] = kind::runs_hit(runs, probe, tolerance, runs.run_closed[0] != 0) ? 1 : 0;
    }
}

void ap_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i]        = 0;
        const Verts v = verts_of(geom, slots[i]);
        if (!v.closed || v.xs.size() < 3) continue;
        auto def = arc_polyline_of(geom, slots[i]);
        if (!def) continue;

        // EXACT: the polygon of the vertices, then each bent edge adds the
        // circular segment between its chord and its arc, or takes it away. On
        // a counter-clockwise ring the interior is to the LEFT of travel; a
        // counter-clockwise arc bulges to the right — outward — and adds, a
        // clockwise one bulges inward and subtracts. On a clockwise ring the
        // signs read the other way and the signed sum below still holds.
        double signed2      = kind::run_area2(v.xs, v.ys);
        const std::size_t n = v.xs.size();
        for (const ArcPolyline::Arc& arc : def.value().arcs) {
            if (arc.segment >= n) continue;
            const Point2 a{v.xs[arc.segment], v.ys[arc.segment]};
            const std::size_t nxt = arc.segment + 1 >= n ? 0 : arc.segment + 1;
            const Point2 b{v.xs[nxt], v.ys[nxt]};
            const std::int64_t sweep =
                arc.ccw ? arc_sweep_udeg(arc.centre, a, b) : arc_sweep_udeg(arc.centre, b, a);
            const auto segment = static_cast<double>(circular_segment_area(arc.radius, sweep));
            signed2 += arc.ccw ? 2.0 * segment : -2.0 * segment;
        }
        const double area     = std::abs(signed2) / 2.0;
        constexpr double kMax = 9.0e18;
        out[i] = area >= kMax ? static_cast<Mm2>(kMax) : static_cast<Mm2>(std::llround(area));
    }
}

void ap_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i]        = 0;
        const Verts v = verts_of(geom, slots[i]);
        if (v.xs.size() < 2) continue;
        auto def               = arc_polyline_of(geom, slots[i]);
        const std::size_t n    = v.xs.size();
        const std::size_t segs = v.closed ? n : n - 1;
        double total           = 0.0;
        std::size_t next_arc   = 0;
        for (std::size_t s = 0; s < segs; ++s) {
            const Point2 a{v.xs[s], v.ys[s]};
            const std::size_t nxt = s + 1 == n ? 0 : s + 1;
            const Point2 b{v.xs[nxt], v.ys[nxt]};
            const ArcPolyline::Arc* arc = nullptr;
            if (def)
                while (next_arc < def.value().arcs.size()) {
                    const ArcPolyline::Arc& candidate = def.value().arcs[next_arc];
                    if (candidate.segment < s) {
                        ++next_arc;
                        continue;
                    }
                    if (candidate.segment == s) arc = &candidate;
                    break;
                }
            if (arc == nullptr) {
                total += distance(a, b);
                continue;
            }
            // r·θ, the sweep in whole micro-degrees — the arc kind's own rule.
            const std::int64_t sweep =
                arc->ccw ? arc_sweep_udeg(arc->centre, a, b) : arc_sweep_udeg(arc->centre, b, a);
            total += static_cast<double>(arc->radius) * static_cast<double>(sweep) *
                     (kPi / (180.0 * 1000000.0));
        }
        out[i] = static_cast<Mm>(std::llround(total));
    }
}

void ap_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
              std::vector<std::uint32_t>& ends)
{
    // The vertex ring, exactly as the polyline writes its rings; the payload
    // column carries the arcs (model.md R9a).
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        put_u32(bytes, rs.count);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            bytes.push_back(static_cast<std::uint8_t>(geom.ring_role[r]));
            put_u16(bytes, geom.ring_part[r]);
            put_u32(bytes, static_cast<std::uint32_t>(xs.size()));
            for (std::size_t v = 0; v < xs.size(); ++v) {
                put_mm(bytes, xs[v]);
                put_mm(bytes, ys[v]);
            }
        }
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> ap_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError, "Yaylı çoklu çizgi yükü halka sayısını taşımıyor.");
    const std::uint32_t ring_count = in.u32();
    if (ring_count != 1 || !in.remaining(7))
        return err(ErrorCode::ParseError, "Yaylı çoklu çizgi tek halka ister; bildirilen " +
                                              std::to_string(ring_count) + ".");
    const std::uint8_t role_byte = in.u8();
    const std::uint16_t part     = in.u16();
    const std::uint32_t verts    = in.u32();
    if (role_byte > static_cast<std::uint8_t>(RingRole::Interior))
        return err(ErrorCode::ParseError, "Bilinmeyen halka rolü: " + std::to_string(role_byte));
    if (!in.remaining_records(verts, 16))
        return err(ErrorCode::ParseError, "Yaylı çoklu çizgi için " + std::to_string(verts) +
                                              " tepe noktası bildirilmiş, bu kadar veri yok.");
    std::vector<Point2> pts(verts);
    for (std::uint32_t v = 0; v < verts; ++v) {
        pts[v].x = in.mm();
        pts[v].y = in.mm();
    }
    const RingGeometry::RingInput ring{pts, static_cast<RingRole>(role_byte), part};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

Status ap_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    if (rings.size() != 1)
        return err(ErrorCode::ValidationFailed, "Yaylı çoklu çizgi tek halka ister; verilen " +
                                                    std::to_string(rings.size()) + ".");
    const std::size_t n = rings[0].points.size();
    if (n < 2)
        return err(ErrorCode::ValidationFailed,
                   "Yaylı çoklu çizgi en az iki tepe noktası ister; verilen " + std::to_string(n) +
                       ".");
    auto def = decode_arc_polyline(payload);
    if (!def) return def.error();
    const bool closed      = rings[0].role != RingRole::Open;
    const std::size_t segs = closed ? n : n - 1;
    for (const ArcPolyline::Arc& arc : def.value().arcs) {
        if (arc.segment >= segs)
            return err(ErrorCode::ValidationFailed, "Yay kenar " + std::to_string(arc.segment) +
                                                        " yok: çizginin " + std::to_string(segs) +
                                                        " kenarı var.");
        // The centre has to be the arc's: equidistant from both ends to within
        // the rounding a stored millimetre allows.
        const Point2 a = rings[0].points[arc.segment];
        const Point2 b = rings[0].points[(arc.segment + 1) % n];
        const auto r   = static_cast<double>(arc.radius);
        if (std::abs(distance(arc.centre, a) - r) > 2.0 ||
            std::abs(distance(arc.centre, b) - r) > 2.0)
            return err(ErrorCode::ValidationFailed,
                       "Yay kenar " + std::to_string(arc.segment) +
                           " için verilen merkez iki ucundan yarıçap kadar uzakta değil.");
    }
    return ok();
}

void ap_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    const Verts v = verts_of(geom, slot);
    if (v.xs.size() < 2) return;
    auto def               = arc_polyline_of(geom, slot);
    const std::size_t n    = v.xs.size();
    const std::size_t segs = v.closed ? n : n - 1;
    std::size_t next_arc   = 0;
    for (std::size_t s = 0; s < n; ++s)
        kind::offer(into, Point2{v.xs[s], v.ys[s]}, kind::kKeyEndpoint);
    for (std::size_t s = 0; s < segs; ++s) {
        const Point2 a{v.xs[s], v.ys[s]};
        const std::size_t nxt = s + 1 == n ? 0 : s + 1;
        const Point2 b{v.xs[nxt], v.ys[nxt]};
        const ArcPolyline::Arc* arc = nullptr;
        if (def)
            while (next_arc < def.value().arcs.size()) {
                const ArcPolyline::Arc& candidate = def.value().arcs[next_arc];
                if (candidate.segment < s) {
                    ++next_arc;
                    continue;
                }
                if (candidate.segment == s) arc = &candidate;
                break;
            }
        if (arc == nullptr) {
            kind::offer(into, Point2{(a.x + b.x) / 2, (a.y + b.y) / 2}, kind::kKeyMidpoint);
            continue;
        }
        kind::offer(into, arc->centre, kind::kKeyCenter);
        kind::offer(into,
                    arc->ccw ? arc_midpoint(arc->centre, arc->radius, a, b)
                             : arc_midpoint(arc->centre, arc->radius, b, a),
                    kind::kKeyMidpoint);
    }
}

} // namespace

std::vector<std::uint8_t> encode_arc_polyline(const ArcPolyline& def)
{
    std::vector<std::uint8_t> out;
    kind::put_header(out, kArcPolylineLayout, def.constant_width != 0 ? kFlagConstantWidth : 0);
    put_i64(out, def.constant_width);
    put_u32(out, static_cast<std::uint32_t>(def.arcs.size()));
    for (const ArcPolyline::Arc& a : def.arcs) {
        put_u32(out, a.segment);
        put_mm(out, a.centre.x);
        put_mm(out, a.centre.y);
        put_mm(out, a.radius);
        put_u8(out, a.ccw ? 1 : 0);
    }
    return out;
}

Result<ArcPolyline> decode_arc_polyline(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(12))
        return err(ErrorCode::ParseError,
                   "Yaylı çoklu çizgi yükü başlığı ve yay sayısını taşıyacak kadar uzun değil.");
    if (h.version != kArcPolylineLayout)
        return err(ErrorCode::Unsupported,
                   "Yaylı çoklu çizgi yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    ArcPolyline def;
    def.constant_width       = in.i64();
    const std::uint32_t arcs = in.u32();
    if (!in.remaining_records(arcs, kArcBytes))
        return err(ErrorCode::ParseError, "Yaylı çoklu çizgi yükü " + std::to_string(arcs) +
                                              " yay bildiriyor, bu kadar veri yok.");
    def.arcs.reserve(arcs);
    for (std::uint32_t i = 0; i < arcs; ++i) {
        ArcPolyline::Arc a;
        a.segment  = in.u32();
        a.centre.x = in.mm();
        a.centre.y = in.mm();
        a.radius   = in.mm();
        a.ccw      = in.u8() != 0;
        if (a.radius <= 0)
            return err(ErrorCode::ValidationFailed,
                       "Yay kenar yarıçapı sıfır ya da negatif: " + std::to_string(a.radius));
        if (!def.arcs.empty() && a.segment <= def.arcs.back().segment)
            return err(ErrorCode::ValidationFailed,
                       "Yay kenarlar kenar numarasına göre artan sırada olmalı, her kenarda en "
                       "çok bir yay.");
        def.arcs.push_back(a);
    }
    if (in.left() != 0)
        return err(ErrorCode::ParseError, "Yaylı çoklu çizgi yükünün sonunda " +
                                              std::to_string(in.left()) + " fazla bayt var.");
    return def;
}

Result<ArcPolyline> arc_polyline_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_arc_polyline(geom.payload_of(slot));
}

void arc_polyline_points(std::span<const Point2> vertices, bool closed, const ArcPolyline& def,
                         std::vector<Mm>& xs, std::vector<Mm>& ys)
{
    xs.clear();
    ys.clear();
    const std::size_t n = vertices.size();
    if (n == 0) return;
    const std::size_t segs = closed ? n : n - 1;
    std::size_t next_arc   = 0;
    std::vector<Mm> ax;
    std::vector<Mm> ay;
    for (std::size_t s = 0; s < n; ++s) {
        xs.push_back(vertices[s].x);
        ys.push_back(vertices[s].y);
        if (s >= segs) break;
        while (next_arc < def.arcs.size() && def.arcs[next_arc].segment < s)
            ++next_arc;
        if (next_arc >= def.arcs.size() || def.arcs[next_arc].segment != s) continue;
        const ArcPolyline::Arc& arc = def.arcs[next_arc];
        const Point2 a              = vertices[s];
        const Point2 b              = vertices[s + 1 == n ? 0 : s + 1];
        ax.clear();
        ay.clear();
        // `arc_outline` sweeps counter-clockwise from its start to its end; a
        // clockwise edge is the same points walked backwards.
        if (arc.ccw)
            arc_outline(arc.centre, arc.radius, a, b, ax, ay);
        else
            arc_outline(arc.centre, arc.radius, b, a, ax, ay);
        if (!arc.ccw) {
            std::reverse(ax.begin(), ax.end());
            std::reverse(ay.begin(), ay.end());
        }
        // Both ends of the outline are the vertices themselves.
        for (std::size_t k = 1; k + 1 < ax.size(); ++k) {
            xs.push_back(ax[k]);
            ys.push_back(ay[k]);
        }
    }
}

bool arc_polyline_outline(const RingGeometry& geom, std::uint32_t slot, std::vector<Mm>& xs,
                          std::vector<Mm>& ys)
{
    xs.clear();
    ys.clear();
    const Verts v = verts_of(geom, slot);
    std::vector<Point2> pts(v.xs.size());
    for (std::size_t i = 0; i < v.xs.size(); ++i)
        pts[i] = Point2{v.xs[i], v.ys[i]};
    auto def = arc_polyline_of(geom, slot);
    if (!def) {
        // Undecodable bytes never pass validate; drawn as the bare vertices
        // rather than not at all (model.md R26).
        for (const Point2& p : pts) {
            xs.push_back(p.x);
            ys.push_back(p.y);
        }
        return v.closed;
    }
    arc_polyline_points(pts, v.closed, def.value(), xs, ys);
    return v.closed;
}

KENTOS_KIND(arc_polyline)
{
    KindSpec s{};
    s.id         = kArcPolylineKind;
    s.stable_id  = "core.arc_polyline";
    s.summary_tr = "Kenarları yay olabilen çoklu çizgi ya da alan; DXF şişkinliğinin türü.";
    s.names[0]   = "YAYLIÇİZGİ";
    s.names[1]   = "YAYLICIZGI";
    s.names[2]   = "ARCPOLYLINE";
    s.names[3]   = "YPL";
    s.bbox       = &ap_bbox;
    s.outline    = &ap_outline;
    s.hit        = &ap_hit;
    s.area       = &ap_area;
    s.perimeter  = &ap_perimeter;
    s.read       = &ap_read;
    s.write      = &ap_write;
    s.validate   = &ap_validate;
    s.key_points = &ap_key_points;
    return s;
}

} // namespace kentos::core
