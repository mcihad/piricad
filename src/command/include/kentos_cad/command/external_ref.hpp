// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: external references, the document half (TODOS C-14).
//
// An EXTERNAL REFERENCE is a block definition whose members come from a file
// (model.md R45a): loaded when the drawing opens and whenever it is reloaded,
// drawn and snapped to through its references like any block, edited by
// nobody here, and never written to the project file — the file holds the
// name and the path, the source holds the drawing. The blocks that file
// defines arrive beside it as its DEPENDENTS, named `NAME|BLOCK`.
//
// Reading the file is /src/io's (Article 3.2); what a definition IS, which
// blocks belong to it and how it is emptied is said once, here, so the loader
// and DIŞREFERANS agree.
#pragma once

#include "kentos_cad/command/transaction.hpp"

#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/document.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace kentos::command {

/// Whether `block` is an external reference still on the drawing: external,
/// and not taken off it (`core::kBlockDetached`).
bool is_external_reference(const core::Document& doc, core::BlockId block);

/// Whether `block` is an external reference or a block its file defines —
/// what no command edits in place.
bool is_external_block(const core::Document& doc, core::BlockId block);

/// The prefix an external reference's layers and blocks arrive under: its
/// name and a bar, `ALTLIK|`.
std::string external_prefix(const core::BlockDef& def);

/// The blocks external reference `block`'s file defines, by their prefix.
std::vector<core::BlockId> external_dependents(const core::Document& doc, core::BlockId block);

/// The live references ON THE SHEET that draw `block` directly.
std::vector<core::EntityId> sheet_references(const core::Document& doc, core::BlockId block);

/// Takes every member out of external reference `block` and its dependents,
/// and brings the box of every reference that draws it up to date: what
/// unloading does, and what a reload does before the file comes in again.
/// How many members went.
core::Result<std::size_t> empty_external(Transaction& tx, core::BlockId block);

} // namespace kentos::command
