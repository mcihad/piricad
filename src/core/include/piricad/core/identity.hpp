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

// ---------------------------------------------------------------- slots -----

using EntityId = std::uint32_t;
using LayerId  = std::uint32_t;

inline constexpr EntityId kNoEntity = std::numeric_limits<EntityId>::max();
inline constexpr LayerId  kNoLayer  = std::numeric_limits<LayerId>::max();

// ----------------------------------------------------------------- keys -----

/// Scoped so a key cannot be passed where a slot is expected, or written into a
/// `DrawList`. The underlying type is fixed because it reaches the file format.
enum class EntityKey : std::uint64_t { None = 0 };
enum class LayerKey : std::uint64_t { None = 0 };

constexpr std::uint64_t raw(EntityKey k) noexcept { return static_cast<std::uint64_t>(k); }
constexpr std::uint64_t raw(LayerKey k) noexcept { return static_cast<std::uint64_t>(k); }

/// R3: `command::Value::Kind::IdList` is `std::vector<std::int64_t>`, so a key
/// above 2^63-1 becomes negative the moment it is journalled. The allocator
/// refuses to mint past this; at one key per microsecond that is 292,000 years.
inline constexpr std::uint64_t kMaxKey = (std::uint64_t{1} << 63) - 1;

/// Mints keys for one document. Monotonic and never reusing, because "which
/// parcel was this?" is a legal question and a reused key makes it unanswerable
/// (R4, §12).
class KeyAllocator {
public:
    /// Returns the next key, or `None` once the space is exhausted. Exhaustion is
    /// reported rather than wrapped: silently reusing a key is worse than failing.
    EntityKey mint_entity() noexcept
    {
        if (next_entity_ > kMaxKey) return EntityKey::None;
        return static_cast<EntityKey>(next_entity_++);
    }

    LayerKey mint_layer() noexcept
    {
        if (next_layer_ > kMaxKey) return LayerKey::None;
        return static_cast<LayerKey>(next_layer_++);
    }

    /// After loading a file, the allocator must not hand out a key the file
    /// already used. Called once per key array with the highest value seen.
    void adopt_entity(EntityKey highest) noexcept
    {
        if (raw(highest) >= next_entity_) next_entity_ = raw(highest) + 1;
    }

    void adopt_layer(LayerKey highest) noexcept
    {
        if (raw(highest) >= next_layer_) next_layer_ = raw(highest) + 1;
    }

    std::uint64_t peek_entity() const noexcept { return next_entity_; }
    std::uint64_t peek_layer() const noexcept { return next_layer_; }

private:
    // Zero is reserved for "none", so minting starts at one.
    std::uint64_t next_entity_{1};
    std::uint64_t next_layer_{1};
};

} // namespace piricad::core
