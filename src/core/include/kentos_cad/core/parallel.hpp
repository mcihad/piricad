// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the PARALLEL of one object, whatever its kind (TODOS C-03).
//
// `offset.hpp` knows rings; this knows what a ring MEANS. An open line's
// parallel is an open line beside it, on the side asked for. A face's is a face,
// with its holes still holes. A circle's is a circle with another radius, an
// arc's a concentric arc. An ellipse, a spline and a polyline with arc edges have
// no parallel of their own kind — the parallel of an ellipse is not an ellipse —
// so theirs is the exact parallel of the curve AS DRAWN, and it says so, with a
// measured figure for how far the drawing is from the curve.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. OFSET computes its result here, and
// the canvas draws the parallel under the cursor by calling the same function
// with the side the cursor is on — so what the preview shows is what the click
// makes, not a second computation that agrees on the easy cases (Article 1.2).
//
// THE TWO-SIDED BAND IS NOT HERE. That is a BUFFER, a GIS operation that makes a
// face (`core::buffer`, the TAMPON tool); a parallel of a line is a line.
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Which side of an object its parallel goes to.
enum class ParallelSide : std::uint8_t {
    Left,    ///< an open run's left, looking along the way it was drawn
    Right,   ///< its right
    Outside, ///< a closed shape grown: a face, a circle, a closed curve; an arc away from its
             ///< centre
    Inside,  ///< a closed shape shrunk; an arc toward its centre
    Both,    ///< both sides at once: two parallels
};

/// The side a user names — `sol`, `sag`, `dis`, `ic`, `iki`, Turkish-folded, or
/// the English `left`, `right`, `outside`, `inside`, `both` — or nothing.
std::optional<ParallelSide> parallel_side_from_name(std::string_view word);

/// The Turkish word for a side, as `parallel_side_from_name` reads it.
std::string_view parallel_side_name(ParallelSide side) noexcept;

/// One parallel, in the shape it is drawn with.
struct ParallelPiece
{
    /// What the piece is.
    enum class Shape : std::uint8_t {
        Run,    ///< a line: `run`, open unless `closed`
        Face,   ///< a face: `faces`, holes and parts included
        Circle, ///< a circle: `centre`, `radius`
        Arc,    ///< an arc: `centre`, `radius`, counter-clockwise from `start` to `end`
    };

    Shape shape{Shape::Run};               ///< which of the four
    ParallelSide side{ParallelSide::Left}; ///< which side of its source it is on
    std::vector<Point2> run;               ///< Run: the vertices
    bool closed{false};                    ///< Run: it met itself and closes
    std::vector<Polygon> faces;            ///< Face: one face, as its polygons
    Point2 centre{};                       ///< Circle, Arc: the centre
    Mm radius{0};                          ///< Circle, Arc: the radius
    Point2 start{};                        ///< Arc: where it starts
    Point2 end{};                          ///< Arc: where it ends
};

/// The parallel of one object.
struct Parallel
{
    /// The pieces, source side first. EMPTY when nothing survives on the side
    /// asked for — a face shrunk past half its width, a circle past its radius —
    /// which is an answer the caller must say out loud, not a failure here.
    std::vector<ParallelPiece> pieces;

    /// The kind has no parallel of its own kind — an ellipse, a spline, a
    /// polyline with arc edges — so the pieces are the exact parallel of the
    /// curve as drawn, and the drawn curve is not the curve.
    bool approximate{false};

    /// When `approximate`: how far the drawn curve is from the true one, at
    /// most, measured chord by chord against the curve's own definition. The
    /// parallel departs from the true parallel by the same order — more on the
    /// outside of a tight bend, where the offset stretches every chord.
    Mm deviation{0};

    /// When `approximate`: how many vertices the drawing of the source has.
    std::size_t drawn_vertices{0};
};

/// Why `e` has no parallel, as the sentence a user reads — or nothing when it has
/// one. A point, a text, a dimension, a leader, a hatch and a block reference
/// have none: a hatch's boundary does, and a point's neighbourhood is a BUFFER.
std::optional<std::string> parallel_refusal(const Document& doc, EntityId e);

/// The side of `e` that `p` is on: `Left`/`Right` of an open run, `Outside`/
/// `Inside` of a closed shape (and of an arc, by its centre). Refused for a
/// point ON an open run, whose side is not a side at all.
Result<ParallelSide> parallel_side_at(const Document& doc, EntityId e, Point2 p);

/// The parallel of `e` at `distance` millimetres (positive) to `side`.
///
/// `Left`/`Right` are for an open run and `Outside`/`Inside` for a closed shape;
/// the other pair is refused by name rather than mapped, because a closed
/// parcel's "left" is a winding nobody drew on purpose. An arc takes all four:
/// it is drawn counter-clockwise, so its left is its inside. `Both` gives the
/// two sides the object has.
Result<Parallel> entity_parallel(const Document& doc, EntityId e, Mm distance, ParallelSide side,
                                 JoinStyle join = JoinStyle::Miter);

/// The payload a PARALLEL preview carries (`command::RubberShape::Parallel`):
/// the objects, the distance and the corner, so the canvas can call
/// `entity_parallel` for the side the cursor is on. Versioned like every other
/// preview payload.
struct ParallelPreview
{
    std::vector<std::int64_t> keys;   ///< the objects, by persistent key
    Mm distance{0};                   ///< millimetres, positive
    JoinStyle join{JoinStyle::Miter}; ///< the corner
};

/// The preview as bytes.
std::vector<std::uint8_t> encode_parallel_preview(const ParallelPreview& preview);

/// The preview back, refused when the bytes are not what the encoder writes.
Result<ParallelPreview> decode_parallel_preview(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
