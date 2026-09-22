// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/circle.hpp"

#include "kentos_cad/core/pick.hpp"

#include <cmath>
#include <cstring>
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

namespace {

/// `p`–`q` shifted `by` millimetres along its own LEFT normal. False when the
/// two points coincide, which names no direction to shift along.
bool offset_line(Point2 p, Point2 q, Mm by, Point2& op, Point2& oq) noexcept
{
    const double dx  = mm_to_metres(q.x - p.x);
    const double dy  = mm_to_metres(q.y - p.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0.0) return false;
    const Mm nx = mm_round(-dy / len * static_cast<double>(by));
    const Mm ny = mm_round(dx / len * static_cast<double>(by));
    op          = Point2{p.x + nx, p.y + ny};
    oq          = Point2{q.x + nx, q.y + ny};
    return true;
}

/// The distance between two points in millimetres, computed in metres.
///
/// Metres before squaring: the square of a TM3 coordinate difference in
/// millimetres leaves the 53-bit mantissa long before it leaves int64, and
/// translating to the first point is what keeps the operands small (core.md R3).
Mm span(Point2 a, Point2 b) noexcept
{
    const double dx = mm_to_metres(b.x - a.x);
    const double dy = mm_to_metres(b.y - a.y);
    return mm_round(std::sqrt(dx * dx + dy * dy) * kMmPerMetre);
}

} // namespace

bool tangent_circle_centre(Point2 a1, Point2 a2, Point2 b1, Point2 b2, Mm radius, Point2 near,
                           Point2& centre) noexcept
{
    if (radius <= 0) return false;

    bool found     = false;
    double nearest = 0.0;
    for (const Mm side_a : {radius, -radius})
        for (const Mm side_b : {radius, -radius}) {
            Point2 p1{};
            Point2 p2{};
            Point2 q1{};
            Point2 q2{};
            if (!offset_line(a1, a2, side_a, p1, p2)) continue;
            if (!offset_line(b1, b2, side_b, q1, q2)) continue;

            Point2 meet{};
            double t = 0.0;
            double u = 0.0;
            if (!line_intersection(p1, p2, q1, q2, meet, t, u)) continue;

            const double to_it = distance_squared(near, meet);
            if (!found || to_it < nearest) {
                found   = true;
                nearest = to_it;
                centre  = meet;
            }
        }
    return found;
}

std::vector<std::uint8_t> encode_circle_guide(const CircleGuide& guide)
{
    std::vector<std::uint8_t> bytes(9);
    bytes[0] = static_cast<std::uint8_t>(guide.build);
    std::memcpy(bytes.data() + 1, &guide.radius, sizeof(guide.radius));
    return bytes;
}

std::optional<CircleGuide> decode_circle_guide(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 9) return std::nullopt;
    if (bytes[0] > static_cast<std::uint8_t>(CircleBuild::Centre)) return std::nullopt;

    CircleGuide guide{};
    guide.build = static_cast<CircleBuild>(bytes[0]);
    std::memcpy(&guide.radius, bytes.data() + 1, sizeof(guide.radius));
    return guide;
}

bool circle_from_guide(const CircleGuide& guide, std::span<const Point2> chain, Point2 cursor,
                       Point2& centre, Mm& radius) noexcept
{
    switch (guide.build) {
    case CircleBuild::Diameter: {
        if (chain.size() < 1) return false;
        const Point2 first = chain.front();
        // The radius comes from the FULL span rather than from the midpoint,
        // because halving a rounded half is a rounding twice over — the same
        // arithmetic `DAİRE yontem=2n` does.
        centre = Point2{(first.x + cursor.x) / 2, (first.y + cursor.y) / 2};
        radius = span(first, cursor) / 2;
        return radius > 0;
    }
    case CircleBuild::ThreePoint:
        if (chain.size() < 2) return false;
        return circumcircle(chain[0], chain[1], cursor, centre, radius) && radius > 0;
    case CircleBuild::Tangent:
        if (chain.size() < 4) return false;
        radius = guide.radius;
        return tangent_circle_centre(chain[0], chain[1], chain[2], chain[3], guide.radius, cursor,
                                     centre);
    case CircleBuild::Centre:
        if (chain.empty()) return false;
        centre = chain.front();
        radius = span(centre, cursor);
        return radius > 0;
    }
    return false;
}

} // namespace kentos::core
