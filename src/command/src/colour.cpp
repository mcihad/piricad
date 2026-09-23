// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/colour.hpp"

#include "kentos_cad/core/text.hpp"

#include <array>
#include <charconv>
#include <string>
#include <string_view>

namespace kentos::command {
namespace {

constexpr std::array<NamedColour, 9> kNamed{{
    {"siyah", 0xFF000000u},
    {"kırmızı", 0xFFFF0000u},
    {"sarı", 0xFFFFFF00u},
    {"yeşil", 0xFF00FF00u},
    {"camgöbeği", 0xFF00FFFFu},
    {"mavi", 0xFF0000FFu},
    {"macenta", 0xFFFF00FFu},
    {"gri", 0xFF808080u},
    {"beyaz", 0xFFFFFFFFu},
}};

std::optional<std::uint64_t> number_of(std::string_view digits, int base)
{
    if (digits.empty()) return std::nullopt;
    const std::string text(digits);
    std::uint64_t value = 0;
    const char* end     = text.c_str() + text.size();
    const auto [at, ec] = std::from_chars(text.c_str(), end, value, base);
    if (ec != std::errc{} || at != end) return std::nullopt;
    return value;
}

} // namespace

std::span<const NamedColour> named_colours() noexcept
{
    return kNamed;
}

std::optional<std::uint32_t> parse_colour(std::string_view word)
{
    if (word.empty()) return std::nullopt;
    for (const NamedColour& n : kNamed)
        if (core::turkish_key_equals(word, n.word)) return n.rgba;

    std::string_view hex = word;
    if (hex.front() == '#') {
        hex.remove_prefix(1);
    } else if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex.remove_prefix(2);
    } else {
        // What the command line made of `0x…`. Taken only with an alpha byte:
        // below that the number could be hex that lost its `#` (see header).
        const auto n = number_of(word, 10);
        if (n && *n > 0xFFFFFFu && *n <= 0xFFFFFFFFu) return static_cast<std::uint32_t>(*n);
        return std::nullopt;
    }

    if (hex.size() != 6 && hex.size() != 8) return std::nullopt;
    const auto value = number_of(hex, 16);
    if (!value) return std::nullopt;
    auto rgba = static_cast<std::uint32_t>(*value);
    if (hex.size() == 6) rgba |= 0xFF000000u; // opaque unless said otherwise
    return rgba;
}

std::string colour_hex(std::uint32_t rgba)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string out                   = "#";
    const bool translucent            = (rgba >> 24U) != 0xFFU;
    for (int shift = translucent ? 28 : 20; shift >= 0; shift -= 4)
        out += digits[(rgba >> static_cast<unsigned>(shift)) & 0xFU];
    return out;
}

std::string_view colour_word(std::uint32_t rgba) noexcept
{
    for (const NamedColour& n : kNamed)
        if (n.rgba == rgba) return n.word;
    return {};
}

} // namespace kentos::command
