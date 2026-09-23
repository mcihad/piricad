// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: BUDA and UZAT on lines, arcs and circles (TODOS C-04).
//
// THE PIECE YOU CLICK IS THE PIECE THAT GOES. A trim finds every place the
// clicked object meets the cutting edges, and removes the stretch between the
// two either side of the click — the middle of a line between two roads, the
// arc of a circle between two chords, the end of a line past its boundary.
// What stays keeps its kind: an arc trimmed is an arc, a circle trimmed is the
// arc that is left, and a line trimmed to an arc stops ON the arc. With `keep`
// the click names the piece that STAYS, and everything past its two cuts goes.
//
// An extension carries the end nearer the click along its own direction — a
// straight end along its line, an arc's end round its circle — to the first
// cutting edge it reaches.
//
// A FENCE IS MANY CLICKS AT ONCE. Every piece a drawn fence crosses is named,
// on every object it crosses, and the plan is made from the drawing as it is
// before any of it changes — so a fence that crosses two pieces of one line
// takes both, and the preview of the whole fence is the edit the fence makes.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. `BUDA` and `UZAT` edit with these
// functions, and the canvas draws the piece under the cursor — or every piece
// under the fence — with the same calls and the same cutting edges
// (`cutting_edges`), so what the preview marks is what the click does.
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// How a BUDA or UZAT run cuts — its edges and its rules — and the payload its
/// prompts carry to the canvas (`command::RubberShape::Trim`, `TrimFence`).
struct TrimGuide
{
    bool extend{false};             ///< UZAT rather than BUDA
    bool every{false};              ///< every visible object cuts (`cutting_edges`)
    std::vector<std::int64_t> keys; ///< otherwise these, by persistent key
    bool keep{false};               ///< the piece named stays and the rest goes (`tut`)
    bool carry{false};              ///< the edges run on along their own paths (`uzanti`)
};

/// Every place `target` is cut by `edges`, in order along it, one per point:
/// two edges meeting it where they meet each other are one cut, and on a closed
/// target a cut at the seam is counted once.
std::vector<PathCrossing> cuts_of(const CurvePath& target, std::span<const CurvePath> edges);

/// What a trim does to one object.
struct CurveTrim
{
    std::vector<CurvePath> removed; ///< what goes, piece by piece
    std::vector<CurvePath> kept;    ///< what stays: one piece, or more when a middle goes
    std::vector<PathCrossing> cuts; ///< every cut — the candidates a preview marks
};

/// The pieces of `target` between `cuts` that hold one of `marks` go — or, with
/// `keep`, stay while every other goes. Adjacent pieces with one fate become one
/// path. A mark ON a cut names no piece and is passed over. A closed target
/// needs two cuts; refused, with the sentence a user reads, when nothing would
/// go or nothing would be left.
Result<CurveTrim> cut_pieces(const CurvePath& target, std::vector<PathCrossing> cuts,
                             std::span<const PathPlace> marks, bool keep);

/// BUDA: `target` cut at its meets with `edges`, and the piece holding the
/// place nearest `pick` goes — or stays, with `keep`. A click exactly on a cut
/// names no piece and is refused.
Result<CurveTrim> trim_curve(const CurvePath& target, std::span<const CurvePath> edges, Point2 pick,
                             bool keep = false);

/// What an extension does to one object.
struct CurveExtension
{
    CurvePath extended; ///< the object as it will be
    CurvePath added;    ///< the reach it gains, from the old end to the edge
};

/// UZAT: the end of the open `target` nearer `pick` carried along its own
/// direction to the first of `edges` it reaches.
Result<CurveExtension> extend_curve(const CurvePath& target, std::span<const CurvePath> edges,
                                    Point2 pick);

/// The cutting edges a run uses for `target`: with `run.every`, every other
/// visible object whose box meets the target's reach (`BUDA` with nothing
/// selected, as every CAD's quick trim reads it); otherwise the objects
/// `run.keys` name. The target itself never cuts itself, and an object that is
/// not a path is passed over. With `run.carry` an open edge runs on past both
/// its ends — a straight end along its line, an arc's round its circle — so a
/// boundary that stops short still cuts, the way AutoCAD's implied edges do.
std::vector<CurvePath> cutting_edges(const Document& doc, EntityId target, const TrimGuide& run);

/// What a fence does to one object it crosses.
struct FenceEdit
{
    EntityId target{kNoEntity}; ///< the object
    CurveTrim cut;              ///< BUDA: what goes and what stays
    CurveExtension reach;       ///< UZAT: the object extended, and what it gains
};

/// What a fence does to the drawing.
struct FencePlan
{
    std::vector<FenceEdit> edits; ///< one per object the fence edits, in drawing order
    std::size_t passed_over{0};   ///< objects it crossed that it could not edit
};

/// Every object the open polyline `fence` crosses, edited at each crossing as a
/// click there would edit it — BUDA takes (or, with `run.keep`, keeps) every
/// piece the fence crosses; UZAT carries each end the fence crosses near. The
/// plan is made from the drawing before any of it changes. An object nothing
/// cuts, one BUDA leaves alone (an area, an ellipse, a locked layer's) and one
/// no end of which reaches an edge is counted in `passed_over`, not refused.
FencePlan plan_fence(const Document& doc, std::span<const Point2> fence, const TrimGuide& run);

/// The guide as bytes.
std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<TrimGuide> decode_trim_guide(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
