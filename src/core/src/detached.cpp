// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/detached.hpp"

#include "kentos_cad/core/document.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kentos::core {

namespace {

/// The floor of the majority's radius, in millimetres: 100 m.
constexpr double kRadiusFloorMm = 100'000.0;

/// The median of `values`, the lower one for an even count. Reorders them.
Mm median_of(std::vector<Mm>& values)
{
    const std::size_t mid = (values.size() - 1) / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid),
                     values.end());
    return values[mid];
}

} // namespace

DetachedReport find_detached(const Document& doc, int factor)
{
    DetachedReport out;

    struct Sample
    {
        EntityId entity;
        Point2 centre;
    };

    std::vector<Sample> samples;
    for (EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        const Box2 box = doc.entity_extent(e);
        if (box.empty()) continue;
        // The midpoint in integers, halved towards zero: exact on every platform.
        samples.push_back({e, Point2{box.min_x + (box.max_x - box.min_x) / 2,
                                     box.min_y + (box.max_y - box.min_y) / 2}});
    }
    out.checked = samples.size();
    if (samples.size() < 3) return out;

    std::vector<Mm> xs, ys;
    xs.reserve(samples.size());
    ys.reserve(samples.size());
    for (const Sample& s : samples) {
        xs.push_back(s.centre.x);
        ys.push_back(s.centre.y);
    }
    out.centre = Point2{median_of(xs), median_of(ys)};

    std::vector<double> distance(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double dx = static_cast<double>(samples[i].centre.x - out.centre.x);
        const double dy = static_cast<double>(samples[i].centre.y - out.centre.y);
        distance[i]     = std::sqrt(dx * dx + dy * dy);
    }
    std::vector<std::size_t> order(samples.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    // Ties by slot, so the order — and the report — is the same everywhere.
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return distance[a] != distance[b] ? distance[a] < distance[b]
                                          : samples[a].entity < samples[b].entity;
    });

    // THE NEARER HALF IS THE MAJORITY; walk outwards for the first empty band.
    const std::size_t majority = (samples.size() + 1) / 2;
    const double wide          = static_cast<double>(factor);
    std::size_t cut            = samples.size();
    out.radius_mm              = distance[order[samples.size() - 1]];
    for (std::size_t k = majority - 1; k + 1 < samples.size(); ++k) {
        const double reach = distance[order[k]];
        const double next  = distance[order[k + 1]];
        if (next - reach >= wide * std::max(reach, kRadiusFloorMm)) {
            cut           = k + 1;
            out.radius_mm = reach;
            break;
        }
    }

    for (std::size_t k = cut; k < samples.size(); ++k) {
        const Sample& s = samples[order[k]];
        out.detached.push_back({s.entity, s.centre, distance[order[k]]});
    }
    return out;
}

} // namespace kentos::core
