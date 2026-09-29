// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: vector tool icons.
//
// Drawn as paths rather than shipped as bitmaps: they stay crisp at any DPI
// (kentoscad.md §13 — high DPI and mixed-DPI multi-monitor are requirements) and
// they re-tint when the theme changes. A designed SVG set replaces this in
// Phase 1; the call site does not change.
#pragma once

#include <string_view>

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
    Colour, ///< a tipped bucket and its drop: RENK, the chips' command
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
    Scale,         ///< a small square growing into a large one: ÖLÇEKLE
    Mirror,        ///< a shape and its reflection across a dashed axis: AYNALA
    Array,         ///< a grid of small squares: DİZİ
    BlockInsert,   ///< a square dropped onto an insertion cross: BLOKEKLE
    BlockEdit,     ///< a block's square with the pencil over it: BLOKDÜZENLE
    BlockBase,     ///< a block's square, its base cross moved to a new corner: BLOKDÜZENLE taban
    Xref,          ///< a page and the dashed square it becomes on the sheet: DIŞREFERANS
    XrefReload,    ///< a dashed square inside a turning arrow: DIŞREFERANS islem=yenile
    XrefLocalCopy, ///< a dashed square and the solid one it is copied to: YERELKOPYA
    BlockClip,     ///< a crop's two brackets over a circle they cut: BLOKKIRP
    BlockClipPolygon,  ///< a dashed polygon over a circle it cuts: BLOKKIRP tur=cokgen
    BlockClipObject,   ///< a drawn ring over a circle it cuts: BLOKKIRP tur=cizgi
    BlockClipBoundary, ///< the crop's frame drawn out as a line: BLOKKIRP islem=sinir
    BlockUnclip,       ///< a whole circle, the crop's brackets struck: BLOKKIRP islem=kaldir
    MeasureAngle, ///< two arms and the sweep between them: AÇIÖLÇ, which wore ÖLÇ's own mark

    // ---- the corner, end and piece tools the column did not have --------------
    //
    // PAH, YUVARLA and UZAT all wore BUDA's scissors, and the seven verbs P3
    // added wore whatever was near: KIR the cut mark, UZUNLUK the ruler, HİZALA
    // the four-way move. They were menu rows then, where a label carries them;
    // in the column only the icon speaks, so each says what it does.
    Chamfer,      ///< a corner cut off by a straight edge: PAH
    Fillet,       ///< a corner rounded off by an arc: YUVARLA
    Extend,       ///< a line reaching on to a boundary: UZAT
    Break,        ///< a line with a piece taken out of it: KIR
    Lengthen,     ///< a line whose end moves along its own direction: UZUNLUK
    Join,         ///< two runs meeting end to end at one dot: UÇUCA
    Explode,      ///< a square coming apart into its edges: PATLAT
    Align,        ///< a shape carried onto a target pair of points: HİZALA
    Divide,       ///< a line with equal ticks along it: BÖLÜMLE
    VertexMove,   ///< a corner pulled to a new place, its edges following: KÖŞETAŞI
    VertexAdd,    ///< an edge bent through a new corner, with a plus: KÖŞEEKLE
    ToArea,       ///< an open run closing into a filled face: ALANAÇEVİR
    PolylineEdit, ///< a run with its direction arrow: ÇİZGİDÜZENLE
    TextEdit,     ///< a letter with the pencil over it: YAZIDÜZENLE
    VertexDelete, ///< a bent edge made straight, its corner struck out: KÖŞESİL
    EdgeKind,     ///< a straight edge bowed into an arc, its chord dashed: KENARTÜRÜ
    Boundary,     ///< crossing lines, the one cell they close filled, a click inside: SINIR
    DimChain,     ///< one dimension line cut into figures end to end: ZİNCİRÖLÇÜ
    DimBaseline,  ///< dimension lines stacked from one origin: BAZÖLÇÜ
    DimEdit,      ///< a dimension with the pencil over its figure: ÖLÇÜDÜZENLE
    Leader,       ///< an arrow, its line and landing, the note above: LİDER (Kılavuz Çizgi)
    // ---- the ribbon's own marks (`.claude/ui.md` R46) -------------------------
    //
    // SIX TOOLS WORE `Function`, the generic f(x) — SPLINE, POLİGON, OTURT,
    // EŞYÜKSELTİ, HACİM and every processing tool — so on a ribbon, where the
    // picture is what the eye finds first, six different acts looked like one.
    Guide,           ///< a construction line through a point, off both edges: KILAVUZ
    Spline,          ///< a smooth curve pulled by its control polygon: SPLINE
    Traverse,        ///< stations joined by legs, the angle measured at one: POLİGON
    Helmert,         ///< a dashed frame set onto a solid one: OTURT
    Contour,         ///< nested level lines: EŞYÜKSELTİ
    Volume,          ///< a ground line over a design level, cut above and fill below: HACİM
    Buffer,          ///< a line inside the dashed band around it: TAMPON
    NumberVertices,  ///< a face with its corners numbered: KÖŞENUMARALA
    LabelLength,     ///< an edge with its length written over it: UZUNLUKYAZ
    Detach,          ///< the paperclip struck through: BAĞÇÖZ
    Cleanup,         ///< a broom and two sparks: TEMİZLE
    DimStyle,        ///< a dimension over a list of styles: ÖLÇÜSTİLİ
    DimRefresh,      ///< a dimension and the arrow that goes round: ÖLÇÜYENİLE
    LayerCurrent,    ///< a sheet with a tick: make the selection's layer active
    LayerMove,       ///< an arrow dropping onto a sheet: move to the active layer
    LayerIsolate,    ///< one bright sheet between two faded ones: isolate
    LayerAdd,        ///< a sheet and a plus: a new layer
    IslandNormal,    ///< nested squares hatched in turn: TARAMADÜZENLE stil=normal
    IslandOuter,     ///< only the outer band hatched: stil=dis
    IslandIgnore,    ///< all of it hatched: stil=yoksay
    CopyBase,        ///< the copy mark with its base point: PANOYAKOPYALA tabanli=evet
    Hatch,           ///< a closed shape with its pattern lines: TARAMA (not `Grid`'s cells)
    AreaSplit,       ///< a parcel cut in two, the new piece's area written on it: ALANİFRAZ
    Label,           ///< a parcel with its label written inside: ETİKET (not `Text`'s T)
    Ortho,           ///< two arrows at a right angle: MOD dik_mod
    SurfaceNormal,   ///< an arrow standing square off a slanted edge: MOD yüzey_normali
    Theme,           ///< a circle half dark, half light: the theme
    Hud,             ///< a frame-time bar chart in a box: the developer overlay
    CommandLine,     ///< a prompt and its cursor: the command line
    Database,        ///< a stack of discs: VERİTABANI, a PostGIS server
    Import,          ///< an arrow dropping into a tray: İÇEAKTAR (not Aç's folder)
    ProjectSettings, ///< a sheet with a gear on it: the project's own settings
    Layout,          ///< a sheet with its map frame and title block: a çıktı yerleşimi
    More,            ///< three dots: the commands with no place of their own
    // ---- one picture per dimension type (TODOS C-17) --------------------------
    DimAligned,   ///< a slanted edge with its dimension line along it: ÖLÇÜ tur=hizali
    DimLinear,    ///< two points at two heights measured straight across: ÖLÇÜ tur=dogrusal
    DimRadius,    ///< a circle, one radius to the rim and R beyond: ÖLÇÜ tur=yaricap
    DimDiameter,  ///< a circle, the diameter through it and Ø: ÖLÇÜ tur=cap
    DimAngular,   ///< two arms and the arrowed arc between them: ÖLÇÜ tur=acisal
    DimOrdinate,  ///< an origin's two axes and a point's jogged line: ÖLÇÜ tur=koordinat
    DimArcLength, ///< an arc, the arc beside it that measures it and ⌒: ÖLÇÜ tur=yay
    // ---- ties between objects (TODOS F-04) ------------------------------------
    Dependency, ///< a source, the arrow to what was made from it, a mark on that: BAĞIMLILIK
    // ---- a method told apart from its family (the ribbon review, 28 Sept 2026) --
    // The user: "ikonu kötü olanlar var". Four circles, five arcs and the
    // rest wore their family's picture, so the arrow's list read as one
    // picture repeated; each method now shows what makes it that method.
    CircleTwoPoint,     ///< a circle and the diameter its two points give: DAİRE yontem=2n
    CircleThreePoint,   ///< a circle through three marked points: DAİRE yontem=3n
    CircleTangent,      ///< a circle in the corner two lines make: DAİRE yontem=ttr
    ArcThreePoint,      ///< an arc through its start, a point on it and its end: YAY yontem=3n
    ArcCentreAngle,     ///< the centre, the start, and the angle swept: YAY yontem=bma
    ArcEndsRadius,      ///< the two ends and the radius from the centre: YAY yontem=bby
    ArcContinue,        ///< a line running on, tangent, into an arc: YAY yontem=devam
    RectangleRotated,   ///< a rectangle turned off the axes: DİKDÖRTGEN yontem=3n
    PolygonOutside,     ///< a hexagon round the circle it touches: ÇOKGEN yontem=dis
    PolygonSide,        ///< a hexagon from one side: ÇOKGEN yontem=kenar
    EllipseAxis,        ///< an ellipse and the axis its two points give: ELİPS yontem=eksen
    AreaDraw,           ///< a parcel, drawn corner by corner: ALAN
    Combine,            ///< two faces become one, the seam gone: BİRLEŞTİR
    Polygonize,         ///< lines that cross, and the faces they close: ALANÜRET
    AreaEdit,           ///< an edge moved out to the area asked for: ALANDÜZENLE
    BlockDefine,        ///< objects gathered into one block at a base point: BLOK
    HatchEdit,          ///< a hatch, and the pencil that changes it: TARAMADÜZENLE
    SaveAs,             ///< the disk, and the pencil that names a new file: FARKLIKAYDET
    ArrayPolar,         ///< copies round a centre: DİZİ mod=KUTUPSAL
    ArrayPath,          ///< copies along a path: DİZİ mod=YOL
    Toolbox,            ///< the processing tools, all of them: İşlem Araçları
    TrimFence,          ///< Buda, with a fence drawn across the pieces
    TrimKeep,           ///< Buda, the piece clicked kept
    TrimCarry,          ///< Buda, the boundaries carried on
    ExtendFence,        ///< Uzat, with a fence
    ExtendCarry,        ///< Uzat, the boundaries carried on
    SplitPoints,        ///< Böl at points on the object
    SplitCrossings,     ///< Böl where the objects cross
    SplitEqual,         ///< Böl into equal parts
    SplitDistance,      ///< Böl at a distance from the start
    RotateReference,    ///< Döndür by a reference angle
    ScaleReference,     ///< Ölçekle by a reference length
    MirrorCopy,         ///< Aynala, the original kept
    FilletAll,          ///< Yuvarla, every corner
    ChamferAll,         ///< Pah, every corner
    MeasureAreaCorners, ///< Alan Ölç, by clicking the corners
    IntersectDistances, ///< Kesişim from two distances
    IntersectLines,     ///< Kesişim of two lines
    AlongDistance,      ///< Ara Nokta at a distance
    GuideHorizontal,    ///< a guide across, off the ruler's top: KILAVUZ yon=yatay
    GuideVertical,      ///< a guide down, off the ruler's side: KILAVUZ yon=düşey
    GuideList,          ///< the guides, listed: KILAVUZ
    ViewPrevious,       ///< the view before this one: YAKINLAŞ ÖNCEKİ
    ViewNext,           ///< the view after, again: YAKINLAŞ SONRAKİ
    ViewWindow,         ///< a window drawn and zoomed into: YAKINLAŞ PENCERE
    ZoomSelection,      ///< the selection framed: YAKINLAŞ SEÇİM
};

/// The inks of a picture, one per ROLE (`Tokens::icon*`, `design.md` §5): what
/// a command draws, what it cuts, what it writes, what it keeps, what it adds,
/// a page, and the ink everything else is drawn in.
struct GlyphInks
{
    QColor ink;   ///< the frame and the tool itself
    QColor shape; ///< what the command draws or changes
    QColor fill;  ///< the inside of a drawn face
    QColor cut;   ///< what it cuts, trims or deletes
    QColor note;  ///< what it writes or measures
    QColor data;  ///< what it keeps
    QColor add;   ///< what it adds or joins
    QColor paper; ///< the face of a page

    /// Every role in `colour` and no fill: the picture as a one-ink icon.
    static GlyphInks mono(const QColor& colour);
};

/// Renders `glyph` at `size` logical pixels in `colour`, with a checked variant
/// tinted by `accent`. One ink: a panel's own buttons and marks.
QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size = 22);

/// An ACTION's picture, in colour — every role in its own ink. The picture keeps
/// its colours in every state, the way the Office ribbon's do: the button's
/// ground says hover and checked, and a disabled picture fades.
QIcon colour_icon(Glyph glyph, const GlyphInks& inks, int size = 32);

/// The same drawing as a bare pixmap, for a widget that paints its own chrome
/// and wants one glyph inside it rather than a whole `QIcon` with states.
QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr = 1.0);

/// THE MARK A PROCESSING TOOL NAMES IN WORDS (`processing::ToolSpec::icon`),
/// as a picture: the Qt-free module never names a glyph, and this is the one
/// place its words become one — for the Araçlar tree and the ribbon alike. A
/// word it does not know gets the generic tool mark rather than nothing.
Glyph glyph_named(std::string_view name);

} // namespace kentos::app
