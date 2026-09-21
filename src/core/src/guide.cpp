// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/guide.hpp"

#include <cstdlib>

namespace kentos::core {

GuideRow GuideStore::row(std::size_t i) const
{
    return GuideRow{axes_[i], coords_[i], angles_[i], Point2{through_x_[i], through_y_[i]},
                    rays_[i] != 0};
}

std::size_t GuideStore::add(GuideAxis axis, Mm coordinate)
{
    return add_row(GuideRow{axis, coordinate, 0, Point2{}, false});
}

std::size_t GuideStore::add_angled(Point2 at, std::int64_t angle, bool ray)
{
    return add_row(GuideRow{GuideAxis::Angled, 0, angle, at, ray});
}

std::size_t GuideStore::add_row(const GuideRow& row)
{
    // EVERY COLUMN GROWS TOGETHER, always. A column that is appended to only for
    // the kind that uses it would leave the others short, and then `axis(i)` and
    // `angle(i)` would name two different guides.
    axes_.push_back(row.axis);
    coords_.push_back(row.coordinate);
    angles_.push_back(row.angle);
    through_x_.push_back(row.through.x);
    through_y_.push_back(row.through.y);
    rays_.push_back(row.ray ? 1u : 0u);
    return coords_.size() - 1;
}

bool GuideStore::remove(std::size_t i)
{
    if (i >= coords_.size()) return false;
    const auto at = static_cast<std::ptrdiff_t>(i);
    axes_.erase(axes_.begin() + at);
    coords_.erase(coords_.begin() + at);
    angles_.erase(angles_.begin() + at);
    through_x_.erase(through_x_.begin() + at);
    through_y_.erase(through_y_.begin() + at);
    rays_.erase(rays_.begin() + at);
    return true;
}

std::size_t GuideStore::nearest(GuideAxis axis, Mm coordinate, Mm tolerance) const noexcept
{
    std::size_t best = coords_.size();
    Mm best_distance = tolerance;

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
    angles_.clear();
    through_x_.clear();
    through_y_.clear();
    rays_.clear();
}

void GuideStore::load(std::vector<GuideRow> rows)
{
    clear();
    for (const GuideRow& one : rows)
        (void)add_row(one);
}

std::vector<GuideRow> GuideStore::rows() const
{
    std::vector<GuideRow> out;
    out.reserve(coords_.size());
    for (std::size_t i = 0; i < coords_.size(); ++i)
        out.push_back(row(i));
    return out;
}

bool GuideStore::any_angled() const noexcept
{
    for (const GuideAxis a : axes_)
        if (a == GuideAxis::Angled) return true;
    return false;
}

} // namespace kentos::core
