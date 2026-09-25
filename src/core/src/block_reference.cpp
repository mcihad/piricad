// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/block_reference.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_fields.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace kentos::core {
namespace {

constexpr std::size_t kPayloadBytes = kind::kHeaderBytes + std::size_t{4} + std::size_t{8} * 4 + 8 +
                                      2 + 2 + 8 + 8 + std::size_t{8} * 4;

/// How many vertices a clip may carry: far beyond any boundary a hand draws,
/// and a bound on what a payload off disk may claim.
constexpr std::uint32_t kMaxClipVertices = 65'535;

/// `runs`, cropped to `clip`, into `out` — in DEFINITION space, before any
/// placement, so one clip crops every copy alike.
///
/// An open run keeps its pieces inside. A closed run wholly inside is kept as
/// it is. One the boundary cuts becomes two things: the pieces of its outline
/// inside, open, and its face inside, closed and FILL-ONLY
/// (`EmitBuffer::run_fill_only`) — filled where it fills, and never stroked
/// along the cut, which is a line nobody drew. A caption shows whole or not at
/// all, by where it stands. The clipping is Clipper2's, through the facade in
/// `core/offset.hpp` (CLAUDE.md 5.16).
void clip_runs(const EmitBuffer& runs, std::span<const Point2> clip, EmitBuffer& out)
{
    const std::vector<Point2> window(clip.begin(), clip.end());
    const auto copy_run = [&runs, &out](std::size_t r, bool closed) {
        out.begin_run(closed, runs.run_hole[r] != 0, runs.run_style[r], runs.run_layer[r],
                      runs.run_text[r]);
        if (runs.run_solid[r] != 0) out.mark_solid();
        if (!runs.run_edge(r)) out.mark_fill_only();
        const auto xs = runs.run_xs(r);
        const auto ys = runs.run_ys(r);
        for (std::size_t v = 0; v < xs.size(); ++v)
            out.push_vertex(xs[v], ys[v]);
    };

    std::vector<Point2> path;
    for (std::size_t r = 0; r < runs.run_total(); ++r) {
        const auto xs = runs.run_xs(r);
        const auto ys = runs.run_ys(r);
        if (xs.empty()) continue;
        if (runs.run_text[r] != kNoRunText) {
            if (inside_clip(clip, Point2{xs[0], ys[0]})) copy_run(r, runs.run_closed[r] != 0);
            continue;
        }
        const bool closed = runs.run_closed[r] != 0;
        path.clear();
        for (std::size_t v = 0; v < xs.size(); ++v)
            path.push_back(Point2{xs[v], ys[v]});
        if (closed) path.push_back(path.front());
        const std::vector<std::vector<Point2>> pieces = clip_path_to(path, window);
        // WHOLE AND INSIDE: one piece carrying every vertex of the run, which
        // is what an untouched run comes back as.
        if (closed && pieces.size() == 1 && pieces.front().size() == path.size()) {
            copy_run(r, true);
            continue;
        }
        // A fill-only run is a face already; it has no outline to keep.
        if (runs.run_edge(r))
            for (const std::vector<Point2>& piece : pieces) {
                if (piece.size() < 2) continue;
                out.begin_run(false, false, runs.run_style[r], runs.run_layer[r], runs.run_text[r]);
                for (const Point2 q : piece)
                    out.push_vertex(q.x, q.y);
            }
        if (!closed) continue;
        path.pop_back();
        for (const std::vector<Point2>& face : clip_ring_to(path, window)) {
            if (face.size() < 3) continue;
            out.begin_run(true, runs.run_hole[r] != 0, runs.run_style[r], runs.run_layer[r],
                          runs.run_text[r]);
            if (runs.run_solid[r] != 0) out.mark_solid();
            out.mark_fill_only();
            for (const Point2 q : face)
                out.push_vertex(q.x, q.y);
        }
    }
}

Point2 insertion_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.empty()) return Point2{};
    return Point2{xs[0], ys[0]};
}

/// The runs of one member, in DEFINITION space, into `scratch`. A nested
/// reference expands itself first; a polyline is its rings; a curve is its
/// outline.
void member_runs(const Document& doc, EntityId m, EmitBuffer& scratch, int depth)
{
    scratch.clear();
    const EntityTable& ents  = doc.entities();
    const RingGeometry& geom = doc.geometry();
    if (ents.kind[m] == kBlockReferenceKind) {
        expand_block_reference(doc, m, scratch, depth + 1);
        return;
    }
    if (curve_outline(ents.kind[m], geom, ents.slot[m], scratch)) return;
    const RingSpan rs = geom.rings_of(ents.slot[m]);
    for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
        scratch.begin_run(geom.ring_role[r] != RingRole::Open,
                          geom.ring_role[r] == RingRole::Interior);
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        for (std::size_t v = 0; v < xs.size(); ++v)
            scratch.push_vertex(xs[v], ys[v]);
    }
}

} // namespace

std::vector<std::string> block_fields(const Document& doc, BlockId block)
{
    std::vector<std::string> out;
    std::vector<BlockId> seen;
    const auto walk = [&doc, &out, &seen](const auto& self, BlockId b, int depth) -> void {
        if (depth > kMaxBlockDepth || b >= doc.blocks().size()) return;
        if (std::find(seen.begin(), seen.end(), b) != seen.end()) return;
        seen.push_back(b);
        for (const EntityKey key : doc.blocks().at(b).members) {
            const EntityId m = doc.slot_of(key);
            if (m == kNoEntity || !doc.alive(m)) continue;
            const std::uint32_t slot = doc.entities().slot[m];
            if (doc.entities().kind[m] == kBlockReferenceKind) {
                if (auto inner = block_reference_of(doc.geometry(), slot); inner)
                    self(self, inner.value().block, depth + 1);
                continue;
            }
            if (!doc.texts().has(slot)) continue;
            for (std::string& name : field_names(doc.texts().text(slot)))
                if (std::find(out.begin(), out.end(), name) == out.end())
                    out.push_back(std::move(name));
        }
    };
    walk(walk, block, 0);
    return out;
}

bool style_by_block(const Document& doc, StyleId style)
{
    if (style == kByLayerStyle || style >= doc.styles().size()) return false;
    const Appearance& a = doc.styles().at(style);
    return a.src_colour == Source::ByBlock || a.src_width == Source::ByBlock ||
           a.src_dash == Source::ByBlock || a.src_fill == Source::ByBlock;
}

bool inside_clip(std::span<const Point2> clip, Point2 p) noexcept
{
    if (clip.size() < 3) return true;
    // The crossing count `ring_contains` makes, over the corners as they are
    // stored — no copy, so the test the snap asks per point allocates nothing
    // — and ON THE BOUNDARY counts as inside: a corner the clip was drawn
    // through is shown, and snapped to.
    bool inside = false;
    for (std::size_t i = 0, j = clip.size() - 1; i < clip.size(); j = i++) {
        const Point2 a     = clip[j];
        const Point2 b     = clip[i];
        const Int128 cross = (static_cast<Int128>(b.x - a.x) * (p.y - a.y)) -
                             (static_cast<Int128>(b.y - a.y) * (p.x - a.x));
        if (cross == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
            p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y))
            return true;
        if ((b.y > p.y) == (a.y > p.y)) continue;
        // Where the edge crosses the probe's row, compared without dividing:
        // which way the edge runs says which side of it counts.
        const bool upward = b.y > a.y;
        if (upward ? cross > 0 : cross < 0) inside = !inside;
    }
    return inside;
}

std::vector<std::uint8_t> encode_block_reference(const BlockReference& ref)
{
    std::vector<std::uint8_t> out;
    kind::put_header(out, ref.clip.empty() ? kBlockReferenceLayout : kBlockReferenceClipLayout, 0);
    put_u32(out, ref.block);
    put_i64(out, ref.sx.num);
    put_i64(out, ref.sx.den);
    put_i64(out, ref.sy.num);
    put_i64(out, ref.sy.den);
    put_i64(out, ref.rotation_udeg);
    put_u16(out, ref.columns);
    put_u16(out, ref.rows);
    put_mm(out, ref.column_spacing);
    put_mm(out, ref.row_spacing);
    put_mm(out, ref.bounds.min_x);
    put_mm(out, ref.bounds.min_y);
    put_mm(out, ref.bounds.max_x);
    put_mm(out, ref.bounds.max_y);
    if (!ref.clip.empty()) {
        put_u32(out, static_cast<std::uint32_t>(ref.clip.size()));
        for (const Point2 p : ref.clip) {
            put_mm(out, p.x);
            put_mm(out, p.y);
        }
    }
    return out;
}

Result<BlockReference> decode_block_reference(std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    kind::Header h;
    if (payload.size() < kPayloadBytes || !kind::read_header(in, h))
        return err(ErrorCode::ParseError,
                   "Blok referansı yükü en az " + std::to_string(kPayloadBytes) +
                       " bayt olmalı; verilen " + std::to_string(payload.size()) + ".");
    if (h.version != kBlockReferenceLayout && h.version != kBlockReferenceClipLayout)
        return err(ErrorCode::Unsupported,
                   "Blok referansı yükünün düzeni bu yapının tanımadığı bir sürümde: " +
                       std::to_string(h.version));
    if (h.version == kBlockReferenceLayout && payload.size() != kPayloadBytes)
        return err(ErrorCode::ParseError, "Blok referansı yükü " + std::to_string(kPayloadBytes) +
                                              " bayt olmalı; verilen " +
                                              std::to_string(payload.size()) + ".");
    BlockReference ref;
    ref.block          = in.u32();
    ref.sx.num         = in.i64();
    ref.sx.den         = in.i64();
    ref.sy.num         = in.i64();
    ref.sy.den         = in.i64();
    ref.rotation_udeg  = in.i64();
    ref.columns        = in.u16();
    ref.rows           = in.u16();
    ref.column_spacing = in.mm();
    ref.row_spacing    = in.mm();
    ref.bounds.min_x   = in.mm();
    ref.bounds.min_y   = in.mm();
    ref.bounds.max_x   = in.mm();
    ref.bounds.max_y   = in.mm();
    if (h.version == kBlockReferenceClipLayout) {
        // Bounded against the bytes that are actually there: a count off disk
        // is a claim, and a claim is checked before anything is reserved.
        const std::size_t left = payload.size() - kPayloadBytes;
        if (left < 4)
            return err(ErrorCode::ParseError, "Kırpılmış blok referansının köşe sayısı eksik.");
        const std::uint32_t count = in.u32();
        if (count < 3 || count > kMaxClipVertices || left != 4 + std::size_t{16} * count)
            return err(ErrorCode::ParseError,
                       "Kırpılmış blok referansının köşeleri yükle uyuşmuyor (" +
                           std::to_string(count) + " köşe).");
        ref.clip.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const Mm x = in.mm();
            const Mm y = in.mm();
            ref.clip.push_back(Point2{x, y});
        }
    }
    if (ref.block == kNoBlock)
        return err(ErrorCode::ValidationFailed, "Blok referansı bir blok tanımı adlandırmıyor.");
    if (ref.sx.den <= 0 || ref.sy.den <= 0 || ref.sx.num == 0 || ref.sy.num == 0)
        return err(ErrorCode::ValidationFailed,
                   "Blok referansının ölçeği sıfır olamaz ve paydası pozitif olmalı.");
    if (ref.columns == 0 || ref.rows == 0)
        return err(ErrorCode::ValidationFailed, "Blok referansının dizisi en az 1×1 olmalı.");
    return ref;
}

Result<BlockReference> block_reference_of(const RingGeometry& geom, std::uint32_t slot)
{
    return decode_block_reference(geom.payload_of(slot));
}

Point2 block_reference_insertion(const RingGeometry& geom, std::uint32_t slot)
{
    return insertion_of(geom, slot);
}

Point2 place_block_point(const BlockReference& ref, Point2 insertion, Point2 base, Point2 p,
                         int column, int row) noexcept
{
    // Scale about the base, step along the grid, turn about the origin, place.
    // Every step is integer arithmetic: `mul_div_round` carries the product in
    // 128 bits, `rotate_udeg` is exact at the quarter turns.
    const Point2 local{mul_div_round(p.x - base.x, ref.sx.num, ref.sx.den) +
                           static_cast<Mm>(column) * ref.column_spacing,
                       mul_div_round(p.y - base.y, ref.sy.num, ref.sy.den) +
                           static_cast<Mm>(row) * ref.row_spacing};
    const Point2 turned = rotate_udeg(local, Point2{0, 0}, ref.rotation_udeg);
    return Point2{insertion.x + turned.x, insertion.y + turned.y};
}

Point2 unplace_block_point(const BlockReference& ref, Point2 insertion, Point2 base,
                           Point2 world) noexcept
{
    // The steps of `place_block_point` backwards: unplace, turn back, unscale.
    // A ratio's sign lives in its numerator and `mul_div_round` divides by a
    // positive number, so the sign moves across.
    const Point2 turned{world.x - insertion.x, world.y - insertion.y};
    const Point2 local = rotate_udeg(turned, Point2{0, 0}, -ref.rotation_udeg);
    const auto unscale = [](Mm v, Ratio r) {
        return r.num < 0 ? mul_div_round(v, -r.den, -r.num) : mul_div_round(v, r.den, r.num);
    };
    return Point2{base.x + unscale(local.x, ref.sx), base.y + unscale(local.y, ref.sy)};
}

bool expand_block_reference(const Document& doc, EntityId e, EmitBuffer& into, int depth)
{
    const EntityTable& ents  = doc.entities();
    const RingGeometry& geom = doc.geometry();
    if (e >= ents.size()) return false;
    const std::uint32_t slot = ents.slot[e];
    auto decoded             = block_reference_of(geom, slot);
    if (!decoded) return false;
    return expand_block_definition(doc, insertion_of(geom, slot), decoded.value(), into, depth);
}

bool expand_block_definition(const Document& doc, Point2 insertion, const BlockReference& ref,
                             EmitBuffer& into, int depth)
{
    if (depth > kMaxBlockDepth) return false;
    const EntityTable& ents = doc.entities();
    if (ref.block >= doc.blocks().size()) return false;
    const BlockDef& def = doc.blocks().at(ref.block);
    const LayerId zero  = doc.find_layer("0");

    EmitBuffer scratch;
    for (const EntityKey key : def.members) {
        const EntityId m = doc.slot_of(key);
        if (m == kNoEntity || !ents.alive(m) || (ents.flags[m] & FlagHidden) != 0) continue;
        // A member on a switched-off layer is off, as it would be on its own;
        // one on `0` follows the layer the reference is on.
        if (ents.layer[m] != zero && ents.layer[m] < doc.layers().size() &&
            !doc.layers()[ents.layer[m]].visible)
            continue;
        member_runs(doc, m, scratch, depth);
        if (!ref.clip.empty()) {
            // CROPPED WHERE IT IS DEFINED: the clip is in the definition's own
            // coordinates, so every copy below is cropped alike.
            EmitBuffer cropped;
            clip_runs(scratch, ref.clip, cropped);
            scratch = std::move(cropped);
        }
        if (scratch.run_total() == 0) continue;

        // What the member keeps of its own: its layer, unless it is the
        // drawing's `0` layer, which means "the layer I am placed on"; its
        // style, unless it is ByLayer (then the layer decides) or ByBlock (then
        // the reference decides); its caption.
        const LayerId lyr         = ents.layer[m];
        const std::uint32_t layer = lyr == zero ? kInheritRunLayer : lyr;
        const StyleId st          = ents.style[m];
        const std::uint32_t style =
            (st == kByLayerStyle || style_by_block(doc, st)) ? kInheritRunStyle : st;
        const std::uint32_t text = doc.texts().has(ents.slot[m]) ? ents.slot[m] : kNoRunText;

        for (int row = 0; row < static_cast<int>(ref.rows); ++row) {
            for (int col = 0; col < static_cast<int>(ref.columns); ++col) {
                for (std::size_t r = 0; r < scratch.run_total(); ++r) {
                    // A nested reference already resolved its own members' style
                    // and layer; what it left as "inherit" is now ours to say.
                    const std::uint32_t rs =
                        scratch.run_style[r] == kInheritRunStyle ? style : scratch.run_style[r];
                    const std::uint32_t rl =
                        scratch.run_layer[r] == kInheritRunLayer ? layer : scratch.run_layer[r];
                    const std::uint32_t rt =
                        scratch.run_text[r] == kNoRunText ? text : scratch.run_text[r];
                    into.begin_run(scratch.run_closed[r] != 0, scratch.run_hole[r] != 0, rs, rl,
                                   rt);
                    if (scratch.run_solid[r] != 0) into.mark_solid();
                    if (!scratch.run_edge(r)) into.mark_fill_only();
                    const auto xs = scratch.run_xs(r);
                    const auto ys = scratch.run_ys(r);
                    for (std::size_t v = 0; v < xs.size(); ++v) {
                        const Point2 placed = place_block_point(ref, insertion, def.base,
                                                                Point2{xs[v], ys[v]}, col, row);
                        into.push_vertex(placed.x, placed.y);
                    }
                }
            }
        }
    }
    return true;
}

Box2 block_reference_bounds(const Document& doc, Point2 insertion, const BlockReference& ref)
{
    Box2 box{};
    if (ref.block >= doc.blocks().size()) return box;
    const BlockDef& def     = doc.blocks().at(ref.block);
    const EntityTable& ents = doc.entities();
    EmitBuffer scratch;
    EmitBuffer cropped;
    for (const EntityKey key : def.members) {
        const EntityId m = doc.slot_of(key);
        if (m == kNoEntity || !ents.alive(m)) continue;
        member_runs(doc, m, scratch, 0);
        // The box of what is DRAWN: a clipped reference's box is its crop's.
        if (!ref.clip.empty()) {
            cropped.clear();
            clip_runs(scratch, ref.clip, cropped);
            std::swap(scratch, cropped);
        }
        for (int row = 0; row < static_cast<int>(ref.rows); ++row)
            for (int col = 0; col < static_cast<int>(ref.columns); ++col)
                for (std::size_t v = 0; v < scratch.xs.size(); ++v)
                    box.extend(place_block_point(ref, insertion, def.base,
                                                 Point2{scratch.xs[v], scratch.ys[v]}, col, row));
    }
    return box;
}

Mm caption_height_along(const RingGeometry& geom, std::uint32_t slot, Point2 a, Point2 b, Mm height)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return height;
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.size() < 2) return height;
    // Metres, so the squares stay far inside a double's exact range; `sqrt` is
    // correctly rounded by IEEE-754, the one root §7.3 allows.
    const auto length = [](Mm x0, Mm y0, Mm x1, Mm y1) {
        const double dx = mm_to_metres(x1 - x0);
        const double dy = mm_to_metres(y1 - y0);
        return std::sqrt(dx * dx + dy * dy);
    };
    const double was = length(xs.front(), ys.front(), xs.back(), ys.back());
    if (was <= 0.0) return height;
    const double now = length(a.x, a.y, b.x, b.y);
    return std::max<Mm>(1, mm_round(static_cast<double>(height) * now / was));
}

namespace {

void br_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        auto ref = block_reference_of(geom, slots[i]);
        // The stored box of the drawn form; the insertion point when the
        // definition had nothing to draw.
        if (ref && !ref.value().bounds.empty())
            out[i] = ref.value().bounds;
        else
            out[i] = geom.bounds_of(slots[i]);
    }
}

void br_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    // Over the geometry alone a reference is its insertion point; the members
    // need the document and come through `entity_outline` (outline.hpp).
    for (const std::uint32_t slot : slots) {
        const Point2 p = insertion_of(geom, slot);
        into.begin_run(false);
        into.push_vertex(p.x, p.y);
    }
}

void br_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
            std::span<std::uint8_t> out)
{
    // The pick test walks the drawn runs through the document (pick.cpp); this
    // geometry-only answer reaches for the insertion point.
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 p = insertion_of(geom, slots[i]);
        out[i] =
            kind::segment_distance2_m(p, p, probe) <= limit ? std::uint8_t{1} : std::uint8_t{0};
    }
}

void br_area(const RingGeometry&, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm2{0};
}

void br_perimeter(const RingGeometry&, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm{0};
}

void br_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
              std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const Point2 p = insertion_of(geom, slot);
        put_mm(bytes, p.x);
        put_mm(bytes, p.y);
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> br_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    WireReader in(payload);
    if (!in.remaining(16))
        return err(ErrorCode::ParseError, "Blok referansı yükü ekleme noktasını taşımıyor.");
    const Mm x = in.mm();
    const Mm y = in.mm();
    const Point2 pts[1]{Point2{x, y}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 1), RingRole::Open, 0};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

Status br_validate(std::span<const RingGeometry::RingInput> rings,
                   std::span<const std::uint8_t> payload)
{
    if (auto st = kind::one_open_ring("Blok referansı", rings, 1); !st) return st;
    auto ref = decode_block_reference(payload);
    if (!ref) return ref.error();
    return ok();
}

void br_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    kind::offer(into, insertion_of(geom, slot), kind::kKeyInsertion);
}

} // namespace

KENTOS_KIND(block_reference)
{
    KindSpec s{};
    s.id        = kBlockReferenceKind;
    s.stable_id = "core.block_reference";
    s.summary_tr = "Bir blok tanımını noktaya, ölçekle, açıyla ve dizi olarak yerleştiren nesne.";
    s.names[0]   = "BLOKREFERANSI";
    s.names[1]   = "BLOKREFERANSI";
    s.names[2]   = "INSERT";
    s.names[3]   = "BR";
    s.bbox       = &br_bbox;
    s.outline    = &br_outline;
    s.hit        = &br_hit;
    s.area       = &br_area;
    s.perimeter  = &br_perimeter;
    s.read       = &br_read;
    s.write      = &br_write;
    s.validate   = &br_validate;
    s.key_points = &br_key_points;
    return s;
}

} // namespace kentos::core
