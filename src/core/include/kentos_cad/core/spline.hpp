// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a NURBS curve.
//
// A spline is its CONTROL POINTS, its degree and its knots; what is drawn is a
// picture of them. Ring 0 holds the control points, ring 1 (when the source had
// them) the fit points; degree, knots and weights are the payload (model.md
// R9a). Knots and weights are stored as nano-fixed-point integers, because no
// stored field is floating point (R21).
//
// THE DRAWING IS DE BOOR'S ALGORITHM, forty lines of convex combinations over a
// frame translated to the first control point, with the knots read back as
// doubles. Nothing but +, −, × and ÷ touches it, so three platforms agree to
// the millimetre (§7.3). A library was considered — tinynurbs, OpenNURBS — and
// rejected because neither pins its operation order, and the order is the
// determinism (CLAUDE.md Article 9, the hand-rolled exception, stated here).
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// One nano-unit: knots and weights are stored as value × 10⁹.
inline constexpr std::int64_t kNano = 1'000'000'000;

/// The payload of a `core.spline` slot.
struct SplineDef
{
    std::uint8_t degree{3}; ///< 1..15
    bool closed{false};     ///< the drawn run closes back to its start
    bool periodic{false};   ///< the source called it periodic (DXF flag 2); kept for the round trip
    bool rational{false};   ///< weights carry meaning (DXF flag 4)
    bool has_fit{false};    ///< ring 1 holds the fit points the source gave
    bool planar{true};      ///< DXF flag 8; always true for a plan
    bool linear{false};     ///< DXF flag 16; kept for the round trip

    /// `controls + degree + 1` knots, non-decreasing, or none — then the curve is
    /// drawn over uniform clamped knots (`uniform_clamped_knots`).
    std::vector<std::int64_t> knots_nano;

    /// One weight per control point, all positive, or none for a non-rational curve.
    std::vector<std::int64_t> weights_nano;

    friend bool operator==(const SplineDef&, const SplineDef&) = default;
};

/// The payload layout version `encode_spline` writes.
inline constexpr std::uint16_t kSplineLayout = 1;

/// The payload bytes: the R9a header (flags), the degree, the knots, the weights.
std::vector<std::uint8_t> encode_spline(const SplineDef& def);

/// The payload back, refused when the bytes are not what `encode_spline` writes,
/// the degree is out of range, the knots decrease or a weight is not positive.
Result<SplineDef> decode_spline(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<SplineDef> spline_of(const RingGeometry& geom, std::uint32_t slot);

/// The knot vector `SPLINE` draws with when the user gives only control points:
/// `degree + 1` zeros, the interior knots evenly spaced, `degree + 1` ones — all
/// in nano units. Empty when `controls < degree + 1`.
std::vector<std::int64_t> uniform_clamped_knots(std::size_t controls, int degree);

/// Appends the DRAWN form: `samples_per_span` points on every non-empty knot
/// span, the curve's end included exactly once. Empty when the definition does
/// not fit the control points (the validate refused it on the way in).
void spline_points(std::span<const Point2> controls, const SplineDef& def, int samples_per_span,
                   std::vector<Mm>& xs, std::vector<Mm>& ys);

/// The drawn form of the slot at the kind's density (16 per span), and whether
/// it closes.
bool spline_outline(const RingGeometry& geom, std::uint32_t slot, std::vector<Mm>& xs,
                    std::vector<Mm>& ys);

} // namespace kentos::core
