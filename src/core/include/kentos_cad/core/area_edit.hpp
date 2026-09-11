// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: bringing a face to a WANTED AREA.
//
// A surveyor is handed a parcel and a figure: the tapu says 1 250,00 m² and the
// drawing says 1 248,71. The correction is not "draw it again" but "move this
// edge until the area is right", or "pull this corner", or "shrink the whole
// thing a hair on every side". This header is that arithmetic, deterministic and
// in millimetres: the shape an edge or a corner must take for the area to be
// exactly the figure, and the ghost the canvas shows while the hand looks for it.
//
// Three modes: UNIFORM offsets every edge by one distance (Clipper2's mitre
// offset, `offset_ring`) and bisects the distance to the area; EDGE slides one
// edge along its outward normal, its two neighbours stretching to meet it, and
// solves the resulting quadratic; VERTEX moves one corner, whose effect on the
// area is linear, so the places it may go form a line and the cursor is
// projected onto it. In every mode the original is untouched until the caller
// writes the result; nothing here reads or writes a document.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kentos::core {

/// Which part of the face moves.
enum class AreaEditMode : std::uint8_t {
    Uniform = 0, ///< every edge, by one distance
    Edge    = 1, ///< one edge, along its normal
    Vertex  = 2, ///< one corner
};

/// What an interactive edit is about: the face, the mode, the part, the wanted
/// area, and where the hand took hold. Travels as the point prompt's rubber
/// payload so the canvas draws the same ghost the command will commit.
struct AreaEditRequest
{
    std::int64_t key{0};                   ///< the face's persistent key
    AreaEditMode mode{AreaEditMode::Edge}; ///< what moves
    std::uint32_t index{0}; ///< the edge (from vertex `index`) or the vertex, 0-based
    Mm2 target{0};          ///< the wanted area, square millimetres
    Point2 grab{};          ///< where the hand took hold, for the rubber origin

    /// Member-wise equality.

    friend constexpr bool operator==(const AreaEditRequest&,
                                     const AreaEditRequest&) noexcept = default;
};

/// The request as bytes, and back. Little-endian, 37 bytes.
std::vector<std::uint8_t> encode_area_edit(const AreaEditRequest& request);
/// The request from bytes; nothing for a payload that is not one.
std::optional<AreaEditRequest> decode_area_edit(std::span<const std::uint8_t> bytes);

/// The ring with edge `edge` (from vertex `edge` to the next) shifted by
/// `offset` millimetres along the ring's OUTWARD normal — positive grows the
/// face — its two neighbouring edges stretched or trimmed to meet it. Empty
/// when a neighbour is parallel to the edge (nothing to meet) or the ring has
/// fewer than three vertices.
std::vector<Point2> area_edit_shift_edge(const std::vector<Point2>& ring, std::size_t edge,
                                         double offset);

/// The outward offset of edge `edge` that brings the ring's area to `target`,
/// or nothing when no such shift exists on the side the target lies.
std::optional<double> area_edit_edge_offset(const std::vector<Point2>& ring, std::size_t edge,
                                            Mm2 target);

/// The ring with vertex `index` at `to`.
std::vector<Point2> area_edit_move_vertex(const std::vector<Point2>& ring, std::size_t index,
                                          Point2 to);

/// Where vertex `index` must stand, nearest to `cursor`, for the area to be
/// `target`: the cursor projected onto the line of such places. Nothing when
/// the neighbours coincide.
std::optional<Point2> area_edit_vertex_for(const std::vector<Point2>& ring, std::size_t index,
                                           Point2 cursor, Mm2 target);

/// The ring offset on every edge by the one distance that brings its area to
/// `target` (mitre joins). An error when the face vanishes or splits before it
/// reaches the figure, or the figure is not positive.
Result<std::vector<Point2>> area_edit_uniform(const std::vector<Point2>& ring, Mm2 target);

/// What the canvas draws while the hand moves.
struct AreaGhost
{
    std::vector<Point2> points; ///< the face as the cursor has it, snapped or free
    Mm2 area{0};                ///< its area
    bool snapped{false};        ///< true when it sits exactly on the target
    Point2 commit{};            ///< the point to supply for the target — what Enter sends
};

/// The ghost for `request` with the cursor at `cursor`: the shape the cursor
/// implies, snapped to the exact target when within `snap` millimetres of it.
/// `commit` always holds the target's point, so Enter commits the figure even
/// when the hand is not on it. Empty `points` when the request cannot be met.
AreaGhost area_edit_ghost(const std::vector<Point2>& ring, const AreaEditRequest& request,
                          Point2 cursor, Mm snap);

/// The face `request` makes when the hand lets go at `at` (a click, or Enter's
/// commit point): an edge slides to the offset `at` lies at, a vertex goes to
/// `at`. Uniform mode ignores `at` and meets the target.
Result<std::vector<Point2>> area_edit_apply(const std::vector<Point2>& ring,
                                            const AreaEditRequest& request, Point2 at);

/// `1250,00 m²`: square metres to two decimals, from square millimetres, in
/// integers so the printed figure is the stored one rounded (Article 2.4).
std::string format_square_metres(Mm2 area);

} // namespace kentos::core
