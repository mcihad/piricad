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

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
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

TEST_CASE("PYTHON: a caught refusal takes back its own edits and no one else's")
{
    // A refusal is an error now (TODOS F-01), so `cad.run` raises on one, and a
    // script may catch it. What the catch must not do is decide the fate of the
    // lines before it: the refused command shares the script's transaction, and
    // rolling THAT back would drop the first line while the journal still said
    // it was drawn.
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
cad.run("ÇİZGİ 0,0 10,0")
try:
    cad.run("SİL nesneler=99")
except Exception as hata:
    caught = str(hata)
cad.run("ÇİZGİ 0,5 10,5")
assert "99" in caught
)",
                                  "yakalanan ret");

    REQUIRE(report.ok());
    CHECK(rig.doc.live_entity_count() == 2);
    CHECK(rig.undo.undo_depth() == 1);
}

TEST_CASE("PYTHON: cad.undo() on its own is the command line's GERİAL")
{
    // docs/komutlar/undo.md says so, and this is where it is held to it. A console
    // line runs in a batch; one that only undoes has nothing to cut across, closes
    // with no step of its own, and leaves the redo stack for cad.redo().
    Rig rig;
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    REQUIRE(runner.run_text("cad.undo()", "konsol").ok());
    CHECK(rig.doc.live_entity_count() == 0);
    REQUIRE(runner.run_text("cad.redo()", "konsol").ok());
    CHECK(rig.doc.live_entity_count() == 1);

    // After the script has drawn, the same call is refused, and the script's own
    // line goes with the failed run rather than the user's earlier one.
    auto mixed = runner.run_text("cad.run('ÇİZGİ 0,5 10,5')\ncad.undo()", "karışık");
    CHECK_FALSE(mixed.ok());
    CHECK(rig.doc.live_entity_count() == 1);
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
before = cad.doc.entity_count()
cad.run("ÇİZGİ 485320150,4310220400 485370150,4310250400")
assert cad.doc.entity_count() == before + 1, "sayım yanlış"
assert isinstance(cad.doc.layers(), list), "layers() liste değil"
assert isinstance(cad.doc.crs(), str), "crs() metin değil"
assert isinstance(cad.doc.layer_count(), int), "layer_count() tamsayı değil"
assert isinstance(cad.doc.selection_count(), int)
assert isinstance(cad.doc.active_layer(), str)
assert cad.sandbox() == "güvenli"

# A setting arrives as the type it IS, which is the whole reason `setting` does
# not hand everything back as a string.
assert isinstance(cad.doc.setting("core.arayuz.dinamik_girdi"), bool)
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

// ---- the projected surface (CLAUDE.md 5.10, 5.20) ---------------------------

TEST_CASE("PYTHON: every command is a callable, generated from the registry")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // ONE CALLABLE PER COMMAND, and the count is the registry's, not a list's.
    // The point of the projection is that nobody maintains this number.
    std::size_t scriptable = 0;
    for (const CommandSpec& spec : rig.reg.all())
        if (spec.run != nullptr && has_flag(spec.flags, Flags::Scriptable)) ++scriptable;
    REQUIRE(scriptable > 80);

    auto report = runner.run_text("import kentos.cad\n"
                                  "assert len(cad.__all__) == " +
                                      std::to_string(scriptable) +
                                      ", f'{len(cad.__all__)} callables'\n"
                                      "assert 'line' in cad.__all__\n"
                                      "assert callable(cad.line)\n",
                                  "projeksiyon");

    CHECK(report.ok());
}

TEST_CASE("PYTHON: a generated callable draws, with English keywords")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
cad.run("KATMAN PARSEL")
cad.line(points=[[485320150, 4310220400], [485370150, 4310250400]])
cad.circle_draw(center=[485400000, 4310230000], rim=[485410000, 4310230000])
)",
                                  "üretilmiş çağrı");

    REQUIRE(report.ok());
    CHECK(rig.doc.live_entity_count() == 2);

    // R1 AGAIN, AND IT IS THE WHOLE POINT: a generated callable is not a second
    // way in. It builds an `Invocation` and dispatches it, so the journal cannot
    // tell it from the command line — which is what makes Article 6.4's equality
    // proof hold with Python in it.
    REQUIRE(rig.journal.entries().size() == 3);
    CHECK(rig.journal.entries()[1].command_id == "core.line");
    CHECK(rig.journal.entries()[2].command_id == "core.circle_draw");
}

TEST_CASE("PYTHON: the Turkish keyword is not the Python keyword")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // THE API IS ENGLISH AND ONLY ENGLISH. Accepting `noktalar=` as well would be
    // two names for one argument in one call, which is the thing `Param::english`
    // exists to avoid rather than to double.
    CHECK_FALSE(runner.run_text("cad.line(noktalar=[[0,0],[1,1]])", "türkçe anahtar").ok());
    CHECK(rig.doc.live_entity_count() == 0);

    // And the refusal SAYS WHAT IS ACCEPTED, because the reader is at a prompt
    // with the answer one line away (script.md R21).
    auto refused = runner.run_text("cad.line(noktalar=[[0,0],[1,1]])", "türkçe anahtar");
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("points") != std::string::npos);
}

TEST_CASE("PYTHON: a generated callable takes keywords only")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // A command's parameters are a SET; the command line has never had a
    // positional order for them. Inventing one here would be a second grammar and
    // it would reorder silently the day a parameter is added (CLAUDE.md 5.11).
    auto refused = runner.run_text("cad.line([[0,0],[1,1]])", "konumsal");
    CHECK_FALSE(refused.ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

TEST_CASE("PYTHON: True is a boolean and not the integer one")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // In Python `isinstance(True, int)` is true, so a converter that asked about
    // integers first would turn every yes/no argument into 1. `core.layer` takes
    // `gorunur` as a boolean; this passes only if the order is right.
    auto report = runner.run_text(R"(
cad.layer(name="GİZLİ")
cad.layer_visibility(action="gizle", layer="GİZLİ")
)",
                                  "mantıksal");
    CHECK(report.ok());
}

TEST_CASE("PYTHON: the registry's English names are unique and importable")
{
    Rig rig;

    // THE GATE THAT THE SHELL GATE CANNOT BE. `ci-gate-python-api.sh` reads the
    // SOURCE and so it sees a declaration at a time; only the assembled registry
    // can say whether two parameters of ONE command ended up with one English
    // word, or whether two commands claim one callable name.
    std::set<std::string> callables;
    for (const CommandSpec& spec : rig.reg.all()) {
        if (spec.run == nullptr || !has_flag(spec.flags, Flags::Scriptable)) continue;

        std::set<std::string> keywords;
        for (const Param& p : spec.params) {
            INFO(spec.id << " / " << p.name);
            REQUIRE_FALSE(p.english.empty());

            // A valid Python identifier, and never one of the words that would
            // make the call a SyntaxError.
            CHECK(p.english.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") ==
                  std::string::npos);
            CHECK_FALSE(std::isdigit(static_cast<unsigned char>(p.english.front())) != 0);
            CHECK(p.english != "class");
            CHECK(p.english != "from");
            CHECK(p.english != "import");
            CHECK(p.english != "lambda");
            CHECK(p.english != "None");

            CHECK(keywords.insert(p.english).second);
        }

        const std::string fn = command::python_callable_name(spec);
        INFO(spec.id << " -> " << fn);
        CHECK(callables.insert(fn).second);

        // AND IT IS ENGLISH, which the derivation gives for an English id and a
        // `CommandSpec::python` gives for a Turkish one. The check a machine CAN
        // make is that the name never carries a Turkish letter — the rest is the
        // reviewer's, and Article 6.15 says so.
        CHECK(fn.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos);
    }
}

// ---- the value types and the viewport -------------------------------------

TEST_CASE("PYTHON: Point and Box are the program's own types")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    auto report = runner.run_text(R"(
p = cad.Point(east=485320150, north=4310220400)
assert p.east == 485320150
assert p.north == 4310220400

# Indexable and iterable in the order the command line and the journal use:
# east first. `list(p)` and a hand-written pair are the same two numbers.
assert list(p) == [485320150, 4310220400]
assert p[0] == p.east and p[1] == p.north
assert len(p) == 2

q = cad.Point(east=485320150, north=4310220400)
assert p == q and hash(p) == hash(q)
assert cad.Point(east=0, north=0) != p

# Metres, not millimetres: a distance is a measurement and the drawing's unit is
# what a user reads off a pafta.
a = cad.Point(east=0, north=0)
b = cad.Point(east=3000, north=4000)
assert abs(a.distance_to(b) - 5.0) < 1e-9

box = cad.Box(min_east=0, min_north=0, max_east=10000, max_north=20000)
assert box.width == 10000 and box.height == 20000
assert box.center == cad.Point(east=5000, north=10000)
assert box.contains(cad.Point(east=0, north=0))
assert not box.contains(cad.Point(east=-1, north=0))
assert not box.is_empty()
assert len(box.corners) == 4
assert list(box) == [0, 0, 10000, 20000]

# An empty box is a real state, not an error.
assert cad.Box(min_east=1, min_north=1, max_east=0, max_north=0).is_empty()
)",
                                  "tipler");

    CHECK(report.ok());
}

TEST_CASE("PYTHON: a Point may be passed where a coordinate is expected")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // A read hands back Points; a command takes them. Making the user spell
    // `list(p)` would be one more thing to remember for no reason.
    auto report = runner.run_text(R"(
a = cad.Point(east=485320150, north=4310220400)
b = cad.Point(east=485370150, north=4310250400)
cad.line(points=[a, b])
)",
                                  "nokta argümanı");

    REQUIRE(report.ok());
    CHECK(rig.doc.live_entity_count() == 1);
}

TEST_CASE("PYTHON: the viewport says when there is no window")
{
    Rig rig;
    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);

    // A HEADLESS RUN HAS NO VIEW and saying so is the whole point. Inventing a
    // rectangle would put the next drawing somewhere nobody is looking, which is
    // the same reason `core.view_info` refuses rather than guessing.
    REQUIRE(runner.run_text("assert cad.viewport.exists() is False", "pencere yok").ok());
    CHECK_FALSE(runner.run_text("cad.viewport.bbox()", "pencere yok, kutu istendi").ok());
}

TEST_CASE("PYTHON: the viewport answers with values, not a report")
{
    Rig rig;

    // The same hook `GÖRÜNÜMBİLGİSİ` reads. One source, two presentations: the
    // command writes a sentence a person reads, this hands back numbers a script
    // computes with (CLAUDE.md 5.10 is about second LISTS, not second views).
    rig.bus.on_view_query = [] {
        command::ViewInfo v;
        v.window       = core::Box2{485300000, 4310200000, 485500000, 4310300000};
        v.centre       = core::Point2{485400000, 4310250000};
        v.scale        = 1000;
        v.mm_per_pixel = 42.5;
        v.width_px     = 1880;
        v.height_px    = 1058;
        v.crs          = "TUREF/TM36";
        return v;
    };

    script::PythonRunner runner(rig.bus, script::Sandbox::Safe);
    auto report = runner.run_text(R"(
assert cad.viewport.exists()

bbox = cad.viewport.bbox()
assert isinstance(bbox, cad.Box)
assert bbox.min_east == 485300000 and bbox.max_north == 4310300000
assert bbox.width == 200000 and bbox.height == 100000

assert cad.viewport.center() == cad.Point(east=485400000, north=4310250000)
assert cad.viewport.scale() == 1000
assert abs(cad.viewport.mm_per_pixel() - 42.5) < 1e-9
assert cad.viewport.size_px() == (1880, 1058)
assert cad.viewport.crs() == "TUREF/TM36"

# And the box composes with the types: a drawing fitted to what is on screen.
assert bbox.contains(cad.viewport.center())
)",
                                  "görünüm");

    CHECK(report.ok());
}

#endif // KENTOS_HAVE_PYTHON
