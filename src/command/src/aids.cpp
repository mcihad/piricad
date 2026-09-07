// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/aids.hpp"

#include "kentos_cad/core/document.hpp"

namespace kentos::command {
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
    out.modes           = static_cast<std::uint32_t>(declared & core::SnapAllMask);

    // One engine input, two doors: F9 / `MOD ızgaraya_yakala` and the mask bit are
    // the same switch, so the canvas and the engine cannot disagree about whether
    // the grid is live.
    if (session.get("core.yakalama.izgara").as_bool())
        out.modes = static_cast<std::uint32_t>(out.modes | core::SnapGrid);

    out.ortho       = session.get("core.yakalama.dik_mod").as_bool();
    out.normal_lock = session.get("core.yakalama.yuzey_normali").as_bool();
    out.polar_step  = session.get("core.yakalama.kutupsal_aci").as_int();

    // The diagonal lock is polar tracking at 45°, and it OVERRIDES the configured
    // step while it is held. A second corner locked to a 45° diagonal from the
    // first is a square, which is what Ctrl means in a drawing program — so this
    // needs no new constraint in the engine, only a step it already understands.
    if (session.get("core.yakalama.kosegen").as_bool()) {
        constexpr std::int64_t kDiagonal = 45'000'000; // micro-degrees
        out.polar_step                   = kDiagonal;
        out.modes                        = static_cast<std::uint32_t>(out.modes | core::SnapPolar);
    }

    out.step        = session.get("core.yakalama.adim").as_length();
    out.snap_radius = radius_from_pixels(app.get("core.yakalama.tolerans").as_int(), mm_per_pixel_);

    // THE SAME DISTANCE THE OBJECT SNAP USES, and set AFTER it for that reason.
    // The surface a perpendicular is struck from is the one under the point the
    // run leaves, so the lock looks exactly as far as a snap would — and no
    // further, because a perpendicular to an edge the user cannot see is a
    // direction they did not choose.
    out.normal_reach = out.snap_radius;

    // A multiple of the aperture, not a fixed distance. The reach then follows the
    // zoom the way a user expects: an extension worth offering at 1:1000 covers
    // tens of metres, and the same gesture at 1:100 covers tens of centimetres.
    // Zero switches the constructed modes off, which is the contract the engine
    // already keeps for `grid_step` and `polar_step`.
    const std::int64_t factor = app.get("core.yakalama.uzanti_carpani").as_int();
    out.reach                 = factor > 0 ? static_cast<core::Mm>(out.snap_radius * factor) : 0;
    out.pick_radius = radius_from_pixels(app.get("core.secim.tolerans").as_int(), mm_per_pixel_);
    // The step ACTUALLY IN FORCE at this zoom, not the declared one. With
    // adaptive spacing on — the default — the lines on screen are the 1-2-5
    // ladder's and the declared step is not among them; snapping to the declared
    // one puts the point on a lattice nowhere on the canvas.
    out.grid_step =
        core::grid_step_in_force(app.get("core.izgara.adim").as_length(),
                                 app.get("core.izgara.mod").as_enum() == 0, mm_per_pixel_);

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

    // CARRIED, and it was not. The lock was read from the settings, cached in
    // `AidSettings` and then dropped on the floor here — so `MOD yüzey_normali
    // evet` reported success, the menu tick came on, and every line came out
    // exactly where it would have without it. A query field that is never
    // written is a feature that is never wired.
    q.normal_lock  = s.normal_lock;
    q.normal_reach = s.normal_reach;
    q.reach        = s.reach;
    q.step         = s.step;

    return core::snap(doc, q);
}

} // namespace kentos::command
