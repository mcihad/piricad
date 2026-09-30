// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the active selection.
//
// .claude/model.md R43: selection is NOT document state. It never touches
// `content_hash()`, it never touches `revision()`, and changing it is never
// journalled as a document mutation. A drawing selected differently is the same
// drawing, and two engineers who select different parcels must be able to sign
// the same pafta.
//
// R44: a selection is stored as `EntityKey`, never as a dense slot. The reason is
// specific and not stylistic — a journalled or restored selection must survive a
// save, a reorder and a reload, and a slot is valid only inside one in-memory
// `Document` (R1). `scripts/ci-gate-model.sh` enforces exactly that on this file
// by name: an `EntityId` appearing here breaks the build.
//
// The store is a SORTED vector of keys rather than a hash set, for the same
// reason `SettingCatalog` uses parallel arrays: a selection is O(hundreds) in
// practice, a binary search beats a hash lookup at that size, and iteration order
// is fixed instead of depending on the hash function (core.md P11). A selection
// that reported its members in a different order on two machines would produce
// two different journal lines from one user action.
#pragma once

#include "piricad/core/identity.hpp"

#include <cstdint>
#include <vector>

namespace piricad::command {

class Selection
{
public:
    /// True when the key was not already present.
    bool add(core::EntityKey key);

    /// True when the key was present and has been dropped.
    bool remove(core::EntityKey key);

    /// Adds when absent, removes when present. Returns true when it ended up in.
    bool toggle(core::EntityKey key);

    void clear();

    bool contains(core::EntityKey key) const;

    bool empty() const noexcept { return keys_.empty(); }

    std::size_t size() const noexcept { return keys_.size(); }

    /// Ascending key order, always. The command transcript, the canvas highlight
    /// and the journal all read this, and all three must agree.
    const std::vector<core::EntityKey>& keys() const noexcept { return keys_; }

    /// Bumped by every change. A view caches its resolved slot list against this
    /// number so a frame does not re-resolve an unchanged selection (§10.1).
    std::uint64_t revision() const noexcept { return revision_; }

private:
    std::vector<core::EntityKey> keys_; ///< sorted ascending, unique
    std::uint64_t revision_{0};
};

} // namespace piricad::command
