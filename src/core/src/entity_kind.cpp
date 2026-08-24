// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/entity_kind.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <cstring>

namespace piricad::core {
namespace {

// ---------------------------------------------------------- core.polyline ----
//
// The one built-in kind. Its six functions are free functions over spans: they
// take the geometry store and a batch of slots, and they close over nothing.

void polyline_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.bounds_of(slots[i]);
}

void polyline_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.area_of(slots[i]);
}

void polyline_emit(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            into.begin_run(geom.ring_role[r] != RingRole::Open);
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                into.push_vertex(xs[v], ys[v]);
        }
    }
}

/// Squared distance from `p` to segment `a`-`b`, in square metres.
///
/// Metres rather than millimetres because the square of a TM3 coordinate
/// difference in mm overflows the 53-bit mantissa long before it overflows
/// int64, and the whole point of translating to the probe first is to keep the
/// operands small. `double` is transient here and never reaches a member
/// (core.md R3).
double segment_distance2_m(Point2 a, Point2 b, Point2 p)
{
    const double vx = mm_to_metres(b.x - a.x);
    const double vy = mm_to_metres(b.y - a.y);
    const double wx = mm_to_metres(p.x - a.x);
    const double wy = mm_to_metres(p.y - a.y);

    const double vv = vx * vx + vy * vy;
    double t        = 0.0;
    if (vv > 0.0) {
        t = (wx * vx + wy * vy) / vv;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }

    const double dx = wx - t * vx;
    const double dy = wy - t * vy;
    return dx * dx + dy * dy;
}

void polyline_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
                  std::span<std::uint8_t> out)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i]            = 0;
        const RingSpan rs = geom.rings_of(slots[i]);

        for (std::uint32_t r = rs.first; r < rs.first + rs.count && out[i] == 0; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            if (xs.empty()) continue;

            // A one-vertex ring has no segment; measure to the vertex itself so a
            // degenerate record is still selectable rather than invisible.
            if (xs.size() == 1) {
                const Point2 v{xs[0], ys[0]};
                if (segment_distance2_m(v, v, probe) <= limit) out[i] = 1;
                continue;
            }

            const bool closed      = geom.ring_role[r] != RingRole::Open;
            const std::size_t last = xs.size() - 1;
            const std::size_t segs = closed ? xs.size() : last;
            for (std::size_t s = 0; s < segs; ++s) {
                const std::size_t j = (s == last) ? 0 : s + 1;
                const Point2 a{xs[s], ys[s]};
                const Point2 b{xs[j], ys[j]};
                if (segment_distance2_m(a, b, probe) <= limit) {
                    out[i] = 1;
                    break;
                }
            }
        }
    }
}

// -------------------------------------------------------------- payload ------
//
// Little-endian everywhere, assembled byte by byte: a file written on x86 is
// read on Apple Silicon and R26 promises the payload comes back byte-identical.
// memcpy rather than a cast for the signed/unsigned hop, so no coordinate ever
// meets static_cast<Mm> outside the units.hpp rounding helper (core.md R20).

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    for (int i = 0; i < 2; ++i)
        out.push_back(
            static_cast<std::uint8_t>((static_cast<std::uint32_t>(v) >> (i * 8)) & 0xFFu));
}

void put_mm(std::vector<std::uint8_t>& out, Mm v)
{
    std::uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<std::uint8_t>((u >> (i * 8)) & 0xFFu));
}

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::size_t left() const noexcept { return bytes_.size() - at_; }

    bool remaining(std::size_t n) const noexcept { return left() >= n; }

    /// `count` records of `stride` bytes each, without ever computing the
    /// product: a hostile file declares four billion rings, and on a 32-bit
    /// build `count * stride` wraps to a number the payload happily satisfies.
    bool remaining_records(std::uint32_t count, std::size_t stride) const noexcept
    {
        return count <= left() / stride;
    }

    std::uint32_t u32()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 4;
        return v;
    }

    std::uint16_t u16()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 2; ++i)
            v |= static_cast<std::uint32_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 2;
        return static_cast<std::uint16_t>(v);
    }

    std::uint8_t u8() { return bytes_[at_++]; }

    Mm mm()
    {
        std::uint64_t u = 0;
        for (int i = 0; i < 8; ++i)
            u |= static_cast<std::uint64_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 8;
        Mm v = 0;
        std::memcpy(&v, &u, sizeof(v));
        return v;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t at_{0};
};

void polyline_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
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

Result<std::uint32_t> polyline_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError,
                   "Çoklu çizgi yükü halka sayısını taşıyacak kadar uzun değil.");

    const std::uint32_t ring_count = in.u32();
    // Cheapest possible ring is 7 header bytes; refuse a count the file cannot
    // hold before reserving anything for it.
    if (!in.remaining_records(ring_count, 7))
        return err(ErrorCode::ParseError, "Çoklu çizgi yükü " + std::to_string(ring_count) +
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

} // namespace

PIRICAD_KIND(polyline)
{
    KindSpec s{};
    s.id         = 1;
    s.stable_id  = "core.polyline";
    s.summary_tr = "Açık ya da kapalı halkalardan oluşan temel çizgi nesnesi.";
    s.names[0]   = "ÇOKLUÇİZGİ";
    s.names[1]   = "COKLUCIZGI";
    s.names[2]   = "POLYLINE";
    s.names[3]   = "PL";
    s.bbox       = &polyline_bbox;
    s.emit       = &polyline_emit;
    s.hit        = &polyline_hit;
    s.area       = &polyline_area;
    s.read       = &polyline_read;
    s.write      = &polyline_write;
    return s;
}

// ----------------------------------------------------------- KindTable -------

Status KindTable::add(const KindSpec& spec)
{
    if (spec.id == kNoKind) return err(ErrorCode::InvalidArgument, "Nesne türü kimliği atanmamış.");
    if (spec.stable_id == nullptr || *spec.stable_id == '\0')
        return err(ErrorCode::InvalidArgument,
                   "Nesne türü kalıcı kimliği boş olamaz (örnek: \"core.polyline\").");
    if (find(spec.id) != nullptr)
        return err(ErrorCode::ValidationFailed,
                   "Nesne türü kimliği zaten kayıtlı: " + std::to_string(spec.id));

    // A null here would become an indirect call through zero in the middle of a
    // frame; refusing at registration turns a crash into a message.
    if (spec.bbox == nullptr || spec.emit == nullptr || spec.hit == nullptr ||
        spec.area == nullptr || spec.read == nullptr || spec.write == nullptr)
        return err(ErrorCode::InvalidArgument, std::string("'") + spec.stable_id +
                                                   "' nesne türünün eksik işlev işaretçisi var.");

    if (spec.names[0] == nullptr || *spec.names[0] == '\0')
        return err(ErrorCode::InvalidArgument,
                   std::string("'") + spec.stable_id + "' nesne türünün Türkçe adı yok.");

    std::vector<std::pair<std::string, KindId>> pending;
    for (const char* n : spec.names) {
        if (n == nullptr || *n == '\0') continue;
        std::string folded = turkish_fold_key(n);
        if (find_name(folded) != nullptr)
            return err(ErrorCode::ValidationFailed, "Nesne türü adı zaten kullanılıyor: " + folded);

        // A repeat WITHIN one spec is not a defect and must not be one: the names
        // are declared as a Turkish spelling and its ASCII fold (CLAUDE.md 2.6),
        // and a lookup key folds the two alphabets together on purpose, so
        // `ÇOKLUÇİZGİ` and `COKLUCIZGI` arrive as the same key. They name the same
        // kind, so the second is simply already accounted for. Across two specs it
        // is still an error, and that is the check above.
        bool already = false;
        for (const auto& p : pending)
            if (p.first == folded) already = true;
        if (already) continue;

        pending.emplace_back(std::move(folded), spec.id);
    }

    specs_.push_back(spec);
    for (auto& p : pending) {
        const auto at =
            std::lower_bound(folded_.begin(), folded_.end(), p.first,
                             [](const auto& e, const std::string& k) { return e.first < k; });
        folded_.insert(at, std::move(p));
    }
    return ok();
}

const KindSpec* KindTable::find(KindId id) const noexcept
{
    for (const auto& s : specs_)
        if (s.id == id) return &s;
    return nullptr;
}

const KindSpec* KindTable::find_name(std::string_view name) const
{
    const std::string folded = turkish_fold_key(name);
    const auto at =
        std::lower_bound(folded_.begin(), folded_.end(), folded,
                         [](const auto& e, const std::string& k) { return e.first < k; });
    if (at == folded_.end() || at->first != folded) return nullptr;
    return find(at->second);
}

// The one and only list of built-in entity kinds (R25). Adding a kind means one
// factory above and one line here — the same idiom as the command list, and the
// integer id lives with the factory because it reaches the file format.
#define PIRICAD_BUILTIN_KINDS(X) X(polyline)

const KindTable& builtin_kinds()
{
    // Immutable from the first read onward: built once, never added to, handed
    // out as const. That is const state, which core.md P8 permits — it is not a
    // registry, and nothing can reach in and change what "core.polyline" means.
    static const KindTable table = [] {
        KindTable t;
#define PIRICAD_REGISTER_KIND(sym) (void)t.add(piricad_kind_##sym());
        PIRICAD_BUILTIN_KINDS(PIRICAD_REGISTER_KIND)
#undef PIRICAD_REGISTER_KIND
        return t;
    }();
    return table;
}

#undef PIRICAD_BUILTIN_KINDS

} // namespace piricad::core
