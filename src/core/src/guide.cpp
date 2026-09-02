// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/guide.hpp"

#include <cstdlib>

namespace kentos::core {

std::size_t GuideStore::add(GuideAxis axis, Mm coordinate)
{
    axes_.push_back(axis);
    coords_.push_back(coordinate);
    return coords_.size() - 1;
}

bool GuideStore::remove(std::size_t i)
{
    if (i >= coords_.size()) return false;
    axes_.erase(axes_.begin() + static_cast<std::ptrdiff_t>(i));
    coords_.erase(coords_.begin() + static_cast<std::ptrdiff_t>(i));
    return true;
}

std::size_t GuideStore::nearest(GuideAxis axis, Mm coordinate, Mm tolerance) const noexcept
{
    std::size_t best  = coords_.size();
    Mm best_distance  = tolerance;

    for (std::size_t i = 0; i < coords_.size(); ++i) {
        if (axes_[i] != axis) continue;

        const Mm d = coords_[i] > coordinate ? coords_[i] - coordinate : coordinate - coords_[i];
        // Strictly nearer, so the FIRST of two guides at the same coordinate wins
        // and the answer does not depend on iteration order.
        if (d < best_distance || (best == coords_.size() && d <= tolerance)) {
            best          = i;
            best_distance = d;
        }
    }
    return best;
}

void GuideStore::clear() noexcept
{
    axes_.clear();
    coords_.clear();
}

void GuideStore::load(std::vector<GuideAxis> axes, std::vector<Mm> coords)
{
    // The shorter of the two bounds both: a file whose columns disagree is
    // corrupt, and the reader refuses it before this is called. Truncating here
    // is belt and braces rather than a policy.
    const std::size_t n = axes.size() < coords.size() ? axes.size() : coords.size();
    axes.resize(n);
    coords.resize(n);
    axes_   = std::move(axes);
    coords_ = std::move(coords);
}

} // namespace kentos::core
