// SPDX-License-Identifier: GPL-3.0-or-later
#include "microtest.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"
#include "piricad/core/units.hpp"

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
