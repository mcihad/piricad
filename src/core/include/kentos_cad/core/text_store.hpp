// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the text store.
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

#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// Where the baseline sits relative to the glyphs.
enum class TextAnchor : std::uint8_t {
    BaselineLeft = 0, ///< the CAD default: baseline starts at the first point
    BaselineCentre,
    BaselineRight,
    MiddleCentre, ///< centred both ways — what a parcel number wants
};

/// Stable machine name for a file, a message or a test.
const char* text_anchor_name(TextAnchor a) noexcept;

/// "This slot carries no text", the value every slot starts at.
inline constexpr std::uint32_t kNoText = 0xFFFFFFFFu;

/// One text per entity slot: content, height and anchor.
///
/// Content is dictionary-encoded for the same reason an attribute text column is:
/// a cadastral sheet repeats "Ada:" thousands of times and stores it once.
class TextTable
{
public:
    /// Follows the geometry: every entity slot is a text slot, occupied or not.
    ///
    /// ALLOCATES NOTHING until the first `set()`. Only the count is recorded here;
    /// the three columns are materialised on first write. The eager version cost
    /// thirteen bytes per entity and seventeen per cent of a bulk load in EVERY
    /// document, including the overwhelming majority that carry no text at all —
    /// measured, not guessed: `make bench` reported it against the recorded
    /// baseline the day it was introduced.
    ///
    /// Still dense once materialised, which is the right trade while a text
    /// entity's slot sits among other text entities' slots. A drawing that mixes
    /// five million parcels with fifty thousand labels would want a sparse table
    /// keyed by slot; nothing measures that case yet, and building for it now
    /// would be a structure chosen from a guess.
    ///
    /// The parameter is NOT called `slots`: Qt defines `slots` as a macro, so any
    /// translation unit that sees both this header and Qt expands it to nothing.
    /// Core is Qt-free, but /src/app is not and it includes both.
    void resize(std::size_t count);

    std::size_t slot_count() const noexcept { return count_; }

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

    /// Allocates the three columns on first write. Doing it here rather than in
    /// `resize` is what keeps a text-free document free of them.
    void materialise();

    std::size_t count_{0};           ///< logical slots, whether or not allocated
    std::vector<std::uint32_t> ref_; ///< index into pool_, or kNoText; empty until first set()
    std::vector<Mm> height_;
    std::vector<std::uint8_t> anchor_;
    std::vector<std::string> pool_; ///< insertion-ordered, never compacted
    std::unordered_map<std::string, std::uint32_t> intern_;
};

} // namespace kentos::core
