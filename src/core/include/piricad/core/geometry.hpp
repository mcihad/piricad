// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: ring geometry.
//
// .claude/model.md R9–R12. A cadastral parcel is a ring, may have interior
// rings, and may be multipart. `(start, count)` — a single open vertex run —
// cannot express a parcel with a hole, and yola terk and irtifak routinely
// produce one. Alan hesabı over such a parcel is the legal output (§12).
//
// Structure-of-arrays throughout, three levels:
//
//   entity (slot)  ->  rings  ->  vertices
//
// A polyline is one Open ring. A parcel is one Exterior ring. A parcel with an
// exclusion is Exterior + Interior in the same part. A multipart parcel uses
// distinct part numbers. Ring order is part ascending, Exterior before its
// Interior rings (R11) — that ordering is part of the content hash.
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace piricad::core {

/// Square millimetres. A 100 km x 100 km area is 1e16 mm², well inside int64.
using Mm2 = std::int64_t;

enum class RingRole : std::uint8_t {
    Open = 0,      ///< a polyline: first and last vertex are not joined
    Exterior = 1,  ///< the outer boundary of a face
    Interior = 2,  ///< a hole inside the exterior of the same part
};

/// One geometry slot's ring range, as returned by the store.
struct RingSpan {
    std::uint32_t first{0};
    std::uint32_t count{0};
};

/// Ring-structured geometry for one entity kind. Indexed by SLOT, never by key.
class RingGeometry {
public:
    // ---- vertices: the hot block, never read by attribute code ----
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    // ---- rings ----
    std::vector<std::uint32_t> ring_start;  ///< first vertex of the ring
    std::vector<std::uint32_t> ring_count;  ///< vertex count of the ring
    std::vector<std::uint16_t> ring_part;   ///< multipart grouping
    std::vector<RingRole>      ring_role;

    // ---- slot -> rings ----
    std::vector<std::uint32_t> first_ring;
    std::vector<std::uint32_t> ring_total;

    std::size_t slot_count() const noexcept { return first_ring.size(); }
    std::size_t ring_count_total() const noexcept { return ring_start.size(); }
    std::size_t vertex_count() const noexcept { return xs.size(); }

    RingSpan rings_of(std::uint32_t slot) const noexcept
    {
        return RingSpan{first_ring[slot], ring_total[slot]};
    }

    std::span<const Mm> ring_xs(std::uint32_t ring) const
    {
        return {xs.data() + ring_start[ring], ring_count[ring]};
    }

    std::span<const Mm> ring_ys(std::uint32_t ring) const
    {
        return {ys.data() + ring_start[ring], ring_count[ring]};
    }

    Point2 vertex(std::uint32_t ring, std::uint32_t index) const
    {
        const std::uint32_t k = ring_start[ring] + index;
        return Point2{xs[k], ys[k]};
    }

    /// One ring of a new slot. Rings MUST be appended in R11 order.
    struct RingInput {
        std::span<const Point2> points;
        RingRole                role{RingRole::Open};
        std::uint16_t           part{0};
    };

    /// Appends a slot built from `rings` and returns its index. Validates R11
    /// ordering, minimum vertex counts and ring closure.
    Result<std::uint32_t> append(std::span<const RingInput> rings);

    /// Bounding box over every ring of the slot.
    Box2 bounds_of(std::uint32_t slot) const;

    /// Signed area of one ring by the shoelace formula, in square millimetres.
    /// Coordinates are translated to the ring's first vertex before multiplying:
    /// a raw shoelace on 1e9-magnitude TM3 coordinates overflows int64, and a
    /// wrong area is a wrong legal document (§12).
    Mm2 ring_area(std::uint32_t ring) const;

    /// Net area of a slot: exterior rings positive, interior rings subtracted.
    /// Open rings contribute nothing. This is alan hesabı.
    Mm2 area_of(std::uint32_t slot) const;

    /// Total length of every ring in the slot, in millimetres. Closed rings
    /// include the closing segment.
    Mm perimeter_of(std::uint32_t slot) const;

    void clear();

private:
    void reserve_vertices(std::size_t extra);
};

/// Square millimetres to square metres, for display only. Never a stored value.
constexpr double mm2_to_m2(Mm2 v) noexcept
{
    return static_cast<double>(v) / (static_cast<double>(kMmPerMetre) *
                                     static_cast<double>(kMmPerMetre));
}

} // namespace piricad::core
