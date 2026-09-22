// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/polygon.hpp"

#include "kentos_cad/core/pick.hpp"

#include <cmath>
#include <cstring>

namespace kentos::core {
namespace {

bool sides_in_range(std::int64_t sides) noexcept
{
    return sides >= kPolygonMinSides && sides <= kPolygonMaxSides;
}

double pi_over_n(std::int64_t sides) noexcept
{
    return std::acos(-1.0) / static_cast<double>(sides);
}

} // namespace

double polygon_circumradius(double given_metres, std::int64_t sides, PolygonFit fit) noexcept
{
    if (!sides_in_range(sides) || !(given_metres > 0.0)) return 0.0;
    switch (fit) {
    case PolygonFit::Inscribed: return given_metres;
    case PolygonFit::Circumscribed: return given_metres / std::cos(pi_over_n(sides));
    case PolygonFit::Side: return given_metres / (2.0 * std::sin(pi_over_n(sides)));
    }
    return given_metres;
}

double polygon_measurement(double circumradius_metres, std::int64_t sides, PolygonFit fit) noexcept
{
    if (!sides_in_range(sides) || !(circumradius_metres > 0.0)) return 0.0;
    switch (fit) {
    case PolygonFit::Inscribed: return circumradius_metres;
    case PolygonFit::Circumscribed: return circumradius_metres * std::cos(pi_over_n(sides));
    case PolygonFit::Side: return circumradius_metres * 2.0 * std::sin(pi_over_n(sides));
    }
    return circumradius_metres;
}

double polygon_half_step_turns(std::int64_t sides) noexcept
{
    if (!sides_in_range(sides)) return 0.0;
    return 0.5 / static_cast<double>(sides);
}

void regular_polygon_corners(Point2 centre, std::int64_t sides, double circumradius_metres,
                             double start_turns, AngleRule rule, std::vector<Point2>& out)
{
    if (!sides_in_range(sides) || !(circumradius_metres > 0.0)) return;

    const auto n = static_cast<double>(sides);
    out.reserve(out.size() + static_cast<std::size_t>(sides));
    for (std::int64_t i = 0; i < sides; ++i) {
        // COUNTER-CLOCKWISE IN THE DRAWING whichever rule is in force: under
        // semt an increasing angle turns clockwise, so the step is negated there
        // to keep the ring wound the one way the model stores (model.md R11).
        const double step = static_cast<double>(i) / n;
        const double at   = rule == AngleRule::Semt ? start_turns - step : start_turns + step;
        out.push_back(centre + polar_offset_turns(circumradius_metres, at, rule));
    }
}

std::vector<Point2> regular_polygon_corners(Point2 centre, std::int64_t sides,
                                            double circumradius_metres, double start_turns,
                                            AngleRule rule)
{
    std::vector<Point2> out;
    regular_polygon_corners(centre, sides, circumradius_metres, start_turns, rule, out);
    return out;
}

bool edge_rectangle_corners(Point2 first, Point2 second, Point2 across,
                            std::array<Point2, 4>& out) noexcept
{
    if (first == second) return false;

    Point2 foot{};
    double t = 0.0;
    if (!closest_point_on_line(first, second, across, foot, t)) return false;

    const Mm dx = across.x - foot.x;
    const Mm dy = across.y - foot.y;
    if (dx == 0 && dy == 0) return false; // the third point is on the edge

    out = {first, second, Point2{second.x + dx, second.y + dy}, Point2{first.x + dx, first.y + dy}};
    return true;
}

std::vector<std::uint8_t> encode_polygon_guide(const PolygonGuide& guide)
{
    std::vector<std::uint8_t> bytes(17);
    std::memcpy(bytes.data(), &guide.sides, sizeof(guide.sides));
    bytes[8] = static_cast<std::uint8_t>(guide.fit);
    std::memcpy(bytes.data() + 9, &guide.circumradius, sizeof(guide.circumradius));
    return bytes;
}

std::optional<PolygonGuide> decode_polygon_guide(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 17) return std::nullopt;
    if (bytes[8] > static_cast<std::uint8_t>(PolygonFit::Side)) return std::nullopt;

    PolygonGuide guide{};
    std::memcpy(&guide.sides, bytes.data(), sizeof(guide.sides));
    guide.fit = static_cast<PolygonFit>(bytes[8]);
    std::memcpy(&guide.circumradius, bytes.data() + 9, sizeof(guide.circumradius));
    if (!sides_in_range(guide.sides)) return std::nullopt;
    return guide;
}

} // namespace kentos::core
