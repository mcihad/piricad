// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: a region asked for by a point inside it.
//
// SINIR asked for one first, and ALANÖLÇ `yontem=ic` asks the same question —
// Netcad's `Alan Seçim Aracı`, "the area round the point I click" (wiki
// 217387115). Asked in one place, so the prompt, the live region under the
// cursor, the click's freedom from the aids and every refusal are said once:
// an open region is refused in one sentence whichever command asked.
#pragma once

#include "piricad/command/context.hpp"
#include "piricad/command/task.hpp"
#include "piricad/core/planar.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace piricad::command {

/// A REGION FOUND BY A POINT INSIDE IT (`ask_region`).
struct FoundRegion
{
    core::RegionQuery query;        ///< how it was asked: the point, islands, bridge, boundary set
    core::Region region;            ///< what was found; `region.face` is always set
    std::vector<std::int64_t> keys; ///< the boundary set as given, by persistent key
};

/// THE REGION AROUND A POINT, asked the way SINIR asks it.
///
/// `nokta` is taken when given, and otherwise shown — with the region drawn
/// under the cursor and no input aid on the click (`PointOptions::aids`): a
/// point that only has to be inside must not be carried onto the linework it is
/// inside. `ada` and `bosluk` are read when given; `nesneler` is read as the
/// boundary set only when `boundary_set` says the command means it so (SINIR
/// does; ALANÖLÇ's `nesneler` are the objects it measures).
///
/// Nothing comes back when the question was refused — a point on a line,
/// nothing closed round it, a region with open ends (marked on the canvas, the
/// gap to the nearest linework measured in the sentence) — or when the user
/// let go. The caller then returns.
Task<std::optional<FoundRegion>> ask_region(Context& ctx, std::string prompt, bool boundary_set);

/// Records how the region was asked — the point, and `ada`, `bosluk` and the
/// boundary set when they were given — so a replay asks it the same way.
void record_region(Context& ctx, const FoundRegion& found);

/// A length the way a gap is read: `5 cm`, `12 mm`, `1,25 m`.
std::string gap_words(core::Mm v);

} // namespace piricad::command
