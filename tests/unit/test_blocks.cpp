// SPDX-License-Identifier: GPL-3.0-or-later
//
// A BLOCK TAKEN APART LOOKS AS IT DID, AND A BLOCK LEFT WHOLE IS REACHED WHERE
// IT IS DRAWN (TODOS C-13).
//
// PATLAT once refused every block holding a circle, an arc or a caption, and
// put what it did take out on the reference's layer as plain lines. A reference
// offered its insertion point to the snap and not one corner of what it drew,
// and its member captions kept their size when the symbol was inserted twice as
// big. Each case below is one of those, held down by the arithmetic that draws
// the reference: `core::place_block_point`.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/render/scene.hpp"

#include <cmath>
#include <cstdlib>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::Point2;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string said;
    core::Json report;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
        report = r.value().report;
    }

    /// The refusal a line meets; the empty string when it ran.
    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        return r.ok() ? std::string() : r.error().message;
    }

    core::EntityId slot(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    std::int64_t key(core::EntityId e) const
    {
        return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
    }

    std::vector<Point2> ring(core::EntityId e, std::size_t which = 0) const
    {
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
        const auto xs = doc.geometry().ring_xs(span.first + static_cast<std::uint32_t>(which));
        const auto ys = doc.geometry().ring_ys(span.first + static_cast<std::uint32_t>(which));
        std::vector<Point2> out;
        for (std::size_t v = 0; v < xs.size(); ++v)
            out.push_back(Point2{xs[v], ys[v]});
        return out;
    }

    /// The live objects outside every block definition, by key ascending.
    std::vector<std::int64_t> standalone() const
    {
        std::vector<std::int64_t> out;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && (doc.entities().flags[e] & core::FlagInBlock) == 0)
                out.push_back(key(e));
        return out;
    }

    /// The newest live block reference outside a definition.
    std::int64_t last_reference() const
    {
        std::int64_t found = 0;
        for (const std::int64_t k : standalone())
            if (doc.entities().kind[slot(k)] == core::kBlockReferenceKind) found = k;
        return found;
    }

    /// The pieces the last PATLAT reported for its first object, in order made.
    std::vector<core::EntityId> pieces() const
    {
        std::vector<core::EntityId> out;
        const core::Json* rows = report.find("nesneler");
        if (rows == nullptr || rows->as_array().empty()) return out;
        const core::Json* made = rows->as_array().front().find("parcalar");
        if (made == nullptr) return out;
        for (const core::Json& k : made->as_array())
            out.push_back(slot(k.as_int()));
        return out;
    }

    /// Member `i` of the definition `ref_key` places.
    core::EntityId member(std::int64_t ref_key, std::size_t i) const
    {
        const auto ref =
            core::block_reference_of(doc.geometry(), doc.entities().slot[slot(ref_key)]);
        return doc.slot_of(doc.blocks().at(ref.value().block).members.at(i));
    }

    /// `p`, a point of the definition, where reference `ref_key` draws it.
    Point2 placed(std::int64_t ref_key, Point2 p, int column = 0, int row = 0) const
    {
        const std::uint32_t gs = doc.entities().slot[slot(ref_key)];
        const auto ref         = core::block_reference_of(doc.geometry(), gs).value();
        return core::place_block_point(ref, core::block_reference_insertion(doc.geometry(), gs),
                                       doc.blocks().at(ref.block).base, p, column, row);
    }

    core::SnapResult snap(Point2 aim, std::uint32_t modes) const
    {
        core::SnapQuery q;
        q.aim    = aim;
        q.radius = 250;
        q.modes  = modes;
        return core::snap(doc, q);
    }
};

std::vector<Point2> placed_ring(const Rig& r, std::int64_t ref_key, core::EntityId member,
                                int column = 0, int row = 0)
{
    std::vector<Point2> out;
    for (const Point2 p : r.ring(member))
        out.push_back(r.placed(ref_key, p, column, row));
    return out;
}

double gap(Point2 a, Point2 b)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

TEST_CASE("C-13 PATLAT: her üye kendi türünde, referansın çizdiği milimetrede çıkar")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 3,0 3,1");                   // 1
    r.run("ALAN 4,0 6,0 6,2 4,2");                     // 2
    r.run("DAİRE merkez=1,3 cevre=2,3");               // 3
    r.run("YAY merkez=5,4 baslangic=6,4 bitis=5,5");   // 4
    r.run("METİN noktalar=0,6 yazi=K1 yukseklik=500"); // 5
    r.run("NOKTA 2,7");                                // 6
    r.run("ELİPS 5,7 7,7 5,7.5");                      // 7
    r.run("BLOK ad=KAPAK taban=0,0 nesneler=1 nesneler=2 nesneler=3 nesneler=4 nesneler=5 "
          "nesneler=6 nesneler=7");
    r.run("BLOKEKLE ad=KAPAK nokta=100,200 olcek=2 aci=30");
    const std::int64_t ref = r.last_reference();
    REQUIRE(ref != 0);

    // Before: the snap reaches what the reference DRAWS, not only where it
    // stands — a corner, a middle, a centre, a quadrant, a centroid, a node.
    const Point2 corner = r.placed(ref, {3'000, 1'000});
    const Point2 a      = r.placed(ref, {0, 0});
    const Point2 b      = r.placed(ref, {3'000, 0});
    const Point2 middle{(a.x + b.x) / 2, (a.y + b.y) / 2};
    const Point2 centre = r.placed(ref, {1'000, 3'000});
    const Point2 top{centre.x, centre.y + 2'000};
    const Point2 node = r.placed(ref, {2'000, 7'000});

    const struct
    {
        Point2 aim;
        std::uint32_t mode;
    } asks[] = {{corner, core::SnapEndpoint}, {middle, core::SnapMidpoint},
                {centre, core::SnapCenter},   {top, core::SnapQuadrant},
                {node, core::SnapNode},       {r.placed(ref, {5'000, 1'000}), core::SnapCentroid}};

    std::vector<core::SnapResult> before;
    for (const auto& ask : asks) {
        const core::SnapResult hit = r.snap(ask.aim, ask.mode);
        CHECK_EQ(hit.mode, ask.mode);
        CHECK_EQ(hit.entity, r.slot(ref));
        before.push_back(hit);
    }
    CHECK_EQ(before[0].point, corner);
    CHECK_EQ(before[1].point, middle);
    CHECK_EQ(before[2].point, centre);
    CHECK_EQ(before[3].point, top);
    CHECK_EQ(before[4].point, node);
    CHECK_LE(gap(before[5].point, r.placed(ref, {5'000, 1'000})), 2.0);

    // Taken apart.
    const std::vector<core::EntityId> members{r.member(ref, 0), r.member(ref, 1), r.member(ref, 2),
                                              r.member(ref, 3), r.member(ref, 4), r.member(ref, 5),
                                              r.member(ref, 6)};
    const std::size_t alive = r.doc.live_entity_count();
    r.said.clear();
    r.run("PATLAT nesne=" + std::to_string(ref));
    CHECK_EQ(r.doc.live_entity_count(), alive + 7 - 1);
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{7});
    const core::EntityTable& ents = r.doc.entities();
    CHECK_EQ(ents.kind[made[0]], core::kPolylineKind);
    CHECK_EQ(ents.kind[made[1]], core::kPolylineKind);
    CHECK_EQ(ents.kind[made[2]], core::kCircleKind);
    CHECK_EQ(ents.kind[made[3]], core::kArcKind);
    CHECK_EQ(ents.kind[made[4]], core::kPolylineKind);
    CHECK_EQ(ents.kind[made[5]], core::kPointKind);
    CHECK_EQ(ents.kind[made[6]], core::kEllipseKind);
    for (const core::EntityId e : made)
        CHECK((ents.flags[e] & core::FlagInBlock) == 0);

    // Each where the reference drew it, to the millimetre.
    CHECK_EQ(r.ring(made[0]), placed_ring(r, ref, members[0]));
    CHECK_EQ(r.ring(made[1]), placed_ring(r, ref, members[1]));
    const core::RingGeometry& g = r.doc.geometry();
    CHECK_EQ(core::circle_centre_of(g, ents.slot[made[2]]), centre);
    CHECK_EQ(core::circle_radius_of(g, ents.slot[made[2]]), 2'000);
    CHECK_EQ(core::arc_centre_of(g, ents.slot[made[3]]), r.placed(ref, {5'000, 4'000}));
    CHECK_EQ(core::arc_radius_of(g, ents.slot[made[3]]), 2'000);
    CHECK_EQ(core::arc_start_of(g, ents.slot[made[3]]), r.placed(ref, {6'000, 4'000}));
    CHECK_EQ(core::arc_end_of(g, ents.slot[made[3]]), r.placed(ref, {5'000, 5'000}));
    CHECK_EQ(r.ring(made[4]), placed_ring(r, ref, members[4]));
    CHECK_EQ(std::string(r.doc.texts().text(ents.slot[made[4]])), "K1");
    CHECK_EQ(r.doc.texts().height(ents.slot[made[4]]), 1'000); ///< twice the size, as drawn
    CHECK_EQ(r.ring(made[5]).front(), node);
    CHECK_EQ(core::ellipse_centre_of(g, ents.slot[made[6]]), r.placed(ref, {5'000, 7'000}));
    CHECK_EQ(core::ellipse_major_of(g, ents.slot[made[6]]), r.placed(ref, {7'000, 7'000}));

    // After: the same points, now on the pieces.
    for (std::size_t i = 0; i < std::size(asks); ++i) {
        const core::SnapResult hit = r.snap(asks[i].aim, asks[i].mode);
        CHECK_EQ(hit.mode, asks[i].mode);
        CHECK_EQ(hit.point, before[i].point);
        CHECK_NE(hit.entity, r.slot(ref));
    }

    // Said and reported: what it came apart into.
    CHECK(r.said.find("'KAPAK'") != std::string::npos);
    CHECK(r.said.find("1 daire") != std::string::npos);
    CHECK(r.said.find("1 yay") != std::string::npos);
    const core::Json& row = r.report.find("nesneler")->as_array().front();
    CHECK_EQ(row.find("blok")->as_string(), "KAPAK");
    CHECK_EQ(row.find("turler")->find("çizgi")->as_int(), 3);
    CHECK_EQ(row.find("turler")->find("elips")->as_int(), 1);
    CHECK_EQ(r.report.find("parca")->as_int(), 7);

    // One command, one step.
    r.run("GERİAL");
    CHECK_EQ(r.doc.live_entity_count(), alive);
    CHECK(r.doc.alive(r.slot(ref)));
}

TEST_CASE("C-13 PATLAT: aynalı dizi — 0 katmanı ve ByBlock referanstan, kendi katmanı ve rengi "
          "üyeden")
{
    Rig r;
    REQUIRE(r.doc.find_layer("0") != core::kNoLayer);
    r.run("KATMAN ad=IC");
    r.run("KATMAN ad=0");
    r.run("ÇİZGİ 0,0 1,0"); // 1: on 0, ByLayer
    r.run("ÇİZGİ 0,1 1,1"); // 2: on 0, ByBlock
    r.run("KATMAN ad=IC");
    r.run("ÇİZGİ 0,2 1,2"); // 3: on IC, red
    r.run("RENK nesneler=3 renk=kırmızı");
    r.run("ÇİZGİ 0,3 2,3"); // 4: on IC, ByBlock
    // No command writes ByBlock — a DXF does (`dxf_reader.cpp`) — so the test
    // sets it the way the reader would, before the objects become members.
    core::Appearance by_block;
    by_block.src_colour    = core::Source::ByBlock;
    const core::StyleId bb = r.doc.intern_style(by_block);
    core::Op undo;
    REQUIRE(r.doc.set_entity_style(r.slot(2), bb, undo).ok());
    REQUIRE(r.doc.set_entity_style(r.slot(4), bb, undo).ok());
    const core::StyleId red = r.doc.entities().style[r.slot(3)];
    REQUIRE(red != core::kByLayerStyle);

    r.run("BLOK ad=SEMBOL taban=0,0 nesneler=1 nesneler=2 nesneler=3 nesneler=4");
    r.run("KATMAN ad=DIS");
    r.run("BLOKEKLE ad=SEMBOL nokta=50,50 olcek=-1 olcek_y=1 sutun=2 satir=2 sutun_aralik=5000 "
          "satir_aralik=7000");
    const std::int64_t ref = r.last_reference();
    r.run("RENK nesneler=" + std::to_string(ref) + " renk=mavi");
    const core::StyleId blue = r.doc.entities().style[r.slot(ref)];
    const core::LayerId dis  = r.doc.find_layer("DIS");
    const core::LayerId ic   = r.doc.find_layer("IC");
    std::vector<core::EntityId> members;
    for (std::size_t i = 0; i < 4; ++i)
        members.push_back(r.member(ref, i));

    r.run("PATLAT nesne=" + std::to_string(ref));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{16});
    const core::EntityTable& ents = r.doc.entities();
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 2; ++col)
            for (std::size_t m = 0; m < 4; ++m) {
                const core::EntityId piece =
                    made[(static_cast<std::size_t>(row) * 2 + static_cast<std::size_t>(col)) * 4 +
                         m];
                CAPTURE(row);
                CAPTURE(col);
                CAPTURE(m);
                // Mirrored across and stepped along the grid, as drawn.
                CHECK_EQ(r.ring(piece), placed_ring(r, ref, members[m], col, row));
                CHECK_EQ(ents.layer[piece], m < 2 ? dis : ic);
                const core::StyleId want = m == 2 ? red : (m == 3 ? core::kByLayerStyle : blue);
                CHECK_EQ(ents.style[piece], want);
            }
    // The mirror puts the first member's east end WEST of the insertion point.
    CHECK_EQ(r.ring(made[0])[1], (Point2{49'000, 50'000}));
    const core::Json& row = r.report.find("nesneler")->as_array().front();
    CHECK_EQ(row.find("kopya")->as_int(), 4);
    CHECK_EQ(row.find("katman_devri")->as_int(), 8);
    CHECK_EQ(row.find("gorunus_devri")->as_int(), 12);
}

TEST_CASE("C-13 PATLAT: iç içe blok bir kat açılır; eşit olmayan ölçekte dönük iç blok adıyla "
          "reddedilir")
{
    Rig r;
    r.run("ÇİZGİ 0,0 2,0");                   // 1
    r.run("BLOK ad=IC taban=0,0 nesneler=1"); // the reference it leaves
    const std::int64_t inner = r.last_reference();
    r.run("DÖNDÜR nesneler=" + std::to_string(inner) + " merkez=0,0 aci=90");
    r.run("DAİRE merkez=5,0 cevre=6,0");
    const std::int64_t circle = r.standalone().back();
    r.run("BLOK ad=DIS taban=0,0 nesneler=" + std::to_string(inner) +
          " nesneler=" + std::to_string(circle));
    r.run("BLOKEKLE ad=DIS nokta=100,100 olcek=2 aci=45");
    const std::int64_t outer = r.last_reference();

    // The inner line's far end, through both placements: (0,2) inside DIS,
    // turned 45° and doubled about (100,100).
    const std::int64_t nested = r.key(r.member(outer, 0));
    const core::EntityId line = r.member(nested, 0);
    const Point2 inside       = r.placed(nested, r.ring(line)[1]);
    CHECK_EQ(inside, (Point2{0, 2'000})); ///< the quarter turn inside DIS, exact
    const Point2 drawn         = r.placed(outer, inside);
    const core::SnapResult end = r.snap(drawn, core::SnapEndpoint);
    CHECK_EQ(end.point, drawn);
    CHECK_EQ(end.entity, r.slot(outer));
    // The inner block's insertion point is offered too.
    CHECK_EQ(r.snap(r.placed(outer, {0, 0}), core::SnapInsertion).point, r.placed(outer, {0, 0}));

    core::EmitBuffer before;
    REQUIRE(core::entity_outline(r.doc, r.slot(outer), before));
    r.run("PATLAT nesne=" + std::to_string(outer));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{2});
    REQUIRE_EQ(r.doc.entities().kind[made[0]], core::kBlockReferenceKind);
    // ONE LEVEL: the inner block is a reference again, drawing its line where
    // the outer one drew it.
    core::EmitBuffer after;
    REQUIRE(core::entity_outline(r.doc, made[0], after));
    REQUIRE_EQ(after.run_total(), std::size_t{1});
    CHECK_EQ((Point2{after.run_xs(0)[0], after.run_ys(0)[0]}),
             (Point2{before.run_xs(0)[0], before.run_ys(0)[0]}));
    CHECK_EQ((Point2{after.run_xs(0)[1], after.run_ys(0)[1]}),
             (Point2{before.run_xs(0)[1], before.run_ys(0)[1]}));
    CHECK_EQ(r.snap(drawn, core::SnapEndpoint).point, drawn);

    // A placement that differs across and up carries a block turned by a
    // quarter — its two scales trade places — and the circle beside it becomes
    // the ellipse it is drawn as.
    r.run("GERİAL");
    r.run("BLOKEKLE ad=DIS nokta=200,100 olcek=2 olcek_y=1");
    const std::int64_t stretched = r.last_reference();
    const std::size_t alive      = r.doc.live_entity_count();
    r.run("PATLAT nesne=" + std::to_string(stretched));
    CHECK_EQ(r.doc.live_entity_count(), alive + 1);
    const std::vector<core::EntityId> apart = r.pieces();
    REQUIRE_EQ(apart.size(), std::size_t{2});
    CHECK_EQ(r.doc.entities().kind[apart[1]], core::kEllipseKind);
    core::EmitBuffer leaned;
    REQUIRE(core::entity_outline(r.doc, apart[0], leaned));
    REQUIRE_EQ(leaned.run_total(), std::size_t{1});
    CHECK_EQ((Point2{leaned.run_xs(0)[0], leaned.run_ys(0)[0]}), (Point2{200'000, 100'000}));
    CHECK_EQ((Point2{leaned.run_xs(0)[1], leaned.run_ys(0)[1]}), (Point2{200'000, 102'000}));
}

TEST_CASE("C-13 PATLAT: eşit olmayan ölçekte dönük iç blok ve yaylı çoklu çizgi adıyla reddedilir")
{
    Rig r;
    r.run("ÇİZGİ 0,0 2,0");
    r.run("BLOK ad=IC taban=0,0 nesneler=1");
    const std::int64_t inner = r.last_reference();
    r.run("DÖNDÜR nesneler=" + std::to_string(inner) + " merkez=0,0 aci=30");
    r.run("BLOK ad=DIS taban=0,0 nesneler=" + std::to_string(inner));
    r.run("BLOKEKLE ad=DIS nokta=100,100 olcek=2 olcek_y=1");
    const std::int64_t outer = r.last_reference();
    const std::size_t alive  = r.doc.live_entity_count();
    const std::string no     = r.refused("PATLAT nesne=" + std::to_string(outer));
    CHECK(no.find("Blok 'DIS'") != std::string::npos);
    CHECK(no.find("döndürülmüş bir iç blok") != std::string::npos);
    CHECK_EQ(r.doc.live_entity_count(), alive); ///< rolled back whole

    // Uniform, the same turned block comes out: its turn added to the outer's.
    r.run("BLOKEKLE ad=DIS nokta=300,100 olcek=2 aci=15");
    r.run("PATLAT nesne=" + std::to_string(r.last_reference()));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{1});
    const auto turned =
        core::block_reference_of(r.doc.geometry(), r.doc.entities().slot[made[0]]).value();
    CHECK_EQ(turned.rotation_udeg, 45 * core::kUDegPerDegree);
    CHECK_EQ(turned.sx, (core::Ratio{2, 1}));
    CHECK_EQ(turned.sy, (core::Ratio{2, 1}));

    // An arc polyline under a stretch would bend its arcs into ellipses.
    Rig k;
    k.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    k.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    const std::int64_t bent = k.standalone().back();
    REQUIRE_EQ(k.doc.entities().kind[k.slot(bent)], core::kArcPolylineKind);
    k.run("BLOK ad=KIVRIM taban=0,0 nesneler=" + std::to_string(bent));
    k.run("BLOKEKLE ad=KIVRIM nokta=0,50 olcek=1 olcek_y=2");
    const std::string arcs = k.refused("PATLAT nesne=" + std::to_string(k.last_reference()));
    CHECK(arcs.find("yaylı bir çoklu çizgi") != std::string::npos);
    CHECK(arcs.find("eşit olmayan bir ölçekle") != std::string::npos);
}

TEST_CASE("C-13 PATLAT: yaylı çoklu çizgi düz kenarlarına ve gerçek yaylarına ayrılır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    r.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    const std::int64_t bent = r.standalone().back();
    const core::EntityId e  = r.slot(bent);
    REQUIRE_EQ(r.doc.entities().kind[e], core::kArcPolylineKind);
    const auto def = core::arc_polyline_of(r.doc.geometry(), r.doc.entities().slot[e]).value();
    REQUIRE_EQ(def.arcs.size(), std::size_t{2});
    const core::Mm length = r.doc.entity_perimeter(e);
    r.run("RENK nesneler=" + std::to_string(bent) + " renk=kırmızı");
    const core::StyleId red = r.doc.entities().style[e];

    r.run("PATLAT nesne=" + std::to_string(bent));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{5}); ///< three straights, two arcs
    std::size_t arcs = 0;
    core::Mm total   = 0;
    for (const core::EntityId p : made) {
        const std::uint32_t gs = r.doc.entities().slot[p];
        CHECK_EQ(r.doc.entities().style[p], red); ///< the run's look goes with its edges
        total += r.doc.entity_perimeter(p);
        if (r.doc.entities().kind[p] != core::kArcKind) continue;
        const core::ArcPolyline::Arc& was = def.arcs[arcs++];
        CHECK_EQ(core::arc_centre_of(r.doc.geometry(), gs), was.centre);
        CHECK_EQ(core::arc_radius_of(r.doc.geometry(), gs), was.radius);
    }
    CHECK_EQ(arcs, std::size_t{2});
    CHECK_LE(std::llabs(total - length), 3); ///< the same run, edge by edge
    CHECK(r.said.find("2 yay") != std::string::npos);
}

TEST_CASE("C-13 AYNALA: dizili blok referansının ikinci satırı aynanın öbür yanına düşer")
{
    // A grid is stepped in the reference's own frame, so a reflection that
    // negated the y scale and not the row step drew the second row on the side
    // it came from.
    Rig r;
    r.run("ÇİZGİ 0,0 1,0");
    r.run("BLOK ad=SIRA taban=0,0 nesneler=1");
    r.run("BLOKEKLE ad=SIRA nokta=10,10 satir=2 satir_aralik=4000");
    const std::int64_t ref = r.last_reference();
    r.run("AYNALA nesneler=" + std::to_string(ref) + " baslangic=0,0 bitis=20,0");
    core::EmitBuffer runs;
    REQUIRE(core::entity_outline(r.doc, r.slot(ref), runs));
    REQUIRE_EQ(runs.run_total(), std::size_t{2});
    // Row 0 at y = −10 m, row 1 four metres further from the axis: −14 m.
    CHECK_EQ(runs.run_ys(0)[0], -10'000);
    CHECK_EQ(runs.run_ys(1)[0], -14'000);
    CHECK_EQ(runs.run_xs(1)[1], 11'000);
}

TEST_CASE("C-13 SEÇME ve YAKALAMA: döndürülmüş, aynalanmış ve iç içe blok çizdiği yerden tutulur")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 4,0 4,2 0,2"); // three sides, open: nothing drawn at its middle
    r.run("BLOK ad=KUTU taban=0,0 nesneler=1");
    r.run("BLOKEKLE ad=KUTU nokta=50,0 aci=90 olcek=-1 olcek_y=1");
    const std::int64_t ref = r.last_reference();

    // A point on a drawn edge picks the reference; the middle of the box,
    // which it does not draw on, does not.
    const Point2 a = r.placed(ref, {4'000, 0});
    const Point2 b = r.placed(ref, {4'000, 2'000});
    const Point2 on{(a.x + b.x) / 2, (a.y + b.y) / 2};
    CHECK_EQ(core::pick_nearest(r.doc, on, 100), r.slot(ref));
    CHECK_EQ(core::pick_nearest(r.doc, r.placed(ref, {2'000, 1'000}), 100), core::kNoEntity);
    // The quarter turn and the mirror are exact: (4,2) goes to (48,−4).
    CHECK_EQ(b, (Point2{48'000, -4'000}));
    CHECK_EQ(r.snap(Point2{b.x + 60, b.y - 40}, core::SnapEndpoint).point, b);
    CHECK_EQ(r.snap(Point2{on.x + 30, on.y + 30}, core::SnapMidpoint).point, on);

    // Nested one deeper and turned again: the same corner, through both.
    r.run("BLOK ad=KAT taban=50,0 nesneler=" + std::to_string(ref));
    r.run("BLOKEKLE ad=KAT nokta=200,200 aci=30");
    const std::int64_t outer = r.last_reference();
    const Point2 deep        = r.placed(outer, b);
    CHECK_EQ(r.snap(deep, core::SnapEndpoint).point, deep);
    CHECK_EQ(core::pick_nearest(r.doc, r.placed(outer, on), 100), r.slot(outer));
}

TEST_CASE("C-13 GÖRÜNÜM: ölçekli referansta üye yazısı referansla büyür")
{
    Rig r;
    r.run("METİN noktalar=0,0 yazi=R1 yukseklik=500");
    r.run("BLOK ad=ETIKET taban=0,0 nesneler=1"); // one reference at 0,0, size 1
    r.run("BLOKEKLE ad=ETIKET nokta=20,0 olcek=2");

    render::ViewTransform view;
    view.set_viewport(1000, 800);
    view.set_centre(Point2{10'000, 0}, 50.0);
    render::DrawList list;
    render::build_scene(r.doc, view, render::SceneOptions{}, list);
    REQUIRE_EQ(list.texts.size(), std::size_t{2});
    float small = list.texts[0].height_px;
    float large = list.texts[1].height_px;
    if (list.texts[0].x0 > list.texts[1].x0) std::swap(small, large);
    CHECK(small > 0.0f);
    CHECK(std::abs(large / small - 2.0f) < 0.01f);

    // And PATLAT keeps the size it was drawn at.
    r.run("PATLAT nesne=" + std::to_string(r.last_reference()));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{1});
    CHECK_EQ(r.doc.texts().height(r.doc.entities().slot[made[0]]), 1'000);
}

TEST_CASE("C-13 KOPYA: dönüşümün ret sebebi 'kopya dönüştürülemedi' ile ezilmez")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    r.run("YUVARLA nesne=1 hepsi=evet yaricap=2");
    const std::int64_t bent = r.standalone().back();
    const std::string no    = r.refused("ÖLÇEKLE nesneler=" + std::to_string(bent) +
                                        " merkez=0,0 carpan=2 carpan_y=1 kopya=evet");
    CHECK(no.find("yaylı bir çoklu çizgi") != std::string::npos);
    CHECK(no.find("Kopya dönüştürülemedi") == std::string::npos);
}

namespace {

/// The keys BLOKDÜZENLE aç reported, joined for the next command line.
std::string opened_keys(const Rig& r)
{
    std::string out;
    const core::Json* made = r.report.find("parcalar");
    if (made == nullptr) return out;
    for (const core::Json& k : made->as_array())
        out += (out.empty() ? "" : " nesneler=") + std::to_string(k.as_int());
    return out;
}

/// A fingerprint of one block definition's live members: kind, layer, style,
/// rings and payload, in member order — what "unchanged" has to mean.
std::string definition_of(const Rig& r, const std::string& name)
{
    const core::BlockId b = r.doc.blocks().find(name);
    std::string out;
    for (const core::EntityKey k : r.doc.blocks().at(b).members) {
        const core::EntityId m = r.doc.slot_of(k);
        if (m == core::kNoEntity || !r.doc.alive(m)) continue;
        out += std::to_string(core::raw(k)) + ":" + std::to_string(r.doc.entities().kind[m]) + ":" +
               std::to_string(r.doc.entities().layer[m]) + ":" +
               std::to_string(r.doc.entities().style[m]);
        for (const Point2 p : r.ring(m))
            out += "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")";
        const auto bytes = r.doc.geometry().payload_of(r.doc.entities().slot[m]);
        out += "#" + std::to_string(bytes.size()) + ";";
    }
    return out;
}

} // namespace

TEST_CASE("C-13 BLOKDÜZENLE: açıp dokunmadan kaydetmek tanımı bayt bayt aynı bırakır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 3,0 3,1");
    r.run("DAİRE merkez=1,3 cevre=2,3");
    r.run("METİN noktalar=0,6 yazi=K1 yukseklik=500");
    r.run("BLOK ad=KAPAK taban=0,0 nesneler=1 nesneler=2 nesneler=3");
    r.run("BLOKEKLE ad=KAPAK nokta=100,200 olcek=2 aci=30");
    const std::int64_t turned = r.last_reference();
    const std::string was     = definition_of(r, "KAPAK");
    const auto sheet          = r.standalone();
    const auto payload        = [&r](std::int64_t k) {
        const auto bytes = r.doc.geometry().payload_of(r.doc.entities().slot[r.slot(k)]);
        return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
    };
    const std::vector<std::uint8_t> placed = payload(turned);

    // Opened from the turned reference: upright and full size at its point.
    r.run("BLOKDÜZENLE nesne=" + std::to_string(turned));
    CHECK((r.doc.entities().flags[r.slot(turned)] & core::FlagHidden) != 0);
    const std::string keys = opened_keys(r);
    REQUIRE_FALSE(keys.empty());
    const core::EntityId first = r.slot(std::stoll(keys.substr(0, keys.find(' '))));
    CHECK_EQ(r.ring(first).front(), (Point2{100'000, 200'000})); ///< (0,0) moved to the point
    CHECK_EQ(r.ring(first)[1], (Point2{103'000, 200'000}));      ///< and not turned
    CHECK(r.said.find("kendi yönünde ve 1:1") != std::string::npos);

    r.run("BLOKDÜZENLE islem=kaydet nesne=" + std::to_string(turned) + " nesneler=" + keys);
    CHECK_EQ(definition_of(r, "KAPAK"), was);
    CHECK_EQ(r.report.find("korunan")->as_int(), 3);
    CHECK_EQ(r.report.find("yazilan")->as_int(), 0);
    CHECK_EQ(r.report.find("cikarilan")->as_int(), 0);
    CHECK((r.doc.entities().flags[r.slot(turned)] & core::FlagHidden) == 0);
    // Nothing on the sheet changed either: the same objects, the reference's
    // record byte for byte (its box needed no refresh).
    CHECK_EQ(r.standalone(), sheet);
    CHECK(payload(turned) == placed);
    CHECK_EQ(r.report.find("kutusu_yenilenen")->as_int(), 0);
}

TEST_CASE("C-13 BLOKDÜZENLE: değişen tanım bütün referanslara ve kutularına yansır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 2,0");           // 1
    r.run("DAİRE merkez=1,1 cevre=1.5,1"); // 2
    r.run("BLOK ad=ISARET taban=0,0 nesneler=1 nesneler=2");
    const std::int64_t at_base = r.last_reference();
    r.run("BLOKEKLE ad=ISARET nokta=50,0 olcek=3");
    const std::int64_t big = r.last_reference();
    r.run("BLOK ad=GRUP taban=50,0 nesneler=" + std::to_string(big)); // ISARET inside GRUP
    const std::int64_t group = r.last_reference();
    r.run("BLOKEKLE ad=GRUP nokta=200,0 aci=90");
    const std::int64_t group_turned = r.last_reference();
    const core::Box2 before         = r.doc.entities().box_of(r.slot(group_turned));

    // Opened by name, edited: the line made longer, the circle deleted, a
    // new point drawn.
    r.run("BLOKDÜZENLE ad=ISARET");
    const std::string keys  = opened_keys(r);
    const std::int64_t line = std::stoll(keys.substr(0, keys.find(' ')));
    const std::int64_t ring = std::stoll(keys.substr(keys.rfind('=') + 1));
    r.run("KÖŞETAŞI nesne=" + std::to_string(line) + " kose=2 nokta=6,0");
    r.run("SİL nesneler=" + std::to_string(ring));
    r.run("NOKTA 3,2");
    const std::int64_t drawn = r.standalone().back();
    r.run("BLOKDÜZENLE islem=kaydet ad=ISARET nesneler=" + std::to_string(line) +
          " nesneler=" + std::to_string(drawn));
    CHECK_EQ(r.report.find("korunan")->as_int(), 0);
    CHECK_EQ(r.report.find("yazilan")->as_int(), 2);
    CHECK_EQ(r.report.find("cikarilan")->as_int(), 2);
    CHECK(r.said.find("'ISARET' bloğu kaydedildi") != std::string::npos);
    // Nothing stray left on the sheet: only the references.
    for (const std::int64_t k : r.standalone())
        CHECK(r.doc.entities().kind[r.slot(k)] == core::kBlockReferenceKind);

    // EVERY reference draws the new definition: the one at the base, the big
    // one inside GRUP, and GRUP's turned reference — the line now 6 m long.
    core::EmitBuffer runs;
    REQUIRE(core::entity_outline(r.doc, r.slot(at_base), runs));
    REQUIRE_EQ(runs.run_total(), std::size_t{2});
    CHECK_EQ(runs.run_xs(0)[1], 6'000);
    runs.clear();
    REQUIRE(core::entity_outline(r.doc, r.slot(group_turned), runs));
    REQUIRE_EQ(runs.run_total(), std::size_t{2});
    // (6,0) in ISARET → ×3 at (50,0) → (68,0) in GRUP → turned 90° about its
    // base (50,0) and set on (200,0): (200, 18).
    CHECK_EQ((Point2{runs.run_xs(0)[1], runs.run_ys(0)[1]}), (Point2{200'000, 18'000}));
    // And the boxes the index culls by followed: GRUP's grew with the line.
    const core::Box2 after = r.doc.entities().box_of(r.slot(group_turned));
    CHECK(after.max_y > before.max_y);
    CHECK_EQ(after.max_y, 18'000);
    CHECK_EQ(core::pick_nearest(r.doc, Point2{200'000, 17'000}, 100), r.slot(group_turned));

    // One step back: the old definition, everywhere.
    r.run("GERİAL");
    runs.clear();
    REQUIRE(core::entity_outline(r.doc, r.slot(at_base), runs));
    CHECK_EQ(runs.run_xs(0)[1], 2'000);
    CHECK_EQ(r.doc.entities().box_of(r.slot(group_turned)), before);
    (void)group;
}

TEST_CASE("C-13 BLOKDÜZENLE: vazgeç açılanı kaldırır; açık olmayan referans kaydedilmez; "
          "blok kendini içeremez")
{
    Rig r;
    r.run("ÇİZGİ 0,0 1,0");
    r.run("BLOK ad=TEK taban=0,0 nesneler=1");
    const std::int64_t ref  = r.last_reference();
    const std::string was   = definition_of(r, "TEK");
    const std::size_t alive = r.doc.live_entity_count();

    const std::string closed = r.refused("BLOKDÜZENLE islem=kaydet nesne=" + std::to_string(ref) +
                                         " nesneler=" + std::to_string(ref));
    CHECK(closed.find("düzenlemeye açık değil") != std::string::npos);

    r.run("SEÇ nesneler=" + std::to_string(ref));
    r.run("BLOKDÜZENLE nesne=" + std::to_string(ref));
    const std::string keys = opened_keys(r);
    CHECK_EQ(r.doc.live_entity_count(), alive + 1);
    // Hidden, and out of the selection: one SİL away from deleted unseen.
    CHECK_FALSE(
        r.bus.selection().contains(static_cast<core::EntityKey>(static_cast<std::uint64_t>(ref))));
    CHECK(r.refused("BLOKDÜZENLE nesne=" + std::to_string(ref)).find("zaten düzenlemeye açık") !=
          std::string::npos);

    // A reference to the block being edited cannot go into it.
    r.run("BLOKEKLE ad=TEK nokta=5,5");
    const std::int64_t inside = r.last_reference();
    const std::string loop    = r.refused("BLOKDÜZENLE islem=kaydet nesne=" + std::to_string(ref) +
                                          " nesneler=" + keys + " nesneler=" + std::to_string(inside));
    CHECK(loop.find("Blok 'TEK' kaydedilemedi") != std::string::npos);
    CHECK_EQ(definition_of(r, "TEK"), was);
    r.run("SİL nesneler=" + std::to_string(inside));

    r.run("BLOKDÜZENLE islem=vazgec nesne=" + std::to_string(ref) + " nesneler=" + keys);
    CHECK_EQ(r.doc.live_entity_count(), alive);
    CHECK((r.doc.entities().flags[r.slot(ref)] & core::FlagHidden) == 0);
    CHECK_EQ(definition_of(r, "TEK"), was);
    CHECK(r.said.find("vazgeçildi") != std::string::npos);
}

TEST_CASE("C-13 BLOKDÜZENLE: kılavuzdaki komut satırı örneği yazıldığı gibi çalışır")
{
    // docs/komutlar/block_edit.md, line for line: the keys it names and the
    // sentences it prints (CLAUDE.md 11.6).
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=0.6,0");
    r.run("ÇİZGİ -0.6,0 0.6,0");
    r.run("BLOK ad=KAPAK taban=0,0 nesneler=1 nesneler=2");
    CHECK_EQ(r.last_reference(), 5);
    r.said.clear();
    r.run("BLOKDÜZENLE nesne=5");
    CHECK(r.said.find("'KAPAK' bloğu düzenlemeye açıldı: 2 nesne referansın yerinde; referans "
                      "düzenleme bitene dek gizli. Bitirince BLOKDÜZENLE islem=kaydet (ya da "
                      "vazgec).") != std::string::npos);
    CHECK_EQ(r.doc.entities().kind[r.slot(6)], core::kCircleKind);
    CHECK_EQ(r.doc.entities().kind[r.slot(7)], core::kPolylineKind);
    r.run("KÖŞETAŞI nesne=7 kose=2 nokta=1,0");
    r.run("NOKTA 0,0.8");
    CHECK_EQ(r.standalone().back(), 8);
    r.said.clear();
    r.run("BLOKDÜZENLE islem=kaydet nesne=5 nesneler=6 nesneler=7 nesneler=8");
    CHECK(r.said.find("'KAPAK' bloğu kaydedildi: 1 üye olduğu gibi kaldı, 2 üye yazıldı, 1 üye "
                      "çıkarıldı. Bloğun 1 referansı yeni biçimi çiziyor.") != std::string::npos);

    // And the way back, from a second open.
    r.run("BLOKDÜZENLE nesne=5");
    const std::size_t opened = r.doc.live_entity_count();
    r.run("BLOKDÜZENLE islem=vazgec nesne=5 nesneler=" + opened_keys(r));
    CHECK_EQ(r.doc.live_entity_count(), opened - 3);
}

TEST_CASE("C-13 ÖZNİTELİK: {no} alanlı blokta her referans kendi değerini çizer; PATLAT değeri "
          "yazıya işler")
{
    Rig r;
    r.run("METİN noktalar=0,1.5 yazi={no} yukseklik=500"); // 1: the field
    r.run("DAİRE merkez=0,0 cevre=1,0");                   // 2
    r.said.clear();
    r.run("BLOK ad=NOKTA taban=0,0 nesneler=1 nesneler=2");
    CHECK(r.said.find("alanları: no") != std::string::npos);
    REQUIRE(r.doc.attributes().find("no") != core::kNoAttr); ///< its column, declared
    CHECK_EQ(core::block_fields(r.doc, r.doc.blocks().find("NOKTA")),
             (std::vector<std::string>{"no"}));

    r.said.clear();
    r.run("BLOKEKLE ad=NOKTA nokta=10,0 deger=no:K-1");
    CHECK(r.said.find("değerler: no:K-1") != std::string::npos);
    const std::int64_t first = r.last_reference();
    r.run("BLOKEKLE ad=NOKTA nokta=20,0 deger=no:K-2");
    const std::int64_t second = r.last_reference();
    const core::AttrId no     = r.doc.attributes().find("no");
    CHECK_EQ(r.doc.attribute(no, r.slot(first)).value().text, "K-1");
    CHECK_EQ(r.doc.attribute(no, r.slot(second)).value().text, "K-2");

    // DRAWN: one caption, three pictures of it — each with its reference's value.
    render::ViewTransform view;
    view.set_viewport(1200, 600);
    view.set_centre(Point2{10'000, 0}, 50.0);
    render::DrawList list;
    render::build_scene(r.doc, view, render::SceneOptions{}, list);
    std::vector<std::string> said;
    for (const render::TextItem& t : list.texts)
        said.push_back(t.text);
    CHECK(std::find(said.begin(), said.end(), "K-1") != said.end());
    CHECK(std::find(said.begin(), said.end(), "K-2") != said.end());
    CHECK(std::find(said.begin(), said.end(), "{no}") == said.end()); ///< never the placeholder

    // A name the block does not carry, and a pair without its colon, are said.
    CHECK(r.refused("BLOKEKLE ad=NOKTA nokta=30,0 deger=nox:1")
              .find("'nox' alanını taşımıyor. "
                    "Alanları: no.") != std::string::npos);
    CHECK(r.refused("BLOKEKLE ad=NOKTA nokta=30,0 deger=no").find("sutun:değer") !=
          std::string::npos);

    // TAKEN APART: the caption comes out saying what it said.
    r.said.clear();
    r.run("PATLAT nesne=" + std::to_string(second));
    const std::vector<core::EntityId> made = r.pieces();
    REQUIRE_EQ(made.size(), std::size_t{2});
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[made[0]])), "K-2");
    const core::Json& row = r.report.find("nesneler")->as_array().front();
    CHECK_EQ(row.find("yazilan_deger")->as_int(), 1);
    CHECK_EQ(row.find("birakilan_oznitelik")->as_int(), 0);
    CHECK(r.said.find("1 yazıya referansın öznitelik değeri işlendi") != std::string::npos);

    // OPENED FOR EDITING, the definition shows its field, not a value.
    r.run("BLOKDÜZENLE nesne=" + std::to_string(first));
    const std::string keys       = opened_keys(r);
    const core::EntityId caption = r.slot(std::stoll(keys.substr(0, keys.find(' '))));
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[caption])), "{no}");
}

namespace {

/// Every vertex reference `key` draws, run by run.
std::vector<Point2> drawn(const Rig& r, std::int64_t key)
{
    core::EmitBuffer runs;
    std::vector<Point2> out;
    if (!core::entity_outline(r.doc, r.slot(key), runs)) return out;
    for (std::size_t i = 0; i < runs.xs.size(); ++i)
        out.push_back(Point2{runs.xs[i], runs.ys[i]});
    return out;
}

} // namespace

TEST_CASE("C-13 TABAN: taban noktası taşınır, bütün referanslar çizildikleri yerde kalır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 2,0");
    r.run("DAİRE merkez=1,1 cevre=1.5,1");
    r.run("BLOK ad=B taban=0,0 nesneler=1 nesneler=2");
    const std::int64_t at_base = r.last_reference();
    r.run("BLOKEKLE ad=B nokta=10,0 olcek=2 aci=90");
    const std::int64_t turned = r.last_reference();
    r.run("BLOKEKLE ad=B nokta=30,0 olcek=-1 olcek_y=1");
    const std::int64_t mirrored = r.last_reference();
    r.run("BLOKEKLE ad=B nokta=50,0");
    const std::int64_t inner = r.last_reference();
    r.run("BLOK ad=G taban=50,0 nesneler=" + std::to_string(inner)); // B inside G
    const std::int64_t group = r.last_reference();
    const std::vector<std::int64_t> all{at_base, turned, mirrored, group};
    std::vector<std::vector<Point2>> before;
    for (const std::int64_t k : all)
        before.push_back(drawn(r, k));

    // Shown on the turned, doubled reference: the line's far end, (2,0) in
    // the definition, drawn at (10,4) — a quarter turn and ×2 are exact.
    const Point2 far = r.placed(turned, {2'000, 0});
    CHECK_EQ(far, (Point2{10'000, 4'000}));
    r.said.clear();
    r.run("BLOKDÜZENLE islem=taban nesne=" + std::to_string(turned) + " taban=10,4");
    const core::BlockId b = r.doc.blocks().find("B");
    CHECK_EQ(r.doc.blocks().at(b).base, (Point2{2'000, 0}));
    CHECK(r.said.find("taban noktası değişti; 4 referansı çizildiği yerde tutuldu") !=
          std::string::npos); ///< the three on the sheet and the one inside G

    // NOTHING ON THE SHEET MOVED — the one inside G included.
    for (std::size_t i = 0; i < all.size(); ++i) {
        CAPTURE(i);
        CHECK_EQ(drawn(r, all[i]), before[i]);
    }
    // And the next insertion stands the block on its new base: the line's end
    // lands on the point clicked.
    r.run("BLOKEKLE ad=B nokta=100,0");
    const std::vector<Point2> fresh = drawn(r, r.last_reference());
    REQUIRE_FALSE(fresh.empty());
    CHECK_EQ(fresh[1], (Point2{100'000, 0}));
    r.run("GERİAL");

    // One step back puts the base and every reference back.
    r.run("GERİAL");
    CHECK_EQ(r.doc.blocks().at(b).base, (Point2{0, 0}));
    for (std::size_t i = 0; i < all.size(); ++i)
        CHECK_EQ(drawn(r, all[i]), before[i]);

    // By name, in the definition's own coordinates.
    r.run("BLOKDÜZENLE islem=taban ad=B taban=1,1");
    CHECK_EQ(r.doc.blocks().at(b).base, (Point2{1'000, 1'000}));
    for (std::size_t i = 0; i < all.size(); ++i)
        CHECK_EQ(drawn(r, all[i]), before[i]);

    // Not while an edit is out, and not through a reference that leans.
    r.run("BLOKDÜZENLE nesne=" + std::to_string(at_base));
    CHECK(r.refused("BLOKDÜZENLE islem=taban ad=B taban=0,0").find("düzenlemesi açık") !=
          std::string::npos);
    r.run("GERİAL");
    r.run("BLOKEKLE ad=B nokta=70,0 olcek=2 olcek_y=1");
    CHECK(r.refused("BLOKDÜZENLE islem=taban nesne=" + std::to_string(r.last_reference()) +
                    " taban=70,0")
              .find("farklı ölçekli") != std::string::npos);
}
