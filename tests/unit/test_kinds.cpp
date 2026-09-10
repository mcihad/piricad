// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: the kind registry's general mutators (model.md R9a, R22, R26).
//
// `Document::add_kind` is what every `add_*` spells for one kind, and what a file
// reader hands an entity of ANY kind to — the ones this build knows, validated by
// their own `KindSpec`, and the ones it does not, preserved as given. These cases
// lock that contract: the floor a kind declares is enforced, an unknown kind is
// kept and refused by every edit, and a curve's measures come from the kind.
#include "kentos_test.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <span>
#include <vector>

using namespace kentos::core;

namespace {

RingGeometry::RingInput open_ring(const std::vector<Point2>& pts)
{
    return RingGeometry::RingInput{pts, RingRole::Open, 0};
}

} // namespace

TEST_CASE("TÜR: add_kind türün kendi zeminini uygular")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");

    // A circle is a centre and a handle due east; three vertices are not one.
    const std::vector<Point2> three{{0, 0}, {5000, 0}, {0, 5000}};
    const auto refused = doc.add_kind(lyr, kCircleKind, std::vector{open_ring(three)}, {}, undo);
    CHECK_FALSE(refused.ok());
    CHECK_EQ(doc.live_entity_count(), 0u);

    // A handle that is not due east is refused too: the radius reads back as an
    // exact integer subtraction only when it is.
    const std::vector<Point2> tilted{{0, 0}, {3000, 4000}};
    CHECK_FALSE(doc.add_kind(lyr, kCircleKind, std::vector{open_ring(tilted)}, {}, undo).ok());

    // The five built-in kinds carry no payload, and say so.
    const std::vector<Point2> circle{{10000, 10000}, {15000, 10000}};
    const std::uint8_t junk[2]{1, 2};
    CHECK_FALSE(doc.add_kind(lyr, kCircleKind, std::vector{open_ring(circle)},
                             std::span<const std::uint8_t>(junk, 2), undo)
                    .ok());

    // Well-formed, it is the same circle `add_circle` makes: the box is the
    // kind's, and the area is pi r squared, not the shoelace over two vertices.
    const auto made = doc.add_kind(lyr, kCircleKind, std::vector{open_ring(circle)}, {}, undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();
    CHECK_EQ(doc.entities().kind[e], kCircleKind);
    CHECK_EQ(doc.entities().box_of(e), (Box2{5000, 5000, 15000, 15000}));
    CHECK(std::abs(doc.entity_area(e) - 78539816) <= 1);
    CHECK(std::abs(doc.entity_perimeter(e) - 31416) <= 1);

    // The inverse hides it again.
    CHECK_EQ(undo.kind, Op::Kind::SetEntityAlive);
    REQUIRE(doc.apply(undo).ok());
    CHECK_EQ(doc.live_entity_count(), 0u);
}

TEST_CASE("TÜR: bilinmeyen tür olduğu gibi korunur, çizilir, düzenlenemez")
{
    // model.md R26. A kind from a later build or a plugin arrives with rings and
    // bytes this build cannot interpret. It is kept exactly as given, bounded by
    // its rings so it culls and zooms like anything else, and every edit that
    // would have to understand it is refused.
    Document doc;
    Op undo;
    const LayerId lyr        = doc.ensure_layer("TEST");
    constexpr KindId kFuture = 4242;

    const std::vector<Point2> pts{{0, 0}, {8000, 0}, {8000, 6000}};
    const std::uint8_t bytes[4]{9, 8, 7, 6};
    const auto made = doc.add_kind(lyr, kFuture, std::vector{open_ring(pts)},
                                   std::span<const std::uint8_t>(bytes, 4), undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    CHECK_EQ(doc.entities().kind[e], kFuture);
    CHECK_FALSE(doc.kind_known(e));
    CHECK(doc.entities().visible(e));
    CHECK_EQ(doc.entities().box_of(e), (Box2{0, 0, 8000, 6000}));

    const auto back = doc.geometry().payload_of(doc.entities().slot[e]);
    REQUIRE_EQ(back.size(), 4u);
    CHECK_EQ(back[0], 9);
    CHECK_EQ(back[3], 6);

    // Measured as its rings: an open ring has no area and its length is the run.
    CHECK_EQ(doc.entity_area(e), 0);
    CHECK_EQ(doc.entity_perimeter(e), 14000);

    // Non-editable: the rings and the payload are meaning this build cannot
    // preserve through a change, so the change is refused.
    const std::vector<Point2> moved{{1, 1}, {8001, 1}, {8001, 6001}};
    Op ignored;
    CHECK_FALSE(doc.set_geometry(e, std::vector{open_ring(moved)}, ignored).ok());
    const std::uint8_t other[1]{0};
    CHECK_FALSE(doc.set_kind_payload(e, std::span<const std::uint8_t>(other, 1), ignored).ok());
    CHECK_EQ(doc.geometry().payload_of(doc.entities().slot[e]).size(), 4u);

    // Hiding and erasing need no understanding of the kind, and stay possible.
    CHECK(doc.set_entity_hidden(e, true, ignored).ok());
    CHECK(doc.set_entity_alive(e, false, ignored).ok());
}

TEST_CASE("TÜR: yayın ortası ve üzerinde-olma sınaması açı hesabı olmadan")
{
    // Quarter arc, east to north: the midpoint is at 45°, r/√2 out on both axes.
    const Point2 c{100000, 100000};
    const Point2 mid = arc_midpoint(c, 10000, Point2{110000, 100000}, Point2{100000, 110000});
    CHECK(std::abs(mid.x - 107071) <= 1);
    CHECK(std::abs(mid.y - 107071) <= 1);

    // Half turn, east to west counter-clockwise: the midpoint is due north.
    CHECK_EQ(arc_midpoint(c, 10000, Point2{110000, 100000}, Point2{90000, 100000}),
             (Point2{100000, 110000}));

    // Three quarters, east to south counter-clockwise: the long way round, so the
    // midpoint is at 135°.
    const Point2 far = arc_midpoint(c, 10000, Point2{110000, 100000}, Point2{100000, 90000});
    CHECK(std::abs(far.x - 92929) <= 1);
    CHECK(std::abs(far.y - 107071) <= 1);

    // Coincident ends are a full turn; the midpoint is the far side.
    CHECK_EQ(arc_midpoint(c, 10000, Point2{110000, 100000}, Point2{110000, 100000}),
             (Point2{90000, 100000}));

    // On the drawn sweep, not merely on the circle.
    const Point2 east{110000, 100000}, north{100000, 110000};
    CHECK(on_arc(c, east, north, Point2{107071, 107071}));
    CHECK_FALSE(on_arc(c, east, north, Point2{92929, 92929}));
    CHECK(on_arc(c, east, Point2{100000, 90000}, Point2{92929, 107071})); // the long way
    CHECK_FALSE(on_arc(c, east, Point2{100000, 90000}, Point2{107071, 92929}));
    CHECK(on_arc(c, east, east, Point2{90000, 100000})); // a full turn holds everything
}

TEST_CASE("YABANCI: başka programın baytları yuvaya bağlanır, geri alınır, özete girer")
{
    // model.md R26a. XDATA from a DXF is kept as bytes under a tag: not read, not
    // shown beyond its count, folded into the fingerprint, and undone in one step.
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> pts{{0, 0}, {5000, 0}};
    const auto made = doc.add_polyline(lyr, pts, undo);
    REQUIRE(made.ok());
    const EntityId e           = made.value();
    const std::uint32_t gs     = doc.entities().slot[e];
    const std::uint64_t before = doc.content_hash();

    const std::uint8_t bytes[3]{0x10, 0x20, 0x30};
    Op detach;
    REQUIRE(doc.attach_foreign(e, kForeignDxfXdata, std::span<const std::uint8_t>(bytes, 3), detach)
                .ok());
    CHECK_EQ(doc.foreign().count_for(gs), 1u);
    const auto back = doc.foreign().bytes(gs, kForeignDxfXdata);
    REQUIRE_EQ(back.size(), 3u);
    CHECK_EQ(back[1], 0x20);
    CHECK(doc.content_hash() != before);

    // The source carried one; a second under the same tag is an invention.
    Op ignored;
    CHECK_FALSE(
        doc.attach_foreign(e, kForeignDxfXdata, std::span<const std::uint8_t>(bytes, 3), ignored)
            .ok());
    // Nothing attached is not a record.
    CHECK_FALSE(doc.attach_foreign(e, "x.other", std::span<const std::uint8_t>(), ignored).ok());
    // Another tag on the same slot is fine and counted.
    REQUIRE(
        doc.attach_foreign(e, "x.other", std::span<const std::uint8_t>(bytes, 1), ignored).ok());
    CHECK_EQ(doc.foreign().count_for(gs), 2u);
    CHECK_EQ(doc.foreign().count_for(gs + 1), 0u);

    // Undo detaches; the inverse of that attaches the same bytes again.
    CHECK_EQ(detach.kind, Op::Kind::DetachForeign);
    Op reattach;
    REQUIRE(doc.apply(detach, &reattach).ok());
    CHECK(doc.foreign().bytes(gs, kForeignDxfXdata).empty());
    CHECK_EQ(reattach.kind, Op::Kind::AttachForeign);
    REQUIRE(doc.apply(reattach).ok());
    CHECK_EQ(doc.foreign().bytes(gs, kForeignDxfXdata).size(), 3u);
}

TEST_CASE("BLOK: tanım üyesi görünmez, dizinde yoktur, düzenlenemez; döngü reddedilir")
{
    // model.md R45. A definition's entities live in the entity table flagged
    // FlagInBlock: never culled in, never indexed, never picked, never edited in
    // place. The table is append-only and refuses a name taken under Turkish
    // folding and a definition that would contain itself.
    Document doc;
    Op undo;
    const LayerId lyr         = doc.ensure_layer("TEST");
    const std::uint64_t plain = doc.content_hash();

    const auto kapak = doc.add_block("Kapak", "rögar kapağı", Point2{0, 0});
    REQUIRE(kapak.ok());
    CHECK_FALSE(doc.add_block("KAPAK", "", Point2{0, 0}).ok());
    CHECK_EQ(doc.blocks().find("kapak"), kapak.value());
    CHECK(doc.content_hash() != plain);

    const std::vector<Point2> pts{{0, 0}, {600, 0}};
    const RingGeometry::RingInput ring{pts, RingRole::Open, 0};
    const auto member =
        doc.add_kind(lyr, kPolylineKind, std::span<const RingGeometry::RingInput>(&ring, 1), {},
                     undo, kapak.value());
    REQUIRE(member.ok());
    const EntityId m = member.value();
    CHECK((doc.entities().flags[m] & FlagInBlock) != 0);
    CHECK(doc.alive(m));
    CHECK_FALSE(doc.entities().visible(m));
    CHECK_FALSE(doc.entities().standalone(m));
    REQUIRE_EQ(doc.blocks().at(kapak.value()).members.size(), 1u);
    CHECK_EQ(doc.blocks().at(kapak.value()).members[0], doc.key_of(m));

    // Not a pick candidate, so no command reaches it by pointing.
    std::vector<EntityId> found;
    pick_candidates(doc, Box2{-100, -100, 700, 100}, found);
    CHECK(found.empty());

    // Not editable in place.
    const auto refused = doc.editable(m);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("Blok tanımındaki nesne") != std::string::npos);
    const std::vector<Point2> moved{{1, 1}, {601, 1}};
    CHECK_FALSE(
        doc.set_geometry(m, std::vector{RingGeometry::RingInput{moved, RingRole::Open, 0}}, undo)
            .ok());

    // Uses and cycles.
    const auto baca = doc.add_block("Baca", "", Point2{0, 0});
    REQUIRE(baca.ok());
    REQUIRE(doc.add_block_use(kapak.value(), baca.value()).ok());
    CHECK(doc.blocks().would_cycle(kapak.value(), kapak.value()));
    CHECK(doc.blocks().would_cycle(baca.value(), kapak.value()));
    CHECK_FALSE(doc.add_block_use(baca.value(), kapak.value()).ok());
    CHECK_FALSE(doc.add_kind(lyr, kPolylineKind, std::span<const RingGeometry::RingInput>(&ring, 1),
                             {}, undo, 99)
                    .ok());
}

TEST_CASE("BELİRLENİMCİLİK: atan2 eksen ve köşegenlerde tam, arada libm'in 2 µderecesi içinde")
{
    // Axes and diagonals come out of integer comparisons, so they are exact.
    CHECK_EQ(atan2_udeg(0, 5), 0);
    CHECK_EQ(atan2_udeg(5, 0), 90 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(0, -5), 180 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(-5, 0), 270 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(7, 7), 45 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(7, -7), 135 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(-7, -7), 225 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(-7, 7), 315 * kUDegPerDegree);
    CHECK_EQ(atan2_udeg(0, 0), 0);

    // In between: within two micro-degrees of libm, all the way round.
    for (int i = 0; i < 720; ++i) {
        const double a = (static_cast<double>(i) + 0.37) * 0.5 * 3.14159265358979323846 / 180.0;
        const auto dx  = static_cast<std::int64_t>(std::llround(std::cos(a) * 123456789.0));
        const auto dy  = static_cast<std::int64_t>(std::llround(std::sin(a) * 123456789.0));
        double ref     = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
        if (ref < 0.0) ref += 2.0 * 3.14159265358979323846;
        const auto ref_udeg =
            static_cast<std::int64_t>(std::llround(ref * 180e6 / 3.14159265358979323846)) %
            kUDegFullCircle;
        const std::int64_t ours = atan2_udeg(dy, dx);
        std::int64_t diff       = ours - ref_udeg;
        if (diff > kUDegFullCircle / 2) diff -= kUDegFullCircle;
        if (diff < -kUDegFullCircle / 2) diff += kUDegFullCircle;
        INFO("i=" << i << " ours=" << ours << " ref=" << ref_udeg);
        CHECK(std::abs(diff) <= 2);
    }

    // mul_div_round: half away from zero, mirrored, exact past 2^53.
    CHECK_EQ(mul_div_round(7, 1, 2), 4);
    CHECK_EQ(mul_div_round(-7, 1, 2), -4);
    CHECK_EQ(mul_div_round(5, 3, 10), 2);
    CHECK_EQ(mul_div_round(-5, 3, 10), -2);
    CHECK_EQ(mul_div_round(4310200000, 3, 7), 1847228571); // 12930600000 / 7 = 1847228571.43
    CHECK_EQ(mul_div_round(4448118450000, 1000003, 1000000), 4448131794355);

    // rotate_udeg: quarter turns are integer swaps; two 30° turns land within a
    // millimetre of one 60° turn.
    const Point2 o{100000, 100000};
    CHECK_EQ(rotate_udeg(Point2{110000, 100000}, o, 90 * kUDegPerDegree), (Point2{100000, 110000}));
    CHECK_EQ(rotate_udeg(Point2{110000, 100000}, o, 180 * kUDegPerDegree), (Point2{90000, 100000}));
    CHECK_EQ(rotate_udeg(Point2{110000, 100000}, o, -90 * kUDegPerDegree), (Point2{100000, 90000}));
    const Point2 twice = rotate_udeg(rotate_udeg(Point2{110000, 100000}, o, 30 * kUDegPerDegree), o,
                                     30 * kUDegPerDegree);
    const Point2 once  = rotate_udeg(Point2{110000, 100000}, o, 60 * kUDegPerDegree);
    CHECK(std::abs(twice.x - once.x) <= 1);
    CHECK(std::abs(twice.y - once.y) <= 1);
    CHECK(std::abs(once.x - 105000) <= 1);
    CHECK(std::abs(once.y - 108660) <= 1);

    // A half-turn segment is half the disc.
    CHECK(std::abs(circular_segment_area(10000, 180 * kUDegPerDegree) - 157079633) <= 1);
    // A 90° segment: r²(π/2 − 1)/2 = 28 539 816.
    CHECK(std::abs(circular_segment_area(10000, 90 * kUDegPerDegree) - 28539816) <= 1);
}

// ---------------------------------------------------------------- grips ----

#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/grips.hpp"

TEST_CASE("TUTAMAK: yaylı çizginin köşesi yayın şişkinliğini korur, yay ortası yayı üç noktadan "
          "kurar, önizleme türün çizimidir")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
    ArcPolyline ap;
    ap.arcs.push_back(ArcPolyline::Arc{1, Point2{10000, 5000}, 5000, true});
    const auto made = doc.add_kind(
        lyr, kArcPolylineKind, std::vector{RingGeometry::RingInput{square, RingRole::Exterior, 0}},
        encode_arc_polyline(ap), undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    // Four corners and the midpoint of the one bend, on its far side.
    const auto grips = entity_grips(doc, e);
    REQUIRE_EQ(grips.size(), 5u);
    CHECK_EQ(grips[4].role, GripRole::ArcMid);
    CHECK_EQ(grips[4].at, (Point2{15000, 5000}));

    // The corner at the bend's end moves 10 m north: the half circle stays a
    // half circle (bulge 1) over the new 20 m chord.
    auto moved = move_grip(doc, e, 2, Point2{10000, 20000});
    REQUIRE(moved.ok());
    auto def = decode_arc_polyline(moved.value().payload);
    REQUIRE(def.ok());
    REQUIRE_EQ(def.value().arcs.size(), 1u);
    CHECK_EQ(def.value().arcs[0].centre, (Point2{10000, 10000}));
    CHECK_EQ(def.value().arcs[0].radius, Mm{10000});
    CHECK(def.value().arcs[0].ccw);
    CHECK_EQ(moved.value().points[0][2], (Point2{10000, 20000}));

    // The midpoint pulled in to 2 m past the chord: the circle through the two
    // ends and the handle, centre on the chord's bisector.
    auto bent = move_grip(doc, e, 4, Point2{12000, 5000});
    REQUIRE(bent.ok());
    def = decode_arc_polyline(bent.value().payload);
    REQUIRE(def.ok());
    REQUIRE_EQ(def.value().arcs.size(), 1u);
    CHECK_EQ(def.value().arcs[0].centre, (Point2{4750, 5000}));
    CHECK_EQ(def.value().arcs[0].radius, Mm{7250});
    CHECK(def.value().arcs[0].ccw);

    // On the chord the bend is gone: a straight edge.
    auto straight = move_grip(doc, e, 4, Point2{10000, 5000});
    REQUIRE(straight.ok());
    def = decode_arc_polyline(straight.value().payload);
    REQUIRE(def.ok());
    CHECK(def.value().arcs.empty());

    // The preview is the edited shape drawn by the kind: one closed run that
    // reaches the pulled-out bend.
    EmitBuffer preview;
    REQUIRE(grip_preview(doc, e, 2, Point2{10000, 20000}, preview));
    REQUIRE_EQ(preview.run_total(), 1u);
    CHECK(preview.run_closed[0] != 0);
    Mm far_x = 0;
    for (const Mm x : preview.xs)
        far_x = std::max(far_x, x);
    CHECK(std::abs(far_x - 20000) <= 2);

    // A circle's quadrant preview is still a circle, of the new radius.
    const auto circle = doc.add_circle(lyr, Point2{50000, 50000}, 5000, undo);
    REQUIRE(circle.ok());
    EmitBuffer round;
    REQUIRE(grip_preview(doc, circle.value(), 1, Point2{62000, 50000}, round));
    REQUIRE_EQ(round.run_total(), 1u);
    Mm east = 0;
    for (const Mm x : round.xs)
        east = std::max(east, x);
    CHECK_EQ(east, Mm{62000});
    CHECK(entity_grips(doc, circle.value()).size() == 5u);

    // A grip that does not exist is refused by name.
    CHECK_FALSE(move_grip(doc, e, 9, Point2{}).ok());
}

// ------------------------------------------------------ Phase 2 kinds (C1–C5) --

#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/spline.hpp"

namespace {

RingGeometry::RingInput face_ring(const std::vector<Point2>& pts)
{
    return RingGeometry::RingInput{pts, RingRole::Exterior, 0};
}

} // namespace

TEST_CASE("YAYLIÇİZGİ: alanı tam, çevresi r·θ, kutusu yaya kadar, yükü bayt bayt geri gelir")
{
    // A 10 m square whose east edge is a half circle of radius 5 m, bulging
    // outward (counter-clockwise from (10,0) to (10,10) about (10,5)).
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
    ArcPolyline ap;
    ap.arcs.push_back(ArcPolyline::Arc{1, Point2{10000, 5000}, 5000, true});
    const auto payload = encode_arc_polyline(ap);

    // The payload survives its own encoding.
    const auto back = decode_arc_polyline(payload);
    REQUIRE(back.ok());
    CHECK(back.value() == ap);

    const auto made =
        doc.add_kind(lyr, kArcPolylineKind, std::vector{face_ring(square)}, payload, undo);
    REQUIRE(made.ok());
    const EntityId e = made.value();

    // EXACT: 100 m² plus the half disc, r²(π − sin π)/2 = 39 269 908 mm².
    CHECK(std::abs(doc.entity_area(e) - Mm2{139'269'908}) <= 1);
    // Three straight edges and a half circle: 30 m + 5π m.
    CHECK(std::abs(doc.entity_perimeter(e) - Mm{45708}) <= 1);
    // The box reaches the arc's far side, 5 m past the vertices.
    CHECK_EQ(doc.entities().box_of(e), (Box2{0, 0, 15000, 10000}));

    // The same figure drawn clockwise on the same edge bulges INTO the square.
    ArcPolyline inward;
    inward.arcs.push_back(ArcPolyline::Arc{1, Point2{10000, 5000}, 5000, false});
    const auto dented = doc.add_kind(lyr, kArcPolylineKind, std::vector{face_ring(square)},
                                     encode_arc_polyline(inward), undo);
    REQUIRE(dented.ok());
    CHECK(std::abs(doc.entity_area(dented.value()) - Mm2{60'730'092}) <= 1);
    CHECK_EQ(doc.entities().box_of(dented.value()), (Box2{0, 0, 10000, 10000}));

    // Refused: an edge the ring does not have, a centre not on the bisector.
    ArcPolyline wrong_edge;
    wrong_edge.arcs.push_back(ArcPolyline::Arc{7, Point2{10000, 5000}, 5000, true});
    CHECK_FALSE(doc.add_kind(lyr, kArcPolylineKind, std::vector{face_ring(square)},
                             encode_arc_polyline(wrong_edge), undo)
                    .ok());
    ArcPolyline wrong_centre;
    wrong_centre.arcs.push_back(ArcPolyline::Arc{1, Point2{12000, 5000}, 5000, true});
    CHECK_FALSE(doc.add_kind(lyr, kArcPolylineKind, std::vector{face_ring(square)},
                             encode_arc_polyline(wrong_centre), undo)
                    .ok());

    // The arc's own points: its centre as MERKEZ, its far point as ORTA.
    std::vector<Point2> pts;
    std::vector<std::uint32_t> modes;
    KeyPointSink sink{pts, modes};
    builtin_kinds()
        .find(kArcPolylineKind)
        ->key_points(doc.geometry(), doc.entities().slot[e], sink);
    bool centre = false, far = false;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (pts[i] == Point2{10000, 5000} && modes[i] == (1u << 2)) centre = true;
        if (pts[i] == Point2{15000, 5000} && modes[i] == (1u << 1)) far = true;
    }
    CHECK(centre);
    CHECK(far);

    // Bulge in, bulge out: what DXF writes comes back to within rounding.
    Point2 c{};
    Mm r     = 0;
    bool ccw = false;
    REQUIRE(arc_from_bulge(Point2{10000, 0}, Point2{10000, 10000}, 1.0, c, r, ccw));
    CHECK_EQ(c, (Point2{10000, 5000}));
    CHECK_EQ(r, 5000);
    CHECK(ccw);
    CHECK(std::abs(bulge_from_arc(Point2{10000, 0}, Point2{10000, 10000}, c, r, true) - 1.0) <
          1e-9);
}

TEST_CASE("SPLINE: simetrik kontrol çokgeninin orta parametresi tam ortada, TM'ye ötelense de aynı")
{
    // Four control points, degree 3, one span: a Bézier. Its point at u = 1/2 is
    // (15, 15) exactly, and the 16-per-span sampling lands on u = 1/2.
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> ctrl{{0, 0}, {10000, 20000}, {20000, 20000}, {30000, 0}};
    SplineDef def;
    def.degree     = 3;
    def.knots_nano = uniform_clamped_knots(4, 3);
    REQUIRE_EQ(def.knots_nano.size(), 8u);
    const auto payload = encode_spline(def);
    const auto back    = decode_spline(payload);
    REQUIRE(back.ok());
    CHECK(back.value() == def);

    const auto made = doc.add_kind(lyr, kSplineKind, std::vector{open_ring(ctrl)}, payload, undo);
    REQUIRE(made.ok());
    std::vector<Mm> xs, ys;
    CHECK_FALSE(spline_outline(doc.geometry(), doc.entities().slot[made.value()], xs, ys));
    REQUIRE_EQ(xs.size(), 17u);
    CHECK_EQ(xs.front(), 0);
    CHECK_EQ(ys.front(), 0);
    CHECK_EQ(xs.back(), 30000);
    CHECK_EQ(xs[8], 15000);
    CHECK_EQ(ys[8], 15000);

    // Translated to a TUREF coordinate the differences are the same millimetres:
    // the frame is the first control point (core.md R3).
    const Point2 far{485300000, 4310200000};
    std::vector<Point2> moved;
    for (const Point2& p : ctrl)
        moved.push_back(Point2{p.x + far.x, p.y + far.y});
    const auto far_made =
        doc.add_kind(lyr, kSplineKind, std::vector{open_ring(moved)}, payload, undo);
    REQUIRE(far_made.ok());
    std::vector<Mm> fx, fy;
    spline_outline(doc.geometry(), doc.entities().slot[far_made.value()], fx, fy);
    REQUIRE_EQ(fx.size(), xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i) {
        CHECK_EQ(fx[i] - far.x, xs[i]);
        CHECK_EQ(fy[i] - far.y, ys[i]);
    }

    // Refused: a knot vector that does not fit, a weight of zero, too few points.
    SplineDef bad_knots = def;
    bad_knots.knots_nano.pop_back();
    CHECK_FALSE(
        doc.add_kind(lyr, kSplineKind, std::vector{open_ring(ctrl)}, encode_spline(bad_knots), undo)
            .ok());
    SplineDef bad_weight = def;
    bad_weight.weights_nano.assign(4, kNano);
    bad_weight.weights_nano[2] = 0;
    CHECK_FALSE(decode_spline(encode_spline(bad_weight)).ok());
    const std::vector<Point2> two{{0, 0}, {1000, 0}};
    SplineDef plain;
    CHECK_FALSE(
        doc.add_kind(lyr, kSplineKind, std::vector{open_ring(two)}, encode_spline(plain), undo)
            .ok());

    // Closed: the run closes and the area is what the drawn form encloses.
    SplineDef closed = def;
    closed.closed    = true;
    const auto loop =
        doc.add_kind(lyr, kSplineKind, std::vector{open_ring(ctrl)}, encode_spline(closed), undo);
    REQUIRE(loop.ok());
    CHECK(doc.entity_area(loop.value()) > 0);
}

TEST_CASE("TARAMA: yükü bayt bayt geri gelir, alanı sınırınki, aile aralığı ölçekle iner")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    HatchDef def;
    def.solid      = false;
    def.name       = "ANSI31";
    def.angle_udeg = 15'000'000;
    def.scale      = Ratio{1000, 1};
    def.origin     = Point2{100, 200};
    HatchDef::Family fam;
    fam.angle_udeg  = 45'000'000;
    fam.offset_y_um = 3175;
    fam.dashes_um   = {6350, -3175};
    def.families.push_back(fam);
    const auto payload = encode_hatch(def);
    const auto back    = decode_hatch(payload);
    REQUIRE(back.ok());
    CHECK(back.value() == def);

    // 3,175 mm on the pattern at scale 1000 is 3,175 m on the ground.
    CHECK_EQ(hatch_family_spacing_mm(def, fam), 3175);

    const std::vector<Point2> outer{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
    const std::vector<Point2> hole{{4000, 4000}, {4000, 6000}, {6000, 6000}, {6000, 4000}};
    const std::vector<RingGeometry::RingInput> rings{
        face_ring(outer), RingGeometry::RingInput{hole, RingRole::Interior, 0}};
    const auto made = doc.add_kind(lyr, kHatchKind, rings, payload, undo);
    REQUIRE(made.ok());
    CHECK_EQ(doc.entity_area(made.value()), Mm2{96'000'000});

    // An open ring is not a boundary; junk is not a pattern.
    CHECK_FALSE(doc.add_kind(lyr, kHatchKind, std::vector{open_ring(outer)}, payload, undo).ok());
    const std::uint8_t junk[3]{1, 2, 3};
    CHECK_FALSE(
        doc.add_kind(lyr, kHatchKind, rings, std::span<const std::uint8_t>(junk, 3), undo).ok());
}

TEST_CASE("BLOKREFERANSI: 90° dönme bit-tam, ayna, iç içe, döngü reddi, dizi kutusu")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const auto kapak  = doc.add_block("KAPAK", "", Point2{0, 0});
    REQUIRE(kapak.ok());
    const std::vector<Point2> line{{0, 0}, {2000, 0}};
    REQUIRE(doc.add_kind(lyr, kPolylineKind, std::vector{open_ring(line)}, {}, undo, kapak.value())
                .ok());
    const std::vector<Point2> circle{{0, 0}, {1000, 0}};
    REQUIRE(doc.add_kind(lyr, kCircleKind, std::vector{open_ring(circle)}, {}, undo, kapak.value())
                .ok());

    const auto place = [&](Point2 at, BlockReference ref) {
        ref.bounds = block_reference_bounds(doc, at, ref);
        const std::vector<Point2> pt{at};
        return doc.add_kind(lyr, kBlockReferenceKind, std::vector{open_ring(pt)},
                            encode_block_reference(ref), undo);
    };

    // Scaled ×2 and turned a quarter: (2,0) becomes (0,4), exactly.
    BlockReference turned;
    turned.block         = kapak.value();
    turned.sx            = Ratio{2, 1};
    turned.sy            = Ratio{2, 1};
    turned.rotation_udeg = 90 * kUDegPerDegree;
    const auto t         = place(Point2{100000, 200000}, turned);
    REQUIRE(t.ok());
    EmitBuffer runs;
    REQUIRE(entity_outline(doc, t.value(), runs));
    REQUIRE_EQ(runs.run_total(), 2u);
    CHECK_EQ(runs.run_xs(0)[1], 100000);
    CHECK_EQ(runs.run_ys(0)[1], 204000);
    CHECK_EQ(runs.run_closed[1], 1); // the circle's rim
    CHECK_EQ(runs.run_style[0], kInheritRunStyle);
    const Box2 box = doc.entities().box_of(t.value());
    CHECK_EQ(box.min_x, 98000);
    CHECK_EQ(box.max_x, 102000);
    CHECK_EQ(box.max_y, 204000);

    // Mirrored in x: the line points west.
    BlockReference mirrored;
    mirrored.block = kapak.value();
    mirrored.sx    = Ratio{-1, 1};
    const auto m   = place(Point2{300000, 200000}, mirrored);
    REQUIRE(m.ok());
    runs.clear();
    REQUIRE(entity_outline(doc, m.value(), runs));
    CHECK_EQ(runs.run_xs(0)[1], 298000);

    // A 3×2 grid: three copies along x, two along y, spacing unscaled.
    BlockReference grid;
    grid.block          = kapak.value();
    grid.columns        = 3;
    grid.rows           = 2;
    grid.column_spacing = 5000;
    grid.row_spacing    = 4000;
    const auto g        = place(Point2{0, 0}, grid);
    REQUIRE(g.ok());
    const Box2 gbox = doc.entities().box_of(g.value());
    CHECK_EQ(gbox.min_x, -1000);
    CHECK_EQ(gbox.max_x, 12000); // 2·5000 + the line's 2000
    CHECK_EQ(gbox.max_y, 5000);  // 4000 + the circle's 1000

    // Nested: BACA holds a reference to KAPAK; a reference to BACA draws the line.
    const auto baca = doc.add_block("BACA", "", Point2{0, 0});
    REQUIRE(baca.ok());
    BlockReference inner;
    inner.block  = kapak.value();
    inner.bounds = block_reference_bounds(doc, Point2{0, 5000}, inner);
    const std::vector<Point2> at{{0, 5000}};
    REQUIRE(doc.add_kind(lyr, kBlockReferenceKind, std::vector{open_ring(at)},
                         encode_block_reference(inner), undo, baca.value())
                .ok());
    BlockReference outer;
    outer.block  = baca.value();
    const auto o = place(Point2{1000000, 2000000}, outer);
    REQUIRE(o.ok());
    runs.clear();
    REQUIRE(entity_outline(doc, o.value(), runs));
    REQUIRE_EQ(runs.run_total(), 2u);
    CHECK_EQ(runs.run_xs(0)[0], 1000000);
    CHECK_EQ(runs.run_ys(0)[0], 2005000);

    // A definition may not contain itself, at any depth.
    BlockReference self;
    self.block = baca.value();
    CHECK_FALSE(doc.add_kind(lyr, kBlockReferenceKind, std::vector{open_ring(at)},
                             encode_block_reference(self), undo, baca.value())
                    .ok());
    BlockReference loop;
    loop.block = baca.value();
    CHECK_FALSE(doc.add_kind(lyr, kBlockReferenceKind, std::vector{open_ring(at)},
                             encode_block_reference(loop), undo, kapak.value())
                    .ok());
    // And a reference to a block that does not exist is refused.
    BlockReference nowhere;
    nowhere.block = 99;
    CHECK_FALSE(place(Point2{0, 0}, nowhere).ok());

    // The insertion point is offered as EKLEME.
    std::vector<Point2> pts;
    std::vector<std::uint32_t> modes;
    KeyPointSink sink{pts, modes};
    builtin_kinds()
        .find(kBlockReferenceKind)
        ->key_points(doc.geometry(), doc.entities().slot[t.value()], sink);
    REQUIRE_EQ(pts.size(), 1u);
    CHECK_EQ(pts[0], (Point2{100000, 200000}));
    CHECK_EQ(modes[0], 1u << 17);
}

TEST_CASE("ÖLÇÜ: metin tam sayı aritmetiğiyle, döndürülmüş uzatmalar bit-tam, açı atan2'den")
{
    CHECK_EQ(format_dimension_length(12500, DrawingUnit::Metre, 2, ','), "12,50");
    CHECK_EQ(format_dimension_length(12500, DrawingUnit::Metre, 0, ','), "13");
    CHECK_EQ(format_dimension_length(12500, DrawingUnit::Centimetre, 1, '.'), "1250.0");
    CHECK_EQ(format_dimension_length(-125, DrawingUnit::Millimetre, 3, ','), "-125,000");
    CHECK_EQ(format_dimension_angle(90 * kUDegPerDegree, 2, ','), "90,00°");
    CHECK_EQ(format_dimension_angle(45'500'000, 1, '.'), "45.5°");

    const std::vector<Point2> aligned{{0, 0}, {12500, 0}, {0, 3000}};
    CHECK_EQ(dimension_measure(DimensionType::Aligned, aligned, 0), 12500);
    const std::vector<Point2> linear{{0, 0}, {3000, 7000}, {0, 0}};
    CHECK_EQ(dimension_measure(DimensionType::Linear, linear, 90 * kUDegPerDegree), 7000);
    CHECK_EQ(dimension_measure(DimensionType::Linear, linear, 0), 3000);
    const std::vector<Point2> angle{{0, 0}, {10000, 0}, {0, 10000}, {7071, 7071}};
    CHECK_EQ(dimension_measure(DimensionType::Angular3P, angle, 0), 90 * kUDegPerDegree);
    // The arc point on the other side measures the other way round.
    const std::vector<Point2> reflex{{0, 0}, {10000, 0}, {0, 10000}, {-7071, -7071}};
    CHECK_EQ(dimension_measure(DimensionType::Angular3P, reflex, 0), 270 * kUDegPerDegree);

    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    DimensionDef def;
    def.type           = DimensionType::Aligned;
    def.measurement    = 12500;
    def.arrow_size     = 2500;
    const auto payload = encode_dimension(def);
    const auto back    = decode_dimension(payload);
    REQUIRE(back.ok());
    CHECK(back.value() == def);
    CHECK_EQ(dimension_text(def, DrawingUnit::Metre), "12,50");

    const std::vector<Point2> baseline{{6250, 3800}, {10000, 3800}};
    const std::vector<RingGeometry::RingInput> rings{open_ring(baseline), open_ring(aligned)};
    const auto made = doc.add_kind(lyr, kDimensionKind, rings, payload, undo);
    REQUIRE(made.ok());
    EmitBuffer runs;
    REQUIRE(entity_outline(doc, made.value(), runs));
    // Two extension lines, the dimension line, two closed arrowheads.
    REQUIRE_EQ(runs.run_total(), 5u);
    // The extension lines are exactly vertical: the points are on the x axis and
    // the dimension line is 3 m due north of it.
    CHECK_EQ(runs.run_xs(0)[0], runs.run_xs(0)[1]);
    CHECK_EQ(runs.run_xs(1)[0], runs.run_xs(1)[1]);
    CHECK_EQ(runs.run_ys(2)[0], 3000);
    CHECK_EQ(runs.run_ys(2)[1], 3000);
    CHECK_EQ(runs.run_closed[3], 1);
    // The box takes in the arrows and the baseline; the area is nothing.
    CHECK(doc.entities().box_of(made.value()).max_y >= 4250);
    CHECK_EQ(doc.entity_area(made.value()), 0);

    // Refused: a type that wants three points given two.
    const std::vector<Point2> two{{0, 0}, {12500, 0}};
    CHECK_FALSE(doc.add_kind(lyr, kDimensionKind, std::vector{open_ring(baseline), open_ring(two)},
                             payload, undo)
                    .ok());

    // A leader: its vertices and a closed arrowhead at the first.
    LeaderDef ld;
    ld.arrow_size = 1000;
    const std::vector<Point2> path{{0, 0}, {3000, 3000}, {6000, 3000}};
    const auto lead =
        doc.add_kind(lyr, kLeaderKind, std::vector{open_ring(path)}, encode_leader(ld), undo);
    REQUIRE(lead.ok());
    runs.clear();
    REQUIRE(entity_outline(doc, lead.value(), runs));
    REQUIRE_EQ(runs.run_total(), 2u);
    CHECK_EQ(runs.run_closed[1], 1);
    CHECK_EQ(runs.run_xs(1)[0], 0); // the tip is the first vertex
    CHECK_EQ(doc.entity_perimeter(lead.value()), 7243);
}

TEST_CASE("ELİPS: kısmi elipsin yükü sweep'i taşır, çizimi açık, kutusu yayın")
{
    Document doc;
    Op undo;
    const LayerId lyr = doc.ensure_layer("TEST");
    const std::vector<Point2> def{{0, 0}, {10000, 0}, {0, 5000}};
    const EllipseArc arc{0, 90 * kUDegPerDegree};
    const auto payload = encode_ellipse_arc(arc);
    REQUIRE_EQ(payload.size(), 24u);
    const auto back = decode_ellipse_arc(payload);
    REQUIRE(back.ok());
    CHECK(back.value() == arc);

    const auto made = doc.add_kind(lyr, kEllipseKind, std::vector{open_ring(def)}, payload, undo);
    REQUIRE(made.ok());
    // The quarter from the first axis to the second: from (10,0) to (0,5).
    CHECK_EQ(doc.entities().box_of(made.value()), (Box2{0, 0, 10000, 5000}));
    CHECK_EQ(doc.entity_area(made.value()), 0);
    EmitBuffer runs;
    REQUIRE(entity_outline(doc, made.value(), runs));
    REQUIRE_EQ(runs.run_total(), 1u);
    CHECK_EQ(runs.run_closed[0], 0);
    CHECK_EQ(runs.run_xs(0).front(), 10000);
    CHECK_EQ(runs.run_ys(0).back(), 5000);

    // Equal angles are no sweep; a whole ellipse carries no payload.
    CHECK_FALSE(decode_ellipse_arc(encode_ellipse_arc(EllipseArc{5, 5})).ok());
    const std::uint8_t junk[5]{1, 2, 3, 4, 5};
    CHECK_FALSE(doc.add_kind(lyr, kEllipseKind, std::vector{open_ring(def)},
                             std::span<const std::uint8_t>(junk, 5), undo)
                    .ok());
}
