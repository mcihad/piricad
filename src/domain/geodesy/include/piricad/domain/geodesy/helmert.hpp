// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — geodesy: the 2D similarity (Helmert) transformation.
//
// WHAT IT IS FOR. A survey is very often measured in a LOCAL system: the crew
// sets a station, calls it 0,0, and works from there for a week. The drawing is
// correct in itself — every distance and every angle is right — and it is
// nowhere on the map. Fitting it onto the map is this transformation, and it is
// what `OTURT` runs.
//
// FOUR PARAMETERS: two translations, one rotation, one scale. It moves, turns and
// uniformly scales; it never shears and never stretches one axis more than the
// other, and that is the whole point. A survey's internal geometry is measured
// data — the angles between its own lines are facts — so a fit that could
// deform it would be fitting the measurements to the control instead of the
// other way round.
//
// SCALE MAY BE LOCKED TO 1. A crew working with a calibrated tape or a total
// station has already reduced its distances; letting the fit rescale them would
// silently absorb a control-point error into every length in the drawing. That is
// what `lock_scale` refuses.
//
// DETERMINISM (§7.3, core.md R9). Only +, -, *, / and `sqrt` — every one of them
// correctly rounded by IEEE-754 and, with `-ffp-contract=off` in force, evaluated
// the same way on every platform. No `atan2`, no `sin`, no `cos`: the rotation
// never becomes an ANGLE, it stays the pair (a, b) that the closed-form least
// squares produces, and the angle is derived only for a message a human reads.
//
// The points are reduced to their own centroid in INTEGERS before anything is
// converted, so the doubles only ever see small differences and not TUREF
// eastings — an 8·10^8 mm easting squared leaves the 53-bit mantissa immediately.
#pragma once

#include "piricad/core/geometry.hpp"
#include "piricad/core/result.hpp"

#include <cstddef>
#include <vector>

namespace piricad::domain::geodesy {

/// One control point: where it is in the drawing, and where it belongs on the map.
struct ControlPoint
{
    core::Point2 local{}; ///< as measured, in the drawing's own system
    core::Point2 map{};   ///< the published coordinate it must land on
};

/// The fitted transformation, plus what it cost to fit.
struct Helmert2D
{
    /// `X = a*x - b*y + tx`, `Y = b*x + a*y + ty`. Together a and b carry both
    /// the rotation and the scale; keeping them as a pair is what avoids trig.
    double a{1.0};
    double b{0.0};
    core::Mm tx{0};
    core::Mm ty{0};

    /// `sqrt(a² + b²)`. 1 exactly when the fit was scale-locked.
    double scale{1.0};

    /// Clockwise from north, in grad — the unit a Turkish surveyor reads. Derived
    /// for the report only; nothing transforms with it.
    double rotation_grad{0.0};

    /// Residual per control point, in millimetres: how far the fitted point lands
    /// from where the control says it should. Same order as the input.
    std::vector<core::Mm> residuals;

    /// Root mean square of the residuals, in millimetres. The number a surveyor
    /// judges the fit by.
    core::Mm rms{0};

    /// The largest single residual, in millimetres.
    core::Mm worst{0};

    /// Applies the fit to one point.
    core::Point2 apply(core::Point2 p) const noexcept;
};

/// Fits a similarity transformation taking every `local` onto its `map`.
///
/// Two points give an exact fit and no residuals to speak of; three or more give
/// a least-squares fit and the residuals that say whether the control agrees with
/// itself. Fewer than two is refused: one point fixes a translation and leaves
/// the rotation and the scale unknown, and inventing either would be inventing
/// data.
///
/// `lock_scale` fixes the scale at exactly 1, so the fit only moves and turns the
/// drawing.
core::Result<Helmert2D> fit_helmert(const std::vector<ControlPoint>& points, bool lock_scale);

} // namespace piricad::domain::geodesy
