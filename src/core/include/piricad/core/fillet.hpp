// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: rounding or cutting the corner BETWEEN two objects
// (TODOS C-06).
//
// `corner.hpp` cuts a corner a polyline already has. This file makes the corner
// two separate objects meet at: a kerb line and a road arc, two walls drawn as
// two lines, the edge of a parcel and a curve. Line–line, line–arc and arc–arc,
// by one construction.
//
// THE CENTRE IS WHERE TWO LOCI MEET. A circle of radius r tangent to a line has
// its centre on one of the line's two parallels at r; tangent to a circle of
// radius R, on one of the circles R + r and |R − r| about the same centre. The
// candidates are those loci's crossings — up to four for two lines, up to eight
// with an arc — and every one of them is a real tangent circle.
//
// THE PICKS SAY WHICH, AND THE WRONG ONE IS NEVER TAKEN. Where each object was
// picked names the part of it that stays, and the fillet lies in the corner those
// two parts make: its centre on the second pick's side of the first object and
// on the first pick's side of the second, as every CAD reads a fillet. A radius
// whose tangent point falls outside the part that stays does not fit, and is
// refused rather than drawn on the far side.
//
// NOTHING HERE IS A SECOND COPY. The command, its preview and a test all ask
// `fillet_pair` / `chamfer_pair`, so what the ghost shows is what the click
// makes (C-06: "önizleme ve çıktı aynıdır").
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// How two objects meet at the corner made between them.
struct PairCorner
{
    CurvePath a; ///< the first object trimmed or extended to the corner, its picked part kept
    CurvePath b; ///< the same for the second
    bool has_link{false}; ///< a fillet arc or a chamfer edge joins them; false for a sharp corner
    PathPiece link;       ///< that arc or edge, from where `a` now ends to where `b` does
    Point2 on_a{};        ///< where `a` now meets the corner
    Point2 on_b{};        ///< where `b` does
    Point2 centre{};      ///< a fillet's centre
    Mm radius{0};         ///< a fillet's radius; 0 for a sharp corner and a chamfer
};

/// The fillet of `radius` between the piece of `a` nearest `pick_a` and the
/// piece of `b` nearest `pick_b`, each trimmed or extended to its tangent point
/// with the picked part kept. A radius of ZERO meets the two at their crossing
/// nearest the picks — a sharp corner — with no arc. Refused, with the sentence
/// a user reads, when no circle of that radius touches both on the picked
/// sides, when it does not fit on the picked part, and when a tangent point
/// would have to be reached by extending a piece in the middle of a polyline.
Result<PairCorner> fillet_pair(const CurvePath& a, Point2 pick_a, const CurvePath& b, Point2 pick_b,
                               Mm radius);

/// The chamfer between two STRAIGHT pieces: each cut back `distance_a` and
/// `distance_b` from where their lines cross, toward the picked part, and joined
/// by a straight edge. Refused for an arc, for parallel lines, and when a
/// distance runs past the picked part.
Result<PairCorner> chamfer_pair(const CurvePath& a, Point2 pick_a, const CurvePath& b,
                                Point2 pick_b, Mm distance_a, Mm distance_b);

/// The payload a two-object corner preview carries
/// (`command::RubberShape::PairCorner`): both objects by persistent key, both
/// picks, and which cut — so the canvas can call `fillet_pair` or
/// `chamfer_pair` with the size the cursor shows.
struct PairCornerGuide
{
    std::int64_t key_a{0}; ///< the first object
    std::int64_t key_b{0}; ///< the second
    Point2 pick_a{};       ///< where the first was picked
    Point2 pick_b{};       ///< where the second was
    bool fillet{true};     ///< round rather than straight
    bool trim{true};       ///< the objects are cut back to the corner (`budama`)
};

/// The guide as bytes.
std::vector<std::uint8_t> encode_pair_corner_guide(const PairCornerGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<PairCornerGuide> decode_pair_corner_guide(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
