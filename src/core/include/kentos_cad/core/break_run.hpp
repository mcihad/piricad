// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: breaking an open run, KIR's cut.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. KIR cuts here, and the canvas
// draws the piece about to go under the cursor by calling the same function
// with the cursor as the second point — so what the preview marks for removal
// is what the click removes, not a second computation that agrees on the easy
// cases (Article 1.2).
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// What breaking a run between two points makes.
struct BreakCut
{
    /// The run before the gap. Fewer than two points when the gap reaches the
    /// run's start: that end is broken off and nothing is left before it.
    std::vector<Point2> head;

    /// What the break takes away, from the first cut to the second, along the
    /// run. A single point when both cuts fall together: a split with no gap.
    std::vector<Point2> gap;

    /// The run after the gap; fewer than two points when the gap reaches the end.
    std::vector<Point2> tail;

    Point2 first{};  ///< the first cut, on the run
    Point2 second{}; ///< the second cut, on the run
};

/// Breaks the open `run` between the points of it nearest `a` and `b`, ordered
/// ALONG the run whichever was given first — a hand clicks the far end first as
/// often as not, and a gap is a gap either way. `a == b` splits with no gap.
/// Refused, with the sentence a user reads, when the run has no segment and
/// when the break would leave nothing of it.
Result<BreakCut> break_run(std::span<const Point2> run, Point2 a, Point2 b);

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
