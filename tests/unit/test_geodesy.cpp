// SPDX-License-Identifier: GPL-3.0-or-later
//
// piricad.md §11, Phase 0 asks for a comparison of PROJ's TUREF accuracy against
// TKGM reference data — comparing PROJ's TUREF accuracy against TKGM
// reference data — and §12's opening requirement.
//
// The thing being tested is not PROJ's mathematics — PROJ is correct and has been
// for thirty years. What is tested is the wrapper's ONE job: axis order. EPSG:5254
// declares northing first; PiriCAD stores easting first. A wrapper that gets this
// wrong returns a coordinate that is plausible and wrong, which is the worst
// failure this product has (.claude/model.md R37a).
#include "piricad_test.hpp"

#include "piricad/command/registry.hpp"

#include "piricad/command/bus.hpp"

#include "piricad/domain/geodesy/commands.hpp"
#include "piricad/domain/geodesy/helmert.hpp"
#include "piricad/domain/geodesy/crs_service.hpp"

#include "piricad/domain/geodesy/crs_catalog.hpp"
#include "piricad/domain/geodesy/transform.hpp"

#include <cmath>
#include <vector>

using namespace piricad;
using namespace piricad::domain::geodesy;
using piricad::core::Point2;

namespace {

/// A parcel corner in the 30th zone. Easting first, as PiriCAD stores it.
constexpr Point2 kUsak{485320150, 4310220400};

bool near_deg(double a, double b, double tolerance = 1e-6)
{
    return std::abs(a - b) < tolerance;
}

} // namespace

TEST_CASE("KATALOG: TM 3 derece dilimleri veriden okunuyor")
{
    auto loaded = CrsCatalog::load(PIRICAD_DATA_DIR "/crs");
    CHECK(loaded.ok());
    if (!loaded.ok()) return;

    const CrsCatalog& cat = loaded.value();

    // Seven zones, 27..45 in threes — the BÖHHBÜY table, loaded not compiled.
    CHECK_EQ(cat.zones().size(), std::size_t{7});
    CHECK_EQ(cat.zones().front().central_meridian, 27);
    CHECK_EQ(cat.zones().back().central_meridian, 45);

    CHECK_EQ(cat.parameters().scale_factor, 1.0);
    CHECK_EQ(cat.parameters().false_easting_m, 500000L);
    CHECK_EQ(cat.parameters().false_northing_m, 0L);

    // Provenance is required, not decorative: domain.md R23 makes an error message
    // cite the catalogue version.
    CHECK(!cat.source().empty());
    CHECK(!cat.published().empty());
    CHECK(!cat.package_version().empty());
    CHECK(cat.source().find("BÖHHBÜY") != std::string::npos);
}

TEST_CASE("KATALOG: dilim boylamdan, EPSG'den ve addan bulunuyor")
{
    auto loaded = CrsCatalog::load(PIRICAD_DATA_DIR "/crs");
    CHECK(loaded.ok());
    if (!loaded.ok()) return;
    const CrsCatalog& cat = loaded.value();

    const Tm3Zone* tm30 = cat.zone_by_epsg(5254);
    CHECK(tm30 != nullptr);
    if (tm30) {
        CHECK_EQ(tm30->central_meridian, 30);
        CHECK_EQ(tm30->name, std::string("TM30"));
    }

    CHECK(cat.zone_by_name("tm33") != nullptr); // Turkish-folded lookup
    CHECK(cat.zone_by_name("TM33") != nullptr);
    CHECK(cat.zone_by_name("TM31") == nullptr);

    // 29.83 E is inside the 30-degree zone; 20 E is in no Turkish zone at all,
    // and guessing would hand back a coordinate with an unstated error.
    const Tm3Zone* z = cat.zone_for_longitude(29.830716);
    CHECK(z != nullptr);
    if (z) CHECK_EQ(z->central_meridian, 30);

    CHECK(cat.zone_for_longitude(20.0) == nullptr);
    CHECK(cat.zone_for_longitude(50.0) == nullptr);

    // The zones are three degrees apart and each reaches 1.5 degrees either side,
    // so they tile continuously from 25.5 E to 46.5 E with no gap. Only a longitude
    // outside that span has no zone.
    CHECK(cat.zone_for_longitude(31.5) != nullptr); // exactly on a boundary
    CHECK(cat.zone_for_longitude(31.6) != nullptr); // inside TM33's half-width
    CHECK(cat.zone_for_longitude(25.4) == nullptr); // west of TM27's reach
    CHECK(cat.zone_for_longitude(46.6) == nullptr); // east of TM45's reach
}

TEST_CASE("DÖNÜŞÜM: eksen sırası doğru — nokta Türkiye'ye düşüyor")
{
    if (!Transform::available()) return; // PROJ kapalıysa sessizce atla

    auto tf = Transform::between("EPSG:5254", "EPSG:4326");
    CHECK(tf.ok());
    if (!tf.ok()) return;

    double easting  = core::mm_to_metres(kUsak.x);
    double northing = core::mm_to_metres(kUsak.y);
    CHECK(tf.value().forward(easting, northing));

    // After the transform the pair is (longitude, latitude) — still easting-first,
    // because the wrapper normalised the axis order. Turkey is 26..45 E, 36..42 N.
    // If the wrapper had NOT normalised, EPSG:5254 would have read 485320 as a
    // northing and this point would land near 62 N, 3 E — in the North Sea.
    CHECK(easting > 26.0);
    CHECK(easting < 45.0);
    CHECK(northing > 36.0);
    CHECK(northing < 42.0);

    CHECK(near_deg(easting, 29.830716));
    CHECK(near_deg(northing, 38.925260));
}

TEST_CASE("DÖNÜŞÜM: coğrafi hedefte milimetre API'si reddediyor")
{
    if (!Transform::available()) return;

    auto tf = Transform::between("EPSG:5254", "EPSG:4326");
    CHECK(tf.ok());
    if (!tf.ok()) return;

    CHECK(tf.value().target_is_angular());
    CHECK(!tf.value().projected_both_ways());

    // Rounding 29.830716 degrees to the nearest millimetre moves the point about
    // a hundred metres. Refusing loudly is the only acceptable behaviour.
    std::vector<Point2> p{kUsak};
    auto st = tf.value().forward(p);
    CHECK(!st.ok());
    if (!st.ok()) CHECK(st.error().message.find("coğrafi") != std::string::npos);

    // The scalar path is the sanctioned one for degrees, and still works.
    double easting  = core::mm_to_metres(kUsak.x);
    double northing = core::mm_to_metres(kUsak.y);
    CHECK(tf.value().forward(easting, northing));
}

TEST_CASE("DÖNÜŞÜM: projeksiyonlar arası gidiş-dönüş milimetreyi koruyor")
{
    if (!Transform::available()) return;

    // Projected to projected — TM30 to TM33 — is what a document transform
    // actually is, and there millimetres are the right unit on both sides.
    auto tf = Transform::between("EPSG:5254", "EPSG:5255");
    CHECK(tf.ok());
    if (!tf.ok()) return;

    CHECK(tf.value().projected_both_ways());

    // Every zone, at a realistic parcel coordinate for that zone.
    std::vector<Point2> points;
    for (int zone = 0; zone < 7; ++zone)
        points.push_back(Point2{485320150 + zone * 1000, 4310220400 + zone * 1000});

    const std::vector<Point2> before = points;

    CHECK(tf.value().forward(points).ok());
    CHECK(tf.value().inverse(points).ok());

    // The round trip must not move a point by more than the storage unit itself.
    // Measured PROJ error here is nanometres; one millimetre is the honest bound.
    for (std::size_t i = 0; i < points.size(); ++i) {
        CHECK(std::abs(points[i].x - before[i].x) <= 1);
        CHECK(std::abs(points[i].y - before[i].y) <= 1);
    }
}

TEST_CASE("DÖNÜŞÜM: dilimler arası TM30 -> TM33")
{
    if (!Transform::available()) return;

    auto tf = Transform::between("EPSG:5254", "EPSG:5255");
    CHECK(tf.ok());
    if (!tf.ok()) return;

    std::vector<Point2> p{kUsak};
    CHECK(tf.value().forward(p).ok());

    // The point sits ~3.17 degrees WEST of TM33's central meridian, so its easting
    // must fall well below the 500 000 m false easting. A wrapper with the axes
    // crossed produces a number in the millions here.
    CHECK(p[0].x > 200000000); // 200 km
    CHECK(p[0].x < 300000000); // 300 km
    CHECK(p[0].y > 4300000000);
    CHECK(p[0].y < 4330000000);
}

TEST_CASE("DÖNÜŞÜM: PROJ yokken sessizce birim dönüşüm yapmıyor")
{
    // The failure mode this guards: a transform that quietly does nothing produces
    // a document whose coordinates are in the wrong system and look right (§12).
    auto bad = Transform::between("EPSG:5254", "EPSG:BOYLE-BIR-SEY-YOK");
    CHECK(!bad.ok());
    if (!bad.ok()) CHECK(!bad.error().message.empty());

    if (!Transform::available()) {
        auto any = Transform::between("EPSG:5254", "EPSG:4326");
        CHECK(!any.ok());
        if (!any.ok()) CHECK(any.error().message.find("PIRICAD_WITH_PROJ") != std::string::npos);
    }
}

// ------------------------------------------------------------ CRS resolver ----

TEST_CASE("CRS: kimlik çözülür ve belge tek doğruyu taşır")
{
    // The conflict this closes. `Document::crs()` and the `core.crs.id` setting
    // were two stores for one fact, and nothing wrote the first: `AYAR
    // koordinat_sistemi` moved the setting while the document stayed at its
    // constructed default forever. A drawing therefore reported one CRS to the
    // exporter and another to its own file — which is the field blunder model.md
    // R36 is written against.
    auto catalogue = domain::geodesy::CrsCatalog::load(std::string(PIRICAD_DATA_DIR) + "/crs");
    REQUIRE(catalogue.ok());

    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    command::register_builtin_commands(reg);

    domain::geodesy::CrsService service(bus, std::move(catalogue.value()));

    // Every spelling a user or a file might actually produce resolves to the same
    // zone. Refusing three of the four would push a mistake onto somebody who did
    // not make one.
    for (const char* id : {"TUREF/TM30", "TM30", "EPSG:5254", "5254"}) {
        const core::Crs crs = service.resolve(id);
        CHECK_EQ(crs.epsg(), 5254);
        CHECK_EQ(crs.central_meridian_deg(), 30);
        if (crs.epsg() != 5254) FAIL_WITH("çözülemedi", id);
    }

    // An unknown id keeps its name and reports itself unresolved. NOT a fallback:
    // a CRS guessed wrong moves every coordinate by kilometres while the numbers
    // still look like Turkish coordinates.
    const core::Crs unknown = service.resolve("BÖYLE-BİR-SİSTEM-YOK");
    CHECK(!unknown.resolved());
    CHECK_EQ(unknown.id(), std::string("BÖYLE-BİR-SİSTEM-YOK"));

    // And the write path: AYAR moves the DOCUMENT, not only the setting.
    REQUIRE(bus.execute_line("AYAR koordinat_sistemi TUREF/TM33", command::Origin::Test).ok());
    CHECK_EQ(doc.crs().id(), std::string("TUREF/TM33"));
    CHECK_EQ(doc.crs().epsg(), 5255);
    CHECK_EQ(doc.crs().central_meridian_deg(), 33);

    // The CRS is document content, so changing it changes the fingerprint.
    const std::uint64_t after33 = doc.content_hash();
    REQUIRE(bus.execute_line("AYAR koordinat_sistemi TUREF/TM30", command::Origin::Test).ok());
    CHECK(doc.content_hash() != after33);

    // And it is undoable, because it is a document change like any other.
    REQUIRE(bus.execute_line("GERİAL", command::Origin::Test).ok());
    CHECK_EQ(doc.crs().id(), std::string("TUREF/TM33"));
    CHECK_EQ(doc.crs().epsg(), 5255); // the resolved metadata came back too
}

// =============================================================================
// Helmert 2D — fitting a local survey onto the map (OTURT's arithmetic)
// =============================================================================

TEST_CASE("HELMERT: iki nokta tam çözüm verir, artık bırakmaz")
{
    using namespace piricad::domain::geodesy;

    // A local survey rotated a quarter turn and moved onto TUREF/TM36.
    std::vector<ControlPoint> control{
        {{0, 0}, {485300000, 4310200000}},
        {{10000, 0}, {485300000, 4310210000}},
    };

    auto fit = fit_helmert(control, false);
    REQUIRE(fit.ok());

    // Exact through both points: two points and four parameters leave nothing
    // over, so a residual here would mean the arithmetic is wrong.
    CHECK(fit.value().rms == 0);
    CHECK(fit.value().worst == 0);

    // Scale 1 and a quarter turn: the local x axis became the map's north.
    CHECK(std::abs(fit.value().scale - 1.0) < 1e-9);
    CHECK(std::abs(fit.value().rotation_grad - 300.0) < 1e-6);
}

TEST_CASE("HELMERT: ölçeği bulur")
{
    using namespace piricad::domain::geodesy;

    // The same shape at twice the size, no rotation.
    std::vector<ControlPoint> control{
        {{0, 0}, {0, 0}},
        {{1000, 0}, {2000, 0}},
    };

    auto fit = fit_helmert(control, false);
    REQUIRE(fit.ok());
    CHECK(std::abs(fit.value().scale - 2.0) < 1e-9);

    // Locked, the same input must NOT rescale — a calibrated tape's distances
    // are measured data, and absorbing a control error into every length is the
    // failure this option exists to prevent.
    auto locked = fit_helmert(control, true);
    REQUIRE(locked.ok());
    CHECK(std::abs(locked.value().scale - 1.0) < 1e-12);
    CHECK(locked.value().rms > 0); // and it says so, in the residuals
}

TEST_CASE("HELMERT: üç noktada artıkları ve RMS'i raporlar")
{
    using namespace piricad::domain::geodesy;

    // Three points that cannot all be satisfied: the third is 20 mm off the line
    // the first two define. A similarity cannot absorb that, and must not
    // pretend to.
    std::vector<ControlPoint> control{
        {{0, 0}, {0, 0}},
        {{10000, 0}, {10000, 0}},
        {{5000, 0}, {5000, 20}},
    };

    auto fit = fit_helmert(control, false);
    REQUIRE(fit.ok());

    CHECK(fit.value().residuals.size() == 3);
    CHECK(fit.value().rms > 0);
    CHECK(fit.value().worst > 0);
    CHECK(fit.value().worst <= 20);
}

TEST_CASE("HELMERT: bir nokta ve çakışık noktalar gerekçesiyle reddedilir")
{
    using namespace piricad::domain::geodesy;

    std::vector<ControlPoint> one{{{0, 0}, {100, 100}}};
    CHECK(!fit_helmert(one, false).ok());

    std::vector<ControlPoint> same{
        {{5000, 5000}, {0, 0}},
        {{5000, 5000}, {100, 100}},
    };
    CHECK(!fit_helmert(same, false).ok());
}

TEST_CASE("HELMERT: dönüşüm her noktayı kontrolüne taşır")
{
    using namespace piricad::domain::geodesy;

    std::vector<ControlPoint> control{
        {{0, 0}, {485300000, 4310200000}},
        {{100000, 0}, {485400000, 4310200000}},
    };

    auto fit = fit_helmert(control, false);
    REQUIRE(fit.ok());

    for (const ControlPoint& p : control) {
        const piricad::core::Point2 landed = fit.value().apply(p.local);
        CHECK(landed.x == p.map.x);
        CHECK(landed.y == p.map.y);
    }
}

TEST_CASE("OTURT: yerel çizimi kontrol noktalarıyla haritaya taşır")
{
    // A survey measured from a station the crew called 0,0. Two published points
    // put it on TUREF/TM36 — the job this command exists for.
    piricad::core::Document doc;
    piricad::command::Registry reg;
    piricad::command::Journal journal;
    piricad::command::UndoStack undo;
    piricad::command::Bus bus{doc, reg, journal, undo};
    piricad::command::register_builtin_commands(reg);
    piricad::domain::geodesy::register_geodesy_commands(reg);

    using piricad::command::Origin;
    REQUIRE(bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());

    const std::size_t before = doc.live_entity_count();

    // local 0,0 -> map 485300,4310200 and local 10,0 -> map 485310,4310200:
    // a pure translation, no rotation, no scale.
    auto fitted = bus.execute_line(
        "OTURT noktalar=0,0 485300,4310200 10,0 485310,4310200 sistem=TUREF/TM36", Origin::Test);
    if (!fitted) FAIL_WITH("OTURT", fitted.error().message);

    // Nothing was added or removed — the drawing MOVED.
    CHECK(doc.live_entity_count() == before);

    const piricad::core::Box2 box = doc.extent();
    CHECK(box.min_x == 485300000);
    CHECK(box.min_y == 4310200000);
    CHECK(doc.crs().id() == "TUREF/TM36");
}

TEST_CASE("OTURT tek geri alma adımıdır: ya hepsi taşınır ya hiçbiri")
{
    piricad::core::Document doc;
    piricad::command::Registry reg;
    piricad::command::Journal journal;
    piricad::command::UndoStack undo;
    piricad::command::Bus bus{doc, reg, journal, undo};
    piricad::command::register_builtin_commands(reg);
    piricad::domain::geodesy::register_geodesy_commands(reg);

    using piricad::command::Origin;
    REQUIRE(bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(bus.execute_line("ÇİZGİ noktalar=0,0 5,5", Origin::Test).ok());

    const std::uint64_t before = doc.content_hash();

    REQUIRE(bus.execute_line("OTURT noktalar=0,0 100,100 10,0 110,100", Origin::Test).ok());
    CHECK(doc.content_hash() != before);

    // A half-transformed cadastral sheet is the failure Article 1.6 names, and
    // both halves would look plausible. One undo must put every vertex back.
    REQUIRE(bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(doc.content_hash() == before);
}

TEST_CASE("OTURT: eksik ya da tek sayıda nokta gerekçesiyle reddedilir")
{
    piricad::core::Document doc;
    piricad::command::Registry reg;
    piricad::command::Journal journal;
    piricad::command::UndoStack undo;
    piricad::command::Bus bus{doc, reg, journal, undo};
    piricad::command::register_builtin_commands(reg);
    piricad::domain::geodesy::register_geodesy_commands(reg);

    using piricad::command::Origin;
    std::string said;
    bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    const std::uint64_t before = doc.content_hash();

    // One pair is a translation with an unknown rotation; the parser's own arity
    // floor of four points catches it before the body runs.
    (void)bus.execute_line("OTURT noktalar=0,0 100,100", Origin::Test);
    CHECK(doc.content_hash() == before);
}

TEST_CASE("OTURT ölçeği kilitlenebilir: saha ölçüsü yeniden ölçeklenmez")
{
    piricad::core::Document doc;
    piricad::command::Registry reg;
    piricad::command::Journal journal;
    piricad::command::UndoStack undo;
    piricad::command::Bus bus{doc, reg, journal, undo};
    piricad::command::register_builtin_commands(reg);
    piricad::domain::geodesy::register_geodesy_commands(reg);

    using piricad::command::Origin;
    REQUIRE(bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(bus.execute_line("ÇİZGİ noktalar=0,0 100,0", Origin::Test).ok());

    // The control asks for double the size. With the scale locked the line must
    // keep its measured 100 m length.
    REQUIRE(bus.execute_line("OTURT noktalar=0,0 0,0 100,0 200,0 olcek_kilitli=evet", Origin::Test)
                .ok());

    const piricad::core::Box2 box = doc.extent();
    CHECK(box.max_x - box.min_x == 100000);
}
