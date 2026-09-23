// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: BUDA and UZAT on lines, arcs and circles (TODOS C-04).
//
// THE PIECE YOU CLICK IS THE PIECE THAT GOES. A trim finds every place the
// clicked object meets the cutting edges, and removes the stretch between the
// two either side of the click — the middle of a line between two roads, the
// arc of a circle between two chords, the end of a line past its boundary.
// What stays keeps its kind: an arc trimmed is an arc, a circle trimmed is the
// arc that is left, and a line trimmed to an arc stops ON the arc.
//
// An extension carries the end nearer the click along its own direction — a
// straight end along its line, an arc's end round its circle — to the first
// cutting edge it reaches.
//
// ONE ANSWER FOR THE COMMAND AND THE PREVIEW. `BUDA` and `UZAT` edit with these
// functions, and the canvas draws the piece under the cursor with the same
// calls and the same cutting edges (`cutting_edges`), so what the preview marks
// is what the click does.
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::core {

/// What a trim does to one object.
struct CurveTrim
{
    CurvePath removed;           ///< the piece the click names, which goes
    std::vector<CurvePath> kept; ///< what stays: one piece, or two when a middle goes
};

/// BUDA: the piece of `target` holding the place nearest `pick`, between the
/// meets with `edges` either side of it — or the target's own end where there
/// is none on that side. A closed target (a circle, a closed ring) needs a meet
/// on both sides, and what stays of it is one open piece.
Result<CurveTrim> trim_curve(const CurvePath& target, std::span<const CurvePath> edges,
                             Point2 pick);

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

/// The cutting edges a run uses for `target`: with `every` set, every other
/// visible object whose box meets the target's reach (`BUDA` with nothing
/// selected, as every CAD's quick trim reads it); otherwise the objects `keys`
/// name. The target itself never cuts itself, and an object that is not a path
/// is passed over.
std::vector<CurvePath> cutting_edges(const Document& doc, EntityId target, bool every,
                                     std::span<const std::int64_t> keys);

/// The payload BUDA's and UZAT's pick prompt carries (`command::RubberShape::Trim`).
struct TrimGuide
{
    bool extend{false};             ///< UZAT rather than BUDA
    bool every{false};              ///< every visible object cuts (`cutting_edges`)
    std::vector<std::int64_t> keys; ///< otherwise these, by persistent key
};

/// The guide as bytes.
std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide);

/// The guide back, refused when the bytes are not what the encoder writes.
Result<TrimGuide> decode_trim_guide(std::span<const std::uint8_t> bytes);

} // namespace kentos::core
