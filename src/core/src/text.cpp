// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/text.hpp"

#include <array>
#include <cstring>

namespace piricad::core {
namespace {

// UTF-8 lower -> upper pairs that ASCII toupper gets wrong in Turkish.
struct Pair
{
    const char* lower;
    const char* upper;
};

constexpr std::array<Pair, 7> kTurkishPairs{{
    {"i", "\xC4\xB0"},        // i  -> İ   (dotted capital I)
    {"\xC4\xB1", "I"},        // ı  -> I   (dotless small i)
    {"\xC3\xA7", "\xC3\x87"}, // ç  -> Ç
    {"\xC4\x9F", "\xC4\x9E"}, // ğ  -> Ğ
    {"\xC3\xB6", "\xC3\x96"}, // ö  -> Ö
    {"\xC5\x9F", "\xC5\x9E"}, // ş  -> Ş
    {"\xC3\xBC", "\xC3\x9C"}, // ü  -> Ü
}};

} // namespace

std::string turkish_upper(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 4);

    std::size_t i = 0;
    while (i < s.size()) {
        bool matched = false;
        for (const auto& p : kTurkishPairs) {
            const std::size_t n = std::strlen(p.lower);
            if (s.size() - i >= n && s.compare(i, n, p.lower) == 0) {
                out += p.upper;
                i += n;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 'a' && c <= 'z') {
            out += static_cast<char>(c - 'a' + 'A');
            ++i;
        } else if (c < 0x80) {
            out += static_cast<char>(c);
            ++i;
        } else {
            // Pass through any other multi-byte sequence unchanged.
            std::size_t n = 1;
            if ((c & 0xE0) == 0xC0)
                n = 2;
            else if ((c & 0xF0) == 0xE0)
                n = 3;
            else if ((c & 0xF8) == 0xF0)
                n = 4;
            n = (i + n <= s.size()) ? n : 1;
            out.append(s.substr(i, n));
            i += n;
        }
    }
    return out;
}

bool turkish_iequals(std::string_view a, std::string_view b)
{
    return turkish_upper(a) == turkish_upper(b);
}

} // namespace piricad::core
