// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/outline.hpp"

#include "kentos_cad/core/block_reference.hpp"

namespace kentos::core {

bool curve_entity_outline(const Document& doc, EntityId e, EmitBuffer& into)
{
    const EntityTable& entities = doc.entities();
    // The block reference is the one kind drawn from more than its own slot:
    // its members, placed (block_reference.hpp). Every other kind answers from
    // its geometry alone.
    if (entities.kind[e] == kBlockReferenceKind) {
        into.clear();
        return expand_block_reference(doc, e, into, 0);
    }
    return curve_outline(entities.kind[e], doc.geometry(), entities.slot[e], into);
}

} // namespace kentos::core
