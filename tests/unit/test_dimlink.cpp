// SPDX-License-Identifier: GPL-3.0-or-later
//
// A DIMENSION FOLLOWS WHAT IT MEASURES (TODOS C-10).
//
// Each case is a way a surveyor's drawing changes under a dimension: a corner
// moved, a parcel erased, the dimension's own point dragged, the two moved
// together, a circle rescaled. The figure is asserted to the millimetre,
// because a dimension that says what a side used to be is a wrong number on a
// signed sheet.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::Point2;

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
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    core::EntityId slot(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    /// The dimension's figure, as stored.
    std::int64_t measured(std::int64_t key) const
    {
        auto def = core::dimension_of(doc.geometry(), doc.entities().slot[slot(key)]);
        REQUIRE(def.ok());
        return def.value().measurement;
    }

    std::string caption(std::int64_t key) const
    {
        return std::string(doc.texts().text(doc.entities().slot[slot(key)]));
    }

    std::vector<core::DimLink> links(std::int64_t key) const
    {
        const auto* got = doc.dimension_links().get(slot(key));
        return got == nullptr ? std::vector<core::DimLink>{} : *got;
    }
};

} // namespace

TEST_CASE("BAĞLI ÖLÇÜ: köşeye çizilen ölçü köşeye bağlanır; köşe taşınınca yeniden ölçülür")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                            // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=20,0 konum=10,-3"); // 2
    CHECK(r.said.find("2 noktası ölçtüğü nesneye bağlı") != std::string::npos);
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{2});
    CHECK_EQ(links[0].anchor, core::DimAnchor::Vertex);
    CHECK_EQ(links[0].index, 0u);
    CHECK_EQ(links[1].index, 1u);
    CHECK_EQ(r.measured(2), 20'000);

    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    CHECK_EQ(r.measured(2), 25'000);
    CHECK_EQ(r.caption(2), std::string("25,00"));
    CHECK(r.said.find("Bağlı 1 ölçü kaynağını izledi ve yeniden ölçüldü.") != std::string::npos);
    // The dimension line kept its distance from the side.
    const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]);
    CHECK_EQ(r.doc.geometry().vertex(span.first + 1, 1), (Point2{25'000, 0}));

    // One undo step takes the corner and the dimension back together.
    r.run("GERİAL");
    CHECK_EQ(r.measured(2), 20'000);
    CHECK_EQ(r.caption(2), std::string("20,00"));
}

TEST_CASE("BAĞLI ÖLÇÜ: ölçtüğü nesne silinince bağ kopar, ölçü yerinde kalır; geri alınca döner")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                     // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.said.clear();
    const std::uint64_t before = r.doc.content_hash();
    r.run("SİL nesneler=1");
    const std::uint64_t after = r.doc.content_hash();
    REQUIRE(r.doc.alive(r.slot(2)));
    const auto broken = r.links(2);
    REQUIRE_EQ(broken.size(), std::size_t{2});
    CHECK(broken[0].broken);
    CHECK(broken[1].broken);
    CHECK_EQ(r.measured(2), 12'000); // where it was, saying what it was
    CHECK(r.said.find("2 ölçü bağı koptu") != std::string::npos);

    r.run("GERİAL");
    const auto back = r.links(2);
    REQUIRE_EQ(back.size(), std::size_t{2});
    CHECK_FALSE(back[0].broken);
    CHECK_EQ(r.doc.content_hash(), before);

    // And forward again: erased, and broken, in the one step it was.
    r.run("YİNELE");
    CHECK_FALSE(r.doc.alive(r.slot(1)));
    CHECK(r.links(2)[0].broken);
    CHECK_EQ(r.doc.content_hash(), after);
}

TEST_CASE("BAĞLI ÖLÇÜ: ölçünün kendi noktası elle taşınınca o bağ çözülür")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                     // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.said.clear();
    r.run("KÖŞETAŞI nesne=2 kose=2 nokta=15,0");
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{1}); // the first point stays tied
    CHECK_EQ(links[0].point, 0);
    CHECK_EQ(r.measured(2), 15'000);
    CHECK(r.said.find("1 ölçü noktası elle taşındığı için bağından çözüldü.") != std::string::npos);

    // The line now moves without it: the free point stays at 15 m.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=20,0");
    CHECK_EQ(r.measured(2), 15'000);
}

TEST_CASE("BAĞLI ÖLÇÜ: ölçü ve kaynağı birlikte taşınınca bağ ve ölçü yerinde kalır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                     // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.run("TAŞI nesneler=1 nesneler=2 baslangic=0,0 bitis=5,5");
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{2});
    CHECK_FALSE(links[0].broken);
    CHECK_EQ(r.measured(2), 12'000);
    // And the pair still follows afterwards.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=23,5");
    CHECK_EQ(r.measured(2), 18'000);
}

TEST_CASE("BAĞLI ÖLÇÜ: yarıçap ölçüsü dairenin merkezine ve çemberine bağlanır, ölçeği izler")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0");                        // 1
    r.run("ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=8,2"); // 2
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{2});
    CHECK_EQ(links[0].anchor, core::DimAnchor::Centre);
    CHECK_EQ(links[1].anchor, core::DimAnchor::OnCircle);
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2");
    CHECK_EQ(r.measured(2), 10'000);
}

TEST_CASE(
    "BAĞLI ÖLÇÜ: yalnız kaynağı taşınan ölçü onunla birlikte kayar, ölçü çizgisi yerini korur")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                            // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=20,0 konum=10,-3"); // 2
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=0,5");
    const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]);
    CHECK_EQ(r.doc.geometry().vertex(span.first + 1, 0), (Point2{0, 5'000}));
    CHECK_EQ(r.doc.geometry().vertex(span.first + 1, 1), (Point2{20'000, 5'000}));
    // Three metres below the side, as it was drawn.
    CHECK_EQ(r.doc.geometry().vertex(span.first + 1, 2).y, 2'000);
    CHECK_EQ(r.measured(2), 20'000);
}

TEST_CASE(
    "BAĞLI ÖLÇÜ: döndürülen parselin hizalı ölçüsü kenarıyla birlikte döner, aynı yakada kalır")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                            // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=20,0 konum=10,-3"); // 2
    r.run("DÖNDÜR nesneler=1 merkez=0,0 aci=90");
    const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]);
    const Point2 p1           = r.doc.geometry().vertex(span.first + 1, 0);
    const Point2 p2           = r.doc.geometry().vertex(span.first + 1, 1);
    const Point2 line         = r.doc.geometry().vertex(span.first + 1, 2);
    CHECK_EQ(p1, (Point2{0, 0}));
    // The side now runs north (under whichever angle rule the session keeps,
    // a quarter turn about its first corner lays it on an axis).
    CHECK_EQ(p2.x * p2.y, 0);
    CHECK_EQ(r.measured(2), 20'000);
    // Three metres out from the side, on the outside of the parcel still.
    const std::int64_t off = p2.x == 0 ? line.x : line.y;
    CHECK_EQ(off < 0 ? -off : off, 3'000);
    // Outside: the far corner of the parcel is on the other side of the side.
    const Point2 far = r.doc.geometry().vertex(
        r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(1)]).first, 2);
    const std::int64_t far_off = p2.x == 0 ? far.x : far.y;
    CHECK((far_off < 0) != (off < 0));
}

TEST_CASE("BAĞLI ÖLÇÜ: yay uzunluğu ölçüsü yayın merkezine ve iki ucuna bağlanır")
{
    Rig r;
    r.run("YAY 0,0 10,0 0,10");                                         // 1
    r.run("ÖLÇÜ tur=yay birinci=0,0 ikinci=10,0 bitis=0,10 konum=9,9"); // 2
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{3});
    CHECK_EQ(links[0].anchor, core::DimAnchor::Centre);
    CHECK_EQ(links[1].anchor, core::DimAnchor::ArcStart);
    CHECK_EQ(links[2].anchor, core::DimAnchor::ArcEnd);
    CHECK_EQ(r.measured(2), 15'708); // a quarter of a 10 m circle, along the arc
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2");
    CHECK_EQ(r.measured(2), 31'416);
}

TEST_CASE("BAĞLI ÖLÇÜ: kilitli katmandaki ölçü izleyemez, bunu söyler ve bağını korur")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0"); // 1, on the drawing's first layer
    r.run("KATMAN ad=OLCU");
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.run("KATMAN ad=OLCU kilitli=evet");
    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=15,0");
    CHECK_EQ(r.measured(2), 12'000);
    CHECK(r.said.find("kilitli katmanda") != std::string::npos);
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{2});
    CHECK_FALSE(links[1].broken);

    // Unlocked, the next change to its line brings it up to date.
    r.run("KATMAN ad=OLCU kilitli=hayır");
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=18,0");
    CHECK_EQ(r.measured(2), 18'000);
}

TEST_CASE("BAĞLI ÖLÇÜ: bağ köşeyi izler, sırasını değil — köşe eklenir, çizgi ters çevrilir")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                             // 1
    r.run("ÖLÇÜ tur=hizali birinci=20,0 ikinci=20,10 konum=23,5"); // 2
    REQUIRE_EQ(r.links(2).size(), std::size_t{2});
    CHECK_EQ(r.links(2)[0].index, 1u);

    // A corner inserted BEFORE the measured ones: they are corners 3 and 4 now,
    // and the dimension did not move.
    r.said.clear();
    r.run("KÖŞEEKLE nesne=1 kose=1 nokta=10,-1");
    CHECK_EQ(r.measured(2), 10'000);
    CHECK_EQ(r.links(2)[0].index, 2u);
    CHECK_EQ(r.links(2)[1].index, 3u);
    CHECK(r.said.find("izledi") == std::string::npos); // nothing moved, nothing to say

    // And it still follows the corner it measured.
    r.run("KÖŞETAŞI nesne=1 kose=4 nokta=20,14");
    CHECK_EQ(r.measured(2), 14'000);

    // The line reversed: every corner renumbered, none moved.
    Rig t;
    t.run("ÇİZGİ 0,0 12,0");                                     // 1
    t.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    t.run("ÇİZGİDÜZENLE nesne=1 islem=ters");
    CHECK_EQ(t.links(2)[0].index, 1u);
    CHECK_EQ(t.links(2)[1].index, 0u);
    CHECK_EQ(t.measured(2), 12'000);
    t.run("KÖŞETAŞI nesne=1 kose=1 nokta=15,0"); // the first corner is 12,0 now
    CHECK_EQ(t.measured(2), 15'000);
}

TEST_CASE("BAĞLI ÖLÇÜ: ölçtüğü köşe silinince bağ kopar; yeri başka köşeye geçmez")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                             // 1
    r.run("ÖLÇÜ tur=hizali birinci=20,0 ikinci=20,10 konum=23,5"); // 2
    r.said.clear();
    r.run("KÖŞESİL nesne=1 kose=2");
    const auto links = r.links(2);
    REQUIRE_EQ(links.size(), std::size_t{2});
    CHECK(links[0].broken);
    CHECK_FALSE(links[1].broken);
    CHECK_EQ(links[1].index, 1u); // 20,10 is the second corner now
    CHECK_EQ(r.measured(2), 10'000);
    CHECK(r.said.find("Ölçtüğü köşe kaldırıldığı için 1 ölçü bağı koptu") != std::string::npos);
}

TEST_CASE("BAĞLI ÖLÇÜ: UÇUCA ve PATLAT bağı yerini alan nesneye aktarır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                    // 1
    r.run("ÇİZGİ 12,0 12,8");                                   // 2
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,8 konum=4,6"); // 3
    REQUIRE_EQ(r.links(3).size(), std::size_t{2});
    r.said.clear();
    r.run("UÇUCA nesne=1 nesne=2");
    const auto joined = r.links(3);
    REQUIRE_EQ(joined.size(), std::size_t{2});
    CHECK_FALSE(joined[0].broken);
    CHECK_FALSE(joined[1].broken);
    CHECK_EQ(joined[0].source, joined[1].source);
    CHECK(r.said.find("1 ölçü bağı, aynı noktada yerini alan nesneye aktarıldı.") !=
          std::string::npos);
    CHECK(r.said.find("koptu") == std::string::npos);

    // Taken apart again: each end goes to the piece that holds it.
    r.run("PATLAT nesne=" + std::to_string(static_cast<std::uint64_t>(joined[0].source)));
    const auto pieces = r.links(3);
    REQUIRE_EQ(pieces.size(), std::size_t{2});
    CHECK_FALSE(pieces[0].broken);
    CHECK_FALSE(pieces[1].broken);
    CHECK_NE(pieces[0].source, pieces[1].source);
    // The dimension still measures the corner it did: moving it re-measures.
    r.run("KÖŞETAŞI nesne=" + std::to_string(static_cast<std::uint64_t>(pieces[1].source)) +
          " kaynak=12,8 nokta=12,16");
    CHECK_EQ(r.measured(3), 20'000);
}

TEST_CASE("BAĞLI ÖLÇÜ: NESNEBİLGİ ölçünün neye bağlı olduğunu, nesnenin kimce ölçüldüğünü söyler")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                     // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.said.clear();
    r.run("NESNEBİLGİ nesneler=2");
    CHECK(r.said.find("bağları: 1. nokta → nesne 1 köşe 1, 2. nokta → nesne 1 köşe 2") !=
          std::string::npos);
    r.said.clear();
    r.run("NESNEBİLGİ nesneler=1");
    CHECK(r.said.find("1 bağlı ölçü bunu ölçüyor") != std::string::npos);

    r.run("SİL nesneler=1");
    r.said.clear();
    r.run("NESNEBİLGİ nesneler=2");
    CHECK(r.said.find("2 bağı kopuk") != std::string::npos);
}

TEST_CASE("BAĞLI ÖLÇÜ: bagla=hayır bağlamaz; boşlukta çizilen ölçü de bağsızdır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2 bagla=hayır");
    CHECK(r.links(2).empty());
    r.run("ÖLÇÜ tur=hizali birinci=30,0 ikinci=40,0 konum=35,-2");
    CHECK(r.links(3).empty());
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=20,0");
    CHECK_EQ(r.measured(2), 12'000);
}

TEST_CASE(
    "BAĞLI ÖLÇÜ KANIT: arayüz, komut satırı, betik ve oynatma aynı bağları ve aynı izlemeyi verir")
{
    Rig gui;
    REQUIRE(gui.bus.execute_line("ÇİZGİ 0,0 12,0", Origin::Gui).ok());
    {
        auto started = gui.bus.begin_interactive("ÖLÇÜ tur=hizali", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(Point2{12'000, 0})).ok());
        CHECK(session.supply(Value::point(Point2{6'000, -2'000})).ok());
        CHECK(gui.bus.finish(session).ok());
    }
    REQUIRE(gui.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=16,0", Origin::Gui).ok());

    Rig cli;
    REQUIRE(cli.bus.execute_line("ÇİZGİ 0,0 12,0", Origin::CommandLine).ok());
    REQUIRE(
        cli.bus
            .execute_line("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2", Origin::CommandLine)
            .ok());
    REQUIRE(cli.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=16,0", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        REQUIRE(runner
                    .run_text(R"({"ad":"Kanıt","komutlar":[
                      {"cmd":"core.line","args":{"noktalar":[[0,0],[12000,0]]}},
                      {"cmd":"core.dimension","args":{"tur":"hizali","birinci":[0,0],
                        "ikinci":[12000,0],"konum":[6000,-2000]}},
                      {"cmd":"core.vertex_move","args":{"nesne":[1],"kose":2,
                        "nokta":[16000,0]}}]})")
                    .ok());
    }
    CHECK_EQ(gui.measured(2), 16'000);
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    Rig replay;
    for (const auto& e : gui.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), gui.doc.content_hash());
}
