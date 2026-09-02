// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: appearance and the interned style column.
//
// .claude/model.md R13–R19. The reconciliation between CAD and GIS, stated once:
//
//   **A GIS renderer is a command that writes the style column.
//     Style is never derived at frame time.**
//
// CAD gives every object its own colour and linetype; GIS derives appearance
// from a per-layer renderer over feature attributes. KentOSCad needs both, and the
// way to have both without paying for it every frame is to resolve at commit
// time inside a Transaction and materialise the answer into one u32 per entity.
// The renderer then reads an index; it never evaluates a rule, an expression or
// a cascade.
//
// This is Esri's RuleID column, and Esri ships it at cadastral scale.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/image_store.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// Where a single appearance property gets its value. In memory the cascade is
/// this enum; the wire sentinels (DXF 62 == 256, 370 == -1, "BYLAYER") exist
/// only inside /src/io (R19).
enum class Source : std::uint8_t {
    Explicit = 0, ///< the value in this record
    ByLayer  = 1, ///< take the layer's value
    ByBlock  = 2, ///< take the containing block reference's value
};

/// One resolved appearance. POD, trivially copyable, deduplicated in a table.
/// Carries line AND area symbology: MPYY EK-1 plan `gösterim` is overwhelmingly
/// area symbology, and a line-only record cannot represent an `imar planı` (R18).
struct Appearance
{
    std::uint32_t rgba{0xFF6C7686u}; ///< 0xAARRGGBB, stroke colour
    std::int32_t width_um{0};        ///< PAPER micrometres, 1 µm = 1/1000 mm (R20)
    std::uint16_t dash{0};           ///< index into the dash table, from /data
    std::uint16_t symbol{0};         ///< index into the MPYY/BÖHHBÜY symbol atlas
    std::uint32_t fill_rgba{0};      ///< 0 = no fill
    std::uint16_t hatch{0};          ///< index into the hatch table, from /data
    std::int16_t z_order{0};         ///< MPYY prescribes a draw order

    /// Where each property gets its value. `ByLayer` by default, which is what an
    /// ordinary cadastral entity carries and what makes the default Appearance
    /// intern back to the sentinel at id 0 for free (R13).
    ///
    /// There is no `src_hatch`: hatch is the pattern half of the fill property, so
    /// resolving a fill colour while leaving the pattern unresolved would draw an
    /// `imar lekesi` in the layer's colour with the entity's empty pattern.
    Source src_colour{Source::ByLayer};
    Source src_width{Source::ByLayer};
    Source src_dash{Source::ByLayer};
    Source src_fill{Source::ByLayer};

    friend bool operator==(const Appearance&, const Appearance&) = default;
};

static_assert(sizeof(Appearance) <= 32, "Appearance is interned; keep it small");

/// Index into a Document's StyleTable. Slot 0 always means "inherit everything
/// from my layer", so the overwhelmingly common case costs a zero.
using StyleId = std::uint32_t;

inline constexpr StyleId kByLayerStyle = 0;

/// Deduplicated appearance pool owned by the Document.
///
/// In a real cadastral drawing almost every entity is pure ByLayer or pure
/// ByCatalog, so interning collapses millions of entities onto a handful of
/// distinct values. That is what keeps the batch key `(layer, style, kind)`
/// bounded and the draw-call count under the §10.3 target of 100 per frame.
/// The unit a symbol's size is expressed in.
///
/// QGIS carries a unit on nearly every size property and it is not decoration; in
/// Turkish planning work the distinction decides what the sheet says. A boundary
/// width is PAPER: MPYY prescribes 0,5 mm on the plot and it stays 0,5 mm whether
/// the sheet is 1/1000 or 1/5000. A hatch spacing for an `imar lekesi` is often
/// GROUND: the pattern belongs to the area, and letting it shrink with the plot
/// scale turns a legible texture into a grey wash.
enum class Unit : std::uint8_t {
    Paper = 0, ///< micrometres on the printed sheet, 1 µm = 1/1000 mm (R20)
    Ground,    ///< millimetres on the ground — scales with the drawing
    Pixel,     ///< screen pixels; never plotted at a fixed size
};

/// Stable machine name, for a file, a message or a test.
const char* unit_name(Unit u) noexcept;

/// A size and the unit it is measured in.
///
/// Integer, like every stored length in this program (CLAUDE.md 2.4): a symbol
/// size is written into a file, compared in a golden fixture and hashed into the
/// document fingerprint, so it obeys the same rule a coordinate does.
struct Measure
{
    std::int32_t value{0};  ///< in `unit`
    Unit unit{Unit::Paper}; ///< what `value` counts

    /// Whether anything was declared. Zero is a legal offset but not a legal
    /// size, so each caller decides what an unset measure means for its property.
    bool empty() const noexcept { return value == 0; }

    friend bool operator==(const Measure&, const Measure&) = default;
};

/// What a symbol layer draws — QGIS calls this the symbol layer type.
///
/// The list is the subset of QGIS's that MPYY EK-1 actually needs, and each entry
/// earns its place from a gösterim in /data: a `demiryolu` is a HashLine, an
/// `orman alanı` is a PointPatternFill, a `sit alanı` boundary is a MarkerLine,
/// a `tarım alanı` is a LinePatternFill. What is NOT here is what QGIS has and
/// this product has no use for yet — gradient and shapeburst fills, vector field
/// markers, animated markers — and what needs an asset pipeline that has not
/// landed: the raster fills and markers that carry MPYY's 451 görsel images.
enum class SymbolLayerType : std::uint8_t {
    SimpleLine = 0,   ///< a stroke: colour, width, dash, cap, join, offset
    MarkerLine,       ///< a glyph repeated along the line
    HashLine,         ///< short ticks across the line — demiryolu, şev
    SimpleFill,       ///< a solid or hatched interior
    LinePatternFill,  ///< parallel lines at an angle
    PointPatternFill, ///< a grid of glyphs
    CentroidFill,     ///< one glyph at the centre of the face
    SimpleMarker,     ///< a shape at a point

    /// The three that draw a PICTURE the drawing carries.
    ///
    /// MPYY publishes its symbology as images — a hatch for `orman`, a glyph for
    /// `cami`, a line type for `il sınırı` — and drawing the published picture is
    /// the only faithful answer until each one has a vector definition a harita
    /// mühendisi has signed off (CLAUDE.md 6.11). The bytes travel inside the
    /// document; see `kentos_cad/core/image_store.hpp` for why.
    RasterFill,   ///< the image tiled into the interior — a MPYY tarama
    RasterMarker, ///< the image as a glyph — a MPYY sembol
    RasterLine,   ///< the image repeated along the line — a MPYY çizgi tipi

    /// FIXED text at the centre of the geometry.
    ///
    /// The word that belongs to the SYMBOL rather than to the feature. MPYY's
    /// `yapılaşma koşulu` gösterim is a circle with `TAKS` written above a rule
    /// and `KAKS` below it: those two words are the same on every parcel in
    /// Turkey, so they are part of the symbol. Only the numbers beside them come
    /// from the feature, and those are a label written by `ETİKET`.
    ///
    /// A symbol layer may NOT read an attribute — model.md P29 keeps the frame
    /// path out of the attribute columns — which is exactly why the fixed half
    /// and the data half are two different mechanisms.
    TextMarker,
};

/// Stable machine name, for a file, a message or a test.
const char* symbol_layer_type_name(SymbolLayerType t) noexcept;

/// Whether this type paints the interior of a face.
bool draws_fill(SymbolLayerType t) noexcept;

/// Whether this type paints a line along the geometry.
bool draws_stroke(SymbolLayerType t) noexcept;

/// Whether this type places glyphs.
bool draws_marker(SymbolLayerType t) noexcept;

/// The glyph a marker draws.
///
/// Shapes rather than images, for the same reason CAD has always drawn point
/// symbols this way: a shape is exact at every zoom, plots at any scale, and
/// carries no asset to lose. The MPYY images are a separate mechanism and land
/// with the atlas.
enum class MarkerShape : std::uint8_t {
    Circle = 0,
    Square,
    Triangle,
    Diamond,
    Star,
    Cross,      ///< a plus
    XCross,     ///< a diagonal cross
    Arrow,      ///< a filled head pointing along the line
    HalfCircle, ///< the flat side on the line — used for şev and kıyı
    Pentagon,
    Hexagon,
    Tick, ///< a bare stroke across the line, which is what a hash line places
};

/// Stable machine name, for a file, a message or a test.
const char* marker_shape_name(MarkerShape s) noexcept;

/// Where along a line a marker line puts its glyphs.
enum class MarkerPlacement : std::uint8_t {
    Interval = 0, ///< every `interval` of length
    Vertex,       ///< one on each vertex
    FirstVertex,  ///< one, at the start — an arrow tail
    LastVertex,   ///< one, at the end — an arrow head
    Centre,       ///< one, at the middle of the whole run
};

/// Stable machine name, for a file, a message or a test.
const char* marker_placement_name(MarkerPlacement p) noexcept;

/// How a stroke ends.
enum class LineCap : std::uint8_t {
    Butt = 0,
    Round,
    Square,
};

/// How two stroke segments meet.
enum class LineJoin : std::uint8_t {
    Miter = 0,
    Round,
    Bevel,
};

/// One layer of a symbol.
///
/// This is `Appearance` — the colours and widths — plus everything that makes a
/// STACK meaningful: what this layer draws, where it sits relative to the
/// geometry, and the parameters its type reads.
///
/// The parameters are FLAT rather than polymorphic, and that is a decision about
/// what this type is for. A symbol layer is interned, hashed into the document
/// fingerprint, written to a file record and compared in a golden fixture; a
/// polymorphic hierarchy behind a pointer is none of those things cheaply, and
/// QGIS pays for its class-per-type design with a serialisation layer that reads
/// and writes a property map by string key. A closed set of named fields costs
/// some unused bytes per layer and buys value semantics everywhere else.
///
/// A symbol of one SimpleLine layer is exactly what CAD has always had, which is
/// why `Appearance` stays the resolved single-layer form and this wraps it rather
/// than replacing it — a file format and every golden fixture depend on that
/// record keeping its shape (R18).
struct SymbolLayer
{
    Appearance look{}; ///< the colours and widths this layer draws with

    /// What it draws. The default is the CAD case: one stroke along the geometry.
    SymbolLayerType type{SymbolLayerType::SimpleLine};

    /// Perpendicular offset from the geometry. A road casing is two strokes at
    /// the same offset with different widths; a boundary with an inner hatch band
    /// is a stroke and a fill at different ones.
    Measure offset{};

    /// How far ALONG the line the first marker sits, before the interval starts.
    ///
    /// The one thing a marker line could not say, and MPYY needs it twice on the
    /// first page of its own annex. An ETAPLAMA SINIRI alternates a filled circle
    /// with an open one: two marker lines at the same interval, the second half a
    /// step along. A ÜLKE SINIRI is a heavy bar with a perpendicular tick at each
    /// END of it: two marker lines at the bar's repeat, one at the bar's start and
    /// one at its finish. Without a phase both pairs land on top of each other and
    /// the symbol loses half of what it says.
    ///
    /// Zero is the old behaviour and every drawing written before this reads it,
    /// so nothing that used to draw one way now draws another.
    Measure phase{};

    /// Marker diameter, or the length of a hash tick. Unread by the line and fill
    /// types that place no glyph.
    Measure size{};

    /// Spacing ALONG a line for MarkerLine and HashLine, and the first axis of a
    /// PointPatternFill or LinePatternFill.
    Measure interval{};

    /// The second axis of a PointPatternFill. Zero means square: use `interval`
    /// on both axes, which is what a regular glyph grid wants.
    Measure spacing_y{};

    /// Pattern angle for the fill types, glyph rotation for the marker types, in
    /// MICRO-DEGREES — the same unit every stored angle in this program uses, and
    /// for the same reason (model.md R21).
    std::int32_t angle_udeg{0};

    MarkerShape shape{MarkerShape::Circle};               ///< which glyph
    MarkerPlacement placement{MarkerPlacement::Interval}; ///< where on the line
    LineCap cap{LineCap::Round};                          ///< how a stroke ends
    LineJoin join{LineJoin::Round};                       ///< how segments meet

    /// 0 transparent to 255 opaque, multiplied into this layer's colours. Separate
    /// from the alpha in `look.rgba` so a whole layer can be faded without
    /// rewriting the catalogue colour a regulation prescribes.
    std::uint8_t opacity{255};

    /// Whether this layer is drawn at all.
    ///
    /// A designer needs to switch one layer off and see what the rest look like
    /// without losing it — QGIS puts a checkbox on every symbol layer for exactly
    /// this. A disabled layer is still STORED, still round-trips and still folds
    /// into the fingerprint: it is part of the symbol, it is simply not painted.
    bool enabled{true};

    /// Keeps this layer's own colour when the SYMBOL's colour is set.
    ///
    /// A published gösterim is routinely one colour the user is meant to choose
    /// and one the regulation fixes: a lekesi whose fill a planner picks, with a
    /// black boundary and a black glyph that MPYY prints black and that must stay
    /// black whatever the fill becomes. Without a lock, setting the symbol's
    /// colour repaints the boundary too and the row stops being the published
    /// one. QGIS calls this locking the layer's colours and it is what makes a
    /// whole-symbol colour usable at all.
    bool colour_locked{false};

    /// What a `TextMarker` writes. Empty for every other type.
    ///
    /// The one field on a symbol layer that is not a number, and it is here rather
    /// than in a pool because a symbol layer is compared, hashed and written whole;
    /// a handle into a side table would make two symbols equal that draw different
    /// words whenever the table was rebuilt in another order.
    std::string text;

    /// The picture a raster type draws, as an index into the document's
    /// `ImageStore`. `kNoImage` for every other type.
    ///
    /// Deliberately NOT `Appearance::symbol`, which is a u16 index into the
    /// symbol atlas published in /data — a different thing that resolves against
    /// a package rather than against the document, and conflating them would make
    /// a drawing's appearance depend on what happens to be installed.
    ImageId image{kNoImage};

    friend bool operator==(const SymbolLayer&, const SymbolLayer&) = default;
};

/// An ordered stack of symbol layers, drawn back to front.
///
/// This is the shape QGIS reaches with QgsSymbol and its symbol layer list, and
/// the reason it is needed here is MPYY: a plan `gösterim` is routinely a fill, a
/// boundary stroke of a different colour, and a repeated glyph on top. A single
/// colour-and-width record cannot state one, and 476 of them are sitting in
/// /data waiting to be stated.
///
/// One layer is the overwhelmingly common case and costs one vector element.
/// The renderer walks the stack and emits one pass per layer; it still never
/// evaluates a rule (R14) — the stack was resolved at commit time like everything
/// else in the style column.
struct Symbol
{
    /// Drawn back to front. One layer is the common case and costs one vector
    /// element; a plan gösterim is routinely three.
    std::vector<SymbolLayer> layers;

    /// Below this scale denominator the symbol is not drawn; zero means always.
    /// Scale-dependent symbology is not decoration in planning work: an
    /// `imar planı` at 1/25000 shows a `lekesi` where 1/1000 shows its parcels.
    std::uint32_t min_scale{0};
    std::uint32_t max_scale{0}; ///< above this, not drawn; zero means always

    friend bool operator==(const Symbol&, const Symbol&) = default;

    /// The single-layer symbol a CAD entity has always had.
    static Symbol of(const Appearance& a)
    {
        Symbol s;
        SymbolLayer l;
        l.look = a;
        s.layers.push_back(l);
        return s;
    }

    /// The layer the CAD cascade resolves to — the first stroke, or the first
    /// layer at all. `Appearance` is what a per-entity override and every file
    /// format field talks about, so a stack has to be able to name one.
    const Appearance& primary() const noexcept
    {
        for (const SymbolLayer& l : layers)
            if (draws_stroke(l.type)) return l.look;
        static const Appearance fallback{};
        return layers.empty() ? fallback : layers.front().look;
    }
};

/// Parses a machine name back to its enum.
///
/// Empty on an unknown name, NEVER a default. A symbol layer type nobody
/// recognised is a symbol the user meant and did not get, and defaulting it to a
/// plain line would draw a `demiryolu` as an ordinary boundary — wrong, and
/// wrong quietly. Every caller reports the name it could not place along with
/// the list of names that are valid.
std::optional<Unit> unit_from_name(std::string_view name) noexcept;
std::optional<SymbolLayerType> symbol_layer_type_from_name(std::string_view name) noexcept;
std::optional<MarkerShape> marker_shape_from_name(std::string_view name) noexcept;
std::optional<MarkerPlacement> marker_placement_from_name(std::string_view name) noexcept;
std::optional<LineCap> line_cap_from_name(std::string_view name) noexcept;
std::optional<LineJoin> line_join_from_name(std::string_view name) noexcept;

/// Every valid name for one of the enums above, comma separated, for an error
/// message. Built from the same switch the parser uses, so a name added in one
/// place cannot go missing from the other.
std::string symbol_layer_type_names();
std::string marker_shape_names();
std::string marker_placement_names();
std::string unit_names();

/// Folds a stack into a content hash, layer order included.
std::uint64_t fold_symbol(const Symbol& sym, std::uint64_t seed);

class StyleTable
{
public:
    /// Builds a table whose entry 0 is already the ByLayer sentinel, so an entity
    /// that declares nothing costs no interning at all.
    StyleTable();

    /// Returns the id of `a`, adding it if new. Deterministic: the same sequence
    /// of interns always produces the same ids, because golden fixtures depend on
    /// it (§7.3).
    StyleId intern(const Appearance& a);

    /// Interns a full stack. A single-layer stack and the `Appearance` it wraps
    /// intern to the SAME id, so a drawing that never uses a stack is byte for
    /// byte the drawing it was before stacks existed.
    StyleId intern(const Symbol& sym);

    /// The resolved single-layer appearance — what the CAD cascade and every
    /// file-format field mean by "the style of this entity".
    const Appearance& at(StyleId id) const;

    /// The full stack behind `id`. Entries interned as a bare Appearance report a
    /// one-layer stack, so a caller never has to ask which form was used.
    const Symbol& symbol_at(StyleId id) const;

    bool contains(StyleId id) const noexcept { return id < entries_.size(); }

    std::size_t size() const noexcept { return entries_.size(); }

    const std::vector<Appearance>& entries() const noexcept { return entries_; }

    /// Folds a hash of every entry into `seed`, for `Document::content_hash()`.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    struct Hash
    {
        std::size_t operator()(const Appearance& a) const noexcept;
    };

    std::vector<Appearance> entries_; ///< the resolved look, one per id
    std::vector<Symbol> symbols_;     ///< parallel to entries_, the full stack
    std::unordered_map<Appearance, StyleId, Hash> intern_;

    struct SymbolHash
    {
        std::size_t operator()(const Symbol& s) const noexcept;
    };

    std::unordered_map<Symbol, StyleId, SymbolHash> symbol_intern_;
};

/// Applies the cascade to produce the appearance actually drawn. Called at
/// COMMIT time by the command that changed something, never per frame (R14).
Appearance resolve_appearance(const Appearance& own, const Appearance& layer_default);

} // namespace kentos::core
