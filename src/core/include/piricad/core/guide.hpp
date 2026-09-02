// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: drafting guides.
//
// The infinite construction line a drafter pulls off a ruler: a horizontal or
// vertical line at a fixed coordinate that the cursor snaps to and the plot never
// prints. Every drawing board had them as pencil ticks on the frame and every
// program since Inkscape has had them as draggable lines.
//
// NOT AN ENTITY, and that is the decision this file records. A guide has no
// geometry the document owns, no style, no layer, no attributes, and must never
// appear in a selection, a cull pass, an export or an area sum — putting it in
// the entity table would mean teaching every one of those to ignore it, which is
// how a construction line ends up in a tapu. It is document FURNITURE: saved with
// the file, restored with it, and invisible to everything that reasons about
// what is drawn.
//
// A guide is two facts — which axis it is perpendicular to, and where it sits —
// so the store is two parallel columns and nothing else (model.md's SoA rule).
#pragma once

#include "piricad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace piricad::core {

/// Which way a guide runs.
enum class GuideAxis : std::uint8_t {
    /// Runs east-west; its coordinate is a NORTHING (`yukarı`).
    Horizontal = 0,
    /// Runs north-south; its coordinate is an EASTING (`sağa`).
    Vertical = 1,
};

/// The guides a document carries, in the order they were made.
///
/// Order is kept rather than sorted: a user who places three guides and removes
/// "the second one" means the second they placed. Nothing here is a hot path —
/// a drawing has a handful of guides, not a million — so this is a plain vector
/// pair rather than an arena.
class GuideStore
{
public:
    /// How many guides this document carries.
    std::size_t size() const noexcept { return coords_.size(); }

    bool empty() const noexcept { return coords_.empty(); }

    /// Which way the guide at `i` runs. `i` must be below `size()`.
    GuideAxis axis(std::size_t i) const noexcept { return axes_[i]; }

    /// The guide's coordinate: a northing for a horizontal guide, an easting for
    /// a vertical one.
    Mm coordinate(std::size_t i) const noexcept { return coords_[i]; }

    /// Appends a guide and returns its index.
    std::size_t add(GuideAxis axis, Mm coordinate);

    /// Removes the guide at `i`. Returns false when `i` is past the end.
    ///
    /// The later guides shift down, which is why nothing may hold an index across
    /// a removal — and why the command addresses a guide by its coordinate rather
    /// than by an index a user would have to count.
    bool remove(std::size_t i);

    /// Index of the guide nearest `coordinate` on `axis` within `tolerance`, or
    /// `size()` when none is that close. How a click on a guide finds it.
    std::size_t nearest(GuideAxis axis, Mm coordinate, Mm tolerance) const noexcept;

    void clear() noexcept;

    /// Restores a store from a file, replacing whatever is here.
    void load(std::vector<GuideAxis> axes, std::vector<Mm> coords);

    /// The raw columns, for the writer.
    const std::vector<GuideAxis>& axes() const noexcept { return axes_; }

    const std::vector<Mm>& coordinates() const noexcept { return coords_; }

private:
    std::vector<GuideAxis> axes_;
    std::vector<Mm> coords_;
};

} // namespace piricad::core
