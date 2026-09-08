// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: vector tool icons.
//
// Drawn as paths rather than shipped as bitmaps: they stay crisp at any DPI
// (kentoscad.md §13 — high DPI and mixed-DPI multi-monitor are requirements) and
// they re-tint when the theme changes. A designed SVG set replaces this in
// Phase 1; the call site does not change.
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace kentos::app {

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
    Split,
    MeasureArea,
    Coordinate,
    StyleCopy,
    Topology,

    // ---- the component standard, `bileşen_standardı.png` ----------------------
    Trash,    ///< a bin: the destructive button's mark, not the eraser tool's
    Pencil,   ///< editing, as a state: the mode toggle and the edit-mode tool
    Tune,     ///< three sliders: the icon-only button's example
    Check,    ///< a tick: the checked box, a validation rule that holds
    Calendar, ///< a date field's picker
    Warning,  ///< a triangle with a bar: a banner that asks for care
    Info,     ///< a ring with a dot: a banner that only informs
    Ruler,    ///< a length input's leading mark
    Refresh,  ///< two arcs chasing each other: read it again from the server
};

/// Renders `glyph` at `size` logical pixels in `colour`, with a checked variant
/// tinted by `accent`.
QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size = 22);

/// The same drawing as a bare pixmap, for a widget that paints its own chrome
/// and wants one glyph inside it rather than a whole `QIcon` with states.
QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr = 1.0);

} // namespace kentos::app
