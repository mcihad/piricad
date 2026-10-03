// SPDX-License-Identifier: GPL-3.0-or-later
// core.layer_state — KATMANDURUM
//
// WHAT IT IS FOR (TODOS U-05, "kayıtlı çalışma alanları"). A drawing is worked in several ways: the
// surveyed points alone, the plan for review, the same plan as it prints. Moving between them by
// toggling forty layers is the work this command takes away: a STATE is a named picture of which
// layers are shown, locked, printed and picked, saved in the drawing (so a colleague finds it),
// applied in one step and undone in one.
//
//   KATMANDURUM                          the saved states
//   KATMANDURUM islem=kaydet ad=PLAN     save what every layer is now
//   KATMANDURUM islem=uygula ad=PLAN     put every layer back as the state had it
//   KATMANDURUM islem=sil ad=PLAN        forget it
//
// A STATE NAMES LAYERS BY KEY (core/layer_state.hpp): a layer renamed since is still the layer the
// state meant, a layer deleted since is SAID TO BE SKIPPED, and a layer made since is left as it
// is. It does not touch the active layer, the colours or the scale windows — those are a layer's
// look and its place in the map, not which of them are in the way.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/text.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

core::LayerState snapshot(const core::Document& doc, std::string name)
{
    core::LayerState state;
    state.name = std::move(name);
    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const core::Layer& l = doc.layers()[i];
        state.rows.push_back(
            core::LayerStateRow{l.key, l.visible, l.locked, l.plottable, l.selectable});
    }
    return state;
}

Task<void> run(Context& ctx)
{
    const core::Document& doc = ctx.document();

    const Value verb  = ctx.argument("islem");
    const Value named = ctx.argument("ad");

    // The verb, matched Turkish-folded against the four words; `word` is the canonical spelling the
    // journal keeps, whatever case or dotted letter was typed.
    std::string word;
    if (verb.empty()) {
        word = "liste";
    } else {
        for (const char* candidate : {"liste", "kaydet", "uygula", "sil"})
            if (core::turkish_key_equals(verb.as_text(), candidate)) word = candidate;
    }

    if (word == "liste") {
        const auto& states = doc.layer_states().all();
        if (states.empty()) {
            ctx.echo("Kayıtlı katman durumu yok. KATMANDURUM islem=kaydet ad=<ad> ile kaydedin.");
            co_return;
        }
        ctx.echo(std::to_string(states.size()) + " kayıtlı katman durumu:");
        for (const core::LayerState& s : states) {
            std::size_t shown = 0;
            for (const core::LayerStateRow& r : s.rows)
                if (r.visible) ++shown;
            ctx.echo("    " + s.name + "  — " + std::to_string(s.rows.size()) + " katman, " +
                     std::to_string(shown) + " görünür");
        }
        co_return;
    }

    if (word.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tanınmayan işlem: '" + verb.as_text() +
                       "'. İşlemler: liste / kaydet / uygula / sil");
        co_return;
    }
    if (named.empty() || named.as_text().empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + word + "' için durumun adı gerekir: ad=<ad>");
        co_return;
    }
    const std::string name = named.as_text();
    ctx.record("islem", Value::text(word));
    ctx.record("ad", Value::text(name));

    std::vector<core::LayerState> list = doc.layer_states().all();

    if (word == "kaydet") {
        const bool had = doc.layer_states().find(name) != nullptr;
        core::LayerStateStore next;
        next.load(std::move(list));
        if (auto st = next.upsert(snapshot(doc, name)); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        if (auto st = ctx.transaction().set_layer_states(next.all()); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.echo("'" + name + "' katman durumu " + (had ? "yeniden kaydedildi" : "kaydedildi") +
                 ": " + std::to_string(doc.layers().size()) + " katman.");
        co_return;
    }

    const core::LayerState* found = doc.layer_states().find(name);
    if (found == nullptr) {
        std::string known;
        for (const core::LayerState& s : doc.layer_states().all())
            known += (known.empty() ? "" : ", ") + s.name;
        ctx.refuse(core::ErrorCode::NotFound,
                   "Katman durumu yok: '" + name + "'." +
                       (known.empty() ? " Hiç kayıtlı durum yok." : " Kayıtlılar: " + known + "."));
        co_return;
    }

    if (word == "sil") {
        core::LayerStateStore next;
        next.load(std::move(list));
        next.remove(name);
        if (auto st = ctx.transaction().set_layer_states(next.all()); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.echo("'" + name + "' katman durumu silindi.");
        co_return;
    }

    // APPLY. Each row finds its layer by KEY. A row whose layer is gone is skipped and counted,
    // never matched to whatever now sits at its old slot; a layer the state does not know is left
    // alone. Everything lands in this one transaction, so the whole change is one undo step.
    std::size_t changed = 0;
    std::size_t missing = 0;
    for (const core::LayerStateRow& row : found->rows) {
        const core::LayerId slot = doc.layer_slot_of(row.key);
        if (slot == core::kNoLayer) {
            ++missing;
            continue;
        }
        const core::Layer* now = doc.layer(slot);
        bool touched           = false;
        if (now->visible != row.visible) {
            if (auto st = ctx.transaction().set_layer_visible(slot, row.visible); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            touched = true;
        }
        if (now->locked != row.locked) {
            if (auto st = ctx.transaction().set_layer_locked(slot, row.locked); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            touched = true;
        }
        if (now->plottable != row.plottable || now->selectable != row.selectable) {
            core::LayerProps props = now->props();
            props.plottable        = row.plottable;
            props.selectable       = row.selectable;
            if (auto st = ctx.transaction().set_layer_props(slot, props); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            touched = true;
        }
        if (touched) ++changed;
    }

    std::string said =
        "'" + name + "' katman durumu uygulandı: " + std::to_string(changed) + " katman değişti";
    if (missing != 0) said += "; " + std::to_string(missing) + " katman artık çizimde yok, atlandı";
    ctx.echo(said + ".");
}

} // namespace

PIRICAD_COMMAND(layer_state)
{
    return CommandSpec{
        .id       = "core.layer_state",
        .names    = {"KATMANDURUM", "KATMANDURUMU", "LAYERSTATE", "KDR"},
        .title    = "Katman Durumu",
        .category = Category::Layer,
        .params =
            {
                Param::text("islem", Arity::optional(),
                            "liste (varsayılan), kaydet, uygula ya da sil")
                    .en("action"),
                Param::text("ad", Arity::optional(),
                            "Durumun adı; kaydet, uygula ve sil için gerekir")
                    .en("name"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Hangi katmanların görünür, kilitli, basılır ve seçilir olduğunu adla kaydeder, "
                   "tek adımda uygular ve siler.",
        .run     = &run,
    };
}

} // namespace piricad::command
