// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE CORNER BETWEEN TWO OBJECTS (TODOS C-06).
//
// A fillet is right when its centre is the radius away from both objects, its
// tangent points lie on both, and it sits in the corner the picks name — never
// the one across from it. Every expected value below is a closed form a
// surveyor can check by hand, asserted to the millimetre rounding allows.
#include "piricad_test.hpp"

#include "piricad/core/corner.hpp"
#include "piricad/core/curve_path.hpp"
#include "piricad/core/fillet.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/core/pick.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace piricad::core;

namespace {

CurvePath line(Point2 a, Point2 b)
{
    CurvePath p;
    p.pieces.push_back(PathPiece{.from = a, .to = b});
    return p;
}

CurvePath arc(Point2 centre, Mm radius, Point2 from, Point2 to)
{
    CurvePath p;
    p.pieces.push_back(arc_piece(centre, radius, from, to, true));
    return p;
}

double gap(Point2 a, Point2 b)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

/// The distance from `p` to the line through a and b, in millimetres.
double off_line(Point2 p, Point2 a, Point2 b)
{
    const double ex = static_cast<double>(b.x - a.x);
    const double ey = static_cast<double>(b.y - a.y);
    return std::abs(ex * static_cast<double>(p.y - a.y) - ey * static_cast<double>(p.x - a.x)) /
           std::sqrt(ex * ex + ey * ey);
}

} // namespace

TEST_CASE("C-06: iki çizginin L köşesi yuvarlanır; teğetlik ve yarıçap tutar")
{
    const CurvePath a = line({0, 0}, {10'000, 0});
    const CurvePath b = line({10'000, 0}, {10'000, 10'000});
    auto f            = fillet_pair(a, {5'000, 0}, b, {10'000, 5'000}, 2'000);
    REQUIRE(f.ok());
    const PairCorner& c = f.value();
    CHECK(c.centre == (Point2{8'000, 2'000}));
    CHECK(c.on_a == (Point2{8'000, 0}));
    CHECK(c.on_b == (Point2{10'000, 2'000}));
    CHECK(path_vertices(c.a) == std::vector<Point2>{{0, 0}, {8'000, 0}});
    CHECK(path_vertices(c.b) == std::vector<Point2>{{10'000, 2'000}, {10'000, 10'000}});
    REQUIRE(c.has_link);
    CHECK(c.link.kind == PathPiece::Kind::Arc);
    CHECK_EQ(c.link.radius, Mm{2'000});
    CHECK(std::abs(std::abs(c.link.sweep_udeg) - 90'000'000) <= 1); ///< a quarter turn
    // Tangent: the centre is the radius from both lines, and from both tangent points.
    CHECK(std::abs(off_line(c.centre, {0, 0}, {10'000, 0}) - 2'000.0) <= 1.0);
    CHECK(std::abs(off_line(c.centre, {10'000, 0}, {10'000, 10'000}) - 2'000.0) <= 1.0);
}

TEST_CASE("C-06: kesişen iki çizgide yuvarlama seçilen çeyreğe gider, karşısına gitmez")
{
    const CurvePath a = line({-10'000, 0}, {10'000, 0});
    const CurvePath b = line({0, -10'000}, {0, 10'000});

    // The right part of A and the upper part of B: the upper-right corner.
    auto ne = fillet_pair(a, {5'000, 0}, b, {0, 5'000}, 2'000);
    REQUIRE(ne.ok());
    CHECK(ne.value().centre == (Point2{2'000, 2'000}));
    CHECK(path_vertices(ne.value().a) == std::vector<Point2>{{2'000, 0}, {10'000, 0}});
    CHECK(path_vertices(ne.value().b) == std::vector<Point2>{{0, 2'000}, {0, 10'000}});

    // The left part of A instead: the upper-left corner, and A keeps its left.
    auto nw = fillet_pair(a, {-5'000, 0}, b, {0, 5'000}, 2'000);
    REQUIRE(nw.ok());
    CHECK(nw.value().centre == (Point2{-2'000, 2'000}));
    CHECK(path_vertices(nw.value().a) == std::vector<Point2>{{-10'000, 0}, {-2'000, 0}});

    // And the lower parts: the lower-left, whatever order the picks came in.
    auto sw = fillet_pair(b, {0, -5'000}, a, {-5'000, 0}, 2'000);
    REQUIRE(sw.ok());
    CHECK(sw.value().centre == (Point2{-2'000, -2'000}));
}

TEST_CASE("C-06: çizgi ile yay arasındaki yuvarlama ikisine de teğettir")
{
    // A quarter circle of radius 10 m and a line along y = 5 m that crosses it.
    const CurvePath curve = arc({0, 0}, 10'000, {10'000, 0}, {0, 10'000});
    const CurvePath road  = line({-5'000, 5'000}, {15'000, 5'000});
    auto f                = fillet_pair(road, {0, 5'000}, curve, {9'900, 1'411}, 1'000);
    REQUIRE(f.ok());
    const PairCorner& c = f.value();
    // Inside the circle and below the line: |c| = R − r, and 1 m under y = 5.
    CHECK(std::abs(gap(c.centre, {0, 0}) - 9'000.0) <= 1.5);
    CHECK(std::abs(static_cast<double>(5'000 - c.centre.y) - 1'000.0) <= 1.0);
    CHECK(std::abs(gap(c.centre, c.on_a) - 1'000.0) <= 1.5);
    CHECK(std::abs(gap(c.centre, c.on_b) - 1'000.0) <= 1.5);
    CHECK(std::abs(gap(c.on_b, {0, 0}) - 10'000.0) <= 1.5); ///< the tangent point is ON the arc
    // The arc stays an arc, cut back to its tangent point.
    REQUIRE_EQ(c.b.pieces.size(), 1u);
    CHECK(c.b.pieces[0].kind == PathPiece::Kind::Arc);
}

TEST_CASE("C-06: iki yay arasındaki yuvarlama iki çembere de dıştan teğettir")
{
    const CurvePath left  = arc({0, 0}, 10'000, {10'000, 0}, {0, 10'000});
    const CurvePath right = arc({18'000, 0}, 10'000, {18'000, 10'000}, {8'000, 0});
    auto f                = fillet_pair(left, {7'071, 7'071}, right, {10'929, 7'071}, 1'000);
    REQUIRE(f.ok());
    const PairCorner& c = f.value();
    CHECK(std::abs(gap(c.centre, {0, 0}) - 11'000.0) <= 1.5);
    CHECK(std::abs(gap(c.centre, {18'000, 0}) - 11'000.0) <= 1.5);
    CHECK(c.centre.x == 9'000);
    CHECK(std::abs(gap(c.centre, c.on_a) - 1'000.0) <= 1.5);
    CHECK(std::abs(gap(c.centre, c.on_b) - 1'000.0) <= 1.5);
}

TEST_CASE("C-06: sıfır yarıçap iki nesneyi keskin köşede buluşturur")
{
    // Two lines that stop short of each other: both carried to their crossing.
    const CurvePath a = line({0, 0}, {8'000, 0});
    const CurvePath b = line({10'000, 2'000}, {10'000, 10'000});
    auto f            = fillet_pair(a, {4'000, 0}, b, {10'000, 6'000}, 0);
    REQUIRE(f.ok());
    CHECK_FALSE(f.value().has_link);
    CHECK(path_vertices(f.value().a) == std::vector<Point2>{{0, 0}, {10'000, 0}});
    CHECK(path_vertices(f.value().b) == std::vector<Point2>{{10'000, 0}, {10'000, 10'000}});
}

TEST_CASE("C-06: sığmayan yarıçap söylenerek reddedilir; öbür tarafta çizilmez")
{
    const CurvePath a = line({0, 0}, {10'000, 0});
    const CurvePath b = line({10'000, 0}, {10'000, 10'000});
    auto f            = fillet_pair(a, {5'000, 0}, b, {10'000, 5'000}, 20'000);
    REQUIRE_FALSE(f.ok());
    CHECK(f.error().message.find("sığmıyor") != std::string::npos);
}

TEST_CASE("C-06: köşeye yakın tıklamak büyük yarıçapı engellemez; seçilen parça kalır")
{
    // The mouse's natural pick is close to the corner, and the radius is shown
    // after it: a tangent point past the pick keeps the picked side, as in
    // every CAD, rather than being refused or flipping to the corner's side.
    const CurvePath a = line({0, 0}, {10'000, 0});
    const CurvePath b = line({10'000, 0}, {10'000, 10'000});
    auto f            = fillet_pair(a, {9'500, 0}, b, {10'000, 500}, 3'000);
    REQUIRE(f.ok());
    CHECK(f.value().centre == (Point2{7'000, 3'000}));
    CHECK(path_vertices(f.value().a) == std::vector<Point2>{{0, 0}, {7'000, 0}});
    CHECK(path_vertices(f.value().b) == std::vector<Point2>{{10'000, 3'000}, {10'000, 10'000}});

    // The same for a chamfer: cut back past the picks, the far parts kept.
    auto c = chamfer_pair(a, {9'500, 0}, b, {10'000, 500}, 3'000, 3'000);
    REQUIRE(c.ok());
    CHECK(path_vertices(c.value().a) == std::vector<Point2>{{0, 0}, {7'000, 0}});
    CHECK(path_vertices(c.value().b) == std::vector<Point2>{{10'000, 3'000}, {10'000, 10'000}});

    // A pick ON the crossing names no part, and is said so.
    auto on = fillet_pair(a, {10'000, 0}, b, {10'000, 500}, 3'000);
    REQUIRE_FALSE(on.ok());
    CHECK(on.error().message.find("kalacak parçalarından") != std::string::npos);
}

TEST_CASE("C-06: yay ile çizgide teğet noktası seçimin ötesine düşse de yay kısalır")
{
    // The quarter circle and the line along y = 5 m of the line–arc case, the
    // arc picked near where the two cross: the tangent point lands between
    // the arc's start and the pick, and the arc keeps its start side.
    const CurvePath curve = arc({0, 0}, 10'000, {10'000, 0}, {0, 10'000});
    const CurvePath road  = line({-5'000, 5'000}, {15'000, 5'000});
    auto f                = fillet_pair(road, {7'000, 5'000}, curve, {9'063, 4'226}, 2'000);
    REQUIRE(f.ok());
    const PairCorner& c = f.value();
    CHECK(std::abs(gap(c.centre, {0, 0}) - 8'000.0) <= 1.5);
    CHECK(std::abs(static_cast<double>(5'000 - c.centre.y) - 2'000.0) <= 1.0);
    REQUIRE_EQ(c.b.pieces.size(), 1u);
    CHECK(c.b.pieces[0].from == (Point2{10'000, 0})); ///< the start side stays
    CHECK(std::abs(gap(c.b.pieces[0].to, c.on_b)) <= 1.5);
}

TEST_CASE("C-06: paralel çizgiler arasında yuvarlama ve pah reddedilir")
{
    const CurvePath a = line({0, 0}, {10'000, 0});
    const CurvePath b = line({0, 5'000}, {10'000, 5'000});
    CHECK_FALSE(fillet_pair(a, {5'000, 0}, b, {5'000, 5'000}, 1'000).ok());
    CHECK_FALSE(chamfer_pair(a, {5'000, 0}, b, {5'000, 5'000}, 1'000, 1'000).ok());
}

TEST_CASE("C-06: iki çizgi arasındaki pah verilen mesafelerde keser")
{
    const CurvePath a = line({0, 0}, {10'000, 0});
    const CurvePath b = line({10'000, 0}, {10'000, 10'000});
    auto f            = chamfer_pair(a, {5'000, 0}, b, {10'000, 5'000}, 2'000, 3'000);
    REQUIRE(f.ok());
    CHECK(f.value().on_a == (Point2{8'000, 0}));
    CHECK(f.value().on_b == (Point2{10'000, 3'000}));
    CHECK(path_vertices(f.value().a) == std::vector<Point2>{{0, 0}, {8'000, 0}});
    CHECK(path_vertices(f.value().b) == std::vector<Point2>{{10'000, 3'000}, {10'000, 10'000}});
    REQUIRE(f.value().has_link);
    CHECK(f.value().link.kind == PathPiece::Kind::Segment);

    // A chamfer is between straight edges only.
    const CurvePath curve = arc({0, 0}, 10'000, {10'000, 0}, {0, 10'000});
    CHECK_FALSE(chamfer_pair(a, {5'000, 0}, curve, {7'071, 7'071}, 1'000, 1'000).ok());
}

TEST_CASE("C-06: bir çizgi daireye yuvarlanınca daire bütün kalır")
{
    CurvePath round;
    round.pieces.push_back(PathPiece{.kind       = PathPiece::Kind::Arc,
                                     .from       = {10'000, 0},
                                     .to         = {10'000, 0},
                                     .centre     = {0, 0},
                                     .radius     = 10'000,
                                     .sweep_udeg = kUDegFullCircle});
    round.closed      = true;
    const CurvePath a = line({-20'000, 12'000}, {20'000, 12'000}); ///< passes above the circle
    auto f            = fillet_pair(a, {5'000, 12'000}, round, {0, 10'000}, 1'000);
    REQUIRE(f.ok());
    CHECK(f.value().b == round);
    CHECK(std::abs(gap(f.value().centre, {0, 0}) - 11'000.0) <= 1.5);
}

TEST_CASE("C-06: köşe önizlemesinin baytları gidip gelir, bozuğu reddedilir")
{
    const PairCornerGuide guide{.key_a  = 3,
                                .key_b  = 7,
                                .pick_a = {1'000, -2'000},
                                .pick_b = {5'000, 6'000},
                                .fillet = false,
                                .trim   = false};
    const auto bytes = encode_pair_corner_guide(guide);
    auto back        = decode_pair_corner_guide(bytes);
    REQUIRE(back.ok());
    CHECK_EQ(back.value().key_a, 3);
    CHECK_EQ(back.value().key_b, 7);
    CHECK(back.value().pick_a == guide.pick_a);
    CHECK(back.value().pick_b == guide.pick_b);
    CHECK_FALSE(back.value().fillet);
    CHECK_FALSE(back.value().trim);
    std::vector<std::uint8_t> bad = bytes;
    bad[0]                        = 9;
    CHECK_FALSE(decode_pair_corner_guide(bad).ok());
    CHECK_FALSE(decode_pair_corner_guide(std::vector<std::uint8_t>{}).ok());

    // The every-corner payload carries the other objects it cuts, and a
    // truncated or padded one is refused.
    const auto many = encode_corner_preview(
        CornerPreview{.key = 3, .at = 1, .fillet = true, .every = true, .also = {5, 9}});
    auto read = decode_corner_preview(many);
    REQUIRE(read.ok());
    CHECK_EQ(read.value().key, 3);
    CHECK(read.value().every);
    CHECK(read.value().also == std::vector<std::int64_t>{5, 9});
    CHECK_FALSE(decode_corner_preview(std::span(many).first(many.size() - 1)).ok());
    std::vector<std::uint8_t> padded = many;
    padded.push_back(0);
    CHECK_FALSE(decode_corner_preview(padded).ok());
}

// =============================================================================
// YUVARLA and PAH between two objects, and on every corner of a run
// =============================================================================

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/core/document.hpp"

namespace {

using namespace piricad;
using namespace piricad::command;

struct Rig
{
    Document doc;
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

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }
};

/// Every live object's kind and walked path, in key order.
std::vector<std::pair<KindId, CurvePath>> live(const Document& doc)
{
    std::vector<std::pair<KindId, CurvePath>> out;
    for (EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        auto p = path_of(doc, e, PathScope::Curves);
        out.emplace_back(doc.entities().kind[e], p ? *p : CurvePath{});
    }
    return out;
}

} // namespace

TEST_CASE("C-06: YUVARLA iki çizgi arasında — yazarak; kaynaklar teğet noktalarına budanır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0");
    r.run("ÇOKLUÇİZGİ 10,0 10,10");
    r.run("YUVARLA nesne=1 2 nokta=5,0 ikinci_nokta=10,5 yaricap=2");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    CHECK(path_vertices(all[0].second) == std::vector<Point2>{{0, 0}, {8'000, 0}});
    CHECK(path_vertices(all[1].second) == std::vector<Point2>{{10'000, 2'000}, {10'000, 10'000}});
    CHECK(all[2].first == kArcKind);
    CHECK(all[2].second.pieces[0].centre == (Point2{8'000, 2'000}));
    CHECK(r.echoed.find("köşe yuvarlatıldı (yarıçap 2,000 m)") != std::string::npos);
}

TEST_CASE("C-06: YUVARLA iki çizgi arasında — tıklayarak; yazılanla aynı çizim ve aynı günlük")
{
    Rig clicked;
    clicked.run("ÇOKLUÇİZGİ 0,0 10,0");
    clicked.run("ÇOKLUÇİZGİ 10,0 10,10");
    auto started = clicked.bus.begin_interactive("YUVARLA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point({5'000, 0})).ok()); ///< the middle of a line: the first of two
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "ikinci_nokta");
    REQUIRE(s.supply(Value::point({10'000, 5'000})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::PairCorner);
    CHECK(s.prompt().pick_distance);
    CHECK(s.prompt().rubber_origin == (Point2{10'000, 0})); ///< where the two meet
    REQUIRE(s.supply(Value::number(2.0)).ok());
    REQUIRE(clicked.bus.finish(s).ok());

    Rig typed;
    typed.run("ÇOKLUÇİZGİ 0,0 10,0");
    typed.run("ÇOKLUÇİZGİ 10,0 10,10");
    typed.run("YUVARLA nesne=1 2 nokta=5,0 ikinci_nokta=10,5 yaricap=2");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    CHECK(clicked.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());
}

TEST_CASE("C-06: budama=hayir kaynaklara dokunmaz, yalnız yayı koyar")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0");
    r.run("ÇOKLUÇİZGİ 10,0 10,10");
    r.run("YUVARLA nesne=1 2 nokta=5,0 ikinci_nokta=10,5 yaricap=2 budama=hayir");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    CHECK(path_vertices(all[0].second) == std::vector<Point2>{{0, 0}, {10'000, 0}});
    CHECK(path_vertices(all[1].second) == std::vector<Point2>{{10'000, 0}, {10'000, 10'000}});
    CHECK(all[2].first == kArcKind);
}

TEST_CASE("C-06: sıfır yarıçap iki çizgiyi keskin köşede buluşturur")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 8,0");
    r.run("ÇOKLUÇİZGİ 10,2 10,10");
    r.run("YUVARLA nesne=1 2 nokta=4,0 ikinci_nokta=10,6 yaricap=0");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 2u); ///< no arc
    CHECK(path_vertices(all[0].second) == std::vector<Point2>{{0, 0}, {10'000, 0}});
    CHECK(path_vertices(all[1].second) == std::vector<Point2>{{10'000, 0}, {10'000, 10'000}});
    CHECK(r.echoed.find("keskin köşede buluştu") != std::string::npos);
}

TEST_CASE("C-06: PAH iki çizgi arasında iki ayrı mesafeyle keser")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0");
    r.run("ÇOKLUÇİZGİ 10,0 10,10");
    r.run("PAH nesne=1 2 nokta=5,0 ikinci_nokta=10,5 mesafe=2 ikinci_mesafe=3");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    CHECK(path_vertices(all[0].second) == std::vector<Point2>{{0, 0}, {8'000, 0}});
    CHECK(path_vertices(all[1].second) == std::vector<Point2>{{10'000, 3'000}, {10'000, 10'000}});
    CHECK(path_vertices(all[2].second) == std::vector<Point2>{{8'000, 0}, {10'000, 3'000}});
}

TEST_CASE("C-06: YUVARLA çizgiyle yay arasında; yay yay kalır, yuvarlama ikisine teğet")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    r.run("ÇOKLUÇİZGİ -5,5 15,5");
    r.run("YUVARLA nesne=2 1 nokta=0,5 ikinci_nokta=9.9,1.411 yaricap=1");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    CHECK(all[0].first == kArcKind); ///< the arc, cut back but still an arc
    CHECK(all[2].first == kArcKind); ///< the fillet
    const PathPiece& fillet = all[2].second.pieces[0];
    CHECK_EQ(fillet.radius, Mm{1'000});
}

TEST_CASE("C-06: aynı çoklu çizginin bitişik iki kenarına tıklamak o köşeyi yuvarlar")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10");
    auto started = r.bus.begin_interactive("YUVARLA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point({5'000, 0})).ok());
    REQUIRE(s.supply(Value::point({10'000, 5'000})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::Corner); ///< the polyline's own corner
    REQUIRE(s.supply(Value::number(2.0)).ok());
    REQUIRE(r.bus.finish(s).ok());
    // ONE OBJECT: the line with its corner an arc, as the corner tool makes it.
    CHECK_EQ(r.doc.live_entity_count(), 1u);
    CHECK(r.doc.entities().kind[r.doc.slot_of(EntityKey{1})] == kArcPolylineKind);
}

TEST_CASE("C-06: bütün köşeler — açık çizgi tek yaylı çoklu çizgi olur, kapalı alan alan kalır")
{
    Rig open_run;
    open_run.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    open_run.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    auto all = live(open_run.doc);
    REQUIRE_EQ(all.size(), 1u);
    CHECK(all[0].first == kArcPolylineKind);
    std::size_t arcs = 0;
    for (const PathPiece& p : all[0].second.pieces)
        arcs += p.kind == PathPiece::Kind::Arc ? 1 : 0;
    CHECK_EQ(arcs, 2u);
    CHECK(open_run.echoed.find("2 köşe yuvarlatıldı; çizgi tek bir yaylı çoklu çizgi oldu") !=
          std::string::npos);

    // A square's four corners cut by 1 m: the parcel stays a parcel and loses
    // four half-square-metres.
    Rig square;
    square.run("ALAN 0,0 10,0 10,10 0,10");
    square.run("PAH nesne=1 hepsi=evet mesafe=1");
    all = live(square.doc);
    REQUIRE_EQ(all.size(), 1u);
    CHECK(all[0].first == kPolylineKind);
    CHECK_EQ(path_vertices(all[0].second).size(), 9u); ///< eight corners, the seam repeated
    CHECK(square.echoed.find("4 köşeye pah kırıldı") != std::string::npos);

    // A size one corner cannot take is passed over and counted: the first
    // corner has 10 m either side, the second only 1 m on its last edge.
    Rig tight;
    tight.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 11,10");
    tight.run("YUVARLA nesne=1 hepsi=evet yaricap=3");
    CHECK(tight.echoed.find("1 köşe yuvarlatıldı") != std::string::npos);
    CHECK(tight.echoed.find("1 köşe bu değere sığmadığı ya da düz olduğu için olduğu gibi kaldı") !=
          std::string::npos);

    // And a size no corner can take is refused, the run untouched.
    Rig none;
    none.run("ÇOKLUÇİZGİ 0,0 10,0 10,1 20,1");
    const std::uint64_t before = none.doc.content_hash();
    auto refused = none.bus.execute_line("YUVARLA nesne=1 hepsi=evet yaricap=3", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("hiçbir köşeye sığmıyor") != std::string::npos);
    CHECK_EQ(none.doc.content_hash(), before);
}

TEST_CASE("C-06: YUVARLA ve PAH sayfalarının örnekleri yazıldığı gibi çalışır")
{
    // docs/komutlar/fillet.md and chamfer.md print these lines and these
    // answers; each example starts a new drawing (CLAUDE.md 11.6).
    const auto said = [](std::initializer_list<const char*> lines) {
        Rig r;
        for (const char* line : lines)
            r.run(line);
        return r.echoed;
    };
    CHECK(said({"ÇİZGİ 0,0 10,0", "ÇİZGİ 10,0 10,10",
                "YUVARLA nesne=1 2 nokta=5,0 ikinci_nokta=10,5 yaricap=2"})
              .find("İki nesne arasında köşe yuvarlatıldı (yarıçap 2,000 m).") !=
          std::string::npos);
    CHECK(said({"ÇİZGİ 0,0 8,0", "ÇİZGİ 10,2 10,10",
                "YUVARLA nesne=1 2 nokta=4,0 ikinci_nokta=10,6 yaricap=0"})
              .find("İki nesne keskin köşede buluştu.") != std::string::npos);
    if (kernel_available())
        CHECK(said({"ELİPS merkez=0,0 birinci=10,0 ikinci=0,5 baslangic=0 bitis=180",
                    "ÇİZGİ -15,3 15,3",
                    "YUVARLA nesne=1 2 nokta=9.6,1.4 ikinci_nokta=12,3 yaricap=1"})
                  .find("İki nesne arasında köşe yuvarlatıldı (yarıçap 1,000 m).") !=
              std::string::npos);
    CHECK(said({"ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10", "YUVARLA nesne=1 hepsi=evet yaricap=2"})
              .find("2 köşe yuvarlatıldı; çizgi tek bir yaylı çoklu çizgi oldu.") !=
          std::string::npos);
    CHECK(said({"ÇİZGİ 0,0 10,0", "ÇİZGİ 10,0 10,10",
                "PAH nesne=1 2 nokta=5,0 ikinci_nokta=10,5 mesafe=2 ikinci_mesafe=3"})
              .find("İki çizgi arasında pah kırıldı.") != std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,10 0,10", "PAH nesne=1 hepsi=evet mesafe=1"})
              .find("4 köşeye pah kırıldı.") != std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,10 0,10", "ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30",
                "YUVARLA nesne=1 2 hepsi=evet yaricap=2"})
              .find("6 köşe yuvarlatıldı (2 nesnede); 1 açık çizgi yaylı çoklu çizgi ve 1 alan "
                    "yaylı kenarlı alan oldu.") != std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,10 0,10", "YUVARLA nesne=1 hepsi=evet yaricap=2"})
              .find("4 köşe yuvarlatıldı; alan yaylı kenarlı bir alan oldu.") != std::string::npos);
    CHECK(said({"ALAN 0,0 40,0 40,30 0,30", "YUVARLA nesne=1 nokta=0,0 yaricap=5"})
              .find("Köşe yuvarlatıldı (yarıçap 5,000 m); alan yaylı kenarlı bir alan oldu.") !=
          std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,10 0,10", "ALAN 20,0 30,0 30,10 20,10",
                "PAH nesne=1 2 hepsi=evet mesafe=1"})
              .find("8 köşeye pah kırıldı (2 nesnede).") != std::string::npos);
}

TEST_CASE("C-06: Yuvarla — bütün köşeler aracı nesnenin herhangi bir yerine bir tıklamayla çalışır")
{
    // The column's entry runs `YUVARLA hepsi=evet`: the click names the whole
    // run, not a corner, so a click mid-edge must not start the two-object
    // mode — and what it makes is what the typed line makes.
    Rig clicked;
    clicked.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    auto started = clicked.bus.begin_interactive("YUVARLA hepsi=evet", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    CHECK(s.prompt().message.find("Bütün köşeleri") != std::string::npos);
    REQUIRE(s.supply(Value::point({5'000, 0})).ok()); ///< the middle of the first edge
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "yaricap");
    CHECK(s.prompt().rubber_shape == RubberShape::Corner);
    REQUIRE(s.supply(Value::number(2.0)).ok());
    REQUIRE(clicked.bus.finish(s).ok());

    Rig typed;
    typed.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    typed.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    CHECK(clicked.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());

    // Straight two-point lines have no corner between their own edges, and
    // the pair of them is not a chain: said, and nothing written.
    Rig two;
    two.run("ÇİZGİ 0,0 10,0");
    two.run("ÇİZGİ 10,0 10,10");
    auto refused = two.bus.execute_line("YUVARLA nesne=1 2 hepsi=evet yaricap=1", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("hiçbirinde işlenecek köşe yok") != std::string::npos);
}

TEST_CASE("C-06: bütün köşeler seçili bütün nesnelere birden uygulanır; uymayan sayılır")
{
    // Two parcels and a run selected, and a circle among them: every corner
    // of the three is cut in one step, the circle passed over and said.
    Rig selected;
    selected.run("ALAN 0,0 10,0 10,10 0,10");
    selected.run("ALAN 20,0 30,0 30,10 20,10");
    selected.run("ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30");
    selected.run("DAİRE merkez=40,5 cevre=45,5");
    selected.run("SEÇ HEPSİ");
    // As the column's button runs it: the body takes the selection.
    auto pressed = selected.bus.begin_interactive("PAH hepsi=evet mesafe=1", Origin::Gui);
    REQUIRE(pressed.ok());
    REQUIRE_FALSE(pressed.value()->waiting());
    REQUIRE(selected.bus.finish(*pressed.value()).ok());
    CHECK_MESSAGE(selected.echoed.find("10 köşeye pah kırıldı (3 nesnede).") != std::string::npos,
                  selected.echoed);
    CHECK(selected.echoed.find("1 nesne köşeli bir çizgi ya da alan olmadığı için olduğu gibi "
                               "kaldı") != std::string::npos);
    CHECK_EQ(selected.undo.undo_depth(), 5u); ///< four drawings and ONE cut

    // The same work named by key is the same drawing and the same journal line.
    Rig typed;
    typed.run("ALAN 0,0 10,0 10,10 0,10");
    typed.run("ALAN 20,0 30,0 30,10 20,10");
    typed.run("ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30");
    typed.run("DAİRE merkez=40,5 cevre=45,5");
    typed.run("PAH nesne=1 2 3 4 hepsi=evet mesafe=1");
    CHECK_EQ(selected.doc.content_hash(), typed.doc.content_hash());
    CHECK(selected.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());

    // Rounded, the open run becomes one arc polyline and the parcel an area
    // with arc edges — each still one object, its arcs true arcs (O-2).
    Rig round;
    round.run("ALAN 0,0 10,0 10,10 0,10");
    round.run("ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30");
    round.run("YUVARLA nesne=1 2 hepsi=evet yaricap=2");
    CHECK_MESSAGE(round.echoed.find("6 köşe yuvarlatıldı (2 nesnede); 1 açık çizgi yaylı çoklu "
                                    "çizgi ve 1 alan yaylı kenarlı alan oldu.") !=
                      std::string::npos,
                  round.echoed);
    const auto all = live(round.doc);
    REQUIRE_EQ(all.size(), 2u);
    CHECK(all[0].first == kArcPolylineKind);
    CHECK(all[0].second.closed);
    CHECK(all[1].first == kArcPolylineKind);

    // The preview names every object the click will cut.
    Rig shown;
    shown.run("ALAN 0,0 10,0 10,10 0,10");
    shown.run("ALAN 20,0 30,0 30,10 20,10");
    shown.run("SEÇ HEPSİ");
    auto started = shown.bus.begin_interactive("YUVARLA hepsi=evet", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    auto guide = decode_corner_preview(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().every);
    CHECK_EQ(guide.value().key, 1);
    CHECK(guide.value().also == std::vector<std::int64_t>{2});
    s.cancel();

    // Without `hepsi`, three objects make no corner, and it is said how to ask.
    Rig three;
    three.run("ÇİZGİ 0,0 10,0");
    three.run("ÇİZGİ 10,0 10,10");
    three.run("ÇİZGİ 10,10 0,10");
    auto many = three.bus.execute_line("YUVARLA nesne=1 2 3 yaricap=1", Origin::Test);
    REQUIRE_FALSE(many.ok());
    CHECK(many.error().message.find("hepsi=evet") != std::string::npos);
}

TEST_CASE("C-06: içbükey köşe de iki kenarına teğet yuvarlanır; pah girintiyi keser")
{
    // An L-shaped parcel: its corner at (10, 10) turns inward. The fillet's
    // centre is out in the notch, the radius from both edges, and the parcel
    // GAINS the corner's r² − πr²/4; a chamfer there gains the triangle it cuts.
    Rig round;
    round.run("ALAN 0,0 20,0 20,10 10,10 10,20 0,20");
    const EntityId e = round.doc.slot_of(EntityKey{1});
    const double before =
        std::abs(static_cast<double>(round.doc.geometry().area_of(round.doc.entities().slot[e])));
    round.run("YUVARLA nesne=1 nokta=10,10 yaricap=2");
    const EntityId after_e = round.doc.slot_of(EntityKey{1});
    REQUIRE(after_e != kNoEntity);
    const auto path = path_of(round.doc, after_e);
    REQUIRE(path);
    bool on_a = false;
    bool on_b = false;
    for (const PathPiece& p : path->pieces) {
        on_a = on_a || p.from == Point2{12'000, 10'000} || p.to == Point2{12'000, 10'000};
        on_b = on_b || p.from == Point2{10'000, 12'000} || p.to == Point2{10'000, 12'000};
        // The arc's centre is out in the notch, the radius from both edges.
        if (p.kind == PathPiece::Kind::Arc) {
            CHECK(p.centre == Point2{12'000, 12'000});
            CHECK_EQ(p.radius, Mm{2'000});
        }
    }
    CHECK(on_a);
    CHECK(on_b);
    // THE AREA IS THE ARC'S (`entity_area`, the kind's own): exact, not the
    // chords' — the parcel gains r² − πr²/4.
    const double rounded = std::abs(static_cast<double>(round.doc.entity_area(after_e)));
    const double gained  = (rounded - before) / 1.0e6;
    CHECK(std::abs(gained - (4.0 - std::acos(-1.0))) < 1e-5);

    Rig cut;
    cut.run("ALAN 0,0 20,0 20,10 10,10 10,20 0,20");
    cut.run("PAH nesne=1 nokta=10,10 mesafe=2");
    const EntityId c = cut.doc.slot_of(EntityKey{1});
    const double chamfered =
        std::abs(static_cast<double>(cut.doc.geometry().area_of(cut.doc.entities().slot[c])));
    CHECK(std::abs((chamfered - before) / 1.0e6 - 2.0) < 1.0e-6);
}

TEST_CASE("C-06: önizleme ve çıktı aynıdır — iki nesne arasında ve bütün köşelerde")
{
    // What the canvas draws is computed from the prompt's payload by the same
    // core call; the objects the click leaves must be exactly that.
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    r.run("ÇİZGİ -5,5 15,5");
    auto started = r.bus.begin_interactive("YUVARLA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point({7'000, 5'000})).ok()); ///< the line, inside the circle
    REQUIRE(s.supply(Value::point({9'600, 2'800})).ok()); ///< exactly on the arc, below the line
    REQUIRE(s.waiting());
    auto guide = decode_pair_corner_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    const auto slot_of = [&r](std::int64_t key) {
        return r.doc.slot_of(static_cast<EntityKey>(static_cast<std::uint64_t>(key)));
    };
    const auto pa = path_of(r.doc, slot_of(guide.value().key_a));
    const auto pb = path_of(r.doc, slot_of(guide.value().key_b));
    REQUIRE(pa);
    REQUIRE(pb);
    auto shown = fillet_pair(*pa, guide.value().pick_a, *pb, guide.value().pick_b, Mm{1'500});
    REQUIRE(shown.ok());
    REQUIRE(s.supply(Value::number(1.5)).ok());
    REQUIRE(r.bus.finish(s).ok());
    const auto made_a = path_of(r.doc, slot_of(guide.value().key_a));
    const auto made_b = path_of(r.doc, slot_of(guide.value().key_b));
    REQUIRE(made_a);
    REQUIRE(made_b);
    CHECK(path_vertices(*made_a) == path_vertices(shown.value().a));
    CHECK(path_vertices(*made_b) == path_vertices(shown.value().b));
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    REQUIRE_EQ(all[2].second.pieces.size(), 1u);
    // The same arc; an arc object is stored counter-clockwise, so its ends may
    // come back the other way round from the link the preview walked.
    const PathPiece& made = all[2].second.pieces[0];
    const PathPiece& link = shown.value().link;
    CHECK(made.centre == link.centre);
    CHECK_EQ(made.radius, link.radius);
    CHECK(((made.from == link.from && made.to == link.to) ||
           (made.from == link.to && made.to == link.from)));

    // Every corner: the run the preview draws is the run written.
    Rig run;
    run.run("ÇOKLUÇİZGİ 0,0 20,0 20,12 32,12 32,0 44,0");
    auto every = run.bus.begin_interactive("YUVARLA hepsi=evet", Origin::Gui);
    REQUIRE(every.ok());
    Session& e = *every.value();
    REQUIRE(e.supply(Value::point({10'000, 0})).ok());
    auto corner = decode_corner_preview(e.prompt().rubber_payload);
    REQUIRE(corner.ok());
    CHECK(corner.value().every);
    const auto before = path_of(run.doc, run.doc.slot_of(EntityKey{1}));
    REQUIRE(before);
    const PathCorners drawn = cut_every_path_corner(*before, Mm{3'000}, true);
    REQUIRE_EQ(drawn.cut, 4u);
    REQUIRE(e.supply(Value::number(3.0)).ok());
    REQUIRE(run.bus.finish(e).ok());
    const auto written = live(run.doc);
    REQUIRE_EQ(written.size(), 1u);
    CHECK(path_vertices(written[0].second) == path_vertices(drawn.path));
    REQUIRE_EQ(written[0].second.pieces.size(), drawn.path.pieces.size());
    for (std::size_t i = 0; i < drawn.path.pieces.size(); ++i)
        CHECK(written[0].second.pieces[i].centre == drawn.path.pieces[i].centre);
}

TEST_CASE("C-06: ekransız bir istemcinin yayın tam üstüne tıklaması yayı bulur")
{
    // A pick radius of zero — a script, a test, an agent — picks what lies
    // exactly under the point. On a curve that is the curve, not the chords
    // it is drawn with: (9.6, 2.8) is on the circle of radius 10, and a few
    // millimetres outside the 16-sided outline drawn for it.
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    r.run("DAİRE merkez=30,0 cevre=40,0");
    CHECK(pick_nearest(r.doc, {9'600, 2'800}, 0) == r.doc.slot_of(EntityKey{1}));
    CHECK(pick_nearest(r.doc, {39'600, 2'800}, 0) == r.doc.slot_of(EntityKey{2}));
    CHECK(pick_nearest(r.doc, {9'600, 2'900}, 0) == kNoEntity); ///< 1 cm off it is off it

    // An ellipse and a spline too (O-5): (69,6; 1,4) is on the ellipse about
    // (60, 0) with axes 10 and 5 m — 9,6² + 4·1,4² = 10² — and the quadratic
    // curve through (90,0) (95,10) (100,0) at its middle, (95, 5).
    r.run("ELİPS merkez=60,0 birinci=70,0 ikinci=60,5");
    r.run("SPLINE noktalar=90,0 95,10 100,0 derece=2");
    CHECK(pick_nearest(r.doc, {69'600, 1'400}, 0) == r.doc.slot_of(EntityKey{3}));
    CHECK(pick_nearest(r.doc, {95'000, 5'000}, 0) == r.doc.slot_of(EntityKey{4}));
    CHECK(pick_nearest(r.doc, {69'600, 1'410}, 0) == kNoEntity);
}

// =============================================================================
// O-2: a rounded corner is an arc, and a rounded parcel is still a parcel
// =============================================================================

TEST_CASE("O-2: yuvarlanmış parselin öteki köşesi de yuvarlanıyor; iki yay da gerçek yay")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2"); // an arc polyline now
    r.run("YUVARLA nesne=1 nokta=0,0 yaricap=3");   // its corner too
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 1u);
    CHECK(all[0].first == kArcPolylineKind);
    CHECK(all[0].second.closed);
    std::vector<std::pair<Point2, Mm>> arcs;
    for (const PathPiece& p : all[0].second.pieces)
        if (p.kind == PathPiece::Kind::Arc) arcs.emplace_back(p.centre, p.radius);
    REQUIRE_EQ(arcs.size(), 2u);
    CHECK(std::find(arcs.begin(), arcs.end(), std::pair{Point2{18'000, 8'000}, Mm{2'000}}) !=
          arcs.end());
    CHECK(std::find(arcs.begin(), arcs.end(), std::pair{Point2{3'000, 3'000}, Mm{3'000}}) !=
          arcs.end());
    // EXACT AREA: 200 m² less both corners' r² − πr²/4.
    const double pi       = std::acos(-1.0);
    const double expected = 200.0 - (4.0 - pi) - (9.0 - pi * 9.0 / 4.0);
    const EntityId e      = r.doc.slot_of(EntityKey{1});
    CHECK(std::abs(std::abs(static_cast<double>(r.doc.entity_area(e))) / 1.0e6 - expected) < 1e-5);
    // AND IT IS STILL THE PARCEL: one key, two undo steps back to the square.
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());
    const auto back = live(r.doc);
    REQUIRE_EQ(back.size(), 1u);
    CHECK(back[0].first == kPolylineKind);
    CHECK(path_vertices(back[0].second).size() == 5u); // four corners, the seam repeated
}

TEST_CASE("O-2: yay kenarı ile düz kenarın köşesi ikisine teğet yayla yuvarlanıyor")
{
    // A parcel whose south edge bends out through (10, −3): the corner at
    // (20, 0) is between that arc and the straight east edge.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3");
    const auto bent = live(r.doc);
    REQUIRE_EQ(bent.size(), 1u);
    PathPiece edge;
    for (const PathPiece& p : bent[0].second.pieces)
        if (p.kind == PathPiece::Kind::Arc) edge = p;
    REQUIRE(edge.radius > 0);

    r.run("YUVARLA nesne=1 nokta=20,0 yaricap=1");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 1u);
    const CurvePath& face = all[0].second;
    PathPiece fillet;
    std::size_t arcs = 0;
    for (const PathPiece& p : face.pieces)
        if (p.kind == PathPiece::Kind::Arc) {
            ++arcs;
            if (p.radius == 1'000) fillet = p;
        }
    CHECK_EQ(arcs, 2u); // the bent edge, shortened, and the fillet
    REQUIRE_EQ(fillet.radius, Mm{1'000});
    // TANGENT TO BOTH: the east edge's line x = 20 is one radius from the centre,
    // and the bent edge's circle is its radius plus or minus one.
    CHECK(std::abs(static_cast<double>(20'000 - fillet.centre.x) - 1'000.0) <= 1.0);
    const double to_edge = gap(fillet.centre, edge.centre);
    CHECK((std::abs(to_edge - static_cast<double>(edge.radius - 1'000)) <= 1.5 ||
           std::abs(to_edge - static_cast<double>(edge.radius + 1'000)) <= 1.5));
    // The bent edge kept its circle.
    for (const PathPiece& p : face.pieces)
        if (p.kind == PathPiece::Kind::Arc && p.radius != 1'000) {
            CHECK(p.centre == edge.centre);
            CHECK_EQ(p.radius, edge.radius);
        }
    // The path runs on without a gap.
    for (std::size_t i = 0; i < face.pieces.size(); ++i)
        CHECK(face.pieces[i].to == face.pieces[(i + 1) % face.pieces.size()].from);
}

TEST_CASE("O-2: yay kenarının yanında PAH sebebini söyleyerek reddediliyor; teğet köşe köşe değil")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3");
    const std::uint64_t before = r.doc.content_hash();
    auto chamfer = r.bus.execute_line("PAH nesne=1 nokta=20,0 mesafe=1", Origin::Test);
    REQUIRE_FALSE(chamfer.ok());
    CHECK(chamfer.error().message.find("bir kenarı yay") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), before);

    // A tangent join — where a fillet ran into its edge — is no corner: every
    // corner of a rounded square rounded again finds only the four arcs' ends,
    // and passes over all eight.
    Rig again;
    again.run("ALAN 0,0 10,0 10,10 0,10");
    again.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    const std::uint64_t rounded = again.doc.content_hash();
    auto twice = again.bus.execute_line("YUVARLA nesne=1 hepsi=evet yaricap=1", Origin::Test);
    REQUIRE_FALSE(twice.ok());
    CHECK(twice.error().message.find("hiçbir köşeye sığmıyor") != std::string::npos);
    CHECK_EQ(again.doc.content_hash(), rounded);
}

TEST_CASE("O-2: yuvarlanan parselin kimliği ve özniteliği kalıyor, tek geri alma adımı")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("SÜTUN kimlik=ada_no tur=metin");
    r.run("ÖZNİTELİK ad=ada_no nesne=1 deger=1284");
    const std::size_t before = r.doc.live_entity_count();
    const std::size_t steps  = r.undo.undo_depth();
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    CHECK_EQ(r.doc.live_entity_count(), before);
    const EntityId e = r.doc.slot_of(EntityKey{1});
    REQUIRE(e != kNoEntity);
    CHECK(r.doc.entities().kind[e] == kArcPolylineKind);
    bool kept = false;
    for (std::size_t c = 0; c < r.doc.attributes().columns(); ++c) {
        auto v = r.doc.attribute(static_cast<AttrId>(c), e);
        kept   = kept || (v && v.value().present);
    }
    CHECK(kept);
    CHECK_EQ(r.undo.undo_depth(), steps + 1); // ONE step for the rounding
}

// =============================================================================
// O-5: an ellipse or a spline on one side of the corner — the kernel's fillet
// =============================================================================

#include "piricad/core/spline.hpp"
#include "piricad/script/json_runner.hpp"

namespace {

/// The upper half of the ellipse x²/10² + y²/5² = 1 (metres), walked from
/// (10, 0) to (−10, 0).
CurvePath upper_ellipse()
{
    PathPiece p;
    p.kind       = PathPiece::Kind::Ellipse;
    p.centre     = {0, 0};
    p.major      = {10'000, 0};
    p.minor      = {0, 5'000};
    p.start_udeg = 0;
    p.sweep_udeg = 180'000'000;
    p.from       = {10'000, 0};
    p.to         = {-10'000, 0};
    CurvePath out;
    out.pieces.push_back(p);
    return out;
}

/// The quadratic Bézier (0,0) (10,10) (20,0) m: the parabola y = x·(1 − x/20).
CurvePath parabola()
{
    PathPiece p;
    p.kind              = PathPiece::Kind::Spline;
    p.controls          = {{0, 0}, {10'000, 10'000}, {20'000, 0}};
    p.spline.degree     = 2;
    p.spline.knots_nano = uniform_clamped_knots(3, 2);
    p.from              = {0, 0};
    p.to                = {20'000, 0};
    CurvePath out;
    out.pieces.push_back(p);
    return out;
}

/// How far `p` is off the ellipse, as the residual of its equation.
double off_ellipse(Point2 p)
{
    const double x = static_cast<double>(p.x) / 10'000.0;
    const double y = static_cast<double>(p.y) / 5'000.0;
    return std::abs(x * x + y * y - 1.0);
}

} // namespace

TEST_CASE("O-5: elips ile çizgi arasındaki yuvarlama ikisine teğet, elips elips kalır")
{
    if (!kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    // y = 3 m crosses the ellipse at x = ±8 m exactly (64/100 + 9/25 = 1). The
    // line's right part and the ellipse below the crossing name the corner
    // outside the ellipse and under the line.
    const CurvePath road = line({-15'000, 3'000}, {15'000, 3'000});
    auto f = fillet_pair(road, {12'000, 3'000}, upper_ellipse(), {9'539, 1'500}, 1'000);
    REQUIRE_MESSAGE(f.ok(), (f.ok() ? std::string() : f.error().message));
    const PairCorner& c = f.value();
    // Under the line by the radius, outside the ellipse, right of the crossing.
    CHECK_EQ(c.centre.y, 2'000);
    CHECK(c.centre.x > 8'000);
    CHECK(off_ellipse(c.centre) > 0.0);
    // Tangent: both touches at the radius, the ellipse's ON the ellipse, and
    // the radius there along the ellipse's normal (x/a², y/b²).
    CHECK(std::abs(gap(c.centre, c.on_a) - 1'000.0) <= 1.5);
    CHECK(std::abs(gap(c.centre, c.on_b) - 1'000.0) <= 1.5);
    CHECK(c.on_a == (Point2{c.centre.x, 3'000}));
    CHECK(off_ellipse(c.on_b) <= 2e-4);
    const double nx    = static_cast<double>(c.on_b.x) / 1e8;
    const double ny    = static_cast<double>(c.on_b.y) / 25e6;
    const double rx    = static_cast<double>(c.centre.x - c.on_b.x);
    const double ry    = static_cast<double>(c.centre.y - c.on_b.y);
    const double cross = (nx * ry - ny * rx) / std::sqrt(nx * nx + ny * ny) / 1'000.0;
    CHECK(std::abs(cross) <= 2e-3); ///< the radius is the normal, to a couple of millimetres
    // The line keeps its picked right part from the touch; the ellipse its
    // part below the crossing, cut at its touch and still an ellipse.
    CHECK(path_vertices(c.a) == std::vector<Point2>{c.on_a, {15'000, 3'000}});
    REQUIRE_EQ(c.b.pieces.size(), 1u);
    CHECK(c.b.pieces[0].kind == PathPiece::Kind::Ellipse);
    CHECK(c.b.pieces[0].from == (Point2{10'000, 0}));
    CHECK(c.b.pieces[0].to == c.on_b);
    REQUIRE(c.has_link);
    CHECK_EQ(c.link.radius, Mm{1'000});
    CHECK(c.link.from == c.on_a);
    CHECK(c.link.to == c.on_b);

    // The same corner asked again is the same millimetres (§7.3).
    auto again = fillet_pair(road, {12'000, 3'000}, upper_ellipse(), {9'539, 1'500}, 1'000);
    REQUIRE(again.ok());
    CHECK(again.value().centre == c.centre);
    CHECK(again.value().b == c.b);
}

TEST_CASE("O-5: elipsin öbür köşesi seçilince yay oraya gider; sıfır yarıçap tam kesişim")
{
    if (!kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const CurvePath road = line({-15'000, 3'000}, {15'000, 3'000});
    // The line's LEFT part and the ellipse above the crossing on the right:
    // the corner inside the ellipse, over the line.
    auto inside = fillet_pair(road, {4'000, 3'000}, upper_ellipse(), {6'000, 4'000}, 1'000);
    REQUIRE_MESSAGE(inside.ok(), (inside.ok() ? std::string() : inside.error().message));
    CHECK_EQ(inside.value().centre.y, 4'000);
    CHECK(inside.value().centre.x < 8'000);
    CHECK(path_vertices(inside.value().a) ==
          std::vector<Point2>{{-15'000, 3'000}, inside.value().on_a});
    CHECK(inside.value().b.pieces[0].from == inside.value().on_b);
    CHECK(inside.value().b.pieces[0].to == (Point2{-10'000, 0}));

    // Zero: the sharp corner at (8, 3) m, the ellipse cut there.
    auto sharp = fillet_pair(road, {12'000, 3'000}, upper_ellipse(), {9'539, 1'500}, 0);
    REQUIRE_MESSAGE(sharp.ok(), (sharp.ok() ? std::string() : sharp.error().message));
    CHECK(sharp.value().on_a == (Point2{8'000, 3'000}));
    CHECK(sharp.value().on_b == (Point2{8'000, 3'000}));
    CHECK_FALSE(sharp.value().has_link);
    CHECK(path_vertices(sharp.value().a) == std::vector<Point2>{{8'000, 3'000}, {15'000, 3'000}});
    CHECK(sharp.value().b.pieces[0].to == (Point2{8'000, 3'000}));
}

TEST_CASE("O-5: spline ile çizgi arasında yuvarlama; kısa çizgi köşeye uzar")
{
    if (!kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    // y = 3 m meets the parabola at x = 10 − √40 m (≈ 3,675). The line stops
    // short at x = 2 m, and is carried on to its touch.
    const CurvePath road = line({-10'000, 3'000}, {2'000, 3'000});
    auto f               = fillet_pair(road, {-5'000, 3'000}, parabola(), {1'000, 950}, 500);
    REQUIRE_MESSAGE(f.ok(), (f.ok() ? std::string() : f.error().message));
    const PairCorner& c = f.value();
    CHECK_EQ(c.centre.y, 2'500); ///< under the line by the radius
    CHECK(c.on_a == (Point2{c.centre.x, 3'000}));
    CHECK(c.on_a.x > 2'000); ///< carried past its end
    CHECK(path_vertices(c.a) == std::vector<Point2>{{-10'000, 3'000}, c.on_a});
    // The spline's touch lies on y = x(1 − x/20), at the radius, along its normal.
    const double x = static_cast<double>(c.on_b.x);
    CHECK(std::abs(static_cast<double>(c.on_b.y) - x * (1.0 - x / 20'000.0)) <= 1.5);
    CHECK(std::abs(gap(c.centre, c.on_b) - 500.0) <= 1.5);
    const double tx  = 1.0;
    const double ty  = 1.0 - x / 10'000.0; ///< dy/dx
    const double dot = (tx * static_cast<double>(c.centre.x - c.on_b.x) +
                        ty * static_cast<double>(c.centre.y - c.on_b.y)) /
                       std::sqrt(tx * tx + ty * ty);
    CHECK(std::abs(dot) <= 1.5);
    // The spline keeps its start, cut at the touch, and is still a spline.
    REQUIRE_EQ(c.b.pieces.size(), 1u);
    CHECK(c.b.pieces[0].kind == PathPiece::Kind::Spline);
    CHECK(c.b.pieces[0].from == (Point2{0, 0}));
    CHECK(c.b.pieces[0].to == c.on_b);
}

TEST_CASE("O-5: PAH elipsle köşede sebebini söyleyerek reddediyor")
{
    const CurvePath road = line({-15'000, 3'000}, {15'000, 3'000});
    auto f = chamfer_pair(road, {12'000, 3'000}, upper_ellipse(), {9'539, 1'500}, 1'000, 1'000);
    REQUIRE_FALSE(f.ok());
    CHECK(f.error().message.find("elips") != std::string::npos);
}

TEST_CASE("O-5: YUVARLA elips ile çizgi arasında — komut satırı, günlük tekrarı ve geri alma")
{
    if (!kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.run("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5 baslangic=0 bitis=180");
    r.run("ÇOKLUÇİZGİ -15,3 15,3");
    const std::uint64_t before = r.doc.content_hash();
    r.run("YUVARLA nesne=1 2 nokta=9.539,1.5 ikinci_nokta=12,3 yaricap=1");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), 3u);
    CHECK(all[0].first == kEllipseKind); ///< cut back, still an ellipse
    REQUIRE_EQ(all[0].second.pieces.size(), 1u);
    CHECK(all[0].second.pieces[0].from == (Point2{10'000, 0}));
    CHECK(off_ellipse(all[0].second.pieces[0].to) <= 2e-4);
    CHECK(all[2].first == kArcKind);
    const PathPiece& fillet = all[2].second.pieces[0];
    CHECK_EQ(fillet.radius, Mm{1'000});
    CHECK_EQ(fillet.centre.y, 2'000);
    // The fillet meets both cut objects exactly.
    const auto ends     = [](const PathPiece& p) { return std::vector<Point2>{p.from, p.to}; };
    const auto arc_ends = ends(fillet);
    CHECK(std::ranges::find(arc_ends, all[0].second.pieces[0].to) != arc_ends.end());
    CHECK(std::ranges::find(arc_ends, all[1].second.pieces[0].from) != arc_ends.end());

    // The journal replays to the same document.
    Rig replay;
    for (const auto& e : r.journal.entries()) {
        auto again = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        REQUIRE(again.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), r.doc.content_hash());

    // One undo step brings the two objects back whole.
    r.run("GERİAL");
    CHECK_EQ(r.doc.content_hash(), before);
}

TEST_CASE("O-5: YUVARLA elips ile çizgi — tıklayarak, yazarak ve betikten aynı çizim, aynı günlük")
{
    if (!kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const auto drawn = [](Rig& r) {
        r.run("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5 baslangic=0 bitis=180");
        r.run("ÇOKLUÇİZGİ -15,3 15,3");
    };

    Rig gui;
    drawn(gui);
    auto started = gui.bus.begin_interactive("YUVARLA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point({9'600, 1'400})).ok()); ///< ON the ellipse (9,6² + 4·1,4² = 10²)
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "ikinci_nokta");
    REQUIRE(s.supply(Value::point({12'000, 3'000})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::PairCorner);
    CHECK(s.prompt().rubber_origin == (Point2{8'000, 3'000})); ///< where the two meet, exactly
    REQUIRE(s.supply(Value::number(1.0)).ok());
    auto done = gui.bus.finish(s);
    REQUIRE_MESSAGE(done.ok(), (done.ok() ? std::string() : done.error().message));

    Rig cli;
    drawn(cli);
    cli.run("YUVARLA nesne=1 2 nokta=9.6,1.4 ikinci_nokta=12,3 yaricap=1");

    Rig scr;
    drawn(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto ran = runner.run_text(R"({
            "ad": "Elipsle yuvarlama kanıtı",
            "komutlar": [
                {"cmd": "core.fillet",
                 "args": {"nesne": [1, 2], "nokta": [9600, 1400], "ikinci_nokta": [12000, 3000],
                          "yaricap": 1}}
            ]
        })");
        REQUIRE_MESSAGE(ran.ok(), (ran.ok() ? std::string() : ran.error().message));
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    const auto last = [](const Journal& j) { return j.entries().back(); };
    CHECK(last(gui.journal).args == last(cli.journal).args);
    CHECK(last(cli.journal).args == last(scr.journal).args);
}
