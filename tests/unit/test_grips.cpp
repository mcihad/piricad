// SPDX-License-Identifier: GPL-3.0-or-later
//
// GRIPS THAT MEAN WHAT THEY SAY (TODOS C-07).
//
// A handle promises a shape: a circle's quadrant handle a radius and still a
// circle, an arc's end that end and the other one where it was, a spline's
// handle a point the curve is drawn from. Each case below holds a kind to the
// meaning `core/grips.hpp` states, by the numbers a surveyor can check.
#include "kentos_test.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/spline.hpp"

#include <cmath>
#include <vector>

using namespace kentos::core;

namespace {

double gap(Point2 a, Point2 b)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

TEST_CASE("C-07: dairenin yarıçap tutamağı daireyi bozmadan yarıçapı kurar")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    auto made         = doc.add_circle(lyr, Point2{10'000, 10'000}, 5'000, undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    // Every quadrant handle sets the radius to its distance from the centre;
    // the centre stays, and the record stays a circle.
    for (std::size_t handle = 1; handle <= 4; ++handle) {
        auto edit = move_grip(doc, e, handle, Point2{10'000 + 3'000, 10'000 + 4'000});
        REQUIRE(edit.ok());
        REQUIRE_EQ(edit.value().points.size(), 1u);
        CHECK_EQ(edit.value().points[0][0], (Point2{10'000, 10'000}));
        CHECK_EQ(edit.value().points[0][1], (Point2{15'000, 10'000})); ///< radius 5 m, exact
    }
    auto wider = move_grip(doc, e, 2, Point2{10'000, 17'000});
    REQUIRE(wider.ok());
    CHECK_EQ(wider.value().points[0][1], (Point2{17'000, 10'000}));
    CHECK_EQ(doc.entities().kind[e], kCircleKind);

    // A handle dropped on the centre would make no circle, and says so.
    CHECK_FALSE(move_grip(doc, e, 1, Point2{10'000, 10'000}).ok());
}

TEST_CASE(
    "C-07: yayın ortası çekilince iki ucu yerinde kalır; kirişin öbür yanına geçince yön döner")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    // A half circle over the chord (−10, 0)–(10, 0), bulging north.
    auto made = doc.add_arc(lyr, Point2{0, 0}, 10'000, Point2{10'000, 0}, Point2{-10'000, 0}, undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    // Pulled in to 5 m: flatter, through the handle, ends held.
    auto flatter = move_grip(doc, e, 3, Point2{0, 5'000});
    REQUIRE(flatter.ok());
    const std::vector<Point2>& v = flatter.value().points[0];
    CHECK_EQ(v[2], (Point2{10'000, 0}));
    CHECK_EQ(v[3], (Point2{-10'000, 0}));
    CHECK_EQ(v[0], (Point2{0, -7'500})); ///< (10² − 5²) / (2·5) below the chord
    CHECK_EQ(v[1].x - v[0].x, Mm{12'500});

    // Pulled through to the south: the arc now runs the other way round, and
    // is stored counter-clockwise from the other end — the same two ends.
    auto flipped = move_grip(doc, e, 3, Point2{0, -5'000});
    REQUIRE(flipped.ok());
    const std::vector<Point2>& w = flipped.value().points[0];
    CHECK_EQ(w[2], (Point2{-10'000, 0}));
    CHECK_EQ(w[3], (Point2{10'000, 0}));
    CHECK_EQ(w[0], (Point2{0, 7'500}));
    const Point2 mid = arc_midpoint(w[0], w[1].x - w[0].x, w[2], w[3]);
    CHECK(gap(mid, {0, -5'000}) <= 1.5);
}

TEST_CASE("C-07: ESNET bütün yayı pencereye alınca yay kaymadan yalnız taşınır")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    auto made         = doc.add_arc(lyr, Point2{3'217, 1'099}, 7'313, Point2{10'530, 1'099},
                                    Point2{3'217, 8'412}, undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();
    auto moved       = stretch_entity(doc, e, Box2{-10'000, -10'000, 20'000, 20'000}, 1'234, -567);
    REQUIRE(moved.ok());
    REQUIRE(moved.value().has_value());
    const std::vector<Point2>& v = moved.value()->edit.points[0];
    CHECK_EQ(v[0], (Point2{3'217 + 1'234, 1'099 - 567}));
    CHECK_EQ(v[1], (Point2{3'217 + 7'313 + 1'234, 1'099 - 567}));
    CHECK_EQ(v[2], (Point2{10'530 + 1'234, 1'099 - 567}));
    CHECK_EQ(v[3], (Point2{3'217 + 1'234, 8'412 - 567}));
}

TEST_CASE(
    "C-07: spline tutamakları kontrol noktalarıdır; biri taşınınca eski uydurma noktaları düşer")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> controls{{0, 0}, {10'000, 10'000}, {20'000, -10'000}, {30'000, 0}};
    const std::vector<Point2> fits{{0, 0}, {15'000, 1'000}, {30'000, 0}};
    SplineDef def;
    def.degree     = 3;
    def.has_fit    = true;
    def.knots_nano = uniform_clamped_knots(controls.size(), 3);
    auto made      = doc.add_kind(lyr, kSplineKind,
                                  std::vector{RingGeometry::RingInput{controls, RingRole::Open, 0},
                                         RingGeometry::RingInput{fits, RingRole::Open, 0}},
                                  encode_spline(def), undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    // Four handles, one per control point — none on the fit points, which the
    // curve is not drawn from.
    const auto grips = entity_grips(doc, e);
    REQUIRE_EQ(grips.size(), 4u);
    for (std::size_t i = 0; i < grips.size(); ++i) {
        CHECK_EQ(grips[i].role, GripRole::Control);
        CHECK_EQ(grips[i].at, controls[i]);
    }

    // The second control point moves; the fit points no longer describe the
    // curve, so the record drops them and says it has none.
    auto edit = move_grip(doc, e, 1, Point2{10'000, 20'000});
    REQUIRE(edit.ok());
    REQUIRE_EQ(edit.value().points.size(), 1u);
    CHECK_EQ(edit.value().points[0][1], (Point2{10'000, 20'000}));
    auto after = decode_spline(edit.value().payload);
    REQUIRE(after.ok());
    CHECK_FALSE(after.value().has_fit);
    CHECK_EQ(after.value().knots_nano, def.knots_nano);

    // And the document takes the edit: one ring, the curve through the new point's pull.
    Op write;
    const auto inputs = edit.value().inputs();
    REQUIRE(doc.set_kind_geometry(e, inputs, edit.value().payload, write).ok());
    CHECK_EQ(doc.geometry().rings_of(doc.entities().slot[e]).count, 1u);
}

// =============================================================================
// ESNET: what it passes over, by reason
// =============================================================================

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/processing/registry.hpp"

TEST_CASE("C-07: ESNET kilitli katmandaki nesneyi atlar ve sebebiyle söyler")
{
    using namespace kentos::command;
    Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    register_builtin_commands(reg);
    std::string said;
    bus.on_echo    = [&said](std::string_view s) { said.append(s).append("\n"); };
    const auto run = [&bus](const std::string& line) {
        return bus.execute_line(line, Origin::Test);
    };

    REQUIRE(run("KATMAN ad=YOL").ok());
    REQUIRE(run("ÇOKLUÇİZGİ 0,0 10,0 10,10").ok());
    REQUIRE(run("KATMAN ad=TAPU").ok());
    REQUIRE(run("ÇOKLUÇİZGİ 0,1 10,1 10,11").ok());
    REQUIRE(run("KATMAN ad=TAPU kilitli=evet").ok());

    REQUIRE(run("ESNET pencere=8,-2 pencere=12,12 baslangic=0,0 bitis=2,0").ok());
    CHECK_MESSAGE(said.find("köşe esnetildi (1 nesne), 1 nesne kilitli katmanda atlandı.") !=
                      std::string::npos,
                  said);

    // Only the locked one under the window: refused, the reason in the refusal.
    auto refused = run("ESNET pencere=-1,0.5 pencere=1,1.5 baslangic=0,1 bitis=0,2");
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("1 nesne kilitli katmanda atlandı") != std::string::npos);
}

// =============================================================================
// KÖŞETAŞI at a shared corner
// =============================================================================

namespace {

struct Rig
{
    kentos::command::Registry reg;
    Document doc;
    kentos::command::Journal journal;
    kentos::command::UndoStack undo;
    kentos::command::Bus bus{doc, reg, journal, undo};
    std::string said;

    Rig()
    {
        kentos::command::register_builtin_commands(reg);
        kentos::processing::register_processing_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, kentos::command::Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    std::vector<Point2> ring(std::int64_t key) const
    {
        const EntityId e    = doc.slot_of(static_cast<EntityKey>(static_cast<std::uint64_t>(key)));
        const RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
        const auto xs       = doc.geometry().ring_xs(span.first);
        const auto ys       = doc.geometry().ring_ys(span.first);
        std::vector<Point2> out;
        for (std::size_t v = 0; v < xs.size(); ++v)
            out.push_back(Point2{xs[v], ys[v]});
        return out;
    }
};

} // namespace

TEST_CASE("C-07: iki parselin ortak köşesi birlikte taşınır; ara açılmaz")
{
    using namespace kentos::command;
    // Two parcels side by side sharing the edge x = 10.
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    r.run("ALAN 10,0 20,0 20,10 10,10");
    r.run("KÖŞETAŞI nesne=1 2 kaynak=10,10 nokta=11,12");
    CHECK(r.ring(1)[2] == (Point2{11'000, 12'000}));
    CHECK(r.ring(2)[3] == (Point2{11'000, 12'000}));
    CHECK(r.said.find("2 nesnenin ortak köşesi taşındı.") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), 3u); ///< two drawings and ONE move

    // The first object's corner number names the place as well.
    r.run("KÖŞETAŞI nesne=1 2 kose=2 nokta=10,-1");
    CHECK(r.ring(1)[1] == (Point2{10'000, -1'000}));
    CHECK(r.ring(2)[0] == (Point2{10'000, -1'000}));

    // A named object without a corner there is refused by name, nothing moved.
    r.run("ALAN 30,0 40,0 40,10 30,10");
    const std::uint64_t before = r.doc.content_hash();
    auto refused = r.bus.execute_line("KÖŞETAŞI nesne=1 3 kaynak=11,12 nokta=12,12", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("Nesne 3'in bu noktada köşesi") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), before);
}

TEST_CASE("C-07: seçili parsellerde köşeye tıklamak ortak köşeyi taşır; yazılanla aynı günlük")
{
    using namespace kentos::command;
    Rig clicked;
    clicked.run("ALAN 0,0 10,0 10,10 0,10");
    clicked.run("ALAN 10,0 20,0 20,10 10,10");
    clicked.run("ALAN 30,0 40,0 40,10 30,10");
    clicked.run("SEÇ HEPSİ");
    auto started = clicked.bus.begin_interactive("KÖŞETAŞI", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "yer");
    REQUIRE(s.supply(Value::point({10'000, 10'000})).ok()); ///< the shared corner, exactly
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "nokta");
    // The preview names both parcels' corners; the third parcel is not in it.
    auto guide = decode_grip_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK_EQ(guide.value().key, 1);
    CHECK_EQ(guide.value().index, 2u);
    REQUIRE_EQ(guide.value().also.size(), 1u);
    CHECK((guide.value().also[0] == GripGuide::More{.key = 2, .index = 3}));
    REQUIRE(s.supply(Value::point({11'000, 12'000})).ok());
    REQUIRE(clicked.bus.finish(s).ok());

    Rig typed;
    typed.run("ALAN 0,0 10,0 10,10 0,10");
    typed.run("ALAN 10,0 20,0 20,10 10,10");
    typed.run("ALAN 30,0 40,0 40,10 30,10");
    typed.run("KÖŞETAŞI nesne=1 2 kaynak=10,10 nokta=11,12");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    CHECK(clicked.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());
}

TEST_CASE("C-07: kilitli katmandaki seçili nesne ortak köşede atlanır ve söylenir")
{
    using namespace kentos::command;
    Rig r;
    r.run("KATMAN ad=YENI");
    r.run("ALAN 0,0 10,0 10,10 0,10");
    r.run("KATMAN ad=TAPU");
    r.run("ALAN 10,0 20,0 20,10 10,10");
    r.run("KATMAN ad=TAPU kilitli=evet");
    r.run("SEÇ HEPSİ");
    auto started = r.bus.begin_interactive("KÖŞETAŞI yer=10,10 nokta=11,11", Origin::Gui);
    REQUIRE(started.ok());
    REQUIRE_FALSE(started.value()->waiting());
    REQUIRE(r.bus.finish(*started.value()).ok());
    CHECK(r.ring(1)[2] == (Point2{11'000, 11'000}));
    CHECK(r.ring(2)[3] == (Point2{10'000, 10'000})); ///< the locked title stays put
    CHECK(r.said.find("1 nesne kilitli katmanda atlandı") != std::string::npos);

    // Named, the locked one refuses the whole edit with its own reason.
    auto refused = r.bus.execute_line("KÖŞETAŞI nesne=1 2 kaynak=11,11 nokta=12,12", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("kilitli") != std::string::npos);
}

TEST_CASE("C-07: tutamak önizlemesinin baytları birlikte taşınan tutamakları taşır")
{
    const GripGuide guide{.key = 4, .index = 2, .insert = false, .also = {{7, 1}, {9, 0}}};
    const auto bytes = encode_grip_guide(guide);
    auto back        = decode_grip_guide(bytes);
    REQUIRE(back.ok());
    CHECK_EQ(back.value().key, 4);
    CHECK_EQ(back.value().index, 2u);
    CHECK(back.value().also == guide.also);
    CHECK_FALSE(decode_grip_guide(std::span(bytes).first(bytes.size() - 1)).ok());
    const auto one = encode_grip_guide(GripGuide{.key = 1, .index = 0, .insert = true, .also = {}});
    REQUIRE(decode_grip_guide(one).ok());
    CHECK(decode_grip_guide(one).value().insert);
}

TEST_CASE(
    "C-07: köşe taşınınca bağlı ölçü yazıları izler; kilitli katmandaki izleyemez ve söylenir")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 40,0 40,30 0,30");
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");
    r.said.clear();

    // The corner moves 4 m east: the two edges that meet it change length,
    // and their captions follow and re-say their number.
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=44,30");
    CHECK_MESSAGE(r.said.find("Bağlı 2 yazı kaynağını izledi; 2 yazının metni yenilendi.") !=
                      std::string::npos,
                  r.said);

    // With the captions' layer locked they cannot follow: they stay, and the
    // user is told they now stand apart from what they describe.
    r.run("KATMAN ad=OLCU kilitli=evet");
    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=48,30");
    CHECK_MESSAGE(r.said.find("yazı kilitli katmanda olduğu için kaynağını izleyemedi") !=
                      std::string::npos,
                  r.said);
}

// =============================================================================
// KÖŞESİL and KENARTÜRÜ
// =============================================================================

TEST_CASE("C-07: KÖŞESİL köşeyi siler; iki kenar tek kenar olur; en az köşe korunur")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    r.run("KÖŞESİL nesne=1 kose=2");
    CHECK(r.ring(1) == std::vector<Point2>{{0, 0}, {10'000, 10'000}, {20'000, 10'000}});
    CHECK(r.said.find("Köşe silindi.") != std::string::npos);

    // A line keeps two corners, a face three: said, nothing written.
    r.run("ÇİZGİ 0,20 10,20");
    const std::uint64_t before = r.doc.content_hash();
    auto two = r.bus.execute_line("KÖŞESİL nesne=2 kose=1", kentos::command::Origin::Test);
    REQUIRE_FALSE(two.ok());
    CHECK(two.error().message.find("en az iki köşeyle") != std::string::npos);
    r.run("ALAN 0,30 10,30 10,40 0,40");
    r.run("KÖŞESİL nesne=3 kose=3");
    CHECK_EQ(r.ring(3).size(), 3u);
    auto three = r.bus.execute_line("KÖŞESİL nesne=3 kose=1", kentos::command::Origin::Test);
    REQUIRE_FALSE(three.ok());
    CHECK(three.error().message.find("en az üç köşeyle") != std::string::npos);
    CHECK_NE(r.doc.content_hash(), before);

    // A circle's handles are its definition, not corners.
    r.run("DAİRE merkez=50,50 cevre=55,50");
    auto round = r.bus.execute_line("KÖŞESİL nesne=4 kose=2", kentos::command::Origin::Test);
    REQUIRE_FALSE(round.ok());
    CHECK(round.error().message.find("tanımı köşelerinden oluşmuyor") != std::string::npos);
}

TEST_CASE("C-07: KÖŞESİL iki parselin ortak köşesini ikisinden birden siler")
{
    Rig r;
    r.run("ALAN 0,0 10,0 10,5 10,10 0,10");
    r.run("ALAN 10,0 20,0 20,10 10,10 10,5");
    r.run("KÖŞESİL nesne=1 2 kaynak=10,5");
    CHECK(r.ring(1) == std::vector<Point2>{{0, 0}, {10'000, 0}, {10'000, 10'000}, {0, 10'000}});
    CHECK(r.ring(2) ==
          std::vector<Point2>{{10'000, 0}, {20'000, 0}, {20'000, 10'000}, {10'000, 10'000}});
    CHECK(r.said.find("2 nesnenin ortak köşesi silindi.") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), 3u);
}

TEST_CASE("C-07: yaylı yolda köşe silinince aynı çemberin iki yayı tek yay, başkası düz kenar olur")
{
    // Two quarter arcs of one circle: taking the vertex between them leaves one
    // half circle. An arc then a straight piece: the chord of the two.
    CurvePath path;
    path.pieces.push_back(arc_piece({0, 0}, 10'000, {10'000, 0}, {0, 10'000}, true));
    path.pieces.push_back(arc_piece({0, 0}, 10'000, {0, 10'000}, {-10'000, 0}, true));
    path.pieces.push_back(PathPiece{.from = {-10'000, 0}, .to = {-10'000, -5'000}});
    auto one = path_without_vertex(path, 1);
    REQUIRE(one.ok());
    REQUIRE_EQ(one.value().pieces.size(), 2u);
    CHECK(one.value().pieces[0].kind == PathPiece::Kind::Arc);
    CHECK_EQ(one.value().pieces[0].sweep_udeg, std::int64_t{180'000'000});
    auto chord = path_without_vertex(path, 2);
    REQUIRE(chord.ok());
    REQUIRE_EQ(chord.value().pieces.size(), 2u);
    CHECK(chord.value().pieces[1].kind == PathPiece::Kind::Segment);
    CHECK(chord.value().pieces[1].from == (Point2{0, 10'000}));
    CHECK(chord.value().pieces[1].to == (Point2{-10'000, -5'000}));
    // An open path's end takes its end piece with it.
    auto end = path_without_vertex(path, 3);
    REQUIRE(end.ok());
    CHECK_EQ(end.value().pieces.size(), 2u);
}

TEST_CASE("C-07: KÖŞESİL spline kontrol noktasını siler; derece noktalara göre düşer")
{
    Rig r;
    r.run("SPLINE noktalar=0,0 noktalar=10,10 noktalar=20,-10 noktalar=30,0");
    r.run("KÖŞESİL nesne=1 kose=2");
    CHECK_EQ(r.ring(1).size(), 3u);
    CHECK(r.said.find("derecesi 2 oldu") != std::string::npos);
    const EntityId e = r.doc.slot_of(EntityKey{1});
    auto def         = decode_spline(r.doc.geometry().payload_of(r.doc.entities().slot[e]));
    REQUIRE(def.ok());
    CHECK_EQ(def.value().degree, 2);
    CHECK_EQ(def.value().knots_nano, uniform_clamped_knots(3, 2));
}

TEST_CASE("C-07: KENARTÜRÜ parselin kenarını yaya çevirir; kimlik, öznitelik ve bağlı yazı kalır")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");
    const std::size_t alive_before = r.doc.live_entity_count();

    // The south edge bent out through (10, −3): the parcel is an arc polyline
    // now, by the same key, its captions still there and following.
    r.said.clear();
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3");
    const EntityId e = r.doc.slot_of(EntityKey{1});
    REQUIRE(e != kNoEntity);
    CHECK_EQ(r.doc.entities().kind[e], kArcPolylineKind);
    CHECK_EQ(r.doc.live_entity_count(), alive_before); ///< nothing erased, nothing added
    CHECK(r.said.find("Kenar yaya çevrildi (yarıçap") != std::string::npos);
    CHECK(r.said.find("kimliği ve öznitelikleri korundu") != std::string::npos);
    const auto bent = path_of(r.doc, e);
    REQUIRE(bent);
    CHECK(bent->pieces[0].kind == PathPiece::Kind::Arc);
    CHECK(std::abs(gap(point_at(*bent, PathPlace{0, 0.5}), {10'000, -3'000})) <= 2.0);

    // And straight again: a polyline, the same key.
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=duz");
    CHECK_EQ(r.doc.entities().kind[e], kPolylineKind);
    CHECK(r.said.find("yay kalmadığı için düz çoklu çizgi oldu") != std::string::npos);

    // Undo puts the kind back with the geometry: one step each.
    r.run("GERİAL");
    CHECK_EQ(r.doc.entities().kind[e], kArcPolylineKind);
    r.run("GERİAL");
    CHECK_EQ(r.doc.entities().kind[e], kPolylineKind);

    // A point on the edge's line bends nothing, and says so.
    auto flat = r.bus.execute_line("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=30,0",
                                   kentos::command::Origin::Test);
    REQUIRE_FALSE(flat.ok());
    CHECK(flat.error().message.find("kenarın doğrultusunda") != std::string::npos);
}

TEST_CASE("C-07: KENARTÜRÜ tıklayarak: kenara tıklamak, yayın noktasını göstermek; yazılanla aynı")
{
    using namespace kentos::command;
    Rig clicked;
    clicked.run("ÇOKLUÇİZGİ 0,0 20,0 20,10");
    auto started = clicked.bus.begin_interactive("KENARTÜRÜ", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "yer");
    REQUIRE(s.supply(Value::point({10'000, 0})).ok()); ///< on the first edge
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "nokta");
    CHECK(s.prompt().rubber_shape == RubberShape::EdgeArc);
    auto guide = decode_edge_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK_EQ(guide.value().key, 1);
    CHECK_EQ(guide.value().edge, 0u);
    REQUIRE(s.supply(Value::point({10'000, -4'000})).ok());
    REQUIRE(clicked.bus.finish(s).ok());

    Rig typed;
    typed.run("ÇOKLUÇİZGİ 0,0 20,0 20,10");
    typed.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-4");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    CHECK(clicked.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());
}

TEST_CASE("C-07: KÖŞESİL ve KENARTÜRÜ sayfalarının örnekleri yazıldığı gibi çalışır")
{
    // docs/komutlar/vertex_delete.md, edge_kind.md and vertex_move.md print
    // these lines and these answers; each example starts a new drawing
    // (CLAUDE.md 11.6).
    const auto said = [](std::initializer_list<const char*> lines) {
        Rig r;
        for (const char* line : lines)
            r.run(line);
        return r.said;
    };
    CHECK(
        said({"ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10", "KÖŞESİL nesne=1 kose=2"}).find("Köşe silindi.") !=
        std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,5 10,10 0,10", "ALAN 10,0 20,0 20,10 10,10 10,5",
                "KÖŞESİL nesne=1 2 kaynak=10,5"})
              .find("2 nesnenin ortak köşesi silindi.") != std::string::npos);
    const std::string bent =
        said({"ALAN 0,0 20,0 20,10 0,10", "KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3"});
    CHECK_MESSAGE(bent.find("Kenar yaya çevrildi (yarıçap 18,167 m); nesne yaylı çoklu çizgi "
                            "oldu, kimliği ve öznitelikleri korundu.") != std::string::npos,
                  bent);
    CHECK(said({"ALAN 0,0 20,0 20,10 0,10", "KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3",
                "KENARTÜRÜ nesne=1 kenar=1 tur=duz"})
              .find("Kenar düzleştirildi; yay kalmadığı için düz çoklu çizgi oldu.") !=
          std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 10,10 0,10", "ALAN 10,0 20,0 20,10 10,10",
                "KÖŞETAŞI nesne=1 2 kaynak=10,10 nokta=11,12"})
              .find("2 nesnenin ortak köşesi taşındı.") != std::string::npos);
}

TEST_CASE("C-07: bir noktanın tutamağı taşınınca kotu (Z) korunur")
{
    // The plan is 2D and a height is the `kot` attribute (model.md); moving a
    // surveyed point in plan must not drop the height it was measured with.
    Rig r;
    r.run("NOKTA 5,5");
    r.run("SÜTUN kimlik=kot tur=uzunluk");
    r.run("ÖZNİTELİK kot 1 1250350"); ///< millimetres, as a Length column stores it
    r.said.clear();
    r.run("ÖZNİTELİK kot 1");
    const std::string before = r.said;
    r.run("KÖŞETAŞI nesne=1 kose=1 nokta=6,7");
    r.said.clear();
    r.run("ÖZNİTELİK kot 1");
    CHECK_MESSAGE(r.said == before, before << " → " << r.said);
    CHECK(before.find("1250") != std::string::npos); ///< 1250,350 m
}
