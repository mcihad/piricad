// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/scene.hpp"

#include <cmath>

namespace piricad::render {
namespace {

bool boxes_overlap(const Box2& a, const Box2& b)
{
    if (a.empty() || b.empty()) return false;
    return !(a.max_x < b.min_x || a.min_x > b.max_x || a.max_y < b.min_y || a.min_y > b.max_y);
}

} // namespace

void build_scene(const core::Document& doc, const ViewTransform& view, const SceneOptions& options,
                 DrawList& out)
{
    out.clear();

    const auto& layers = doc.layers();
    if (out.polylines.size() < layers.size()) out.polylines.resize(layers.size());

    for (std::size_t i = 0; i < layers.size(); ++i) {
        out.polylines[i].rgba     = layers[i].style.rgba;
        out.polylines[i].width_px = layers[i].style.width_px;
    }

    const Box2 visible = view.visible_box();
    const auto& poly   = doc.polylines();

    // A vertex closer than this to its predecessor cannot be told apart on screen.
    // Precomputed LOD tiles replace this per-frame filter in Phase 1 (§10.3).
    const double lod_mm = options.lod ? options.lod_pixels * view.mm_per_pixel() : 0.0;

    for (core::EntityId e = 0; e < poly.size(); ++e) {
        if (!poly.alive[e]) continue;

        const core::LayerId lid = poly.layer[e];
        if (lid >= layers.size() || !layers[lid].visible) continue;

        if (options.cull && !boxes_overlap(visible, doc.entity_extent(e))) {
            ++out.culled_count;
            continue;
        }

        const auto xs = poly.xs_of(e);
        const auto ys = poly.ys_of(e);
        if (xs.size() < 2) continue;

        PolylineBatch& batch    = out.polylines[lid];
        const std::size_t first = batch.xs.size();

        for (std::size_t v = 0; v < xs.size(); ++v) {
            const bool endpoint = (v == 0 || v + 1 == xs.size());
            if (!endpoint && lod_mm > 0.0 && batch.xs.size() > first) {
                const double px = static_cast<double>(xs[v] - xs[v - 1]);
                const double py = static_cast<double>(ys[v] - ys[v - 1]);
                if (std::sqrt(px * px + py * py) < lod_mm) continue;
            }
            // Offset against the view centre BEFORE narrowing to float. Writing an
            // absolute TUREF/TM3 coordinate into a float here would shimmer by
            // metres on screen (§10.3).
            batch.xs.push_back(view.offset_x_f(xs[v]));
            batch.ys.push_back(view.offset_y_f(ys[v]));
        }

        const auto emitted = static_cast<std::uint32_t>(batch.xs.size() - first);
        if (emitted < 2) {
            batch.xs.resize(first);
            batch.ys.resize(first);
            continue;
        }

        batch.runs.push_back(emitted);
        out.vertex_count += emitted;
        ++out.entity_count;
    }
}

} // namespace piricad::render
