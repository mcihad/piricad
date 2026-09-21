// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: how an angle is written, read and turned into a direction.
//
// A surveyor and a mathematician disagree about what "45" means. The instrument
// on the tripod reads a SEMT AÇISI — clockwise from north, in grad, a full circle
// being 400 — and every Turkish traverse sheet, setting-out list and ölçü krokisi
// is written that way (kentoscad.md §5.6, §13). The trigonometry underneath reads
// the mathematical angle — counter-clockwise from east, in degrees or radians. A
// program that lets the two meet without saying which is which puts a corner
// ninety degrees from where the engineer meant it, mirrored about the 50-grad
// line, and that corner is a boundary.
//
// So the convention is DATA, carried as two session/project settings —
// `core.aci.birim` (grad · derece · radyan) and `core.aci.kural` (semt ·
// matematik) — and every place that reads an angle out of text or writes one into
// a report is handed an `AngleConvention` and asks nothing else (TODOS-CAD P0).
// The default is semt + grad, because the users are Turkish surveying engineers
// and the Turkish domain language is the product (CLAUDE.md 2.6).
//
// DETERMINISM. A polar coordinate typed at the command line becomes a stored
// coordinate — it goes into the journal and into the document — so its sine and
// cosine come from `sin_cos_udeg` (trig.hpp), never from libm: §7.3 promises the
// same drawing bit for bit on Linux, Windows and macOS, and libm does not. The
// angle is rounded ONCE to a whole micro-degree before the trigonometry, which is
// the resolution `core.yakalama.kutupsal_aci` already stores angles at
// (model.md R21). Half a micro-degree at ten kilometres is a tenth of a
// millimetre, under the storage unit.
#pragma once

#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <string>

namespace kentos::core {

/// The unit an angle is typed and printed in.
///
/// The values ARE the indices of the `core.aci.birim` enum list — `grad, derece,
/// radyan` (settings.cpp) — and MUST NOT be reordered: a saved setting is an index.
enum class AngleUnit : std::uint8_t {
    Grad   = 0, ///< a full circle is 400; the Turkish surveying default
    Degree = 1, ///< a full circle is 360
    Radian = 2, ///< a full circle is 2π
};

/// Where an angle's zero is and which way it grows.
///
/// The values ARE the indices of the `core.aci.kural` enum list — `semt,
/// matematik` — and MUST NOT be reordered for the same reason.
enum class AngleRule : std::uint8_t {
    Semt      = 0, ///< clockwise from north: what an instrument reads (semt açısı, azimut)
    Matematik = 1, ///< counter-clockwise from east: what the trigonometry reads
};

/// The two settings together — everything a reader or writer of an angle needs.
///
/// Passed as a parameter, never read from a global (TODOS-CAD P0-2): the parser
/// is one grammar for the command line, the script engine and the AI (CLAUDE.md
/// 5.11), and which convention it resolves under is the caller's context.
struct AngleConvention
{
    AngleUnit unit{AngleUnit::Grad}; ///< the unit a bare angle is written in
    AngleRule rule{AngleRule::Semt}; ///< where zero is and which way it grows

    /// Both fields compared; two conventions that print the same are equal.
    friend constexpr bool operator==(const AngleConvention&, const AngleConvention&) = default;
};

/// The unit a `core.aci.birim` setting index names. Anything outside the three
/// the setting declares is read as grad, which is the setting's own default.
constexpr AngleUnit angle_unit_from_setting(std::uint16_t index) noexcept
{
    return index <= 2 ? static_cast<AngleUnit>(index) : AngleUnit::Grad;
}

/// The rule a `core.aci.kural` setting index names; anything else is semt.
constexpr AngleRule angle_rule_from_setting(std::uint16_t index) noexcept
{
    return index == 1 ? AngleRule::Matematik : AngleRule::Semt;
}

/// The unit a one-letter suffix on a typed angle names — `@100<45g`, `@100<45d`,
/// `@100<0.7r` — or `false` when `c` is not a suffix. Upper case is accepted
/// because the command line is case-insensitive everywhere else; these three
/// letters have no dotted/dotless form, so a plain comparison is safe (5.6).
constexpr bool angle_unit_from_suffix(char c, AngleUnit& unit) noexcept
{
    switch (c) {
    case 'g':
    case 'G': unit = AngleUnit::Grad; return true;
    case 'd':
    case 'D': unit = AngleUnit::Degree; return true;
    case 'r':
    case 'R': unit = AngleUnit::Radian; return true;
    default: return false;
    }
}

/// The letter that names `unit` when written after an angle: `g`, `d` or `r`.
/// The inverse of `angle_unit_from_suffix`, for a token rendered back to text.
constexpr char angle_unit_suffix(AngleUnit unit) noexcept
{
    switch (unit) {
    case AngleUnit::Grad: return 'g';
    case AngleUnit::Degree: return 'd';
    case AngleUnit::Radian: return 'r';
    }
    return 'g';
}

/// Micro-degrees in one unit of `unit`: 900 000 per grad, 1 000 000 per degree,
/// 180 000 000 / π per radian. Written as constants so the conversion is one
/// multiplication on every platform.
constexpr double udeg_per_angle_unit(AngleUnit unit) noexcept
{
    switch (unit) {
    case AngleUnit::Grad: return 900000.0;
    case AngleUnit::Degree: return 1000000.0;
    case AngleUnit::Radian: return 180000000.0 / kPi;
    }
    return 900000.0;
}

/// An angle written in `unit` as whole MICRO-DEGREES, rounded half away from
/// zero — the one rounding a typed angle goes through before the trigonometry.
/// 45 grad is exactly 40 500 000; 45.1234 grad is exactly 40 611 060; a radian
/// value is never exact and rounds to the nearest micro-degree.
///
/// The rounding itself is `mm_round`'s (units.hpp, core.md R20), which this used
/// to keep a second copy of. `Mm` is `std::int64_t`, and round-half-away-from-zero
/// is a rule about numbers rather than about millimetres, so the micro-degree
/// scale borrows the one helper instead of repeating its cast — and borrows with
/// it the saturation that keeps a typed `@0<(2^1000)` an absurd ANGLE rather than
/// undefined behaviour. (Not `1e308`: the command line's number grammar reads
/// digits and a point, so an exponent is refused before it ever gets here. An
/// expression is the way an absurd magnitude actually arrives.) Past
/// ±`kMmSaturated` micro-degrees — some seven billion turns — the answer
/// saturates, and `sin_cos_udeg` folds it into one circle by exact integer
/// arithmetic like any other.
constexpr std::int64_t udeg_from_angle(double value, AngleUnit unit) noexcept
{
    return mm_round(value * udeg_per_angle_unit(unit));
}

/// A whole number of turns from micro-degrees, as a fraction in [0, 1) — the
/// unit-free form a report converts to grad, degree or radian in one multiply.
constexpr double turns_from_udeg(std::int64_t udeg) noexcept
{
    std::int64_t a = udeg % kUDegFullCircle;
    if (a < 0) a += kUDegFullCircle;
    return static_cast<double>(a) / static_cast<double>(kUDegFullCircle);
}

/// The offset `distance` metres along `angle` — the angle written in
/// `convention.unit`, counted under `convention.rule` — in millimetres.
///
/// Semt: east = d·sin θ, north = d·cos θ, so `@100<0` goes north and `@100<100`
/// (grad) goes east. Matematik: east = d·cos θ, north = d·sin θ, so `@100<0` goes
/// east. Each axis is rounded once by `mm_from_metres` (core.md R20); the sine
/// and cosine are `sin_cos_udeg`'s, so the four axes come out exact and the rest
/// bit-identical on every platform (§7.3).
Point2 polar_offset(double distance_metres, double angle, AngleConvention convention) noexcept;

/// The offset `distance` metres along a direction given as a fraction of a full
/// turn under `rule` — the form `direction_turns` answers in.
///
/// WHAT AN INSTRUMENT ZEROED ON A BACKSIGHT MEASURES. The operator sights a
/// known direction, sets the circle to zero, and every reading after that is an
/// angle FROM there: the absolute direction is the backsight's turn plus the
/// reading's. Turns add; angles in a unit do not add until they are turns. `ALIM`
/// reduces a field book this way and `APLİKASYON` produces one, so the two ends
/// of the same job share the arithmetic rather than each rounding its own way
/// (CLAUDE.md 5.10).
///
/// Determinism is `polar_offset`'s: `sin_cos_udeg`, and one `mm_from_metres` per
/// axis (§7.3).
Point2 polar_offset_turns(double distance_metres, double turns, AngleRule rule) noexcept;

/// The MATHEMATICAL direction of an angle written under `convention`: whole
/// micro-degrees counter-clockwise from east, which is `atan2_udeg`'s own unit
/// and what `sin_cos_udeg` takes back.
///
/// The inverse of the conversion `direction_turns` does on the way out, and here
/// so that both ends of it live in one file: a stored direction and a typed one
/// must agree or a drafting guide drawn at 45 grad snaps at 55. Semt is clockwise
/// from north, so the mathematical angle is a quarter turn minus the reading,
/// folded into one circle by exact integer steps.
std::int64_t math_udeg_from_angle(double value, AngleConvention convention) noexcept;

/// The direction from `from` to `to` under `rule`, as a fraction of a full turn
/// in [0, 1). Semt is clockwise from north, matematik counter-clockwise from
/// east. The zero vector answers 0. Deterministic: `atan2_udeg`, not libm.
double direction_turns(Point2 from, Point2 to, AngleRule rule) noexcept;

/// `turns` of a full turn written in `unit`, the way a setting-out sheet writes
/// an angle: comma decimal, four places for grad and degree (the cc a total
/// station displays), five for radian, and the unit's own suffix —
/// `62,5000 grad`, `56,2500°`, `0,98175 rad`. Negative turns are folded into
/// [0, 1) first, so a relative angle read backwards prints as the forward one.
std::string angle_text(double turns, AngleUnit unit);

/// The words a report uses to say which rule its angles obey:
/// `kuzeyden saat yönünde` for semt, `doğudan saat yönünün tersine` for
/// matematik. Turkish, because the reader is the engineer signing the sheet.
const char* angle_rule_label(AngleRule rule) noexcept;

} // namespace kentos::core
