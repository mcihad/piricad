// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/transaction.hpp"

#include <algorithm>

namespace piricad::command {

Transaction::Transaction(Document& doc, std::string label) : doc_(doc), label_(std::move(label)) {}

LayerId Transaction::ensure_layer(std::string_view name)
{
    // No inverse is recorded: see the header. Creating a layer is additive and
    // slot-stable, and an undo that removed it would leave `entity.layer` values
    // pointing at nothing.
    return doc_.ensure_layer(name);
}

StyleId Transaction::intern_style(const Appearance& a)
{
    return doc_.intern_style(a);
}

Result<EntityId> Transaction::add_polyline(LayerId layer, std::span<const Point2> pts)
{
    core::Op undo;
    auto id = doc_.add_polyline(layer, pts, undo);
    if (!id) return id;
    inverse_.push_back(std::move(undo));
    return id;
}

Result<EntityId> Transaction::add_area(LayerId layer,
                                       std::span<const RingGeometry::RingInput> rings)
{
    core::Op undo;
    auto id = doc_.add_area(layer, rings, undo);
    if (!id) return id;
    inverse_.push_back(std::move(undo));
    return id;
}

Status Transaction::set_entity_layer(EntityId e, LayerId layer)
{
    core::Op undo;
    auto st = doc_.set_entity_layer(e, layer, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::erase_entity(EntityId e)
{
    core::Op undo;
    auto st = doc_.set_entity_alive(e, false, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::restore_entity(EntityId e)
{
    core::Op undo;
    auto st = doc_.set_entity_alive(e, true, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_visible(LayerId l, bool visible)
{
    core::Op undo;
    auto st = doc_.set_layer_visible(l, visible, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_locked(LayerId l, bool locked)
{
    core::Op undo;
    auto st = doc_.set_layer_locked(l, locked, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_group(LayerId l, std::string group)
{
    core::Op undo;
    auto st = doc_.set_layer_group(l, std::move(group), undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_appearance(LayerId l, const Appearance& a)
{
    core::Op undo;
    auto st = doc_.set_layer_appearance(l, a, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_layer_style(LayerId l, StyleId style)
{
    core::Op undo;
    auto st = doc_.set_layer_style(l, style, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_entity_style(EntityId e, StyleId style)
{
    core::Op undo;
    auto st = doc_.set_entity_style(e, style, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Result<EntityId> Transaction::add_circle(LayerId layer, Point2 centre, core::Mm radius)
{
    core::Op undo;
    auto made = doc_.add_circle(layer, centre, radius, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Result<EntityId> Transaction::add_arc(LayerId layer, Point2 centre, core::Mm radius, Point2 start,
                                      Point2 end)
{
    core::Op undo;
    auto made = doc_.add_arc(layer, centre, radius, start, end, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Result<EntityId> Transaction::add_point(LayerId layer, Point2 at)
{
    core::Op undo;
    auto made = doc_.add_point(layer, at, undo);
    if (!made) return made;
    inverse_.push_back(std::move(undo));
    return made;
}

Status Transaction::set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings)
{
    core::Op undo;
    auto st = doc_.set_geometry(e, rings, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

StyleId Transaction::intern_symbol(const core::Symbol& sym)
{
    return doc_.intern_symbol(sym);
}

core::Result<core::ImageId> Transaction::intern_image(std::span<const std::byte> bytes,
                                                      std::string_view origin)
{
    return doc_.intern_image(bytes, origin);
}

core::Result<core::DashId> Transaction::intern_dash(const core::DashPattern& pattern,
                                                    std::string_view origin)
{
    return doc_.intern_dash(pattern, origin);
}

Status Transaction::set_attribute(core::AttrId col, EntityId e, const core::AttrValue& v)
{
    core::Op undo;
    auto st = doc_.set_attribute(col, e, v, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_text(EntityId e, std::string content, core::Mm height,
                             core::TextAnchor anchor)
{
    core::Op undo;
    auto st = doc_.set_text(e, std::move(content), height, anchor, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

core::Result<core::AttrId> Transaction::declare_attribute(core::AttrSpec spec)
{
    return doc_.declare_attribute(std::move(spec));
}

Status Transaction::add_guide(core::GuideAxis axis, core::Mm coordinate)
{
    core::Op undo;
    auto st = doc_.add_guide(axis, coordinate, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::remove_guide(std::size_t index)
{
    core::Op undo;
    auto st = doc_.remove_guide(index, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_entity_hidden(EntityId e, bool hidden)
{
    core::Op undo;
    auto st = doc_.set_entity_hidden(e, hidden, undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

Status Transaction::set_crs(core::Crs crs)
{
    core::Op undo;
    auto st = doc_.set_crs(std::move(crs), undo);
    if (!st) return st;
    inverse_.push_back(std::move(undo));
    return core::ok();
}

void Transaction::rollback()
{
    // Newest first: the inverse of a sequence is the reversed sequence of inverses.
    for (auto it = inverse_.rbegin(); it != inverse_.rend(); ++it)
        (void)doc_.apply(*it);
    inverse_.clear();
}

std::vector<core::Op> Transaction::release()
{
    return std::move(inverse_);
}

void UndoStack::push(UndoEntry e)
{
    if (e.inverse.empty()) return; // a no-op command never occupies an undo step
    undo_.push_back(std::move(e));
    redo_.clear(); // a new edit invalidates the redo branch
}

Status UndoStack::undo(Document& doc, std::string* label_out)
{
    if (undo_.empty()) return core::err(core::ErrorCode::NotFound, "Geri alınacak işlem yok");

    UndoEntry entry = std::move(undo_.back());
    undo_.pop_back();

    UndoEntry redo_entry;
    redo_entry.label = entry.label;
    redo_entry.inverse.reserve(entry.inverse.size());

    for (auto it = entry.inverse.rbegin(); it != entry.inverse.rend(); ++it) {
        core::Op back;
        auto st = doc.apply(*it, &back);
        if (!st) return st;
        redo_entry.inverse.push_back(std::move(back));
    }
    std::reverse(redo_entry.inverse.begin(), redo_entry.inverse.end());

    if (label_out) *label_out = entry.label;
    redo_.push_back(std::move(redo_entry));
    return core::ok();
}

Status UndoStack::redo(Document& doc, std::string* label_out)
{
    if (redo_.empty()) return core::err(core::ErrorCode::NotFound, "Yinelenecek işlem yok");

    UndoEntry entry = std::move(redo_.back());
    redo_.pop_back();

    UndoEntry undo_entry;
    undo_entry.label = entry.label;
    undo_entry.inverse.reserve(entry.inverse.size());

    for (auto it = entry.inverse.rbegin(); it != entry.inverse.rend(); ++it) {
        core::Op back;
        auto st = doc.apply(*it, &back);
        if (!st) return st;
        undo_entry.inverse.push_back(std::move(back));
    }
    std::reverse(undo_entry.inverse.begin(), undo_entry.inverse.end());

    if (label_out) *label_out = entry.label;
    undo_.push_back(std::move(undo_entry));
    return core::ok();
}

void UndoStack::clear()
{
    undo_.clear();
    redo_.clear();
}

} // namespace piricad::command
