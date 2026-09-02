// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: slot and key identity.
//
// .claude/model.md R1–R5. Two id kinds exist and confusing them is the single
// most expensive mistake available in this codebase, so they are different types
// and the compiler refuses the confusion.
//
//   EntityId / LayerId   dense u32 SLOT. Valid only inside one in-memory
//                        Document. Used by the cull path, the spatial index,
//                        DrawList and Op. Invalidated by compaction or reorder.
//
//   EntityKey / LayerKey persistent u64 KEY. Monotonic, never reused, survives
//                        save, load, reorder and compaction. Used by the journal,
//                        selection, external references and AI tool calls.
//
// Translation happens at the command bus boundary only (R2). A key never enters
// the frame path; a slot never enters a file, a journal line or a user's screen.
#pragma once

#include <cstdint>
#include <limits>

namespace piricad::core {

/// Which KIND of thing an entity is — `KindSpec::id`, and the reason two entities
/// holding the same two vertices can be a line and a circle.
///
/// The TYPE lives here rather than in `entity_kind.hpp` so that a caller who only
/// needs to ask "is this a circle?" does not have to include the kind machinery.
/// That is not tidiness: `KindSpec` has a member called `emit`, `emit` is a Qt
/// macro, and pulling that struct into a Qt translation unit does not fail with a
/// message about Qt — it fails with `expected unqualified-id` on a line that
/// looks perfectly good.
using KindId = std::uint16_t;

inline constexpr KindId kNoKind = 0xFFFFu;

/// The built-in kind ids, DECLARED by each kind rather than handed out in
/// registration order. The project writer stores this number, so a value once
/// used can never be re-meant (model.md R26).
inline constexpr KindId kPolylineKind = 1;
inline constexpr KindId kCircleKind   = 2;
inline constexpr KindId kArcKind      = 3;
inline constexpr KindId kPointKind    = 4;
inline constexpr KindId kEllipseKind  = 5;


// ---------------------------------------------------------------- slots -----

using EntityId = std::uint32_t;
using LayerId  = std::uint32_t;

inline constexpr EntityId kNoEntity = std::numeric_limits<EntityId>::max();
inline constexpr LayerId kNoLayer   = std::numeric_limits<LayerId>::max();

// ----------------------------------------------------------------- keys -----

/// Scoped so a key cannot be passed where a slot is expected, or written into a
/// `DrawList`. The underlying type is fixed because it reaches the file format.
enum class EntityKey : std::uint64_t { None = 0 };
enum class LayerKey : std::uint64_t { None = 0 };

constexpr std::uint64_t raw(EntityKey k) noexcept
{
    return static_cast<std::uint64_t>(k);
}

/// The same, for a layer key.
constexpr std::uint64_t raw(LayerKey k) noexcept
{
    return static_cast<std::uint64_t>(k);
}

/// R3: `command::Value::Kind::IdList` is `std::vector<std::int64_t>`, so a key
/// above 2^63-1 becomes negative the moment it is journalled. The allocator
/// refuses to mint past this; at one key per microsecond that is 292,000 years.
inline constexpr std::uint64_t kMaxKey = (std::uint64_t{1} << 63) - 1;

/// Mints keys for one document. Monotonic and never reusing, because "which
/// parcel was this?" is a legal question and a reused key makes it unanswerable
/// (R4, §12).
class KeyAllocator
{
public:
    /// Returns the next key, or `None` once the space is exhausted. Exhaustion is
    /// reported rather than wrapped: silently reusing a key is worse than failing.
    EntityKey mint_entity() noexcept
    {
        if (next_entity_ > kMaxKey) return EntityKey::None;
        return static_cast<EntityKey>(next_entity_++);
    }

    /// The next unused layer key, or `None` when the space is exhausted.
    LayerKey mint_layer() noexcept
    {
        if (next_layer_ > kMaxKey) return LayerKey::None;
        return static_cast<LayerKey>(next_layer_++);
    }

    /// After loading a file, the allocator must not hand out a key the file
    /// already used. Called once per key array with the highest value seen.
    ///
    /// The clamp is not defensive tidiness. A key above kMaxKey is exactly the
    /// value R3 exists to make impossible, and it arrives from UNTRUSTED input —
    /// a file, a hostile payload, a table written by a build that got this wrong.
    /// `next = raw(highest) + 1` on 2^64-1 wrapped the counter to 0, so the next
    /// mint returned EntityKey{0}, which is indistinguishable from
    /// EntityKey::None (a false exhaustion report), and every mint after that
    /// handed out 1, 2, 3 — keys the file had already used. That is silent key
    /// REUSE reached from a file, and R4/P5 make "which parcel was this?"
    /// unanswerable the moment it happens.
    ///
    /// Saturating at kMaxKey + 1 rather than wrapping means the allocator reports
    /// exhaustion (mint returns None) instead of reissuing. Returns false when the
    /// adopted key was out of range, so a loader can refuse the file rather than
    /// carry an id it can never journal.
    bool adopt_entity(EntityKey highest) noexcept
    {
        const std::uint64_t v      = raw(highest);
        const std::uint64_t capped = v > kMaxKey ? kMaxKey : v;
        if (capped >= next_entity_) next_entity_ = capped + 1;
        return v <= kMaxKey;
    }

    /// Moves the layer counter past a key read from a file. Same clamping and
    /// same reason as `adopt_entity`.
    bool adopt_layer(LayerKey highest) noexcept
    {
        const std::uint64_t v      = raw(highest);
        const std::uint64_t capped = v > kMaxKey ? kMaxKey : v;
        if (capped >= next_layer_) next_layer_ = capped + 1;
        return v <= kMaxKey;
    }

    /// Where each counter stands, WITHOUT minting. Written into the file so a
    /// reopened drawing cannot hand a new entity a key a dead one already used.
    std::uint64_t peek_entity() const noexcept { return next_entity_; }

    std::uint64_t peek_layer() const noexcept { return next_layer_; }

    /// Test and recovery hook: positions the counter without minting. Clamped the
    /// same way adopt_* is, so no caller can install a counter that wraps.
    void seek_entity(std::uint64_t next) noexcept
    {
        next_entity_ = next > kMaxKey + 1 ? kMaxKey + 1 : next;
    }

    /// The same, for layers.
    void seek_layer(std::uint64_t next) noexcept
    {
        next_layer_ = next > kMaxKey + 1 ? kMaxKey + 1 : next;
    }

private:
    // Zero is reserved for "none", so minting starts at one.
    std::uint64_t next_entity_{1};
    std::uint64_t next_layer_{1};
};

} // namespace piricad::core
