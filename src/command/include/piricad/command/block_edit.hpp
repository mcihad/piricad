// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: moving objects into and out of a block definition.
//
// Three verbs cross the line between the sheet and a definition, and each
// crossing is said once, here, so the three agree (TODOS C-13):
//
//   * PATLAT takes a reference's members OUT as drawing objects, each carried by
//     the placement the reference draws it with (`place_member`);
//   * BLOK puts drawing objects IN as members (`copy_into_block`);
//   * BLOKDÜZENLE does both — out to be edited, back in when saved — and then
//     every reference that draws the definition, directly or through a block
//     inside a block, has its stored box brought up to date
//     (`refresh_references`).
#pragma once

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/transform.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace kentos::command {

/// What one member became when a reference's placement made it a drawing
/// object, and which of the reference's gifts it took.
struct PlacedMember
{
    core::EntityId piece{core::kNoEntity}; ///< the drawing object made
    bool onto_reference{false};            ///< set down on the reference's layer, off `0`
    bool reference_look{false};            ///< given the reference's look, or its layer's
    bool hidden{false};                    ///< the member was hidden, and so is the piece
    bool filled{false}; ///< a caption with fields, written with the reference's values
};

/// `member`, of the definition the reference in `reference` places, made again
/// as a drawing object IN ITS OWN KIND and carried by `x` (`core::block_placement`
/// for one copy of the reference's grid). It keeps what the drawing gave it: a
/// member on the drawing's `0` layer goes onto the reference's layer, a ByLayer
/// member there and a ByBlock member anywhere takes the look it was drawn in, a
/// hidden member stays hidden. The reason refused when its kind cannot hold `x`
/// — an arc polyline, or an inner block turned off a quarter, under a placement
/// that differs across and up.
core::Result<PlacedMember> place_member(Context& ctx, core::EntityId member, const core::Xform& x,
                                        core::EntityId reference);

/// A copy of drawing object `e`, moved by (`dx`, `dy`), made a member of
/// `block`: kind, rings, payload, style, caption, attribute cells and hidden
/// flag as they are. BLOK's copy (no move) and BLOKDÜZENLE's (the move that
/// takes an object opened at an insertion point back to the definition's own
/// place). The block refuses a copy that would make it contain itself.
core::Result<core::EntityId> copy_into_block(Context& ctx, core::EntityId e, core::BlockId block,
                                             core::Mm dx = 0, core::Mm dy = 0);

/// A drawing object that is member `member` moved by (`dx`, `dy`): its own
/// kind, layer, look, caption, cells and hidden flag, exactly as the definition
/// holds them — `copy_into_block` the other way. What BLOKDÜZENLE opens, so
/// what is edited is the definition itself and not the look one reference
/// gave it.
core::Result<core::EntityId> copy_out_of_block(Context& ctx, core::EntityId member, core::Mm dx,
                                               core::Mm dy);

/// Whether drawing object `e` moved by (`dx`, `dy`) is member `member` exactly:
/// the same kind, layer, look, rings, payload, caption, attribute cells and
/// hidden flag. How BLOKDÜZENLE knows a member came back untouched and keeps it
/// as it was, so opening a block and saving it changes nothing.
bool same_as_member(const core::Document& doc, core::EntityId e, core::Mm dx, core::Mm dy,
                    core::EntityId member);

/// Declares a TEXT column for every field `block`'s captions name that the
/// drawing has no column for (`core::block_fields`), so a reference has a cell
/// to hold its value and the caption does not stand there reading `{no}`.
/// The names declared, in order — none when every field had its column.
core::Result<std::vector<std::string>> ensure_field_columns(Context& ctx, core::BlockId block);

/// Places one reference with `ref` at `at` on the active layer, its box
/// computed from what it draws — BLOKEKLE's placement, and DIŞREFERANS's.
core::Result<core::EntityId> place_reference(Context& ctx, core::Point2 at,
                                             core::BlockReference ref);

/// Brings the stored box of every block reference that draws `block` — itself,
/// or through a block inside a block, on the sheet or inside another
/// definition — up to date with what it draws now. How many boxes changed.
core::Result<std::size_t> refresh_references(Context& ctx, core::BlockId block);

} // namespace kentos::command
