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

/// One paper millimetre is one screen pixel.
///
/// A PLACEHOLDER, and it is the same one the previous code carried unnamed as
/// `width_um / 1000.0`. What it stands in for is the plot scale: a symbol declared
/// in paper units is 0,5 mm on the sheet whatever the drawing scale, and turning
/// that into pixels needs the sheet's own resolution, which arrives with the
/// layout and plotting work. Naming it is what makes it findable when that lands.
constexpr double kPixelsPerPaperMm = 1.0;

/// A measure in this frame's pixels.
///
/// Three units, three conversions, and the difference is visible on screen: a
/// paper size holds still while the user zooms, a ground size grows with the
/// drawing, a pixel size is already what it is.
float to_pixels(const core::Measure& m, double mm_per_pixel)
{
    switch (m.unit) {
    case core::Unit::Paper:
        return static_cast<float>(static_cast<double>(m.value) / 1000.0 * kPixelsPerPaperMm);
    case core::Unit::Ground:
        return mm_per_pixel > 0.0 ? static_cast<float>(static_cast<double>(m.value) / mm_per_pixel)
                                  : 0.0f;
    case core::Unit::Pixel: return static_cast<float>(m.value);
    }
    return 0.0f;
}

} // namespace

float stroke_width_px(const core::SymbolLayer& layer)
{
    return std::max(1.0f, static_cast<float>(layer.look.width_um) / 1000.0f);
}

PassStyle pass_of(const core::SymbolLayer& sl, const core::ImageStore& images, double mm_per_pixel)
{
    PassStyle ps;
    ps.type          = sl.type;
    ps.shape         = sl.shape;
    ps.placement     = sl.placement;
    ps.cap           = sl.cap;
    ps.join          = sl.join;
    ps.size_px       = to_pixels(sl.size, mm_per_pixel);
    ps.interval_px   = to_pixels(sl.interval, mm_per_pixel);
    ps.spacing_y_px  = to_pixels(sl.spacing_y, mm_per_pixel);
    ps.offset_px     = to_pixels(sl.offset, mm_per_pixel);
    ps.angle_udeg    = sl.angle_udeg;
    ps.opacity       = sl.opacity;
    ps.line_rgba     = sl.look.rgba;
    ps.image         = images.bytes(sl.image);
    ps.image_key     = images.content_key(sl.image);
    ps.dash          = sl.look.dash;
    ps.line_width_px = stroke_width_px(sl);

    // A marker pass needs the line to walk along; a centroid marker needs the ring
    // to find a centre in. A published sembol sits INSIDE the lekesi it labels, so
    // its ring reaches the polygon batch even though the layer places a glyph.
    ps.wants_stroke = core::draws_stroke(sl.type) || core::draws_marker(sl.type);
    ps.wants_fill   = core::draws_fill(sl.type) || sl.type == core::SymbolLayerType::CentroidFill ||
                    sl.type == core::SymbolLayerType::RasterMarker;
    return ps;
}

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
    // ---- the pass table -------------------------------------------------
    //
    // One pass per SYMBOL LAYER, not per style. A style whose symbol is a fill
    // under a boundary under a glyph produces three passes, and an entity carrying
    // it emits its geometry into all three.
    //
    // `style_first[i]` is where style i's passes begin and `style_count[i]` how
    // many there are, so finding an entity's passes is two array reads — the frame
    // path still evaluates nothing (model.md R14).
    const double mm_per_pixel = view.mm_per_pixel();

    out.pass_first.assign(styles.size() + layers.size(), 0);
    out.pass_count.assign(styles.size() + layers.size(), 0);

    std::size_t pass_count = 0;
    for (std::size_t i = 0; i < styles.size(); ++i)
        pass_count +=
            std::max<std::size_t>(1, styles.symbol_at(static_cast<core::StyleId>(i)).layers.size());
    pass_count += layers.size();

    if (out.passes.size() < pass_count) out.passes.resize(pass_count);
    if (out.polylines.size() < pass_count) out.polylines.resize(pass_count);
    if (out.polygons.size() < pass_count) out.polygons.resize(pass_count);
    out.passes.resize(pass_count);
    out.order.reserve(pass_count);

    // Each pass records the z_order it is drawn at, so the order array can be
    // built without a second walk of the style table.
    out.z_keys.clear();
    out.z_keys.reserve(pass_count);

    std::size_t next    = 0;
    const auto add_pass = [&](const core::SymbolLayer& sl) {
        out.passes[next] = pass_of(sl, doc.images(), mm_per_pixel);

        PolylineBatch& stroke = out.polylines[next];
        PolygonBatch& fill    = out.polygons[next];
        stroke.rgba           = sl.look.rgba;
        stroke.width_px       = stroke_width_px(sl);
        fill.rgba             = sl.look.fill_rgba;
        fill.hatch            = sl.look.hatch;

        out.z_keys.push_back(DrawList::ZKey{sl.look.z_order, static_cast<std::uint32_t>(next)});
        ++next;
    };

    for (std::size_t i = 0; i < styles.size(); ++i) {
        const core::Symbol& sym = styles.symbol_at(static_cast<core::StyleId>(i));
        out.pass_first[i]       = static_cast<std::uint32_t>(next);
        if (sym.layers.empty()) {
            core::SymbolLayer only;
            only.look = styles.entries()[i];
            add_pass(only);
        } else {
            for (const core::SymbolLayer& sl : sym.layers)
                add_pass(sl);
        }
        out.pass_count[i] = static_cast<std::uint32_t>(next) - out.pass_first[i];
    }

    // Layer passes occupy the tail: an entity carrying the ByLayer sentinel draws
    // its layer's own appearance, which is a single plain stroke-and-fill.
    const auto layer_slot = [&](core::LayerId l) {
        return static_cast<std::size_t>(styles.size()) + l;
    };
    for (core::LayerId l = 0; l < layers.size(); ++l) {
        out.pass_first[layer_slot(l)] = static_cast<std::uint32_t>(next);
        core::SymbolLayer only;
        only.look = layers[l].appearance;
        add_pass(only);
        out.pass_count[layer_slot(l)] = 1;
    }

    // Draw order: by z_order, ties broken by pass index so a symbol's own stack
    // stays bottom layer first. std::stable_sort rather than sort, because the tie
    // break IS the stack order and losing it would put a fill over its boundary.
    out.order.resize(pass_count);
    std::stable_sort(out.z_keys.begin(), out.z_keys.end(),
                     [](const DrawList::ZKey& a, const DrawList::ZKey& b) { return a.z < b.z; });
    for (std::size_t i = 0; i < out.z_keys.size(); ++i)
        out.order[i] = out.z_keys[i].pass;

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
            sid == core::kByLayerStyle || sid >= styles.size() ? layer_slot(lid) : sid;

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

        const std::uint32_t first = out.pass_first[slot];
        const std::uint32_t count = out.pass_count[slot];
        if (count == 0) return;

        const core::RingSpan span = geometry.rings_of(entities.slot[e]);

        // A text entity is its baseline plus a string. The baseline is an ordinary
        // open ring, so it is culled, snapped and hit-tested by the same code every
        // other entity uses; only the drawing differs.
        if (texts.has(entities.slot[e]) && span.count > 0) {
            const auto xs = geometry.ring_xs(span.first);
            const auto ys = geometry.ring_ys(span.first);
            if (xs.size() >= 2) {
                TextItem item;
                item.rgba = out.polylines[first].rgba;
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

        // The same geometry into EVERY pass of this style. A gösterim that is a
        // fill, a boundary and a glyph draws the parcel three times, once per
        // layer, which is what a stack means.
        for (std::uint32_t p = first; p < first + count; ++p) {
            const PassStyle& ps  = out.passes[p];
            PolylineBatch& batch = out.polylines[p];
            PolygonBatch& fill   = out.polygons[p];

            // A plain fill with no colour paints nothing, so its rings are not
            // worth collecting; a pattern fill paints whatever its own ink is.
            const bool fill_wanted =
                ps.wants_fill && (fill.rgba != 0 || ps.type != core::SymbolLayerType::SimpleFill);

            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                // The line to walk along and the ring to clip to come from the
                // same geometry; what differs is which buffer they land in.
                if (ps.wants_stroke) emit_ring(batch, r);
                if (fill_wanted && geometry.ring_role[r] != core::RingRole::Open)
                    emit_fill_ring(fill, r);
            }
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
