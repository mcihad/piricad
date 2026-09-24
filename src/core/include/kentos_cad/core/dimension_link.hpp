// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: which geometry a dimension measures, so it can follow it.
//
// A DIMENSION THAT DOES NOT FOLLOW ITS SOURCE IS A WRONG NUMBER WAITING
// (TODOS C-10). A parcel's corner moves by a metre, and the dimension beside it
// keeps saying what the side used to be — on a sheet a licensed engineer signs.
// So a dimension's definition points may be LINKED to features of other
// objects: a vertex, a circle's or an arc's centre, an arc's end, a point on a
// circle. When a transaction moves the feature, the dimension is re-laid out
// around it at commit, re-measured and re-worded; when the object is erased, the
// link is kept and marked BROKEN, so the dimension stays where it was and says
// it no longer measures anything.
//
// WHAT COUNTS AS LIVE IS THE SOURCE MOVING. Dragging a dimension's own
// definition point off the feature it sat on releases that link — the user has
// said the point is somewhere else now — and moving the dimension and its source
// together leaves every link as it was.
//
// A SEPARATE TABLE, NOT THE ATTACHMENT TABLE (model.md R46 holds ONE source per
// dependent; a dimension measures between two objects), sparse and entity-
// indexed like it: a drawing with no linked dimension pays nothing and keeps
// its fingerprint and its bytes.
#pragma once

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// The document the table lives in; the lookups below read it.
class Document;

/// Which feature of its source a definition point sits on.
enum class DimAnchor : std::uint8_t {
    Vertex   = 0, ///< a vertex of a ring: `ring`, `index`
    Centre   = 1, ///< the centre of a circle or an arc
    ArcStart = 2, ///< an arc's start
    ArcEnd   = 3, ///< an arc's end
    OnCircle = 4, ///< the point of a circle or an arc at the angle `index`, micro-degrees
};

/// One definition point of a dimension, tied to a feature of another object.
struct DimLink
{
    std::uint8_t point{0};               ///< which definition point: its index in ring 1
    DimAnchor anchor{DimAnchor::Vertex}; ///< which feature
    std::uint16_t ring{0};               ///< Vertex: which ring of the source
    std::uint32_t index{0};              ///< Vertex: which vertex; OnCircle: the angle, µ°
    EntityKey source{EntityKey::None};   ///< the object measured, by key (model.md R1)
    bool broken{false};                  ///< the object is gone: the point stays where it was

    /// Member-wise equality.
    friend bool operator==(const DimLink&, const DimLink&) = default;
};

/// Every linked dimension's links, by the dimension's entity row — a row never
/// moves, where a slot changes on every edit (the reason model.md R46 gives).
class DimLinkTable
{
public:
    /// The links of `dim`, or null when it has none.
    const std::vector<DimLink>* get(EntityId dim) const;

    /// Replaces the links of `dim`; an empty list clears it.
    void set(EntityId dim, std::vector<DimLink> links);

    /// Every dimension that has links, ascending — the order the writer, the
    /// fold and the commit-time update walk, so two documents built alike agree.
    std::vector<EntityId> linked() const;

    bool empty() const noexcept { return rows_.empty(); }

    std::size_t size() const noexcept { return rows_.size(); }

    /// Folds into the document hash; the seed comes back unchanged when the
    /// table is empty, so every drawing without a linked dimension keeps its
    /// fingerprint.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::map<EntityId, std::vector<DimLink>> rows_;
};

/// The links as bytes — what an undo record carries.
std::vector<std::uint8_t> encode_dim_links(std::span<const DimLink> links);

/// The links back, refused when the bytes are not what the encoder writes.
Result<std::vector<DimLink>> decode_dim_links(std::span<const std::uint8_t> bytes);

/// What a definition point is to its dimension, for finding the feature under it.
enum class DimRole : std::uint8_t {
    Point,    ///< a point measured: a vertex, an arc's end, a centre
    Centre,   ///< the centre of a circle or an arc
    OnCircle, ///< a point on a circle or an arc
    ArcStart, ///< an arc's start
    ArcEnd,   ///< an arc's end
};

/// The role of each definition point of a `type`, in ring-1 order; nothing for
/// a point the layout derives (a dimension line's place, an arc through which
/// an angle is drawn), which follows the others rather than any geometry.
std::vector<std::optional<DimRole>> dim_roles(DimensionType type);

/// The feature of `doc` a definition point with `role` sits EXACTLY on — the
/// same integer millimetre — for linking it: a vertex, a centre, an arc's end,
/// or, for `OnCircle`, a circle or arc within a millimetre of it. Visible
/// standalone entities other than `exclude`, the oldest first; nothing when no
/// feature is there. Exact because the association must be the same for every
/// client: a click that snapped, a typed coordinate and a script's point on the
/// corner all link to the corner (Article 1.2). A non-empty `among`, sorted,
/// limits the search to those entities — the objects a command created in
/// place of one it erased.
std::optional<DimLink> dim_anchor_at(const Document& doc, Point2 p, DimRole role, EntityId exclude,
                                     std::span<const EntityId> among = {});

/// Where the feature a link names is now; nothing when its source is gone or
/// no longer has that feature (a vertex index past the ring's end).
std::optional<Point2> dim_anchor_point(const Document& doc, const DimLink& link);

} // namespace kentos::core
