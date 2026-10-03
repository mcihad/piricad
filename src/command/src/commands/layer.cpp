// SPDX-License-Identifier: GPL-3.0-or-later
// core.layer — KATMAN
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

namespace piricad::command {
namespace {

Task<void> run(Context& ctx)
{
    // THE LAYERS THIS DRAWING HAS. A prompt for a name is unanswerable by a
    // mouse; the shell offers what the command names (`Prompt::choices`).
    std::vector<std::string> known;
    for (const core::Layer& l : ctx.document().layers())
        known.push_back(l.name);
    auto name = co_await ctx.text("ad", "Katman adı", known);
    if (!name || name->empty()) co_return;

    Bus& bus = ctx.session().bus();

    // Creating an empty layer is inert and would invalidate stored ids if undone,
    // so it is deliberately not an undo step (see .claude/core.md).
    const core::LayerId existing = bus.document().find_layer(*name);
    const core::LayerId id =
        existing != core::kNoLayer ? existing : ctx.transaction().ensure_layer(*name);

    if (const Value v = ctx.argument("grup"); !v.empty()) {
        auto st = ctx.transaction().set_layer_group(id, v.as_text());
        if (!st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.record("grup", v);
    }

    if (const Value v = ctx.argument("gorunur"); !v.empty()) {
        auto st = ctx.transaction().set_layer_visible(id, v.as_bool());
        if (!st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.record("gorunur", v);
    }

    if (const Value v = ctx.argument("kilitli"); !v.empty()) {
        auto st = ctx.transaction().set_layer_locked(id, v.as_bool());
        if (!st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.record("kilitli", v);
    }

    if (const Value v = ctx.argument("renk"); !v.empty()) {
        core::Appearance appearance = bus.document().layer(id)->appearance;
        appearance.rgba =
            static_cast<std::uint32_t>(v.as_int(static_cast<std::int64_t>(appearance.rgba)));
        auto st = ctx.transaction().set_layer_appearance(id, appearance);
        if (!st) {
            ctx.refuse(st.error());
            co_return;
        }
        ctx.record("renk", v);
    }

    // THE PLAIN PROPERTIES, together: printed or not, picked or not, the scale window, opacity and
    // the description (TODOS U-05). One write and one undo record whichever of them were named, and
    // only the named ones change — the rest of the layer's values are read and written back as they
    // are.
    {
        core::LayerProps props = bus.document().layer(id)->props();
        bool given             = false;
        if (const Value v = ctx.argument("basilir"); !v.empty()) {
            props.plottable = v.as_bool();
            given           = true;
        }
        if (const Value v = ctx.argument("secilebilir"); !v.empty()) {
            props.selectable = v.as_bool();
            given            = true;
        }
        if (const Value v = ctx.argument("en_kucuk_olcek"); !v.empty()) {
            props.min_scale = static_cast<core::ScaleDenominator>(v.as_int());
            given           = true;
        }
        if (const Value v = ctx.argument("en_buyuk_olcek"); !v.empty()) {
            props.max_scale = static_cast<core::ScaleDenominator>(v.as_int());
            given           = true;
        }
        if (const Value v = ctx.argument("opaklik"); !v.empty()) {
            props.opacity = static_cast<std::uint8_t>(v.as_int());
            given         = true;
        }
        if (const Value v = ctx.argument("aciklama"); !v.empty()) {
            props.description = v.as_text();
            given             = true;
        }
        // VIEW OR WORKING COPY (TODOS G-02): one switch, and the lock follows it in the same step.
        if (const Value v = ctx.argument("salt"); !v.empty()) {
            props.viewonly = v.as_bool();
            given          = true;
        }
        if (given) {
            auto st = ctx.transaction().set_layer_props(id, props);
            if (!st) {
                ctx.refuse(st.error());
                co_return;
            }
            for (const char* named : {"basilir", "secilebilir", "en_kucuk_olcek", "en_buyuk_olcek",
                                      "opaklik", "aciklama", "salt"})
                if (const Value v = ctx.argument(named); !v.empty()) ctx.record(named, v);
        }
    }

    bus.set_active_layer(id);
    ctx.echo("Aktif katman: " + *name);
}

} // namespace

PIRICAD_COMMAND(layer)
{
    return CommandSpec{
        .id       = "core.layer",
        .names    = {"KATMAN", "LAYER", "KAT"},
        .known_as = {{"TABAKA", "Netcad"}}, // a layer may be called that: never a name
        .title    = "Katman",
        .category = Category::Layer,
        .params =
            {
                Param::text("ad", Arity::exactly(1),
                            "Katman adı; yoksa oluşturulur ve aktif yapılır")
                    .en("name"),
                Param::text("grup", Arity::optional(),
                            "Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök")
                    .en("group"),
                Param::boolean("gorunur", Arity::optional(), "Katmanın görünürlüğü").en("visible"),
                Param::boolean("kilitli", Arity::optional(), "Katmanın kilit durumu").en("locked"),
                Param::integer("renk", Arity::optional(), "Çizim rengi, 0xAARRGGBB").en("color"),
                Param::boolean("basilir", Arity::optional(),
                               "Paftaya basılsın mı; hayır = ekranda çizilir, çıktıda yoktur")
                    .en("plottable"),
                Param::boolean(
                    "secilebilir", Arity::optional(),
                    "Seçim bu katmanın nesnelerini alsın mı; hayır = çizilir ve "
                    "yakalanır ama seçilmez (kilitten ayrıdır: kilit düzenlemeyi engeller)")
                    .en("selectable"),
                Param::integer_range("en_kucuk_olcek", Arity::optional(), 0, 100'000'000,
                                     "Görünür olduğu en küçük ölçeğin 1:N paydası (en uzak "
                                     "görünüm); bundan uzaktan bakınca gizlenir. 0 = sınırsız")
                    .en("min_scale"),
                Param::integer_range("en_buyuk_olcek", Arity::optional(), 0, 100'000'000,
                                     "Görünür olduğu en büyük ölçeğin 1:N paydası (en yakın "
                                     "görünüm); bundan yakından bakınca gizlenir. 0 = sınırsız")
                    .en("max_scale"),
                Param::integer_range("opaklik", Arity::optional(), 0, 255,
                                     "Ekranda opaklık, 0 saydam – 255 opak; paftada her zaman opak")
                    .en("opacity"),
                Param::text("aciklama", Arity::optional(), "Katmanın açıklaması, serbest metin")
                    .en("description"),
                Param::boolean(
                    "salt", Arity::optional(),
                    "evet = kaynağından salt görüntü olarak alınmış (kilitlenir, kilidi "
                    "doğrudan açılamaz); hayır = düzenlenebilir kopyaya çevirir ve kilidi "
                    "açar")
                    .en("view_only"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katman oluşturur, aktif yapar ve özelliklerini değiştirir.",
        .run     = &run,
    };
}

} // namespace piricad::command
