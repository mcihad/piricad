// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: what a measurement leaves on the canvas once it has answered.
//
// A MEASUREMENT THAT VANISHES IS A MEASUREMENT TAKEN TWICE. ÖLÇ, ALANÖLÇ,
// AÇIÖLÇ and KOORDİNAT answered in the transcript and left the canvas as it was,
// so the figure the user had just read was gone from the drawing it described —
// and measuring three sides of a parcel meant three answers in a panel and no
// picture of which was which. The command now also says WHAT it measured, as
// points and words, and a client that has a canvas draws it there.
//
// NOT DOCUMENT STATE. A mark is never journalled, never undone, never saved:
// the command that makes it is read-only and stays so (model.md R43). A client
// with no canvas — a script, a test, an agent — leaves `Bus::on_measure_mark`
// unset and loses nothing, because every figure a mark carries is also in the
// command's words and its report.
#pragma once

#include "kentos_cad/core/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::command {

/// One measurement, as the canvas draws it.
struct MeasureMark
{
    /// What was measured.
    enum class Shape : std::uint8_t {
        Run,   ///< a run of segments: `points` are its vertices; `labels` one per segment,
               ///< then the total when there is more than one
        Ring,  ///< a closed face: `points` are its corners; `labels[0]` its area and perimeter
        Angle, ///< an angle: `points` are the vertex and a point on each arm; `labels[0]` the
               ///< reading
        Point, ///< one point: `points[0]`; `labels[0]` what was read there
        Gap,   ///< an end that meets nothing: `points[0]` the end, `points[1]` the nearest
               ///< linework when there is any; `labels[0]` the gap's width (SINIR)
    };

    Shape shape{Shape::Run};          ///< which of the five
    std::vector<core::Point2> points; ///< see `Shape`
    std::vector<std::string> labels;  ///< see `Shape`, in the user's language and units
};

} // namespace kentos::command
