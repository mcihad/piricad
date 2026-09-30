// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: the mark an input aid draws where it fired.
//
// WHY THIS IS A FUNCTION AND NOT A `switch` IN THE CANVAS.
//
// It WAS a switch in the canvas, with a `default: break;` at the end — and that
// default is a silent hole. A mode with no case drew nothing at all, so the aid
// fired, the point moved, and the marker said it had not. The comment on the
// node snap's own glyph records the first time this was found: it had no case and
// fell through, so snapping to a control point drew nothing at all — the one
// thing on a cadastral sheet from which every boundary is measured.
//
// Seven more had grown the same hole since: the surface normal, the quadrant,
// the tangent, the guide, the centroid, the tracking mark and the step. Adding
// seven cases would have fixed today and left the hole open for the eighth.
//
// So the marks live here, Qt-free, and `tests/unit` walks every bit of
// `core::SnapAllMask` and fails when one of them draws nothing. A mode added
// without a mark now breaks the build instead of shipping invisible.
#pragma once

#include "kentos_cad/render/view.hpp"

#include <cstdint>
#include <vector>

namespace kentos::render {

/// One stroke of a marker: a run of points, closed or open.
struct MarkerRun
{
    std::vector<ScreenPointF> points; ///< at least two, in the local frame
    bool closed{false};               ///< whether the last point joins the first
};

/// A marker as strokes and, when it has one, a ring.
struct Marker
{
    std::vector<MarkerRun> runs; ///< the strokes, in the local frame

    /// The ring's radius, 0 when the mark has none. Centred on the point.
    float ring{0.0F};

    /// Whether anything at all is drawn.
    bool empty() const noexcept { return runs.empty() && ring <= 0.0F; }
};

/// The mark for `mode`, centred on `(x, y)`, sized so it fits in a box of
/// half-width `h` pixels.
///
/// `mode` is ONE bit — the mode a `SnapResult` reports — not a mask. An unknown
/// bit answers the fallback mark rather than nothing, because a point the engine
/// moved must never look like a point it left alone.
Marker snap_marker(std::uint32_t mode, float x, float y, float h);

} // namespace kentos::render
