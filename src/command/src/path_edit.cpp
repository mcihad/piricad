// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/path_edit.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/identity.hpp"

#include <cstddef>

namespace kentos::command {
namespace {

std::int64_t key_of(const core::Document& doc, core::EntityId slot)
{
    return static_cast<std::int64_t>(core::raw(doc.key_of(slot)));
}

} // namespace

bool write_path(Context& ctx, core::EntityId slot, const core::CurvePath& path)
{
    const core::PathRecord rec = core::path_record(path);
    const core::KindId kind    = ctx.document().entities().kind[slot];
    if (rec.kind != kind) {
        ctx.refuse(core::ErrorCode::Unsupported, "Kalan parça bu nesnenin türünde yazılamıyor.");
        return false;
    }
    const core::RingGeometry::RingInput ring{rec.ring, rec.role, 0};
    const core::Status st =
        kind == core::kPolylineKind
            ? ctx.transaction().set_geometry(slot, {&ring, 1})
            : ctx.transaction().set_kind_geometry(slot, {&ring, 1}, rec.payload);
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    return true;
}

bool rewrite_path(Context& ctx, core::EntityId slot, const core::CurvePath& path)
{
    const core::PathRecord rec = core::path_record(path);
    const core::RingGeometry::RingInput ring{rec.ring, rec.role, 0};
    core::Status st;
    if (rec.kind == ctx.document().entities().kind[slot])
        st = rec.kind == core::kPolylineKind
                 ? ctx.transaction().set_geometry(slot, {&ring, 1})
                 : ctx.transaction().set_kind_geometry(slot, {&ring, 1}, rec.payload);
    else
        st = ctx.transaction().set_kind_geometry(slot, rec.kind, {&ring, 1}, rec.payload);
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    return true;
}

bool add_path_like(Context& ctx, core::EntityId like, const core::CurvePath& path,
                   std::vector<std::int64_t>& keys, std::span<const core::EntityKey> from)
{
    const core::Document& doc  = ctx.document();
    const core::PathRecord rec = core::path_record(path);
    const core::RingGeometry::RingInput ring{rec.ring, rec.role, 0};
    auto made =
        ctx.transaction().add_kind(doc.entities().layer[like], rec.kind, {&ring, 1}, rec.payload);
    if (!made) {
        ctx.refuse(made.error());
        return false;
    }
    if (const core::StyleId style = doc.entities().style[like]; style != core::kByLayerStyle)
        if (const auto st = ctx.transaction().set_entity_style(made.value(), style); !st) {
            ctx.refuse(st.error());
            return false;
        }
    // EVERY COLUMN TRAVELS: a piece of a thing is still that thing.
    const core::AttrTable& table = doc.attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col = static_cast<core::AttrId>(c);
        auto had       = doc.attribute(col, like);
        if (!had || !had.value().present) continue;
        if (const auto st = ctx.transaction().set_attribute(col, made.value(), had.value()); !st) {
            ctx.refuse(st.error());
            return false;
        }
    }
    // WHERE IT CAME FROM (core/lineage.hpp): the pieces of a trim know the
    // object they were cut from after the object itself is gone.
    const core::EntityKey self[] = {doc.entities().key[like]};
    if (const auto st =
            ctx.derive(made.value(), from.empty() ? std::span<const core::EntityKey>(self) : from);
        !st) {
        ctx.refuse(st.error());
        return false;
    }
    keys.push_back(key_of(doc, made.value()));
    return true;
}

bool replace_with_pieces(Context& ctx, core::EntityId slot,
                         const std::vector<core::CurvePath>& pieces, PathEdit& edit)
{
    const core::Document& doc = ctx.document();
    edit.source               = key_of(doc, slot);
    edit.result.clear();
    if (pieces.empty()) return true;

    // THE FIRST PIECE STAYS IN THE OBJECT when its kind can hold it; a circle's
    // pieces are arcs, a different kind, so there every piece is new.
    std::size_t first_new = 0;
    if (core::path_record(pieces.front()).kind == doc.entities().kind[slot]) {
        if (!write_path(ctx, slot, pieces.front())) return false;
        edit.result.push_back(edit.source);
        first_new = 1;
    }
    for (std::size_t i = first_new; i < pieces.size(); ++i)
        if (!add_path_like(ctx, slot, pieces[i], edit.result)) return false;

    // Erased LAST: the new pieces were drawn like it, from its own columns.
    if (first_new == 0)
        if (const auto st = ctx.transaction().erase_entity(slot); !st) {
            ctx.refuse(st.error());
            return false;
        }
    return true;
}

std::vector<core::CurvePath> split_at_points(const core::CurvePath& path,
                                             std::span<const core::Point2> points)
{
    std::vector<core::PathPlace> cuts;
    cuts.reserve(points.size());
    for (const core::Point2& p : points)
        cuts.push_back(core::place_of(path, p));
    return core::split_path(path, std::move(cuts));
}

core::Json edits_json(const std::vector<PathEdit>& edits)
{
    core::Json out = core::Json::array({});
    for (const PathEdit& e : edits) {
        core::Json keys = core::Json::array({});
        for (const std::int64_t k : e.result)
            keys.push(core::Json::integer(k));
        core::Json one = core::Json::object({});
        one.set("kaynak", core::Json::integer(e.source));
        one.set("sonuc", std::move(keys));
        out.push(std::move(one));
    }
    return out;
}

} // namespace kentos::command
