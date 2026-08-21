// SPDX-License-Identifier: GPL-3.0-or-later
//
// The snap and selection engine.
//
// Two properties carry more weight here than any individual case:
//
//   IDEMPOTENCE — `Bus` records the RESOLVED point in the journal, so replaying a
//   journal feeds a snapped point straight back into the same engine. If any rule
//   moved an already-resolved point, every replay would drift and the Article 6.4
//   proof would rot silently. Each rule is tested twice over.
//
//   SOURCE-BLINDNESS — the same aim through a GUI session, through the command
//   line and through a JSON script must land on the same millimetre, because the
//   aids are applied on the one path all three take (piricad.md §2.4).
#include "microtest.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/script/json_runner.hpp"

#include <array>

using namespace piricad;
using namespace piricad::command;

namespace {

using core::Mm;
using core::Point2;

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string echoed;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    /// Gives the bus a view, so the pixel tolerances become real millimetres.
    /// One millimetre per pixel keeps the arithmetic in the test readable.
    void with_view(double mm_per_pixel = 1.0) { bus.aids().set_view_scale(mm_per_pixel); }

    core::Result<DispatchResult> line(const std::string& text)
    {
        return bus.execute_line(text, Origin::Test);
    }
};

/// A closed square, 10 m on a side, as one Exterior ring.
core::EntityId add_square(core::Document& doc, Mm x, Mm y, Mm side)
{
    const std::array<Point2, 4> ring{Point2{x, y}, Point2{x + side, y}, Point2{x + side, y + side},
                                     Point2{x, y + side}};
    const core::RingGeometry::RingInput input{ring, core::RingRole::Exterior, 0};
    core::Op op;
    auto created = doc.add_area(doc.ensure_layer("PARSEL"), {&input, 1}, op);
    return created ? created.value() : core::kNoEntity;
}

} // namespace

// ------------------------------------------------------------- grid rule ----

TEST_CASE("YAKALAMA: ızgara en yakın kesişime oturur ve ikinci kez oynamaz")
{
    const Mm step = 1000; // 1 m

    CHECK(core::apply_grid(Point2{1400, 2600}, step) == (Point2{1000, 3000}));
    CHECK(core::apply_grid(Point2{-1400, -2600}, step) == (Point2{-1000, -3000}));
    CHECK(core::apply_grid(Point2{500, -500}, step) ==
          (Point2{1000, -1000})); // yarım, sıfırdan uzağa

    // Idempotence: a point already on the lattice is left where it is.
    const Point2 once  = core::apply_grid(Point2{1400, 2600}, step);
    const Point2 twice = core::apply_grid(once, step);
    CHECK(once == twice);

    // A step of zero is "no grid", not a division by zero.
    CHECK(core::apply_grid(Point2{7, 9}, 0) == (Point2{7, 9}));
}

// ------------------------------------------------------------ ortho rule ----

TEST_CASE("YAKALAMA: dik mod imleci en yakın eksene kilitler")
{
    const Point2 base{100000, 200000};

    // Mostly horizontal aim -> the Y of the base is kept.
    CHECK(core::apply_ortho(base, Point2{150000, 203000}) == (Point2{150000, 200000}));
    // Mostly vertical aim -> the X of the base is kept.
    CHECK(core::apply_ortho(base, Point2{101000, 260000}) == (Point2{100000, 260000}));
    // A tie is total and reproducible: the horizontal wins.
    CHECK(core::apply_ortho(base, Point2{110000, 210000}) == (Point2{110000, 200000}));

    const Point2 once = core::apply_ortho(base, Point2{150000, 203000});
    CHECK(core::apply_ortho(base, once) == once);
}

// ------------------------------------------------------------ polar rule ----

TEST_CASE("YAKALAMA: kutupsal izleme açıyı adıma yuvarlar, uzaklığı korur")
{
    const Point2 base{0, 0};
    const std::int64_t step = 90 * core::kUDegPerDegree;

    // 10 m out at ~5 degrees snaps onto the east axis with its length intact.
    const Point2 east = core::apply_polar(base, Point2{9962, 872}, step);
    CHECK(east.y == 0);
    CHECK(east.x > 9990 && east.x < 10010);

    // ~93 degrees snaps onto north.
    const Point2 north = core::apply_polar(base, Point2{-523, 9986}, step);
    CHECK(north.x == 0);
    CHECK(north.y > 9990 && north.y < 10010);

    // A 45-degree step keeps a 45-degree aim where it is, to the millimetre.
    const std::int64_t step45 = 45 * core::kUDegPerDegree;
    const Point2 diagonal     = core::apply_polar(base, Point2{7071, 7071}, step45);
    CHECK(diagonal.x == 7071);
    CHECK(diagonal.y == 7071);

    // Step zero is "no polar tracking".
    CHECK(core::apply_polar(base, Point2{123, 456}, 0) == (Point2{123, 456}));
}

// ------------------------------------------------------- object snapping ----

TEST_CASE("YAKALAMA: uç nokta en yakın tepeye oturur, öncelik en yakından yüksektir")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim    = Point2{300, 200}; // near the corner at (0,0)
    q.radius = 1000;
    q.modes  = core::SnapEndpoint | core::SnapNearest;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapEndpoint));
    CHECK(r.point == (Point2{0, 0}));
    CHECK(r.entity != core::kNoEntity);

    // Re-snapping the resolved point returns it unchanged (journal replay).
    q.aim = r.point;
    CHECK(core::snap(doc, q).point == r.point);
}

TEST_CASE("YAKALAMA: orta nokta, en yakın ve merkez modları")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    // Midpoint of the bottom edge is (5000, 0).
    core::SnapQuery mid;
    mid.aim                  = Point2{5100, 150};
    mid.radius               = 1000;
    mid.modes                = core::SnapMidpoint;
    const core::SnapResult m = core::snap(doc, mid);
    CHECK_EQ(static_cast<int>(m.mode), static_cast<int>(core::SnapMidpoint));
    CHECK(m.point == (Point2{5000, 0}));

    // Nearest lands on the edge itself, not on a vertex.
    core::SnapQuery near;
    near.aim                 = Point2{7000, 300};
    near.radius              = 1000;
    near.modes               = core::SnapNearest;
    const core::SnapResult n = core::snap(doc, near);
    CHECK_EQ(static_cast<int>(n.mode), static_cast<int>(core::SnapNearest));
    CHECK(n.point == (Point2{7000, 0}));

    // Centre of a closed square is its middle.
    core::SnapQuery centre;
    centre.aim               = Point2{5200, 4900};
    centre.radius            = 1000;
    centre.modes             = core::SnapCenter;
    const core::SnapResult c = core::snap(doc, centre);
    CHECK_EQ(static_cast<int>(c.mode), static_cast<int>(core::SnapCenter));
    CHECK(c.point == (Point2{5000, 5000}));
}

TEST_CASE("YAKALAMA: kesişim iki ayrı çizginin gerçek kesişme noktasını verir")
{
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("CIZIM");

    const std::array<Point2, 2> a{Point2{0, 0}, Point2{10000, 10000}};
    const std::array<Point2, 2> b{Point2{0, 10000}, Point2{10000, 0}};
    (void)doc.add_polyline(layer, a, op);
    (void)doc.add_polyline(layer, b, op);

    core::SnapQuery q;
    q.aim    = Point2{5200, 4800};
    q.radius = 1000;
    q.modes  = core::SnapIntersection;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapIntersection));
    CHECK(r.point == (Point2{5000, 5000}));
}

TEST_CASE("YAKALAMA: dik ayak önceki noktadan indirilen dikin ayağıdır")
{
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 0}};
    (void)doc.add_polyline(doc.ensure_layer("CIZIM"), edge, op);

    core::SnapQuery q;
    q.aim      = Point2{3200, 400};
    q.radius   = 1000;
    q.modes    = core::SnapPerpendicular;
    q.has_base = true;
    q.base     = Point2{3000, 5000};

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapPerpendicular));
    CHECK(r.point == (Point2{3000, 0}));
}

TEST_CASE("YAKALAMA: yarıçap sıfırken nesne yakalama devre dışıdır")
{
    // This is what keeps a headless journal replay honest: no view, no pixels, no
    // aperture, so a recorded point is never re-snapped onto a neighbour that the
    // recording session did not have (piricad/command/aids.hpp).
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim    = Point2{300, 200};
    q.radius = 0;
    q.modes  = core::SnapAllMask;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapNone));
    CHECK(r.point == (Point2{300, 200}));
}

TEST_CASE("YAKALAMA: sıra nesne > yön > ızgara")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim       = Point2{300, 200};
    q.radius    = 1000;
    q.modes     = core::SnapEndpoint | core::SnapGrid;
    q.grid_step = 1000;
    q.ortho     = true;
    q.has_base  = true;
    q.base      = Point2{-5000, 0};

    // A real corner beats both the axis lock and the lattice.
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapEndpoint));

    // Out of the aperture, dik mod takes over.
    q.aim = Point2{50000, 400};
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapOrtho));

    // Without a previous point there is no direction to lock to, so the grid wins.
    q.has_base               = false;
    const core::SnapResult g = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(g.mode), static_cast<int>(core::SnapGrid));
    CHECK(g.point == (Point2{50000, 0}));
}

// -------------------------------------------------------- pick primitives ---

TEST_CASE("SEÇİM: kutu testleri kesin tam sayı aritmetiğiyle çalışır")
{
    const core::Box2 box{0, 0, 10000, 10000};

    CHECK(core::segment_touches_box(Point2{5000, 5000}, Point2{6000, 6000}, box)); // içeride
    CHECK(
        core::segment_touches_box(Point2{-5000, 5000}, Point2{5000, 5000}, box)); // kenardan girer
    CHECK(core::segment_touches_box(Point2{-5000, -5000}, Point2{15000, 15000}, box)); // köşegen
    CHECK(
        core::segment_touches_box(Point2{-1000, 12000}, Point2{12000, -1000}, box)); // köşeyi keser
    CHECK(!core::segment_touches_box(Point2{-5000, 20000}, Point2{15000, 20000}, box));
    // Passes the bounding-box test but misses the corner it appears to reach: this
    // is the case a bbox-only crossing test would get wrong.
    CHECK(!core::segment_touches_box(Point2{-2000, 8000}, Point2{2000, 14000}, box));

    Point2 hit{};
    CHECK(core::segment_intersection(Point2{0, 0}, Point2{10000, 10000}, Point2{0, 10000},
                                     Point2{10000, 0}, hit));
    CHECK(hit == (Point2{5000, 5000}));
    // Parallel segments have no single crossing point.
    CHECK(!core::segment_intersection(Point2{0, 0}, Point2{10000, 0}, Point2{0, 1000},
                                      Point2{10000, 1000}, hit));

    CHECK(core::closest_point_on_segment(Point2{0, 0}, Point2{10000, 0}, Point2{3000, 5000}) ==
          (Point2{3000, 0}));
    // Beyond the end the answer is the endpoint, never the infinite line.
    CHECK(core::closest_point_on_segment(Point2{0, 0}, Point2{10000, 0}, Point2{20000, 5000}) ==
          (Point2{10000, 0}));
}

TEST_CASE("SEÇİM: pencere tamamen içeride olanı, kesen değene alır")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);    // wholly inside the query below
    (void)add_square(doc, 8000, 0, 10000); // straddles the right edge

    std::vector<core::EntityId> hits;
    core::pick_in_box(doc, core::Box2{-1000, -1000, 12000, 12000}, core::PickMode::Window, hits);
    CHECK_EQ(hits.size(), std::size_t{1});

    hits.clear();
    core::pick_in_box(doc, core::Box2{-1000, -1000, 12000, 12000}, core::PickMode::Crossing, hits);
    CHECK_EQ(hits.size(), std::size_t{2});
}

TEST_CASE("SEÇİM: tek nokta en yakın nesneyi bulur, eşitlikte küçük slotu seçer")
{
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("CIZIM");
    const std::array<Point2, 2> a{Point2{0, 0}, Point2{10000, 0}};
    const std::array<Point2, 2> b{Point2{0, 0}, Point2{0, 10000}};
    (void)doc.add_polyline(layer, a, op);
    (void)doc.add_polyline(layer, b, op);

    CHECK_EQ(core::pick_nearest(doc, Point2{5000, 200}, 1000), core::EntityId{0});
    CHECK_EQ(core::pick_nearest(doc, Point2{200, 5000}, 1000), core::EntityId{1});
    // Equidistant from both: the lower slot wins, so the answer is stable.
    CHECK_EQ(core::pick_nearest(doc, Point2{0, 0}, 1000), core::EntityId{0});
    // Nothing within reach.
    CHECK_EQ(core::pick_nearest(doc, Point2{500000, 500000}, 1000), core::kNoEntity);
}

// ---------------------------------------------------------- the selection ---

TEST_CASE("SEÇİM: küme anahtar sıralı, tekil ve sürüm sayar")
{
    Selection s;
    CHECK(s.empty());

    CHECK(s.add(core::EntityKey{7}));
    CHECK(s.add(core::EntityKey{3}));
    CHECK(!s.add(core::EntityKey{7})); // already there
    CHECK_EQ(s.size(), std::size_t{2});
    CHECK(s.keys().front() == core::EntityKey{3});
    CHECK(s.keys().back() == core::EntityKey{7});

    CHECK(s.contains(core::EntityKey{3}));
    CHECK(!s.contains(core::EntityKey{4}));

    const std::uint64_t revision = s.revision();
    CHECK(!s.toggle(core::EntityKey{3})); // was in, now out
    CHECK(s.toggle(core::EntityKey{3}));  // and back in
    CHECK(s.revision() > revision);

    CHECK(!s.add(core::EntityKey::None)); // "no entity" is never selected

    s.clear();
    CHECK(s.empty());
}

// ------------------------------------------------------------- the command --

TEST_CASE("SEÇ: tümü, temizle, kimlik, pencere, kesen ve nokta")
{
    Rig r;
    CHECK(r.line("KATMAN ad=CIZIM").ok());
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());
    CHECK(r.line("ÇİZGİ 100,100 110,100").ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{3});

    CHECK(r.line("SEÇ TÜMÜ").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{3});

    CHECK(r.line("SEÇ TEMİZLE").ok());
    CHECK(r.bus.selection().empty());

    // Keys start at 1 (EntityKey{0} is "none").
    CHECK(r.line("SEÇ NESNE nesneler=1 nesneler=3").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
    CHECK(r.bus.selection().contains(core::EntityKey{3}));

    // A window around the first two lines takes exactly those two.
    CHECK(r.line("SEÇ PENCERE -1,-1 11,6").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    // A crossing box that only clips the first line still takes it.
    CHECK(r.line("SEÇ KESEN 5,-1 20,1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));

    // KUTU reads the drag direction: right to left is a crossing box.
    CHECK(r.line("SEÇ KUTU 20,1 5,-1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});

    // Left to right is a window, and this one contains nothing whole.
    CHECK(r.line("SEÇ KUTU 5,-1 20,1").ok());
    CHECK(r.bus.selection().empty());

    // A point pick with an explicit tolerance works without a screen.
    CHECK(r.line("SEÇ NOKTA 5,0 tolerans=0.5").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
}

TEST_CASE("SEÇ: ekle ve çıkar seçimi büyütür ve küçültür")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());

    CHECK(r.line("SEÇ NESNE nesneler=1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});

    CHECK(r.line("SEÇ NESNE nesneler=2 islem=EKLE").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});

    CHECK(r.line("SEÇ NESNE nesneler=1 islem=ÇIKAR").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    CHECK(r.line("SEÇ NESNE nesneler=2 islem=TERSİNE").ok());
    CHECK(r.bus.selection().empty());
}

TEST_CASE("SEÇ: iki kimlikli JSON dizisi nokta sanılmaz")
{
    // Regression. `[485320, 4310220]` is a point and `[1, 2]` is a pair of ids, and
    // JSON cannot tell them apart. Before the bus consulted the SPEC, a script that
    // wrote `{"nesneler": [1, 2]}` was told its ids were the wrong type — and with
    // keys starting at 1, that is the FIRST thing a user writes.
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());
    CHECK(r.line("ÇİZGİ 0,9 10,9").ok());

    Args args;
    args.set("nesneler", Value::point(core::Point2{1, 2}));
    auto sent = r.bus.dispatch(Invocation{"core.select", args, Origin::Script});
    CHECK(sent.ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    // A point parameter is untouched by the repair: only a Selection parameter
    // reinterprets the pair.
    Args drawn;
    drawn.set("noktalar", Value::points({{0, 0}, {1000, 0}}));
    CHECK(r.bus.dispatch(Invocation{"core.line", drawn, Origin::Script}).ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{4});
}

TEST_CASE("SEÇ: seçim belge durumu değildir")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());

    const std::uint64_t hash     = r.doc.content_hash();
    const std::uint64_t revision = r.doc.revision();
    const std::size_t undo_depth = r.undo.undo_depth();
    const std::size_t journalled = r.journal.entries().size();

    CHECK(r.line("SEÇ TÜMÜ").ok());

    // model.md R43 and P15, checked rather than asserted in prose.
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.doc.revision(), revision);
    CHECK_EQ(r.undo.undo_depth(), undo_depth);
    CHECK_EQ(r.journal.entries().size(), journalled);
}

TEST_CASE("SEÇ: hatalı mod ve işlem beklenen ile girileni söyler")
{
    Rig r;
    CHECK(r.line("SEÇ OLMAYAN").ok()); // reported to the transcript, not an error
    CHECK(r.echoed.find("Beklenen mod") != std::string::npos);
    CHECK(r.echoed.find("OLMAYAN") != std::string::npos);

    r.echoed.clear();
    CHECK(r.line("SEÇ TÜMÜ islem=BİLİNMEYEN").ok());
    CHECK(r.echoed.find("Beklenen işlem") != std::string::npos);
}

TEST_CASE("SİL: argüman verilmezse etkin seçimi siler")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());

    CHECK(r.line("SEÇ NESNE nesneler=1").ok());
    CHECK(r.line("SİL").ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{1});

    // The erased key leaves the selection with it: a retired key is never reused.
    CHECK(r.bus.selection().empty());

    r.echoed.clear();
    CHECK(r.line("SİL").ok());
    CHECK(r.echoed.find("seçim boş") != std::string::npos);
}

// --------------------------------------------- the aids on the input path ---

TEST_CASE("YAKALAMA: aynı nişan arayüzden, komut satırından ve betikten aynı yere düşer")
{
    // The Article 6.4 proof, for snapping. Each client aims 200 mm off the corner
    // of an existing line; all three must draw to the corner itself, and all three
    // must journal the same resolved coordinate.
    const auto seed = [](Rig& r) {
        r.with_view(1.0); // 1 mm per pixel, so the 12-pixel aperture is 12 mm
        CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
        CHECK(r.line("MOD yakalama_modları 1").ok()); // uç nokta only
    };

    // ---- client 1: a GUI session fed by mouse clicks ----
    Rig gui;
    seed(gui);
    {
        auto started = gui.bus.begin_interactive("ÇİZGİ");
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(Point2{20000, 20000})).ok());
        CHECK(session.supply(Value::point(Point2{10008, 6})).ok()); // 10 mm off (10000,0)
        session.cancel();
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    seed(cli);
    CHECK(cli.line("ÇİZGİ 20,20 10.008,0.006").ok());

    // ---- client 3: a JSON script ----
    Rig scr;
    seed(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto ran = runner.run_text(
            R"([{"cmd":"core.line","args":{"noktalar":[[20000,20000],[10008,6]]}}])");
        CHECK(ran.ok());
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // And the point actually moved onto the corner.
    const core::RingGeometry& g = gui.doc.geometry();
    const core::RingSpan span   = g.rings_of(gui.doc.entities().slot[1]);
    CHECK(g.vertex(span.first, 1) == (Point2{10000, 0}));

    // The JOURNAL carries the resolved point, not the aim, for all three.
    const auto& gui_args = gui.journal.entries().back().args;
    const auto& cli_args = cli.journal.entries().back().args;
    const auto& scr_args = scr.journal.entries().back().args;
    CHECK(gui_args.to_json().dump() == cli_args.to_json().dump());
    CHECK(cli_args.to_json().dump() == scr_args.to_json().dump());
    CHECK(gui_args.to_json().dump().find("10000") != std::string::npos);
}

TEST_CASE("YAKALAMA: görünüm yokken çizim nişan aldığı yere düşer")
{
    // No view scale published: the aperture is zero, so a headless run — a batch
    // job, a journal replay, this test — draws exactly what it was given.
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 20,20 10.008,0.006").ok());

    const core::RingGeometry& g = r.doc.geometry();
    const core::RingSpan span   = g.rings_of(r.doc.entities().slot[1]);
    CHECK(g.vertex(span.first, 1) == (Point2{10008, 6}));
}

TEST_CASE("YAKALAMA: dik mod ve ızgara komut satırından çizilen noktayı da yönlendirir")
{
    Rig r;
    CHECK(r.line("MOD dik_mod evet").ok());
    CHECK(r.line("ÇİZGİ 0,0 10,3").ok());

    // The second point had no rubber-band origin only for the FIRST vertex; the
    // segment end is locked onto the horizontal axis through the start.
    const core::RingGeometry& g = r.doc.geometry();
    const core::RingSpan span   = g.rings_of(r.doc.entities().slot[0]);
    CHECK(g.vertex(span.first, 0) == (Point2{0, 0}));
    CHECK(g.vertex(span.first, 1) == (Point2{10000, 0}));

    Rig grid;
    CHECK(grid.line("MOD ızgaraya_yakala evet").ok());
    CHECK(grid.line("TERCİH ızgara_adımı 1000").ok());
    CHECK(grid.line("ÇİZGİ 0.4,0.4 10.4,0.4").ok());

    const core::RingGeometry& gg = grid.doc.geometry();
    const core::RingSpan gspan   = gg.rings_of(grid.doc.entities().slot[0]);
    CHECK(gg.vertex(gspan.first, 0) == (Point2{0, 0}));
    CHECK(gg.vertex(gspan.first, 1) == (Point2{10000, 0}));
}

TEST_CASE("YAKALAMA: günlük tekrar oynatıldığında belge değişmez")
{
    // Idempotence where it matters: a snapped run is replayed through the bus and
    // must reproduce the same document (piricad.md §2.2, CLAUDE.md 6.4).
    Rig original;
    original.with_view(1.0);
    CHECK(original.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(original.line("MOD yakalama_modları 1").ok());
    CHECK(original.line("ÇİZGİ 20,20 10.008,0.006").ok());

    Rig replay;
    replay.with_view(1.0);
    CHECK(replay.line("MOD yakalama_modları 1").ok());
    for (const auto& e : original.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), original.doc.content_hash());
}
