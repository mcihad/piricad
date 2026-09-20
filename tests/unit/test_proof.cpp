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
