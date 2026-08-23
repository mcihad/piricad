// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: transaction and undo.
//
// piricad.md §2.5:
//   * one command  = one undo step (default)
//   * one script block or one AI suggestion = ONE merged undo step
//   * a validation failure inside a transaction = full rollback, no partial apply
//   * a half-applied edit on cadastral or zoning data is never acceptable
#pragma once

#include "piricad/core/document.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

using core::Appearance;
using core::Document;
using core::EntityId;
using core::LayerId;
using core::Op;
using core::Point2;
using core::Result;
using core::RingGeometry;
using core::Status;
using core::StyleId;

/// Collects the inverse of every primitive edit made through it.
/// The ONLY sanctioned route to Document mutation (Constitution Article 1).
class Transaction
{
public:
    Transaction(Document& doc, std::string label);

    /// Returns the slot of the layer with this name, creating it if absent.
    ///
    /// NOT undoable, and deliberately so — the same decision `Document` records:
    /// an empty layer is inert, and removing it on undo would invalidate every
    /// stored slot in `entity.layer`. It lives here anyway because a caller
    /// outside /src/command must have ONE sanctioned handle for document work
    /// (Article 5.9) rather than reaching past the transaction for this one call.
    LayerId ensure_layer(std::string_view name);

    /// Interns an appearance and returns its id, for a command — or a file reader
    /// — that resolves a style at commit time (model.md R14).
    ///
    /// Also not undoable, for two reasons that reinforce each other. The style
    /// table is a deduplicated pool: adding to it changes nothing that is drawn
    /// until an entity's style column points at the new entry, and that write IS
    /// undoable (`set_entity_style`). And the table only ever grows, so an id once
    /// handed out stays valid for the document's lifetime — rolling an intern back
    /// would renumber ids that other entities, and the journal's recorded previous
    /// values, already point at.
    StyleId intern_style(const Appearance& a);

    Result<EntityId> add_polyline(LayerId layer, std::span<const Point2> pts);

    /// A face: one exterior ring, optionally with holes, optionally multipart.
    /// This is what a parcel is (model.md R9).
    Result<EntityId> add_area(LayerId layer, std::span<const RingGeometry::RingInput> rings);
    Status erase_entity(EntityId e);
    Status restore_entity(EntityId e);
    Status set_layer_visible(LayerId l, bool visible);
    Status set_layer_locked(LayerId l, bool locked);
    Status set_layer_appearance(LayerId l, const Appearance& a);
    Status set_entity_style(EntityId e, StyleId style);

    Status set_entity_hidden(EntityId e, bool hidden);
    Status set_crs(std::string id);

    /// R28's generic attribute write, and the only sanctioned way to reach one.
    /// Undoable: an ada number typed wrong is exactly the kind of mistake Ctrl+Z
    /// exists for, and the previous value is what the document hands back.
    Status set_attribute(core::AttrId col, EntityId e, const core::AttrValue& v);

    /// Declares a column. NOT undoable — see Document::declare_attribute.
    core::Result<core::AttrId> declare_attribute(core::AttrSpec spec);

    /// Attaches or replaces the text on an entity. Height is ground millimetres;
    /// an empty `content` detaches it. Undoable like any other edit.
    Status set_text(EntityId e, std::string content, core::Mm height, core::TextAnchor anchor);

    /// Reverts every edit made through this transaction, newest first.
    void rollback();

    /// Hands the inverse record over to the undo stack and clears it.
    std::vector<Op> release();

    bool empty() const noexcept { return inverse_.empty(); }

    std::size_t size() const noexcept { return inverse_.size(); }

    const std::string& label() const noexcept { return label_; }

    void set_label(std::string l) { label_ = std::move(l); }

    Document& document() noexcept { return doc_; }

private:
    Document& doc_;
    std::string label_;
    std::vector<Op> inverse_; ///< newest last
};

struct UndoEntry
{
    std::string label;
    std::vector<Op> inverse;
};

class UndoStack
{
public:
    void push(UndoEntry e);

    bool can_undo() const noexcept { return !undo_.empty(); }

    bool can_redo() const noexcept { return !redo_.empty(); }

    /// Applies the top inverse record and moves it to the redo stack.
    Status undo(Document& doc, std::string* label_out = nullptr);
    Status redo(Document& doc, std::string* label_out = nullptr);

    void clear();

    std::size_t undo_depth() const noexcept { return undo_.size(); }

    std::size_t redo_depth() const noexcept { return redo_.size(); }

    std::string next_undo_label() const
    {
        return undo_.empty() ? std::string{} : undo_.back().label;
    }

    std::string next_redo_label() const
    {
        return redo_.empty() ? std::string{} : redo_.back().label;
    }

private:
    std::vector<UndoEntry> undo_;
    std::vector<UndoEntry> redo_;
};

} // namespace piricad::command
