// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE TURKISH END-TO-END EVALUATION SET, and what a test can honestly say about it
// (TODOS A-08, kentoscad.md §5.6/§14).
//
// THE SET ANSWERS TWO QUESTIONS AND MUST NOT CONFUSE THEM.
//
// 1. IS THE EXPECTED SEQUENCE STILL VALID? Every step is parsed and resolved
//    against the LIVE registry. A set that named a command which has since been
//    renamed, or an argument it no longer takes, would be a set that rots
//    quietly — and a rotten answer key measures nothing. This is what makes the
//    file a test rather than a document.
//
// 2. DO THE THREE CLIENTS PRODUCE THE SAME THING? Each scenario is run from the
//    command line, from a JSON script and as an AI plan, and the document's
//    content hash and the journal's lines must match across all three. That is
//    A-08's acceptance sentence — "bölüm 9'daki ortak fixture'lar GUI, sohbet ve
//    MCP için aynı beklenen proje/çıktı durumunu üretir" — and it is the half a
//    test can actually prove.
//
// WHAT IS NOT MEASURED HERE, and cannot be: whether a MODEL produces these
// sequences. No test in this program calls a live provider (.claude/ai.md P10).
// The set defines the right answer; measuring a model against it is a separate
// runner with a provider behind it, and its accuracy baseline belongs there.
//
// THE COUNT IS REPORTED rather than asserted at 200. CLAUDE.md Article 8.9 says
// the shortfall must be VISIBLE rather than assumed, so the case count is
// printed on every run and the floor only guards against the set being emptied.
#include "kentos_test.hpp"

#include "kentos_cad/ai/commands.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/command/registry.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include "kentos_cad/script/json_runner.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

struct Case
{
    std::string id;
    std::string request;
    std::vector<std::string> terms;
    std::vector<std::string> setup;
    std::vector<std::string> commands;
};

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
};

/// The comparable part of a journal: what was commanded, with what arguments.
/// `origin` and `ts` are excluded — they are the only two fields that may
/// legitimately differ between clients (`test_proof.cpp` makes the same cut).
std::string what_happened(const Journal& j)
{
    std::string out;
    for (const auto& e : j.entries()) {
        out += e.command_id;
        out += ' ';
        out += e.args.to_json().dump();
        out += '\n';
    }
    return out;
}

std::vector<std::string> strings_at(const core::Json& object, const char* key)
{
    std::vector<std::string> out;
    const core::Json* held = object.find(key);
    if (held == nullptr || !held->is_array()) return out;
    for (const core::Json& one : held->as_array())
        if (one.is_string()) out.push_back(one.as_string());
    return out;
}

std::vector<Case> load()
{
    // THE SET IS A TEST FIXTURE, so it lives with the tests and its directory is
    // handed in by CMake — the same way the golden, journal and fuzz corpora are
    // found. It is deliberately NOT under `/data`: `/data` ships with the
    // program, and an answer key is not something a user installs.
    const std::string path = std::string(KENTOS_EVAL_DIR) + "/senaryolar.json";
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "değerlendirme seti açılamadı: " << path);
    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    core::Result<core::Json> parsed = core::Json::parse(body);
    REQUIRE_MESSAGE(parsed.ok(), parsed.error().message);

    std::vector<Case> out;
    const core::Json* listed = parsed.value().find("senaryolar");
    REQUIRE(listed != nullptr);
    for (const core::Json& entry : listed->as_array()) {
        Case one;
        if (const core::Json* v = entry.find("id"); v != nullptr) one.id = v->as_string();
        if (const core::Json* v = entry.find("istek"); v != nullptr) one.request = v->as_string();
        one.terms    = strings_at(entry, "terimler");
        one.setup    = strings_at(entry, "kurulum");
        one.commands = strings_at(entry, "komutlar");
        out.push_back(std::move(one));
    }
    return out;
}

} // namespace

TEST_CASE("A-08: değerlendirme setindeki her dizi hâlâ geçerli")
{
    const std::vector<Case> cases = load();
    REQUIRE_FALSE(cases.empty());

    Rig f;
    for (const Case& one : cases) {
        CAPTURE(one.id);
        CHECK_FALSE(one.request.empty());
        CHECK_FALSE(one.commands.empty());
        CHECK_FALSE(one.terms.empty());

        for (const std::string& line : one.commands) {
            CAPTURE(line);
            core::Result<ParsedLine> read = parse_line(line);
            REQUIRE_MESSAGE(read.ok(), read.error().message);

            const CommandSpec* spec = f.reg.resolve(read.value().command);
            REQUIRE_MESSAGE(spec != nullptr, "bilinmeyen komut: " << read.value().command);

            // EVERY `anahtar=` IS A DECLARED PARAMETER. An answer key that named
            // an argument the command does not take would teach a model to send
            // it, and the bus would refuse every such call at run time.
            for (const Token& token : read.value().tokens) {
                if (token.kind != Token::Kind::KeyValue) continue;
                bool declared = false;
                for (const Param& param : spec->params)
                    if (core::turkish_key_equals(param.name, token.word) ||
                        (!param.was.empty() && core::turkish_key_equals(param.was, token.word)))
                        declared = true;
                CHECK_MESSAGE(declared, "tanımsız argüman: " << token.word << " — " << spec->id);
            }
        }
    }

    // THE COUNT IS REPORTED, NOT ASSERTED AT 200. CLAUDE.md Article 8.9 asks for
    // the shortfall to be visible rather than assumed; a test that failed until
    // somebody wrote two hundred cases would simply be disabled.
    MESSAGE("ai-eval: " << cases.size() << " senaryo (hedef 200-300, Article 8.9)");
    CHECK(cases.size() >= 10u);
}

TEST_CASE("A-08: üç istemci aynı belgeyi ve aynı günlüğü üretir")
{
    const std::vector<Case> cases = load();

    for (const Case& one : cases) {
        CAPTURE(one.id);

        // ---- client 1: the command line, typed by a surveyor -----------------
        Rig cli;
        for (const std::string& line : one.setup)
            REQUIRE_MESSAGE(cli.bus.execute_line(line, Origin::Test).ok(), one.id << " / " << line);

        const std::uint64_t before  = cli.doc.content_hash();
        const std::size_t setup_ops = cli.journal.entries().size();

        for (const std::string& line : one.commands) {
            CAPTURE(line);
            auto ran = cli.bus.execute_line(line, Origin::CommandLine);
            REQUIRE_MESSAGE(ran.ok(), ran.error().message);
        }

        // WHAT THE OTHER TWO CLIENTS SEND IS WHAT THE FIRST ONE RESOLVED — the
        // command id and its arguments, taken out of the journal. That is not a
        // shortcut: a script step and a plan step both carry RESOLVED arguments
        // (`{cmd, args}` and `PlanStep`), never a line, so sending them a line
        // would be comparing two different requests. And it puts every argument
        // through `Value::to_json`/`from_json` on the way, which is Article 1.4's
        // own claim under test.
        core::JsonArray steps;
        std::vector<Invocation> resolved;
        for (std::size_t i = setup_ops; i < cli.journal.entries().size(); ++i) {
            const auto& entry = cli.journal.entries()[i];
            core::Json step;
            step.set("cmd", core::Json::string(entry.command_id));
            step.set("args", entry.args.to_json());
            steps.push_back(std::move(step));
            resolved.push_back(Invocation{entry.command_id, entry.args, Origin::Ai});
        }
        REQUIRE_FALSE(resolved.empty());

        // ---- client 2: a JSON script ----------------------------------------
        Rig scr;
        for (const std::string& line : one.setup)
            REQUIRE(scr.bus.execute_line(line, Origin::Test).ok());
        {
            core::Json body;
            body.set("ad", core::Json::string("A-08 " + one.id));
            body.set("komutlar", core::Json::array(steps));

            script::JsonRunner runner(scr.bus, script::Sandbox::Project);
            auto ran = runner.run_text(body.dump());
            REQUIRE_MESSAGE(ran.ok(), one.id << ": " << ran.error().message);
        }

        // ---- client 3: an agent, through the same dispatch an applied plan
        //      takes (`AiService::applyPlan`: `Origin::Ai`, resolved arguments)
        Rig ai_side;
        for (const std::string& line : one.setup)
            REQUIRE(ai_side.bus.execute_line(line, Origin::Test).ok());
        for (const Invocation& step : resolved) {
            auto ran = ai_side.bus.dispatch(step);
            REQUIRE_MESSAGE(ran.ok(), one.id << ": " << ran.error().message);
        }

        // ---- the proof ------------------------------------------------------
        CHECK_MESSAGE(cli.doc.content_hash() == scr.doc.content_hash(),
                      one.id << ": komut satırı ile betik ayrı belge üretti");
        CHECK_MESSAGE(scr.doc.content_hash() == ai_side.doc.content_hash(),
                      one.id << ": betik ile ajan ayrı belge üretti");

        CHECK_MESSAGE(what_happened(cli.journal) == what_happened(scr.journal),
                      one.id << ": günlük satırları ayrı");
        CHECK_MESSAGE(what_happened(scr.journal) == what_happened(ai_side.journal),
                      one.id << ": günlük satırları ayrı");

        // AND THE SCENARIO ACTUALLY DID SOMETHING. A case whose commands left
        // the document untouched would pass every equality above for the worst
        // possible reason — three clients agreeing that nothing happened.
        CHECK_MESSAGE(cli.doc.content_hash() != before, one.id << ": hiçbir şey değişmedi");
    }
}
