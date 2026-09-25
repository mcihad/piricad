// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/text_metrics.hpp"

#include "text_metrics_table.hpp"

#include <algorithm>
#include <iterator>

namespace kentos::core {
namespace {

/// What the shaper reads for a byte that does not begin a well-formed UTF-8
/// sequence (HarfBuzz's buffer default).
constexpr char32_t kReplacement = 0xFFFD;

/// The next scalar of `s` at `i`, with `i` moved past it — HarfBuzz's own
/// reading of UTF-8 (hb-utf.hh): a lead byte whose sequence is short, broken,
/// overlong, a surrogate or past U+10FFFF is one U+FFFD, and only that byte is
/// consumed. Reading it any other way would measure a broken string one width
/// here and draw it another.
char32_t next_scalar(std::string_view s, std::size_t& i) noexcept
{
    const auto at = [&s](std::size_t k) {
        return static_cast<char32_t>(static_cast<unsigned char>(s[k]));
    };
    const auto tail = [&s, &at](std::size_t k) { return k < s.size() && (at(k) & 0xC0U) == 0x80U; };

    const char32_t lead = at(i);
    ++i;
    if (lead < 0x80U) return lead;
    if (lead >= 0xC2U && lead <= 0xDFU) {
        if (!tail(i)) return kReplacement;
        const char32_t c = ((lead & 0x1FU) << 6U) | (at(i) & 0x3FU);
        i += 1;
        return c;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        if (!tail(i) || !tail(i + 1)) return kReplacement;
        const char32_t c = ((lead & 0x0FU) << 12U) | ((at(i) & 0x3FU) << 6U) | (at(i + 1) & 0x3FU);
        if (c < 0x800U || (c >= 0xD800U && c <= 0xDFFFU)) return kReplacement;
        i += 2;
        return c;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        if (!tail(i) || !tail(i + 1) || !tail(i + 2)) return kReplacement;
        const char32_t c = ((lead & 0x07U) << 18U) | ((at(i) & 0x3FU) << 12U) |
                           ((at(i + 1) & 0x3FU) << 6U) | (at(i + 2) & 0x3FU);
        if (c < 0x10000U || c > 0x10FFFFU) return kReplacement;
        i += 3;
        return c;
    }
    return kReplacement;
}

} // namespace

TextFace text_face() noexcept
{
    return text_table::face();
}

std::int32_t text_advance(char32_t c) noexcept
{
    if (c < text_table::kDirect) return text_table::direct()[c];
    const auto runs = text_table::runs();
    const auto past = std::ranges::upper_bound(runs, c, {}, &text_table::Run::first);
    if (past != runs.begin()) {
        const text_table::Run& r = *std::prev(past);
        if (c <= r.last) return r.advance;
    }
    return text_table::missing();
}

std::int64_t text_run_advance(std::string_view utf8) noexcept
{
    std::int64_t pen = 0;
    std::size_t i    = 0;
    while (i < utf8.size())
        pen += text_advance(next_scalar(utf8, i));
    return pen;
}

} // namespace kentos::core
