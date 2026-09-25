// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/block_edit.hpp"

#include "kentos_cad/command/transform_edit.hpp"

#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text_fields.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kentos::command {

core::Result<PlacedMember> place_member(Context& ctx, core::EntityId member, const core::Xform& x,
                                        core::EntityId reference)
{
    const core::Document& doc = ctx.document();
    const core::LayerId home  = doc.entities().layer[reference];
    const core::StyleId look  = doc.entities().style[reference];
    const core::LayerId zero  = doc.find_layer("0");
    const core::KindId kind   = doc.entities().kind[member];
    const core::LayerId own   = doc.entities().layer[member];
    const bool on_zero        = own == zero && zero != core::kNoLayer;
    const core::StyleId mine  = doc.entities().style[member];

    auto made = clone_entity(ctx, member, x, on_zero ? home : core::kNoLayer);
    if (!made) return made.error();
    PlacedMember out;
    out.piece          = made.value();
    out.onto_reference = on_zero && home != own;

    // THE LOOK IT WAS DRAWN IN. A hatch keeps its own: its style is its
    // pattern, set again by the transform.
    const bool inherits = mine == core::kByLayerStyle || core::style_by_block(doc, mine);
    if (inherits && kind != core::kHatchKind) {
        const core::StyleId want = on_zero ? look : core::kByLayerStyle;
        if (doc.entities().style[out.piece] != want)
            if (auto st = ctx.transaction().set_entity_style(out.piece, want); !st)
                return st.error();
        out.reference_look = want != core::kByLayerStyle || core::style_by_block(doc, mine);
    }
    if ((doc.entities().flags[member] & core::FlagHidden) != 0) {
        if (auto st = ctx.transaction().set_entity_hidden(out.piece, true); !st) return st.error();
        out.hidden = true;
    }
    // A CAPTION WITH FIELDS comes out saying what it said on the sheet: the
    // reference's own values written in, since the piece has no reference to
    // read them from any more (the attribute's value, kept — TODOS C-13).
    const std::uint32_t slot = doc.entities().slot[out.piece];
    if (doc.texts().has(slot) && core::has_fields(doc.texts().text(slot))) {
        auto words = core::fill_fields(doc, reference, doc.texts().text(slot));
        if (!words) return words.error();
        if (auto st = ctx.transaction().set_text(out.piece, words.value(), doc.texts().height(slot),
                                                 doc.texts().anchor(slot), doc.texts().lines(slot));
            !st)
            return st.error();
        out.filled = true;
    }
    return out;
}

namespace {

/// `e` moved by (`dx`, `dy`), made again inside `block` — or on the sheet, for
/// `core::kNoBlock` — with everything it carries.
core::Result<core::EntityId> copy_across(Context& ctx, core::EntityId e, core::BlockId block,
                                         core::Mm dx, core::Mm dy)
{
    const core::Document& doc = ctx.document();
    const std::uint32_t gslot = doc.entities().slot[e];
    auto moved                = core::translated_record(doc, e, dx, dy);
    if (!moved) return moved.error();
    const std::vector<core::RingGeometry::RingInput> rings = moved.value().inputs();
    auto member = ctx.transaction().add_kind(doc.entities().layer[e], doc.entities().kind[e], rings,
                                             moved.value().payload, block);
    if (!member) return member;
    const core::EntityId m = member.value();

    if (const core::StyleId st = doc.entities().style[e]; st != core::kByLayerStyle)
        if (auto s = ctx.transaction().set_entity_style(m, st); !s) return s.error();
    if (doc.texts().has(gslot))
        if (auto s = ctx.transaction().set_text(
                m, std::string(doc.texts().text(gslot)), doc.texts().height(gslot),
                doc.texts().anchor(gslot), doc.texts().lines(gslot));
            !s)
            return s.error();
    const core::AttrTable& attrs = doc.attributes();
    for (std::size_t c = 0; c < attrs.columns(); ++c) {
        auto cell = attrs.get(static_cast<core::AttrId>(c), gslot);
        if (!cell || !cell.value().present) continue;
        if (auto s = ctx.transaction().set_attribute(static_cast<core::AttrId>(c), m, cell.value());
            !s)
            return s.error();
    }
    if ((doc.entities().flags[e] & core::FlagHidden) != 0)
        if (auto s = ctx.transaction().set_entity_hidden(m, true); !s) return s.error();
    return m;
}

} // namespace

core::Result<core::EntityId> copy_into_block(Context& ctx, core::EntityId e, core::BlockId block,
                                             core::Mm dx, core::Mm dy)
{
    return copy_across(ctx, e, block, dx, dy);
}

core::Result<core::EntityId> copy_out_of_block(Context& ctx, core::EntityId member, core::Mm dx,
                                               core::Mm dy)
{
    return copy_across(ctx, member, core::kNoBlock, dx, dy);
}

bool same_as_member(const core::Document& doc, core::EntityId e, core::Mm dx, core::Mm dy,
                    core::EntityId member)
{
    const core::EntityTable& ents = doc.entities();
    if (ents.kind[e] != ents.kind[member] || ents.layer[e] != ents.layer[member] ||
        ents.style[e] != ents.style[member] ||
        (ents.flags[e] & core::FlagHidden) != (ents.flags[member] & core::FlagHidden))
        return false;

    // The rings and the payload, moved back where the member stands.
    auto moved = core::translated_record(doc, e, dx, dy);
    if (!moved) return false;
    const core::RingGeometry& g = doc.geometry();
    const std::uint32_t mine    = ents.slot[member];
    const core::RingSpan span   = g.rings_of(mine);
    if (moved.value().rings.size() != span.count) return false;
    for (std::uint32_t r = 0; r < span.count; ++r) {
        const auto xs                      = g.ring_xs(span.first + r);
        const auto ys                      = g.ring_ys(span.first + r);
        const std::vector<core::Point2>& p = moved.value().rings[r];
        if (p.size() != xs.size() || moved.value().roles[r] != g.ring_role[span.first + r] ||
            moved.value().parts[r] != g.ring_part[span.first + r])
            return false;
        for (std::size_t v = 0; v < p.size(); ++v)
            if (p[v].x != xs[v] || p[v].y != ys[v]) return false;
    }
    const auto bytes = g.payload_of(mine);
    if (!std::equal(bytes.begin(), bytes.end(), moved.value().payload.begin(),
                    moved.value().payload.end()))
        return false;

    // The caption and the cells, which are the member's too.
    const std::uint32_t theirs = ents.slot[e];
    const core::TextTable& t   = doc.texts();
    if (t.has(theirs) != t.has(mine)) return false;
    if (t.has(mine) && (t.text(theirs) != t.text(mine) || t.height(theirs) != t.height(mine) ||
                        t.anchor(theirs) != t.anchor(mine) || !(t.lines(theirs) == t.lines(mine))))
        return false;
    const core::AttrTable& attrs = doc.attributes();
    for (std::size_t c = 0; c < attrs.columns(); ++c) {
        auto a           = attrs.get(static_cast<core::AttrId>(c), theirs);
        auto b           = attrs.get(static_cast<core::AttrId>(c), mine);
        const bool has_a = a && a.value().present;
        const bool has_b = b && b.value().present;
        if (has_a != has_b || (has_a && !(a.value() == b.value()))) return false;
    }
    return true;
}

core::Result<std::vector<std::string>> ensure_field_columns(Context& ctx, core::BlockId block)
{
    std::vector<std::string> declared;
    for (const std::string& name : core::block_fields(ctx.document(), block)) {
        if (ctx.document().attributes().find(name) != core::kNoAttr) continue;
        core::AttrSpec spec;
        spec.id         = name;
        spec.name_tr    = name;
        spec.summary_tr = "Blok alanı: bir blok referansının " + name + " değeri";
        spec.type       = core::AttrType::Text;
        auto made       = ctx.transaction().declare_attribute(std::move(spec));
        if (!made) return made.error();
        declared.push_back(name);
    }
    return declared;
}

core::Result<core::EntityId> place_reference(Context& ctx, core::Point2 at,
                                             core::BlockReference ref)
{
    ref.bounds = core::block_reference_bounds(ctx.document(), at, ref);
    const core::Point2 pts[1]{at};
    const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts, 1),
                                             core::RingRole::Open, 0};
    const std::vector<std::uint8_t> payload = core::encode_block_reference(ref);
    return ctx.transaction().add_kind(ctx.active_layer(), core::kBlockReferenceKind,
                                      std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                      payload);
}

core::Result<std::size_t> refresh_references(Context& ctx, core::BlockId block)
{
    return ctx.transaction().refresh_block_references(block);
}

} // namespace kentos::command
