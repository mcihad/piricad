// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the ghost a drawing prompt shows (TODOS C-02).
//
// THE GHOST IS THE OBJECT. While a drawing command waits for its next point the
// canvas draws what that point would make — the segment, the face, the circle,
// the arc, the ellipse, the polygon, the rectangle, the curve. That shape is
// worked out HERE, once, from the prompt the command put up and by the core
// constructions the command bodies commit with, at the density the kind itself
// is drawn at. So the ghost under the cursor and the object the click writes
// are not two answers that usually agree: they are one answer, and a test can
// prove it without a window (tests/unit/test_draw_methods.cpp).
//
// Before this file each canvas branch carried its own copy of the arithmetic —
// the circle's radius, the arc's, the ellipse's second axis, the polygon's
// rotation, the spline's sampling — and ÇOKGEN's copy had already drifted: with
// `aci` given it turned the ghost towards the cursor while the click drew the
// polygon at `aci`.
//
// What is not the object — the radius line to the cursor, a control polygon,
// the reference a guide hangs from — is decoration, and stays the canvas's.
#pragma once

#include "kentos_cad/command/input.hpp"
#include "kentos_cad/core/angle.hpp"

#include <vector>

namespace kentos::command {

/// One drawn run of a ghost, in world millimetres.
struct GhostRun
{
    std::vector<core::Point2> points; ///< the vertices, the closing one not repeated
    bool closed{false};               ///< the segment back to the first vertex is drawn

    /// Both fields: a ghost is the object only if it closes where the object does.
    friend bool operator==(const GhostRun&, const GhostRun&) = default;
};

/// The object answering `prompt` with `at` would make, as the runs it is drawn
/// with. Empty when the prompt's shape does not make an object by itself (a
/// selection, a measurement, an edit) or when `at` makes none — on the centre,
/// three points in a line, a chain too short, a rectangle with no width.
///
/// `convention` is the bus's angle convention: ÇOKGEN measures its rotation
/// under it, and the canvas must use the one the command will, not a copy.
std::vector<GhostRun> ghost_outline(const Prompt& prompt, core::Point2 at,
                                    core::AngleConvention convention);

} // namespace kentos::command
