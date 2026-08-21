// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — render: the backend-independent draw list.
//
// The scene builder produces this; a backend consumes it. Swapping the QPainter
// backend for the QRhi backend must not touch anything above this header.
#pragma once

#include "piricad/core/document.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace piricad::render {

struct PolylineBatch
{
    std::uint32_t rgba{0xFFFFFFFFu};
    float width_px{1.0f};
    /// Screen-space vertices, already offset-corrected (see view.hpp).
    std::vector<float> xs;
    std::vector<float> ys;
    /// Vertex count of each polyline in the batch; sums to xs.size().
    std::vector<std::uint32_t> runs;
};

/// Filled faces. Separate from PolylineBatch because a fill is not a wide line:
/// it needs the ring structure kept (an exterior followed by its holes) so the
/// backend can punch the holes out, and a stroke does not care.
struct PolygonBatch
{
    std::uint32_t rgba{0};  ///< 0 = nothing to fill
    std::uint16_t hatch{0}; ///< index into the hatch table, from /data
    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<std::uint32_t> runs;   ///< vertex count of each ring
    std::vector<std::uint8_t> is_hole; ///< parallel to runs
};

struct DrawList
{
    std::vector<PolylineBatch> polylines;

    /// One entry per style id that has something to fill, in id order.
    std::vector<PolygonBatch> polygons;

    /// Rubber band from a running interactive command, if any.
    bool has_preview{false};
    float preview_x0{0.0f}, preview_y0{0.0f};
    float preview_x1{0.0f}, preview_y1{0.0f};

    /// Scratch buffer for the spatial index query. Lives here so its capacity
    /// survives between frames and the draw loop allocates nothing (§10.4).
    std::vector<core::EntityId> candidates;

    std::size_t fill_count{0}; ///< filled rings emitted

    std::size_t vertex_count{0};
    std::size_t entity_count{0};
    std::size_t culled_count{0};
    std::size_t indexed_count{0}; ///< candidates the index returned
    std::size_t tail_count{0};    ///< entities scanned outside the index

    void clear();
};

} // namespace piricad::render
