// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core (private to text_metrics.cpp): the drawing face's advance
// table, whose data `kentos_yazi_olcusu` writes into text_metrics_table.cpp.
// What the numbers are and why they are compiled in is said in
// kentos_cad/core/text_metrics.hpp.
#pragma once

#include "kentos_cad/core/text_metrics.hpp"

#include <cstdint>
#include <span>

namespace kentos::core::text_table {

/// A stretch of characters that share one advance.
struct Run
{
    char32_t first{0};       ///< its first character
    char32_t last{0};        ///< its last, inclusive
    std::int32_t advance{0}; ///< every one of them, in font units
};

/// U+0000 to here, one advance each: ASCII, Latin-1 and Latin Extended-A —
/// every letter of the Turkish alphabet — looked up without a search.
inline constexpr char32_t kDirect = 0x180;

/// The face's grid and cap height.
TextFace face() noexcept;

/// `.notdef`'s advance: every character not in `direct()` or `runs()`.
std::int32_t missing() noexcept;

/// The advance of every character below `kDirect`.
std::span<const std::uint16_t> direct() noexcept;

/// Every other character whose advance is not `missing()`, as runs in order.
std::span<const Run> runs() noexcept;

} // namespace kentos::core::text_table
