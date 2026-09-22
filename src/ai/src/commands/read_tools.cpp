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
#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/job_templates.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/log.hpp"
#include "kentos_cad/command/registry.hpp"
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

/// WHAT AM I WORKING ON. The first question any agent, script or macro asks.
///
/// Five read tools already answer five narrower questions, and an agent that has
/// to call all of them before it can act spends its first turn finding out that
/// the drawing has one layout and nothing selected. This answers the whole shape
/// in one call, so "bunu A3'e yerleştir" resolves against the one valid selection
/// and the one layout without asking the user to pick anything (TODOS A-01).
///
/// IT SUMMARISES, IT DOES NOT DUMP. No geometry, no attribute rows, no column
/// values: a context that grew with the drawing would put a five-million-parcel
/// sheet into a prompt. Counts and names; the narrow tools answer the rest.
Task<void> run_context(Context& ctx)
{
    command::Bus& bus         = ctx.session().bus();
    const core::Document& doc = bus.document();

    Json out;
    out.set("surum", Json::integer(static_cast<std::int64_t>(doc.revision())));
    out.set("crs", Json::string(doc.crs().id()));
    out.set("nesne_sayisi", Json::integer(static_cast<std::int64_t>(doc.live_entity_count())));

    const core::Box2 extent = doc.extent();
    if (!extent.empty()) {
        Json box = Json::array({});
        box.push(Json::integer(extent.min_x));
        box.push(Json::integer(extent.min_y));
        box.push(Json::integer(extent.max_x));
        box.push(Json::integer(extent.max_y));
        out.set("kapsam", std::move(box));
    }

    // ---- layers, by name and count ------------------------------------------
    Json layers = Json::array({});
    for (const core::Layer& one : doc.layers())
        layers.push(Json::string(one.name));
    out.set("katmanlar", std::move(layers));

    // ---- layouts, and whether each is ready to print -------------------------
    //
    // "IS IT AIMED" IS THE QUESTION AN AGENT ACTUALLY HAS. A layout that exists
    // and looks at nothing prints an empty box, and finding that out after the
    // PDF is written is finding it out too late (L-15).
    Json sheets = Json::array({});
    for (const core::Layout& one : doc.layouts().all()) {
        Json sheet;
        sheet.set("ad", Json::string(one.name));
        sheet.set("sayfa", Json::integer(static_cast<std::int64_t>(one.pages.size())));
        sheet.set("oge", Json::integer(static_cast<std::int64_t>(one.items.size())));
        if (!one.paper.empty()) sheet.set("kagit", Json::string(one.paper));
        const core::LayoutItem* map = one.first_map();
        sheet.set("hedefli", Json::boolean(map != nullptr && !map->extent.empty()));
        const std::size_t trouble = core::layout_trouble(one).size();
        if (trouble != 0) sheet.set("sorun", Json::integer(static_cast<std::int64_t>(trouble)));
        sheets.push(std::move(sheet));
    }
    out.set("cikti_yerlesimleri", std::move(sheets));

    // ---- the selection, as a count and its keys ------------------------------
    // THE KEYS, NOT THE SLOTS (model.md R44). A slot is reused; a key is the
    // answer to "which parcel was this" six months later.
    Json picked = Json::array({});
    for (const core::EntityKey key : bus.selection().keys())
        picked.push(Json::integer(static_cast<std::int64_t>(core::raw(key))));
    out.set("secili", std::move(picked));

    // ---- the view, when there is one ----------------------------------------
    if (bus.on_view_query) {
        const command::ViewInfo view = bus.on_view_query();
        Json seen;
        Json box = Json::array({});
        box.push(Json::integer(view.window.min_x));
        box.push(Json::integer(view.window.min_y));
        box.push(Json::integer(view.window.max_x));
        box.push(Json::integer(view.window.max_y));
        seen.set("pencere", std::move(box));
        if (view.scale != 0) seen.set("olcek", Json::integer(view.scale));
        out.set("gorunum", std::move(seen));
    } else {
        // AN HONEST ABSENCE. A headless run has no window, and inventing one
        // would put an agent's next drawing somewhere nobody was looking.
        out.set("gorunum", Json::null());
    }

    ctx.report(std::move(out));
    ctx.echo("Bağlam: " + std::to_string(doc.live_entity_count()) + " nesne, " +
             std::to_string(doc.layers().size()) + " katman, " +
             std::to_string(doc.layouts().size()) + " çıktı yerleşimi, " +
             std::to_string(bus.selection().size()) + " seçili.");
    co_return;
}

/// FINDING A TOOL IN A CATALOGUE THAT NO LONGER FITS IN A GLANCE.
///
/// The catalogue is generated and it grows with the program: seventy-odd tools
/// today, and every command that gains `Flags::AiAccessible` adds one. A client
/// that must read all of it to find `ÖLÇEKLE` is spending a turn on something a
/// substring match answers.
///
/// AND IT HIDES NOTHING, which is the whole difficulty with a search over a tool
/// surface (TODOS M-09). A filtered list that looked complete would be worse
/// than no search at all: an agent that asked for "alan" and got three tools
/// would conclude the other seventy do not exist. So the answer always carries
/// how many matched, how many are being shown, and the fact that `tools/list`
/// serves the whole catalogue — and a search that matched nothing says so
/// plainly rather than returning an empty list that reads like an answer.
///
/// IT IS NOT A SECOND DESCRIPTION OF A COMMAND (CLAUDE.md 5.10). The name, the
/// title and the summary it returns are the catalogue's own fields, and the
/// schema — the part a client needs to CALL the tool — is not here: that is what
/// `tools/list` is for, and the answer says so.
Task<void> run_tool_search(Context& ctx)
{
    auto asked = co_await ctx.text("sorgu", "Aranan sözcük");
    if (!asked || asked->empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Aranacak bir sözcük gerekir: sorgu=<sözcük>."));
        co_return;
    }
    ctx.record("sorgu", Value::text(*asked));

    std::string field = "hepsi";
    if (const Value given = ctx.argument("alan"); !given.empty()) {
        field = given.as_text();
        ctx.record("alan", given);
    }

    std::int64_t limit = 20;
    if (const Value given = ctx.argument("sinir"); !given.empty()) {
        limit = given.as_int();
        ctx.record("sinir", given);
    }

    const Catalog catalogue = build_catalog(ctx.session().bus().registry());

    // TURKISH FOLDING, not `std::tolower` (CLAUDE.md 5.6): a surveyor searching
    // for "ölçek" must find `ÖLÇEKLE`, and `ı`/`i` must not collide the way an
    // ASCII lowercase would make them.
    const std::string needle = core::turkish_fold_key(*asked);

    Json hits            = Json::array({});
    std::int64_t matched = 0;
    std::int64_t shown   = 0;
    for (const ToolDef& tool : catalogue.tools) {
        bool hit = false;
        if (field == "ad" || field == "hepsi")
            hit = core::turkish_fold_key(tool.name).find(needle) != std::string::npos ||
                  core::turkish_fold_key(tool.title).find(needle) != std::string::npos;
        if (!hit && (field == "ozet" || field == "hepsi"))
            hit = core::turkish_fold_key(tool.description).find(needle) != std::string::npos;
        if (!hit) continue;

        ++matched;
        if (shown >= limit) continue;
        ++shown;

        Json one;
        one.set("arac", Json::string(tool.name));
        one.set("ad", Json::string(tool.title));
        one.set("komut", Json::string(tool.command_id));
        one.set("degistirir", Json::boolean(tool.mutates));
        // THE FIRST SENTENCE ONLY. The whole description carries the names, the
        // units and the approval rule, which is what a client needs when it is
        // about to CALL the tool — not when it is deciding which one to look at.
        const std::size_t stop = tool.description.find('\n');
        one.set("ozet", Json::string(stop == std::string::npos ? tool.description
                                                               : tool.description.substr(0, stop)));
        hits.push(std::move(one));
    }

    // THE HUMAN LINE IS BUILT BEFORE THE REPORT IS HANDED OVER, because
    // `ctx.report` takes the object by value and reading it afterwards is a
    // use-after-move — which is exactly how the first version of this crashed.
    std::string told;
    for (const Json& one : hits.as_array())
        told += "\n  " + one.find("arac")->as_string() + " — " + one.find("ad")->as_string();

    Json out;
    out.set("sorgu", Json::string(*asked));
    out.set("alan", Json::string(field));
    out.set("eslesen", Json::integer(matched));
    out.set("gosterilen", Json::integer(shown));
    out.set("katalog", Json::integer(static_cast<std::int64_t>(catalogue.tools.size())));
    out.set("araclar", std::move(hits));
    // SAID IN THE ANSWER ITSELF, every time. A client holding a filtered list has
    // to know it is filtered, and where the unfiltered one is.
    out.set("aciklama",
            Json::string("Bu bir ARAMA sonucudur, kataloğun tamamı değil. Araçların tam "
                         "listesi ve çağrı şemaları `tools/list` ile alınır; bu arama "
                         "yalnız ad ve özet üzerinde çalışır ve hiçbir aracı katalogdan "
                         "çıkarmaz."));
    ctx.report(std::move(out));

    if (matched == 0) {
        ctx.echo("'" + *asked + "' için eşleşen araç yok. Kataloğun tamamı " +
                 std::to_string(catalogue.tools.size()) +
                 " araç taşıyor; tam listeyi `tools/list` ile alın.");
        co_return;
    }

    std::string head = std::to_string(matched) + " araç eşleşti";
    if (shown < matched) head += ", ilk " + std::to_string(shown) + " tanesi";
    head += " (katalog: " + std::to_string(catalogue.tools.size()) + "):";
    if (shown < matched)
        told += "\n  … " + std::to_string(matched - shown) +
                " tane daha. Sınırı büyütün (sinir=…) ya da tam listeyi `tools/list` ile alın.";
    ctx.echo(head + told);
    co_return;
}

/// THE SHIPPED JOB TEMPLATES: what to do, in what order, as command lines.
///
/// AN ATLAS IS SIX COMMANDS AND THE ORDER IS THE PART NOBODY CAN GUESS. Place
/// the sheet, put a map frame on it, aim the atlas at a layer, check the sheet,
/// then print — and checking before printing rather than after is the whole
/// difference between finding a broken link on screen and finding it in a PDF
/// somebody signed. An agent that rediscovers that sequence rediscovers it
/// differently every time (TODOS M-09).
///
/// IT RUNS NOTHING. It hands back lines. Every one of them goes through the
/// ordinary tool surface afterwards, where a write still becomes a suggestion a
/// person applies (CLAUDE.md 5.7); a template that executed itself would be a
/// fast path, and there are none (Article 1.2).
Task<void> run_job_template(Context& ctx)
{
    auto typed = co_await ctx.text("islem", "İşlem: listele / goster");
    if (!typed) co_return;

    const bool show = core::turkish_key_equals(*typed, "goster");
    if (!show && !core::turkish_key_equals(*typed, "listele")) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *typed + "'. İşlemler: listele / goster"));
        co_return;
    }
    ctx.record("islem", Value::text(show ? "goster" : "listele"));

    // READ AT EVERY CALL, not cached. The package is a few kilobytes and a data
    // release must take effect without a restart — which is the reason the
    // templates are data at all (CLAUDE.md 3.5).
    core::Result<std::string> text =
        command::read_catalog_text(std::string("data/") + kJobTemplatePath);
    if (!text) {
        ctx.session().fail(text.error());
        co_return;
    }

    core::Result<JobTemplateCatalog> loaded = JobTemplateCatalog::from_json(text.value());
    if (!loaded) {
        ctx.session().fail(loaded.error());
        co_return;
    }
    const JobTemplateCatalog& catalogue = loaded.value();

    if (!show) {
        Json listing     = Json::array({});
        std::string told = std::to_string(catalogue.templates.size()) + " iş şablonu (paket " +
                           catalogue.package_version + "):";
        for (const JobTemplate& one : catalogue.templates) {
            listing.push(one.to_json(false));
            told += "\n  " + one.id + " — " + one.title;
        }
        told += "\nAdımları görmek için: İŞŞABLONU islem=goster sablon=<kimlik>";

        Json out;
        out.set("paket_surumu", Json::string(catalogue.package_version));
        out.set("sablonlar", std::move(listing));
        ctx.report(std::move(out));
        ctx.echo(told);
        co_return;
    }

    auto wanted = co_await ctx.text("sablon", "Şablonun kimliği");
    if (!wanted || wanted->empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Hangi şablon: sablon=<kimlik>. Kimlikleri İŞŞABLONU "
                                     "islem=listele ile görün."));
        co_return;
    }
    ctx.record("sablon", Value::text(*wanted));

    const JobTemplate* one = catalogue.find(*wanted);
    if (one == nullptr) {
        std::string known;
        for (const JobTemplate& held : catalogue.templates)
            known += (known.empty() ? "" : ", ") + held.id;
        ctx.session().fail(
            core::err(core::ErrorCode::NotFound,
                      "Böyle bir iş şablonu yok: '" + *wanted + "'. Olanlar: " + known));
        co_return;
    }

    Json out = one->to_json(true);
    out.set("paket_surumu", Json::string(catalogue.package_version));
    // SAID IN THE ANSWER, because a client holding a list of command lines is
    // one misreading away from believing it has already done the work.
    out.set("aciklama",
            Json::string("Bu adımlar ÇALIŞTIRILMADI. Her biri sıradan bir komut satırıdır: "
                         "yer tutucuları doldurup olağan araç yüzeyinden gönderin. Yazan her "
                         "adım yine önizlemeli bir öneriye dönüşür ve bilgisayar başındaki "
                         "kişi uygular."));

    std::string told = one->title + " (" + one->id + " " + one->version + "), " +
                       std::to_string(one->steps.size()) + " adım:";
    for (const std::string& step : one->steps)
        told += "\n  " + step;
    told += "\nBu satırlar çalıştırılmadı.";
    ctx.report(std::move(out));
    ctx.echo(told);
    co_return;
}

} // namespace

std::vector<CommandSpec> detail::read_tool_specs()
{
    std::vector<CommandSpec> specs;

    specs.push_back(CommandSpec{
        .id       = "core.layers",
        .names    = {"KATMANLAR", "LAYERS", "KTL"},
        .title    = "Katmanları Listele",
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
        .title    = "Öznitelik Şeması",
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
        .title    = "Sorgula",
        .category = command::Category::Query,
        .params =
            {
                Param::text("katman", Arity::optional(),
                            "Hangi katmanda aranacağı; verilmezse bütün çizim")
                    .en("layer"),
                Param::text("alan", Arity::optional(),
                            "Öznitelik sütunu; verilirse o sütunu taşıyan nesneler")
                    .en("field"),
                Param::text("deger", Arity::optional(),
                            "Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte")
                    .en("value"),
                Param::integer_range("sinir", Arity::optional(), 1, 1000,
                                     "En çok kaç nesne bildirileceği; varsayılan 200")
                    .en("limit"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.",
        .run = &run_query,
    });

    specs.push_back(CommandSpec{
        .id       = "core.selection_info",
        .names    = {"SEÇİMBİLGİSİ", "SECIMBILGISI", "SELECTIONINFO", "SÇB"},
        .title    = "Seçim Bilgisi",
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
        .title    = "Görünüm Bilgisi",
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini "
                   "bildirir.",
        .run = &run_view_info,
    });

    specs.push_back(CommandSpec{
        .id       = "core.context",
        .names    = {"BAĞLAM", "BAGLAM", "CONTEXT", "BĞL"},
        .title    = "Bağlam",
        .category = command::Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat "
                   "sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, "
                   "seçili nesneler ve görünüm. Özet verir, döküm değil.",
        .run    = &run_context,
        .effect = command::Effect::Query,
    });

    specs.push_back(CommandSpec{
        .id       = "core.tool_search",
        .names    = {"ARAÇARA", "ARACARA", "TOOLSEARCH", "ARA"},
        .title    = "Araç Ara",
        .category = command::Category::Query,
        .params =
            {
                Param::text("sorgu", Arity::exactly(1),
                            "Aranan sözcük; ad ve özet içinde Türkçe katlamayla eşleşir")
                    .en("query"),
                Param::choice("alan", Arity::optional(), {"hepsi", "ad", "ozet"},
                              "Nerede aranacağı: hepsi (öntanımlı), ad ya da ozet")
                    .en("field"),
                Param::integer_range("sinir", Arity::optional(), 1, 200,
                                     "En çok kaç sonuç gösterilsin; öntanımlı 20. Eşleşme "
                                     "sayısı her hâlde bildirilir")
                    .en("limit"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın "
                   "eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını "
                   "söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır.",
        .run     = &run_tool_search,
        .effect  = command::Effect::Query,
    });

    specs.push_back(CommandSpec{
        .id       = "core.job_template",
        .names    = {"İŞŞABLONU", "ISSABLONU", "JOBTEMPLATE", "İŞŞ"},
        .title    = "İş Şablonu",
        .category = command::Category::Query,
        .params =
            {
                Param::choice("islem", Arity::exactly(1), {"listele", "goster"},
                              "Ne yapılacağı: listele ya da goster")
                    .en("action"),
                Param::text("sablon", Arity::optional(), "Şablonun kimliği; goster için gerekir")
                    .en("template"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::ReadOnly | Flags::NoEffect | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut "
                   "satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç "
                   "yüzeyinden gönderilir ve yazan her adım yine öneri olur.",
        .run     = &run_job_template,
        .effect  = command::Effect::Query,
    });

    return specs;
}

} // namespace kentos::ai
