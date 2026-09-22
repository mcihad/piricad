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
    Trash,       ///< a bin: the destructive button's mark, not the eraser tool's
    Pencil,      ///< editing, as a state: the mode toggle and the edit-mode tool
    Tune,        ///< three sliders: the icon-only button's example
    Check,       ///< a tick: the checked box, a validation rule that holds
    Calendar,    ///< a date field's picker
    Warning,     ///< a triangle with a bar: a banner that asks for care
    Info,        ///< a ring with a dot: a banner that only informs
    Ruler,       ///< a length input's leading mark
    Refresh,     ///< two arcs chasing each other: read it again from the server
    Sigma,       ///< Σ: the column statistics of the attribute table
    Invert,      ///< two squares, one filled: invert the selection
    ChevronLeft, ///< the pager's `previous`
    PageFirst,   ///< a chevron against a bar: the pager's `first`
    PageLast,    ///< its mirror: the pager's `last`
    ChevronUp,   ///< the stack's `move up`
    DataObject,  ///< `{ }`: a property driven by a column — QGIS's data-defined override

    // ---- the conversation and the agent server -------------------------------
    Send,   ///< a paper plane: the composer's own action
    Attach, ///< a paperclip: add a file to the turn
    Stop,   ///< a filled square: cancel the turn in flight
    Chat,   ///< a speech bubble: the panel's own mark on the tool bar
    Server, ///< a stack of two boxes: the MCP listener
    Plug,   ///< a two-pin plug: a client connected to it

    // ---- shapes and edits that were wearing another tool's mark ---------------
    //
    // A COLUMN SHOWS ICONS, NOT LABELS, so two tools with one glyph are one tool
    // to the hand that reaches for them. These four were reported as exactly
    // that: `ESNET` wore the four-way arrows that belong to `TAŞI`, and `ELİPS`,
    // `HALKA` and `DİLİM` all wore the circle of `DAİRE` although not one of
    // them is a circle.
    Stretch, ///< a corner pulled out of a frame: ESNET
    Ellipse, ///< an ellipse with its two axes: ELİPS
    Annulus, ///< two concentric circles: HALKA
    Sector,  ///< a wedge cut from a circle: DAİRE DİLİMİ

    // ---- the point family, five tools that wore three marks ------------------
    //
    // `NOKTA`, `KESİŞİMNOKTA` and `ARANOKTA` all wore the plain point; `DİKAYAK`
    // borrowed the ruler that belongs to `ÖLÇÜ` and `ALIM` the locator that
    // belongs to `LİDER`. A card shows labels, but the four that are not the
    // family's face are still picked from a list of icons.
    PointIntersect, ///< two crossing lines with a dot where they meet: KESİŞİMNOKTA
    PointAlong,     ///< a line with dots spaced along it: ARANOKTA
    PerpOffset,     ///< a baseline with a right-angle tick out to a dot: DİKAYAK
    Survey,         ///< a station with two rays and a dot on one: ALIM

    // ---- the edit verbs, which shared two marks between five tools -----------
    //
    // `DÖNDÜR`, `ÖLÇEKLE` and `AYNALA` all wore the turning arrow, and `KOPYALA`
    // shared the two-outlines mark with `BLOKEKLE`. Each of the five does a
    // different thing to the same objects, which is exactly when an icon has to
    // say which.
    Scale,        ///< a small square growing into a large one: ÖLÇEKLE
    Mirror,       ///< a shape and its reflection across a dashed axis: AYNALA
    Array,        ///< a grid of small squares: DİZİ
    BlockInsert,  ///< a square dropped onto an insertion cross: BLOKEKLE
    MeasureAngle, ///< two arms and the sweep between them: AÇIÖLÇ, which wore ÖLÇ's own mark
};

/// Renders `glyph` at `size` logical pixels in `colour`, with a checked variant
/// tinted by `accent`.
QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size = 22);

/// The same drawing as a bare pixmap, for a widget that paints its own chrome
/// and wants one glyph inside it rather than a whole `QIcon` with states.
QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr = 1.0);

} // namespace kentos::app
