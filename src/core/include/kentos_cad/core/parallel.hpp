// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the PARALLEL of one object, whatever its kind (TODOS C-03).
//
// `offset.hpp` knows rings; this knows what a ring MEANS. An open line's
// parallel is an open line beside it, on the side asked for. A face's is a face,
// with its holes still holes. A circle's is a circle with another radius, an
// arc's a concentric arc. A polyline with arc edges has the parallel it really
// has — its arcs concentric arcs, its segments offset segments — from the
// geometry kernel, and so has a ROUND corner asked of any line or face: a true
// arc about the corner, not a fan of short edges (TODOS O-4, CLAUDE.md 2.11). An
// ellipse and a spline have no parallel of their own kind — the parallel of an
// ellipse is not an ellipse — so theirs is the exact parallel of the curve AS
// DRAWN, and it says so, with a measured figure for how far the drawing is from
// the curve.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. OFSET computes its result here, and
// the canvas draws the parallel under the cursor by calling the same function
// with the side the cursor is on — so what the preview shows is what the click
// makes, not a second computation that agrees on the easy cases (Article 1.2).
//
// THE TWO-SIDED BAND IS NOT HERE. That is a BUFFER, a GIS operation that makes a
// face (`core::buffer`, the TAMPON tool); a parallel of a line is a line.
//
// A DOUBLE LINE IS (`double_line`, TODOS N-11): an axis and the two parallels
// that run beside it, drawn as the axis is drawn. It is not a second parallel
// computation — each side is `run_parallel`, which is the body `entity_parallel`
// runs for an open line, handed the points instead of a slot — and the canvas
// draws the same call under the cursor, so the preview is the result.
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/result.hpp"

#include <array>
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
        Path,   ///< segments and true arcs: `path`, open or closed — the kernel's answer
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
    CurvePath path;                        ///< Path: the pieces, arcs as arcs
};

/// The parallel of one object.
struct Parallel
{
    /// The pieces, source side first. EMPTY when nothing survives on the side
    /// asked for — a face shrunk past half its width, a circle past its radius —
    /// which is an answer the caller must say out loud, not a failure here.
    std::vector<ParallelPiece> pieces;

    /// The kind has no parallel of its own kind — an ellipse, a spline; an
    /// arc polyline too in a build without the kernel — so the pieces are the
    /// exact parallel of the curve as drawn, and the drawn curve is not the
    /// curve.
    bool approximate{false};

    /// A ROUND corner was drawn as short edges rather than as an arc: a face
    /// with holes, whose bent edges one object could not hold (an arc polyline
    /// has one ring, model.md R9b). Said, like `approximate`, by the caller.
    bool round_as_chords{false};

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

/// How far the drawing of `e` departs from the curve it stands for, measured
/// chord by chord against the curve's own definition: an ellipse, a spline or
/// an arc-polyline drawn as chords. Zero for a kind whose drawing is its
/// definition. The figure a result built from that drawing reports.
Mm drawn_deviation(const Document& doc, EntityId e);

/// Whether `e`'s sides are `Outside`/`Inside` — a closed shape, a circle, an
/// arc — rather than an open run's `Left`/`Right`. What a caller that was given
/// a signed distance and no side needs to know: plus grows a closed shape and
/// minus shrinks it, and an open run has no such pair. False for an entity that
/// has no parallel at all (`parallel_refusal`).
bool parallel_encloses(const Document& doc, EntityId e);

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
///
/// `JoinStyle::Link` keeps the length of every straight edge and is for lines
/// and faces of straight edges only: a shape with an arc or a drawn curve in it
/// is refused with a sentence, and a circle or an arc, which have no corner,
/// are offset as always.
Result<Parallel> entity_parallel(const Document& doc, EntityId e, Mm distance, ParallelSide side,
                                 JoinStyle join = JoinStyle::Miter);

/// The parallel of an OPEN RUN that is not an object — the axis a command is
/// still asking the points of — `distance` millimetres (positive) to its `Left`
/// or `Right`, or `Both`.
///
/// NOT A SECOND COMPUTATION: this builds the same shape `entity_parallel` reads
/// from an open polyline and runs the same body over it, so a run and the object
/// it becomes are moved sideways by one function — the straight and bevelled
/// corners from Clipper2, a round corner as a true arc from the kernel
/// (CLAUDE.md 2.11). The run keeps its direction of travel: the left of
/// `(0,0)→(10,0)` is `y > 0`. Refused for a run of fewer than two distinct
/// points, and for `Outside`/`Inside`, which a run does not have.
Result<Parallel> run_parallel(std::span<const Point2> run, Mm distance, ParallelSide side,
                              JoinStyle join = JoinStyle::Miter);

/// What a DOUBLE LINE is asked for (`ÇİFTÇİZGİ`, netcad_plan.md N-11).
struct DoubleLineSpec
{
    Mm left{0};  ///< how far the left parallel runs from the axis, millimetres; zero draws none
    Mm right{0}; ///< the same for the right of the axis's direction of travel
    JoinStyle join{JoinStyle::Miter}; ///< how each parallel turns a corner it moves away from
    bool close_ends{false};           ///< a straight cap across each end of the axis
};

/// The pieces a double line is made of, in the shapes `ParallelPiece` draws
/// them with: a line or, where a round corner was asked for, a line with true
/// arcs. The axis itself is the caller's own points and is not repeated here.
struct DoubleLine
{
    std::vector<ParallelPiece> left;  ///< the left parallel, source side first; empty for none
    std::vector<ParallelPiece> right; ///< the right parallel; empty for none

    /// The two caps of a closed-ended double line — the one across the axis's
    /// first end, then the one across its last — each as the two points it joins,
    /// left one first. Empty unless `DoubleLineSpec::close_ends`.
    std::vector<std::array<Point2, 2>> caps;
};

/// Whether `spec` asks for a double line at all: refused, with the sentence a
/// user reads, for a negative width, for both widths zero, and for a width past
/// the drawing's coordinate limit. `double_line` refuses the same three; a
/// command asks first, so that it does not collect an axis for nothing.
Status check_double_line(const DoubleLineSpec& spec);

/// The double line of `axis`: the parallel `spec.left` to its left and the one
/// `spec.right` to its right, each side omitted when its width is zero.
///
/// A side may come back in several pieces — the inside of a turn tighter than
/// the width breaks in two — and that is said by the caller, but it makes the
/// ends of that side ambiguous, so a closed-ended double line is refused when a
/// side did. A cap joins the two sides' ends; with one side only, it joins that
/// side's end to the axis's own.
Result<DoubleLine> double_line(std::span<const Point2> axis, const DoubleLineSpec& spec);

/// The payload a DOUBLE LINE preview carries (`command::RubberShape::DoubleLine`):
/// the widths, the corner and whether the ends are closed, so the canvas can call
/// `double_line` for the axis so far plus the cursor. Versioned like every other
/// preview payload.
std::vector<std::uint8_t> encode_double_line_preview(const DoubleLineSpec& spec);

/// The preview back, refused when the bytes are not what the encoder writes.
Result<DoubleLineSpec> decode_double_line_preview(std::span<const std::uint8_t> bytes);

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
