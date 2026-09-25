// SPDX-License-Identifier: GPL-3.0-or-later
//
// Linework as a planar network, and the faces it encloses (TODOS C-09).
//
// Every figure below is one a surveyor can check by hand — a 20 × 10 m parcel,
// a 2 m pool in it, a 5 cm gap at a corner — and every one is asserted to the
// millimetre or the square millimetre, because a boundary found a millimetre
// off is a boundary that does not meet its neighbour (§7.3).
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/cleanup.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/planar.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace kentos::core;

namespace {

#define NEEDS_NETWORK()                                                                            \
    if (!network_available()) PENDING("KENTOS_WITH_CGAL=OFF; düzlemsel ağ sınanamıyor.")

NetworkPiece seg(Point2 a, Point2 b, std::uint32_t source)
{
    NetworkPiece n;
    n.piece.from = a;
    n.piece.to   = b;
    n.source     = source;
    return n;
}

NetworkPiece whole_circle(Point2 c, Mm r, std::uint32_t source)
{
    NetworkPiece n;
    n.piece  = PathPiece{.kind       = PathPiece::Kind::Arc,
                         .from       = Point2{c.x + r, c.y},
                         .to         = Point2{c.x + r, c.y},
                         .centre     = c,
                         .radius     = r,
                         .sweep_udeg = kUDegFullCircle};
    n.source = source;
    return n;
}

/// The 20 × 10 m parcel as four loose lines, sources 1–4.
std::vector<NetworkPiece> parcel(Mm gap = 0)
{
    return {seg({0, 0}, {20'000, 0}, 1), seg({20'000, 0}, {20'000, 10'000}, 2),
            seg({20'000, 10'000}, {0, 10'000}, 3), seg({0, 10'000}, {0, gap}, 4)};
}

Network built(const std::vector<NetworkPiece>& pieces, Mm tol = 10, Mm bridge = 0)
{
    auto net = Network::build(pieces, tol, bridge);
    REQUIRE(net.ok());
    return std::move(net.value());
}

std::size_t arcs_in(const FaceRing& ring)
{
    return static_cast<std::size_t>(
        std::count_if(ring.path.pieces.begin(), ring.path.pieces.end(),
                      [](const PathPiece& p) { return p.kind == PathPiece::Kind::Arc; }));
}

LayerId layer(Document& doc)
{
    return doc.ensure_layer("0");
}

} // namespace

TEST_CASE("DÜZLEMSEL AĞ: dört ayrı çizgi bir parsel kapatır; yüz, alanı ve kaynakları")
{
    NEEDS_NETWORK();
    const Network net = built(parcel());
    const auto face   = net.face_at({5'000, 5'000}, true);
    REQUIRE(face.has_value());
    CHECK_EQ(face->area, Mm2{200'000'000}); // 200 m²
    CHECK_EQ(face->outer.path.pieces.size(), std::size_t{4});
    CHECK(face->holes.empty());
    // The ring starts at its lowest-left corner and runs counter-clockwise.
    CHECK_EQ(face->outer.path.pieces.front().from, (Point2{0, 0}));
    CHECK_EQ(face->outer.path.pieces.front().to, (Point2{20'000, 0}));
    std::vector<std::uint32_t> all;
    for (const auto& s : face->outer.sources)
        all.insert(all.end(), s.begin(), s.end());
    std::sort(all.begin(), all.end());
    CHECK_EQ(all, (std::vector<std::uint32_t>{1, 2, 3, 4}));
    CHECK(net.open_ends().empty());
    CHECK_FALSE(net.face_at({30'000, 5'000}, true).has_value()); // outside
    CHECK(net.on_linework({10'000, 0}));
}

TEST_CASE("DÜZLEMSEL AĞ: içerideki kapalı çizgi ada olur ve delik olarak korunur")
{
    NEEDS_NETWORK();
    auto pieces = parcel();
    pieces.push_back(whole_circle({10'000, 5'000}, 2'000, 5)); // a 2 m pool
    const Network net = built(pieces);

    const auto face = net.face_at({3'000, 3'000}, true);
    REQUIRE(face.has_value());
    REQUIRE_EQ(face->holes.size(), std::size_t{1});
    CHECK_EQ(arcs_in(face->holes.front()), std::size_t{1});
    CHECK_EQ(face->holes.front().path.pieces.front().sweep_udeg, -kUDegFullCircle); // clockwise
    const Mm2 pool = circular_segment_area(2'000, kUDegFullCircle);
    CHECK_EQ(face->outer_area, Mm2{200'000'000});
    CHECK_EQ(face->area, Mm2{200'000'000} - pool);

    // Without islands, the same click gives the whole parcel.
    const auto filled = net.face_at({3'000, 3'000}, false);
    REQUIRE(filled.has_value());
    CHECK(filled->holes.empty());
    CHECK_EQ(filled->area, Mm2{200'000'000});

    // Inside the pool, the pool itself — one whole arc, the circle's own centre.
    const auto inner = net.face_at({10'000, 5'000}, true);
    REQUIRE(inner.has_value());
    REQUIRE_EQ(inner->outer.path.pieces.size(), std::size_t{1});
    CHECK_EQ(inner->outer.path.pieces.front().centre, (Point2{10'000, 5'000}));
    CHECK_EQ(inner->outer.path.pieces.front().radius, Mm{2'000});
    CHECK_EQ(inner->area, pool);
}

TEST_CASE("DÜZLEMSEL AĞ: yakın ama açık uç sessizce kapanmaz; boşluk ölçülüp gösterilir")
{
    NEEDS_NETWORK();
    const Network net = built(parcel(50)); // the last line stops 5 cm short of the corner
    CHECK_FALSE(net.face_at({5'000, 5'000}, true).has_value());
    const auto open = net.open_ends();
    REQUIRE_EQ(open.size(), std::size_t{2});
    // One end is the short line's, 5 cm from the corner it missed ...
    const auto shorter = std::find_if(open.begin(), open.end(),
                                      [](const OpenEnd& e) { return e.at == Point2{0, 50}; });
    REQUIRE(shorter != open.end());
    CHECK_EQ(shorter->distance, Mm{50});
    CHECK_EQ(shorter->nearest, (Point2{0, 0}));
    CHECK_EQ(shorter->source, std::uint32_t{4});
}

TEST_CASE("DÜZLEMSEL AĞ: köprü istenince boşluk kapanır ve kaydedilir")
{
    NEEDS_NETWORK();
    const Network net = built(parcel(50), 10, 100);
    REQUIRE_EQ(net.bridges().size(), std::size_t{1});
    CHECK_EQ(net.bridges().front().width, Mm{50});
    const auto face = net.face_at({5'000, 5'000}, true);
    REQUIRE(face.has_value());
    CHECK_EQ(face->area, Mm2{200'000'000});
    CHECK_EQ(face->outer.bridges, (std::vector<std::size_t>{0}));
    // A bridge shorter than the gap is not laid.
    const Network narrow = built(parcel(50), 10, 40);
    CHECK(narrow.bridges().empty());
    CHECK_FALSE(narrow.face_at({5'000, 5'000}, true).has_value());
}

TEST_CASE("DÜZLEMSEL AĞ: düğüm toleransı içindeki uç birleşir ve bu sayılır")
{
    NEEDS_NETWORK();
    const Network net = built(parcel(5)); // 5 mm short, tolerance 10 mm
    const auto face   = net.face_at({5'000, 5'000}, true);
    REQUIRE(face.has_value());
    CHECK_EQ(net.snaps().moved, std::size_t{1});
    CHECK_EQ(net.snaps().largest, Mm{5});
    CHECK(net.open_ends().empty());
}

TEST_CASE("DÜZLEMSEL AĞ: kenara birkaç milimetre varmayan T birleşimi o kenarda buluşur")
{
    NEEDS_NETWORK();
    auto pieces = parcel();
    pieces.push_back(seg({10'000, 3}, {10'000, 9'996}, 5)); // 3 mm and 4 mm short of the sides
    const Network net = built(pieces);
    const auto left   = net.face_at({5'000, 5'000}, true);
    const auto right  = net.face_at({15'000, 5'000}, true);
    REQUIRE(left.has_value());
    REQUIRE(right.has_value());
    // The stray ends moved onto the sides; the sides did not bend to them.
    CHECK_EQ(left->area, Mm2{100'000'000});
    CHECK_EQ(right->area, Mm2{100'000'000});
    CHECK(net.open_ends().empty());
    CHECK_EQ(net.snaps().moved, std::size_t{2});
    CHECK_EQ(net.snaps().largest, Mm{4});
}

TEST_CASE("DÜZLEMSEL AĞ: yay yay kalır; D biçimli alanın alanı yarım daire")
{
    NEEDS_NETWORK();
    // A diameter from (-5,0) to (5,0) and the upper half circle back.
    NetworkPiece arc;
    arc.piece         = arc_piece({0, 0}, 5'000, {5'000, 0}, {-5'000, 0}, true);
    arc.source        = 2;
    const Network net = built({seg({-5'000, 0}, {5'000, 0}, 1), arc});
    const auto face   = net.face_at({0, 2'000}, true);
    REQUIRE(face.has_value());
    CHECK_EQ(face->outer.path.pieces.size(), std::size_t{2});
    REQUIRE_EQ(arcs_in(face->outer), std::size_t{1});
    const auto a = std::find_if(face->outer.path.pieces.begin(), face->outer.path.pieces.end(),
                                [](const PathPiece& p) { return p.kind == PathPiece::Kind::Arc; });
    CHECK_EQ(a->centre, (Point2{0, 0}));
    CHECK_EQ(a->radius, Mm{5'000});
    CHECK_EQ(a->sweep_udeg, kUDegFullCircle / 2); // one arc again, not the arrangement's two
    CHECK_EQ(face->area, circular_segment_area(5'000, kUDegFullCircle / 2));
}

TEST_CASE("DÜZLEMSEL AĞ: kesişen çizgi ağı her gözü ayrı yüz yapar; taşan uçlar bozmaz")
{
    NEEDS_NETWORK();
    // A 2 × 2 grid of 10 m cells, every line running 1 m past the frame.
    std::vector<NetworkPiece> grid;
    std::uint32_t id = 1;
    for (Mm k = 0; k <= 20'000; k += 10'000) {
        grid.push_back(seg({-1'000, k}, {21'000, k}, id++));
        grid.push_back(seg({k, -1'000}, {k, 21'000}, id++));
    }
    const Network net = built(grid);
    const auto faces  = net.faces(true);
    REQUIRE_EQ(faces.size(), std::size_t{4});
    for (const NetworkFace& f : faces)
        CHECK_EQ(f.area, Mm2{100'000'000});
    CHECK_EQ(net.open_ends().size(), std::size_t{12}); // every overhang ends in the open
    // AN OVERSHOOT IS NOT A GAP. Each overhang ends a metre past the frame
    // line it crossed, and walking back to that crossing is a metre too: the
    // crossing is a node of the network, and a bridge there would close a
    // sliver, not a gap.
    for (const OpenEnd& end : net.open_ends())
        CHECK_FALSE(end.has_nearest);
    // Listed by position: the lowest-left cell first.
    CHECK_EQ(faces.front().outer.path.pieces.front().from, (Point2{0, 0}));
}

TEST_CASE("DÜZLEMSEL AĞ: taşan uçlar boşluk sayılmaz; kısa kalan çizgi ve bozuk köşe sayılır")
{
    NEEDS_NETWORK();
    // Six parcels drawn as lines that run a metre past the frame; the inner
    // line at x=20 stops 2.5 m short of the top, and the right frame line
    // stops 1.5 m above the bottom one.
    const Network net = built(
        {seg({-1'000, 0}, {31'000, 0}, 1), seg({-1'000, 10'000}, {31'000, 10'000}, 2),
         seg({-1'000, 20'000}, {31'000, 20'000}, 3), seg({0, -1'000}, {0, 21'000}, 4),
         seg({10'000, -1'000}, {10'000, 21'000}, 5), seg({20'000, -1'000}, {20'000, 17'500}, 6),
         seg({30'000, 21'000}, {30'000, 1'500}, 7)});
    const auto open = net.open_ends();
    const auto at   = [&open](Point2 p) {
        return std::find_if(open.begin(), open.end(), [p](const OpenEnd& e) { return e.at == p; });
    };
    // The short inner line: 2.5 m below the top line.
    REQUIRE(at({20'000, 17'500}) != open.end());
    CHECK(at({20'000, 17'500})->has_nearest);
    CHECK_EQ(at({20'000, 17'500})->nearest, (Point2{20'000, 20'000}));
    CHECK_EQ(at({20'000, 17'500})->distance, Mm{2'500});
    // The broken corner: the frame line 1.5 m above the bottom line.
    REQUIRE(at({30'000, 1'500}) != open.end());
    CHECK_EQ(at({30'000, 1'500})->nearest, (Point2{30'000, 0}));
    CHECK_EQ(at({30'000, 1'500})->distance, Mm{1'500});
    // Overshoots past a crossing measure nothing: the crossing is theirs.
    for (const Point2 p : {Point2{0, 21'000}, Point2{10'000, 21'000}, Point2{-1'000, 10'000},
                           Point2{-1'000, 20'000}, Point2{0, -1'000}, Point2{10'000, -1'000}}) {
        REQUIRE(at(p) != open.end());
        CHECK_FALSE(at(p)->has_nearest);
    }
    // And the faces that do close are the four whole cells and the one the
    // short line leaves open at the top, merged.
    CHECK_EQ(net.faces(true).size(), std::size_t{4});
}

TEST_CASE("DÜZLEMSEL AĞ: üst üste çizilmiş iki çizgi örtüşme olarak bulunur")
{
    NEEDS_NETWORK();
    auto pieces = parcel();
    pieces.push_back(seg({5'000, 0}, {15'000, 0}, 9)); // drawn again over the south side
    const Network net   = built(pieces);
    const auto overlaps = net.overlaps();
    REQUIRE_EQ(overlaps.size(), std::size_t{1});
    CHECK_EQ(overlaps.front(), (std::vector<std::uint32_t>{1, 9}));
    const auto face = net.face_at({5'000, 5'000}, true);
    REQUIRE(face.has_value());
    CHECK_EQ(face->area, Mm2{200'000'000});
}

TEST_CASE("DÜZLEMSEL AĞ: sınıra bir çizgiyle bağlı ada da delik olur")
{
    NEEDS_NETWORK();
    auto pieces = parcel();
    // A 4 × 2 m shed in the parcel, joined to the south side by a path.
    pieces.push_back(seg({8'000, 4'000}, {12'000, 4'000}, 5));
    pieces.push_back(seg({12'000, 4'000}, {12'000, 6'000}, 5));
    pieces.push_back(seg({12'000, 6'000}, {8'000, 6'000}, 5));
    pieces.push_back(seg({8'000, 6'000}, {8'000, 4'000}, 5));
    pieces.push_back(seg({10'000, 0}, {10'000, 4'000}, 6));
    const Network net = built(pieces);
    const auto face   = net.face_at({3'000, 8'000}, true);
    REQUIRE(face.has_value());
    REQUIRE_EQ(face->holes.size(), std::size_t{1});
    CHECK_EQ(face->area, Mm2{200'000'000 - 8'000'000});
}

TEST_CASE("DÜZLEMSEL AĞ: bölge sorgusu çizimden okur; görünüm değil, çizim belirler")
{
    NEEDS_NETWORK();
    Document doc;
    Op op;
    const LayerId lyr = layer(doc);
    const std::array<Point2, 2> south{Point2{0, 0}, Point2{20'000, 0}};
    const std::array<Point2, 2> east{Point2{20'000, 0}, Point2{20'000, 10'000}};
    const std::array<Point2, 2> north{Point2{20'000, 10'000}, Point2{0, 10'000}};
    const std::array<Point2, 2> west{Point2{0, 10'000}, Point2{0, 0}};
    for (const auto* line : {&south, &east, &north, &west})
        REQUIRE(doc.add_polyline(lyr, *line, op).ok());
    REQUIRE(doc.add_circle(lyr, {10'000, 5'000}, 2'000, op).ok());
    // A far-away square the answer must not depend on.
    const std::array<Point2, 4> far{Point2{900'000, 0}, Point2{910'000, 0}, Point2{910'000, 10'000},
                                    Point2{900'000, 10'000}};
    const std::array<RingGeometry::RingInput, 1> rings{
        RingGeometry::RingInput{far, RingRole::Exterior, 0}};
    REQUIRE(doc.add_area(lyr, rings, op).ok());

    RegionQuery q;
    q.at     = {3'000, 3'000};
    auto got = region_at(doc, q);
    REQUIRE(got.ok());
    const Region& r = got.value();
    REQUIRE(r.face.has_value());
    CHECK_EQ(r.face->holes.size(), std::size_t{1});
    CHECK_EQ(r.sources.size(), std::size_t{5}); // four lines and the circle
    CHECK_FALSE(r.approximate);

    // The same click with a boundary set of the four lines alone: no pool.
    RegionQuery only = q;
    for (EntityId e = 0; e < 4; ++e)
        only.only.push_back(e);
    auto lines = region_at(doc, only);
    REQUIRE(lines.ok());
    REQUIRE(lines.value().face.has_value());
    CHECK(lines.value().face->holes.empty());

    // Clicking on a line is not clicking in a region.
    RegionQuery on = q;
    on.at          = {10'000, 0};
    auto online    = region_at(doc, on);
    REQUIRE(online.ok());
    CHECK(online.value().on_linework);
    CHECK_FALSE(online.value().face.has_value());

    // Inside the far square, far beyond the first reach.
    RegionQuery away = q;
    away.at          = {905'000, 5'000};
    auto square      = region_at(doc, away);
    REQUIRE(square.ok());
    REQUIRE(square.value().face.has_value());
    CHECK_EQ(square.value().face->area, Mm2{100'000'000});
}

TEST_CASE("DÜZLEMSEL AĞ: kapanmayan bölgenin açık uçları tıklanan yere yakından uzağa")
{
    NEEDS_NETWORK();
    Document doc;
    Op op;
    const LayerId lyr = layer(doc);
    const std::array<Point2, 4> almost{Point2{0, 120}, Point2{0, 0}, Point2{20'000, 0},
                                       Point2{20'000, 10'000}};
    const std::array<Point2, 2> top{Point2{20'000, 10'000}, Point2{0, 10'000}};
    const std::array<Point2, 2> down{Point2{0, 10'000}, Point2{0, 4'000}}; // stops 3.88 m short
    REQUIRE(doc.add_polyline(lyr, almost, op).ok());
    REQUIRE(doc.add_polyline(lyr, top, op).ok());
    REQUIRE(doc.add_polyline(lyr, down, op).ok());
    RegionQuery q;
    q.at     = {5'000, 5'000};
    auto got = region_at(doc, q);
    REQUIRE(got.ok());
    CHECK_FALSE(got.value().face.has_value());
    REQUIRE_EQ(got.value().open.size(), std::size_t{2});
    CHECK_EQ(got.value().open.front().at, (Point2{0, 4'000})); // nearer the click
    CHECK_EQ(got.value().open.front().distance, Mm{3'880});
    // THE LINE AN END BELONGS TO IS NOT ITS GAP. The polyline's own first leg
    // is 12 cm long, so its corner at the origin is 12 cm from the open end —
    // and walking there along the line is 12 cm too. The gap is the 3.88 m
    // across to the other line, not a sliver back along its own.
    CHECK_EQ(got.value().open.back().at, (Point2{0, 120}));
    CHECK_EQ(got.value().open.back().nearest, (Point2{0, 4'000}));
    CHECK_EQ(got.value().open.back().distance, Mm{3'880});

    // Bridging a gap that wide is a choice, and it is reported.
    q.bridge   = 4'000;
    auto fixed = region_at(doc, q);
    REQUIRE(fixed.ok());
    REQUIRE(fixed.value().face.has_value());
    REQUIRE_EQ(fixed.value().bridges.size(), std::size_t{1});
    CHECK_EQ(fixed.value().bridges.front().width, Mm{3'880});
}

TEST_CASE(
    "DÜZLEMSEL AĞ: bir zikzağın kendi köşeleri boşluk sayılmaz; yakındaki başka çizgi sayılır")
{
    NEEDS_NETWORK();
    // A zig-zag with short legs ending at (0,0): its own next corners are all
    // within a metre of the end, and none of them is a gap.
    std::vector<NetworkPiece> pieces{seg({0, 0}, {500, 400}, 1), seg({500, 400}, {1'000, 0}, 1),
                                     seg({1'000, 0}, {1'500, 400}, 1),
                                     seg({1'500, 400}, {20'000, 400}, 1)};
    Network lonely = built(pieces);
    for (const OpenEnd& end : lonely.open_ends())
        CHECK_FALSE(end.has_nearest); // nothing but its own line to be near
    // A separate line 30 cm below the end IS a gap, and it is the nearest.
    pieces.push_back(seg({-2'000, -300}, {2'000, -300}, 2));
    const Network net = built(pieces);
    const auto open   = net.open_ends();
    const auto end    = std::find_if(open.begin(), open.end(),
                                     [](const OpenEnd& e) { return e.at == Point2{0, 0}; });
    REQUIRE(end != open.end());
    REQUIRE(end->has_nearest);
    CHECK_EQ(end->nearest, (Point2{0, -300}));
    CHECK_EQ(end->distance, Mm{300});
}

TEST_CASE("DÜZLEMSEL AĞ: elipsle sınırlanan bölge çizildiği hâliyle bulunur ve bu söylenir")
{
    NEEDS_NETWORK();
    Document doc;
    Op op;
    const LayerId lyr = layer(doc);
    REQUIRE(doc.add_ellipse(lyr, {0, 0}, {6'000, 0}, {0, 3'000}, op).ok());
    RegionQuery q;
    q.at     = {100, 100};
    auto got = region_at(doc, q);
    REQUIRE(got.ok());
    REQUIRE(got.value().face.has_value());
    CHECK(got.value().approximate);
    CHECK(got.value().deviation > 0);
    // π·6·3 m² ≈ 56.55 m², within what the drawing's chords give up.
    const double area = static_cast<double>(got.value().face->area) / 1e6;
    CHECK(area > 55.5);
    CHECK(area < 56.6);
}

TEST_CASE("DÜZLEMSEL AĞ: ağ olmadan derlenmişse söyler")
{
    if (network_available()) return;
    auto net = Network::build(parcel(), 10);
    REQUIRE_FALSE(net.ok());
    CHECK(net.error().message.find("CGAL") != std::string::npos);
}

// ================================================================== SINIR ===

namespace {

struct Rig
{
    Document doc;
    kentos::command::Registry reg;
    kentos::command::Journal journal;
    kentos::command::UndoStack undo;
    kentos::command::Bus bus{doc, reg, journal, undo};
    std::string said;
    std::vector<kentos::command::MeasureMark> marks;

    Rig()
    {
        kentos::command::register_builtin_commands(reg);
        bus.on_echo         = [this](std::string_view s) { said.append(s).append("\n"); };
        bus.on_measure_mark = [this](const kentos::command::MeasureMark& m) { marks.push_back(m); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, kentos::command::Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, kentos::command::Origin::Test);
        REQUIRE_FALSE(r.ok());
        return r.error().message;
    }

    EntityId slot(std::int64_t key) const
    {
        return doc.slot_of(static_cast<EntityKey>(static_cast<std::uint64_t>(key)));
    }

    KindId kind(std::int64_t key) const { return doc.entities().kind[slot(key)]; }

    std::vector<RingRole> roles(std::int64_t key) const
    {
        const RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot(key)]);
        std::vector<RingRole> out;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            out.push_back(doc.geometry().ring_role[r]);
        return out;
    }
};

/// The 20 × 10 m parcel as four separate lines, keys 1–4.
void four_lines(Rig& r, const std::string& last = "ÇİZGİ 0,10 0,0")
{
    r.run("ÇİZGİ 0,0 20,0");
    r.run("ÇİZGİ 20,0 20,10");
    r.run("ÇİZGİ 20,10 0,10");
    r.run(last);
}

} // namespace

TEST_CASE("SINIR: dört ayrı çizginin içine tıklamak 200 m²'lik alanı çıkarır, çizgiler kalır")
{
    NEEDS_NETWORK();
    Rig r;
    four_lines(r);
    r.run("SINIR nokta=5,5");
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{5});
    CHECK_EQ(r.kind(5), kPolylineKind);
    CHECK_EQ(r.roles(5), (std::vector<RingRole>{RingRole::Exterior}));
    CHECK_EQ(r.doc.entity_area(r.slot(5)), Mm2{200'000'000});
    CHECK(r.said.find("Sınır çıkarıldı: 200,00 m² alan; 4 nesnenin çizgisinden.") !=
          std::string::npos);
    // One undo step takes the area away and leaves the lines.
    r.run("GERİAL");
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{4});
}

TEST_CASE("SINIR: içerideki havuz delik olur; ada=hayır onu yok sayar; içine tıklanırsa daire")
{
    NEEDS_NETWORK();
    Rig r;
    four_lines(r);
    r.run("DAİRE 10,5 12,5"); // a 2 m pool, key 5
    // Inside the pool first: the pool itself, a circle again.
    r.run("SINIR nokta=10,5");
    CHECK_EQ(r.kind(6), kCircleKind);

    r.run("SINIR nokta=3,3");
    CHECK_EQ(r.roles(7), (std::vector<RingRole>{RingRole::Exterior, RingRole::Interior}));
    const double net = static_cast<double>(r.doc.entity_area(r.slot(7))) / 1e6;
    CHECK(net > 200.0 - 12.6); // 200 − π·2² ≈ 187.43 m², the pool drawn as chords
    CHECK(net < 200.0 - 12.4);
    CHECK(r.said.find("1 ada") != std::string::npos);
    CHECK(r.said.find("kirişlerle") != std::string::npos);

    r.run("SINIR nokta=3,3 ada=hayır");
    CHECK_EQ(r.roles(8), (std::vector<RingRole>{RingRole::Exterior}));
    CHECK_EQ(r.doc.entity_area(r.slot(8)), Mm2{200'000'000});
}

TEST_CASE("SINIR: yay sınırı yay kalır — yarım daire yaylı çoklu çizgi olur")
{
    NEEDS_NETWORK();
    Rig r;
    r.run("ÇİZGİ -5,0 5,0");
    r.run("YAY 0,0 5,0 -5,0"); // centre, start, end: the upper half
    r.run("SINIR nokta=0,2");
    CHECK_EQ(r.kind(3), kArcPolylineKind);
    const Mm2 half = circular_segment_area(5'000, kUDegFullCircle / 2);
    CHECK_EQ(r.doc.entity_area(r.slot(3)), half);
}

TEST_CASE(
    "SINIR: 5 cm açık kalan köşe sessizce kapanmaz; uçlar işaretlenir, köprü istenirse kapanır")
{
    NEEDS_NETWORK();
    Rig r;
    four_lines(r, "ÇİZGİ 0,10 0,0.05");
    const std::string why = r.refused("SINIR nokta=5,5");
    CHECK(why.find("kapanmıyor") != std::string::npos);
    CHECK(why.find("5 cm") != std::string::npos);
    CHECK(why.find("bosluk=") != std::string::npos);
    REQUIRE_FALSE(r.marks.empty());
    CHECK_EQ(r.marks.front().shape, kentos::command::MeasureMark::Shape::Gap);
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{4}); // nothing written

    r.run("SINIR nokta=5,5 bosluk=100");
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{5});
    CHECK_EQ(r.doc.entity_area(r.slot(5)), Mm2{200'000'000});
    CHECK(r.said.find("1 boşluk köprülendi: 5 cm") != std::string::npos);
}

TEST_CASE("SINIR: çizginin üstüne ya da hiçbir şeyin çevrelemediği yere tıklamak reddedilir")
{
    NEEDS_NETWORK();
    Rig r;
    four_lines(r);
    CHECK(r.refused("SINIR nokta=10,0").find("çizginin üstünde") != std::string::npos);
    CHECK(r.refused("SINIR nokta=50,50").find("kapalı bir çizgi yok") != std::string::npos);
    CHECK(r.refused("SINIR nokta=5,5 bosluk=-1").find("eksi") != std::string::npos);
}

TEST_CASE("SINIR: sınır kümesi verilirse yalnız onlar sayılır")
{
    NEEDS_NETWORK();
    Rig r;
    four_lines(r);
    r.run("ÇİZGİ 10,0 10,10"); // splits the parcel, key 5
    r.run("SINIR nokta=5,5");
    CHECK_EQ(r.doc.entity_area(r.slot(6)), Mm2{100'000'000});
    r.run("SINIR nokta=5,5 nesneler=1 nesneler=2 nesneler=3 nesneler=4");
    CHECK_EQ(r.doc.entity_area(r.slot(7)), Mm2{200'000'000});
}

// ================================================================ TEMİZLE ===

TEST_CASE(
    "TEMİZLE: yinelenen, boş ve tekrarlanan köşeli nesneler bulunur; bul hiçbir şeyi değiştirmez")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");            // 1
    r.run("ALAN 20,10 0,10 0,0 20,0");            // 2: the same parcel from another corner
    r.run("ALAN 30,0 40,0 40,0.004 40,10 30,10"); // 3: a corner 4 mm from the one before
    r.run("ÇİZGİ 50,0 50,0.003");                 // 4: a line of 3 mm: nothing, at 1 cm tolerance
    r.run("ÇİZGİ 60,0 70,0");                     // 5
    r.run("ÇİZGİ 70,0 60,0");                     // 6: the same line drawn back
    r.run("ALAN 0,0 20,0 20,10 0,10");            // 7: parcel 1 a third time
    const std::uint64_t before = r.doc.content_hash();

    const auto found = find_redundant(r.doc, {}, 10);
    REQUIRE_EQ(found.size(), std::size_t{5});
    const auto count = [&found](RedundancyKind k) {
        return std::ranges::count_if(found, [k](const Redundancy& x) { return x.kind == k; });
    };
    CHECK_EQ(count(RedundancyKind::Duplicate), 3); // 2, 6 and 7
    CHECK_EQ(count(RedundancyKind::Empty), 1);     // 4
    CHECK_EQ(count(RedundancyKind::RepeatedVertex), 1);
    // A duplicate is reported against the oldest copy.
    for (const Redundancy& x : found)
        if (x.kind == RedundancyKind::Duplicate && x.entity == r.slot(7))
            CHECK_EQ(x.kept, r.slot(1));

    r.run("TEMİZLE");
    CHECK_EQ(r.doc.content_hash(), before); // bul changes nothing
    CHECK(r.said.find("Temizlik (bütün çizim): 3 yinelenen, 1 boş nesne, 1 nesnede 1 tekrarlanan "
                      "köşe. Seçildi ve işaretlendi.") != std::string::npos);
    CHECK_EQ(r.bus.selection().size(), std::size_t{5});
    CHECK_EQ(r.marks.size(), std::size_t{5});
}

TEST_CASE(
    "TEMİZLE onar: tek adımda siler ve düzeltir, alanı önce/sonra söyler, veri taşıyanı silmez")
{
    {
        Rig r;
        r.run("ALAN 0,0 20,0 20,10 0,10");                // 1
        r.run("ALAN 0,0 20,0 20,10 0,10");                // 2: a copy
        r.run("ALAN 30,0 40,0 40.009,0.009 40,10 30,10"); // 3: a corner 9 mm from the last
        r.run("ÇİZGİ 50,0 50,0.003");                     // 4: nothing
        const std::uint64_t before = r.doc.content_hash();

        r.run("TEMİZLE islem=onar");
        CHECK_FALSE(r.doc.alive(r.slot(2)));
        CHECK_FALSE(r.doc.alive(r.slot(4)));
        CHECK_EQ(r.doc.geometry()
                     .ring_count[r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(3)]).first],
                 4u);
        CHECK(r.said.find("Temizlik onarıldı (bütün çizim): 2 nesne silindi, 1 nesneden 1 köşe "
                          "çıkarıldı; değişen alanlar önce 100,05 m², sonra 100,00 m².") !=
              std::string::npos);
        CHECK(r.said.find("Nesne 3: 1 köşe çıkarıldı; alan önce 100,05 m², sonra 100,00 m².") !=
              std::string::npos);
        CHECK(r.said.find("Nesne 2 silindi (aynısı nesne 1 duruyor, alanı 200,00 m²).") !=
              std::string::npos);
        CHECK(r.said.find("Nesne 4 silindi (hiçbir şey çizmiyordu).") != std::string::npos);

        // One undo step brings every one of them back.
        r.run("GERİAL");
        CHECK_EQ(r.doc.content_hash(), before);
    }
    {
        // A copy that carries data its twin lacks is kept, and named.
        Rig r;
        r.run("ALAN 0,0 20,0 20,10 0,10"); // 1
        r.run("ALAN 0,0 20,0 20,10 0,10"); // 2: a copy
        r.run("ALAN 0,0 20,0 20,10 0,10"); // 3: a copy that holds an attribute
        r.run("SÜTUN kimlik=ada tur=metin");
        r.run("ÖZNİTELİK ada 3 1284");
        r.run("TEMİZLE islem=onar");
        CHECK_FALSE(r.doc.alive(r.slot(2)));
        CHECK(r.doc.alive(r.slot(3)));
        CHECK(r.said.find("Nesne 3 silinmedi: öznitelik taşıyor") != std::string::npos);
    }
}

TEST_CASE("TOPOLOJİ: aynı çekirdekten yinelenen, boş, tekrarlanan köşe ve çizgi ağı boşluğu")
{
    NEEDS_NETWORK();
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10"); // 1
    r.run("ALAN 0,0 20,0 20,10 0,10"); // 2: a copy
    r.run("ÇİZGİ 50,0 50,0.003");      // 3: nothing
    r.run("ÇİZGİ 60,0 70,0");          // 4
    r.run("ÇİZGİ 70,0 70,10");         // 5
    r.run("ÇİZGİ 70,10 60,10");        // 6
    r.run("ÇİZGİ 60,10 60,0.5");       // 7: stops 50 cm short of the corner
    kentos::domain::cadastre::register_cadastre_commands(r.reg);
    r.run("TOPOLOJİ");
    CHECK(r.said.find("Nesne 2, nesne 1'in aynısı (yinelenen; TEMİZLE islem=onar siler).") !=
          std::string::npos);
    CHECK(r.said.find("Nesne 3: uzunluğu yok, hiçbir şey çizmiyor.") != std::string::npos);
    CHECK(r.said.find("açık uç, en yakın çizgiye 50 cm (boşluk).") != std::string::npos);
    // The copies also overlap, as the pairwise check has always said.
    CHECK(r.said.find("örtüşüyor") != std::string::npos);
}

TEST_CASE("TOPOLOJİ: kılavuzdaki çizgi ağı örneği kelimesi kelimesine")
{
    NEEDS_NETWORK();
    Rig r;
    kentos::domain::cadastre::register_cadastre_commands(r.reg);
    r.run("ÇİZGİ 60,0 70,0");
    r.run("ÇİZGİ 70,0 70,10");
    r.run("ÇİZGİ 70,10 60,10");
    r.run("ÇİZGİ 60,10 60,0.5");
    r.run("ÇİZGİ 60,0 70,0");
    r.said.clear();
    r.run("TOPOLOJİ");
    // Word for word what docs/komutlar/topology.md prints.
    CHECK_EQ(r.said,
             std::string("Topoloji denetimi (bütün çizim, 5 nesne): 2 kusur — 1 yinelenen nesne, "
                         "1 boşluk.\n"
                         "  Nesne 5, nesne 1'in aynısı (yinelenen; TEMİZLE islem=onar "
                         "siler).\n"
                         "  Nesne 1: açık uç, en yakın çizgiye 50 cm (boşluk).\n"
                         "  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.\n"));
}

TEST_CASE("TEMİZLE: kılavuzdaki örnek kelimesi kelimesine; bul'un seçtiğini onar onarır")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("ALAN 30,0 40,0 40.009,0.009 40,10 30,10");
    r.run("ÇİZGİ 50,0 50,0.003");
    r.said.clear();
    r.run("TEMİZLE");
    // The copies are now the selection, and their originals are not in it:
    // the repair still knows what they are copies of.
    r.run("TEMİZLE islem=onar");
    CHECK_EQ(
        r.said,
        std::string("Temizlik (bütün çizim): 1 yinelenen, 1 boş nesne, 1 nesnede 1 tekrarlanan "
                    "köşe. Seçildi ve işaretlendi.\n"
                    "  Nesne 2, nesne 1'in aynısı (aynı tür, aynı katman, aynı köşeler).\n"
                    "  Nesne 3: 1 köşe bir öncekiyle aynı yerde.\n"
                    "  Nesne 4 hiçbir şey çizmiyor (uzunluğu ya da alanı yok).\n"
                    "  Onarmak için: TEMİZLE islem=onar\n"
                    "Temizlik onarıldı (3 nesne): 2 nesne silindi, 1 nesneden 1 köşe "
                    "çıkarıldı; değişen alanlar önce 100,05 m², sonra 100,00 m².\n"
                    "  Nesne 2 silindi (aynısı nesne 1 duruyor, alanı 200,00 m²).\n"
                    "  Nesne 3: 1 köşe çıkarıldı; alan önce 100,05 m², sonra 100,00 m².\n"
                    "  Nesne 4 silindi (hiçbir şey çizmiyordu).\n"));
    CHECK(r.doc.alive(r.slot(1)));
}
