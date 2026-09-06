// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: the geometry of a published gösterim, without a painter.
//
// WHAT THIS IS FOR. MPYY's symbology is not lines and washes: of the 1 574 symbol
// layers in `/data/catalogs/mpyy-vektor`, 816 are markers, marker lines and
// pattern fills. A canvas that draws strokes and solid faces draws about a third
// of the annex, and the third that is easiest.
//
// WHERE a glyph goes and WHAT SHAPE it is are arithmetic. HOW it reaches the
// screen is a backend's business, and the two backends answer it differently —
// one hands a `QPainterPath` to `QPainter`, the other appends triangles and
// segments to a vertex buffer. Keeping the arithmetic here is what stops the two
// from disagreeing about where MPYY's ETAPLAMA SINIRI puts its second circle,
// which is the kind of difference nobody notices until a plan sheet is printed.
//
// Qt-free, like everything under /src/render (CLAUDE.md 3.4), and therefore
// testable in a suite that links no Qt.
#pragma once

#include "kentos_cad/core/style.hpp"

#include <cstdint>
#include <vector>

namespace kentos::render {

/// One glyph placement along a line: where, and which way it faces.
///
/// The rotation is a DIRECTION VECTOR and not an angle, for the same reason a
/// caption's is (see `drawlist.hpp`): it comes from the segment the glyph sits
/// on, and turning it into degrees and back is two chances to lose a sign.
struct Stamp
{
    float x{0.0f}, y{0.0f};         ///< screen pixels
    float cos_a{1.0f}, sin_a{0.0f}; ///< the direction the glyph's local +x points
};

/// Walks one run of a polyline and appends the stamps its placement asks for.
///
/// `xs`/`ys` are SCREEN pixels — the caller has already applied the origin offset
/// and the y flip, because that is where the two conventions of `drawlist.hpp`
/// meet and doing it twice is how they diverge.
///
/// LOCAL +X IS ALONG THE LINE for every glyph, which is the whole convention the
/// shapes below are drawn to: an arrow points at +x, and a tick runs along local
/// y and therefore already crosses the line. A quarter turn added for hash ticks
/// lays them flat ON the line, where they are invisible.
///
/// `interval` and `phase` are in pixels. A phase of zero means half an interval:
/// a marker that starts ON the first vertex reads as part of the corner rather
/// than as one of a series. The phase is what lets two marker lines at one
/// spacing say different things — MPYY's ETAPLAMA SINIRI alternates a filled
/// circle with an open one, half a step apart.
/// A rectangle in SCREEN pixels. Used for a face's bounding box and for the
/// visible area, which the pattern generators need to tell apart.
struct PixelBox
{
    float min_x{0.0f}; ///< left edge, pixels from the canvas's left
    float min_y{0.0f}; ///< top edge, pixels from the canvas's top, y down
    float max_x{0.0f}; ///< right edge
    float max_y{0.0f}; ///< bottom edge

    /// True when the rectangle encloses nothing. A default-constructed box is
    /// empty, and the generators read that as "no clip given".
    bool empty() const noexcept { return max_x <= min_x || max_y <= min_y; }
};

/// `clip` drops the stamps that cannot be seen, and it is the difference between
/// a plan that pans and one that does not. The interval is in PIXELS while the
/// run is a parcel boundary in world units, so at 1:1 one edge is hundreds of
/// thousands of pixels long and carries tens of thousands of glyphs — each of
/// them a full marker outline, built and uploaded every frame for a mark nobody
/// can see. The WALK still crosses the whole run, so the phase is exactly what it
/// would have been; only the emission stops. An empty clip means "not known" and
/// stamps everything, which is what this did before.
void place_along_run(const float* xs, const float* ys, std::uint32_t count,
                     core::MarkerPlacement placement, double interval, double phase,
                     const PixelBox& clip, std::vector<Stamp>& out);

/// A glyph's outline in LOCAL pixels, centred on the origin.
///
/// Contours rather than one polygon, because two of the shapes are open strokes:
/// a `Cross` is two segments that do not close, and a `Tick` is one. A backend
/// fills the closed contours and strokes all of them.
struct MarkerOutline
{
    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<std::uint32_t> runs;  ///< vertex count of each contour
    std::vector<std::uint8_t> closed; ///< parallel to `runs`

    /// Resets the sizes and KEEPS the buffers. A pattern fill asks for the same
    /// outline once per face and the draw loop must not allocate (render.md R20).
    void clear();
};

/// Builds `shape` at `size` pixels across, centred on the origin.
///
/// EVERY SHAPE HERE IS STAR-SHAPED ABOUT THE ORIGIN, and that is not a
/// coincidence — it is what "a marker centred on its point" means. It is also
/// what lets a backend triangulate one by fanning from (0,0) without a
/// triangulator: the star's concave notches and the arrow's tail are both
/// visible from the centre.
void marker_outline(core::MarkerShape shape, double size, MarkerOutline& out);

/// The parallel lines of a `çizgi-desen-dolgu`, over a face, at an angle.
///
/// Appended as flat segments in SCREEN pixels: x0, y0, x1, y1 per line. The lines
/// are laid out about the FACE's centre so the pattern is continuous across the
/// whole face rather than restarting at each ring, and they overshoot by a spacing
/// so a rotated set still covers the corners.
///
/// `clip` IS WHY THIS FUNCTION IS FAST, and it is not an optimisation that can be
/// skipped. The spacing is in PIXELS while the face is in world units, so at 1:1
/// a parcel's bounding box is hundreds of screen widths across and covering it at
/// six-pixel spacing means hundreds of thousands of segments — every one of them
/// built, uploaded and then thrown away by the rasteriser. Measured on a styled
/// imar plan: 3.8 MILLION vertices and 19.8 ms a frame with a handful of parcels
/// on screen, against a 16 ms budget (§10.1).
///
/// The clip narrows the RANGE and never the PHASE: line i still sits at
/// `i * spacing` from the face's own centre, so panning slides the pattern with
/// the parcel instead of making it crawl across it.
void hatch_lines(const PixelBox& face, const PixelBox& clip, double spacing, double angle_degrees,
                 std::vector<float>& out);

/// The anchor points of a `nokta-desen-dolgu` over a face.
///
/// Anchored to the pixel grid with `floor(edge / step) * step` rather than to the
/// face itself, so the glyphs do not crawl across it as the user pans.
///
/// `clip` bounds the grid to what can be seen, for the reason `hatch_lines`
/// gives — and more sharply, because a grid is TWO dimensional: at four times the
/// magnification a hatch costs four times as much and a glyph grid sixteen.
void pattern_points(const PixelBox& face, const PixelBox& clip, double step_x, double step_y,
                    std::vector<float>& out);

/// The size a point with NO symbology of its own is drawn at, in PAPER
/// micrometres — 1,6 mm on the sheet.
///
/// A `NOKTA` outlines to a single vertex and nothing else: `point_outline_fn`
/// deliberately emits no marker, because what a nirengi or a röper LOOKS like is
/// a regulated gösterim and belongs in `/data`, not in the geometry (model.md
/// R14, CLAUDE.md 5.13). The consequence was that a point carrying no style drew
/// nothing at all — the user placed one and the canvas stayed empty.
///
/// So this is the placeholder, and it is deliberately NOT any published symbol: a
/// plain disc, which imitates no gösterim and reads instantly as "unstyled". The
/// moment a style gives the point a marker layer, that marker is what draws.
inline constexpr std::int32_t kDefaultPointSizeUm = 1600;

} // namespace kentos::render
