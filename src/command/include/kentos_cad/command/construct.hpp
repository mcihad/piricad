// SPDX-License-Identifier: GPL-3.0-or-later
// The constructions a surveyor names, over VALUES rather than over tokens.
//
// Two clients build the same points. `kes(...)`, `ara(...)` and `uzanti(...)` in
// the one grammar (`point_function.hpp`) resolve a typed expression; the
// `KESİŞİMNOKTA` and `ARANOKTA` commands ask a hand for the same inputs and draw
// the answers. What must NOT differ between them is the arithmetic, which
// solution of two is chosen, and what a refusal says — a construction whose
// error message reads one way typed and another way clicked is two features
// wearing one name (CLAUDE.md 5.10, Article 1.2).
//
// The pure geometry lives in `/src/core` (`line_intersection`,
// `circle_intersection`, `perpendicular_offset`). What lives here is the part
// that needs this program's vocabulary: the angle convention, the parallel test
// in integers, the left/right choice, and the Turkish refusal that names both
// figures (`command.md` R19).
#pragma once

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

namespace kentos::command {

/// Which of the two solutions a two-distance intersection has.
///
/// THERE ARE TWO AND CHOOSING ONE IS THE CALLER'S JOB. `core::circle_intersection`
/// refuses to pick; so does this file. A silent pick is what puts a boundary on
/// the wrong side of a road.
enum class Side : std::uint8_t {
    Left,  ///< the solution on the left of the direction a→b
    Right, ///< the one on its right
};

/// Where the direction `angle_a` from `a` crosses the direction `angle_b` from
/// `b`, each angle written in its own convention's unit and counted under its
/// rule.
///
/// Parallel is decided in INTEGERS before any trigonometry: two directions a
/// half turn apart give ray endpoints whose cross product is a rounding artefact
/// rather than zero, and a line intersection would then answer with a point
/// somewhere past the moon instead of refusing.
core::Result<core::Point2> direction_crossing(core::Point2 a, double angle_a,
                                              core::AngleConvention convention_a, core::Point2 b,
                                              double angle_b, core::AngleConvention convention_b);

/// Where the circle of radius `r1_m` about `a` meets the circle of radius `r2_m`
/// about `b`, on the named side.
///
/// A corner re-established from two tape measurements off two known monuments.
/// Every refusal names both radii AND the distance between the centres, so the
/// user can see at a glance which measurement is the wrong one.
core::Result<core::Point2> distance_crossing(core::Point2 a, double r1_m, core::Point2 b,
                                             double r2_m, Side side);

/// Where the line a→b crosses the line c→d, as INFINITE lines: the corner two
/// boundaries WOULD make, which is the point an ifraz needs when the monument is
/// gone.
core::Result<core::Point2> line_crossing(core::Point2 a, core::Point2 b, core::Point2 c,
                                         core::Point2 d);

/// The point at fraction `t` of the way from `a` to `b`; `t` outside [0, 1] runs
/// past an end, which is what an extension is.
core::Result<core::Point2> along_ratio(core::Point2 a, core::Point2 b, double t);

/// The point `distance_m` metres from `a` towards `b`.
core::Result<core::Point2> along_distance(core::Point2 a, core::Point2 b, double distance_m);

/// `distance_m` metres past `b`, along a→b.
core::Result<core::Point2> beyond(core::Point2 a, core::Point2 b, double distance_m);

/// Metres with three decimals, in integers, for a refusal that names a figure.
std::string metres_text(core::Mm v);

} // namespace kentos::command
