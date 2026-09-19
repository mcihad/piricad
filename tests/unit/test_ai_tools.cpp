// SPDX-License-Identifier: GPL-3.0-or-later
//
// The read tools, the handle store, the plan store and the approval gate — the
// four things that stand between an agent and this drawing. Qt-free: every one of
// them is a function over a bus and a document, which is why they can be proved
// here rather than through a socket (.claude/test.md R23).
//
// WHAT THESE CASES ARE REALLY FOR. Each one guards a specific way the feature
// could look finished and be wrong: a read tool that answers a person but tells a
// machine nothing; a handle that still resolves after the drawing moved under it;
// a plan that applies without anybody deciding; an audit log with a hole in it
// exactly where the refusals go.
#include "kentos_test.hpp"

#include "kentos_cad/ai/audit.hpp"
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/gate.hpp"
#include "kentos_cad/ai/handles.hpp"
#include "kentos_cad/ai/job_templates.hpp"

#include "kentos_cad/ai/plan.hpp"
#include "kentos_cad/core/text.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/command/registry.hpp"

using namespace kentos;
using namespace kentos::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);
        ai::register_ai_commands(reg);
    }

    DispatchResult must(const char* line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
        return r.value();
    }
};

/// One drawing with two layers and three objects, so a count can be wrong.
void seed(Rig& f)
{
    f.must("KATMAN ad=PARSEL");
    f.must("ALAN 0,0 10,0 10,10 0,10");
    f.must("KATMAN ad=YOL");
    f.must("ÇİZGİ 0,20 30,20");
    f.must("ÇİZGİ 0,24 30,24");
}

const core::Json* field(const core::Json& report, const char* name)
{
    return report.find(name);
}

} // namespace

TEST_CASE("Okuma araçları hem insana hem makineye cevap veriyor")
{
    Rig f;
    seed(f);

    // THE SEAM THIS PROVES. Before `DispatchResult::lines` and `::report`
    // existed, a read command's answer went to one global echo sink and the
    // caller got an empty `message` — so a script, a test or an agent could not
    // learn what it had just asked. Both halves are checked here on purpose.
    const DispatchResult layers = f.must("KATMANLAR");
    REQUIRE_FALSE(layers.lines.empty());
    CHECK(layers.lines.front().find("PARSEL") != std::string::npos);

    const core::Json* rows = field(layers.report, "katmanlar");
    REQUIRE(rows != nullptr);
    REQUIRE(rows->is_array());
    // Layer 0 plus the two that were made.
    CHECK_EQ(rows->as_array().size(), std::size_t{3});

    std::size_t parcel_objects = 0;
    for (const core::Json& row : rows->as_array()) {
        const core::Json* name = row.find("ad");
        const core::Json* n    = row.find("nesne_sayisi");
        if (name != nullptr && n != nullptr && name->as_string() == "PARSEL")
            parcel_objects = static_cast<std::size_t>(n->as_int());
    }
    CHECK_EQ(parcel_objects, std::size_t{1});
    CHECK(field(layers.report, "crs") != nullptr);
}

TEST_CASE("SORGULA sayıyı dürüst verir, listeyi sınırlar")
{
    Rig f;
    seed(f);

    const DispatchResult all = f.must("SORGULA katman=YOL");
    const core::Json* count  = field(all.report, "adet");
    REQUIRE(count != nullptr);
    CHECK_EQ(count->as_int(), 2);

    // THE CAP BOUNDS WHAT IS REPORTED, NOT WHAT IS COUNTED (.claude/ai.md R11
    // asks for a hard result cap). A tool that answered "1" to "how many" would
    // send an agent to draw on a wrong assumption; one that answered "2" and
    // listed one is telling the truth twice.
    const DispatchResult capped = f.must("SORGULA katman=YOL sinir=1");
    CHECK_EQ(field(capped.report, "adet")->as_int(), 2);
    CHECK_EQ(field(capped.report, "bildirilen")->as_int(), 1);
    REQUIRE(field(capped.report, "nesneler") != nullptr);
    CHECK_EQ(field(capped.report, "nesneler")->as_array().size(), std::size_t{1});

    // The cap is a DECLARED range, so the bus refuses an impossible one before
    // the body runs — no tool-side check, no silent clamp.
    CHECK(!f.bus.execute_line("SORGULA katman=YOL sinir=0", Origin::Test).ok());
    CHECK(!f.bus.execute_line("SORGULA katman=YOL sinir=9999", Origin::Test).ok());

    // An unknown layer is a sentence, not a crash, and names the tool that
    // lists the real ones.
    const DispatchResult missing = f.must("SORGULA katman=YOKBÖYLE");
    REQUIRE_FALSE(missing.lines.empty());
    CHECK(missing.lines.front().find("KATMANLAR") != std::string::npos);
}

TEST_CASE("GÖRÜNÜMBİLGİSİ pencere yoksa uydurmuyor")
{
    Rig f;
    seed(f);

    // NO VIEWPORT IS AN HONEST ANSWER. A headless run, a journal replay and this
    // test genuinely have no window; inventing a rectangle would put an agent's
    // next drawing somewhere nobody was looking.
    const DispatchResult blind = f.must("GÖRÜNÜMBİLGİSİ");
    REQUIRE_FALSE(blind.lines.empty());
    CHECK(blind.lines.front().find("başsız") != std::string::npos);
    CHECK(blind.report.is_null());

    f.bus.on_view_query = [] {
        ViewInfo info;
        info.window       = core::Box2{100, 200, 5100, 3200};
        info.centre       = core::Point2{2600, 1700};
        info.mm_per_pixel = 5.0;
        info.scale        = 1000;
        info.width_px     = 1000;
        info.height_px    = 600;
        info.crs          = "TUREF/TM30";
        return info;
    };

    const DispatchResult seen = f.must("GÖRÜNÜMBİLGİSİ");
    const core::Json* box     = field(seen.report, "kutu_mm");
    REQUIRE(box != nullptr);
    REQUIRE_EQ(box->as_array().size(), std::size_t{4});
    CHECK_EQ(box->as_array()[0].as_int(), 100);
    CHECK_EQ(box->as_array()[3].as_int(), 3200);
    CHECK_EQ(field(seen.report, "olcek")->as_int(), 1000);
    CHECK_EQ(field(seen.report, "crs")->as_string(), std::string("TUREF/TM30"));
}

TEST_CASE("Okuma araçları hiçbir şeyi değiştirmez ve NoEffect taşır")
{
    Rig f;
    seed(f);
    const std::uint64_t before = f.doc.content_hash();

    for (const char* line :
         {"KATMANLAR", "ÖZNİTELİKŞEMASI", "SORGULA", "SEÇİMBİLGİSİ", "GÖRÜNÜMBİLGİSİ"}) {
        const DispatchResult ran = f.must(line);
        CHECK_FALSE(ran.mutated);
        CHECK_EQ(ran.ops, std::size_t{0});
    }
    CHECK_EQ(f.doc.content_hash(), before);

    // THE DISTINCTION THAT PROTECTS THE AGENT PATH. `ReadOnly` means "skips the
    // transaction path" and says nothing about safety: these all carry it, and
    // handing an agent `core.undo` or `core.save` unattended would let it
    // reverse the drawing or write over a file. `NoEffect` is the narrow claim,
    // and only the read tools make it.
    for (const char* id : {"core.layers", "core.query", "core.view_info", "core.selection_info",
                           "core.attr_schema"}) {
        const CommandSpec* spec = f.reg.by_id(id);
        REQUIRE(spec != nullptr);
        CHECK(has_flag(spec->flags, Flags::NoEffect));
        CHECK(has_flag(spec->flags, Flags::AiAccessible));
    }
    for (const char* id : {"core.undo", "core.redo", "core.save", "core.export"}) {
        const CommandSpec* spec = f.reg.by_id(id);
        if (spec == nullptr) continue;
        CHECK(has_flag(spec->flags, Flags::ReadOnly));
        CHECK_FALSE(has_flag(spec->flags, Flags::NoEffect));
    }
}

TEST_CASE("Tutamak: biçim, çözüm ve çizim değişince reddedilme")
{
    ai::HandleStore store;

    const ai::HandleValue& points =
        store.mint_points({core::Point2{1000, 2000}, core::Point2{3000, 4000}}, "sorgula", 7);
    CHECK_EQ(points.id.size(), std::size_t{17});
    CHECK_EQ(points.id.front(), '@');

    auto ref = ai::HandleRef::parse(points.id);
    REQUIRE(ref.has_value());
    auto whole = store.resolve(*ref, 7);
    REQUIRE(whole.ok());
    CHECK_EQ(whole.value().points.size(), std::size_t{2});

    // One element of a list, which is how a client says "the second corner".
    auto second = ai::HandleRef::parse(points.id + ".1");
    REQUIRE(second.has_value());
    auto one = store.resolve(*second, 7);
    REQUIRE(one.ok());
    REQUIRE_EQ(one.value().points.size(), std::size_t{1});
    CHECK_EQ(one.value().points.front().x, 3000);

    // Past the end is a refusal that says how many there were.
    auto third = ai::HandleRef::parse(points.id + ".9");
    REQUIRE(third.has_value());
    CHECK(!store.resolve(*third, 7).ok());

    // A HANDLE IS ONLY AS TRUE AS THE DRAWING IT WAS READ FROM. Between the read
    // and the write somebody may have moved or erased what it names, and drawing
    // at those coordinates would put a line where nothing is any more.
    auto stale = store.resolve(*ref, 8);
    CHECK(!stale.ok());
    if (!stale.ok()) CHECK(stale.error().message.find("eski") != std::string::npos);

    // Nothing that is not a handle parses as one — which is what makes a
    // coordinate literal inexpressible rather than merely refused.
    for (const char* not_one : {"485300,4310200", "@zzzz", "@0123", "", "12", "@0123456789abcdef."})
        CHECK_FALSE(ai::HandleRef::parse(not_one).has_value());

    // And the description a client gets carries no coordinate list (ai.md P9).
    const core::Json told = ai::HandleStore::describe(points);
    CHECK(told.find("tutamak") != nullptr);
    CHECK_EQ(told.find("adet")->as_int(), 2);
    CHECK(told.find("noktalar") == nullptr);
}

TEST_CASE("M-09: her iş şablonu adımı gerçek bir komut satırıdır")
{
    Rig f;

    // THE PACKAGE IS DATA, so the test reads it the same way the command does.
    // A template shipped with a typo would be worse than no template: an agent
    // reading plausible-looking lines that do not run wastes a turn discovering
    // that one by one, and a surveyor reading them cannot tell which one is
    // wrong (TODOS M-09).
    core::Result<std::string> text =
        command::read_catalog_text(std::string("data/") + ai::kJobTemplatePath);
    REQUIRE_MESSAGE(text.ok(), text.error().message);

    core::Result<ai::JobTemplateCatalog> loaded = ai::JobTemplateCatalog::from_json(text.value());
    REQUIRE_MESSAGE(loaded.ok(), loaded.error().message);
    const ai::JobTemplateCatalog& catalogue = loaded.value();
    CHECK(catalogue.templates.size() >= 3u);

    for (const ai::JobTemplate& one : catalogue.templates) {
        CAPTURE(one.id);
        CHECK_FALSE(one.title.empty());
        CHECK_FALSE(one.summary.empty());
        CHECK_FALSE(one.version.empty());

        // RENDERED WITH THE EXAMPLES, because a template is only runnable if its
        // own examples are: `ad=<yerlesim>` with "Ada 1284 Atlası" in it must be
        // quoted in the data or it parses as three tokens.
        core::Json values;
        for (const ai::JobParam& param : one.params) {
            CHECK_FALSE(param.help.empty());
            if (!param.example.empty()) values.set(param.name, core::Json::string(param.example));
        }

        const std::vector<std::string> lines = one.render(values);
        REQUIRE_EQ(lines.size(), one.steps.size());

        for (const std::string& line : lines) {
            CAPTURE(line);
            core::Result<command::ParsedLine> parsed = command::parse_line(line);
            REQUIRE_MESSAGE(parsed.ok(), parsed.error().message);

            const CommandSpec* spec = f.reg.resolve(parsed.value().command);
            REQUIRE_MESSAGE(spec != nullptr, "bilinmeyen komut: " << parsed.value().command);

            // EVERY `key=` IS A DECLARED PARAMETER. This is the mistake that
            // actually happens — a plausible argument name the command never
            // took — and the bus would refuse it at run time with a message
            // about the wrong thing.
            for (const command::Token& token : parsed.value().tokens) {
                if (token.kind != command::Token::Kind::KeyValue) continue;
                bool declared = false;
                for (const command::Param& param : spec->params)
                    if (core::turkish_key_equals(param.name, token.word) ||
                        (!param.was.empty() && core::turkish_key_equals(param.was, token.word)))
                        declared = true;
                CHECK_MESSAGE(declared, "tanımsız argüman: " << token.word << " — " << spec->id);
            }
        }
    }

    // ---- AND THE COMMAND SERVES IT ------------------------------------------
    const DispatchResult listed = f.must("İŞŞABLONU islem=listele");
    REQUIRE(field(listed.report, "sablonlar") != nullptr);
    CHECK_EQ(field(listed.report, "sablonlar")->as_array().size(), catalogue.templates.size());

    const DispatchResult shown = f.must("İŞŞABLONU islem=goster sablon=atlas-pafta");
    REQUIRE(field(shown.report, "adimlar") != nullptr);
    CHECK(field(shown.report, "adimlar")->as_array().size() >= 5u);
    // SAID EVERY TIME: a client holding a list of command lines is one
    // misreading away from believing the work is already done.
    CHECK(field(shown.report, "aciklama")->as_string().find("ÇALIŞTIRILMADI") != std::string::npos);

    // An unknown id names what does exist rather than answering with nothing.
    auto missing = f.bus.execute_line("İŞŞABLONU islem=goster sablon=yok", Origin::Test);
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().message.find("atlas-pafta") != std::string::npos);

    // A HALF-FILLED TEMPLATE READS AS UNFINISHED. Blanking a placeholder would
    // produce `ad=""` — a line that looks finished and is not.
    const ai::JobTemplate* atlas = catalogue.find("atlas-pafta");
    REQUIRE(atlas != nullptr);
    const std::vector<std::string> bare = atlas->render(core::Json{});
    bool kept                           = false;
    for (const std::string& line : bare)
        if (line.find("<yerlesim>") != std::string::npos) kept = true;
    CHECK(kept);
}

TEST_CASE("M-09: araç araması hiçbir aracı gizlemez")
{
    Rig f;

    // A SEARCH OVER A TOOL SURFACE IS DANGEROUS IN ONE PARTICULAR WAY: a
    // filtered list that LOOKS complete makes an agent conclude that the tools
    // it did not see do not exist. So every answer carries three numbers — how
    // many matched, how many are shown, how many the catalogue holds — and says
    // where the unfiltered list is (TODOS M-09).
    const DispatchResult found = f.must("ARAÇARA sorgu=katman");
    const core::Json& report   = found.report;
    REQUIRE(field(report, "eslesen") != nullptr);
    REQUIRE(field(report, "gosterilen") != nullptr);
    REQUIRE(field(report, "katalog") != nullptr);
    CHECK(field(report, "eslesen")->as_int() > 0);
    CHECK(field(report, "katalog")->as_int() >= field(report, "eslesen")->as_int());
    CHECK(field(report, "aciklama")->as_string().find("tools/list") != std::string::npos);

    // TURKISH FOLDING, not `std::tolower` (CLAUDE.md 5.6): a surveyor typing
    // "olcek" must reach `ÖLÇEKLE`, and `ı`/`i` must not collide.
    const DispatchResult folded = f.must("ARAÇARA sorgu=olcek");
    CHECK(folded.report.find("eslesen")->as_int() > 0);
    CHECK_EQ(f.must("ARAÇARA sorgu=ÖLÇEK").report.find("eslesen")->as_int(),
             folded.report.find("eslesen")->as_int());

    // THE LIMIT CAPS WHAT IS SHOWN, NEVER WHAT IS COUNTED. A client that asked
    // for one result still learns there are more, and is told how to get them.
    const DispatchResult capped = f.must("ARAÇARA sorgu=katman sinir=1");
    CHECK_EQ(capped.report.find("gosterilen")->as_int(), 1);
    CHECK(capped.report.find("eslesen")->as_int() >= 1);
    REQUIRE_EQ(capped.report.find("araclar")->as_array().size(), 1u);
    if (capped.report.find("eslesen")->as_int() > 1) {
        std::string said;
        for (const std::string& line : capped.lines)
            said += line;
        CHECK(said.find("tane daha") != std::string::npos);
    }

    // NOTHING MATCHED IS SAID IN WORDS. An empty array reads like an answer;
    // "there is no such tool, and the catalogue holds N" is one.
    const DispatchResult none = f.must("ARAÇARA sorgu=zzqqxx");
    CHECK_EQ(none.report.find("eslesen")->as_int(), 0);
    CHECK_EQ(none.report.find("araclar")->as_array().size(), 0u);
    std::string empty_said;
    for (const std::string& line : none.lines)
        empty_said += line;
    CHECK(empty_said.find("eşleşen araç yok") != std::string::npos);
    CHECK(empty_said.find("tools/list") != std::string::npos);

    // THE FIELD NARROWS, and the bus refuses a word that is not one of the
    // three before the body runs.
    CHECK(f.bus.execute_line("ARAÇARA sorgu=katman alan=ad", Origin::Test).ok());
    CHECK_FALSE(f.bus.execute_line("ARAÇARA sorgu=katman alan=ne", Origin::Test).ok());
    CHECK_FALSE(f.bus.execute_line("ARAÇARA sorgu=katman sinir=0", Origin::Test).ok());
    CHECK_FALSE(f.bus.execute_line("ARAÇARA sorgu=katman sinir=9999", Origin::Test).ok());

    // AND IT IS A READ. An agent may call it without anybody approving
    // anything, which is only true because it carries `NoEffect`.
    const CommandSpec* spec = f.reg.by_id("core.tool_search");
    REQUIRE(spec != nullptr);
    CHECK(has_flag(spec->flags, Flags::NoEffect));
    CHECK(has_flag(spec->flags, Flags::AiAccessible));
}

TEST_CASE("M-07: istemci depoları ayrıdır ve en eskisi düşer")
{
    ai::HandleScopes scopes;

    // ASKING CREATES, so the first call from a client is not a special case at
    // the call site.
    CHECK(scopes.peek("bir") == nullptr);
    (void)scopes.for_client("bir");
    CHECK(scopes.peek("bir") != nullptr);
    CHECK_EQ(scopes.clients(), 1u);

    // Asking twice is the same store, or a client would lose its handles
    // between two calls of one turn.
    const std::string minted =
        scopes.for_client("bir").mint_points({core::Point2{5, 5}}, "sorgula", 3).id;
    CHECK(scopes.for_client("bir").find(minted) != nullptr);

    // BOUNDED, because a caller that keeps inventing names must not grow the
    // program's memory. The cap is reached exactly once `bir` plus fifteen
    // others are held; the sixteenth newcomer is what pushes one out, and the
    // one pushed out is the FIRST opened — `bir` — never whichever store was
    // cheapest to drop.
    for (std::size_t i = 0; i + 1 < ai::HandleScopes::kMaxClients; ++i)
        (void)scopes.for_client("istemci-" + std::to_string(i));
    CHECK_EQ(scopes.clients(), ai::HandleScopes::kMaxClients);
    CHECK(scopes.peek("bir") != nullptr);

    (void)scopes.for_client("bir-fazla");
    CHECK_EQ(scopes.clients(), ai::HandleScopes::kMaxClients);
    CHECK(scopes.peek("bir") == nullptr);
    CHECK(scopes.peek("istemci-0") != nullptr);
    CHECK(scopes.peek("bir-fazla") != nullptr);

    // A DROPPED CLIENT IS NOT REFUSED. It gets a fresh store and mints again —
    // the same recovery a stale handle already asks for.
    CHECK(scopes.for_client("bir").find(minted) == nullptr);
    CHECK(scopes.for_client("bir").size() == 0u);
}

TEST_CASE("M-07: bir planın sahibi vardır, masadaki kişi hepsini görür")
{
    ai::PlanStore plans;

    ai::Plan mine;
    mine.requester = "Ajan A";
    ai::PlanStep step;
    step.command_id = "core.layer";
    step.line       = "KATMAN ad=parsel";
    mine.steps.push_back(step);
    const std::string id = plans.add(std::move(mine));

    // The owner reaches it; another client does not, and gets a NULL rather
    // than a different error — telling the two apart would say that somebody
    // else's plan exists.
    CHECK(plans.find_for(id, "Ajan A") != nullptr);
    CHECK(plans.find_for(id, "Ajan B") == nullptr);

    // AN EMPTY LABEL IS THE PERSON AT THE KEYBOARD, who applies the plans and
    // therefore has to see every one of them (`ai::ClientScope::client`).
    CHECK(plans.find_for(id, "") != nullptr);

    // Appending follows the same rule, and that is the clause that matters: the
    // approval a person gives is for the lines they READ.
    ai::PlanStep intruder;
    intruder.command_id = "core.erase";
    intruder.line       = "SİL nesneler=11";
    CHECK_FALSE(plans.append_for(id, "Ajan B", intruder));
    CHECK_EQ(plans.find(id)->steps.size(), 1u);
    CHECK(plans.append_for(id, "Ajan A", intruder));
    CHECK_EQ(plans.find(id)->steps.size(), 2u);
}

TEST_CASE("Öneri defteri: ekle, adım ekle, tek karar")
{
    ai::PlanStore plans;

    ai::Plan plan;
    plan.requester = "sınama istemcisi";
    plan.steps.push_back(ai::PlanStep{"core.line", Args{}, "ÇİZGİ @abc.0 @abc.1", {}});
    const std::string id = plans.add(std::move(plan));
    REQUIRE(plans.find(id) != nullptr);
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Pending);

    // A SEQUENCE THAT ONE APPROVAL APPLIES (ai.md R4): the agent composes, the
    // person decides once.
    REQUIRE(plans.append(id, ai::PlanStep{"core.layer", Args{}, "KATMAN ad=YOL", {}}).ok());
    CHECK_EQ(plans.find(id)->steps.size(), std::size_t{2});
    CHECK_EQ(plans.pending().size(), std::size_t{1});

    REQUIRE(plans.settle(id, ai::PlanState::Applied).ok());
    CHECK(plans.pending().empty());

    // ONE DECISION PER PLAN. A second would mean either applying something twice
    // or recording two answers to one question.
    CHECK(!plans.settle(id, ai::PlanState::Rejected).ok());
    CHECK(!plans.append(id, ai::PlanStep{"core.layer", Args{}, "KATMAN ad=X", {}}).ok());

    // The answer a client receives says, every time, that nothing was applied.
    ai::Plan second;
    second.steps.push_back(ai::PlanStep{"core.line", Args{}, "ÇİZGİ @abc.0 @abc.1", {}});
    const std::string open = plans.add(std::move(second));
    const core::Json told  = plans.find(open)->to_json();
    CHECK_EQ(told.find("durum")->as_string(), std::string("beklemede"));
    CHECK(told.find("aciklama")->as_string().find("uygulanmadı") != std::string::npos);
    CHECK_EQ(told.find("adimlar")->as_array().size(), std::size_t{1});
}

TEST_CASE("Onay kapısı: uygulanan öneri tek adım, reddedilen hiçbir şey")
{
    Rig f;
    seed(f);

    std::vector<std::string> log;
    ai::AuditLog audit([&log](const std::string& line) { log.push_back(line); });
    ai::PlanStore plans;

    // The runner is what the application supplies: one batch, every step through
    // the bus, all or nothing.
    ai::Gate gate(plans, audit, [&f](const ai::Plan& plan) -> core::Status {
        if (auto opened = f.bus.begin_batch("Yapay zeka önerisi"); !opened) return opened.error();
        for (const ai::PlanStep& step : plan.steps) {
            auto ran = f.bus.dispatch(Invocation{step.command_id, step.args, Origin::Ai});
            if (!ran) {
                f.bus.abort_batch();
                return ran.error();
            }
        }
        auto done = f.bus.end_batch();
        if (!done) return done.error();
        return core::ok();
    });

    // GEOMETRY, NOT LAYERS, and the difference is not cosmetic: creating a layer
    // is deliberately NOT an undo step in this program (`Transaction::ensure_layer`
    // — an empty layer is inert, and undoing its creation would invalidate stored
    // ids). A plan of two `KATMAN` calls therefore leaves the undo stack where it
    // was, which is correct and which is exactly how the first cut of this case
    // managed to "prove" one undo entry while proving nothing.
    Args first;
    first.set("noktalar", Value::points({core::Point2{0, 0}, core::Point2{5000, 0}}));
    Args second;
    second.set("noktalar", Value::points({core::Point2{0, 1000}, core::Point2{5000, 1000}}));

    ai::Plan plan;
    plan.requester = "sınama";
    plan.prompt    = "iki çizgi çiz";
    plan.steps.push_back(ai::PlanStep{"core.line", first, "ÇİZGİ 0,0 5,0", {}});
    plan.steps.push_back(ai::PlanStep{"core.line", second, "ÇİZGİ 0,1 5,1", {}});
    const std::string id = plans.add(std::move(plan));

    const std::size_t undo_before  = f.undo.undo_depth();
    const std::size_t count_before = f.doc.live_entity_count();
    REQUIRE(gate.decide(gate.approve(id, "Harita Mühendisi", ai::Decision::Apply, 1700000000000,
                                     "her_degisiklikte"))
                .ok());

    // TWO COMMANDS, ONE UNDO ENTRY (ai.md R4). Eleven would be the same one
    // click and the same one Ctrl+Z.
    CHECK_EQ(f.undo.undo_depth(), undo_before + 1);
    CHECK_EQ(f.doc.live_entity_count(), count_before + 2);
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Applied);

    // And one step back takes BOTH lines with it, which is the promise the whole
    // approval model rests on.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), count_before);

    // THE RECORD HAS ALL SIX R6 FIELDS, and it is written at the moment of the
    // decision rather than at the end of the session.
    REQUIRE_EQ(log.size(), std::size_t{1});
    auto parsed = core::Json::parse(log.front());
    REQUIRE(parsed.ok());
    const core::Json& record = parsed.value();
    for (const char* key : {"sürüm", "kayit", "oneri", "zaman_utc_ms", "karar", "kullanici",
                            "isteyen", "istem", "komutlar"})
        CHECK(record.find(key) != nullptr);
    CHECK_EQ(record.find("karar")->as_string(), std::string("uygula"));
    CHECK_EQ(record.find("kullanici")->as_string(), std::string("Harita Mühendisi"));
    CHECK_EQ(record.find("komutlar")->as_array().size(), std::size_t{2});

    // ---- WHICH PERMISSION CARRIED OUT THIS WORK (TODOS S-06) ---------------
    //
    // WRITTEN NOW, WHILE THERE IS ONLY ONE ANSWER. Today only a person can
    // decide (CLAUDE.md 5.7), so every line says `insan` — and that is exactly
    // why it is written rather than left implicit: a record that says nothing
    // cannot be told apart from one written after the rule changes, and
    // "otomatik işlem insan tıklaması gibi yazılmasın" would be unenforceable in
    // retrospect.
    REQUIRE(record.find("karar_veren") != nullptr);
    CHECK_EQ(record.find("karar_veren")->as_string(), std::string("insan"));

    // AND THE RULE IT WAS MADE UNDER. A decision is only explicable against the
    // policy in force at the time, and that setting may change twice before
    // anybody reads the log.
    REQUIRE(record.find("onay_politikasi") != nullptr);
    CHECK_EQ(record.find("onay_politikasi")->as_string(), std::string("her_degisiklikte"));

    // NO SECRET REACHES THE RECORD, whatever was added to it (CLAUDE.md 5.21).
    const std::string line = log.front();
    for (const char* forbidden : {"Bearer", "api_key", "apiKey", "password", "parola"})
        CHECK(line.find(forbidden) == std::string::npos);
}

TEST_CASE("S-04: onay karttaki satırlara bağlıdır, öneri kimliğine değil")
{
    // A PERSON APPROVES THE COMMAND LINES THEY READ. The plan is looked up again
    // when the decision is carried out, and between those two moments the client
    // that filed it can APPEND a step — that is how a sequence becomes one undo
    // entry. A card showing two lines must never apply three, and the audit
    // record must never say the engineer approved the third (TODOS S-04).
    Rig f;
    seed(f);

    std::vector<std::string> log;
    ai::AuditLog audit([&log](const std::string& line) { log.push_back(line); });
    ai::PlanStore plans;
    ai::Gate gate(plans, audit, [&f](const ai::Plan& plan) -> core::Status {
        for (const ai::PlanStep& step : plan.steps)
            if (auto ran = f.bus.dispatch(Invocation{step.command_id, step.args, Origin::Ai}); !ran)
                return ran.error();
        return core::ok();
    });

    Args first;
    first.set("ad", Value::text("ONAYLANAN"));
    ai::Plan plan;
    plan.requester = "Ajan A";
    plan.steps.push_back(ai::PlanStep{"core.layer", first, "KATMAN ad=ONAYLANAN", {}});
    const std::string id = plans.add(std::move(plan));

    // THE CARD READS THE PLAN and remembers what it drew.
    const std::uint64_t as_drawn = plans.find(id)->content_fingerprint();
    CHECK(as_drawn != 0);

    // THEN THE CLIENT APPENDS. Legitimately — it owns the plan — but after the
    // person has already read the card.
    Args sneaked;
    sneaked.set("ad", Value::text("OKUNMAYAN"));
    REQUIRE(plans.append_for(id, "Ajan A",
                             ai::PlanStep{"core.layer", sneaked, "KATMAN ad=OKUNMAYAN", {}}));
    CHECK_NE(plans.find(id)->content_fingerprint(), as_drawn);

    // THE APPROVAL IS REFUSED, not trimmed: the honest answer is a fresh card
    // showing what the plan says now.
    const core::Status decided = gate.decide(gate.approve(
        id, "Mühendis", ai::Decision::Apply, 1700000000000, "her_degisiklikte", as_drawn));
    CHECK_FALSE(decided);
    CHECK(decided.error().message.find("adım eklenmiş") != std::string::npos);

    // NOTHING WAS APPLIED and the plan is still waiting — a refusal here is not
    // a decision, so the person can still answer the fresh card.
    CHECK_EQ(f.doc.find_layer("ONAYLANAN"), core::kNoLayer);
    CHECK_EQ(f.doc.find_layer("OKUNMAYAN"), core::kNoLayer);
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Pending);

    // AND WITH THE CURRENT FINGERPRINT — which is what a redrawn card carries —
    // it applies, both steps, because now the person has read both.
    const std::uint64_t now = plans.find(id)->content_fingerprint();
    REQUIRE(gate.decide(
        gate.approve(id, "Mühendis", ai::Decision::Apply, 1700000000001, "her_degisiklikte", now)));
    CHECK_NE(f.doc.find_layer("ONAYLANAN"), core::kNoLayer);
    CHECK_NE(f.doc.find_layer("OKUNMAYAN"), core::kNoLayer);

    // A CALLER THAT MAKES NO CLAIM IS NOT SILENTLY TRUSTED — it simply is not
    // claiming, and the check does not run. That is the seam for a caller with
    // no card behind it; the card always claims.
    Args third;
    third.set("ad", Value::text("İDDİASIZ"));
    ai::Plan other;
    other.steps.push_back(ai::PlanStep{"core.layer", third, "KATMAN ad=İDDİASIZ", {}});
    const std::string loose = plans.add(std::move(other));
    CHECK(gate.decide(gate.approve(loose, "Mühendis", ai::Decision::Apply, 1700000000002)));
}

TEST_CASE("Onay kapısı: ret de kayda geçer, çizim değişmez")
{
    Rig f;
    seed(f);

    std::vector<std::string> log;
    ai::AuditLog audit([&log](const std::string& line) { log.push_back(line); });
    ai::PlanStore plans;
    bool ran = false;
    ai::Gate gate(plans, audit, [&ran](const ai::Plan&) {
        ran = true;
        return core::ok();
    });

    Args args;
    args.set("ad", Value::text("OLMAYACAK"));
    ai::Plan plan;
    plan.steps.push_back(ai::PlanStep{"core.layer", args, "KATMAN ad=OLMAYACAK", {}});
    const std::string id = plans.add(std::move(plan));

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(gate.decide(gate.approve(id, "Mühendis", ai::Decision::Reject, 1700000000000)).ok());

    // ai.md R20: a rejected suggestion leaves the document bit-identical.
    CHECK_FALSE(ran);
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Rejected);

    // R6 again: "Rejected suggestions MUST be recorded too" — because the
    // interesting question months later is often what the engineer refused.
    REQUIRE_EQ(log.size(), std::size_t{1});
    auto parsed = core::Json::parse(log.front());
    REQUIRE(parsed.ok());
    CHECK_EQ(parsed.value().find("karar")->as_string(), std::string("reddet"));
}

TEST_CASE("Onay kapısı: uygulama reddedilirse hiçbir adım kalmaz")
{
    Rig f;
    seed(f);

    std::vector<std::string> log;
    ai::AuditLog audit([&log](const std::string& line) { log.push_back(line); });
    ai::PlanStore plans;

    ai::Gate gate(plans, audit, [&f](const ai::Plan& plan) -> core::Status {
        if (auto opened = f.bus.begin_batch("Yapay zeka önerisi"); !opened) return opened.error();
        for (const ai::PlanStep& step : plan.steps) {
            auto dispatched = f.bus.dispatch(Invocation{step.command_id, step.args, Origin::Ai});
            if (!dispatched) {
                f.bus.abort_batch();
                return dispatched.error();
            }
        }
        auto done = f.bus.end_batch();
        if (!done) return done.error();
        return core::ok();
    });

    Args good;
    good.set("noktalar", Value::points({core::Point2{0, 0}, core::Point2{5000, 0}}));
    Args bad; // one point where two are declared: the bus refuses it before the body runs

    ai::Plan plan;
    plan.steps.push_back(ai::PlanStep{"core.line", good, "ÇİZGİ 0,0 5,0", {}});
    plan.steps.push_back(ai::PlanStep{"core.line", bad, "ÇİZGİ 9,9", {}});
    const std::string id = plans.add(std::move(plan));

    const std::uint64_t before  = f.doc.content_hash();
    const std::size_t was_alive = f.doc.live_entity_count();
    CHECK(!gate.decide(gate.approve(id, "Mühendis", ai::Decision::Apply, 1700000000000)).ok());

    // ALL OR NOTHING (Article 1.6, ai.md R5, R20): the first step succeeded and
    // is gone again, so the drawing is bit-identical to what it was. A plan that
    // half-applied would be a parcel with one boundary.
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(f.doc.live_entity_count(), was_alive);
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Failed);
    REQUIRE_EQ(log.size(), std::size_t{1});
    CHECK(core::Json::parse(log.front()).value().find("sonuc") != nullptr);
}

TEST_CASE("Katman yaratmak geri alma adımı değildir — öneride de değil")
{
    // A NUANCE A USER MUST BE TOLD ABOUT, and it is not the AI layer's doing:
    // `Transaction::ensure_layer` is deliberately outside the undo record
    // (core.md — an empty layer is inert, and undoing its creation would
    // invalidate ids stored against it). So a suggestion that opened a layer and
    // drew on it gives back the drawing on one Ctrl+Z and leaves the empty layer
    // behind, exactly as the same two commands typed by hand would. This case
    // exists so that nobody "fixes" it inside the approval path and so that the
    // manual can state it (docs/yapay-zeka/onay.md).
    Rig f;
    const std::size_t undo_before = f.undo.undo_depth();
    f.must("KATMAN ad=YENİ");
    CHECK_EQ(f.undo.undo_depth(), undo_before);
    CHECK(f.doc.find_layer("YENİ") != core::kNoLayer);
}

TEST_CASE("Koordinat reddi denetim kaydına yazılır")
{
    std::vector<std::string> log;
    ai::AuditLog audit([&log](const std::string& line) { log.push_back(line); });

    // ai.md R10 asks for this one by name: a coordinate that does not trace to a
    // tool-call result is rejected BEFORE validation "with the rejection
    // audit-logged". It never becomes a plan, so without this the most
    // interesting refusal the program makes would leave no trace at all.
    const std::string id = audit.write_coordinate_refusal(
        "sınama istemcisi", "core_line", "noktalar doğrudan sayı taşıdı", 1700000000000);
    CHECK_FALSE(id.empty());
    REQUIRE_EQ(log.size(), std::size_t{1});
    auto parsed = core::Json::parse(log.front());
    REQUIRE(parsed.ok());
    CHECK_EQ(parsed.value().find("karar")->as_string(), std::string("koordinat_reddi"));
    CHECK_EQ(parsed.value().find("isteyen")->as_string(), std::string("sınama istemcisi"));
}

TEST_CASE("ÖNERİ ve MCPSUNUCU motor yokken dürüst konuşur")
{
    Rig f;

    // NOTHING ATTACHED IS AN ANSWER, not a crash and not a pretence — the same
    // shape `YAZDIR` uses when no print engine is wired (commands/print.cpp).
    auto listed = f.bus.execute_line("ÖNERİ islem=listele", Origin::Test);
    CHECK(!listed.ok());
    if (!listed.ok()) CHECK(listed.error().message.find("bağlı değil") != std::string::npos);

    auto server = f.bus.execute_line("MCPSUNUCU islem=durum", Origin::Test);
    CHECK(!server.ok());

    // The words are declared, so the bus refuses an unknown one with the list.
    auto wrong = f.bus.execute_line("MCPSUNUCU islem=parlat", Origin::Test);
    CHECK(!wrong.ok());
    if (!wrong.ok()) CHECK(wrong.error().message.find("baslat") != std::string::npos);

    // And the port is a declared range.
    CHECK(!f.bus.execute_line("MCPSUNUCU islem=baslat port=42", Origin::Test).ok());
}

TEST_CASE("Bağlam: üzerinde çalışılan her şeyi tek çağrıda söyler")
{
    // TODOS A-01's acceptance: "bunu A3'e yerleştir" has to resolve against the
    // one valid selection and the one layout without asking the user to pick
    // anything. An agent that must call five narrow tools first spends its first
    // turn discovering that the drawing has one layout and nothing selected.
    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=parsel", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ALAN noktalar=0,0 100,0 100,80 0,80", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇIKTIYERLEŞİMİ islem=ekle ad=Kroki kagit=A3", Origin::Test).ok());

    auto said = rig.bus.execute_line("BAĞLAM", Origin::Test);
    REQUIRE(said.ok());
    const core::Json& out = said.value().report;

    REQUIRE(out.find("nesne_sayisi") != nullptr);
    CHECK_EQ(out.find("nesne_sayisi")->as_int(), 1);
    REQUIRE(out.find("katmanlar") != nullptr);
    CHECK(!out.find("katmanlar")->as_array().empty());

    // THE LAYOUT, AND WHETHER IT IS AIMED. A layout that exists and looks at
    // nothing prints an empty box, and finding that out after the PDF is written
    // is finding it out too late.
    REQUIRE(out.find("cikti_yerlesimleri") != nullptr);
    REQUIRE_EQ(out.find("cikti_yerlesimleri")->as_array().size(), std::size_t{1});
    const core::Json& sheet = out.find("cikti_yerlesimleri")->as_array()[0];
    CHECK_EQ(sheet.find("ad")->as_string(), "Kroki");
    CHECK_FALSE(sheet.find("hedefli")->as_bool());

    // Aim it and the answer changes.
    REQUIRE(rig.bus
                .execute_line("ÇIKTIÖĞE islem=ayarla ad=harita pencere=0,0 pencere=100,80",
                              Origin::Test)
                .ok());
    auto again = rig.bus.execute_line("BAĞLAM", Origin::Test);
    REQUIRE(again.ok());
    CHECK(
        again.value().report.find("cikti_yerlesimleri")->as_array()[0].find("hedefli")->as_bool());

    // NO WINDOW IS AN HONEST ANSWER, not an invented rectangle: a headless run
    // has none, and guessing would put an agent's next drawing somewhere nobody
    // was looking.
    REQUIRE(again.value().report.find("gorunum") != nullptr);
    CHECK(again.value().report.find("gorunum")->is_null());

    // AND IT SUMMARISES RATHER THAN DUMPS: no geometry, no attribute rows.
    CHECK(out.find("nesneler") == nullptr);
    CHECK(out.find("geometri") == nullptr);
}

TEST_CASE("Öneri: ajan durumunu okuyabilir, uygulayamaz")
{
    // TODOS M-04's read half. An agent that proposes and can never see what came
    // of it has to guess, and a client that guesses re-proposes — which is how a
    // careful protocol turns into eleven duplicate suggestions on a screen.
    //
    // The write half is refused from every command path, not by a check on WHO is
    // calling: `AiService` answers `uygula` with "a suggestion is applied by the
    // button on its card", so the command line and an agent get the same answer.
    Rig rig;
    const CommandSpec* spec = rig.reg.by_id("core.suggestion");
    REQUIRE(spec != nullptr);
    CHECK(has_flag(spec->flags, Flags::AiAccessible));

    // READING IS A READ, in the effect the policy will read.
    Args listing;
    listing.set("islem", Value::text("listele"));
    CHECK(has_effect(effect_of(*spec, listing), Effect::Query));
    CHECK_FALSE(has_effect(effect_of(*spec, listing), Effect::DocumentEdit));

    Args status;
    status.set("islem", Value::text("durum"));
    CHECK(has_effect(effect_of(*spec, status), Effect::Query));

    // APPLYING IS NOT, and the worst case of the command as a whole says so.
    Args applying;
    applying.set("islem", Value::text("uygula"));
    CHECK(has_effect(effect_of(*spec, applying), Effect::DocumentEdit));

    // AND IT IS REFUSED IN FACT, not only classified: no suggestion book is
    // wired in a headless run, and even with one the answer names the card.
    auto tried = rig.bus.execute_line("ÖNERİ islem=uygula oneri=p0000000000000001", Origin::Test);
    CHECK_FALSE(tried.ok());
}
