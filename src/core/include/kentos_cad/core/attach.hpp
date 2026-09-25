// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: objects that FOLLOW other objects.
//
// A length written along a parcel edge is not a free caption: it is a statement
// ABOUT that edge, and the moment the edge moves the statement is standing in the
// wrong place saying the wrong number. The same is true of a corner number: it
// belongs to the corner, not to the coordinates it happened to be written at.
//
// An ATTACHMENT records that relationship. The DEPENDENT (a caption) names its
// SOURCE (the object it is about), WHICH feature of the source it hangs off — a
// vertex or an edge of one ring, by index — HOW it is placed relative to that
// feature (which side, how far), WHAT it says (its own text, or a derived figure
// such as the edge length) and, because a surveyor drags a label to where it
// reads best, the HAND OFFSET from the rule's place to where the user put it.
//
// Nothing here reads or writes a document. The arithmetic is deterministic and
// in millimetres; the command layer decides WHEN to re-evaluate (at the commit of
// the command that changed the source — model.md R14's pattern, never per frame)
// and the processing tools decide WHICH captions get one. The table below is one
// more slot-adjacent store the document owns, like `TextTable`: keyed by the
// dependent's entity row, folded into the content hash, written to the file.
#pragma once

#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Which feature of the source a dependent hangs off.
enum class AttachAnchor : std::uint8_t {
    Vertex = 0, ///< one corner of one ring
    Edge   = 1, ///< one edge of one ring: from vertex `index` to the next
    Centre = 2, ///< the middle of the ring's box: where a parcel's number and area sit
    /// The free end of an open line — a leader's landing (TODOS C-12): the
    /// caption stands beside it on the side the last segment points, reading
    /// along the page, its near edge `gap` from the point and aligned to that
    /// side, so however long the words are they run away from the line.
    Landing = 3,
};

/// Which side of the anchored edge the dependent sits on. `Outside`/`Inside` are
/// about the face (a closed ring); `Left`/`Right` are about the READING direction
/// of the edge, which is what an open line has instead of an inside.
enum class AttachSide : std::uint8_t {
    Outside = 0,
    Inside  = 1,
    Left    = 2,
    Right   = 3,
};

/// What the dependent's text is.
enum class AttachDerive : std::uint8_t {
    Keep   = 0, ///< its own words: a corner number, a note
    Length = 1, ///< the anchored edge's length, in the unit and format recorded
    /// `format` filled from the source each time it changes: a column's value
    /// for `{sutun}`, and a figure measured from its geometry for `{#alan}`,
    /// `{#cevre}` and `{#uzunluk}` (core/text_fields.hpp) — a parcel's
    /// number with its area, which says the new area the moment a corner moves.
    Fields = 2,
};

/// Stable machine names, for a message, a file and a test.
const char* attach_anchor_name(AttachAnchor a) noexcept;
const char* attach_side_name(AttachSide s) noexcept;
const char* attach_derive_name(AttachDerive d) noexcept;

/// The word to enum, Turkish-folded by the caller; nothing for an unknown word.
std::optional<AttachSide> attach_side_from_name(std::string_view word) noexcept;

/// One attachment: everything needed to re-place and re-word a dependent from
/// its source's geometry alone. No floating point is stored (model.md R21).
struct Attachment
{
    EntityKey source{EntityKey::None};       ///< the object this one follows
    AttachAnchor anchor{AttachAnchor::Edge}; ///< a vertex or an edge of it
    AttachSide side{AttachSide::Outside};    ///< which side of the edge
    AttachDerive derive{AttachDerive::Keep}; ///< what the text is
    std::uint16_t ring{0};                   ///< which ring of the source, R11 order
    std::uint32_t index{0};                  ///< the vertex, or the edge's first vertex
    Mm gap{0};                               ///< between the feature and the text's near edge
    Mm along{0};                             ///< the hand's offset ALONG the reading direction
    Mm across{0};                            ///< and ACROSS it, positive to the reading left
    std::uint8_t unit{0};                    ///< `DrawingUnit`, for a derived length
    std::uint8_t precision{2};               ///< decimals of a derived length
    char separator{','};                     ///< its decimal separator
    std::string format;                      ///< `{}` takes the figure: `"{} m"`, `"L={}"`

    friend bool operator==(const Attachment&, const Attachment&) = default;
};

/// Where the rule puts the dependent's caption, and which way it reads.
struct AttachPlacement
{
    Point2 centre{};   ///< where the caption's anchor goes: its middle, unless `anchor` says
    double dir_x{1.0}; ///< the reading direction, unit length: x
    double dir_y{0.0}; ///< and y
    /// The caption's own anchor at `centre`, when the rule decides it: a
    /// landing's caption is aligned to the side it stands on, and changes
    /// alignment when the line turns round. Empty for every other rule, whose
    /// caption keeps the anchor it has.
    std::optional<TextAnchor> anchor{};
};

/// The rule's place for a caption of height `height` attached by `a` to the
/// ring `ring` (`closed` says whether it is a face). With `with_offset` the
/// hand's `along`/`across` are added; without it the bare rule is returned,
/// which is what measuring a new offset needs. Nothing when the anchor is not
/// on the ring — an index past its end, a degenerate edge.
std::optional<AttachPlacement> attach_place(std::span<const Point2> ring, bool closed,
                                            const Attachment& a, Mm height,
                                            bool with_offset = true);

/// The text the rule derives for `a` over `ring`, or nothing when the dependent
/// keeps its own words (`AttachDerive::Keep`), when its words are filled from
/// the whole source rather than one ring (`AttachDerive::Fields`, which the
/// command layer does) or when the anchor is not on the ring.
std::optional<std::string> attach_text(std::span<const Point2> ring, bool closed,
                                       const Attachment& a);

/// Writes into `a.along`/`a.across` the offset that carries the bare rule's
/// caption (`rule`, computed WITHOUT offset) to `actual`, in the rule's reading
/// frame — so the offset survives the source turning or moving.
void attach_measure_offset(const AttachPlacement& rule, Point2 actual, Attachment& a);

/// `a` re-anchored on `now` after the source's ring changed from `was`. With the
/// same vertex count the index is trusted and `a` returns unchanged; otherwise
/// the anchor becomes the feature of `now` nearest to the feature `a` named on
/// `was` — the corner inserted on an edge takes the caption to the nearer half,
/// a corner removed sends it to the merged edge. Exact integer distances, ties
/// to the lower index, so two machines agree.
Attachment attach_reanchor(std::span<const Point2> was, bool was_closed,
                           std::span<const Point2> now, bool now_closed, const Attachment& a);

/// The index of the vertex, or of the edge, of `ring` nearest to `at`. For an
/// edge the distance is to the segment itself. Ties to the lower index.
std::uint32_t attach_nearest(std::span<const Point2> ring, bool closed, AttachAnchor anchor,
                             Point2 at);

/// Which side of edge `index` the point `at` lies on, as the side word the
/// attachment stores: for a face `Outside`/`Inside`, for a line `Left`/`Right`
/// of the reading direction. What BAĞLA records for a caption that already sits
/// somewhere, so the rule's place is near the caption and the offset small.
AttachSide attach_side_of(std::span<const Point2> ring, bool closed, std::uint32_t index,
                          Point2 at);

/// `{}` in `format` becomes `figure`; a format without it gets the figure
/// appended, so a typo cannot swallow the number. An empty format is `{}`.
std::string attach_fill(std::string_view format, std::string_view figure);

/// The unit's suffix as a caption writes it: ` m`, ` cm`, ` mm`, ` km`.
const char* attach_unit_suffix(std::uint8_t unit) noexcept;

/// "This entity is attached to nothing", the value every row starts at.
inline constexpr std::uint32_t kNoAttach = 0xFFFFFFFFu;

/// The attachments of one document, keyed by the DEPENDENT's entity row.
///
/// Entity-indexed rather than slot-indexed on purpose: every edit of a caption's
/// geometry appends a new slot, and a slot-indexed table would have to be carried
/// on each one (as `TextTable` is); an entity row is appended once and never
/// moves. Like `TextTable`, nothing is allocated until the first attachment, so
/// the five-million-parcel sheet with none pays nothing (§10.1).
class AttachTable
{
public:
    /// Follows the entity table: every entity row is a possible dependent.
    void resize(std::size_t entity_count);

    std::size_t entity_count() const noexcept { return count_; }

    bool has(EntityId e) const noexcept;

    /// The attachment of `e`, or null.
    const Attachment* get(EntityId e) const noexcept;

    /// Attaches, or replaces, in place. The previous record is what the caller
    /// keeps for undo; nothing else points at it.
    void set(EntityId e, Attachment a);

    /// Detaches `e`. False when it was attached to nothing.
    bool clear(EntityId e);

    /// How many entities are attached to something.
    std::size_t size() const noexcept { return live_; }

    bool empty() const noexcept { return live_ == 0; }

    /// Every attached entity, ascending. The order the writer, the fold and the
    /// commit-time update walk, so two documents built alike agree.
    std::vector<EntityId> attached() const;

    /// The entities attached to `source`, ascending; `out` is cleared first.
    void dependents_of(EntityKey source, std::vector<EntityId>& out) const;

    /// Folds into the document hash. A document with no attachment folds to the
    /// seed unchanged, so every fixture written before attachments existed keeps
    /// its fingerprint.
    std::uint64_t fold(std::uint64_t seed, std::span<const std::uint32_t> position = {}) const;

private:
    void materialise();

    std::size_t count_{0};           ///< logical rows, whether or not allocated
    std::vector<std::uint32_t> ref_; ///< per entity: record index or kNoAttach
    std::vector<Attachment> records_;
    std::vector<EntityId> owner_;     ///< per record: the entity, or kNoEntity when free
    std::vector<std::uint32_t> free_; ///< records to reuse
    std::size_t live_{0};
};

} // namespace kentos::core
