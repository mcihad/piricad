// SPDX-License-Identifier: GPL-3.0-or-later
// core.layer_visibility — KATMANGÖRÜNÜM
//
// WHAT IT IS FOR. `KATMAN ad=X gorunur=hayır` already hides one layer, and that
// is the right command for one layer. What it cannot say is anything about the
// OTHERS: "show everything", "swap what is showing for what is not", "leave only
// this one up". Those three read the layer table, and reading the layer table is
// not something a menu may do on its own — a convenience that exists only as a
// mouse gesture is a convenience no script and no model can ever use (CLAUDE.md
// 5.15, 2.8). So they are a command, and the panel's `Görünüm` submenu types it.
//
// AND IT LEAVES THE ACTIVE LAYER ALONE, which is the other half of why it exists.
// `KATMAN` makes the layer it touched active, because naming a layer is how a
// user picks one to draw on. Hiding forty layers is not picking one to draw on,
// and the last of the forty is certainly not the choice.
//
// SEVERAL LAYERS AT ONCE are several invocations inside one batch, which is one
// undo step (`Bus::begin_batch`) — the same road a script takes and therefore no
// privilege for the window (ui.md P3). One argument names one layer here because
// the command model has no list-of-text value; adding one would be a change to
// the journal's value shapes, which is a bigger thing than this menu.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// What the verb does to the table.
enum class Verb : std::uint8_t {
    Show,   ///< the named layer, up
    Hide,   ///< the named layer, down
    Only,   ///< the named layer up and every other one down
    All,    ///< every layer up
    Invert, ///< every layer swapped, or just the named one
};

struct Operation
{
    const char* name;
    Verb verb;
    bool needs_layer; ///< refused without `katman`
    bool takes_layer; ///< `katman` is read when given
};

// The words are ASCII-folded lowercase, like every other `islem` in this program
// (`YAZDIRMAPROFİLİ islem=ekle`), and matched with Turkish folding, so the
// dotless-i spelling `yalnız` reaches the same operation.
constexpr Operation kOperations[] = {
    {"goster", Verb::Show, true, true},     {"gizle", Verb::Hide, true, true},
    {"yalniz", Verb::Only, true, true},     {"tumu", Verb::All, false, false},
    {"tersine", Verb::Invert, false, true},
};

std::string operation_list()
{
    std::string out;
    for (const Operation& op : kOperations)
        out += (out.empty() ? "" : " / ") + std::string(op.name);
    return out;
}

std::string layer_count(std::size_t n)
{
    return std::to_string(n) + " katman";
}

Task<void> run(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    auto typed = co_await ctx.text("islem", "İşlem: " + operation_list());
    if (!typed) co_return;

    const Operation* op = nullptr;
    for (const Operation& candidate : kOperations)
        if (core::turkish_key_equals(*typed, candidate.name)) op = &candidate;
    if (op == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *typed + "'. İşlemler: " + operation_list()));
        co_return;
    }
    ctx.record("islem", Value::text(op->name));

    // WHICH LAYER, for the verbs that name one. `tumu` REFUSES a layer rather
    // than ignoring it: an argument that is quietly dropped is an argument the
    // user believed in (command.md P15).
    const Value given = ctx.argument("katman");
    if (!given.empty() && !op->takes_layer) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'" + std::string(op->name) +
                                         "' bütün katmanlara bakar; katman= almaz. Bir katmanı "
                                         "göstermek için islem=goster katman=<ad> yazın."));
        co_return;
    }

    core::LayerId named = core::kNoLayer;
    if (op->needs_layer || !given.empty()) {
        auto name = co_await ctx.text("katman", "Katman adı");
        if (!name || name->empty()) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "'" + std::string(op->name) + "' için katman adı gerekir: katman=<ad>"));
            co_return;
        }
        named = bus.document().find_layer(*name);
        if (named == core::kNoLayer) {
            ctx.session().fail(
                core::err(core::ErrorCode::NotFound, "Katman yok: '" + *name + "'."));
            co_return;
        }
        ctx.record("katman", Value::text(*name));
    }

    // READ FIRST, THEN WRITE. `set_layer_visible` walks the entities of the layer
    // it changes to mirror the flag, so the table is being written while it is
    // being read; the snapshot keeps `tersine` from flipping a layer it has
    // already flipped.
    const std::vector<core::Layer>& layers = bus.document().layers();
    std::vector<std::uint8_t> was;
    was.reserve(layers.size());
    for (const core::Layer& l : layers)
        was.push_back(l.visible ? 1U : 0U);

    std::size_t changed = 0;
    bool failed         = false;
    const auto set      = [&](core::LayerId slot, bool on) {
        if (failed || was[slot] == (on ? 1U : 0U)) return;
        if (auto st = ctx.transaction().set_layer_visible(slot, on); !st) {
            ctx.session().fail(st.error());
            failed = true;
            return;
        }
        ++changed;
    };

    const auto every = [&](auto&& want) {
        for (std::size_t i = 0; i < was.size(); ++i)
            set(static_cast<core::LayerId>(i), want(static_cast<core::LayerId>(i)));
    };

    switch (op->verb) {
    case Verb::Show: set(named, true); break;
    case Verb::Hide: set(named, false); break;
    case Verb::Only: every([named](core::LayerId slot) { return slot == named; }); break;
    case Verb::All: every([](core::LayerId) { return true; }); break;
    case Verb::Invert:
        if (named != core::kNoLayer)
            set(named, was[named] == 0U);
        else
            every([&was](core::LayerId slot) { return was[slot] == 0U; });
        break;
    }
    if (failed) co_return;

    const std::string name = named != core::kNoLayer ? bus.document().layer(named)->name : "";
    switch (op->verb) {
    case Verb::Show: ctx.echo(changed == 0 ? name + " zaten görünür." : name + " görünür."); break;
    case Verb::Hide: ctx.echo(changed == 0 ? name + " zaten gizli." : name + " gizlendi."); break;
    case Verb::Only:
        ctx.echo("Yalnız " + name + " görünür; " + layer_count(changed) + " değişti.");
        break;
    case Verb::All:
        ctx.echo(changed == 0 ? "Bütün katmanlar zaten görünür."
                              : layer_count(changed) + " gösterildi; artık hepsi görünür.");
        break;
    case Verb::Invert: ctx.echo("Görünürlük ters çevrildi: " + layer_count(changed) + "."); break;
    }
}

} // namespace

KENTOS_COMMAND(layer_visibility)
{
    return CommandSpec{
        .id       = "core.layer_visibility",
        .names    = {"KATMANGÖRÜNÜM", "KATMANGORUNUM", "LAYERVIEW", "KGÖ", "KGO"},
        .category = Category::Layer,
        .params =
            {
                Param::text("islem", Arity::exactly(1),
                            "goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) "
                            "ya da tersine"),
                Param::text("katman", Arity::optional(),
                            "Katman adı; goster, gizle ve yalniz için gerekir, tersine için "
                            "isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        // `görünürlüğü` and not `gösterimi`: the second word is the MPYY term for a
        // symbology row and `scripts/ci-gate-hardcoded-thresholds.sh` guards it
        // (CLAUDE.md 5.13). The menu entry a user reads still says what the user
        // said, annotated `ui-label` on its own line (panels.cpp).
        .summary = "Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da "
                   "gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters "
                   "çevirir.",
        .run = &run,
    };
}

} // namespace kentos::command
