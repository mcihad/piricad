// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/circle.hpp"

#include <cmath>
#include <utility>

namespace kentos::core {

/// The unit circle, built by REPEATED EXACT BISECTION.
///
/// Not `std::cos`/`std::sin`, and that is a portability requirement rather than a
/// preference: libm's trigonometric functions are not correctly rounded and are
/// not required to agree between platforms, so the same circle drawn on x86 and
/// on Apple Silicon could differ in the last bit — and §7.3 promises they do not.
/// `sqrt` IS correctly rounded by IEEE-754, and bisecting from the four exact axis
/// points uses nothing but +, * and sqrt.
///
/// A function-local `static const` built once: const state, not a registry
/// (core.md P8) — the same shape `builtin_kinds()` uses.
const std::vector<std::pair<double, double>>& unit_circle()
{
    static const std::vector<std::pair<double, double>> table = [] {
        // Exact, and the only four points that are.
        std::vector<std::pair<double, double>> p{{1.0, 0.0}, {0.0, 1.0}, {-1.0, 0.0}, {0.0, -1.0}};

        while (p.size() < kCircleSegments) {
            std::vector<std::pair<double, double>> next;
            next.reserve(p.size() * 2);
            for (std::size_t i = 0; i < p.size(); ++i) {
                const auto& a = p[i];
                const auto& b = p[(i + 1) % p.size()];
                next.push_back(a);

                // The chord midpoint, normalised: for two unit vectors that is
                // exactly the angular bisector.
                const double mx  = a.first + b.first;
                const double my  = a.second + b.second;
                const double len = std::sqrt(mx * mx + my * my);
                next.emplace_back(mx / len, my / len);
            }
            p = std::move(next);
        }
        return p;
    }();
    return table;
}

Mm circle_radius_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return 0;
    const auto xs = geom.ring_xs(rs.first);
    if (xs.size() < 2) return 0;
    const Mm r = xs[1] - xs[0];
    return r < 0 ? -r : r;
}

Point2 circle_centre_of(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.empty()) return Point2{};
    return Point2{xs[0], ys[0]};
}

void circle_outline(Point2 centre, Mm radius, std::vector<Mm>& xs, std::vector<Mm>& ys)
{
    const auto& unit = unit_circle();
    const auto r     = static_cast<double>(radius);

    xs.reserve(xs.size() + unit.size());
    ys.reserve(ys.size() + unit.size());

    for (const auto& u : unit) {
        xs.push_back(centre.x + mm_round(r * u.first));
        ys.push_back(centre.y + mm_round(r * u.second));
    }
}

} // namespace kentos::core
