// SPDX-License-Identifier: GPL-3.0-or-later
// Colour words: the colours `RENK` reads and the colour chips offer.
//
// ONE LIST, READ BY BOTH. The chips at the foot of the tool column show a row of
// swatches and the command line offers the same colours as words; were the two
// kept apart, a swatch could paint a colour the command did not know by name, or
// the other way round (CLAUDE.md 5.10, applied to a vocabulary).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace kentos::command {

/// A colour a user can name instead of spelling it in hex.
struct NamedColour
{
    std::string_view word; ///< Turkish, lower case, exactly as it is typed: `kırmızı`
    std::uint32_t rgba;    ///< 0xAARRGGBB, always opaque
};

/// The colours of AutoCAD's index 1 to 8 — the palette every DWG still carries
/// and every CAD user knows by number — under their Turkish names, black first
/// because it is what a plan is printed in. Index 7 is black on paper and white
/// on a dark screen, so it is here twice, as `siyah` and as `beyaz`.
std::span<const NamedColour> named_colours() noexcept;

/// Reads `#RRGGBB`, `#AARRGGBB`, `0xAARRGGBB`, a named colour in any case or
/// spelling (`kırmızı`, `KIRMIZI`, `kirmizi`), or the DECIMAL value of an
/// `0xAARRGGBB` that carries an alpha byte — which is what the command line
/// makes of `0xFFC0392B`, and the integer `STİL renk=` has always taken.
///
/// Nothing for anything else, and in particular for a bare `112233`: the line
/// reads that as a number, and a number without an alpha byte could as well be
/// hex that lost its `#`.
std::optional<std::uint32_t> parse_colour(std::string_view word);

/// `#RRGGBB`, or `#AARRGGBB` when the colour is not opaque: the spelling the
/// journal keeps, whatever was typed.
std::string colour_hex(std::uint32_t rgba);

/// The word of the named colour equal to `rgba`, or empty.
std::string_view colour_word(std::uint32_t rgba) noexcept;

} // namespace kentos::command
