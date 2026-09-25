// SPDX-License-Identifier: GPL-3.0-or-later
// core.local_copy (YERELKOPYA) — an editable copy of what a linked file holds
// (TODOS F-02).
//
// A LINKED FILE IS READ HERE AND EDITED THERE. An external reference's objects
// come from their own file and are read again on every open and reload
// (model.md R45a), so an edit made to one here would be gone on the next —
// which is why every edit refuses them. What a user who wants to change one
// needs is a COPY that is this drawing's own: taken out where the reference
// draws it, with its values and its look, onto a layer of this drawing, and
// the link left exactly as it was. That is this command, and it is what the
// refusal of such an edit offers (`core::Error::remedy`), to every client alike.
//
// THE LAYER IS THE SOURCE'S OWN NAME. A reference's layers arrive as
// `NAME|LAYER` (`command::external_prefix`); its copies land on `LAYER` — the
// layer the source calls them by, made when this drawing has none — unless
// `katman=` names another.
//
// EACH COPY KNOWS WHERE IT CAME FROM (core/lineage.hpp): the reference it was
// taken out of, by key — a member's own key is read again with every reload,
// the reference's is this drawing's and stays.
#include "kentos_cad/command/block_edit.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/transform.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One external reference placed on the sheet, and what it is asked to give up.
struct Taking
{
    core::EntityId reference{core::kNoEntity}; ///< the placement that draws them
    core::BlockId block{core::kNoBlock};       ///< its definition
    std::vector<core::EntityId> only;          ///< its members to copy; empty = all of them
};

/// The first live placement of `block` on the sheet, or `kNoEntity`.
core::EntityId placement_of(const core::Document& doc, core::BlockId block)
{
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.entities().standalone(e) || doc.entities().kind[e] != core::kBlockReferenceKind)
            continue;
        auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[e]);
        if (ref && ref.value().block == block) return e;
    }
    return core::kNoEntity;
}

/// The external definition `member` belongs to, or `kNoBlock`.
core::BlockId definition_of(const core::Document& doc, core::EntityId member)
{
    const core::EntityKey key = doc.entities().key[member];
    for (core::BlockId b = 0; b < doc.blocks().size(); ++b) {
        const core::BlockDef& def = doc.blocks().at(b);
        if (def.external() && std::ranges::find(def.members, key) != def.members.end()) return b;
    }
    return core::kNoBlock;
}

/// `layer` without the reference's `NAME|` in front of it.
std::string local_name(const std::string& layer, const std::string& prefix)
{
    return !prefix.empty() && layer.starts_with(prefix) ? layer.substr(prefix.size()) : layer;
}

/// Whether the box of piece `e` meets the window `w`.
bool meets(const core::Document& doc, core::EntityId e, const core::Box2& w)
{
    const core::Box2 b = doc.entities().box_of(e);
    return b.max_x >= w.min_x && b.min_x <= w.max_x && b.max_y >= w.min_y && b.min_y <= w.max_y;
}

Task<void> run_local_copy(Context& ctx)
{
    // BY NAME, or by pointing: a script knows the reference by the name it
    // linked it under, a user by where it is on the sheet.
    const Value by_name         = ctx.argument("ad");
    const std::string named_ref = by_name.empty() ? std::string() : std::string(by_name.as_text());
    std::vector<std::int64_t> chosen;
    if (named_ref.empty()) {
        if (!co_await want_objects(ctx, "nesneler",
                                   "Kopyası alınacak dış referansı seçin, Enter'a basın", chosen, 0,
                                   "YERELKOPYA nesneler=1"))
            co_return;
        if (chosen.empty()) co_return;
    }

    const core::Document& doc = ctx.document();

    // ---- what is asked for, by the reference that draws it ----
    std::map<core::EntityId, Taking> takings; // by reference
    if (!named_ref.empty()) {
        const core::BlockId b = doc.blocks().find(named_ref);
        if (b == core::kNoBlock || !is_external_block(doc, b)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "'" + named_ref + "' adında bir dış referans yok.");
            co_return;
        }
        const core::EntityId placed = placement_of(doc, b);
        if (placed == core::kNoEntity) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "'" + named_ref +
                           "' dış referansı çizimde hiçbir yere yerleşmemiş; kopyanın konacağı "
                           "yer yok.");
            co_return;
        }
        takings[placed] = Taking{placed, b, {}};
    }
    for (const std::int64_t id : chosen) {
        const auto key         = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId e = doc.slot_of(key);
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
            co_return;
        }
        if (doc.entities().kind[e] == core::kBlockReferenceKind && doc.entities().standalone(e)) {
            auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[e]);
            if (!ref || !is_external_block(doc, ref.value().block)) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Nesne " + std::to_string(id) +
                               " bir dış referans değil; bu çizimin kendi nesnesi zaten "
                               "düzenlenebilir. Bir kopyası için KOPYALA kullanın.");
                co_return;
            }
            Taking& t   = takings[e];
            t.reference = e;
            t.block     = ref.value().block;
            t.only.clear(); // the whole reference wins over any of its members
            continue;
        }
        const core::BlockId b = definition_of(doc, e);
        if (b == core::kNoBlock) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne " + std::to_string(id) +
                           " bir dış referansın parçası değil; bu çizimin kendi nesnesi zaten "
                           "düzenlenebilir. Bir kopyası için KOPYALA kullanın.");
            co_return;
        }
        const core::EntityId placed = placement_of(doc, b);
        if (placed == core::kNoEntity) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "'" + doc.blocks().at(b).name +
                           "' dış referansı çizimde hiçbir yere yerleşmemiş; kopyanın konacağı "
                           "yer yok.");
            co_return;
        }
        auto [it, fresh] = takings.try_emplace(placed, Taking{placed, b, {}});
        if (fresh || !it->second.only.empty()) it->second.only.push_back(e);
    }

    // ---- where the copies go ----
    const Value named        = ctx.argument("katman");
    const std::string forced = named.empty() ? std::string() : std::string(named.as_text());
    core::Box2 window{};
    const bool windowed = [&] {
        const Value w = ctx.argument("pencere");
        if (w.empty() || w.as_points().size() < 2) return false;
        const core::Point2 a = w.as_points()[0];
        const core::Point2 b = w.as_points()[1];
        window = core::Box2{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
                            std::max(a.y, b.y)};
        return true;
    }();

    std::size_t copied = 0;
    std::size_t nested = 0; // inner references to the file's own blocks, left behind
    std::map<std::string, std::size_t> onto;
    core::Json rows = core::Json::array({});
    for (const auto& [reference, taking] : takings) {
        const core::BlockDef def = doc.blocks().at(taking.block); // copied: the table may grow
        const std::string prefix = external_prefix(def);
        auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[reference]);
        if (!ref) {
            ctx.refuse(ref.error());
            co_return;
        }
        const core::BlockReference& placed = ref.value();
        const core::Point2 insertion =
            core::block_reference_insertion(doc.geometry(), doc.entities().slot[reference]);

        std::vector<core::EntityId> members;
        if (taking.only.empty()) {
            for (const core::EntityKey k : def.members)
                if (const core::EntityId m = doc.slot_of(k); m != core::kNoEntity && doc.alive(m))
                    members.push_back(m);
        } else {
            members = taking.only;
        }

        core::Json made              = core::Json::array({});
        const std::array<core::EntityKey, 1> from{doc.entities().key[reference]};
        for (int row = 0; row < static_cast<int>(placed.rows); ++row)
            for (int column = 0; column < static_cast<int>(placed.columns); ++column) {
                const core::Xform x =
                    core::block_placement(placed, insertion, def.base, column, row);
                for (const core::EntityId member : members) {
                    // A reference to one of the FILE's own blocks is the file's
                    // too: a copy of it would draw a definition this drawing
                    // does not keep. It is counted and left.
                    if (doc.entities().kind[member] == core::kBlockReferenceKind) {
                        auto inner =
                            core::block_reference_of(doc.geometry(), doc.entities().slot[member]);
                        if (!inner || is_external_block(doc, inner.value().block)) {
                            ++nested;
                            continue;
                        }
                    }
                    auto piece = place_member(ctx, member, x, reference);
                    if (!piece) {
                        ctx.refuse(piece.error());
                        co_return;
                    }
                    const core::EntityId p = piece.value().piece;
                    if (windowed && !meets(ctx.document(), p, window)) {
                        if (auto st = ctx.transaction().erase_entity(p); !st) {
                            ctx.refuse(st.error());
                            co_return;
                        }
                        continue;
                    }
                    // ONTO THIS DRAWING'S LAYER: the named one, or the one the
                    // source calls it by.
                    const core::Layer* home =
                        ctx.document().layer(ctx.document().entities().layer[p]);
                    const std::string want =
                        !forced.empty() ? forced
                                        : local_name(home != nullptr ? home->name : "0", prefix);
                    const core::LayerId target = ctx.transaction().ensure_layer(want);
                    if (target == core::kNoLayer) {
                        ctx.refuse(core::ErrorCode::InvalidArgument,
                                   "Katman oluşturulamadı: " + want);
                        co_return;
                    }
                    if (target != ctx.document().entities().layer[p])
                        if (auto st = ctx.transaction().set_entity_layer(p, target); !st) {
                            ctx.refuse(st.error());
                            co_return;
                        }
                    if (auto st = ctx.derive(p, std::span<const core::EntityKey>(from)); !st) {
                        ctx.refuse(st.error());
                        co_return;
                    }
                    made.push(core::Json::integer(
                        static_cast<std::int64_t>(core::raw(ctx.document().key_of(p)))));
                    ++onto[want];
                    ++copied;
                }
            }
        core::Json one = core::Json::object({});
        one.set("referans",
                core::Json::integer(static_cast<std::int64_t>(core::raw(doc.key_of(reference)))));
        one.set("ad", core::Json::string(def.name));
        one.set("kopyalar", std::move(made));
        rows.push(std::move(one));
    }

    if (copied == 0) {
        ctx.refuse(core::ErrorCode::NotFound,
                   windowed ? "Pencereye değen bir nesne yok; hiçbir şey kopyalanmadı."
                            : "Kopyalanacak nesne yok.");
        co_return;
    }

    if (!named_ref.empty())
        ctx.record("ad", Value::text(named_ref));
    else
        ctx.record("nesneler", Value::ids(chosen));
    if (!forced.empty()) ctx.record("katman", Value::text(forced));
    if (windowed) ctx.record("pencere", ctx.argument("pencere"));

    std::string layers;
    for (const auto& [name, count] : onto)
        layers += (layers.empty() ? "" : ", ") + name + " (" + std::to_string(count) + ")";
    std::string said = "Yerel kopya: " + std::to_string(copied) + " nesne " + layers +
                       " katmanına alındı; bağlantı yerinde, kopyalar bu çizimin.";
    if (nested > 0)
        said += " Dosyanın kendi bloğuna " + std::to_string(nested) +
                " iç referans alınmadı; onlar için referansı DIŞREFERANS islem=bagla ile "
                "çizime bağlayın.";
    ctx.echo(said);

    core::Json report = core::Json::object({});
    report.set("adet", core::Json::integer(static_cast<std::int64_t>(copied)));
    report.set("referanslar", std::move(rows));
    if (nested > 0)
        report.set("alinmayan_ic_referans", core::Json::integer(static_cast<std::int64_t>(nested)));
    ctx.report(std::move(report));
}

} // namespace

KENTOS_COMMAND(local_copy)
{
    return CommandSpec{
        .id       = "core.local_copy",
        .names    = {"YERELKOPYA", "LOCALCOPY", "YK"},
        .title    = "Yerel Kopya",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kopyası alınacak dış referans ya da onun nesneleri"}
                    .en("objects"),
                Param::text("ad", Arity::optional(),
                            "Kopyası alınacak dış referansın adı; nesneler yerine")
                    .en("name"),
                Param::text("katman", Arity::optional(),
                            "Kopyaların katmanı; yoksa her biri kaynağındaki katmanın adıyla")
                    .en("layer"),
                // NONE OR TWO: a window is its two corners, or there is none.
                Param::points("pencere", Arity{0, 2},
                              "Yalnız bu dikdörtgene değen nesneler: iki köşe")
                    .en("window"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir dış referanstaki nesnelerin düzenlenebilir kopyalarını bu çizime alır; "
                   "bağlantı olduğu gibi kalır.",
        .run    = &run_local_copy,
        .effect = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
