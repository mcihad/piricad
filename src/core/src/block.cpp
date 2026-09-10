// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/block.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <limits>

namespace kentos::core {

Result<BlockId> BlockTable::add(std::string_view name, std::string_view description, Point2 base)
{
    if (name.empty()) return err(ErrorCode::InvalidArgument, "Blok adı boş olamaz.");
    if (find(name) != kNoBlock)
        return err(ErrorCode::ValidationFailed, "'" + std::string(name) +
                                                    "' adlı blok zaten var; blok adları Türkçe "
                                                    "katlamayla benzersizdir.");
    if (defs_.size() >= std::numeric_limits<std::uint32_t>::max() - 1)
        return err(ErrorCode::ValidationFailed, "Blok tablosu dolu.");

    BlockDef def;
    def.name        = std::string(name);
    def.folded      = turkish_fold_key(name);
    def.description = std::string(description);
    def.base        = base;
    defs_.push_back(std::move(def));
    return static_cast<BlockId>(defs_.size() - 1);
}

BlockId BlockTable::find(std::string_view name) const
{
    const std::string folded = turkish_fold_key(name);
    for (std::size_t i = 0; i < defs_.size(); ++i)
        if (defs_[i].folded == folded) return static_cast<BlockId>(i);
    return kNoBlock;
}

Status BlockTable::add_member(BlockId id, EntityKey member, BlockId uses)
{
    if (id >= defs_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(id));
    if (uses != kNoBlock) {
        if (uses >= defs_.size())
            return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(uses));
        if (would_cycle(id, uses))
            return err(ErrorCode::ValidationFailed,
                       "'" + defs_[id].name + "' bloğu '" + defs_[uses].name +
                           "' bloğunu içeremez: kendini içeren bir tanım sonsuza dek açılır.");
        if (std::find(defs_[id].uses.begin(), defs_[id].uses.end(), uses) == defs_[id].uses.end())
            defs_[id].uses.push_back(uses);
    }
    defs_[id].members.push_back(member);
    return ok();
}

bool BlockTable::would_cycle(BlockId container, BlockId referenced) const
{
    if (container == referenced) return true;
    if (referenced >= defs_.size()) return false;
    // Depth-first over `uses`, with a visited set so a diamond is walked once.
    std::vector<BlockId> stack{referenced};
    std::vector<bool> seen(defs_.size(), false);
    while (!stack.empty()) {
        const BlockId at = stack.back();
        stack.pop_back();
        if (at == container) return true;
        if (at >= defs_.size() || seen[at]) continue;
        seen[at] = true;
        for (const BlockId next : defs_[at].uses)
            stack.push_back(next);
    }
    return false;
}

std::uint64_t BlockTable::fold(std::uint64_t seed) const
{
    std::uint64_t h = seed;
    for (const BlockDef& d : defs_) {
        h = fnv1a(d.folded, h);
        h = fnv1a(d.description, h);
        h = fnv1a_int(d.base.x, h);
        h = fnv1a_int(d.base.y, h);
        for (const EntityKey k : d.members)
            h = fnv1a_int(static_cast<std::int64_t>(raw(k)), h);
        h = fnv1a_int(static_cast<std::int64_t>(d.flags), h);
        h = fnv1a_int(-3, h); // block terminator
    }
    return h;
}

} // namespace kentos::core
