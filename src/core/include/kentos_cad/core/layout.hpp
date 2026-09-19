// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the output layout, as data.
//
// WHAT A LAYOUT IS. A named sheet composition: pages of a given size, and items
// placed on them in PAPER coordinates — a map frame with its own ground extent
// and scale, a title, a scale bar, a north arrow, a legend, a logo, a frame line,
// an attribute table. It is what QGIS calls a print layout and what a surveyor
// calls a pafta — a plan sheet — and it is the thing that gets signed and filed.
//
// THE PROGRAM CALLS IT AN `ÇIKTI YERLEŞİMİ`, an output layout, and not a pafta.
// In this country `pafta` also names a SHEET OF A SUBDIVIDED MAP — the unit a
// cadastral archive is indexed by — and a program about cadastre must not use
// one word for both. The English identifiers stay `Layout`.
//
// IT LIVES IN THE DRAWING, and that is the decision this file records. A layout
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

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Read to resolve an atlas's targets; never written here.
class Document;

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

/// What an item IS. What a sheet needs, and nothing speculative.
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
/// A LAYOUT'S OWN IDENTITY FOR ITS PAGES AND ITS ITEMS.
///
/// Scoped so a page key cannot be passed where an item key is expected. Minted
/// from the layout's own counter rather than the document's: a layout travels
/// in a template file with no document around it, so an identity that depended
/// on one would not survive the trip.
///
/// SESSION IDENTITY, NOT STORED IDENTITY. The file names things by name, which
/// is what makes a saved layout readable and diffable; keys are minted when it
/// is read and are an allocation detail — exactly what `Document::content_hash`
/// says about `EntityKey` ("two documents built the same way from the same input
/// must agree"). So they are in neither the fold nor the equality.
///
/// What they buy is that a rename does not break a link: `ÇIKTIÖĞE islem=ad`
/// changes what an item is called and every reference to it still finds it.
enum class LayoutItemKey : std::uint64_t { None = 0 };
enum class LayoutPageKey : std::uint64_t { None = 0 };

constexpr std::uint64_t raw(LayoutItemKey k) noexcept
{
    return static_cast<std::uint64_t>(k);
}

/// The same, for a page key.
constexpr std::uint64_t raw(LayoutPageKey k) noexcept
{
    return static_cast<std::uint64_t>(k);
}

struct LayoutItem
{
    // FIELDS ARE ORDERED BY WIDTH, NOT BY MEANING. Grouped the readable way —
    // frame, then Label, then Map, then Table — the eight-byte handles and the
    // one-byte enums interleave and the struct spends 39 bytes on padding for
    // every item on every page. Each field still says which part of the item it
    // belongs to, and `operator==` is defaulted, so the order is a layout
    // decision and nothing else depends on it.

    // ---- eight-byte handles and quantities ----------------------------------

    /// The item's name inside its layout, unique under Turkish folding. A
    /// COMMAND NAMES AN ITEM BY THIS, not by an index: an index changes when
    /// something before it is deleted, and a journal replayed six months later
    /// would move the wrong box (Article 1.4).
    std::string id;

    /// `Label`: the text, which may carry the placeholders `<ada>`, `<parsel>`,
    /// `<olcek>`, `<tarih>`, `<yerlesim>`, `<crs>` — resolved when the sheet is
    /// drawn, never stored resolved, so a re-export after an edit says the truth.
    /// `Picture`: the file path. `Table`: the layer name. `Legend`/`ScaleBar`:
    /// the caption above it.
    std::string text;

    /// Map: which layers this frame draws. EMPTY MEANS ALL VISIBLE LAYERS, which
    /// is what a first map item wants; naming layers is how a second frame shows
    /// a different theme of the same ground.
    std::vector<std::string> layers;

    /// Table: the attribute columns to print, in order. Empty means every column.
    std::vector<std::string> columns;

    /// WHICH MAP FRAME THIS ITEM BELONGS TO, by that item's own id.
    ///
    /// A scale bar states a map's scale, a north arrow its rotation, a legend the
    /// symbols it draws and a `<olcek>` placeholder its denominator — and on a
    /// sheet with two map frames at two scales, "the map" is not a question the
    /// program may answer by taking the first one it finds. Empty means the
    /// first map, which is right for the one-map sheet and is what every layout
    /// written before this field says.
    ///
    /// A name that no longer resolves is NOT quietly turned back into the first
    /// map: an item whose link was deleted is reported, because a scale bar
    /// silently restating a different map's scale is a wrong number on a legal
    /// document (`layout_trouble`).
    ///
    /// STORED AS A NAME AND RESOLVED TO A KEY. The file says the name, which is
    /// what keeps a saved layout readable; `Layout::relink` turns it into `linked`
    /// when the layout is loaded, and a rename rewrites the name from the key. So
    /// renaming a map frame does not orphan the things that point at it.
    std::string linked_map;

    /// The map frame this item follows, by identity. `None` means "the first map",
    /// the same as an empty `linked_map`.
    ///
    /// Not part of equality or of the fold, for the reason the type says: it is an
    /// allocation detail, and `linked_map` is the content.
    LayoutItemKey linked{LayoutItemKey::None};

    /// This item's identity inside its layout. Not part of equality.
    LayoutItemKey key{LayoutItemKey::None};

    /// Map: the ground window this frame shows, in `Mm`. Empty means "not aimed
    /// yet": the designer then shows the drawing's extent and says so.
    Box2 extent{};

    /// Map: the denominator of 1:N. 0 means the scale FOLLOWS the extent and the
    /// frame; a non-zero value pins it and the extent is recomputed about its
    /// centre — which is what a sheet at a declared scale needs.
    std::int64_t scale{0};

    /// Map: ground millimetres between grid lines; 0 = chosen for the scale.
    Mm grid_interval{0};

    // ---- four-byte geometry, colour and count -------------------------------

    /// Where the item sits on the page, from the page's top-left corner.
    PaperRect frame{};

    /// Paint order, low to high. Not the array index: a user raising an item
    /// must not renumber every command that names another one.
    std::int32_t z{0};

    /// Rotation in micro-degrees, clockwise on the page. Whole-item, around its
    /// own centre.
    std::int32_t rotation_udeg{0};

    Um frame_width{um_from_mm(0)};               ///< the outline's width; 0 = a hairline
    std::uint32_t frame_colour{0xFF000000};      ///< AARRGGBB
    std::uint32_t background_colour{0xFFFFFFFF}; ///< the fill's colour, AARRGGBB

    Um text_height{um_from_mm(3)}; ///< cap height on paper
    std::uint32_t text_colour{0xFF000000};

    /// Map: the grid line's width on paper; 0 = a hairline.
    Um grid_width{0};
    /// Map: the grid's colour, AARRGGBB.
    std::uint32_t grid_colour{0xFF000000};
    /// Map: cap height of the grid's coordinate labels, on paper.
    Um grid_text_height{um_from_mm(2)};

    /// `ScaleBar`: how many segments. `NorthArrow`: which of the drawn arrows.
    std::int32_t style{0};

    std::int32_t row_limit{0}; ///< Table: 0 = as many rows as fit

    // ---- one-byte kinds and switches ----------------------------------------

    LayoutItemKind kind{LayoutItemKind::Label};

    /// Whether the designer refuses to move or resize it. A title block that has
    /// been placed is usually locked so a drag on the map does not take it along.
    bool locked{false};

    bool frame_visible{false}; ///< draw an outline around the item
    bool background{false};    ///< fill behind the item

    /// 0 left / 1 centre / 2 right, and 0 top / 1 middle / 2 bottom.
    std::uint8_t align_h{0};
    std::uint8_t align_v{0};

    GridStyle grid{GridStyle::None};
    GridLabels grid_labels{GridLabels::Outside};

    LayoutShape shape{LayoutShape::Rectangle};

    /// KEYS ARE EXCLUDED, deliberately. Two items built the same way from the
    /// same input must compare equal — the same sentence `Document::content_hash`
    /// uses about `EntityKey` — or a saved and reloaded layout would stop being
    /// the layout that was saved.
    friend bool operator==(const LayoutItem& a, const LayoutItem& b)
    {
        return a.id == b.id && a.kind == b.kind && a.frame == b.frame && a.z == b.z &&
               a.locked == b.locked && a.rotation_udeg == b.rotation_udeg &&
               a.frame_visible == b.frame_visible && a.frame_width == b.frame_width &&
               a.frame_colour == b.frame_colour && a.background == b.background &&
               a.background_colour == b.background_colour && a.text == b.text &&
               a.text_height == b.text_height && a.text_colour == b.text_colour &&
               a.align_h == b.align_h && a.align_v == b.align_v && a.extent == b.extent &&
               a.scale == b.scale && a.grid == b.grid && a.grid_labels == b.grid_labels &&
               a.grid_interval == b.grid_interval && a.grid_width == b.grid_width &&
               a.grid_colour == b.grid_colour && a.grid_text_height == b.grid_text_height &&
               a.layers == b.layers && a.style == b.style && a.shape == b.shape &&
               a.columns == b.columns && a.row_limit == b.row_limit && a.linked_map == b.linked_map;
    }
};

/// One page of a layout, already oriented: `w` is what the sheet is wide.
struct LayoutPage
{
    Um w{um_from_mm(210)}; ///< how wide the sheet is, as used
    Um h{um_from_mm(297)}; ///< how tall it is

    /// This page's identity inside its layout. Not part of equality: a key is an
    /// allocation detail and two pages of the same size ARE the same page as far
    /// as the drawing is concerned.
    LayoutPageKey key{LayoutPageKey::None};

    friend bool operator==(const LayoutPage& a, const LayoutPage& b)
    {
        return a.w == b.w && a.h == b.h;
    }
};

/// ONE SHEET, PRINTED ONCE PER OBJECT.
///
/// The thing a cadastral office actually asks for: a hundred parcels, a hundred
/// sheets, each aimed at its own parcel and named after it. Without it the answer
/// is "aim the map, print, aim it again" a hundred times, which is how a day
/// disappears and how one of the hundred comes out aimed at the previous one.
///
/// Empty `coverage_layer` means the layout is not an atlas, which is what every
/// layout written before this field says.
struct Atlas
{
    /// Which layer's objects are walked. Empty = this is not an atlas.
    std::string coverage_layer;

    /// The attribute the objects are ordered by, and the one their sheets are
    /// named after. Empty = the order the drawing holds them in, which is stable
    /// but not meaningful to a person.
    ///
    /// ORDER MATTERS BEYOND TIDINESS: a hundred sheets have to come out the same
    /// way twice, or a reprint of sheet 47 is a different parcel (TODOS L-10:
    /// "sıralama ve dosya adları deterministik").
    std::string sort_by;

    /// How much room to leave around the object, as a percentage of its size.
    /// Zero puts the boundary exactly on the frame's edge, which prints a parcel
    /// touching the neatline.
    std::uint8_t margin_percent{10};

    /// Whether the run produces one document of many pages or one file per
    /// object. Both are asked for: a single PDF is what gets mailed, and separate
    /// files are what get filed against parcel numbers.
    bool single_file{true};

    friend bool operator==(const Atlas&, const Atlas&) = default;
};

/// One object an atlas will print, resolved before anything is drawn.
struct AtlasTarget
{
    EntityKey key{EntityKey::None}; ///< which object, persistently
    std::string name;               ///< what its sheet is called
    Box2 bounds;                    ///< the object's own extent, before any margin
};

/// A named sheet composition.
struct Layout
{
    /// What the user calls it, unique under Turkish folding. It is also the name
    /// the print profile list shows and the name `YAZDIR` takes.
    std::string name;

    /// At least one. A sheet series is several pages of one size; a report is a
    /// map page and a table page.
    std::vector<LayoutPage> pages{LayoutPage{}};

    /// Placed items, in no particular order; `z` decides what covers what.
    std::vector<LayoutItem> items;

    /// Which page each item sits on, parallel to `items`. A separate column
    /// rather than a field on the item because the overwhelmingly common layout
    /// is one page, and a column of zeroes costs nothing to write or read.
    std::vector<std::int32_t> item_pages;

    /// What this sheet repeats over, when it repeats. Default-constructed — an
    /// empty coverage layer — means it does not.
    Atlas atlas;

    /// The next key this layout will hand out. Its own counter, because a layout
    /// travels in a template with no document around it.
    ///
    /// Not part of equality or of the fold: an allocation detail.
    std::uint64_t next_key{1};

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

    /// The map frame `item` belongs to: the one it names, or the first one when
    /// it names none. Null when it names one that is not there — which is a
    /// defect to report, never a reason to fall back.
    const LayoutItem* map_for(const LayoutItem& item) const;

    /// Whether `item` names a map frame that is not on this layout.
    bool link_is_broken(const LayoutItem& item) const;

    /// The item with this key, or null.
    LayoutItem* find_key(LayoutItemKey key);

    /// The item with this key, or null.
    const LayoutItem* find_key(LayoutItemKey key) const;

    /// MINTS WHAT IS MISSING AND MAKES THE TWO HALVES OF A LINK AGREE.
    ///
    /// Called after a layout arrives from anywhere that speaks names — a file, a
    /// template, a JSON script — and after any edit that could have left the two
    /// out of step. It gives every page and item a key if it has none, resolves
    /// `linked_map` to `linked`, and writes `linked_map` back from `linked` so a
    /// renamed target is named by its new name.
    ///
    /// `linked` WINS when both are set and they disagree, because the key is the
    /// one that survived the rename.
    void relink();

    /// Renames an item and leaves every link to it pointing at the same item.
    /// False when there is no such item or the new name is taken.
    bool rename_item(std::string_view from, std::string to);

    /// Which page `items[i]` is on, 0 when the column is short (an older file).
    std::int32_t page_of(std::size_t i) const noexcept
    {
        return i < item_pages.size() ? item_pages[i] : 0;
    }

    /// `next_key` is excluded for the reason the keys are: two layouts built the
    /// same way from the same input must compare equal.
    friend bool operator==(const Layout& a, const Layout& b)
    {
        return a.name == b.name && a.pages == b.pages && a.items == b.items &&
               a.item_pages == b.item_pages && a.dpi == b.dpi && a.margin == b.margin &&
               a.paper == b.paper && a.landscape == b.landscape && a.atlas == b.atlas;
    }
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
    void load(std::vector<Layout> layouts)
    {
        layouts_ = std::move(layouts);
        // THE FILE SPEAKS NAMES; memory speaks keys. Settling it here means the
        // reader does not have to know about identity at all.
        for (Layout& one : layouts_)
            one.relink();
    }

    /// Folds into the document's content hash. Order matters, because two
    /// drawings whose layouts differ only in order are two different documents
    /// to a reviewer looking at page 1.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<Layout> layouts_;
};

/// A layout as JSON text — the form a TEMPLATE is stored in.
///
/// WHY JSON AND NOT THE DOCUMENT'S OWN BLOCKS. A template lives outside any
/// drawing, in the office's own file, and is edited by hand more often than
/// anybody admits — a firm's standard sheet is copied between machines, put in
/// version control and patched when the title block changes. The document's
/// binary blocks are right for a file the program writes and reads a thousand
/// times; a template is written once and read by people (`io.md` P5's rule about
/// a version field binds it all the same, and the object carries one).
///
/// THE GROUND EXTENT IS DELIBERATELY NOT WRITTEN. A template says how a sheet is
/// ARRANGED, not where it looks: carrying one drawing's coordinates into another
/// drawing's layout is how a template for Ankara aims a sheet at Ankara in a file
/// about Trabzon.
std::string layout_to_json(const Layout& layout, std::string_view name);

/// A layout parsed back from that text, named `name` rather than whatever the
/// file said — the caller decides what the new sheet is called.
Result<Layout> layout_from_json(std::string_view text, std::string name);

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
/// 1:1000 sheet shows MORE ground rather than the same ground smaller. With no
/// declared scale it is the stored extent, widened to the frame's aspect so the
/// picture is not stretched.
Box2 map_window(const LayoutItem& item);

/// A layout for `paper` at `width` × `height` paper micrometres, with one page,
/// a map frame inside the margin, a title at the top and a scale bar under the
/// map: the sheet a new layout starts as, so a new layout produces something
/// printable rather than an empty page.
/// A fresh item of `kind`, with everything but its name and its box.
///
/// ONE ANSWER, because there were two: `default_layout` seeded a sheet's four
/// items with its own choices and `ÇIKTIÖĞE islem=ekle` made its own, so a table
/// or a legend added by hand came out with NO BACKGROUND — and over a map that
/// means the grid lines and the parcel boundaries run straight through the rows.
/// A printed attribute table is opaque; so is a legend box.
LayoutItem default_item(LayoutItemKind kind);

/// WHAT THIS SHEET CANNOT HONOUR, in Turkish, one sentence each.
///
/// Read before an export rather than after it. A sheet with a map frame nobody
/// aimed prints an empty box; one with an item hanging off the page prints a
/// cut-off box; one with a broken map link prints a scale bar stating nothing.
/// None of these fails — the file appears and looks finished — which is exactly
/// why they have to be said out loud (TODOS L-15).
///
/// Qt-free and filesystem-free: everything here is answerable from the model, so
/// a script and a headless run get the same report the designer shows. The
/// checks that need a disk — a missing picture, a font that is not installed —
/// belong to `/src/app` and are added to this list there.
std::vector<std::string> layout_trouble(const Layout& layout);

/// THE FILES THIS SHEET NEEDS THAT ARE NOT IN IT.
///
/// A template is mailed between offices, and a sheet whose logo lives at
/// `/Users/ayse/Belgeler/amblem.png` arrives at the next desk as a layout that
/// prints an empty box. Naming the dependencies is what lets the receiving end
/// be told what is missing instead of discovering it on the plot.
///
/// Paths as the items carry them; whether they EXIST is a question for the
/// machine they land on, and `/src/core` has no filesystem (Article 3.2).
std::vector<std::string> layout_dependencies(const Layout& layout);

/// EVERY OBJECT THIS SHEET WILL PRINT, in the order it will print them.
///
/// Resolved before anything is drawn, so a run that would produce nothing says so
/// instead of writing zero files and reporting success, and so the names can be
/// checked for collisions while there is still something to do about it.
///
/// Named from `Atlas::sort_by` when it is set, and from the object's persistent
/// key when it is not — never from a slot, which is reused (model.md R44).
/// A NAME THAT REPEATS GETS A SUFFIX rather than overwriting its twin: two
/// parcels numbered 21 in different ada is an ordinary thing in this country.
std::vector<AtlasTarget> atlas_targets(const Document& document, const Layout& layout);

Layout default_layout(std::string name, Um width, Um height, Um margin);

} // namespace kentos::core
