// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the numerical contract, in one place (TODOS F-03).
//
// FIVE DIFFERENT QUANTITIES, and each has one home. They used to be literals
// scattered through twenty files, and that is how a screen tolerance ended up
// deciding a typed coordinate and a picture's chord count ended up in a
// GeoPackage.
//
//   1. STORAGE RESOLUTION — one millimetre. `Mm` (units.hpp); every stored
//      coordinate is exact at it, and every transient double is rounded to it
//      once, by `mm_round`. Not a tolerance: nothing is "within" it.
//
//   2. COMPUTATION — how near two COMPUTED things must be to be one thing. The
//      constants below. Each follows from the storage resolution, and none is
//      a setting: they are properties of the arithmetic, not choices.
//
//   3. SCREEN — pixels: `core.yakalama.tolerans` and `core.secim.tolerans`, per
//      user and machine (App scope). They act on a point a HAND aimed and only
//      on that (command.md R9a); a stated coordinate is never moved by one.
//
//   4. TOPOLOGY — `core.topoloji.dugum_toleransi`, per project, 10 mm by
//      default: two corners closer than it are one node for SINIR, ALANÜRET,
//      TEMİZLE, TOPOLOJİ, BİRLEŞTİR and ALANAÇEVİR. A setting, because it is the
//      engineer's call about the data, and it changes a signed result.
//
//   5. EXPORT — `core.aktarim.egri_sapmasi`, per project, 1 mm by default: how
//      far a chord written for a curve may stand off it in a file that cannot
//      hold a curve (stroke.hpp). A setting, for the same reason.
//
// And one precondition that is not a tolerance at all: the document's
// coordinate system COUNTS METRES (crs.hpp `CrsUnit`, model.md R36a).
#pragma once

namespace kentos::core {

/// How far rounding moves a point at most: half a millimetre on each axis, so
/// √2/2 mm. A stored vertex is within this of the point it stands for.
inline constexpr double kRoundingReachMm = 0.70710678118654752440;

/// A computed point is ON a curve within half a millimetre — the distance
/// inside which two coordinates round to the same `Mm`. A line touches a circle,
/// a curve meets a curve, a point lies on an edge.
inline constexpr double kOnCurveMm = 0.5;

/// Two computed points closer than one millimetre — one storage unit — are one
/// point: two crossings found from two sides, a click on a cut.
inline constexpr double kSamePointMm = 1.0;

/// A vertex read back from a file lies on the circle fitted through it within
/// 1,5 mm: each rounded vertex carries up to `kRoundingReachMm` of noise, and
/// the fitted centre a fraction more. A shape that merely resembles a circle
/// misses by centimetres.
inline constexpr double kRoundedFitMm = 1.5;

/// A stored arc's centre and its two ends agree within 2 mm: each of the three
/// is rounded to the millimetre, so their distances can disagree by twice
/// `kRoundingReachMm`, and the rest is slack for a centre recovered from a
/// DXF bulge.
inline constexpr double kStoredArcMm = 2.0;

} // namespace kentos::core
