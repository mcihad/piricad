// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: Turkish-correct text handling.
//
// std::toupper / std::tolower are BANNED on Turkish text: they map
// 'i' -> 'I' and 'I' -> 'i', which is wrong in Turkish ('i' -> 'İ', 'ı' -> 'I').
// The UI layer uses QLocale (.claude/ui.md); core is Qt-free and therefore
// carries this explicit UTF-8 mapping instead.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace piricad::core {

/// Turkish-aware UTF-8 upper-casing, restricted to ASCII + the six Turkish pairs.
/// Used to fold command names and layer names for lookup.
std::string turkish_upper(std::string_view utf8);

/// Case-insensitive (Turkish-aware) equality.
bool turkish_iequals(std::string_view a, std::string_view b);

/// Deterministic 64-bit FNV-1a. Used for document content hashing in tests.
constexpr std::uint64_t fnv1a(std::string_view s, std::uint64_t seed = 1469598103934665603ULL)
{
    std::uint64_t h = seed;
    for (char ch : s) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
        h *= 1099511628211ULL;
    }
    return h;
}

/// Folds an integer, byte by byte in a FIXED order. Never a memcpy of the value:
/// the byte order of an int64 differs between platforms and a hash that inherited
/// it could not produce identical golden output (core.md R9, §7.3).
constexpr std::uint64_t fnv1a_int(std::int64_t v, std::uint64_t seed)
{
    std::uint64_t h = seed;
    auto u          = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i) {
        h ^= (u >> (i * 8)) & 0xFFu;
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace piricad::core
