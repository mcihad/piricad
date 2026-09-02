// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Lua host (`.claude/script.md`). Compiled only when `KENTOS_WITH_LUA=ON`;
// R6 requires the whole suite to build and pass with the option off, so this file
// must be empty in that build rather than skipped at runtime.
//
// WHAT IS PROVED HERE, rule by rule:
//   R1/R4  a Lua mutation is a command line through the bus and the one parser
//   R11    every run writes its sandbox level as a `{kind:"meta"}` journal line
//   R12    `tam` runs only after consent given for THAT script's text
//   R13    a stop token stops a running chunk
//   R14    one script is one undo step, and a failure rolls the whole block back
//   R9/P4  a read binding returns a value, and there is no write path but h.komut
//   P8     `güvenli` has no filesystem; `proje` cannot escape the project directory
#include "kentos_test.hpp"

#if KENTOS_HAVE_LUA

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/script/lua_runner.hpp"

#include <filesystem>
#include <fstream>
#include <thread>

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

    Rig() { register_builtin_commands(reg); }
};

/// The sandbox level a run recorded, read back out of the journal's meta lines.
std::string recorded_level(const Journal& j)
{
    if (j.meta_records().empty()) return {};
    const core::Json& last = j.meta_records().back();
    const core::Json* v    = last.find("kum_havuzu");
    return v != nullptr && v->is_string() ? v->as_string() : std::string{};
}

} // namespace

TEST_CASE("LUA: a mutation goes through the bus and lands in the document")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
        h.komut("ÇİZGİ 485320150,4310220400 485370150,4310250400")
    )",
                                  "test");

    REQUIRE(report.ok());
    CHECK(report.value().commands == 1);
    CHECK(rig.doc.live_entity_count() == 1);

    // R1: it is in the JOURNAL, which is the whole point — a script gets undo,
    // validation and the audit record whether it asked for them or not.
    REQUIRE(rig.journal.entries().size() == 1);
    CHECK(rig.journal.entries().front().command_id == "core.line");
}

TEST_CASE("LUA: one script is one undo step")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
        for i = 0, 4 do
            local y = 4310220400 + i * 1000
            h.komut("ÇİZGİ 485320150," .. y .. " 485370150," .. y)
        end
    )",
                                  "beş çizgi");

    REQUIRE(report.ok());
    CHECK(report.value().commands == 5);
    CHECK(rig.doc.live_entity_count() == 5);

    // §2.5: FIVE commands, ONE undo entry. A script that produced five would make
    // Ctrl+Z mean something different after a script than after a mouse.
    CHECK(rig.undo.undo_depth() == 1);

    std::string label;
    CHECK(rig.undo.undo(rig.doc, &label).ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

TEST_CASE("LUA: a failing script leaves nothing behind")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    // Two good commands, then one that cannot validate. CLAUDE.md 1.6: the whole
    // block rolls back. A half-applied ifraz is a build-blocking defect, and a
    // script is exactly where one would come from.
    auto report = runner.run_text(R"(
        h.komut("ÇİZGİ 485320150,4310220400 485370150,4310250400")
        h.komut("ÇİZGİ 485370150,4310250400 485440861,4310321111")
        h.komut("BÖYLEBİRKOMUTYOK 1 2 3")
    )",
                                  "yarım kalan");

    CHECK_FALSE(report.ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

TEST_CASE("LUA: the run records its sandbox level in the journal")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    // R11: written BEFORE the body, and written even when the body does nothing.
    REQUIRE(runner.run_text("", "boş").ok());
    CHECK(recorded_level(rig.journal) == "güvenli");

    // The meta line is not a command and must never replay as one (command.md
    // R20), so it stays out of the byte-identity rendering entirely.
    CHECK(rig.journal.canonical().empty());
}

TEST_CASE("LUA: tam needs consent for this exact script")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Full);

    const std::string body = R"(h.komut("ÇİZGİ 0,0 1,1"))";

    // P7: nothing auto-grants `tam`. Without a matching grant the script does not
    // run at all — not one command of it.
    auto refused = runner.run_text(body, "onaysız");
    CHECK_FALSE(refused.ok());
    CHECK(rig.doc.live_entity_count() == 0);

    // R12: consent is given to the TEXT. Consent for a different body does not
    // carry over, which is what makes an edited script ask again.
    runner.grant_full(script::script_identity("bambaşka bir betik"));
    CHECK_FALSE(runner.run_text(body, "yanlış onay").ok());

    runner.grant_full(script::script_identity(body));
    CHECK(runner.run_text(body, "onaylı").ok());
}

TEST_CASE("LUA: güvenli has no filesystem and proje cannot leave the project")
{
    Rig rig;

    // P8: at `güvenli` the file libraries are never opened, so `io` is not a thing
    // that was taken away — it was never there.
    {
        script::LuaRunner safe(rig.bus, script::Sandbox::Safe);
        CHECK(safe.run_text("return io.open('/etc/passwd')", "io").ok() == false);
        CHECK(safe.run_text("h.dosya_oku('/etc/passwd')", "oku").ok() == false);
        CHECK(safe.run_text("return require('os')", "require").ok() == false);
    }

    // P8: at `proje` the jail is resolved, not string-matched. `..` out of the
    // project directory is a path inside it as text and outside it in fact.
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "kentoscad-lua-test";
        std::filesystem::create_directories(root);
        {
            std::ofstream(root / "içeride.txt") << "merhaba";
        }

        script::LuaRunner project(rig.bus, script::Sandbox::Project);
        project.set_project_root(root.string());

        CHECK(project
                  .run_text("return h.dosya_oku('" + (root / "içeride.txt").string() + "')",
                            "içeride")
                  .ok());
        CHECK_FALSE(project
                        .run_text("return h.dosya_oku('" +
                                      (root / ".." / ".." / "etc" / "passwd").string() + "')",
                                  "dışarıda")
                        .ok());

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
}

TEST_CASE("LUA: a read binding returns a value")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    // R9: values only. What comes back is a Lua number and a Lua table, never a
    // handle a later command could invalidate.
    auto report = runner.run_text(R"(
        local before = h.nesne_sayisi()
        h.komut("ÇİZGİ 485320150,4310220400 485370150,4310250400")
        if h.nesne_sayisi() ~= before + 1 then error("sayım yanlış") end
        if type(h.katmanlar()) ~= "table" then error("katmanlar tablo değil") end
        if type(h.krs()) ~= "string" then error("krs metin değil") end
    )",
                                  "okuma");

    CHECK(report.ok());
}

TEST_CASE("LUA: BETİK picks the host from the file extension")
{
    Rig rig;
    script::JsonRunner json(rig.bus, script::Sandbox::Project);
    script::LuaRunner lua(rig.bus, script::Sandbox::Project);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "kentoscad-host-dispatch";
    std::filesystem::create_directories(root);
    lua.set_project_root(root.string());

    // The same drawing, written twice in two languages. Both reach the bus; which
    // host ran is the file's business, not the user's.
    {
        std::ofstream(root / "a.lua")
            << R"(h.komut("ÇİZGİ 485320150,4310220400 485370150,4310250400"))";
        std::ofstream(root / "b.json")
            << R"([{"cmd":"core.line","args":{"noktalar":[[485320150,4310220400],)"
               R"([485370150,4310250400]]}}])";
    }

    script::install(rig.bus, json, lua);
    REQUIRE(rig.bus.on_run_script);

    CHECK(rig.bus.on_run_script((root / "a.lua").string()).ok());
    CHECK(rig.doc.live_entity_count() == 1);

    CHECK(rig.bus.on_run_script((root / "b.json").string()).ok());
    CHECK(rig.doc.live_entity_count() == 2);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("LUA: a stop token stops a running chunk")
{
    Rig rig;
    script::LuaRunner runner(rig.bus, script::Sandbox::Safe);

    // R13/P11: cancellation is not advisory. The stop is already requested when
    // the chunk starts, so the first hook fires and the loop never completes.
    std::stop_source source;
    source.request_stop();

    auto report = runner.run_text("while true do end", "sonsuz", source.get_token());
    CHECK_FALSE(report.ok());
}

#endif // KENTOS_HAVE_LUA
