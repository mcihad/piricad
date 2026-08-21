// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/spatial_index.hpp"

#include "piricad/core/document.hpp"

#include <algorithm>
#include <cmath>

namespace piricad::core {
namespace {

inline bool overlaps(Mm amin_x, Mm amin_y, Mm amax_x, Mm amax_y, const Box2& b)
{
    return !(amax_x < b.min_x || amin_x > b.max_x || amax_y < b.min_y || amin_y > b.max_y);
}

} // namespace

void SpatialIndex::clear()
{
    min_x_.clear();
    min_y_.clear();
    max_x_.clear();
    max_y_.clear();
    first_.clear();
    count_.clear();
    leaf_.clear();
    order_.clear();
    root_  = kNoNode;
    depth_ = 0;
}

void SpatialIndex::build(const EntityTable& store)
{
    clear();

    order_.reserve(store.size());
    for (EntityId e = 0; e < store.size(); ++e)
        if (store.alive(e)) order_.push_back(e);

    if (order_.empty()) return;

    const std::size_t n      = order_.size();
    const std::size_t leaves = (n + kFanout - 1) / kFanout;
    const std::size_t strips =
        static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(leaves))));
    const std::size_t per_strip = (n + strips - 1) / strips;

    const auto centre_x = [&store](EntityId e) { return store.min_x[e] / 2 + store.max_x[e] / 2; };
    const auto centre_y = [&store](EntityId e) { return store.min_y[e] / 2 + store.max_y[e] / 2; };

    // Ties break on the entity id so the packing is identical on every platform.
    std::sort(order_.begin(), order_.end(), [&](EntityId a, EntityId b) {
        const Mm ca = centre_x(a);
        const Mm cb = centre_x(b);
        return ca != cb ? ca < cb : a < b;
    });

    for (std::size_t s = 0; s < n; s += per_strip) {
        const auto begin = order_.begin() + static_cast<std::ptrdiff_t>(s);
        const auto end   = order_.begin() + static_cast<std::ptrdiff_t>(std::min(s + per_strip, n));
        std::sort(begin, end, [&](EntityId a, EntityId b) {
            const Mm ca = centre_y(a);
            const Mm cb = centre_y(b);
            return ca != cb ? ca < cb : a < b;
        });
    }

    // ---- leaves ----
    const std::size_t reserve_nodes = leaves + leaves / (kFanout - 1) + 8;
    min_x_.reserve(reserve_nodes);
    min_y_.reserve(reserve_nodes);
    max_x_.reserve(reserve_nodes);
    max_y_.reserve(reserve_nodes);
    first_.reserve(reserve_nodes);
    count_.reserve(reserve_nodes);
    leaf_.reserve(reserve_nodes);

    for (std::size_t s = 0; s < n; s += kFanout) {
        const std::size_t upto = std::min(s + std::size_t{kFanout}, n);

        Mm bx0 = store.min_x[order_[s]];
        Mm by0 = store.min_y[order_[s]];
        Mm bx1 = store.max_x[order_[s]];
        Mm by1 = store.max_y[order_[s]];

        for (std::size_t i = s + 1; i < upto; ++i) {
            const EntityId e = order_[i];
            bx0              = std::min(bx0, store.min_x[e]);
            by0              = std::min(by0, store.min_y[e]);
            bx1              = std::max(bx1, store.max_x[e]);
            by1              = std::max(by1, store.max_y[e]);
        }

        min_x_.push_back(bx0);
        min_y_.push_back(by0);
        max_x_.push_back(bx1);
        max_y_.push_back(by1);
        first_.push_back(static_cast<std::uint32_t>(s));
        count_.push_back(static_cast<std::uint32_t>(upto - s));
        leaf_.push_back(1);
    }

    // ---- interior levels, packed over the level below ----
    depth_                    = 1;
    std::uint32_t level_first = 0;
    auto level_count          = static_cast<std::uint32_t>(first_.size());

    while (level_count > 1) {
        const std::uint32_t produced = pack_level(level_first, level_count);
        level_first                  = static_cast<std::uint32_t>(first_.size()) - produced;
        level_count                  = produced;
        ++depth_;
    }

    root_ = static_cast<std::uint32_t>(first_.size()) - 1;
}

std::uint32_t SpatialIndex::pack_level(std::uint32_t level_first, std::uint32_t level_count)
{
    std::uint32_t produced = 0;

    for (std::uint32_t i = 0; i < level_count; i += kFanout) {
        const std::uint32_t begin = level_first + i;
        const std::uint32_t upto  = std::min(begin + kFanout, level_first + level_count);

        Mm bx0 = min_x_[begin];
        Mm by0 = min_y_[begin];
        Mm bx1 = max_x_[begin];
        Mm by1 = max_y_[begin];

        for (std::uint32_t k = begin + 1; k < upto; ++k) {
            bx0 = std::min(bx0, min_x_[k]);
            by0 = std::min(by0, min_y_[k]);
            bx1 = std::max(bx1, max_x_[k]);
            by1 = std::max(by1, max_y_[k]);
        }

        min_x_.push_back(bx0);
        min_y_.push_back(by0);
        max_x_.push_back(bx1);
        max_y_.push_back(by1);
        first_.push_back(begin);
        count_.push_back(upto - begin);
        leaf_.push_back(0);
        ++produced;
    }
    return produced;
}

Box2 SpatialIndex::bounds() const
{
    if (root_ == kNoNode) return Box2{};
    return Box2{min_x_[root_], min_y_[root_], max_x_[root_], max_y_[root_]};
}

void SpatialIndex::query(const Box2& box, std::vector<EntityId>& out) const
{
    if (root_ == kNoNode || box.empty()) return;

    // A fixed stack: the tree is at most log_16(N) deep, so 64 slots covers a
    // layer of 16^64 entities. No allocation on the query path (§10.4).
    std::uint32_t stack[64];
    int top      = 0;
    stack[top++] = root_;

    while (top > 0) {
        const std::uint32_t node = stack[--top];

        if (!overlaps(min_x_[node], min_y_[node], max_x_[node], max_y_[node], box)) continue;

        const std::uint32_t begin = first_[node];
        const std::uint32_t upto  = begin + count_[node];

        if (leaf_[node]) {
            for (std::uint32_t i = begin; i < upto; ++i)
                out.push_back(order_[i]);
            continue;
        }
        for (std::uint32_t child = begin; child < upto && top < 64; ++child)
            stack[top++] = child;
    }
}

} // namespace piricad::core
