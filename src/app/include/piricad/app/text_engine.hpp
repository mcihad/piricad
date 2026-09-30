// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the text engine the shell checks captions with outside a frame.
//
// ONE SHAPER, THE CANVAS'S OWN. The PDF breaks lines with it, the canvas asks
// it which letters of a caption the typeface lacks, and NESNEBİLGİ is answered
// through it (`command::Bus::on_glyph_query`) — so the three agree with what
// the GPU canvas draws, which is the same shaper over the same bundled faces.
#pragma once

#include "kentos_cad/command/bus.hpp"

#include <string_view>
#include <vector>

namespace kentos::render {
/// The shaper and glyph atlas over the bundled faces (`render/text_atlas.hpp`).
class TextAtlas;
} // namespace kentos::render

namespace kentos::app {

/// The shaper over the bundled faces, opened once per thread on first use: a
/// `render::TextAtlas` is not shared between threads. Null in a build without
/// the text engine, or when the faces are not where the data is.
render::TextAtlas* text_engine();

/// The characters of `utf8` the drawing's typeface cannot draw, each once, in
/// the order they appear (TODOS C-12). Empty when there is no engine to ask.
std::vector<command::MissingGlyph> missing_glyphs(std::string_view utf8);

} // namespace kentos::app
