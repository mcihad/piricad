// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/aids.hpp"

#include "piricad/core/document.hpp"

namespace piricad::command {
namespace {

/// Screen pixels to document millimetres. Rounds up to one millimetre when a
/// scale is published at all, so a legal aperture never collapses to zero and
/// silently turns snapping off — zero has one meaning here, "there is no view".
core::Mm radius_from_pixels(std::int64_t pixels, double mm_per_pixel)
{
    if (mm_per_pixel <= 0.0 || pixels <= 0) return 0;
    const core::Mm mm = core::mm_round(static_cast<double>(pixels) * mm_per_pixel);
    return mm > 0 ? mm : 1;
}

} // namespace

void InputAids::set_view_scale(double mm_per_pixel) noexcept
{
    mm_per_pixel_ = mm_per_pixel > 0.0 ? mm_per_pixel : 0.0;
}

const AidSettings& InputAids::settings(const core::Settings& app,
                                       const core::Settings& session) const
{
    if (cached_scale_ == mm_per_pixel_ && cached_app_revision_ == app.revision() &&
        cached_session_revision_ == session.revision())
        return cache_;

    AidSettings out;

    const auto declared = static_cast<std::uint16_t>(session.get("core.yakalama.modlar").as_int());
    out.modes           = static_cast<std::uint16_t>(declared & core::SnapAllMask);

    // One engine input, two doors: F9 / `MOD ızgaraya_yakala` and the mask bit are
    // the same switch, so the canvas and the engine cannot disagree about whether
    // the grid is live.
    if (session.get("core.yakalama.izgara").as_bool())
        out.modes = static_cast<std::uint16_t>(out.modes | core::SnapGrid);

    out.ortho      = session.get("core.yakalama.dik_mod").as_bool();
    out.polar_step = session.get("core.yakalama.kutupsal_aci").as_int();

    out.snap_radius = radius_from_pixels(app.get("core.yakalama.tolerans").as_int(), mm_per_pixel_);
    out.pick_radius = radius_from_pixels(app.get("core.secim.tolerans").as_int(), mm_per_pixel_);
    out.grid_step   = app.get("core.izgara.adim").as_length();

    cache_                   = out;
    cached_scale_            = mm_per_pixel_;
    cached_app_revision_     = app.revision();
    cached_session_revision_ = session.revision();
    return cache_;
}

core::SnapResult InputAids::resolve(const core::Document& doc, const AidSettings& s,
                                    core::Point2 aim, bool has_base, core::Point2 base) const
{
    core::SnapQuery q;
    q.aim        = aim;
    q.radius     = s.snap_radius;
    q.modes      = s.modes;
    q.grid_step  = s.grid_step;
    q.ortho      = s.ortho;
    q.polar_step = s.polar_step;
    q.has_base   = has_base;
    q.base       = base;

    return core::snap(doc, q);
}

} // namespace piricad::command
