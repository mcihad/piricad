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
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"
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

    Rig c;
    for (const char* line :
         {"ALAN 0,0 20,0 20,10 0,10", "TARAMA nesneler=1 aralik=2.5 aci=30 cift=evet"})
        c.run(line);
    CHECK(c.said.find("2,500 m aralıklı kendi deseninizle çapraz tarama çizildi (1 sınır "
                      "halkası); 1 sınır nesnesine bağlı, o değişince tarama da güncellenir.\n") !=
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

// ------------------------------------------------------ pattern, 2nd stage ----

namespace {

/// The line-pattern layers of the hatch's symbol.
std::vector<core::SymbolLayer> pattern_layers(const Rig& r, std::int64_t key)
{
    std::vector<core::SymbolLayer> out;
    const core::StyleId st = r.doc.entities().style[r.slot(key)];
    for (const core::SymbolLayer& l : r.doc.styles().symbol_at(st).layers)
        if (l.type == core::SymbolLayerType::LinePatternFill) out.push_back(l);
    return out;
}

/// The distance of `p` across a line pattern layer's lines, modulo its spacing,
/// less the layer's own phase: zero when the lines pass through `p`.
double off_lattice(const core::SymbolLayer& l, Point2 p)
{
    const core::SinCos t = core::sin_cos_udeg(l.angle_udeg);
    const double s       = static_cast<double>(l.interval.value);
    double d = std::fmod(-static_cast<double>(p.x) * t.sin + static_cast<double>(p.y) * t.cos -
                             static_cast<double>(l.offset.value),
                         s);
    if (d < 0.0) d += s;
    return std::min(d, s - d);
}

} // namespace

TEST_CASE("TARAMA: desen başlangıç noktasından geçer; sınır taşınınca desen de taşınır")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                                    // 1
    r.run("TARAMA nesneler=1 desen=ANSI31 olcek=1000 baslangic=3.3,1.7"); // 2
    auto layers = pattern_layers(r, 2);
    REQUIRE_FALSE(layers.empty());
    CHECK(off_lattice(layers[0], Point2{3'300, 1'700}) < 1.0);

    // The parcel moved as a whole: the pattern's point went with it, so the
    // lines lie on the parcel where they lay before.
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=7.1,2.9");
    layers = pattern_layers(r, 2);
    REQUIRE_FALSE(layers.empty());
    CHECK(off_lattice(layers[0], Point2{10'400, 4'600}) < 1.0);
}

TEST_CASE("TARAMA: kendi aralığı metre cinsinden; cift=evet çapraz tarar")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");                      // 1
    r.run("TARAMA nesneler=1 aralik=2.5 aci=30 cift=evet"); // 2
    auto def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(2)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().name, std::string("_USER"));
    CHECK_EQ(def.value().pattern_type, 0);
    CHECK(def.value().double_lines);
    const auto layers = pattern_layers(r, 2);
    REQUIRE_EQ(layers.size(), std::size_t{2}); // the lines and the lines turned
    CHECK_EQ(layers[0].interval.value, 2'500);
    CHECK_EQ(layers[0].angle_udeg, 30'000'000);
    CHECK_EQ(layers[1].angle_udeg, 120'000'000);
    // Named by its spacing: `_USER` is the file's word for it, not the user's.
    CHECK(r.said.find("2,500 m aralıklı kendi deseninizle çapraz tarama çizildi (1 sınır "
                      "halkası)") != std::string::npos);
    CHECK(r.said.find("_USER") == std::string::npos);

    auto both = r.bus.execute_line("TARAMA nesneler=1 desen=ANSI31 aralik=2", Origin::Test);
    CHECK_FALSE(both.ok());
}

TEST_CASE("TARAMADÜZENLE: desen, açı ve ada kuralı değişir; bağ sürer, tek geri alma adımı")
{
    Rig r;
    r.run("ALAN 0,0 30,0 30,30 0,30");                  // 1
    r.run("DAİRE merkez=15,15 cevre=20,15");            // 2
    r.run("TARAMA nesneler=1 nesneler=2 desen=ANSI31"); // 3
    REQUIRE_EQ(r.rings(3).size(), std::size_t{2});

    r.said.clear();
    r.run("TARAMADÜZENLE nesneler=3 desen=ANSI37 aci=15");
    auto def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(3)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().name, std::string("ANSI37"));
    CHECK_EQ(def.value().angle_udeg, 15'000'000);
    CHECK(r.said.find("Tarama düzenlendi: 1 tarama; 'ANSI37' deseni, açı 15,00°") !=
          std::string::npos);

    // Islands ignored: the pool is filled; normal again: it is a hole again —
    // the hatch follows its objects, so it can find the island it dropped.
    r.run("TARAMADÜZENLE nesneler=3 stil=yoksay");
    CHECK_EQ(r.rings(3).size(), std::size_t{1});
    r.run("TARAMADÜZENLE nesneler=3 stil=normal");
    CHECK_EQ(r.rings(3).size(), std::size_t{2});
    REQUIRE_EQ(r.sources(3).size(), std::size_t{2});

    // Still following.
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=40,40");
    CHECK(r.has_corner(3, Point2{40'000, 40'000}));

    const std::size_t depth = r.undo.undo_depth();
    r.run("TARAMADÜZENLE nesneler=3 desen=NET");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    r.run("GERİAL");
    def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(3)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().name, std::string("ANSI37"));
}

TEST_CASE("TARAMA: ekranda desen yere bağlıdır — sahne çapası desenin kafesindedir")
{
    Rig r;
    r.run("ALAN 485300,4310200 485320,4310200 485320,4310210 485300,4310210"); // 1, TUREF-sized
    r.run("TARAMA nesneler=1 desen=ANSI31 olcek=1000 baslangic=485303.3,4310201.7");
    render::ViewTransform view;
    view.set_centre(Point2{485'311'000, 4'310'204'000}, 20.0);
    render::DrawList list;
    render::build_scene(r.doc, view, render::SceneOptions{}, list);
    bool checked = false;
    for (const render::PassStyle& ps : list.passes) {
        if (ps.type != core::SymbolLayerType::LinePatternFill) continue;
        REQUIRE(ps.anchored);
        // The anchor, back on the ground, lies on the pattern's lattice through
        // the hatch's origin: a whole number of spacings from it, across.
        const core::SinCos t = core::sin_cos_udeg(ps.angle_udeg);
        const double ax      = 485'311'000.0 + static_cast<double>(ps.anchor_x) * 20.0;
        const double ay      = 4'310'204'000.0 + static_cast<double>(ps.anchor_y) * 20.0;
        const double across  = -(ax - 485'303'300.0) * t.sin + (ay - 4'310'201'700.0) * t.cos;
        const double spacing = static_cast<double>(ps.interval_px) * 20.0;
        const double k       = across / spacing;
        CHECK(std::abs(k - std::round(k)) < 0.01);
        checked = true;
    }
    CHECK(checked);
}

TEST_CASE("TARAMADÜZENLE: kılavuzdaki örnek kelimesi kelimesine")
{
    Rig r;
    for (const char* line :
         {"ALAN 0,0 30,0 30,30 0,30", "DAİRE merkez=15,15 cevre=20,15",
          "TARAMA nesneler=1 nesneler=2 desen=ANSI31",
          "TARAMADÜZENLE nesneler=3 desen=ANSI37 aci=15", "TARAMADÜZENLE nesneler=3 stil=yoksay"})
        r.run(line);
    CHECK(r.said.find("Tarama düzenlendi: 1 tarama; 'ANSI37' deseni, açı 15,00°.\n"
                      "Tarama düzenlendi: 1 tarama; 'ANSI37' deseni, açı 15,00°.\n") !=
          std::string::npos);
    CHECK_EQ(r.rings(3).size(), std::size_t{1});
}

TEST_CASE("TARAMADÜZENLE: sınır nesnesini göstermek ona bağlı taramayı düzenler")
{
    // The hatch lies on the parcel's edges, so the parcel is what a hand points
    // at and what a surveyor names. Both roads reach the same hatch; an object
    // no hatch follows is counted as skipped, not guessed at.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");       // 1
    r.run("ÇİZGİ 0,20 10,20");               // 2
    r.run("TARAMA nesneler=1 desen=ANSI31"); // 3
    r.said.clear();
    r.run("TARAMADÜZENLE nesneler=1 nesneler=2 desen=ANSI37");
    auto def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(3)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().name, std::string("ANSI37"));
    CHECK(r.said.find("Tarama düzenlendi: 1 tarama; 'ANSI37' deseni; tarama olmayan 1 nesne "
                      "atlandı.") != std::string::npos);
    CHECK_EQ(r.sources(3).size(), std::size_t{1});

    // A broken link is no road: once the parcel no longer closes, the hatch
    // stops following it, and pointing at the parcel no longer means it.
    r.run("ÇİZGİDÜZENLE nesne=1 islem=ac");
    REQUIRE(r.sources(3).front().broken);
    auto refused = r.bus.execute_line("TARAMADÜZENLE nesneler=1 desen=NET", Origin::Test);
    CHECK_FALSE(refused.ok());
    def = core::hatch_of(r.doc.geometry(), r.doc.entities().slot[r.slot(3)]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().name, std::string("ANSI37"));
}

TEST_CASE("TARAMA: kâğıtta desen, paftasının ölçeğinde kendi aralığındadır — PDF'in çizdiği "
          "aralık (TODOS C-11)")
{
    // The sheet the PDF is painted from (app/print_service.cpp): an A4 at 1/500
    // and 300 dpi, the window fitted to the page. ANSI31 at olcek=500 is 3,175 mm
    // apart on that paper — the pattern's own spacing — and 1,5875 m on the
    // ground, which is what the DXF carries (test_io.cpp "tarama deseni çizim
    // biriminde…"). The model holds the ground to the millimetre, so the paper
    // is exact to 1/500 of one.
    Rig r;
    r.run("ALAN 0,0 140,0 140,100 0,100");                   // 1
    r.run("TARAMA nesneler=1 desen=ANSI31 olcek=500 aci=0"); // 2
    constexpr double kDpi        = 300.0;
    constexpr double kPxPerPaper = kDpi / 25.4;
    render::ViewTransform view;
    view.set_viewport(static_cast<int>(std::lround(297.0 * kPxPerPaper)),
                      static_cast<int>(std::lround(210.0 * kPxPerPaper)));
    core::Box2 sheet;
    sheet.extend(Point2{0, 0});
    sheet.extend(Point2{148'500, 105'000}); // 297 × 210 mm at 1/500
    view.fit(sheet, 0.0);
    render::SceneOptions options;
    options.pixels_per_paper_mm = kPxPerPaper;
    render::DrawList list;
    render::build_scene(r.doc, view, options, list);
    bool seen = false;
    for (const render::PassStyle& ps : list.passes) {
        if (ps.type != core::SymbolLayerType::LinePatternFill) continue;
        const double paper_mm = static_cast<double>(ps.interval_px) / kPxPerPaper;
        CHECK(paper_mm == doctest::Approx(3.175).epsilon(0.001));
        seen = true;
    }
    CHECK(seen);
}
