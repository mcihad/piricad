// SPDX-License-Identifier: GPL-3.0-or-later
//
// The spatial index answers a question the renderer trusts every frame, so it is
// checked against brute force rather than against itself.
#include "microtest.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/spatial_index.hpp"

#include <algorithm>
#include <array>
#include <vector>

using namespace piricad::core;

namespace {

/// A deterministic, slightly irregular parcel grid: uniform data would hide a
/// packing bug that only shows when strips are uneven.
Document make_grid(std::size_t columns, std::size_t rows)
{
    Document doc;
    const LayerId layer = doc.ensure_layer("PARSEL");

    Op op;
    std::array<Point2, 5> ring{};

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < columns; ++c) {
            const Mm jitter = static_cast<Mm>((r * 7 + c * 13) % 5) * 100;
            const Mm x      = 485000000 + static_cast<Mm>(c) * 20000 + jitter;
            const Mm y      = 4310000000 + static_cast<Mm>(r) * 20000 + jitter;

            ring[0] = {x, y};
            ring[1] = {x + 15000, y};
            ring[2] = {x + 15000, y + 15000};
            ring[3] = {x, y + 15000};
            ring[4] = {x, y};
            (void)doc.add_polyline(layer, ring, op);
        }
    }
    return doc;
}

std::vector<EntityId> brute_force(const Document& doc, const Box2& box)
{
    std::vector<EntityId> out;
    const auto& poly = doc.entities();

    for (EntityId e = 0; e < poly.size(); ++e) {
        if (!poly.alive(e)) continue;
        const Box2 b = poly.box_of(e);
        if (b.max_x < box.min_x || b.min_x > box.max_x) continue;
        if (b.max_y < box.min_y || b.min_y > box.max_y) continue;
        out.push_back(e);
    }
    return out;
}

/// The index returns candidates, so the caller's exact test is applied here too.
std::vector<EntityId> via_index(const Document& doc, const Box2& box)
{
    SpatialIndex index;
    index.build(doc.entities());

    std::vector<EntityId> candidates;
    index.query(box, candidates);

    const auto& poly = doc.entities();
    std::vector<EntityId> out;
    for (EntityId e : candidates) {
        if (!poly.alive(e)) continue;
        const Box2 b = poly.box_of(e);
        if (b.max_x < box.min_x || b.min_x > box.max_x) continue;
        if (b.max_y < box.min_y || b.min_y > box.max_y) continue;
        out.push_back(e);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("index matches brute force over many query boxes")
{
    const Document doc = make_grid(60, 40); // 2400 parsel

    // A point, one parcel, a few parcels, a strip, everything, and nothing.
    const std::vector<Box2> queries{
        Box2{485000000, 4310000000, 485000000, 4310000000},
        Box2{485000000, 4310000000, 485020000, 4310020000},
        Box2{485100000, 4310100000, 485260000, 4310260000},
        Box2{485000000, 4310300000, 486200000, 4310320000},
        Box2{484000000, 4309000000, 487000000, 4312000000},
        Box2{400000000, 4000000000, 400100000, 4000100000},
    };

    for (const Box2& q : queries) {
        auto expected = brute_force(doc, q);
        auto actual   = via_index(doc, q);
        CHECK_EQ(actual.size(), expected.size());
        CHECK(actual == expected);
    }
}

TEST_CASE("index candidate set is a superset, never a subset")
{
    const Document doc = make_grid(40, 40);

    SpatialIndex index;
    index.build(doc.entities());

    const Box2 q{485150000, 4310150000, 485350000, 4310350000};

    std::vector<EntityId> candidates;
    index.query(q, candidates);
    std::sort(candidates.begin(), candidates.end());

    // Every truly overlapping entity must appear among the candidates. Missing one
    // means a parcel silently vanishes from the screen.
    for (EntityId e : brute_force(doc, q))
        CHECK(std::binary_search(candidates.begin(), candidates.end(), e));
}

TEST_CASE("index skips erased entities and reports its shape")
{
    Document doc = make_grid(20, 20); // 400

    Op op;
    for (EntityId e = 0; e < 100; ++e)
        CHECK(doc.set_entity_alive(e, false, op).ok());

    SpatialIndex index;
    index.build(doc.entities());

    CHECK_EQ(index.entity_count(), std::size_t{300});
    CHECK(!index.empty());
    CHECK(index.depth() >= 2);
    CHECK(index.node_count() > 300 / SpatialIndex::kFanout);
}

TEST_CASE("empty document produces an empty index")
{
    Document doc;
    SpatialIndex index;
    index.build(doc.entities());

    CHECK(index.empty());
    CHECK_EQ(index.entity_count(), std::size_t{0});
    CHECK(index.bounds().empty());

    std::vector<EntityId> out;
    index.query(Box2{0, 0, 1000, 1000}, out);
    CHECK(out.empty());
}

TEST_CASE("index packing is deterministic")
{
    // Two identical documents must produce byte-identical trees, or a golden
    // comparison across platforms is meaningless (piricad.md §7.3).
    const Document a = make_grid(30, 30);
    const Document b = make_grid(30, 30);

    SpatialIndex ia, ib;
    ia.build(a.entities());
    ib.build(b.entities());

    CHECK_EQ(ia.node_count(), ib.node_count());
    CHECK_EQ(ia.depth(), ib.depth());

    const Box2 q{485050000, 4310050000, 485250000, 4310250000};
    std::vector<EntityId> ra, rb;
    ia.query(q, ra);
    ib.query(q, rb);
    CHECK(ra == rb);
}

TEST_CASE("live entity count is maintained, not recomputed")
{
    Document doc = make_grid(10, 10);
    CHECK_EQ(doc.live_entity_count(), std::size_t{100});

    Op op;
    CHECK(doc.set_entity_alive(0, false, op).ok());
    CHECK_EQ(doc.live_entity_count(), std::size_t{99});

    CHECK(doc.set_entity_alive(0, false, op).ok()); // idempotent
    CHECK_EQ(doc.live_entity_count(), std::size_t{99});

    CHECK(doc.set_entity_alive(0, true, op).ok());
    CHECK_EQ(doc.live_entity_count(), std::size_t{100});
}
