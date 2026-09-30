// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a polyline whose edges may be arcs.
//
// What a DXF `LWPOLYLINE` with bulges is, and what a kerb line, a road edge and
// a building with a rounded corner are in a Turkish cadastral drawing: straight
// edges and arc edges in one closed or open run. The VERTICES are the ring, so
// the arena bounds, culls and indexes it like any polyline; which edges bend,
// and about what, is the kind's payload (model.md R9a).
//
// EACH ARC IS ITS DEFINITION — centre, exact radius, direction — never a bulge
// number and never its picture. The bulge is a DXF encoding, converted on the way
// in and out (`arc_from_bulge`, `bulge_from_arc`); the picture is drawn by
// `arc_outline`, the same deterministic routine YAY uses (core/arc.hpp).
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// The payload of a `core.arc_polyline` slot.
struct ArcPolyline
{
    /// One bent edge.
    struct Arc
    {
        std::uint32_t segment{0}; ///< the edge it bends: from vertex `segment` to the next
        Point2 centre{};          ///< the arc's centre
        Mm radius{0};             ///< the arc's radius, exact and positive
        bool ccw{true};           ///< counter-clockwise from the edge's start to its end

        /// Member-wise equality.
        friend constexpr bool operator==(const Arc&, const Arc&) noexcept = default;
    };

    Mm constant_width{0};  ///< a DXF constant width, kept for the round trip; 0 = none
    std::vector<Arc> arcs; ///< ascending by `segment`, at most one per edge

    friend bool operator==(const ArcPolyline&, const ArcPolyline&) = default;
};

/// The payload layout version `encode_arc_polyline` writes.
inline constexpr std::uint16_t kArcPolylineLayout = 1;

/// The payload bytes: the R9a header, the width, then the arcs.
std::vector<std::uint8_t> encode_arc_polyline(const ArcPolyline& def);

/// The payload back, refused when the bytes are not what `encode_arc_polyline`
/// writes, an arc has no radius, or the arcs are not ascending by edge.
Result<ArcPolyline> decode_arc_polyline(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<ArcPolyline> arc_polyline_of(const RingGeometry& geom, std::uint32_t slot);

/// Appends the DRAWN form: the vertices in order, every bent edge replaced by
/// its arc's points (`arc_outline`, ends excluded because they are the vertices).
/// `closed` says the last edge runs back to the first vertex. Nothing but the
/// arc routine's arithmetic touches it (§7.3).
void arc_polyline_points(std::span<const Point2> vertices, bool closed, const ArcPolyline& def,
                         std::vector<Mm>& xs, std::vector<Mm>& ys);

/// The drawn form of the slot, and whether it closes.
bool arc_polyline_outline(const RingGeometry& geom, std::uint32_t slot, std::vector<Mm>& xs,
                          std::vector<Mm>& ys);

} // namespace kentos::core
