// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: a curve path written back into the drawing (TODOS C-05).
//
// BÖL, KIR, UÇUCA, BUDA and UZAT all end the same way: pieces of a walked curve
// (`core::CurvePath`) become objects again. What must not differ between them is
// what a piece keeps and what it becomes, so it is said here once:
//
//   * A PIECE IS STORED IN ITS OWN KIND (`core::path_record`): straight pieces
//     a polyline, one arc an arc, a whole turn a circle, and anything that bends
//     part of the way an arc-polyline — an arc cut from a kerb line stays an
//     arc, it is never its chord (C-05: "yaylar düzleşmez").
//   * THE FIRST PIECE KEEPS THE OBJECT when its kind can hold it, so its key,
//     layer, style and attributes stay put (model.md R4, R28); every other piece
//     is a new object drawn like it — the same layer, style and every attribute
//     cell, because a road cut in two is still that road on both sides.
//   * WHAT BECAME OF WHICH KEY IS SAID (`PathEdit`): an agent, a script or a
//     person who tracks objects by key is told the source and its results
//     rather than left to find them.
#pragma once

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/json.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kentos::command {

/// What one source object became: its key, and the keys of the objects that
/// now hold it, in order along it. The source's own key is among them when the
/// first piece stayed in it.
struct PathEdit
{
    std::int64_t source{0};           ///< the key edited
    std::vector<std::int64_t> result; ///< the keys holding it now
};

/// Writes `path` into `slot` in place, keeping its key, layer, style and
/// attributes. False, having refused, when the slot's kind cannot hold the path
/// — a polyline cannot hold an arc; `replace_with_pieces` makes a new object
/// then.
bool write_path(Context& ctx, core::EntityId slot, const core::CurvePath& path);

/// A new object holding `path`, drawn like `like`: its layer, its style and
/// every attribute cell. Its key is appended to `keys`. False, having refused,
/// when the document refuses the geometry.
bool add_path_like(Context& ctx, core::EntityId like, const core::CurvePath& path,
                   std::vector<std::int64_t>& keys);

/// Replaces the object in `slot` by `pieces`, in order: the first written in
/// place when the slot's kind holds it, every other piece — or all of them,
/// when the kind changes (a circle's pieces are arcs) — new and drawn like it,
/// and the source erased when nothing stayed in it.
bool replace_with_pieces(Context& ctx, core::EntityId slot,
                         const std::vector<core::CurvePath>& pieces, PathEdit& edit);

/// The key→keys map of a run's edits, for `Context::report`: one entry per
/// source, `{"kaynak": key, "sonuc": [keys]}`.
core::Json edits_json(const std::vector<PathEdit>& edits);

/// The pieces `path` comes apart into at the places on it nearest `points` —
/// BÖL's point method, and the preview that draws what it will make.
std::vector<core::CurvePath> split_at_points(const core::CurvePath& path,
                                             std::span<const core::Point2> points);

} // namespace kentos::command
