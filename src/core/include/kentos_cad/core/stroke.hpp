// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a curve flattened to a STATED error, for a file that cannot
// hold a curve (TODOS F-03).
//
// TWO DIFFERENT QUESTIONS, TWO DIFFERENT ANSWERS. The picture draws every curve
// at a fixed density — 128 chords to a circle, 64 to a turn of an arc — because
// a frame has a budget and a pixel is the error that matters there. A GeoPackage
// or a PostGIS table receives no pixels: it receives coordinates, and a 300 m
// road curve drawn at the picture's density is up to nine centimetres off its
// own arc. This header is the other answer: a chord tolerance in millimetres,
// chosen by the project (`core.aktarim.egri_sapmasi`), honoured for every curve
// kind, and reported with the export.
//
// Deterministic like every other curve routine in core (§7.3): directions are
// `atan2_udeg`, points are placed by `sin_cos_udeg` at whole micro-degrees, the
// step bound is one `sqrt` (correctly rounded by IEEE-754), and every vertex is
// rounded once by `mm_round`. The stored ends of an arc are written as stored, so
// an arc that meets a line meets it exactly in the file too.
#pragma once

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <vector>

namespace kentos::core {

/// One run of a stroked shape: its vertices, without a repeated closing vertex,
/// and the role its ring plays — `Exterior` for a closed curve (a circle, a
/// closed spline, a closed arc polyline), `Open` for one that ends.
struct StrokedRun
{
    std::vector<Point2> points;    ///< the vertices in order
    RingRole role{RingRole::Open}; ///< `Exterior` when the run closes on itself
};

/// A curve's shape as straight runs within a chord tolerance.
struct Stroked
{
    std::vector<StrokedRun> runs; ///< in order; one for every curve kind today

    /// The largest distance a chord was allowed to stand off its curve, in
    /// millimetres — the tolerance asked for, or more when a curve needed more
    /// vertices than `kStrokeMaxVertices` allows (then this says how much more).
    /// Rounding of the vertices to the millimetre comes on top, at most √2/2 mm.
    double deviation{0.0};
};

/// The most vertices one stroked curve may have. A 1 mm tolerance on a
/// kilometre-radius circle needs about 2 200; this is two orders past any curve
/// a map carries, and exists so a hostile file cannot ask for a billion.
inline constexpr std::int64_t kStrokeMaxVertices = 1 << 18;

/// The smallest chord tolerance `stroke_curve` honours: the storage unit. Below
/// it every vertex's own rounding is the larger error, and asking for less
/// would only multiply vertices that say nothing more.
inline constexpr Mm kStrokeMinChord = 1;

/// `slot`'s true shape as straight runs, every chord within `chord` millimetres
/// of the curve it stands for (and within `kStrokeMinChord` whatever is asked).
///
/// For a circle, an arc, an ellipse and an elliptic arc, an arc polyline and a
/// spline: the shape the definition describes, not the picture's fixed density.
/// Returns false for any other kind — a polyline's and a point's stored rings ARE
/// their shape, and a hatch, a block reference or a dimension is written by what
/// it is, not by this — and when the slot's definition does not decode.
bool stroke_curve(KindId kind, const RingGeometry& geom, std::uint32_t slot, Mm chord,
                  Stroked& out);

/// Whether `kind` is one `stroke_curve` answers for.
bool strokes_as_curve(KindId kind) noexcept;

} // namespace kentos::core
