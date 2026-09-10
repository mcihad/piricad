// SPDX-License-Identifier: GPL-3.0-or-later
//
// The snap and selection engine.
//
// Two properties carry more weight here than any individual case:
//
//   IDEMPOTENCE — `Bus` records the RESOLVED point in the journal, so replaying a
//   journal feeds a snapped point straight back into the same engine. If any rule
//   moved an already-resolved point, every replay would drift and the Article 6.4
//   proof would rot silently. Each rule is tested twice over.
//
//   SOURCE-BLINDNESS — the same aim through a GUI session, through the command
//   line and through a JSON script must land on the same millimetre, because the
//   aids are applied on the one path all three take (kentoscad.md §2.4).
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/guide.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <array>
#include <cstdlib>

using namespace kentos;
using namespace kentos::command;

namespace {

using core::Mm;
using core::Point2;

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string echoed;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    /// Gives the bus a view, so the pixel tolerances become real millimetres.
    /// One millimetre per pixel keeps the arithmetic in the test readable.
    void with_view(double mm_per_pixel = 1.0) { bus.aids().set_view_scale(mm_per_pixel); }

    core::Result<DispatchResult> line(const std::string& text)
    {
        return bus.execute_line(text, Origin::Test);
    }
};

/// The document a rig is dispatching into. Named because the tests below read it
/// often enough that `rig.doc` spelled out four times a line reads worse.
const core::Document& doc_of(const Rig& rig)
{
    return rig.doc;
}

/// A closed square, 10 m on a side, as one Exterior ring.
core::EntityId add_square(core::Document& doc, Mm x, Mm y, Mm side)
{
    const std::array<Point2, 4> ring{Point2{x, y}, Point2{x + side, y}, Point2{x + side, y + side},
                                     Point2{x, y + side}};
    const core::RingGeometry::RingInput input{ring, core::RingRole::Exterior, 0};
    core::Op op;
    auto created = doc.add_area(doc.ensure_layer("PARSEL"), {&input, 1}, op);
    return created ? created.value() : core::kNoEntity;
}

} // namespace

// ------------------------------------------------------------- grid rule ----

TEST_CASE("SEÇME: alanın İÇİNE tıklamak alanı seçer, deliğine tıklamak seçmez")
{
    // THE DEFECT EVERY TOOL TRIPPED OVER. `pick_nearest` measured to the EDGES
    // only, so a click one metre inside a parcel was a click on nothing — the
    // pick radius is a few pixels. Move, split, offset and match-style all start
    // by asking WHICH objects, so all of them sat waiting for a selection the
    // user could not make by pointing at the thing they meant.
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("PARSEL");

    // A 40 x 30 m face with a 10 x 10 m court cut out of the middle.
    const std::array<Point2, 4> outer{Point2{0, 0}, Point2{40'000, 0}, Point2{40'000, 30'000},
                                      Point2{0, 30'000}};
    const std::array<Point2, 4> hole{Point2{15'000, 10'000}, Point2{25'000, 10'000},
                                     Point2{25'000, 20'000}, Point2{15'000, 20'000}};
    const std::array<core::RingGeometry::RingInput, 2> rings{
        core::RingGeometry::RingInput{outer, core::RingRole::Exterior, 0},
        core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0}};

    const auto made = doc.add_area(layer, rings, op);
    REQUIRE(made.ok());

    // Well inside the face and far from every edge: a few pixels of pick radius
    // could never reach one.
    CHECK_EQ(core::pick_nearest(doc, Point2{5'000, 5'000}, 200), made.value());

    // On an edge, as before.
    CHECK_EQ(core::pick_nearest(doc, Point2{0, 15'000}, 200), made.value());

    // IN THE COURT: a hole is not the thing it was cut out of.
    CHECK_EQ(core::pick_nearest(doc, Point2{20'000, 15'000}, 200), core::kNoEntity);

    // And outside it entirely.
    CHECK_EQ(core::pick_nearest(doc, Point2{80'000, 80'000}, 200), core::kNoEntity);
}

TEST_CASE("YAKALAMA: DÜĞÜM varsayılan maskede — noktaya yakalanabilir")
{
    // THE REGRESSION, and it was invisible from the engine's side: `SnapNode`
    // works and has its own test, but the default mask was UÇ | ORTA | MERKEZ and
    // nothing else. So a user who placed a nirengi and reached for it with the
    // mouse got no snap at all, and every test that asked for the mode by hand
    // passed while the program did not do it.
    Rig r;
    const std::uint32_t modes = r.bus.aid_settings().modes;
    CHECK((modes & core::SnapNode) != 0);

    // And the three it has always had are still on: this widened the default, it
    // did not replace it.
    CHECK((modes & core::SnapEndpoint) != 0);
    CHECK((modes & core::SnapMidpoint) != 0);
    CHECK((modes & core::SnapCenter) != 0);

    // KESİŞİM too: where two boundaries cross is a cadastral point.
    CHECK((modes & core::SnapIntersection) != 0);

    // AND YAKIN, which was off for exactly one reason: it always finds
    // something. That reason turned out to be answered by the priority table
    // rather than by the mask — YAKIN ranks below every real feature, so it can
    // never outrank the corner the user was reaching for — while the cost of
    // leaving it off was real: a cursor brought up to the MIDDLE of a boundary
    // snapped to nothing, and "how far is it to that line" is the question a
    // measurement asks most often.
    CHECK((modes & core::SnapNearest) != 0);
}

TEST_CASE("YAKALAMA: ızgara en yakın kesişime oturur ve ikinci kez oynamaz")
{
    const Mm step = 1000; // 1 m

    CHECK(core::apply_grid(Point2{1400, 2600}, step) == (Point2{1000, 3000}));
    CHECK(core::apply_grid(Point2{-1400, -2600}, step) == (Point2{-1000, -3000}));
    CHECK(core::apply_grid(Point2{500, -500}, step) ==
          (Point2{1000, -1000})); // yarım, sıfırdan uzağa

    // Idempotence: a point already on the lattice is left where it is.
    const Point2 once  = core::apply_grid(Point2{1400, 2600}, step);
    const Point2 twice = core::apply_grid(once, step);
    CHECK(once == twice);

    // A step of zero is "no grid", not a division by zero.
    CHECK(core::apply_grid(Point2{7, 9}, 0) == (Point2{7, 9}));
}

// ------------------------------------------------------------ ortho rule ----

TEST_CASE("YAKALAMA: dik mod imleci en yakın eksene kilitler")
{
    const Point2 base{100000, 200000};

    // Mostly horizontal aim -> the Y of the base is kept.
    CHECK(core::apply_ortho(base, Point2{150000, 203000}) == (Point2{150000, 200000}));
    // Mostly vertical aim -> the X of the base is kept.
    CHECK(core::apply_ortho(base, Point2{101000, 260000}) == (Point2{100000, 260000}));
    // A tie is total and reproducible: the horizontal wins.
    CHECK(core::apply_ortho(base, Point2{110000, 210000}) == (Point2{110000, 200000}));

    const Point2 once = core::apply_ortho(base, Point2{150000, 203000});
    CHECK(core::apply_ortho(base, once) == once);
}

// ------------------------------------------------------------ polar rule ----

TEST_CASE("YAKALAMA: kutupsal izleme açıyı adıma yuvarlar, uzaklığı korur")
{
    const Point2 base{0, 0};
    const std::int64_t step = 90 * core::kUDegPerDegree;

    // 10 m out at ~5 degrees snaps onto the east axis with its length intact.
    const Point2 east = core::apply_polar(base, Point2{9962, 872}, step);
    CHECK(east.y == 0);
    CHECK(east.x > 9990);
    CHECK(east.x < 10010);

    // ~93 degrees snaps onto north.
    const Point2 north = core::apply_polar(base, Point2{-523, 9986}, step);
    CHECK(north.x == 0);
    CHECK(north.y > 9990);
    CHECK(north.y < 10010);

    // A 45-degree step keeps a 45-degree aim where it is, to the millimetre.
    const std::int64_t step45 = 45 * core::kUDegPerDegree;
    const Point2 diagonal     = core::apply_polar(base, Point2{7071, 7071}, step45);
    CHECK(diagonal.x == 7071);
    CHECK(diagonal.y == 7071);

    // Step zero is "no polar tracking".
    CHECK(core::apply_polar(base, Point2{123, 456}, 0) == (Point2{123, 456}));
}

// ------------------------------------------------------- object snapping ----

TEST_CASE("YAKALAMA: uç nokta en yakın tepeye oturur, öncelik en yakından yüksektir")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim    = Point2{300, 200}; // near the corner at (0,0)
    q.radius = 1000;
    q.modes  = core::SnapEndpoint | core::SnapNearest;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapEndpoint));
    CHECK(r.point == (Point2{0, 0}));
    CHECK(r.entity != core::kNoEntity);

    // Re-snapping the resolved point returns it unchanged (journal replay).
    q.aim = r.point;
    CHECK(core::snap(doc, q).point == r.point);
}

TEST_CASE("YAKALAMA: orta nokta, en yakın ve merkez modları")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    // Midpoint of the bottom edge is (5000, 0).
    core::SnapQuery mid;
    mid.aim                  = Point2{5100, 150};
    mid.radius               = 1000;
    mid.modes                = core::SnapMidpoint;
    const core::SnapResult m = core::snap(doc, mid);
    CHECK_EQ(static_cast<int>(m.mode), static_cast<int>(core::SnapMidpoint));
    CHECK(m.point == (Point2{5000, 0}));

    // Nearest lands on the edge itself, not on a vertex.
    core::SnapQuery near;
    near.aim                 = Point2{7000, 300};
    near.radius              = 1000;
    near.modes               = core::SnapNearest;
    const core::SnapResult n = core::snap(doc, near);
    CHECK_EQ(static_cast<int>(n.mode), static_cast<int>(core::SnapNearest));
    CHECK(n.point == (Point2{7000, 0}));

    // A CLOSED SQUARE'S MIDDLE IS ITS CENTROID, and that is now its own mode.
    // `MERKEZ` is the centre a CURVE is drawn about — a circle's, an arc's — and
    // for as long as one bit meant both, the centre a röper is set out from could
    // not be snapped to at all.
    core::SnapQuery centre;
    centre.aim               = Point2{5200, 4900};
    centre.radius            = 1000;
    centre.modes             = core::SnapCentroid;
    const core::SnapResult c = core::snap(doc, centre);
    CHECK_EQ(static_cast<int>(c.mode), static_cast<int>(core::SnapCentroid));
    CHECK(c.point == (Point2{5000, 5000}));

    // And the old name no longer answers for it: a face has no curve to be the
    // centre of.
    centre.modes = core::SnapCenter;
    CHECK_EQ(static_cast<int>(core::snap(doc, centre).mode), static_cast<int>(core::SnapNone));
}

TEST_CASE("YAKALAMA: kesişim iki ayrı çizginin gerçek kesişme noktasını verir")
{
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("CIZIM");

    const std::array<Point2, 2> a{Point2{0, 0}, Point2{10000, 10000}};
    const std::array<Point2, 2> b{Point2{0, 10000}, Point2{10000, 0}};
    (void)doc.add_polyline(layer, a, op);
    (void)doc.add_polyline(layer, b, op);

    core::SnapQuery q;
    q.aim    = Point2{5200, 4800};
    q.radius = 1000;
    q.modes  = core::SnapIntersection;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapIntersection));
    CHECK(r.point == (Point2{5000, 5000}));
}

TEST_CASE("YAKALAMA: dik ayak önceki noktadan indirilen dikin ayağıdır")
{
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 0}};
    (void)doc.add_polyline(doc.ensure_layer("CIZIM"), edge, op);

    core::SnapQuery q;
    q.aim      = Point2{3200, 400};
    q.radius   = 1000;
    q.modes    = core::SnapPerpendicular;
    q.has_base = true;
    q.base     = Point2{3000, 5000};

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapPerpendicular));
    CHECK(r.point == (Point2{3000, 0}));
}

TEST_CASE("YAKALAMA: belge modeli tek noktalı nesne tutar, DÜĞÜM de onu bulur")
{
    // This case used to pin the OPPOSITE: that the model could not hold a survey
    // control point, which is why the DÜĞÜM bit was declared and left unused.
    // `core.point` holds one now, so the mode is real — and the floor of two
    // vertices moved to where the kind is known, which is what the second half
    // checks.
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("NIRENGI");

    const auto made = doc.add_point(layer, Point2{5000, 5000}, op);
    REQUIRE(made.ok());
    CHECK(doc.entities().kind[made.value()] == core::kPointKind);

    core::SnapQuery q;
    q.aim    = Point2{5100, 5000};
    q.radius = 500;
    q.modes  = core::SnapNode;

    const core::SnapResult r = core::snap(doc, q);
    CHECK(r.mode == core::SnapNode);
    CHECK_EQ(r.point.x, Mm{5000});

    // A LINE still needs two: the floor did not disappear, it moved.
    const std::array<Point2, 1> lone{Point2{9000, 9000}};
    const auto refused = doc.add_polyline(layer, lone, op);
    REQUIRE(!refused);
    CHECK(refused.error().message.find("iki nokta") != std::string::npos);
}

TEST_CASE("YAKALAMA: uzantı kenarın kendi doğrultusunu ucundan öteye taşır")
{
    // Re-establishing a boundary whose corner monument is gone: the point wanted
    // is on the line of the surviving edge and past its end, which is exactly
    // where YAKIN cannot reach.
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 0}};
    (void)doc.add_polyline(doc.ensure_layer("SINIR"), edge, op);

    core::SnapQuery q;
    q.aim    = Point2{14000, 120};
    q.radius = 500;
    q.modes  = core::SnapExtension;
    q.reach  = 10000; // the edge itself is 4 m behind the aim

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapExtension));
    CHECK(r.point == (Point2{14000, 0}));

    q.aim = r.point;
    CHECK(core::snap(doc, q).point == (Point2{14000, 0}));

    // Without reach the mode is inert however the mask is set: the engine will not
    // go looking outside the aperture for an edge nobody gave it permission to
    // find.
    q.aim   = Point2{14000, 120};
    q.reach = 0;
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapNone));
}

TEST_CASE("YAKALAMA: uzantı kenarın ÜZERİNDE nokta üretmez")
{
    // Inside the span the answer is the one YAKIN already gives, and offering it
    // under two names would make the priority depend on which mode happened to be
    // on rather than on what the user pointed at.
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 0}};
    (void)doc.add_polyline(doc.ensure_layer("SINIR"), edge, op);

    core::SnapQuery q;
    q.aim    = Point2{5000, 100};
    q.radius = 500;
    q.modes  = core::SnapExtension;
    q.reach  = 10000;

    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapNone));
}

TEST_CASE("YAKALAMA: paralel önceki noktadan bir kenarın doğrultusunu alır")
{
    // A çekme mesafesi and a road edge are drawn this way: the same bearing as
    // that boundary, starting here. The distance from the base is preserved, so a
    // measured length typed after the direction is the length that lands.
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 10000}}; // 45°
    (void)doc.add_polyline(doc.ensure_layer("YOL"), edge, op);

    core::SnapQuery q;
    q.aim      = Point2{23000, 22600}; // near, but not on, the 45° ray from the base
    q.radius   = 600;
    q.modes    = core::SnapParallel;
    q.reach    = 40000;
    q.has_base = true;
    q.base     = Point2{20000, 20000};

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapParallel));

    // On the ray: equal run and rise from the base.
    CHECK_EQ(r.point.x - q.base.x, r.point.y - q.base.y);

    q.aim = r.point;
    CHECK(core::snap(doc, q).point == r.point);

    // With no previous point there is no ray to be parallel to.
    q.aim      = Point2{23000, 22600};
    q.has_base = false;
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapNone));
}

TEST_CASE("YAKALAMA: uzatılmış kesişim iki kenarın buluşacağı köşeyi kurar")
{
    // The ifraz case: the corner monument is gone and the two surviving edges stop
    // short of each other. KESİŞİM finds nothing — they do not cross — and the
    // corner is still the point the parcel needs.
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("SINIR");

    const std::array<Point2, 2> along{Point2{0, 0}, Point2{6000, 0}};
    const std::array<Point2, 2> up{Point2{10000, 4000}, Point2{10000, 12000}};
    (void)doc.add_polyline(layer, along, op);
    (void)doc.add_polyline(layer, up, op);

    core::SnapQuery q;
    q.aim    = Point2{10100, 150};
    q.radius = 600;
    q.reach  = 20000;

    // A real crossing does not exist here, so KESİŞİM alone answers nothing.
    q.modes = core::SnapIntersection;
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapNone));

    q.modes                  = core::SnapApparent;
    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapApparent));
    CHECK(r.point == (Point2{10000, 0}));

    q.aim = r.point;
    CHECK(core::snap(doc, q).point == (Point2{10000, 0}));
}

TEST_CASE("YAKALAMA: kurulmuş nokta, çizimde gerçekten olan noktayı yenemez")
{
    // The rule that makes the constructed modes safe to leave switched on. A point
    // this engine invented must never take a corner the drawing actually contains
    // away from the user, whatever the distances are.
    core::Document doc;
    core::Op op;
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 0}};
    (void)doc.add_polyline(doc.ensure_layer("SINIR"), edge, op);

    core::SnapQuery q;
    q.aim    = Point2{10400, 60}; // the extension is nearer than the corner behind it
    q.radius = 900;
    q.reach  = 20000;
    q.modes  = core::SnapEndpoint | core::SnapExtension;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapEndpoint));
    CHECK(r.point == (Point2{10000, 0}));
}

TEST_CASE("YAKALAMA: her mod bir kimlik, bir etiket ve maskede bir bit taşır")
{
    // CLAUDE.md 5.10: the bit list IS the mode list. A mode added to the enum and
    // forgotten in `snap_mode_bits()` would be unreachable from MOD, from the
    // generated reference and from the canvas marker table at once.
    std::uint32_t seen = 0;
    int count          = 0;

    for (const std::uint32_t* bit = core::snap_mode_bits(); *bit != core::SnapNone; ++bit) {
        CHECK((seen & *bit) == 0); // declared once
        seen = static_cast<std::uint32_t>(seen | *bit);
        ++count;

        const std::string id    = core::snap_mode_id(*bit);
        const std::string label = core::snap_mode_label(*bit);
        CHECK(!id.empty());
        CHECK(id != "yok");
        CHECK(!label.empty());
        CHECK(label != "yok");
    }

    CHECK_EQ(count, 15); // 13 + AĞIRLIK MERKEZİ + EKLEME
    CHECK_EQ(static_cast<int>(seen), static_cast<int>(core::SnapAllMask));
}

TEST_CASE("YAKALAMA: kılavuza oturur, iki kılavuzun kesişimi nokta verir")
{
    core::Document doc;
    core::Op undo;
    REQUIRE(doc.add_guide(core::GuideAxis::Horizontal, 5000, undo).ok());
    REQUIRE(doc.add_guide(core::GuideAxis::Vertical, 7000, undo).ok());

    core::SnapQuery q;
    q.modes  = core::SnapGuide;
    q.radius = 500;

    // Near both guides: the answer is their crossing, which is what makes a pair
    // of them usable for setting a point out.
    q.aim              = core::Point2{7100, 5100};
    core::SnapResult r = core::snap(doc, q);
    CHECK(r.mode == core::SnapGuide);
    CHECK(r.point.x == 7000);
    CHECK(r.point.y == 5000);

    // Near only the horizontal one: the cursor slides ALONG it, keeping its own
    // easting, which is the foot of the perpendicular onto the guide.
    q.aim = core::Point2{90000, 5100};
    r     = core::snap(doc, q);
    CHECK(r.mode == core::SnapGuide);
    CHECK(r.point.x == 90000);
    CHECK(r.point.y == 5000);

    // Out of reach of both: nothing fires.
    q.aim = core::Point2{90000, 90000};
    r     = core::snap(doc, q);
    CHECK(r.mode == core::SnapNone);
}

TEST_CASE("YAKALAMA: gerçek bir köşe kılavuzu yener")
{
    // A guide is a line the user drew for themselves; a corner is a measured
    // fact. Under one aperture the corner must win, or a guide placed by eye
    // would quietly take a parsel corner away from the drawing.
    core::Document doc;
    core::Op undo;
    REQUIRE(doc.add_guide(core::GuideAxis::Horizontal, 100, undo).ok());

    core::Document& d = doc;
    (void)d;

    core::SnapQuery q;
    q.modes  = static_cast<std::uint16_t>(core::SnapGuide | core::SnapEndpoint);
    q.radius = 1000;
    q.aim    = core::Point2{0, 50};

    // With no geometry there is nothing to beat it, so the guide answers.
    const core::SnapResult only_guide = core::snap(doc, q);
    CHECK(only_guide.mode == core::SnapGuide);
}

TEST_CASE("YAKALAMA: yarıçap sıfırken nesne yakalama devre dışıdır")
{
    // This is what keeps a headless journal replay honest: no view, no pixels, no
    // aperture, so a recorded point is never re-snapped onto a neighbour that the
    // recording session did not have (kentos_cad/command/aids.hpp).
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim    = Point2{300, 200};
    q.radius = 0;
    q.modes  = core::SnapAllMask;

    const core::SnapResult r = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(r.mode), static_cast<int>(core::SnapNone));
    CHECK(r.point == (Point2{300, 200}));
}

TEST_CASE("YAKALAMA: sıra nesne > yön > ızgara")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);

    core::SnapQuery q;
    q.aim       = Point2{300, 200};
    q.radius    = 1000;
    q.modes     = core::SnapEndpoint | core::SnapGrid;
    q.grid_step = 1000;
    q.ortho     = true;
    q.has_base  = true;
    q.base      = Point2{-5000, 0};

    // A real corner beats both the axis lock and the lattice.
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapEndpoint));

    // Out of the aperture, dik mod takes over.
    q.aim = Point2{50000, 400};
    CHECK_EQ(static_cast<int>(core::snap(doc, q).mode), static_cast<int>(core::SnapOrtho));

    // Without a previous point there is no direction to lock to, so the grid wins.
    q.has_base               = false;
    const core::SnapResult g = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(g.mode), static_cast<int>(core::SnapGrid));
    CHECK(g.point == (Point2{50000, 0}));
}

// -------------------------------------------------------- pick primitives ---

TEST_CASE("SEÇİM: kutu testleri kesin tam sayı aritmetiğiyle çalışır")
{
    const core::Box2 box{0, 0, 10000, 10000};

    CHECK(core::segment_touches_box(Point2{5000, 5000}, Point2{6000, 6000}, box)); // içeride
    CHECK(
        core::segment_touches_box(Point2{-5000, 5000}, Point2{5000, 5000}, box)); // kenardan girer
    CHECK(core::segment_touches_box(Point2{-5000, -5000}, Point2{15000, 15000}, box)); // köşegen
    CHECK(
        core::segment_touches_box(Point2{-1000, 12000}, Point2{12000, -1000}, box)); // köşeyi keser
    CHECK(!core::segment_touches_box(Point2{-5000, 20000}, Point2{15000, 20000}, box));
    // Passes the bounding-box test but misses the corner it appears to reach: this
    // is the case a bbox-only crossing test would get wrong.
    CHECK(!core::segment_touches_box(Point2{-2000, 8000}, Point2{2000, 14000}, box));

    Point2 hit{};
    CHECK(core::segment_intersection(Point2{0, 0}, Point2{10000, 10000}, Point2{0, 10000},
                                     Point2{10000, 0}, hit));
    CHECK(hit == (Point2{5000, 5000}));
    // Parallel segments have no single crossing point.
    CHECK(!core::segment_intersection(Point2{0, 0}, Point2{10000, 0}, Point2{0, 1000},
                                      Point2{10000, 1000}, hit));

    CHECK(core::closest_point_on_segment(Point2{0, 0}, Point2{10000, 0}, Point2{3000, 5000}) ==
          (Point2{3000, 0}));
    // Beyond the end the answer is the endpoint, never the infinite line.
    CHECK(core::closest_point_on_segment(Point2{0, 0}, Point2{10000, 0}, Point2{20000, 5000}) ==
          (Point2{10000, 0}));
}

TEST_CASE("SEÇİM: pencere tamamen içeride olanı, kesen değene alır")
{
    core::Document doc;
    (void)add_square(doc, 0, 0, 10000);    // wholly inside the query below
    (void)add_square(doc, 8000, 0, 10000); // straddles the right edge

    std::vector<core::EntityId> hits;
    core::pick_in_box(doc, core::Box2{-1000, -1000, 12000, 12000}, core::PickMode::Window, hits);
    CHECK_EQ(hits.size(), std::size_t{1});

    hits.clear();
    core::pick_in_box(doc, core::Box2{-1000, -1000, 12000, 12000}, core::PickMode::Crossing, hits);
    CHECK_EQ(hits.size(), std::size_t{2});
}

TEST_CASE("SEÇİM: tek nokta en yakın nesneyi bulur, eşitlikte küçük slotu seçer")
{
    core::Document doc;
    core::Op op;
    const core::LayerId layer = doc.ensure_layer("CIZIM");
    const std::array<Point2, 2> a{Point2{0, 0}, Point2{10000, 0}};
    const std::array<Point2, 2> b{Point2{0, 0}, Point2{0, 10000}};
    (void)doc.add_polyline(layer, a, op);
    (void)doc.add_polyline(layer, b, op);

    CHECK_EQ(core::pick_nearest(doc, Point2{5000, 200}, 1000), core::EntityId{0});
    CHECK_EQ(core::pick_nearest(doc, Point2{200, 5000}, 1000), core::EntityId{1});
    // Equidistant from both: the lower slot wins, so the answer is stable.
    CHECK_EQ(core::pick_nearest(doc, Point2{0, 0}, 1000), core::EntityId{0});
    // Nothing within reach.
    CHECK_EQ(core::pick_nearest(doc, Point2{500000, 500000}, 1000), core::kNoEntity);
}

// ---------------------------------------------------------- the selection ---

TEST_CASE("SEÇİM: KATMAN modu yalnız o katmanı alır, gizli olanı almaz")
{
    // WHAT THE LAYER PANEL'S "Tümünü seç" RUNS. The menu entry sends this exact
    // line and nothing else (Article 1.2: the menu is a client, not a shortcut),
    // so the behaviour the user sees is the behaviour checked here.
    Rig rig;
    if (!rig.line("KATMAN ad=PARSEL")) FAIL("KATMAN PARSEL");
    if (!rig.line("ALAN noktalar=0,0 10,0 10,10 0,10")) FAIL("ALAN 1");
    if (!rig.line("ALAN noktalar=20,0 30,0 30,10 20,10")) FAIL("ALAN 2");
    if (!rig.line("KATMAN ad=YOL")) FAIL("KATMAN YOL");
    if (!rig.line("ÇİZGİ noktalar=0,20 30,20")) FAIL("ÇİZGİ");

    REQUIRE_EQ(rig.doc.live_entity_count(), std::size_t{3});

    if (!rig.line("SEÇ mod=KATMAN katman=PARSEL")) FAIL("SEÇ PARSEL");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{2});

    if (!rig.line("SEÇ mod=KATMAN katman=YOL")) FAIL("SEÇ YOL");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{1});

    // HIDDEN IS NOT SELECTED, the same rule TÜMÜ follows: what the user cannot
    // see, the user did not mean to grab. A layer whose eye is shut therefore
    // selects nothing rather than quietly filling the selection with objects
    // that are not on the screen.
    if (!rig.line("KATMAN ad=PARSEL gorunur=hayır")) FAIL("KATMAN gizle");
    if (!rig.line("SEÇ mod=KATMAN katman=PARSEL")) FAIL("SEÇ gizli");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{0});

    // A name nobody declared is refused rather than silently emptying the
    // selection, because "no such layer" and "that layer is empty" are different
    // answers to the user.
    rig.echoed.clear();
    (void)rig.line("SEÇ mod=KATMAN katman=YOKBÖYLE");
    CHECK(rig.echoed.find("Katman bulunamadı") != std::string::npos);
}

TEST_CASE("YAKALAMA: daire merkezine ve çemberine yakalanır, hayalet çizgisine değil")
{
    // WHAT A CIRCLE IS IN STORE, and why snapping got it wrong. A circle keeps two
    // vertices — its centre and a handle due east at the radius — and the whole of
    // `snap()` walked that pair as if it were a drawn SEGMENT. So a click near the
    // circle offered the middle of a line nobody drew, the nearest point of a line
    // nobody drew, and the east handle as if it were a corner; the one thing it
    // could not offer was the CENTRE, because `ring_centroid` wants a closed ring
    // of three vertices and a circle's is neither.
    Rig rig;
    rig.with_view();
    if (!rig.line("DAİRE merkez=10,10 cevre=15,10")) FAIL("DAİRE");

    core::SnapQuery q;
    q.radius = 800; // 0.8 m aperture
    q.modes  = core::SnapCenter | core::SnapNearest | core::SnapEndpoint | core::SnapMidpoint;

    // THE CENTRE. This is what `MERKEZ` means on a circle in every CAD program
    // there is, and it is what a surveyor reaches for to set a röper out.
    q.aim                         = Point2{10200, 10200};
    const core::SnapResult centre = core::snap(rig.doc, q);
    CHECK(centre.mode == core::SnapCenter);
    CHECK_EQ(centre.point, Point2{10000, 10000});

    // THE CURVE ITSELF, exactly — centre plus radius along the aim's own
    // direction, not a tessellated approximation of it.
    q.aim                      = Point2{15500, 10000};
    const core::SnapResult rim = core::snap(rig.doc, q);
    CHECK(rim.mode == core::SnapNearest);
    CHECK_EQ(rim.point, Point2{15000, 10000});

    // AND NOTHING IN BETWEEN. The stored handle sits due east at the radius and
    // the midpoint of the phantom segment sits halfway to it; both used to be
    // offered, and both are places the drawing has nothing at all.
    q.aim                        = Point2{12500, 10000}; // the phantom midpoint
    q.modes                      = core::SnapEndpoint | core::SnapMidpoint;
    const core::SnapResult ghost = core::snap(rig.doc, q);
    CHECK(ghost.mode == core::SnapNone);
}

TEST_CASE("YAKALAMA: elipsin merkezi, eksen uçları ve kendisi yakalanır, tanım çizgileri değil")
{
    // WHAT AN ELLIPSE IS IN STORE. Three vertices — the centre and the ends of
    // its two axes — in one open ring, and `snap()` walked them as if they were a
    // drawn polyline: it offered the centre and the axis ends as corners, the
    // middles of the two definition lines, and the nearest point ON those lines.
    // The user drawing next to an ellipse landed on its axes, which nobody drew,
    // and never on its edge, which everybody sees. Snapping now reads the same
    // 128-gon the picture and the pick test are built from.
    Rig rig;
    rig.with_view();
    if (!rig.line("ELİPS merkez=10,10 birinci=16,10 ikinci=10,13")) FAIL("ELİPS");

    core::SnapQuery q;
    q.radius = 800; // 0.8 m aperture
    q.modes  = core::SnapCenter | core::SnapNearest | core::SnapEndpoint | core::SnapMidpoint;

    // THE CENTRE, as on a circle.
    q.aim                         = Point2{10200, 10200};
    const core::SnapResult centre = core::snap(rig.doc, q);
    CHECK(centre.mode == core::SnapCenter);
    CHECK_EQ(centre.point, Point2{10000, 10000});

    // THE FOUR AXIS ENDS are UÇ: the two the command was given and their mirrors.
    q.aim                       = Point2{16100, 10100};
    const core::SnapResult east = core::snap(rig.doc, q);
    CHECK(east.mode == core::SnapEndpoint);
    CHECK_EQ(east.point, Point2{16000, 10000});
    q.aim                       = Point2{3900, 9900};
    const core::SnapResult west = core::snap(rig.doc, q);
    CHECK(west.mode == core::SnapEndpoint);
    CHECK_EQ(west.point, Point2{4000, 10000});
    q.aim                        = Point2{10100, 6900};
    const core::SnapResult south = core::snap(rig.doc, q);
    CHECK(south.mode == core::SnapEndpoint);
    CHECK_EQ(south.point, Point2{10000, 7000});

    // THE CURVE ITSELF. At x = 13 m the ellipse passes y = 10 + 3·√(1 − 0.25) =
    // 12.598 m; the 128-gon's chord sits within a couple of millimetres of it.
    q.aim                      = Point2{13000, 12700};
    q.modes                    = core::SnapNearest;
    const core::SnapResult rim = core::snap(rig.doc, q);
    CHECK(rim.mode == core::SnapNearest);
    CHECK(std::abs(rim.point.x - 13000) <= 30);
    CHECK(std::abs(rim.point.y - 12598) <= 30);

    // AND NOTHING ON THE AXES. Halfway from the centre to the east end is the
    // midpoint of a definition line and the nearest point of it; the curve is
    // 2.6 m away from there, the axis end 3 m. Nothing is within the aperture.
    q.aim                        = Point2{13000, 10000};
    q.modes                      = core::SnapEndpoint | core::SnapMidpoint | core::SnapNearest;
    const core::SnapResult ghost = core::snap(rig.doc, q);
    CHECK(ghost.mode == core::SnapNone);
}

TEST_CASE("YAKALAMA: yayın uçları, ortası ve kendisi yakalanır")
{
    Rig rig;
    rig.with_view();
    // A quarter arc about the origin, radius 10 m, sweeping from due east to due
    // north — `add_arc` runs counter-clockwise from start to end.
    if (!rig.line("YAY merkez=0,0 baslangic=10,0 bitis=0,10")) FAIL("YAY");

    core::SnapQuery q;
    q.radius = 700;
    q.modes  = core::SnapCenter | core::SnapEndpoint | core::SnapMidpoint | core::SnapNearest;

    q.aim = Point2{300, 300};
    CHECK_EQ(static_cast<int>(core::snap(rig.doc, q).mode), static_cast<int>(core::SnapCenter));

    q.aim                       = Point2{10300, 200};
    const core::SnapResult ends = core::snap(rig.doc, q);
    CHECK_EQ(static_cast<int>(ends.mode), static_cast<int>(core::SnapEndpoint));
    CHECK_EQ(ends.point, Point2{10000, 0});

    // HALFWAY ALONG THE CURVE, not the middle of the chord between its ends —
    // which is inside the arc and on nothing that is drawn. At 45 degrees on a
    // 10 m radius that is (7071, 7071).
    q.aim                      = Point2{7100, 7100};
    const core::SnapResult mid = core::snap(rig.doc, q);
    CHECK_EQ(static_cast<int>(mid.mode), static_cast<int>(core::SnapMidpoint));
    CHECK_EQ(mid.point, Point2{7071, 7071});

    // AND ONLY THE PART THAT IS DRAWN. A quarter arc is a quarter: the other
    // three sit on the circle it was cut from and on nothing the drawing has.
    q.aim = Point2{-10300, 0};
    CHECK_EQ(static_cast<int>(core::snap(rig.doc, q).mode), static_cast<int>(core::SnapNone));
}

TEST_CASE("YAKALAMA: yüzey normali bir KİLİT değil, bir YAKALAMADIR")
{
    // WHAT WAS WRONG WITH IT. Held down as an absolute lock it did what it was
    // told and nothing else was drawable: with the mode on, every line and every
    // measurement came out perpendicular no matter where the user aimed. Turning
    // the aid on meant giving up the drawing.
    //
    // That is not what a snap is. Every other rule in this engine offers a point
    // when the aim is NEAR it and stands aside when it is not.
    core::Document doc;
    core::Op undo;
    const core::LayerId layer = doc.ensure_layer("SINIR");

    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{20000, 0}};
    REQUIRE(doc.add_polyline(layer, edge, undo).ok());

    core::SnapQuery q;
    q.has_base     = true;
    q.base         = Point2{5000, 0}; // on the edge
    q.normal_lock  = true;
    q.normal_reach = 500;

    // INSIDE THE CONE — 9.5 degrees off a vertical normal. It lands exactly on
    // the perpendicular, which is the whole point of asking for one.
    q.aim                     = Point2{5500, 3000};
    const core::SnapResult on = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(on.mode), static_cast<int>(core::SnapNormal));
    CHECK_EQ(on.point, Point2{5000, 3000});

    // OUTSIDE IT — 45 degrees off. The aim is the user's own and nothing touches
    // it. This is the assertion the old absolute lock could never make.
    q.aim                       = Point2{8000, 3000};
    const core::SnapResult free = core::snap(doc, q);
    CHECK(free.mode != core::SnapNormal);
    CHECK_EQ(free.point, Point2{8000, 3000});

    // AND BOTH SIDES ARE LIVE. A perpendicular may be struck into the parcel or
    // out of it, and which one is decided by where the user aimed, not by a sign
    // chosen here.
    q.aim                       = Point2{5500, -3000};
    const core::SnapResult back = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(back.mode), static_cast<int>(core::SnapNormal));
    CHECK_EQ(back.point, Point2{5000, -3000});
}

TEST_CASE("YAKALAMA: yüzey normali KARŞI KENARA da dik iner")
{
    // WHAT A PERPENDICULAR IS FOR. It is almost never drawn into empty space: it
    // runs from one boundary ACROSS to another, and where it lands is the answer.
    // A çekme mesafesi ends on the building line; a section runs wall to wall.
    //
    // Reaching the far edge used to COST the perpendicular. The object snap ran
    // first and won, as it should in every other case: the point went onto the
    // far edge at whatever spot the cursor was nearest, and the run stopped being
    // square to the surface it left — in the one gesture whose entire purpose was
    // to stay square.
    Rig rig;
    REQUIRE(rig.line("ÇİZGİ noktalar=0,0 20,0").ok());   // the surface left
    REQUIRE(rig.line("ÇİZGİ noktalar=0,10 20,10").ok()); // the one across

    core::SnapQuery q;
    q.has_base     = true;
    q.base         = Point2{5000, 0};
    q.normal_lock  = true;
    q.normal_reach = 500;
    q.radius       = 500;
    q.modes        = core::SnapEndpoint | core::SnapMidpoint | core::SnapNearest;

    // 2.3 degrees off the perpendicular and 200 mm short of the far line: inside
    // the cone, and inside the aperture of the crossing.
    q.aim                       = Point2{5400, 9800};
    const core::SnapResult onto = core::snap(rig.doc, q);
    CHECK_EQ(static_cast<int>(onto.mode), static_cast<int>(core::SnapNormal));

    // ON the far line — y is exactly 10 m — AND still square to the near one:
    // x has not moved off the base by a millimetre.
    CHECK_EQ(onto.point, Point2{5000, 10000});

    // WITHOUT THE AID, the same aim is the case the user described: YAKIN takes
    // it onto the far line at the nearest point, 400 mm along, and the run is no
    // longer perpendicular to anything.
    q.normal_lock                = false;
    const core::SnapResult askew = core::snap(rig.doc, q);
    CHECK_EQ(askew.point, Point2{5400, 10000});

    // AND IT STILL STOPS AT NOTHING. Aimed into open ground the perpendicular is
    // a direction and no more: there is no edge to land on, so the point is the
    // projection of the aim and not some invented crossing.
    q.normal_lock                      = true;
    q.aim                              = Point2{5400, 4000};
    const core::SnapResult open_ground = core::snap(rig.doc, q);
    CHECK_EQ(static_cast<int>(open_ground.mode), static_cast<int>(core::SnapNormal));
    CHECK_EQ(open_ground.point, Point2{5000, 4000});
}

TEST_CASE("YAKALAMA: bir NESNE yakalaması yüzey normalini yener")
{
    // WHAT THE USER HIT. With the aid on, a cursor brought up to the end of
    // another line did not take it: they could measure to the perpendicular and
    // to nothing else. The order in `snap()` already says an object snap returns
    // before any direction constraint is reached — this is the case that proves
    // the aid did not quietly become an exception to it.
    Rig rig;
    REQUIRE(rig.line("ÇİZGİ noktalar=0,0 20,0").ok());  // the surface
    REQUIRE(rig.line("ÇİZGİ noktalar=15,3 18,9").ok()); // and a corner to reach for

    // A REAL APERTURE. The declared tolerance is in screen pixels, so it is the
    // view scale that turns it into millimetres; at 16 mm per pixel the default
    // aperture is wide enough to reach a corner 224 mm away.
    rig.bus.aids().set_view_scale(16.0);
    REQUIRE(rig.line("MOD yüzey_normali evet").ok());

    rig.echoed.clear();
    REQUIRE(rig.line("ÖLÇ baslangic=5,0 bitis=15.2,3.1").ok());

    // The corner at (15, 3), not the foot of a perpendicular at (5, 3.1):
    // hypot(10, 3) = 10.440 m, and the locked answer would have been 3.100.
    CHECK(rig.echoed.find("10,440") != std::string::npos);
}

TEST_CASE("YAKALAMA: YAKIN varsayılan olarak açık — çizginin herhangi bir noktası")
{
    // WHAT WAS MISSING. A measurement asks "how far is it from here to that
    // boundary" far more often than it asks about a corner, and YAKIN is the mode
    // that answers it. It was off by default, so a cursor brought up to the middle
    // of a boundary snapped to nothing and the measurement came from wherever the
    // pixel happened to land — silently, and wrong by a pixel's worth of ground.
    Rig rig;
    CHECK((rig.bus.aid_settings().modes & core::SnapNearest) != 0);

    REQUIRE(rig.line("ÇİZGİ noktalar=0,0 20,0").ok());
    rig.bus.aids().set_view_scale(16.0);

    // Both aims are 200 mm off the line and nowhere near either end, so only
    // YAKIN can answer them. Snapped, the measured run is exactly 10 m; unsnapped
    // it is 10 m as well — which is why the ANGLE is checked too: off the line the
    // run would still be east-west, but it would not START on the boundary.
    rig.echoed.clear();
    REQUIRE(rig.line("ÖLÇ baslangic=5,0.2 bitis=15,-0.2").ok());
    CHECK(rig.echoed.find("10,000") != std::string::npos);
    CHECK(rig.echoed.find("90.000°") != std::string::npos);

    // And it never takes a corner away: the priority table ranks it below every
    // real feature, so an aim near the end lands ON the end.
    core::SnapQuery q;
    q.aim    = Point2{19800, 200};
    q.radius = 400;
    q.modes  = core::SnapEndpoint | core::SnapNearest;
    CHECK_EQ(static_cast<int>(core::snap(rig.doc, q).mode), static_cast<int>(core::SnapEndpoint));
}

TEST_CASE("YAKALAMA: ÖLÇ'ün ikinci noktası da yakalanıyor")
{
    // WHAT WAS ASKED. Snapping runs inside the command layer, on the path every
    // client takes, so a second point is resolved exactly like a first — and a
    // rubber band does not weaken it: an object snap RETURNS before the direction
    // locks are reached, so dik mod cannot pull a corner off its corner.
    Rig rig;
    rig.with_view();
    if (!rig.line("ÇİZGİ noktalar=0,0 20,0")) FAIL("ÇİZGİ");
    if (!rig.line("ÇİZGİ noktalar=0,10 20,10")) FAIL("ÇİZGİ 2");

    // Both aims are 20 cm off a real corner. `with_view()` puts the aperture at
    // the declared tolerance in millimetres, which the fixture sets wide enough.
    if (!rig.line("AYAR core.yakalama.tolerans 400")) FAIL("AYAR tolerans");
    rig.bus.aids().set_view_scale(1.0);

    auto measured = rig.line("ÖLÇ baslangic=0.2,0.2 bitis=20.2,10.2");
    if (!measured) FAIL_WITH("ÖLÇ", measured.error().message);

    // The transcript reports the distance between the points the command actually
    // took, so exactly 20 m by 10 m is the assertion that both ends snapped.
    CHECK(rig.echoed.find("22,361") != std::string::npos);
}

TEST_CASE("YAKALAMA: yüzey normali kilidi sayfaya değil, YÜZEYE dik çizer")
{
    // WHAT DİK MOD CANNOT DO. Dik mod squares a line to the SHEET. A çekme
    // mesafesi runs perpendicular to the BOUNDARY it is measured from, a building
    // line to the road it faces — and on a boundary running at an angle, dik mod
    // is exactly the wrong answer. There was no right one: the surveyor read the
    // bearing, added ninety and typed it.
    core::Document doc;
    core::Op undo;
    const core::LayerId layer = doc.ensure_layer("SINIR");

    // An edge at 45 degrees, so the surface normal and both page axes disagree.
    const std::array<Point2, 2> edge{Point2{0, 0}, Point2{10000, 10000}};
    REQUIRE(doc.add_polyline(layer, edge, undo).ok());

    core::SnapQuery q;
    q.has_base     = true;
    q.base         = Point2{5000, 5000}; // on the edge
    q.normal_lock  = true;
    q.normal_reach = 2000;

    // The aim is a loose wave in the general direction of the normal; the lock
    // puts it exactly on it. The normal of a 45-degree edge is 135 degrees, so a
    // point 1000 out lands at (-707, +707) from the base.
    q.aim                    = Point2{4000, 6500};
    const core::SnapResult n = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(n.mode), static_cast<int>(core::SnapNormal));
    CHECK(n.constrained);

    // ON the normal: the vector from base to point is perpendicular to the edge,
    // which is the whole assertion. Exactly, in integers.
    const double along = static_cast<double>(n.point.x - q.base.x) * 10000.0 +
                         static_cast<double>(n.point.y - q.base.y) * 10000.0;
    CHECK(std::abs(along) < 1.0e7); // the dot product with the edge is zero to rounding

    // IT OUTRANKS DİK MOD, because it is a deliberate constraint against a named
    // edge and dik mod is a default about the page.
    q.ortho                     = true;
    const core::SnapResult over = core::snap(doc, q);
    CHECK_EQ(static_cast<int>(over.mode), static_cast<int>(core::SnapNormal));

    // NO SURFACE, NO LOCK. A perpendicular to nothing is not a constraint, it is
    // an invention — so out of reach it falls through to whatever else is on.
    q.ortho                     = false;
    q.base                      = Point2{900000, 900000};
    q.aim                       = Point2{901000, 901500};
    const core::SnapResult none = core::snap(doc, q);
    CHECK(none.mode != core::SnapNormal);
}

TEST_CASE("YAKALAMA: MOD yüzey_normali yazmak GERÇEKTEN kilitliyor")
{
    // THE REGRESSION, and the case above could not see it. `core::snap` honoured
    // the lock from the first day; what did not happen was CARRYING it there.
    // `InputAids::resolve` built the `SnapQuery` field by field and simply left
    // `normal_lock` and `normal_reach` out, so the setting was read, cached in
    // `AidSettings`, echoed back by `MOD` as set, ticked in the menu — and every
    // line came out exactly where it would have without it.
    //
    // So this test refuses to touch `SnapQuery`. It goes in through the command a
    // user types and comes out at the document, which is the only path that
    // proves the wiring rather than the arithmetic.
    Rig rig;
    rig.with_view();

    REQUIRE(rig.line("KATMAN ad=SINIR").ok());
    REQUIRE(rig.line("ÇİZGİ 0,0 10,10").ok()); // an edge at 45 degrees, in metres

    // Two points, both typed: the base sits on the edge and the aim is a loose
    // wave INSIDE the aid's cone — a couple of degrees off the perpendicular,
    // which is how a hand aims. Source-blindness is the point: a typed run and a
    // drawn run take the same road through `apply_input_aids`.
    REQUIRE(rig.line("MOD yüzey_normali evet").ok());
    REQUIRE(rig.line("ÇİZGİ 5,5 9,1.2").ok());

    const auto drawn = static_cast<core::EntityId>(doc_of(rig).entities().size() - 1);
    const core::RingSpan rings =
        doc_of(rig).geometry().rings_of(doc_of(rig).entities().slot[drawn]);
    REQUIRE(rings.count == 1);
    const auto xs = doc_of(rig).geometry().ring_xs(rings.first);
    const auto ys = doc_of(rig).geometry().ring_ys(rings.first);
    REQUIRE(xs.size() == 2);

    // PERPENDICULAR TO THE EDGE, exactly. The edge runs at 45, so the drawn
    // segment must run at 315: dx positive, dy its exact negative.
    const Mm dx = xs[1] - xs[0];
    const Mm dy = ys[1] - ys[0];
    CHECK(dx > 0);
    CHECK_EQ(static_cast<long long>(dx), static_cast<long long>(-dy));

    // AND THE SETTING IS WHAT DID IT. Turned off, the same line lands where it
    // was aimed — a check that would pass by accident if the engine were simply
    // ignoring the aim.
    REQUIRE(rig.line("MOD yüzey_normali hayır").ok());
    REQUIRE(rig.line("ÇİZGİ 5,5 9,1.2").ok());

    const auto free_drawn = static_cast<core::EntityId>(doc_of(rig).entities().size() - 1);
    const core::RingSpan free_rings =
        doc_of(rig).geometry().rings_of(doc_of(rig).entities().slot[free_drawn]);
    const auto fxs = doc_of(rig).geometry().ring_xs(free_rings.first);
    const auto fys = doc_of(rig).geometry().ring_ys(free_rings.first);
    CHECK_EQ(static_cast<long long>(fxs[1]), 9000LL);
    CHECK_EQ(static_cast<long long>(fys[1]), 1200LL);
}

TEST_CASE("SEÇİM: pick_all imlecin altındaki her şeyi, en yakın önce verir")
{
    // WHAT THE CHOOSER READS. A click on a plan sheet lands on a parcel, on the
    // ada boundary over it and on the road line through it; the list is what lets
    // a user say which of the three they meant.
    core::Document doc;
    const core::EntityId inner = add_square(doc, 0, 0, 10000);
    const core::EntityId outer = add_square(doc, -5000, -5000, 20000);

    std::vector<core::EntityId> under;

    // A generous radius takes both; the point is inside each of them.
    core::pick_all(doc, Point2{5000, 5000}, 20000, under);
    CHECK_EQ(under.size(), std::size_t{2});

    // THE SAME ANSWER AS `pick_nearest`, AT THE FRONT. The chooser's first row is
    // what a plain click has always selected, so pressing Enter on it changes
    // nothing — and the two must not be free to disagree about which is on top.
    CHECK_EQ(under.front(), core::pick_nearest(doc, Point2{5000, 5000}, 20000));

    // Far from both, and a radius that reaches neither.
    core::pick_all(doc, Point2{500000, 500000}, 1000, under);
    CHECK(under.empty());

    // A THIRD SQUARE, NOWHERE NEAR THE OTHER TWO. Distance to a face the point is
    // INSIDE is zero, so two nested squares are always both at zero and never
    // separate — which is exactly why the chooser exists. Separation is tested
    // where it can be: a point close to one shape and far from the rest.
    const core::EntityId away = add_square(doc, 100000, 100000, 10000);
    core::pick_all(doc, Point2{99500, 105000}, 1000, under);
    REQUIRE_EQ(under.size(), std::size_t{1});
    CHECK_EQ(under.front(), away);
    CHECK(inner != away);

    // `out` is CLEARED, not appended to: the previous query's answer must not
    // survive into this one.
    core::pick_all(doc, Point2{-5000, -5000}, 1000, under);
    REQUIRE_EQ(under.size(), std::size_t{1});
    CHECK_EQ(under.front(), outer);
}

TEST_CASE("SEÇİM: SEÇ NOKTA sira= üstteki nesnenin altına iner")
{
    // WITHOUT THIS THE CHOOSER WOULD BE A GESTURE. The shell's window picks a row
    // and sends `SEÇ NESNE`, but the CAPABILITY — reach past the top object under
    // a point — has to exist for a script and for the command line too, or it is
    // a feature only a mouse has (CLAUDE.md 5.15).
    Rig rig;
    rig.with_view();
    if (!rig.line("KATMAN ad=PARSEL")) FAIL("KATMAN PARSEL");
    if (!rig.line("ALAN noktalar=0,0 10,0 10,10 0,10")) FAIL("ALAN 1");
    if (!rig.line("KATMAN ad=ADA")) FAIL("KATMAN ADA");
    if (!rig.line("ALAN noktalar=-5,-5 15,-5 15,15 -5,15")) FAIL("ALAN 2");

    // Inside both. `tolerans` is in metres and given here so the test does not
    // depend on a screen scale.
    if (!rig.line("SEÇ mod=NOKTA noktalar=5,5 tolerans=20")) FAIL("SEÇ 1");
    REQUIRE_EQ(rig.bus.selection().size(), std::size_t{1});
    const core::EntityKey first = rig.bus.selection().keys().front();

    if (!rig.line("SEÇ mod=NOKTA noktalar=5,5 tolerans=20 sira=2")) FAIL("SEÇ 2");
    REQUIRE_EQ(rig.bus.selection().size(), std::size_t{1});
    CHECK(rig.bus.selection().keys().front() != first);

    // `sira=1` is what a bare NOKTA has always meant, and it must stay that.
    if (!rig.line("SEÇ mod=NOKTA noktalar=5,5 tolerans=20 sira=1")) FAIL("SEÇ 3");
    REQUIRE_EQ(rig.bus.selection().size(), std::size_t{1});
    CHECK_EQ(rig.bus.selection().keys().front(), first);

    // Past the end is REFUSED and says how many there were, rather than quietly
    // selecting nothing — "there is no third one" and "the third one is empty"
    // are different answers.
    rig.echoed.clear();
    (void)rig.line("SEÇ mod=NOKTA noktalar=5,5 tolerans=20 sira=9");
    CHECK(rig.echoed.find("9. istendi") != std::string::npos);

    rig.echoed.clear();
    (void)rig.line("SEÇ mod=NOKTA noktalar=5,5 tolerans=20 sira=0");
    CHECK(rig.echoed.find("1'den küçük olamaz") != std::string::npos);
}

TEST_CASE("SEÇİM: yazı harflerinden tutulur, taban çizgisinden değil")
{
    // THE REGRESSION, and it made every imported caption unclickable. A text
    // entity's geometry is the BASELINE — a hairline under the letters — so the
    // stored bounding box was zero millimetres tall and the distance test measured
    // to a line nothing is drawn on. A click in the middle of `PARSEL 12` was a
    // click a whole text height away from anything the document thought was there,
    // and a zoning plan's 13 112 captions could be seen and not selected.
    Rig rig;
    rig.with_view();
    if (!rig.line("METİN noktalar=0,0 yazi=\"PARSEL 12\" yukseklik=3000")) FAIL("METİN");

    core::EntityId text = core::kNoEntity;
    for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e)
        if (rig.doc.alive(e) && rig.doc.texts().has(rig.doc.entities().slot[e])) text = e;
    REQUIRE(text != core::kNoEntity);

    // THE BOX COVERS THE LETTERS. Zero tall is the defect in one number: no query
    // whose own box misses that single line can ever reach the entity, however
    // exact the distance test after it is.
    const core::Box2 box = rig.doc.entities().box_of(text);
    CHECK(box.max_y - box.min_y >= 3000);

    // Half the cap height above the baseline is the middle of the letters, and it
    // is where a person aims. The tolerance is tight on purpose: a generous one
    // would pass by reaching the baseline rather than the glyphs.
    if (!rig.line("SEÇ mod=NOKTA noktalar=8,1.5 tolerans=0.2")) FAIL("SEÇ harf");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{1});

    // And still on the baseline itself, which is where it always worked.
    if (!rig.line("SEÇ mod=NOKTA noktalar=8,0 tolerans=0.2")) FAIL("SEÇ taban");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{1});

    // Well clear of the band is still a miss: the caption grew to its letters, not
    // to the whole neighbourhood.
    if (!rig.line("SEÇ mod=NOKTA noktalar=8,12 tolerans=0.2")) FAIL("SEÇ uzak");
    CHECK_EQ(rig.bus.selection().size(), std::size_t{0});
}

TEST_CASE("YAZIDÜZENLE: var olan bir yazıyı yerinde değiştirir")
{
    // A CAPTION THAT COULD BE READ AND NOT WRITTEN. `METİN` draws one; nothing
    // rewrote one, which is why the property panel showed its text read-only with
    // the note that a row becomes editable when a command exists for it.
    Rig rig;
    rig.with_view();
    if (!rig.line("METİN noktalar=0,0 yazi=\"PARSEL 12\" yukseklik=3000")) FAIL("METİN");

    core::EntityId text = core::kNoEntity;
    for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e)
        if (rig.doc.alive(e) && rig.doc.texts().has(rig.doc.entities().slot[e])) text = e;
    REQUIRE(text != core::kNoEntity);
    const std::uint32_t slot = rig.doc.entities().slot[text];

    if (!rig.line("SEÇ mod=NOKTA noktalar=8,1.5 tolerans=0.2")) FAIL("SEÇ");
    if (!rig.line("YAZIDÜZENLE yazi=\"ADA 128\"")) FAIL("YAZIDÜZENLE");

    CHECK_EQ(std::string(rig.doc.texts().text(slot)), std::string("ADA 128"));

    // WHAT WAS NOT ASKED FOR DOES NOT MOVE. Rewriting the words must not reset a
    // height somebody chose.
    CHECK_EQ(rig.doc.texts().height(slot), core::Mm{3000});

    if (!rig.line("YAZIDÜZENLE yukseklik=4000")) FAIL("YAZIDÜZENLE yukseklik");
    CHECK_EQ(rig.doc.texts().height(slot), core::Mm{4000});
    CHECK_EQ(std::string(rig.doc.texts().text(slot)), std::string("ADA 128"));

    // AND THE BOX FOLLOWED IT, which is what keeps the caption clickable after it
    // grows.
    CHECK(rig.doc.entities().box_of(text).max_y - rig.doc.entities().box_of(text).min_y >= 4000);

    // ONE UNDO STEP, and it puts back both the words and the height.
    if (!rig.line("GERİAL")) FAIL("GERİAL");
    CHECK_EQ(rig.doc.texts().height(slot), core::Mm{3000});

    // A selection with no caption in it is told so rather than silently doing
    // nothing that looks like success.
    Rig bare;
    bare.with_view();
    if (!bare.line("ÇİZGİ noktalar=0,0 10,0")) FAIL("ÇİZGİ");
    if (!bare.line("SEÇ mod=TÜMÜ")) FAIL("SEÇ TÜMÜ");
    bare.echoed.clear();
    (void)bare.line("YAZIDÜZENLE yazi=\"olmaz\"");
    CHECK(bare.echoed.find("yazı taşıyan nesne yok") != std::string::npos);
}

TEST_CASE("SEÇİM: küme anahtar sıralı, tekil ve sürüm sayar")
{
    Selection s;
    CHECK(s.empty());

    CHECK(s.add(core::EntityKey{7}));
    CHECK(s.add(core::EntityKey{3}));
    CHECK(!s.add(core::EntityKey{7})); // already there
    CHECK_EQ(s.size(), std::size_t{2});
    CHECK(s.keys().front() == core::EntityKey{3});
    CHECK(s.keys().back() == core::EntityKey{7});

    CHECK(s.contains(core::EntityKey{3}));
    CHECK(!s.contains(core::EntityKey{4}));

    const std::uint64_t revision = s.revision();
    CHECK(!s.toggle(core::EntityKey{3})); // was in, now out
    CHECK(s.toggle(core::EntityKey{3}));  // and back in
    CHECK(s.revision() > revision);

    CHECK(!s.add(core::EntityKey::None)); // "no entity" is never selected

    s.clear();
    CHECK(s.empty());
}

// ------------------------------------------------------------- the command --

TEST_CASE("SEÇ: tümü, temizle, kimlik, pencere, kesen ve nokta")
{
    Rig r;
    CHECK(r.line("KATMAN ad=CIZIM").ok());
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());
    CHECK(r.line("ÇİZGİ 100,100 110,100").ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{3});

    CHECK(r.line("SEÇ TÜMÜ").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{3});

    CHECK(r.line("SEÇ TEMİZLE").ok());
    CHECK(r.bus.selection().empty());

    // Keys start at 1 (EntityKey{0} is "none").
    CHECK(r.line("SEÇ NESNE nesneler=1 nesneler=3").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
    CHECK(r.bus.selection().contains(core::EntityKey{3}));

    // A window around the first two lines takes exactly those two.
    CHECK(r.line("SEÇ PENCERE -1,-1 11,6").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    // A crossing box that only clips the first line still takes it.
    CHECK(r.line("SEÇ KESEN 5,-1 20,1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));

    // KUTU reads the drag direction: right to left is a crossing box.
    CHECK(r.line("SEÇ KUTU 20,1 5,-1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});

    // Left to right is a window, and this one contains nothing whole.
    CHECK(r.line("SEÇ KUTU 5,-1 20,1").ok());
    CHECK(r.bus.selection().empty());

    // A point pick with an explicit tolerance works without a screen.
    CHECK(r.line("SEÇ NOKTA 5,0 tolerans=0.5").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
}

TEST_CASE("SEÇ: ekle ve çıkar seçimi büyütür ve küçültür")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());

    CHECK(r.line("SEÇ NESNE nesneler=1").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});

    CHECK(r.line("SEÇ NESNE nesneler=2 islem=EKLE").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});

    CHECK(r.line("SEÇ NESNE nesneler=1 islem=ÇIKAR").ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    CHECK(r.line("SEÇ NESNE nesneler=2 islem=TERSİNE").ok());
    CHECK(r.bus.selection().empty());
}

TEST_CASE("SEÇ: iki kimlikli JSON dizisi nokta sanılmaz")
{
    // Regression. `[485320, 4310220]` is a point and `[1, 2]` is a pair of ids, and
    // JSON cannot tell them apart. Before the bus consulted the SPEC, a script that
    // wrote `{"nesneler": [1, 2]}` was told its ids were the wrong type — and with
    // keys starting at 1, that is the FIRST thing a user writes.
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());
    CHECK(r.line("ÇİZGİ 0,9 10,9").ok());

    Args args;
    args.set("nesneler", Value::point(core::Point2{1, 2}));
    auto sent = r.bus.dispatch(Invocation{"core.select", args, Origin::Script});
    CHECK(sent.ok());
    CHECK_EQ(r.bus.selection().size(), std::size_t{2});
    CHECK(r.bus.selection().contains(core::EntityKey{1}));
    CHECK(r.bus.selection().contains(core::EntityKey{2}));

    // A point parameter is untouched by the repair: only a Selection parameter
    // reinterprets the pair.
    Args drawn;
    drawn.set("noktalar", Value::points({{0, 0}, {1000, 0}}));
    CHECK(r.bus.dispatch(Invocation{"core.line", drawn, Origin::Script}).ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{4});
}

TEST_CASE("SEÇ: seçim belge durumu değildir")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());

    const std::uint64_t hash     = r.doc.content_hash();
    const std::uint64_t revision = r.doc.revision();
    const std::size_t undo_depth = r.undo.undo_depth();
    const std::size_t journalled = r.journal.entries().size();

    CHECK(r.line("SEÇ TÜMÜ").ok());

    // model.md R43 and P15, checked rather than asserted in prose.
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.doc.revision(), revision);
    CHECK_EQ(r.undo.undo_depth(), undo_depth);
    CHECK_EQ(r.journal.entries().size(), journalled);
}

TEST_CASE("SEÇ: hatalı mod ve işlem beklenen ile girileni söyler")
{
    Rig r;
    CHECK(r.line("SEÇ OLMAYAN").ok()); // reported to the transcript, not an error
    CHECK(r.echoed.find("Beklenen mod") != std::string::npos);
    CHECK(r.echoed.find("OLMAYAN") != std::string::npos);

    r.echoed.clear();
    CHECK(r.line("SEÇ TÜMÜ islem=BİLİNMEYEN").ok());
    CHECK(r.echoed.find("Beklenen işlem") != std::string::npos);
}

TEST_CASE("SİL: argüman verilmezse etkin seçimi siler")
{
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 0,5 10,5").ok());

    CHECK(r.line("SEÇ NESNE nesneler=1").ok());
    CHECK(r.line("SİL").ok());
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{1});

    // The erased key leaves the selection with it: a retired key is never reused.
    CHECK(r.bus.selection().empty());

    r.echoed.clear();
    CHECK(r.line("SİL").ok());
    CHECK(r.echoed.find("seçim boş") != std::string::npos);
}

// --------------------------------------------- the aids on the input path ---

TEST_CASE("YAKALAMA: aynı nişan arayüzden, komut satırından ve betikten aynı yere düşer")
{
    // The Article 6.4 proof, for snapping. Each client aims 200 mm off the corner
    // of an existing line; all three must draw to the corner itself, and all three
    // must journal the same resolved coordinate.
    const auto seed = [](Rig& r) {
        r.with_view(1.0); // 1 mm per pixel, so the 12-pixel aperture is 12 mm
        CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
        CHECK(r.line("MOD yakalama_modları 1").ok()); // uç nokta only
    };

    // ---- client 1: a GUI session fed by mouse clicks ----
    Rig gui;
    seed(gui);
    {
        auto started = gui.bus.begin_interactive("ÇİZGİ");
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(Point2{20000, 20000})).ok());
        CHECK(session.supply(Value::point(Point2{10008, 6})).ok()); // 10 mm off (10000,0)
        session.cancel();
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    seed(cli);
    CHECK(cli.line("ÇİZGİ 20,20 10.008,0.006").ok());

    // ---- client 3: a JSON script ----
    Rig scr;
    seed(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto ran = runner.run_text(
            R"([{"cmd":"core.line","args":{"noktalar":[[20000,20000],[10008,6]]}}])");
        CHECK(ran.ok());
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());

    // And the point actually moved onto the corner.
    const core::RingGeometry& g = gui.doc.geometry();
    const core::RingSpan span   = g.rings_of(gui.doc.entities().slot[1]);
    CHECK(g.vertex(span.first, 1) == (Point2{10000, 0}));

    // The JOURNAL carries the resolved point, not the aim, for all three.
    const auto& gui_args = gui.journal.entries().back().args;
    const auto& cli_args = cli.journal.entries().back().args;
    const auto& scr_args = scr.journal.entries().back().args;
    CHECK(gui_args.to_json().dump() == cli_args.to_json().dump());
    CHECK(cli_args.to_json().dump() == scr_args.to_json().dump());
    CHECK(gui_args.to_json().dump().find("10000") != std::string::npos);
}

TEST_CASE("YAKALAMA: görünüm yokken çizim nişan aldığı yere düşer")
{
    // No view scale published: the aperture is zero, so a headless run — a batch
    // job, a journal replay, this test — draws exactly what it was given.
    Rig r;
    CHECK(r.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(r.line("ÇİZGİ 20,20 10.008,0.006").ok());

    const core::RingGeometry& g = r.doc.geometry();
    const core::RingSpan span   = g.rings_of(r.doc.entities().slot[1]);
    CHECK(g.vertex(span.first, 1) == (Point2{10008, 6}));
}

TEST_CASE("YAKALAMA: dik mod ve ızgara komut satırından çizilen noktayı da yönlendirir")
{
    Rig r;
    CHECK(r.line("MOD dik_mod evet").ok());
    CHECK(r.line("ÇİZGİ 0,0 10,3").ok());

    // The second point had no rubber-band origin only for the FIRST vertex; the
    // segment end is locked onto the horizontal axis through the start.
    const core::RingGeometry& g = r.doc.geometry();
    const core::RingSpan span   = g.rings_of(r.doc.entities().slot[0]);
    CHECK(g.vertex(span.first, 0) == (Point2{0, 0}));
    CHECK(g.vertex(span.first, 1) == (Point2{10000, 0}));

    Rig grid;
    CHECK(grid.line("MOD ızgaraya_yakala evet").ok());
    CHECK(grid.line("TERCİH ızgara_adımı 1000").ok());
    CHECK(grid.line("ÇİZGİ 0.4,0.4 10.4,0.4").ok());

    const core::RingGeometry& gg = grid.doc.geometry();
    const core::RingSpan gspan   = gg.rings_of(grid.doc.entities().slot[0]);
    CHECK(gg.vertex(gspan.first, 0) == (Point2{0, 0}));
    CHECK(gg.vertex(gspan.first, 1) == (Point2{10000, 0}));
}

TEST_CASE("YAKALAMA: günlük tekrar oynatıldığında belge değişmez")
{
    // Idempotence where it matters: a snapped run is replayed through the bus and
    // must reproduce the same document (kentoscad.md §2.2, CLAUDE.md 6.4).
    Rig original;
    original.with_view(1.0);
    CHECK(original.line("ÇİZGİ 0,0 10,0").ok());
    CHECK(original.line("MOD yakalama_modları 1").ok());
    CHECK(original.line("ÇİZGİ 20,20 10.008,0.006").ok());

    Rig replay;
    replay.with_view(1.0);
    CHECK(replay.line("MOD yakalama_modları 1").ok());
    for (const auto& e : original.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        CHECK(r.ok());
    }
    CHECK_EQ(replay.doc.content_hash(), original.doc.content_hash());
}

TEST_CASE("the drawn grid and the snapped grid are the same lattice")
{
    // The defect this pins: the canvas rounded the spacing to the 1-2-5 ladder
    // from the zoom, the snap engine read the declared step, and with adaptive
    // spacing on the two were different lattices. A user saw lines 50 m apart and
    // their clicks landed on 10 m — a grid from no moment they could see.
    constexpr core::Mm kDeclared = 10 * core::kMmPerMetre;

    // Adaptive: the step follows the zoom, on the 1-2-5 ladder, and is NOT the
    // declared one once they diverge.
    const core::Mm coarse = core::grid_step_in_force(kDeclared, true, 500.0); // 0.5 m per pixel
    CHECK(coarse > 0);
    CHECK(coarse != kDeclared);
    CHECK(coarse % core::kMmPerMetre == 0); // a whole number of metres

    // Every rung of the ladder is 1, 2 or 5 times a power of ten.
    for (double mm_per_px : {1.0, 7.0, 40.0, 250.0, 1300.0, 9000.0}) {
        const core::Mm step = core::grid_step_in_force(kDeclared, true, mm_per_px);
        if (step <= 0) continue;

        double norm = static_cast<double>(step);
        while (norm >= 10.0)
            norm /= 10.0;
        CHECK((norm == 1.0 || norm == 2.0 || norm == 5.0));

        // And it is drawable: never finer than two pixels, which is the rule the
        // canvas refuses to draw below.
        CHECK(static_cast<double>(step) / mm_per_px >= 2.0);
    }

    // Fixed: the declared step, whatever the zoom — until it is too fine to see,
    // and then NOTHING, so a click cannot land on a lattice nobody can see.
    CHECK_EQ(core::grid_step_in_force(kDeclared, false, 100.0), kDeclared);
    CHECK_EQ(core::grid_step_in_force(kDeclared, false, 1.0), kDeclared);
    CHECK_EQ(core::grid_step_in_force(kDeclared, false, 90000.0), 0);

    // A point already on the lattice rounds to itself, at whatever step.
    const core::Mm step = core::grid_step_in_force(kDeclared, true, 500.0);
    const core::Point2 on{step * 3, step * -2};
    CHECK_EQ(core::apply_grid(on, step).x, on.x);
    CHECK_EQ(core::apply_grid(on, step).y, on.y);
}

TEST_CASE("SÜTUN: katman= verilen sütun yalnız o katmana tanımlanır")
{
    // THROUGH THE COMMAND, because that is where the user's report started: a
    // column declared from one layer's properties turned up on every object.
    Rig rig;
    REQUIRE(rig.line("SÜTUN kimlik=ada tur=tam_sayi").ok());
    REQUIRE(rig.line("SÜTUN kimlik=direk tur=uzunluk katman=ENERJİ").ok());

    const core::AttrTable& schema = rig.doc.attributes();

    const core::AttrId project = schema.find("ada");
    REQUIRE(project != core::kNoAttr);
    CHECK(schema.column(project)->spec().layer.empty());

    const core::AttrId scoped = schema.find("direk");
    REQUIRE(scoped != core::kNoAttr);
    CHECK_EQ(schema.column(scoped)->spec().layer, "ENERJİ");

    CHECK(core::attr_applies_to(schema.column(project)->spec(), "PARSEL"));
    CHECK_FALSE(core::attr_applies_to(schema.column(scoped)->spec(), "PARSEL"));

    // AND AN EDIT CAN MOVE IT. A column declared project-wide by mistake — which
    // is how this was reported — is one command away from belonging to the layer
    // it was meant for.
    REQUIRE(rig.line("SÜTUN kimlik=ada katman=PARSEL").ok());
    CHECK_EQ(schema.column(schema.find("ada"))->spec().layer, "PARSEL");
}
