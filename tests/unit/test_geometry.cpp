// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ring geometry — .claude/model.md R9-R12.
//
// Alan hesabı is the legal output of this product (§12), so the numbers below are
// asserted as exact integers in square millimetres. A tolerance here would be a
// tolerance on a cadastral area, and there is no such thing.
#include "microtest.hpp"

#include "piricad/core/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace piricad::core;

namespace {

/// Counter-clockwise rectangle, stored without a closing vertex.
std::vector<Point2> rect(Mm x, Mm y, Mm w, Mm h)
{
    return {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
}

RingGeometry::RingInput ring(const std::vector<Point2>& pts, RingRole role, std::uint16_t part = 0)
{
    return RingGeometry::RingInput{pts, role, part};
}

std::uint32_t must_add(RingGeometry& g, std::vector<RingGeometry::RingInput> rings)
{
    auto r = g.append(rings);
    if (!r.ok()) {
        ::microtest::report(__FILE__, __LINE__, "append beklenmedik şekilde reddedildi",
                            r.error().message);
        return 0;
    }
    return r.value();
}

/// The error path is half the product: a rejection must say what is wrong.
void reject(RingGeometry& g, std::vector<RingGeometry::RingInput> rings, const char* needle)
{
    const std::size_t vertices_before = g.vertex_count();
    const std::size_t slots_before    = g.slot_count();
    const std::size_t rings_before    = g.ring_count_total();

    auto r = g.append(rings);
    if (r.ok()) {
        ::microtest::report(__FILE__, __LINE__, "append kabul edildi ama reddedilmeliydi",
                            std::string("beklenen ileti parçası: ") + needle);
        return;
    }
    if (r.error().message.find(needle) == std::string::npos)
        ::microtest::report(__FILE__, __LINE__, "hata iletisi sorunu adlandırmıyor",
                            std::string("beklenen: ") + needle +
                                "\n        alınan : " + r.error().message);

    // A rejected append leaves the store exactly as it was (Article 1.6).
    CHECK_EQ(g.vertex_count(), vertices_before);
    CHECK_EQ(g.slot_count(), slots_before);
    CHECK_EQ(g.ring_count_total(), rings_before);
}

// A real TM30 parcel corner: easting ~485 km, northing ~4310 km, in millimetres.
constexpr Mm kTmX = 485320150;
constexpr Mm kTmY = 4310220400;

} // namespace

// ------------------------------------------------------------------ açık ----

TEST_CASE("çoklu çizginin alanı sıfır, çevresi kenarlarının toplamı")
{
    RingGeometry g;

    const std::vector<Point2> two{{0, 0}, {3000, 4000}};
    const std::uint32_t a = must_add(g, {ring(two, RingRole::Open)});

    CHECK_EQ(g.area_of(a), Mm2{0});
    CHECK_EQ(g.perimeter_of(a), Mm{5000}); // 3-4-5

    // A zigzag whose implied closure has a non-zero shoelace: an Open ring must
    // still contribute nothing to alan hesabı.
    const std::vector<Point2> five{{0, 0}, {1000, 0}, {1000, 1000}, {2000, 1000}, {2000, 4000}};
    const std::uint32_t b = must_add(g, {ring(five, RingRole::Open)});

    CHECK_EQ(g.area_of(b), Mm2{0});
    CHECK_EQ(g.perimeter_of(b), Mm{6000});
    CHECK(g.ring_area(g.rings_of(b).first) != 0); // the shoelace itself is not zero
}

TEST_CASE("açık halkanın ilk ve son noktası çakışsa bile kırpılmaz")
{
    RingGeometry g;

    // Only a closed ring carries a closure convention; on a polyline the repeat
    // is the caller's geometry.
    std::vector<Point2> pts = rect(0, 0, 1000, 1000);
    pts.push_back(pts.front());

    const std::uint32_t slot = must_add(g, {ring(pts, RingRole::Open)});
    CHECK_EQ(g.ring_count[g.rings_of(slot).first], std::uint32_t{5});
    CHECK_EQ(g.perimeter_of(slot), Mm{4000});
}

// ------------------------------------------------------------- alan hesabı --

TEST_CASE("30 m x 45 m kare parselin alanı tam 1350 m²")
{
    RingGeometry g;
    const auto pts           = rect(0, 0, 30000, 45000);
    const std::uint32_t slot = must_add(g, {ring(pts, RingRole::Exterior)});

    CHECK_EQ(g.area_of(slot), Mm2{1350000000});
    CHECK_EQ(g.ring_area(g.rings_of(slot).first), Mm2{1350000000}); // CCW is positive
    CHECK_EQ(g.perimeter_of(slot), Mm{150000});                     // closing side included

    const Box2 b = g.bounds_of(slot);
    CHECK_EQ(b.min_x, Mm{0});
    CHECK_EQ(b.min_y, Mm{0});
    CHECK_EQ(b.max_x, Mm{30000});
    CHECK_EQ(b.max_y, Mm{45000});
}

TEST_CASE("saat yönünde sarılmış parsel aynı alanı verir, işaret yalnız halkanındır")
{
    RingGeometry g;

    const std::vector<Point2> cw{{0, 0}, {0, 45000}, {30000, 45000}, {30000, 0}};
    const std::uint32_t slot = must_add(g, {ring(cw, RingRole::Exterior)});

    CHECK_EQ(g.ring_area(g.rings_of(slot).first), Mm2{-1350000000});
    CHECK_EQ(g.area_of(slot), Mm2{1350000000});
}

TEST_CASE("kapanış noktası yinelenmiş parsel, yinelenmemişiyle bit bit aynı saklanır")
{
    // DXF and GeoJSON repeat the first vertex; the store must not.
    RingGeometry g;

    const auto clean           = rect(kTmX, kTmY, 30000, 45000);
    std::vector<Point2> closed = clean;
    closed.push_back(closed.front());

    const std::uint32_t a = must_add(g, {ring(clean, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(closed, RingRole::Exterior)});

    CHECK_EQ(g.ring_count[g.rings_of(a).first], std::uint32_t{4});
    CHECK_EQ(g.ring_count[g.rings_of(b).first], std::uint32_t{4});
    CHECK_EQ(g.vertex_count(), std::size_t{8});
    CHECK_EQ(g.area_of(a), g.area_of(b));
    CHECK_EQ(g.perimeter_of(a), g.perimeter_of(b));
}

TEST_CASE("bir boşluklu parselin net alanı dış eksi iç")
{
    RingGeometry g;

    const auto dis = rect(0, 0, 40000, 30000);        // 1200 m²
    const auto ic  = rect(10000, 10000, 10000, 5000); //   50 m²

    const std::uint32_t slot =
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});

    CHECK_EQ(g.area_of(slot), Mm2{1150000000});

    // The hole is wound counter-clockwise, so its own shoelace is POSITIVE; it is
    // subtracted because of its role, not because of its winding.
    CHECK_EQ(g.ring_area(g.rings_of(slot).first + 1), Mm2{50000000});

    // The perimeter is every ring, including the hole's boundary.
    CHECK_EQ(g.perimeter_of(slot), Mm{140000 + 30000});
}

TEST_CASE("saat yönündeki boşluk da çıkarılır")
{
    RingGeometry g;

    const auto dis = rect(0, 0, 40000, 30000);
    const std::vector<Point2> ic{{10000, 10000}, {10000, 15000}, {20000, 15000}, {20000, 10000}};

    const std::uint32_t slot =
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});

    CHECK_EQ(g.ring_area(g.rings_of(slot).first + 1), Mm2{-50000000});
    CHECK_EQ(g.area_of(slot), Mm2{1150000000});
}

TEST_CASE("iki boşluklu parsel")
{
    RingGeometry g;

    const auto dis = rect(0, 0, 40000, 30000);        // 1200 m²
    const auto ic1 = rect(10000, 10000, 10000, 5000); //   50 m²
    const auto ic2 = rect(25000, 20000, 4000, 4000);  //   16 m²

    const std::uint32_t slot =
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic1, RingRole::Interior),
                     ring(ic2, RingRole::Interior)});

    CHECK_EQ(g.area_of(slot), Mm2{1134000000});
}

TEST_CASE("çok parçalı parselin alanı yüzlerinin toplamı")
{
    RingGeometry g;

    const auto yuz0 = rect(0, 0, 20000, 20000);     // 400 m², parça 0
    const auto yuz1 = rect(50000, 0, 10000, 10000); // 100 m², parça 1

    const std::uint32_t slot =
        must_add(g, {ring(yuz0, RingRole::Exterior, 0), ring(yuz1, RingRole::Exterior, 1)});

    CHECK_EQ(g.area_of(slot), Mm2{500000000});
    CHECK_EQ(g.ring_total[slot], std::uint32_t{2});

    const Box2 b = g.bounds_of(slot);
    CHECK_EQ(b.min_x, Mm{0});
    CHECK_EQ(b.max_x, Mm{60000});
    CHECK_EQ(b.max_y, Mm{20000});
}

TEST_CASE("çok parçalı parselin her parçası kendi boşluğunu taşır")
{
    RingGeometry g;

    const auto yuz0 = rect(0, 0, 20000, 20000);      // 400 m²
    const auto bos0 = rect(5000, 5000, 5000, 5000);  //  25 m²
    const auto yuz1 = rect(50000, 0, 10000, 10000);  // 100 m²
    const auto bos1 = rect(52000, 2000, 1000, 1000); //   1 m²

    const std::uint32_t slot =
        must_add(g, {ring(yuz0, RingRole::Exterior, 0), ring(bos0, RingRole::Interior, 0),
                     ring(yuz1, RingRole::Exterior, 1), ring(bos1, RingRole::Interior, 1)});

    CHECK_EQ(g.area_of(slot), Mm2{400000000 - 25000000 + 100000000 - 1000000});
}

// -------------------------------------------------------------- taşma testi --

TEST_CASE("TM30 koordinatlarında altı köşeli parsel: alan tam ve ötelemeden bağımsız")
{
    // Six corners at ~4.85e8 / ~4.31e9 mm. A shoelace that multiplies the raw
    // coordinates sums six terms of ~2.1e18 and runs past int64's 9.2e18 ceiling.
    // Two's complement happens to wrap back to the right number here, so this
    // case pins the contract rather than the arithmetic: the area is exact and it
    // does not depend on where the parcel sits. The dilim-prefixed case below is
    // the one where the overflow is not recoverable, and the UBSan job is what
    // turns either into a failure.
    const auto sekil = [](Mm x, Mm y) {
        return std::vector<Point2>{{x, y},
                                   {x + 60000, y},
                                   {x + 60000, y + 20000},
                                   {x + 30000, y + 20000},
                                   {x + 30000, y + 50000},
                                   {x, y + 50000}};
    };

    RingGeometry g;
    const auto arazide  = sekil(kTmX, kTmY);
    const auto merkezde = sekil(0, 0);

    const std::uint32_t a = must_add(g, {ring(arazide, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(merkezde, RingRole::Exterior)});

    // 60 x 50 m minus the 30 x 30 m notch = 2100 m².
    CHECK_EQ(g.area_of(a), Mm2{2100000000});
    CHECK_EQ(g.area_of(a), g.area_of(b));
    CHECK_EQ(g.perimeter_of(a), g.perimeter_of(b));
}

TEST_CASE("dilim ön ekli sağa değerde alan hesabı tam kalır")
{
    // TUREF/TM3 sağa değer with the dilim number in front: 30 485 320,150 m.
    // Stored in millimetres that is 3.05e10, and a single raw x*y term against a
    // 4.31e9 mm yukarı değer is 1.3e20 — fourteen times int64's ceiling. This is
    // the coordinate range where a shoelace without the translation is not merely
    // undefined but unrecoverable.
    constexpr Mm kPrefixedX = 30485320150;

    RingGeometry g;
    const auto dilimde  = rect(kPrefixedX, kTmY, 30000, 45000);
    const auto merkezde = rect(0, 0, 30000, 45000);

    const std::uint32_t a = must_add(g, {ring(dilimde, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(merkezde, RingRole::Exterior)});

    CHECK_EQ(g.area_of(a), Mm2{1350000000});
    CHECK_EQ(g.area_of(a), g.area_of(b));
    CHECK_EQ(g.perimeter_of(a), g.perimeter_of(b));
}

TEST_CASE("TM30 koordinatlarında 10 km x 10 km parselin alanı tam 1e14 mm²")
{
    RingGeometry g;
    const auto pts           = rect(kTmX, kTmY, 10000000, 10000000);
    const std::uint32_t slot = must_add(g, {ring(pts, RingRole::Exterior)});

    CHECK_EQ(g.area_of(slot), Mm2{100000000000000});
    CHECK_EQ(g.perimeter_of(slot), Mm{40000000});
}

TEST_CASE("TM30 koordinatlarında boşluklu parselin net alanı tam")
{
    RingGeometry g;

    const auto dis = rect(kTmX, kTmY, 40000, 30000);
    const auto ic  = rect(kTmX + 10000, kTmY + 10000, 10000, 5000);

    const std::uint32_t arazide =
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});

    const auto dis0 = rect(0, 0, 40000, 30000);
    const auto ic0  = rect(10000, 10000, 10000, 5000);

    const std::uint32_t merkezde =
        must_add(g, {ring(dis0, RingRole::Exterior), ring(ic0, RingRole::Interior)});

    CHECK_EQ(g.area_of(arazide), Mm2{1150000000});
    CHECK_EQ(g.area_of(arazide), g.area_of(merkezde));
}

// ----------------------------------------------------------- append reddi ---

TEST_CASE("halkasız geometri reddedilir")
{
    RingGeometry g;
    reject(g, {}, "en az bir halka");
}

TEST_CASE("açık halka en az 2, kapalı halka en az 3 tepe noktası ister")
{
    RingGeometry g;

    const std::vector<Point2> bir{{0, 0}};
    reject(g, {ring(bir, RingRole::Open)}, "en az 2 tepe noktası");

    const std::vector<Point2> iki{{0, 0}, {1000, 0}};
    reject(g, {ring(iki, RingRole::Exterior)}, "en az 3 tepe noktası");

    // Three points where the third repeats the first is a two-corner ring after
    // closure normalisation, and two corners are not a polygon.
    const std::vector<Point2> yalanci{{0, 0}, {1000, 0}, {0, 0}};
    reject(g, {ring(yalanci, RingRole::Exterior)}, "kapanış noktası düşüldükten sonra");
}

TEST_CASE("dış halkası olmayan iç halka reddedilir")
{
    RingGeometry g;

    const auto ic = rect(1000, 1000, 1000, 1000);
    reject(g, {ring(ic, RingRole::Interior)}, "dış halka yok");
}

TEST_CASE("R11: iç halka kendi dış halkasından önce verilemez")
{
    RingGeometry g;

    const auto dis = rect(0, 0, 40000, 30000);
    const auto ic  = rect(10000, 10000, 10000, 5000);

    // The same two rings in the wrong order.
    reject(g, {ring(ic, RingRole::Interior), ring(dis, RingRole::Exterior)}, "dış halka yok");

    // In the right order they are accepted, so the rejection is about ordering
    // and nothing else.
    const std::uint32_t slot =
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});
    CHECK_EQ(g.area_of(slot), Mm2{1150000000});
}

TEST_CASE("R11: parça numaraları azalan sırada verilemez")
{
    RingGeometry g;

    const auto yuz0 = rect(0, 0, 20000, 20000);
    const auto yuz1 = rect(50000, 0, 10000, 10000);

    reject(g, {ring(yuz1, RingRole::Exterior, 1), ring(yuz0, RingRole::Exterior, 0)},
           "artan sırada");

    // A part may not be revisited after it has been left, either.
    const auto ic0 = rect(5000, 5000, 5000, 5000);
    reject(g,
           {ring(yuz0, RingRole::Exterior, 0), ring(yuz1, RingRole::Exterior, 1),
            ring(ic0, RingRole::Interior, 0)},
           "artan sırada");
}

TEST_CASE("R10: aynı parça içinde ikinci dış halka reddedilir")
{
    RingGeometry g;

    const auto yuz0 = rect(0, 0, 20000, 20000);
    const auto yuz1 = rect(50000, 0, 10000, 10000);

    reject(g, {ring(yuz0, RingRole::Exterior, 0), ring(yuz1, RingRole::Exterior, 0)},
           "ikinci dış halka");
}

TEST_CASE("tanımsız halka rolü reddedilir")
{
    RingGeometry g;

    const auto pts = rect(0, 0, 1000, 1000);
    reject(g, {ring(pts, static_cast<RingRole>(7))}, "tanımsız bir halka rolü");
}

TEST_CASE("reddedilen append hiçbir dizide iz bırakmaz")
{
    RingGeometry g;

    const auto dis           = rect(0, 0, 40000, 30000);
    const std::uint32_t slot = must_add(g, {ring(dis, RingRole::Exterior)});
    CHECK_EQ(slot, std::uint32_t{0});

    // A rejection in the second of three rings must not leave the first one
    // behind: a half-written parcel is exactly the failure Article 1.6 forbids.
    const auto ic    = rect(10000, 10000, 10000, 5000);
    const auto bozuk = std::vector<Point2>{{0, 0}, {1000, 0}};
    reject(g,
           {ring(dis, RingRole::Exterior), ring(bozuk, RingRole::Exterior, 1),
            ring(ic, RingRole::Interior, 1)},
           "en az 3 tepe noktası");

    CHECK_EQ(g.slot_count(), std::size_t{1});
    CHECK_EQ(g.vertex_count(), std::size_t{4});
    CHECK_EQ(g.area_of(slot), Mm2{1200000000});
}

// ------------------------------------------------------------ determinizm ---

TEST_CASE("aynı girdi iki kez, bit bit aynı diziler")
{
    const auto dis = rect(kTmX, kTmY, 40000, 30000);
    const auto ic  = rect(kTmX + 10000, kTmY + 10000, 10000, 5000);

    const auto build = [&](RingGeometry& g) {
        must_add(g, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});
        must_add(g, {ring(dis, RingRole::Exterior, 3)});
    };

    RingGeometry a;
    RingGeometry b;
    build(a);
    build(b);

    CHECK(a.xs == b.xs);
    CHECK(a.ys == b.ys);
    CHECK(a.ring_start == b.ring_start);
    CHECK(a.ring_count == b.ring_count);
    CHECK(a.ring_part == b.ring_part);
    CHECK(a.ring_role == b.ring_role);
    CHECK(a.first_ring == b.first_ring);
    CHECK(a.ring_total == b.ring_total);
    CHECK_EQ(a.area_of(0), b.area_of(0));
    CHECK_EQ(a.perimeter_of(1), b.perimeter_of(1));
}

TEST_CASE("clear boşaltır, sonraki append sıfırdan başlar")
{
    RingGeometry g;

    const auto pts = rect(kTmX, kTmY, 30000, 45000);
    must_add(g, {ring(pts, RingRole::Exterior)});
    g.clear();

    CHECK_EQ(g.slot_count(), std::size_t{0});
    CHECK_EQ(g.ring_count_total(), std::size_t{0});
    CHECK_EQ(g.vertex_count(), std::size_t{0});

    const std::uint32_t slot = must_add(g, {ring(pts, RingRole::Exterior)});
    CHECK_EQ(slot, std::uint32_t{0});
    CHECK_EQ(g.area_of(slot), Mm2{1350000000});
}
