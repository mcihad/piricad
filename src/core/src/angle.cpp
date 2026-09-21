// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/angle.hpp"

#include "kentos_cad/core/trig.hpp"

#include <cstdint>
#include <string>

namespace kentos::core {

Point2 polar_offset(double distance_metres, double angle, AngleConvention convention) noexcept
{
    const SinCos t = sin_cos_udeg(udeg_from_angle(angle, convention.unit));

    // The two rules differ only in which trigonometric function feeds which
    // axis: an azimuth's sine is its easting, a mathematical angle's cosine is.
    const double east  = convention.rule == AngleRule::Semt ? t.sin : t.cos;
    const double north = convention.rule == AngleRule::Semt ? t.cos : t.sin;

    return Point2{
        .x = mm_from_metres(distance_metres * east),
        .y = mm_from_metres(distance_metres * north),
    };
}

double direction_turns(Point2 from, Point2 to, AngleRule rule) noexcept
{
    const Mm dx = to.x - from.x;
    const Mm dy = to.y - from.y;
    if (dx == 0 && dy == 0) return 0.0;

    // `atan2_udeg` answers counter-clockwise from east — the mathematical angle.
    // An azimuth is the same direction read the other way round: a quarter turn
    // minus it, kept in [0, 360°) by an exact integer step.
    const std::int64_t math = atan2_udeg(dy, dx);
    if (rule == AngleRule::Matematik) return turns_from_udeg(math);

    const std::int64_t quarter = kUDegFullCircle / 4;
    return turns_from_udeg(quarter - math);
}

std::string angle_text(double turns, AngleUnit unit)
{
    // Into [0, 1) whatever came in: a relative angle computed as a difference of
    // two directions may be negative, and a sheet prints the forward reading.
    turns -= static_cast<double>(static_cast<std::int64_t>(turns));
    if (turns < 0.0) turns += 1.0;

    // A full turn and the printed precision, per unit. Grad and degree print four
    // places — the cc a total station displays; radians five, because 0,0001 rad
    // is 6,4 cc and would be coarser than the instrument. `whole_turn` is the
    // circle in whole units where there is one, so a reading that rounds up to
    // the top of the range can be folded back to zero exactly; a radian circle
    // is not a whole number and is left alone.
    double full             = 400.0;
    std::int64_t whole_turn = 400;
    int places              = 4;
    const char* suffix      = " grad";
    switch (unit) {
    case AngleUnit::Grad: break;
    case AngleUnit::Degree:
        full       = 360.0;
        whole_turn = 360;
        suffix     = "°";
        break;
    case AngleUnit::Radian:
        full       = 2.0 * kPi;
        whole_turn = 0;
        places     = 5;
        suffix     = " rad";
        break;
    }

    std::int64_t scale = 1;
    for (int i = 0; i < places; ++i)
        scale *= 10;

    // Half away from zero on a non-negative value, spelled out so no libm
    // rounding function decides the last digit of a report.
    const double scaled = turns * full * static_cast<double>(scale);
    auto whole          = static_cast<std::int64_t>(scaled);
    if (scaled - static_cast<double>(whole) >= 0.5) ++whole;

    // 400,0000 is how a zero angle reads after rounding at the top of the range;
    // it is written as zero.
    if (whole_turn != 0 && whole >= whole_turn * scale) whole -= whole_turn * scale;

    std::string frac = std::to_string(whole % scale);
    frac             = std::string(static_cast<std::size_t>(places) - frac.size(), '0') + frac;

    return std::to_string(whole / scale) + "," + frac + suffix;
}

const char* angle_rule_label(AngleRule rule) noexcept
{
    return rule == AngleRule::Matematik ? "doğudan saat yönünün tersine" : "kuzeyden saat yönünde";
}

} // namespace kentos::core
