// SPDX-License-Identifier: GPL-3.0-or-later
// The terrain model: contours traced from a levelling survey.
//
// The assertions are the two a surveyor checks on a contour set: are the LEVELS
// the round numbers a plan sheet prints, and does a line stay at its own height
// all the way along.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/domain/surface/contour.hpp"

#include <string>
#include <vector>

using namespace kentos;
using command::Origin;

namespace {

struct Rig
{
    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    std::string said;

    Rig()
    {
        command::register_builtin_commands(reg);
        domain::surface::register_surface_commands(reg);
        bus.on_echo = [this](std::string_view t) { said += std::string(t); };
    }
};

/// A square site sloping evenly from 100 m to 110 m along the easting.
std::vector<domain::surface::Level> ramp()
{
    std::vector<domain::surface::Level> out;
    for (int i = 0; i <= 10; ++i)
        for (int j = 0; j <= 10; ++j)
            out.push_back(
                domain::surface::Level{core::Point2{i * 10000, j * 10000}, 100000 + i * 1000});
    return out;
}

} // namespace

TEST_CASE("EŞYÜKSELTİ: kotlar aralığın tam katlarına düşer")
{
    if (!domain::surface::available()) return;

    auto traced = domain::surface::trace_contours(ramp(), 1000); // 1 m
    REQUIRE(traced.ok());
    REQUIRE(!traced.value().empty());

    // A plan sheet prints 100, 101, 102 — not "the lowest point plus one metre".
    for (const domain::surface::Contour& c : traced.value())
        CHECK(c.height % 1000 == 0);
}

TEST_CASE("EŞYÜKSELTİ: eğriler kendi kotlarında kalır ve zincirlenir")
{
    if (!domain::surface::available()) return;

    auto traced = domain::surface::trace_contours(ramp(), 2000); // 2 m
    REQUIRE(traced.ok());
    REQUIRE(!traced.value().empty());

    // EVERY VERTEX IS WHERE ITS LEVEL IS. The site rises 1 m per 10 m along the
    // easting, so a contour at height h can only be the line x = (h - 100 m) x 10.
    // This is the property that matters: a chained run that wandered off its own
    // level would draw a contour through ground at another height.
    for (const domain::surface::Contour& c : traced.value()) {
        REQUIRE(c.path.size() >= 2);
        const core::Mm expected = (c.height - 100000) * 10;
        for (const core::Point2& p : c.path)
            CHECK(p.x == expected);
    }
}

TEST_CASE("EŞYÜKSELTİ: düz arazide eğri yok")
{
    if (!domain::surface::available()) return;

    std::vector<domain::surface::Level> flat;
    for (int i = 0; i <= 3; ++i)
        for (int j = 0; j <= 3; ++j)
            flat.push_back(domain::surface::Level{core::Point2{i * 10000, j * 10000}, 100000});

    auto traced = domain::surface::trace_contours(flat, 1000);
    REQUIRE(traced.ok());
    // A dead-flat site has one contour at most and no crossing anywhere; saying
    // "no lines" is the honest answer rather than drawing the whole boundary.
    CHECK(traced.value().empty());
}

TEST_CASE("EŞYÜKSELTİ: aynı yerdeki iki nokta üçgenlemeyi bozmaz")
{
    if (!domain::surface::available()) return;

    // A station occupied twice is an ordinary field mistake.
    std::vector<domain::surface::Level> points = ramp();
    points.push_back(points.front());
    points.push_back(points[5]);

    auto traced = domain::surface::trace_contours(points, 1000);
    CHECK(traced.ok());
}

TEST_CASE("EŞYÜKSELTİ: üç noktadan az ve sıfır aralık reddedilir")
{
    if (!domain::surface::available()) return;

    std::vector<domain::surface::Level> two{
        {core::Point2{0, 0}, 100000},
        {core::Point2{10000, 0}, 101000},
    };
    CHECK(!domain::surface::trace_contours(two, 1000).ok());
    CHECK(!domain::surface::trace_contours(ramp(), 0).ok());
}

TEST_CASE("EŞYÜKSELTİ komutu kotlu noktalardan eğri çizer")
{
    if (!domain::surface::available()) return;

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=NIRENGI", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=kot tur=uzunluk", Origin::Test).ok());

    // A small slope: nine points, 100 m to 102 m.
    int key = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const std::string line =
                "NOKTA noktalar=" + std::to_string(i * 10) + "," + std::to_string(j * 10);
            REQUIRE(r.bus.execute_line(line, Origin::Test).ok());
            ++key;
            REQUIRE(r.bus
                        .execute_line("ÖZNİTELİK ad=kot nesne=" + std::to_string(key) +
                                          " deger=" + std::to_string(100000 + i * 1000),
                                      Origin::Test)
                        .ok());
        }

    const std::size_t before = r.doc.live_entity_count();
    r.said.clear();

    auto drawn = r.bus.execute_line("EŞYÜKSELTİ aralik=1000", Origin::Test);
    if (!drawn) FAIL_WITH("EŞYÜKSELTİ", drawn.error().message);

    CHECK(r.doc.live_entity_count() > before);
    CHECK(r.said.find("eş yükselti eğrisi çizildi") != std::string::npos);

    // The lines land on their own layer so a plan sheet can style and switch them
    // as a set.
    bool found = false;
    for (const core::Layer& l : r.doc.layers())
        if (l.name == "ESYUKSELTI") found = true;
    CHECK(found);
}

TEST_CASE("EŞYÜKSELTİ: kotsuz nokta sıfır sayılmaz")
{
    if (!domain::surface::available()) return;

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=kot tur=uzunluk", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTA noktalar=0,0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTA noktalar=10,0", Origin::Test).ok());

    r.said.clear();
    // Two points, neither levelled: a sea-level point in the middle of a hillside
    // would drag every contour around it, so an unlevelled point is left out.
    REQUIRE(r.bus.execute_line("EŞYÜKSELTİ aralik=1000", Origin::Test).ok());
    CHECK(r.said.find("yetersiz") != std::string::npos);
}

TEST_CASE("EŞYÜKSELTİ tek geri alma adımıdır")
{
    if (!domain::surface::available()) return;

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=kot tur=uzunluk", Origin::Test).ok());

    int key = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            REQUIRE(r.bus
                        .execute_line("NOKTA noktalar=" + std::to_string(i * 10) + "," +
                                          std::to_string(j * 10),
                                      Origin::Test)
                        .ok());
            ++key;
            REQUIRE(r.bus
                        .execute_line("ÖZNİTELİK ad=kot nesne=" + std::to_string(key) +
                                          " deger=" + std::to_string(100000 + i * 1000),
                                      Origin::Test)
                        .ok());
        }

    const std::size_t before = r.doc.live_entity_count();
    REQUIRE(r.bus.execute_line("EŞYÜKSELTİ aralik=1000", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() > before);

    // A contour set is one step, however many lines it drew.
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(r.doc.live_entity_count() == before);
}

// =============================================================================
// HACİM — cut and fill
// =============================================================================

TEST_CASE("HACİM: düz bir platformun hacmi alan x yükseklik")
{
    if (!domain::surface::available()) return;

    // A flat 100 m x 100 m plateau at 110 m, measured against 100 m: exactly
    // 10 000 m² x 10 m = 100 000 m³ of cut and no fill.
    std::vector<domain::surface::Level> flat;
    for (int i = 0; i <= 10; ++i)
        for (int j = 0; j <= 10; ++j)
            flat.push_back(domain::surface::Level{core::Point2{i * 10000, j * 10000}, 110000});

    auto work = domain::surface::earthwork(flat, 100000);
    REQUIRE(work.ok());

    CHECK(work.value().fill == 0);
    CHECK(work.value().area == 10'000'000'000LL); // 10 000 m² = 1e10 mm²

    // 100 000 m³ = 1e14 mm³, within a thousandth.
    const core::Mm3 expected = 100'000'000'000'000LL;
    const core::Mm3 got      = work.value().cut;
    CHECK(got > expected - expected / 1000);
    CHECK(got < expected + expected / 1000);
}

TEST_CASE("HACİM: kazı ve dolgu ayrı raporlanır, birbirini yemez")
{
    if (!domain::surface::available()) return;

    // A ramp from 90 m to 110 m across the site, measured against 100 m: half of
    // it is above and half below, and the two must come back as two numbers.
    // Netting them would report a balanced site as no work at all — and the
    // machines are hired against the separate figures.
    std::vector<domain::surface::Level> ramp;
    for (int i = 0; i <= 10; ++i)
        for (int j = 0; j <= 10; ++j)
            ramp.push_back(
                domain::surface::Level{core::Point2{i * 10000, j * 10000}, 90000 + i * 2000});

    auto work = domain::surface::earthwork(ramp, 100000);
    REQUIRE(work.ok());

    CHECK(work.value().cut > 0);
    CHECK(work.value().fill > 0);

    // Symmetric ramp: the two are equal to within a rounding, and the net is ~0.
    const core::Mm3 difference = work.value().cut - work.value().fill;
    const core::Mm3 bound      = work.value().cut / 100;
    CHECK(difference < bound);
    CHECK(difference > -bound);
}

TEST_CASE("HACİM: kotun tam üstündeki yüzey sıfır verir")
{
    if (!domain::surface::available()) return;

    std::vector<domain::surface::Level> flat;
    for (int i = 0; i <= 4; ++i)
        for (int j = 0; j <= 4; ++j)
            flat.push_back(domain::surface::Level{core::Point2{i * 10000, j * 10000}, 100000});

    auto work = domain::surface::earthwork(flat, 100000);
    REQUIRE(work.ok());
    CHECK(work.value().cut == 0);
    CHECK(work.value().fill == 0);
    CHECK(work.value().area > 0); // the area was still measured
}

TEST_CASE("HACİM komutu kazıyı ve dolguyu ayrı satırlarda yazar")
{
    if (!domain::surface::available()) return;

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=kot tur=uzunluk", Origin::Test).ok());

    int key = 0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            REQUIRE(r.bus
                        .execute_line("NOKTA noktalar=" + std::to_string(i * 10) + "," +
                                          std::to_string(j * 10),
                                      Origin::Test)
                        .ok());
            ++key;
            REQUIRE(r.bus
                        .execute_line("ÖZNİTELİK ad=kot nesne=" + std::to_string(key) +
                                          " deger=" + std::to_string(100000 + i * 2000),
                                      Origin::Test)
                        .ok());
        }

    r.said.clear();
    const std::uint64_t before = r.doc.content_hash();
    auto done                  = r.bus.execute_line("HACİM kot=103000", Origin::Test);
    if (!done) FAIL_WITH("HACİM", done.error().message);

    CHECK(r.said.find("kazı") != std::string::npos);
    CHECK(r.said.find("dolgu") != std::string::npos);
    CHECK(r.said.find("hesap alanı") != std::string::npos);

    // Read-only: a report is not an edit.
    CHECK(r.doc.content_hash() == before);
}
