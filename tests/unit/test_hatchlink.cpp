// SPDX-License-Identifier: GPL-3.0-or-later
//
// A HATCH FOLLOWS ITS BOUNDARY (TODOS C-11).
//
// Each case is a way a parcel changes under its hatch: a corner moved, its
// courtyard redrawn, an island drawn inside it, the parcel erased, the hatch
// dragged away on its own, the boundary opened. The loops are asserted exactly,
// because a hatch that spills into a courtyard is a wrong picture of a title.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <cmath>
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

    /// The hatch's rings: role and corners of each.
    std::vector<std::pair<core::RingRole, std::vector<Point2>>> rings(std::int64_t key) const
    {
        std::vector<std::pair<core::RingRole, std::vector<Point2>>> out;
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot(key)]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            std::vector<Point2> pts;
            const auto xs = doc.geometry().ring_xs(r);
            const auto ys = doc.geometry().ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                pts.push_back(Point2{xs[v], ys[v]});
            out.emplace_back(doc.geometry().ring_role[r], std::move(pts));
        }
        return out;
    }

    std::vector<core::HatchSource> sources(std::int64_t key) const
    {
        const auto* got = doc.hatch_links().get(slot(key));
        return got == nullptr ? std::vector<core::HatchSource>{} : *got;
    }

    bool has_corner(std::int64_t key, Point2 p) const
    {
        for (const auto& [role, pts] : rings(key))
            for (const Point2 q : pts)
                if (q == p) return true;
        return false;
    }
};

} // namespace

TEST_CASE("BAĞLI TARAMA: parselin köşesi taşınınca tarama yeni sınıra oturur; tek geri alma adımı")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");       // 1
    r.run("TARAMA nesneler=1 desen=ANSI31"); // 2
    CHECK(r.said.find("1 sınır nesnesine bağlı") != std::string::npos);
    REQUIRE_EQ(r.sources(2).size(), std::size_t{1});

    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=26,14");
    CHECK(r.has_corner(2, Point2{26'000, 14'000}));
    CHECK_FALSE(r.has_corner(2, Point2{20'000, 10'000}));
    CHECK(r.said.find("Bağlı 1 tarama sınırını izledi ve yeniden kuruldu.") != std::string::npos);

    r.run("GERİAL");
    CHECK(r.has_corner(2, Point2{20'000, 10'000}));
}

TEST_CASE("BAĞLI TARAMA: delikli parsel değişince tarama delikten taşmaz")
{
    Rig r;
    // A parcel with a courtyard: the second ring is the hole.
    r.run("ALAN 0,0 20,0 20,20 0,20 5,5 10,5 10,10 5,10 bolum=4 bolum=4"); // 1
    r.run("TARAMA nesneler=1 desen=ANSI31");                               // 2
    auto rings = r.rings(2);
    REQUIRE_EQ(rings.size(), std::size_t{2});
    CHECK_EQ(rings[1].first, core::RingRole::Interior);

    // The courtyard grows: its corner moves out. The hatch's hole goes with it.
    r.run("KÖŞETAŞI nesne=1 kose=7 nokta=14,14");
    rings = r.rings(2);
    REQUIRE_EQ(rings.size(), std::size_t{2});
    CHECK_EQ(rings[1].first, core::RingRole::Interior);
    CHECK(r.has_corner(2, Point2{14'000, 14'000}));
    CHECK_FALSE(r.has_corner(2, Point2{10'000, 10'000}));
}

TEST_CASE("BAĞLI TARAMA: içteki seçili nesne ada olur; ada taşınınca delik de taşınır")
{
    Rig r;
    r.run("ALAN 0,0 30,0 30,30 0,30");                  // 1
    r.run("DAİRE merkez=15,15 cevre=20,15");            // 2, an island inside
    r.run("TARAMA nesneler=1 nesneler=2 desen=ANSI31"); // 3
    auto rings = r.rings(3);
    REQUIRE_EQ(rings.size(), std::size_t{2});
    CHECK_EQ(rings[0].first, core::RingRole::Exterior);
    CHECK_EQ(rings[1].first, core::RingRole::Interior);
    CHECK(r.said.find("1 delik") != std::string::npos);

    r.run("TAŞI nesneler=2 baslangic=15,15 bitis=10,10");
    rings = r.rings(3);
    REQUIRE_EQ(rings.size(), std::size_t{2});
    // The hole is the circle where it is now: every corner within its radius of 10,10.
    for (const Point2 p : rings[1].second) {
        const double dx = static_cast<double>(p.x - 10'000);
        const double dy = static_cast<double>(p.y - 10'000);
        CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - 5'000.0) < 2.0);
    }
}

TEST_CASE(
    "BAĞLI TARAMA: sınırı silinince bağ kopar, tarama durur ve bu söylenir; geri alınca döner")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");       // 1
    r.run("TARAMA nesneler=1 desen=ANSI31"); // 2
    const std::uint64_t before = r.doc.content_hash();
    r.said.clear();
    r.run("SİL nesneler=1");
    REQUIRE(r.doc.alive(r.slot(2)));
    REQUIRE_EQ(r.sources(2).size(), std::size_t{1});
    CHECK(r.sources(2)[0].broken);
    CHECK(r.has_corner(2, Point2{20'000, 10'000}));
    CHECK(r.said.find("Sınırı silindiği için 1 tarama bağı koptu") != std::string::npos);

    r.said.clear();
    r.run("NESNEBİLGİ nesneler=2");
    CHECK(r.said.find("1 sınır bağı kopuk") != std::string::npos);

    r.run("GERİAL");
    CHECK_FALSE(r.sources(2)[0].broken);
    CHECK_EQ(r.doc.content_hash(), before);
}

TEST_CASE("BAĞLI TARAMA: yalnız tarama taşınırsa bağından çözülür; birlikte taşınırsa bağ sürer")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");       // 1
    r.run("TARAMA nesneler=1 desen=ANSI31"); // 2
    r.run("TAŞI nesneler=1 nesneler=2 baslangic=0,0 bitis=5,5");
    REQUIRE_EQ(r.sources(2).size(), std::size_t{1});
    auto def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(2)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().origin, (Point2{5'000, 5'000})); // the pattern went with the parcel

    r.said.clear();
    r.run("TAŞI nesneler=2 baslangic=0,0 bitis=40,0");
    CHECK(r.sources(2).empty());
    CHECK(r.said.find("1 tarama sınırından ayrı taşındığı için bağından çözüldü.") !=
          std::string::npos);
    // Free now: the parcel moves without it.
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=30,20");
    CHECK(r.has_corner(2, Point2{65'000, 15'000}));
}

TEST_CASE("BAĞLI TARAMA: sınır açılınca tarama son hâlinde kalır, bağı kopar; bagla=hayır bağlamaz")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0 20,10 0,10 0,0"); // 1, closed by its last corner
    const auto closed = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(1)]);
    (void)closed;
    r.run("ALAN 40,0 60,0 60,10 40,10");     // 2
    r.run("TARAMA nesneler=2 desen=ANSI31"); // 3
    r.run("ÇİZGİDÜZENLE nesne=2 islem=ac");
    REQUIRE_EQ(r.sources(3).size(), std::size_t{1});
    CHECK(r.sources(3)[0].broken);
    CHECK(r.has_corner(3, Point2{60'000, 10'000}));
    CHECK(r.said.find("Sınırı artık kapanmadığı için 1 tarama bağı koptu") != std::string::npos);

    r.run("ALAN 80,0 90,0 90,10 80,10");                 // 4
    r.run("TARAMA nesneler=4 desen=ANSI31 bagla=hayır"); // 5
    CHECK(r.sources(5).empty());
}

TEST_CASE(
    "BAĞLI TARAMA KANIT: arayüz, komut satırı, betik ve oynatma aynı taramayı ve izlemeyi verir")
{
    Rig gui;
    gui.run("ALAN 0,0 20,0 20,10 0,10");
    {
        auto started = gui.bus.begin_interactive("TARAMA desen=ANSI31", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        if (session.waiting()) CHECK(session.supply(Value::ids({1})).ok());
        CHECK(gui.bus.finish(session).ok());
    }
    REQUIRE(gui.bus.execute_line("KÖŞETAŞI nesne=1 kose=3 nokta=26,14", Origin::Gui).ok());

    Rig cli;
    cli.run("ALAN 0,0 20,0 20,10 0,10");
    REQUIRE(cli.bus.execute_line("TARAMA nesneler=1 desen=ANSI31", Origin::CommandLine).ok());
    REQUIRE(cli.bus.execute_line("KÖŞETAŞI nesne=1 kose=3 nokta=26,14", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        REQUIRE(runner
                    .run_text(R"json({"ad":"Kanıt","komutlar":[
                      {"cmd":"core.area","args":{"noktalar":[[0,0],[20000,0],[20000,10000],
                        [0,10000]]}},
                      {"cmd":"core.hatch","args":{"nesneler":[1],"desen":"ANSI31"}},
                      {"cmd":"core.vertex_move","args":{"nesne":[1],"kose":3,
                        "nokta":[26000,14000]}}]})json")
                    .ok());
    }
    CHECK(gui.has_corner(2, Point2{26'000, 14'000}));
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    Rig replay;
    for (const auto& e : gui.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), gui.doc.content_hash());
}

TEST_CASE("BAĞLI TARAMA: yalnız sınırı taşınan taramanın deseni de parselle birlikte kayar")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");       // 1
    r.run("TARAMA nesneler=1 desen=ANSI31"); // 2
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=7,3");
    CHECK(r.has_corner(2, Point2{27'000, 13'000}));
    auto def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(2)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().origin, (Point2{7'000, 3'000}));

    // A corner moved is a reshape, not a move: the pattern stays put.
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=30,15");
    def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(2)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().origin, (Point2{7'000, 3'000}));
}

TEST_CASE("TARAMA: kılavuzdaki bağlı tarama örnekleri kelimesi kelimesine")
{
    // docs/komutlar/hatch.md prints these lines (CLAUDE.md 11.6).
    Rig a;
    for (const char* line : {"ALAN 0,0 20,0 20,10 0,10", "TARAMA nesneler=1 desen=ANSI31",
                             "KÖŞETAŞI nesne=1 kose=3 nokta=26,14"})
        a.run(line);
    CHECK(a.said.find("'ANSI31' deseniyle tarama çizildi (1 sınır halkası); 1 sınır nesnesine "
                      "bağlı, o değişince tarama da güncellenir.\n") != std::string::npos);
    CHECK(a.said.find("Bağlı 1 tarama sınırını izledi ve yeniden kuruldu.\n") != std::string::npos);

    Rig b;
    for (const char* line :
         {"ALAN 0,0 30,0 30,30 0,30 4,4 12,4 12,12 4,12 bolum=4 bolum=4",
          "DAİRE merkez=20,20 cevre=24,20", "TARAMA nesneler=1 nesneler=2 desen=ANSI31"})
        b.run(line);
    CHECK(b.said.find("'ANSI31' deseniyle tarama çizildi (3 sınır halkası, 2 delik); 2 sınır "
                      "nesnesine bağlı, o değişince tarama da güncellenir.\n") !=
          std::string::npos);
}

TEST_CASE("BAĞLI TARAMA: bir sınırı silinen tarama kalanlardan kurulmaz; ada dolu hâle gelmez")
{
    Rig r;
    r.run("ALAN 0,0 30,0 30,30 0,30");                  // 1
    r.run("DAİRE merkez=15,15 cevre=20,15");            // 2, the island
    r.run("TARAMA nesneler=1 nesneler=2 desen=ANSI31"); // 3
    const auto before = r.rings(3);
    REQUIRE_EQ(before.size(), std::size_t{2});
    r.run("SİL nesneler=1");
    // The hatch is what it was: the parcel's face with the island a hole — not
    // the island filled, which is what a rebuild from the circle alone would be.
    CHECK(r.rings(3) == before);
    // And it no longer follows the island either.
    r.run("TAŞI nesneler=2 baslangic=15,15 bitis=10,10");
    CHECK(r.rings(3) == before);
}
