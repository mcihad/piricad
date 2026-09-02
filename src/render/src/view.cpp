// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/view.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::render {

ScreenPointF to_f(ScreenPoint p) noexcept
{
    return ScreenPointF{static_cast<float>(p.x), static_cast<float>(p.y)};
}

void ViewTransform::set_viewport(int width_px, int height_px)
{
    width_  = std::max(1, width_px);
    height_ = std::max(1, height_px);
}

void ViewTransform::set_centre(Point2 centre, double mm_per_pixel)
{
    centre_       = centre;
    mm_per_pixel_ = mm_per_pixel > 0.0 ? mm_per_pixel : 1.0;
}

void ViewTransform::fit(const Box2& box, double margin)
{
    if (box.empty()) {
        set_centre(Point2{}, 10.0);
        return;
    }

    const double w  = static_cast<double>(box.width());
    const double h  = static_cast<double>(box.height());
    const double sx = w > 0.0 ? w / static_cast<double>(width_) : 0.0;
    const double sy = h > 0.0 ? h / static_cast<double>(height_) : 0.0;

    double scale = std::max(sx, sy);
    if (scale <= 0.0) scale = 1.0; // a single point: pick a sane default
    scale *= (1.0 + 2.0 * margin);

    set_centre(box.centre(), scale);
}

void ViewTransform::pan_pixels(double dx_px, double dy_px)
{
    centre_.x -= static_cast<Mm>(std::llround(dx_px * mm_per_pixel_));
    centre_.y += static_cast<Mm>(std::llround(dy_px * mm_per_pixel_));
}

void ViewTransform::zoom_at(ScreenPoint anchor, double factor)
{
    if (factor <= 0.0) return;

    const Point2 before = to_world(anchor);
    mm_per_pixel_       = std::clamp(mm_per_pixel_ / factor, 1e-4, 1e9);
    const Point2 after  = to_world(anchor);

    centre_.x += before.x - after.x;
    centre_.y += before.y - after.y;
}

ScreenPoint ViewTransform::to_screen(Point2 world) const
{
    const double dx = static_cast<double>(world.x - centre_.x) / mm_per_pixel_;
    const double dy = static_cast<double>(world.y - centre_.y) / mm_per_pixel_;
    return ScreenPoint{static_cast<double>(width_) * 0.5 + dx,
                       static_cast<double>(height_) * 0.5 - dy};
}

Point2 ViewTransform::to_world(ScreenPoint screen) const
{
    const double dx = (screen.x - static_cast<double>(width_) * 0.5) * mm_per_pixel_;
    const double dy = (static_cast<double>(height_) * 0.5 - screen.y) * mm_per_pixel_;
    return Point2{centre_.x + static_cast<Mm>(std::llround(dx)),
                  centre_.y + static_cast<Mm>(std::llround(dy))};
}

float ViewTransform::offset_x_f(Mm world_x) const
{
    return static_cast<float>(static_cast<double>(world_x - centre_.x) / mm_per_pixel_);
}

float ViewTransform::offset_y_f(Mm world_y) const
{
    return static_cast<float>(static_cast<double>(world_y - centre_.y) / mm_per_pixel_);
}

Box2 ViewTransform::visible_box() const
{
    const Point2 a = to_world(ScreenPoint{0.0, static_cast<double>(height_)});
    const Point2 b = to_world(ScreenPoint{static_cast<double>(width_), 0.0});
    Box2 box{};
    box.extend(a);
    box.extend(b);
    return box;
}

double ViewTransform::scale_denominator(double dpi) const
{
    const double mm_per_screen_mm = mm_per_pixel_ * (dpi / 25.4);
    return mm_per_screen_mm;
}

} // namespace kentos::render
