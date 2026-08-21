// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/style.hpp"

#include "piricad/core/text.hpp"

namespace piricad::core {
namespace {

/// A per-record-type seed, so that folding a style into a document hash cannot
/// collide with folding a layer or an attribute record that happens to carry the
/// same integers.
constexpr std::uint64_t kAppearanceSeed = fnv1a("piricad.core.appearance");

} // namespace

std::uint64_t fold_appearance(const Appearance& a, std::uint64_t seed)
{
    // Field by field, never a memcpy of the struct. The padding bytes are
    // indeterminate and the byte order of a multi-byte member is not, so hashing
    // the raw object would give one answer on x86 and another somewhere else —
    // and a golden fixture that disagrees across platforms is a legal defect
    // (.claude/core.md R9, piricad.md §7.3).
    std::uint64_t h = seed;
    h               = fnv1a_int(static_cast<std::int64_t>(a.rgba), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.width_um), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.dash), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.symbol), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.fill_rgba), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.hatch), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.z_order), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_colour), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_width), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_dash), h);
    h               = fnv1a_int(static_cast<std::int64_t>(a.src_fill), h);
    return h;
}

std::size_t StyleTable::Hash::operator()(const Appearance& a) const noexcept
{
    // Every field, including z_order and the four Source tags: two appearances
    // that differ only in draw order are two different styles (R18).
    return static_cast<std::size_t>(fold_appearance(a, kAppearanceSeed));
}

StyleTable::StyleTable()
{
    // Entry 0 is the kByLayerStyle sentinel and exists before anything is
    // interned, so a default Appearance — every property ByLayer, which is what
    // an ordinary cadastral entity carries — interns back to 0 and costs nothing
    // (R13).
    const Appearance by_layer{};
    entries_.push_back(by_layer);
    intern_.emplace(by_layer, kByLayerStyle);
}

StyleId StyleTable::intern(const Appearance& a)
{
    if (const auto it = intern_.find(a); it != intern_.end()) return it->second;

    // Ids are handed out in first-seen order. Nothing here reads a pointer, an
    // address or the hash map's iteration order, so the same sequence of interns
    // yields the same ids on every platform and in every run — which is what the
    // golden fixtures record (.claude/core.md P11).
    const auto id = static_cast<StyleId>(entries_.size());
    entries_.push_back(a);
    intern_.emplace(a, id);
    return id;
}

const Appearance& StyleTable::at(StyleId id) const
{
    // A corrupt or stale style column must not be able to read past the end and
    // take the process down mid-frame; entry 0 is always present and always a
    // legal appearance, so an unknown id degrades to "inherit from the layer".
    return id < entries_.size() ? entries_[id] : entries_[kByLayerStyle];
}

std::uint64_t StyleTable::fold(std::uint64_t seed) const
{
    // In id order, and with the id mixed in: the id is what every entity stores,
    // so two tables holding the same appearances under different ids describe two
    // different documents.
    std::uint64_t h = seed;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        h = fnv1a_int(static_cast<std::int64_t>(i), h);
        h = fold_appearance(entries_[i], h);
    }
    return h;
}

Appearance resolve_appearance(const Appearance& own, const Appearance& layer_default)
{
    // ByBlock has no block reference at this level. Blocks (INSERT / hücre) are a
    // Phase-2 deliverable; until the block table exists, a ByBlock property
    // resolves exactly as ByLayer does. Dropping the property instead would lose
    // data on every DWG/DXF import, which P11 forbids.
    const auto from_own = [](Source s) { return s == Source::Explicit; };

    Appearance out = own;

    if (!from_own(own.src_colour)) out.rgba = layer_default.rgba;
    if (!from_own(own.src_width)) out.width_um = layer_default.width_um;
    if (!from_own(own.src_dash)) out.dash = layer_default.dash;
    if (!from_own(own.src_fill)) {
        // hatch is the pattern half of the same fill property — Appearance carries
        // no src_hatch, and a resolved fill colour with an unresolved hatch would
        // draw an imar lekesi in the layer's colour with the entity's (empty)
        // pattern (R18).
        out.fill_rgba = layer_default.fill_rgba;
        out.hatch     = layer_default.hatch;
    }

    // symbol and z_order have no Source of their own and therefore never cascade;
    // they are carried through from `own` unchanged.

    // The result IS the resolution, so every property now speaks for itself. The
    // command that writes the style column stores this, and the renderer reads
    // one u32 and no cascade (R14).
    out.src_colour = Source::Explicit;
    out.src_width  = Source::Explicit;
    out.src_dash   = Source::Explicit;
    out.src_fill   = Source::Explicit;
    return out;
}

} // namespace piricad::core
