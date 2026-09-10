// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/spline.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace kentos::core {
namespace {

constexpr std::uint16_t kFlagClosed   = 1u << 0;
constexpr std::uint16_t kFlagPeriodic = 1u << 1;
constexpr std::uint16_t kFlagRational = 1u << 2;
constexpr std::uint16_t kFlagHasFit   = 1u << 3;
constexpr std::uint16_t kFlagPlanar   = 1u << 4;
constexpr std::uint16_t kFlagLinear   = 1u << 5;
constexpr int kSamplesPerSpan         = 16;
constexpr int kMaxDegree              = 15;
constexpr std::size_t kMaxControls    = 100000;

std::vector<Point2> controls_of(const RingGeometry& geom, std::uint32_t slot)
{
    std::vector<Point2> pts;
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return pts;
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    pts.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i)
        pts.push_back(Point2{xs[i], ys[i]});
    return pts;
}

void sp_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (const std::uint32_t slot : slots) {
        const bool closed = spline_outline(geom, slot, xs, ys);
        into.begin_run(closed);
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
}

void sp_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        spline_outline(geom, slots[i], xs, ys);
        Box2 box = kind::box_of_points(xs, ys);
        // The control polygon too, so a degenerate definition is still
        // somewhere and zoom-to-extents finds it.
        if (box.empty()) box = geom.bounds_of(slots[i]);
        out[i] = box;
    }
}

void sp_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        const std::uint32_t one[1]{slots[i]};
        sp_outline(geom, SlotSpan(one, 1), runs);
        out[i] = kind::runs_hit(runs, probe, tolerance, false) ? 1 : 0;
    }
}

void sp_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i] = 0;
        // A closed spline encloses what its DRAWN form encloses — an
        // approximation, and the page says so; an open one encloses nothing.
        if (!spline_outline(geom, slots[i], xs, ys)) continue;
        const double area     = std::abs(kind::run_area2(xs, ys)) / 2.0;
        constexpr double kMax = 9.0e18;
        out[i] = area >= kMax ? static_cast<Mm2>(kMax) : static_cast<Mm2>(std::llround(area));
    }
}

void sp_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const bool closed = spline_outline(geom, slots[i], xs, ys);
        out[i]            = kind::run_length(xs, ys, closed);
    }
}

void sp_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
              std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        put_u32(bytes, rs.count);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            put_u32(bytes, static_cast<std::uint32_t>(xs.size()));
            for (std::size_t v = 0; v < xs.size(); ++v) {
                put_mm(bytes, xs[v]);
                put_mm(bytes, ys[v]);
            }
        }
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> sp_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError, "Spline yükü halka sayısını taşımıyor.");
    const std::uint32_t ring_count = in.u32();
    if (ring_count < 1 || ring_count > 2)
        return err(ErrorCode::ParseError, "Spline bir ya da iki halka ister; bildirilen " +
                                              std::to_string(ring_count) + ".");
    std::vector<std::vector<Point2>> store(ring_count);
    std::vector<RingGeometry::RingInput> rings(ring_count);
    for (std::uint32_t r = 0; r < ring_count; ++r) {
        if (!in.remaining(4))
            return err(ErrorCode::ParseError, "Spline halkası " + std::to_string(r) + " eksik.");
        const std::uint32_t verts = in.u32();
        if (!in.remaining_records(verts, 16))
            return err(ErrorCode::ParseError, "Spline halkası için " + std::to_string(verts) +
                                                  " nokta bildirilmiş, bu kadar veri yok.");
        store[r].resize(verts);
        for (std::uint32_t v = 0; v < verts; ++v) {
            store[r][v].x = in.mm();
            store[r][v].y = in.mm();
        }
        rings[r].points = std::span<const Point2>(store[r]);
        rings[r].role   = RingRole::Open;
        rings[r].part   = 0;
    }
    return geom.append(std::span<const RingGeometry::RingInput>(rings));
}

Status sp_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    if (rings.empty() || rings.size() > 2)
        return err(ErrorCode::ValidationFailed,
                   "Spline kontrol noktaları halkası ve isteğe bağlı uydurma noktaları halkası "
                   "ister; verilen " +
                       std::to_string(rings.size()) + " halka.");
    for (const RingGeometry::RingInput& r : rings)
        if (r.role != RingRole::Open)
            return err(ErrorCode::ValidationFailed,
                       "Spline halkaları açıktır; kapalılık yükün bayrağındadır.");
    auto def = decode_spline(payload);
    if (!def) return def.error();
    const std::size_t n = rings[0].points.size();
    if (n > kMaxControls)
        return err(ErrorCode::ValidationFailed,
                   "Spline en çok " + std::to_string(kMaxControls) + " kontrol noktası taşır.");
    if (n < static_cast<std::size_t>(def.value().degree) + 1)
        return err(ErrorCode::ValidationFailed,
                   std::to_string(static_cast<int>(def.value().degree)) +
                       ". dereceden spline en az " +
                       std::to_string(static_cast<int>(def.value().degree) + 1) +
                       " kontrol noktası ister; verilen " + std::to_string(n) + ".");
    if (!def.value().knots_nano.empty() &&
        def.value().knots_nano.size() != n + def.value().degree + 1)
        return err(ErrorCode::ValidationFailed,
                   "Düğüm sayısı kontrol noktası + derece + 1 olmalı: " +
                       std::to_string(n + def.value().degree + 1) + " beklenir, verilen " +
                       std::to_string(def.value().knots_nano.size()) + ".");
    if (!def.value().weights_nano.empty() && def.value().weights_nano.size() != n)
        return err(ErrorCode::ValidationFailed,
                   "Her kontrol noktasına bir ağırlık düşer: " + std::to_string(n) +
                       " beklenir, verilen " + std::to_string(def.value().weights_nano.size()) +
                       ".");
    if (def.value().has_fit != (rings.size() == 2))
        return err(ErrorCode::ValidationFailed,
                   "Uydurma noktaları bayrağı ile ikinci halka birbirini tutmuyor.");
    return ok();
}

void sp_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    const bool closed = spline_outline(geom, slot, xs, ys);
    if (!xs.empty() && !closed) {
        kind::offer(into, Point2{xs.front(), ys.front()}, kind::kKeyEndpoint);
        kind::offer(into, Point2{xs.back(), ys.back()}, kind::kKeyEndpoint);
    }
    // Control and fit points are what the curve was drawn from: reached as
    // NODES, the way a surveyed point is, so they never masquerade as an end.
    const RingSpan rs = geom.rings_of(slot);
    for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
        const auto rx = geom.ring_xs(r);
        const auto ry = geom.ring_ys(r);
        for (std::size_t i = 0; i < rx.size(); ++i)
            kind::offer(into, Point2{rx[i], ry[i]}, kind::kKeyNode);
    }
}

} // namespace

std::vector<std::uint8_t> encode_spline(const SplineDef& def)
{
    std::vector<std::uint8_t> out;
    std::uint16_t flags = 0;
    if (def.closed) flags |= kFlagClosed;
    if (def.periodic) flags |= kFlagPeriodic;
    if (def.rational) flags |= kFlagRational;
    if (def.has_fit) flags |= kFlagHasFit;
    if (def.planar) flags |= kFlagPlanar;
    if (def.linear) flags |= kFlagLinear;
    kind::put_header(out, kSplineLayout, flags);
    put_u8(out, def.degree);
    put_u32(out, static_cast<std::uint32_t>(def.knots_nano.size()));
    for (const std::int64_t k : def.knots_nano)
        put_i64(out, k);
    put_u32(out, static_cast<std::uint32_t>(def.weights_nano.size()));
    for (const std::int64_t w : def.weights_nano)
        put_i64(out, w);
    return out;
}

Result<SplineDef> decode_spline(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(5))
        return err(ErrorCode::ParseError,
                   "Spline yükü başlığı, dereceyi ve düğüm sayısını taşıyacak kadar uzun değil.");
    if (h.version != kSplineLayout)
        return err(ErrorCode::Unsupported,
                   "Spline yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    SplineDef def;
    def.closed   = (h.flags & kFlagClosed) != 0;
    def.periodic = (h.flags & kFlagPeriodic) != 0;
    def.rational = (h.flags & kFlagRational) != 0;
    def.has_fit  = (h.flags & kFlagHasFit) != 0;
    def.planar   = (h.flags & kFlagPlanar) != 0;
    def.linear   = (h.flags & kFlagLinear) != 0;
    def.degree   = in.u8();
    if (def.degree < 1 || def.degree > kMaxDegree)
        return err(ErrorCode::ValidationFailed,
                   "Spline derecesi 1 ile 15 arasında olmalı; verilen " +
                       std::to_string(static_cast<int>(def.degree)) + ".");
    const std::uint32_t knots = in.u32();
    if (!in.remaining_records(knots, 8) || knots > kMaxControls + kMaxDegree + 1)
        return err(ErrorCode::ParseError, "Spline yükü " + std::to_string(knots) +
                                              " düğüm bildiriyor, bu kadar veri yok.");
    def.knots_nano.reserve(knots);
    for (std::uint32_t i = 0; i < knots; ++i) {
        const std::int64_t k = in.i64();
        if (!def.knots_nano.empty() && k < def.knots_nano.back())
            return err(ErrorCode::ValidationFailed, "Spline düğümleri azalmayan sırada olmalı.");
        def.knots_nano.push_back(k);
    }
    if (!in.remaining(4))
        return err(ErrorCode::ParseError, "Spline yükü ağırlık sayısını taşımıyor.");
    const std::uint32_t weights = in.u32();
    if (!in.remaining_records(weights, 8) || weights > kMaxControls)
        return err(ErrorCode::ParseError, "Spline yükü " + std::to_string(weights) +
                                              " ağırlık bildiriyor, bu kadar veri yok.");
    def.weights_nano.reserve(weights);
    for (std::uint32_t i = 0; i < weights; ++i) {
        const std::int64_t w = in.i64();
        if (w <= 0)
            return err(ErrorCode::ValidationFailed, "Spline ağırlıkları sıfırdan büyük olmalı.");
        def.weights_nano.push_back(w);
    }
    if (in.left() != 0)
        return err(ErrorCode::ParseError,
                   "Spline yükünün sonunda " + std::to_string(in.left()) + " fazla bayt var.");
    return def;
}

Result<SplineDef> spline_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_spline(geom.payload_of(slot));
}

std::vector<std::int64_t> uniform_clamped_knots(std::size_t controls, int degree)
{
    std::vector<std::int64_t> knots;
    if (degree < 1 || controls < static_cast<std::size_t>(degree) + 1) return knots;
    const auto p        = static_cast<std::size_t>(degree);
    const std::size_t m = controls + p + 1;
    // The interior knots split the unit interval evenly: `controls - p` spans.
    const auto spans = static_cast<std::int64_t>(controls - p);
    knots.reserve(m);
    for (std::size_t i = 0; i < m; ++i) {
        if (i <= p)
            knots.push_back(0);
        else if (i >= controls)
            knots.push_back(kNano);
        else
            knots.push_back((static_cast<std::int64_t>(i - p) * kNano) / spans);
    }
    return knots;
}

void spline_points(std::span<const Point2> controls, const SplineDef& def, int samples_per_span,
                   std::vector<Mm>& xs, std::vector<Mm>& ys)
{
    xs.clear();
    ys.clear();
    const std::size_t n = controls.size();
    const auto p        = static_cast<std::size_t>(def.degree);
    if (def.degree < 1 || n < p + 1 || samples_per_span < 1) return;
    std::vector<std::int64_t> knots_nano = def.knots_nano;
    if (knots_nano.empty()) knots_nano = uniform_clamped_knots(n, def.degree);
    if (knots_nano.size() != n + p + 1) return;
    if (!def.weights_nano.empty() && def.weights_nano.size() != n) return;

    // THE FRAME IS THE FIRST CONTROL POINT (core.md R3): a TUREF coordinate is
    // seven digits of metres, and multiplying two of them in a double loses the
    // millimetres this promises. Everything below is a difference from `origin`.
    const Point2 origin = controls[0];
    std::vector<double> knots(knots_nano.size());
    for (std::size_t i = 0; i < knots.size(); ++i)
        knots[i] = static_cast<double>(knots_nano[i]) / static_cast<double>(kNano);

    // Homogeneous control points: (w·x, w·y, w). Non-rational is w = 1.
    std::vector<double> hx(n);
    std::vector<double> hy(n);
    std::vector<double> hw(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = def.weights_nano.empty() ? 1.0
                                                  : static_cast<double>(def.weights_nano[i]) /
                                                        static_cast<double>(kNano);
        hx[i]          = static_cast<double>(controls[i].x - origin.x) * w;
        hy[i]          = static_cast<double>(controls[i].y - origin.y) * w;
        hw[i]          = w;
    }

    std::vector<double> dx(p + 1);
    std::vector<double> dy(p + 1);
    std::vector<double> dw(p + 1);
    // The last non-empty span carries the curve's end point exactly once.
    std::size_t last_span = 0;
    for (std::size_t span = p; span < n; ++span)
        if (knots[span + 1] > knots[span]) last_span = span;

    for (std::size_t span = p; span < n; ++span) {
        const double u0 = knots[span];
        const double u1 = knots[span + 1];
        if (!(u1 > u0)) continue; // a repeated knot draws nothing
        const bool last = span == last_span;
        for (int s = 0; s < samples_per_span + (last ? 1 : 0); ++s) {
            const double u =
                u0 + (u1 - u0) * (static_cast<double>(s) / static_cast<double>(samples_per_span));
            // de Boor: p rounds of convex combinations over the span's p+1 points.
            for (std::size_t j = 0; j <= p; ++j) {
                dx[j] = hx[span - p + j];
                dy[j] = hy[span - p + j];
                dw[j] = hw[span - p + j];
            }
            for (std::size_t r = 1; r <= p; ++r) {
                for (std::size_t j = p; j >= r; --j) {
                    const std::size_t i = span - p + j;
                    const double den    = knots[i + p - r + 1] - knots[i];
                    const double alpha  = den == 0.0 ? 0.0 : (u - knots[i]) / den;
                    dx[j]               = (1.0 - alpha) * dx[j - 1] + alpha * dx[j];
                    dy[j]               = (1.0 - alpha) * dy[j - 1] + alpha * dy[j];
                    dw[j]               = (1.0 - alpha) * dw[j - 1] + alpha * dw[j];
                }
            }
            if (dw[p] == 0.0) continue;
            const Mm x = origin.x + mm_round(dx[p] / dw[p]);
            const Mm y = origin.y + mm_round(dy[p] / dw[p]);
            if (!xs.empty() && xs.back() == x && ys.back() == y) continue;
            xs.push_back(x);
            ys.push_back(y);
        }
    }
    // A closed curve that ends where it began does not repeat its start.
    if (def.closed && xs.size() > 1 && xs.front() == xs.back() && ys.front() == ys.back()) {
        xs.pop_back();
        ys.pop_back();
    }
}

bool spline_outline(const RingGeometry& geom, std::uint32_t slot, std::vector<Mm>& xs,
                    std::vector<Mm>& ys)
{
    xs.clear();
    ys.clear();
    const std::vector<Point2> controls = controls_of(geom, slot);
    auto def                           = spline_of(geom, slot);
    if (!def) {
        for (const Point2& p : controls) {
            xs.push_back(p.x);
            ys.push_back(p.y);
        }
        return false;
    }
    spline_points(controls, def.value(), kSamplesPerSpan, xs, ys);
    return def.value().closed;
}

KENTOS_KIND(spline)
{
    KindSpec s{};
    s.id         = kSplineKind;
    s.stable_id  = "core.spline";
    s.summary_tr = "Kontrol noktaları, derece ve düğümlerle tanımlı NURBS eğrisi.";
    s.names[0]   = "SPLINE";
    s.names[1]   = "SPLINE";
    s.names[2]   = "SPLINE";
    s.names[3]   = "SPL";
    s.bbox       = &sp_bbox;
    s.outline    = &sp_outline;
    s.hit        = &sp_hit;
    s.area       = &sp_area;
    s.perimeter  = &sp_perimeter;
    s.read       = &sp_read;
    s.write      = &sp_write;
    s.validate   = &sp_validate;
    s.key_points = &sp_key_points;
    return s;
}

} // namespace kentos::core
