// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: what a drawing holds twice, or holds for nothing.
//
// NONE OF IT IS VISIBLE, ALL OF IT IS FOUND LATER (TODOS C-09). A DXF brought
// in twice, a parcel digitised on top of itself, a line a double click left at
// one point, a corner a survey recorded twice: the sheet looks right, and the
// defect surfaces somewhere else — an overlap a topology check reports that
// nobody drew, an export that writes the parcel twice, a vertex count one too
// many on a röper krokisi. TEMİZLE finds and repairs these; the topology check
// (TOPOLOJİ) reports them through this same finder, so the two can never
// disagree about what is redundant.
//
// THE TOLERANCE IS THE PROJECT'S NODE TOLERANCE (`core.topoloji.dugum_toleransi`),
// the one ALANAÇEVİR, SINIR and İFRAZ read: two corners closer than it are one
// corner everywhere in the program, so they are one corner here too.
//
// COMPARED IN INTEGERS. A duplicate is the same kind on the same layer with the
// same vertices, exactly — every coordinate is an integer millimetre, so the
// test is exact and the same on every machine (§7.3).
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// What is redundant about an object.
enum class RedundancyKind : std::uint8_t {
    Duplicate,      ///< drawn again: the same kind, on the same layer, the same geometry
    Empty,          ///< a line of no length or a face of no area: it draws nothing
    RepeatedVertex, ///< consecutive corners within the node tolerance: one of them is enough
};

/// One redundancy, named so a user can go and look at it.
struct Redundancy
{
    RedundancyKind kind{RedundancyKind::Duplicate}; ///< what is redundant
    EntityId entity{kNoEntity};                     ///< the object at fault
    EntityId kept{kNoEntity};                       ///< Duplicate: the copy that stays, the oldest
    std::size_t vertices{0};                        ///< RepeatedVertex: how many corners would go
    Point2 at{};                                    ///< where to look: its first vertex
};

/// Every redundancy among `slots` — every standalone entity when `slots` is
/// empty — in slot order. A duplicate is reported against the OLDEST of its
/// copies anywhere in the drawing, in scope or not, which is the one a repair
/// keeps. A caption is never empty (its words are what it is) and its
/// baseline's corners are never repeats.
std::vector<Redundancy> find_redundant(const Document& doc, std::span<const EntityId> slots,
                                       Mm tolerance);

/// One ring of a repaired object.
struct RepairedRing
{
    std::vector<Point2> points;    ///< its corners, repeats taken out
    RingRole role{RingRole::Open}; ///< its role, unchanged
    std::uint16_t part{0};         ///< its part, unchanged
};

/// The rings of polyline `e` with every repeated corner taken out — a corner
/// within `tolerance` of the one kept before it — or nothing when there is none
/// to take, or when taking them would leave a ring with too few corners (that
/// object is empty, not repaired).
std::optional<std::vector<RepairedRing>> rings_without_repeats(const Document& doc, EntityId e,
                                                               Mm tolerance);

/// Whether two entities carry the same attributes, cell for cell: a copy that
/// holds data the other lacks is not a copy a repair may delete.
bool same_attributes(const Document& doc, EntityId a, EntityId b);

/// Whether `e` carries any attribute value at all.
bool has_attributes(const Document& doc, EntityId e);

} // namespace kentos::core
