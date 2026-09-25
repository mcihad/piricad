// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/dimension.hpp"

#include "kentos_cad/core/document.hpp"

#include "kentos_cad/core/angle.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/text.hpp"
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

/// A linear or aligned dimension's line: its direction and the feet of its two
/// definition points on it — what its layout, its fit and its picture share.
bool dimension_feet(const DimensionDef& def, std::span<const Point2> defs, Dir& u, Point2& q1,
                    Point2& q2)
{
    if (defs.size() < 3) return false;
    u = def.type == DimensionType::Aligned ? unit_between(defs[0], defs[1])
                                           : unit_at(def.rotation_udeg);
    if (u.zero()) return false;
    const Dir n = u.perp();
    q1          = along(defs[0], n, dot(defs[0], defs[2], n));
    q2          = along(defs[1], n, dot(defs[1], defs[2], n));
    return true;
}

/// Whether two arrowheads fit between the feet `inner` apart, with a stretch
/// of line between them for the eye to read as a line.
bool arrows_fit(const DimensionDef& def, double inner)
{
    return inner >= 2.5 * static_cast<double>(def.arrow_size);
}

/// How wide a caption of `text` at `height` is: the width the canvas and the
/// paper draw it at (`text_width`, TODOS C-18), and never less than a
/// millimetre, because the caption's baseline is also its direction.
double caption_width(std::string_view text, Mm height)
{
    return static_cast<double>(std::max<Mm>(1, text_width(text, height)));
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

/// The dimension arc of an angular or arc-length dimension: its centre, its
/// radius, its two ends counter-clockwise and where the arms it runs between
/// were picked — what the layout's fit and the picture share, so a figure
/// moved clear of an arm and the line drawn under it agree.
struct ArcFrame
{
    Point2 vertex;         ///< the angle's vertex, or the measured arc's centre
    Point2 p1;             ///< the first arm's point, as picked
    Point2 p2;             ///< the second arm's point
    Point2 e1;             ///< where the dimension arc meets the first arm
    Point2 e2;             ///< where it meets the second
    bool from_p1{true};    ///< the arc runs counter-clockwise from e1 to e2
    double radius{0.0};    ///< of the dimension arc
    std::int64_t start{0}; ///< direction of the counter-clockwise start, µ°
    std::int64_t sweep{0}; ///< counter-clockwise sweep, µ°

    Point2 first() const noexcept { return from_p1 ? e1 : e2; }

    Point2 last() const noexcept { return from_p1 ? e2 : e1; }

    /// Its length, along the arc.
    double length() const noexcept
    {
        return radius * static_cast<double>(sweep) * (kPi / 180'000'000.0);
    }
};

bool arc_frame(const DimensionDef& def, std::span<const Point2> defs, ArcFrame& f)
{
    Point2 arc_pt{};
    switch (def.type) {
    case DimensionType::Angular3P:
        if (defs.size() < 4) return false;
        f.vertex = defs[0];
        f.p1     = defs[1];
        f.p2     = defs[2];
        arc_pt   = defs[3];
        break;
    case DimensionType::Angular:
        if (defs.size() < 5 || !intersect(defs[0], defs[1], defs[2], defs[3], f.vertex))
            return false;
        f.p1   = defs[1];
        f.p2   = defs[3];
        arc_pt = defs[4];
        break;
    case DimensionType::ArcLength:
        if (defs.size() < 4) return false;
        f.vertex = defs[0];
        f.p1     = defs[1];
        f.p2     = defs[2];
        arc_pt   = defs[3];
        break;
    default: return false;
    }
    f.radius = distance(f.vertex, arc_pt);
    if (f.radius < 1.0) return false;
    const Dir d1 = unit_between(f.vertex, f.p1);
    const Dir d2 = unit_between(f.vertex, f.p2);
    if (d1.zero() || d2.zero()) return false;
    f.e1 = along(f.vertex, d1, f.radius);
    f.e2 = along(f.vertex, d2, f.radius);
    // An arc-length dimension runs the way its arc does, start to end; an
    // angle the side its arc point is on.
    if (def.type == DimensionType::ArcLength) {
        f.from_p1 = true;
        f.sweep   = arc_sweep_udeg(f.vertex, f.p1, f.p2);
    } else {
        f.sweep = angle_at(f.vertex, f.p1, f.p2, arc_pt, f.from_p1);
    }
    const Point2 s = f.from_p1 ? f.p1 : f.p2;
    f.start        = atan2_udeg(s.y - f.vertex.y, s.x - f.vertex.x);
    return true;
}

/// How far round the arc from its start `p`'s direction is, µ°, in [0, 360°).
std::int64_t around(const ArcFrame& f, Point2 p)
{
    const std::int64_t at = atan2_udeg(p.y - f.vertex.y, p.x - f.vertex.x) - f.start;
    return ((at % kUDegFullCircle) + kUDegFullCircle) % kUDegFullCircle;
}

/// A point `by` from `tip` along `d`, far enough that the direction back to
/// `tip` survives rounding to the millimetre: an arrowhead aimed through a
/// point a millimetre away pointed anywhere within 45° of where it meant.
Point2 aim_from(Point2 tip, Dir d, const DimensionDef& def)
{
    return along(tip, d, std::max(1000.0, 2.0 * static_cast<double>(def.arrow_size)));
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
    return rings_read(geom, payload, "Kılavuz çizgi", 1);
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
    case DimensionType::ArcLength: return "yay";
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
    case DimensionType::ArcLength: return 4;
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
    // LAYOUT 2 ONLY WHEN IT SAYS SOMETHING: a dimension with no prefix, no
    // tolerance, no unit of its own and no recorded sheet scale is written
    // byte for byte as before those existed.
    const bool layout2 = !def.prefix.empty() || !def.suffix.empty() ||
                         def.tolerance != DimTolerance::None || def.tolerance_plus != 0 ||
                         def.tolerance_minus != 0 || def.unit != 0 || def.scale_basis != 0;
    kind::put_header(out, layout2 ? kDimensionLayout2 : kDimensionLayout, flags);
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
    if (layout2) {
        kind::put_string(out, def.prefix.size() > kMaxText ? def.prefix.substr(0, kMaxText)
                                                           : def.prefix);
        kind::put_string(out, def.suffix.size() > kMaxText ? def.suffix.substr(0, kMaxText)
                                                           : def.suffix);
        put_u8(out, static_cast<std::uint8_t>(def.tolerance));
        put_i64(out, def.tolerance_plus);
        put_i64(out, def.tolerance_minus);
        put_u8(out, def.unit);
        put_i64(out, def.scale_basis);
    }
    return out;
}

std::vector<std::uint8_t> encode_dimension_guide(const DimensionGuide& guide)
{
    std::vector<std::uint8_t> out;
    put_mm(out, guide.text_height);
    const std::vector<std::uint8_t> payload = encode_dimension(guide.def);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::optional<DimensionGuide> decode_dimension_guide(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 8) return std::nullopt;
    WireReader in(bytes.first(8));
    DimensionGuide guide;
    guide.text_height = in.mm();
    auto def          = decode_dimension(bytes.subspan(8));
    if (!def) return std::nullopt;
    guide.def = std::move(def.value());
    return guide;
}

Result<DimensionDef> decode_dimension(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(4 + 8 * 6))
        return err(ErrorCode::ParseError,
                   "Ölçü yükü başlığı ve sayılarını taşıyacak kadar uzun değil.");
    if (h.version != kDimensionLayout && h.version != kDimensionLayout2)
        return err(ErrorCode::Unsupported,
                   "Ölçü yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    DimensionDef def;
    def.user_text_position   = (h.flags & kFlagUserText) != 0;
    def.ordinate_x           = (h.flags & kFlagOrdinateX) != 0;
    const std::uint8_t type  = in.u8();
    const std::uint8_t arrow = in.u8();
    if (type > static_cast<std::uint8_t>(DimensionType::ArcLength))
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
    if (h.version == kDimensionLayout2) {
        if (!kind::read_string(in, def.prefix, kMaxText) ||
            !kind::read_string(in, def.suffix, kMaxText))
            return err(ErrorCode::ParseError, "Ölçünün öneki ya da soneki okunamadı.");
        if (!in.remaining(1 + 8 + 8 + 1 + 8))
            return err(ErrorCode::ParseError,
                       "Ölçü yükü toleransı, birimi ve ölçeği taşıyacak kadar uzun değil.");
        const std::uint8_t tolerance = in.u8();
        if (tolerance > static_cast<std::uint8_t>(DimTolerance::Limits))
            return err(ErrorCode::ValidationFailed,
                       "Bilinmeyen tolerans biçimi: " + std::to_string(tolerance));
        def.tolerance       = static_cast<DimTolerance>(tolerance);
        def.tolerance_plus  = in.i64();
        def.tolerance_minus = in.i64();
        def.unit            = in.u8();
        def.scale_basis     = in.i64();
        if (def.tolerance_plus < 0 || def.tolerance_minus < 0)
            return err(ErrorCode::ValidationFailed, "Ölçü toleransı negatif olamaz.");
        if (def.unit > static_cast<std::uint8_t>(DrawingUnit::Hectometre) + 1)
            return err(ErrorCode::ValidationFailed,
                       "Bilinmeyen ölçü birimi: " + std::to_string(def.unit));
        if (def.scale_basis < 0)
            return err(ErrorCode::ValidationFailed, "Ölçünün pafta ölçeği negatif olamaz.");
    }
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
                               std::int64_t rotation_udeg, bool ordinate_x) noexcept
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
    case DimensionType::Ordinate:
        // THE READING, ALREADY SIGNED. An ordinate is the feature's easting or
        // northing measured FROM the origin, and the sign is part of the answer:
        // a point west of the origin reads negative and printing it as positive
        // would put it on the wrong side of the sheet.
        if (defs.size() < 2) return 0;
        return ordinate_x ? defs[1].x - defs[0].x : defs[1].y - defs[0].y;

    case DimensionType::ArcLength: {
        // THE LENGTH ALONG, not the chord across. r · θ, with θ the swept angle
        // in turns — so the one rounding is the final one and a quarter of a
        // 50 m circle comes out 78,540 m rather than 78,539 or 78,541.
        if (defs.size() < 3) return 0;
        const Mm radius = segment_length(defs[0], defs[1]);
        if (radius <= 0) return 0;
        double sweep = direction_turns(defs[0], defs[2], AngleRule::Matematik) -
                       direction_turns(defs[0], defs[1], AngleRule::Matematik);
        sweep -= std::floor(sweep); ///< counter-clockwise, as an arc is stored
        return mm_round(static_cast<double>(radius) * sweep * 2.0 * std::acos(-1.0));
    }
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

std::string format_area(Mm2 value, unsigned precision, char separator)
{
    std::int64_t pow = 1;
    for (unsigned i = 0; i < precision && i < 8; ++i)
        pow *= 10;
    const bool negative       = value < 0;
    const std::int64_t mag    = negative ? -value : value;
    const std::int64_t scaled = mul_div_round(mag, pow, 1'000'000); // mm² to m², × 10^p
    return with_decimals(scaled, precision > 8 ? 8 : precision, separator, negative);
}

std::string format_dimension_angle(std::int64_t udeg, unsigned precision, char separator)
{
    return format_dimension_angle(udeg, precision, separator, AngleUnit::Degree);
}

std::string format_dimension_angle(std::int64_t udeg, unsigned precision, char separator,
                                   AngleUnit unit)
{
    std::int64_t pow = 1;
    for (unsigned i = 0; i < precision && i < 8; ++i)
        pow *= 10;
    const bool negative    = udeg < 0;
    const std::int64_t mag = negative ? -udeg : udeg;
    const unsigned places  = precision > 8 ? 8 : precision;
    switch (unit) {
    case AngleUnit::Grad:
        // A degree is ten ninths of a grad: µ° · 10 / 9 · 10^p / 10^6, rounded
        // once, half away from zero.
        return with_decimals(mul_div_round(mag, pow * 10, 9'000'000), places, separator, negative) +
               "g";
    case AngleUnit::Radian: {
        const double rad = static_cast<double>(mag) * (kPi / 180.0) / 1'000'000.0;
        return with_decimals(mm_round(rad * static_cast<double>(pow)), places, separator,
                             negative) +
               "r";
    }
    case AngleUnit::Degree: break;
    }
    return with_decimals(mul_div_round(mag, pow, 1'000'000), places, separator, negative) + "°";
}

AngleUnit dimension_angle_unit(const DimensionDef& def) noexcept
{
    switch (def.unit) {
    case 1: return AngleUnit::Grad;
    case 3: return AngleUnit::Radian;
    default: return AngleUnit::Degree;
    }
}

bool dimension_is_angle(const DimensionDef& def) noexcept
{
    return def.type == DimensionType::Angular || def.type == DimensionType::Angular3P;
}

const char* dimension_unit_word(const DimensionDef& def) noexcept
{
    if (dimension_is_angle(def)) {
        switch (dimension_angle_unit(def)) {
        case AngleUnit::Grad: return "grad";
        case AngleUnit::Radian: return "radyan";
        case AngleUnit::Degree: break;
        }
        return "derece";
    }
    if (def.unit == 0) return "cizim";
    switch (static_cast<DrawingUnit>(def.unit - 1)) {
    case DrawingUnit::Millimetre: return "mm";
    case DrawingUnit::Centimetre: return "cm";
    case DrawingUnit::Kilometre: return "km";
    default: return "m";
    }
}

std::optional<std::uint8_t> dimension_unit_code(const DimensionDef& def, std::string_view word)
{
    const auto plus_one = [](auto unit) {
        return static_cast<std::uint8_t>(static_cast<std::uint8_t>(unit) + 1);
    };
    if (dimension_is_angle(def)) {
        if (turkish_key_equals(word, "grad")) return plus_one(AngleUnit::Grad);
        if (turkish_key_equals(word, "derece")) return plus_one(AngleUnit::Degree);
        if (turkish_key_equals(word, "radyan")) return plus_one(AngleUnit::Radian);
        return std::nullopt;
    }
    if (turkish_key_equals(word, "cizim")) return std::uint8_t{0};
    if (turkish_key_equals(word, "mm")) return plus_one(DrawingUnit::Millimetre);
    if (turkish_key_equals(word, "cm")) return plus_one(DrawingUnit::Centimetre);
    if (turkish_key_equals(word, "m")) return plus_one(DrawingUnit::Metre);
    if (turkish_key_equals(word, "km")) return plus_one(DrawingUnit::Kilometre);
    return std::nullopt;
}

namespace {

bool is_angle(DimensionType t) noexcept
{
    return t == DimensionType::Angular || t == DimensionType::Angular3P;
}

/// A figure of the dimension's kind: a length in its unit, or an angle.
std::string figure(const DimensionDef& def, std::int64_t v, DrawingUnit unit)
{
    if (is_angle(def.type))
        return format_dimension_angle(v, def.precision, def.decimal_separator,
                                      dimension_angle_unit(def));
    return format_dimension_length(v, dimension_unit(def, unit), def.precision,
                                   def.decimal_separator);
}

} // namespace

DrawingUnit dimension_unit(const DimensionDef& def, DrawingUnit unit) noexcept
{
    if (def.unit == 0 || def.unit > static_cast<std::uint8_t>(DrawingUnit::Hectometre) + 1)
        return unit;
    return static_cast<DrawingUnit>(def.unit - 1);
}

std::string dimension_value_text(const DimensionDef& def, DrawingUnit unit)
{
    return figure(def, def.measurement, unit);
}

bool dimension_text_is_manual(const DimensionDef& def) noexcept
{
    return !def.override_text.empty() && def.override_text.find("<>") == std::string::npos;
}

std::string dimension_tolerance_text(const DimensionDef& def, DrawingUnit unit)
{
    switch (def.tolerance) {
    case DimTolerance::None:
    case DimTolerance::Limits: return {};
    case DimTolerance::Symmetric: return "±" + figure(def, def.tolerance_plus, unit);
    case DimTolerance::Deviation:
        return "+" + figure(def, def.tolerance_plus, unit) + "/-" +
               figure(def, def.tolerance_minus, unit);
    }
    return {};
}

std::string dimension_text(const DimensionDef& def, DrawingUnit unit)
{
    // TYPED BY HAND: exactly what was typed, and nothing is added to it — a
    // prefix or a tolerance around a figure that is not the measured one would
    // dress a typed number up as a measured one.
    if (dimension_text_is_manual(def)) return def.override_text;
    const std::string shown =
        def.tolerance == DimTolerance::Limits
            ? figure(def, def.measurement + def.tolerance_plus, unit) + "/" +
                  figure(def, def.measurement - def.tolerance_minus, unit)
            : figure(def, def.measurement, unit) + dimension_tolerance_text(def, unit);
    std::string written = def.prefix + shown + def.suffix;
    if (def.override_text.empty()) return written;
    const std::string_view typed = def.override_text;
    std::string out;
    std::size_t from = 0;
    for (std::size_t at = typed.find("<>"); at != std::string_view::npos;
         at             = typed.find("<>", from)) {
        out += typed.substr(from, at - from);
        out += written;
        from = at + 2;
    }
    out += typed.substr(from);
    return out;
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
        // FILLED, as its name says and as every sheet prints it (TODOS C-17):
        // drawn as an outline alone it read as the open head of another style.
        into.begin_run(true);
        into.mark_solid();
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

Point2 dimension_rim_point(Point2 centre, Mm radius, Point2 toward) noexcept
{
    if (radius <= 0) return centre;
    const Dir u = unit_between(centre, toward);
    if (u.zero()) return Point2{centre.x + radius, centre.y};
    const Point2 rough = along(centre, u, static_cast<double>(radius));
    // A RADIUS BEYOND A THOUSAND KILOMETRES keeps the rounded point: its square
    // no longer fits the integers the search below compares.
    constexpr Mm kSearchable = 1'000'000'000;
    if (radius > kSearchable) return rough;

    // THE NEIGHBOUR WHOSE DISTANCE IS THE RADIUS. The rounded point lies up to
    // seven tenths of a millimetre off the circle, which a figure written to the
    // millimetre shows; among the points three millimetres round it, the one whose
    // squared distance from the centre is nearest the radius squared lies well
    // within half of one, so the figure rounds to the radius — and the line
    // turns by a few millionths of a radian. Integers only, and ties broken by
    // position, so every machine picks the same point (§7.3).
    const std::int64_t want = radius * radius;
    Point2 best             = rough;
    std::int64_t best_off   = -1;
    for (Mm dy = -3; dy <= 3; ++dy)
        for (Mm dx = -3; dx <= 3; ++dx) {
            const Point2 at{rough.x + dx, rough.y + dy};
            const std::int64_t rx  = at.x - centre.x;
            const std::int64_t ry  = at.y - centre.y;
            const std::int64_t got = (rx * rx) + (ry * ry);
            const std::int64_t off = got > want ? got - want : want - got;
            if (best_off < 0 || off < best_off) {
                best     = at;
                best_off = off;
            }
        }
    return best;
}

std::array<Point2, 2> dimension_diameter_ends(Point2 centre, Mm diameter, Point2 toward) noexcept
{
    const Point2 near =
        dimension_rim_point(centre, mm_round(static_cast<double>(diameter) / 2.0), toward);
    const Point2 mirror{(2 * centre.x) - near.x, (2 * centre.y) - near.y};
    // THE FAR END MAKES THE LENGTH. Mirrored through the centre, the two ends
    // double the near one's rounding, and at forty-five degrees that is more
    // than half a millimetre; among the points one millimetre round the
    // mirror, the one whose distance from the near end is nearest the diameter
    // lies within a third of one. The mirror itself first on a tie.
    if (diameter <= 0 || diameter > 2'000'000'000) return {mirror, near};
    const std::int64_t want = diameter * diameter;
    Point2 far              = mirror;
    std::int64_t best_off   = -1;
    for (const Mm dy : {Mm{0}, Mm{-1}, Mm{1}})
        for (const Mm dx : {Mm{0}, Mm{-1}, Mm{1}}) {
            const Point2 at{mirror.x + dx, mirror.y + dy};
            const std::int64_t rx  = near.x - at.x;
            const std::int64_t ry  = near.y - at.y;
            const std::int64_t got = (rx * rx) + (ry * ry);
            const std::int64_t off = got > want ? got - want : want - got;
            if (best_off < 0 || off < best_off) {
                far      = at;
                best_off = off;
            }
        }
    return {far, near};
}

bool dimension_layout(DimensionDef& def, std::span<const Point2> picks, Point2 where,
                      Mm text_height, DimensionLayout& out, bool fixed_rotation)
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
        } else if (fixed_rotation) {
            u = unit_at(def.rotation_udeg);
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
        // THE FIGURE STANDS ON ITS LINE (ISO 129-1, TODOS C-17): above the
        // dimension line as the figure reads — left of a vertical one, which
        // reads from the bottom — whichever side of the line the measured
        // points lie. It used to go on the side away from the points, so a
        // dimension placed below a parcel hung its figure under the line.
        const Dir reads = (u.x < 0.0 || (u.x == 0.0 && u.y < 0.0)) ? Dir{-u.x, -u.y} : u;
        out.text_centre = along(mid, reads.perp(), half_text);
        out.text_dir_x  = u.x;
        out.text_dir_y  = u.y;
        break;
    }
    case DimensionType::Radial:
    case DimensionType::Diametric: {
        // THE LINE AIMS AT ITS FIGURE (TODOS C-17). The rim point is where the
        // line from the centre toward the caption meets the circle, so a radius
        // or a diameter is always read along the line it names — the way every
        // CAD draws one — and a caption dragged round the circle carries the
        // line with it. The length stays the circle's own
        // (`dimension_rim_point`), so the figure does not move by a millimetre
        // because the line turned.
        // THE FIGURE SITS ON THE LINE, not across it (ISO 129-1): the caption's
        // centre is `where`, and the line is turned off it by the angle that
        // puts the caption half a text height above the line as it reads. The
        // caption stays where it was put, so a rebuild that reads it back lays
        // the same line again.
        const double half =
            static_cast<double>(def.text_gap) + (static_cast<double>(text_height) / 2.0);
        const auto aim_at = [&](Point2 centre) {
            const Dir d0     = unit_between(centre, where);
            const double far = distance(centre, where);
            if (d0.zero() || far <= half) return where;
            // The turn's sine is the caption's height over its distance, and its
            // cosine the square root that completes it: +, ×, / and `sqrt`
            // only, all correctly rounded, never libm's trigonometry (§7.3).
            const double sn = half / far;
            const double c  = std::sqrt(1.0 - (sn * sn));
            // Clockwise when the line reads left to right (its figure lies on
            // its left), counter-clockwise when it reads the other way.
            const bool rightward = d0.x > 0.0 || (d0.x == 0.0 && d0.y > 0.0);
            const Dir u = rightward ? Dir{(d0.x * c) + (d0.y * sn), (d0.y * c) - (d0.x * sn)}
                                    : Dir{(d0.x * c) - (d0.y * sn), (d0.y * c) + (d0.x * sn)};
            return along(centre, u, far);
        };
        if (def.type == DimensionType::Radial) {
            const Mm radius = mm_round(distance(p1, p2));
            out.defs        = {p1, where == p1 ? p2 : dimension_rim_point(p1, radius, aim_at(p1))};
        } else {
            // A DIAMETER ALREADY AIMED AT ITS FIGURE KEEPS ITS ENDS. Re-aiming is
            // for a caption that moved; doing it on every rebuild would walk the
            // ends round by the millimetre the midpoint rounds to, each time.
            const Point2 centre{(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
            const Mm across     = mm_round(distance(p1, p2));
            const Point2 target = aim_at(centre);
            const Dir line      = unit_between(p1, p2);
            const Dir aim       = unit_between(centre, target);
            const double tol    = 1e-3 + (16.0 / static_cast<double>(across > 0 ? across : 1));
            const double off    = std::abs((line.x * aim.y) - (line.y * aim.x));
            if (aim.zero() || ((line.x * aim.x) + (line.y * aim.y) > 0.0 && off <= tol)) {
                out.defs = {p1, p2};
            } else {
                const std::array<Point2, 2> ends = dimension_diameter_ends(centre, across, target);
                out.defs                         = {ends[0], ends[1]};
            }
        }
        out.text_centre = where;
        const Dir u     = unit_between(out.defs[0], out.defs[1]);
        out.text_dir_x  = u.x;
        out.text_dir_y  = u.y;
        break;
    }
    case DimensionType::Angular3P: {
        const Point2 apex = picks.size() >= 3 ? picks[2] : p1;
        if (apex == p1 || apex == p2) return false;
        out.defs = {apex, p1, p2, where};
        // The text sits outside the arc at the point it was taken through;
        // `dimension_fit` moves it clear of an arm it would cross.
        const Dir outward = unit_between(apex, where);
        if (outward.zero()) return false;
        out.text_centre = along(where, outward, half_text);
        out.text_dir_x  = -outward.y;
        out.text_dir_y  = outward.x;
        break;
    }
    case DimensionType::Ordinate: {
        // ORIGIN, FEATURE, LEADER END. The origin is the point every ordinate on
        // the sheet is read from — a block corner, a station — and the leader
        // end is where the figure is written.
        out.defs = {p1, p2, where};

        // WHICH AXIS, DECIDED BY THE JOG. A leader taken sideways from the
        // feature reads its easting; one taken up or down reads its northing.
        // That is the gesture an ordinate table is built with and it costs the
        // user no extra answer.
        const Mm run   = where.x > p2.x ? where.x - p2.x : p2.x - where.x;
        const Mm rise  = where.y > p2.y ? where.y - p2.y : p2.y - where.y;
        def.ordinate_x = run >= rise;

        const Dir outward = unit_between(p2, where);
        if (outward.zero()) return false;
        out.text_centre = along(where, outward, half_text);
        out.text_dir_x  = 1.0;
        out.text_dir_y  = 0.0;
        break;
    }

    case DimensionType::ArcLength: {
        // CENTRE, START, END, and where the figure goes. The first three are the
        // arc itself; the fourth is the caption, outside the curve on the
        // bisector, the way an angular dimension's is.
        if (picks.size() < 3) return false;
        const Point2 end = picks[2];
        if (end == p1) return false;
        out.defs = {p1, p2, end, where};

        const Dir outward = unit_between(p1, where);
        if (outward.zero()) return false;
        out.text_centre = along(where, outward, half_text);
        out.text_dir_x  = -outward.y;
        out.text_dir_y  = outward.x;
        break;
    }

    default: return false;
    }
    def.measurement = dimension_measure(def.type, out.defs, def.rotation_udeg, def.ordinate_x);
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
    case DimensionType::Ordinate:
        picks = {defs[0], defs[1]};
        where = defs[2];
        return true;
    case DimensionType::ArcLength:
        picks = {defs[0], defs[1], defs[2]};
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
    return {centre, along(centre, u, caption_width(text, height))};
}

namespace {

/// An angular or arc-length dimension's arc and its heads, SIZED TO FIT the
/// way a linear dimension's line is (ISO 129-1, TODOS C-17): heads that do not
/// fit along the arc stand outside its ends pointing in, each on a tangent of
/// its own, and a figure standing beyond an end (`dimension_fit`) stands on
/// that end's tangent carried on under it. The heads follow the arc's tangent
/// at its ends, as a sheet draws them.
void dimension_arc(const RingGeometry& geom, std::uint32_t slot, const DimensionDef& def,
                   const ArcFrame& f, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    arc_outline(f.vertex, mm_round(f.radius), f.first(), f.last(), xs, ys);
    if (xs.size() < 2) return;
    into.begin_run(false);
    for (std::size_t i = 0; i < xs.size(); ++i)
        into.push_vertex(xs[i], ys[i]);

    const Point2 a{xs.front(), ys.front()};
    const Point2 b{xs.back(), ys.back()};
    const Dir ra = unit_between(f.vertex, a);
    const Dir rb = unit_between(f.vertex, b);
    const Dir out_a{ra.y, -ra.x}; ///< clockwise at the start: away from the arc
    const Dir out_b{-rb.y, rb.x}; ///< counter-clockwise at the end
    const bool outside = !arrows_fit(def, f.length());
    const double tail  = outside ? 2.0 * static_cast<double>(def.arrow_size) : 0.0;
    double reach_a     = tail;
    double reach_b     = tail;
    // A FIGURE BEYOND AN END — round the circle past it, not over the arc —
    // stands on that end's tangent, carried on to the far end of the words.
    if (const std::vector<Point2> baseline = ring_points(geom, slot, 0); baseline.size() >= 2) {
        const Point2 c       = baseline[0];
        const std::int64_t t = around(f, c);
        if (t > f.sweep) {
            const double half = distance(baseline[0], baseline[1]) / 2.0;
            if (t - f.sweep <= kUDegFullCircle - t)
                reach_b = std::max(reach_b, dot(b, c, out_b) + half);
            else
                reach_a = std::max(reach_a, dot(a, c, out_a) + half);
        }
    }
    if (reach_a > 0.0) line(into, a, along(a, out_a, reach_a));
    if (reach_b > 0.0) line(into, b, along(b, out_b, reach_b));
    const Dir in_a{-out_a.x, -out_a.y};
    const Dir in_b{-out_b.x, -out_b.y};
    arrowhead_outline(a, aim_from(a, outside ? out_a : in_a, def), def.arrow_size, def.arrow, into);
    arrowhead_outline(b, aim_from(b, outside ? out_b : in_b, def), def.arrow_size, def.arrow, into);
}

} // namespace

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
        Dir u{};
        Point2 q1{};
        Point2 q2{};
        // Each definition point's foot on the dimension line, which runs
        // through `dl` along `u`.
        if (!dimension_feet(def, defs, u, q1, q2)) return;
        const Dir n     = u.perp();
        const double t1 = dot(p1, dl, n);
        const double t2 = dot(p2, dl, n);
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

        // SIZED TO FIT (ISO 129-1, TODOS C-17). Two heads that do not fit
        // between the extension lines stand outside them pointing in, on a
        // stretch of line of their own; and a figure moved outside
        // (`dimension_fit`) stands on the line carried on under it.
        const double inner = distance(q1, q2);
        const Dir along_u  = unit_between(q1, q2);
        const bool outside = !arrows_fit(def, inner) && !along_u.zero();
        const double tail  = 2.0 * static_cast<double>(def.arrow_size);
        Point2 from        = outside ? along(q1, along_u, -tail) : q1;
        Point2 to          = outside ? along(q2, along_u, tail) : q2;
        if (const std::vector<Point2> baseline = ring_points(geom, slot, 0);
            baseline.size() >= 2 && !along_u.zero()) {
            const double half = distance(baseline[0], baseline[1]) / 2.0;
            const double at   = dot(q1, baseline[0], along_u);
            if (at + half > dot(q1, to, along_u)) to = along(q1, along_u, at + half);
            if (at - half < dot(q1, from, along_u)) from = along(q1, along_u, at - half);
        }
        line(into, from, to);
        if (outside) {
            arrowhead_outline(q1, aim_from(q1, Dir{-along_u.x, -along_u.y}, def), def.arrow_size,
                              def.arrow, into);
            arrowhead_outline(q2, aim_from(q2, along_u, def), def.arrow_size, def.arrow, into);
        } else {
            arrowhead_outline(q1, q2, def.arrow_size, def.arrow, into);
            arrowhead_outline(q2, q1, def.arrow_size, def.arrow, into);
        }
        break;
    }
    case DimensionType::Radial:
    case DimensionType::Diametric: {
        // THE LINE THROUGH THE CENTRE, arrowed where it meets the circle — and
        // ON, UNDER THE FIGURE, when the figure stands outside the circle
        // (ISO 129-1, TODOS C-17): a figure written beyond the rim sits on the
        // line that names it rather than floating beside it. The caption's
        // place is ring 0, its centre and its width, so the line reaches the
        // far end of the words.
        const Point2 from = defs[0]; ///< the centre, or the far end of a diameter
        const Point2 rim  = defs[1];
        const Dir u       = unit_between(from, rim);
        if (u.zero()) break;
        Point2 end                         = rim;
        const std::vector<Point2> baseline = ring_points(geom, slot, 0);
        if (baseline.size() >= 2) {
            const double reach =
                dot(from, baseline[0], u) + (distance(baseline[0], baseline[1]) / 2.0);
            if (reach > distance(from, rim)) end = along(from, u, reach);
        }
        line(into, from, end);
        arrowhead_outline(rim, from, def.arrow_size, def.arrow, into);
        if (def.type == DimensionType::Diametric)
            arrowhead_outline(from, rim, def.arrow_size, def.arrow, into);
        break;
    }
    case DimensionType::Angular3P:
    case DimensionType::Angular: {
        ArcFrame f;
        if (!arc_frame(def, defs, f)) return;
        // Extension lines reach the arc from the points inside it.
        if (distance(f.vertex, f.p1) < f.radius) line(into, f.p1, f.e1);
        if (distance(f.vertex, f.p2) < f.radius) line(into, f.p2, f.e2);
        dimension_arc(geom, slot, def, f, into);
        break;
    }
    case DimensionType::ArcLength: {
        // A DIMENSION ARC BESIDE THE ARC IT MEASURES (ISO 129-1, TODOS C-17):
        // concentric with it and through the point the figure stands on
        // (`defs[3]`), radial extension lines from the arc's two ends, and an
        // arrowhead at each end of the dimension arc along its tangent. It used
        // to draw the measured arc a second time over itself and two radii to
        // its ends, which named nothing on a sheet.
        ArcFrame f;
        if (!arc_frame(def, defs, f)) break;
        const Point2 centre = f.vertex;
        const double reach  = f.radius;
        const Dir d1        = unit_between(centre, f.p1);
        const Dir d2        = unit_between(centre, f.p2);
        // Extension lines: from a gap off the arc's end to a little past the
        // dimension arc, whichever side of the arc the figure is on.
        const auto ext = [&](Point2 from, Dir d) {
            const double at   = distance(centre, from);
            const double sign = reach >= at ? 1.0 : -1.0;
            const double gap  = static_cast<double>(def.extension_offset);
            if (std::abs(reach - at) <= gap) return;
            line(into, along(from, d, sign * gap),
                 along(centre, d, reach + (sign * static_cast<double>(def.extension_beyond))));
        };
        ext(f.p1, d1);
        ext(f.p2, d2);
        dimension_arc(geom, slot, def, f, into);
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

void dimension_fit(const DimensionDef& def, DimensionLayout& layout, std::string_view text,
                   Mm text_height)
{
    const double width = caption_width(text, text_height);
    const double gap   = static_cast<double>(def.text_gap);
    const double arrow = static_cast<double>(def.arrow_size);
    const double half  = gap + (static_cast<double>(text_height) / 2.0);

    if (ArcFrame f; arc_frame(def, layout.defs, f)) {
        // AN ANGLE OR AN ARC LENGTH KEEPS ITS FIGURE WHERE IT WAS PUT — the
        // point the dimension arc was taken through — unless the words would
        // cross an arm there. Round the arc from its start, the nearer end
        // when rounding put it a hair past one.
        std::int64_t at = around(f, layout.defs.back());
        if (at > f.sweep) at = at - f.sweep <= kUDegFullCircle - at ? f.sweep : 0;
        const double inner = f.length();
        if (inner >= width + (2.0 * gap)) {
            // IT FITS ALONG THE ARC: as near where it was put as clears both
            // arms by a gap, outward of the arc like a figure anywhere on it.
            auto clear = static_cast<std::int64_t>(
                std::llround(((width / 2.0) + gap) / f.radius * (180'000'000.0 / kPi)));
            clear                 = std::min(clear, f.sweep / 2);
            const std::int64_t to = std::clamp(at, clear, f.sweep - clear);
            if (to == at) return;
            const Dir r        = unit_at(f.start + to);
            layout.text_centre = along(f.vertex, r, f.radius + half);
            layout.text_dir_x  = -r.y;
            layout.text_dir_y  = r.x;
            return;
        }
        // OUTSIDE, past the arm nearer to where it was put, on that end's
        // tangent carried on: clear of that end's head when the heads stand
        // outside too, and half a text height outward, as inside.
        const bool first   = 2 * at <= f.sweep;
        const Point2 end   = first ? f.first() : f.last();
        const Dir r        = unit_between(f.vertex, end);
        const Dir o        = first ? Dir{r.y, -r.x} : Dir{-r.y, r.x};
        const double past  = (arrows_fit(def, inner) ? 0.0 : 2.0 * arrow) + gap + (width / 2.0);
        layout.text_centre = along(along(end, o, past), r, half);
        layout.text_dir_x  = o.x;
        layout.text_dir_y  = o.y;
        return;
    }

    if (def.type != DimensionType::Linear && def.type != DimensionType::Aligned) return;
    Dir u{};
    Point2 q1{};
    Point2 q2{};
    if (!dimension_feet(def, layout.defs, u, q1, q2)) return;
    const double inner = distance(q1, q2);
    // THE FIGURE STANDS ABOVE THE LINE, so it does not compete with the heads
    // on it: it fits when it fits between the extension lines, a gap clear of
    // each.
    if (inner >= width + (2.0 * gap)) return;

    // OUTSIDE, past the end the figure reads toward, clear of that end's
    // arrowhead when the heads stand outside too — and half a text height
    // above the line as it reads, like a figure inside.
    const Dir reads    = (u.x < 0.0 || (u.x == 0.0 && u.y < 0.0)) ? Dir{-u.x, -u.y} : u;
    const Point2 last  = dot(q1, q2, reads) >= 0.0 ? q2 : q1;
    const double past  = (arrows_fit(def, inner) ? 0.0 : 2.0 * arrow) + gap + (width / 2.0);
    layout.text_centre = along(along(last, reads, past), reads.perp(), half);
}

std::array<Point2, 2> dimension_caption_baseline(const DimensionDef& def, DimensionLayout& layout,
                                                 std::string_view text, Mm text_height,
                                                 Point2 by_hand)
{
    if (!def.user_text_position) dimension_fit(def, layout, text, text_height);
    return dimension_baseline(def.user_text_position ? by_hand : layout.text_centre,
                              layout.text_dir_x, layout.text_dir_y, text_height, text);
}

Result<DimensionRebuild> dimension_rebuild(const Document& doc, EntityId e,
                                           const DimensionEdit& edit, DrawingUnit unit)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e) || ents.kind[e] != kDimensionKind)
        return err(ErrorCode::InvalidArgument, "Bu nesne bir ölçü değil.");
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];
    const RingSpan span      = geom.rings_of(slot);
    if (span.count < 2 || geom.ring_count[span.first] < 2)
        return err(ErrorCode::InvalidArgument, "Ölçünün halkaları eksik.");
    auto decoded = dimension_of(geom, slot);
    if (!decoded) return decoded.error();
    DimensionDef def = std::move(decoded.value());
    if (edit.def != nullptr) def = *edit.def;

    const std::array<Point2, 2> base{geom.vertex(span.first, 0), geom.vertex(span.first, 1)};
    std::vector<Point2> defs;
    defs.reserve(geom.ring_count[span.first + 1]);
    for (std::uint32_t v = 0; v < geom.ring_count[span.first + 1]; ++v)
        defs.push_back(geom.vertex(span.first + 1, v));

    // THE MEAN DISPLACEMENT carries what no geometry holds: the dimension
    // line's place and the caption's, so a dimension stays as far from its
    // points as it was when they move.
    std::int64_t sx           = 0;
    std::int64_t sy           = 0;
    std::vector<Point2> moved = defs;
    for (const auto& [index, to] : edit.moves) {
        if (index >= moved.size()) return err(ErrorCode::InvalidArgument, "Ölçünün o noktası yok.");
        sx += to.x - defs[index].x;
        sy += to.y - defs[index].y;
        moved[index] = to;
    }
    const auto n    = static_cast<std::int64_t>(edit.moves.empty() ? 1 : edit.moves.size());
    const Point2 md = Point2{sx / n, sy / n};

    const Mm height = edit.text_height > 0 ? edit.text_height : doc.texts().height(slot);
    // A CAPTION PLACED BY HAND stays where the hand put it, and travels with
    // what it describes; a new place given now is a caption placed by hand.
    if (edit.caption != nullptr) def.user_text_position = true;
    const Point2 kept =
        edit.caption != nullptr ? *edit.caption : Point2{base[0].x + md.x, base[0].y + md.y};

    DimensionRebuild out;
    out.text_height = height;
    std::vector<Point2> picks;
    Point2 where{};
    if (!dimension_picks(def.type, defs, base[0], picks, where)) {
        // A type the layout cannot draw: the points move, the figure is
        // re-measured, the caption slides with them and keeps its reading.
        def.measurement  = dimension_measure(def.type, moved, def.rotation_udeg, def.ordinate_x);
        out.defs         = std::move(moved);
        out.text         = dimension_text(def, unit);
        const auto dx    = static_cast<double>(base[1].x - base[0].x);
        const auto dy    = static_cast<double>(base[1].y - base[0].y);
        const double len = std::sqrt(dx * dx + dy * dy);
        out.baseline     = dimension_baseline(kept, len > 0.0 ? dx / len : 1.0,
                                          len > 0.0 ? dy / len : 0.0, height, out.text);
        out.payload      = encode_dimension(def);
        out.def          = std::move(def);
        return out;
    }
    if (!dimension_picks(def.type, moved, base[0], picks, where))
        return err(ErrorCode::ValidationFailed, "Ölçü yeni noktalarıyla kurulamıyor.");
    std::vector<Point2> ignored;
    Point2 old_where{};
    (void)dimension_picks(def.type, defs, base[0], ignored, old_where);
    where = Point2{old_where.x + md.x, old_where.y + md.y};
    if (def.type == DimensionType::Aligned) {
        // AN ALIGNED DIMENSION TURNS WITH ITS SIDE. Its line is placed by the
        // offset from the side it measures, not by a place on the sheet, so a
        // parcel rotated a quarter turn keeps its dimension the same distance
        // out on the same side, rather than dragging it across the parcel.
        const Dir was = unit_between(defs[0], defs[1]);
        const Dir now = unit_between(picks[0], picks[1]);
        if (!was.zero() && !now.zero()) {
            const double a = dot(defs[0], old_where, was);
            const double o = dot(defs[0], old_where, was.perp());
            where          = along(along(picks[0], now, a), now.perp(), o);
        }
    }
    // A LINEAR DIMENSION KEEPS ITS DIRECTION. Chosen once, from where its line
    // was put; a corner that moves must not turn a horizontal figure vertical.
    DimensionLayout layout;
    if (!dimension_layout(def, picks, where, height, layout, true))
        return err(ErrorCode::ValidationFailed,
                   "Ölçü yeni noktalarıyla kurulamıyor: iki nokta çakıştı ya da tepe kolun ucuna "
                   "geldi.");
    out.text     = dimension_text(def, unit);
    out.baseline = dimension_caption_baseline(def, layout, out.text, height, kept);
    out.defs     = std::move(layout.defs);
    out.payload  = encode_dimension(def);
    out.def      = std::move(def);
    return out;
}

Result<DimensionRebuild> dimension_follow(const Document& doc, EntityId e,
                                          std::span<const std::pair<std::size_t, Point2>> moves,
                                          DrawingUnit unit)
{
    return dimension_rebuild(doc, e, DimensionEdit{.moves = moves}, unit);
}

} // namespace kentos::core
