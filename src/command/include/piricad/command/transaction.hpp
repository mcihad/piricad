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
