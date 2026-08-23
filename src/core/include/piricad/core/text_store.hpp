// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the text store.
//
// A drawing is not only geometry. A pafta carries ada and parsel numbers, plan
// notes, north arrows and legend captions, and every one of them is a CAD entity
// with an exact position, an exact height and an exact rotation — not a label the
// renderer invents from an attribute.
//
// Why text is NOT an attribute column: .claude/model.md R29/P29 forbids the frame
// path from reading attribute columns at all. Drawing text requires reading the
// string every frame, so a text entity that stored its content as an attribute
// would either break that rule or never be drawn. Text is geometry-adjacent data
// and lives beside RingGeometry, indexed by the same slot.
//
// Why the baseline is a two-vertex ring rather than a point: an entity needs a
// bounding box to be culled and snapped, and a text's extent is not its anchor.
// Storing the baseline as an ordinary open ring gives the cull test a real box,
// gives snapping two real endpoints, and encodes rotation as the direction of an
// integer segment — exactly, with no angle stored and no trigonometry to disagree
// about across platforms (§7.3). This is what DXF TEXT does with its insertion
// and alignment points, and it does it for the same reasons.
#pragma once

#include "piricad/core/result.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace piricad::core {

/// Where the baseline sits relative to the glyphs.
enum class TextAnchor : std::uint8_t {
    BaselineLeft = 0, ///< the CAD default: baseline starts at the first point
    BaselineCentre,
    BaselineRight,
    MiddleCentre, ///< centred both ways — what a parcel number wants
};

const char* text_anchor_name(TextAnchor a) noexcept;

inline constexpr std::uint32_t kNoText = 0xFFFFFFFFu;

/// One text per entity slot: content, height and anchor.
///
/// Content is dictionary-encoded for the same reason an attribute text column is:
/// a cadastral sheet repeats "Ada:" thousands of times and stores it once.
class TextTable
{
public:
    /// Grows with every new slot empty. Slots follow RingGeometry slots exactly,
    /// so a document with no text pays one u32 per entity and nothing else.
    /// The parameter is NOT called `slots`: Qt defines `slots` as a macro, so any
    /// translation unit that sees both this header and Qt expands it to nothing.
    /// Core is Qt-free, but /src/app is not and it includes both.
    void resize(std::size_t count);

    std::size_t slot_count() const noexcept { return ref_.size(); }

    /// Attaches text to `slot`. Returns the previous state, which is what the undo
    /// record needs and all it needs.
    Status set(std::size_t slot, std::string_view content, Mm height, TextAnchor anchor);

    /// Detaches text from `slot`. The pool is never compacted: an index handed out
    /// stays valid for the document's lifetime, and renumbering it would
    /// invalidate the previous values the journal already holds.
    void clear(std::size_t slot);

    bool has(std::size_t slot) const noexcept;

    /// Empty when the slot carries no text.
    std::string_view text(std::size_t slot) const;

    /// Ground millimetres, the same unit every coordinate uses. A text height in
    /// paper units would change what the drawing SAYS when the plot scale changes,
    /// and on a pafta the height of a parcel number is part of the drawing.
    Mm height(std::size_t slot) const noexcept;

    TextAnchor anchor(std::size_t slot) const noexcept;

    /// Distinct interned strings.
    std::size_t pool_size() const noexcept { return pool_.size(); }

    /// Folds into the document hash. A slot with no text folds distinctly from a
    /// slot carrying the empty string: "this entity is not text" and "this text
    /// entity says nothing" are different drawings.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::uint32_t intern(std::string_view s);

    std::vector<std::uint32_t> ref_; ///< index into pool_, or kNoText
    std::vector<Mm> height_;
    std::vector<std::uint8_t> anchor_;
    std::vector<std::string> pool_; ///< insertion-ordered, never compacted
    std::unordered_map<std::string, std::uint32_t> intern_;
};

} // namespace piricad::core
