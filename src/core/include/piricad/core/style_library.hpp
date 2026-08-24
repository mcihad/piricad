// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the browsable symbol library.
//
// WHAT THIS IS, and how it differs from `StyleCatalog` next door.
//
//   `StyleCatalog` is a LOADED DATA PACKAGE: a regulation's rows, its rules, its
//   provenance, and the classification that turns a feature into one of them. It
//   answers "which gösterim does this parcel get?".
//
//   `StyleLibrary` is what a PERSON BROWSES. It answers "show me the ones under
//   SINIRLAR", "which ones mention orman", "what did I mark as a favourite". It
//   holds named symbols in a group tree and knows nothing about classification.
//
// This is QGIS's split between a layer's renderer and the Style Manager, and it
// is worth keeping for the same reason QGIS keeps it: the rules that decide
// appearance and the shelf you pick a symbol off are different objects with
// different lifetimes, and merging them makes a drawing's symbology depend on
// what the user happened to have installed.
//
// THE GROUP TREE IS THE REGULATION'S OWN. MPYY EK-1 already organises its 476
// gösterim by annex — EK-1a ortak, EK-1b MSP, EK-1c ÇDP, EK-1ç NİP, EK-1d UİP —
// and inside each annex by a section path such as SINIRLAR > İDARİ SINIRLAR.
// Nothing here invents a taxonomy: the path arrives in the package and this class
// indexes it (CLAUDE.md 5.13, data.md R1).
#pragma once

#include "piricad/core/dash_store.hpp"
#include "piricad/core/image_store.hpp"
#include "piricad/core/style.hpp"
#include "piricad/core/style_rule.hpp"

#include <functional>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

/// What kind of geometry a shelf entry is drawn on.
///
/// The shelf is browsed by this before anything else, the way QGIS's style
/// manager separates Marker, Line and Fill: a planner looking for a boundary does
/// not want to scroll past four hundred area gösterim to find it.
///
/// Read from what the PACKAGE says about the row — a hatch or a fill colour makes
/// it an area, a published line type makes it a line, a bare glyph makes it a
/// point — never guessed from the symbol after the fact.
enum class SymbolKind : std::uint8_t {
    Area = 0,
    Line,
    Point,
};

/// Stable machine name, for a file, a message or a test.
const char* symbol_kind_name(SymbolKind k) noexcept;

/// One browsable symbol.
///
/// A superset of `StyleEntry`: the same identity and provenance, plus the group
/// path and tags a person navigates by, and a full `Symbol` rather than a single
/// `Appearance` — because a gösterim is routinely a stack and the shelf has to
/// show what will actually be drawn.
struct LibraryEntry
{
    std::string id;                    ///< stable forever; a retired id is never reused (R5)
    std::string label;                 ///< Turkish, what a user reads on the shelf
    std::vector<std::string> group;    ///< hierarchical path, outermost first
    std::vector<std::string> tags;     ///< free labels, searched across groups
    std::string source_ref;            ///< the annex and madde this row encodes
    std::string package_path;          ///< catalogue file that supplied this row
    SymbolKind kind{SymbolKind::Area}; ///< which geometry it belongs to
    Symbol symbol{};                   ///< what it draws
    ScaleWindow scale{};               ///< the scales it applies at
    bool deprecated{false};            ///< retained and still loadable, never dropped (R5)

    /// The package could not read this row's appearance with confidence.
    ///
    /// Fourteen MPYY rows are flagged this way. Carried onto the shelf so every
    /// surface that offers one can say so: a row shown as certain when the package
    /// says otherwise is the program asserting something the annex did not print.
    bool uncertain{false};
    std::vector<std::string> uncertain_reasons; ///< why, verbatim from the package
};

/// Turns a package-relative image path into an id, or `kNoImage`.
///
/// Supplied by the CALLER because core does no I/O (core.md P9). The command layer
/// reads the file and interns it into the document; the shelf reads it and interns
/// it into its own store. Both go through the same builder below, so what a
/// gallery thumbnail shows is what the command will apply.
using ImageResolver = std::function<ImageId(const std::string& package_relative_path)>;

/// Builds the symbol a catalogue row describes.
///
/// The stack is bottom to top and the order is what a plan sheet reads like: the
/// row's fill colour, the hatch the annex printed over it, the row's boundary or
/// its published line type, and the glyph last so nothing covers it.
///
/// ONE implementation, shared by the shelf and by `STİL`. Two would be two answers
/// to "what does this gösterim look like", and the day they differ the thumbnail
/// stops predicting the drawing.
Symbol symbol_of_entry(const StyleEntry& row, const ImageResolver& resolve);

/// A shelf of named symbols, organised by the group path they arrived with.
///
/// Insertion-ordered throughout. Group children, group members and search results
/// all come back in the order the package declared them, because a regulation's
/// annex has an order and a shelf that sorted it alphabetically would put
/// `ÜLKE SINIRI` after `İL SINIRI` and stop matching the printed table.
class StyleLibrary
{
public:
    /// Adds one entry. A repeated id REPLACES the earlier entry rather than
    /// shadowing it: two rows under one id is a package defect, and keeping both
    /// would make `find` depend on which one was asked for.
    void add(LibraryEntry entry);

    /// Loads every row of a catalogue package onto the shelf.
    ///
    /// Returns how many were added. The catalogue keeps its own copy of the rows;
    /// this is a projection of them, so reloading a newer package version replaces
    /// what it declares and leaves anything else alone.
    /// `resolve` supplies the bytes of the pictures a row was published with; pass
    /// an empty function to load a package without them, in which case every row
    /// keeps its colours and loses its hatch.
    std::size_t add_catalog(const StyleCatalog& catalog, const ImageResolver& resolve = {},
                            std::string_view package_path = {});

    /// The pictures the shelf's own symbols draw. Borrowed by a preview.
    const ImageStore& images() const noexcept { return images_; }

    /// The line types the shelf's own symbols draw, for the same reason and
    /// borrowed the same way: a gallery swatch has to show the dash the row
    /// declares, and the row has not reached any document yet.
    const DashStore& dashes() const noexcept { return dashes_; }

    /// Adds a line type to the shelf's store.
    Result<DashId> intern_dash(const DashPattern& pattern, std::string_view origin)
    {
        return dashes_.intern(pattern, origin);
    }

    /// Adds a picture to the shelf's store, for a resolver to hand back an id.
    Result<ImageId> intern_image(std::span<const std::byte> bytes, std::string_view origin)
    {
        return images_.intern(bytes, origin);
    }

    /// How many symbols are on the shelf.
    std::size_t size() const noexcept { return entries_.size(); }

    bool empty() const noexcept { return entries_.empty(); }

    /// Every entry, in shelf order.
    const std::vector<LibraryEntry>& entries() const noexcept { return entries_; }

    /// The entry with this id, or null. Exact, byte-wise: an id is a machine name.
    const LibraryEntry* find(std::string_view id) const;

    /// The child group names directly under `path`, in first-seen order.
    ///
    /// An empty path gives the top level. A path nobody declared gives nothing,
    /// which is the honest answer rather than an error: a shelf can be asked about
    /// a drawer that does not exist.
    std::vector<std::string> children(std::span<const std::string> path) const;

    /// Entries of one kind, in shelf order. The first thing a browser filters by.
    std::vector<const LibraryEntry*> of_kind(SymbolKind kind) const;

    /// Entries whose group is EXACTLY `path` — not its descendants.
    ///
    /// Exact rather than recursive because the tree is how a user narrows down: a
    /// recursive answer at the top level is the whole package, which is the thing
    /// they opened the tree to avoid.
    std::vector<const LibraryEntry*> in_group(std::span<const std::string> path) const;

    /// Entries whose label, id, tags or group path contain `needle`.
    ///
    /// Turkish-folded on both sides, so `orman` finds `ORMAN` and `İl` finds `il`
    /// — which `std::tolower` cannot do and which CLAUDE.md 5.6 bans outright.
    std::vector<const LibraryEntry*> search(std::string_view needle) const;

    /// Marks an entry as a favourite, or clears it. Unknown ids are ignored: a
    /// favourite naming a symbol a newer package retired is not an error, it is a
    /// preference that no longer applies.
    void set_favourite(std::string_view id, bool on);

    bool favourite(std::string_view id) const;

    /// Every favourite, in the order the shelf holds them.
    std::vector<const LibraryEntry*> favourites() const;

    /// Folds the whole shelf, IN ORDER, into a fingerprint.
    ///
    /// Order is part of it because order is what a user sees, and two shelves that
    /// hold the same symbols in a different order are two different shelves.
    std::uint64_t content_hash() const;

private:
    std::vector<LibraryEntry> entries_;
    std::vector<std::uint8_t> favourite_; ///< parallel to entries_
    ImageStore images_{};                 ///< the pictures the shelf's symbols draw
    DashStore dashes_{};                  ///< the line types the shelf's symbols draw
};

} // namespace piricad::core
