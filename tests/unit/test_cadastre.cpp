// SPDX-License-Identifier: GPL-3.0-or-later
// Cadastral operations: tevhit, ifraz and the topology check.
//
// What these produce is a legal document, so the assertions here are about the
// two things a surveyor checks first: does the AREA balance, and does the command
// refuse rather than guess when the answer is not arithmetic.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/cadastre/topology.hpp"

#include <string>
#include <vector>

using namespace kentos;
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

    Rig()
    {
        command::register_builtin_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        bus.on_echo = [this](std::string_view t) { said += std::string(t); };
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

    REQUIRE(r.bus.execute_line("TEVHİT nesneler=1 2", Origin::Test).ok());

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

    REQUIRE(r.bus.execute_line("İFRAZ nesneler=1 noktalar=100,100 200,200", Origin::Test).ok());

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
    auto cut = r.bus.execute_line(
        "ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test);
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
    REQUIRE(r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=500000000", Origin::Test)
                .ok());

    CHECK(r.doc.live_entity_count() == 1);
    CHECK(r.said.find("küçük olmalı") != std::string::npos);
}

TEST_CASE("ALANİFRAZ tek geri alma adımıdır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    const std::uint64_t before = r.doc.content_hash();
    REQUIRE(r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test)
                .ok());
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());

    CHECK(r.doc.content_hash() == before);
}

TEST_CASE("ALANİFRAZ: elde edilen alanı raporlar, istenen alanı değil")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    r.said.clear();
    REQUIRE(r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test)
                .ok());

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

    REQUIRE(r.bus.execute_line("ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000", Origin::Test)
                .ok());

    const core::AttrId ada = r.doc.attributes().find("ada_no");
    std::size_t carried    = 0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        auto had = r.doc.attribute(ada, e);
        if (had && had.value().present && had.value().text == "1284") ++carried;
    }
    CHECK(carried == 2);
}
