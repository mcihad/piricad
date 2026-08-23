// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/drawlist.hpp"

namespace piricad::render {

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
