// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/dimension.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace kentos::core {
namespace {

constexpr std::uint16_t kFlagUserText  = 1u << 0;
constexpr std::uint16_t kFlagOrdinateX = 1u << 1;
constexpr std::uint16_t kFlagArrow     = 1u << 0;
constexpr std::uint16_t kFlagSpline    = 1u << 1;
constexpr std::size_t kMaxText         = 255;

/// A direction as a pair of doubles, of unit length; zero when the points meet.
struct Dir
{
    double x{0};
    double y{0};

    bool zero() const noexcept { return x == 0.0 && y == 0.0; }

    Dir perp() const noexcept { return Dir{-y, x}; }
};

Dir unit_between(Point2 a, Point2 b)
{
    const auto dx  = static_cast<double>(b.x - a.x);
    const auto dy  = static_cast<double>(b.y - a.y);
    const double l = std::sqrt(dx * dx + dy * dy);
    if (l == 0.0) return Dir{};
    return Dir{dx / l, dy / l};
}

Dir unit_at(std::int64_t udeg)
{
    const SinCos t = sin_cos_udeg(udeg);
    return Dir{t.cos, t.sin};
}

Point2 along(Point2 p, Dir d, double by)
{
    return Point2{p.x + mm_round(d.x * by), p.y + mm_round(d.y * by)};
}

double dot(Point2 origin, Point2 p, Dir d)
{
    return static_cast<double>(p.x - origin.x) * d.x + static_cast<double>(p.y - origin.y) * d.y;
}

double distance(Point2 a, Point2 b)
{
    const auto dx = static_cast<double>(b.x - a.x);
    const auto dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

void line(EmitBuffer& into, Point2 a, Point2 b)
{
    into.begin_run(false);
    into.push_vertex(a.x, a.y);
    into.push_vertex(b.x, b.y);
}

std::vector<Point2> ring_points(const RingGeometry& geom, std::uint32_t slot, std::uint32_t index)
{
    std::vector<Point2> pts;
    const RingSpan rs = geom.rings_of(slot);
    if (index >= rs.count) return pts;
    const auto xs = geom.ring_xs(rs.first + index);
    const auto ys = geom.ring_ys(rs.first + index);
    for (std::size_t i = 0; i < xs.size(); ++i)
        pts.push_back(Point2{xs[i], ys[i]});
    return pts;
}

/// Where two lines cross, or nothing when parallel. Doubles over differences
/// from `a`, rounded once.
bool intersect(Point2 a, Point2 b, Point2 c, Point2 d, Point2& out)
{
    const auto r_x   = static_cast<double>(b.x - a.x);
    const auto r_y   = static_cast<double>(b.y - a.y);
    const auto s_x   = static_cast<double>(d.x - c.x);
    const auto s_y   = static_cast<double>(d.y - c.y);
    const double den = r_x * s_y - r_y * s_x;
    if (den == 0.0) return false;
    const auto acx = static_cast<double>(c.x - a.x);
    const auto acy = static_cast<double>(c.y - a.y);
    const double t = (acx * s_y - acy * s_x) / den;
    out            = Point2{a.x + mm_round(r_x * t), a.y + mm_round(r_y * t)};
    return true;
}

/// The angle at `vertex` swept counter-clockwise from `p1` to `p2`, or the
/// other way when `arc_pt` lies on the other side — the side the arc is drawn.
std::int64_t angle_at(Point2 vertex, Point2 p1, Point2 p2, Point2 arc_pt, bool& from_p1)
{
    from_p1 = on_arc(vertex, p1, p2, arc_pt);
    return from_p1 ? arc_sweep_udeg(vertex, p1, p2) : arc_sweep_udeg(vertex, p2, p1);
}

std::string with_decimals(std::int64_t scaled, unsigned precision, char separator, bool negative)
{
    std::string digits = std::to_string(scaled);
    if (precision > 0) {
        while (digits.size() <= precision)
            digits.insert(digits.begin(), '0');
        digits.insert(digits.end() - static_cast<std::ptrdiff_t>(precision), separator);
    }
    return negative ? "-" + digits : digits;
}

// ---------------------------------------------------------- kind functions ---

void dm_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    for (const std::uint32_t slot : slots)
        dimension_outline(geom, slot, into);
}

void dm_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        dimension_outline(geom, slots[i], runs);
        Box2 box = kind::box_of_runs(runs);
        box.extend(geom.bounds_of(slots[i])); // the baseline and the points themselves
        out[i] = box;
    }
}

void dm_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        dimension_outline(geom, slots[i], runs);
        // The baseline too, so the text is a handle on the whole thing.
        const std::vector<Point2> base = ring_points(geom, slots[i], 0);
        if (base.size() >= 2) line(runs, base[0], base[1]);
        out[i] = kind::runs_hit(runs, probe, tolerance, false) ? 1 : 0;
    }
}

void zero_area(const RingGeometry&, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm2{0};
}

void zero_perimeter(const RingGeometry&, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm{0};
}

void rings_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
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

Result<std::uint32_t> rings_read(RingGeometry& geom, std::span<const std::uint8_t> payload,
                                 const char* what, std::uint32_t most_rings)
{
    WireReader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError, std::string(what) + " yükü halka sayısını taşımıyor.");
    const std::uint32_t ring_count = in.u32();
    if (ring_count == 0 || ring_count > most_rings)
        return err(ErrorCode::ParseError,
                   std::string(what) + " en çok " + std::to_string(most_rings) +
                       " halka ister; bildirilen " + std::to_string(ring_count) + ".");
    std::vector<std::vector<Point2>> store(ring_count);
    std::vector<RingGeometry::RingInput> rings(ring_count);
    for (std::uint32_t r = 0; r < ring_count; ++r) {
        if (!in.remaining(4))
            return err(ErrorCode::ParseError,
                       std::string(what) + " halkası " + std::to_string(r) + " eksik.");
        const std::uint32_t verts = in.u32();
        if (!in.remaining_records(verts, 16))
            return err(ErrorCode::ParseError, std::string(what) + " halkası için " +
                                                  std::to_string(verts) +
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

Result<std::uint32_t> dm_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    return rings_read(geom, payload, "Ölçü", 2);
}

Status dm_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    auto def = decode_dimension(payload);
    if (!def) return def.error();
    if (rings.size() != 2)
        return err(ErrorCode::ValidationFailed,
                   "Ölçü iki halka ister: yazı taban çizgisi ve tanım noktaları; verilen " +
                       std::to_string(rings.size()) + ".");
    if (rings[0].role != RingRole::Open || rings[0].points.size() != 2)
        return err(ErrorCode::ValidationFailed,
                   "Ölçünün ilk halkası iki noktalı açık yazı taban çizgisidir.");
    const std::size_t want = dimension_point_count(def.value().type);
    if (rings[1].role != RingRole::Open || rings[1].points.size() != want)
        return err(ErrorCode::ValidationFailed, std::string(dimension_type_name(def.value().type)) +
                                                    " ölçüsü " + std::to_string(want) +
                                                    " tanım noktası ister; verilen " +
                                                    std::to_string(rings[1].points.size()) + ".");
    return ok();
}

void dm_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    for (const Point2& p : ring_points(geom, slot, 1))
        kind::offer(into, p, kind::kKeyEndpoint);
}

void ld_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    for (const std::uint32_t slot : slots)
        leader_outline(geom, slot, into);
}

void ld_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        leader_outline(geom, slots[i], runs);
        out[i] = kind::box_of_runs(runs);
    }
}

void ld_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        leader_outline(geom, slots[i], runs);
        out[i] = kind::runs_hit(runs, probe, tolerance, false) ? 1 : 0;
    }
}

void ld_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    // A leader's length is its line's; the arrowhead is a mark, not a distance.
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.perimeter_of(slots[i]);
}

Result<std::uint32_t> ld_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    return rings_read(geom, payload, "Lider", 1);
}

Status ld_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    auto def = decode_leader(payload);
    if (!def) return def.error();
    if (rings.size() != 1 || rings[0].role != RingRole::Open || rings[0].points.size() < 2)
        return err(ErrorCode::ValidationFailed,
                   "Lider en az iki noktalı tek bir açık halka ister.");
    return ok();
}

void ld_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    for (const Point2& p : ring_points(geom, slot, 0))
        kind::offer(into, p, kind::kKeyEndpoint);
}

} // namespace

const char* dimension_type_name(DimensionType t) noexcept
{
    switch (t) {
    case DimensionType::Linear: return "dogrusal";
    case DimensionType::Aligned: return "hizali";
    case DimensionType::Angular: return "acisal";
    case DimensionType::Diametric: return "cap";
    case DimensionType::Radial: return "yaricap";
    case DimensionType::Angular3P: return "acisal3";
    case DimensionType::Ordinate: return "ordinat";
    }
    return "hizali";
}

std::size_t dimension_point_count(DimensionType t) noexcept
{
    switch (t) {
    case DimensionType::Angular: return 5;
    case DimensionType::Diametric:
    case DimensionType::Radial: return 2;
    case DimensionType::Angular3P: return 4;
    case DimensionType::Linear:
    case DimensionType::Aligned:
    case DimensionType::Ordinate: break;
    }
    return 3;
}

std::vector<std::uint8_t> encode_dimension(const DimensionDef& def)
{
    std::vector<std::uint8_t> out;
    std::uint16_t flags = 0;
    if (def.user_text_position) flags |= kFlagUserText;
    if (def.ordinate_x) flags |= kFlagOrdinateX;
    kind::put_header(out, kDimensionLayout, flags);
    put_u8(out, static_cast<std::uint8_t>(def.type));
    put_u8(out, static_cast<std::uint8_t>(def.arrow));
    put_u8(out, def.precision);
    put_u8(out, static_cast<std::uint8_t>(def.decimal_separator));
    put_i64(out, def.rotation_udeg);
    put_i64(out, def.measurement);
    put_mm(out, def.arrow_size);
    put_mm(out, def.extension_beyond);
    put_mm(out, def.extension_offset);
    put_mm(out, def.text_gap);
    kind::put_string(out, def.style.size() > kMaxText ? def.style.substr(0, kMaxText) : def.style);
    kind::put_string(out, def.override_text.size() > kMaxText
                              ? def.override_text.substr(0, kMaxText)
                              : def.override_text);
    return out;
}

Result<DimensionDef> decode_dimension(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(4 + 8 * 6))
        return err(ErrorCode::ParseError,
                   "Ölçü yükü başlığı ve sayılarını taşıyacak kadar uzun değil.");
    if (h.version != kDimensionLayout)
        return err(ErrorCode::Unsupported,
                   "Ölçü yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    DimensionDef def;
    def.user_text_position   = (h.flags & kFlagUserText) != 0;
    def.ordinate_x           = (h.flags & kFlagOrdinateX) != 0;
    const std::uint8_t type  = in.u8();
    const std::uint8_t arrow = in.u8();
    if (type > static_cast<std::uint8_t>(DimensionType::Ordinate))
        return err(ErrorCode::ValidationFailed, "Bilinmeyen ölçü türü: " + std::to_string(type));
    if (arrow > static_cast<std::uint8_t>(ArrowStyle::Tick))
        return err(ErrorCode::ValidationFailed, "Bilinmeyen ok biçimi: " + std::to_string(arrow));
    def.type              = static_cast<DimensionType>(type);
    def.arrow             = static_cast<ArrowStyle>(arrow);
    def.precision         = in.u8();
    def.decimal_separator = static_cast<char>(in.u8());
    if (def.precision > 8)
        return err(ErrorCode::ValidationFailed, "Ölçü en çok 8 ondalık basamak yazar.");
    if (def.decimal_separator != ',' && def.decimal_separator != '.')
        return err(ErrorCode::ValidationFailed, "Ondalık ayracı virgül ya da nokta olmalı.");
    def.rotation_udeg    = in.i64();
    def.measurement      = in.i64();
    def.arrow_size       = in.mm();
    def.extension_beyond = in.mm();
    def.extension_offset = in.mm();
    def.text_gap         = in.mm();
    if (def.arrow_size < 0 || def.extension_beyond < 0 || def.extension_offset < 0 ||
        def.text_gap < 0)
        return err(ErrorCode::ValidationFailed, "Ölçü stilinin uzunlukları negatif olamaz.");
    if (!kind::read_string(in, def.style, kMaxText))
        return err(ErrorCode::ParseError, "Ölçü stil adı okunamadı.");
    if (!kind::read_string(in, def.override_text, kMaxText))
        return err(ErrorCode::ParseError, "Ölçünün elle yazılan metni okunamadı.");
    if (in.left() != 0)
        return err(ErrorCode::ParseError,
                   "Ölçü yükünün sonunda " + std::to_string(in.left()) + " fazla bayt var.");
    return def;
}

Result<DimensionDef> dimension_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_dimension(geom.payload_of(slot));
}

std::int64_t dimension_measure(DimensionType t, std::span<const Point2> defs,
                               std::int64_t rotation_udeg) noexcept
{
    if (defs.size() < dimension_point_count(t)) return 0;
    switch (t) {
    case DimensionType::Linear: {
        // The separation along the fixed direction, which is a dot product.
        const Dir u    = unit_at(rotation_udeg);
        const double d = dot(defs[0], defs[1], u);
        return mm_round(d < 0.0 ? -d : d);
    }
    case DimensionType::Aligned:
    case DimensionType::Diametric:
    case DimensionType::Radial: return mm_round(distance(defs[0], defs[1]));
    case DimensionType::Angular3P: {
        bool from_p1 = false;
        return angle_at(defs[0], defs[1], defs[2], defs[3], from_p1);
    }
    case DimensionType::Angular: {
        Point2 vertex{};
        if (!intersect(defs[0], defs[1], defs[2], defs[3], vertex)) return 0;
        bool from_p1 = false;
        return angle_at(vertex, defs[1], defs[3], defs[4], from_p1);
    }
    case DimensionType::Ordinate: return 0; // decided by the flag; see dimension_text
    }
    return 0;
}

std::string format_dimension_length(Mm value, DrawingUnit unit, unsigned precision, char separator)
{
    // `mm_from_drawing_units` is `v · num / den`, so units × 10^p is
    // mm · den · 10^p / num, rounded half away from zero, all in integers.
    const UnitRatio r = drawing_unit_ratio(unit);
    std::int64_t pow  = 1;
    for (unsigned i = 0; i < precision && i < 8; ++i)
        pow *= 10;
    const bool negative       = value < 0;
    const std::int64_t mag    = negative ? -value : value;
    const std::int64_t scaled = mul_div_round(mag, r.den * pow, r.num);
    return with_decimals(scaled, precision > 8 ? 8 : precision, separator, negative);
}

std::string format_dimension_angle(std::int64_t udeg, unsigned precision, char separator)
{
    std::int64_t pow = 1;
    for (unsigned i = 0; i < precision && i < 8; ++i)
        pow *= 10;
    const bool negative       = udeg < 0;
    const std::int64_t mag    = negative ? -udeg : udeg;
    const std::int64_t scaled = mul_div_round(mag, pow, 1000000);
    return with_decimals(scaled, precision > 8 ? 8 : precision, separator, negative) + "°";
}

std::string dimension_text(const DimensionDef& def, DrawingUnit unit)
{
    if (!def.override_text.empty()) return def.override_text;
    if (def.type == DimensionType::Angular || def.type == DimensionType::Angular3P)
        return format_dimension_angle(def.measurement, def.precision, def.decimal_separator);
    return format_dimension_length(def.measurement, unit, def.precision, def.decimal_separator);
}

void arrowhead_outline(Point2 tip, Point2 from, Mm size, ArrowStyle style, EmitBuffer& into)
{
    if (size <= 0) return;
    const Dir d = unit_between(from, tip); // pointing at the tip
    if (d.zero()) return;
    const auto len = static_cast<double>(size);
    const Dir n    = d.perp();
    // The base of the head is `size` back from the tip, a third as wide.
    const Point2 back = along(tip, d, -len);
    const Point2 l    = along(back, n, len / 3.0);
    const Point2 r    = along(back, n, -len / 3.0);
    switch (style) {
    case ArrowStyle::Closed:
        into.begin_run(true);
        into.push_vertex(tip.x, tip.y);
        into.push_vertex(l.x, l.y);
        into.push_vertex(r.x, r.y);
        break;
    case ArrowStyle::Open:
        line(into, l, tip);
        line(into, tip, r);
        break;
    case ArrowStyle::Tick: {
        // An oblique stroke through the tip, half the size each way at 45°.
        const Dir diag{(d.x + n.x), (d.y + n.y)};
        const double dl = std::sqrt(diag.x * diag.x + diag.y * diag.y);
        const Dir dd{diag.x / dl, diag.y / dl};
        line(into, along(tip, dd, -len / 2.0), along(tip, dd, len / 2.0));
        break;
    }
    }
}

bool dimension_layout(DimensionDef& def, std::span<const Point2> picks, Point2 where,
                      Mm text_height, DimensionLayout& out)
{
    out = DimensionLayout{};
    if (picks.size() < 2) return false;
    const Point2 p1 = picks[0];
    const Point2 p2 = picks[1];
    if (p1 == p2) return false;
    const double half_text =
        static_cast<double>(def.text_gap) + static_cast<double>(text_height) / 2.0;

    switch (def.type) {
    case DimensionType::Linear:
    case DimensionType::Aligned: {
        out.defs = {p1, p2, where};
        Dir u{1.0, 0.0};
        if (def.type == DimensionType::Aligned) {
            u = unit_between(p1, p2);
        } else {
            // A linear dimension measures horizontally when its line is placed
            // above or below the points, vertically when beside them.
            const Point2 mid{(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
            const Mm dx           = where.x - mid.x;
            const Mm dy           = where.y - mid.y;
            const bool horizontal = (dy < 0 ? -dy : dy) >= (dx < 0 ? -dx : dx);
            def.rotation_udeg     = horizontal ? 0 : kUDegFullCircle / 4;
            u                     = horizontal ? Dir{1.0, 0.0} : Dir{0.0, 1.0};
        }
        const Dir n = u.perp();
        // The feet of the two points on the dimension line, and the side the
        // line lies on.
        const double t1 = dot(p1, where, n);
        const Point2 q1 = along(p1, n, t1);
        const double t2 = dot(p2, where, n);
        const Point2 q2 = along(p2, n, t2);
        const Point2 mid{(q1.x + q2.x) / 2, (q1.y + q2.y) / 2};
        const double side = t1 < 0.0 ? -1.0 : 1.0;
        out.text_centre   = along(mid, n, side * half_text);
        out.text_dir_x    = u.x;
        out.text_dir_y    = u.y;
        break;
    }
    case DimensionType::Radial:
    case DimensionType::Diametric: {
        out.defs        = {p1, p2};
        out.text_centre = where;
        const Dir u     = unit_between(p1, p2);
        out.text_dir_x  = u.x;
        out.text_dir_y  = u.y;
        break;
    }
    case DimensionType::Angular3P: {
        const Point2 apex = picks.size() >= 3 ? picks[2] : p1;
        if (apex == p1 || apex == p2) return false;
        out.defs = {apex, p1, p2, where};
        // The text sits outside the arc, on the bisector through the arc point.
        const Dir outward = unit_between(apex, where);
        if (outward.zero()) return false;
        out.text_centre = along(where, outward, half_text);
        out.text_dir_x  = -outward.y;
        out.text_dir_y  = outward.x;
        break;
    }
    default: return false;
    }
    def.measurement = dimension_measure(def.type, out.defs, def.rotation_udeg);
    return true;
}

bool dimension_picks(DimensionType type, std::span<const Point2> defs, Point2 baseline_start,
                     std::vector<Point2>& picks, Point2& where)
{
    picks.clear();
    if (defs.size() < dimension_point_count(type)) return false;
    switch (type) {
    case DimensionType::Linear:
    case DimensionType::Aligned:
        picks = {defs[0], defs[1]};
        where = defs[2];
        return true;
    case DimensionType::Radial:
    case DimensionType::Diametric:
        picks = {defs[0], defs[1]};
        where = baseline_start;
        return true;
    case DimensionType::Angular3P:
        picks = {defs[1], defs[2], defs[0]};
        where = defs[3];
        return true;
    default: return false;
    }
}

std::array<Point2, 2> dimension_baseline(Point2 centre, double dx, double dy, Mm height,
                                         std::string_view text)
{
    Dir u{dx, dy};
    if (u.x < 0.0 || (u.x == 0.0 && u.y < 0.0)) u = Dir{-u.x, -u.y};
    std::size_t glyphs = 0;
    for (const char c : text)
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) ++glyphs;
    const auto advance =
        static_cast<double>(std::max<Mm>(1, (height * 6 * static_cast<Mm>(glyphs)) / 10));
    return {centre, along(centre, u, advance)};
}

void dimension_outline(const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into)
{
    auto decoded = dimension_of(geom, slot);
    if (!decoded) return;
    const DimensionDef& def        = decoded.value();
    const std::vector<Point2> defs = ring_points(geom, slot, 1);
    if (defs.size() < dimension_point_count(def.type)) return;

    switch (def.type) {
    case DimensionType::Linear:
    case DimensionType::Aligned: {
        const Point2 p1 = defs[0];
        const Point2 p2 = defs[1];
        const Point2 dl = defs[2];
        const Dir u =
            def.type == DimensionType::Aligned ? unit_between(p1, p2) : unit_at(def.rotation_udeg);
        if (u.zero()) return;
        const Dir n = u.perp();
        // Each definition point's foot on the dimension line, which runs
        // through `dl` along `u`.
        const double t1 = dot(p1, dl, n);
        const double t2 = dot(p2, dl, n);
        const Point2 q1 = along(p1, n, t1);
        const Point2 q2 = along(p2, n, t2);
        // Extension lines: from a gap past the point to a little past the line.
        const auto ext = [&](Point2 p, Point2 q, double t) {
            const double sign = t < 0.0 ? -1.0 : 1.0;
            const double gap  = static_cast<double>(def.extension_offset);
            if (std::abs(t) <= gap) return;
            line(into, along(p, n, sign * gap),
                 along(q, n, sign * static_cast<double>(def.extension_beyond)));
        };
        ext(p1, q1, t1);
        ext(p2, q2, t2);
        line(into, q1, q2);
        arrowhead_outline(q1, q2, def.arrow_size, def.arrow, into);
        arrowhead_outline(q2, q1, def.arrow_size, def.arrow, into);
        break;
    }
    case DimensionType::Radial: {
        line(into, defs[0], defs[1]);
        arrowhead_outline(defs[1], defs[0], def.arrow_size, def.arrow, into);
        break;
    }
    case DimensionType::Diametric: {
        line(into, defs[0], defs[1]);
        arrowhead_outline(defs[0], defs[1], def.arrow_size, def.arrow, into);
        arrowhead_outline(defs[1], defs[0], def.arrow_size, def.arrow, into);
        break;
    }
    case DimensionType::Angular3P:
    case DimensionType::Angular: {
        Point2 vertex{};
        Point2 p1{};
        Point2 p2{};
        Point2 arc_pt{};
        if (def.type == DimensionType::Angular3P) {
            vertex = defs[0];
            p1     = defs[1];
            p2     = defs[2];
            arc_pt = defs[3];
        } else {
            if (!intersect(defs[0], defs[1], defs[2], defs[3], vertex)) return;
            p1     = defs[1];
            p2     = defs[3];
            arc_pt = defs[4];
        }
        const double radius = distance(vertex, arc_pt);
        if (radius < 1.0) return;
        const Dir d1 = unit_between(vertex, p1);
        const Dir d2 = unit_between(vertex, p2);
        if (d1.zero() || d2.zero()) return;
        const Point2 e1 = along(vertex, d1, radius);
        const Point2 e2 = along(vertex, d2, radius);
        // Extension lines reach the arc from the points inside it.
        if (distance(vertex, p1) < radius) line(into, p1, e1);
        if (distance(vertex, p2) < radius) line(into, p2, e2);
        bool from_p1 = false;
        (void)angle_at(vertex, p1, p2, arc_pt, from_p1);
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        if (from_p1)
            arc_outline(vertex, mm_round(radius), e1, e2, xs, ys);
        else
            arc_outline(vertex, mm_round(radius), e2, e1, xs, ys);
        into.begin_run(false);
        for (std::size_t i = 0; i < xs.size(); ++i)
            into.push_vertex(xs[i], ys[i]);
        // Arrowheads along the arc's tangents at its ends.
        if (xs.size() >= 2) {
            arrowhead_outline(Point2{xs.front(), ys.front()}, Point2{xs[1], ys[1]}, def.arrow_size,
                              def.arrow, into);
            arrowhead_outline(Point2{xs.back(), ys.back()},
                              Point2{xs[xs.size() - 2], ys[ys.size() - 2]}, def.arrow_size,
                              def.arrow, into);
        }
        break;
    }
    case DimensionType::Ordinate: {
        // From the feature to the leader end with one right-angle jog.
        const Point2 feature = defs[1];
        const Point2 end     = defs[2];
        const Point2 knee    = def.ordinate_x ? Point2{feature.x, end.y} : Point2{end.x, feature.y};
        into.begin_run(false);
        into.push_vertex(feature.x, feature.y);
        into.push_vertex(knee.x, knee.y);
        into.push_vertex(end.x, end.y);
        break;
    }
    }
}

std::vector<std::uint8_t> encode_leader(const LeaderDef& def)
{
    std::vector<std::uint8_t> out;
    std::uint16_t flags = 0;
    if (def.arrow) flags |= kFlagArrow;
    if (def.spline) flags |= kFlagSpline;
    kind::put_header(out, kLeaderLayout, flags);
    put_mm(out, def.arrow_size);
    return out;
}

Result<LeaderDef> decode_leader(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (payload.size() != kind::kHeaderBytes + 8 || !kind::read_header(in, h))
        return err(ErrorCode::ParseError,
                   "Lider yükü 16 bayt olmalı; verilen " + std::to_string(payload.size()) + ".");
    if (h.version != kLeaderLayout)
        return err(ErrorCode::Unsupported,
                   "Lider yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    LeaderDef def;
    def.arrow      = (h.flags & kFlagArrow) != 0;
    def.spline     = (h.flags & kFlagSpline) != 0;
    def.arrow_size = in.mm();
    if (def.arrow_size < 0)
        return err(ErrorCode::ValidationFailed, "Lider ok boyu negatif olamaz.");
    return def;
}

Result<LeaderDef> leader_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_leader(geom.payload_of(slot));
}

void leader_outline(const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into)
{
    const std::vector<Point2> pts = ring_points(geom, slot, 0);
    if (pts.size() < 2) return;
    into.begin_run(false);
    for (const Point2& p : pts)
        into.push_vertex(p.x, p.y);
    auto def = leader_of(geom, slot);
    if (def && def.value().arrow)
        arrowhead_outline(pts[0], pts[1], def.value().arrow_size, ArrowStyle::Closed, into);
}

KENTOS_KIND(dimension)
{
    KindSpec s{};
    s.id         = kDimensionKind;
    s.stable_id  = "core.dimension";
    s.summary_tr = "Uzunluğu ya da açıyı yazısı, çizgisi ve oklarıyla gösteren ölçü.";
    s.names[0]   = "ÖLÇÜ";
    s.names[1]   = "OLCU";
    s.names[2]   = "DIMENSION";
    s.names[3]   = "DIM";
    s.bbox       = &dm_bbox;
    s.outline    = &dm_outline;
    s.hit        = &dm_hit;
    s.area       = &zero_area;
    s.perimeter  = &zero_perimeter;
    s.read       = &dm_read;
    s.write      = &rings_write;
    s.validate   = &dm_validate;
    s.key_points = &dm_key_points;
    return s;
}

KENTOS_KIND(leader)
{
    KindSpec s{};
    s.id         = kLeaderKind;
    s.stable_id  = "core.leader";
    s.summary_tr = "Bir noktayı gösteren oklu çizgi; yazısı ayrı bir metin nesnesidir.";
    s.names[0]   = "LİDER";
    s.names[1]   = "LIDER";
    s.names[2]   = "LEADER";
    s.names[3]   = "LD";
    s.bbox       = &ld_bbox;
    s.outline    = &ld_outline;
    s.hit        = &ld_hit;
    s.area       = &zero_area;
    s.perimeter  = &ld_perimeter;
    s.read       = &ld_read;
    s.write      = &rings_write;
    s.validate   = &ld_validate;
    s.key_points = &ld_key_points;
    return s;
}

} // namespace kentos::core
