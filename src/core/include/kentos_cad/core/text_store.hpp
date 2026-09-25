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

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// Where the anchor point sits on the text: which corner, edge or centre of it.
///
/// THE ANCHOR IS THE BASELINE'S FIRST POINT, whatever the value. A column (left,
/// centre, right) says where along each line the anchor falls; a row where across
/// the lines: on the BASELINE of the last line (a single line's own baseline, a
/// block's bottom — DXF MTEXT's bottom row), halfway between the first line's
/// capital tops and the last line's baseline, or on the first line's capital
/// tops. For one line these are exactly DXF TEXT's baseline, middle and top.
///
/// APPEND ONLY: the byte is written into every saved drawing (model.md R26), so
/// the four values that came first keep their numbers and the rest follow them.
enum class TextAnchor : std::uint8_t {
    BaselineLeft = 0, ///< the CAD default: baseline starts at the first point
    BaselineCentre,
    BaselineRight,
    MiddleCentre, ///< centred both ways — what a parcel number wants
    TopLeft,      ///< hanging from the point: a note under a title
    TopCentre,
    TopRight,
    MiddleLeft, ///< beside a symbol, centred on it
    MiddleRight,
};

/// How many anchor values there are; a byte past the last is not one.
inline constexpr std::uint8_t kTextAnchorCount = 9;

/// Stable machine name for a file, a message or a test.
const char* text_anchor_name(TextAnchor a) noexcept;

/// Where along each line the anchor falls: 0 left, 1 centre, 2 right.
int text_anchor_column(TextAnchor a) noexcept;

/// Where across the lines: 0 on the last baseline, 1 in the middle, 2 on the top.
int text_anchor_row(TextAnchor a) noexcept;

/// The anchor at `column` (0–2) and `row` (0–2), as the two functions above count.
TextAnchor text_anchor_at(int column, int row) noexcept;

/// The standard distance from one baseline to the next, in text heights: DXF's
/// "3 on 5" — three units of capital letter on five of line — so a caption with
/// a line spacing of 1 keeps the pitch AutoCAD gives the same MTEXT.
inline constexpr double kTextLinePitch = 5.0 / 3.0;

/// How the lines of one text are laid out.
struct TextLines
{
    /// The distance between baselines, in thousandths of `kTextLinePitch`:
    /// 1000 is single spacing, 1500 one and a half. DXF's own range, 0,25–4.
    std::uint16_t spacing{1000};
    /// Whether the lines break to fit the baseline's length, as an MTEXT does to
    /// its width. Without it a line is broken only where the text says so.
    bool wrap{false};

    friend bool operator==(const TextLines&, const TextLines&) = default;
};

/// The line spacing's bounds, in thousandths: DXF's 0,25 and 4.
inline constexpr std::uint16_t kTextSpacingMin = 250;
inline constexpr std::uint16_t kTextSpacingMax = 4000;

/// The characters of `utf8` — code points, not bytes: `Ş` is one letter and two
/// bytes, and a width counted in bytes made every Turkish caption's box too long.
std::size_t text_characters(std::string_view utf8) noexcept;

/// A text's width at `height`: its longest line, every character at the drawing
/// face's own advance (core/text_metrics.hpp), rounded to the millimetre — the
/// width both backends draw it at (TODOS C-18). What a new text's baseline
/// length is, and so its box for the cull and the pick, and what a dimension's
/// figure is fitted between its extension lines by.
Mm text_width(std::string_view utf8, Mm height) noexcept;

/// The length of a text's baseline from `a` to `b`, to the millimetre: the width
/// a wrapping text breaks to. One function, so the box and the drawing break a
/// text at the same word.
Mm text_baseline_length(Point2 a, Point2 b) noexcept;

/// A baseline from `from`, `length` long, pointing at `toward` — along the page
/// when `toward` is `from`, because a baseline with no length has no direction.
std::array<Point2, 2> text_baseline(Point2 from, Point2 toward, Mm length) noexcept;

/// The lines `utf8` is set in, as views into it: split at every newline — a
/// blank line is kept, it is how one paragraph is set apart from the next — and,
/// for a text that wraps to `width`, at the last space that keeps a line within
/// `width` at `height`, that space belonging to neither line. A word wider than
/// `width` stands on a line of its own. THE ONE PLACE A LINE BREAKS: the box
/// counts these lines and the scene hands these lines to both backends, so the
/// box and the sheet cannot disagree about where a plan note's second line
/// begins. `out` is cleared first.
void text_lines(std::string_view utf8, Mm height, TextLines lines, Mm width,
                std::vector<std::string_view>& out);

/// How many lines `text_lines` sets `utf8` in, allocating nothing.
std::size_t text_line_count(std::string_view utf8, Mm height, TextLines lines, Mm width) noexcept;

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
    Status set(std::size_t slot, std::string_view content, Mm height, TextAnchor anchor,
               TextLines lines = {});

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

    /// How the slot's lines are laid out; the default for a slot that never said.
    TextLines lines(std::size_t slot) const noexcept;

    /// Distinct interned strings.
    std::size_t pool_size() const noexcept { return pool_.size(); }

    /// Folds into the document hash. A slot with no text folds distinctly from a
    /// slot carrying the empty string: "this entity is not text" and "this text
    /// entity says nothing" are different drawings.
    std::uint64_t fold(std::uint64_t seed) const;

    /// The same fold over `slots`, in that order — one per row of a document,
    /// each the slot its geometry holds NOW (`Document::content_hash`). A slot a
    /// geometry edit left behind is history, not content, and never folds.
    std::uint64_t fold(std::uint64_t seed, std::span<const std::uint32_t> slots) const;

private:
    std::uint32_t intern(std::string_view s);

    /// Allocates the three columns on first write. Doing it here rather than in
    /// `resize` is what keeps a text-free document free of them.
    void materialise();

    std::size_t count_{0};           ///< logical slots, whether or not allocated
    std::vector<std::uint32_t> ref_; ///< index into pool_, or kNoText; empty until first set()
    std::vector<Mm> height_;
    std::vector<std::uint8_t> anchor_;
    /// Line layout, allocated only when a text first says something other than
    /// the default — so a drawing of one-line captions carries neither column.
    std::vector<std::uint16_t> spacing_;
    std::vector<std::uint8_t> wrap_;
    std::vector<std::string> pool_; ///< insertion-ordered, never compacted
    std::unordered_map<std::string, std::uint32_t> intern_;
};

} // namespace kentos::core
