// SPDX-License-Identifier: GPL-3.0-or-later
// Cadastral operations: tevhit, ifraz and the topology check.
//
// What these produce is a legal document, so the assertions here are about the
// two things a surveyor checks first: does the AREA balance, and does the command
// refuse rather than guess when the answer is not arithmetic.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/measure_mark.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/document.hpp"
#include "piricad/domain/cadastre/commands.hpp"
#include "piricad/domain/cadastre/topology.hpp"

#include <string>
#include <vector>

using namespace piricad;
using command::Origin;

namespace {

struct Rig
{
    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    std::string said;
    std::vector<command::MeasureMark> marks;

    Rig()
    {
        command::register_builtin_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        bus.on_echo         = [this](std::string_view t) { said += std::string(t); };
        bus.on_measure_mark = [this](const command::MeasureMark& mark) { marks.push_back(mark); };
    }
};

core::Mm2 abs_area(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

/// The area of one live entity, through the kind registry the document uses.
core::Mm2 area_of(const core::Document& doc, core::EntityId e)
{
    return doc.geometry().area_of(doc.entities().slot[e]);
}

core::Mm2 total_area(const core::Document& doc)
{
    core::Mm2 sum = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.alive(e)) sum += abs_area(area_of(doc, e));
    return sum;
}

void draw_coverage_frame(Rig& r, bool island = false)
{
    for (const auto* line :
         {"ALAN noktalar=0,0 30,0 30,10 0,10", "ALAN noktalar=0,10 10,10 10,20 0,20",
          "ALAN noktalar=20,10 30,10 30,20 20,20", "ALAN noktalar=0,20 30,20 30,30 0,30"})
        REQUIRE(r.bus.execute_line(line, Origin::Test).ok());
    if (island)
        REQUIRE(r.bus.execute_line("ALAN noktalar=12,12 18,12 18,18 12,18", Origin::Test).ok());
}

} // namespace

// =============================================================================
// TEVHİT
// =============================================================================

TEST_CASE("TEVHİT: komşu iki parsel tek parsel olur, alan korunur")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=10,0 20,0 20,10 10,10", Origin::Test).ok());

    const core::Mm2 before = total_area(r.doc);

    auto merged = r.bus.execute_line("TEVHİT nesneler=1 2", Origin::Test);
    if (!merged) FAIL_WITH("TEVHİT", merged.error().message);

    CHECK(r.doc.live_entity_count() == 1);

    // The seam is gone, not drawn twice: the merged parcel must hold exactly what
    // the two held between them.
    CHECK(total_area(r.doc) == before);
}

TEST_CASE("TEVHİT: bitişik olmayan parseller reddedilir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=100,0 110,0 110,10 100,10", Origin::Test).ok());

    r.said += REFUSED(r.bus.execute_line("TEVHİT nesneler=1 2", Origin::Test));

    // Drawing two parcels and calling them one would produce a record TKGM would
    // reject, so the command says so and changes nothing.
    CHECK(r.doc.live_entity_count() == 2);
    CHECK(r.said.find("bitişik değil") != std::string::npos);
}

TEST_CASE("TEVHİT: sütunlar ayrışıyorsa uydurmaz, boş bırakır ve söyler")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=10,0 20,0 20,10 10,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=malik tur=metin", Origin::Test).ok());

    // ada agrees on both, malik does not.
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=ada_no nesne=1 deger=1284", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=ada_no nesne=2 deger=1284", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=malik nesne=1 deger=AHMET", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=malik nesne=2 deger=MEHMET", Origin::Test).ok());

    r.said.clear();
    REQUIRE(r.bus.execute_line("TEVHİT nesneler=1 2", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() == 1);

    core::EntityId merged = core::kNoEntity;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        if (r.doc.alive(e)) merged = e;
    REQUIRE(merged != core::kNoEntity);

    const core::AttrId ada   = r.doc.attributes().find("ada_no");
    const core::AttrId malik = r.doc.attributes().find("malik");

    // What every input agreed on survives...
    auto kept = r.doc.attribute(ada, merged);
    REQUIRE(kept.ok());
    CHECK(kept.value().present);
    CHECK(kept.value().text == "1284");

    // ...and what they disagreed about comes back EMPTY, with the column named.
    // Picking the first parcel's owner would be inventing a land record.
    auto dropped = r.doc.attribute(malik, merged);
    REQUIRE(dropped.ok());
    CHECK(!dropped.value().present);
    CHECK(r.said.find("malik") != std::string::npos);
}

TEST_CASE("TEVHİT tek geri alma adımıdır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=10,0 20,0 20,10 10,10", Origin::Test).ok());

    const std::uint64_t before = r.doc.content_hash();
    REQUIRE(r.bus.execute_line("TEVHİT nesneler=1 2", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());

    CHECK(r.doc.content_hash() == before);
}

// =============================================================================
// İFRAZ
// =============================================================================

TEST_CASE("İFRAZ: parseli ikiye böler ve toplam alan korunur")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    const core::Mm2 before = total_area(r.doc);

    // A vertical cut down the middle.
    auto cut = r.bus.execute_line("İFRAZ nesneler=1 noktalar=10,-5 10,15", Origin::Test);
    if (!cut) FAIL_WITH("İFRAZ", cut.error().message);

    CHECK(r.doc.live_entity_count() == 2);

    // AN IFRAZ THAT LOSES AREA HAS CUT SOMETHING IT SHOULD NOT HAVE. A few square
    // centimetres missing is a few square centimetres somebody owns.
    CHECK(total_area(r.doc) == before);
}

TEST_CASE("İFRAZ: öznitelikler iki parçaya da geçer")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=ada_no nesne=1 deger=1284", Origin::Test).ok());

    REQUIRE(r.bus.execute_line("İFRAZ nesneler=1 noktalar=10,-5 10,15", Origin::Test).ok());

    const core::AttrId ada = r.doc.attributes().find("ada_no");
    std::size_t carried    = 0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        auto had = r.doc.attribute(ada, e);
        if (had && had.value().present && had.value().text == "1284") ++carried;
    }
    CHECK(carried == 2);
}

TEST_CASE("İFRAZ: parseli kesmeyen çizgi reddedilir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    r.said +=
        REFUSED(r.bus.execute_line("İFRAZ nesneler=1 noktalar=100,100 200,200", Origin::Test));

    CHECK(r.doc.live_entity_count() == 1);
    CHECK(r.said.find("kesmiyor") != std::string::npos);
}

// =============================================================================
// TOPOLOJİ
// =============================================================================

TEST_CASE("TOPOLOJİ: temiz çizimde kusur bulmaz")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=10,0 20,0 20,10 10,10", Origin::Test).ok());

    r.said.clear();
    REQUIRE(r.bus.execute_line("TOPOLOJİ", Origin::Test).ok());

    // Two parcels sharing an edge is not an overlap; reporting it would bury the
    // real ones under a report nobody reads.
    CHECK(r.said.find("kusur bulunamadı") != std::string::npos);
}

TEST_CASE("TOPOLOJİ: örtüşen iki parseli alanıyla birlikte bildirir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=5,5 15,5 15,15 5,15", Origin::Test).ok());

    r.said.clear();
    REQUIRE(r.bus.execute_line("TOPOLOJİ", Origin::Test).ok());

    CHECK(r.said.find("örtüşüyor") != std::string::npos);
    CHECK(r.said.find("25,00 m²") != std::string::npos); // 5 m x 5 m
}

TEST_CASE("TOPOLOJİ hiçbir şeyi düzeltmez")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=5,5 15,5 15,15 5,15", Origin::Test).ok());

    const std::uint64_t before = r.doc.content_hash();
    const std::size_t depth    = r.undo.undo_depth();
    REQUIRE(r.bus.execute_line("TOPOLOJİ", Origin::Test).ok());

    // A boundary is measured data; only the surveyor decides what a defect means.
    // The drawing must be untouched AND the check must not become a step the user
    // has to press Ctrl+Z through.
    CHECK(r.doc.content_hash() == before);
    CHECK(r.undo.undo_depth() == depth);
}

TEST_CASE("TOPOLOJİ KAPSAMA G-05: kapalı boşluk ada ile işaretlenir, belge değişmez")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    draw_coverage_frame(r, true);
    const auto hash    = r.doc.content_hash();
    const auto depth   = r.undo.undo_depth();
    const auto journal = r.journal.canonical();
    auto checked       = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    CHECK_EQ(checked.value().report.find("kapsama_kurali")->as_string(), "yok");
    checked = r.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    const auto& finding = checked.value().report.find("kusurlar")->as_array()[0];
    CHECK_EQ(finding.find("tur")->as_string(), "kapsama_boslugu");
    CHECK_EQ(finding.find("alan_mm2")->as_int(), 64'000'000);
    CHECK(finding.find("nesne") == nullptr); // uncovered ground has no owning parcel
    CHECK_EQ(finding.find("katman")->as_int(),
             static_cast<std::int64_t>(core::raw(r.doc.layers().back().key)));
    CHECK_EQ(finding.find("sinir")->as_array().size(), 4u);
    CHECK_EQ(finding.find("adalar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kapsama_kurali")->as_string(),
             "katman_icinde_kapali_bosluk");
    REQUIRE_EQ(r.marks.size(), 1u);
    CHECK_EQ(r.marks[0].shape, command::MeasureMark::Shape::Ring);
    CHECK_EQ(r.marks[0].holes.size(), 1u);
    CHECK_EQ(r.marks[0].labels[0], "kapsama boşluğu 64,00 m²");
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), depth);
    CHECK_EQ(r.journal.canonical(), journal);
}

TEST_CASE("TOPOLOJİ KAPSAMA G-05: farklı katmanlar ve seçim dışı sınırlar boşluğu kapatmaz")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    draw_coverage_frame(r);
    auto checked = r.bus.execute_line("TOPOLOJİ kapsama=evet nesneler=1 2 3", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    CHECK_EQ(checked.value().report.find("bakilan")->as_int(), 3);
    REQUIRE(r.bus.execute_line("KATMAN ad=BINA", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=10,10 20,10 20,20 10,20", Origin::Test).ok());
    checked = r.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kusurlar")->as_array()[0].find("alan_mm2")->as_int(),
             100'000'000);

    Rig split;
    REQUIRE(split.bus.execute_line("ALAN noktalar=0,0 30,0 30,10 0,10", Origin::Test).ok());
    REQUIRE(split.bus.execute_line("ALAN noktalar=0,10 10,10 10,20 0,20", Origin::Test).ok());
    REQUIRE(split.bus.execute_line("ALAN noktalar=20,10 30,10 30,20 20,20", Origin::Test).ok());
    REQUIRE(split.bus.execute_line("KATMAN ad=DIGER", Origin::Test).ok());
    REQUIRE(split.bus.execute_line("ALAN noktalar=0,20 30,20 30,30 0,30", Origin::Test).ok());
    checked = split.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
}

TEST_CASE("TOPOLOJİ KAPSAMA G-05: küçük boşluğu alan eşiği gizlemez ve yanlış seçenek reddedilir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 30,0 30,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,10 10,10 10,110 0,110", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("ALAN noktalar=10.001,10 30,10 30,110 10.001,110", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,110 30,110 30,120 0,120", Origin::Test).ok());
    auto checked = r.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    REQUIRE(r.bus.execute_line("AYAR dugum_toleransi 0", Origin::Test).ok());
    checked = r.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kusurlar")->as_array()[0].find("alan_mm2")->as_int(),
             100'000);
    const auto hash    = r.doc.content_hash();
    const auto journal = r.journal.canonical();
    CHECK_FALSE(r.bus.execute_line("TOPOLOJİ kapsama=belki", Origin::Test).ok());
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.journal.canonical(), journal);
}

TEST_CASE("TOPOLOJİ KAPSAMA G-05 PROOF: GUI, komut satırı ve JSON aynı teşhisi verir")
{
    Rig gui, cli, json;
    for (auto* rig : {&gui, &cli, &json})
        draw_coverage_frame(*rig, true);
    command::Args gui_args;
    gui_args.set("kapsama", command::Value::boolean(true));
    auto mouse = gui.bus.dispatch({"core.topology", gui_args, Origin::Gui});
    REQUIRE(mouse.ok());
    auto line = cli.bus.execute_line("TOPOLOJİ kapsama=evet", Origin::CommandLine);
    REQUIRE(line.ok());
    core::Json args;
    args.set("kapsama", core::Json::boolean(true));
    const auto decoded = command::Args::from_json(args);
    REQUIRE(decoded.ok());
    auto script = json.bus.dispatch({"core.topology", decoded.value(), Origin::Script});
    REQUIRE(script.ok());
    CHECK_EQ(mouse.value().report.dump(), line.value().report.dump());
    CHECK_EQ(mouse.value().report.dump(), script.value().report.dump());
    CHECK_EQ(gui.said, cli.said);
    CHECK_EQ(gui.said, json.said);
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(gui.doc.content_hash(), json.doc.content_hash());
    CHECK_EQ(gui.journal.canonical(), cli.journal.canonical());
    CHECK_EQ(gui.journal.canonical(), json.journal.canonical());
    REQUIRE_EQ(gui.marks.size(), 1u);
    REQUIRE_EQ(json.marks.size(), 1u);
    CHECK_EQ(gui.marks[0].points, json.marks[0].points);
    CHECK_EQ(gui.marks[0].holes, json.marks[0].holes);
}

TEST_CASE("TOPOLOJİ G-05: uzun ortak kenarda tolerans alan değil genişliktir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,100 0,100", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=9.999,0 20,0 20,100 9.999,100", Origin::Test).ok());
    const auto hash  = r.doc.content_hash();
    const auto depth = r.undo.undo_depth();
    auto checked     = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), depth);

    // Zero tolerance means exact overlap, even below the minimum-area setting:
    // that setting identifies small faces; it must never hide an overlap.
    REQUIRE(r.bus.execute_line("AYAR dugum_toleransi 0", Origin::Test).ok());
    checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kusurlar")->as_array()[0].find("alan_mm2")->as_int(),
             100'000);
}

TEST_CASE("TOPOLOJİ G-05: toleransı aşan dar örtüşme bulunur, alanı küçültülmez")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,100 0,100", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=9.975,0 20,0 20,100 9.975,100", Origin::Test).ok());
    auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kusurlar")->as_array()[0].find("alan_mm2")->as_int(),
             2'500'000);
}

TEST_CASE("TOPOLOJİ G-05: delikli örtüşmenin alanından delikler çıkarılır")
{
    Rig r;
    REQUIRE(r.bus
                .execute_line("ALAN noktalar=0,0 10,0 10,10 0,10 1,1 1,9 9,9 9,1 bolum=4 bolum=4",
                              Origin::Test)
                .ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=-1,-1 11,-1 11,11 -1,11", Origin::Test).ok());
    auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    REQUIRE_EQ(checked.value().report.find("kusurlar")->as_array().size(), 1u);
    CHECK_EQ(checked.value().report.find("kusurlar")->as_array()[0].find("alan_mm2")->as_int(),
             36'000'000);
}

TEST_CASE("TOPOLOJİ G-05: en küçük alan ayarı delikler çıkarıldıktan sonra uygulanır")
{
    Rig r;
    REQUIRE(r.bus
                .execute_line("ALAN noktalar=0,0 1,0 1,1 0,1 0.05,0.05 0.05,0.95 0.95,0.95 "
                              "0.95,0.05 bolum=4 bolum=4",
                              Origin::Test)
                .ok());
    const auto hash  = r.doc.content_hash();
    const auto depth = r.undo.undo_depth();
    auto checked     = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    const auto& findings = checked.value().report.find("kusurlar")->as_array();
    REQUIRE_EQ(findings.size(), 1u);
    CHECK_EQ(findings[0].find("tur")->as_string(), "kirpinti");
    CHECK_EQ(findings[0].find("alan_mm2")->as_int(), 190'000);
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), depth);

    REQUIRE(r.bus.execute_line("AYAR en_kucuk_alan 190000", Origin::Test).ok());
    checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    REQUIRE(r.bus.execute_line("AYAR en_kucuk_alan 0", Origin::Test).ok());
    checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
}

TEST_CASE("TOPOLOJİ G-05: dairenin örtüşmesi gerçek yaydan ölçülür")
{
    Rig r;
    REQUIRE(r.bus.execute_line("DAİRE merkez=0,0 cevre=5,0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE_EQ(r.doc.live_entity_count(), 2u);
    const auto hash = r.doc.content_hash();
    auto checked    = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    const auto& findings = checked.value().report.find("kusurlar")->as_array();
    REQUIRE_EQ(findings.size(), 1u);
    CHECK_EQ(findings[0].find("tur")->as_string(), "ortusme");
    // One quarter of pi * (5000 mm)^2, rounded once to the square millimetre.
    CHECK_EQ(findings[0].find("alan_mm2")->as_int(), 19'634'954);
    CHECK_EQ(r.doc.content_hash(), hash);
}

TEST_CASE("TOPOLOJİ G-05: geçersiz delik çizime girmez, denetim sahte bulgu vermez")
{
    Rig r;
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.bus
                    .execute_line(
                        "ALAN noktalar=0,0 10,0 10,10 0,10 20,20 20,21 21,21 21,20 bolum=4 bolum=4",
                        Origin::Test)
                    .ok());
    CHECK_EQ(r.doc.content_hash(), hash);
    auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
}

TEST_CASE("TOPOLOJİ G-05: elipsin örtüşmesi gerçek eğriden ölçülür")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ELİPS merkez=50,50 birinci=60,50 ikinci=50,55", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=50,50 70,50 70,70 50,70", Origin::Test).ok());
    auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    const auto& findings = checked.value().report.find("kusurlar")->as_array();
    REQUIRE_EQ(findings.size(), 1u);
    CHECK_EQ(findings[0].find("tur")->as_string(), "ortusme");
    CHECK_EQ(findings[0].find("alan_mm2")->as_int(), 39'269'908);
}

TEST_CASE("TOPOLOJİ G-05: büyük koordinatta aynı tolerans ve sonuç")
{
    Rig r;
    REQUIRE(r.bus
                .execute_line(
                    "ALAN noktalar=485000,4310000 485010,4310000 485010,4310100 485000,4310100",
                    Origin::Test)
                .ok());
    REQUIRE(
        r.bus
            .execute_line(
                "ALAN noktalar=485009.999,4310000 485020,4310000 485020,4310100 485009.999,4310100",
                Origin::Test)
            .ok());
    auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    CHECK_EQ(checked.value().report.find("kusur")->as_int(), 0);
    CHECK_EQ(checked.value().report.find("dugum_toleransi_mm")->as_int(), 10);
    CHECK_EQ(checked.value().report.find("en_kucuk_alan_mm2")->as_int(), 500'000);
    CHECK(
        checked.value().report.find("geometri_cekirdegi")->as_string().starts_with("OpenCASCADE"));
}

TEST_CASE("TOPOLOJİ G-05: milimetrelik kırpıntı sıfır alan gibi yazılmaz")
{
    Rig r;
    REQUIRE(r.bus.execute_line("AYAR dugum_toleransi 0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 0.001,0 0.001,0.001 0,0.001", Origin::Test).ok());
    const auto checked = r.bus.execute_line("TOPOLOJİ", Origin::Test);
    REQUIRE(checked.ok());
    const auto& findings = checked.value().report.find("kusurlar")->as_array();
    REQUIRE_EQ(findings.size(), 1u);
    CHECK_EQ(findings[0].find("tur")->as_string(), "kirpinti");
    CHECK_EQ(findings[0].find("alan_mm2")->as_int(), 1);
    CHECK(findings[0].find("aciklama")->as_string().find("1 mm²") != std::string::npos);
    CHECK(r.said.find("0,00 m²") == std::string::npos);
}

// =============================================================================
// ALANİFRAZ — cutting to a target area
// =============================================================================

TEST_CASE("ALANİFRAZ: istenen alanı tolerans içinde ayırır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    // A 20 m x 10 m parcel: 200 m².
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    const core::Mm2 before = total_area(r.doc);

    // Cut parallel to the north-south direction, taking 80 m² off.
    auto cut = r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test);
    if (!cut) FAIL_WITH("ALANİFRAZ", cut.error().message);

    REQUIRE(r.doc.live_entity_count() == 2);

    // The total must be unchanged — an ifraz that loses area has cut something it
    // should not have.
    CHECK(total_area(r.doc) == before);

    // And one of the two pieces must be the 80 m² that was asked for, within the
    // default hundredth of a square metre.
    bool found = false;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        const core::Mm2 a = abs_area(area_of(r.doc, e));
        if (a >= 80000000 - 10000 && a <= 80000000 + 10000) found = true;
    }
    CHECK(found);
}

TEST_CASE("ALANİFRAZ: parselden büyük bir alan istemek reddedilir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    r.said.clear();
    r.said += REFUSED(
        r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=500000000", Origin::Test));

    CHECK(r.doc.live_entity_count() == 1);
    CHECK(r.said.find("küçük olmalı") != std::string::npos);
}

TEST_CASE("ALANİFRAZ tek geri alma adımıdır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    const std::uint64_t before = r.doc.content_hash();
    REQUIRE(
        r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());

    CHECK(r.doc.content_hash() == before);
}

TEST_CASE("ALANİFRAZ: elde edilen alanı raporlar, istenen alanı değil")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    r.said.clear();
    REQUIRE(
        r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test).ok());

    // A command that printed the target instead of what it achieved would be
    // lying about a number that goes on a tapu.
    CHECK(r.said.find("elde edilen") != std::string::npos);
    CHECK(r.said.find("fark") != std::string::npos);
    CHECK(r.said.find("ifrazdan önce") != std::string::npos);
}

TEST_CASE("ALANİFRAZ: öznitelikler iki parçaya da geçer")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=ada_no nesne=1 deger=1284", Origin::Test).ok());

    REQUIRE(
        r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test).ok());

    const core::AttrId ada = r.doc.attributes().find("ada_no");
    std::size_t carried    = 0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        auto had = r.doc.attribute(ada, e);
        if (had && had.value().present && had.value().text == "1284") ++carried;
    }
    CHECK(carried == 2);
}
