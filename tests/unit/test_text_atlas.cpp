// SPDX-License-Identifier: GPL-3.0-or-later
//
// The SDF glyph atlas (`.claude/render.md` R8). Compiled only when
// `KENTOS_WITH_TEXT=ON`.
//
// THIS IS WHY THE ATLAS IS QT-FREE. A backend cannot be constructed in a suite
// that links no Qt (CLAUDE.md 3.4), so if the shaping and the field generation
// lived in the backend nothing here could check them — and what they get wrong is
// exactly the kind of thing a screenshot does not show: a Turkish letter that
// quietly became a box, a run whose kerning collapsed, a field with no gradient
// in it.
#include "kentos_test.hpp"

#include "kentos_cad/render/backend.hpp"

#include <string>

using namespace kentos;

TEST_CASE("TEXT: yazısız bir yapı bunu söylüyor")
{
    // THE DEFECT THIS LOCKS. The SDF atlas is optional and the QRhi canvas's whole
    // text path is compiled out with it, so a build without it opens a plan sheet
    // correctly, at full speed, and silently without the 13 112 captions on it.
    // Nothing is broken, nothing is slow, and what the user came for is not there
    // — which is exactly the shape that gets reported as "the import lost my
    // labels" rather than as a missing package.
    //
    // Written to hold in BOTH configurations, because the one that matters is the
    // one this suite is usually not built in.
#if KENTOS_HAVE_TEXT
    CHECK(render::text_backend_status().empty());
#else
    const std::string said = render::text_backend_status();
    CHECK_FALSE(said.empty());

    // It names the switch and both packages, because "text is missing" without
    // them is a fact the reader can do nothing with.
    CHECK(said.find("KENTOS_WITH_TEXT") != std::string::npos);
    CHECK(said.find("FreeType") != std::string::npos);
    CHECK(said.find("HarfBuzz") != std::string::npos);
#endif
}

#if KENTOS_HAVE_TEXT

#include "kentos_cad/core/text_metrics.hpp"
#include "kentos_cad/render/text_atlas.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

using namespace kentos;

namespace {

std::string fonts()
{
    return std::string(KENTOS_DATA_DIR) + "/fonts";
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

TEST_CASE("TEXT: the cap height is available before anything is shaped")
{
    // THE REGRESSION. A CAD text height is the height of a CAPITAL LETTER — a
    // DXF's group code 40, the METIN command's own promise, the lettering height
    // a regulation names. A font's em size is a different and larger number, so a
    // backend that hands a cap height to a face as its em size draws every
    // caption about a third too small and a third too narrow. That is what an
    // imported TAKS-over-KAKS label looked like: the fraction no longer filled
    // the circle drawn around it.
    //
    // Both backends now divide the height by this ratio before they scale
    // anything, and they need it BEFORE they shape a run — hence the accessor.
    for (const render::Face face :
         {render::Face::Sans, render::Face::SansMedium, render::Face::SansSemiBold,
          render::Face::Mono, render::Face::MonoMedium}) {
        const float cap = shared().cap_height(face);

        // A real cap height, not a placeholder: a capital letter is most of the
        // em and never all of it. A 1.0 here would mean the conversion had
        // quietly become a no-op and the defect had come back.
        CHECK(cap > 0.5f);
        CHECK(cap < 0.85f);

        // The same number a shaped run reports, because the backends read it from
        // one place and the anchor maths reads it from the other.
        std::vector<render::PlacedGlyph> glyphs;
        CHECK(shared().shape(face, "0", glyphs).cap == cap);
    }
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

    // The width in both the units this test needs it in, converted ONCE. Mixing
    // `int` and `std::size_t` in the arithmetic below is what the sign-conversion
    // warnings were about, and an atlas index computed half-signed is a real way
    // to read the wrong pixel.
    const int side                          = shared().width();
    const auto wide                         = static_cast<std::size_t>(side);
    const auto across                       = static_cast<float>(side);
    const std::vector<std::uint8_t>& pixels = shared().pixels();
    REQUIRE(pixels.size() == wide * wide * 4u);

    // A FIELD, not a mask. Inside the letter the distance is high, outside it is
    // low, and the two must actually differ — a generator that failed silently
    // writes one constant everywhere and the glyph comes out as a filled block or
    // as nothing at all.
    const int x0 = static_cast<int>(box.u0 * across);
    const int x1 = static_cast<int>(box.u1 * across);
    const int y0 = static_cast<int>(box.v0 * across);
    const int y1 = static_cast<int>(box.v1 * across);

    int lowest  = 255;
    int highest = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t at =
                (static_cast<std::size_t>(y) * wide + static_cast<std::size_t>(x)) * 4u;
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

TEST_CASE("TEXT: a character the face does not have is named, once, and draws as the box")
{
    // TODOS C-12: a caption with a letter the bundled face lacks shows the
    // font's own box — which says nothing about WHICH letter. The face's
    // character map names it, and a Turkish sentence names nothing.
    render::TextAtlas& atlas = shared();
    CHECK(atlas.uncovered(render::Face::Sans, "Şişli İlçesi ğüöç ıI Ø±°").empty());
    CHECK(atlas.uncovered(render::Face::Sans, "satır\nsatır").empty()); // a break is not text

    const auto gone = atlas.uncovered(render::Face::Sans, "A漢B⌀漢");
    REQUIRE_EQ(gone.size(), std::size_t{2}); // 漢 twice, ⌀ once: each named once
    CHECK_EQ(gone[0].code, 0x6F22u);
    CHECK_EQ(gone[0].utf8, std::string("漢"));
    CHECK_EQ(gone[1].code, 0x2300u);
    CHECK_EQ(gone[1].utf8, std::string("⌀"));

    // And it is the box the canvas draws: a real outline, not a blank.
    std::vector<render::PlacedGlyph> glyphs;
    (void)atlas.shape(render::Face::Sans, "A漢B", glyphs);
    REQUIRE_EQ(glyphs.size(), std::size_t{3});
    CHECK_FALSE(atlas.box(glyphs[1].box).blank);
    std::size_t missing = 0;
    (void)atlas.measure(render::Face::Sans, "A漢B", &missing);
    CHECK_EQ(missing, std::size_t{1});
}

// --------------------------------------------- one measure (TODOS C-18) ----

namespace {

/// `c` as UTF-8.
std::string utf8_of(char32_t c)
{
    std::string s;
    if (c < 0x80) {
        s += static_cast<char>(c);
    } else if (c < 0x800) {
        s += static_cast<char>(0xC0 | (c >> 6));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        s += static_cast<char>(0xE0 | (c >> 12));
        s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (c >> 18));
        s += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (c & 0x3F));
    }
    return s;
}

/// How wide the atlas SETS `utf8` as a drawing's text, in font units: the width
/// both backends draw it at. `glyphs`, when given, receives how many glyphs.
std::int64_t drawn(std::string_view utf8, std::size_t* glyphs = nullptr)
{
    std::vector<render::FontGlyph> set;
    const float em =
        shared().glyphs(render::Face::Sans, utf8, render::Spacing::Technical, set).advance;
    if (glyphs != nullptr) *glyphs = set.size();
    return std::llround(static_cast<double>(em) * core::text_face().units_per_em);
}

/// The shaper's own advance over `utf8`, in technical spacing, before anything
/// anchors it to the core's measure.
std::int64_t natural(std::string_view utf8)
{
    return shared().advance_units(render::Face::Sans, utf8, render::Spacing::Technical);
}

/// Whether `utf8` carries a combining accent (U+0300–U+036F, lead bytes CC
/// and CD): a letter it follows is the shaper's to compose with it or to swap
/// for a variant that makes room for it — `g` does both.
bool accented(std::string_view utf8)
{
    return std::ranges::any_of(utf8, [](char c) {
        const auto b = static_cast<unsigned char>(c);
        return b == 0xCCU || b == 0xCDU;
    });
}

} // namespace

TEST_CASE("TEXT: the core's table is the face — its grid, its cap and every letter alone")
{
    // `kentos_yazi_olcusu` wrote the table from this very atlas, and the gate
    // regenerates it; this holds the two together inside the suite too, over
    // everything a Turkish sheet writes and then some.
    CHECK_EQ(shared().units_per_em(render::Face::Sans), core::text_face().units_per_em);
    CHECK_EQ(shared().cap_height_units(render::Face::Sans), core::text_face().cap_height);
    for (char32_t c = 0; c < 0x2200; ++c) {
        if (c >= 0xD800 && c <= 0xDFFF) continue;
        const std::string one = utf8_of(c);
        if (natural(one) != core::text_advance(c))
            FAIL_CHECK("U+" << std::hex << static_cast<unsigned>(c) << ": the face " << std::dec
                            << natural(one) << ", the table " << core::text_advance(c));
    }
    // The hidden ones measure nothing, the missing one is the box's width, and
    // a broken byte is U+FFFD — as the shaper reads it.
    CHECK_EQ(core::text_advance(0x00AD), 0); // soft hyphen
    CHECK_EQ(core::text_advance(0x200D), 0); // zero-width joiner
    CHECK_EQ(core::text_advance(0x6F22), natural("漢"));
    CHECK_EQ(core::text_run_advance("\xC3"), natural("\xC3"));
    CHECK_EQ(core::text_run_advance("A\xE2\x82"
                                    "B"),
             natural("A\xE2\x82"
                     "B"));
    CHECK_EQ(core::text_run_advance("\xF0\x9F\x98"), natural("\xF0\x9F\x98"));
}

TEST_CASE("TEXT: a drawing's text is exactly as wide as the core measures it, letter by letter "
          "and in context")
{
    // THE ACCEPTANCE (TODOS C-18): the box a text is picked by, whether a
    // dimension's figure fits and the baseline it stands on come from the
    // measure the letters are DRAWN with. Technical spacing makes the drawn
    // width a sum over the characters; this checks that it really is one —
    // every pair of an alphabet of Turkish text, accents that compose with the
    // letter before them, joiners and hidden characters, a missing letter and
    // broken bytes — against the core's sum, to the font unit.
    //
    // Two claims, checked separately. What both backends DRAW is the core's
    // width, for every string, because the atlas sets each cluster where the
    // core's sum puts it. And that sum is the face's own spacing, not a
    // correction of it: its unanchored advance is the table's to the unit for
    // every pair but the ones with a combining accent in them — decomposed
    // text, where the shaper composes the accent onto its letter or swaps the
    // letter for a variant with room for it. Those are counted, so a font whose
    // spacing drifted from the table would show here as pairs failing rather
    // than as a few accented.
    std::vector<std::string> alphabet;
    for (char32_t c = 0x20; c < 0x7F; ++c)
        alphabet.push_back(utf8_of(c));
    for (const char* extra :
         {"ç",      "ğ",      "ı",      "ö",      "ş",      "ü",      "Ç",      "Ğ",      "İ",
          "Ö",      "Ş",      "Ü",      "â",      "î",      "û",      "é",      "Ø",      "±",
          "°",      "²",      "€",      "—",      "…",      "\u00A0", "\u00AD", "\u200D", "\u0301",
          "\u0302", "\u0306", "\u0307", "\u0308", "\u0327", "漢",     "\xC3",   "\n"})
        alphabet.emplace_back(extra);

    std::size_t pairs   = 0;
    std::size_t accents = 0; // pairs with a combining accent
    std::size_t shifted = 0; // of those, the ones the shaper spaces otherwise
    for (const std::string& a : alphabet)
        for (const std::string& b : alphabet) {
            const std::string run      = a + b;
            const std::int64_t measure = core::text_run_advance(run);
            if (drawn(run) != measure)
                FAIL_CHECK("\"" << run << "\": drawn " << drawn(run) << ", measured " << measure);
            if (accented(run)) {
                ++accents;
                shifted += natural(run) != measure ? 1 : 0;
            } else if (natural(run) != measure) {
                FAIL_CHECK("\"" << run << "\": the face " << natural(run) << ", the table "
                                << measure);
            }
            ++pairs;
        }
    CHECK_EQ(pairs, alphabet.size() * alphabet.size());
    // `ğ` spelled g and a breve, `ǵ`, a g with two dots: a handful, and every
    // one of them drawn at the core's width all the same.
    CHECK_GT(shifted, std::size_t{0});
    CHECK_LT(shifted * 20, accents);

    // Decomposed Turkish — as a file name on macOS spells it — draws at the
    // width the core measures: `ğ` as g and a combining breve.
    CHECK_EQ(drawn("g\u0306"), core::text_run_advance("g\u0306"));
    CHECK_EQ(core::text_run_advance("g\u0306"), 528);

    for (const char* sentence :
         {"20,00 (tapu)", "Ada 1284, Parsel 21 — İmar Kanunu 18. madde", "TAKS=0,30 KAKS=1,50",
          "fi fl ffi AV To Ta Yo", "ŞİŞLİ ÇAĞLAYAN ığdır Iğdır", "ATATÜRK CADDESİ",
          "Yapı yaklaşma sınırı 5,00 m"}) {
        CHECK_MESSAGE(drawn(sentence) == core::text_run_advance(sentence), sentence);
        CHECK_MESSAGE(natural(sentence) == core::text_run_advance(sentence), sentence);
    }
    CHECK_EQ(drawn("s\u0327 i\u0307 A\u0301"), core::text_run_advance("s\u0327 i\u0307 A\u0301"));

    // And it is technical spacing that makes it so: the face kerns `TAKS`
    // tighter when it is typeset, and a drawing's text is not typeset.
    CHECK_GT(natural("TAKS"),
             shared().advance_units(render::Face::Sans, "TAKS", render::Spacing::Typeset));
    CHECK_EQ(drawn("TAKS"), std::int64_t{572 + 641 + 634 + 581});

    // The glyphs the QPainter path sets are the ones the canvas sets, where it
    // sets them: the same count, the same run width.
    std::vector<render::FontGlyph> ids;
    std::vector<render::PlacedGlyph> boxes;
    const render::RunMetrics a =
        shared().glyphs(render::Face::Sans, "Ada 1284", render::Spacing::Technical, ids);
    const render::RunMetrics b =
        shared().shape(render::Face::Sans, "Ada 1284", boxes, render::Spacing::Technical);
    CHECK_EQ(ids.size(), std::size_t{8});   // the space too: it is a glyph of the run
    CHECK_EQ(boxes.size(), std::size_t{7}); // the space draws nothing
    CHECK(a.advance == b.advance);
    CHECK(a.advance * static_cast<float>(core::text_face().units_per_em) ==
          doctest::Approx(static_cast<double>(core::text_run_advance("Ada 1284"))));
}

#endif // KENTOS_HAVE_TEXT
