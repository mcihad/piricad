// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: which class of object a document entity is, as a
// command's `targets` names them (spec.hpp).
//
// ONE ANSWER FOR THE QUESTION A TOOL IS GREYED BY. The ribbon asks it of the
// selection before a tool is pressed (ui.md R54), and the answer has to be the
// same class the command declared itself for — a parcel with a rounded corner
// is an area, not a curve, whatever kind id holds it (model.md R9b).
#pragma once

#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/identity.hpp"

#include <cstddef>
#include <span>

namespace kentos::command {

/// The class of object `e` of `doc` is, or `Targets::None` for a dead slot and
/// a kind this build does not know.
Targets target_of(const core::Document& doc, core::EntityId e);

/// Whether a command declared for `targets` acts on object `e`.
bool acts_on(Targets targets, const core::Document& doc, core::EntityId e);

/// WHAT A SELECTION HOLDS, by class: what the ribbon greys a tool by.
struct Held
{
    Targets classes{Targets::None}; ///< every class some object of it is
    bool unclassed{false};          ///< an object of no class a command names
    std::size_t count{0};           ///< the live objects counted
};

/// The classes the live objects among `keys` of `doc` are; a dead key is not
/// counted.
Held held_by(const core::Document& doc, std::span<const core::EntityKey> keys);

/// Whether a command declared for `targets` acts on EVERY object `held` counts.
/// A command refuses a run it was handed one wrong object in — OFSET does not
/// offset the lines of a selection that also holds a caption — so a tool is
/// offered for a selection only when all of it is the tool's own. An empty
/// selection is every tool's: the tool asks for its objects.
bool acts_on_all(Targets targets, const Held& held);

} // namespace kentos::command
