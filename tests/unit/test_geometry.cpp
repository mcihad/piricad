// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ring geometry — .claude/model.md R9-R12.
//
// `Alan hesabı` is the legal output of this product (§12), so the numbers below are
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

// ------------------------------------------------------------- open rings ----

TEST_CASE("çoklu çizginin alanı sıfır, çevresi kenarlarının toplamı")
{
    RingGeometry g;

    const std::vector<Point2> two{{0, 0}, {3000, 4000}};
    const std::uint32_t a = must_add(g, {ring(two, RingRole::Open)});

    CHECK_EQ(g.area_of(a), Mm2{0});
    CHECK_EQ(g.perimeter_of(a), Mm{5000}); // 3-4-5

    // A zigzag whose IMPLIED closure would have a non-zero shoelace. R10 says a
    // polyline's first and last vertex are not joined, so both area functions
    // must report nothing: an implied-closure figure for this shape (3 m², as the
    // old ring_area returned) is an area for something that does not exist, and
    // the two public area functions must never disagree about the same ring.
    const std::vector<Point2> five{{0, 0}, {1000, 0}, {1000, 1000}, {2000, 1000}, {2000, 4000}};
    const std::uint32_t b = must_add(g, {ring(five, RingRole::Open)});

    CHECK_EQ(g.area_of(b), Mm2{0});
    CHECK_EQ(g.perimeter_of(b), Mm{6000});
    CHECK_EQ(g.ring_area(g.rings_of(b).first), Mm2{0});
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

// ------------------------------------------------------------- `alan hesabı` --

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
    CHECK_EQ(b.min_y, Mm{0});
    CHECK_EQ(b.max_x, Mm{60000});
    CHECK_EQ(b.max_y, Mm{20000});
}

TEST_CASE("R9: sınır kutusu başlangıç noktasından uzakta da doğru")
{
    // Both bounds_of assertions elsewhere are over geometry touching the origin,
    // so the interesting failure — Box2 encodes empty as min > max and extend()
    // special-cases the first point, so a mishandled seed clamps a far-from-origin
    // parcel back toward 0 — was never probed, even with TM30 fixtures at hand.
    RingGeometry g;

    const auto yuz0 = rect(kTmX, kTmY, 20000, 20000);
    const auto bos0 = rect(kTmX + 5000, kTmY + 5000, 5000, 5000);
    const auto yuz1 = rect(kTmX + 50000, kTmY - 10000, 10000, 10000);

    const std::uint32_t slot =
        must_add(g, {ring(yuz0, RingRole::Exterior, 0), ring(bos0, RingRole::Interior, 0),
                     ring(yuz1, RingRole::Exterior, 1)});

    const Box2 b = g.bounds_of(slot);
    CHECK_EQ(b.min_x, kTmX);
    CHECK_EQ(b.min_y, kTmY - 10000);
    CHECK_EQ(b.max_x, kTmX + 60000);
    CHECK_EQ(b.max_y, kTmY + 20000);
    CHECK(!b.empty());
    CHECK_EQ(b.width(), Mm{60000});
    CHECK_EQ(b.height(), Mm{30000});
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

TEST_CASE("kabul edilen en küçük halkalar: üç köşeli parsel, iki köşeli çoklu çizgi")
{
    // Only the REJECTION side of the vertex-count boundary was tested, so an
    // off-by-one that refused a legitimate üçgen parsel (`stored <= needed`)
    // passed the whole file: every other accepted ring here has four corners.
    RingGeometry g;

    const std::vector<Point2> ucgen{{0, 0}, {30000, 0}, {0, 40000}};
    const std::uint32_t parsel = must_add(g, {ring(ucgen, RingRole::Exterior)});
    CHECK_EQ(g.ring_count[g.rings_of(parsel).first], std::uint32_t{3});
    CHECK_EQ(g.area_of(parsel), Mm2{600000000});                 // 30 x 40 / 2 = 600 m²
    CHECK_EQ(g.perimeter_of(parsel), Mm{30000 + 40000 + 50000}); // 3-4-5

    const std::vector<Point2> iki{{0, 0}, {0, 12000}};
    const std::uint32_t cizgi = must_add(g, {ring(iki, RingRole::Open)});
    CHECK_EQ(g.ring_count[g.rings_of(cizgi).first], std::uint32_t{2});
    CHECK_EQ(g.area_of(cizgi), Mm2{0});
    CHECK_EQ(g.perimeter_of(cizgi), Mm{12000});
}

TEST_CASE("R10/R11: parça ve rol sütunları yazıldıkları gibi okunur")
{
    // ring_part and ring_role are written by append and were read by nothing the
    // suite asserted: `ring_part.push_back(0)` would have passed every case.
    RingGeometry g;

    const auto yuz0 = rect(0, 0, 20000, 20000);
    const auto bos0 = rect(5000, 5000, 5000, 5000);
    const auto yuz1 = rect(50000, 0, 10000, 10000);
    const auto bos1 = rect(52000, 2000, 1000, 1000);

    const std::uint32_t slot =
        must_add(g, {ring(yuz0, RingRole::Exterior, 0), ring(bos0, RingRole::Interior, 0),
                     ring(yuz1, RingRole::Exterior, 7), ring(bos1, RingRole::Interior, 7)});

    const RingSpan span = g.rings_of(slot);
    CHECK_EQ(span.count, std::uint32_t{4});

    const std::uint16_t parts[4]{0, 0, 7, 7};
    const RingRole roles[4]{RingRole::Exterior, RingRole::Interior, RingRole::Exterior,
                            RingRole::Interior};
    for (std::uint32_t k = 0; k < 4; ++k) {
        CHECK_EQ(g.ring_part[span.first + k], parts[k]);
        CHECK_EQ(static_cast<int>(g.ring_role[span.first + k]), static_cast<int>(roles[k]));
    }
}

// ------------------------------------------------------------- yuvarlama ----

TEST_CASE("R12: tek sayılı iki kat alan sıfırdan uzağa yuvarlanır, iki sarımda da aynı")
{
    // Every other area here is a rectangle, so twice_area is always EVEN and
    // halve()'s round-half-away-from-zero branch never runs: plain `twice / 2`
    // passed all sixteen assertions. A yola terk slivi is a triangle, and its
    // doubled shoelace is routinely odd.
    RingGeometry g;

    const std::vector<Point2> ccw{{0, 0}, {1001, 0}, {0, 1001}}; // 2A = 1002001
    const std::vector<Point2> cw{{0, 0}, {0, 1001}, {1001, 0}};

    const std::uint32_t a = must_add(g, {ring(ccw, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(cw, RingRole::Exterior)});

    CHECK_EQ(g.ring_area(g.rings_of(a).first), Mm2{501001});  // 1002001 -> +501001
    CHECK_EQ(g.ring_area(g.rings_of(b).first), Mm2{-501001}); // symmetric in sign
    CHECK_EQ(g.area_of(a), Mm2{501001});
    CHECK_EQ(g.area_of(b), g.area_of(a));

    // And the halving happens ONCE over the whole slot, not per ring: both rings
    // round on their own, the net does not.
    RingGeometry h;
    const std::vector<Point2> dis{{0, 0}, {4001, 0}, {0, 4001}};        // 2A = 16008001
    const std::vector<Point2> ic{{100, 100}, {1101, 100}, {100, 1101}}; // 2A = 1002001
    const std::uint32_t net =
        must_add(h, {ring(dis, RingRole::Exterior), ring(ic, RingRole::Interior)});
    CHECK_EQ(h.area_of(net), Mm2{7503000}); // (16008001 - 1002001) / 2, exact
}

TEST_CASE("R21: kenar uzunluğu en yakın milimetreye yuvarlanır, hiçbir yerde kayan nokta yok")
{
    // Every perimeter asserted elsewhere is axis-aligned or 3-4-5, i.e. an exact
    // integer root: round_sqrt returning floor() passed all of them. A kenar
    // A `kenar uzunluğu` printed on a röper krokisi that is systematically 1 mm short is a
    // wrong figure on a legal document.
    RingGeometry g;

    // sqrt(2) = 1.41 -> 1;  sqrt(5) = 2.24 -> 2;  sqrt(13) = 3.61 -> 4.
    const std::vector<Point2> kirik{{0, 0}, {1, 1}, {3, 2}, {5, 5}};
    const std::uint32_t a = must_add(g, {ring(kirik, RingRole::Open)});
    CHECK_EQ(g.perimeter_of(a), Mm{1 + 2 + 4});

    // Both sides of the half-way point, at full TM30 width: 3162² = 9998244 and
    // 3163² = 10004569, and 10000000 is nearer 3162.
    const std::vector<Point2> uzun{{kTmX, kTmY}, {kTmX + 3000, kTmY + 1000}};
    const std::uint32_t b = must_add(g, {ring(uzun, RingRole::Open)});
    CHECK_EQ(g.perimeter_of(b), Mm{3162});

    // 5² = 25 and the next square is 36; 30 is nearer 5 than 6, 31 is nearer 6.
    const std::vector<Point2> asagi{{0, 0}, {5, 5}}; // 50 -> 7.07 -> 7
    CHECK_EQ(g.perimeter_of(must_add(g, {ring(asagi, RingRole::Open)})), Mm{7});
}

TEST_CASE("R9: iki dilimi kapsayan kenar tam ölçülür, sıfır bildirmez")
{
    // The old segment_length squared the deltas in signed int64 on the strength
    // of a comment. A 3100 km side overflowed, round_sqrt's `v <= 0 return 0`
    // turned the overflow into the sentinel 0, and the çevre came back SHORT with
    // no word to the user. Verified against the old code: this ring reported
    // 1000 mm instead of ~6 200 001 000 mm.
    RingGeometry g;

    const std::vector<Point2> genis{{0, 0}, {3100000000, 0}, {3100000000, 1000}};
    const std::uint32_t slot = must_add(g, {ring(genis, RingRole::Open)});
    CHECK_EQ(g.perimeter_of(slot), Mm{3100000000 + 1000});

    // A `dilim`-prefixed `sağa değer` beside an unprefixed neighbour — one vertex that
    // kept its prefix — is 3.05e10 mm apart, and dx² is 9.3e20.
    constexpr Mm kPrefixedX = 30485320150;
    const std::vector<Point2> dilimler{{kPrefixedX, kTmY}, {150, kTmY}};
    const std::uint32_t iki = must_add(g, {ring(dilimler, RingRole::Open)});
    CHECK_EQ(g.perimeter_of(iki), Mm{30485320000});
}

TEST_CASE("dilim ön ekli sağa değerde çevre de tam kalır")
{
    // The `dilim`-prefixed AREA was pinned; the `dilim`-prefixed PERIMETER was not,
    // and the perimeter is the half that used to overflow.
    constexpr Mm kPrefixedX = 30485320150;

    RingGeometry g;
    const auto dilimde  = rect(kPrefixedX, kTmY, 30000, 45000);
    const auto merkezde = rect(0, 0, 30000, 45000);

    const std::uint32_t a = must_add(g, {ring(dilimde, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(merkezde, RingRole::Exterior)});

    CHECK_EQ(g.perimeter_of(a), Mm{150000});
    CHECK_EQ(g.perimeter_of(a), g.perimeter_of(b));
}

// ----------------------------------------------------------- overflow tests --

TEST_CASE("TM30 koordinatlarında altı köşeli parsel: alan tam ve ötelemeden bağımsız")
{
    // Six corners at ~4.85e8 / ~4.31e9 mm. A shoelace that multiplies the raw
    // coordinates sums six terms of ~2.1e18 and runs past int64's 9.2e18 ceiling.
    //
    // What this case can and cannot catch, stated so nobody over-trusts it: the
    // shoelace sum is exactly translation-invariant in ℤ, and twice_area_acc
    // accumulates in uint64 where wraparound is defined, so deleting the
    // translation would NOT change either number here — this pins the CONTRACT
    // (exact, and independent of where the parcel sits), not the arithmetic. What
    // the translation buys is small intermediates, so that narrowing the
    // accumulator, or reintroducing a signed one, is a compile-time-visible change
    // rather than silent UB. The can-fail witness for the overflow class is
    // "R9: iki dilimi kapsayan kenar tam ölçülür", where the old code reported a
    // 3100 km side as zero.
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
    // TUREF/TM3 `sağa değer` with the `dilim` number in front: 30 485 320,150 m.
    // Stored in millimetres that is 3.05e10, and a single raw x*y term against a
    // 4.31e9 mm `yukarı değer` is 1.3e20 — fourteen times int64's ceiling. The area
    // is exact anyway, because the accumulator is unsigned and the sum is taken
    // modulo 2^64; the perimeter over the same coordinates is the half that used
    // to overflow, and it is pinned in its own case below.
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

TEST_CASE("R11: iki kez kapatılmış halka, bir kez kapatılmışla bit bit aynı saklanır")
{
    // stored_count used to drop exactly ONE trailing repeat, so the same parcel
    // stored as 4 corners when closed once and 5 when closed twice — identical
    // area, identical perimeter, DIFFERENT xs/ys and ring_count, i.e. a
    // permanently forked content hash decided by which writer produced the file.
    // Real DXF and GeoJSON writers do emit double-closed rings.
    RingGeometry g;

    const auto clean         = rect(kTmX, kTmY, 30000, 45000);
    std::vector<Point2> once = clean;
    once.push_back(once.front());
    std::vector<Point2> twice = once;
    twice.push_back(twice.front());

    const std::uint32_t a = must_add(g, {ring(clean, RingRole::Exterior)});
    const std::uint32_t b = must_add(g, {ring(once, RingRole::Exterior)});
    const std::uint32_t c = must_add(g, {ring(twice, RingRole::Exterior)});

    for (const std::uint32_t slot : {a, b, c})
        CHECK_EQ(g.ring_count[g.rings_of(slot).first], std::uint32_t{4});

    CHECK_EQ(g.vertex_count(), std::size_t{12}); // 3 x 4, not 4 + 4 + 5

    // Vertex for vertex, not merely area for area: the hash is over the arrays.
    const std::uint32_t ra = g.rings_of(a).first, rc = g.rings_of(c).first;
    for (std::uint32_t v = 0; v < 4; ++v) {
        CHECK_EQ(g.xs[g.ring_start[ra] + v], g.xs[g.ring_start[rc] + v]);
        CHECK_EQ(g.ys[g.ring_start[ra] + v], g.ys[g.ring_start[rc] + v]);
    }
}

TEST_CASE("R12: sıfır alanlı kapalı halka reddedilir")
{
    RingGeometry g;

    // Three collinear corners: accepted before, stored as a parsel of 0 m².
    const std::vector<Point2> dogrusal{{0, 0}, {1000, 0}, {2000, 0}};
    reject(g, {ring(dogrusal, RingRole::Exterior)}, "sıfır alanlı");

    // The commonest digitising blunder: two middle corners of a rectangle
    // swapped. The two lobes of the resulting bowtie cancel exactly, so the
    // shoelace is 0 and a 1350 m² parcel used to be stored as 0 m².
    const std::vector<Point2> papyon{{0, 0}, {30000, 0}, {0, 45000}, {30000, 45000}};
    reject(g, {ring(papyon, RingRole::Exterior)}, "sıfır alanlı");

    // A double-closed degenerate ring: normalisation no longer smuggles it in
    // with a corner count that depends on the writer.
    const std::vector<Point2> katlanmis{{0, 0}, {1000, 0}, {0, 0}, {0, 0}};
    reject(g, {ring(katlanmis, RingRole::Exterior)}, "en az 3 tepe noktası");

    const std::vector<Point2> geri{{0, 0}, {1000, 0}, {1000, 0}, {0, 0}};
    reject(g, {ring(geri, RingRole::Exterior)}, "sıfır alanlı");

    // An Open ring is a polyline and encloses nothing by definition, so the rule
    // does not touch it: a survey traverse doubling back on itself is legal data.
    const std::uint32_t cizgi = must_add(g, {ring(dogrusal, RingRole::Open)});
    CHECK_EQ(g.perimeter_of(cizgi), Mm{2000});
}

TEST_CASE("R12: dış halkasının dışına taşan boşluk reddedilir")
{
    // area_of subtracts a hole by its ROLE, so a hole bigger than its exterior
    // subtracts more than there is: a 10 m x 10 m parcel with a 40 m x 30 m
    // "hole" used to report -1100.000 m². A negative alan is not a number a
    // harita mühendisi can sign, and nobody was told anything went wrong.
    RingGeometry g;

    const auto kucuk_dis = rect(0, 0, 10000, 10000);
    const auto buyuk_ic  = rect(0, 0, 40000, 30000);
    reject(g, {ring(kucuk_dis, RingRole::Exterior), ring(buyuk_ic, RingRole::Interior)},
           "dışına taşıyor");

    // Wholly outside, same size: a hole belonging to another parcel.
    const auto uzak_ic = rect(90000, 90000, 1000, 1000);
    reject(g,
           {ring(rect(0, 0, 40000, 30000), RingRole::Exterior), ring(uzak_ic, RingRole::Interior)},
           "dışına taşıyor");

    // A hole flush against the exterior's own boundary is legal: containment is
    // inclusive, because a `yola terk boşluğu` commonly shares an edge.
    const std::uint32_t slot = must_add(g, {ring(rect(0, 0, 40000, 30000), RingRole::Exterior),
                                            ring(rect(0, 0, 10000, 10000), RingRole::Interior)});
    CHECK_EQ(g.area_of(slot), Mm2{1200000000 - 100000000});
}

TEST_CASE("okunamayan ya da temsil edilemeyen koordinat depoya giremez")
{
    RingGeometry g;

    // kMmInvalid is units.hpp's "no value" sentinel (INT64_MIN). It used to be
    // accepted as an ordinary coordinate, and then EVERY delta taken against it
    // overflowed: segment_length, twice_area and Box2::extend alike.
    const std::vector<Point2> yok{{0, 0}, {1000, 0}, {1000, kMmInvalid}};
    reject(g, {ring(yok, RingRole::Exterior)}, "'değer yok'");

    const std::vector<Point2> tasan{{0, 0}, {kMmCoordinateLimit + 1, 0}, {0, 1000}};
    reject(g, {ring(tasan, RingRole::Exterior)}, "temsil edilebilir aralığın dışında");

    const std::vector<Point2> eksi{{0, 0}, {1000, 0}, {0, -kMmCoordinateLimit - 1}};
    reject(g, {ring(eksi, RingRole::Exterior)}, "temsil edilebilir aralığın dışında");

    // The message names WHICH vertex, because a five-million-vertex import is
    // unfixable without it.
    auto r = g.append(std::vector<RingGeometry::RingInput>{ring(yok, RingRole::Exterior)});
    CHECK(!r.ok());
    CHECK(r.error().message.find("3. tepe noktası") != std::string::npos);

    // The limit itself is inside the store: it is a bound on the COORDINATE, not
    // a taboo. A parcel-sized ring sitting exactly on it is stored and measured
    // exactly, because every intermediate is translated to the first vertex.
    const Mm L = kMmCoordinateLimit;
    const std::vector<Point2> sinirda{{L, L}, {L - 3000, L}, {L, L - 4000}};
    const std::uint32_t slot = must_add(g, {ring(sinirda, RingRole::Exterior)});
    CHECK_EQ(g.area_of(slot), Mm2{6000000});
    CHECK_EQ(g.perimeter_of(slot), Mm{3000 + 4000 + 5000});
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
