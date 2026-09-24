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

TEST_CASE("YARIÇAP ÇİZGİSİ yazısına döner: yazı nereye konursa çizgi oraya uzanır, yarıçap aynı")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0");                        // 1
    r.run("ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=0,8"); // 2: straight up
    const auto d = r.defs(2);
    REQUIRE_EQ(d.size(), std::size_t{2});
    CHECK_EQ(d[0], (Point2{0, 0}));
    CHECK_EQ(d[1], (Point2{0, 5'000}));
    CHECK_EQ(r.def(2).measurement, 5'000);
    CHECK_EQ(r.caption(2), std::string("5,00"));

    // A diameter straddles the centre, both ends on the line to its figure —
    // the far one within a millimetre of the near one's mirror, placed so the
    // length is the diameter exactly.
    r.run("ÖLÇÜ tur=cap birinci=-5,0 ikinci=5,0 konum=-6,-6"); // 3
    const auto c = r.defs(3);
    REQUIRE_EQ(c.size(), std::size_t{2});
    CHECK_LE(std::abs(c[0].x + c[1].x), 1);
    CHECK_LE(std::abs(c[0].y + c[1].y), 1);
    CHECK_EQ(r.def(3).measurement, 10'000);
    CHECK_LT(between(Point2{0, 0}, c[1], Point2{-6'000, -6'000}), 1e-3);
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
    CHECK_EQ(r.caption(2), std::string("7,50"));
    const auto d = r.defs(2);
    CHECK_EQ(d[0], (Point2{10'000, 10'000}));
    CHECK_LT(between(d[0], d[1], Point2{16'000, 20'000}), 1e-3);
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
    CHECK_EQ(r.caption(2), std::string("24,69"));
    const auto d = r.defs(2);
    CHECK_LE(std::abs(d[0].x + d[1].x), 1);
    CHECK_LE(std::abs(d[0].y + d[1].y), 1);
    CHECK_LT(between(Point2{0, 0}, d[1], Point2{20'000, 20'000}), 1e-3);

    // AND A REBUILD KEEPS THEM: a diameter already aimed at its figure is not
    // re-aimed, so re-wording it cannot walk its ends round.
    r.run("ÖLÇÜDÜZENLE nesneler=2 onek=Ø");
    CHECK_EQ(r.defs(2), d);
    CHECK_EQ(r.caption(2), std::string("Ø24,69"));

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
