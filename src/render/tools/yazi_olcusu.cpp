// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — kentos_yazi_olcusu: the drawing face's advances, written into the
// core as constant data (TODOS C-18, kentos_cad/core/text_metrics.hpp).
//
//   kentos_yazi_olcusu <font directory> <output .cpp>
//
// EVERY UNICODE SCALAR, SHAPED ALONE, by the atlas's own shaper in the spacing a
// drawing's text is set in (`render::Spacing::Technical`) — not read off the
// font's character map. What the core has to predict is what the shaper DOES:
// a soft hyphen or a joiner it hides has no width, a precomposed letter the face
// lacks it builds from a base and an accent, and a character with no glyph at
// all it draws as `.notdef`. Asking the shaper answers all three the way the
// canvas will, and the suite then holds the atlas to the table over real text.
//
// AT THE TOP OF THE GRAPH like every generator (Article 3.2a): it links the
// atlas and nothing links it, and it includes no Qt — it runs in CI with no
// display. Its output is held to the face by scripts/ci-gate-yazi-olcusu.sh.
#include "kentos_cad/render/text_atlas.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using kentos::render::Face;
using kentos::render::Spacing;
using kentos::render::TextAtlas;

/// The face a drawing's text is set in, and the file it ships as.
constexpr Face kFace               = Face::Sans;
constexpr const char* kFaceFile    = "IBMPlexSans-Regular.ttf";
constexpr char32_t kLastScalar     = 0x10FFFF;
constexpr char32_t kDirect         = 0x180; // text_metrics_table.hpp `kDirect`
constexpr int kDirectPerLine       = 16;
constexpr std::int32_t kNotAScalar = -1;

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

/// `c` as `0x` and at least four hex digits, the way a table reads best.
std::string hex(char32_t c)
{
    char buf[16];
    (void)std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(c));
    return buf;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: kentos_yazi_olcusu <font directory> <output .cpp>\n");
        return 2;
    }
    auto opened = TextAtlas::open(argv[1]);
    if (!opened) {
        std::fprintf(stderr, "kentos_yazi_olcusu: %s\n", opened.error().message.c_str());
        return 1;
    }
    TextAtlas& atlas = *opened.value();

    // ---- every scalar, alone ----
    std::vector<std::int32_t> advance(static_cast<std::size_t>(kLastScalar) + 1, kNotAScalar);
    std::map<std::int32_t, std::size_t> tally;
    for (char32_t c = 0; c <= kLastScalar; ++c) {
        if (c >= 0xD800 && c <= 0xDFFF) continue; // surrogates: never decoded as a scalar
        const auto a =
            static_cast<std::int32_t>(atlas.advance_units(kFace, utf8_of(c), Spacing::Technical));
        advance[c] = a;
        ++tally[a];
    }

    // `.notdef`: the advance of the characters the face has no glyph for, which
    // are most of Unicode — so the commonest advance is that one.
    std::int32_t missing = 0;
    std::size_t most     = 0;
    for (const auto& [a, n] : tally)
        if (n > most) {
            most    = n;
            missing = a;
        }

    // ---- the table ----
    std::ostringstream out;
    out << "// GENERATED FILE - do not edit: run `make yazi-olcusu` (kentos_yazi_olcusu).\n"
           "// SPDX-License-Identifier: GPL-3.0-or-later\n"
           "//\n"
           "// The drawing face's advance for every Unicode scalar, in font units, as the\n"
           "// atlas shapes each one alone in technical spacing: "
        << kFaceFile
        << ",\n"
           "// SIL Open Font License 1.1 (data/fonts/LICENCE.txt). What the numbers are for\n"
           "// and why they are compiled in: kentos_cad/core/text_metrics.hpp.\n"
           "#include \"text_metrics_table.hpp\"\n"
           "\n"
           "#include <array>\n"
           "\n"
           "// clang-format off\n"
           "namespace kentos::core::text_table {\n"
           "namespace {\n"
           "\n"
           "/// U+0000 to U+017F, sixteen to a line.\n"
           "constexpr std::array<std::uint16_t, kDirect> kDirectAdvances{\n";
    for (char32_t c = 0; c < kDirect; ++c) {
        if (c % kDirectPerLine == 0) out << "    ";
        out << advance[c] << ',';
        if (c % kDirectPerLine == kDirectPerLine - 1)
            out << " // " << hex(c - (kDirectPerLine - 1)) << '\n';
        else
            out << ' ';
    }
    out << "};\n\n";

    // Runs of one advance above the direct range, `.notdef` left out.
    struct Run
    {
        char32_t first;
        char32_t last;
        std::int32_t advance;
    };

    std::vector<Run> runs;
    for (char32_t c = kDirect; c <= kLastScalar; ++c) {
        const std::int32_t a = advance[c];
        if (a == kNotAScalar || a == missing) continue;
        if (!runs.empty() && runs.back().last + 1 == c && runs.back().advance == a)
            runs.back().last = c;
        else
            runs.push_back(Run{c, c, a});
    }

    out << "/// Every other scalar whose advance is not `.notdef`'s, as runs of one advance.\n"
           "constexpr std::array<Run, "
        << runs.size() << "> kRuns{\n";
    for (const Run& r : runs)
        out << "    Run{.first = " << hex(r.first) << ", .last = " << hex(r.last)
            << ", .advance = " << r.advance << "},\n";
    out << "};\n"
           "\n"
           "} // namespace\n"
           "\n"
           "TextFace face() noexcept\n"
           "{\n"
           "    return TextFace{.units_per_em = "
        << atlas.units_per_em(kFace) << ", .cap_height = " << atlas.cap_height_units(kFace)
        << "};\n"
           "}\n"
           "\n"
           "std::int32_t missing() noexcept\n"
           "{\n"
           "    return "
        << missing
        << ";\n"
           "}\n"
           "\n"
           "std::span<const std::uint16_t> direct() noexcept\n"
           "{\n"
           "    return kDirectAdvances;\n"
           "}\n"
           "\n"
           "std::span<const Run> runs() noexcept\n"
           "{\n"
           "    return kRuns;\n"
           "}\n"
           "\n"
           "} // namespace kentos::core::text_table\n"
           "\n"
           "// clang-format on\n";

    std::ofstream file(argv[2], std::ios::binary | std::ios::trunc);
    file << out.str();
    if (!file) {
        std::fprintf(stderr, "kentos_yazi_olcusu: '%s' could not be written\n", argv[2]);
        return 1;
    }
    std::printf("kentos_yazi_olcusu: %zu runs, .notdef %d units, cap %d of %d\n", runs.size(),
                missing, atlas.cap_height_units(kFace), atlas.units_per_em(kFace));
    return 0;
}
