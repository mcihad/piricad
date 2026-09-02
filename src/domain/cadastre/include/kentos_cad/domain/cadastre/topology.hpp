// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — cadastre: the checks a sheet must pass before it is submitted.
//
// A cadastral drawing is a legal document, and the defects below are the ones
// that make one unacceptable: a boundary that crosses itself, two parcels that
// claim the same ground, a parcel with no area. None of them is visible at a
// glance on a sheet with four thousand parcels on it, which is why this exists.
//
// IT REPORTS, IT NEVER REPAIRS. An automatic fix would move a boundary, and a
// boundary is measured data — the surveyor decides what a defect means, and only
// they can sign the result (6.11).
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>
#include <vector>

namespace kentos::domain::cadastre {

/// What kind of defect was found.
enum class DefectKind : std::uint8_t {
    SelfIntersecting, ///< the boundary crosses itself
    ZeroArea,         ///< the ring encloses nothing
    Overlap,          ///< two parcels claim the same ground
};

/// One finding, named so a surveyor can go and look at it.
struct Defect
{
    DefectKind kind{DefectKind::SelfIntersecting}; ///< what is wrong
    core::EntityKey first{core::EntityKey::None};  ///< the parcel at fault
    core::EntityKey second{core::EntityKey::None}; ///< the other one, for an overlap
    core::Mm2 area{0};                             ///< the overlapping area, for an overlap
};

/// Checks every entity in `keys`, or the whole drawing when it is empty.
///
/// Pairwise for overlaps, bounded by the bounding boxes: two parcels whose boxes
/// do not touch cannot overlap, and skipping those is what keeps a sheet-sized
/// check from being quadratic in practice.
std::vector<Defect> check_topology(const core::Document& doc,
                                   const std::vector<core::EntityKey>& keys);

/// The Turkish sentence a user reads for one defect.
std::string describe(const core::Document& doc, const Defect& d);

} // namespace kentos::domain::cadastre
