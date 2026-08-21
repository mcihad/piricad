// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/geometry.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>

namespace piricad::core {
namespace {

/// Grows geometrically, so appending N rings is O(N) rather than O(N²).
/// `reserve(size() + n)` pins capacity to the exact size and turns a bulk load
/// into quadratic copying; that bug already cost this project a two-minute load.
template<class T> void reserve_for(std::vector<T>& v, std::size_t extra)
{
    const std::size_t needed = v.size() + extra;
    if (needed <= v.capacity()) return;
    v.reserve(std::max(needed, v.capacity() * 2));
}

// CLOSURE DECISION — a closed ring is stored WITHOUT the duplicated closing
// vertex, and EVERY trailing repeat of the first vertex is dropped.
//
// DXF LWPOLYLINE (flag 70 bit 0) and GeoJSON both repeat the first vertex at the
// end, and a writer that closes an already-closed ring emits it twice. Storing
// the repeat would put a zero-length segment inside every imported parcel and,
// worse, make two geometrically identical parcels differ in vertex count — so
// they would differ in the content hash and in every golden fixture, depending
// only on which importer produced them (R11). Dropping only ONE repeat left that
// hole open: {A,B,A,A} kept four corners while {A,B,C,A} kept three.
// `ring_count` is therefore the number of distinct corners, and the closing
// segment is implied by the role.
//
// Normalisation is not forgiveness: what the repeats hide is a degenerate ring,
// and append() rejects that separately on its area (see zero_area below), so
// {A,B,A,A} is refused for what is actually wrong with it rather than accepted
// with a corner count that depends on the writer.
//
// An Open ring is never trimmed — first == last on a polyline is the caller's
// geometry, not a closure convention.
std::size_t stored_count(const RingGeometry::RingInput& r) noexcept
{
    std::size_t n = r.points.size();
    if (r.role == RingRole::Open) return n;
    while (n >= 2 && r.points[n - 1] == r.points[0])
        --n;
    return n;
}

const char* role_name(RingRole r) noexcept
{
    switch (r) {
    case RingRole::Open: return "açık";
    case RingRole::Exterior: return "dış";
    case RingRole::Interior: return "iç";
    }
    return "tanımsız";
}

// ---------------------------------------------------------- exact 128-bit ----
//
// Every quantity below is exact for any coordinate append() accepts. __int128 is
// not used: MSVC does not have it and CLAUDE.md 6.1 requires the same numbers on
// all three platforms, so the two limbs are carried by hand.

struct U128
{
    std::uint64_t hi{0};
    std::uint64_t lo{0};
};

constexpr U128 mul_u64(std::uint64_t a, std::uint64_t b) noexcept
{
    const std::uint64_t a0 = a & 0xFFFFFFFFu, a1 = a >> 32;
    const std::uint64_t b0 = b & 0xFFFFFFFFu, b1 = b >> 32;

    const std::uint64_t p00 = a0 * b0;
    const std::uint64_t p01 = a0 * b1;
    const std::uint64_t p10 = a1 * b0;
    const std::uint64_t p11 = a1 * b1;

    const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);

    U128 r;
    r.lo = (p00 & 0xFFFFFFFFu) | (mid << 32);
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

constexpr U128 add_u128(U128 a, U128 b) noexcept
{
    U128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1u : 0u);
    return r;
}

constexpr U128 sub_u128(U128 a, U128 b) noexcept
{
    U128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1u : 0u);
    return r;
}

/// -1, 0, +1. Plain comparison, high limb first.
constexpr int cmp_u128(U128 a, U128 b) noexcept
{
    if (a.hi != b.hi) return a.hi < b.hi ? -1 : 1;
    if (a.lo != b.lo) return a.lo < b.lo ? -1 : 1;
    return 0;
}

constexpr U128 shl2_u128(U128 v) noexcept
{
    U128 r;
    r.hi = (v.hi << 2) | (v.lo >> 62);
    r.lo = v.lo << 2;
    return r;
}

/// |a - b| for two Mm values, as an exact unsigned magnitude.
///
/// Subtracting first would overflow: ax - bx is undefined the moment the two sit
/// at opposite ends of int64, and a coordinate pair that far apart is precisely
/// what a corrupt import produces. Comparing first and subtracting unsigned is
/// exact over the whole range.
constexpr std::uint64_t delta(Mm a, Mm b) noexcept
{
    return a >= b ? static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)
                  : static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a);
}

/// Square root of a 128-bit value, rounded to the nearest integer, digit by digit.
///
/// No floating point anywhere: a kenar uzunluğu ends up printed on a röper
/// krokisi, and seeding this from `std::sqrt` would make the answer depend on the
/// platform's rounding of the seed (§7.3). The restoring loop leaves the
/// remainder v - root², which is exactly what the rounding test needs.
constexpr std::uint64_t round_sqrt_u128(U128 v) noexcept
{
    U128 rest{0, 0};
    std::uint64_t root = 0;

    for (int i = 0; i < 64; ++i) {
        // Pull the next two bits of v into the remainder.
        rest = shl2_u128(rest);
        rest.lo |= (v.hi >> 62) & 0x3u;
        v = shl2_u128(v);

        // Trial divisor 4*root + 1: the remainder holds prefix - root², the
        // prefix has just grown by two bits, and (2·root + 1)² - (2·root)² is
        // 4·root + 1. Carried in 128 bits because root reaches 2^63.
        const U128 trial{root >> 62, (root << 2) | 1u};

        root <<= 1;
        if (cmp_u128(rest, trial) >= 0) {
            rest = sub_u128(rest, trial);
            root |= 1u;
        }
    }

    // root is floor(sqrt(v)) and rest is v - root²; (root + 0.5)² = root² + root +
    // 0.25, so the value rounds up exactly when the remainder passes root.
    return cmp_u128(rest, U128{0, root}) > 0 ? root + 1u : root;
}

/// Twice the signed area of a ring, counter-clockwise positive, modulo 2^64.
///
/// Doubled, because the exact signed area of an integer ring is a half-integer
/// and halving once at the end of `area_of` keeps a parcel-minus-hole result
/// exact instead of rounding each ring on its own.
///
/// Translating to the first vertex is not an optimisation. In TUREF/TM3 the sağa
/// değer routinely carries the dilim number in front — 30 485 320 m, i.e. 3.05e10
/// mm — and one raw x*y term against a 4.3e9 mm yukarı değer is 1.3e20, fourteen
/// times past int64's 9.2e18 ceiling. Even inside one dilim, summing the two
/// halves of the shoelace separately passes it after four vertices.
///
/// Accumulated as unsigned: two's-complement wraparound is defined behaviour,
/// signed overflow is not, and UBSan runs over this suite. For every ring whose
/// true doubled area fits in int64 the bit pattern is the exact value; the
/// representable ceiling is a true area of 2^62-1 mm² ≈ 4.6e6 km², which is 5.9x
/// Turkey's 7.8e17 mm² and 461x a 100 km x 100 km il mosaic.
template<class Get> std::uint64_t twice_area_acc(Get get, std::size_t n) noexcept
{
    if (n < 3) return 0;

    const Point2 origin    = get(0);
    const std::uint64_t x0 = static_cast<std::uint64_t>(origin.x);
    const std::uint64_t y0 = static_cast<std::uint64_t>(origin.y);

    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1 == n) ? 0 : i + 1;
        const Point2 p      = get(i);
        const Point2 q      = get(j);

        const std::uint64_t ax = static_cast<std::uint64_t>(p.x) - x0;
        const std::uint64_t ay = static_cast<std::uint64_t>(p.y) - y0;
        const std::uint64_t bx = static_cast<std::uint64_t>(q.x) - x0;
        const std::uint64_t by = static_cast<std::uint64_t>(q.y) - y0;

        acc += ax * by - bx * ay;
    }
    return acc;
}

std::uint64_t twice_area_u(std::span<const Mm> xs, std::span<const Mm> ys) noexcept
{
    return twice_area_acc([&](std::size_t i) { return Point2{xs[i], ys[i]}; }, xs.size());
}

std::uint64_t twice_area_u(std::span<const Point2> pts, std::size_t n) noexcept
{
    return twice_area_acc([&](std::size_t i) { return pts[i]; }, n);
}

/// |v| for a two's-complement doubled area, without the INT64_MIN negation UB.
constexpr std::uint64_t magnitude_u(std::uint64_t v) noexcept
{
    return (v >> 63) != 0 ? (~v + 1u) : v;
}

/// Halves twice-the-area, rounding half away from zero to match
/// `mm_from_metres`. Symmetric in sign, so a clockwise ring and its
/// counter-clockwise twin report the same magnitude to the square millimetre.
/// Computed unsigned: `twice + 1` on INT64_MAX and `-v` on INT64_MIN are both UB,
/// and this function is reachable from a hostile payload.
constexpr Mm2 halve(std::uint64_t twice) noexcept
{
    const bool negative     = (twice >> 63) != 0;
    const std::uint64_t mag = magnitude_u(twice);
    const std::uint64_t h   = (mag + 1u) / 2u; // mag <= 2^63, so this cannot wrap
    return negative ? -static_cast<std::int64_t>(h) : static_cast<std::int64_t>(h);
}

/// Segment length in millimetres, exact for every coordinate append() accepts.
///
/// The old form squared the deltas in signed int64 on the strength of a comment
/// asserting that "dx and dy stay under ~1e9 mm". Nothing enforced it, and the
/// file's own test declares a dilim-prefixed sağa değer of 3.05e10 mm supported:
/// one vertex that kept its dilim prefix while its neighbour lost it made
/// dx² + dy² overflow, which is UB, traps under the ASan/UBSan job, and — through
/// the old `v <= 0 return 0` guard — reported a 3100 km side as a length of ZERO,
/// silently shortening a çevre printed on a röper krokisi. The bound is now an
/// enforced invariant (kMmCoordinateLimit) and the arithmetic is 128-bit.
Mm segment_length(Mm ax, Mm ay, Mm bx, Mm by) noexcept
{
    const std::uint64_t dx = delta(ax, bx);
    const std::uint64_t dy = delta(ay, by);
    const U128 sum         = add_u128(mul_u64(dx, dx), mul_u64(dy, dy));
    return static_cast<Mm>(round_sqrt_u128(sum));
}

/// Bounding box of the first `n` points of a ring input.
Box2 input_bounds(std::span<const Point2> pts, std::size_t n) noexcept
{
    Box2 box{};
    for (std::size_t i = 0; i < n; ++i)
        box.extend(pts[i]);
    return box;
}

/// Inclusive containment. An empty box contains nothing and is contained by
/// nothing, so a degenerate ring cannot pass by accident.
constexpr bool box_contains(const Box2& outer, const Box2& inner) noexcept
{
    if (outer.empty() || inner.empty()) return false;
    return outer.min_x <= inner.min_x && outer.min_y <= inner.min_y && outer.max_x >= inner.max_x &&
           outer.max_y >= inner.max_y;
}

constexpr bool out_of_range(Point2 p) noexcept
{
    return p.x > kMmCoordinateLimit || p.x < -kMmCoordinateLimit || p.y > kMmCoordinateLimit ||
           p.y < -kMmCoordinateLimit;
}

} // namespace

Result<std::uint32_t> RingGeometry::append(std::span<const RingInput> rings)
{
    const auto ordinal = [](std::size_t i) { return std::to_string(i + 1) + ". halka"; };

    if (rings.empty())
        return err(ErrorCode::InvalidArgument, "Bir geometri en az bir halka ister, verilen: 0.");

    // Everything is validated before a single array is touched. A rejected
    // append must leave the store exactly as it was: a half-written parcel is the
    // failure mode Article 1.6 exists to prevent, and the caller's transaction
    // rolls back around this call, not inside it.
    std::size_t new_vertices = 0;
    std::uint16_t part       = rings[0].part;
    bool exterior_seen       = false;
    Box2 exterior_box{};

    for (std::size_t i = 0; i < rings.size(); ++i) {
        const RingInput& r = rings[i];

        if (r.role != RingRole::Open && r.role != RingRole::Exterior &&
            r.role != RingRole::Interior)
            return err(ErrorCode::InvalidArgument,
                       ordinal(i) + " tanımsız bir halka rolü taşıyor: " +
                           std::to_string(static_cast<unsigned>(r.role)) +
                           ". Geçerli roller: açık (0), dış (1), iç (2).");

        // A coordinate the store cannot compute over must never enter it. Both
        // limits are invariants the arithmetic above relies on, not style rules:
        // kMmInvalid is the "no value" sentinel and every delta taken against it
        // overflows, and kMmCoordinateLimit is what keeps dx² + dy² inside 128
        // bits and its square root inside Mm (see segment_length).
        for (std::size_t v = 0; v < r.points.size(); ++v) {
            const Point2 p   = r.points[v];
            const auto where = [&] {
                return ordinal(i) + ", " + std::to_string(v + 1) + ". tepe noktası";
            };
            if (p.x == kMmInvalid || p.y == kMmInvalid)
                return err(ErrorCode::ValidationFailed,
                           where() + " 'değer yok' işaretini koordinat olarak taşıyor. "
                                     "Kaynak veride okunamayan bir koordinat var.");
            if (out_of_range(p))
                return err(ErrorCode::ValidationFailed,
                           where() + " temsil edilebilir aralığın dışında: (" +
                               std::to_string(p.x) + ", " + std::to_string(p.y) + ") mm. Sınır ±" +
                               std::to_string(kMmCoordinateLimit) + " mm.");
        }

        // R11: part ascending, and therefore a part's rings are contiguous.
        if (r.part < part)
            return err(ErrorCode::ValidationFailed,
                       ordinal(i) + " parça numarası " + std::to_string(r.part) +
                           ", önceki halkanınki " + std::to_string(part) +
                           ". Halkalar parça numarasına göre artan sırada verilmeli.");
        if (r.part > part) {
            part          = r.part;
            exterior_seen = false;
        }

        const std::size_t stored = stored_count(r);
        const std::size_t needed = (r.role == RingRole::Open) ? 2u : 3u;
        if (stored < needed) {
            std::string msg = ordinal(i) + " " + role_name(r.role) + " halka en az " +
                              std::to_string(needed) +
                              " tepe noktası ister, verilen: " + std::to_string(stored);
            if (stored != r.points.size())
                msg += " (yinelenen kapanış noktası düşüldükten sonra; ham sayı " +
                       std::to_string(r.points.size()) + ")";
            return err(ErrorCode::ValidationFailed, msg + ".");
        }

        // R12: a closed ring whose shoelace is zero encloses nothing, and alan
        // hesabı is the legal output. Three collinear corners, a ring folded back
        // on itself and the symmetric bowtie — swapping two corners of a
        // rectangle, the commonest digitising blunder — all land here, and all of
        // them would otherwise be stored as a parcel of 0 m² that can be signed.
        //
        // This is a necessary condition, not a full simplicity test: an asymmetric
        // self-intersection still has a non-zero shoelace. The complete test needs
        // the Shewchuk predicate wrapper core.md R8 mandates; until it exists this
        // catches the degenerate cases, and it catches them at the boundary rather
        // than on a tapu.
        if (r.role != RingRole::Open && twice_area_u(r.points, stored) == 0)
            return err(ErrorCode::ValidationFailed,
                       ordinal(i) + " " + role_name(r.role) +
                           " halka sıfır alanlı: noktalar doğrusal, çakışık ya da halka "
                           "kendi üzerine katlanmış. Sıfır alanlı bir parsel imzalanamaz.");

        // R11: Exterior before its Interior rings, one face per part.
        switch (r.role) {
        case RingRole::Open: break;
        case RingRole::Exterior:
            if (exterior_seen)
                return err(ErrorCode::ValidationFailed,
                           ordinal(i) + " parça " + std::to_string(part) +
                               " içindeki ikinci dış halka. Ayrık yüzler ayrı parça "
                               "numarası ister.");
            exterior_seen = true;
            exterior_box  = input_bounds(r.points, stored);
            break;
        case RingRole::Interior: {
            if (!exterior_seen)
                return err(ErrorCode::ValidationFailed,
                           ordinal(i) + " bir iç halka (boşluk), ama parça " +
                               std::to_string(part) +
                               " içinde kendisinden önce gelen bir dış halka yok. Dış halka "
                               "kendi iç halkalarından önce verilmeli.");

            // R12: a hole is subtracted by its ROLE, so a hole that is not inside
            // its exterior subtracts more than there is and area_of reports a
            // NEGATIVE alan — the routine outcome of a bad yola terk or irtifak
            // import. A bounding-box containment test is the cheap necessary
            // condition; the exact point-in-ring test arrives with the predicate
            // wrapper (core.md R8).
            const Box2 hole = input_bounds(r.points, stored);
            if (!box_contains(exterior_box, hole))
                return err(ErrorCode::ValidationFailed,
                           ordinal(i) + " iç halkası parça " + std::to_string(part) +
                               " dış halkasının dışına taşıyor. Boşluk, ait olduğu yüzün "
                               "içinde olmalı; taşan bir boşluk eksi alan üretir.");
            break;
        }
        }

        new_vertices += stored;
    }

    constexpr std::size_t kIndexLimit = std::numeric_limits<std::uint32_t>::max();
    if (xs.size() + new_vertices > kIndexLimit)
        return err(ErrorCode::ValidationFailed,
                   "Geometri deposu 4294967295 tepe noktası sınırını aşıyor; belgeyi bölün.");
    if (ring_start.size() + rings.size() > kIndexLimit)
        return err(ErrorCode::ValidationFailed,
                   "Geometri deposu 4294967295 halka sınırını aşıyor; belgeyi bölün.");

    reserve_vertices(new_vertices);
    reserve_for(ring_start, rings.size());
    reserve_for(ring_count, rings.size());
    reserve_for(ring_part, rings.size());
    reserve_for(ring_role, rings.size());
    reserve_for(first_ring, 1);
    reserve_for(ring_total, 1);

    const auto first = static_cast<std::uint32_t>(ring_start.size());

    for (const RingInput& r : rings) {
        const std::size_t stored = stored_count(r);

        ring_start.push_back(static_cast<std::uint32_t>(xs.size()));
        ring_count.push_back(static_cast<std::uint32_t>(stored));
        ring_part.push_back(r.part);
        ring_role.push_back(r.role);

        for (std::size_t i = 0; i < stored; ++i) {
            xs.push_back(r.points[i].x);
            ys.push_back(r.points[i].y);
        }
    }

    const auto slot = static_cast<std::uint32_t>(first_ring.size());
    first_ring.push_back(first);
    ring_total.push_back(static_cast<std::uint32_t>(rings.size()));
    return slot;
}

Box2 RingGeometry::bounds_of(std::uint32_t slot) const
{
    const RingSpan span = rings_of(slot);
    Box2 box{};

    for (std::uint32_t k = 0; k < span.count; ++k) {
        const std::uint32_t r = span.first + k;
        const auto rx         = ring_xs(r);
        const auto ry         = ring_ys(r);
        for (std::size_t i = 0; i < rx.size(); ++i)
            box.extend(Point2{rx[i], ry[i]});
    }
    return box;
}

Mm2 RingGeometry::ring_area(std::uint32_t ring) const
{
    // R10: an Open ring is a polyline; its first and last vertex are NOT joined,
    // so it has no area. Reporting the shoelace of its implied closure gave a
    // non-zero figure for a shape that encloses nothing, and made the two public
    // area functions disagree about the same ring — a footgun on a number that
    // ends up on a legal document (§12).
    if (ring_role[ring] == RingRole::Open) return 0;

    // The WINDING sign is kept: this is the ring's own signed shoelace,
    // counter-clockwise positive. `area_of` applies the role signs (R12).
    return halve(twice_area_u(ring_xs(ring), ring_ys(ring)));
}

Mm2 RingGeometry::area_of(std::uint32_t slot) const
{
    const RingSpan span = rings_of(slot);
    std::uint64_t twice = 0;

    for (std::uint32_t k = 0; k < span.count; ++k) {
        const std::uint32_t r = span.first + k;
        if (ring_role[r] == RingRole::Open) continue;

        // The sign comes from the ROLE, never from the winding. A hole digitised
        // counter-clockwise must still be subtracted: imported data is wound
        // whichever way the source system felt like, and DXF, GeoJSON and TKGM
        // exports disagree with each other.
        //
        // Accumulated unsigned for the same reason twice_area_acc is: the running
        // sum is the only remaining place in the area path where a signed
        // intermediate could overflow, and signed overflow is UB the optimiser is
        // entitled to assume away.
        const std::uint64_t area = magnitude_u(twice_area_u(ring_xs(r), ring_ys(r)));
        if (ring_role[r] == RingRole::Interior)
            twice -= area;
        else
            twice += area;
    }

    // Halved once, over the whole slot, so exterior-minus-hole is exact rather
    // than the sum of two independently rounded figures.
    return halve(twice);
}

Mm RingGeometry::perimeter_of(std::uint32_t slot) const
{
    const RingSpan span = rings_of(slot);
    Mm total            = 0;

    for (std::uint32_t k = 0; k < span.count; ++k) {
        const std::uint32_t r = span.first + k;
        const auto rx         = ring_xs(r);
        const auto ry         = ring_ys(r);
        const std::size_t n   = rx.size();

        // Each side is rounded to the millimetre before it is added, because a
        // kenar uzunluğu is itself a reported figure: the perimeter on the
        // document must equal the sum of the side lengths printed beside it.
        for (std::size_t i = 0; i + 1 < n; ++i)
            total += segment_length(rx[i], ry[i], rx[i + 1], ry[i + 1]);

        // The closing vertex is not stored (see stored_count), so the closing
        // segment of a closed ring is added here.
        if (ring_role[r] != RingRole::Open && n >= 3)
            total += segment_length(rx[n - 1], ry[n - 1], rx[0], ry[0]);
    }
    return total;
}

void RingGeometry::clear()
{
    xs.clear();
    ys.clear();
    ring_start.clear();
    ring_count.clear();
    ring_part.clear();
    ring_role.clear();
    first_ring.clear();
    ring_total.clear();
}

void RingGeometry::reserve_vertices(std::size_t extra)
{
    reserve_for(xs, extra);
    reserve_for(ys, extra);
}

} // namespace piricad::core
