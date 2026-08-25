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
#include <QPixmap>

namespace piricad::app {

/// Every icon the shell draws. Drawn as paths rather than shipped as bitmaps, so
/// they stay crisp at any DPI and re-tint with the theme.
enum class Glyph {
    // seçim ve çizim
    Select,
    Line,
    Polyline,
    Arc,
    Circle,
    Rectangle,
    Text,
    Point,
    // düzenleme
    Erase,
    Move,
    Copy,
    Rotate,
    Offset,
    Undo,
    Redo,
    // `katman` ve öznitelik
    Layer,
    LayerManager,
    Table,
    Identify,
    // görünüm ve ölçüm
    Measure,
    ZoomExtents,
    ZoomIn,
    ZoomOut,
    Pan,
    Snap,
    // dosya
    New,
    Open,
    Save,
    Export,
    Print,
    // other
    Script,
    Ai,
    // shell chrome, design.md 7 — the Material Symbols the mockup names, drawn
    // rather than loaded so they re-tint with the theme and never need a font.
    Search,
    Cut,
    Paste,
    Duplicate,
    Close,
    SplitView,
    Fullscreen,
    Grip,
    Collapse,
    Float,
    Eye,
    EyeOff,
    Lock,
    Unlock,
    Filter,
    Plus,
    Minus,
    Fit,
    ChevronDown,
    ChevronRight,
    Cloud,
    Locate,
    Function,
    Polygon,
    History,
    Palette,
    Help,
    Document,
    Globe,
    Terrain,
    Grid,
    Settings,
    // left tool box, design.md 7
    SelectArea,
    Trim,
    Union,
    ParcelSplit,
    MeasureArea,
    Coordinate,
    StyleCopy,
    Topology,
};

/// Renders `glyph` at `size` logical pixels in `colour`, with a checked variant
/// tinted by `accent`.
QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size = 22);

/// The same drawing as a bare pixmap, for a widget that paints its own chrome
/// and wants one glyph inside it rather than a whole `QIcon` with states.
QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr = 1.0);

} // namespace piricad::app
