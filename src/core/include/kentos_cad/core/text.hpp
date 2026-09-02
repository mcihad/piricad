// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: Turkish-correct text handling.
//
// std::toupper / std::tolower are BANNED on Turkish text: they map
// 'i' -> 'I' and 'I' -> 'i', which is wrong in Turkish ('i' -> 'İ', 'ı' -> 'I').
// The UI layer uses QLocale (.claude/ui.md); core is Qt-free and therefore
// carries this explicit UTF-8 mapping instead.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace kentos::core {

/// Turkish-aware UTF-8 upper-casing, restricted to ASCII + the six Turkish pairs.
///
/// This is the DISPLAY operation: what a word looks like written in capitals. It
/// is what a layer name folds through, and it is deliberately NOT what a declared
/// name matches through — see `turkish_fold_key`.
std::string turkish_upper(std::string_view utf8);

/// Case-insensitive (Turkish-aware) equality.
bool turkish_iequals(std::string_view a, std::string_view b);

/// The lookup key of a DECLARED NAME — a command, a setting, a keyword.
///
/// WHY THIS IS NOT `turkish_upper`, and it is not a subtlety. Turkish has two
/// letter i's, and upper-casing keeps them apart on purpose: `i` rises to `İ` and
/// `ı` rises to `I`. Correct for a word being written out; wrong for matching a
/// name somebody typed, and measurably so. `ÇİZGİ` is declared with `CIZGI`
/// beside it (CLAUDE.md 2.6) exactly so an ASCII keyboard can reach it — but
/// `turkish_upper("line")` is `LİNE`, `turkish_upper("LINE")` is `LINE`, and the
/// two do not compare equal. Typing `line` in lower case therefore did not find
/// the ÇİZGİ command; neither did `cizgi`, `import` or `iceaktar`. The names were
/// right and the fold was wrong.
///
/// So a key folds CASE AND ALPHABET together: `i`, `I`, `ı` and `İ` all become
/// `I`, and `ç ğ ö ş ü` become `C G O S U`. Every spelling of a declared name then
/// reaches it — the Turkish one, the ASCII-folded one, and either in any case.
///
/// It is safe precisely because these names are OURS: a command declares its
/// Turkish and ASCII spellings as aliases of one command, so folding them
/// together collides nothing. `Registry` and `SettingCatalog` both refuse a
/// duplicate key, which is the check that keeps it true.
///
/// A LAYER NAME IS NOT A DECLARED NAME. It is the user's own text, two layers
/// differing only by `ı`/`i` are two layers, and that lookup keeps
/// `turkish_upper`.
std::string turkish_fold_key(std::string_view utf8);

/// Case-insensitive equality between two declared names; see `turkish_fold_key`.
bool turkish_key_equals(std::string_view a, std::string_view b);

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

} // namespace kentos::core
