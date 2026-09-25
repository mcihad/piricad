// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: how wide a drawing's text is (TODOS C-18).
//
// ONE MEASURE FOR EVERYTHING THAT ASKS. The box a text is picked by, whether a
// dimension's figure fits between its extension lines, the baseline a caption
// stands on, where a wrapping text breaks — and the width the canvas and the
// PDF draw. They used to come from two places: the core guessed six tenths of
// a height per letter, the backends measured the face, and a `<> (tapu)` on a
// 20 m dimension was 18 m wide to the core and 20 m wide on the sheet, touching
// both extension lines it had been judged to clear.
//
// THE FACE AS NUMBERS. The core links no font engine (core.md R1) and reads no
// file (core.md P9), and a table installed at start-up would be the mutable
// global core.md P8 forbids — and would make a document's geometry depend on
// whether some caller remembered to install it. So the drawing face's advances
// are CONSTANT DATA of this library, generated from the font file the program
// ships (`data/fonts/IBMPlexSans-Regular.ttf`) by `kentos_yazi_olcusu`
// (src/render/tools), which shapes every character with the atlas's own
// shaper (`render::Spacing::Technical`). `scripts/ci-gate-yazi-olcusu.sh`
// regenerates the table and fails on any difference, so the numbers here are
// the face's and a changed face is a changed table in the same commit.
//
// TECHNICAL SPACING. A drawing's text is set with every letter at its own
// advance — no kerning pair and no ligature — so its width is a sum over its
// characters, which is what makes it computable here at all. Both backends set
// it that way (`render::Spacing::Technical`), and the suite holds the atlas's
// integer measure to this one over the face's whole character set.
#pragma once

#include <cstdint>
#include <string_view>

namespace kentos::core {

/// The drawing face's grid, as the font declares it.
struct TextFace
{
    std::int32_t units_per_em{1000}; ///< how many font units make an EM
    std::int32_t cap_height{700};    ///< a capital letter's height, in font units:
                                     ///< what a CAD text height measures
};

/// The drawing face — IBM Plex Sans Regular — as its font file declares it.
TextFace text_face() noexcept;

/// How far the pen moves for the Unicode scalar `c`, in font units: the face's
/// own advance; zero for a character the shaper hides (a soft hyphen, a joiner,
/// a variation selector); `.notdef`'s for one the face does not have, which is
/// drawn as the face's empty box.
std::int32_t text_advance(char32_t c) noexcept;

/// How far the pen moves over `utf8`, in font units: the sum of its characters'
/// advances. A byte that does not begin a well-formed UTF-8 sequence counts as
/// one U+FFFD and the next byte is read afresh — how the shaper reads it — so a
/// broken string measures as it draws.
std::int64_t text_run_advance(std::string_view utf8) noexcept;

} // namespace kentos::core
