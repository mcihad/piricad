// SPDX-License-Identifier: GPL-3.0-or-later
//
// A curve walked piece by piece, and where two curves meet (TODOS C-01, C-04).
//
// Every expected point below is a closed form a surveyor can check by hand —
// a 3-4-5 triangle, a 30-60-90 one — and is asserted to the millimetre, because
// a trim that stops a millimetre off the boundary leaves a gap a topology check
// then reports as a defect (§7.3).
#include "kentos_test.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/trim_curve.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace kentos::core;

namespace {

CurvePath segment(Point2 a, Point2 b)
{
    CurvePath p;
    p.pieces.push_back(PathPiece{.from = a, .to = b});
    return p;
}

CurvePath circle(Point2 c, Mm r)
{
    CurvePath p;
    const Point2 seam{c.x + r, c.y};
    p.pieces.push_back(PathPiece{.kind       = PathPiece::Kind::Arc,
                                 .from       = seam,
                                 .to         = seam,
                                 .centre     = c,
                                 .radius     = r,
                                 .sweep_udeg = kUDegFullCircle});
    p.closed = true;
    return p;
}

CurvePath ring(std::vector<Point2> corners)
{
    CurvePath p;
    for (std::size_t i = 0; i < corners.size(); ++i)
        p.pieces.push_back(PathPiece{.from = corners[i], .to = corners[(i + 1) % corners.size()]});
    p.closed = true;
    return p;
}

} // namespace

TEST_CASE("CURVE: iki doğru parçası tek noktada kesişir")
{
    const auto cuts =
        path_crossings(segment({0, 0}, {100'000, 0}), segment({40'000, -20'000}, {40'000, 20'000}));
    REQUIRE_EQ(cuts.size(), 1u);
    CHECK_EQ(cuts[0].point, (Point2{40'000, 0}));
    CHECK_EQ(cuts[0].at.piece, 0u);
    CHECK(cuts[0].at.t == doctest::Approx(0.4));
    CHECK_FALSE(cuts[0].touching);
}

TEST_CASE("CURVE: doğru daireyi iki yerde keser, sırası doğru boyuncadır")
{
    // 6 m off the centre of a 10 m circle: x = ±sqrt(100 - 36) = ±8.
    const auto cuts =
        path_crossings(segment({-20'000, 6'000}, {20'000, 6'000}), circle({0, 0}, 10'000));
    REQUIRE_EQ(cuts.size(), 2u);
    CHECK_EQ(cuts[0].point, (Point2{-8'000, 6'000}));
    CHECK_EQ(cuts[1].point, (Point2{8'000, 6'000}));
}

TEST_CASE("CURVE: teğet bir doğru daireye tek noktada DOKUNUR")
{
    const auto cuts =
        path_crossings(segment({-20'000, 10'000}, {20'000, 10'000}), circle({0, 0}, 10'000));
    REQUIRE_EQ(cuts.size(), 1u);
    CHECK_EQ(cuts[0].point, (Point2{0, 10'000}));
    CHECK(cuts[0].touching);
}

TEST_CASE("CURVE: iki daire iki yerde kesişir")
{
    // Centres 12 m apart, both 10 m: the meets are at x = 6, y = ±8.
    const auto cuts = path_crossings(circle({0, 0}, 10'000), circle({12'000, 0}, 10'000));
    REQUIRE_EQ(cuts.size(), 2u);
    // Walked counter-clockwise from the first circle's seam at (10, 0): the
    // upper meet comes first.
    CHECK_EQ(cuts[0].point, (Point2{6'000, 8'000}));
    CHECK_EQ(cuts[1].point, (Point2{6'000, -8'000}));
}

TEST_CASE("CURVE: çakışan iki doğru bir NOKTA değildir; köşedeki kesişim bir kez sayılır")
{
    // A shared stretch is not a point, and is not reported as an arbitrary one.
    CHECK(path_crossings(segment({0, 0}, {10'000, 0}), segment({5'000, 0}, {15'000, 0})).empty());

    // A diagonal through the corner of an L meets both legs there — once.
    CurvePath l;
    l.pieces.push_back(PathPiece{.from = {0, 0}, .to = {10'000, 0}});
    l.pieces.push_back(PathPiece{.from = {10'000, 0}, .to = {10'000, 10'000}});
    const auto cuts = path_crossings(l, segment({5'000, -5'000}, {15'000, 5'000}));
    REQUIRE_EQ(cuts.size(), 1u);
    CHECK_EQ(cuts[0].point, (Point2{10'000, 0}));
}

TEST_CASE("CURVE: kapalı yolun bir parçası dikişin üstünden sarar")
{
    // From the middle of the west edge to the middle of the south one: past the
    // corner where the ring starts and ends.
    const CurvePath square = ring({{0, 0}, {10'000, 0}, {10'000, 10'000}, {0, 10'000}});
    const CurvePath part =
        sub_path(square, PathPlace{.piece = 3, .t = 0.5}, PathPlace{.piece = 0, .t = 0.5});
    CHECK(path_vertices(part) == std::vector<Point2>{{0, 5'000}, {0, 0}, {5'000, 0}});
}

TEST_CASE("CURVE: dairenin dikişi aşan parçası TEK yaydır")
{
    // A quarter before the seam to a quarter after: the east half, as one arc
    // swept 180° from the south point — not two arcs meeting at the seam.
    const CurvePath half = sub_path(circle({0, 0}, 10'000), PathPlace{.piece = 0, .t = 0.75},
                                    PathPlace{.piece = 0, .t = 0.25});
    REQUIRE_EQ(half.pieces.size(), 1u);
    CHECK(half.pieces[0].kind == PathPiece::Kind::Arc);
    CHECK_EQ(half.pieces[0].from, (Point2{0, -10'000}));
    CHECK_EQ(half.pieces[0].to, (Point2{0, 10'000}));
    CHECK_EQ(half.pieces[0].sweep_udeg, kUDegFullCircle / 2);
}

TEST_CASE("CURVE: doğrunun ucu daireye doğrultusunda uzar")
{
    const std::array<CurvePath, 1> edges{circle({0, 0}, 10'000)};
    auto reach = extend_curve(segment({-20'000, 0}, {-15'000, 0}), edges, Point2{-15'500, 0});
    REQUIRE(reach.ok());
    CHECK(path_vertices(reach.value().extended) == std::vector<Point2>{{-20'000, 0}, {-10'000, 0}});
    CHECK(path_vertices(reach.value().added) == std::vector<Point2>{{-15'000, 0}, {-10'000, 0}});
}

TEST_CASE("CURVE: kesişmeyen budama ve tek yerden kesilen kapalı şekil söylenerek reddedilir")
{
    const std::array<CurvePath, 1> far{segment({80'000, -20'000}, {80'000, 20'000})};
    auto none = trim_curve(segment({0, 0}, {50'000, 0}), far, Point2{40'000, 0});
    REQUIRE_FALSE(none.ok());
    CHECK(none.error().message.find("kesmiyor") != std::string::npos);

    // A line that only touches the circle cuts it in one place.
    const std::array<CurvePath, 1> tangent{segment({-20'000, 10'000}, {20'000, 10'000})};
    auto once = trim_curve(circle({0, 0}, 10'000), tangent, Point2{-10'000, 0});
    REQUIRE_FALSE(once.ok());
    CHECK(once.error().message.find("1 yerinden") != std::string::npos);

    // A closed shape has no end to extend.
    auto closed = extend_curve(circle({0, 0}, 10'000), far, Point2{10'000, 0});
    REQUIRE_FALSE(closed.ok());
}

TEST_CASE("CURVE: budama önizlemesinin baytları gidip gelir, bozuğu reddedilir")
{
    const TrimGuide guide{.extend = true, .every = false, .keys = {3, 7, 42}};
    auto back = decode_trim_guide(encode_trim_guide(guide));
    REQUIRE(back.ok());
    CHECK(back.value().extend);
    CHECK_FALSE(back.value().every);
    CHECK(back.value().keys == std::vector<std::int64_t>{3, 7, 42});

    std::vector<std::uint8_t> bytes = encode_trim_guide(guide);
    bytes.pop_back();
    CHECK_FALSE(decode_trim_guide(bytes).ok());
    CHECK_FALSE(decode_trim_guide(std::vector<std::uint8_t>{}).ok());

    // THE RULES RIDE ALONG: `tut` and `uzanti` reach the canvas as they reached
    // the command.
    const TrimGuide rules{.extend = false, .every = true, .keys = {}, .keep = true, .carry = true};
    auto ruled = decode_trim_guide(encode_trim_guide(rules));
    REQUIRE(ruled.ok());
    CHECK(ruled.value().every);
    CHECK(ruled.value().keep);
    CHECK(ruled.value().carry);
}

TEST_CASE("CURVE: tutma kipinde tıklanan parça kalır, iki yanındaki uçlar gider")
{
    // A line across two roads, kept between them: both ends go, the middle stays.
    const CurvePath line = segment({0, 0}, {100'000, 0});
    const std::array<CurvePath, 2> roads{segment({30'000, -20'000}, {30'000, 20'000}),
                                         segment({70'000, -20'000}, {70'000, 20'000})};
    auto kept = trim_curve(line, roads, Point2{50'000, 0}, true);
    REQUIRE(kept.ok());
    REQUIRE_EQ(kept.value().kept.size(), 1u);
    CHECK(path_vertices(kept.value().kept.front()) ==
          std::vector<Point2>{{30'000, 0}, {70'000, 0}});
    REQUIRE_EQ(kept.value().removed.size(), 2u);
    CHECK(path_vertices(kept.value().removed[0]) == std::vector<Point2>{{0, 0}, {30'000, 0}});
    CHECK(path_vertices(kept.value().removed[1]) == std::vector<Point2>{{70'000, 0}, {100'000, 0}});

    // A circle kept on its east side: the west half goes, as one arc.
    const std::array<CurvePath, 1> chord{segment({0, -20'000}, {0, 20'000})};
    auto east = trim_curve(circle({0, 0}, 10'000), chord, Point2{10'000, 0}, true);
    REQUIRE(east.ok());
    REQUIRE_EQ(east.value().kept.size(), 1u);
    CHECK_EQ(east.value().kept.front().pieces.front().from, (Point2{0, -10'000}));
    CHECK_EQ(east.value().kept.front().pieces.front().to, (Point2{0, 10'000}));
    REQUIRE_EQ(east.value().removed.size(), 1u);
    CHECK_EQ(east.value().removed.front().pieces.front().sweep_udeg, kUDegFullCircle / 2);
}

TEST_CASE("CURVE: işaretli parçalar gider, komşu aynı kaderdekiler tek parça olur")
{
    // Cuts at 20, 40, 60, 80: marks in the second and third pieces take one
    // stretch, 20..60, not two.
    const CurvePath line = segment({0, 0}, {100'000, 0});
    std::vector<PathCrossing> cuts;
    for (const Mm x : {20'000, 40'000, 60'000, 80'000})
        cuts.push_back(PathCrossing{.at = place_of(line, {x, 0}), .point = {x, 0}});
    const std::array<PathPlace, 2> marks{place_of(line, {30'000, 0}), place_of(line, {50'000, 0})};
    auto cut = cut_pieces(line, cuts, marks, false);
    REQUIRE(cut.ok());
    REQUIRE_EQ(cut.value().removed.size(), 1u);
    CHECK(path_vertices(cut.value().removed.front()) ==
          std::vector<Point2>{{20'000, 0}, {60'000, 0}});
    REQUIRE_EQ(cut.value().kept.size(), 2u);
    CHECK(path_vertices(cut.value().kept[0]) == std::vector<Point2>{{0, 0}, {20'000, 0}});
    CHECK(path_vertices(cut.value().kept[1]) == std::vector<Point2>{{60'000, 0}, {100'000, 0}});
}

TEST_CASE("CURVE: çizginin tam ucuna tıklamak uçtaki parçayı gösterir")
{
    // THE REGRESSION: the support matrix clicks a line's own end, and the piece
    // test was strict at both bounds, so the click fell in no piece and BUDA
    // said there was nothing to throw away. A cut is a boundary no click can
    // name; the path's own ends are not cuts.
    const CurvePath line = segment({0, 0}, {50'000, 0});
    const std::array<CurvePath, 1> edge{segment({25'000, -5'000}, {25'000, 5'000})};
    auto at_end = trim_curve(line, edge, Point2{50'000, 0});
    REQUIRE(at_end.ok());
    CHECK(path_vertices(at_end.value().kept.front()) == std::vector<Point2>{{0, 0}, {25'000, 0}});
    auto at_start = trim_curve(line, edge, Point2{0, 0});
    REQUIRE(at_start.ok());
    CHECK(path_vertices(at_start.value().kept.front()) ==
          std::vector<Point2>{{25'000, 0}, {50'000, 0}});
}

// =============================================================================
// Walked both ways, cut anywhere, written back as what it is (TODOS C-05)
// =============================================================================

namespace {

/// A drawing holding one arc-polyline: (0,0) → (10,0) straight, then a
/// half-circle of radius 5 m CLOCKWISE up to (10,10) — from the south round
/// through the west, a kerb return bending into the block — then straight back
/// to (0,10).
struct KerbDrawing
{
    Document doc;
    EntityId kerb{kNoEntity};

    KerbDrawing()
    {
        const LayerId layer = doc.ensure_layer("0");
        const std::vector<Point2> vertices{{0, 0}, {10'000, 0}, {10'000, 10'000}, {0, 10'000}};
        ArcPolyline def;
        def.arcs.push_back(ArcPolyline::Arc{1, Point2{10'000, 5'000}, 5'000, false});
        const RingGeometry::RingInput input{vertices, RingRole::Open, 0};
        Op op;
        auto made =
            doc.add_kind(layer, kArcPolylineKind, {&input, 1}, encode_arc_polyline(def), op);
        REQUIRE(made.ok());
        kerb = made.value();
    }
};

} // namespace

TEST_CASE("CURVE: yaylı çoklu çizgi yol olarak yürür; saat yönündeki kenar saat yönünde")
{
    KerbDrawing d;
    const auto path = path_of(d.doc, d.kerb);
    REQUIRE(path.has_value());
    REQUIRE_EQ(path->pieces.size(), 3u);
    CHECK(path->pieces[0].kind == PathPiece::Kind::Segment);
    CHECK(path->pieces[1].kind == PathPiece::Kind::Arc);
    CHECK_EQ(path->pieces[1].sweep_udeg, -kUDegFullCircle / 2); ///< clockwise half turn
    CHECK(path->pieces[1].from == (Point2{10'000, 0}));
    CHECK(path->pieces[1].to == (Point2{10'000, 10'000}));

    // THE WALK DRAWS WHAT THE KIND DRAWS, point for point — through the west.
    std::vector<Mm> walked_x;
    std::vector<Mm> walked_y;
    path_outline(*path, walked_x, walked_y);
    std::vector<Mm> kind_x;
    std::vector<Mm> kind_y;
    (void)arc_polyline_outline(d.doc.geometry(), d.doc.entities().slot[d.kerb], kind_x, kind_y);
    CHECK(walked_x == kind_x);
    CHECK(walked_y == kind_y);
    bool through_west = false;
    for (std::size_t i = 0; i < walked_x.size(); ++i)
        through_west = through_west || (walked_x[i] == 5'000 && walked_y[i] == 5'000);
    CHECK(through_west); ///< clockwise from the south passes the west, (5; 5)

    // 10 m + π·5 m + 10 m, to the millimetre.
    CHECK_EQ(path_length(*path), Mm{35'708});
}

TEST_CASE("CURVE: ters yürünen yol aynı uzunlukta, aynı çizimdir")
{
    KerbDrawing d;
    const CurvePath path = *path_of(d.doc, d.kerb);
    const CurvePath back = reversed(path);
    CHECK_EQ(path_length(back), path_length(path));
    CHECK(back.pieces.front().from == (Point2{0, 10'000}));
    CHECK_EQ(back.pieces[1].sweep_udeg, kUDegFullCircle / 2); ///< counter-clockwise now
    CHECK(reversed(back) == path);

    std::vector<Mm> fx;
    std::vector<Mm> fy;
    path_outline(path, fx, fy);
    std::vector<Mm> bx;
    std::vector<Mm> by;
    path_outline(back, bx, by);
    std::ranges::reverse(bx);
    std::ranges::reverse(by);
    CHECK(bx == fx);
    CHECK(by == fy);
}

TEST_CASE("CURVE: bölünen yolun parçaları kaynağın uzunluğunu verir, yay yay kalır")
{
    KerbDrawing d;
    const CurvePath path = *path_of(d.doc, d.kerb);
    const Mm total       = path_length(path);

    // Three cuts: one on the first straight, two on the arc.
    const std::vector<PathPlace> cuts{place_at_length(path, 4'000), place_at_length(path, 13'000),
                                      place_at_length(path, 20'000)};
    const std::vector<CurvePath> pieces = split_path(path, cuts);
    REQUIRE_EQ(pieces.size(), 4u);
    Mm sum = 0;
    for (const CurvePath& p : pieces)
        sum += path_length(p);
    CHECK(std::abs(sum - total) <= 4); ///< a millimetre of rounding per cut at most
    CHECK(std::abs(path_length(pieces[0]) - 4'000) <= 1);

    // THE ARC IS NOT FLATTENED: the pieces cut from it are arcs of the same
    // circle, still clockwise, and store as arcs and arc-polylines.
    CHECK(pieces[2].pieces.size() == 1);
    CHECK(pieces[2].pieces[0].kind == PathPiece::Kind::Arc);
    CHECK(pieces[2].pieces[0].centre == (Point2{10'000, 5'000}));
    CHECK(pieces[2].pieces[0].sweep_udeg < 0);
    CHECK(path_record(pieces[2]).kind == kArcKind);
    CHECK(path_record(pieces[1]).kind == kArcPolylineKind); ///< straight, then arc
    CHECK(path_record(pieces[0]).kind == kPolylineKind);
}

TEST_CASE("CURVE: kayıt biçimi geri okunduğunda aynı yoldur")
{
    KerbDrawing d;
    const CurvePath path = *path_of(d.doc, d.kerb);
    const PathRecord rec = path_record(path);
    REQUIRE(rec.kind == kArcPolylineKind);
    const RingGeometry::RingInput input{rec.ring, rec.role, 0};
    Op op;
    auto again =
        d.doc.add_kind(d.doc.entities().layer[d.kerb], rec.kind, {&input, 1}, rec.payload, op);
    REQUIRE(again.ok());
    CHECK(*path_of(d.doc, again.value()) == path);

    // A circle cut twice is two arcs; one whole turn is a circle again.
    const CurvePath round = circle({0, 0}, 10'000);
    const auto halves =
        split_path(round, {place_of(round, {0, 10'000}), place_of(round, {0, -10'000})});
    REQUIRE_EQ(halves.size(), 2u);
    CHECK(path_record(halves[0]).kind == kArcKind);
    CHECK(path_record(halves[1]).kind == kArcKind);
    CHECK(path_record(round).kind == kCircleKind);
}
