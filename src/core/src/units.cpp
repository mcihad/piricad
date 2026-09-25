// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>

namespace kentos::core {

std::string metres_fixed(Mm v, int decimals, char point)
{
    const int d = std::clamp(decimals, 0, 3);
    // Millimetres per last written digit, and written digits per metre.
    constexpr std::array<std::uint64_t, 4> kStep{1000, 100, 10, 1};
    constexpr std::array<std::uint64_t, 4> kScale{1, 10, 100, 1000};
    const auto at            = static_cast<std::size_t>(d);
    const std::uint64_t step = kStep[at];

    // The magnitude as unsigned, so the most negative value is not negated in
    // a signed type (every stored coordinate is far inside it; a formatter
    // should not rely on that).
    const bool negative = v < 0;
    const std::uint64_t mag =
        negative ? 0 - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
    std::uint64_t units = mag / step;
    if ((mag % step) * 2 >= step) ++units; // half away from zero: the sign is applied after

    std::string out = std::to_string(units / kScale[at]);
    if (d > 0) {
        const std::string frac = std::to_string(units % kScale[at]);
        out += point;
        out += std::string(static_cast<std::size_t>(d) - frac.size(), '0') + frac;
    }
    return negative && units != 0 ? "-" + out : out;
}

} // namespace kentos::core
