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
void place_along_run(const float* xs, const float* ys, std::uint32_t count,
                     core::MarkerPlacement placement, double interval, double phase,
                     std::vector<Stamp>& out);

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

/// The parallel lines of a `çizgi-desen-dolgu`, over a box, at an angle.
///
/// Appended as flat segments in SCREEN pixels: x0, y0, x1, y1 per line. The
/// lines are laid out about the box CENTRE so the pattern is continuous across
/// the whole face rather than restarting at each ring, and they overshoot the box
/// by a spacing on every side so a rotated set still covers the corners.
void hatch_lines(float min_x, float min_y, float max_x, float max_y, double spacing,
                 double angle_degrees, std::vector<float>& out);

/// The anchor points of a `nokta-desen-dolgu` over a box.
///
/// Anchored to the pixel grid with `floor(edge / step) * step` rather than to the
/// box itself, so the glyphs do not crawl across the face as the user pans.
void pattern_points(float min_x, float min_y, float max_x, float max_y, double step_x,
                    double step_y, std::vector<float>& out);

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
