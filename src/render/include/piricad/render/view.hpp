// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — render: the world/screen transform, and the jitter defence.
//
// piricad.md §10.3, the single most important rendering rule:
//
//   TUREF/TM3 coordinates are seven digits. Writing a double world coordinate
//   straight into a float vertex attribute produces metre-scale shimmer on
//   screen. Offset against the view centre FIRST, convert to float only after.
//
// ViewTransform therefore never exposes an absolute world coordinate as a float.
// The only float-producing call takes the origin offset into account by design.
#pragma once

#include "piricad/core/units.hpp"

namespace piricad::render {

using core::Box2;
using core::Mm;
using core::Point2;

struct ScreenPoint
{
    double x{0.0};
    double y{0.0};
};

class ViewTransform
{
public:
    /// Sets the viewport size in device pixels.
    void set_viewport(int width_px, int height_px);

    /// Centres the view on `centre` at `mm_per_pixel` scale.
    void set_centre(Point2 centre, double mm_per_pixel);

    /// Fits `box` into the viewport with a margin fraction (0.05 = 5%).
    void fit(const Box2& box, double margin = 0.05);

    void pan_pixels(double dx_px, double dy_px);
    void zoom_at(ScreenPoint anchor, double factor);

    Point2 centre() const noexcept { return centre_; }

    double mm_per_pixel() const noexcept { return mm_per_pixel_; }

    int width() const noexcept { return width_; }

    int height() const noexcept { return height_; }

    /// World -> screen. Y is flipped: north is up.
    ScreenPoint to_screen(Point2 world) const;

    /// Screen -> world. Exact within one millimetre, which is the storage unit.
    Point2 to_world(ScreenPoint screen) const;

    /// The offset every GPU vertex must be expressed against (§10.3).
    /// Never upload `to_screen(p)` of an absolute coordinate as a float.
    Point2 origin_offset() const noexcept { return centre_; }

    /// World coordinate as a float, relative to the origin offset. This is the
    /// ONLY sanctioned float conversion in the render path.
    float offset_x_f(Mm world_x) const;
    float offset_y_f(Mm world_y) const;

    /// Visible world rectangle, for frustum culling.
    Box2 visible_box() const;

    /// Scale denominator for a 1:N readout, given the display DPI.
    double scale_denominator(double dpi = 96.0) const;

private:
    Point2 centre_{};
    double mm_per_pixel_{10.0};
    int width_{1};
    int height_{1};
};

} // namespace piricad::render
