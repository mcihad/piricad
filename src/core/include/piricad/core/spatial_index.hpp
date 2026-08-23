// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the spatial index.
//
// piricad.md §10.5: "Bulk-load STR R-tree — tek tek insert değil." Inserting five
// million parcels one at a time builds a badly balanced tree slowly; packing them
// bottom-up builds a tight one in a single pass.
//
// Sort-Tile-Recursive packing: sort entities by box centre X into vertical
// strips, sort each strip by centre Y, then fill leaves in that order. Spatially
// close entities end up in the same leaf, so a viewport query touches a handful
// of nodes instead of the whole layer.
//
// The node record is structure-of-arrays like everything else in core, and the
// traversal reads only the four box arrays — never the geometry.
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace piricad::core {

/// Forward-declared: the index is built from the entity columns.
class EntityTable;

class SpatialIndex
{
public:
    /// Entries per node. Sixteen 64-bit boxes are four cache lines, which is the
    /// most a single traversal step should touch.
    static constexpr std::uint32_t kFanout = 16;

    /// Bulk-builds over every live entity in `table`. Any previous contents are
    /// discarded. Deterministic: ties in the sort are broken on entity id, so the
    /// same document always produces the same tree (§7.3).
    ///
    /// Reads the cull block and nothing else — the four bbox arrays and the flags
    /// byte (model.md R6).
    void build(const EntityTable& table);

    void clear();

    bool empty() const noexcept { return root_ == kNoNode; }

    std::size_t entity_count() const noexcept { return order_.size(); }

    std::size_t node_count() const noexcept { return first_.size(); }

    std::size_t depth() const noexcept { return depth_; }

    /// Box of the whole tree. A query that contains this selects everything, and
    /// the caller is better off scanning the store sequentially than walking the
    /// tree to be told so.
    Box2 bounds() const;

    /// Appends CANDIDATES: every entity sharing a leaf with the query box. The
    /// result is a superset — testing each entity's own box here would mean random
    /// access into the store's cull arrays, so the exact test is left to the
    /// caller, which is doing it anyway on a much smaller set.
    /// `out` is not cleared and its capacity is reused, so a warm per-frame query
    /// allocates nothing (§10.4).
    void query(const Box2& box, std::vector<EntityId>& out) const;

private:
    static constexpr std::uint32_t kNoNode = 0xFFFFFFFFu;

    std::uint32_t pack_level(std::uint32_t level_first, std::uint32_t level_count);

    // ---- node records, structure-of-arrays ----
    std::vector<Mm> min_x_;
    std::vector<Mm> min_y_;
    std::vector<Mm> max_x_;
    std::vector<Mm> max_y_;
    std::vector<std::uint32_t> first_; ///< first child node, or first slot in order_
    std::vector<std::uint32_t> count_;
    std::vector<std::uint8_t> leaf_;

    std::vector<EntityId> order_; ///< live entities in STR order
    std::uint32_t root_{kNoNode};
    std::size_t depth_{0};
};

} // namespace piricad::core
