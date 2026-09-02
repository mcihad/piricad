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
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <filesystem>

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
