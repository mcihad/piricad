// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: vector tool icons.
//
// Drawn as paths rather than shipped as bitmaps: they stay crisp at any DPI
// (piricad.md §13 — high DPI and mixed-DPI multi-monitor are requirements) and
// they re-tint when the theme changes. A designed SVG set replaces this in
// Phase 1; the call site does not change.
#pragma once

#include <QColor>
#include <QIcon>

namespace piricad::app {

enum class Glyph {
    Select,
    Line,
    Polyline,
    Erase,
    Layer,
    Measure,
    ZoomExtents,
    ZoomIn,
    ZoomOut,
    Pan,
    Undo,
    Redo,
    Script,
    Ai,
};

/// Renders `glyph` at `size` logical pixels in `colour`, with a checked variant
/// tinted by `accent`.
QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size = 22);

} // namespace piricad::app
