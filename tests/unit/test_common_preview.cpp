// SPDX-License-Identifier: GPL-3.0-or-later
//
// The common preview (TODOS F-05, command/preview.hpp): steps run through the
// real bus inside one batch and are taken back whole, so what they WOULD do is
// counted and outlined while the drawing — its content, revision, keys, undo
// and redo stacks, journal and selection — comes out exactly as it went in.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/preview.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"
#if KENTOS_HAVE_PYTHON
#include "kentos_cad/script/python_runner.hpp"
#endif

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

/// Everything a preview must leave as it found it.
struct State
{
    std::uint64_t hash{0};
    std::uint64_t revision{0};
    std::uint64_t next_key{0};
    std::size_t undo{0};
    std::size_t redo{0};
    std::size_t journal{0};
    std::size_t layers{0};
    std::vector<core::EntityKey> selected;

    bool operator==(const State&) const = default;
};

State state_of(const Rig& r)
{
    return State{r.doc.content_hash(),  r.doc.revision(),        r.doc.keys().peek_entity(),
                 r.undo.undo_depth(),   r.undo.redo_depth(),     r.journal.size(),
                 r.doc.layers().size(), r.bus.selection().keys()};
}

} // namespace

TEST_CASE("ÖNİZLE: ne yapacağını söyler, çizime dokunmaz (F-05)")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("SEÇ nesneler=1");
    const State before = state_of(r);

    r.said.clear();
    auto seen = r.bus.execute_line(
        "ÖNİZLE komut=\"TAŞI nesneler=1 baslangic=0,0 bitis=5,0\" komut=\"ÇİZGİ 0,30 10,30\" "
        "komut=\"KATMAN ad=YENİ\" taslaklar=evet",
        Origin::Test);
    if (!seen) FAIL_WITH("ÖNİZLE", seen.error().message);
    CHECK(r.said.find("Önizleme: 3 adım — uygulanırsa 1 nesne eklenecek; 1 nesnenin yeri ya da "
                      "biçimi değişecek. Çizim değişmedi.") != std::string::npos);

    // NOTHING MOVED: content, revision, the next key, both stacks, the journal,
    // the layers the step made, the selection.
    CHECK(state_of(r) == before);

    // The answer counts and outlines what it would leave.
    const core::Json& report = seen.value().report;
    REQUIRE(report.find("degisiklik") != nullptr);
    CHECK_EQ(report.find("degisiklik")->find("eklenen")->as_int(), 1);
    CHECK_EQ(report.find("degisiklik")->find("yeri_bicimi_degisen")->as_int(), 1);
    REQUIRE(report.find("taslaklar") != nullptr);
    const auto& shapes = report.find("taslaklar")->as_array();
    REQUIRE_EQ(shapes.size(), std::size_t{2});
    // The moved parcel names its key; the new line has none — a preview hands
    // out no key (model.md R4a).
    bool moved = false;
    bool drawn = false;
    for (const core::Json& one : shapes) {
        if (one.find("yeni")->as_bool()) {
            drawn = one.find("nesne") == nullptr;
            CHECK_EQ(one.find("halkalar")->as_array().size(), std::size_t{1});
        } else {
            moved = one.find("nesne")->as_int() == 1;
            const core::Json& first =
                one.find("halkalar")->as_array()[0].find("noktalar")->as_array()[0];
            CHECK_EQ(first.as_array()[0].as_int(), 5000); // five metres east
        }
    }
    CHECK(moved);
    CHECK(drawn);
}

TEST_CASE("ÖNİZLE: duracağı adımı söyler; dış etkili adımları çalıştırmaz (F-05)")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    const State before = state_of(r);

    r.said.clear();
    REQUIRE(r.bus
                .execute_line("ÖNİZLE komut=\"ÇİZGİ 0,5 10,5\" komut=\"SİL nesneler=999999\"",
                              Origin::Test)
                .ok());
    CHECK(r.said.find("Önizleme: 2. adımda duracak — Nesne bulunamadı veya zaten silinmiş: "
                      "999999. Uygulanırsa bütünüyle geri alınacak; çizim değişmedi.") !=
          std::string::npos);
    CHECK(state_of(r) == before);

    // A FILE, THE VIEW, THE UNDO STACK: not run, each with its reason, and the
    // step after them still runs.
    r.said.clear();
    REQUIRE(r.bus
                .execute_line("ÖNİZLE komut=\"DIŞAAKTAR /tmp/onizleme.dxf\" komut=\"GERİAL\" "
                              "komut=\"ÇİZGİ 0,5 10,5\"",
                              Origin::Test)
                .ok());
    CHECK(r.said.find("1. adım (core.export) önizlemede çalıştırılmadı: dosya yazar.") !=
          std::string::npos);
    CHECK(r.said.find("2. adım (core.undo) önizlemede çalıştırılmadı: çizimi işlemin dışında "
                      "değiştirir.") != std::string::npos);
    CHECK(r.said.find("uygulanırsa 1 nesne eklenecek") != std::string::npos);
    CHECK(state_of(r) == before);
}

TEST_CASE("ÖNİZLE: betiğin içinde bir kayıt noktasıdır; ajan için kapalı komutu çalıştırmaz (F-05)")
{
    // INSIDE A SCRIPT the preview is a savepoint in the script's own batch: the
    // script's work before it and after it stands, what the preview ran does
    // not — and the script is still one undo step.
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    const std::size_t undo_before = r.undo.undo_depth();
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"({"ad": "Ortada önizleme", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 5000], [10000, 5000]]}},
        {"cmd": "core.preview", "args": {"komut": ["SİL nesneler=1", "ÇİZGİ 0,9 10,9"]}},
        {"cmd": "core.line", "args": {"noktalar": [[0, 7000], [10000, 7000]]}}]})");
    if (!ran) FAIL_WITH("betik", ran.error().message);
    CHECK(r.said.find("Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek; 1 nesne silinecek.") !=
          std::string::npos);
    CHECK(r.doc.alive(r.doc.slot_of(static_cast<core::EntityKey>(1)))); // not erased
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{3});                // 1 + the script's 2
    CHECK_EQ(r.undo.undo_depth(), undo_before + 1);
    // The keys run on as if no preview had been: the script's second line is 3.
    CHECK(r.doc.slot_of(static_cast<core::EntityKey>(3)) != core::kNoEntity);
    CHECK_EQ(r.doc.keys().peek_entity(), std::uint64_t{4});

    // FOR AN AGENT a preview runs only what an agent may call: SEÇ is closed to
    // one (a changed highlight would change what the next SİL erases).
    const State before = state_of(r);
    Invocation inv;
    inv.name   = "core.select";
    inv.origin = Origin::Ai;
    inv.args.set("nesneler", Value::ids({1}));
    auto seen = r.bus.preview(std::vector<Invocation>{inv});
    REQUIRE(seen.ok());
    REQUIRE_EQ(seen.value().skipped.size(), std::size_t{1});
    CHECK_EQ(seen.value().skipped[0].reason, "yapay zekâya kapalı bir komut");
    CHECK(state_of(r) == before);

    // AND WHEN THE AGENT ASKS THROUGH ÖNİZLE ITSELF — the lines are text, and
    // the bus, not the command body, knows who is asking.
    Invocation asked;
    asked.name   = "core.preview";
    asked.origin = Origin::Ai;
    asked.args.set("komut", Value::texts({"SEÇ nesneler=1", "ÇİZGİ 0,5 10,5"}));
    r.said.clear();
    auto via = r.bus.dispatch(asked);
    REQUIRE(via.ok());
    CHECK(r.said.find("1. adım (core.select) önizlemede çalıştırılmadı: yapay zekâya kapalı bir "
                      "komut.") != std::string::npos);
    CHECK(r.said.find("uygulanırsa 1 nesne eklenecek") != std::string::npos);
    CHECK(state_of(r) == before);
}

TEST_CASE("ÖNİZLE: 500 parsellik bir işi sayar, sınırın ötesini sayıya katar (F-05)")
{
    // A preview of a large job counts all of it and outlines what it may: the
    // canvas draws a bounded number of ghosts, the numbers are whole.
    Rig r;
    std::string json = R"({"ad": "Kurulum", "komutlar": [)";
    for (int i = 0; i < 500; ++i) {
        const int x = (i % 25) * 12000;
        const int y = (i / 25) * 12000;
        json += (i == 0 ? "" : ",");
        json += R"({"cmd": "core.area", "args": {"noktalar": [[)" + std::to_string(x) + "," +
                std::to_string(y) + "],[" + std::to_string(x + 10000) + "," + std::to_string(y) +
                "],[" + std::to_string(x + 10000) + "," + std::to_string(y + 10000) + "]]}}";
    }
    json += "]}";
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    REQUIRE(runner.run_text(json).ok());
    const State before = state_of(r);

    std::vector<Invocation> steps;
    for (int i = 1; i <= 500; ++i) {
        auto inv = r.bus.parse_invocation(
            "TAŞI nesneler=" + std::to_string(i) + " baslangic=0,0 bitis=1,0", Origin::Test);
        REQUIRE(inv.ok());
        steps.push_back(inv.value());
    }
    auto seen = r.bus.preview(steps);
    REQUIRE(seen.ok());
    CHECK_EQ(seen.value().ran, std::size_t{500});
    CHECK_EQ(seen.value().changes.reshaped, std::size_t{500});
    CHECK_EQ(seen.value().shapes.size() + seen.value().shapes_left_out, std::size_t{500});
    CHECK(state_of(r) == before);
}

TEST_CASE("BETİK onizle=evet: JSON betiğinin ne yapacağını söyler, çalıştırmaz (F-05)")
{
    Rig r;
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    script::install(r.bus, runner);
    r.run("ÇİZGİ 0,0 10,0");
    const auto dir = std::filesystem::temp_directory_path() / "kentoscad-betik-onizleme";
    std::filesystem::create_directories(dir);
    const auto path = (dir / "iki.json").string();
    std::ofstream(path) << R"({"ad": "İki", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 5000], [10000, 5000]]}},
        {"cmd": "core.move", "args": {"nesneler": [1], "baslangic": [0, 0], "bitis": [0, 2000]}}]})";
    const State before = state_of(r);

    r.said.clear();
    auto seen = r.bus.execute_line("BETİK \"" + path + "\" onizle=evet", Origin::Test);
    if (!seen) FAIL_WITH("BETİK önizleme", seen.error().message);
    CHECK(r.said.find("Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek; 1 nesnenin yeri ya da "
                      "biçimi değişecek. Çizim değişmedi.") != std::string::npos);
    CHECK(r.said.find("Betik tamamlandı") == std::string::npos);
    CHECK(state_of(r) == before);
    CHECK_EQ(seen.value().report.find("degisiklik")->find("eklenen")->as_int(), 1);
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

#if KENTOS_HAVE_PYTHON
TEST_CASE("BETİK onizle=evet: Python betiği önizlenmez ve sebebini söyler (F-05)")
{
    Rig r;
    script::JsonRunner json(r.bus, script::Sandbox::Project);
    script::PythonRunner python(r.bus, script::Sandbox::Project);
    script::install(r.bus, json, python);
    auto seen = r.bus.execute_line("BETİK /tmp/herhangi.py onizle=evet", Origin::Test);
    REQUIRE_FALSE(seen.ok());
    CHECK(seen.error().message.find("Python betiği önizlenmez: ne yapacağı ancak çalışınca "
                                    "bellidir") != std::string::npos);
}
#endif

TEST_CASE("ÖNİZLE: kılavuzdaki çıktılar kelimesi kelimesine (F-05)")
{
    // docs/komutlar/preview.md prints these lines; the page is only true while
    // this is.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("ÇİZGİ 0,30 20,30");
    const auto said = [&r](const std::string& line) {
        r.said.clear();
        r.run(line);
        return r.said;
    };
    CHECK_EQ(
        said("ÖNİZLE komut=\"TAMPON nesneler=1 mesafe=5 katman=BANT\" komut=\"SİL nesneler=2\""),
        "Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek; 1 nesne silinecek. Çizim "
        "değişmedi.\n");
    CHECK(r.doc.find_layer("BANT") == core::kNoLayer);
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(said("ÖNİZLE komut=\"ÇİZGİ 0,5 10,5\" komut=\"SİL nesneler=99\""),
             "Önizleme: 2. adımda duracak — Nesne bulunamadı veya zaten silinmiş: 99. Uygulanırsa "
             "bütünüyle geri alınacak; çizim değişmedi.\n");
    CHECK_EQ(said("ÖNİZLE komut=\"DIŞAAKTAR teslim/ada.dxf\" komut=\"ÇİZGİ 0,5 10,5\""),
             "Önizleme: 2 adım — uygulanırsa 1 nesne eklenecek. Çizim değişmedi.\n"
             "  1. adım (core.export) önizlemede çalıştırılmadı: dosya yazar.\n");
}

TEST_CASE("BETİK onizle=evet: kılavuzun örnek betiği kılavuzdaki cümleyi yazar (F-05)")
{
    // docs/komutlar/script.md prints this preview of the manual's sample script.
    Rig r;
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto seen = runner.preview_file(std::string(KENTOS_JOURNAL_DIR) + "/ornek-parsel.json");
    if (!seen) FAIL_WITH("önizleme", seen.error().message);
    CHECK_EQ(describe_preview(seen.value()),
             "Önizleme: 9 adım — uygulanırsa 14 nesne eklenecek; 4 katmanın ayarları değişecek. "
             "Çizim değişmedi.");
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(r.doc.layers().size(), std::size_t{1});
}
