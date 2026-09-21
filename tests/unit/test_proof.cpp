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
