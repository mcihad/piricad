// SPDX-License-Identifier: GPL-3.0-or-later
//
// The sample projects (TODOS U-06, data/ornekler): the index loads, every project
// opens through ÖRNEKPROJE as ONE undo step, ends in the sheet its page promises
// at the scale it promises, and every "Deneyin" line it prints runs as printed.
// Opening one is a command like any other, so the three clients are proved to
// leave the same drawing and the same journal (CLAUDE.md 6.4).
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/drawing_catalogs.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/samples.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/layout.hpp"
#include "piricad/domain/cadastre/commands.hpp"
#include "piricad/domain/geodesy/commands.hpp"
#include "piricad/domain/surface/commands.hpp"
#include "piricad/io/service.hpp"
#include "piricad/processing/registry.hpp"
#include "piricad/script/json_runner.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace piricad;
using namespace piricad::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    script::JsonRunner runner{bus, script::Sandbox::Project};
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::geodesy::register_geodesy_commands(reg);
        domain::surface::register_surface_commands(reg);
        script::install(bus, runner);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }
};

/// What a comparison of two clients may look at: command and arguments, not who or when.
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

TEST_CASE("örnek projeler: dizin yüklenir, beş iş vardır, her betik yerinde")
{
    auto catalog = load_samples();
    REQUIRE_MESSAGE(catalog.ok(), (catalog.ok() ? std::string() : catalog.error().message));
    const SampleCatalog& c = catalog.value();
    CHECK_EQ(c.samples.size(), std::size_t{5});
    for (const char* id :
         {"olcuden-harita", "parsel-duzenleme", "plan-cizimi", "gis-analiz", "aplikasyon"}) {
        const Sample* s = c.find(id);
        REQUIRE_MESSAGE(s != nullptr, id);
        CHECK_FALSE(s->title.empty());
        CHECK_FALSE(s->summary.empty());
        CHECK_FALSE(s->steps.empty());
        CHECK_MESSAGE(std::filesystem::exists(s->script), s->script);
    }
    // THE TITLE IS A NAME TOO, with the Turkish folding every command name has: a user who
    // reads "Ölçüden harita" in the menu and types it gets the project.
    CHECK(c.find("ÖLÇÜDEN HARİTA") == c.find("olcuden-harita"));
    CHECK(c.find("yok-boyle-bir-sey") == nullptr);
}

TEST_CASE("ÖRNEKPROJE: her proje tek geri alma adımıdır ve vaat ettiği paftayı vaat ettiği ölçekte "
          "bırakır")
{
    auto catalog = load_samples();
    REQUIRE(catalog.ok());
    for (const Sample& s : catalog.value().samples) {
        Rig r;
        // WORK THE PROJECT MUST SWEEP AWAY: a sample replaces the drawing, it is not poured into
        // it.
        REQUIRE(r.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
        auto opened = r.bus.execute_line("ÖRNEKPROJE ad=" + s.id, Origin::Test);
        REQUIRE_MESSAGE(opened.ok(),
                        s.id << ": " << (opened.ok() ? std::string() : opened.error().message));

        CHECK_MESSAGE(r.doc.live_entity_count() >= std::size_t{4}, s.id);
        CHECK_EQ(r.doc.find_layer("0") != core::kNoLayer, true);
        // THE ONE UNDO STEP is the project; undoing it leaves the empty drawing, not the old one.
        CHECK_EQ(r.undo.undo_depth(), std::size_t{1});
        CHECK(r.said.find("Örnek proje açıldı: " + s.title) != std::string::npos);
        CHECK(r.said.find("Deneyin") != std::string::npos);

        if (s.layout.empty()) {
            CHECK_MESSAGE(r.doc.layouts().size() == 0, s.id);
        } else {
            // THE SCALE IS THE PAGE'S PROMISE: "doğru ölçekli çıktı" is a number on the map frame.
            const core::Layout* sheet = r.doc.layouts().find(s.layout);
            REQUIRE_MESSAGE(sheet != nullptr, s.id << ": '" << s.layout << "' yerleşimi yok");
            bool found = false;
            for (const core::LayoutItem& item : sheet->items)
                if (item.kind == core::LayoutItemKind::Map) {
                    CHECK_EQ(item.scale, s.scale);
                    // THE SCALE THE SHEET IS ACTUALLY AT, not the one declared: one function
                    // answers it for the map drawn, the scale bar and `<olcek>`.
                    CHECK_EQ(core::map_scale(item), s.scale);
                    // AND THE FRAME SHOWS THE WHOLE JOB: a sheet that cut the parcel off at 1:500
                    // would be a correctly scaled picture of the wrong ground.
                    const core::Box2 window = core::map_window(item);
                    const core::Box2 whole  = r.doc.extent();
                    const bool shows_all =
                        window.min_x <= whole.min_x && window.min_y <= whole.min_y &&
                        window.max_x >= whole.max_x && window.max_y >= whole.max_y;
                    CHECK_MESSAGE(shows_all,
                                  s.id << ": harita çerçevesi çizimin tamamını göstermiyor");
                    found = true;
                }
            CHECK_MESSAGE(found, s.id << ": yerleşimde harita çerçevesi yok");
            // AND NOTHING THE CHECK WOULD COMPLAIN ABOUT.
            r.said.clear();
            REQUIRE(r.bus
                        .execute_line("ÇIKTIYERLEŞİMİ islem=denetle ad=\"" + s.layout + "\"",
                                      Origin::Test)
                        .ok());
            CHECK_MESSAGE(r.said.find("eksik") == std::string::npos, s.id << ": " << r.said);
        }

        // EVERY LINE IT PRINTS RUNS AS PRINTED, in order, on the drawing it printed it for.
        for (const SampleStep& step : s.steps) {
            if (step.command.rfind("YAZDIR", 0) == 0) continue; // needs the shell's print service
            auto ran = r.bus.execute_line(step.command, Origin::CommandLine);
            CHECK_MESSAGE(ran.ok(), s.id << ": '" << step.command << "' -> "
                                         << (ran.ok() ? std::string() : ran.error().message));
        }
    }
}

TEST_CASE("PROOF: ÖRNEKPROJE arayüzden, komut satırından ve betikten aynı belgeyi ve aynı günlüğü "
          "bırakır")
{
    // client 1: the window — a session it drives, with the name as the argument.
    Rig gui;
    {
        auto started = gui.bus.begin_interactive("ÖRNEKPROJE ad=parsel-duzenleme", Origin::Gui);
        REQUIRE(started.ok());
        CHECK_FALSE(started.value()->waiting());
        CHECK(gui.bus.finish(*started.value()).ok());
    }
    // client 2: the command line.
    Rig cli;
    CHECK(cli.bus.execute_line("ÖRNEKPROJE ad=parsel-duzenleme", Origin::CommandLine).ok());
    // client 3: a JSON script.
    Rig scr;
    {
        auto ran = scr.runner.run_text(R"({"ad": "Örnek", "komutlar": [
            {"cmd": "core.sample", "args": {"ad": "parsel-duzenleme"}}]})");
        REQUIRE_MESSAGE(ran.ok(), (ran.ok() ? std::string() : ran.error().message));
    }

    CHECK(gui.doc.live_entity_count() >= std::size_t{4});
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
    CHECK(what_happened(gui.journal).find("core.new") != std::string::npos);

    // THE JOURNAL REPLAYS to the same drawing (CLAUDE.md 6.4): a journal that held only the inner
    // commands would replay them on top of whatever the replay started with.
    Rig replay;
    for (const auto& e : cli.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK_MESSAGE(r.ok(), e.command_id);
    }
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
}

TEST_CASE("ÖRNEKPROJE: tanınmayan ad olanları söyler; vazgeçmek çizime dokunmaz")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    const auto hash  = r.doc.content_hash();
    const auto depth = r.undo.undo_depth();

    // THE ERROR NAMES WHAT WAS TYPED AND WHAT COULD HAVE BEEN (TODOS U-06: "nesneyi ve
    // düzeltilebilir nedeni gösterir").
    auto wrong = r.bus.execute_line("ÖRNEKPROJE ad=parsel", Origin::Test);
    REQUIRE_FALSE(wrong.ok());
    CHECK(wrong.error().message.find("'parsel'") != std::string::npos);
    CHECK(wrong.error().message.find("olcuden-harita") != std::string::npos);
    CHECK(wrong.error().message.find("aplikasyon") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), depth);

    // ESC AT THE QUESTION: the bare command lists the projects and asks which; cancelling leaves
    // the drawing exactly as it was and adds no undo step.
    auto started = r.bus.begin_interactive("ÖRNEKPROJE", Origin::CommandLine);
    REQUIRE(started.ok());
    auto& session = *started.value();
    CHECK(session.waiting());
    CHECK(r.said.find("olcuden-harita — Ölçüden harita") != std::string::npos);
    session.cancel();
    CHECK(r.bus.finish(session).ok());
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), depth);
}

TEST_CASE("ÖRNEKPROJE: GERİAL projeyi geri alır — nesneler ve pafta gider")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÖRNEKPROJE ad=parsel-duzenleme", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() >= std::size_t{4});
    REQUIRE(r.doc.layouts().size() == std::size_t{1});
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());
    // The objects and the sheet go; layers and column definitions stay, because neither can be
    // undone on its own (the KATMAN and SÜTUN commands), and the page says so.
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(r.doc.layouts().size(), std::size_t{0});
}

TEST_CASE("ilk dakikanın hataları: nesneyi ve düzeltilebilir nedeni söyler (U-06)")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÖRNEKPROJE ad=parsel-duzenleme", Origin::Test).ok());
    const auto refused = [&r](const std::string& line) {
        auto ran = r.bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(!ran.ok(), line);
        return ran.error().message;
    };
    const auto has = [](const std::string& text, const char* part) {
        return text.find(part) != std::string::npos;
    };

    // A KİMLİK NOBODY WAS GIVEN is a typo: the message says so and what the last one is.
    std::string said = refused("ALANÖLÇ nesneler=99");
    CHECK(has(said, "Nesne bulunamadı veya silinmiş: 99"));
    CHECK(has(said, "hiç nesne olmadı"));
    CHECK(has(said, "verilen son kimlik 6"));
    CHECK(has(said, "SEÇ ya da NESNEBİLGİ"));

    // A KİMLİK THAT WAS AN IFRAZ'S PARENT is not a typo: the message says what replaced it.
    REQUIRE(
        r.bus
            .execute_line("ALANİFRAZ yon=485330,4310200 485330,4310240 nesneler=2 alan=400000000",
                          Origin::Test)
            .ok());
    said = refused("ALANÖLÇ nesneler=2");
    CHECK(has(said, "Nesne bulunamadı veya silinmiş: 2"));
    CHECK(has(said, "ifraz"));
    CHECK(has(said, "GERİAL"));
    CHECK(has(said, "SEÇ"));
    // THE SAME WORDS FROM A COMMAND THAT SAYS "zaten" — they fail through one place.
    CHECK(has(refused("SİL nesneler=77"), "SEÇ ya da NESNEBİLGİ"));

    // A SHEET THAT IS NOT THERE: the names that are.
    said = refused("ÇIKTIYERLEŞİMİ islem=denetle ad=Yok");
    CHECK(has(said, "'Yok'"));
    CHECK(has(said, "Ada 1-500"));
    said = refused("YAZDIR yerlesim=Yok dosya=x.pdf");
    CHECK(has(said, "'Yok'"));
    CHECK(has(said, "Ada 1-500"));

    // A POINT TYPED WRONG: what was typed, and one that works.
    said = refused("ÇİZGİ a,b c,d");
    CHECK(has(said, "X koordinatı"));
    CHECK(has(said, "yazılan 'a'"));
    CHECK(has(said, "485320.150,4310220.400"));
    said = refused("ÇİZGİ 10");
    CHECK(has(said, "'noktalar'"));
    CHECK(has(said, "Girilen: 10.")); // not "10.000000"
    CHECK(has(said, "doğu ve kuzey"));

    // AN AREA THE PARCEL CANNOT GIVE: both numbers.
    said = refused("ALANİFRAZ yon=485300,4310200 485300,4310240 nesneler=1 alan=9000000000");
    CHECK(has(said, "9000,00 m²"));
    CHECK(has(said, "1200,00 m²"));
}

TEST_CASE(
    "iç içe betik dıştakinin geri alma adımına katılır; başarısızlık hepsini geri alır (U-06)")
{
    const auto dir = std::filesystem::temp_directory_path() / "piricad-ic-ice-betik";
    std::filesystem::create_directories(dir);
    const auto inner = (dir / "ic.json").string();
    std::ofstream(inner) << R"({"ad": "İç", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 0], [10000, 0]]}},
        {"cmd": "core.line", "args": {"noktalar": [[0, 5000], [10000, 5000]]}}]})";

    Rig ok;
    const std::string outer = R"({"ad": "Dış", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 9000], [10000, 9000]]}},
        {"cmd": "core.script", "args": {"dosya": ")" +
                              inner + R"("}}]})";
    auto ran = ok.runner.run_text(outer);
    REQUIRE_MESSAGE(ran.ok(), (ran.ok() ? std::string() : ran.error().message));
    CHECK_EQ(ok.doc.live_entity_count(), std::size_t{3});
    CHECK_EQ(ok.undo.undo_depth(), std::size_t{1});
    REQUIRE(ok.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(ok.doc.live_entity_count(), std::size_t{0});

    // AN INNER FAILURE ROLLS THE OUTER SCRIPT BACK WHOLE.
    const auto broken = (dir / "kirik.json").string();
    std::ofstream(broken) << R"({"komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 0], [10000, 0]]}},
        {"cmd": "core.yokboyle", "args": {}}]})";
    Rig bad;
    auto failed = bad.runner.run_text(R"({"ad": "Dış", "komutlar": [
        {"cmd": "core.line", "args": {"noktalar": [[0, 9000], [10000, 9000]]}},
        {"cmd": "core.script", "args": {"dosya": ")" +
                                      broken + R"("}}]})");
    CHECK_FALSE(failed.ok());
    // THE ROLLBACK IS SAID ONCE, by the script that owns the batch, not by every one inside it.
    const std::string rolled = "Betik bütünüyle geri alındı";
    CHECK(failed.error().message.find(rolled) != std::string::npos);
    CHECK_EQ(failed.error().message.find(rolled), failed.error().message.rfind(rolled));
    CHECK_EQ(bad.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(bad.doc.keys().peek_entity(), std::uint64_t{1}); // the keys it minted are given back
    CHECK_EQ(bad.undo.undo_depth(), std::size_t{0});
    std::filesystem::remove_all(dir);
}
