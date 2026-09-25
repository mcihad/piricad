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

#include "kentos_cad/command/job.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/result.hpp"
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
    std::vector<core::Point2> region{};            ///< Overlap: the largest shared piece
};

/// Checks every entity in `keys`, or the whole drawing when it is empty.
///
/// Pairwise for overlaps, and only between parcels whose boxes touch: an STR
/// R-tree over the parcels' boxes (`core::SpatialIndex`) names each one's
/// neighbours, so the check grows with the parcels and not with their square.
/// The every-pair loop this replaced took 0,7 s for 16 000 parcels and would
/// have taken half a minute for the §10.1 budget's 100 000 (TODOS F-05).
///
/// THE SAME CORE AS THE REPAIRS (TODOS C-09): duplicates, lines of no length and
/// repeated corners come from `core::find_redundant` — what TEMİZLE repairs — and
/// the gaps of a LINE network from `core::Network`, what SINIR and ALANÜRET
/// refuse to close across. `tolerance` is the project's node tolerance. Faces
/// are not noded here: a sheet of parcels is checked pairwise, within the
/// §10.1 budget, and a coverage check over parcels is its own work (G-05).
///
/// LONG WORK (command/job.hpp): it counts on `control` across its four passes
/// and, asked to stop, returns `ErrorCode::Cancelled` and no findings — half a
/// check is not a smaller check, and "no defects" from one would be a lie. It
/// only reads `doc`, so it may run on a worker while nothing writes the
/// drawing (`command::Bus::writable`).
core::Result<std::vector<Defect>> check_topology(const core::Document& doc,
                                                 const std::vector<core::EntityKey>& keys,
                                                 core::Mm tolerance                 = 10,
                                                 const command::JobControl& control = {});

/// The Turkish sentence a user reads for one defect.
std::string describe(const core::Document& doc, const Defect& d);

/// An area as the report writes one: square metres to two decimals, with a
/// decimal comma (`25,00 m²`).
std::string square_metres(core::Mm2 area);

} // namespace kentos::domain::cadastre
