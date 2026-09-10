// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/hatch.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace kentos::core {
namespace {

constexpr std::uint16_t kFlagSolid    = 1u << 0;
constexpr std::uint16_t kFlagDouble   = 1u << 1;
constexpr std::uint16_t kFlagGradient = 1u << 2;
constexpr std::uint16_t kFlagAssoc    = 1u << 3;
constexpr std::size_t kMaxName        = 255;
constexpr std::uint32_t kMaxFamilies  = 64;
constexpr std::uint32_t kMaxDashes    = 32;

void ht_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    // The boundary, exactly as a face draws: closed runs, islands as holes.
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            into.begin_run(true, geom.ring_role[r] == RingRole::Interior);
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                into.push_vertex(xs[v], ys[v]);
        }
    }
}

void ht_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.bounds_of(slots[i]);
}

void ht_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    EmitBuffer runs;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        runs.clear();
        const std::uint32_t one[1]{slots[i]};
        ht_outline(geom, SlotSpan(one, 1), runs);
        out[i] = kind::runs_hit(runs, probe, tolerance, true) ? 1 : 0;
    }
}

void ht_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.area_of(slots[i]);
}

void ht_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.perimeter_of(slots[i]);
}

void ht_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
              std::vector<std::uint32_t>& ends)
{
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

Result<std::uint32_t> ht_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError, "Tarama yükü halka sayısını taşımıyor.");
    const std::uint32_t ring_count = in.u32();
    if (!in.remaining_records(ring_count, 7))
        return err(ErrorCode::ParseError, "Tarama yükü " + std::to_string(ring_count) +
                                              " halka bildiriyor, bu kadar veri yok.");
    std::vector<std::vector<Point2>> store(ring_count);
    std::vector<RingGeometry::RingInput> rings(ring_count);
    for (std::uint32_t r = 0; r < ring_count; ++r) {
        if (!in.remaining(7))
            return err(ErrorCode::ParseError, "Halka " + std::to_string(r) + " başlığı eksik.");
        const std::uint8_t role_byte = in.u8();
        const std::uint16_t part     = in.u16();
        const std::uint32_t verts    = in.u32();
        if (role_byte > static_cast<std::uint8_t>(RingRole::Interior))
            return err(ErrorCode::ParseError,
                       "Bilinmeyen halka rolü: " + std::to_string(role_byte));
        if (!in.remaining_records(verts, 16))
            return err(ErrorCode::ParseError, "Halka " + std::to_string(r) + " için " +
                                                  std::to_string(verts) +
                                                  " tepe noktası bildirilmiş, bu kadar veri yok.");
        store[r].resize(verts);
        for (std::uint32_t v = 0; v < verts; ++v) {
            store[r][v].x = in.mm();
            store[r][v].y = in.mm();
        }
        rings[r].points = std::span<const Point2>(store[r]);
        rings[r].role   = static_cast<RingRole>(role_byte);
        rings[r].part   = part;
    }
    return geom.append(std::span<const RingGeometry::RingInput>(rings));
}

Status ht_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    if (rings.empty())
        return err(ErrorCode::ValidationFailed, "Tarama en az bir sınır halkası ister.");
    for (const RingGeometry::RingInput& r : rings)
        if (r.role == RingRole::Open)
            return err(ErrorCode::ValidationFailed,
                       "Tarama sınırı kapalıdır; açık bir halka tarama sınırı olamaz.");
    auto def = decode_hatch(payload);
    if (!def) return def.error();
    return ok();
}

} // namespace

std::vector<std::uint8_t> encode_hatch(const HatchDef& def)
{
    std::vector<std::uint8_t> out;
    std::uint16_t flags = 0;
    if (def.solid) flags |= kFlagSolid;
    if (def.double_lines) flags |= kFlagDouble;
    if (def.gradient_dropped) flags |= kFlagGradient;
    if (def.associative) flags |= kFlagAssoc;
    kind::put_header(out, kHatchLayout, flags);
    put_u16(out, def.style);
    put_u16(out, def.pattern_type);
    put_i64(out, def.angle_udeg);
    put_i64(out, def.scale.num);
    put_i64(out, def.scale.den);
    put_mm(out, def.origin.x);
    put_mm(out, def.origin.y);
    kind::put_string(out, def.name.size() > kMaxName ? def.name.substr(0, kMaxName) : def.name);
    put_u32(out, static_cast<std::uint32_t>(def.families.size()));
    for (const HatchDef::Family& f : def.families) {
        put_i64(out, f.angle_udeg);
        put_i64(out, f.base_x_um);
        put_i64(out, f.base_y_um);
        put_i64(out, f.offset_x_um);
        put_i64(out, f.offset_y_um);
        put_u32(out, static_cast<std::uint32_t>(f.dashes_um.size()));
        for (const std::int64_t d : f.dashes_um)
            put_i64(out, d);
    }
    return out;
}

Result<HatchDef> decode_hatch(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (!kind::read_header(in, h) || !in.remaining(2 + 2 + 8 * 5))
        return err(ErrorCode::ParseError,
                   "Tarama yükü başlığı ve deseni taşıyacak kadar uzun değil.");
    if (h.version != kHatchLayout)
        return err(ErrorCode::Unsupported,
                   "Tarama yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    HatchDef def;
    def.solid            = (h.flags & kFlagSolid) != 0;
    def.double_lines     = (h.flags & kFlagDouble) != 0;
    def.gradient_dropped = (h.flags & kFlagGradient) != 0;
    def.associative      = (h.flags & kFlagAssoc) != 0;
    def.style            = in.u16();
    def.pattern_type     = in.u16();
    def.angle_udeg       = in.i64();
    def.scale.num        = in.i64();
    def.scale.den        = in.i64();
    def.origin.x         = in.mm();
    def.origin.y         = in.mm();
    if (def.scale.den <= 0 || def.scale.num <= 0)
        return err(ErrorCode::ValidationFailed, "Tarama ölçeği pozitif bir oran olmalı.");
    if (!kind::read_string(in, def.name, kMaxName))
        return err(ErrorCode::ParseError, "Tarama desen adı okunamadı.");
    if (!in.remaining(4)) return err(ErrorCode::ParseError, "Tarama yükü aile sayısını taşımıyor.");
    const std::uint32_t families = in.u32();
    if (families > kMaxFamilies)
        return err(ErrorCode::ValidationFailed,
                   "Tarama deseni en çok " + std::to_string(kMaxFamilies) + " çizgi ailesi taşır.");
    def.families.reserve(families);
    for (std::uint32_t i = 0; i < families; ++i) {
        if (!in.remaining(8 * 5 + 4))
            return err(ErrorCode::ParseError,
                       "Tarama yükünün " + std::to_string(i) + ". çizgi ailesi eksik.");
        HatchDef::Family f;
        f.angle_udeg               = in.i64();
        f.base_x_um                = in.i64();
        f.base_y_um                = in.i64();
        f.offset_x_um              = in.i64();
        f.offset_y_um              = in.i64();
        const std::uint32_t dashes = in.u32();
        if (dashes > kMaxDashes || !in.remaining_records(dashes, 8))
            return err(ErrorCode::ParseError, "Tarama yükünün " + std::to_string(i) +
                                                  ". çizgi ailesinin kesik dizisi okunamadı.");
        f.dashes_um.reserve(dashes);
        for (std::uint32_t d = 0; d < dashes; ++d)
            f.dashes_um.push_back(in.i64());
        def.families.push_back(std::move(f));
    }
    if (in.left() != 0)
        return err(ErrorCode::ParseError,
                   "Tarama yükünün sonunda " + std::to_string(in.left()) + " fazla bayt var.");
    return def;
}

Result<HatchDef> hatch_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_hatch(geom.payload_of(slot));
}

Mm hatch_family_spacing_mm(const HatchDef& def, const HatchDef::Family& family) noexcept
{
    // The offset is given in the family's own frame (x along the lines, y
    // across), so the spacing between lines is its y component, scaled, and
    // brought from micrometres to millimetres.
    const std::int64_t across = family.offset_y_um < 0 ? -family.offset_y_um : family.offset_y_um;
    if (across == 0 || def.scale.den <= 0) return 0;
    return mul_div_round(across, def.scale.num, def.scale.den * 1000);
}

KENTOS_KIND(hatch)
{
    KindSpec s{};
    s.id         = kHatchKind;
    s.stable_id  = "core.hatch";
    s.summary_tr = "Sınır halkaları ve deseniyle tanımlı tarama; dolu ya da çizgi desenli.";
    s.names[0]   = "TARAMA";
    s.names[1]   = "TARAMA";
    s.names[2]   = "HATCH";
    s.names[3]   = "TRM";
    s.bbox       = &ht_bbox;
    s.outline    = &ht_outline;
    s.hit        = &ht_hit;
    s.area       = &ht_area;
    s.perimeter  = &ht_perimeter;
    s.read       = &ht_read;
    s.write      = &ht_write;
    s.validate   = &ht_validate;
    return s;
}

} // namespace kentos::core
