// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: every tie between objects, asked one question (TODOS F-04).
//
// FOUR TIES, ONE QUESTION. A caption follows an edge (core/attach.hpp), a
// dimension measures corners (core/dimension_link.hpp), a hatch fills what its
// boundary closes (core/hatch_link.hpp), a result was computed from objects
// (core/lineage.hpp). The first three are kept up to date at commit; the last
// says when it is out of date. But a follower on a locked layer does not follow,
// a file can bring one in out of step, and until now nothing asked afterwards
// whether it still agreed with its source. This header asks: for any object, what
// it depends on and whether it still says what that says — CURRENT, BEHIND (a
// source changed and it did not follow, or a result out of date), BROKEN (a
// follower that lost a source and follows nothing more), SOURCELESS (a result
// whose source is gone).
//
// COMPUTED, NEVER STORED, like a result's state: the answer is read from the
// drawing, so an undo is always right. And the rule's place for a caption has
// ONE home, `caption_follow`: the commit-time settle writes what it returns, and
// the check compares with it, so the two cannot disagree.
#pragma once

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/text_store.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::core {

/// What kind of tie a dependent has to its sources.
enum class TieKind : std::uint8_t {
    Caption   = 0, ///< a caption following an edge, a corner or a face
    Dimension = 1, ///< a dimension whose points sit on features of other objects
    Hatch     = 2, ///< a hatch whose loops its boundary objects give
    Result    = 3, ///< a result computed from objects: a buffer, a contour
};

/// Whether a dependent still agrees with its sources.
enum class TieState : std::uint8_t {
    Current    = 0, ///< it says what its sources say
    Behind     = 1, ///< a source changed and it did not follow — out of date
    Broken     = 2, ///< a follower that lost a source: it stays as it was and follows nothing
    Sourceless = 3, ///< a result whose source is gone and nothing else changed: it stands alone
};

/// The kind's stable machine word: `yazi`, `olcu`, `tarama`, `sonuc`.
const char* tie_kind_id(TieKind k) noexcept;

/// The state's stable machine word: `guncel`, `guncel_degil`, `kopuk`, `kaynaksiz`.
const char* tie_state_id(TieState s) noexcept;

/// One tie of one dependent.
struct Tie
{
    EntityId dependent{kNoEntity};     ///< the object that depends
    TieKind kind{TieKind::Caption};    ///< how
    TieState state{TieState::Current}; ///< whether it still agrees with them
    std::vector<EntityKey> sources;    ///< what it depends on, ascending, once each
    std::vector<EntityKey> changed;    ///< the sources it is behind: moved, re-valued, renumbered
    std::vector<EntityKey> gone;       ///< the sources that are no longer there
};

/// A caption as its rule puts it: where it stands and what it says.
struct CaptionFollow
{
    std::array<Point2, 2> base{};                ///< the baseline the caption stands on
    std::string text;                            ///< the words it says
    TextAnchor anchor{TextAnchor::MiddleCentre}; ///< the point of the words at its place
    Mm height{0};                                ///< its letters' height, as it has it
};

/// Where caption `e`'s rule puts it — its baseline, its words, its anchor —
/// reading its source as it is now, with attachment `a` in place of the stored
/// one (the settle passes one re-anchored after a corner came or went). The
/// commit-time follow writes exactly this. Nothing when `e` is not a caption
/// that follows, its source is gone, or the anchor is not on the source's ring.
std::optional<CaptionFollow> caption_follow(const Document& doc, EntityId e, const Attachment& a);

/// Whether hatch `hatch` fills exactly what `sources` close, in its own island
/// rule: false when a source is broken or gone or the loops differ.
bool hatch_fills(const Document& doc, EntityId hatch, std::span<const HatchSource> sources);

/// Every tie `e` has: none, or one per kind it carries. History is not a tie:
/// a copy's origin names its source and asks nothing of it.
std::vector<Tie> ties_of(const Document& doc, EntityId e);

/// Every tie in the drawing, by dependent row: what BAĞIMLILIK reads.
std::vector<Tie> every_tie(const Document& doc);

} // namespace kentos::core
