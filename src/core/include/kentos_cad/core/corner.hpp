// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: cutting a corner off a run, straight (PAH) or round (YUVARLA).
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. PAH and YUVARLA compute their
// result here, and the canvas draws the cut under the cursor by calling the same
// function with the cursor's distance from the corner — so what the preview
// shows is what the typed number or the click makes, not a second computation
// that agrees on the easy cases (Article 1.2).
//
// EXACT WHERE IT CAN BE. The tangent points come from unit vectors built with
// `sqrt`, which IEEE-754 rounds correctly, and never from `atan2` and a
// trigonometric call, which are not required to agree between platforms (§7.3).
// The half-angle identities below exist for that reason and not to save a call.
//
// ON AN OPEN LINE A FILLET'S ARC IS A SEPARATE OBJECT, because a polyline in
// this model holds vertices and not bulges (model.md R9-R12): the arc is a real
// arc with a real centre and radius, so its length and geometry are exact. A
// CLOSED RING cannot be broken in two without ceasing to enclose anything, so
// there the arc is drawn into the ring with the points a YAY is drawn with, and
// the command says how far that drawing strays from the true arc.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// What cutting one corner makes.
struct CornerCut
{
    /// The run that keeps the object: for a chamfer the whole run with the
    /// corner replaced by its two tangent points; for a fillet on an open line
    /// the leg up to the first tangent point; for a fillet on a closed ring the
    /// whole ring with the arc drawn in between the tangent points.
    std::vector<Point2> kept;

    /// A fillet's second leg, from the second tangent point on; empty for a
    /// chamfer, which stays one object.
    std::vector<Point2> second;

    bool arc{false};      ///< a fillet on an open line: the arc below joins the two legs
    bool rounded{false};  ///< a fillet on a closed ring: the arc is drawn into `kept`
    std::size_t edges{0}; ///< for `rounded`: how many straight edges draw the arc
    Mm deviation{0};      ///< for `rounded`: the most any of them strays inside the arc
    Point2 centre{};      ///< the arc's centre, on the corner's bisector
    Mm radius{0};         ///< the arc's radius: the fillet radius asked for
    Point2 start{};       ///< the arc runs counter-clockwise from here...
    Point2 end{};         ///< ...to here
    Point2 cut_a{};       ///< the tangent point toward the previous vertex
    Point2 cut_b{};       ///< the tangent point toward the next vertex
};

/// The vertex of `run` nearest `probe`, when it IS a corner — one with an edge
/// on each side. Nothing when the nearest vertex is an end of an open run, which
/// has one edge and nothing to cut across, and nothing when the run has fewer
/// than three vertices.
std::optional<std::size_t> nearest_corner(std::span<const Point2> run, bool closed, Point2 probe);

/// Cuts the corner at vertex `at` of `run`.
///
/// `size` is the distance cut back along each edge for a chamfer and the arc's
/// radius for a fillet, in millimetres. Refused, with the sentence a user reads,
/// when the corner is straight and when the cut would run past a neighbouring
/// vertex.
Result<CornerCut> cut_corner(std::span<const Point2> run, bool closed, std::size_t at, Mm size,
                             bool fillet);

/// The payload a corner preview carries (`command::RubberShape::Corner`): the
/// object by its persistent key, which vertex, and which cut — so the canvas can
/// call `cut_corner` with the cursor's distance from the corner.
struct CornerPreview
{
    std::int64_t key{0}; ///< the object, by persistent key
    std::uint32_t at{0}; ///< the vertex index
    bool fillet{false};  ///< round rather than straight
};

/// The preview as bytes.
std::vector<std::uint8_t> encode_corner_preview(const CornerPreview& preview);

/// The preview back, refused when the bytes are not what the encoder writes.
Result<CornerPreview> decode_corner_preview(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
