// SPDX-License-Identifier: GPL-3.0-or-later
// core.set_layer (KATMANAT), core.match_style (STİLKOPYALA)
//
// The two housekeeping edits a drawing needs constantly and no amount of careful
// drawing avoids: something was drawn on the wrong layer, and something should
// look like the thing beside it.
//
// Both keep IDENTITY. An object moved to another layer is the same object with
// the same key and the same attributes — moving a parsel from PARSEL_TASLAK to
// PARSEL must not mint a new ada/parsel row (model.md R4, R28).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The entities a command works on: the named ones, the selection, or the ones
/// the user is asked to point at.
Task<bool> gather(Context& ctx, std::vector<std::int64_t>& requested,
                  std::vector<core::EntityId>& slots, const char* example)
{

    // The argument, the selection, or ASKED FOR — see `want_objects`.
    if (!co_await want_objects(ctx, "nesneler", "Nesneleri seçin, sonra Enter", requested, 0,
                               example))
        co_return false;

    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            co_return false;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return false;
        }
        slots.push_back(slot);
    }
    co_return true;
}

/// How far `probe` is from the nearest edge of `slot`, in millimetres, or -1 when
/// the entity has no edge to measure to.
///
/// Every ring, open or closed, so an area answers by its boundary and a line by
/// itself — "the one I clicked on" has to mean the same thing for both.
double distance_to(const core::Document& doc, core::EntityId slot, core::Point2 probe)
{
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

    double best = -1.0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        if (xs.size() < 2) continue;

        const bool closed      = geom.ring_role[r] != core::RingRole::Open;
        const std::size_t last = closed ? xs.size() : xs.size() - 1;
        for (std::size_t i = 0; i < last; ++i) {
            const std::size_t j  = (i + 1) % xs.size();
            const core::Point2 f = core::closest_point_on_segment(
                core::Point2{xs[i], ys[i]}, core::Point2{xs[j], ys[j]}, probe);
            const double d = core::distance_squared(f, probe);
            if (best < 0.0 || d < best) best = d;
        }
    }
    return best;
}

Task<void> run_set_layer(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "KATMANAT nesneler=1 katman=PARSEL")) co_return;

    auto name = co_await ctx.text("katman", "Taşınacak katmanın adı");
    if (!name) co_return;

    if (name->empty()) {
        ctx.echo("Katman adı boş olamaz.");
        co_return;
    }

    // CREATED IF ABSENT, exactly as drawing on a new layer creates it: a command
    // that refused an unknown name would make the user run KATMAN first for no
    // reason the drawing cares about.
    const core::LayerId layer = ctx.transaction().ensure_layer(*name);
    if (layer == core::kNoLayer) {
        ctx.echo("Katman oluşturulamadı: " + *name);
        co_return;
    }

    for (core::EntityId slot : slots) {
        auto st = ctx.transaction().set_entity_layer(slot, layer);
        if (!st) {
            ctx.echo(st.error().message);
            co_return; // the bus rolls the whole transaction back
        }
    }

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("katman", Value::text(*name));
    ctx.echo(std::to_string(slots.size()) + " nesne '" + *name + "' katmanına taşındı.");
}

Task<void> run_match_style(Context& ctx)
{
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> slots;
    if (!co_await gather(ctx, requested, slots, "STİLKOPYALA kaynak=1 nesneler=2 nesneler=3"))
        co_return;

    Value source_arg = ctx.argument("kaynak");
    if (source_arg.empty()) {
        // THE CLICK SAYS WHICH ONE IS THE SOURCE.
        //
        // Without this the tool column's "Stil Kopyala" button could not work at
        // all: it sends the bare command, so `kaynak` was always empty and the
        // only outcome was a refusal — or, before the parameter was made optional,
        // the raw "zorunlu 'kaynak' parametresi eksik" from post-run validation.
        //
        // Taking the first selected object instead would mean "whichever was drawn
        // first": `Selection::keys()` is sorted by key, not by the order the user
        // clicked, so it is not something the user said. Clicking the object to
        // copy FROM is the gesture every drawing program already trains.
        if (slots.size() < 2) {
            ctx.echo("Stili kopyalanacak kaynak nesne belirtilmedi. Kaynağı ve hedefleri "
                     "birlikte seçin ya da STİLKOPYALA kaynak=1 nesneler=2 yazın.");
            co_return;
        }

        auto at = co_await ctx.point("nokta", "Stili kopyalanacak KAYNAK nesneye tıklayın");
        if (!at) co_return;

        std::size_t nearest = 0;
        double best         = -1.0;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const double d = distance_to(ctx.document(), slots[i], *at);
            if (d >= 0.0 && (best < 0.0 || d < best)) {
                best    = d;
                nearest = i;
            }
        }
        if (best < 0.0) {
            ctx.echo("Seçili nesnelerin hiçbirinin kenarı yok; kaynak belirlenemedi.");
            co_return;
        }

        // RECORDED AS THE ID IT RESOLVED, so a replay copies from the same object
        // whatever the selection holds then (model.md R43).
        source_arg = Value::ids({requested[nearest]});
        ctx.record("nokta", Value::point(*at));

        // The source is not one of its own targets.
        requested.erase(requested.begin() + static_cast<std::ptrdiff_t>(nearest));
        slots.erase(slots.begin() + static_cast<std::ptrdiff_t>(nearest));
    }

    const std::int64_t source_id =
        source_arg.kind() == Value::Kind::IdList
            ? (source_arg.as_ids().size() == 1 ? source_arg.as_ids()[0] : 0)
            : source_arg.as_int();

    if (source_id <= 0) {
        ctx.echo("Geçersiz kaynak kimliği: " + std::to_string(source_id) +
                 ". Kimlikler 1'den başlar.");
        co_return;
    }

    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(source_id));
    const core::EntityId source = ctx.document().slot_of(key);
    if (source == core::kNoEntity || !ctx.document().alive(source)) {
        ctx.echo("Kaynak nesne bulunamadı veya silinmiş: " + std::to_string(source_id));
        co_return;
    }

    // THE EFFECTIVE STYLE, not the column. A source that inherits its layer's look
    // carries `kByLayerStyle`, and copying that sentinel onto an object on a
    // DIFFERENT layer would leave it looking like ITS own layer — which is not
    // what "make this look like that" means.
    //
    // So the inheritance is resolved: a layer that carries a whole symbol stack
    // hands over its `style`, and a layer that only carries colours and widths has
    // its `appearance` interned into one. Interning is additive and hands out an
    // id that stays valid for the document's lifetime (transaction.hpp), so the
    // copy cannot go stale.
    core::StyleId style = ctx.document().entities().style[source];
    if (style == core::kByLayerStyle) {
        const core::LayerId layer = ctx.document().entities().layer[source];
        if (const core::Layer* l = ctx.document().layer_table().at(layer); l != nullptr)
            style = l->style != core::kByLayerStyle ? l->style
                                                    : ctx.transaction().intern_style(l->appearance);
    }

    std::size_t changed = 0;
    for (core::EntityId slot : slots) {
        if (slot == source) continue; // copying onto itself changes nothing

        auto st = ctx.transaction().set_entity_style(slot, style);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++changed;
    }

    ctx.record("kaynak", source_arg);
    ctx.record("nesneler", Value::ids(requested));
    ctx.echo(std::to_string(changed) + " nesne kaynağın stilini aldı.");
}

} // namespace

KENTOS_COMMAND(set_layer)
{
    return CommandSpec{
        .id       = "core.set_layer",
        .names    = {"KATMANAT", "KATMANATA", "SETLAYER", "KA"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"},
                Param::text("katman", Arity::exactly(1), "Hedef katmanın adı; yoksa oluşturulur"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesneleri başka bir katmana taşır.",
        .run     = &run_set_layer,
    };
}

KENTOS_COMMAND(match_style)
{
    return CommandSpec{
        .id       = "core.match_style",
        .names    = {"STİLKOPYALA", "STILKOPYALA", "MATCHPROP", "SK"},
        .category = Category::Modify,
        .params =
            {
                Param{"kaynak", ParamKind::Selection, Arity::optional(),
                      "Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne"},
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Stili alacak nesnelerin kimlikleri; yoksa etkin seçim"},
                // OPTIONAL: only asked for when `kaynak` was not given, which is
                // the tool-column road. A script that names its source never sees
                // this prompt and must not be required to answer it.
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde"},
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesnenin stilini seçilen nesnelere uygular.",
        .run     = &run_match_style,
    };
}

} // namespace kentos::command
