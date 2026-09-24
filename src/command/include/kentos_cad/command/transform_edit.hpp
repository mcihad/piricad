// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: one transform applied to one object, for every verb.
//
// TAŞI, DÖNDÜR, ÖLÇEKLE, AYNALA, KOPYALA, DİZİ and HİZALA all end the same way:
// an object is carried by a `core::Xform`, in place or as a new copy. What an
// object keeps under a transform — a circle its roundness, an ellipse its
// perpendicular axes, a caption its letters the right size and the right way
// up, a dimension its measured figure, a block its turn and its scales — is
// said once, in `transform.cpp`, and every verb comes here for it (TODOS C-08).
#pragma once

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/transform.hpp"

namespace kentos::command {

/// Applies `x` to the object in `slot` in place, whatever kind it is. False,
/// having refused with the reason, when the kind cannot hold the result — an
/// arc polyline stretched unevenly, a turned block stretched.
bool transform_entity(Context& ctx, core::EntityId slot, const core::Xform& x);

/// A NEW object that is `slot` with `x` applied — the same kind and payload,
/// layer, style, text and attribute cells, and its own key — or the reason
/// refused. On `onto` when one is given: PATLAT sets a member drawn on the
/// drawing's `0` layer down on the reference's, and making it there is not the
/// same as making it on `0` and moving it — `0` may be locked.
core::Result<core::EntityId> clone_entity(Context& ctx, core::EntityId slot, const core::Xform& x,
                                          core::LayerId onto = core::kNoLayer);

} // namespace kentos::command
