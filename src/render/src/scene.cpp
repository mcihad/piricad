// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/symbology.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/spatial_index.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::render {
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

/// A measure in this frame's pixels.
///
/// Three units, three conversions, and the difference is visible on screen: a
/// paper size holds still while the user zooms, a ground size grows with the
/// drawing, a pixel size is already what it is.
///
/// `pixels_per_paper_mm` is the OUTPUT RESOLUTION and it used to be the literal
/// 1.0 — one paper millimetre drawn as one screen pixel. A gösterim the annex
/// prints at 8 mm therefore reached the canvas 8 px tall, about a quarter of the
/// size it is printed at, and every published symbol in the program looked like a
/// smudge. It is a resolution, so it comes from the screen.
float to_pixels(const core::Measure& m, double mm_per_pixel, double pixels_per_paper_mm)
{
    switch (m.unit) {
    case core::Unit::Paper:
        return static_cast<float>(static_cast<double>(m.value) / 1000.0 * pixels_per_paper_mm);
    case core::Unit::Ground:
        return mm_per_pixel > 0.0 ? static_cast<float>(static_cast<double>(m.value) / mm_per_pixel)
                                  : 0.0f;
    case core::Unit::Pixel: return static_cast<float>(m.value);
    }
    return 0.0f;
}

} // namespace

float stroke_width_px(const core::SymbolLayer& layer, double pixels_per_paper_mm)
{
    // Width is declared in PAPER micrometres because the regulation declares it
    // on the sheet, so it converts at the output resolution like every other
    // paper measure. At the old one-pixel-per-millimetre it did not: an MPYY
    // 0,5 mm boundary asked for half a pixel, hit the floor below, and every
    // weight in the annex — 0,2 mm, 0,5 mm, 1,0 mm — came out as the same hairline.
    const double px = static_cast<double>(layer.look.width_um) / 1000.0 * pixels_per_paper_mm;
    return std::max(1.0f, static_cast<float>(px));
}

PassStyle pass_of(const core::SymbolLayer& sl, const core::ImageStore& images,
                  const core::DashStore& dashes, double mm_per_pixel, double pixels_per_paper_mm)
{
    PassStyle ps;
    ps.type         = sl.type;
    ps.shape        = sl.shape;
    ps.placement    = sl.placement;
    ps.cap          = sl.cap;
    ps.join         = sl.join;
    ps.size_px      = to_pixels(sl.size, mm_per_pixel, pixels_per_paper_mm);
    ps.interval_px  = to_pixels(sl.interval, mm_per_pixel, pixels_per_paper_mm);
    ps.spacing_y_px = to_pixels(sl.spacing_y, mm_per_pixel, pixels_per_paper_mm);
    ps.offset_px    = to_pixels(sl.offset, mm_per_pixel, pixels_per_paper_mm);
    ps.phase_px     = to_pixels(sl.phase, mm_per_pixel, pixels_per_paper_mm);
    ps.angle_udeg   = sl.angle_udeg;
    ps.opacity      = sl.opacity;
    ps.line_rgba    = sl.look.rgba;
    ps.fill_rgba    = sl.look.fill_rgba;
    ps.image        = images.bytes(sl.image);
    ps.image_key    = images.content_key(sl.image);
    ps.text         = sl.text;
    ps.dash         = sl.look.dash;

    // Resolved HERE, once per pass, not in the backend: the backend has no
    // document and every backend would otherwise have to find one.
    const core::DashPattern& pattern = dashes.at(sl.look.dash);
    ps.dash_count                    = pattern.count;
    for (std::size_t i = 0; i < core::kMaxDashSegments; ++i)
        ps.dash_lengths[i] = pattern.lengths[i];

    ps.line_width_px = stroke_width_px(sl, pixels_per_paper_mm);

    // A marker pass needs the line to walk along; a centroid marker needs the ring
    // to find a centre in. A published sembol sits INSIDE the lekesi it labels, so
    // its ring reaches the polygon batch even though the layer places a glyph.
    ps.wants_stroke = core::draws_stroke(sl.type) || core::draws_marker(sl.type);
    ps.wants_fill   = core::draws_fill(sl.type) || sl.type == core::SymbolLayerType::CentroidFill ||
                    sl.type == core::SymbolLayerType::RasterMarker;

    // A fixed word needs neither the line nor the ring: it is placed from the
    // entity's own bounding box, which the cull test already has.
    if (sl.type == core::SymbolLayerType::TextMarker) {
        ps.wants_stroke = false;
        ps.wants_fill   = false;
    }
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

    // PASSES ARE KEYED BY (LAYER, STYLE), NOT BY STYLE, and that is what makes
    // the layer order mean something. Keyed by style alone, two layers sharing a
    // gösterim shared one batch — so one of them could not be drawn over the
    // other and the layer list said nothing about what covered what. A user turns
    // a layer on expecting it on top; it appeared underneath.
    //
    // Built LAZILY, when an entity on that layer with that style first appears.
    // The alternative — a pass for every combination — is layers x styles, and
    // with the MPYY shelf loaded that is nine thousand batches for a document
    // that uses nine. The vectors keep their capacity between frames, so after
    // the first frame this allocates nothing (render.md R20).
    const std::size_t stride = styles.size() + layers.size();
    out.pass_first.assign(stride * std::max<std::size_t>(1, layers.size()), 0);
    out.pass_count.assign(stride * std::max<std::size_t>(1, layers.size()), 0);

    out.passes.clear();
    out.z_keys.clear();

    // Each pass records the layer and the z_order it is drawn at, so the order
    // array can be built without a second walk of the style table.
    core::LayerId building = 0;

    const auto add_pass = [&](const core::SymbolLayer& sl) {
        // A layer switched off in the designer produces NO PASS. Kept in the
        // symbol, kept in the file, kept in the fingerprint — just not painted,
        // and therefore costing nothing per frame.
        if (!sl.enabled) return;

        const std::size_t at = out.passes.size();
        out.passes.push_back(
            pass_of(sl, doc.images(), doc.dashes(), mm_per_pixel, options.pixels_per_paper_mm));

        // Grown, not indexed into a pre-sized table: how many passes a frame
        // needs is not known until its entities are walked. The capacity survives
        // the frame, so this stops allocating after the first one.
        if (out.polylines.size() <= at) out.polylines.resize(at + 1);
        if (out.polygons.size() <= at) out.polygons.resize(at + 1);

        PolylineBatch& stroke = out.polylines[at];
        PolygonBatch& fill    = out.polygons[at];
        stroke.xs.clear();
        stroke.ys.clear();
        stroke.runs.clear();
        fill.xs.clear();
        fill.ys.clear();
        fill.runs.clear();
        fill.is_hole.clear();
        stroke.rgba = sl.look.rgba;
        stroke.width_px =
            options.line_weights ? stroke_width_px(sl, options.pixels_per_paper_mm) : 1.0f;
        fill.rgba  = sl.look.fill_rgba;
        fill.hatch = sl.look.hatch;

        out.z_keys.push_back(
            DrawList::ZKey{building, sl.look.z_order, static_cast<std::uint32_t>(at)});
    };

    // The slot an entity's style resolves to. Layer slots occupy the tail: an
    // entity carrying the ByLayer sentinel draws its layer's own appearance,
    // which is a single plain stroke-and-fill.
    const auto layer_slot = [&](core::LayerId l) {
        return static_cast<std::size_t>(styles.size()) + l;
    };

    // Builds the passes for one (layer, slot) the first time an entity needs
    // them, and answers with where they start and how many there are.
    const auto passes_for = [&](core::LayerId l, std::size_t slot) {
        const std::size_t key = static_cast<std::size_t>(l) * stride + slot;
        if (out.pass_count[key] != 0) return key;

        building            = l;
        out.pass_first[key] = static_cast<std::uint32_t>(out.passes.size());

        // A SIZE ON THE PLAIN PASS, so a lone vertex has something to draw with.
        // The pass stays a SimpleLine and a polyline is unaffected — a stroke has
        // no use for `size` — but a point, which outlines to one vertex and would
        // otherwise draw nothing, gets the default disc (`kDefaultPointSizeUm`).
        const auto plain = [](core::Appearance look) {
            core::SymbolLayer only;
            only.look = look;
            only.size = core::Measure{kDefaultPointSizeUm, core::Unit::Paper};
            return only;
        };

        if (slot >= styles.size()) {
            add_pass(plain(layers[slot - styles.size()].appearance));
        } else {
            const core::Symbol& sym = styles.symbol_at(static_cast<core::StyleId>(slot));
            if (sym.layers.empty()) {
                add_pass(plain(styles.entries()[slot]));
            } else {
                for (const core::SymbolLayer& sl : sym.layers)
                    add_pass(sl);
            }
        }

        out.pass_count[key] = static_cast<std::uint32_t>(out.passes.size()) - out.pass_first[key];
        return key;
    };

    // One denominator for the whole frame: the view does not change mid-build.
    const double denominator = view.scale_denominator();

    const Box2 visible   = view.visible_box();
    const auto& entities = doc.entities();
    const auto& geometry = doc.geometry();

    // A vertex closer than this to its predecessor cannot be told apart on screen.
    // Precomputed LOD tiles replace this per-frame filter in Phase 1 (§10.3).
    const double lod_mm = options.lod ? options.lod_pixels * view.mm_per_pixel() : 0.0;

    // A CIRCLE'S RINGS ARE ITS DEFINITION, NOT ITS PICTURE: two vertices, the
    // centre and a handle due east (core/circle.hpp). Walked as stored they draw
    // as a short line pointing east, so the drawn form is built here and the ring
    // readers below are pointed at it instead.
    //
    // Rebuilt per entity into buffers that keep their capacity, because the frame
    // path does not allocate (§10.4).
    // While `curve_active`, a "ring" index below is a RUN index into `curve`: the
    // outline of a kind may hold several runs (a hatch's boundary and its
    // islands), and each is walked exactly as a stored ring would be.
    core::EmitBuffer curve;
    bool curve_active = false;

    const auto ring_xs = [&](std::uint32_t ring) {
        return curve_active ? curve.run_xs(ring) : geometry.ring_xs(ring);
    };
    const auto ring_ys = [&](std::uint32_t ring) {
        return curve_active ? curve.run_ys(ring) : geometry.ring_ys(ring);
    };

    /// Whether this run encloses anything — which decides both the closing
    /// segment and whether it can be filled. A circle does; an arc does not.
    const auto ring_closed = [&](std::uint32_t ring) {
        return curve_active ? curve.run_closed[ring] != 0
                            : geometry.ring_role[ring] != core::RingRole::Open;
    };

    /// Whether this run is a void in its entity — a hole to leave unpainted.
    const auto ring_hole = [&](std::uint32_t ring) {
        return curve_active ? curve.run_hole[ring] != 0
                            : geometry.ring_role[ring] == core::RingRole::Interior;
    };

    const auto emit_ring = [&](PolylineBatch& batch, std::uint32_t ring) {
        const auto xs = ring_xs(ring);
        const auto ys = ring_ys(ring);

        // ONE VERTEX IS A POINT AND IT SURVIVES. This used to require two, which
        // is the right rule for a stroke — a run of one has no segment to draw —
        // and the wrong rule for the batch: a `NOKTA` outlines to exactly one
        // vertex (`point_outline_fn`), so every surveyed point was thrown away
        // here before any backend could decide what a point looks like. Every
        // surveyed point was invisible on the canvas.
        if (xs.empty()) return;

        const bool closed       = ring_closed(ring);
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
        if (emitted == 0) {
            batch.xs.resize(first);
            batch.ys.resize(first);
            return;
        }

        batch.runs.push_back(emitted);
        out.vertex_count += emitted;
    };

    const auto emit_fill_ring = [&](PolygonBatch& batch, std::uint32_t ring) {
        const auto xs = ring_xs(ring);
        const auto ys = ring_ys(ring);
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
        batch.is_hole.push_back(ring_hole(ring) ? 1u : 0u);
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

        // What KIND of thing this is decides what its rings mean. Only a curve
        // needs the substitution, so a polyline — which is every entity on the
        // five-million-parcel sheet the budget is written against — pays one
        // comparison and reads its vertices exactly as it always did (§10.1).
        curve_active = core::entity_outline(doc, e, curve);

        const core::LayerId lid = entities.layer[e];
        if (lid >= layers.size()) return;

        // The style column decides, and falls back to the layer only when it
        // carries the ByLayer sentinel. This is the one lookup the frame path
        // does, and it is an array index — never a rule, an expression or a
        // cascade (R14, P29).
        const core::StyleId own_style = entities.style[e];
        const bool has_own_style = own_style != core::kByLayerStyle && own_style < styles.size();
        const core::StyleId layer_style = layers[lid].style;
        const bool has_layer_style =
            layer_style != core::kByLayerStyle && layer_style < styles.size();
        const core::StyleId effective_style = has_own_style ? own_style : layer_style;
        const std::size_t slot =
            has_own_style || has_layer_style ? effective_style : layer_slot(lid);

        // Scale-dependent visibility. Not decoration in planning work: a
        // `çevre düzeni planı` at 1/100000 shows a `lekesi` where the `uygulama imar planı`
        // at 1/1000 shows its parcels, and drawing both at both scales produces a
        // sheet nobody can read. The window is stored on the symbol, so this is
        // an array lookup and a comparison — no rule is evaluated (R14).
        if (has_own_style || has_layer_style) {
            const core::Symbol& sym = styles.symbol_at(effective_style);
            if (sym.min_scale != 0 && denominator < static_cast<double>(sym.min_scale)) {
                ++out.culled_count;
                return;
            }
            if (sym.max_scale != 0 && denominator > static_cast<double>(sym.max_scale)) {
                ++out.culled_count;
                return;
            }
        }

        // Built on first use for THIS layer, so two layers sharing a gösterim
        // get their own batches and can cover one another.
        const std::size_t key     = passes_for(lid, slot);
        const std::uint32_t first = out.pass_first[key];
        const std::uint32_t count = out.pass_count[key];
        if (count == 0) return;

        const core::RingSpan span = geometry.rings_of(entities.slot[e]);

        /// One caption: the baseline from `x0,y0` to `x1,y1` in document
        /// millimetres, the text-table entry at `tslot`, in `rgba`.
        const auto emit_caption = [&](std::uint32_t tslot, core::Mm x0, core::Mm y0, core::Mm x1,
                                      core::Mm y1, std::uint32_t rgba) {
            TextItem item;
            item.rgba = rgba;
            item.x0   = view.offset_x_f(x0);
            item.y0   = view.offset_y_f(y0);
            item.x1   = view.offset_x_f(x1);
            item.y1   = view.offset_y_f(y1);

            // Ground millimetres to pixels, like every other length on screen.
            // A height in paper units would change what the drawing SAYS when
            // the plot scale changes, and on a pafta the height of a parcel
            // number is part of the drawing (R20).
            item.height_px =
                static_cast<float>(static_cast<double>(texts.height(tslot)) / view.mm_per_pixel());
            item.anchor = static_cast<std::uint8_t>(texts.anchor(tslot));
            item.text.assign(texts.text(tslot));

            out.texts.push_back(std::move(item));
            ++out.text_count;
        };

        // A text entity is its baseline plus a string. The baseline is an ordinary
        // open ring, so it is culled, snapped and hit-tested by the same code every
        // other entity uses; only the drawing differs. A DIMENSION carries its
        // caption the same way and draws its lines as well: only a polyline's
        // baseline is construction and nothing else.
        if (texts.has(entities.slot[e]) && span.count > 0) {
            const auto xs = geometry.ring_xs(span.first);
            const auto ys = geometry.ring_ys(span.first);
            if (xs.size() >= 2) {
                emit_caption(entities.slot[e], xs.front(), ys.front(), xs.back(), ys.back(),
                             out.polylines[first].rgba);
                ++out.entity_count;
                if (entities.kind[e] == core::kPolylineKind || !curve_active) return;
            }
        }

        /// Walks run `r` (a ring index, or a run index while `curve_active`)
        /// into every pass from `p_first` for `p_count`.
        const auto emit_run_into = [&](std::uint32_t p_first, std::uint32_t p_count,
                                       std::uint32_t r) {
            for (std::uint32_t p = p_first; p < p_first + p_count; ++p) {
                const PassStyle& ps  = out.passes[p];
                PolylineBatch& batch = out.polylines[p];
                PolygonBatch& fill   = out.polygons[p];

                // A fixed word is placed from the entity's own bounding box, not
                // from its rings: the box is what the cull test already read, and
                // the centre of it is where a plan puts a gösterim's own lettering.
                if (ps.type == core::SymbolLayerType::TextMarker) {
                    if (ps.text.empty()) continue;

                    const core::Box2 box      = entities.box_of(e);
                    const core::Point2 centre = box.centre();

                    TextItem item;
                    item.rgba      = ps.line_rgba;
                    item.x0        = view.offset_x_f(centre.x);
                    item.y0        = view.offset_y_f(centre.y) + ps.offset_px;
                    item.x1        = item.x0 + 1.0f; // horizontal; the baseline IS the rotation
                    item.y1        = item.y0;
                    item.height_px = ps.size_px > 0.5f ? ps.size_px : 10.0f;

                    // Centred both ways, because a word inside a circle sits in the
                    // middle of it and the offset is what moves it off centre.
                    item.anchor = static_cast<std::uint8_t>(core::TextAnchor::MiddleCentre);
                    item.text.assign(ps.text);

                    out.texts.push_back(std::move(item));
                    ++out.text_count;
                    continue;
                }

                // A plain fill with no colour paints nothing, so its rings are not
                // worth collecting; a pattern fill paints whatever its own ink is.
                const bool fill_wanted =
                    ps.wants_fill &&
                    (fill.rgba != 0 || ps.type != core::SymbolLayerType::SimpleFill);

                // The line to walk along and the ring to clip to come from the
                // same geometry; what differs is which buffer they land in.
                if (ps.wants_stroke) emit_ring(batch, r);
                // A CLOSED CURVE IS A FACE. A circle encloses ground exactly as a
                // parsel ring does, so it takes the same fill; an arc encloses
                // nothing and takes none.
                if (fill_wanted && ring_closed(r)) emit_fill_ring(fill, r);
            }
        };

        // A stored entity walks its rings; an outlined kind walks its runs. The
        // same geometry goes into EVERY pass of the style: a gösterim that is a
        // fill, a boundary and a glyph draws the parcel three times, once per
        // layer, which is what a stack means.
        const std::uint32_t first_run = curve_active ? 0u : span.first;
        const std::uint32_t run_count =
            curve_active ? static_cast<std::uint32_t>(curve.run_total()) : span.count;
        for (std::uint32_t r = first_run; r < first_run + run_count; ++r) {
            if (!curve_active) {
                emit_run_into(first, count, r);
                continue;
            }

            // A RUN MAY BRING ITS OWN STYLE, LAYER AND CAPTION — a block
            // reference's members keep theirs (model.md R45, render.md R15).
            // The inherit sentinels mean the entity's, which is the common case
            // and costs three comparisons.
            const std::uint32_t rs = curve.run_style[r];
            const std::uint32_t rl = curve.run_layer[r];
            const std::uint32_t rt = curve.run_text[r];
            if (rs == core::kInheritRunStyle && rl == core::kInheritRunLayer &&
                rt == core::kNoRunText) {
                emit_run_into(first, count, r);
                continue;
            }

            const core::LayerId run_layer = rl == core::kInheritRunLayer ? lid : rl;
            if (run_layer >= layers.size()) continue;
            std::size_t run_slot = slot;
            if (rs != core::kInheritRunStyle) {
                if (rs < styles.size()) run_slot = rs;
            } else if (rl != core::kInheritRunLayer) {
                // The member's own layer decides, exactly as it would for an
                // entity drawn on that layer with the ByLayer sentinel.
                const core::StyleId ls = layers[run_layer].style;
                run_slot =
                    (ls != core::kByLayerStyle && ls < styles.size()) ? ls : layer_slot(run_layer);
            }
            const std::size_t run_key   = passes_for(run_layer, run_slot);
            const std::uint32_t r_first = out.pass_first[run_key];
            const std::uint32_t r_count = out.pass_count[run_key];
            if (r_count == 0) continue;

            if (rt != core::kNoRunText && texts.has(rt)) {
                // The run is a member caption's baseline: drawn as text, in the
                // pass's colour, and the baseline itself is construction.
                const auto xs = curve.run_xs(r);
                const auto ys = curve.run_ys(r);
                if (xs.size() >= 2)
                    emit_caption(rt, xs.front(), ys.front(), xs.back(), ys.back(),
                                 out.polylines[r_first].rgba);
                continue;
            }
            emit_run_into(r_first, r_count, r);
        }

        ++out.entity_count;
    };

    // THE SORT RUNS LAST, after every entity has been walked, and it has to:
    // the passes are built on first use now, so at the top of this function
    // `z_keys` is empty and sorting it there produced an EMPTY draw order —
    // a document that rendered nothing at all.
    const auto finish = [&] {
        // DRAW ORDER: by LAYER first, then by z_order, ties broken by pass index so
        // a symbol's own stack stays bottom layer first.
        //
        // The layer comes first because that is what a user is arranging when they
        // arrange the layer list, and it is the promise the list makes: a layer
        // further down the document covers the one above it. `z_order` refines
        // WITHIN a layer — MPYY prescribes a draw order for the parts of a gösterim,
        // and that order is about one symbol, not about which layer wins.
        //
        // std::stable_sort rather than sort, because the tie break IS the stack order
        // and losing it would put a fill over its own boundary.
        out.order.resize(out.z_keys.size());
        std::stable_sort(out.z_keys.begin(), out.z_keys.end(),
                         [](const DrawList::ZKey& a, const DrawList::ZKey& b) {
                             return a.layer != b.layer ? a.layer < b.layer : a.z < b.z;
                         });
        for (std::size_t i = 0; i < out.z_keys.size(); ++i)
            out.order[i] = out.z_keys[i].pass;
    };

    if (!options.cull) {
        for (core::EntityId e = 0; e < entities.size(); ++e)
            emit(e);
        finish();
        return;
    }

    // The index narrows five million parcels to the handful sharing a leaf with
    // the viewport; entities added since it was last packed are a short tail that
    // is cheaper to scan than to repack (kentoscad.md §10.5).
    const core::SpatialIndex& index = doc.spatial_index();

    // Zoomed far enough out that everything is on screen: the tree can only answer
    // "all of them", and a sequential scan beats collecting five million
    // candidates to say so. LOD tiles take this case over in Phase 1 (§10.3).
    if (index.empty() || box_contains(visible, index.bounds())) {
        for (core::EntityId e = 0; e < entities.size(); ++e)
            emit(e);
        out.tail_count = entities.size();
        finish();
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

    finish();
}

} // namespace kentos::render
