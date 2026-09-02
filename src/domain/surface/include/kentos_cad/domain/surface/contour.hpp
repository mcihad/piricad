// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — surface: contours from a levelling survey.
//
// WHAT A SURVEYOR ACTUALLY WANTS. A crew levels a site and comes back with a few
// hundred numbered points, each with a Z. The deliverable is not the triangulation
// — it is the contour lines on the plan sheet, at 0,5 m or 1 m, that a
// designer reads the ground from.
//
// THE TRIANGULATION IS AN INTERMEDIATE and is not stored. Keeping it would add an
// entity kind the rest of the program would have to learn about — culling,
// picking, area, export — for a thing nobody draws (model.md). It is rebuilt when
// the points change, which is also the only way the contours can stay true to
// them.
//
// NOT HAND-ROLLED (CLAUDE.md 2.7, 5.16). A Delaunay triangulation has well-known
// degeneracies — cocircular points, collinear runs, duplicates — and a grid of
// levelling points is cocircular EVERYWHERE, which is exactly where a naive
// implementation produces crossed triangles. CDT does it with Shewchuk's robust
// predicates, which 5.4 already requires.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <vector>

namespace kentos::domain::surface {

/// One levelled point: where it is and how high it is.
struct Level
{
    core::Point2 at{};  ///< easting and northing, in millimetres
    core::Mm height{0}; ///< the Z, in millimetres
};

/// One contour: a run of vertices, all at the same height.
struct Contour
{
    core::Mm height{0};             ///< the level this line traces
    std::vector<core::Point2> path; ///< in order; closed when the ends meet
    bool closed{false};             ///< the run comes back to where it started
};

/// Traces contours through `points` at every whole multiple of `interval`.
///
/// The points are triangulated and each triangle is cut at every level that
/// crosses it; the resulting segments are chained into runs.
///
/// EXACT CHAINING. A level crossing an edge is interpolated from the edge's two
/// ENDPOINTS in a fixed order, so the two triangles that share that edge compute
/// the identical millimetre for it and the runs join without a tolerance. A
/// chaining that needed one would leave hairline gaps a plot would print.
///
/// Refuses fewer than three points (no surface), a non-positive interval, and —
/// because a bad interval on a large site is how a program is asked for a million
/// lines — more than `kMaxContours` runs.
core::Result<std::vector<Contour>> trace_contours(const std::vector<Level>& points,
                                                  core::Mm interval);

/// How many runs may come back before the request is refused as a mistake.
inline constexpr std::size_t kMaxContours = 200000;

/// True when the triangulator was compiled in.
bool available() noexcept;

} // namespace kentos::domain::surface
