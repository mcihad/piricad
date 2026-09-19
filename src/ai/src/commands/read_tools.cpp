// SPDX-License-Identifier: GPL-3.0-or-later
// core.layers (KATMANLAR), core.attr_schema (ÖZNİTELİKŞEMASI), core.query (SORGULA),
// core.selection_info (SEÇİMBİLGİSİ), core.view_info (GÖRÜNÜMBİLGİSİ)
//
// THE SIX QUESTIONS A CLIENT ASKS BEFORE IT DRAWS ANYTHING, five of which are
// answerable today. `.claude/ai.md` R11 names them and requires that context
// reach a model ONLY through tools like these — never by dumping the drawing
// into a prompt (P9), which for five million parcels is both useless and a
// privacy problem. The sixth, `mevzuat_ara`, waits for the corpus: an answer
// without a madde reference is forbidden (R15, P5), and `/data/corpus` is empty.
//
// EVERY ONE ANSWERS TWICE. A Turkish sentence through `ctx.echo` for the person
// at the keyboard, and the same facts as data through `ctx.report` for a client
// that is not a person. Before `report` existed the only way to learn what a
// read command found was to parse its prose, which is a thing no agent should be
// asked to do and no test should depend on.
//
// AND NONE OF THEM TOUCHES ANYTHING: `ReadOnly | NoEffect`. `NoEffect` is the
// flag the approval gate reads, and it is narrower than `ReadOnly` on purpose —
// `core.undo`, `core.save` and `core.export` are all `ReadOnly` and none of them
// is safe to hand an agent unattended.
#include "kentos_cad/ai/commands.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/log.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kentos::ai {
namespace {

using command::Arity;
using command::CommandSpec;
using command::Context;
using command::Flags;
using command::Param;
using command::Task;
using command::UndoPolicy;
using command::Value;
using core::Json;

/// A length in metres for a person to read: three decimals, which is the
/// millimetre the document stores.
std::string metres(core::Mm mm)
{
    const bool negative    = mm < 0;
    const auto abs_mm      = static_cast<std::uint64_t>(negative ? -mm : mm);
    const std::string frac = std::to_string(abs_mm % 1000);
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," +
           std::string(3 - frac.size(), '0') + frac;
}

/// How many live entities sit on each layer, counted once.
std::vector<std::size_t> counts_per_layer(const core::Document& doc)
{
    std::vector<std::size_t> counts(doc.layers().size(), 0);
    const core::EntityTable& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e)) continue;
        const core::LayerId slot = entities.layer[e];
        if (slot < counts.size()) ++counts[slot];
    }
    return counts;
}

// ------------------------------------------------------- KATMANLAR ----------

Task<void> run_layers(Context& ctx)
{
    const core::Document& doc              = ctx.document();
    const std::vector<std::size_t> counts  = counts_per_layer(doc);
    const std::vector<core::Layer>& layers = doc.layers();

    Json rows = Json::array({});
    std::string said;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const core::Layer& layer = layers[i];
        Json row;
        row.set("ad", Json::string(layer.name));
        row.set("anahtar", Json::integer(static_cast<std::int64_t>(layer.key)));
        row.set("nesne_sayisi", Json::integer(static_cast<std::int64_t>(counts[i])));
        row.set("gorunur", Json::boolean(layer.visible));
        row.set("kilitli", Json::boolean(layer.locked));
        row.set("basilir", Json::boolean(layer.plottable));
        if (!layer.group.empty()) row.set("grup", Json::string(layer.group));
        rows.push(std::move(row));

        said += (said.empty() ? "" : ", ") + layer.name + " (" + std::to_string(counts[i]) + ")";
    }

    Json report;
    report.set("katmanlar", std::move(rows));
    report.set("aktif", Json::string(doc.layer(ctx.active_layer()) != nullptr
                                         ? doc.layer(ctx.active_layer())->name
                                         : std::string()));
    report.set("crs", Json::string(doc.crs().id()));
    ctx.report(std::move(report));

    ctx.echo(std::to_string(layers.size()) + " katman: " + said);
    co_return;
}

// ------------------------------------------------- ÖZNİTELİKŞEMASI ----------

Task<void> run_attr_schema(Context& ctx)
{
    const core::AttrTable& table = ctx.document().attributes();

    Json columns = Json::array({});
    std::string said;
    for (std::size_t i = 0; i < table.columns(); ++i) {
        const core::AttrColumn* held = table.column(static_cast<core::AttrId>(i));
        if (held == nullptr) continue;
        const core::AttrSpec& spec = held->spec();
        Json column;
        column.set("ad", Json::string(spec.id));
        column.set("tur", Json::string(core::attr_type_name(spec.type)));
        if (!spec.name_tr.empty()) column.set("etiket", Json::string(spec.name_tr));
        if (!spec.summary_tr.empty()) column.set("aciklama", Json::string(spec.summary_tr));
        if (!spec.catalog.empty()) column.set("katalog", Json::string(spec.catalog));
        column.set("zorunlu", Json::boolean(spec.required));
        columns.push(std::move(column));
        said += (said.empty() ? "" : ", ") + spec.id;
    }

    Json report;
    report.set("sutunlar", std::move(columns));
    report.set("satir_sayisi", Json::integer(static_cast<std::int64_t>(table.rows())));
    ctx.report(std::move(report));

    ctx.echo(table.columns() == 0 ? "Çizimde tanımlı öznitelik sütunu yok."
                                  : std::to_string(table.columns()) + " sütun: " + said);
    co_return;
}

// --------------------------------------------------------- SORGULA ----------

Task<void> run_query(Context& ctx)
{
    const core::Document& doc = ctx.document();

    // A DECLARED FILTER, NOT AN EXPRESSION, and that is a rule rather than a
    // shortcut: CLAUDE.md 5.11 allows exactly one parser in this program, and
    // `core::StyleCondition` already says in its own comment that a fifth test
    // "starts to be a language". So the filter is the same closed set the style
    // rules use — a layer, optionally one attribute equal to one value, and a
    // cap — expressed as declared parameters the bus validates.
    const std::string layer_name = ctx.argument("katman").as_text();
    const std::string field      = ctx.argument("alan").as_text();
    const std::string wanted     = ctx.argument("deger").as_text();
    const std::int64_t cap       = ctx.argument("sinir").as_int(200);

    core::LayerId layer = core::kNoLayer;
    if (!layer_name.empty()) {
        layer = doc.find_layer(layer_name);
        if (layer == core::kNoLayer) {
            ctx.echo("Katman yok: '" + layer_name + "'. KATMANLAR ile listeyi alın.");
            co_return;
        }
    }

    const core::AttrTable& attrs = doc.attributes();
    const core::AttrId column    = field.empty() ? core::kNoAttr : attrs.find(field);
    if (!field.empty() && column == core::kNoAttr) {
        ctx.echo("Öznitelik sütunu yok: '" + field + "'. ÖZNİTELİKŞEMASI ile listeyi alın.");
        co_return;
    }

    const core::EntityTable& entities = doc.entities();
    Json keys                         = Json::array({});
    std::size_t matched               = 0;
    std::size_t reported              = 0;
    core::Box2 extent{};
    bool have_extent = false;

    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e)) continue;
        if (layer != core::kNoLayer && entities.layer[e] != layer) continue;

        if (column != core::kNoAttr) {
            auto held = doc.attribute(column, e);
            if (!held) continue;
            // COMPARED AS THE USER WOULD SEE IT (`attr_display`), because the
            // value a client has in hand came from this same tool's earlier
            // answer or from the attribute table on screen — and comparing a
            // typed cell against a typed literal would need the second parser
            // CLAUDE.md 5.11 forbids.
            if (!wanted.empty() && core::attr_display(held.value()) != wanted) continue;
        }

        ++matched;
        // THE CAP IS DECLARED AND ENFORCED BY THE BUS (R11 asks for a hard
        // result cap), and it bounds what is REPORTED rather than what is
        // counted: a client that asks "how many" gets an honest total and a
        // bounded list, instead of a truthful list of an arbitrary prefix.
        if (reported < static_cast<std::size_t>(cap)) {
            keys.push(Json::integer(static_cast<std::int64_t>(doc.key_of(e))));
            ++reported;
            const core::Box2 box = entities.box_of(e);
            extent               = have_extent ? core::Box2{std::min(extent.min_x, box.min_x),
                                              std::min(extent.min_y, box.min_y),
                                              std::max(extent.max_x, box.max_x),
                                              std::max(extent.max_y, box.max_y)}
                                               : box;
            have_extent          = true;
        }
    }

    Json report;
    report.set("nesneler", std::move(keys));
    report.set("adet", Json::integer(static_cast<std::int64_t>(matched)));
    report.set("bildirilen", Json::integer(static_cast<std::int64_t>(reported)));
    report.set("sinir", Json::integer(cap));
    if (have_extent) {
        Json box = Json::array({});
        box.push(Json::integer(extent.min_x));
        box.push(Json::integer(extent.min_y));
        box.push(Json::integer(extent.max_x));
        box.push(Json::integer(extent.max_y));
        report.set("kutu_mm", std::move(box));
    }
    ctx.report(std::move(report));

    ctx.echo(std::to_string(matched) + " nesne eşleşti" +
             (matched > reported ? ", ilk " + std::to_string(reported) + " bildirildi" : "") + ".");
    co_return;
}

// ------------------------------------------------- SEÇİMBİLGİSİ -------------

Task<void> run_selection_info(Context& ctx)
{
    const command::Selection& selection = ctx.session().bus().selection();
    const core::Document& doc           = ctx.document();

    Json keys = Json::array({});
    for (const core::EntityKey key : selection.keys())
        keys.push(Json::integer(static_cast<std::int64_t>(key)));

    Json report;
    report.set("nesneler", std::move(keys));
    report.set("adet", Json::integer(static_cast<std::int64_t>(selection.size())));
    report.set("surum", Json::integer(static_cast<std::int64_t>(doc.revision())));
    ctx.report(std::move(report));

    ctx.echo(selection.empty() ? "Seçim boş."
                               : std::to_string(selection.size()) + " nesne seçili.");
    co_return;
}

// ----------------------------------------------- GÖRÜNÜMBİLGİSİ ------------

Task<void> run_view_info(Context& ctx)
{
    command::Bus& bus = ctx.session().bus();

    // NO VIEWPORT IS AN HONEST ANSWER. A headless run, a journal replay and a
    // test genuinely have no window, and inventing a rectangle for them would
    // put an agent's next drawing somewhere nobody was looking. The hook is
    // unset in exactly those cases (`Bus::on_view_query`).
    if (!bus.on_view_query) {
        ctx.echo("Bu çalıştırmada görüntü penceresi yok (başsız çalışıyor).");
        co_return;
    }

    const command::ViewInfo view = bus.on_view_query();

    Json box = Json::array({});
    box.push(Json::integer(view.window.min_x));
    box.push(Json::integer(view.window.min_y));
    box.push(Json::integer(view.window.max_x));
    box.push(Json::integer(view.window.max_y));

    Json centre = Json::array({});
    centre.push(Json::integer(view.centre.x));
    centre.push(Json::integer(view.centre.y));

    Json report;
    report.set("kutu_mm", std::move(box));
    report.set("merkez_mm", std::move(centre));
    report.set("olcek", Json::integer(view.scale));
    report.set("mm_piksel", Json::number(view.mm_per_pixel));
    report.set("ekran_px",
               Json::array({Json::integer(view.width_px), Json::integer(view.height_px)}));
    report.set("crs", Json::string(view.crs.empty() ? ctx.document().crs().id() : view.crs));
    ctx.report(std::move(report));

    ctx.echo("Görünüm: Sağa " + metres(view.window.min_x) + " – " + metres(view.window.max_x) +
             " m, Yukarı " + metres(view.window.min_y) + " – " + metres(view.window.max_y) +
             " m; ölçek 1:" + std::to_string(view.scale));
    co_return;
}

} // namespace

std::vector<CommandSpec> detail::read_tool_specs()
{
    std::vector<CommandSpec> specs;

    specs.push_back(CommandSpec{
        .id       = "core.layers",
        .names    = {"KATMANLAR", "LAYERS", "KTL"},
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.",
        .run = &run_layers,
    });

    specs.push_back(CommandSpec{
        .id       = "core.attr_schema",
        .names    = {"ÖZNİTELİKŞEMASI", "OZNITELIKSEMASI", "ATTRSCHEMA", "ÖŞ"},
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.",
        .run      = &run_attr_schema,
    });

    specs.push_back(CommandSpec{
        .id       = "core.query",
        .names    = {"SORGULA", "QUERY", "SRG"},
        .category = command::Category::Query,
        .params =
            {
                Param::text("katman", Arity::optional(),
                            "Hangi katmanda aranacağı; verilmezse bütün çizim"),
                Param::text("alan", Arity::optional(),
                            "Öznitelik sütunu; verilirse o sütunu taşıyan nesneler"),
                Param::text("deger", Arity::optional(),
                            "Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte"),
                Param::integer_range("sinir", Arity::optional(), 1, 1000,
                                     "En çok kaç nesne bildirileceği; varsayılan 200"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.",
        .run = &run_query,
    });

    specs.push_back(CommandSpec{
        .id       = "core.selection_info",
        .names    = {"SEÇİMBİLGİSİ", "SECIMBILGISI", "SELECTIONINFO", "SÇB"},
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.",
        .run      = &run_selection_info,
    });

    specs.push_back(CommandSpec{
        .id       = "core.view_info",
        .names    = {"GÖRÜNÜMBİLGİSİ", "GORUNUMBILGISI", "VIEWINFO", "GRB"},
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini "
                   "bildirir.",
        .run = &run_view_info,
    });

    return specs;
}

} // namespace kentos::ai
