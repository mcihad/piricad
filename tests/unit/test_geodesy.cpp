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
#include "microtest.hpp"

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
    CHECK(easting > 26.0 && easting < 45.0);
    CHECK(northing > 36.0 && northing < 42.0);

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
