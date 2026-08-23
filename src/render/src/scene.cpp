// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/render/scene.hpp"

#include "piricad/core/spatial_index.hpp"

#include <algorithm>
#include <cmath>

namespace piricad::render {
namespace {

bool box_contains(const Box2& outer, const Box2& inner)
{
    if (outer.empty() || inner.empty()) return false;
    return outer.min_x <= inner.min_x && outer.min_y <= inner.min_y && outer.max_x >= inner.max_x &&
           outer.max_y >= inner.max_y;
}

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
    const auto& styles = doc.styles();
    const auto& texts  = doc.texts();

    // Batching is by STYLE, not by layer. Before this the scene builder read
    // `layers[i].appearance` and ignored `entities.style[e]` entirely, so the
    // style column — the whole point of resolving symbology at commit time
    // (model.md R14) — reached the screen nowhere. A per-entity override, an
    // MPYY `gösterim` and a categorized renderer all write that column, and all
    // three were invisible.
    //
    // One batch per style id, plus one per layer for entities that carry the
    // ByLayer sentinel. Style ids are dense and small: a cadastral sheet has
    // thousands of parcels and tens of styles.
    const std::size_t batches = styles.size() + layers.size();
    if (out.polylines.size() < batches) out.polylines.resize(batches);
    if (out.polygons.size() < batches) out.polygons.resize(batches);

    // Layer batches occupy the tail, so a style id indexes itself directly.
    const auto layer_batch = [&](core::LayerId l) { return styles.size() + l; };

    for (std::size_t i = 0; i < styles.size(); ++i) {
        // Paper micrometres to screen pixels. The document stores the plot width
        // MPYY prescribes; the pixel figure is derived per frame and never stored
        // (model.md R20).
        const core::Appearance& look = styles.entries()[i];
        out.polylines[i].rgba        = look.rgba;
        out.polylines[i].width_px    = std::max(1.0f, static_cast<float>(look.width_um) / 1000.0f);
        out.polygons[i].rgba         = look.fill_rgba;
        out.polygons[i].hatch        = look.hatch;
    }
    for (core::LayerId l = 0; l < layers.size(); ++l) {
        const core::Appearance& look = layers[l].appearance;
        const std::size_t b          = layer_batch(l);
        out.polylines[b].rgba        = look.rgba;
        out.polylines[b].width_px    = std::max(1.0f, static_cast<float>(look.width_um) / 1000.0f);
        out.polygons[b].rgba         = look.fill_rgba;
        out.polygons[b].hatch        = look.hatch;
    }

    // One denominator for the whole frame: the view does not change mid-build.
    const double denominator = view.scale_denominator();

    const Box2 visible   = view.visible_box();
    const auto& entities = doc.entities();
    const auto& geometry = doc.geometry();

    // A vertex closer than this to its predecessor cannot be told apart on screen.
    // Precomputed LOD tiles replace this per-frame filter in Phase 1 (§10.3).
    const double lod_mm = options.lod ? options.lod_pixels * view.mm_per_pixel() : 0.0;

    const auto emit_ring = [&](PolylineBatch& batch, std::uint32_t ring) {
        const auto xs = geometry.ring_xs(ring);
        const auto ys = geometry.ring_ys(ring);
        if (xs.size() < 2) return;

        const bool closed       = geometry.ring_role[ring] != core::RingRole::Open;
        const std::size_t first = batch.xs.size();

        const auto push = [&](std::size_t v) {
            // Offset against the view centre BEFORE narrowing to float. Writing an
            // absolute TUREF/TM3 coordinate into a float here shimmers by metres
            // on screen (§10.3).
            batch.xs.push_back(view.offset_x_f(xs[v]));
            batch.ys.push_back(view.offset_y_f(ys[v]));
        };

        for (std::size_t v = 0; v < xs.size(); ++v) {
            const bool endpoint = (v == 0 || v + 1 == xs.size());
            if (!endpoint && lod_mm > 0.0 && batch.xs.size() > first) {
                const double px = static_cast<double>(xs[v] - xs[v - 1]);
                const double py = static_cast<double>(ys[v] - ys[v - 1]);
                if (std::sqrt(px * px + py * py) < lod_mm) continue;
            }
            push(v);
        }

        // A closed ring is stored without its duplicated closing vertex, so the
        // segment back to the start is added here rather than kept in the data.
        if (closed && batch.xs.size() - first >= 3) push(0);

        const auto emitted = static_cast<std::uint32_t>(batch.xs.size() - first);
        if (emitted < 2) {
            batch.xs.resize(first);
            batch.ys.resize(first);
            return;
        }

        batch.runs.push_back(emitted);
        out.vertex_count += emitted;
    };

    const auto emit_fill_ring = [&](PolygonBatch& batch, std::uint32_t ring) {
        const auto xs = geometry.ring_xs(ring);
        const auto ys = geometry.ring_ys(ring);
        if (xs.size() < 3) return;

        for (std::size_t v = 0; v < xs.size(); ++v) {
            // Same origin offset as the stroke path, and for the same reason: an
            // absolute TUREF coordinate narrowed to float shimmers by metres.
            batch.xs.push_back(view.offset_x_f(xs[v]));
            batch.ys.push_back(view.offset_y_f(ys[v]));
        }

        // No LOD filtering on a fill. Dropping a vertex from a stroke shortens a
        // line nobody can measure at that zoom; dropping one from a face changes
        // the shape being coloured, and a plan lekesi that leaks over its boundary
        // is a wrong drawing rather than a coarse one.
        batch.runs.push_back(static_cast<std::uint32_t>(xs.size()));
        batch.is_hole.push_back(geometry.ring_role[ring] == core::RingRole::Interior ? 1u : 0u);
        ++out.fill_count;
    };

    const auto emit = [&](core::EntityId e) {
        // The cull test reads the flags byte and the four bbox arrays, and nothing
        // else. Layer visibility is mirrored into the byte (model.md R6, R7).
        if (!entities.visible(e)) return;

        if (options.cull && !boxes_overlap(visible, entities.box_of(e))) {
            ++out.culled_count;
            return;
        }

        const core::LayerId lid = entities.layer[e];
        if (lid >= layers.size()) return;

        // The style column decides, and falls back to the layer only when it
        // carries the ByLayer sentinel. This is the one lookup the frame path
        // does, and it is an array index — never a rule, an expression or a
        // cascade (R14, P29).
        const core::StyleId sid = entities.style[e];
        const std::size_t slot =
            sid == core::kByLayerStyle || sid >= styles.size() ? layer_batch(lid) : sid;

        // Scale-dependent visibility. Not decoration in planning work: a
        // `çevre düzeni planı` at 1/100000 shows a `lekesi` where the `uygulama imar planı`
        // at 1/1000 shows its parcels, and drawing both at both scales produces a
        // sheet nobody can read. The window is stored on the symbol, so this is
        // an array lookup and a comparison — no rule is evaluated (R14).
        if (sid != core::kByLayerStyle && sid < styles.size()) {
            const core::Symbol& sym = styles.symbol_at(sid);
            if (sym.min_scale != 0 && denominator < static_cast<double>(sym.min_scale)) {
                ++out.culled_count;
                return;
            }
            if (sym.max_scale != 0 && denominator > static_cast<double>(sym.max_scale)) {
                ++out.culled_count;
                return;
            }
        }

        PolylineBatch& batch      = out.polylines[slot];
        PolygonBatch& fill        = out.polygons[slot];
        const core::RingSpan span = geometry.rings_of(entities.slot[e]);

        // A text entity is its baseline plus a string. The baseline is an ordinary
        // open ring, so it is culled, snapped and hit-tested by the same code every
        // other entity uses; only the drawing differs.
        if (texts.has(entities.slot[e]) && span.count > 0) {
            const auto xs = geometry.ring_xs(span.first);
            const auto ys = geometry.ring_ys(span.first);
            if (xs.size() >= 2) {
                TextItem item;
                item.rgba = out.polylines[slot].rgba;
                item.x0   = view.offset_x_f(xs.front());
                item.y0   = view.offset_y_f(ys.front());
                item.x1   = view.offset_x_f(xs.back());
                item.y1   = view.offset_y_f(ys.back());

                // Ground millimetres to pixels, like every other length on screen.
                // A height in paper units would change what the drawing SAYS when
                // the plot scale changes, and on a pafta the height of a parcel
                // number is part of the drawing (R20).
                item.height_px = static_cast<float>(
                    static_cast<double>(texts.height(entities.slot[e])) / view.mm_per_pixel());
                item.anchor = static_cast<std::uint8_t>(texts.anchor(entities.slot[e]));
                item.text.assign(texts.text(entities.slot[e]));

                out.texts.push_back(std::move(item));
                ++out.text_count;
                ++out.entity_count;
                return; // the baseline itself is construction, not ink
            }
        }

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            emit_ring(batch, r);

            // A closed ring with a fill colour is also a face. Emitted into its
            // own batch with the hole flag kept, because the backend has to punch
            // the holes out and a stroke batch has nowhere to say so.
            if (fill.rgba != 0 && geometry.ring_role[r] != core::RingRole::Open)
                emit_fill_ring(fill, r);
        }

        ++out.entity_count;
    };

    if (!options.cull) {
        for (core::EntityId e = 0; e < entities.size(); ++e)
            emit(e);
        return;
    }

    // The index narrows five million parcels to the handful sharing a leaf with
    // the viewport; entities added since it was last packed are a short tail that
    // is cheaper to scan than to repack (piricad.md §10.5).
    const core::SpatialIndex& index = doc.spatial_index();

    // Zoomed far enough out that everything is on screen: the tree can only answer
    // "all of them", and a sequential scan beats collecting five million
    // candidates to say so. LOD tiles take this case over in Phase 1 (§10.3).
    if (index.empty() || box_contains(visible, index.bounds())) {
        for (core::EntityId e = 0; e < entities.size(); ++e)
            emit(e);
        out.tail_count = entities.size();
        return;
    }

    index.query(visible, out.candidates);
    out.indexed_count = out.candidates.size();
    for (core::EntityId e : out.candidates)
        emit(e);

    const core::EntityId tail = doc.indexed_upto();
    out.tail_count            = entities.size() - tail;
    for (core::EntityId e = tail; e < entities.size(); ++e)
        emit(e);
}

} // namespace piricad::render
