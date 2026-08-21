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
// vertex, and exactly one duplicate is dropped when the caller supplies it.
//
// DXF LWPOLYLINE (flag 70 bit 0) and GeoJSON both repeat the first vertex at the
// end. Storing it would put a zero-length segment inside every imported parcel
// and, worse, make two geometrically identical parcels differ in vertex count —
// so they would differ in the content hash and in every golden fixture,
// depending only on which importer produced them. `ring_count` is therefore the
// number of distinct corners, and the closing segment is implied by the role.
//
// Only one duplicate is dropped: a caller that repeats a vertex twice has a data
// defect, and silently erasing it hides the defect instead of reporting it.
// An Open ring is never trimmed — first == last on a polyline is the caller's
// geometry, not a closure convention.
std::size_t stored_count(const RingGeometry::RingInput& r) noexcept
{
    const std::size_t n = r.points.size();
    if (r.role == RingRole::Open) return n;
    if (n >= 2 && r.points.front() == r.points.back()) return n - 1;
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

/// Twice the signed area of a ring, counter-clockwise positive.
///
/// Doubled, because the exact signed area of an integer ring is a half-integer
/// and halving once at the end of `area_of` keeps a parcel-minus-hole result
/// exact instead of rounding each ring on its own.
std::int64_t twice_area(std::span<const Mm> xs, std::span<const Mm> ys) noexcept
{
    const std::size_t n = xs.size();
    if (n < 3) return 0;

    // Translating to the first vertex is not an optimisation. In TUREF/TM3 the
    // sağa değer routinely carries the dilim number in front — 30 485 320 m, i.e.
    // 3.05e10 mm — and one raw x*y term against a 4.3e9 mm yukarı değer is 1.3e20,
    // fourteen times past int64's 9.2e18 ceiling. Even inside one dilim, summing
    // the two halves of the shoelace separately passes it after four vertices.
    // Signed overflow is undefined behaviour: it traps under the UBSan job and the
    // optimiser is entitled to assume it cannot happen, so "two's complement
    // happens to wrap back to the right answer" is not a defence for a figure that
    // is signed by an engineer (§12).
    //
    // Translated, the deltas are parcel-sized (~1e5 mm) and the products ~1e10, so
    // every intermediate is exactly representable. The shoelace sum is invariant
    // under translation, so nothing is approximated: the result is exact integer
    // square millimetres.
    const std::uint64_t x0 = static_cast<std::uint64_t>(xs[0]);
    const std::uint64_t y0 = static_cast<std::uint64_t>(ys[0]);

    // Accumulated as unsigned: two's-complement wraparound is defined behaviour,
    // signed overflow is not, and UBSan runs over this suite. For every ring
    // whose true doubled area fits in int64 the bit pattern is the exact value.
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1 == n) ? 0 : i + 1;

        const std::uint64_t ax = static_cast<std::uint64_t>(xs[i]) - x0;
        const std::uint64_t ay = static_cast<std::uint64_t>(ys[i]) - y0;
        const std::uint64_t bx = static_cast<std::uint64_t>(xs[j]) - x0;
        const std::uint64_t by = static_cast<std::uint64_t>(ys[j]) - y0;

        acc += ax * by - bx * ay;
    }
    return static_cast<std::int64_t>(acc);
}

/// Halves twice-the-area, rounding half away from zero to match
/// `mm_from_metres`. Symmetric in sign, so a clockwise ring and its
/// counter-clockwise twin report the same magnitude to the square millimetre.
constexpr Mm2 halve(std::int64_t twice) noexcept
{
    return twice >= 0 ? (twice + 1) / 2 : (twice - 1) / 2;
}

constexpr std::int64_t magnitude(std::int64_t v) noexcept
{
    return v < 0 ? -v : v;
}

/// Integer square root rounded to the nearest millimetre, digit by digit.
///
/// No floating point anywhere: a kenar uzunluğu ends up printed on a röper
/// krokisi, and seeding this from `std::sqrt` would make the answer depend on
/// the platform's rounding of the seed (§7.3).
Mm round_sqrt(std::int64_t v) noexcept
{
    if (v <= 0) return 0;

    std::uint64_t rest = static_cast<std::uint64_t>(v);
    std::uint64_t root = 0;
    std::uint64_t bit  = std::uint64_t{1} << 62;

    while (bit > rest)
        bit >>= 2;
    while (bit != 0) {
        if (rest >= root + bit) {
            rest -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    // root is floor(sqrt(v)); (root + 0.5)² = root² + root + 0.25, so the value
    // rounds up exactly when the remainder passes root.
    const auto r = static_cast<std::int64_t>(root);
    return (v - r * r > r) ? r + 1 : r;
}

/// Segment length in millimetres. dx and dy stay under ~1e9 mm (1000 km) for any
/// ring inside a projection zone, so dx² + dy² stays well inside int64.
Mm segment_length(Mm ax, Mm ay, Mm bx, Mm by) noexcept
{
    const std::int64_t dx = ax - bx;
    const std::int64_t dy = ay - by;
    return round_sqrt(dx * dx + dy * dy);
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

    for (std::size_t i = 0; i < rings.size(); ++i) {
        const RingInput& r = rings[i];

        if (r.role != RingRole::Open && r.role != RingRole::Exterior &&
            r.role != RingRole::Interior)
            return err(ErrorCode::InvalidArgument,
                       ordinal(i) + " tanımsız bir halka rolü taşıyor: " +
                           std::to_string(static_cast<unsigned>(r.role)) +
                           ". Geçerli roller: açık (0), dış (1), iç (2).");

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
            break;
        case RingRole::Interior:
            if (!exterior_seen)
                return err(ErrorCode::ValidationFailed,
                           ordinal(i) + " bir iç halka (boşluk), ama parça " +
                               std::to_string(part) +
                               " içinde kendisinden önce gelen bir dış halka yok. Dış halka "
                               "kendi iç halkalarından önce verilmeli.");
            break;
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
    // The role is deliberately not consulted: this is the ring's own signed
    // shoelace area, counter-clockwise positive. `area_of` applies the role
    // signs (R12).
    return halve(twice_area(ring_xs(ring), ring_ys(ring)));
}

Mm2 RingGeometry::area_of(std::uint32_t slot) const
{
    const RingSpan span = rings_of(slot);
    std::int64_t twice  = 0;

    for (std::uint32_t k = 0; k < span.count; ++k) {
        const std::uint32_t r = span.first + k;
        if (ring_role[r] == RingRole::Open) continue;

        // The sign comes from the ROLE, never from the winding. A hole digitised
        // counter-clockwise must still be subtracted: imported data is wound
        // whichever way the source system felt like, and DXF, GeoJSON and TKGM
        // exports disagree with each other.
        const std::int64_t area = magnitude(twice_area(ring_xs(r), ring_ys(r)));
        twice += (ring_role[r] == RingRole::Interior) ? -area : area;
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
