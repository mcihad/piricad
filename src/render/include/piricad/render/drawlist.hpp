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

/// Every stroke that shares one appearance, in one buffer.
///
/// Batched by STYLE rather than by entity: a cadastral sheet has millions of
/// parcels and tens of styles, and one draw call per parcel would spend the whole
/// 16 ms budget on state changes.
struct PolylineBatch
{
    std::uint32_t rgba{0xFFFFFFFFu}; ///< stroke colour for every run in the batch
    float width_px{1.0f};            ///< derived per frame from paper micrometres

    /// Screen-space vertices, already offset-corrected (see view.hpp). Float is
    /// safe HERE and only here: the origin offset has already removed the six
    /// leading digits a TUREF coordinate carries (§10.3).
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

    /// Screen-space ring vertices, offset-corrected like the stroke batches. NOT
    /// LOD-filtered: dropping a vertex from a stroke shortens a line nobody can
    /// measure at that zoom, but dropping one from a face changes the shape being
    /// coloured, and a plan `lekesi` that leaks over its boundary is a wrong
    /// drawing rather than a coarse one.
    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<std::uint32_t> runs;   ///< vertex count of each ring
    std::vector<std::uint8_t> is_hole; ///< parallel to runs
};

/// One caption, resolved to screen space.
///
/// The string is COPIED rather than borrowed from the document. A view into the
/// text pool would be valid only until the next edit, and a draw list that is
/// safe to hold for one frame and unsafe to hold for two is a crash waiting for
/// a slow repaint. A sheet's captions are thousands of short strings, not
/// millions, so the copy is measured in microseconds.
struct TextItem
{
    std::uint32_t rgba{0xFFFFFFFFu}; ///< ink colour
    float x0{0.0f}, y0{0.0f};        ///< baseline start, screen space
    float x1{0.0f}, y1{0.0f};        ///< baseline end — its direction IS the rotation
    float height_px{0.0f};           ///< derived per frame from the ground height
    std::uint8_t anchor{0};          ///< core::TextAnchor, as a byte
    std::string text;                ///< copied, not borrowed — see the note above
};

/// Screen-space lines that are NOT document geometry: the grid, the crosshair, a
/// selection outline, the rubber band, a snap glyph.
///
/// WIDGET COORDINATES, and the difference from the batches above is deliberate.
/// A document batch is centre-relative with y UP, because that is what the origin
/// offset of R2 produces and what a GPU vertex buffer wants. An overlay is
/// computed by the widget from a cursor position and a viewport, both of which are
/// already widget pixels with y DOWN, and converting them into the other
/// convention only to convert them back would be two chances to get a sign wrong.
struct OverlayBatch
{
    std::uint32_t rgba{0xFFFFFFFFu}; ///< line colour
    std::uint32_t fill_rgba{0};      ///< 0 = not filled; used by the selection box
    float width_px{1.0f};            ///< stroke width in logical pixels
    bool dashed{false};              ///< KESEN selection reads as dashed before any label does

    /// Widget-space vertices of every run in the batch.
    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<std::uint32_t> runs;  ///< vertex count of each run
    std::vector<std::uint8_t> closed; ///< parallel to runs: close this run
};

/// One short screen-space string: the snap-mode label, the developer HUD.
///
/// Separate from `TextItem` because it is not a drawing: it has no ground height,
/// no rotation and no anchor measured from the document. It is UI text at a pixel
/// position, and giving it the drawing type would invite one of them to be
/// measured in the other's units.
struct OverlayLabel
{
    std::uint32_t rgba{0xFFFFFFFFu}; ///< ink colour
    float x{0.0f}, y{0.0f};          ///< widget pixels; y is the text baseline
    float px{0.0f};                  ///< 0 = the backend's default UI font size
    std::string text;                ///< copied, like every string in a draw list
};

/// Everything drawn over the document that the document does not contain.
///
/// Built by the canvas widget, which is what knows where the cursor is and what is
/// selected, and drawn by the backend, which is what knows how to draw. Before
/// this existed the widget drew them itself with QPainter, and `render::Backend`
/// had no implementation at all — so the interface Article 8.1 promises to swap
/// the GPU backend behind was an interface nothing went through.
struct Overlay
{
    std::uint32_t background_rgba{0xFF000000u}; ///< cleared to this before anything

    std::vector<OverlayBatch> batches; ///< drawn in order; empty ones are skipped
    std::vector<OverlayLabel> labels;  ///< drawn over the batches

    /// Resets the sizes and KEEPS the buffers, exactly like `DrawList::clear()`.
    void clear();
};

struct DrawList
{
    /// One entry per style id that has something to stroke, in id order.
    std::vector<PolylineBatch> polylines;

    /// One entry per style id that has something to fill, in id order.
    std::vector<PolygonBatch> polygons;

    /// Captions, in entity order. Not batched: text is drawn one string at a
    /// time by every backend that exists, so grouping would buy nothing.
    std::vector<TextItem> texts;

    /// Rubber band from a running interactive command, if any.
    bool has_preview{false};
    float preview_x0{0.0f}, preview_y0{0.0f};
    float preview_x1{0.0f}, preview_y1{0.0f};

    /// Scratch buffer for the spatial index query. Lives here so its capacity
    /// survives between frames and the draw loop allocates nothing (§10.4).
    std::vector<core::EntityId> candidates;

    /// Frame statistics, for the F12 developer overlay and the bench harness.
    /// They are the cheapest evidence there is that the cull and the index are
    /// doing what they claim, and a regression shows up here before it shows up
    /// in a frame time.
    std::size_t fill_count{0};    ///< filled rings emitted
    std::size_t text_count{0};    ///< captions emitted
    std::size_t vertex_count{0};  ///< vertices written into stroke batches
    std::size_t entity_count{0};  ///< entities that reached a batch
    std::size_t culled_count{0};  ///< entities the box test or scale window rejected
    std::size_t indexed_count{0}; ///< candidates the index returned
    std::size_t tail_count{0};    ///< entities scanned outside the index

    /// Resets the sizes and KEEPS the buffers. The draw loop must not allocate
    /// per frame (.claude/render.md), so capacity earned on a busy frame is spent
    /// again on the next one.
    void clear();
};

} // namespace piricad::render
