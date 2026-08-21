// SPDX-License-Identifier: GPL-3.0-or-later
#include "microtest.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"
#include "piricad/core/units.hpp"

#include <span>

using namespace piricad::core;

TEST_CASE("mm fixed point is exact and symmetric")
{
    CHECK_EQ(mm_from_metres(1.0), Mm{1000});
    CHECK_EQ(mm_from_metres(-1.0), Mm{-1000});
    CHECK_EQ(mm_from_metres(485320.150), Mm{485320150});
    CHECK_EQ(mm_from_metres(0.0005), Mm{1}); // half rounds away from zero
    CHECK_EQ(mm_from_metres(-0.0005), Mm{-1});
    CHECK_EQ(mm_to_metres(485320150), 485320.150);
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

TEST_CASE("mm addition is order independent")
{
    // The reason coordinates are integers: a cadastral area computed in a
    // different summation order must give the identical result (piricad.md §7.3).
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
// hesabı over them is the legal output (.claude/model.md R9, R12).
// ---------------------------------------------------------------------------

namespace {

using piricad::core::RingGeometry;
using piricad::core::RingRole;

RingGeometry::RingInput ring(std::span<const Point2> pts, RingRole role, std::uint16_t part = 0)
{
    return RingGeometry::RingInput{pts, role, part};
}

} // namespace

TEST_CASE("parsel: delikli parselin net alanı doğru")
{
    Document doc;
    const LayerId parsel = doc.ensure_layer("PARSEL");

    // 60 m x 45 m dış sınır = 2700 m², içinde 20 m x 15 m irtifak = 300 m².
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

    // İfraz sonrası yolla ikiye bölünmüş bir parsel: iki ayrı yüz, tek tapu.
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

    // A hole bigger than its parcel would make alan hesabı NEGATIVE — the routine
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
    CHECK(a.ok() && b.ok());
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

    CHECK_EQ(doc.slot_of(EntityKey{999999}), piricad::core::kNoEntity);
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
