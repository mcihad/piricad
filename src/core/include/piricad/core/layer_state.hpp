// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: saved layer states.
//
// A LAYER STATE is a named picture of which layers are shown, locked, printed and picked — the four
// answers a layer gives (`LayerProps`, `Layer::visible/locked`) — so a drawing worked in several
// ways (surveyed points only; the plan for review; the same plan for print) moves between them in
// one step instead of toggling forty layers by hand (TODOS U-05, "kayıtlı çalışma alanları").
//
// IT LIVES IN THE DRAWING, for the reason a layout does: the view a reviewer was asked to approve
// is part of the submitted work, and a colleague who opens the file must find the same states. So a
// state is document state — saved, in the content hash, undone with Ctrl+Z — and, like a layout, it
// is furniture and not an entity.
//
// IT NAMES LAYERS BY THEIR PERSISTENT KEY, never by slot or by name: a layer renamed after the
// state was saved is still the layer the state meant, and a layer deleted since is skipped out loud
// rather than matched to a stranger (model.md R1/R2). A layer created after the state was saved is
// not in it and applying the state leaves it as it is.
#pragma once

#include "piricad/core/identity.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

/// One layer's answers inside a state.
struct LayerStateRow
{
    LayerKey key{LayerKey::None}; ///< the layer, by its persistent key
    bool visible{true};           ///< shown
    bool locked{false};           ///< locked against edits
    bool plottable{true};         ///< printed
    bool selectable{true};        ///< taken by a selection

    friend bool operator==(const LayerStateRow&, const LayerStateRow&) = default;
};

/// A named state: every layer the drawing had when it was saved.
struct LayerState
{
    std::string name;                ///< what the person called it
    std::vector<LayerStateRow> rows; ///< one per layer the drawing had

    friend bool operator==(const LayerState&, const LayerState&) = default;
};

/// The drawing's states, in the order they were saved.
class LayerStateStore
{
public:
    /// How many states the drawing has.
    std::size_t size() const noexcept { return states_.size(); }

    /// Whether it has none.
    bool empty() const noexcept { return states_.empty(); }

    /// Every state, in the order they were saved.
    const std::vector<LayerState>& all() const noexcept { return states_; }

    /// The state of that name, Turkish-folded (`PLAN`, `plan` and `Plan` are one), or null.
    const LayerState* find(std::string_view name) const;

    /// Adds `state`, or replaces the one of its name in place. Refuses an empty name.
    Status upsert(LayerState state);

    /// Removes the state of that name. False when there was none.
    bool remove(std::string_view name);

    /// Replaces the whole list — what undo does.
    void load(std::vector<LayerState> states) { states_ = std::move(states); }

    /// Folds every state into `seed`. An EMPTY store returns `seed` untouched, so every fingerprint
    /// written before layer states existed still stands.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    std::vector<LayerState> states_;
};

} // namespace piricad::core
