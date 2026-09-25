// SPDX-License-Identifier: GPL-3.0-or-later
//
// One logical operation (TODOS F-05): a job of five hundred objects that fails
// or is stopped half way leaves the drawing exactly as it was — its content, its
// undo AND redo stacks and its journal — and one that succeeds is one undo step
// that takes every part of it back: geometry, attributes and the followers that
// moved with them.
//
// "Exactly as it was" is measured, not assumed: the content hash, the two stack
// depths and the journal length before and after. The journal is in it because
// it is the record a replay rebuilds the drawing from — a line for a command
// whose edit was rolled back is a drawing that comes back on replay.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/changes.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#if KENTOS_HAVE_PYTHON
#include "kentos_cad/script/python_runner.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
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

/// Everything a half-done job could leave behind.
struct Mark
{
    std::uint64_t hash{0};
    std::size_t undo{0};
    std::size_t redo{0};
    std::size_t journal{0};
    std::size_t live{0};

    bool operator==(const Mark&) const = default;
};

/// Printed field by field, so a failure says WHICH trace the job left.
std::ostream& operator<<(std::ostream& out, const Mark& m)
{
    return out << "{özet " << m.hash << ", geri " << m.undo << ", yinele " << m.redo << ", günlük "
               << m.journal << ", nesne " << m.live << "}";
}

Mark mark(const Rig& r)
{
    return Mark{r.doc.content_hash(), r.undo.undo_depth(), r.undo.redo_depth(), r.journal.size(),
                r.doc.live_entity_count()};
}

/// The key of the `i`-th parcel `draw_parcels` drew, from 1.
constexpr std::int64_t parcel(int i)
{
    return i;
}

/// `n` ten-metre parcels on PARSEL in a grid, each with its `ada` and a caption
/// that FOLLOWS it (`ETİKET`, bound by default): parcels are keys 1..n, their
/// captions n+1..2n.
void draw_parcels(Rig& r, int n)
{
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    std::string json = R"({"ad": "Kurulum", "komutlar": [)";
    for (int i = 0; i < n; ++i) {
        const int x = (i % 25) * 12000;
        const int y = (i / 25) * 12000;
        json += (i == 0 ? "" : ",");
        json += R"({"cmd": "core.area", "args": {"noktalar": [[)" + std::to_string(x) + "," +
                std::to_string(y) + "],[" + std::to_string(x + 10000) + "," + std::to_string(y) +
                "],[" + std::to_string(x + 10000) + "," + std::to_string(y + 10000) + "],[" +
                std::to_string(x) + "," + std::to_string(y + 10000) + "]]}}";
        json += R"(,{"cmd": "core.attribute", "args": {"ad": "ada", "nesne": )" +
                std::to_string(i + 1) + R"(, "deger": ")" + std::to_string(100 + i) + R"("}})";
    }
    json += "]}";
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto made = runner.run_text(json);
    if (!made) FAIL_WITH("kurulum", made.error().message);
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\"");
    REQUIRE_EQ(r.doc.live_entity_count(), static_cast<std::size_t>(2 * n));
}

/// The words the caption of parcel `i` says, from 1.
std::string caption_of(const Rig& r, int n, int i)
{
    const core::EntityId e =
        r.doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(n + i)));
    if (e == core::kNoEntity || !r.doc.alive(e)) return {};
    const std::uint32_t slot = r.doc.entities().slot[e];
    return r.doc.texts().has(slot) ? std::string(r.doc.texts().text(slot)) : std::string();
}

/// A script that moves every parcel five metres east and gives it a new `ada`
/// — the captions follow both — and, when `fail_at` is set, a command that
/// cannot run after that many parcels.
std::string move_and_revalue(int n, int fail_at = 0)
{
    std::string json = R"({"ad": "Kaydırma", "komutlar": [)";
    for (int i = 1; i <= n; ++i) {
        if (i == fail_at) json += R"({"cmd": "core.erase", "args": {"nesneler": [999999]}},)";
        json += R"({"cmd": "core.move", "args": {"nesneler": [)" + std::to_string(i) +
                R"(], "baslangic": [0, 0], "bitis": [5000, 0]}},)";
        json += R"({"cmd": "core.attribute", "args": {"ad": "ada", "nesne": )" + std::to_string(i) +
                R"(, "deger": ")" + std::to_string(1000 + i) + R"("}})";
        json += (i == n ? "" : ",");
    }
    json += "]}";
    return json;
}

} // namespace

TEST_CASE("TEK İŞLEM: 500 parselin taşınması, değeri ve izleyen yazıları tek adımdır (F-05)")
{
    // GEOMETRY, ATTRIBUTES AND WHAT FOLLOWS THEM, applied together. The script
    // moves 500 parcels and re-values them; their 500 captions move with them
    // and re-word themselves. All of it is ONE undo step, and one GERİAL takes
    // back every part — and one YİNELE puts every part back.
    Rig r;
    draw_parcels(r, 500);
    const Mark before = mark(r);
    REQUIRE_EQ(caption_of(r, 500, 1), "100");

    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_text(move_and_revalue(500));
    if (!ran) FAIL_WITH("betik", ran.error().message);

    CHECK_EQ(r.undo.undo_depth(), before.undo + 1);
    CHECK_EQ(caption_of(r, 500, 1), "1001");
    CHECK_EQ(caption_of(r, 500, 500), "1500");
    const std::uint64_t after = r.doc.content_hash();
    CHECK_NE(after, before.hash);

    r.run("GERİAL");
    CHECK_EQ(r.doc.content_hash(), before.hash);
    CHECK_EQ(caption_of(r, 500, 1), "100");
    r.run("YİNELE");
    CHECK_EQ(r.doc.content_hash(), after);
}

TEST_CASE("TEK İŞLEM: 500 parsellik betik ortada hata verince hiçbir iz bırakmaz (F-05)")
{
    // THE FAILURE HALF WAY: 250 parcels moved and re-valued, their captions
    // followed, and then a command that cannot run. Nothing of the 250 may stay
    // — not in the drawing, not on the REDO stack (YİNELE must not bring half a
    // script back), not in the journal (a replay must not either).
    Rig r;
    draw_parcels(r, 500);
    const Mark before = mark(r);

    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_text(move_and_revalue(500, 251));
    REQUIRE_FALSE(ran.ok());
    CHECK(ran.error().message.find("999999") != std::string::npos);

    CHECK_EQ(mark(r), before);
    CHECK_EQ(caption_of(r, 500, 1), "100");

    // And YİNELE has nothing of it to bring back.
    auto again = r.bus.execute_line("YİNELE", Origin::Test);
    CHECK_FALSE(again.ok());
    CHECK_EQ(r.doc.content_hash(), before.hash);
}

TEST_CASE("TEK İŞLEM: 500 nesneyi silen komut 300. nesnede kilide takılınca hiçbiri silinmez "
          "(F-05)")
{
    // ONE COMMAND, FAILING INSIDE ITS OWN BODY: SİL erases 299 objects and meets
    // the 300th on a locked layer. The 299 come back in the same breath, and the
    // captions that were erased with them come back too.
    Rig r;
    draw_parcels(r, 500);
    r.run("KATMAN ad=KİLİTLİ");
    std::vector<std::int64_t> keys;
    for (int i = 1; i <= 500; ++i)
        keys.push_back(parcel(i));
    // Parcel 300 to the layer that will be locked.
    r.run("KATMANAT nesneler=300 katman=KİLİTLİ");
    r.run("KATMAN ad=KİLİTLİ kilitli=evet");
    const Mark before = mark(r);

    Invocation erase;
    erase.name   = "core.erase";
    erase.origin = Origin::Test;
    erase.args.set("nesneler", Value::ids(keys));
    auto erased = r.bus.dispatch(erase);
    REQUIRE_FALSE(erased.ok());
    CHECK(erased.error().message.find("kilitli") != std::string::npos);
    CHECK_EQ(mark(r), before);
    CHECK_EQ(caption_of(r, 500, 1), "100");
}

TEST_CASE("TEK İŞLEM: 500 nesne üzerinde yarıda durdurulan işlem aracı hiçbir iz bırakmaz (F-05)")
{
    // STOPPED WHILE IT WORKS: TAMPON over 500 parcels, on its worker, stopped
    // once it has begun. The runner writes nothing — no output layer, no undo
    // step, no journal line — whatever the worker had reached.
    Rig r;
    draw_parcels(r, 500);
    Session* parked   = nullptr;
    r.bus.on_job_host = [&parked](Session& s) { parked = &s; };
    const Mark before = mark(r);

    std::string line = "TAMPON mesafe=2 katman=BANT";
    for (int i = 1; i <= 500; ++i)
        line += " nesneler=" + std::to_string(i);
    auto started = r.bus.begin_interactive(line);
    REQUIRE(started.ok());
    Session& session = *started.value();
    REQUIRE(session.working());
    REQUIRE(parked == &session);
    Job& job = *session.job();

    // The stop lands while the worker is inside the loop: requested as soon as
    // its progress moves off zero.
    std::atomic<bool> done{false};
    std::uint32_t at_stop = 0;
    std::thread watcher([&] {
        while (!done.load() && job.permille.load() == 0) {
        }
        at_stop = job.permille.load();
        job.stop.request_stop();
    });
    job.work(JobControl{job.stop.get_token()});
    done.store(true);
    watcher.join();
    session.resume_job();
    auto finished = r.bus.finish(session);
    MESSAGE("durdurulduğunda ilerleme: ", at_stop, "‰");

    CHECK_EQ(mark(r), before);
    CHECK(r.doc.find_layer("BANT") == core::kNoLayer);
    (void)finished;
}

#if KENTOS_HAVE_PYTHON

TEST_CASE("TEK İŞLEM: 500 komutluk Python betiği yakalanmayan hatayla hiçbir iz bırakmaz (F-05)")
{
    Rig r;
    draw_parcels(r, 500);
    const Mark before = mark(r);

    script::PythonRunner runner(r.bus, script::Sandbox::Safe);
    auto report = runner.run_text(R"(
for i in range(1, 501):
    cad.run(f"TAŞI nesneler={i} baslangic=0,0 bitis=5,0")
    if i == 250:
        raise RuntimeError("yarıda")
)",
                                  "yarıda kalan");
    REQUIRE_FALSE(report.ok());
    CHECK_EQ(mark(r), before);
    CHECK_FALSE(r.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK_EQ(r.doc.content_hash(), before.hash);
}

TEST_CASE("TEK İŞLEM: Python'da yakalanan ret kendi izini alır, 500 taşıma tek adımdır (F-05)")
{
    // A script may catch a refusal and go on; the refused command's own edits go,
    // the others stay — and the journal holds exactly the ones that stayed.
    Rig r;
    draw_parcels(r, 500);
    const Mark before = mark(r);

    script::PythonRunner runner(r.bus, script::Sandbox::Safe);
    auto report = runner.run_text(R"(
for i in range(1, 501):
    cad.run(f"TAŞI nesneler={i} baslangic=0,0 bitis=5,0")
    if i == 250:
        try:
            cad.run("SİL nesneler=999999")
        except Exception:
            pass
)",
                                  "yakalanan");
    if (!report) FAIL_WITH("betik", report.error().message);
    CHECK_EQ(r.undo.undo_depth(), before.undo + 1);
    CHECK_EQ(r.journal.size(), before.journal + 500);
    r.run("GERİAL");
    CHECK_EQ(r.doc.content_hash(), before.hash);
}

#endif

TEST_CASE("YİNELE: aynı nesneye iki kez dokunan adım, ikinci düzenlemenin sonucuna döner (F-05)")
{
    // THE REGRESSION: the redo record was applied newest first. Right when every
    // object is touched once; wrong when one is touched twice — two moves of a
    // parcel in one batch, or a move and a new value that each re-wrote its
    // caption — and YİNELE came back with the FIRST edit's result.
    const auto one_step = [](const char* steps) {
        Rig r;
        draw_parcels(r, 1);
        script::JsonRunner runner(r.bus, script::Sandbox::Project);
        auto ran =
            runner.run_text(std::string(R"({"ad": "İki kez", "komutlar": [)") + steps + "]}");
        REQUIRE(ran.ok());
        const std::uint64_t after = r.doc.content_hash();
        const std::string said    = caption_of(r, 1, 1);
        r.run("GERİAL");
        r.run("YİNELE");
        CHECK_EQ(r.doc.content_hash(), after);
        CHECK_EQ(caption_of(r, 1, 1), said);
        // And the step it left is still one step back to where it began.
        r.run("GERİAL");
        CHECK_EQ(caption_of(r, 1, 1), "100");
    };
    one_step(
        R"({"cmd": "core.move", "args": {"nesneler": [1], "baslangic": [0, 0], "bitis": [5000, 0]}},
                {"cmd": "core.move", "args": {"nesneler": [1], "baslangic": [0, 0], "bitis": [5000, 0]}})");
    one_step(
        R"({"cmd": "core.move", "args": {"nesneler": [1], "baslangic": [0, 0], "bitis": [5000, 0]}},
                {"cmd": "core.attribute", "args": {"ad": "ada", "nesne": 1, "deger": "1001"}})");
}

TEST_CASE("TEK İŞLEM: 251. öğesi bozuk JSON betiği hiç çalışmaz; ilk 250'si de işlenmez (F-05)")
{
    // THE REGRESSION: a malformed item closed the batch over the commands
    // before it — committed, one undo step — and the script reported failure.
    // Every item is read before any runs, and the refusal names the line.
    Rig r;
    draw_parcels(r, 500);
    const Mark before = mark(r);
    std::string json  = move_and_revalue(250);
    json.insert(json.size() - 2, R"(,{"args": {"nesneler": [1]}})");
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_text(json);
    REQUIRE_FALSE(ran.ok());
    CHECK(ran.error().message.find("Betik satırı 501: \"cmd\" alanı yok") != std::string::npos);
    CHECK(ran.error().message.find("Betik çalıştırılmadı.") != std::string::npos);
    CHECK_EQ(mark(r), before);
}

TEST_CASE("TEK İŞLEM: durdurulan işlem aracı günlüğe yazılmaz; Durdur'la da (F-05)")
{
    // THE REGRESSION: a stopped tool returned normally, its session ended
    // "completed", and the run was journalled as if it had happened — a replay
    // would have run TAMPON to the end. Stopped through the session (the Durdur
    // road) as well as through the job.
    Rig r;
    draw_parcels(r, 3);
    Session* parked   = nullptr;
    r.bus.on_job_host = [&parked](Session& s) { parked = &s; };
    const Mark before = mark(r);
    auto started      = r.bus.begin_interactive("TAMPON nesneler=1 nesneler=2 nesneler=3 mesafe=2");
    REQUIRE(started.ok());
    Session& session = *started.value();
    REQUIRE(session.working());
    session.cancel(); // Durdur, while the worker has it
    session.job()->work(JobControl{session.job()->stop.get_token()});
    session.resume_job();
    CHECK_EQ(std::string(session_state_name(session.state())), "cancelled");
    auto finished = r.bus.finish(session);
    REQUIRE(finished.ok());
    CHECK_FALSE(finished.value().mutated);
    CHECK_EQ(mark(r), before);
    CHECK(r.said.find("İşlem durduruldu; çizim değişmedi.") != std::string::npos);
}

// ============================================================ the summary ===

TEST_CASE("DEĞİŞİKLİK: her adım ne değiştirdiğini sayar, kendi içinde net (F-05)")
{
    Rig r;
    const auto one = [&r](const std::string& line) {
        auto ran = r.bus.execute_line(line, Origin::Test);
        if (!ran) FAIL_WITH(line, ran.error().message);
        return ran ? ran.value().changes : ChangeSummary{};
    };

    // A LINE DRAWN is one object added, and nothing else.
    ChangeSummary drew = one("ÇİZGİ 0,0 100,0");
    CHECK_EQ(drew.created, std::size_t{1});
    CHECK_EQ(describe_changes(drew), "1 nesne eklendi");

    // A MOVE is a change of place, not an addition.
    ChangeSummary moved = one("TAŞI nesneler=1 baslangic=0,0 bitis=5,0");
    CHECK_EQ(moved.created, std::size_t{0});
    CHECK_EQ(moved.reshaped, std::size_t{1});
    CHECK_EQ(describe_changes(moved), "1 nesnenin yeri ya da biçimi değişti");

    // ANOTHER LAYER.
    ChangeSummary relayered = one("KATMANAT nesneler=1 katman=YOL");
    CHECK_EQ(relayered.relayered, std::size_t{1});

    // A VALUE, and its follower re-worded with it.
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 10,0 10,10 0,10");
    r.run("ÖZNİTELİK ad=ada nesne=2 deger=1");
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\"");
    ChangeSummary valued = one("ÖZNİTELİK ad=ada nesne=2 deger=7");
    CHECK_EQ(valued.revalued, std::size_t{1});
    CHECK_EQ(valued.reworded, std::size_t{1});
    CHECK_EQ(describe_changes(valued), "1 yazının metni ve 1 nesnenin öznitelik değeri değişti");

    // ERASED, and the caption bound to it with it.
    ChangeSummary erased = one("SİL nesneler=2");
    CHECK_EQ(erased.erased, std::size_t{2});

    // BORN AND GONE IN ONE STEP is neither: a script that draws a line and
    // erases it changed nothing an object count can see.
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    std::uint64_t next = 1; // the key the script's line will get: one past every key given
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        next = std::max(next, core::raw(r.doc.entities().key[e]) + 1);
    auto ran = runner.run_text(R"({"ad": "Gel git", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 0], [10, 10]]}},
        {"cmd": "core.erase", "args": {"nesneler": [)" +
                               std::to_string(next) + R"(]}}]})");
    if (!ran) FAIL_WITH("betik", ran.error().message);
    CHECK(ran.value().changes.empty());
}

TEST_CASE("DEĞİŞİKLİK: 500 parsellik betik ne yaptığını tek cümlede söyler; geri alma da (F-05)")
{
    Rig r;
    draw_parcels(r, 500);
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_text(move_and_revalue(500));
    if (!ran) FAIL_WITH("betik", ran.error().message);

    // 500 parcels moved and re-valued; their 500 captions followed the move and
    // re-worded themselves: the followers are counted, because they changed.
    const ChangeSummary& did = ran.value().changes;
    CHECK_EQ(did.reshaped, std::size_t{1000});
    CHECK_EQ(did.reworded, std::size_t{500});
    CHECK_EQ(did.revalued, std::size_t{500});
    CHECK_EQ(did.created, std::size_t{0});
    CHECK_EQ(ran.value().said,
             "Kaydırma: 1000 komut, tek geri alma adımı — 1000 nesnenin yeri ya da biçimi, 500 "
             "yazının metni ve 500 nesnenin öznitelik değeri değişti.");

    // TAKING IT BACK says what taking it back did — the same counts.
    r.said.clear();
    r.run("GERİAL");
    CHECK(r.said.find("Geri alındı: Kaydırma\nGeri almayla 1000 nesnenin yeri ya da biçimi, 500 "
                      "yazının metni ve 500 nesnenin öznitelik değeri değişti.") !=
          std::string::npos);
    r.said.clear();
    r.run("YİNELE");
    CHECK(r.said.find("Yinelendi: Kaydırma\nYinelemeyle 1000 nesnenin yeri ya da biçimi") !=
          std::string::npos);

    // An addition taken back is an erasure, counted as one.
    r.run("ÇİZGİ 0,0 1,1");
    r.said.clear();
    r.run("GERİAL");
    CHECK(r.said.find("Geri almayla 1 nesne silindi.") != std::string::npos);
}

TEST_CASE("BETİK: tamamlanan betik ne yaptığını, yarıda kalan hatasını söyler (F-05)")
{
    // BETİK used to END IN SUCCESS for a script that failed, with the error only
    // as a transcript line: a client that was not reading the transcript was
    // told it had worked.
    Rig r;
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    script::install(r.bus, runner);
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "kentoscad-tek-islem-betik";
    std::filesystem::create_directories(dir);
    const std::filesystem::path good = dir / "iyi.json";
    const std::filesystem::path bad  = dir / "kotu.json";
    std::ofstream(good) << R"({"ad": "İki çizgi", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 0], [10, 0]]}},
        {"cmd": "core.line", "args": {"noktalar": [[0, 5], [10, 5]]}}]})";
    std::ofstream(bad) << R"({"ad": "Yarım", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 0], [10, 0]]}},
        {"cmd": "core.erase", "args": {"nesneler": [999999]}}]})";

    r.run("BETİK \"" + good.string() + "\"");
    CHECK(r.said.find("Betik tamamlandı: " + good.string() +
                      "\nİki çizgi: 2 komut, tek geri alma adımı — 2 nesne eklendi.") !=
          std::string::npos);

    const Mark before = mark(r);
    auto failed       = r.bus.execute_line("BETİK \"" + bad.string() + "\"", Origin::Test);
    REQUIRE_FALSE(failed.ok());
    CHECK(failed.error().message.find("Betik hatası: Betik satırı 2 (core.erase)") !=
          std::string::npos);
    CHECK_EQ(mark(r), before);
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("BETİK: kılavuzun örnek betiği kılavuzdaki cümleyi yazar (F-05)")
{
    // docs/betik/README.md prints this run's sentence; the page is only true while
    // this is.
    Rig r;
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto ran = runner.run_file(std::string(KENTOS_JOURNAL_DIR) + "/ornek-parsel.json");
    if (!ran) FAIL_WITH("örnek", ran.error().message);
    CHECK_EQ(ran.value().said, "Örnek parsel çizimi: 9 komut, tek geri alma adımı — 14 nesne "
                               "eklendi; 4 katmanın ayarları değişti.");
}

TEST_CASE("DEĞİŞİKLİK: kılavuzdaki geri alma ve yineleme çıktıları kelimesi kelimesine (F-05)")
{
    // docs/komutlar/undo.md, redo.md, line.md and baslangic/ilk-adimlar.md print
    // these lines; the pages are only true while this is.
    const std::string line = "İki veya daha fazla nokta arasında doğru parçaları çizer.";
    const auto after       = [](Rig& r, const char* command) {
        r.said.clear();
        r.run(command);
        return r.said;
    };
    {
        Rig r; // undo.md
        r.run("ÇİZGİ 0,0 100,0");
        CHECK_EQ(after(r, "GERİAL"), "Geri alındı: " + line + "\nGeri almayla 1 nesne silindi.\n");
        CHECK_EQ(after(r, "YİNELE"), "Yinelendi: " + line + "\nYinelemeyle 1 nesne eklendi.\n");
    }
    {
        Rig r; // line.md
        r.run("ÇİZGİ 0,0 100,0 100,50 0,50");
        CHECK_EQ(after(r, "GERİAL"), "Geri alındı: " + line + "\nGeri almayla 3 nesne silindi.\n");
    }
    {
        Rig r; // ilk-adimlar.md, steps 3 to 5
        r.run("ÇİZGİ 485320.150,4310220.400 @50,30 @100<45");
        r.run("ÇİZGİ dik(0,0,100,0,30,5) dik(0,0,100,0,30,-5)");
        r.run("KATMAN ad=PARSEL renk=4281236786");
        const core::LayerId parsel = r.doc.find_layer("PARSEL");
        CHECK_EQ(after(r, "GERİAL"),
                 "Geri alındı: Katman oluşturur, aktif yapar ve özelliklerini değiştirir.\n"
                 "Geri almayla 1 katmanın ayarları değişti.\n");
        // The layer stays and stays active; its colour is what went back.
        REQUIRE(r.doc.find_layer("PARSEL") == parsel);
        CHECK_EQ(r.bus.active_layer(), parsel);
        CHECK_EQ(r.doc.layer_table().at(parsel)->appearance.rgba, 0xFF000000u);
        CHECK_EQ(after(r, "GERİAL"), "Geri alındı: " + line + "\nGeri almayla 1 nesne silindi.\n");
    }
}

namespace {

/// The bytes of `path`.
std::string bytes_of(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("TEK İŞLEM: yarıda kalan betik hiçbir türde iz bırakmaz; kaydedilen dosya bayt bayt aynı "
          "(F-05)")
{
    // WHAT A ROLLBACK USED TO LEAVE, one kind at a time: a dead row (in the file
    // and in the fingerprint), a layer the script made — and the active layer
    // switched to it — a column, an interned style, a processing tool's output
    // layer and its origin, a block, a hatch, a linked dimension, a caption's
    // tie, and a key counter further on than any journal could replay. Each
    // kind is appended by a script that then fails; the drawing, the active
    // layer, the next key and the saved FILE must be exactly as before.
    Rig r;
    io::FileService files{r.bus};
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 20000,0 20000,10000 0,10000");
    r.run("ÖZNİTELİK ad=ada nesne=1 deger=101");
    r.run("ÇİZGİ 0,20000 30000,20000");
    const auto dir = std::filesystem::temp_directory_path() / "kentoscad-tek-islem-dosya";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string first = (dir / "once.pcad").string();
    r.run("FARKLIKAYDET \"" + first + "\"");
    const std::string saved     = bytes_of(first);
    const Mark before           = mark(r);
    const core::LayerId active  = r.bus.active_layer();
    const std::uint64_t key_was = r.doc.keys().peek_entity();

    const std::vector<std::pair<const char*, std::string>> kinds = {
        {"çizgi", R"({"cmd": "core.line", "args": {"noktalar": [[0, 5000], [10000, 5000]]}})"},
        {"alan ve değer",
         R"({"cmd": "core.area", "args": {"noktalar": [[40000, 0], [50000, 0], [50000, 9000]]}},
                            {"cmd": "core.attribute", "args": {"ad": "ada", "nesne": 3, "deger": "7"}})"},
        {"katman", R"({"cmd": "core.layer", "args": {"ad": "YENİ", "renk": 4281236786}},
                     {"cmd": "core.line", "args": {"noktalar": [[0, 0], [1000, 1000]]}})"},
        {"sütun", R"({"cmd": "core.column", "args": {"kimlik": "no", "tur": "metin"}},
                    {"cmd": "core.attribute", "args": {"ad": "no", "nesne": 1, "deger": "A-1"}})"},
        {"stil", R"({"cmd": "core.colour", "args": {"nesneler": [2], "renk": "#FF0000"}})"},
        {"işlem aracı",
         R"({"cmd": "islem.tampon", "args": {"nesneler": [2], "mesafe": 2, "katman": "BANT"}})"},
        {"blok",
         R"({"cmd": "core.block", "args": {"ad": "KAPAK", "taban": [0, 20000], "nesneler": [2]}})"},
        {"tarama",
         R"({"cmd": "core.hatch", "args": {"noktalar": [[0, 30000], [9000, 30000], [9000, 39000], [0, 39000]], "desen": "ANSI37", "olcek": 1000}})"},
        {"ölçü",
         R"({"cmd": "core.dimension", "args": {"birinci": [0, 0], "ikinci": [20000, 0], "konum": [0, -3000]}})"},
        {"etiket", R"({"cmd": "core.label", "args": {"katman": "PARSEL", "bicim": "{ada}"}})"},
        {"yazı", R"({"cmd": "core.text", "args": {"noktalar": [[1000, 1000]], "yazi": "DENEME"}})"},
    };
    for (const auto& [name, steps] : kinds) {
        CAPTURE(name);
        script::JsonRunner runner(r.bus, script::Sandbox::Project);
        auto ran = runner.run_text(std::string(R"({"ad": "Yarım", "komutlar": [)") + steps +
                                   R"(, {"cmd": "core.erase", "args": {"nesneler": [999999]}}]})");
        REQUIRE_FALSE(ran.ok());
        // Failed where it was meant to: at the last line, after every append.
        CHECK_MESSAGE(ran.error().message.find("999999") != std::string::npos, ran.error().message);
        CHECK_EQ(mark(r), before);
        CHECK_EQ(r.bus.active_layer(), active);
        CHECK_EQ(r.doc.keys().peek_entity(), key_was);
        const std::string again = (dir / "sonra.pcad").string();
        r.run("FARKLIKAYDET \"" + again + "\"");
        CHECK(bytes_of(again) == saved);
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("TEK İŞLEM: yarıda kalan betikten sonra günlük aynı anahtarlarla oynatılır (F-05, R4a)")
{
    // THE KEYS A ROLLED-BACK STEP MINTED WERE NEVER ISSUED (model.md R4a), and
    // this is why it matters: the journal holds no line of the failed script,
    // so a replay mints no key for it. Had the live session burned them, its
    // next line would have a key the replay gives to nothing — and the erase
    // that named it would reach a different object, or none.
    Rig live;
    live.run("ÇİZGİ 0,0 10000,0");
    script::JsonRunner runner(live.bus, script::Sandbox::Project);
    auto failed = runner.run_text(R"({"ad": "Yarım", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 1000], [10000, 1000], [10000, 2000]]}},
        {"cmd": "core.circle_draw", "args": {"merkez": [5000, 5000], "cevre": [6000, 5000]}},
        {"cmd": "core.erase", "args": {"nesneler": [999999]}}]})");
    REQUIRE_FALSE(failed.ok());
    live.run("ÇİZGİ 0,3000 10000,3000");
    CHECK(live.doc.slot_of(static_cast<core::EntityKey>(2)) != core::kNoEntity);
    live.run("ÇİZGİ 0,4000 10000,4000");
    live.run("SİL nesneler=2");

    Rig replay;
    for (const auto& e : live.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), live.doc.content_hash());
    CHECK_EQ(replay.doc.live_entity_count(), live.doc.live_entity_count());
    CHECK_EQ(replay.doc.keys().peek_entity(), live.doc.keys().peek_entity());
}

TEST_CASE("SÜTUN: bir betiğin içinde silinmez ve tanımı değiştirilmez (F-05)")
{
    // A drop takes its cells with it and keeps no record; a scale change
    // rescales every cell. Neither could be given back if the script failed
    // after it, and a script that fails leaves nothing behind — so inside one
    // both are refused, with the way out.
    Rig r;
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 10,0 10,10");
    r.run("ÖZNİTELİK ad=ada nesne=1 deger=7");
    script::JsonRunner runner(r.bus, script::Sandbox::Project);
    auto dropped =
        runner.run_text(R"([{"cmd": "core.column", "args": {"kimlik": "ada", "sil": true}}])");
    REQUIRE_FALSE(dropped.ok());
    CHECK(dropped.error().message.find(
              "'ada' sütunu bir betiğin ya da toplu işin içinde silinmez") != std::string::npos);
    CHECK(dropped.error().message.find("SÜTUN komutunu betikten önce ayrıca çalıştırın") !=
          std::string::npos);
    CHECK(r.doc.attributes().find("ada") != core::kNoAttr);
    auto amended =
        runner.run_text(R"([{"cmd": "core.column", "args": {"kimlik": "ada", "ad": "Ada no"}}])");
    REQUIRE_FALSE(amended.ok());
    CHECK(amended.error().message.find("değiştirilmez") != std::string::npos);

    // Outside a script both still work.
    r.run("SÜTUN kimlik=ada ad=\"Ada no\"");
    r.run("SÜTUN kimlik=ada sil=evet");
    CHECK(r.doc.attributes().find("ada") == core::kNoAttr);
}

TEST_CASE("KUYRUK: yerini başka bir çizime bırakan belgeye eski kuyruk kesilmez (F-05)")
{
    // AÇ and YENİ replace a drawing by move-assigning into the bus's document; a
    // tail taken before must not cut the drawing that arrived. The move carries
    // the content's generation forward.
    core::Document doc;
    const core::Document::Tail before = doc.tail();
    core::Op undo;
    REQUIRE(doc.add_polyline(0, std::vector<core::Point2>{{0, 0}, {1000, 0}}, undo).ok());
    doc = core::Document{};
    CHECK_NE(doc.generation(), before.generation);
    CHECK_FALSE(doc.truncate_to(before).ok());

    // A row still alive past the tail is refused too, and nothing is cut.
    core::Document live;
    const core::Document::Tail empty = live.tail();
    REQUIRE(live.add_polyline(0, std::vector<core::Point2>{{0, 0}, {1000, 0}}, undo).ok());
    CHECK_FALSE(live.truncate_to(empty).ok());
    CHECK_EQ(live.live_entity_count(), std::size_t{1});
}
