// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the GRIPS of an entity, and what moving one does.
//
// A polyline's grips are its corners, and moving one is replacing a vertex.
// Every other kind stores a DEFINITION rather than a boundary — a circle keeps
// a centre and a radius handle, an arc its two ends and a radius, an ellipse
// two axis ends, a dimension its definition points, a block reference an
// insertion point — and until this existed those definitions could not be
// edited at all: `KÖŞETAŞI` refused every kind but the polyline, and the canvas
// drew handles on nothing else. A user who had drawn a circle a metre too wide
// erased it and drew it again, minting a new key and dropping its attributes.
//
// So every kind names its grips HERE, once, and says what a grip means when it
// moves: a circle's centre translates the circle, its quadrant handle sets the
// radius; an arc's end moves that end and re-fits the radius; an ellipse's axis
// end turns and stretches the axis and keeps the other axis perpendicular; an
// arc polyline's corner keeps the bulge of the arcs that meet at it, and the
// midpoint of an arc re-fits the arc through three points; a dimension's
// definition point re-lays the dimension out (`dimension_layout`) so the line,
// the extension lines and the caption follow. The command (`KÖŞETAŞI`), the
// canvas that draws the handles, and the preview under a dragged handle all
// read this one table, so what a handle promises is what the command does.
//
// Numbering: grip `i` (0-based) is corner `i + 1` at the command line, and for
// a polyline that is the vertex numbering `KÖŞETAŞI` always used (R11 order
// across the rings). For every other kind the order is the one this header's
// `entity_grips` states, and the documentation of the kind names it.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kentos::core {

/// What a grip is on the shape, so the canvas can draw it as such and a
/// message can name it.
enum class GripRole : std::uint8_t {
    Vertex,     ///< a corner of a polyline, spline, hatch, leader or ordinate dimension
    Centre,     ///< the centre of a circle, arc or ellipse: moving it translates the shape
    Radius,     ///< a quadrant handle of a circle, or the midpoint of an arc: sets the radius
    Endpoint,   ///< an end of an arc
    AxisEnd,    ///< an axis end of an ellipse (or its mirror image)
    ArcMid,     ///< the midpoint of a bent edge of an arc polyline: re-fits the bend
    Definition, ///< a definition point of a dimension
    Caption,    ///< where a dimension's text sits
    Insertion,  ///< the insertion point of a block reference
};

/// One grip: where it is and what it is.
struct GripPoint
{
    Point2 at{};                     ///< where the grip sits, document millimetres
    GripRole role{GripRole::Vertex}; ///< what moving it does
};

/// The grips of a live entity, in the order that numbers them. Empty for an
/// entity this build cannot edit (unknown kind, inside a block definition).
std::vector<GripPoint> entity_grips(const Document& doc, EntityId e);

/// The result of moving a grip: new rings and, for a kind whose payload holds
/// coordinates or a measurement, a new payload. What `Transaction::set_kind_geometry`
/// takes, held in a form that owns its vertices.
struct GripEdit
{
    std::vector<std::vector<Point2>> points; ///< one vector per ring, R11 order
    std::vector<RingRole> roles;             ///< the role of each ring
    std::vector<std::uint16_t> parts;        ///< the part of each ring
    std::vector<std::uint8_t>
        payload; ///< the payload to store alongside — the old bytes when unchanged

    /// A dimension's caption must be re-laid after its definition changes: where
    /// it is centred and which way it reads, from `dimension_layout`. The text
    /// itself needs the drawing unit, which the command has and this does not,
    /// so the command redoes ring 0 with `dimension_baseline` once it has the
    /// text. Unset for every other kind.
    std::optional<Point2> caption_centre;
    double caption_dir_x{1.0};
    double caption_dir_y{0.0};

    /// The rings as `set_kind_geometry` wants them. The spans borrow from
    /// `points`; the edit must outlive the call.
    std::vector<RingGeometry::RingInput> inputs() const;
};

/// Moves grip `index` of `e` to `to`. An error names why the shape refuses:
/// a radius of zero, an ellipse axis of zero, a grip the entity does not have.
Result<GripEdit> move_grip(const Document& doc, EntityId e, std::size_t index, Point2 to);

/// The drawn form `e` would have with grip `index` at `to`: what the canvas
/// shows under the pointer while the handle is dragged. Uses the kind's own
/// outline over the edited geometry, so the preview is the future drawing.
/// False when the move is refused; the caller then draws nothing.
bool grip_preview(const Document& doc, EntityId e, std::size_t index, Point2 to, EmitBuffer& into);

} // namespace kentos::core
