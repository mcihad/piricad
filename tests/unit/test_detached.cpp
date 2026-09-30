// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — tests: KAPSAMDENETİM, the objects cut off from the drawing's
// majority (`core::find_detached`, netcad_plan.md N-01).
//
// The acceptance is the plan's own: ten thousand parcels and three stragglers,
// exactly three reported and none of them touched. The other half is the rule's
// promise — a drawing that thins out gradually has no straggler — because a
// check that cries wolf on the outskirts of every town plan is switched off.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/detached.hpp"
#include "piricad/script/json_runner.hpp"

#include <string>
#include <vector>

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
    std::string said;
    std::vector<MeasureMark> marks;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo         = [this](std::string_view t) { said += std::string(t) + "\n"; };
        bus.on_measure_mark = [this](const MeasureMark& m) { marks.push_back(m); };
    }

    void run(const std::string& line)
    {
        auto done             = bus.execute_line(line, Origin::Test);
        const std::string why = done.ok() ? std::string() : line + " — " + done.error().message;
        INFO(why);
        REQUIRE(done.ok());
    }
};

/// Ten thousand 20 m parcels in TUREF/TM, a 2 km square, and three stragglers:
/// one fallen near 0,0, one 250 km west as a wrong TM zone puts it, one with its
/// easting and northing swapped.
void town_with_stragglers(Rig& r)
{
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 485300,4310200 485320,4310200 485320,4310220 485300,4310220");
    r.run("DİZİ nesneler=1 satir=100 sutun=100 satir_aralik=20 sutun_aralik=20");
    r.run("KATMAN ad=YANLIŞ");
    r.run("ÇİZGİ 10,10 12,12");
    r.run("NOKTA 235300,4311000");
    r.run("ÇİZGİ 4311000,486300 4311010,486310");
}

} // namespace

TEST_CASE("KAPSAMDENETİM: on bin parselde üç kopuk nesneyi tam bulur, hiçbirini değiştirmez")
{
    Rig r;
    town_with_stragglers(r);
    REQUIRE(r.doc.live_entity_count() == 10003);
    const std::uint64_t before = r.doc.content_hash();
    const std::size_t logged   = r.journal.entries().size();
    const std::size_t steps    = r.undo.undo_depth();

    auto checked = r.bus.execute_line("KAPSAMDENETİM", Origin::CommandLine);
    REQUIRE(checked.ok());
    const core::Json& report = checked.value().report;
    CHECK(report.find("denetlenen")->as_int() == 10003);
    const auto& stragglers = report.find("kopuk")->as_array();
    REQUIRE(stragglers.size() == 3);

    // Nearest first: the wrong zone (250 km west), the one fallen to 0,0
    // (4 340 km from a TUREF town), and the swapped axes (5 400 km) last.
    CHECK(stragglers[0].find("uzaklik_mm")->as_int() > 240'000'000);
    CHECK(stragglers[0].find("uzaklik_mm")->as_int() < 260'000'000);
    CHECK_FALSE(stragglers[0].find("sifira_yakin")->as_bool());
    CHECK(stragglers[1].find("sifira_yakin")->as_bool());
    CHECK_FALSE(stragglers[2].find("sifira_yakin")->as_bool());
    CHECK(stragglers[1].find("uzaklik_mm")->as_int() < stragglers[2].find("uzaklik_mm")->as_int());
    for (const core::Json& row : stragglers)
        CHECK(row.find("katman")->as_string() == "YANLIŞ");

    // MARKED WHERE THEY ARE, and the next step named and not taken.
    CHECK(r.marks.size() == 3);
    CHECK(r.marks.front().shape == MeasureMark::Shape::Point);
    CHECK(r.said.find("3 nesne çizimin çoğunluğundan kopuk") != std::string::npos);
    CHECK(r.said.find("sıfıra yakın") != std::string::npos);
    CHECK(r.said.find("Seçmek için: SEÇ nesneler=") != std::string::npos);
    CHECK(r.said.find("katman=HATALI") != std::string::npos);

    // NOTHING MOVED: not the drawing, not the undo stack, not the journal.
    CHECK(r.doc.content_hash() == before);
    CHECK(r.undo.undo_depth() == steps);
    CHECK(r.journal.entries().size() == logged);

    // And the line it offers does what it says: it selects exactly those three.
    // The shell's button runs the same line the transcript printed.
    const std::string lead   = "Seçmek için: "; ///< counted in bytes: `ç` is two
    const std::size_t at     = r.said.find(lead) + lead.size();
    const std::string select = r.said.substr(at, r.said.find('\n', at) - at);
    REQUIRE(checked.value().offer.has_value());
    CHECK(checked.value().offer->line == select);
    CHECK(checked.value().offer->label == "Seç");
    r.run(select);
    CHECK(r.bus.selection().keys().size() == 3);
}

TEST_CASE("KAPSAMDENETİM: yavaş yavaş seyrekleşen çizimde kopuk nesne yoktur")
{
    // A dense centre and outskirts that thin out — fifty points within 100 m,
    // then one at 200 m, 400 m, … 12,8 km, each gap the size of what is inside
    // it. No empty band is eight times the majority's reach, so nothing is cut
    // off; a distance threshold would have flagged the outskirts.
    Rig r;
    for (int i = 0; i < 50; ++i)
        r.run("NOKTA " + std::to_string(485300 + i * 2) + "," + std::to_string(4310200 + i));
    for (int reach = 200; reach <= 12800; reach *= 2)
        r.run("NOKTA " + std::to_string(485300 + reach) + ",4310200");

    auto checked = r.bus.execute_line("KAPSAMDENETİM", Origin::CommandLine);
    REQUIRE(checked.ok());
    CHECK(checked.value().report.find("kopuk")->as_array().empty());
    CHECK(r.said.find("hiçbiri çizimin çoğunluğundan kopuk değil") != std::string::npos);
    CHECK(r.marks.empty());
}

TEST_CASE("KAPSAMDENETİM: üçten az nesnede çoğunluk yoktur; eşik proje ayarıdır")
{
    Rig few;
    few.run("NOKTA 0,0");
    few.run("NOKTA 500000,4000000");
    auto two = few.bus.execute_line("KAPSAMDENETİM", Origin::CommandLine);
    REQUIRE(two.ok());
    CHECK(two.value().report.find("kopuk")->as_array().empty());
    CHECK(few.said.find("en az üç nesne gerekir; çizimde 2 nesne var") != std::string::npos);

    // THE THRESHOLD IS THE PROJECT'S, and it moves the answer the way the rule
    // says. At 100 the band must be 100 times the town's reach of ~1,4 km —
    // 141 km — and the nearest straggler is 250 km out: all three are cut off.
    // At 200 the band is 283 km, the 250 km straggler is inside it and joins
    // the majority, and nothing is 200 times ITS reach further out: none.
    const auto detached_at = [](int factor) {
        Rig r;
        town_with_stragglers(r);
        r.run("AYAR kopukluk_carpani " + std::to_string(factor));
        auto checked = r.bus.execute_line("KAPSAMDENETİM", Origin::CommandLine);
        REQUIRE(checked.ok());
        CHECK(checked.value().report.find("carpan")->as_int() == factor);
        return checked.value().report.find("kopuk")->as_array().size();
    };
    CHECK(detached_at(100) == 3);
    CHECK(detached_at(200) == 0);
}

TEST_CASE("PROOF: KAPSAMDENETİM gui, komut satırı ve betikten aynı cevabı verir")
{
    // Article 6.4 for a check that changes nothing: the three clients get the
    // same answer, byte for byte, and leave the drawing as they found it.
    Rig gui;
    Rig cli;
    Rig scr;
    for (Rig* rig : {&gui, &cli, &scr})
        town_with_stragglers(*rig);
    const std::uint64_t hash = cli.doc.content_hash();

    const auto from_button = gui.bus.execute_line("KAPSAMDENETİM", Origin::Gui);
    const auto typed       = cli.bus.execute_line("KPD", Origin::CommandLine);
    REQUIRE(from_button.ok());
    REQUIRE(typed.ok());
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto ran = runner.run_text(R"({
            "ad": "Kapsam denetimi kanıtı",
            "komutlar": [ {"cmd": "core.extent_check", "args": {}} ]
        })");
        REQUIRE(ran.ok());
    }
    const auto scripted = scr.bus.dispatch(Invocation{"core.extent_check", Args{}, Origin::Script});
    REQUIRE(scripted.ok());

    CHECK_EQ(from_button.value().report.dump(), typed.value().report.dump());
    CHECK_EQ(typed.value().report.dump(), scripted.value().report.dump());
    for (const Rig* rig : {&gui, &cli, &scr})
        CHECK_EQ(rig->doc.content_hash(), hash);
}
