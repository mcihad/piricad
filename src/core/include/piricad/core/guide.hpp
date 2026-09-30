// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: drafting guides.
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
// A CARDINAL guide is two facts — which axis it runs along, and where it sits.
// An ANGLED one is three — a point it passes through, a direction, and whether it
// runs both ways or only forward — so the store is five parallel columns and a
// flag (model.md's SoA rule), and a value type carries a row across the seams
// where SoA would be six parameters in a row.
//
// The angled guide arrived after the cardinal one and the model GAINED FIELDS
// rather than changing meaning, which is the only kind of change `.claude/model.md`
// allows without a migration of its own (CLAUDE.md 0.2a). On disk the new columns
// are optional blocks (io.md R10), so a file with no angled guide in it still
// opens in a build that has never heard of one; a file that HAS one raises
// `min_reader_version`, so such a build refuses it by name instead of reading a
// construction line it cannot represent.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace kentos::core {

/// Which way a guide runs.
///
/// The values reach the file (`kBlkGuideAxis`), so a value once used can never be
/// re-meant. `Angled` was appended, and a reader that does not know it refuses the
/// file rather than guessing — see this header's note on versions.
enum class GuideAxis : std::uint8_t {
    /// Runs east-west; its coordinate is a NORTHING (`yukarı`).
    Horizontal = 0,
    /// Runs north-south; its coordinate is an EASTING (`sağa`).
    Vertical = 1,
    /// Runs at `angle_udeg` through `through`; `coordinate` means nothing.
    Angled = 2,
};

/// One guide as a VALUE: what an undo record holds, what the reader hands over
/// and what the writer takes. The store stays SoA; this is the seam.
///
/// A row rather than six parallel parameters because six of them in a row is how
/// a caller comes to pass the northing where the easting goes.
struct GuideRow
{
    GuideAxis axis{GuideAxis::Horizontal}; ///< which of the three kinds this row is
    Mm coordinate{0};      ///< cardinal only: northing for horizontal, easting for vertical
    std::int64_t angle{0}; ///< angled only: micro-degrees, `core::atan2_udeg`'s own unit
    Point2 through{};      ///< angled only: a point the line passes through
    bool ray{false};       ///< angled only: forward from `through` only, not both ways

    friend bool operator==(const GuideRow&, const GuideRow&) = default;
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
    /// a vertical one. Meaningless for an angled guide, which has no single one.
    Mm coordinate(std::size_t i) const noexcept { return coords_[i]; }

    /// The angled guide's direction in micro-degrees, counter-clockwise from
    /// east — `core::atan2_udeg`'s own unit, so no conversion sits between the
    /// snap test and the stored number. Zero for a cardinal guide.
    std::int64_t angle(std::size_t i) const noexcept { return angles_[i]; }

    /// A point the angled guide passes through. Meaningless for a cardinal one.
    Point2 through(std::size_t i) const noexcept { return Point2{through_x_[i], through_y_[i]}; }

    /// Whether the angled guide runs FORWARD ONLY from its point (`tur=isin`).
    /// A cardinal guide is always infinite in both directions.
    bool ray(std::size_t i) const noexcept { return rays_[i] != 0; }

    /// The whole guide at `i`, as a value.
    GuideRow row(std::size_t i) const;

    /// Appends a cardinal guide and returns its index.
    std::size_t add(GuideAxis axis, Mm coordinate);

    /// Appends an angled guide through `at`, running at `angle` micro-degrees.
    std::size_t add_angled(Point2 at, std::int64_t angle, bool ray);

    /// Appends whatever `row` describes.
    std::size_t add_row(const GuideRow& row);

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

    /// Restores a store from a file or an undo record, replacing whatever is here.
    void load(std::vector<GuideRow> rows);

    /// Every guide, as values — what the writer and an undo record take.
    std::vector<GuideRow> rows() const;

    /// Whether any guide here is angled. What decides the file's
    /// `min_reader_version`: a drawing with only cardinal guides still opens in a
    /// build that has never heard of an angled one.
    bool any_angled() const noexcept;

private:
    std::vector<GuideAxis> axes_;
    std::vector<Mm> coords_;
    std::vector<std::int64_t> angles_;
    std::vector<Mm> through_x_;
    std::vector<Mm> through_y_;
    std::vector<std::uint8_t> rays_;
};

} // namespace kentos::core
