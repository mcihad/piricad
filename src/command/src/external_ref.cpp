// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/external_ref.hpp"

#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/text.hpp"

#include <string>
#include <vector>

namespace kentos::command {

bool is_external_reference(const core::Document& doc, core::BlockId block)
{
    if (block >= doc.blocks().size()) return false;
    const std::uint8_t flags = doc.blocks().at(block).flags;
    return (flags & core::kBlockExternal) != 0 && (flags & core::kBlockDetached) == 0;
}

bool is_external_block(const core::Document& doc, core::BlockId block)
{
    return block < doc.blocks().size() && doc.blocks().at(block).external();
}

std::string external_prefix(const core::BlockDef& def)
{
    return def.name + "|";
}

std::vector<core::BlockId> external_dependents(const core::Document& doc, core::BlockId block)
{
    std::vector<core::BlockId> out;
    if (block >= doc.blocks().size()) return out;
    // Compared under the same Turkish folding block names are unique under, so
    // `altlik|kapak` belongs to `ALTLIK` exactly as the table would find it.
    const std::string prefix = core::turkish_fold_key(external_prefix(doc.blocks().at(block)));
    for (core::BlockId b = 0; b < doc.blocks().size(); ++b) {
        const core::BlockDef& def = doc.blocks().at(b);
        if ((def.flags & core::kBlockDependent) == 0) continue;
        if (def.folded.starts_with(prefix)) out.push_back(b);
    }
    return out;
}

std::vector<core::EntityId> sheet_references(const core::Document& doc, core::BlockId block)
{
    std::vector<core::EntityId> out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.entities().standalone(e) || doc.entities().kind[e] != core::kBlockReferenceKind)
            continue;
        auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[e]);
        if (ref && ref.value().block == block) out.push_back(e);
    }
    return out;
}

core::Result<std::size_t> empty_external(Transaction& tx, core::BlockId block)
{
    std::size_t gone               = 0;
    std::vector<core::BlockId> all = external_dependents(tx.document(), block);
    all.push_back(block);
    for (const core::BlockId b : all) {
        auto cleared = tx.clear_block_members(b);
        if (!cleared) return cleared.error();
        gone += cleared.value();
    }
    if (auto refreshed = tx.refresh_block_references(block); !refreshed) return refreshed.error();
    return gone;
}

} // namespace kentos::command
