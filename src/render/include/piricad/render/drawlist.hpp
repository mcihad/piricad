// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — render: the backend-independent draw list.
//
// The scene builder produces this; a backend consumes it. Swapping the QPainter
// backend for the QRhi backend must not touch anything above this header.
#pragma once

#include "piricad/core/document.hpp"
#include "piricad/core/units.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::render {

/// One symbol layer of one style, resolved to this frame's pixels.
///
/// The scene builder walks each symbol's stack and produces one PASS per layer;
/// this is what a pass draws. Every size arrives already converted from its
/// `core::Measure` — paper micrometres, ground millimetres or screen pixels are
/// three different conversions and the backend performs none of them, because a
/// backend that converted units would have to know the view scale, and a backend
/// that knows the view scale is a backend that can disagree with the scene about
/// what the drawing looks like.
struct PassStyle
{
    core::SymbolLayerType type{core::SymbolLayerType::SimpleLine};    ///< what it draws
    core::MarkerShape shape{core::MarkerShape::Circle};               ///< which glyph
    core::MarkerPlacement placement{core::MarkerPlacement::Interval}; ///< where on the line
    core::LineCap cap{core::LineCap::Round};                          ///< how a stroke ends
    core::LineJoin join{core::LineJoin::Round};                       ///< how segments meet

    float size_px{0.0f};      ///< marker diameter, or hash tick length
    float interval_px{0.0f};  ///< spacing along a line, or the first pattern axis
    float spacing_y_px{0.0f}; ///< the second pattern axis; 0 means square
    float offset_px{0.0f};    ///< perpendicular offset from the geometry
    float phase_px{0.0f};     ///< distance ALONG the line before the first marker

    std::int32_t angle_udeg{0}; ///< pattern angle, or glyph rotation
    std::uint8_t opacity{255};  ///< multiplied into this layer's colours

    /// Ink for the glyphs of a marker type and the lines of a pattern fill.
    ///
    /// The layer's STROKE colour, carried here as well as on the stroke batch
    /// because a pattern fill has geometry in the polygon batch and its ink in the
    /// stroke one, and a backend should not have to hold both to draw one thing.
    std::uint32_t line_rgba{0xFF000000u};
    float line_width_px{1.0f}; ///< width of those glyph and pattern strokes

    /// The interior of a glyph, 0 for outline-only.
    ///
    /// A marker has two colours the way every drawn shape does — an outline and an
    /// interior — and they are the layer's stroke and fill. Carried here for the
    /// same reason `line_rgba` is: a centroid marker's geometry is in the polygon
    /// batch and its ink in the stroke one.
    std::uint32_t fill_rgba{0};
    std::uint16_t dash{0}; ///< index into the document's DashStore

    /// The line type's own segment lengths, resolved out of the store so a
    /// backend needs no document. Mark first, in hundredths of the stroke width;
    /// `dash_count` of them are meaningful and zero means solid.
    ///
    /// Copied into the pass rather than reached for through a pointer because a
    /// draw list outlives the call that built it by a frame and a backend that
    /// followed a pointer into a document being edited would read a freed table.
    std::uint16_t dash_lengths[8]{};
    std::uint8_t dash_count{0};

    /// The picture a raster type draws, BORROWED from the document's image store.
    ///
    /// A view rather than a copy: a hatch is kilobytes and the pass table is
    /// rebuilt every frame, so copying would be a per-frame allocation for data
    /// that has not changed. Valid for exactly as long as the frame is: the
    /// document does not move while it is being drawn, because render never
    /// mutates it (render.md P7) and the draw list is consumed inside the same
    /// paint.
    std::span<const std::byte> image;

    /// Cache key for the decoded form of `image`, 0 when there is none. See
    /// `core::ImageStore::content_key` for why it is not an address.
    std::uint64_t image_key{0};

    /// What a `TextMarker` writes, BORROWED from the symbol for this frame.
    ///
    /// A view rather than a copy, for the same reason `image` is one: the pass
    /// table is rebuilt every frame and the words have not changed. Valid exactly
    /// as long as the frame, because render never mutates the document
    /// (render.md P7) and the draw list is consumed inside the same paint.
    std::string_view text;

    /// Whether this pass wants the geometry as a line, as a face, or both.
    ///
    /// Derived from `type` and stored, because the emit loop asks the question
    /// once per RING per ENTITY — five million times on the cadastral bench — and
    /// the answer depends only on the pass. Asking `core::draws_stroke()` there
    /// cost thirteen per cent of the full-extent frame; the pass table is built
    /// once, so this is the place for it.
    bool wants_stroke{true};
    bool wants_fill{false};
};

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

/// How many stamps go on one edge, and how far apart.
///
/// The answer to a question that produced two visible defects in a row, in
/// opposite directions. Marching by a fixed interval from the start of an edge
/// leaves the remainder as a GAP before the corner; letting a stamp land wherever
/// the interval falls lets a wide one OVERSHOOT the corner. A published çizgi
/// tipi is a picture and pictures are wide, so both are visible on a parcel
/// boundary: one draws a boundary that stops short of the parcel, the other one
/// that runs past it.
///
/// Lives here, in Qt-free render, rather than in the backend that draws: it is
/// arithmetic about geometry, it is what went wrong twice, and a backend is not
/// somewhere a test can reach.
struct EdgeStamps
{
    int count{0};      ///< how many stamps; 0 when none fits
    double first{0.0}; ///< distance along the edge to the first stamp centre
    double step{0.0};  ///< distance between consecutive centres; 0 when count is 1
};

/// Distributes stamps along one edge so both ends are closed.
///
/// `margin` is half the stamp's own length: the first centre sits exactly that
/// far from the start and the last exactly that far from the end, so nothing
/// crosses either corner and nothing stops short of one. The count is the one
/// nearest `interval`, and the resulting spacing differs from it by less than
/// half a step — which no reader can see and which is what a printed annex does.
///
/// An edge shorter than one whole stamp gets NONE. Drawing it anyway is what
/// produced the overshoot: the picture cannot fit and the difference goes outside
/// the geometry.
EdgeStamps distribute_along(double length, double interval, double margin) noexcept;

struct DrawList
{
    /// What each batch index draws. Parallel to `polylines` and `polygons`: one
    /// entry per PASS, where a pass is one symbol layer of one style.
    ///
    /// Before this, a batch was one STYLE and the renderer read only the stack's
    /// primary layer — so a gösterim declared as a fill under a boundary under a
    /// glyph reached the screen as the boundary alone, and the other two layers
    /// were carried through the document, the file and the hash to be dropped in
    /// the last step.
    std::vector<PassStyle> passes;

    /// Stroke geometry per pass, parallel to `passes`.
    std::vector<PolylineBatch> polylines;

    /// Fill geometry per pass, parallel to `passes`.
    std::vector<PolygonBatch> polygons;

    /// Batch indices in DRAW ORDER, back to front.
    ///
    /// Not the same as index order, and the difference is the point: MPYY
    /// prescribes a draw order for plan sheets (`Appearance::z_order`), and within
    /// one symbol the stack is drawn bottom layer first. A renderer that drew
    /// batches in id order would put a road under the block it crosses whenever
    /// the road's style happened to be interned first.
    std::vector<std::uint32_t> order;

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

    /// Where each style's passes begin, and how many it has. Indexed by style id,
    /// with the layer defaults in the tail. Scratch, kept here for the same reason
    /// `candidates` is: the capacity survives between frames.
    std::vector<std::uint32_t> pass_first;
    std::vector<std::uint32_t> pass_count;

    /// One pass and the depth it draws at, for building `order`.
    struct ZKey
    {
        std::int16_t z{0};     ///< Appearance::z_order of the symbol layer
        std::uint32_t pass{0}; ///< index into `passes`
    };

    /// Sort scratch for `order`. Also kept between frames.
    std::vector<ZKey> z_keys;

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
