// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the drawn shape of one entity.
//
// `curve_outline` (entity_kind.hpp) answers for a KIND over the geometry alone,
// which is all a circle, an arc or an ellipse needs. Some shapes need the
// document as well: a block reference is drawn by expanding a definition the
// document holds, and a dimension's text comes from the text table. This is the
// one document-aware entry the renderer, the pick test, the snap engine and the
// canvas call, so a kind that needs the document becomes visible, selectable and
// snappable by being handled HERE — once — and not by four edits that have to
// agree (model.md R22).
#pragma once

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"

namespace kentos::core {

/// The out-of-line half of `entity_outline`, for a kind that is not a polyline.
bool curve_entity_outline(const Document& doc, EntityId e, EmitBuffer& into);

/// Fills `into` with the runs that draw entity `e`, and returns true.
///
/// Returns false for `core.polyline`, whose rings ARE its outline and are read
/// straight out of the arena with no copy — every entity on the five-million-
/// parcel sheet the frame budget is written against (§10.1) — and for a kind this
/// build does not know, which is drawn from its rings for the same reason
/// (model.md R26: visible, as stored).
///
/// INLINE FOR THE POLYLINE, on purpose: the full-extent frame calls this once per
/// entity, and a call across a translation unit for the one comparison that says
/// "a polyline, read the rings" measured as a fifth of that frame.
inline bool entity_outline(const Document& doc, EntityId e, EmitBuffer& into)
{
    const EntityTable& entities = doc.entities();
    if (e >= entities.size() || entities.kind[e] == kPolylineKind) return false;
    return curve_entity_outline(doc, e, into);
}

} // namespace kentos::core
