// SPDX-License-Identifier: GPL-3.0-or-later
//
// EVERY DIMENSION TYPE DRAWN WITH ITS OWN TOOL (TODOS C-17, 1st stage).
//
// A radius, a diameter and an arc length are taken with ONE click on the curve
// they measure: the centre, the radius and an arc's ends are the curve's own,
// exact as stored, so the figure is the circle's radius rather than the one a
// hand found by eye. An angle is asked for vertex first, the way a hand measures
// one. A radius line aims at its figure. And every client — the canvas, the
// command line, a script, a journal replay — ends with the same drawing.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::DimensionDef;
using core::DimensionType;
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

    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(!r.ok(), line << " was not refused");
        return r.error().message;
    }

    core::EntityId entity(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    DimensionDef def(std::int64_t key) const
    {
        auto d = core::dimension_of(doc.geometry(), doc.entities().slot[entity(key)]);
        REQUIRE(d.ok());
        return d.value();
    }

    /// Ring 1 of a dimension: its definition points.
    std::vector<Point2> defs(std::int64_t key) const
    {
        const std::uint32_t slot = doc.entities().slot[entity(key)];
        const core::RingSpan rs  = doc.geometry().rings_of(slot);
        REQUIRE(rs.count >= 2);
        const auto xs = doc.geometry().ring_xs(rs.first + 1);
        const auto ys = doc.geometry().ring_ys(rs.first + 1);
        std::vector<Point2> out;
        for (std::size_t i = 0; i < xs.size(); ++i)
            out.push_back(Point2{xs[i], ys[i]});
        return out;
    }

    std::string caption(std::int64_t key) const
    {
        return std::string(doc.texts().text(doc.entities().slot[entity(key)]));
    }

    std::size_t links(std::int64_t key) const
    {
        const auto* l = doc.dimension_links().get(entity(key));
        return l == nullptr ? 0 : l->size();
    }
};

std::string what_happened(const Journal& j)
{
    std::string out;
    for (const auto& e : j.entries())
        out += e.command_id + ' ' + e.args.to_json().dump() + '\n';
    return out;
}

double length(Point2 a, Point2 b)
{
    const auto dx = static_cast<double>(b.x - a.x);
    const auto dy = static_cast<double>(b.y - a.y);
    return std::sqrt((dx * dx) + (dy * dy));
}

/// The angle between two directions from `o`, in radians.
double between(Point2 o, Point2 a, Point2 b)
{
    const double ax = static_cast<double>(a.x - o.x);
    const double ay = static_cast<double>(a.y - o.y);
    const double bx = static_cast<double>(b.x - o.x);
    const double by = static_cast<double>(b.y - o.y);
    return std::abs(std::atan2((ax * by) - (ay * bx), (ax * bx) + (ay * by)));
}

/// How far `p` stands off the line through `a` and `b`, toward the side a
/// figure on that line stands on — above it as it reads (ISO 129-1): the left
/// of a line read left to right, the right of one read the other way.
double above(Point2 a, Point2 b, Point2 p)
{
    double ux      = static_cast<double>(b.x - a.x);
    double uy      = static_cast<double>(b.y - a.y);
    const double l = std::sqrt((ux * ux) + (uy * uy));
    ux /= l;
    uy /= l;
    if (ux < 0.0 || (ux == 0.0 && uy < 0.0)) {
        ux = -ux;
        uy = -uy;
    }
    return (static_cast<double>(p.x - a.x) * -uy) + (static_cast<double>(p.y - a.y) * ux);
}

/// ISO-25 at 1/1000: a 2,5 m caption a 0,625 m gap off its line, so its centre
/// stands half its height plus the gap above it — to within the few millimetres
/// the rim point moves to keep the radius exact (`core::dimension_rim_point`),
/// which on a 1/1000 sheet is a few thousandths of a millimetre of paper.
constexpr double kCaptionAbove = 625.0 + (2'500.0 / 2.0);

} // namespace

// ------------------------------------------------------------ the rim point ----

TEST_CASE("ÇEMBER NOKTASI: yarıçap çizgisinin çembere değdiği nokta yarıçapı tam verir")
{
    // Every radius a drawing uses, in every direction round the clock: the point
    // lies toward the aim and its distance rounds to the radius itself — the
    // rounded point alone is up to seven tenths of a millimetre off, which a
    // figure written to the millimetre shows.
    const Point2 centre{485'320'150, 4'310'220'400};
    for (const core::Mm radius : {1'000, 1'234, 7'500, 50'000, 123'457, 4'999'999}) {
        for (int step = 0; step < 72; ++step) {
            const double a = static_cast<double>(step) * (2.0 * std::acos(-1.0) / 72.0);
            const Point2 aim{centre.x + static_cast<core::Mm>(std::llround(std::cos(a) * 1e6)),
                             centre.y + static_cast<core::Mm>(std::llround(std::sin(a) * 1e6))};
            const Point2 rim = core::dimension_rim_point(centre, radius, aim);
            CHECK_MESSAGE(std::llround(length(centre, rim)) == radius,
                          "r=" << radius << " step=" << step);
            CHECK_LT(std::abs(length(centre, rim) - static_cast<double>(radius)), 0.45);
            CHECK_LT(between(centre, rim, aim), 5.0 / static_cast<double>(radius));
        }
    }
    // Aimed at the centre itself: the point at angle zero.
    CHECK_EQ(core::dimension_rim_point(Point2{0, 0}, 5'000, Point2{0, 0}), (Point2{5'000, 0}));
}

TEST_CASE("YARIÇAP ÇİZGİSİ yazısına döner: yazı çizginin üstünde durur, yarıçap aynı")
{
    // THE CAPTION STAYS WHERE IT WAS PUT and the line turns under it: its centre
    // stands half a text height and the gap above the line as the line reads,
    // and the line meets the circle where it must for that (ISO 129-1).
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0");                        // 1
    r.run("ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=0,8"); // 2: straight up
    const auto d = r.defs(2);
    REQUIRE_EQ(d.size(), std::size_t{2});
    CHECK_EQ(d[0], (Point2{0, 0}));
    CHECK_EQ(std::llround(length(d[0], d[1])), 5'000);
    CHECK_LT(std::abs(above(d[0], d[1], Point2{0, 8'000}) - kCaptionAbove), 10.0);
    CHECK_EQ(r.def(2).measurement, 5'000);
    CHECK_EQ(r.caption(2), std::string("R5,00")); // R is part of the figure

    // A diameter straddles the centre, both ends on the line to its figure —
    // the far one within a millimetre of the near one's mirror, placed so the
    // length is the diameter exactly.
    r.run("ÖLÇÜ tur=cap birinci=-5,0 ikinci=5,0 konum=-6,-6"); // 3
    const auto c = r.defs(3);
    REQUIRE_EQ(c.size(), std::size_t{2});
    CHECK_LE(std::abs(c[0].x + c[1].x), 1);
    CHECK_LE(std::abs(c[0].y + c[1].y), 1);
    CHECK_EQ(r.def(3).measurement, 10'000);
    CHECK_LT(std::abs(above(c[0], c[1], Point2{-6'000, -6'000}) - kCaptionAbove), 10.0);
    CHECK_EQ(r.caption(3), std::string("Ø10,00"));

    // AND `onek=""` writes none, which a sheet with its own convention asks for.
    r.run("ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=8,0 onek=\"\""); // 4
    CHECK_EQ(r.caption(4), std::string("5,00"));
}

// -------------------------------------------------------------- one click ----

TEST_CASE("YARIÇAP ARACI: çembere tek tıklama merkezi ve yarıçapı verir, yazıya uzanır, bağlanır")
{
    Rig r;
    r.run("DAİRE merkez=10,10 cevre=17.5,10"); // 1: radius 7,5 m

    auto started = r.bus.begin_interactive("ÖLÇÜ tur=yaricap", Origin::Gui);
    REQUIRE(started.ok());
    auto& s = *started.value();
    CHECK_EQ(s.prompt().param, std::string("nokta"));
    CHECK_EQ(s.prompt().message, std::string("Ölçülecek daireye ya da yaya tıklayın"));
    CHECK(s.supply(Value::point(Point2{10'000, 17'500})).ok()); // on the circle, at the top
    CHECK_EQ(s.prompt().param, std::string("konum"));
    CHECK(s.prompt().message.find("yarıçap çizgisi") != std::string::npos);
    CHECK(s.prompt().rubber_shape == RubberShape::Dimension);
    REQUIRE_EQ(s.prompt().rubber_chain.size(), std::size_t{2});
    CHECK_EQ(s.prompt().rubber_chain[0], (Point2{10'000, 10'000}));
    CHECK(s.supply(Value::point(Point2{16'000, 20'000})).ok());
    REQUIRE(r.bus.finish(s).ok());

    CHECK_EQ(r.def(2).type, DimensionType::Radial);
    CHECK_EQ(r.def(2).measurement, 7'500);
    CHECK_EQ(r.caption(2), std::string("R7,50"));
    const auto d = r.defs(2);
    CHECK_EQ(d[0], (Point2{10'000, 10'000}));
    CHECK_LT(std::abs(above(d[0], d[1], Point2{16'000, 20'000}) - kCaptionAbove), 10.0);
    CHECK_EQ(r.links(2), std::size_t{2}); // the centre and the circle

    // THE JOURNAL HOLDS THE POINTS, NOT THE CLICK: a replay needs neither the
    // click nor the circle to still be there.
    const auto& args = r.journal.entries().back().args;
    CHECK(args.has("birinci"));
    CHECK(args.has("ikinci"));
    CHECK_FALSE(args.has("nokta"));
}

TEST_CASE("ÇAP ARACI: çembere tek tıklama, çap yarıçapın tam iki katı, 45 derecede bile")
{
    // FORTY-FIVE DEGREES IS THE HARD DIRECTION: the whole millimetres there lie
    // seven tenths of one apart across the circle, and two ends mirrored
    // through the centre doubled the near one's rounding — 24,689 for a
    // 12,345 m radius. The far end is chosen for the length.
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=12.345,0"); // 1: r = 12,345 m
    r.run("ÖLÇÜ tur=cap nokta=0,12.345 konum=20,20");
    CHECK_EQ(r.def(2).type, DimensionType::Diametric);
    CHECK_EQ(r.def(2).measurement, 24'690);
    CHECK_EQ(r.caption(2), std::string("Ø24,69"));
    const auto d = r.defs(2);
    CHECK_LE(std::abs(d[0].x + d[1].x), 1);
    CHECK_LE(std::abs(d[0].y + d[1].y), 1);
    CHECK_LT(std::abs(above(d[0], d[1], Point2{20'000, 20'000}) - kCaptionAbove), 10.0);

    // AND A REBUILD KEEPS THEM: a diameter already aimed at its figure is not
    // re-aimed, so re-wording it cannot walk its ends round.
    r.run("ÖLÇÜDÜZENLE nesneler=2 sonek=\" m\"");
    CHECK_EQ(r.defs(2), d);
    CHECK_EQ(r.caption(2), std::string("Ø24,69 m"));

    // Every direction round the clock gives the circle's own diameter.
    for (int step = 0; step < 36; ++step) {
        const double a = static_cast<double>(step) * (2.0 * std::acos(-1.0) / 36.0);
        const Point2 aim{static_cast<core::Mm>(std::llround(std::cos(a) * 30'000.0)),
                         static_cast<core::Mm>(std::llround(std::sin(a) * 30'000.0))};
        const auto ends = core::dimension_diameter_ends(Point2{0, 0}, 24'690, aim);
        CHECK_MESSAGE(std::llround(length(ends[0], ends[1])) == 24'690, "step=" << step);
    }
}

TEST_CASE("YAY UZUNLUĞU ARACI: yaya tek tıklama merkezini ve uçlarını verir, boyunu ölçer")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10"); // 1: a quarter of r = 10 m
    auto started = r.bus.begin_interactive("ÖLÇÜ tur=yay", Origin::Gui);
    REQUIRE(started.ok());
    auto& s = *started.value();
    CHECK_EQ(s.prompt().message, std::string("Ölçülecek yaya tıklayın"));
    CHECK(s.supply(Value::point(Point2{7'071, 7'071})).ok()); // on the arc, half way
    CHECK_EQ(s.prompt().param, std::string("konum"));
    CHECK(s.supply(Value::point(Point2{9'000, 9'000})).ok());
    REQUIRE(r.bus.finish(s).ok());

    CHECK_EQ(r.def(2).type, DimensionType::ArcLength);
    CHECK_EQ(r.def(2).measurement, 15'708); // π/2 · 10 m, along — not the 14,142 m chord
    const auto d = r.defs(2);
    CHECK_EQ(d[0], (Point2{0, 0}));
    CHECK_EQ(d[1], (Point2{10'000, 0}));
    CHECK_EQ(d[2], (Point2{0, 10'000}));
    CHECK_EQ(r.links(2), std::size_t{3}); // centre, start, end
    CHECK(r.journal.entries().back().args.has("bitis"));
}

TEST_CASE("TEK TIKLAMA RETLERİ: boşluğa ya da yayı olmayan yere tıklama nedeniyle söylenir")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0"); // 1
    const std::string off = r.refused("ÖLÇÜ tur=yaricap nokta=2,2 konum=8,8");
    CHECK(off.find("Orada bir daire ya da yay yok") != std::string::npos);
    CHECK(off.find("birinci=<merkez>") != std::string::npos);
    // A whole circle has no arc length to measure.
    const std::string circle = r.refused("ÖLÇÜ tur=yay nokta=5,0 konum=8,8");
    CHECK(circle.find("Orada bir yay yok") != std::string::npos);
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{1});
}

// ------------------------------------------------------------ the angle ----

TEST_CASE("AÇI ARACI: önce tepe, sonra kollar; ikinci kol açının taramasıyla önizlenir")
{
    Rig r;
    auto started = r.bus.begin_interactive("ÖLÇÜ tur=acisal", Origin::Gui);
    REQUIRE(started.ok());
    auto& s = *started.value();
    CHECK_EQ(s.prompt().param, std::string("tepe"));
    CHECK_EQ(s.prompt().message, std::string("Açının tepe noktası"));
    CHECK(s.supply(Value::point(Point2{0, 0})).ok());
    CHECK_EQ(s.prompt().param, std::string("birinci"));
    CHECK(s.prompt().rubber_shape == RubberShape::Line);
    CHECK_EQ(s.prompt().rubber_origin, (Point2{0, 0}));
    CHECK(s.supply(Value::point(Point2{10'000, 0})).ok());
    CHECK_EQ(s.prompt().param, std::string("ikinci"));
    CHECK(s.prompt().rubber_shape == RubberShape::Angle);
    REQUIRE_EQ(s.prompt().rubber_chain.size(), std::size_t{1});
    CHECK_EQ(s.prompt().rubber_chain[0], (Point2{10'000, 0}));
    CHECK(s.supply(Value::point(Point2{0, 10'000})).ok());
    CHECK_EQ(s.prompt().param, std::string("konum"));
    CHECK(s.supply(Value::point(Point2{5'000, 5'000})).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(r.def(1).type, DimensionType::Angular3P);
    CHECK_EQ(r.def(1).measurement, 90'000'000);
}

TEST_CASE("ÖLÇÜ İSTEMLERİ her türün noktalarını adıyla ister")
{
    const auto first = [](const char* line) {
        Rig r;
        auto started = r.bus.begin_interactive(line, Origin::Gui);
        REQUIRE(started.ok());
        std::string asked = started.value()->prompt().message;
        started.value()->cancel();
        (void)r.bus.finish(*started.value());
        return asked;
    };
    CHECK_EQ(first("ÖLÇÜ"), std::string("Ölçülecek ilk nokta"));
    CHECK_EQ(first("ÖLÇÜ tur=dogrusal"), std::string("Ölçülecek ilk nokta"));
    CHECK_EQ(first("ÖLÇÜ tur=koordinat"), std::string("Ordinatların okunduğu başlangıç noktası"));
    CHECK_EQ(first("ÖLÇÜ tur=cap"), std::string("Ölçülecek daireye ya da yaya tıklayın"));
    CHECK_EQ(first("ÖLÇÜ tur=yaricap birinci=0,0"), std::string("Çember üzerinde bir nokta"));
}

// ------------------------------------------------------------- the proof ----

TEST_CASE("ÖLÇÜ ARACI KANIT: çembere tıklayan arayüz, komut satırı, betik ve oynatma aynı çizim")
{
    Rig gui;
    REQUIRE(gui.bus.execute_line("DAİRE merkez=0,0 cevre=8,0", Origin::Gui).ok());
    {
        auto started = gui.bus.begin_interactive("ÖLÇÜ tur=yaricap", Origin::Gui);
        REQUIRE(started.ok());
        auto& s = *started.value();
        CHECK(s.supply(Value::point(Point2{0, -8'000})).ok());
        CHECK(s.supply(Value::point(Point2{9'000, 6'000})).ok());
        REQUIRE(gui.bus.finish(s).ok());
    }

    Rig cli;
    REQUIRE(cli.bus.execute_line("DAİRE merkez=0,0 cevre=8,0", Origin::CommandLine).ok());
    REQUIRE(
        cli.bus.execute_line("ÖLÇÜ tur=yaricap nokta=0,-8 konum=9,6", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        REQUIRE(runner
                    .run_text(R"({"ad":"Kanıt","komutlar":[
                      {"cmd":"core.circle_draw","args":{"merkez":[0,0],"cevre":[8000,0]}},
                      {"cmd":"core.dimension","args":{"tur":"yaricap","nokta":[0,-8000],
                        "konum":[9000,6000]}}]})")
                    .ok());
    }

    CHECK_EQ(gui.def(2).measurement, 8'000);
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    Rig replay;
    for (const auto& e : gui.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), gui.doc.content_hash());
}

// ------------------------------------------------------------ the units ----

TEST_CASE("AÇI ÖLÇÜSÜ projenin açı biriminde yazılır; birim= açıda açı, uzunlukta uzunluk ister")
{
    // THE UNIT THE PROJECT MEASURES IN (TODOS C-17): grad unless changed — the
    // unit ÖLÇ, AÇIÖLÇ and a typed @d<a already use — written with the mark
    // AutoCAD writes after it.
    Rig r;
    r.run("ÖLÇÜ tur=acisal tepe=0,0 birinci=10,0 ikinci=0,10 konum=5,5"); // 1: a right angle
    CHECK_EQ(r.caption(1), std::string("100,00g"));
    CHECK_EQ(r.def(1).measurement, 90'000'000); // measured in µ°, whatever is written

    // Asked for in another unit, and the unit is kept with the dimension.
    r.run("ÖLÇÜ tur=acisal tepe=20,0 birinci=30,0 ikinci=20,10 konum=25,5 birim=derece"); // 2
    CHECK_EQ(r.caption(2), std::string("90,00°"));
    r.run("ÖLÇÜDÜZENLE nesneler=2 birim=radyan hassasiyet=4");
    CHECK_EQ(r.caption(2), std::string("1,5708r"));

    // Reset, it writes what a new one would: the project's unit, not the
    // degrees an angle without a unit meant before C-17.
    r.run("ÖLÇÜDÜZENLE nesneler=2 sifirla=birim");
    CHECK_EQ(r.caption(2), std::string("100,0000g"));

    // A TOLERANCE IS GIVEN IN DEGREES, like every angle parameter of the
    // program, and written in the dimension's own unit: 0,9° is 1 grad.
    r.run("ÖLÇÜ tur=acisal tepe=60,0 birinci=70,0 ikinci=60,10 konum=65,5 tolerans=0.9");
    CHECK_EQ(r.caption(3), std::string("100,00g±1,00g"));

    // A project measuring in degrees writes degrees from then on.
    r.run("AYAR ad=açı_birimi deger=derece");
    r.run("ÖLÇÜ tur=acisal tepe=40,0 birinci=50,0 ikinci=40,10 konum=45,5"); // 4
    CHECK_EQ(r.caption(4), std::string("90,00°"));

    // The wrong list for the type is said, not guessed at.
    CHECK(r.refused("ÖLÇÜ birinci=0,20 ikinci=10,20 konum=5,23 birim=grad")
              .find("bir açı birimi; bu ölçü bir uzunluk yazar") != std::string::npos);
    CHECK(r.refused("ÖLÇÜDÜZENLE nesneler=1 birim=m")
              .find("bir uzunluk birimi; açı ölçüsü grad, derece ya da radyan yazar") !=
          std::string::npos);

    // AND A DIMENSION FROM BEFORE ANGLES HAD A UNIT reads as it always did.
    DimensionDef old;
    old.type        = DimensionType::Angular3P;
    old.measurement = 45'000'000;
    CHECK_EQ(core::dimension_text(old, core::DrawingUnit::Metre), std::string("45,00°"));
}

// ------------------------------------------ drawn like the sheet (2nd stage) ----

namespace {

/// Where dimension `key`'s figure stands: its caption baseline's first vertex,
/// which is the caption's centre.
Point2 caption_centre(const Rig& r, std::int64_t key)
{
    const std::uint32_t slot = r.doc.entities().slot[r.entity(key)];
    const core::RingSpan rs  = r.doc.geometry().rings_of(slot);
    return Point2{r.doc.geometry().ring_xs(rs.first)[0], r.doc.geometry().ring_ys(rs.first)[0]};
}

/// Dimension `key` as it is drawn.
core::EmitBuffer outline(const Rig& r, std::int64_t key)
{
    core::EmitBuffer buf;
    core::dimension_outline(r.doc.geometry(), r.doc.entities().slot[r.entity(key)], buf);
    return buf;
}

std::size_t solid_runs(const core::EmitBuffer& buf)
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < buf.run_total(); ++i)
        if (buf.run_solid[i] != 0) ++n;
    return n;
}

} // namespace

TEST_CASE("ÖLÇÜ YAZISI çizginin üstünde durur: nesnenin altındaki ölçüde de, düşeyde solunda")
{
    // ISO 129-1: above the line as the figure reads, whichever side of the line
    // the measured edge is. It hung under a line placed below its parcel.
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1: the line below the edge
    CHECK_EQ(caption_centre(r, 1).y, -3'000 + 1'875);
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,3"); // 2: the line above it
    CHECK_EQ(caption_centre(r, 2).y, 3'000 + 1'875);
    r.run("ÖLÇÜ birinci=30,0 ikinci=30,10 konum=33,5"); // 3: vertical, read from the bottom
    CHECK_EQ(caption_centre(r, 3).x, 33'000 - 1'875);
}

TEST_CASE("OKLAR: kapalı ok doludur, çizginin kendi mürekkebiyle — ekran ve çıktı aynı sahneden")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1: ISO-25, closed heads
    const core::EmitBuffer buf = outline(r, 1);
    CHECK_EQ(solid_runs(buf), std::size_t{2});
    for (std::size_t i = 0; i < buf.run_total(); ++i)
        if (buf.run_solid[i] != 0) {
            CHECK(buf.run_closed[i] != 0);
            CHECK_EQ(buf.run_count[i], 3u);
        }
    // A tick is a stroke and stays one.
    r.run("ÖLÇÜ birinci=0,20 ikinci=20,20 konum=10,23 stil=MIMARI"); // 2
    CHECK_EQ(solid_runs(outline(r, 2)), std::size_t{0});

    // A leader's head is the same closed head, drawn by the same function.
    r.run("LİDER noktalar=24,4 27,7 29,7"); // 3, inside the view below
    core::EmitBuffer leader;
    core::leader_outline(r.doc.geometry(), r.doc.entities().slot[r.entity(3)], leader);
    CHECK_EQ(solid_runs(leader), std::size_t{1});

    // IN THE SCENE the heads are a fill in the stroke's own ink — a plain line
    // style fills nothing, so without it the heads drew as outlines.
    render::ViewTransform view;
    view.set_viewport(1000, 800);
    view.set_centre(Point2{10'000, 10'000}, 40.0);
    render::DrawList list;
    render::build_scene(r.doc, view, render::SceneOptions{}, list);
    std::size_t heads = 0;
    for (std::size_t p = 0; p < list.passes.size(); ++p) {
        if (list.passes[p].type != core::SymbolLayerType::SimpleFill ||
            list.polygons[p].runs.empty())
            continue;
        heads += list.polygons[p].runs.size();
        bool inked = false;
        for (const render::PolylineBatch& stroke : list.polylines)
            if (!stroke.runs.empty() && stroke.rgba == list.polygons[p].rgba) inked = true;
        CHECK(inked);
    }
    CHECK_EQ(heads, std::size_t{3}); // the dimension's two and the leader's one
}

TEST_CASE("YAY UZUNLUĞU: ölçü yayı yazıdan geçer, uçlarında uzatma çizgileri ve oklar")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");   // 1
    r.run("ÖLÇÜ tur=yay nokta=7.071,7.071 konum=10,10"); // 2: 14,142 m from the centre
    const core::EmitBuffer buf = outline(r, 2);
    std::size_t longest        = 0;
    for (std::size_t i = 1; i < buf.run_total(); ++i)
        if (buf.run_count[i] > buf.run_count[longest]) longest = i;
    const auto xs = buf.run_xs(longest);
    const auto ys = buf.run_ys(longest);
    REQUIRE_GT(xs.size(), std::size_t{8});
    for (std::size_t v = 0; v < xs.size(); ++v)
        CHECK_LT(std::abs(length(Point2{0, 0}, Point2{xs[v], ys[v]}) - 14'142.0), 2.0);
    CHECK_EQ(solid_runs(buf), std::size_t{2});
    CHECK_EQ(r.def(2).measurement, 15'708); // still the length along the arc it measures
}

TEST_CASE("SIĞMAYAN ÖLÇÜ: yazı ikinci uzatma çizgisinin dışına, oklar dışarıdan içeri")
{
    // A metre between the extension lines and 2,5 m heads: neither the figure
    // nor the heads fit, so both stand outside and the line runs on under the
    // figure (ISO 129-1).
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=1,0 konum=0.5,-3"); // 1
    const Point2 c = caption_centre(r, 1);
    CHECK_GT(c.x, 1'000);
    CHECK_EQ(c.y, -3'000 + 1'875);

    const core::EmitBuffer buf = outline(r, 1);
    CHECK_EQ(solid_runs(buf), std::size_t{2});
    for (std::size_t i = 0; i < buf.run_total(); ++i) {
        if (buf.run_solid[i] == 0) continue;
        const auto xs = buf.run_xs(i);
        // The tip is on a foot; the base is outside the two lines.
        if (xs[0] == 0)
            CHECK_LT(std::max(xs[1], xs[2]), 0);
        else
            CHECK_GT(std::min(xs[1], xs[2]), 1'000);
    }
    core::Mm reach = 0;
    for (std::size_t i = 0; i < buf.run_total(); ++i) {
        const auto ys = buf.run_ys(i);
        if (buf.run_solid[i] != 0 || ys.size() != 2 || ys[0] != -3'000 || ys[1] != -3'000) continue;
        const auto xs = buf.run_xs(i);
        reach         = std::max({reach, xs[0], xs[1]});
    }
    CHECK_GE(reach, c.x);

    // ONE THAT FITS is left where it is laid out: in the middle, heads inside.
    r.run("ÖLÇÜ birinci=0,10 ikinci=20,10 konum=10,7"); // 2
    CHECK_EQ(caption_centre(r, 2).x, 10'000);
}

namespace {

/// A closed arrowhead of a picture: where its tip is and which way it points.
struct Head
{
    Point2 tip;
    double dx{0.0};
    double dy{0.0};
};

std::vector<Head> heads_of(const core::EmitBuffer& buf)
{
    std::vector<Head> out;
    for (std::size_t i = 0; i < buf.run_total(); ++i) {
        if (buf.run_solid[i] == 0 || buf.run_count[i] != 3) continue;
        const std::uint32_t at = buf.run_start[i];
        const Point2 tip{buf.xs[at], buf.ys[at]};
        const double bx =
            (static_cast<double>(buf.xs[at + 1]) + static_cast<double>(buf.xs[at + 2])) / 2.0;
        const double by =
            (static_cast<double>(buf.ys[at + 1]) + static_cast<double>(buf.ys[at + 2])) / 2.0;
        const double dx = static_cast<double>(tip.x) - bx;
        const double dy = static_cast<double>(tip.y) - by;
        const double l  = std::sqrt((dx * dx) + (dy * dy));
        out.push_back(Head{tip, dx / l, dy / l});
    }
    return out;
}

/// Which way `p` lies from `c`, degrees counter-clockwise from due east.
double bearing(Point2 c, Point2 p)
{
    return static_cast<double>(core::atan2_udeg(p.y - c.y, p.x - c.x)) / 1e6;
}

double apart(Point2 a, Point2 b)
{
    return std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y));
}

/// The longest straight stroke of `buf` that starts or ends within 2 mm of `at`.
double stroke_from(const core::EmitBuffer& buf, Point2 at)
{
    double best = 0.0;
    for (std::size_t i = 0; i < buf.run_total(); ++i) {
        if (buf.run_count[i] != 2 || buf.run_solid[i] != 0) continue;
        const std::uint32_t k = buf.run_start[i];
        const Point2 a{buf.xs[k], buf.ys[k]};
        const Point2 b{buf.xs[k + 1], buf.ys[k + 1]};
        if (apart(a, at) <= 2.0 || apart(b, at) <= 2.0) best = std::max(best, apart(a, b));
    }
    return best;
}

} // namespace

TEST_CASE("AÇI ÖLÇÜSÜNDE SIĞDIRMA: yazı kola çarpmaz; yay yetiyorsa kayar, yetmiyorsa kolun "
          "dışına çıkar ve oklar dışarıdan içeri döner")
{
    // THE ANGLE'S TWIN OF A LINEAR FIT (ISO 129-1, TODOS C-17). A caption set
    // where the arc was taken through, 9 m of words on a 4,9 m arc, read
    // across both arms and both heads — the probe sheet showed it.
    Rig r;

    // 1: a right angle on a 20 m arc — the words fit where they were put, and
    // stay there: outward of the arc, a gap and half a text height.
    r.run("ÖLÇÜ tur=acisal tepe=0,0 birinci=20,0 ikinci=0,20 konum=14.142,14.142");
    const Point2 c1 = caption_centre(r, 1);
    CHECK(std::abs(bearing(Point2{0, 0}, c1) - 45.0) < 0.05);
    CHECK(std::abs(apart(Point2{0, 0}, c1) - 21'875.0) < 5.0);
    for (const Head& h : heads_of(outline(r, 1))) {
        // Inside the arms, pointing out at them: a step back from the tip is on the arc.
        const Point2 back{h.tip.x - static_cast<core::Mm>(std::llround(h.dx * 1000.0)),
                          h.tip.y - static_cast<core::Mm>(std::llround(h.dy * 1000.0))};
        const double b = bearing(Point2{0, 0}, back);
        CHECK((b > 0.0 && b < 90.0));
    }

    // 2: the same angle taken through a point 3° off its first arm. The
    // figure there would cross the arm; it slides round the arc until it
    // clears it by a gap — (10,5 m / 2 + 0,625 m) / 20 m = 16,83°.
    r.run("ÖLÇÜ tur=acisal tepe=40,0 birinci=60,0 ikinci=40,20 konum=59.973,1.047");
    CHECK_EQ(r.caption(2), std::string("100,00g"));
    CHECK(std::abs(bearing(Point2{40'000, 0}, caption_centre(r, 2)) - 16.83) < 0.05);

    // 3: the probe sheet's angle: 36,87° on a 7,6 m arc. Neither the words
    // (9 m) nor the heads (2,5 m each) fit: the words go past the second arm
    // — the nearer to where the arc was taken through, 23,2° of 36,87° —
    // on its tangent, and the heads stand outside pointing in.
    r.run("ÖLÇÜ tur=acisal tepe=80,-14 birinci=92,-14 ikinci=88,-8 konum=87,-11");
    const Point2 vertex{80'000, -14'000};
    const Point2 c3 = caption_centre(r, 3);
    CHECK(bearing(vertex, c3) > 36.87);
    CHECK(bearing(vertex, c3) < 180.0);
    const core::EmitBuffer pic    = outline(r, 3);
    const std::vector<Head> heads = heads_of(pic);
    REQUIRE_EQ(heads.size(), std::size_t{2});
    for (const Head& h : heads) {
        // A step back from each tip is OUTSIDE the arms: the head comes in from there.
        const Point2 back{h.tip.x - static_cast<core::Mm>(std::llround(h.dx * 1000.0)),
                          h.tip.y - static_cast<core::Mm>(std::llround(h.dy * 1000.0))};
        const double b = bearing(vertex, back);
        CHECK((b > 36.87 || b < 1e-9 || b > 180.0));
    }
    // The words stand on the second arm's tangent, carried on to their far
    // end: past the head's tail (5 m), a gap and the whole 9 m caption.
    const double radius = std::hypot(7'000.0, 3'000.0);
    const Point2 e2{80'000 + static_cast<core::Mm>(std::llround(0.8 * radius)),
                    -14'000 + static_cast<core::Mm>(std::llround(0.6 * radius))};
    CHECK(stroke_from(pic, e2) > 14'000.0);

    // A REBUILD — what a moved arm, an edit and a grip all run — puts the
    // figure where the command did: one fit, not two opinions about it.
    const core::DimensionEdit none;
    auto again = core::dimension_rebuild(r.doc, r.entity(3), none, core::DrawingUnit::Metre);
    REQUIRE(again.ok());
    CHECK_EQ(again.value().baseline[0], c3);
}

TEST_CASE("YAY UZUNLUĞUNDA SIĞDIRMA: kısa yayın yazısı ucun dışına, oklar dışarıdan içeri")
{
    // A 30° arc of 5 m radius, measured on a 5,2 m dimension arc: 2,72 m of
    // arc for 6 m of words and 5 m of heads.
    Rig r;
    r.run("ÖLÇÜ tur=yay birinci=0,40 ikinci=5,40 bitis=4.330,42.5 konum=5.121,40.903");
    CHECK_EQ(r.caption(1), std::string("2,62"));
    const Point2 centre{0, 40'000};
    const double b = bearing(centre, caption_centre(r, 1));
    // Taken through at 10° of 30°: nearer the first end, and it goes past
    // that one, below it on its clockwise tangent.
    CHECK((b > 180.0 || b < 0.0));
    const std::vector<Head> heads = heads_of(outline(r, 1));
    REQUIRE_EQ(heads.size(), std::size_t{2});
    for (const Head& h : heads) {
        const Point2 back{h.tip.x - static_cast<core::Mm>(std::llround(h.dx * 1000.0)),
                          h.tip.y - static_cast<core::Mm>(std::llround(h.dy * 1000.0))};
        const double at = bearing(centre, back);
        CHECK((at > 30.0 || at < 1e-9 || at > 180.0));
    }
}

TEST_CASE("DIŞARIDAKİ OKLAR eğik bir ölçüde de ölçü çizgisi doğrultusundadır")
{
    // REGRESSION: a head outside was aimed through a point a millimetre off
    // its tip, rounded to the millimetre — (0,6; 0,8) became (1; 1), and the
    // head of a 53° dimension pointed at 45°.
    Rig r;
    r.run("ÖLÇÜ birinci=0,80 ikinci=1.2,81.6 konum=-1.8,82.6"); // 2 m along (0,6; 0,8)
    const std::vector<Head> heads = heads_of(outline(r, 1));
    REQUIRE_EQ(heads.size(), std::size_t{2});
    for (const Head& h : heads)
        CHECK(std::abs((h.dx * 0.8) - (h.dy * 0.6)) < 0.0087); // within half a degree
}

TEST_CASE("YARIÇAP ÇİZGİSİ yazı çemberin dışındayken yazının altından sonuna uzanır")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0");            // 1
    r.run("ÖLÇÜ tur=yaricap nokta=5,0 konum=12,0"); // 2: the figure 7 m past the rim
    const core::EmitBuffer buf = outline(r, 2);
    double far                 = 0.0;
    for (std::size_t i = 0; i < buf.run_total(); ++i) {
        if (buf.run_solid[i] != 0) continue;
        const auto xs = buf.run_xs(i);
        const auto ys = buf.run_ys(i);
        for (std::size_t v = 0; v < xs.size(); ++v)
            far = std::max(far, length(Point2{0, 0}, Point2{xs[v], ys[v]}));
    }
    // The caption "R5,00" is 7,5 m wide and centred 12 m out: the line reaches
    // its far end, past 15 m.
    CHECK_GT(far, 15'000.0);
    CHECK_EQ(r.def(2).measurement, 5'000);
}

TEST_CASE("ÖLÇÜ ÖNİZLEMESİ: yük ve yazı yüksekliği birlikte gider, eksik bayt reddedilir")
{
    DimensionDef d;
    d.type           = DimensionType::Radial;
    d.prefix         = "R";
    const auto bytes = core::encode_dimension_guide({d, 2'500});
    const auto back  = core::decode_dimension_guide(bytes);
    REQUIRE(back.has_value());
    CHECK_EQ(back->text_height, 2'500);
    CHECK(back->def == d);
    CHECK_FALSE(core::decode_dimension_guide(std::span<const std::uint8_t>(bytes).first(4)));
}
