// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/drawlist.hpp"

namespace piricad::render {

EdgeStamps distribute_along(double length, double interval, double margin) noexcept
{
    if (length <= 0.0 || interval <= 0.0 || margin < 0.0) return {};
    if (length < 2.0 * margin) return {};

    const double usable = length - 2.0 * margin;

    // At least one, always: an edge long enough to hold a stamp gets a stamp, and
    // rounding a short-but-adequate edge down to zero would leave a corner bare.
    const auto count = static_cast<int>(usable / interval + 0.5) + 1;

    EdgeStamps out;
    out.count = count < 1 ? 1 : count;
    out.first = margin;
    out.step  = out.count > 1 ? usable / (out.count - 1) : 0.0;
    return out;
}

void Overlay::clear()
{
    // Same contract as DrawList::clear(): the sizes go, the capacity stays. The
    // overlay is rebuilt on every mouse move, so a frame that allocates here
    // allocates on every mouse move.
    for (auto& batch : batches) {
        batch.xs.clear();
        batch.ys.clear();
        batch.runs.clear();
        batch.closed.clear();
    }
    labels.clear();
}

void DrawList::clear()
{
    // Buffers are kept, only their sizes reset: the draw loop must not allocate
    // per frame (.claude/render.md).
    for (auto& batch : polylines) {
        batch.xs.clear();
        batch.ys.clear();
        batch.runs.clear();
    }
    for (auto& batch : polygons) {
        batch.xs.clear();
        batch.ys.clear();
        batch.runs.clear();
        batch.is_hole.clear();
    }
    passes.clear();
    order.clear();
    texts.clear();
    candidates.clear();
    has_preview   = false;
    fill_count    = 0;
    text_count    = 0;
    vertex_count  = 0;
    entity_count  = 0;
    culled_count  = 0;
    indexed_count = 0;
    tail_count    = 0;
}

} // namespace piricad::render
