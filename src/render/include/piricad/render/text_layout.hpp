// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: where the lines of a caption go.
//
// ONE ANSWER FOR THE SCREEN AND THE PAPER (TODOS C-12). The GPU canvas and the
// QPainter path — which is also the PDF and the printer — used to break, stack
// and align a caption each in its own way: one dropped a blank line and the
// other kept it, one stepped 1,25 EM between lines and the other 1,25 of the
// font's own height, so the sheet on the screen and the sheet on paper disagreed
// about where a plan note's second line was. Both now ask this function, with
// the same measure, and draw what it answers.
//
// THE RULES ARE DXF MTEXT'S, so a text written to a file lands where it was
// drawn: the pitch between baselines is `core::kTextLinePitch` times the line
// spacing; each line is aligned on its own; the row of the anchor puts it on
// the first line's capital tops, halfway down the block, or on the last line's
// baseline (`core::TextAnchor`). A break is a newline. A text that WRAPS
// arrives already broken: the scene hands over the lines `core::text_lines`
// set it in, which are the lines the text's box counts (TODOS C-18) — one
// place decides where a line breaks, and it is not here.
//
// Qt-free and font-free: the caller hands in how wide a run is, which is what
// lets the suite check the rules with a ruler of its own. Both backends hand
// in `drawing_measure`, the core's measure of the drawing face.
#pragma once

#include "kentos_cad/core/text_store.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace kentos::render {

/// One laid-out line, in the caption's own frame and in pixels: `u` along the
/// baseline from the anchor, `v` across it, DOWN positive — the way text is set
/// on a page.
struct TextLine
{
    std::string_view text; ///< a view into the caption, break characters excluded
    float u{0.0f};         ///< where the line's baseline starts, along
    float v{0.0f};         ///< where its baseline lies, across
    float advance{0.0f};   ///< the line's width
};

/// How wide `run` is when its capital letters are one pixel tall.
using MeasureRun = std::function<float(std::string_view run)>;

/// How wide `run` of a drawing's text is when its capital letters are one pixel
/// tall: its advance in the drawing face's units over the face's cap height,
/// by the core's own measure (`core::text_run_advance`) — the same width the
/// text's box has, and the width the technical spacing of either backend
/// draws it at.
float drawing_measure(std::string_view run) noexcept;

/// Lays `text` out at a capital height of `height_px`: breaks it at its
/// newlines, stacks the lines `spacing` apart (thousandths of
/// `core::kTextLinePitch`, as `core::TextLines::spacing`) and aligns them to
/// `anchor`. `out` is cleared first. An empty text lays out as nothing.
void lay_out_text(std::string_view text, float height_px, core::TextAnchor anchor,
                  std::uint16_t spacing, const MeasureRun& measure, std::vector<TextLine>& out);

} // namespace kentos::render
