// SPDX-License-Identifier: GPL-3.0-or-later
//
// OFSET KEEPS AN ARC AN ARC (TODOS O-4, CLAUDE.md 2.11). A parcel rounded at a
// corner used to be offset as its chords — its parallel a fan of short edges
// and a sentence saying how far the drawing strayed from the curve — and a
// round corner asked of a straight line was a fan too. The geometry kernel
// answers both: an arc polyline's parallel has concentric arcs, a round corner
// is a true arc about the corner. The figures are closed forms: a quarter
// circle of radius r cut from a corner square leaves r² − πr²/4.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/kernel.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::CurvePath;
using core::PathPiece;
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
        if (!r) FAIL_WITH(line, r.error().message);
    }

    core::EntityId slot(std::uint64_t key) const { return doc.slot_of(core::EntityKey{key}); }

    /// The object's path, arcs as arcs; empty when it has none.
    CurvePath path(std::uint64_t key) const
    {
        const auto p = core::path_of(doc, slot(key));
        return p ? *p : CurvePath{};
    }
};

/// The arcs of a path.
std::vector<PathPiece> arcs_of(const CurvePath& path)
{
    std::vector<PathPiece> out;
    for (const PathPiece& piece : path.pieces)
        if (piece.kind == PathPiece::Kind::Arc) out.push_back(piece);
    return out;
}

/// A face's area in square millimetres, as a double, for a closed form.
double area_of(const CurvePath& path)
{
    return std::fabs(static_cast<double>(core::path_area(path)));
}

/// The rectangle 20 × 10 m with its north-east corner rounded to 2 m: one arc,
/// centre (18, 8).
void rounded_parcel(Rig& r)
{
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    REQUIRE(r.doc.entities().kind[r.slot(1)] == core::kArcPolylineKind);
}

constexpr double kPi = std::numbers::pi;

} // namespace

TEST_CASE("OFSET YAY: yaylı parselin dış paraleli aynı merkezli yayla, alanı kapalı biçimden")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    rounded_parcel(r);
    r.said.clear();
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis kose=KÖŞE");

    // ONE OBJECT, AN ARC POLYLINE, NOT A FAN: said without the "as drawn" note.
    CHECK(r.said.find("kendi türünde paraleli olmayan") == std::string::npos);
    REQUIRE(r.slot(2) != core::kNoEntity);
    CHECK(r.doc.entities().kind[r.slot(2)] == core::kArcPolylineKind);
    const CurvePath grown = r.path(2);
    CHECK(grown.closed);
    const std::vector<PathPiece> arcs = arcs_of(grown);
    REQUIRE_EQ(arcs.size(), 1u);
    CHECK_EQ(arcs.front().centre, Point2{18'000, 8'000});
    CHECK_EQ(arcs.front().radius, core::Mm{3'000});
    // 22 × 12 m, its corner cut by a quarter circle of 3 m: 264 − (9 − 9π/4).
    CHECK(std::fabs(area_of(grown) - ((264.0 - (9.0 - (9.0 * kPi / 4.0))) * 1e6)) < 5'000.0);
    // Sharp elsewhere: the three straight corners a metre out.
    std::vector<Point2> corners;
    for (const PathPiece& piece : grown.pieces)
        if (piece.kind == PathPiece::Kind::Segment) corners.push_back(piece.from);
    CHECK(std::ranges::find(corners, Point2{-1'000, -1'000}) != corners.end());
    CHECK(std::ranges::find(corners, Point2{21'000, -1'000}) != corners.end());
    CHECK(std::ranges::find(corners, Point2{-1'000, 11'000}) != corners.end());

    // The source is untouched, the parallel is one undo step.
    CHECK(r.doc.entities().kind[r.slot(1)] == core::kArcPolylineKind);
    r.run("GERİAL");
    CHECK((r.slot(2) == core::kNoEntity || !r.doc.alive(r.slot(2))));
    CHECK(r.doc.alive(r.slot(1)));
}

TEST_CASE("OFSET YAY: iç paralelde yay küçülüyor; yarıçaptan derine inince yay kalkıyor")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    rounded_parcel(r);
    r.run("OFSET nesneler=1 mesafe=1000 taraf=ic kose=KÖŞE");
    const CurvePath shrunk            = r.path(2);
    const std::vector<PathPiece> arcs = arcs_of(shrunk);
    REQUIRE_EQ(arcs.size(), 1u);
    CHECK_EQ(arcs.front().centre, Point2{18'000, 8'000});
    CHECK_EQ(arcs.front().radius, core::Mm{1'000});
    // 18 × 8 m, its corner cut by a quarter circle of 1 m: 144 − (1 − π/4).
    CHECK(std::fabs(area_of(shrunk) - ((144.0 - (1.0 - (kPi / 4.0))) * 1e6)) < 5'000.0);

    // Three metres in, past the arc's 2 m radius: the corner comes to a point,
    // a plain area again.
    r.run("OFSET nesneler=1 mesafe=3000 taraf=ic kose=KÖŞE");
    REQUIRE(r.slot(3) != core::kNoEntity);
    CHECK(r.doc.entities().kind[r.slot(3)] == core::kPolylineKind);
    CHECK(std::fabs(area_of(r.path(3)) - (14.0 * 4.0 * 1e6)) < 5'000.0);
}

TEST_CASE("OFSET YAY: düz dikdörtgenin yuvarlak köşeli paraleli köşelerde gerçek yay")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis kose=YUVARLAK");
    REQUIRE(r.doc.entities().kind[r.slot(2)] == core::kArcPolylineKind);
    const std::vector<PathPiece> arcs = arcs_of(r.path(2));
    REQUIRE_EQ(arcs.size(), 4u);
    for (const PathPiece& arc : arcs)
        CHECK_EQ(arc.radius, core::Mm{1'000});
    std::vector<Point2> centres;
    for (const PathPiece& arc : arcs)
        centres.push_back(arc.centre);
    for (const Point2 corner :
         {Point2{0, 0}, Point2{20'000, 0}, Point2{20'000, 10'000}, Point2{0, 10'000}})
        CHECK(std::ranges::find(centres, corner) != centres.end());
    // 200 + a metre along each side + a whole circle of 1 m at the four corners.
    CHECK(std::fabs(area_of(r.path(2)) - ((200.0 + 60.0 + kPi) * 1e6)) < 5'000.0);

    // A SHARP CORNER IS THE POLYGON ROAD STILL: the same rectangle, mitred.
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis kose=KÖŞE");
    CHECK(r.doc.entities().kind[r.slot(3)] == core::kPolylineKind);
    CHECK(std::fabs(area_of(r.path(3)) - (22.0 * 12.0 * 1e6)) < 1.0);
}

TEST_CASE("OFSET YAY: açık L'nin dış yanı köşede yay, iç yanı keskin köşe")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10"); // east, then north: the corner's outside is the right
    r.run("OFSET nesneler=1 mesafe=1000 taraf=sag kose=YUVARLAK");
    REQUIRE(r.doc.entities().kind[r.slot(2)] == core::kArcPolylineKind);
    const CurvePath right = r.path(2);
    CHECK_FALSE(right.closed);
    CHECK_EQ(right.pieces.front().from, Point2{0, -1'000});
    CHECK_EQ(right.pieces.back().to, Point2{11'000, 10'000});
    const std::vector<PathPiece> arcs = arcs_of(right);
    REQUIRE_EQ(arcs.size(), 1u);
    CHECK_EQ(arcs.front().centre, Point2{10'000, 0});
    CHECK_EQ(arcs.front().radius, core::Mm{1'000});

    // The inside of a corner is trimmed, not rounded: a plain line.
    r.run("OFSET nesneler=1 mesafe=1000 taraf=sol kose=YUVARLAK");
    REQUIRE(r.doc.entities().kind[r.slot(3)] == core::kPolylineKind);
    const CurvePath left = r.path(3);
    CHECK(arcs_of(left).empty());
    CHECK_EQ(left.pieces.front().from, Point2{0, 1'000});
    CHECK_EQ(left.pieces.back().to, Point2{9'000, 10'000});
}

TEST_CASE("OFSET YAY: açık yaylı çizginin iki yanı aynı merkezli iki yay")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0 20,10");
    r.run("YUVARLA nesne=1 nokta=20,0 yaricap=2"); // the bend turns left: centre (18, 2)
    REQUIRE(r.doc.entities().kind[r.slot(1)] == core::kArcPolylineKind);
    r.said.clear();
    r.run("OFSET nesneler=1 mesafe=1000 taraf=sol");
    r.run("OFSET nesneler=1 mesafe=1000 taraf=sag");
    CHECK(r.said.find("kendi türünde paraleli olmayan") == std::string::npos);
    const std::vector<PathPiece> inner = arcs_of(r.path(2));
    const std::vector<PathPiece> outer = arcs_of(r.path(3));
    REQUIRE_EQ(inner.size(), 1u);
    REQUIRE_EQ(outer.size(), 1u);
    CHECK_EQ(inner.front().centre, Point2{18'000, 2'000});
    CHECK_EQ(inner.front().radius, core::Mm{1'000});
    CHECK_EQ(outer.front().centre, Point2{18'000, 2'000});
    CHECK_EQ(outer.front().radius, core::Mm{3'000});
    // Its ends a metre off the source's, the travel kept.
    CHECK_EQ(r.path(2).pieces.front().from, Point2{0, 1'000});
    CHECK_EQ(r.path(3).pieces.back().to, Point2{21'000, 10'000});
}

TEST_CASE("OFSET YAY: önizleme sonucun kendisi — tuvalin çağırdığı paralel yazılanla aynı yol")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    rounded_parcel(r);
    auto shown = core::entity_parallel(r.doc, r.slot(1), 1'000, core::ParallelSide::Outside,
                                       core::JoinStyle::Miter);
    REQUIRE(shown.ok());
    REQUIRE_EQ(shown.value().pieces.size(), 1u);
    CHECK(shown.value().pieces.front().shape == core::ParallelPiece::Shape::Path);
    CHECK_FALSE(shown.value().approximate);
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis");
    CHECK(r.path(2) == shown.value().pieces.front().path);
}

TEST_CASE("OFSET YAY: arayüz, komut satırı ve betik aynı belge, aynı günlük")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    // THE HAND: the distance typed, the side shown with the cursor outside.
    Rig gui;
    rounded_parcel(gui);
    auto started = gui.bus.begin_interactive("OFSET nesneler=1", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::number(1.0)).ok()); // metres, as the prompt asks
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::point({30'000, 20'000})).ok());
    auto done = gui.bus.finish(s);
    if (!done) MESSAGE(done.error().message);
    REQUIRE(done.ok());

    Rig cli;
    rounded_parcel(cli);
    cli.run("OFSET nesneler=1 mesafe=1000 nokta=30,20");

    Rig scr;
    rounded_parcel(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto ran = runner.run_text(R"({
            "ad": "Yaylı ofset kanıtı",
            "komutlar": [
                {"cmd": "core.offset",
                 "args": {"nesneler": [1], "mesafe": 1000, "nokta": [30000, 20000]}}
            ]
        })");
        if (!ran) FAIL_WITH("betik", ran.error().message);
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    const auto last = [](const Journal& j) { return j.entries().back(); };
    CHECK(last(gui.journal).args == last(cli.journal).args);
    CHECK(last(cli.journal).args == last(scr.journal).args);

    // And the journal replays into the same document.
    Rig replay;
    for (const auto& e : cli.journal.entries()) {
        auto again = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        if (!again) FAIL_WITH(e.command_id.c_str(), again.error().message);
    }
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
}

TEST_CASE("OFSET YAY: yaylı çizginin pahlı paraleli köşede düz kiriş, kendi yayı yay")
{
    if (!core::kernel_available()) PENDING("KENTOS_WITH_OCCT=OFF; geometri çekirdeği yok.");
    // A rectangle rounded at one corner and offset out with bevelled corners:
    // the three square corners are cut straight, the arc stays an arc.
    Rig r;
    rounded_parcel(r);
    r.said.clear();
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis kose=PAH");
    CHECK(r.said.find("kendi türünde paraleli olmayan") == std::string::npos);
    const CurvePath cut               = r.path(2);
    const std::vector<PathPiece> arcs = arcs_of(cut);
    REQUIRE_EQ(arcs.size(), 1u);
    CHECK_EQ(arcs.front().centre, Point2{18'000, 8'000});
    CHECK_EQ(arcs.front().radius, core::Mm{3'000});
    // The bevel at (0, 0): from a metre below the corner to a metre left of it.
    bool bevelled = false;
    for (const PathPiece& piece : cut.pieces)
        if (piece.kind == PathPiece::Kind::Segment &&
            ((piece.from == Point2{-1'000, 0} && piece.to == Point2{0, -1'000}) ||
             (piece.from == Point2{0, -1'000} && piece.to == Point2{-1'000, 0})))
            bevelled = true;
    CHECK(bevelled);
}

TEST_CASE("OFSET YAY: delikli alanın yuvarlak köşeli paraleli kısa kenarlarla, söylenerek")
{
    // An arc polyline has one ring (model.md R9b), so the parallel is the face
    // with its hole, its round corners drawn as short edges — and said.
    Rig r;
    r.run("ALAN 0,0 20,0 20,20 0,20 bolum=4 5,5 15,5 15,15 5,15 bolum=4");
    r.said.clear();
    r.run("OFSET nesneler=1 mesafe=1000 taraf=dis kose=YUVARLAK");
    CHECK(r.said.find("kısa kenarlarla") != std::string::npos);
    CHECK(r.doc.entities().kind[r.slot(2)] == core::kPolylineKind);
    CHECK(r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]).count == 2);
}
