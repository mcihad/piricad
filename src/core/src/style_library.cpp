// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/style_library.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>
#include <utility>

namespace piricad::core {
namespace {

constexpr std::uint64_t kLibrarySeed = fnv1a("piricad.core.style_library");

/// Whether `path` is exactly the first `path.size()` names of `group`.
bool group_starts_with(const std::vector<std::string>& group, std::span<const std::string> path)
{
    if (group.size() < path.size()) return false;
    for (std::size_t i = 0; i < path.size(); ++i)
        if (group[i] != path[i]) return false;
    return true;
}

/// Case-insensitive substring, Turkish rules on both sides.
///
/// Folded to upper case rather than lower, because that is the direction
/// `turkish_upper` implements and the direction the dotted/dotless pair is
/// unambiguous in: `i` upper-cases to `İ` and `ı` to `I`, so two words that
/// differ only in the dot stay different (CLAUDE.md 5.6).
bool contains_folded(std::string_view haystack, const std::string& folded_needle)
{
    return turkish_fold_key(haystack).find(folded_needle) != std::string::npos;
}

} // namespace

void StyleLibrary::add(LibraryEntry entry)
{
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id != entry.id) continue;

        // Replaced in place, keeping its position and its favourite mark: a newer
        // package version restating a row is an update to that row, not a new one
        // at the end of the shelf.
        entries_[i] = std::move(entry);
        return;
    }

    entries_.push_back(std::move(entry));
    favourite_.push_back(0);
}

Symbol symbol_of_entry(const StyleEntry& row, const ImageResolver& resolve,
                       const DashResolver& intern_dash)
{
    // A DECLARED stack wins over the pictures. The pictures stay on the row as
    // provenance — they are what the declaration was read from — but a symbol
    // said in numbers is the one that turns a corner, recolours and exports as a
    // line type, and that is the whole reason a row may declare one.
    if (!row.layers.empty()) {
        Symbol declared;
        for (const DeclaredLayer& d : row.layers) {
            SymbolLayer layer = d.layer;
            if (d.dash.count > 0 && intern_dash) layer.look.dash = intern_dash(d.dash, row.id);

            // A `gorsel-*` layer names a file; the id belongs to the document
            // that is about to carry the bytes, so it is minted here and not by
            // the parser. A layer whose picture will not resolve is DROPPED
            // rather than drawn empty: an image layer with no image paints
            // nothing, and a stack silently one layer short is easier to see
            // than a stack with an invisible member in it.
            if (!d.image.empty()) {
                if (!resolve) continue;
                const ImageId image = resolve(d.image);
                if (image == kNoImage) continue;
                layer.image = image;
            }
            declared.layers.push_back(layer);
        }
        if (!declared.layers.empty()) return declared;
    }

    Symbol sym;

    const auto add = [&](const std::string& file, SymbolLayerType type, std::int32_t size_um,
                         std::int32_t gap_um = 0) {
        if (file.empty() || !resolve) return false;

        const ImageId image = resolve(file);
        if (image == kNoImage) return false;

        SymbolLayer layer;
        layer.look  = row.appearance;
        layer.type  = type;
        layer.image = image;

        // A size in PAPER micrometres, because a published symbol is printed at a
        // size the annex fixes and it stays that size whatever the plot scale is.
        // Whoever wants it to follow the ground says so in the designer.
        layer.size = Measure{size_um, Unit::Paper};
        if (gap_um > 0) layer.interval = Measure{gap_um, Unit::Paper};
        sym.layers.push_back(layer);
        return true;
    };

    if (row.appearance.fill_rgba != 0) {
        SymbolLayer base;
        base.look = row.appearance;
        base.type = SymbolLayerType::SimpleFill;
        sym.layers.push_back(base);
    }

    // Sizes chosen for legibility, not from the regulation: the annex prints a
    // picture and states no millimetre for it. They are a starting point the
    // designer changes, never a claim about what MPYY requires — and a row that
    // names its own size overrides them, because a picture somebody DREW has a
    // size that is part of the drawing.
    const auto sized = [](std::int32_t declared, std::int32_t fallback) {
        return declared > 0 ? declared : fallback;
    };

    add(row.image_hatch, SymbolLayerType::RasterFill, sized(row.image_hatch_um, 24000),
        row.image_hatch_gap_um);

    if (!add(row.image_line, SymbolLayerType::RasterLine, sized(row.image_line_um, 8000))) {
        SymbolLayer stroke;
        stroke.look = row.appearance;
        stroke.type = SymbolLayerType::SimpleLine;
        sym.layers.push_back(stroke);
    }

    add(row.image_symbol, SymbolLayerType::RasterMarker, sized(row.image_symbol_um, 12000));

    if (sym.layers.empty()) sym = Symbol::of(row.appearance);
    return sym;
}

std::size_t StyleLibrary::add_catalog(const StyleCatalog& catalog, const ImageResolver& resolve,
                                      std::string_view package_path)
{
    std::size_t added = 0;
    for (const StyleEntry& row : catalog.entries()) {
        LibraryEntry entry;
        entry.id                = row.id;
        entry.label             = row.label;
        entry.group             = row.group;
        entry.tags              = row.tags;
        entry.source_ref        = row.source_ref;
        entry.package_path      = std::string(package_path);
        entry.scale             = row.scale;
        entry.deprecated        = row.deprecated;
        entry.uncertain         = row.uncertain;
        entry.uncertain_reasons = row.uncertain_reasons;

        // The SAME builder `STİL` uses, so a gallery thumbnail is a prediction of
        // what applying the row will draw rather than an approximation of it.
        entry.symbol =
            symbol_of_entry(row, resolve, [this](const DashPattern& p, std::string_view origin) {
                auto id = dashes_.intern(p, origin);
                return id ? id.value() : kSolidDash;
            });

        // From what the PACKAGE says, in the order a gösterim is actually read: a
        // hatch or a fill colour makes it an area, a published line type makes it
        // a line, a bare glyph makes it a point. A row with none of those is a
        // stroke, which is what a plain colour has always meant in CAD.
        if (!row.image_hatch.empty() || row.appearance.fill_rgba != 0)
            entry.kind = SymbolKind::Area;
        else if (!row.image_line.empty())
            entry.kind = SymbolKind::Line;
        else if (!row.image_symbol.empty())
            entry.kind = SymbolKind::Point;
        else
            entry.kind = SymbolKind::Line;

        add(std::move(entry));
        ++added;
    }
    return added;
}

const char* symbol_kind_name(SymbolKind k) noexcept
{
    switch (k) {
    case SymbolKind::Area: return "alan";
    case SymbolKind::Line: return "cizgi";
    case SymbolKind::Point: return "nokta";
    }
    return "?";
}

std::vector<const LibraryEntry*> StyleLibrary::of_kind(SymbolKind kind) const
{
    std::vector<const LibraryEntry*> out;
    for (const LibraryEntry& e : entries_)
        if (e.kind == kind) out.push_back(&e);
    return out;
}

const LibraryEntry* StyleLibrary::find(std::string_view id) const
{
    for (const LibraryEntry& e : entries_)
        if (e.id == id) return &e;
    return nullptr;
}

std::vector<std::string> StyleLibrary::children(std::span<const std::string> path) const
{
    std::vector<std::string> out;
    for (const LibraryEntry& e : entries_) {
        if (e.group.size() <= path.size()) continue;
        if (!group_starts_with(e.group, path)) continue;

        const std::string& child = e.group[path.size()];
        if (std::find(out.begin(), out.end(), child) == out.end()) out.push_back(child);
    }
    return out;
}

std::vector<const LibraryEntry*> StyleLibrary::in_group(std::span<const std::string> path) const
{
    std::vector<const LibraryEntry*> out;
    for (const LibraryEntry& e : entries_)
        if (e.group.size() == path.size() && group_starts_with(e.group, path)) out.push_back(&e);
    return out;
}

std::vector<const LibraryEntry*> StyleLibrary::search(std::string_view needle) const
{
    std::vector<const LibraryEntry*> out;
    if (needle.empty()) return out;

    const std::string folded = turkish_fold_key(needle);

    for (const LibraryEntry& e : entries_) {
        // Label first because that is what a user typed at, then id, then the
        // tags and the group path — a search for `sinir` should find everything
        // filed under SINIRLAR even when the label says `ÜLKE`.
        bool hit = contains_folded(e.label, folded) || contains_folded(e.id, folded);

        for (std::size_t i = 0; !hit && i < e.tags.size(); ++i)
            hit = contains_folded(e.tags[i], folded);
        for (std::size_t i = 0; !hit && i < e.group.size(); ++i)
            hit = contains_folded(e.group[i], folded);

        if (hit) out.push_back(&e);
    }
    return out;
}

void StyleLibrary::set_favourite(std::string_view id, bool on)
{
    for (std::size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].id == id) favourite_[i] = on ? 1 : 0;
}

bool StyleLibrary::favourite(std::string_view id) const
{
    for (std::size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].id == id) return favourite_[i] != 0;
    return false;
}

std::vector<const LibraryEntry*> StyleLibrary::favourites() const
{
    std::vector<const LibraryEntry*> out;
    for (std::size_t i = 0; i < entries_.size(); ++i)
        if (favourite_[i] != 0) out.push_back(&entries_[i]);
    return out;
}

std::uint64_t StyleLibrary::content_hash() const
{
    std::uint64_t h = kLibrarySeed;
    for (const LibraryEntry& e : entries_) {
        h = fnv1a(e.id, h);
        h = fnv1a(e.label, h);
        h = fnv1a(e.source_ref, h);
        h = fnv1a(e.package_path, h);
        for (const std::string& g : e.group)
            h = fnv1a(g, h);
        for (const std::string& t : e.tags)
            h = fnv1a(t, h);
        h = fnv1a_int(static_cast<std::int64_t>(e.kind), h);
        h = fold_symbol(e.symbol, h);
        h = fnv1a_int(static_cast<std::int64_t>(e.scale.low), h);
        h = fnv1a_int(static_cast<std::int64_t>(e.scale.high), h);
        h = fnv1a_int(e.deprecated ? 1 : 0, h);
        h = fnv1a_int(e.uncertain ? 1 : 0, h);
        for (const std::string& why : e.uncertain_reasons)
            h = fnv1a(why, h);
    }
    return h;
}

} // namespace piricad::core
