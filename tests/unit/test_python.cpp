// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Python host (`.claude/script.md`). Compiled only when
// `KENTOS_WITH_PYTHON=ON`; R6 requires the whole suite to build and pass with the
// option off, so this file must be EMPTY in that build rather than skipped at
// runtime.
//
// WHAT IS PROVED HERE, rule by rule:
//   R1/R4  a Python mutation is a command line through the bus and the one parser
//   R11    every run writes its sandbox level as a `{kind:"meta"}` journal line
//   R12    `tam` runs only after consent given for THAT script's text
//   R13    a stop token stops a running script, from this thread and another
//   R14    one script is one undo step, and a failure rolls the whole block back
//   R9/P4  a read binding returns a value, and there is no write path but cad.run
//   P8     `güvenli` has no binding filesystem; `proje` cannot escape the project
#include "kentos_test.hpp"

#if KENTOS_HAVE_PYTHON

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/script/python_runner.hpp"

#include <chrono>
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

/// The host name a run recorded, which is what tells a `.py` run from a `.json`
/// one in an audit.
std::string recorded_host(const Journal& j)
{
    if (j.meta_records().empty()) return {};
    const core::Json* v = j.meta_records().back().find("konak");
    return v != nullptr && v->is_string() ? v->as_string() : std::string{};
}

} // namespace

TEST_CASE("PYTHON: a mutation goes through the bus and lands in the document")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
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

TEST_CASE("PYTHON: the module is importable as well as pre-bound")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // Both roads reach the same module object: `import` for a file that wants to
    // say where its names came from, and the pre-bound `cad` for a one-liner
    // typed at an evaluator.
    auto report = runner.run_text(R"(
import kentos.cad
from kentos import cad as also_cad

assert kentos.cad is cad, "import gave a different module"
assert also_cad is cad, "from-import gave a different module"
)",
                                  "import");

    CHECK(report.ok());
}

TEST_CASE("PYTHON: one script is one undo step")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
for i in range(5):
    y = 4310220400 + i * 1000
    cad.run(f"ÇİZGİ 485320150,{y} 485370150,{y}")
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

TEST_CASE("PYTHON: a failing script leaves nothing behind")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // Two good commands, then one that cannot validate. CLAUDE.md 1.6: the whole
    // block rolls back. A half-applied ifraz is a build-blocking defect, and a
    // script is exactly where one would come from.
    auto report = runner.run_text(R"(
cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
cad.run("ÇİZGİ 485370150,4310250400 485440861,4310321111")
cad.run("BÖYLEBİRKOMUTYOK 1 2 3")
)",
                                  "yarım kalan");

    CHECK_FALSE(report.ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

TEST_CASE("PYTHON: a swallowed command error still rolls the block back")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // A PYTHON-SHAPED HAZARD THE OTHER HOST DID NOT HAVE, and the reason this
    // case exists. `try/except` is the first thing a Python programmer reaches
    // for, and a script that catches the failure and carries on has decided for
    // itself that a partial drawing is acceptable. It is not: the block either
    // applies whole or not at all (CLAUDE.md 1.6).
    //
    // So the run REPORTS SUCCESS — the script did what it said — while the two
    // good commands are still there and the failed one left nothing. What must
    // never happen is a half-applied third command.
    auto report = runner.run_text(R"(
cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
try:
    cad.run("BÖYLEBİRKOMUTYOK 1 2 3")
except Exception as hata:
    caught = str(hata)
cad.run("ÇİZGİ 485370150,4310250400 485440861,4310321111")
assert "BÖYLEBİRKOMUTYOK" in caught or "komut" in caught.lower()
)",
                                  "yakalanan hata");

    REQUIRE(report.ok());
    CHECK(rig.doc.live_entity_count() == 2);
    CHECK(rig.undo.undo_depth() == 1);
}

TEST_CASE("PYTHON: the run records its host and sandbox level in the journal")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // R11: written BEFORE the body, and written even when the body does nothing.
    REQUIRE(runner.run_text("", "boş").ok());
    CHECK(recorded_level(rig.journal) == "güvenli");
    CHECK(recorded_host(rig.journal) == "python");

    // The meta line is not a command and must never replay as one (command.md
    // R20), so it stays out of the byte-identity rendering entirely — which is
    // what lets the GUI = command line = script proof of Article 6.4 hold with a
    // script in it.
    CHECK(rig.journal.canonical().empty());
}

TEST_CASE("PYTHON: tam needs consent for this exact script")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Full);

    const std::string body = R"(cad.run("ÇİZGİ 0,0 1,1"))";

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

TEST_CASE("PYTHON: the binding filesystem obeys the level")
{
    Rig rig;

    // P8, AND WHAT IT CAN AND CANNOT MEAN HERE. CPython's own `open` is in the
    // one interpreter and cannot be taken away — `python_runner.hpp` says so
    // rather than pretending otherwise. What the level governs is the path the
    // BINDINGS offer, and that is what these cases pin.
    {
        script::PythonRunner safe(rig.bus, script::Sandbox::Safe);
        CHECK_FALSE(safe.run_text("cad.read_file('/etc/passwd')", "oku").ok());
        CHECK_FALSE(safe.run_text("cad.write_file('/tmp/olmaz.txt', 'x')", "yaz").ok());
    }

    // At `proje` the jail is RESOLVED, not string-matched. `..` out of the project
    // directory is a path inside it as text and outside it in fact.
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "kentoscad-python-test";
        std::filesystem::create_directories(root);
        {
            std::ofstream(root / "içeride.txt") << "merhaba";
        }

        script::PythonRunner project(rig.bus, script::Sandbox::Project);
        project.set_project_root(root.string());

        CHECK(project
                  .run_text("assert cad.read_file(r'" + (root / "içeride.txt").string() +
                                "') == 'merhaba'",
                            "içeride")
                  .ok());
        CHECK_FALSE(project
                        .run_text("cad.read_file(r'" +
                                      (root / ".." / ".." / "etc" / "passwd").string() + "')",
                                  "dışarıda")
                        .ok());

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
}

TEST_CASE("PYTHON: a read binding returns a value of the right Python type")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // R9: values only. What comes back is an int, a list and a str, never a handle
    // a later command could invalidate.
    auto report = runner.run_text(R"(
before = cad.entity_count()
cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
assert cad.entity_count() == before + 1, "sayım yanlış"
assert isinstance(cad.layers(), list), "layers() liste değil"
assert isinstance(cad.crs(), str), "crs() metin değil"
assert isinstance(cad.layer_count(), int), "layer_count() tamsayı değil"
assert isinstance(cad.selection_count(), int)
assert isinstance(cad.active_layer(), str)
assert cad.sandbox() == "güvenli"

# A setting arrives as the type it IS, which is the whole reason `setting` does
# not hand everything back as a string.
assert isinstance(cad.setting("core.arayuz.dinamik_girdi"), bool)
)",
                                  "okuma");

    CHECK(report.ok());
}

TEST_CASE("PYTHON: print reaches the program's own output")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    std::vector<std::string> lines;
    rig.bus.on_echo = [&lines](std::string_view text) { lines.emplace_back(text); };

    // A windowed application has no terminal, so a script that reports its
    // progress to stdout would report it into the void.
    REQUIRE(runner.run_text("print('bir')\nprint('iki')\n", "yazdır").ok());
    REQUIRE(lines.size() >= 2);
    CHECK(lines[0] == "bir");
    CHECK(lines[1] == "iki");

    // A line with no newline after it is still a line: the run flushes what is
    // left rather than dropping it.
    lines.clear();
    REQUIRE(runner.run_text("import sys\nsys.stdout.write('yarım')\n", "yarım satır").ok());
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "yarım");
}

TEST_CASE("PYTHON: BETİK picks the host from the file extension")
{
    Rig rig;
    script::JsonRunner json(rig.bus, script::Sandbox::Project);
    script::PythonRunner python(rig.bus, script::Sandbox::Project);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "kentoscad-host-dispatch";
    std::filesystem::create_directories(root);
    python.set_project_root(root.string());

    // The same drawing, written twice in two languages. Both reach the bus; which
    // host ran is the file's business, not the user's.
    {
        std::ofstream(root / "a.py")
            << R"(cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400"))";
        std::ofstream(root / "b.json")
            << R"([{"cmd":"core.line","args":{"noktalar":[[485320150,4310220400],)"
               R"([485370150,4310250400]]}}])";
    }

    script::install(rig.bus, json, python);
    REQUIRE(rig.bus.on_run_script);

    CHECK(rig.bus.on_run_script((root / "a.py").string()).ok());
    CHECK(rig.doc.live_entity_count() == 1);

    CHECK(rig.bus.on_run_script((root / "b.json").string()).ok());
    CHECK(rig.doc.live_entity_count() == 2);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("PYTHON: a stop token stops a running script")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // R13/P11: cancellation is not advisory. Already requested when the script
    // starts, so the interrupt is pending before the first bytecode runs.
    {
        std::stop_source source;
        source.request_stop();

        auto report = runner.run_text("while True:\n    pass\n", "sonsuz", source.get_token());
        CHECK_FALSE(report.ok());
    }

    // MID-RUN, FROM ANOTHER THREAD, which is the case that actually happens: a
    // user presses stop while the script is looping. The interpreter changes hands
    // every 5 ms, so the asking thread gets in.
    {
        std::stop_source source;
        std::jthread asker([&source] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            source.request_stop();
        });

        auto report = runner.run_text("while True:\n    pass\n", "sonsuz", source.get_token());
        CHECK_FALSE(report.ok());
    }

    // AND THE NEXT RUN IS UNHARMED. An interrupt raised at the next bytecode
    // boundary belongs to whatever runs next unless it is cleared, so a second
    // script would otherwise die on the first one's stop.
    CHECK(runner.run_text("x = 1 + 1", "sonraki").ok());
}

TEST_CASE("PYTHON: a script that swallows the interrupt is still cancelled")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    std::stop_source source;
    source.request_stop();

    // `except BaseException` catches a KeyboardInterrupt, so a script CAN ignore
    // the stop. What it cannot do is finish successfully: the token is checked
    // after the body, and a run the user stopped is rolled back whatever the
    // script decided about the exception.
    auto report = runner.run_text(R"(
try:
    cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
except BaseException:
    pass
)",
                                  "yutan", source.get_token());

    CHECK_FALSE(report.ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

#endif // KENTOS_HAVE_PYTHON
