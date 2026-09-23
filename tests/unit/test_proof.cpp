// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE PHASE-0 KEYSTONE PROOF (kentoscad.md §16.5).
//
// The Phase-0 proof kentoscad.md asks for: `ÇİZGİ` must run from the button, from
// the command line and from a JSON file, and leave the same document behind.
//
// Three clients — a GUI button feeding mouse clicks, a typed command line, and a
// JSON script — run the same command. This test asserts that all three produce a
// byte-identical document and a byte-identical journal entry. If that ever stops
// being true, the architecture has been broken and the build must fail.
#include <algorithm>
#include <functional>
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <filesystem>
#include <string>

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
    std::string echoed;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }
};

// Three vertices, exact in millimetres, in TUREF/TM30 (30th 3-degree zone).
constexpr core::Point2 kP0{485320150, 4310220400};
constexpr core::Point2 kP1{485370150, 4310250400};
constexpr core::Point2 kP2{485440861, 4310321111};

/// The same rig with the FILE ENGINE attached.
///
/// `YENİ` is a file command like `AÇ`: the spec and the body live in
/// /src/command and the work arrives through `Bus::on_file_request`, which
/// `io::FileService` installs (io/service.hpp). Without one, the command answers
/// "Dosya motoru bağlı değil" — which is itself the same answer for every client
/// and is checked in test_io.cpp.
struct FileRig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    std::string echoed;

    FileRig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }
};

/// The work a `YENİ` is meant to sweep away: a layer, a line on it, and a file
/// the drawing belongs to. Identical for every client, so the journal each one
/// carries into the YENİ is identical too.
void fill(FileRig& rig, const std::string& path, Origin origin)
{
    CHECK(rig.bus.execute_line("KATMAN ad=PARSEL", origin).ok());
    CHECK(rig.bus.execute_line("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400", origin).ok());
    CHECK(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", origin).ok());

    // The state the reset has to reach. A selection is session state and is never
    // journalled (model.md R43), so setting it here cannot make the three
    // journals differ — it only gives `YENİ` something to clear.
    REQUIRE(rig.doc.live_entity_count() == std::size_t{1});
    rig.bus.selection().add(rig.doc.entities().key[0]);
    CHECK(!rig.bus.selection().empty());
    CHECK(!rig.bus.on_current_file().empty());
    CHECK(rig.undo.undo_depth() > std::size_t{0});
}

/// The comparable part of a journal: what was commanded, with what arguments.
/// Deliberately excludes `origin` and `ts` — those are the only two fields that
/// may legitimately differ between clients.
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

} // namespace

TEST_CASE("PROOF: gui, command line and script produce identical state and journal")
{
    // ---- client 1: the GUI. A toolbar button starts the command, mouse clicks
    //      feed it points, ESC ends it. Exactly what MapCanvas does. ----
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÇİZGİ");
        CHECK(started.ok());
        auto& session = *started.value();

        CHECK(session.waiting());
        CHECK(session.prompt().message == "İlk nokta");

        CHECK(session.supply(Value::point(kP0)).ok());
        CHECK(session.waiting());
        CHECK(session.prompt().has_rubber_band);

        CHECK(session.supply(Value::point(kP1)).ok());
        CHECK(session.supply(Value::point(kP2)).ok());

        session.cancel(); // ESC
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line, typed by the user ----
    Rig cli;
    {
        auto r = cli.bus.execute_line(
            "ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400 485440.861,4310321.111",
            Origin::CommandLine);
        CHECK(r.ok());
    }

    // ---- client 3: a JSON script file ----
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Kanıt betiği",
            "komutlar": [
                {"cmd": "core.line",
                 "args": {"noktalar": [[485320150,4310220400],
                                       [485370150,4310250400],
                                       [485440861,4310321111]]}}
            ]
        })");
        CHECK(r.ok());
    }

    // ---- the proof ----
    CHECK_EQ(gui.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(cli.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(scr.doc.live_entity_count(), std::size_t{2});

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    // And each client is recorded honestly as itself.
    CHECK(gui.journal.entries().at(0).origin == Origin::Gui);
    CHECK(cli.journal.entries().at(0).origin == Origin::CommandLine);
    CHECK(scr.journal.entries().at(0).origin == Origin::Script);
}

TEST_CASE("PROOF: `@100<50` arayüzden, komut satırından ve betikten aynı belge, aynı günlük")
{
    // TODOS-CAD P0-6. The polar form is the one coordinate whose meaning depends
    // on a setting, so it is the one that could split the three clients: the
    // command line resolves it in `bind_tokens`, a script string in `dispatch`,
    // and a typed answer to a prompt in the controller — three call sites, one
    // `resolve_point`, one `Bus::angle_convention()`. Under the default semt +
    // grad, 50 grad is 45° from north: the north-east diagonal.
    constexpr core::Point2 kOrigin{0, 0};
    constexpr core::Point2 kDiagonal{70711, 70711};

    // ---- client 1: the GUI, answering the prompts with typed text exactly as
    //      the controller does — parsed, resolved against the rubber origin under
    //      the bus's convention, supplied as a point. ----
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÇİZGİ");
        REQUIRE(started.ok());
        auto& session = *started.value();

        for (const char* typed : {"0,0", "@100<50"}) {
            REQUIRE(session.waiting());
            auto answer = parse_line(std::string("YANIT ") + typed);
            REQUIRE(answer.ok());
            auto pt = resolve_point(answer.value().tokens.front(), session.prompt().rubber_origin,
                                    gui.bus.resolve_context());
            REQUIRE_MESSAGE(pt.ok(), pt.error().message);
            CHECK(session.supply(Value::point(pt.value())).ok());
        }
        session.cancel(); // ESC
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    CHECK(cli.bus.execute_line("ÇİZGİ 0,0 @100<50", Origin::CommandLine).ok());

    // ---- client 3: a JSON script carrying the coordinates as TEXT, the way the
    //      command line writes them, in metres ----
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Kutupsal kanıt",
            "komutlar": [
                {"cmd": "core.line", "args": {"noktalar": ["0,0", "@100<50"]}}
            ]
        })");
        REQUIRE_MESSAGE(r.ok(), r.error().message);
    }

    // ---- the proof ----
    for (Rig* rig : {&gui, &cli, &scr}) {
        REQUIRE_EQ(rig->doc.live_entity_count(), std::size_t{1});
        const auto& doc = rig->doc;
        const auto span = doc.geometry().rings_of(doc.entities().slot[0]);
        const auto xs   = doc.geometry().ring_xs(span.first);
        const auto ys   = doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{2});
        CHECK_EQ((core::Point2{xs[0], ys[0]}), kOrigin);
        CHECK_EQ((core::Point2{xs[1], ys[1]}), kDiagonal);
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    // The recorded arguments are byte-identical, and they carry the RESOLVED
    // millimetres — no `@`, no `<`, no grad — which is what keeps every journal
    // written before this change replaying unchanged. (`origin` is the one field
    // that legitimately differs, and `what_happened` above already leaves it out.)
    REQUIRE_EQ(cli.journal.entries().size(), std::size_t{1});
    const std::string recorded = cli.journal.entries().front().args.to_json().dump();
    CHECK_EQ(recorded, scr.journal.entries().front().args.to_json().dump());
    CHECK_EQ(recorded, gui.journal.entries().front().args.to_json().dump());
    CHECK(recorded.find("70711") != std::string::npos);
    CHECK(recorded.find('<') == std::string::npos);

    // ---- replay safety: the same journal reproduces the same document in a
    //      session holding the OTHER convention, because a journal argument is a
    //      resolved point and the convention never reaches it (journal.hpp). ----
    Rig replay;
    CHECK(replay.bus.execute_line("MOD kural matematik", Origin::Test).ok());
    CHECK(replay.bus.execute_line("AYAR açı_birimi derece", Origin::Test).ok());
    CHECK(replay.bus.angle_convention() ==
          core::AngleConvention{core::AngleUnit::Degree, core::AngleRule::Matematik});
    for (const auto& e : cli.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());

    // And the same TEXT, under another convention, is a different line — which
    // is the whole point of the setting, and why only live text changed meaning.
    // (50 grad is 45°, the one direction both RULES agree on, so the unit is
    // what is changed here: 50° counter-clockwise from east is not the diagonal.)
    Rig other;
    CHECK(other.bus.execute_line("MOD kural matematik", Origin::Test).ok());
    CHECK(other.bus.execute_line("AYAR açı_birimi derece", Origin::Test).ok());
    CHECK(other.bus.execute_line("ÇİZGİ 0,0 @100<50", Origin::CommandLine).ok());
    CHECK(other.doc.content_hash() != cli.doc.content_hash());
}

TEST_CASE("PROOF: NOKTA'nın her noktası günlüğe geçer, sonuncusu değil")
{
    // A COMMAND THAT AWAITS A LIST IN A LOOP RECORDS THE WHOLE LOOP.
    //
    // `NOKTA 1,1 2,2 3,3` placed three points and journalled one. The awaiter
    // records each value it resolves under the parameter it was asked for, and
    // `noktalar` is ONE list, so every point after the first replaced the one
    // before it: three entities in the document, `"noktalar":[3000,3000]` in the
    // journal, and a replay that reproduced neither. That is Article 6.4 broken
    // by a command that never wrote a line of journal code.
    //
    // ÇİZGİ hid it. Its body keeps a parallel vector and records the whole run
    // at the end (line.cpp), so the proof above passed while the mechanism
    // underneath it was wrong — which is why this case awaits the run and
    // asserts the RECORDED points, not only that three clients agree.
    constexpr core::Point2 kA{1000, 1000};
    constexpr core::Point2 kB{2000, 2000};
    constexpr core::Point2 kC{3000, 3000};

    // ---- client 1: the GUI. Three clicks, then ESC. ----
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("NOKTA");
        REQUIRE(started.ok());
        auto& session = *started.value();

        for (const core::Point2 at : {kA, kB, kC}) {
            REQUIRE(session.waiting());
            CHECK(session.prompt().message == "Nokta");
            CHECK(session.supply(Value::point(at)).ok());
        }
        session.cancel(); // ESC
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    CHECK(cli.bus.execute_line("NOKTA 1,1 2,2 3,3", Origin::CommandLine).ok());

    // ---- client 3: a JSON script ----
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Nokta kanıtı",
            "komutlar": [
                {"cmd": "core.point_draw",
                 "args": {"noktalar": [[1000,1000],[2000,2000],[3000,3000]]}}
            ]
        })");
        REQUIRE_MESSAGE(r.ok(), r.error().message);
    }

    // ---- the proof ----
    for (Rig* rig : {&gui, &cli, &scr})
        REQUIRE_EQ(rig->doc.live_entity_count(), std::size_t{3});

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    // ---- the clause that actually failed: replay (Article 6.4) ----
    Rig replay;
    for (const auto& e : cli.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());

    CHECK_EQ(replay.doc.live_entity_count(), std::size_t{3});
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
    // And the replay's own journal is the line it replayed, so the run is a
    // fixed point rather than something that decays a little on every pass.
    CHECK_EQ(what_happened(replay.journal), what_happened(cli.journal));

    // EVERY point, named. Three clients agreeing is not enough on its own here:
    // before the fix all three agreed, and all three were wrong the same way.
    REQUIRE_EQ(cli.journal.entries().size(), std::size_t{1});
    const Value run = cli.journal.entries().front().args.get("noktalar");
    CHECK(run.kind() == Value::Kind::PointList);
    REQUIRE_EQ(run.as_points().size(), std::size_t{3});
    CHECK_EQ(run.as_points()[0], kA);
    CHECK_EQ(run.as_points()[1], kB);
    CHECK_EQ(run.as_points()[2], kC);
}

TEST_CASE("PROOF: tek noktalık bir NOKTA da liste olarak kaydedilir")
{
    // The other half of the rule. A run's FIRST value replaces what the client
    // supplied up front and the rest extend it, so a one-point run has to come
    // out as a one-point LIST — not as the bare point the awaiter handed over,
    // and not as the preset it happened to be drained from. One shape for the
    // parameter, whatever the count, or a journal reader has two cases to know.
    constexpr core::Point2 kOnly{5000, 5000};

    Rig gui;
    {
        auto started = gui.bus.begin_interactive("NOKTA");
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting());
        CHECK(session.supply(Value::point(kOnly)).ok());
        session.cancel(); // ESC
        CHECK(gui.bus.finish(session).ok());
    }

    Rig cli;
    CHECK(cli.bus.execute_line("NOKTA 5,5", Origin::CommandLine).ok());

    CHECK_EQ(gui.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(cli.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));

    REQUIRE_EQ(cli.journal.entries().size(), std::size_t{1});
    const Value run = cli.journal.entries().front().args.get("noktalar");
    CHECK(run.kind() == Value::Kind::PointList);
    REQUIRE_EQ(run.as_points().size(), std::size_t{1});
    CHECK_EQ(run.as_points()[0], kOnly);
}

TEST_CASE("PROOF: `dik(...)` arayüzden, komut satırından ve betikten aynı belge, aynı günlük")
{
    // TODOS-CAD P1a. A point function is a CONSTRUCTION written where a
    // coordinate is written, and the three clients read it through the three
    // seams that resolve a coordinate: `bind_tokens` for a typed line,
    // `dispatch` for a script string, and the shell for an answer typed into a
    // running prompt. One `resolve_point`, one `Bus::resolve_context()` — and
    // if a fourth road to a point were ever added, this is the test that fails.
    //
    // The line drawn is the perpendicular a surveyor sets out every day: from
    // the base 0,0 → 100,0, thirty metres along and five to the right, then the
    // midpoint of the base. `boy` is negative because right is negative and
    // left is positive (P1a-6), which is what the command page says too.
    constexpr core::Point2 kFoot{30000, -5000};
    constexpr core::Point2 kMid{50000, 0};
    const char* const kTyped[] = {"dik(0,0,100,0,30,-5)", "orta(0,0,100,0)"};

    // ---- client 1: the GUI, answering each prompt with typed text exactly as
    //      the controller does ----
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÇİZGİ");
        REQUIRE(started.ok());
        auto& session = *started.value();

        for (const char* typed : kTyped) {
            REQUIRE(session.waiting());
            auto answer = parse_line(std::string("YANIT ") + typed);
            REQUIRE(answer.ok());
            REQUIRE(is_coordinate(answer.value().tokens.front()));
            auto pt = resolve_point(answer.value().tokens.front(), session.prompt().rubber_origin,
                                    gui.bus.resolve_context());
            REQUIRE_MESSAGE(pt.ok(), pt.error().message);
            CHECK(session.supply(Value::point(pt.value())).ok());
        }
        session.cancel(); // ESC
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    CHECK(
        cli.bus
            .execute_line(std::string("ÇİZGİ ") + kTyped[0] + " " + kTyped[1], Origin::CommandLine)
            .ok());

    // ---- client 3: a JSON script carrying the constructions as text ----
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        // A CUSTOM DELIMITER, because the script text contains `)"`: a point
        // function's closing bracket sits right before the JSON string's quote.
        auto r = runner.run_text(R"betik({
            "ad": "Dik ayak kanıtı",
            "komutlar": [
                {"cmd": "core.line",
                 "args": {"noktalar": ["dik(0,0,100,0,30,-5)", "orta(0,0,100,0)"]}}
            ]
        })betik");
        REQUIRE_MESSAGE(r.ok(), r.error().message);
    }

    // ---- the proof ----
    for (Rig* rig : {&gui, &cli, &scr}) {
        REQUIRE_EQ(rig->doc.live_entity_count(), std::size_t{1});
        const auto& doc = rig->doc;
        const auto span = doc.geometry().rings_of(doc.entities().slot[0]);
        const auto xs   = doc.geometry().ring_xs(span.first);
        const auto ys   = doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{2});
        CHECK_EQ((core::Point2{xs[0], ys[0]}), kFoot);
        CHECK_EQ((core::Point2{xs[1], ys[1]}), kMid);
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    // The journal holds the RESOLVED millimetres and no trace of the
    // construction — a replay must not need a grammar, a document or a session
    // setting to mean what it meant (CLAUDE.md 1.4).
    REQUIRE_EQ(cli.journal.entries().size(), std::size_t{1});
    const std::string recorded = cli.journal.entries().front().args.to_json().dump();
    CHECK_EQ(recorded, gui.journal.entries().front().args.to_json().dump());
    CHECK_EQ(recorded, scr.journal.entries().front().args.to_json().dump());
    CHECK(recorded.find("dik") == std::string::npos);
    CHECK(recorded.find("-5000") != std::string::npos);

    // ---- replay: the same journal rebuilds the same document ----
    Rig replay;
    for (const auto& e : cli.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
}

TEST_CASE("PROOF: undo collapses each client's run into exactly one step")
{
    Rig cli;
    CHECK(cli.bus.execute_line("ÇİZGİ 0,0 10,0 20,0 30,0", Origin::CommandLine).ok());
    CHECK_EQ(cli.undo.undo_depth(), std::size_t{1});

    Rig scr;
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);
    CHECK(runner
              .run_text(R"([
        {"cmd":"core.line","args":{"noktalar":[[0,0],[10000,0]]}},
        {"cmd":"core.line","args":{"noktalar":[[0,0],[0,10000]]}},
        {"cmd":"core.line","args":{"noktalar":[[10000,0],[10000,10000]]}}
    ])")
              .ok());

    // A whole script block is ONE undo step (kentoscad.md §2.5).
    CHECK_EQ(scr.doc.live_entity_count(), std::size_t{3});
    CHECK_EQ(scr.undo.undo_depth(), std::size_t{1});

    CHECK(scr.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(scr.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("PROOF: a failing script leaves nothing behind")
{
    Rig scr;
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);

    auto r = runner.run_text(R"([
        {"cmd":"core.line","args":{"noktalar":[[0,0],[10000,0]]}},
        {"cmd":"core.line","args":{"noktalar":[[0,0],[0,10000]]}},
        {"cmd":"core.yokboyle","args":{}}
    ])");

    CHECK(!r.ok());
    CHECK(r.error().message.find("core.yokboyle") != std::string::npos);

    // Half-applied cadastral or zoning edits are never acceptable (§2.5).
    CHECK_EQ(scr.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(scr.undo.undo_depth(), std::size_t{0});
}

TEST_CASE("PROOF: DİKAYAK gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.perp_offset`. A baseline along the east axis and two
    // details off it: (10 m along, 5 m left) and (30 m along, 5 m right). The
    // three clients differ only in the road the readings take — a session the
    // window drives with one answer per click and keystroke, a typed line with
    // repeated keys, and a JSON script with a run of numbers.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("DİKAYAK", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        REQUIRE(session.waiting());
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        CHECK(session.supply(Value::number(10.0)).ok());
        CHECK(session.supply(Value::number(5.0)).ok());
        CHECK(session.supply(Value::number(30.0)).ok());
        CHECK(session.supply(Value::number(-5.0)).ok());
        CHECK(gui.bus.finish(session).ok()); ///< ESC / right button ends the run
    }

    Rig cli;
    CHECK(
        cli.bus.execute_line("DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5", Origin::CommandLine)
            .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Dik ayak kanıtı",
            "komutlar": [ {"cmd": "core.perp_offset", "args": {
                "baslangic": [0, 0], "bitis": [100000, 0],
                "ayak": [10, 30], "boy": [5, -5] }} ]
        })");
        CHECK(r.ok());
    }

    // ---- the proof ----
    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{2});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1}); ///< one command, one step
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // AND THE POINTS ARE THE ONES A TAPE WOULD HAVE PUT THERE. Without this the
    // three could agree on the same wrong answer.
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(span.first)[0],
                           cli.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{10'000, 5'000}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    CHECK(what_happened(gui.journal).find("core.perp_offset") != std::string::npos);

    // THE RUN OF READINGS IS IN THE JOURNAL AS A RUN. A `Number` parameter used
    // to be the one list-shaped kind `bind_tokens` replaced instead of
    // accumulating, so the second pair never reached the document and never
    // reached this line either (`Value::Kind::NumberList`).
    const JournalEntry& wrote = cli.journal.entries().back();
    CHECK_EQ(wrote.args.get("ayak").as_numbers().size(), std::size_t{2});
    CHECK_EQ(wrote.args.get("boy").as_numbers().size(), std::size_t{2});
}

TEST_CASE("PROOF: ALIM gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.survey_polar`. Two readings off a station at the
    // origin, in the session's defaults — grad, clockwise from north — so 0 is
    // north and 100 is east and both answers are exact.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ALIM", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting());
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::number(0.0)).ok());
        CHECK(session.supply(Value::number(100.0)).ok());
        CHECK(session.supply(Value::number(100.0)).ok());
        CHECK(session.supply(Value::number(100.0)).ok());
        CHECK(gui.bus.finish(session).ok());
    }

    Rig cli;
    CHECK(cli.bus.execute_line("ALIM 0,0 aci=0 kenar=100 aci=100 kenar=100", Origin::CommandLine)
              .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Alım kanıtı",
            "komutlar": [ {"cmd": "core.survey_polar", "args": {
                "istasyon": [0, 0], "aci": [0, 100], "kenar": [100, 100] }} ]
        })");
        CHECK(r.ok());
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{2});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // AND THE COORDINATES A FIELD BOOK WOULD HAVE REDUCED TO. Without this the
    // three could agree on the same rotation error.
    const auto north = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(north.first)[0],
                           cli.doc.geometry().ring_ys(north.first)[0]}),
             (core::Point2{0, 100'000}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    CHECK(what_happened(gui.journal).find("core.survey_polar") != std::string::npos);
}

TEST_CASE("PROOF: DİKAYAK günlükten yeniden oynatılabilir")
{
    Rig live;
    REQUIRE(live.bus
                .execute_line("DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5 cizgi=evet",
                              Origin::CommandLine)
                .ok());
    const std::uint64_t golden = live.doc.content_hash();

    // Replayed into an empty drawing from the journal's own JSON, which is what a
    // crash recovery and a macro both do (kentoscad.md §16.5).
    Rig again;
    for (const auto& e : live.journal.entries()) {
        const auto ran = again.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        REQUIRE(ran.ok());
    }
    CHECK_EQ(again.doc.content_hash(), golden);
    CHECK_EQ(what_happened(live.journal), what_happened(again.journal));
}

TEST_CASE("PROOF: replaying a journal reproduces the document exactly")
{
    // This is the foundation of the nightly journal regression pack and of crash
    // recovery (kentoscad.md §2.2, §14).
    Rig original;
    CHECK(original.bus.execute_line("KATMAN ad=PARSEL", Origin::CommandLine).ok());
    CHECK(original.bus
              .execute_line("ÇİZGİ 485320.150,4310220.400 485380,4310250", Origin::CommandLine)
              .ok());
    CHECK(original.bus.execute_line("ÇİZGİ @0,0 @25.5,-13.25", Origin::CommandLine).ok());

    const auto path = std::filesystem::temp_directory_path() / "kentoscad-proof-journal.jsonl";
    std::filesystem::remove(path);

    {
        Journal sink;
        CHECK(sink.open_sink(path.string()).ok());
        for (const auto& e : original.journal.entries())
            sink.append(e);
        sink.flush();
        sink.close_sink();
    }

    auto reloaded = Journal::read_jsonl(path.string());
    CHECK(reloaded.ok());
    CHECK_EQ(reloaded.value().size(), original.journal.entries().size());

    Rig replay;
    for (const auto& e : reloaded.value()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }

    CHECK_EQ(replay.doc.content_hash(), original.doc.content_hash());
    std::filesystem::remove(path);
}

TEST_CASE("PROOF: the safe sandbox refuses filesystem access")
{
    Rig rig;
    script::JsonRunner safe(rig.bus, script::Sandbox::Safe);

    auto r = safe.run_file("/etc/hostname");
    CHECK(!r.ok());
    CHECK(static_cast<int>(r.error().code) == static_cast<int>(core::ErrorCode::Unsupported));
}

TEST_CASE("PROOF: YENİ gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.new`. The setup is identical for the three clients on
    // purpose — it is not what is under test — and only the road the YENİ takes
    // differs: a session the window drives, a typed line, and a JSON script.
    const auto path = std::filesystem::temp_directory_path() / "kentoscad-proof-new.pcad";
    std::filesystem::remove(path);

    // ---- client 1: the GUI. `Dosya ▸ Yeni`, Ctrl+N, or the `+` on the document
    //      tab strip; the window asks about unsaved work FIRST and then hands the
    //      bus the same line anyone else would (MainWindow::newProject). ----
    FileRig gui;
    fill(gui, path.string(), Origin::Gui);
    {
        auto started = gui.bus.begin_interactive("YENİ", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        // It asks nothing. That is the decision, not an omission: the question a
        // person needs has no answer in a batch run, so it lives in the window.
        CHECK(!session.waiting());
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line, typed by the user ----
    FileRig cli;
    fill(cli, path.string(), Origin::CommandLine);
    CHECK(cli.bus.execute_line("YENİ", Origin::CommandLine).ok());

    // ---- client 3: a JSON script ----
    FileRig scr;
    fill(scr, path.string(), Origin::Script);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Yeni çizim kanıtı",
            "komutlar": [ {"cmd": "core.new", "args": {}} ]
        })");
        CHECK(r.ok());
    }

    // ---- the proof ----
    for (const FileRig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{0});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{0});
        CHECK(rig->bus.selection().empty());
        CHECK(rig->bus.on_current_file().empty());
        CHECK_EQ(rig->bus.active_layer(), core::LayerId{0});
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // And it is the hash of a DRAWING NOBODY HAS TOUCHED. Without this the three
    // could agree on any shared wreckage.
    const core::Document untouched;
    CHECK_EQ(gui.doc.content_hash(), untouched.content_hash());

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    CHECK(what_happened(gui.journal).find("core.new") != std::string::npos);

    CHECK(gui.journal.entries().back().origin == Origin::Gui);
    CHECK(cli.journal.entries().back().origin == Origin::CommandLine);
    CHECK(scr.journal.entries().back().origin == Origin::Script);

    std::filesystem::remove(path);
}

TEST_CASE("PROOF: YENİ günlükten yeniden oynatılabilir")
{
    // A journal that dropped the YENİ would replay the commands of the SECOND
    // drawing on top of the first one. This is why `core.new` does not carry
    // `Flags::ReadOnly` — `Bus::finish` writes no journal entry for a command
    // that does.
    FileRig original;
    CHECK(original.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
    CHECK(original.bus.execute_line("YENİ", Origin::CommandLine).ok());
    CHECK(original.bus.execute_line("KATMAN ad=İMAR", Origin::CommandLine).ok());
    CHECK(original.bus.execute_line("ÇİZGİ 0,0 25.5,-13.25", Origin::CommandLine).ok());

    FileRig replay;
    for (const auto& e : original.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), original.doc.content_hash());
}

TEST_CASE("YENİ: neyi sıfırlar, neyi bırakır")
{
    FileRig rig;
    const auto path = std::filesystem::temp_directory_path() / "kentoscad-new-reset.pcad";
    std::filesystem::remove(path);
    fill(rig, path.string(), Origin::Test);

    // What belongs to the PERSON rather than to the drawing, set before the reset
    // so it can be shown to survive it — and one PROJECT setting, which must not.
    auto catalogue = core::SettingValue::text("data/catalogs/dxf/kendi-desenlerim.json");
    REQUIRE(catalogue.ok());
    CHECK(rig.bus.app_settings().set("core.tarama.desen_katalogu", catalogue.value()).ok());
    CHECK(rig.bus.project_settings().set("core.plan.olcek", core::SettingValue::integer(500)).ok());

    core::LibraryEntry shelf;
    shelf.id    = "deneme.sembol";
    shelf.label = "Deneme";
    rig.bus.style_library().add(shelf);
    const std::size_t shelf_size = rig.bus.style_library().size();

    CHECK(rig.bus.execute_line("YENİ", Origin::Test).ok());

    // ---- reset ----
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(rig.doc.find_layer("PARSEL"), core::kNoLayer);
    CHECK_EQ(rig.undo.undo_depth(), std::size_t{0});
    CHECK_EQ(rig.undo.redo_depth(), std::size_t{0});
    CHECK(rig.bus.on_current_file().empty());
    CHECK(rig.bus.selection().empty());
    CHECK_EQ(rig.bus.active_layer(), core::LayerId{0});

    // The drawing's OWN settings go with the drawing: back to the declared
    // default, not the last project's value (model.md R40).
    CHECK_EQ(rig.bus.project_settings().get("core.plan.olcek").as_int(), std::int64_t{1000});

    // ---- kept ----
    CHECK_EQ(rig.bus.app_settings().get("core.tarama.desen_katalogu").as_text(),
             std::string_view("data/catalogs/dxf/kendi-desenlerim.json"));
    CHECK_EQ(rig.bus.style_library().size(), shelf_size);

    std::filesystem::remove(path);
}

TEST_CASE("YENİ: yeni çizim eskisinin izleme işaretlerini devralmaz")
{
    // Two İZ marks made on one drawing drew their traces across every drawing
    // opened after it: nothing ever forgot them (see `Bus::finish`).
    FileRig rig;
    REQUIRE(rig.bus.execute_line("İZ 10,0", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("İZ 0,20", Origin::Test).ok());
    REQUIRE_EQ(rig.bus.tracking_marks().size(), std::size_t{2});
    REQUIRE(rig.bus.execute_line("YENİ", Origin::Test).ok());
    CHECK(rig.bus.tracking_marks().empty());
}

TEST_CASE("YENİ: betiğin ortasında çalışınca eski belgenin geri alma adımını bırakmaz")
{
    // A script is ONE merged undo step (§2.5), so a swap in the middle of one
    // leaves the batch holding the inverse of edits made to a document that has
    // gone. Pushed at `end_batch` it would become an undo entry that rewrites the
    // NEW drawing — in a cadastral file, over somebody else's parcel (model.md
    // R5). `Bus::document_replaced` drops those ops and re-bases the batch, which
    // is why the script below still completes and still merges.
    FileRig rig;
    script::JsonRunner runner(rig.bus, script::Sandbox::Project);

    auto r = runner.run_text(R"([
        {"cmd":"core.line","args":{"noktalar":[[0,0],[10000,0]]}},
        {"cmd":"core.new","args":{}},
        {"cmd":"core.line","args":{"noktalar":[[0,0],[0,10000]]}}
    ])");
    REQUIRE(r.ok());

    // Only what was drawn AFTER the swap is there, and it is one undo step.
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(rig.undo.undo_depth(), std::size_t{1});

    // And that step undoes into an EMPTY drawing, not into the departed one.
    CHECK(rig.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{0});
}

// ==================================== PANO (P6) ==============================

TEST_CASE("PANO: kopyala ve yapıştır aynı geometriyi yeni kimliklerle verir")
{
    const auto clip =
        (std::filesystem::temp_directory_path() / "kentoscad-pano-kanit.pcad").string();
    std::filesystem::remove(clip);

    FileRig source;
    REQUIRE(source.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("ALAN 0,0 40,0 40,30 0,30", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("ÇİZGİ 60,0 60,40", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("SEÇ mod=TÜMÜ", Origin::Test).ok());

    REQUIRE(source.bus.execute_line("PANOYAKOPYALA dosya=\"" + clip + "\"", Origin::Test).ok());
    CHECK(std::filesystem::exists(clip));

    // A COPY CHANGES NOTHING and leaves no undo step: it writes a file, and R43
    // is why that is a `File` command rather than an edit.
    CHECK_EQ(source.doc.live_entity_count(), std::size_t{2});
    const std::uint64_t untouched = source.doc.content_hash();

    // ---- pasted IN PLACE into another drawing: the same coordinates ----
    FileRig same;
    REQUIRE(
        same.bus.execute_line("YAPIŞTIR yerinde=evet dosya=\"" + clip + "\"", Origin::Test).ok());
    CHECK_EQ(same.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(same.doc.content_hash(), untouched); ///< byte for byte the same drawing

    // THE LAYER CAME WITH IT. A parcel pasted onto layer `0` is a parcel that
    // has lost what it is.
    CHECK(same.doc.find_layer("PARSEL") != core::kNoLayer);

    // ---- and pasted AT A POINT: moved, not re-placed ----
    FileRig moved;
    REQUIRE(moved.bus.execute_line("YAPIŞTIR nokta=1000,500 dosya=\"" + clip + "\"", Origin::Test)
                .ok());
    REQUIRE_EQ(moved.doc.live_entity_count(), std::size_t{2});
    const core::Box2 box = moved.doc.extent();
    CHECK_EQ(box.min_x, 1'000'000);
    CHECK_EQ(box.min_y, 500'000);
    // The shape is unchanged: 60 m wide and 40 m tall, wherever it landed.
    CHECK_EQ(box.max_x - box.min_x, 60'000);
    CHECK_EQ(box.max_y - box.min_y, 40'000);

    // ONE UNDO STEP. A paste of two entities that took two steps back would be a
    // paste the user could not take back (Article 1.5).
    CHECK_EQ(moved.undo.undo_depth(), std::size_t{1});
    REQUIRE(moved.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(moved.doc.live_entity_count(), std::size_t{0});

    std::filesystem::remove(clip);
}

TEST_CASE("PANO: KES panoya alır ve siler, tek geri alma adımıyla")
{
    const auto clip = (std::filesystem::temp_directory_path() / "kentoscad-pano-kes.pcad").string();
    std::filesystem::remove(clip);

    FileRig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ALAN 0,0 40,0 40,30 0,30", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 60,0 60,40", Origin::Test).ok());
    const auto key = static_cast<std::int64_t>(core::raw(rig.doc.entities().key[0]));

    REQUIRE(rig.bus
                .execute_line("KES nesneler=" + std::to_string(key) + " dosya=\"" + clip + "\"",
                              Origin::Test)
                .ok());
    CHECK(std::filesystem::exists(clip));
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{1}); ///< the parcel is gone

    // THE COPY AND THE ERASE ARE ONE STEP. A cut whose copy survived its own
    // undo would be a cut the user could not take back.
    REQUIRE(rig.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{2});

    // AND WHAT WAS CUT IS STILL ON THE CLIPBOARD, which is the whole point of a
    // cut: undoing the removal must not empty the clipboard the user is about to
    // paste from.
    FileRig into;
    REQUIRE(
        into.bus.execute_line("YAPIŞTIR yerinde=evet dosya=\"" + clip + "\"", Origin::Test).ok());
    CHECK_EQ(into.doc.live_entity_count(), std::size_t{1});

    std::filesystem::remove(clip);
}

TEST_CASE("PANO: boş seçim ve boş pano reddedilir, sebebiyle")
{
    FileRig rig;
    const auto nothing = rig.bus.execute_line("PANOYAKOPYALA", Origin::Test);
    CHECK_FALSE(nothing.ok());
    CHECK(nothing.error().message.find("nesne yok") != std::string::npos);

    const auto empty = rig.bus.execute_line(
        "YAPIŞTIR yerinde=evet dosya=\"/tmp/kentoscad-boyle-bir-pano-yok.pcad\"", Origin::Test);
    CHECK_FALSE(empty.ok());
    CHECK(empty.error().message.find("Panoda bir şey yok") != std::string::npos);
    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("PROOF: NESNEBİLGİ gui, komut satırı ve betikten aynı cevabı verir")
{
    // Article 6.4 for `core.entity_info`. A QUESTION has no document delta, so
    // the proof is the other half of the same claim: the three clients get the
    // SAME ANSWER and leave the drawing and the undo stack exactly as they found
    // them. A query that differed by client would be a query whose answer a
    // script could not trust.
    const char* kSetup[] = {"KATMAN ad=PARSEL", "ALAN 0,0 20,0 20,10 0,10"};

    Rig gui;
    Rig cli;
    Rig scr;
    for (Rig* rig : {&gui, &cli, &scr})
        for (const char* line : kSetup)
            REQUIRE(rig->bus.execute_line(line, Origin::Test).ok());

    const std::uint64_t before = cli.doc.content_hash();
    const std::size_t depth    = cli.undo.undo_depth();

    core::Json from_gui;
    {
        auto started = gui.bus.begin_interactive("NESNEBİLGİ", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting());
        CHECK(session.supply(Value::ids({1})).ok());
        auto done = gui.bus.finish(session);
        REQUIRE(done.ok());
        from_gui = done.value().report;
    }

    const auto typed = cli.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::CommandLine);
    REQUIRE(typed.ok());

    core::Json from_script;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Nesne bilgisi kanıtı",
            "komutlar": [ {"cmd": "core.entity_info", "args": {"nesneler": [1]}} ]
        })");
        REQUIRE(r.ok());
    }
    // The script road answers through the same dispatch, so the same call typed
    // by hand is the comparison that matters; the runner keeps no report.
    const auto again = scr.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Script);
    REQUIRE(again.ok());
    from_script = again.value().report;

    // ---- the proof: one answer, byte for byte ----
    CHECK_EQ(from_gui.dump(), typed.value().report.dump());
    CHECK_EQ(typed.value().report.dump(), from_script.dump());

    // 20 m × 10 m: the figure a tape would have read, so the three cannot agree
    // on the same wrong answer.
    const core::Json& row = typed.value().report.find("nesneler")->as_array().front();
    CHECK_EQ(row.find("alan_mm2")->as_int(), 200000000);
    CHECK_EQ(row.find("cevre_mm")->as_int(), 60000);
    CHECK_EQ(row.find("katman")->as_string(), std::string("PARSEL"));

    // ---- and nothing moved ----
    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.content_hash(), before);
        CHECK_EQ(rig->undo.undo_depth(), depth);
    }
}

TEST_CASE("PROOF: AÇIÖLÇ gui, komut satırı ve betikten aynı açıyı okur")
{
    // Article 6.4 for `core.measure_angle`, and the reading is the one a hand
    // would take: the vertex at the origin, one arm north, one arm east. Under
    // the default convention — grad, semt, clockwise from north — the sweep from
    // the first arm to the second is a quarter turn, 100 grad.
    Rig gui;
    core::Json from_gui;
    {
        auto started = gui.bus.begin_interactive("AÇIÖLÇ", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{0, 10'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{10'000, 0})).ok());
        auto done = gui.bus.finish(session);
        REQUIRE(done.ok());
        from_gui = done.value().report;
    }

    Rig cli;
    const auto typed = cli.bus.execute_line("AÇIÖLÇ 0,0 0,10 10,0", Origin::CommandLine);
    REQUIRE(typed.ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Açı ölçüsü kanıtı",
            "komutlar": [ {"cmd": "core.measure_angle", "args": {
                "tepe": [0, 0], "birinci": [0, 10000], "ikinci": [10000, 0] }} ]
        })");
        REQUIRE(r.ok());
    }
    const auto again = scr.bus.execute_line("AÇIÖLÇ 0,0 0,10 10,0", Origin::Script);
    REQUIRE(again.ok());

    // ---- the proof ----
    CHECK_EQ(from_gui.dump(), typed.value().report.dump());
    CHECK_EQ(typed.value().report.dump(), again.value().report.dump());

    CHECK_EQ(typed.value().report.find("aci_udeg")->as_int(), core::kUDegFullCircle / 4);
    CHECK_EQ(typed.value().report.find("aci_metin")->as_string(), std::string("100,0000 grad"));

    // A QUESTION IS NOT AN EDIT. `ÖLÇÜ tur=acisal` draws an angular dimension and
    // costs an undo step; this one costs nothing and leaves nothing behind.
    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{0});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{0});
    }
}

TEST_CASE("PROOF: ESNET gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.stretch`. A 20 m × 10 m parcel whose RIGHT edge is
    // windowed and pulled 5 m east: the two right corners follow, the two left
    // ones stay on the tapu. The three clients differ only in the road the
    // answers take.
    const char* kSetup = "ALAN 0,0 20,0 20,10 0,10";

    Rig gui;
    {
        REQUIRE(gui.bus.execute_line(kSetup, Origin::Test).ok());
        auto started = gui.bus.begin_interactive("ESNET", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        REQUIRE(session.waiting());
        CHECK(session.supply(Value::point(core::Point2{15'000, -5'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{25'000, 15'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{5'000, 0})).ok());
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    REQUIRE(cli.bus.execute_line(kSetup, Origin::Test).ok());
    REQUIRE(cli.bus
                .execute_line("ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0",
                              Origin::CommandLine)
                .ok());

    Rig scr;
    {
        REQUIRE(scr.bus.execute_line(kSetup, Origin::Test).ok());
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Esnetme kanıtı",
            "komutlar": [ {"cmd": "core.stretch", "args": {
                "pencere": [[15000, -5000], [25000, 15000]],
                "baslangic": [0, 0], "bitis": [5000, 0] }} ]
        })");
        REQUIRE(r.ok());
    }

    // ---- the proof ----
    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{2}); ///< the ALAN, then the stretch
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // AND THE CORNERS ARE WHERE A TAPE WOULD HAVE PUT THEM, so the three cannot
    // agree on the same wrong answer.
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    const auto xs   = cli.doc.geometry().ring_xs(span.first);
    CHECK_EQ(xs[0], 0);
    CHECK_EQ(xs[1], 25'000);
    CHECK_EQ(xs[2], 25'000);
    CHECK_EQ(xs[3], 0);

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    CHECK(what_happened(gui.journal).find("core.stretch") != std::string::npos);

    // THE RESOLVED OBJECT IS IN THE JOURNAL, not the window's luck. A replay must
    // stretch what THIS run stretched, and a document replayed into may hold other
    // things under the same window (model.md P4).
    CHECK(what_happened(cli.journal).find("\"nesneler\":[1]") != std::string::npos);
}

TEST_CASE("PROOF: ESNET günlükten yeniden oynatılabilir")
{
    Rig first;
    REQUIRE(first.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(
        first.bus
            .execute_line("ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0", Origin::Test)
            .ok());

    Rig again;
    for (const auto& entry : first.journal.entries()) {
        auto r = again.bus.dispatch(Invocation{entry.command_id, entry.args, Origin::Batch});
        REQUIRE_MESSAGE(r.ok(), entry.command_id);
    }
    CHECK_EQ(again.doc.content_hash(), first.doc.content_hash());
}

TEST_CASE("ESNET iptal edilince boş geri alma deltası bırakır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    const std::uint64_t before = r.doc.content_hash();
    const std::size_t depth    = r.undo.undo_depth();

    auto started = r.bus.begin_interactive("ESNET", Origin::Gui);
    REQUIRE(started.ok());
    auto& session = *started.value();
    CHECK(session.supply(Value::point(core::Point2{15'000, -5'000})).ok());
    CHECK(session.supply(Value::point(core::Point2{25'000, 15'000})).ok());
    session.cancel(); ///< Esc after the window, before the displacement
    REQUIRE(r.bus.finish(session).ok());

    CHECK_EQ(r.doc.content_hash(), before);
    CHECK_EQ(r.undo.undo_depth(), depth);
}

TEST_CASE("PROOF: KESİŞİMNOKTA gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.intersect_point`, on the `dogru` method — the one
    // whose answer a ruler can check. AB runs east along y = 0 and CD runs north
    // along x = 40 m, so the crossing is (40, 0) exactly, with no rounding to
    // argue about.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("KESİŞİMNOKTA yontem=dogru", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        REQUIRE(session.waiting());
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{40'000, -50'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{40'000, 50'000})).ok());
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    REQUIRE(
        cli.bus
            .execute_line("KESİŞİMNOKTA yontem=dogru 0,0 100,0 40,-50 40,50", Origin::CommandLine)
            .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Kesişim kanıtı",
            "komutlar": [ {"cmd": "core.intersect_point", "args": {
                "yontem": "dogru",
                "birinci": [0, 0], "ikinci": [100000, 0],
                "ucuncu": [40000, -50000], "dorduncu": [40000, 50000] }} ]
        })");
        REQUIRE(r.ok());
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // AND THE POINT A RULER WOULD HAVE PUT THERE.
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(span.first)[0],
                           cli.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{40'000, 0}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    // THE ANSWER IS IN THE JOURNAL, not just its inputs: `kesisim` is recorded so
    // a replay lands the point this run computed rather than recomputing it.
    CHECK(what_happened(cli.journal).find("\"kesisim\"") != std::string::npos);
}

TEST_CASE("PROOF: ARANOKTA gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.point_along`. Three readings along a 100 m line, in
    // metres from the first point: 25, 40 and 60 — the road a hand takes with
    // repeated keys, a typed line with a run of numbers, and a script with an
    // array.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ARANOKTA yontem=mesafe", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        CHECK(session.supply(Value::number(25.0)).ok());
        CHECK(session.supply(Value::number(40.0)).ok());
        CHECK(session.supply(Value::number(60.0)).ok());
        REQUIRE(gui.bus.finish(session).ok()); ///< ESC / right button ends the run
    }

    Rig cli;
    REQUIRE(cli.bus
                .execute_line("ARANOKTA yontem=mesafe 0,0 100,0 deger=25 deger=40 deger=60",
                              Origin::CommandLine)
                .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Ara nokta kanıtı",
            "komutlar": [ {"cmd": "core.point_along", "args": {
                "yontem": "mesafe",
                "birinci": [0, 0], "ikinci": [100000, 0],
                "deger": [25, 40, 60] }} ]
        })");
        REQUIRE(r.ok());
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{3});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1}); ///< three points, one step
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    for (std::size_t i = 0; i < 3; ++i) {
        const core::Mm want[3]{25'000, 40'000, 60'000};
        const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[i]);
        CHECK_EQ(cli.doc.geometry().ring_xs(span.first)[0], want[i]);
        CHECK_EQ(cli.doc.geometry().ring_ys(span.first)[0], 0);
    }

    // THE SAME ARGUMENTS IN THE SAME ORDER, across the three roads, and that is
    // what the three lines above are written to give. `Args` keeps insertion
    // order and `to_json` prints it, so `yontem` named before the points and
    // named after them are two byte sequences for one invocation. What this
    // proves is that the CLIENT does not change the record; canonicalising the
    // record against declared order would change the journal bytes of every
    // stored golden fixture, so it is a decision with its own change, not a
    // side effect of this test (TODOS-CAD).
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

TEST_CASE("PROOF: açılı KILAVUZ gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for the angled half of `core.guide`. 50 grad through (10, 20)
    // — under the default semt rule that is north-east, and the stored direction
    // is the mathematical one, so the three roads have to agree on the
    // conversion as well as on the point.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("KILAVUZ yon=50g", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();

        REQUIRE(session.waiting()); ///< it asks for the point it passes through
        CHECK(session.supply(Value::point(core::Point2{10'000, 20'000})).ok());
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    REQUIRE(cli.bus.execute_line("KILAVUZ yon=50g nokta=10,20", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Açılı kılavuz kanıtı",
            "komutlar": [ {"cmd": "core.guide", "args": {
                "yon": "50g", "nokta": [10000, 20000] }} ]
        })");
        REQUIRE(r.ok());
    }

    // ---- the proof ----
    for (const Rig* rig : {&gui, &cli, &scr}) {
        REQUIRE_EQ(rig->doc.guides().size(), std::size_t{1});
        CHECK(rig->doc.guides().axis(0) == core::GuideAxis::Angled);
        CHECK_EQ(rig->doc.guides().through(0), (core::Point2{10'000, 20'000}));
        CHECK_FALSE(rig->doc.guides().ray(0));
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // AND THE DIRECTION A PROTRACTOR WOULD HAVE READ. 50 grad clockwise from
    // north is 45° east of north, which counter-clockwise from east is 45° —
    // exactly, because `sin_cos_udeg` is exact on the diagonals.
    CHECK_EQ(cli.doc.guides().angle(0), 45 * core::kUDegPerDegree);

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    // THE UNIT IS IN THE JOURNAL. A line that said `yon=50` would mean one
    // direction today and another after somebody changed `core.aci.birim`
    // (Article 1.4).
    CHECK(what_happened(cli.journal).find("50") != std::string::npos);
    CHECK(what_happened(cli.journal).find("g\"") != std::string::npos);
}

TEST_CASE("PROOF: açılı KILAVUZ günlükten yeniden oynatılabilir")
{
    Rig first;
    REQUIRE(first.bus.execute_line("KILAVUZ yon=yatay deger=5000", Origin::Test).ok());
    REQUIRE(first.bus.execute_line("KILAVUZ yon=50g nokta=10,20", Origin::Test).ok());
    REQUIRE(first.bus.execute_line("KILAVUZ yon=30d nokta=0,0 tur=isin", Origin::Test).ok());

    Rig again;
    for (const auto& e : first.journal.entries()) {
        auto r = again.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        REQUIRE_MESSAGE(r.ok(), e.command_id);
    }
    CHECK_EQ(again.doc.content_hash(), first.doc.content_hash());
    REQUIRE_EQ(again.doc.guides().size(), std::size_t{3});
    CHECK(again.doc.guides().ray(2));
}

TEST_CASE("açı birimi değişse de kaydedilmiş bir açılı kılavuz aynı yeri gösterir")
{
    // WHY THE SUFFIX IS RECORDED. The stored direction is mathematical
    // micro-degrees and the journal line carries the unit it was typed in, so a
    // replay under another `core.aci.birim` reproduces the same line. Without the
    // suffix the same `50` would be read as 50 degrees and the guide would move.
    Rig first;
    REQUIRE(first.bus.execute_line("KILAVUZ yon=50g nokta=10,20", Origin::Test).ok());
    const std::int64_t want = first.doc.guides().angle(0);

    Rig again;
    REQUIRE(again.bus.execute_line("AYAR açı_birimi derece", Origin::Test).ok());
    for (const auto& e : first.journal.entries())
        REQUIRE(again.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());

    CHECK_EQ(again.doc.guides().angle(0), want);
}

TEST_CASE("PROOF: günlük satırı BİLDİRİLEN sıraya yazılır, yazım sırasına değil")
{
    // ARTICLE 6.4's other half. `Args` keeps insertion order and `to_json` prints
    // it, so one invocation typed two ways wrote two different journal lines —
    // and 6.4 asks for a BYTE-IDENTICAL journal from the GUI, the command line
    // and a script. The three cannot be made to agree on a typing order: a tool
    // that starts `KILAVUZ yon=45g` and then asks for the point genuinely binds
    // `yon` first, while a typed line puts the positional point first.
    //
    // The declared order is the one thing every client shares, so the record is
    // written in it.
    Rig keyword_first;
    REQUIRE(keyword_first.bus
                .execute_line("ÇOKGEN yontem=ic 0,0 kenar_sayisi=6 yaricap=10", Origin::CommandLine)
                .ok());

    Rig keyword_last;
    REQUIRE(keyword_last.bus
                .execute_line("ÇOKGEN 0,0 kenar_sayisi=6 yaricap=10 yontem=ic", Origin::CommandLine)
                .ok());

    CHECK_EQ(what_happened(keyword_first.journal), what_happened(keyword_last.journal));
    CHECK_EQ(keyword_first.doc.content_hash(), keyword_last.doc.content_hash());

    // AND THE ORDER IS THE SPEC'S. Read from the registry rather than typed here,
    // so a parameter added or moved in the declaration moves the record with it
    // and this test cannot drift from what it is checking.
    const CommandSpec* spec = keyword_first.reg.resolve("ÇOKGEN");
    REQUIRE(spec != nullptr);
    REQUIRE_EQ(keyword_first.journal.entries().size(), std::size_t{1});

    std::vector<std::string> written;
    for (const auto& [name, value] : keyword_first.journal.entries().front().args.items())
        written.push_back(name);

    std::vector<std::string> expected;
    for (const Param& p : spec->params)
        for (const std::string& name : written)
            if (name == p.name) expected.push_back(name);

    CHECK_EQ(written, expected);
}

// ============================================================================
// P2 — the classical construction methods, each proven across the three clients
// ============================================================================

TEST_CASE("PROOF: DAİRE yontem=3n gui, komut satırı ve betikten aynı belgeyi bırakır")
{
    // Article 6.4 for `core.circle`'s three-point method. The three points are a
    // right triangle on the axes, so the circumcircle's centre is the midpoint of
    // the hypotenuse and every figure is exact in millimetres — the three clients
    // cannot agree on the same wrong answer.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("DAİRE yontem=3n", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{60'000, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{0, 80'000})).ok());
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    // NAMED, not positional, and the docs say so: `merkez` and `cevre` are
    // declared before `birinci`, so a bare run of points would fill THOSE — the
    // one-point-per-method ordering is what the positional slots belong to.
    REQUIRE(cli.bus
                .execute_line("DAİRE yontem=3n birinci=0,0 ikinci=60,0 ucuncu=0,80",
                              Origin::CommandLine)
                .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Üç noktalı daire kanıtı",
            "komutlar": [ {"cmd": "core.circle_draw", "args": {
                "yontem": "3n", "birinci": [0, 0], "ikinci": [60000, 0],
                "ucuncu": [0, 80000] }} ]
        })");
        REQUIRE(r.ok());
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // The circumcircle of a 60-80-100 right triangle: centre at the hypotenuse's
    // midpoint (30, 40), radius 50 — the 3-4-5 a surveyor knows by heart.
    REQUIRE(cli.doc.live_entity_count() == 1);
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(span.first)[0],
                           cli.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{30'000, 40'000}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

TEST_CASE("PROOF: YAY yontem=bby gui, komut satırı ve betikten aynı belgeyi bırakır")
{
    // Article 6.4 for `core.arc`'s start-end-radius method, on the two-semicircle
    // case: a 100 m chord with a 50 m radius is a half turn, which is the one
    // reading where both solutions are the same size and `yon` is what tells them
    // apart. The side is named, so the three clients cannot differ by luck.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("YAY yontem=bby yon=sol", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        CHECK(session.supply(Value::number(50.0)).ok());
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    REQUIRE(cli.bus
                .execute_line("YAY yontem=bby yon=sol baslangic=0,0 bitis=100,0 yaricap=50",
                              Origin::CommandLine)
                .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Başlangıç-bitiş-yarıçap kanıtı",
            "komutlar": [ {"cmd": "core.arc_draw", "args": {
                "yontem": "bby", "yon": "sol",
                "baslangic": [0, 0], "bitis": [100000, 0], "yaricap": 50 }} ]
        })");
        if (!r.ok()) FAIL_WITH("YAY bby betiği", r.error().message);
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // The centre of a half turn is the chord's own midpoint.
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(span.first)[0],
                           cli.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{50'000, 0}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

TEST_CASE("PROOF: ÇOKGEN gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // Article 6.4 for `core.polygon_regular`. A square on the axes: four sides,
    // `yontem=ic` with a 10 m radius and no turn, so the corners are the four
    // points a protractor would give and every one of them is exact.
    //
    // THE HAND POINTS, and that is the gesture this proof has to cover. A GUI
    // run with no `yaricap` in the line asks for a place instead of a number:
    // the distance from the centre is the radius and the direction is the turn,
    // both in one click. Pointing due NORTH is a turn of zero under semt, which
    // is what the other two clients get by default — so `aci=0` is written
    // into their lines to make the three do the SAME job rather than three jobs
    // that happen to look alike.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÇOKGEN yontem=ic", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::integer(4)).ok());                     // kenar_sayisi
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());      // merkez
        CHECK(session.supply(Value::point(core::Point2{0, 10'000})).ok()); // kose: 10 m, kuzey
        REQUIRE(gui.bus.finish(session).ok());
    }

    Rig cli;
    REQUIRE(cli.bus
                .execute_line("ÇOKGEN yontem=ic merkez=0,0 kenar_sayisi=4 yaricap=10 aci=0",
                              Origin::CommandLine)
                .ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Çokgen kanıtı",
            "komutlar": [ {"cmd": "core.polygon_regular", "args": {
                "yontem": "ic", "merkez": [0, 0], "kenar_sayisi": 4, "yaricap": 10,
                "aci": 0 }} ]
        })");
        if (!r.ok()) FAIL_WITH("ÇOKGEN betiği", r.error().message);
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{1});
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // FOUR CORNERS, and the first one where the convention puts it. Under the
    // default semt rule an angle of zero is NORTH, so the first corner is 10 m
    // due north and `sin_cos_udeg` is exact there.
    const auto span = cli.doc.geometry().rings_of(cli.doc.entities().slot[0]);
    REQUIRE_EQ(cli.doc.geometry().ring_xs(span.first).size(), std::size_t{4});
    CHECK_EQ((core::Point2{cli.doc.geometry().ring_xs(span.first)[0],
                           cli.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{0, 10'000}));

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

// ============================================================================
// P3 — the seven editing verbs, each proven across the three clients
// ============================================================================

namespace {

/// Runs `setup` on a rig, then the verb three ways, and proves the three agree.
///
/// THE SEVEN VERBS DIFFER ONLY IN WHAT THEY ASK FOR, so the proof they each need
/// is one shape with the answers substituted. Written once here rather than seven
/// times below: a proof copied seven times is a proof that drifts in six of them.
///
/// `objects` is the selection the verb works on, `answers` what it asks for after
/// that in order, `typed` the command line that says the same thing and `scripted`
/// the JSON that does.
struct VerbProof
{
    const char* name;                   ///< the verb, as a user types it
    const char* id;                     ///< its command id, for the script road
    std::vector<const char*> setup;     ///< the drawing the verb is run against
    std::vector<std::int64_t> objects;  ///< what it works on
    std::vector<Value> answers;         ///< every answer after the objects, in order
    const char* typed;                  ///< the whole command line
    const char* scripted;               ///< the whole script
    std::size_t undo_steps_expected{1}; ///< on top of the setup's own
};

void prove_verb(const VerbProof& v)
{
    const auto lay_out = [&v](Rig& rig) {
        for (const char* line : v.setup)
            REQUIRE_MESSAGE(rig.bus.execute_line(line, Origin::Test).ok(), line);
    };

    Rig gui;
    lay_out(gui);
    const std::size_t depth = gui.undo.undo_depth();
    {
        auto started = gui.bus.begin_interactive(v.name, Origin::Gui);
        REQUIRE_MESSAGE(started.ok(), v.name);
        auto& session = *started.value();
        if (!v.objects.empty()) CHECK(session.supply(Value::ids(v.objects)).ok());
        for (const Value& answer : v.answers)
            CHECK_MESSAGE(session.supply(answer).ok(), v.name);
        auto done = gui.bus.finish(session);
        if (!done.ok()) FAIL_WITH(v.name, done.error().message);
    }

    Rig cli;
    lay_out(cli);
    {
        auto r = cli.bus.execute_line(v.typed, Origin::CommandLine);
        if (!r.ok()) FAIL_WITH(v.typed, r.error().message);
    }

    Rig scr;
    lay_out(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(v.scripted);
        if (!r.ok()) FAIL_WITH(v.name, r.error().message);
    }

    // ---- the proof ----
    CHECK_MESSAGE(gui.doc.content_hash() == cli.doc.content_hash(), v.name);
    CHECK_MESSAGE(cli.doc.content_hash() == scr.doc.content_hash(), v.name);
    if (what_happened(gui.journal) != what_happened(cli.journal))
        FAIL_WITH("gui ile komut satırı günlüğü farklı",
                  what_happened(gui.journal) + " ---VS--- " + what_happened(cli.journal));
    if (what_happened(cli.journal) != what_happened(scr.journal))
        FAIL_WITH("komut satırı ile betik günlüğü farklı",
                  what_happened(cli.journal) + " ---VS--- " + what_happened(scr.journal));

    // ONE COMMAND, ONE UNDO STEP — on every road (Article 1.5).
    for (const Rig* rig : {&gui, &cli, &scr})
        CHECK_MESSAGE(rig->undo.undo_depth() == depth + v.undo_steps_expected, v.name);

    // AND THE EDIT ACTUALLY HAPPENED. Three clients agreeing that nothing
    // occurred would satisfy every check above — so the verb's own journal line
    // has to be there, on top of the setup's.
    const bool ran = gui.journal.entries().size() > v.setup.size();
    CHECK_MESSAGE(ran, v.name);
    if (ran) CHECK_MESSAGE(gui.journal.entries().back().command_id == std::string(v.id), v.name);
}

} // namespace

TEST_CASE("PROOF: KIR gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb(
        {.name     = "KIR",
         .id       = "core.break",
         .setup    = {"ÇOKLUÇİZGİ 0,0 100,0"},
         .objects  = {1},
         .answers  = {Value::point(core::Point2{30'000, 0}), Value::point(core::Point2{70'000, 0})},
         .typed    = "KIR nesne=1 birinci=30,0 ikinci=70,0",
         .scripted = R"({"ad":"KIR","komutlar":[{"cmd":"core.break","args":{
                    "nesne":[1],"birinci":[30000,0],"ikinci":[70000,0]}}]})"});
}

TEST_CASE("PROOF: UZUNLUK gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb({.name     = "UZUNLUK",
                .id       = "core.lengthen",
                .setup    = {"ÇOKLUÇİZGİ 0,0 100,0"},
                .objects  = {1},
                .answers  = {Value::number(25.0)}, ///< it ASKS for delta now
                .typed    = "UZUNLUK nesne=1 delta=25",
                .scripted = R"({"ad":"UZUNLUK","komutlar":[{"cmd":"core.lengthen","args":{
                    "nesne":[1],"delta":25}}]})"});
}

TEST_CASE("PROOF: PATLAT gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb({.name     = "PATLAT",
                .id       = "core.explode",
                .setup    = {"ALAN 0,0 20,0 20,10 0,10"},
                .objects  = {1},
                .answers  = {},
                .typed    = "PATLAT nesne=1",
                .scripted = R"({"ad":"PATLAT","komutlar":[{"cmd":"core.explode","args":{
                    "nesne":[1]}}]})"});
}

TEST_CASE("PROOF: HİZALA gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // One pair: the move. The GUI is asked for a second source and answers it
    // with Enter — an empty answer — which is "move and nothing else".
    prove_verb({.name     = "HİZALA",
                .id       = "core.align",
                .setup    = {"ALAN 0,0 20,0 20,10 0,10"},
                .objects  = {1},
                .answers  = {Value::point(core::Point2{0, 0}),
                             Value::point(core::Point2{50'000, 50'000}), Value{}},
                .typed    = "HİZALA nesne=1 kaynak=0,0 hedef=50,50",
                .scripted = R"({"ad":"HİZALA","komutlar":[{"cmd":"core.align","args":{
                    "nesne":[1],"kaynak":[0,0],"hedef":[50000,50000]}}]})"});

    // Two pairs: the move and the turn, now asked of the GUI too.
    prove_verb(
        {.name     = "HİZALA",
         .id       = "core.align",
         .setup    = {"ALAN 0,0 20,0 20,10 0,10"},
         .objects  = {1},
         .answers  = {Value::point(core::Point2{0, 0}), Value::point(core::Point2{50'000, 50'000}),
                      Value::point(core::Point2{20'000, 0}),
                      Value::point(core::Point2{50'000, 70'000})},
         .typed    = "HİZALA nesne=1 kaynak=0,0 hedef=50,50 kaynak2=20,0 hedef2=50,70",
         .scripted = R"({"ad":"HİZALA","komutlar":[{"cmd":"core.align","args":{
                    "nesne":[1],"kaynak":[0,0],"hedef":[50000,50000],
                    "kaynak2":[[20000,0]],"hedef2":[[50000,70000]]}}]})"});
}

TEST_CASE("PROOF: BÖLÜMLE gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb({.name     = "BÖLÜMLE",
                .id       = "core.divide",
                .setup    = {"ÇOKLUÇİZGİ 0,0 100,0"},
                .objects  = {1},
                .answers  = {Value::integer(4)}, ///< it ASKS for sayi now
                .typed    = "BÖLÜMLE nesne=1 sayi=4",
                .scripted = R"({"ad":"BÖLÜMLE","komutlar":[{"cmd":"core.divide","args":{
                    "nesne":[1],"sayi":4}}]})"});
}

TEST_CASE("PROOF: ÇİZGİDÜZENLE gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb({.name     = "ÇİZGİDÜZENLE",
                .id       = "core.pedit",
                .setup    = {"ÇOKLUÇİZGİ 0,0 20,0 20,10"},
                .objects  = {1},
                .answers  = {Value::text("kapat")}, ///< it asks which operation
                .typed    = "ÇİZGİDÜZENLE nesne=1 islem=kapat",
                .scripted = R"({"ad":"ÇİZGİDÜZENLE","komutlar":[{"cmd":"core.pedit","args":{
                    "nesne":[1],"islem":"kapat"}}]})"});
}

TEST_CASE("PROOF: UÇUCA gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    prove_verb({.name     = "UÇUCA",
                .id       = "core.join",
                .setup    = {"ÇOKLUÇİZGİ 0,0 20,0", "ÇOKLUÇİZGİ 20,0 40,0"},
                .objects  = {1, 2},
                .answers  = {},
                .typed    = "UÇUCA nesne=1 nesne=2",
                .scripted = R"({"ad":"UÇUCA","komutlar":[{"cmd":"core.join","args":{
                    "nesne":[1,2]}}]})"});
}

TEST_CASE("PROOF: RENK gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır")
{
    // The colour chip's road: nothing selected, the objects asked for, then the
    // colour. The GUI answers `#c0392b` in lower case on purpose — the journal
    // holds the canonical spelling, so the three lines still agree.
    prove_verb({.name     = "RENK",
                .id       = "core.colour",
                .setup    = {"ALAN 0,0 10,0 10,10 0,10", "ÇİZGİ 0,20 10,20"},
                .objects  = {1, 2},
                .answers  = {Value::text("#c0392b")},
                .typed    = "RENK nesneler=1 nesneler=2 renk=#C0392B",
                .scripted = R"({"ad":"RENK","komutlar":[{"cmd":"core.colour","args":{
                    "nesneler":[1,2],"renk":"#C0392B"}}]})"});
}

// ============================================================================
// P4, P5, P6 and the tracking marks — the changed commands the plan also names
// ============================================================================

TEST_CASE("PROOF: SEÇ ÇİT gui, komut satırı ve betikten aynı seçimi verir")
{
    // Article 6.4 for `core.select`'s new modes. A SELECTION is not document
    // state (model.md R43), so the thing that has to agree is the SELECTION — and
    // it is what the next command would delete, so a client that selected
    // something else would delete something else.
    const char* kSetup[] = {"ÇOKLUÇİZGİ 0,0 10,0", "ÇOKLUÇİZGİ 0,20 10,20",
                            "ÇOKLUÇİZGİ 0,40 10,40"};

    const auto keys_of = [](const Rig& rig) {
        std::vector<std::uint64_t> out;
        for (const core::EntityKey k : rig.bus.selection().keys())
            out.push_back(core::raw(k));
        std::sort(out.begin(), out.end());
        return out;
    };

    Rig gui;
    Rig cli;
    Rig scr;
    for (Rig* rig : {&gui, &cli, &scr})
        for (const char* line : kSetup)
            REQUIRE(rig->bus.execute_line(line, Origin::Test).ok());

    // A fence straight up the middle crosses the first two runs and misses the
    // third, which is what a fence is FOR: picking a row of things a window would
    // have to be drawn around.
    {
        auto started = gui.bus.begin_interactive("SEÇ ÇİT", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        // ONE POINT PER CLICK, then the right button. A fence takes an unbounded
        // run, so the interactive road asks for them one at a time — a whole list
        // supplied at once is the SCRIPT's road and the two must not be confused
        // (the same distinction `want_objects` keeps).
        CHECK(session.supply(Value::point(core::Point2{5'000, -5'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{5'000, 25'000})).ok());
        session.cancel(); ///< right button: that is the fence, go
        REQUIRE(gui.bus.finish(session).ok());
    }
    REQUIRE(cli.bus.execute_line("SEÇ ÇİT 5,-5 5,25", Origin::CommandLine).ok());
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Çit kanıtı",
            "komutlar": [ {"cmd": "core.select", "args": {
                "mod": "ÇİT", "noktalar": [[5000, -5000], [5000, 25000]] }} ]
        })");
        if (!r.ok()) FAIL_WITH("ÇİT betiği", r.error().message);
    }

    CHECK_EQ(keys_of(gui), keys_of(cli));
    CHECK_EQ(keys_of(cli), keys_of(scr));
    CHECK_EQ(keys_of(cli).size(), std::size_t{2}); ///< the third run is not crossed

    // AND NOTHING WAS DRAWN. A selection is not a mutation.
    for (const Rig* rig : {&gui, &cli, &scr})
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{3}); ///< the three runs only
}

TEST_CASE("PROOF: ÖLÇÜ tur=koordinat gui, komut satırı ve betikten aynı belgeyi bırakır")
{
    // Article 6.4 for `core.dimension`'s ordinate type — P5's own addition. The
    // golden fixture pins the TEXT; this pins that the three clients produce it.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÖLÇÜ tur=koordinat", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        // THREE POINTS, as every dimension type takes: the two being measured and
        // where the caption goes.
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{30'000, 20'000})).ok());
        CHECK(session.supply(Value::point(core::Point2{45'000, 20'000})).ok());
        auto done = gui.bus.finish(session);
        if (!done.ok()) FAIL_WITH("ÖLÇÜ koordinat", done.error().message);
    }

    Rig cli;
    {
        auto r = cli.bus.execute_line("ÖLÇÜ tur=koordinat birinci=0,0 ikinci=30,20 konum=45,20",
                                      Origin::CommandLine);
        if (!r.ok()) FAIL_WITH("ÖLÇÜ koordinat komut satırı", r.error().message);
    }

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Ordinat ölçüsü kanıtı",
            "komutlar": [ {"cmd": "core.dimension", "args": {
                "tur": "koordinat", "birinci": [0, 0], "ikinci": [30000, 20000],
                "konum": [45000, 20000] }} ]
        })");
        if (!r.ok()) FAIL_WITH("ÖLÇÜ koordinat betiği", r.error().message);
    }

    for (const Rig* rig : {&gui, &cli, &scr})
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

TEST_CASE("PROOF: PANOYAKOPYALA ve YAPIŞTIR gui, komut satırı ve betikten aynı belgeyi bırakır")
{
    // Article 6.4 for P6's three verbs. The payload is a file, so the three
    // clients are proven on ONE clipboard in turn: each copies, each pastes into
    // a fresh drawing, and the three pasted drawings have to be one drawing.
    const auto copy_and_paste = [](FileRig& rig, const std::function<void(FileRig&)>& copy) {
        REQUIRE(rig.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
        REQUIRE(rig.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
        REQUIRE(rig.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());
        copy(rig);
        REQUIRE(rig.bus.execute_line("YENİ", Origin::Test).ok());
        REQUIRE(rig.bus.execute_line("YAPIŞTIR yerinde=evet", Origin::Test).ok());
    };

    FileRig gui;
    copy_and_paste(gui, [](FileRig& rig) {
        auto started = rig.bus.begin_interactive("PANOYAKOPYALA", Origin::Gui);
        REQUIRE(started.ok());
        auto done = rig.bus.finish(*started.value());
        if (!done.ok()) FAIL_WITH("PANOYAKOPYALA arayüzden", done.error().message);
    });

    FileRig cli;
    copy_and_paste(cli, [](FileRig& rig) {
        REQUIRE(rig.bus.execute_line("PANOYAKOPYALA", Origin::CommandLine).ok());
    });

    FileRig scr;
    copy_and_paste(scr, [](FileRig& rig) {
        script::JsonRunner runner(rig.bus, script::Sandbox::Project);
        auto r =
            runner.run_text(R"({"ad":"Pano","komutlar":[{"cmd":"core.copy_clip","args":{}}]})");
        if (!r.ok()) FAIL_WITH("PANOYAKOPYALA betikten", r.error().message);
    });

    for (const FileRig* rig : {&gui, &cli, &scr})
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
}

TEST_CASE("PROOF: İZ gui, komut satırı ve betikten aynı işaretleri bırakır")
{
    // Article 6.4 for `core.tracking`. A mark is SESSION state (model.md R43), so
    // what has to agree is the marks — and they are what the next point snaps to,
    // so a client that marked something else would draw somewhere else.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("İZ 12,8", Origin::Gui);
        REQUIRE(started.ok());
        auto done = gui.bus.finish(*started.value());
        if (!done.ok()) FAIL_WITH("İZ arayüzden", done.error().message);
    }
    REQUIRE(gui.bus.execute_line("İZ 26,18", Origin::Gui).ok());

    Rig cli;
    REQUIRE(cli.bus.execute_line("İZ 12,8", Origin::CommandLine).ok());
    REQUIRE(cli.bus.execute_line("İZ 26,18", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "İz kanıtı",
            "komutlar": [ {"cmd": "core.tracking", "args": {"nokta": [12000, 8000]}},
                          {"cmd": "core.tracking", "args": {"nokta": [26000, 18000]}} ]
        })");
        if (!r.ok()) FAIL_WITH("İZ betikten", r.error().message);
    }

    for (const Rig* rig : {&gui, &cli, &scr}) {
        REQUIRE_EQ(rig->bus.tracking_marks().size(), std::size_t{2});
        CHECK_EQ(rig->bus.tracking_marks()[0], (core::Point2{12'000, 8'000}));
        CHECK_EQ(rig->bus.tracking_marks()[1], (core::Point2{26'000, 18'000}));
        // A MARK IS NOT DOCUMENT STATE: no entity, no undo step, no journal line.
        CHECK_EQ(rig->doc.live_entity_count(), std::size_t{0});
        CHECK_EQ(rig->undo.undo_depth(), std::size_t{0});
        for (const auto& e : rig->journal.entries())
            CHECK(e.command_id != "core.tracking");
    }
}

namespace {

/// A READING PROVEN ACROSS THE THREE CLIENTS. A measurement is read-only and so
/// is not journalled (a Ctrl+Z must not undo a question): the proof it owes is
/// that the three roads SAY the same thing and LEAVE the same mark, and that the
/// drawing is untouched on every one of them.
struct ReadingProof
{
    const char* name;           ///< the command line that starts it at the canvas
    std::vector<Value> answers; ///< what the canvas supplies, in order
    const char* typed;          ///< the whole command line
    const char* scripted;       ///< the whole script
};

void prove_reading(const ReadingProof& v)
{
    struct Seen
    {
        std::string said;
        std::vector<MeasureMark> marks;
    };

    const auto watch = [](Rig& rig, Seen& seen) {
        rig.bus.on_echo         = [&seen](std::string_view t) { seen.said.append(t).append("\n"); };
        rig.bus.on_measure_mark = [&seen](const MeasureMark& m) { seen.marks.push_back(m); };
    };

    Rig gui;
    Seen from_gui;
    watch(gui, from_gui);
    {
        auto started = gui.bus.begin_interactive(v.name, Origin::Gui);
        REQUIRE_MESSAGE(started.ok(), v.name);
        auto& session = *started.value();
        for (const Value& answer : v.answers)
            CHECK_MESSAGE(session.supply(answer).ok(), v.name);
        auto done = gui.bus.finish(session);
        if (!done.ok()) FAIL_WITH(v.name, done.error().message);
    }

    Rig cli;
    Seen from_cli;
    watch(cli, from_cli);
    if (auto r = cli.bus.execute_line(v.typed, Origin::CommandLine); !r.ok())
        FAIL_WITH(v.typed, r.error().message);

    Rig scr;
    Seen from_scr;
    watch(scr, from_scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        if (auto r = runner.run_text(v.scripted); !r.ok()) FAIL_WITH(v.name, r.error().message);
    }

    // THE SAME WORDS AND THE SAME MARK. The script runner adds its own summary
    // line after the command's, so only the command's words are compared.
    const auto words = [](const std::string& all) { return all.substr(0, all.rfind("Betik")); };
    CHECK_MESSAGE(!from_gui.said.empty(), v.name);
    CHECK_EQ(from_gui.said, from_cli.said);
    CHECK(words(from_scr.said).find(from_cli.said) != std::string::npos);
    REQUIRE_EQ(from_gui.marks.size(), from_cli.marks.size());
    REQUIRE_EQ(from_cli.marks.size(), from_scr.marks.size());
    for (std::size_t i = 0; i < from_gui.marks.size(); ++i) {
        CHECK(from_gui.marks[i].points == from_cli.marks[i].points);
        CHECK(from_gui.marks[i].labels == from_cli.marks[i].labels);
        CHECK(from_cli.marks[i].points == from_scr.marks[i].points);
        CHECK(from_cli.marks[i].labels == from_scr.marks[i].labels);
    }
    // A QUESTION CHANGES NOTHING, on any road.
    for (const Rig* rig : {&gui, &cli, &scr}) {
        CHECK(rig->journal.entries().empty());
        CHECK(rig->undo.undo_depth() == 0);
    }
}

} // namespace

TEST_CASE("PROOF: ÖLÇ noktadan noktaya gui, komut satırı ve betikten aynı cevabı ve işareti verir")
{
    prove_reading(
        {.name     = "ÖLÇ",
         .answers  = {Value::point(core::Point2{0, 0}), Value::point(core::Point2{3'000, 4'000}),
                      Value::point(core::Point2{3'000, 10'000}), Value{}},
         .typed    = "ÖLÇ 0,0 3,4 devam=3,10",
         .scripted = R"({"ad":"ÖLÇ","komutlar":[{"cmd":"core.measure","args":{
                    "baslangic":[0,0],"bitis":[3000,4000],"devam":[[3000,10000]]}}]})"});
}

TEST_CASE("PROOF: ALANÖLÇ köşelerden gui, komut satırı ve betikten aynı cevabı ve işareti verir")
{
    prove_reading(
        {.name     = "ALANÖLÇ yontem=nokta",
         .answers  = {Value::point(core::Point2{0, 0}), Value::point(core::Point2{20'000, 0}),
                      Value::point(core::Point2{20'000, 10'000}),
                      Value::point(core::Point2{0, 10'000}), Value{}},
         .typed    = "ALANÖLÇ yontem=nokta noktalar=0,0 20,0 20,10 0,10",
         .scripted = R"({"ad":"ALANÖLÇ","komutlar":[{"cmd":"core.measure_area","args":{
                    "yontem":"nokta","noktalar":[[0,0],[20000,0],[20000,10000],[0,10000]]}}]})"});
}
