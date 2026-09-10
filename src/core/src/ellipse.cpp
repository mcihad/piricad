// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"
#include <algorithm>
#include "kind_common.hpp"

#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/units.hpp"

namespace kentos::core {
namespace {

Point2 vertex_at(const RingGeometry& geom, std::uint32_t slot, std::size_t i)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};

    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (i >= xs.size()) return Point2{};
    return Point2{xs[i], ys[i]};
}

} // namespace

Point2 ellipse_centre_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 0);
}

Point2 ellipse_major_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 1);
}

Point2 ellipse_minor_of(const RingGeometry& geom, std::uint32_t slot)
{
    return vertex_at(geom, slot, 2);
}

void ellipse_outline(Point2 centre, Point2 major, Point2 minor, std::vector<Mm>& xs,
                     std::vector<Mm>& ys)
{
    // The two axis VECTORS. Together they carry the lengths and the rotation, so
    // nothing here needs an angle — which is what keeps the run deterministic.
    const auto ax = static_cast<double>(major.x - centre.x);
    const auto ay = static_cast<double>(major.y - centre.y);
    const auto bx = static_cast<double>(minor.x - centre.x);
    const auto by = static_cast<double>(minor.y - centre.y);

    // The circle's own table: `(cos t, sin t)` for `kCircleSegments` values of t,
    // built by repeated exact bisection of the four axis directions. Scaling it
    // along the two axes is exactly an ellipse, and it costs four multiplies.
    const std::vector<std::pair<double, double>>& unit = unit_circle();

    xs.reserve(xs.size() + unit.size());
    ys.reserve(ys.size() + unit.size());

    for (const auto& u : unit) {
        xs.push_back(centre.x + mm_round(ax * u.first + bx * u.second));
        ys.push_back(centre.y + mm_round(ay * u.first + by * u.second));
    }
}

std::vector<std::uint8_t> encode_ellipse_arc(const EllipseArc& arc)
{
    std::vector<std::uint8_t> out;
    kind::put_header(out, kEllipseArcLayout, 0);
    put_i64(out, arc.start_udeg);
    put_i64(out, arc.end_udeg);
    return out;
}

Result<EllipseArc> decode_ellipse_arc(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(16) || payload.size() != kind::kHeaderBytes + 16)
        return err(ErrorCode::ParseError, "Elips yayı yükü başlık ve iki açıyı taşıyacak 24 bayt "
                                          "değil; verilen " +
                                              std::to_string(payload.size()) + " bayt.");
    if (h.version != kEllipseArcLayout)
        return err(ErrorCode::Unsupported, "Elips yayı yükünün düzeni bu yapının tanımadığı bir "
                                           "sürümde: " +
                                               std::to_string(h.version));
    EllipseArc arc;
    arc.start_udeg = in.i64();
    arc.end_udeg   = in.i64();
    if (arc.start_udeg < 0 || arc.start_udeg >= kUDegFullCircle || arc.end_udeg < 0 ||
        arc.end_udeg >= kUDegFullCircle)
        return err(ErrorCode::ValidationFailed,
                   "Elips yayının açıları 0 ile 360 derece arasında olmalı.");
    if (arc.start_udeg == arc.end_udeg)
        return err(ErrorCode::ValidationFailed,
                   "Elips yayının başlangıcı ve bitişi aynı; tam elips için yük verilmez.");
    return arc;
}

std::optional<EllipseArc> ellipse_arc_of(const RingGeometry& geom, std::uint32_t slot)
{
    const auto bytes = geom.payload_of(slot);
    if (bytes.empty()) return std::nullopt;
    auto arc = decode_ellipse_arc(bytes);
    if (!arc) return std::nullopt;
    return arc.value();
}

void ellipse_arc_outline(Point2 centre, Point2 major, Point2 minor, std::int64_t start_udeg,
                         std::int64_t end_udeg, std::vector<Mm>& xs, std::vector<Mm>& ys)
{
    std::int64_t sweep = end_udeg - start_udeg;
    while (sweep <= 0)
        sweep += kUDegFullCircle;
    const std::int64_t step_max = kUDegFullCircle / 128;
    const auto steps            = std::max<std::int64_t>(1, (sweep + step_max - 1) / step_max);
    const auto ax               = static_cast<double>(major.x - centre.x);
    const auto ay               = static_cast<double>(major.y - centre.y);
    const auto bx               = static_cast<double>(minor.x - centre.x);
    const auto by               = static_cast<double>(minor.y - centre.y);
    for (std::int64_t i = 0; i <= steps; ++i) {
        const std::int64_t at = start_udeg + (sweep * i) / steps;
        const SinCos t        = sin_cos_udeg(at);
        xs.push_back(centre.x + mm_round(t.cos * ax + t.sin * bx));
        ys.push_back(centre.y + mm_round(t.cos * ay + t.sin * by));
    }
}

} // namespace kentos::core
