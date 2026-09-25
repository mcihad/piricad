// SPDX-License-Identifier: GPL-3.0-or-later
// Ellipses and splines as paths: one way to ask where a curve is, how long it
// is, where it points and where it meets another (TODOS C-01).
//
// Every expected value below is worked out by hand from the curve's own
// equation — a parabola as a quadratic spline, a quarter circle as a rational
// one, an ellipse by x²/a² + y²/b² = 1 — so the tests check the solve against
// the geometry, not against itself.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/spline.hpp"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using namespace kentos;
using core::CurvePath;
using core::PathPiece;
using core::Point2;

namespace {

constexpr std::int64_t kTurn = core::kUDegFullCircle;

/// An ellipse piece: centre, first axis end, second axis end, and the sweep of
/// its parameter from `start` (micro-degrees).
PathPiece ellipse(Point2 c, Point2 major, Point2 minor, std::int64_t start = 0,
                  std::int64_t sweep = kTurn)
{
    PathPiece p;
    p.kind        = PathPiece::Kind::Ellipse;
    p.centre      = c;
    p.major       = major;
    p.minor       = minor;
    p.start_udeg  = start;
    p.sweep_udeg  = sweep;
    const auto at = [&](std::int64_t udeg) {
        const core::SinCos sc = core::sin_cos_udeg(udeg);
        return Point2{c.x + core::mm_round((sc.cos * static_cast<double>(major.x - c.x)) +
                                           (sc.sin * static_cast<double>(minor.x - c.x))),
                      c.y + core::mm_round((sc.cos * static_cast<double>(major.y - c.y)) +
                                           (sc.sin * static_cast<double>(minor.y - c.y)))};
    };
    p.from = at(start);
    p.to   = at(start + sweep);
    return p;
}

/// A spline piece over clamped knots, its ends the first and last controls.
PathPiece spline(std::vector<Point2> controls, int degree, std::vector<std::int64_t> weights = {})
{
    PathPiece p;
    p.kind                = PathPiece::Kind::Spline;
    p.controls            = std::move(controls);
    p.spline.degree       = static_cast<std::uint8_t>(degree);
    p.spline.knots_nano   = core::uniform_clamped_knots(p.controls.size(), degree);
    p.spline.weights_nano = std::move(weights);
    p.spline.rational     = !p.spline.weights_nano.empty();
    p.from                = p.controls.front();
    p.to                  = p.controls.back();
    return p;
}

CurvePath path(PathPiece p, bool closed = false)
{
    CurvePath out;
    out.pieces.push_back(std::move(p));
    out.closed = closed;
    return out;
}

CurvePath segment(Point2 a, Point2 b)
{
    return path(PathPiece{.from = a, .to = b});
}

/// The parabola y = 2x − 0.2x² from (0, 0) to (10, 0), as the quadratic
/// Bézier (0,0)–(5,10)–(10,0): x = 10t, y = 20t(1 − t).
PathPiece parabola()
{
    return spline({{0, 0}, {5'000, 10'000}, {10'000, 0}}, 2);
}

bool near(Point2 a, Point2 b, core::Mm within = 1)
{
    return std::llabs(a.x - b.x) <= within && std::llabs(a.y - b.y) <= within;
}

} // namespace

TEST_CASE("C-01: elips ile doğru — iki kesişim, teğet dokunuş ve ıska ayrı sonuçlanır")
{
    // x²/100 + y²/25 = 1: at y = 3, x = ±8.
    const CurvePath e         = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}), true);
    const core::PathMeets two = core::path_meets(segment({-20'000, 3'000}, {20'000, 3'000}), e);
    REQUIRE_EQ(two.crossings.size(), std::size_t{2});
    CHECK_EQ(two.crossings[0].point, (Point2{-8'000, 3'000}));
    CHECK_EQ(two.crossings[1].point, (Point2{8'000, 3'000}));
    CHECK_FALSE(two.crossings[0].touching);
    CHECK_FALSE(two.unresolved);

    // y = 5 grazes the top: one point, touching.
    const core::PathMeets graze = core::path_meets(segment({-20'000, 5'000}, {20'000, 5'000}), e);
    REQUIRE_EQ(graze.crossings.size(), std::size_t{1});
    CHECK(near(graze.crossings[0].point, Point2{0, 5'000}));
    CHECK(graze.crossings[0].touching);

    // y = 6 misses — and a miss is not a failure to decide.
    const core::PathMeets miss = core::path_meets(segment({-20'000, 6'000}, {20'000, 6'000}), e);
    CHECK(miss.crossings.empty());
    CHECK_FALSE(miss.unresolved);
}

TEST_CASE("C-01: elips ile daire ve elips ile elips — çoklu çözüm tam noktalarında")
{
    const CurvePath e = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}), true);
    // r² = 73 m²: x² = 64, y² = 9 — four meets at (±8, ±3).
    CurvePath circle;
    circle.closed = true;
    circle.pieces.push_back(PathPiece{.kind       = PathPiece::Kind::Arc,
                                      .from       = {8'544, 0},
                                      .to         = {8'544, 0},
                                      .centre     = {0, 0},
                                      .radius     = 8'544,
                                      .sweep_udeg = kTurn});
    const std::vector<core::PathCrossing> four = core::path_crossings(e, circle);
    REQUIRE_EQ(four.size(), std::size_t{4});
    for (const Point2 expected : {Point2{8'000, 3'000}, Point2{-8'000, 3'000},
                                  Point2{-8'000, -3'000}, Point2{8'000, -3'000}}) {
        bool found = false;
        for (const core::PathCrossing& c : four)
            found = found || near(c.point, expected);
        CHECK_MESSAGE(found, expected.x << "," << expected.y);
    }

    // The same ellipse turned a quarter: x² = y² = 20 m², four meets.
    const CurvePath turned = path(ellipse({0, 0}, {0, 10'000}, {-5'000, 0}), true);
    const std::vector<core::PathCrossing> cross = core::path_crossings(e, turned);
    REQUIRE_EQ(cross.size(), std::size_t{4});
    for (const core::PathCrossing& c : cross) {
        CHECK(std::llabs(std::llabs(c.point.x) - 4'472) <= 1);
        CHECK(std::llabs(std::llabs(c.point.y) - 4'472) <= 1);
    }
}

TEST_CASE("C-01: spline ile doğru — parabolün iki kesişimi ve tepesindeki teğet")
{
    // 0.2x² − 2x + 3.2 = 0 at y = 3.2: x = 2 and x = 8.
    const CurvePath curve = path(parabola());
    const std::vector<core::PathCrossing> two =
        core::path_crossings(curve, segment({-1'000, 3'200}, {11'000, 3'200}));
    REQUIRE_EQ(two.size(), std::size_t{2});
    CHECK(near(two[0].point, Point2{2'000, 3'200}));
    CHECK(near(two[1].point, Point2{8'000, 3'200}));
    CHECK(two[0].at.t < two[1].at.t);

    // The vertex (5, 5) is touched by y = 5.
    const std::vector<core::PathCrossing> top =
        core::path_crossings(curve, segment({-1'000, 5'000}, {11'000, 5'000}));
    REQUIRE_EQ(top.size(), std::size_t{1});
    CHECK(near(top[0].point, Point2{5'000, 5'000}));
    CHECK(top[0].touching);
}

TEST_CASE("C-01: rasyonel spline — ağırlıklı çeyrek daire gerçek çemberdir")
{
    // (10,0)–(10,10)–(0,10), weights 1, √2/2, 1: the quarter circle of radius
    // 10 round the origin. It meets y = x at 10/√2 on both axes.
    const CurvePath quarter = path(spline({{10'000, 0}, {10'000, 10'000}, {0, 10'000}}, 2,
                                          {core::kNano, 707'106'781, core::kNano}));
    const std::vector<core::PathCrossing> meet =
        core::path_crossings(quarter, segment({0, 0}, {20'000, 20'000}));
    REQUIRE_EQ(meet.size(), std::size_t{1});
    CHECK(near(meet[0].point, Point2{7'071, 7'071}));
    // Every point of it is ten metres from the centre.
    for (const double t : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        const Point2 p = core::point_at(quarter, core::PathPlace{0, t});
        CHECK(std::abs(std::sqrt(static_cast<double>(p.x) * static_cast<double>(p.x) +
                                 static_cast<double>(p.y) * static_cast<double>(p.y)) -
                       10'000.0) <= 1.0);
    }
    // Its length is a quarter of 2π·10.
    CHECK(std::llabs(core::path_length(quarter) - 15'708) <= 1);
}

TEST_CASE("C-01: uzunluk, yön, en yakın nokta ve kutu eğrinin kendisinden")
{
    // The parabola's length: 2.5 (2√5 + asinh 2) = 14.789 m.
    const CurvePath curve = path(parabola());
    CHECK(std::llabs(core::path_length(curve) - 14'789) <= 1);
    // The ellipse a = 10, b = 5: Ramanujan's perimeter 48.442 m.
    const CurvePath e = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}), true);
    CHECK(std::llabs(core::path_length(e) - 48'442) <= 1);

    // Walked counter-clockwise from the first axis, the ellipse leaves due north.
    CHECK_EQ(core::direction_at(e, core::PathPlace{0, 0.0}), 90'000'000);
    // The parabola rises at 2 in 1 from its start: atan 2 = 63.43°.
    CHECK(std::llabs(core::direction_at(curve, core::PathPlace{0, 0.0}) - 63'434'949) <= 2);

    // The place nearest (5, 6) is the vertex, halfway along.
    const core::PathPlace mid = core::place_of(curve, Point2{5'000, 6'000});
    CHECK(std::abs(mid.t - 0.5) < 1e-6);
    CHECK(near(core::point_at(curve, mid), Point2{5'000, 5'000}));

    // Halfway by length is halfway by symmetry.
    const core::PathPlace half = core::place_at_length(curve, 14'789 / 2);
    CHECK(near(core::point_at(curve, half), Point2{5'000, 5'000}, 2));

    // The box is the curve's, not its control polygon's: the top is 5 m, not 10.
    const core::Box2 box = core::path_bounds(curve);
    CHECK_EQ(box.min_x, 0);
    CHECK_EQ(box.max_x, 10'000);
    CHECK_EQ(box.min_y, 0);
    CHECK_EQ(box.max_y, 5'000);
    const core::Box2 ebox = core::path_bounds(e);
    CHECK_EQ(ebox.max_x, 10'000);
    CHECK_EQ(ebox.max_y, 5'000);
}

TEST_CASE("C-01: spline kesilince kısa eğri tam olarak aynı çizgide kalır")
{
    const CurvePath curve = path(parabola());
    const CurvePath middle =
        core::sub_path(curve, core::PathPlace{0, 0.25}, core::PathPlace{0, 0.75});
    REQUIRE_EQ(middle.pieces.size(), std::size_t{1});
    const PathPiece& piece = middle.pieces.front();
    REQUIRE(piece.kind == PathPiece::Kind::Spline);
    CHECK_EQ(piece.from, (Point2{2'500, 3'750}));
    CHECK_EQ(piece.to, (Point2{7'500, 3'750}));
    // Knot insertion keeps the shape: its middle is the parabola's vertex.
    CHECK(near(core::point_at(middle, core::PathPlace{0, 0.5}), Point2{5'000, 5'000}));

    // Written as a spline, ends where it was cut, read back the same.
    const core::PathRecord rec = core::path_record(middle);
    CHECK(rec.kind == core::kSplineKind);
    CHECK_EQ(rec.ring.front(), (Point2{2'500, 3'750}));
    CHECK_EQ(rec.ring.back(), (Point2{7'500, 3'750}));
    auto def = core::decode_spline(rec.payload);
    REQUIRE(def);
    CHECK_EQ(def.value().knots_nano.size(), rec.ring.size() + 3);

    // Turned round, it runs the other way over the same points.
    const CurvePath back = core::reversed(curve);
    CHECK_EQ(back.pieces.front().from, (Point2{10'000, 0}));
    CHECK(near(core::point_at(back, core::PathPlace{0, 0.8}),
               core::point_at(curve, core::PathPlace{0, 0.2})));

    // And split at its vertex, the two halves meet there and join into one again.
    const std::vector<CurvePath> halves = core::split_path(curve, {core::PathPlace{0, 0.5}});
    REQUIRE_EQ(halves.size(), std::size_t{2});
    CHECK_EQ(halves[0].pieces.back().to, (Point2{5'000, 5'000}));
    CHECK_EQ(halves[1].pieces.front().from, (Point2{5'000, 5'000}));
    CurvePath rejoined;
    rejoined.pieces              = {halves[0].pieces.front(), halves[1].pieces.front()};
    const core::PathRecord whole = core::path_record(rejoined);
    CHECK(whole.kind == core::kSplineKind);
    CHECK_EQ(whole.ring.front(), (Point2{0, 0}));
    CHECK_EQ(whole.ring.back(), (Point2{10'000, 0}));
}

TEST_CASE("C-01: elips kesilince yayları kendi parametresiyle kaydedilir")
{
    const CurvePath e = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}), true);
    // Cut at the top and at the bottom: the left half and the right half.
    const std::vector<CurvePath> halves =
        core::split_path(e, {core::PathPlace{0, 0.25}, core::PathPlace{0, 0.75}});
    REQUIRE_EQ(halves.size(), std::size_t{2});
    const core::PathRecord left = core::path_record(halves[0]);
    CHECK(left.kind == core::kEllipseKind);
    CHECK_EQ(left.ring, (std::vector<Point2>{{0, 0}, {10'000, 0}, {0, 5'000}}));
    auto arc = core::decode_ellipse_arc(left.payload);
    REQUIRE(arc);
    CHECK_EQ(arc.value().start_udeg, 90'000'000);
    CHECK_EQ(arc.value().end_udeg, 270'000'000);
    // The other half runs across the seam and is still one arc.
    const core::PathRecord right = core::path_record(halves[1]);
    auto other                   = core::decode_ellipse_arc(right.payload);
    REQUIRE(other);
    CHECK_EQ(other.value().start_udeg, 270'000'000);
    CHECK_EQ(other.value().end_udeg, 90'000'000);
    // Walked clockwise, an arc is still stored counter-clockwise.
    const core::PathRecord cw = core::path_record(core::reversed(halves[0]));
    auto stored               = core::decode_ellipse_arc(cw.payload);
    REQUIRE(stored);
    CHECK_EQ(stored.value().start_udeg, 90'000'000);
    CHECK_EQ(stored.value().end_udeg, 270'000'000);
}

TEST_CASE("C-01: ortak bölüm nokta değildir — aynı doğru, aynı çember, aynı elips")
{
    // Two segments on one line share a stretch.
    const core::PathMeets line =
        core::path_meets(segment({0, 0}, {10'000, 0}), segment({4'000, 0}, {20'000, 0}));
    CHECK(line.crossings.empty());
    REQUIRE_EQ(line.overlaps.size(), std::size_t{1});
    CHECK(std::abs(line.overlaps[0].from.t - 0.4) < 1e-9);
    CHECK(std::abs(line.overlaps[0].to.t - 1.0) < 1e-9);

    // Two arcs of one circle.
    const auto arc = [](Point2 from, Point2 to, std::int64_t sweep) {
        return path(PathPiece{.kind       = PathPiece::Kind::Arc,
                              .from       = from,
                              .to         = to,
                              .centre     = {0, 0},
                              .radius     = 10'000,
                              .sweep_udeg = sweep});
    };
    const core::PathMeets circle = core::path_meets(arc({10'000, 0}, {-10'000, 0}, 180'000'000),
                                                    arc({0, 10'000}, {0, -10'000}, 180'000'000));
    CHECK(circle.crossings.empty());
    CHECK_EQ(circle.overlaps.size(), std::size_t{1});

    // One ellipse, two partial arcs of it.
    const CurvePath a = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}, 0, 180'000'000));
    const CurvePath b = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}, 90'000'000, 180'000'000));
    const core::PathMeets shared = core::path_meets(a, b);
    CHECK(shared.crossings.empty());
    REQUIRE_EQ(shared.overlaps.size(), std::size_t{1});
    CHECK(std::abs(shared.overlaps[0].from.t - 0.5) < 1e-9);
    CHECK(std::abs(shared.overlaps[0].to.t - 1.0) < 1e-9);

    // A straight spline lying along a segment shares the stretch, and is not
    // reported as a scatter of points.
    const CurvePath straight    = path(spline({{0, 0}, {5'000, 0}, {10'000, 0}}, 2));
    const core::PathMeets along = core::path_meets(straight, segment({2'000, 0}, {8'000, 0}));
    CHECK(along.crossings.empty());
    CHECK_FALSE(along.overlaps.empty());
}

TEST_CASE("C-01: belgedeki elips ve spline yalnız istenince yoldur")
{
    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    command::register_builtin_commands(reg);
    REQUIRE(
        bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", command::Origin::Test).ok());
    REQUIRE(bus.execute_line("ELİPS merkez=30,0 birinci=40,0 ikinci=30,5 baslangic=0 bitis=90",
                             command::Origin::Test)
                .ok());
    REQUIRE(
        bus.execute_line("SPLINE noktalar=0,20 5,30 10,20 derece=2", command::Origin::Test).ok());

    // The tools that bend and join see no path; the ones that cut do.
    CHECK_FALSE(core::path_of(doc, 0).has_value());
    const auto whole = core::path_of(doc, 0, core::PathScope::Curves);
    REQUIRE(whole.has_value());
    CHECK(whole->closed);
    CHECK_EQ(whole->pieces.front().from, (Point2{10'000, 0}));

    const auto quarter = core::path_of(doc, 1, core::PathScope::Curves);
    REQUIRE(quarter.has_value());
    CHECK_FALSE(quarter->closed);
    CHECK_EQ(quarter->pieces.front().from, (Point2{40'000, 0}));
    CHECK_EQ(quarter->pieces.front().to, (Point2{30'000, 5'000}));

    const auto curve = core::path_of(doc, 2, core::PathScope::Curves);
    REQUIRE(curve.has_value());
    CHECK_EQ(curve->pieces.front().from, (Point2{0, 20'000}));
    CHECK_EQ(curve->pieces.front().to, (Point2{10'000, 20'000}));
    // It is the same parabola, twenty metres up.
    CHECK(near(core::point_at(*curve, core::PathPlace{0, 0.5}), Point2{5'000, 25'000}));
}

TEST_CASE("C-01: kapalı eğrinin dikişi tek yerdir — başlangıçtaki kesişim bir kez sayılır")
{
    // The whole ellipse starts and ends at (10, 0). A line through that point
    // at 45° crosses there once — found at t ≈ 0 and t ≈ 1, counted once — and
    // leaves through the ellipse again at x = 6 (x²/100 + (x−10)²/25 = 1).
    const CurvePath e = path(ellipse({0, 0}, {10'000, 0}, {0, 5'000}), true);
    const std::vector<core::PathCrossing> through =
        core::path_crossings(e, segment({12'000, 2'000}, {0, -10'000}));
    REQUIRE_EQ(through.size(), std::size_t{2});
    CHECK(near(through[0].point, Point2{10'000, 0}));
    CHECK(near(through[1].point, Point2{6'000, -4'000}));

    // The tangent at the seam touches it once.
    const std::vector<core::PathCrossing> seam =
        core::path_crossings(e, segment({10'000, -8'000}, {10'000, 8'000}));
    REQUIRE_EQ(seam.size(), std::size_t{1});
    CHECK(near(seam[0].point, Point2{10'000, 0}));
    CHECK(seam[0].touching);
}
