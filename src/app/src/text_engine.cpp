// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/text_engine.hpp"

#include "kentos_cad/app/data_root.hpp"
#if KENTOS_HAVE_TEXT
#include "kentos_cad/render/text_atlas.hpp"
#endif

#include <memory>
#include <utility>

namespace kentos::app {

render::TextAtlas* text_engine()
{
#if KENTOS_HAVE_TEXT
    thread_local std::unique_ptr<render::TextAtlas> engine;
    thread_local bool tried = false;
    if (!tried) {
        tried = true;
        if (auto opened = render::TextAtlas::open(data_path("fonts")); opened)
            engine = std::move(opened.value());
    }
    return engine.get();
#else
    return nullptr;
#endif
}

std::vector<command::MissingGlyph> missing_glyphs(std::string_view utf8)
{
    std::vector<command::MissingGlyph> out;
#if KENTOS_HAVE_TEXT
    render::TextAtlas* engine = text_engine();
    if (engine == nullptr) return out;
    for (render::UncoveredCharacter& c : engine->uncovered(render::Face::Sans, utf8))
        out.push_back(command::MissingGlyph{c.code, std::move(c.utf8)});
#else
    (void)utf8;
#endif
    return out;
}

} // namespace kentos::app
