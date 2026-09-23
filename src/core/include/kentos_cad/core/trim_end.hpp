// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: pulling a line's end back to a boundary, or pushing it out.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. BUDA and UZAT compute their edit
// here, and the canvas draws it under the cursor with the same call — the piece
// a trim throws away marked as going, the reach an extension adds drawn as
// coming — so what the preview promises is what the click does.
//
// THE END SEGMENT, AND ONLY IT. A trim pulls the end nearer the pick back to
// where the boundary crosses the segment at that end; an extension pushes that
// end out along its own direction to the first crossing past it. A crossing
// deeper inside the line, and a boundary that is a curve, are C-04's to add.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// What BUDA or UZAT makes of one line.
struct TrimEnd
{
    std::vector<Point2> run;     ///< the line as it will be
    std::vector<Point2> changed; ///< a trim: the piece cut off; an extension: the reach added
    Point2 cut{};                ///< where the moved end now is
    bool at_start{false};        ///< the start moved, rather than the end
};

/// BUDA (`extend` false) or UZAT (`extend` true) on the open `target`: its end
/// nearer `pick` pulled back to, or pushed out to, the crossing of `boundary`
/// nearest that end. Refused, with the sentence a user reads, when there is no
/// such crossing.
Result<TrimEnd> trim_end(std::span<const Point2> target, std::span<const Point2> boundary,
                         Point2 pick, bool extend);

/// Whether `pick` names `a` rather than `b` as the line to edit: the one it
/// lands nearer. What a click decides when two lines are selected and neither
/// was named — you click the piece you want gone.
bool picks_first(std::span<const Point2> a, std::span<const Point2> b, Point2 pick);

/// The payload BUDA's and UZAT's pick prompt carries (`command::RubberShape::Trim`).
struct TrimGuide
{
    std::int64_t first{0};  ///< the line to edit — or, when `paired`, one of the two
    std::int64_t second{0}; ///< the boundary — or, when `paired`, the other
    bool paired{false};     ///< the pick decides which of the two is edited
    bool extend{false};     ///< UZAT rather than BUDA
};

/// The guide as bytes.
std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<TrimGuide> decode_trim_guide(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
