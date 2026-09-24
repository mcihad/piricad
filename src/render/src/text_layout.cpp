// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/text_layout.hpp"

namespace kentos::render {
namespace {

/// The paragraph `para` broken into lines no wider than `wrap_px` (in pixels at
/// the caption's height), at spaces; appended to `out` unplaced.
void wrap_paragraph(std::string_view para, float height_px, float wrap_px,
                    const MeasureRun& measure, std::vector<TextLine>& out)
{
    std::size_t start = 0; // where the line being filled begins
    std::size_t end   = 0; // where it ends so far: after its last whole word
    std::size_t at    = 0;
    while (at <= para.size()) {
        std::size_t word_end = para.find(' ', at);
        if (word_end == std::string_view::npos) word_end = para.size();
        // The line with this word on it, spaces between included: one view,
        // one measurement, no copy.
        const std::string_view with = para.substr(start, word_end - start);
        const bool first_word       = end == start;
        if (!first_word && measure(with) * height_px > wrap_px) {
            out.push_back(TextLine{para.substr(start, end - start)});
            start = at; // the space that broke the line belongs to neither
        }
        end = word_end;
        if (word_end == para.size()) break;
        at = word_end + 1;
    }
    out.push_back(TextLine{para.substr(start, end - start)});
}

} // namespace

void lay_out_text(std::string_view text, float height_px, core::TextAnchor anchor,
                  core::TextLines lines, float wrap_px, const MeasureRun& measure,
                  std::vector<TextLine>& out)
{
    out.clear();
    if (text.empty() || !(height_px > 0.0f)) return;

    // ---- the breaks: every newline, and the spaces the width asks for ----
    //
    // A BLANK LINE IS KEPT. It is how a paragraph is set apart from the next,
    // and dropping it — which both backends used to do — pulled the paragraphs
    // together on the screen that the file still kept apart.
    std::size_t at = 0;
    while (true) {
        const std::size_t nl        = text.find('\n', at);
        const std::size_t to        = nl == std::string_view::npos ? text.size() : nl;
        const std::string_view para = text.substr(at, to - at);
        const bool wraps            = lines.wrap && wrap_px > 0.0f;
        if (wraps && !para.empty())
            wrap_paragraph(para, height_px, wrap_px, measure, out);
        else
            out.push_back(TextLine{para});
        if (nl == std::string_view::npos) break;
        at = nl + 1;
    }

    // ---- the stack: the pitch, and the row the anchor names ----
    const float pitch = static_cast<float>(core::kTextLinePitch) *
                        (static_cast<float>(lines.spacing) / 1000.0f) * height_px;
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
        line.u         = column == 1 ? -line.advance * 0.5f : column == 2 ? -line.advance : 0.0f;
    }
}

} // namespace kentos::render
