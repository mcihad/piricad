// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_test.hpp"

#include <algorithm>

#include <cmath>
#include <limits>

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/trig.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <span>

using namespace kentos::core;

// ------------------------------------------------------------- angles ----
//
// core/angle.hpp: how a typed angle becomes a direction and how a direction is
// written back. The trigonometry underneath is `sin_cos_udeg` and `atan2_udeg`,
// so every figure here is the same on every platform (§7.3).

TEST_CASE("AÇI: birim → mikro derece tek yuvarlamayla, tam olduğu yerde tam")
{
    CHECK_EQ(udeg_from_angle(45.0, AngleUnit::Grad), std::int64_t{40500000});
    CHECK_EQ(udeg_from_angle(45.1234, AngleUnit::Grad), std::int64_t{40611060});
    CHECK_EQ(udeg_from_angle(-45.0, AngleUnit::Grad), std::int64_t{-40500000});
    CHECK_EQ(udeg_from_angle(400.0, AngleUnit::Grad), kUDegFullCircle);
    CHECK_EQ(udeg_from_angle(90.0, AngleUnit::Degree), std::int64_t{90000000});
    CHECK_EQ(udeg_from_angle(12.345678, AngleUnit::Degree), std::int64_t{12345678});
    CHECK_EQ(udeg_from_angle(kPi / 2.0, AngleUnit::Radian), std::int64_t{90000000});
    CHECK_EQ(udeg_from_angle(kPi, AngleUnit::Radian), std::int64_t{180000000});

    // The setting indices and the suffix letters are the enums' own values.
    CHECK(angle_unit_from_setting(0) == AngleUnit::Grad);
    CHECK(angle_unit_from_setting(1) == AngleUnit::Degree);
    CHECK(angle_unit_from_setting(2) == AngleUnit::Radian);
    CHECK(angle_unit_from_setting(7) == AngleUnit::Grad);
    CHECK(angle_rule_from_setting(0) == AngleRule::Semt);
    CHECK(angle_rule_from_setting(1) == AngleRule::Matematik);
    CHECK(angle_rule_from_setting(9) == AngleRule::Semt);

    AngleUnit named{};
    CHECK((angle_unit_from_suffix('g', named) && named == AngleUnit::Grad));
    CHECK((angle_unit_from_suffix('D', named) && named == AngleUnit::Degree));
    CHECK((angle_unit_from_suffix('r', named) && named == AngleUnit::Radian));
    CHECK(!angle_unit_from_suffix('x', named));
    CHECK(!angle_unit_from_suffix('0', named));
    CHECK_EQ(angle_unit_suffix(AngleUnit::Grad), 'g');
    CHECK_EQ(angle_unit_suffix(AngleUnit::Degree), 'd');
    CHECK_EQ(angle_unit_suffix(AngleUnit::Radian), 'r');
}

TEST_CASE("AÇI: kutupsal ofset eksenlerde tam, köşegende iki eksen aynı milimetre")
{
    const AngleConvention semt{};
    const AngleConvention matematik{AngleUnit::Grad, AngleRule::Matematik};

    CHECK_EQ(polar_offset(100.0, 0.0, semt), (Point2{0, 100000}));
    CHECK_EQ(polar_offset(100.0, 100.0, semt), (Point2{100000, 0}));
    CHECK_EQ(polar_offset(100.0, 200.0, semt), (Point2{0, -100000}));
    CHECK_EQ(polar_offset(100.0, 300.0, semt), (Point2{-100000, 0}));
    CHECK_EQ(polar_offset(100.0, 50.0, semt), (Point2{70711, 70711}));

    CHECK_EQ(polar_offset(100.0, 0.0, matematik), (Point2{100000, 0}));
    CHECK_EQ(polar_offset(100.0, 100.0, matematik), (Point2{0, 100000}));
    CHECK_EQ(polar_offset(100.0, 200.0, matematik), (Point2{-100000, 0}));
    CHECK_EQ(polar_offset(100.0, 300.0, matematik), (Point2{0, -100000}));

    // Semt 45 grad = 40.5°: north-east, steeper than the diagonal. Under
    // matematik the same figure is the mirror image about the diagonal.
    const Point2 s = polar_offset(100.0, 45.0, semt);
    const Point2 m = polar_offset(100.0, 45.0, matematik);
    CHECK_EQ(s, (Point2{m.y, m.x}));
    CHECK(s.y > s.x);

    // A negative distance walks the other way; a zero one stays put.
    CHECK_EQ(polar_offset(-100.0, 0.0, semt), (Point2{0, -100000}));
    CHECK_EQ(polar_offset(0.0, 123.456, semt), (Point2{0, 0}));
}

TEST_CASE("AÇI: yön semt'te kuzeyden saat yönüne, matematik'te doğudan tersine")
{
    const Point2 o{0, 0};
    CHECK_EQ(direction_turns(o, Point2{0, 1000}, AngleRule::Semt), 0.0);
    CHECK_EQ(direction_turns(o, Point2{1000, 0}, AngleRule::Semt), 0.25);
    CHECK_EQ(direction_turns(o, Point2{0, -1000}, AngleRule::Semt), 0.5);
    CHECK_EQ(direction_turns(o, Point2{-1000, 0}, AngleRule::Semt), 0.75);

    CHECK_EQ(direction_turns(o, Point2{1000, 0}, AngleRule::Matematik), 0.0);
    CHECK_EQ(direction_turns(o, Point2{0, 1000}, AngleRule::Matematik), 0.25);
    CHECK_EQ(direction_turns(o, Point2{-1000, 0}, AngleRule::Matematik), 0.5);
    CHECK_EQ(direction_turns(o, Point2{0, -1000}, AngleRule::Matematik), 0.75);

    // The diagonal is exact in both rules (atan2_udeg's octant is an integer).
    CHECK_EQ(direction_turns(o, Point2{1000, 1000}, AngleRule::Semt), 0.125);
    CHECK_EQ(direction_turns(o, Point2{1000, 1000}, AngleRule::Matematik), 0.125);

    // A direction and the offset that produced it agree: the round trip.
    const Point2 p    = polar_offset(100.0, 62.5, AngleConvention{});
    const double back = direction_turns(o, p, AngleRule::Semt) * 400.0;
    CHECK(std::abs(back - 62.5) < 0.001);

    CHECK_EQ(direction_turns(o, o, AngleRule::Semt), 0.0);
}

TEST_CASE("AÇI: metin aplikasyon cetveli gibi yazılır — virgül, dört hane, birim")
{
    CHECK_EQ(angle_text(0.15625, AngleUnit::Grad), std::string("62,5000 grad"));
    CHECK_EQ(angle_text(0.15625, AngleUnit::Degree), std::string("56,2500°"));
    CHECK_EQ(angle_text(0.15625, AngleUnit::Radian), std::string("0,98175 rad"));
    CHECK_EQ(angle_text(0.0, AngleUnit::Grad), std::string("0,0000 grad"));
    CHECK_EQ(angle_text(0.25, AngleUnit::Grad), std::string("100,0000 grad"));

    // Negative turns — a relative angle read backwards — fold into the circle.
    CHECK_EQ(angle_text(-0.25, AngleUnit::Grad), std::string("300,0000 grad"));
    CHECK_EQ(angle_text(1.25, AngleUnit::Degree), std::string("90,0000°"));

    // A full turn is zero, never 400,0000.
    CHECK_EQ(angle_text(0.99999999, AngleUnit::Grad), std::string("0,0000 grad"));

    CHECK_EQ(std::string(angle_rule_label(AngleRule::Semt)), std::string("kuzeyden saat yönünde"));
    CHECK_EQ(std::string(angle_rule_label(AngleRule::Matematik)),
             std::string("doğudan saat yönünün tersine"));
}

TEST_CASE("mm fixed point is exact and symmetric")
{
    CHECK_EQ(mm_from_metres(1.0), Mm{1000});
    CHECK_EQ(mm_from_metres(-1.0), Mm{-1000});
    CHECK_EQ(mm_from_metres(485320.150), Mm{485320150});
    CHECK_EQ(mm_from_metres(0.0005), Mm{1}); // half rounds away from zero
    CHECK_EQ(mm_from_metres(-0.0005), Mm{-1});
    CHECK_EQ(mm_to_metres(485320150), 485320.150);
}

TEST_CASE("HASSASİYET: koordinat metreye tam sayılarla ve yarımdan uzağa yuvarlanarak yazılır")
{
    // The ONE formatter a coordinate table goes through (`core.crs.hassasiyet`).
    // In integers, so 485320,155 at two decimals is ,16 on every machine — a
    // double 485320.155 is 485320.15499999999…, and printf would say ,15.
    CHECK_EQ(metres_fixed(485320155, 3, ','), std::string("485320,155"));
    CHECK_EQ(metres_fixed(485320155, 2, ','), std::string("485320,16"));
    CHECK_EQ(metres_fixed(485320154, 2, ','), std::string("485320,15"));
    CHECK_EQ(metres_fixed(485320150, 1, '.'), std::string("485320.2"));
    CHECK_EQ(metres_fixed(485320499, 0, '.'), std::string("485320"));
    CHECK_EQ(metres_fixed(485320500, 0, '.'), std::string("485321"));

    // Half AWAY from zero on the negative side too, and no "-0" for a value that
    // rounds to nothing.
    CHECK_EQ(metres_fixed(-1005, 2, ','), std::string("-1,01"));
    CHECK_EQ(metres_fixed(-4, 2, ','), std::string("0,00"));
    CHECK_EQ(metres_fixed(-5, 2, ','), std::string("-0,01"));

    // Leading zeros of the fraction kept; more than three decimals is clamped,
    // because the fourth would be a digit the millimetre store never held.
    CHECK_EQ(metres_fixed(4310220007, 3, '.'), std::string("4310220.007"));
    CHECK_EQ(metres_fixed(4310220007, 6, '.'), std::string("4310220.007"));
    CHECK_EQ(metres_fixed(4310220007, -2, '.'), std::string("4310220"));
}

TEST_CASE("mm rounding never rounds twice")
{
    // The regression this locks. mm_from_metres used to compute (scaled + 0.5) and
    // truncate, which lets the ADDITION round before the truncation runs.
    //
    // 4503599627370.497 metres scales to exactly 4503599627370497 — an integer, so
    // rounding has nothing to do. Above 2^52 consecutive doubles are one apart, so
    // adding a half lands on a tie and ties-to-even carries it to ...498. The old
    // code invented a millimetre at an input that needed no rounding.
    CHECK_EQ(mm_from_metres(4503599627370.497), Mm{4503599627370497});
    CHECK_EQ(mm_from_metres(-4503599627370.497), Mm{-4503599627370497});

    // The magnitude above is 4.5e12 metres and nothing terrestrial reaches it, so
    // no drawing was ever wrong because of this. It is fixed because a rounding
    // primitive that holds only inside the range someone remembered to check is
    // not a rounding primitive, and because the same expression is wrong at the
    // small end too: (long long)(0x1.fffffffffffffp-2 + 0.5) is 1 where the answer
    // is 0. That value cannot be produced by metres * 1000, so it is asserted here
    // as the arithmetic it is rather than as an input.
    CHECK_EQ(static_cast<Mm>(0x1.fffffffffffffp-2 + 0.5), Mm{1}); // the old rule
    CHECK_EQ(static_cast<Mm>(0x1.fffffffffffffp-2), Mm{0});       // the right answer

    // The declared contract, at the boundary and on both sides of zero.
    CHECK_EQ(mm_from_metres(0.5 / 1000.0), Mm{1});
    CHECK_EQ(mm_from_metres(-0.5 / 1000.0), Mm{-1});
    CHECK_EQ(mm_from_metres(0.4999 / 1000.0), Mm{0});
    CHECK_EQ(mm_from_metres(-0.4999 / 1000.0), Mm{0});
    CHECK_EQ(mm_from_metres(1.5 / 1000.0), Mm{2});
    CHECK_EQ(mm_from_metres(-1.5 / 1000.0), Mm{-2});

    // A TUREF northing rounds by the same rule as everything else.
    CHECK_EQ(mm_from_metres(4310200.0004), Mm{4310200000});
    CHECK_EQ(mm_from_metres(4310200.0005), Mm{4310200001});

    // constexpr, because units.hpp promises it: a coordinate folded at compile time
    // must agree with one computed at run time.
    static_assert(mm_from_metres(0.0005) == Mm{1});
    static_assert(mm_from_metres(-0.0005) == Mm{-1});
    static_assert(mm_from_metres(4503599627370.497) == Mm{4503599627370497});
}

TEST_CASE("mm rounding saturates instead of leaving int64 undefined")
{
    // `static_cast<Mm>` of a double outside Mm's range is undefined behaviour, and
    // so is the half step that follows once the truncation has hit an endpoint.
    // This was reachable from input already in the repository: `@(2^1000),0` in
    // tests/fuzz/tohum/komut/14-asiri-sayi.txt is 1,07e301 metres, and
    // mm_from_metres scales it past what Mm can hold. Nothing failed because the
    // unit build is not sanitized; the asan preset's UBSan job is where it showed.
    //
    // What the hardware actually did is why the answer is stated rather than left
    // to it. On AArch64 the cast saturates and the overflowing step then wraps, so
    // the coordinate came back as INT64_MIN — kMmInvalid, the "no value" sentinel.
    // One that had run off the end of the world arrived as one never read at all,
    // and RingGeometry::append would have refused it for the wrong reason.
    CHECK_EQ(mm_round(1e308), kMmSaturated);
    CHECK_EQ(mm_round(-1e308), -kMmSaturated);
    CHECK_EQ(mm_from_metres(1e308), kMmSaturated); // × 1000 is already infinite
    CHECK_EQ(mm_from_metres(-1e308), -kMmSaturated);

    constexpr double inf = std::numeric_limits<double>::infinity();
    CHECK_EQ(mm_round(inf), kMmSaturated);
    CHECK_EQ(mm_round(-inf), -kMmSaturated);

    // NaN takes the same road +inf does. Zero would look like the principled
    // answer and is the trap: zero is the ORIGIN, which in TUREF is a thousand
    // kilometres from any parcel, so a NaN answered as zero puts a corner there
    // silently. Saturated, it is out of range and the store refuses it.
    CHECK_EQ(mm_round(std::numeric_limits<double>::quiet_NaN()), kMmSaturated);

    // Saturation is symmetric ON PURPOSE: it must never land on the sentinel,
    // which RingGeometry::append reads as "the source data held a coordinate it
    // could not read" rather than as one that was merely too large.
    CHECK_NE(-kMmSaturated, kMmInvalid);

    // Nor is saturating a way of ACCEPTING the value — it is four times the limit
    // a ring will take, so it is still refused, by the range message and not by
    // undefined behaviour (test_geometry.cpp, "temsil edilebilir aralığın dışında").
    static_assert(kMmSaturated > kMmCoordinateLimit);

    // The edge, exactly. kMmSaturated is 2^63 - 1024, the largest Mm that is also
    // exactly a double; 2^63 is the first magnitude past it and saturates, and the
    // neighbour below — doubles are 1024 apart up there — goes down the ordinary
    // path untouched.
    CHECK_EQ(mm_round(kMmSaturatedReal), kMmSaturated);
    CHECK_EQ(mm_round(-kMmSaturatedReal), -kMmSaturated);
    CHECK_EQ(mm_round(0x1p63), kMmSaturated);
    CHECK_EQ(mm_round(-0x1p63), -kMmSaturated);
    CHECK_EQ(mm_round(0x1.ffffffffffffep62), Mm{9223372036854773760});
    CHECK_EQ(mm_round(-0x1.ffffffffffffep62), Mm{-9223372036854773760});

    // The contract of the function this guards is unchanged for everything that
    // was already in range — including the two cases the half-away-from-zero rule
    // is written the way it is to get right.
    CHECK_EQ(mm_round(0.5), Mm{1});
    CHECK_EQ(mm_round(-0.5), Mm{-1});
    CHECK_EQ(mm_from_metres(4503599627370.497), Mm{4503599627370497});

    // Still constexpr, which units.hpp promises: a coordinate folded at compile
    // time must agree with one computed at run time, saturated ones included.
    static_assert(mm_round(1e308) == kMmSaturated);
    static_assert(mm_round(-1e308) == -kMmSaturated);
    static_assert(mm_round(0x1p63) == kMmSaturated);
    static_assert(mm_round(0x1.ffffffffffffep62) == Mm{9223372036854773760});
}

TEST_CASE("AÇI: aşırı bir açı tanımsız davranış değil, doymuş bir açıdır")
{
    // The same defect on the other helper — udeg_from_angle kept its own copy of
    // the cast, and a typed `@0<(2^1000)` reaches it first. It now rounds through
    // mm_round, so the saturation is the same one, at the same magnitude.
    CHECK_EQ(udeg_from_angle(1e308, AngleUnit::Grad), kMmSaturated);
    CHECK_EQ(udeg_from_angle(-1e308, AngleUnit::Grad), -kMmSaturated);
    CHECK_EQ(udeg_from_angle(1e308, AngleUnit::Degree), kMmSaturated);
    CHECK_EQ(udeg_from_angle(-1e308, AngleUnit::Radian), -kMmSaturated);
    CHECK_EQ(udeg_from_angle(std::numeric_limits<double>::infinity(), AngleUnit::Grad),
             kMmSaturated);
    CHECK_EQ(udeg_from_angle(std::numeric_limits<double>::quiet_NaN(), AngleUnit::Grad),
             kMmSaturated);

    // A saturated angle is still an angle: sin_cos_udeg folds it into one circle
    // by exact integer arithmetic, so the direction is defined and finite.
    const SinCos t = sin_cos_udeg(udeg_from_angle(1e308, AngleUnit::Grad));
    CHECK(t.sin >= -1.0);
    CHECK(t.sin <= 1.0);
    CHECK(t.cos >= -1.0);
    CHECK(t.cos <= 1.0);

    // And the rounding every ordinary angle goes through is the one it always was.
    CHECK_EQ(udeg_from_angle(45.0, AngleUnit::Grad), std::int64_t{40500000});
    CHECK_EQ(udeg_from_angle(45.1234, AngleUnit::Grad), std::int64_t{40611060});
    static_assert(udeg_from_angle(45.0, AngleUnit::Grad) == std::int64_t{40500000});
    static_assert(udeg_from_angle(1e308, AngleUnit::Grad) == kMmSaturated);
}

TEST_CASE("mm addition is order independent")
{
    // The reason coordinates are integers: a cadastral area computed in a
    // different summation order must give the identical result (kentoscad.md §7.3).
    const Mm a = 485320150, b = -4310220400, c = 7;
    CHECK_EQ(a + b + c, c + b + a);
    CHECK_EQ((a + b) + c, a + (b + c));
}

TEST_CASE("turkish upper casing handles i and dotless i")
{
    CHECK_EQ(turkish_upper("çizgi"), std::string("ÇİZGİ"));
    CHECK_EQ(turkish_upper("ışık"), std::string("IŞIK"));
    CHECK_EQ(turkish_upper("ifraz"), std::string("İFRAZ"));
    CHECK_EQ(turkish_upper("tevhit"), std::string("TEVHİT"));
    CHECK(turkish_iequals("çizgi", "ÇİZGİ"));
    CHECK(!turkish_iequals("ısı", "İSİ"));
}

TEST_CASE("box extends and reports empty correctly")
{
    Box2 b;
    CHECK(b.empty());
    b.extend(Point2{10, 20});
    CHECK(!b.empty());
    CHECK_EQ(b.width(), Mm{0});
    b.extend(Point2{30, 60});
    CHECK_EQ(b.width(), Mm{20});
    CHECK_EQ(b.height(), Mm{40});
    CHECK_EQ(b.centre(), (Point2{20, 40}));
}

TEST_CASE("document add and erase round trip through ops")
{
    Document doc;
    const LayerId lyr = doc.ensure_layer("SINIR");

    const Point2 pts[2] = {{485320150, 4310220400}, {485380000, 4310250000}};

    Op undo;
    auto id = doc.add_polyline(lyr, pts, undo);
    CHECK(id.ok());
    CHECK_EQ(doc.live_entity_count(), std::size_t{1});

    const std::uint64_t after_add = doc.content_hash();

    Op redo;
    CHECK(doc.apply(undo, &redo).ok());
    CHECK_EQ(doc.live_entity_count(), std::size_t{0});

    CHECK(doc.apply(redo).ok());
    CHECK_EQ(doc.content_hash(), after_add);
}

TEST_CASE("document rejects a degenerate polyline and a locked layer")
{
    Document doc;
    const LayerId lyr = doc.ensure_layer("PARSEL");

    const Point2 single[1] = {{0, 0}};
    Op undo;
    CHECK(!doc.add_polyline(lyr, single, undo).ok());

    CHECK(doc.set_layer_locked(lyr, true, undo).ok());
    const Point2 pair[2] = {{0, 0}, {1000, 1000}};
    auto blocked         = doc.add_polyline(lyr, pair, undo);
    CHECK(!blocked.ok());
    CHECK_EQ(static_cast<int>(blocked.error().code), static_cast<int>(ErrorCode::ValidationFailed));
}

TEST_CASE("content hash ignores insertion order of layers but not geometry")
{
    Document a;
    Document b;
    Op op;

    const Point2 pts[2] = {{1000, 2000}, {3000, 4000}};
    CHECK(a.add_polyline(a.ensure_layer("YOL"), pts, op).ok());
    CHECK(b.add_polyline(b.ensure_layer("YOL"), pts, op).ok());
    CHECK_EQ(a.content_hash(), b.content_hash());

    const Point2 other[2] = {{1000, 2000}, {3000, 4001}};
    Document c;
    CHECK(c.add_polyline(c.ensure_layer("YOL"), other, op).ok());
    CHECK(a.content_hash() != c.content_hash());
}

TEST_CASE("json round trips utf-8 and preserves key order")
{
    const std::string src =
        R"({"cmd":"core.line","args":{"noktalar":[[485320150,4310220400],[485380000,4310250000]]},"katman":"SINIR — ÇİZİM"})";

    auto parsed = Json::parse(src);
    CHECK(parsed.ok());
    CHECK_EQ(parsed.value().dump(), src);

    CHECK(!Json::parse("{").ok());
    CHECK(!Json::parse("[1,2,]").ok());
    CHECK(!Json::parse(R"({"a":1} trailing)").ok());
}

TEST_CASE("json numbers are locale independent and round trip")
{
    auto j = Json::parse("[1.5,-0.25,1e10,3]");
    CHECK(j.ok());
    CHECK_EQ(j.value().as_array()[0].as_double(), 1.5);
    CHECK_EQ(j.value().as_array()[3].as_int(), std::int64_t{3});
    CHECK(j.value().as_array()[3].is_int());
    CHECK_EQ(Json::number(1.5).dump(), std::string("1.5"));
}

// ---------------------------------------------------------------------------
// The payoff of the ring model: a parcel that a single (start, count) vertex run
// could not represent. Yola terk and irtifak produce these routinely, and alan
// `alan hesabı` over them is the legal output (.claude/model.md R9, R12).
// ---------------------------------------------------------------------------

namespace {

using kentos::core::RingGeometry;
using kentos::core::RingRole;

RingGeometry::RingInput ring(std::span<const Point2> pts, RingRole role, std::uint16_t part = 0)
{
    return RingGeometry::RingInput{pts, role, part};
}

} // namespace

TEST_CASE("parsel: delikli parselin net alanı doğru")
{
    Document doc;
    const LayerId parsel = doc.ensure_layer("PARSEL");

    // A 60 m x 45 m exterior of 2700 m², with a 20 m x 15 m easement of 300 m² in it.
    // Net alan 2400 m² = 2 400 000 000 mm².
    const Point2 outer[4] = {{485300000, 4310200000},
                             {485360000, 4310200000},
                             {485360000, 4310245000},
                             {485300000, 4310245000}};
    const Point2 hole[4]  = {{485320000, 4310215000},
                             {485340000, 4310215000},
                             {485340000, 4310230000},
                             {485320000, 4310230000}};

    const RingGeometry::RingInput rings[2] = {ring(outer, RingRole::Exterior),
                                              ring(hole, RingRole::Interior)};

    Op op;
    auto id = doc.add_area(parsel, rings, op);
    CHECK(id.ok());
    if (!id.ok()) return;

    CHECK_EQ(doc.entity_area(id.value()), Mm2{2400000000});
    CHECK_EQ(doc.geometry().rings_of(doc.entities().slot[id.value()]).count, std::uint32_t{2});

    // The bounding box is the exterior's, not the hole's.
    const Box2 box = doc.entity_extent(id.value());
    CHECK_EQ(box.min_x, Mm{485300000});
    CHECK_EQ(box.max_y, Mm{4310245000});
}

TEST_CASE("parsel: iki parçalı parselin alanı parçaların toplamı")
{
    Document doc;
    const LayerId parsel = doc.ensure_layer("PARSEL");

    // A parcel split in two by a road after an ifraz: two separate faces, one title.
    const Point2 a[4] = {{0, 0}, {30000, 0}, {30000, 20000}, {0, 20000}};         //  600 m²
    const Point2 b[4] = {{40000, 0}, {60000, 0}, {60000, 20000}, {40000, 20000}}; // 400 m²

    const RingGeometry::RingInput rings[2] = {ring(a, RingRole::Exterior, 0),
                                              ring(b, RingRole::Exterior, 1)};

    Op op;
    auto id = doc.add_area(parsel, rings, op);
    CHECK(id.ok());
    if (id.ok()) CHECK_EQ(doc.entity_area(id.value()), Mm2{1000000000}); // 1000 m²
}

TEST_CASE("parsel: dışına taşan boşluk reddediliyor")
{
    Document doc;
    const LayerId parsel = doc.ensure_layer("PARSEL");

    // A hole bigger than its parcel would make `alan hesabı` NEGATIVE — the routine
    // outcome of a malformed yola terk import, and unsignable.
    const Point2 outer[4] = {{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
    const Point2 hole[4]  = {{-5000, -5000}, {40000, -5000}, {40000, 30000}, {-5000, 30000}};

    const RingGeometry::RingInput rings[2] = {ring(outer, RingRole::Exterior),
                                              ring(hole, RingRole::Interior)};

    Op op;
    auto id = doc.add_area(parsel, rings, op);
    CHECK(!id.ok());
    if (!id.ok()) CHECK(!id.error().message.empty());
    CHECK_EQ(doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("parsel: kalıcı anahtar yuvadan bağımsız ve tekrar edilmiyor")
{
    Document doc;
    const LayerId lyr = doc.ensure_layer("PARSEL");

    const Point2 pts[2] = {{0, 0}, {10000, 0}};
    Op op;

    auto a = doc.add_polyline(lyr, pts, op);
    auto b = doc.add_polyline(lyr, pts, op);
    CHECK(a.ok());
    CHECK(b.ok());
    if (!a.ok() || !b.ok()) return;

    const EntityKey ka = doc.key_of(a.value());
    const EntityKey kb = doc.key_of(b.value());

    CHECK(ka != kb);
    CHECK(ka != EntityKey::None);
    CHECK_EQ(doc.slot_of(ka), a.value());
    CHECK_EQ(doc.slot_of(kb), b.value());

    // Erasing must not release the key: "which parcel was this?" is a legal
    // question and a reused key makes it unanswerable (R4).
    CHECK(doc.set_entity_alive(a.value(), false, op).ok());
    auto c = doc.add_polyline(lyr, pts, op);
    CHECK(c.ok());
    if (c.ok()) {
        CHECK(doc.key_of(c.value()) != ka);
        CHECK(raw(doc.key_of(c.value())) > raw(kb));
    }

    CHECK_EQ(doc.slot_of(EntityKey{999999}), kentos::core::kNoEntity);
}

TEST_CASE("katman görünürlüğü nesne bayrağına yansıyor")
{
    // R7: the cull test reads one byte. The layer bit is mirrored on toggle so the
    // frame path never follows an indirection to the layer record.
    Document doc;
    const LayerId lyr = doc.ensure_layer("YOL");

    const Point2 pts[2] = {{0, 0}, {10000, 0}};
    Op op;
    auto id = doc.add_polyline(lyr, pts, op);
    CHECK(id.ok());
    if (!id.ok()) return;

    CHECK(doc.entities().visible(id.value()));

    CHECK(doc.set_layer_visible(lyr, false, op).ok());
    CHECK(!doc.entities().visible(id.value()));
    CHECK(doc.entities().alive(id.value())); // hidden is not erased

    CHECK(doc.set_layer_visible(lyr, true, op).ok());
    CHECK(doc.entities().visible(id.value()));

    // An entity hidden on its own stays hidden when its layer comes back.
    CHECK(doc.set_entity_hidden(id.value(), true, op).ok());
    CHECK(!doc.entities().visible(id.value()));
    CHECK(doc.set_layer_visible(lyr, false, op).ok());
    CHECK(doc.set_layer_visible(lyr, true, op).ok());
    CHECK(!doc.entities().visible(id.value()));
}

TEST_CASE("JSON: bozuk girdi reddedilir, çökmez")
{
    // Parsing is the dangerous half of a JSON facade: a hostile script, a corrupt
    // catalogue and a truncated journal all reach it first. This is nlohmann's
    // parser now rather than a hand-rolled scanner, and the point of the change is
    // this list — every one of these was a shape our own scanner had to be trusted
    // to handle.
    for (const char* bad :
         {"{", "[1,2", "{\"a\":}", "\"kapanmamis", "{\"a\":1,}", "[1,]", "nan", "01",
          "{\"a\" \"b\"}", "1e999999", "[[[[[[[[[[[[[[[[", "{\"a\":\"\\uD800\"}", ""}) {
        auto parsed = Json::parse(bad);
        CHECK(!parsed.ok());
        if (parsed.ok()) FAIL_WITH("kabul edildi", bad);
    }
}

TEST_CASE("JSON: nesne anahtar SIRASI korunur")
{
    // ordered_json, not json. The default container sorts keys, and the journal is
    // compared byte for byte across three clients (CLAUDE.md 6.4). Sorted keys
    // would rewrite every golden fixture and make the file's key order an accident
    // of the alphabet rather than a decision.
    auto parsed = Json::parse(R"({"zeta":1,"alpha":2,"mu":3})");
    REQUIRE(parsed.ok());

    const JsonObject& fields = parsed.value().as_object();
    REQUIRE(fields.size() == std::size_t{3});
    CHECK_EQ(fields[0].first, std::string("zeta"));
    CHECK_EQ(fields[1].first, std::string("alpha"));
    CHECK_EQ(fields[2].first, std::string("mu"));

    // And the round trip writes them back in the same order.
    CHECK_EQ(parsed.value().dump(), std::string(R"({"zeta":1,"alpha":2,"mu":3})"));
}

TEST_CASE("JSON: tam sayı ile ondalık ayrı kalır")
{
    // A coordinate is an integer count of millimetres. Collapsing it to a double
    // would write 485320150.0 back into the journal and make a replay disagree
    // with the run it replays (model.md R21).
    auto parsed = Json::parse(R"({"mm":485320150,"oran":0.5})");
    REQUIRE(parsed.ok());

    const Json* mm = parsed.value().find("mm");
    REQUIRE(mm != nullptr);
    CHECK(mm->is_int());
    CHECK_EQ(mm->as_int(), std::int64_t{485320150});

    const Json* oran = parsed.value().find("oran");
    REQUIRE(oran != nullptr);
    CHECK(!oran->is_int());

    CHECK_EQ(parsed.value().dump(), std::string(R"({"mm":485320150,"oran":0.5})"));
}

TEST_CASE("TRIG: kendi sinüs ve kosinüsümüz libm ile aynı sayıyı veriyor")
{
    // Why this test exists rather than a comment claiming accuracy: the first two
    // versions of trig.hpp were WRONG and both claimed not to be. One had a
    // mistyped cosine coefficient (5e-12 off), the next had the right ones and too
    // few of them (4e-13 off, exactly the truncation error at π/4). Neither is
    // visible in a drawing; both would have moved a golden fixture.
    double worst = 0.0;
    for (std::int64_t udeg = 0; udeg < kUDegFullCircle; udeg += 7919) { // prime step
        const SinCos ours    = sin_cos_udeg(udeg);
        const double radians = static_cast<double>(udeg) * (kPi / (180.0 * 1000000.0));
        worst                = std::max(worst, std::abs(ours.sin - std::sin(radians)));
        worst                = std::max(worst, std::abs(ours.cos - std::cos(radians)));
    }
    CHECK(worst < 1.0e-14);
    if (worst >= 1.0e-14) FAIL_WITH("libm'den sapma", std::to_string(worst));

    // The quarter angles are exact by construction — the fold and the quadrant
    // switch are sign flips, not arithmetic — so they are asserted exactly.
    CHECK_EQ(sin_cos_udeg(0).sin, 0.0);
    CHECK_EQ(sin_cos_udeg(0).cos, 1.0);
    CHECK_EQ(sin_cos_udeg(90 * kUDegPerDegree).sin, 1.0);
    CHECK_EQ(sin_cos_udeg(180 * kUDegPerDegree).cos, -1.0);
    CHECK_EQ(sin_cos_udeg(270 * kUDegPerDegree).sin, -1.0);

    // A full turn and a negative angle land where they should, because the
    // reduction is integer and cannot drift.
    CHECK_EQ(sin_cos_udeg(360 * kUDegPerDegree).cos, sin_cos_udeg(0).cos);
    CHECK_EQ(sin_cos_udeg(-90 * kUDegPerDegree).sin, sin_cos_udeg(270 * kUDegPerDegree).sin);

    // sin² + cos² = 1, to a double's precision, everywhere.
    double pythagoras = 0.0;
    for (std::int64_t udeg = 0; udeg < kUDegFullCircle; udeg += 104729) {
        const SinCos v = sin_cos_udeg(udeg);
        pythagoras     = std::max(pythagoras, std::abs(v.sin * v.sin + v.cos * v.cos - 1.0));
    }
    CHECK(pythagoras < 1.0e-14);
}
