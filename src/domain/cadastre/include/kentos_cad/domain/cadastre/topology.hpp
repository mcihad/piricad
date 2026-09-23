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
    Duplicate,        ///< drawn twice: the same kind, layer and geometry (core::find_redundant)
    ZeroLength,       ///< a line that draws nothing (core::find_redundant)
    RepeatedVertex,   ///< consecutive corners within the node tolerance (core::find_redundant)
    Gap,              ///< an end of a line network a gap away from other linework (core::Network)
};

/// One finding, named so a surveyor can go and look at it.
struct Defect
{
    DefectKind kind{DefectKind::SelfIntersecting}; ///< what is wrong
    core::EntityKey first{core::EntityKey::None};  ///< the parcel at fault
    core::EntityKey second{core::EntityKey::None}; ///< the other one, for an overlap
    core::Mm2 area{0};                             ///< the overlapping area, for an overlap
    std::size_t count{0};                          ///< RepeatedVertex: how many corners
    core::Point2 at{};                             ///< where to look, when there is a place
    core::Point2 to{};                             ///< Gap: the nearest linework
    core::Mm distance{0};                          ///< Gap: how wide
};

/// Checks every entity in `keys`, or the whole drawing when it is empty.
///
/// Pairwise for overlaps, bounded by the bounding boxes: two parcels whose boxes
/// do not touch cannot overlap, and skipping those is what keeps a sheet-sized
/// check from being quadratic in practice.
///
/// THE SAME CORE AS THE REPAIRS (TODOS C-09): duplicates, lines of no length and
/// repeated corners come from `core::find_redundant` — what TEMİZLE repairs — and
/// the gaps of a LINE network from `core::Network`, what SINIR and ALANÜRET
/// refuse to close across. `tolerance` is the project's node tolerance. Faces
/// are not noded here: a sheet of parcels is checked pairwise, within the
/// §10.1 budget, and a coverage check over parcels is its own work (G-05).
std::vector<Defect> check_topology(const core::Document& doc,
                                   const std::vector<core::EntityKey>& keys,
                                   core::Mm tolerance = 10);

/// The Turkish sentence a user reads for one defect.
std::string describe(const core::Document& doc, const Defect& d);

} // namespace kentos::domain::cadastre
