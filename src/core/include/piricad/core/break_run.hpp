// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: breaking a path, KIR's cut.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. KIR cuts here, and the canvas
// draws the piece about to go under the cursor by calling the same function
// with the cursor as the second point — so what the preview marks for removal
// is what the click removes, not a second computation that agrees on the easy
// cases (Article 1.2).
//
// ANY PATH, NOT ONLY A LINE (TODOS C-05). A line, an arc, a circle and a
// polyline whose edges bend are walked as one `CurvePath`, so the piece taken
// out of an arc is an arc and what stays of a circle is the arc left over.
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// What breaking a path between two places makes.
struct PathBreak
{
    /// What stays, in order along the path: two pieces when the gap is inside
    /// an open path, one when it reaches an end or the path was closed.
    std::vector<CurvePath> kept;

    /// What the break takes away, from the first cut to the second along the
    /// path. Empty when both cuts fall together: a split with no gap.
    CurvePath gap;

    Point2 first{};  ///< the first cut, on the path
    Point2 second{}; ///< the second cut, on the path
};

/// Breaks `path` between the places of it nearest `a` and `b`.
///
/// An OPEN path is broken ALONG its direction whichever point was given first —
/// a hand clicks the far end first as often as not, and a gap is a gap either
/// way — and `a == b` splits it with no gap. A CLOSED path loses the piece from
/// `a` round to `b` in its own direction, counter-clockwise on a circle as every
/// CAD breaks one, and needs two distinct points: one point opens a closed
/// shape and takes nothing out. Refused, with the sentence a user reads, when
/// the break would leave nothing.
Result<PathBreak> break_path(const CurvePath& path, Point2 a, Point2 b);

/// The payload KIR's second prompt carries (`command::RubberShape::Break`): the
/// object, by persistent key.
struct BreakGuide
{
    std::int64_t key{0}; ///< the line, by persistent key
};

/// The guide as bytes.
std::vector<std::uint8_t> encode_break_guide(const BreakGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<BreakGuide> decode_break_guide(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
