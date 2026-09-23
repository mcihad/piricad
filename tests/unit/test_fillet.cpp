// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE CORNER BETWEEN TWO OBJECTS (TODOS C-06).
//
// A fillet is right when its centre is the radius away from both objects, its
// tangent points lie on both, and it sits in the corner the picks name — never
// the one across from it. Every expected value below is a closed form a
// surveyor can check by hand, asserted to the millimetre rounding allows.
#include "kentos_test.hpp"

#include "kentos_cad/core/corner.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/fillet.hpp"
#include "kentos_cad/core/pick.hpp"

#include <cmath>
#include <vector>

using namespace kentos::core;

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

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/document.hpp"

namespace {

using namespace kentos;
using namespace kentos::command;

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
        auto p = path_of(doc, e);
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
    CHECK_EQ(r.doc.live_entity_count(), 3u); ///< two legs and the arc, as the corner tool makes
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
              .find("6 köşe yuvarlatıldı (2 nesnede); 1 açık çizgi yaylı çoklu çizgi oldu.") !=
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

    // Rounded, the open run becomes one arc-polyline and the parcels stay faces.
    Rig round;
    round.run("ALAN 0,0 10,0 10,10 0,10");
    round.run("ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30");
    round.run("YUVARLA nesne=1 2 hepsi=evet yaricap=2");
    CHECK_MESSAGE(round.echoed.find("6 köşe yuvarlatıldı (2 nesnede); 1 açık çizgi yaylı çoklu "
                                    "çizgi oldu.") != std::string::npos,
                  round.echoed);
    const auto all = live(round.doc);
    REQUIRE_EQ(all.size(), 2u);
    CHECK(all[0].first == kPolylineKind);
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
    const std::vector<Point2> ring = path_vertices(*path);
    bool on_a                      = false;
    bool on_b                      = false;
    for (const Point2 p : ring) {
        on_a = on_a || p == Point2{12'000, 10'000};
        on_b = on_b || p == Point2{10'000, 12'000};
        // Every point of the drawn arc is the radius from the centre in the notch.
        if (p.x >= 10'000 && p.x <= 12'000 && p.y >= 10'000 && p.y <= 12'000)
            CHECK(std::abs(gap(p, {12'000, 12'000}) - 2'000.0) <= 1.5);
    }
    CHECK(on_a);
    CHECK(on_b);
    const double rounded = std::abs(
        static_cast<double>(round.doc.geometry().area_of(round.doc.entities().slot[after_e])));
    const double gained = (rounded - before) / 1.0e6;
    CHECK(std::abs(gained - (4.0 - 3.14159265358979)) < 0.05);

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
    const CornerRun drawn = cut_every_corner(path_vertices(*before), false, Mm{3'000}, true);
    REQUIRE(drawn.bent);
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
}
