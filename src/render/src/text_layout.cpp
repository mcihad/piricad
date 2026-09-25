// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/text_layout.hpp"

#include "kentos_cad/core/text_metrics.hpp"

namespace kentos::render {

float drawing_measure(std::string_view run) noexcept
{
    const auto cap = static_cast<float>(core::text_face().cap_height);
    return static_cast<float>(core::text_run_advance(run)) / cap;
}

void lay_out_text(std::string_view text, float height_px, core::TextAnchor anchor,
                  std::uint16_t spacing, const MeasureRun& measure, std::vector<TextLine>& out)
{
    out.clear();
    if (text.empty() || !(height_px > 0.0f)) return;

    // ---- the breaks: every newline ----
    //
    // A BLANK LINE IS KEPT. It is how a paragraph is set apart from the next,
    // and dropping it — which both backends used to do — pulled the paragraphs
    // together on the screen that the file still kept apart.
    std::size_t at = 0;
    while (true) {
        const std::size_t nl = text.find('\n', at);
        const std::size_t to = nl == std::string_view::npos ? text.size() : nl;
        out.push_back(TextLine{text.substr(at, to - at)});
        if (nl == std::string_view::npos) break;
        at = nl + 1;
    }

    // ---- the stack: the pitch, and the row the anchor names ----
    const float pitch = static_cast<float>(core::kTextLinePitch) *
                        (static_cast<float>(spacing) / 1000.0f) * height_px;
    const auto n = static_cast<float>(out.size());
    float first  = 0.0f; // the first line's baseline, from the anchor, down
    switch (core::text_anchor_row(anchor)) {
    case 2: first = height_px; break; // the capital tops of the first line
    case 1: {                         // halfway between those and the last baseline
        const float block = height_px + (n - 1.0f) * pitch;
        first             = height_px - block * 0.5f;
        break;
    }
    default: first = -(n - 1.0f) * pitch; break; // the last line's baseline
    }

    // ---- each line on its own: measured, and moved to its column ----
    const int column = core::text_anchor_column(anchor);
    for (std::size_t i = 0; i < out.size(); ++i) {
        TextLine& line = out[i];
        line.advance   = line.text.empty() ? 0.0f : measure(line.text) * height_px;
        line.v         = first + static_cast<float>(i) * pitch;
        line.u         = 0.0f; // the left column
        if (column == 1) line.u = -line.advance * 0.5f;
        if (column == 2) line.u = -line.advance;
    }
}

} // namespace kentos::render
