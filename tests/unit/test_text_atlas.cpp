// SPDX-License-Identifier: GPL-3.0-or-later
//
// The SDF glyph atlas (`.claude/render.md` R8). Compiled only when
// `PIRICAD_WITH_TEXT=ON`.
//
// THIS IS WHY THE ATLAS IS QT-FREE. A backend cannot be constructed in a suite
// that links no Qt (CLAUDE.md 3.4), so if the shaping and the field generation
// lived in the backend nothing here could check them — and what they get wrong is
// exactly the kind of thing a screenshot does not show: a Turkish letter that
// quietly became a box, a run whose kerning collapsed, a field with no gradient
// in it.
#include "piricad_test.hpp"

#if PIRICAD_HAVE_TEXT

#include "piricad/render/text_atlas.hpp"

#include <string>
#include <vector>

using namespace piricad;

namespace {

std::string fonts()
{
    return std::string(PIRICAD_DATA_DIR) + "/fonts";
}

/// The atlas, opened once. Building five faces is the expensive part and every
/// case below wants the same one.
render::TextAtlas& shared()
{
    static std::unique_ptr<render::TextAtlas> atlas = [] {
        auto opened = render::TextAtlas::open(fonts());
        REQUIRE(opened.ok());
        return std::move(opened.value());
    }();
    return *atlas;
}

} // namespace

TEST_CASE("TEXT: the five bundled faces open")
{
    auto opened = render::TextAtlas::open(fonts());
    CHECK(opened.ok());

    // A missing font is a PACKAGING failure and must be reported as one, not
    // papered over with whatever the system happens to have.
    auto missing = render::TextAtlas::open(fonts() + "/böyle-bir-dizin-yok");
    CHECK_FALSE(missing.ok());
}

TEST_CASE("TEXT: a Turkish word shapes into one glyph per letter")
{
    std::vector<render::PlacedGlyph> glyphs;
    const render::RunMetrics m = shared().shape(render::Face::Sans, "ÇİĞDEM", glyphs);

    // Six letters, six glyphs — and the point is the ENCODING: `Ç`, `İ` and `Ğ`
    // are two bytes each in UTF-8, so a renderer that walked bytes would produce
    // nine or twelve. This is the check that says the shaper is a shaper.
    CHECK(glyphs.size() == 6);
    CHECK(m.advance > 0.0f);
    CHECK(m.cap > 0.0f);
    CHECK(m.descent < 0.0f);
}

TEST_CASE("TEXT: the dotted and dotless i are different glyphs")
{
    std::vector<render::PlacedGlyph> dotted;
    std::vector<render::PlacedGlyph> dotless;
    shared().shape(render::Face::Sans, "İ", dotted);
    shared().shape(render::Face::Sans, "I", dotless);

    REQUIRE(dotted.size() == 1);
    REQUIRE(dotless.size() == 1);

    // The single most Turkish thing in this file. `İ` and `I` are different
    // letters, not one letter with an accent, and a text stack that folded them
    // together would print `IZGARA` where the shell says `IZGARA` and `Işık` where
    // it says `İşık` (CLAUDE.md 5.6, one layer down).
    CHECK(dotted.front().box != dotless.front().box);
}

TEST_CASE("TEXT: a space advances the pen and draws nothing")
{
    std::vector<render::PlacedGlyph> one;
    std::vector<render::PlacedGlyph> two;
    const float a = shared().shape(render::Face::Sans, "AB", one).advance;
    const float b = shared().shape(render::Face::Sans, "A B", two).advance;

    // Three characters, two glyphs: whitespace has no outline, and a zero-area
    // quad through the pipeline is a draw call that renders nothing.
    CHECK(one.size() == 2);
    CHECK(two.size() == 2);
    CHECK(b > a);
}

TEST_CASE("TEXT: the mono face is monospaced and the sans face is not")
{
    std::vector<render::PlacedGlyph> scratch;

    const float mono_i = shared().shape(render::Face::Mono, "i", scratch).advance;
    const float mono_m = shared().shape(render::Face::Mono, "m", scratch).advance;
    const float sans_i = shared().shape(render::Face::Sans, "i", scratch).advance;
    const float sans_m = shared().shape(render::Face::Sans, "m", scratch).advance;

    // design.md §3 puts every NUMBER in the monospaced face, and the reason is
    // exactly this: digits that line up column-wise are what makes a coordinate
    // readable at a glance. A build that loaded the same file for both faces would
    // pass every other case in this file.
    CHECK(mono_i == doctest::Approx(mono_m));
    CHECK(sans_i < sans_m);
}

TEST_CASE("TEXT: a glyph's field carries a gradient")
{
    std::vector<render::PlacedGlyph> glyphs;
    shared().shape(render::Face::Sans, "O", glyphs);
    REQUIRE(glyphs.size() == 1);

    const render::GlyphBox& box = shared().box(glyphs.front().box);
    REQUIRE_FALSE(box.blank);

    // The quad is wider than the outline by the field's own range on each side,
    // which is what gives the shader something to interpolate.
    CHECK(box.right > box.left);
    CHECK(box.top > box.bottom);
    CHECK(box.left < 0.0f);

    const int side                          = shared().width();
    const std::vector<std::uint8_t>& pixels = shared().pixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(side) * side * 4u);

    // A FIELD, not a mask. Inside the letter the distance is high, outside it is
    // low, and the two must actually differ — a generator that failed silently
    // writes one constant everywhere and the glyph comes out as a filled block or
    // as nothing at all.
    const int x0 = static_cast<int>(box.u0 * side);
    const int x1 = static_cast<int>(box.u1 * side);
    const int y0 = static_cast<int>(box.v0 * side);
    const int y1 = static_cast<int>(box.v1 * side);

    int lowest  = 255;
    int highest = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t at = (static_cast<std::size_t>(y) * side + x) * 4u;
            const int median =
                std::max(std::min(pixels[at], pixels[at + 1]),
                         std::min(std::max(pixels[at], pixels[at + 1]), pixels[at + 2]));
            lowest  = std::min(lowest, median);
            highest = std::max(highest, median);
        }
    }

    CHECK(lowest < 96);
    CHECK(highest > 160);
}

TEST_CASE("TEXT: shaping the same word twice reuses the same cells")
{
    std::vector<render::PlacedGlyph> first;
    shared().shape(render::Face::Sans, "PARSEL", first);
    const std::uint64_t after_first = shared().revision();

    std::vector<render::PlacedGlyph> again;
    shared().shape(render::Face::Sans, "PARSEL", again);

    // The atlas fills over the first frames a drawing is on screen and is static
    // after that. The backend re-uploads only when the revision moves, so a
    // revision that kept moving would be a 4 MB texture upload every frame.
    CHECK(shared().revision() == after_first);
    REQUIRE(first.size() == again.size());
    for (std::size_t i = 0; i < first.size(); ++i)
        CHECK(first[i].box == again[i].box);
}

#endif // PIRICAD_HAVE_TEXT
