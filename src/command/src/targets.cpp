// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/targets.hpp"

#include "piricad/core/entity_kind.hpp"
#include "piricad/core/geometry.hpp"

#include <cstdint>

namespace piricad::command {

Targets target_of(const core::Document& doc, core::EntityId e)
{
    const core::EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return Targets::None;
    const std::uint32_t slot = ents.slot[e];
    switch (ents.kind[e]) {
    case core::kPointKind: return Targets::Points;
    case core::kPolylineKind:
    case core::kArcPolylineKind: {
        // A CAPTION rides on a polyline's baseline and is a caption to the eye.
        if (doc.texts().has(slot)) return Targets::Texts;
        const core::RingSpan rs = doc.geometry().rings_of(slot);
        if (rs.count == 0) return Targets::None;
        return doc.geometry().ring_role[rs.first] == core::RingRole::Open ? Targets::Lines
                                                                          : Targets::Faces;
    }
    case core::kCircleKind:
    case core::kArcKind:
    case core::kEllipseKind:
    case core::kSplineKind: return Targets::Curves;
    case core::kHatchKind: return Targets::Hatches;
    case core::kDimensionKind: return Targets::Dimensions;
    case core::kBlockReferenceKind: return Targets::Blocks;
    case core::kLeaderKind: return Targets::Leaders;
    default: return Targets::None;
    }
}

bool acts_on(Targets targets, const core::Document& doc, core::EntityId e)
{
    return has_target(targets, target_of(doc, e));
}

Held held_by(const core::Document& doc, std::span<const core::EntityKey> keys)
{
    Held held;
    for (const core::EntityKey k : keys) {
        const core::EntityId e = doc.slot_of(k);
        if (e == core::kNoEntity || !doc.alive(e)) continue;
        ++held.count;
        const Targets one = target_of(doc, e);
        if (one == Targets::None)
            held.unclassed = true;
        else
            held.classes = held.classes | one;
    }
    return held;
}

bool acts_on_all(Targets targets, const Held& held)
{
    if (held.count == 0) return true;
    if (held.unclassed) return targets == Targets::Any;
    // Widened first: a `std::uint16_t` would be promoted to `int` by `~`.
    const auto held_bits  = static_cast<std::uint32_t>(held.classes);
    const auto taken_bits = static_cast<std::uint32_t>(targets);
    return (held_bits & ~taken_bits) == 0U;
}

} // namespace piricad::command
