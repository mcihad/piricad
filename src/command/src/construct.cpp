// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/construct.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::command {
namespace {

using core::err;
using core::ErrorCode;
using core::Mm;
using core::Point2;

/// A direction has no second point, so one is made: a hundred kilometres along
/// it, which is past the far corner of any Turkish sheet and is never written
/// here as a length that matters (CLAUDE.md 5.16).
constexpr double kDirectionRay = 100000.0;

} // namespace

std::string metres_text(Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

core::Result<Point2> direction_crossing(Point2 a, double angle_a,
                                        core::AngleConvention convention_a, Point2 b,
                                        double angle_b, core::AngleConvention convention_b)
{
    // PARALLEL IS DECIDED IN INTEGERS, before any trigonometry. Two directions a
    // half turn apart give ray endpoints whose cross product is a rounding
    // artefact rather than zero, and `line_intersection` would then answer with
    // a point somewhere past the moon instead of refusing.
    const std::int64_t half = core::kUDegFullCircle / 2;
    std::int64_t apart      = (core::udeg_from_angle(angle_a, convention_a.unit) -
                          core::udeg_from_angle(angle_b, convention_b.unit)) %
                         half;
    if (apart < 0) apart += half;

    const auto both_angles = [&] {
        return " Açılar: " +
               core::angle_text(
                   core::turns_from_udeg(core::udeg_from_angle(angle_a, convention_a.unit)),
                   convention_a.unit) +
               " ve " +
               core::angle_text(
                   core::turns_from_udeg(core::udeg_from_angle(angle_b, convention_b.unit)),
                   convention_b.unit) +
               ".";
    };

    if (apart == 0)
        return err(ErrorCode::InvalidArgument,
                   "kes(): iki doğrultu paralel, kesişmiyorlar." + both_angles());

    const Point2 a2 = a + core::polar_offset(kDirectionRay, angle_a, convention_a);
    const Point2 b2 = b + core::polar_offset(kDirectionRay, angle_b, convention_b);

    Point2 out;
    double t = 0.0;
    double u = 0.0;
    if (!core::line_intersection(a, a2, b, b2, out, t, u))
        return err(ErrorCode::InvalidArgument, "kes(): iki doğrultu kesişmiyor." + both_angles());
    return out;
}

core::Result<Point2> distance_crossing(Point2 a, double r1_m, Point2 b, double r2_m, Side side)
{
    if (r1_m < 0.0 || r2_m < 0.0)
        return err(
            ErrorCode::InvalidArgument,
            "kes(): yarıçap negatif olamaz. Girilen: " + metres_text(core::mm_from_metres(r1_m)) +
                " m ve " + metres_text(core::mm_from_metres(r2_m)) + " m.");

    const Mm r1 = core::mm_from_metres(r1_m);
    const Mm r2 = core::mm_from_metres(r2_m);

    Point2 left;
    Point2 right;
    const core::CircleMeet meet = core::circle_intersection(a, r1, b, r2, left, right);

    // Every refusal names both radii AND the distance between the centres, so
    // the user can see at a glance which measurement is the wrong one (R19).
    const auto figures = [&] {
        return " Yarıçaplar " + metres_text(r1) + " m ve " + metres_text(r2) +
               " m, merkezler arası " + metres_text(core::segment_length(a, b)) + " m.";
    };

    const char* reason = "kes(): çemberler kesişmiyor.";
    switch (meet) {
    case core::CircleMeet::Two:
    case core::CircleMeet::Tangent: return side == Side::Left ? left : right;
    case core::CircleMeet::TooFar: reason = "kes(): çemberler birbirine ulaşmıyor."; break;
    case core::CircleMeet::Nested: reason = "kes(): bir çember ötekinin tamamen içinde."; break;
    case core::CircleMeet::SameCentre:
        reason = "kes(): iki merkez aynı nokta, kesişim tek bir nokta değil.";
        break;
    }
    return err(ErrorCode::InvalidArgument, reason + figures());
}

core::Result<Point2> line_crossing(Point2 a, Point2 b, Point2 c, Point2 d)
{
    if (a == b || c == d)
        return err(ErrorCode::InvalidArgument,
                   "kes(): bir doğrunun iki noktası aynı, doğrultu tanımsız.");

    Point2 out;
    double t = 0.0;
    double u = 0.0;
    if (!core::line_intersection(a, b, c, d, out, t, u))
        return err(ErrorCode::InvalidArgument, "kes(): iki doğru paralel, kesişmiyorlar.");
    return out;
}

core::Result<Point2> along_ratio(Point2 a, Point2 b, double t)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    return Point2{a.x + core::mm_round(t * dx), a.y + core::mm_round(t * dy)};
}

core::Result<Point2> along_distance(Point2 a, Point2 b, double distance_m)
{
    const double dx  = static_cast<double>(b.x - a.x);
    const double dy  = static_cast<double>(b.y - a.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0.0)
        return err(ErrorCode::InvalidArgument,
                   "ara(): A ve B aynı nokta, üzerinde mesafe ölçülecek doğru yok.");

    // THE RATIO, not a second normalisation. A metre reading becomes the
    // fraction of the way along, and the one rounding is `along_ratio`'s — so
    // `ara(A,B,0.5)` and `ara(A,B,<half the length> m)` land on the same
    // millimetre rather than within one of each other.
    return along_ratio(a, b, distance_m * static_cast<double>(core::kMmPerMetre) / len);
}

core::Result<Point2> beyond(Point2 a, Point2 b, double distance_m)
{
    const double dx  = static_cast<double>(b.x - a.x);
    const double dy  = static_cast<double>(b.y - a.y);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0.0)
        return err(ErrorCode::InvalidArgument,
                   "uzanti(): A ve B aynı nokta, uzatılacak bir doğrultu yok.");

    const double d = distance_m * static_cast<double>(core::kMmPerMetre);
    return Point2{b.x + core::mm_round(d * dx / len), b.y + core::mm_round(d * dy / len)};
}

} // namespace kentos::command
