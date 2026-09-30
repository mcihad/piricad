// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: cutting a corner off a run, straight (PAH) or round (YUVARLA).
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. PAH and YUVARLA compute their
// result here, and the canvas draws the cut under the cursor by calling the same
// function with the cursor's distance from the corner — so what the preview
// shows is what the typed number or the click makes, not a second computation
// that agrees on the easy cases (Article 1.2).
//
// EXACT WHERE IT CAN BE. The tangent points come from unit vectors built with
// `sqrt`, which IEEE-754 rounds correctly, and never from `atan2` and a
// trigonometric call, which are not required to agree between platforms (§7.3).
// The half-angle identities below exist for that reason and not to save a call.
//
// A CORNER ROUNDED IS AN ARC, EVERYWHERE (TODOS O-2). The corner is cut on the
// object's PATH — its straight and arc pieces — and the result is written back
// into the SAME object: one line whose corner became an arc is one arc
// polyline, a parcel rounded at a corner is the same parcel with an arc edge
// (model.md R9b). The arc keeps its centre and its radius; nothing is drawn
// into the ring as chords, so nothing strays from the arc and the area is the
// arc's own. Between two straight pieces the tangent points are the half-angle
// construction below; where a piece is itself an arc — a corner of a parcel
// rounded before, of a DXF polyline with bulges — the fillet is `fillet_pair`'s
// circle tangent to both (fillet.hpp), the construction the two-object mode
// uses, so there is one answer to "the arc of this radius in this corner".
#pragma once

#include "piricad/core/curve_path.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace piricad::core {

/// The vertex of `run` nearest `probe`, when it IS a corner — one with an edge
/// on each side. Nothing when the nearest vertex is an end of an open run, which
/// has one edge and nothing to cut across, and nothing when the run has fewer
/// than three vertices.
std::optional<std::size_t> nearest_corner(std::span<const Point2> run, bool closed, Point2 probe);

/// What cutting the corners of a path makes (`cut_path_corner`,
/// `cut_every_path_corner`).
struct PathCorners
{
    /// The whole path with its corners cut, every other piece as it was and
    /// closed if it was: a fillet an arc piece, a chamfer a straight one.
    CurvePath path;
    std::size_t cut{0};     ///< corners cut
    std::size_t skipped{0}; ///< corners the size does not fit, straight or smooth ones, and a
                            ///< chamfer's beside an arc
    Point2 cut_a{};         ///< one corner: the tangent point toward the previous vertex
    Point2 cut_b{};         ///< one corner: the tangent point toward the next vertex
    Point2 centre{};        ///< one corner rounded: the arc's centre
    Mm radius{0};           ///< one corner rounded: the arc's radius, the size asked for
};

/// Cuts the corner at vertex `at` of `path` — the vertex numbering the object's
/// ring holds: vertex k is where piece k begins, and for a closed path vertex 0
/// is also where the last piece ends. `size` is the distance cut back along
/// each edge for a chamfer and the arc's radius for a fillet, in millimetres.
///
/// Refused, with the sentence a user reads, for an end of an open path, for a
/// straight corner and a smooth one (a piece that runs on tangent into the
/// next), when a tangent point would pass a neighbouring vertex, and for a
/// chamfer beside an arc piece. A tangent point exactly ON the neighbouring
/// vertex is not refused: the edge between the two is used up, as a 4 m edge
/// between two 2 m fillets is.
Result<PathCorners> cut_path_corner(const CurvePath& path, std::size_t at, Mm size, bool fillet);

/// Cuts every corner of `path` at `size`, the way a chain of corners is cut by
/// hand one after another: each corner is judged against its edges as the
/// corners before it left them, and a corner the size does not fit — its
/// tangent point would pass the one the corner before it put on their shared
/// edge — is passed over and counted rather than refusing the rest. The path
/// comes back whole: an open line rounded at every corner is ONE path with
/// real arcs, a parcel stays one closed path.
PathCorners cut_every_path_corner(const CurvePath& path, Mm size, bool fillet);

/// The payload a corner preview carries (`command::RubberShape::Corner`): the
/// object by its persistent key, which vertex, and which cut — so the canvas can
/// call `cut_path_corner` with the cursor's distance from the corner.
struct CornerPreview
{
    std::int64_t key{0};            ///< the object, by persistent key
    std::uint32_t at{0};            ///< the vertex index
    bool fillet{false};             ///< round rather than straight
    bool every{false};              ///< every corner at once (`cut_every_path_corner`)
    std::vector<std::int64_t> also; ///< with `every`: more objects cut the same way, by key
};

/// The preview as bytes.
std::vector<std::uint8_t> encode_corner_preview(const CornerPreview& preview);

/// The preview back, refused when the bytes are not what the encoder writes.
Result<CornerPreview> decode_corner_preview(std::span<const std::uint8_t> bytes);

} // namespace piricad::core
