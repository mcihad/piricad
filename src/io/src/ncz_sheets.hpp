// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): a Netcad map sheet (`pafta`) drawn as the sheet it is.
//
// A MapSheet record holds two points and the sheet's name, and nothing else. The
// reference plugin reads the two points as opposite corners of an axis-aligned
// rectangle, and this port did the same; on the 1:1000 index of a real plan in
// TM39 that drew 127 rectangles, each overlapping the sheet above it by 5,5 m
// and shifted 7 m sideways from it.
//
// WHAT THE TWO POINTS ARE: the BOUNDING BOX of the sheet. A sheet is a cell of
// latitude and longitude (22,5″ × 22,5″ at 1:1000), and in a transverse
// Mercator zone that cell is a quadrilateral turned by the meridian convergence
// — 0,64° at 38° E in a zone whose central meridian is 39°. Each side of the box
// touches one corner of it: west of the central meridian the south-west corner
// gives the least easting, the south-east the least northing, the north-east
// the greatest easting and the north-west the greatest northing, and east of it
// the mirror image. Measured on that plan: the cell 40°11′15″–40°11′37,5″ N ×
// 38°04′52,5″–38°05′15″ E, projected into TUREF/TM39, has to 0,14 mm the box
// the file stores for its sheet H40-D-07-B-1-C, and the frames recovered this
// way meet their neighbours corner to corner.
//
// So a frame is recovered WITHOUT any rule for naming or dividing sheets: the
// four bounds of the cell are solved for, by Newton's method, until the
// projected corners have exactly the stored box, and those corners are the
// frame. The projection is the one the file declares (its MPROJ: datum, 3° or
// 6° zone), through GDAL's PROJ (Article 2.7). A file that declares no zone, a
// build without GDAL, or a box the model does not reproduce to a centimetre
// keeps the rectangle, and the reader says how many and why.
#pragma once

#include "ncz_format.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>

namespace kentos::io::ncz {

/// Map sheet frames in the projection one file declares.
class SheetFrames
{
public:
    /// Prepares the projection `header` declares. Never fails: when it cannot
    /// recover frames, `usable()` is false and `why_not()` says why.
    explicit SheetFrames(const Header& header);
    ~SheetFrames();
    SheetFrames(const SheetFrames&)            = delete;
    SheetFrames& operator=(const SheetFrames&) = delete;

    /// True when frames can be recovered for this file.
    bool usable() const noexcept;

    /// Why `usable()` is false, as the end of a Turkish sentence: `dosya bir TM
    /// ya da UTM dilimi bildirmiyor`. Empty when it is usable.
    const std::string& why_not() const noexcept;

    /// The sheet whose bounding box is `min`–`max` (easting, northing, metres):
    /// its corners south-west, south-east, north-east, north-west — counter-
    /// clockwise. Nothing when the frames are not usable or the box is not the
    /// box of a cell of latitude and longitude to within a centimetre.
    std::optional<std::array<Coord, 4>> frame(double min_easting, double min_northing,
                                              double max_easting, double max_northing) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace kentos::io::ncz
