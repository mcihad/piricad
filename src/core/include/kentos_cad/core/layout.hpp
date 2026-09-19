// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the pafta, as data.
//
// WHAT A LAYOUT IS. A named sheet composition: pages of a given size, and items
// placed on them in PAPER coordinates — a map frame with its own ground extent
// and scale, a title, a scale bar, a north arrow, a legend, a logo, a frame line,
// an attribute table. It is what QGIS calls a print layout and what a surveyor
// calls a pafta, and it is the thing that actually gets signed and filed.
//
// IT LIVES IN THE DRAWING, and that is the decision this file records. A pafta
// is not a setting of the machine it was drawn on: the sheet layout of an
// 18. madde application is part of the submitted work, it is reviewed with the
// drawing, and a colleague who opens the file must see the same sheet. So a
// layout is document state — saved in the file, in the content hash, and undone
// with Ctrl+Z like every other edit. The office's reusable sheets are a separate
// thing (a template library in application state); this is the copy that belongs
// to THIS drawing.
//
// NOT AN ENTITY, for the reason `guide.hpp` gives about guides and more so: a
// layout has no ground geometry, must never appear in a selection, a cull pass,
// an area sum or an export of the drawing's contents. It is furniture — richer
// furniture than a guide, but furniture.
//
// PAPER MICROMETRES, `int32` (model.md R20). 1 µm = 1/1000 mm, so A0's 1189 mm
// is 1 189 000 — four hundred times inside an `int32`. Millimetres alone cannot
// place a hairline frame or a 0.35 mm offset, and a float would make two runs of
// the same export differ in the last bit (CLAUDE.md 2.4's reasoning, applied to
// paper instead of ground).
//
// ONE STRUCT PER ITEM, NOT ONE CLASS PER KIND. The document model holds values:
// copyable, comparable, hashable, serialisable, with no virtuals and no
// ownership. A `LayoutItem` therefore carries the fields of every kind and each
// field says which kind reads it. The alternative — a polymorphic item hierarchy
// like QGIS's — would put lifetime and dispatch into the one place this program
// keeps flat (model.md, and `core` links nothing).
#pragma once

#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// One ISO 216 A-series paper size, portrait, in whole millimetres.
///
/// IT LIVES IN `core` AND NOT IN `io`, where it started. A4 being 210 × 297 is a
/// geometric fact, not a file format and not a regulation: the print profiles
/// need it, the layout commands need it, and a second copy of the table in the
/// command layer would be a second answer to "how big is A3" (CLAUDE.md 5.10).
std::optional<std::pair<std::int64_t, std::int64_t>> paper_size_mm(std::string_view paper);

/// The paper names, in the order a list shows them, `ozel` last.
std::span<const char* const> paper_names();

/// The name as the table spells it — `a3` becomes `A3`, `custom` becomes `ozel`
/// — or the input unchanged when the table does not know it.
std::string canonical_paper(std::string_view paper);

/// Whether the name means a size the user gives rather than one the table has.
bool is_custom_paper(std::string_view paper);

/// Paper micrometres: 1 µm = 1/1000 mm (model.md R20).
using Um = std::int32_t;

/// Millimetres of paper as micrometres, for a caller writing a literal.
constexpr Um um_from_mm(std::int64_t mm) noexcept
{
    return static_cast<Um>(mm * 1000);
}

/// A rectangle on the page, in paper micrometres from the page's TOP-LEFT.
///
/// TOP-LEFT AND DOWNWARD, unlike everything else in this program, and it is
/// deliberate: paper is read from the top, every page description format from
/// PDF to DXF's paper space places sheet furniture that way, and a user dragging
/// a title block thinks in "20 mm from the top". Ground coordinates stay
/// `Sağa (Y)` / `Yukarı (X)` and upward (model.md R37a); the two frames meet
/// only inside a map item, which is where the flip belongs.
struct PaperRect
{
    Um x{0}; ///< from the left edge
    Um y{0}; ///< from the TOP edge, downward
    Um w{0}; ///< width
    Um h{0}; ///< height

    /// The right edge: `x + w`.
    constexpr Um right() const noexcept { return x + w; }

    /// The bottom edge: `y + h`, further DOWN the page.
    constexpr Um bottom() const noexcept { return y + h; }

    /// Whether it has no area, which is what an unplaced item has.
    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    /// Whether the point is inside, edges included. What a hit test asks.
    constexpr bool contains(Um px, Um py) const noexcept
    {
        return px >= x && px <= right() && py >= y && py <= bottom();
    }

    friend bool operator==(const PaperRect&, const PaperRect&) = default;
};

/// What an item IS. The pafta set, and nothing speculative.
enum class LayoutItemKind : std::uint8_t {
    Map,        ///< a window onto the drawing, with its own extent, scale and grid
    Label,      ///< text: the title, the ada/parsel line, a note, a date
    ScaleBar,   ///< the bar a reader measures with
    NorthArrow, ///< which way north is
    Legend,     ///< which gösterim means what
    Picture,    ///< a logo or a scanned stamp, by path
    Shape,      ///< a rectangle, an ellipse or a line — frames and rules
    Table,      ///< rows of a layer's attributes
};

/// The kind's stable wire word: the file, the command and the journal all use it.
const char* layout_item_kind_id(LayoutItemKind kind) noexcept;

/// Turkish for a kind, for a menu and an item tree.
const char* layout_item_kind_label(LayoutItemKind kind) noexcept;

/// The kind of that id, or nothing when this build does not know it.
std::optional<LayoutItemKind> layout_item_kind_from_id(std::string_view id) noexcept;

/// Which shape a `Shape` item draws.
enum class LayoutShape : std::uint8_t {
    Rectangle,
    Ellipse,
    Line,
};

/// How a map item draws its coordinate grid.
///
/// A PAFTA WITHOUT A GRID IS A PICTURE. BÖHHBÜY output is read by putting a
/// coordinate on it, so the grid is part of the map item rather than a decoration
/// somebody remembers to add.
enum class GridStyle : std::uint8_t {
    None,  ///< no grid
    Cross, ///< a small cross where the lines would meet — the cadastral habit
    Line,  ///< full lines across the frame
    Tick,  ///< ticks on the frame's inside edge only
};

/// Where a grid's coordinate numbers are written.
enum class GridLabels : std::uint8_t {
    None,
    Outside, ///< in the margin around the frame, the usual place
    Inside,  ///< inside the frame, for a map that fills the sheet
};

/// One item on a page.
///
/// EVERY FIELD SAYS WHICH KIND READS IT. A kind that does not read a field
/// leaves it at its default, and a reader that finds one set anyway ignores it —
/// which is what keeps an added kind from being a format break (model.md 0.2a:
/// shapes may gain fields, they may not change meaning).
struct LayoutItem
{
    /// The item's name inside its layout, unique under Turkish folding. A
    /// COMMAND NAMES AN ITEM BY THIS, not by an index: an index changes when
    /// something before it is deleted, and a journal replayed six months later
    /// would move the wrong box (Article 1.4).
    std::string id;

    LayoutItemKind kind{LayoutItemKind::Label};
    PaperRect frame{};

    /// Paint order, low to high. Not the array index: a user raising an item
    /// must not renumber every command that names another one.
    std::int32_t z{0};

    /// Whether the designer refuses to move or resize it. A title block that has
    /// been placed is usually locked so a drag on the map does not take it along.
    bool locked{false};

    /// Rotation in micro-degrees, clockwise on the page. Whole-item, around its
    /// own centre.
    std::int32_t rotation_udeg{0};

    // ---- frame and ground, drawn by every kind ------------------------------

    bool frame_visible{false};                   ///< draw an outline around the item
    Um frame_width{um_from_mm(0)};               ///< that outline's width; 0 = a hairline
    std::uint32_t frame_colour{0xFF000000};      ///< AARRGGBB
    bool background{false};                      ///< fill behind the item
    std::uint32_t background_colour{0xFFFFFFFF}; ///< the fill's colour, AARRGGBB

    // ---- Label, and the caption of ScaleBar / Legend / Table ----------------

    /// `Label`: the text, which may carry the placeholders `<ada>`, `<parsel>`,
    /// `<olcek>`, `<tarih>`, `<pafta>`, `<crs>` — resolved when the sheet is
    /// drawn, never stored resolved, so a re-export after an edit says the truth.
    /// `Picture`: the file path. `Table`: the layer name. `Legend`/`ScaleBar`:
    /// the caption above it.
    std::string text;

    Um text_height{um_from_mm(3)}; ///< cap height on paper
    std::uint32_t text_colour{0xFF000000};

    /// 0 left / 1 centre / 2 right, and 0 top / 1 middle / 2 bottom.
    std::uint8_t align_h{0};
    std::uint8_t align_v{0};

    // ---- Map ----------------------------------------------------------------

    /// The ground window this frame shows, in `Mm`. Empty means "not aimed yet":
    /// the designer then shows the drawing's extent and says so.
    Box2 extent{};

    /// The denominator of 1:N. 0 means the scale FOLLOWS the extent and the
    /// frame; a non-zero value pins it and the extent is recomputed about its
    /// centre — which is what a pafta at a declared scale needs.
    std::int64_t scale{0};

    GridStyle grid{GridStyle::None};
    GridLabels grid_labels{GridLabels::Outside};
    Mm grid_interval{0}; ///< ground millimetres between lines; 0 = chosen for the scale
    Um grid_width{0};    ///< the grid line's width on paper; 0 = a hairline
    std::uint32_t grid_colour{0xFF000000};
    Um grid_text_height{um_from_mm(2)};

    /// Which layers this frame draws. EMPTY MEANS ALL VISIBLE LAYERS, which is
    /// what a first map item wants; naming layers is how a second frame shows a
    /// different theme of the same ground.
    std::vector<std::string> layers;

    // ---- ScaleBar / NorthArrow ----------------------------------------------

    /// `ScaleBar`: how many segments. `NorthArrow`: which of the drawn arrows.
    std::int32_t style{0};

    // ---- Shape ---------------------------------------------------------------

    LayoutShape shape{LayoutShape::Rectangle};

    // ---- Table ---------------------------------------------------------------

    /// The attribute columns to print, in order. Empty means every column.
    std::vector<std::string> columns;

    std::int32_t row_limit{0}; ///< 0 = as many as fit

    friend bool operator==(const LayoutItem&, const LayoutItem&) = default;
};

/// One page of a layout, already oriented: `w` is what the sheet is wide.
struct LayoutPage
{
    Um w{um_from_mm(210)}; ///< how wide the sheet is, as used
    Um h{um_from_mm(297)}; ///< how tall it is

    friend bool operator==(const LayoutPage&, const LayoutPage&) = default;
};

/// A named sheet composition.
struct Layout
{
    /// What the user calls it, unique under Turkish folding. It is also the name
    /// the print profile list shows and the name `YAZDIR` takes.
    std::string name;

    /// At least one. A pafta series is several pages of one size; a report is a
    /// map page and a table page.
    std::vector<LayoutPage> pages{LayoutPage{}};

    /// Placed items, in no particular order; `z` decides what covers what.
    std::vector<LayoutItem> items;

    /// Which page each item sits on, parallel to `items`. A separate column
    /// rather than a field on the item because the overwhelmingly common layout
    /// is one page, and a column of zeroes costs nothing to write or read.
    std::vector<std::int32_t> item_pages;

    std::int32_t dpi{300};     ///< export resolution
    Um margin{um_from_mm(10)}; ///< the guide the designer draws; not a clip

    /// The profile this layout's page size came from, when it came from one.
    /// Kept so the designer can say `A3 yatay` rather than `420 × 297 mm`, and
    /// so a profile renamed elsewhere does not silently change a saved sheet.
    std::string paper;
    bool landscape{false};

    /// The item of that id, or null.
    const LayoutItem* find(std::string_view id) const;
    LayoutItem* find(std::string_view id);

    /// The FIRST map item, which is the one a print frame aims and the one the
    /// scale bar and the north arrow follow. Null when the layout has none.
    const LayoutItem* first_map() const;

    /// Which page `items[i]` is on, 0 when the column is short (an older file).
    std::int32_t page_of(std::size_t i) const noexcept
    {
        return i < item_pages.size() ? item_pages[i] : 0;
    }

    friend bool operator==(const Layout&, const Layout&) = default;
};

/// Every layout a drawing has.
///
/// A WHOLE-LIST STORE, like `GuideStore` and for the same reason: a drawing has
/// a handful of layouts with a few dozen items between them, so the undo record
/// is the previous list rather than a per-item delta whose indices go stale the
/// moment something before them is removed (`Op::Kind::SetGuides` says this at
/// length).
class LayoutStore
{
public:
    /// How many sheets the drawing has.
    std::size_t size() const noexcept { return layouts_.size(); }

    bool empty() const noexcept { return layouts_.empty(); }

    /// Every sheet, in the order they were added.
    const std::vector<Layout>& all() const noexcept { return layouts_; }

    /// The layout of that name, Turkish-folded, or null.
    const Layout* find(std::string_view name) const;
    Layout* find(std::string_view name);

    /// Adds `layout`, or replaces the one of its name in place. Refuses an empty
    /// name, a layout with no page, and a page with no area.
    Status upsert(Layout layout);

    /// Removes the layout of that name. False when there was none.
    bool remove(std::string_view name);

    /// Replaces the whole list — what undo does.
    void load(std::vector<Layout> layouts) { layouts_ = std::move(layouts); }

    /// Folds into the document's content hash. Order matters, because two
    /// drawings whose layouts differ only in order are two different documents
    /// to a reviewer looking at page 1.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<Layout> layouts_;
};

/// The scale denominator `1 : N` a map item is actually at.
///
/// A DECLARED SCALE WINS. `scale` non-zero is the surveyor saying "this sheet is
/// 1:1000", and the window follows from it; zero means the window was dragged
/// and the scale is whatever that came to. One function, so the map drawn, the
/// scale bar measured and the `<olcek>` placeholder printed can never disagree
/// about which of the two happened.
std::int64_t map_scale(const LayoutItem& item);

/// The ground window a map item shows.
///
/// With a declared scale this is the frame's own paper size multiplied by it,
/// centred on the stored extent's centre — which is why changing the paper of a
/// 1:1000 pafta shows MORE ground rather than the same ground smaller. With no
/// declared scale it is the stored extent, widened to the frame's aspect so the
/// picture is not stretched.
Box2 map_window(const LayoutItem& item);

/// A layout for `paper` at `width` × `height` paper micrometres, with one page,
/// a map frame inside the margin, a title at the top and a scale bar under the
/// map: the sheet a new layout starts as, so `Yeni pafta` produces something
/// printable rather than an empty page.
Layout default_layout(std::string name, Um width, Um height, Um margin);

} // namespace kentos::core
