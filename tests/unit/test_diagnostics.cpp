// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: the import diagnostics record and the drawing unit.
//
// io.md P11/P13: a loss is reported, never silent. These cases lock the SHAPE of
// that report — the cap that keeps a broken file from printing ten thousand
// lines, the prefixes a user reads, the census that survives the cap — and the
// one place a file's numbers are scaled to millimetres.
#include "kentos_test.hpp"

#include "kentos_cad/core/units.hpp"
#include "kentos_cad/io/diagnostics.hpp"

#include <string>

using namespace kentos;

TEST_CASE("TANI: not başlığı sekizde durur ve kalanı sayar")
{
    io::ImportDiagnostics diag;
    for (int i = 0; i < 10; ++i)
        diag.note(io::Severity::Info, "not " + std::to_string(i));

    CHECK_EQ(diag.notes.size(), io::ImportDiagnostics::kMaxNotes);
    CHECK_EQ(diag.dropped_notes, 2u);

    // The first eight are kept in emission order; the tail says what was dropped.
    CHECK_EQ(diag.notes.front().text, "not 0");
    CHECK_EQ(diag.notes.back().text, "not 7");
    const std::string said = diag.transcript();
    CHECK(said.find("\n  not: not 7") != std::string::npos);
    CHECK(said.find("not 8") == std::string::npos);
    CHECK(said.find("… ve 2 not daha.") != std::string::npos);
}

TEST_CASE("TANI: seviye ön ekleri ayrı yazılır, sıralama en ciddiden başlar")
{
    io::ImportDiagnostics diag;
    diag.note(io::Severity::Info, "bir bilgi");
    diag.note(io::Severity::Skipped, "bir atlama");
    diag.note(io::Severity::Warning, "bir uyarı");

    // The transcript keeps emission order, each line with its own prefix.
    const std::string said = diag.transcript();
    const auto info        = said.find("\n  not: bir bilgi");
    const auto skipped     = said.find("\n  atlandı: bir atlama");
    const auto warning     = said.find("\n  uyarı: bir uyarı");
    REQUIRE(info != std::string::npos);
    REQUIRE(skipped != std::string::npos);
    REQUIRE(warning != std::string::npos);
    CHECK(info < skipped);
    CHECK(skipped < warning);

    // The wizard's list puts the warning first and the note last.
    const auto ordered = diag.ordered();
    REQUIRE_EQ(ordered.size(), 3u);
    CHECK_EQ(ordered[0].level, io::Severity::Warning);
    CHECK_EQ(ordered[1].level, io::Severity::Skipped);
    CHECK_EQ(ordered[2].level, io::Severity::Info);

    CHECK_EQ(std::string(io::severity_prefix(io::Severity::Degraded)), "düşürme");
    CHECK_EQ(std::string(io::severity_prefix(io::Severity::Error)), "hata");
}

TEST_CASE("TANI: tür sayımı adla saklanır, sayıca büyükten küçüğe yazılır")
{
    io::ImportDiagnostics diag;
    diag.tally("LINE", 3, 0, 0);
    diag.tally("ARC", 5, 0, 0);
    diag.tally("HATCH", 2, 0, 2);
    diag.tally("DIMENSION", 0, 1, 0);
    diag.tally("LINE", 1, 0, 0); // a second batch of the same type adds up

    // Stored sorted by name, so two runs of the same file agree byte for byte.
    REQUIRE_EQ(diag.types.size(), 4u);
    CHECK_EQ(diag.types[0].type, "ARC");
    CHECK_EQ(diag.types[1].type, "DIMENSION");
    CHECK_EQ(diag.types[2].type, "HATCH");
    CHECK_EQ(diag.types[3].type, "LINE");
    CHECK_EQ(diag.types[3].read, 4u);

    CHECK_EQ(diag.type_summary(),
             "Okunan türler: ARC 5, LINE 4, HATCH 2; parçalanan: HATCH 2; atlanan: DIMENSION 1");

    // The census is a field, so it is printed even when every note slot is taken.
    for (int i = 0; i < 20; ++i)
        diag.note(io::Severity::Info, "dolgu");
    CHECK(diag.transcript().find("Okunan türler: ARC 5") != std::string::npos);
}

TEST_CASE("TANI: birim satırı ayarı söyler, dosya başlığıyla karşılaştırır")
{
    // THE SETTING DECIDES; the header only gets to disagree out loud. A file that
    // names no unit is told which one was used, as a note and not a warning: the
    // user chose it.
    io::ImportDiagnostics silent_file;
    silent_file.unit        = core::DrawingUnit::Centimetre;
    silent_file.unit_source = io::UnitSource::Setting;
    const std::string said  = silent_file.transcript();
    CHECK(said.find("not: Dosya birim bildirmiyor; çizim santimetre olarak okundu (AYAR "
                    "çizim_birimi)") != std::string::npos);
    CHECK_FALSE(silent_file.empty());

    // A header that DISAGREES is the one warning, and it names the exact command
    // that reads the file the other way — the Susehri file: `$INSUNITS 4` over
    // numbers that are metres.
    io::ImportDiagnostics disagrees;
    disagrees.unit          = core::DrawingUnit::Metre;
    disagrees.unit_source   = io::UnitSource::Setting;
    disagrees.declared_unit = core::DrawingUnit::Millimetre;
    const std::string warn  = disagrees.transcript();
    CHECK(warn.find("uyarı: Çizim metre olarak okundu (AYAR çizim_birimi); dosya başlığı "
                    "milimetre diyor.") != std::string::npos);
    CHECK(warn.find("AYAR çizim_birimi milimetre deyin") != std::string::npos);

    // A header naming a unit the setting cannot be set to says so instead.
    io::ImportDiagnostics inches;
    inches.unit_source   = io::UnitSource::Setting;
    inches.declared_unit = core::DrawingUnit::Inch;
    CHECK(inches.transcript().find("bu sürüm o birimi çizim birimi olarak sunmuyor") !=
          std::string::npos);

    // Agreement on metres — the geodetic norm — says nothing at all.
    io::ImportDiagnostics plain;
    plain.unit_source   = io::UnitSource::Setting;
    plain.declared_unit = core::DrawingUnit::Metre;
    CHECK(plain.empty());
    CHECK(plain.transcript().empty());

    // Agreement on anything else is worth one note: the numbers were scaled.
    io::ImportDiagnostics agrees_cm;
    agrees_cm.unit          = core::DrawingUnit::Centimetre;
    agrees_cm.unit_source   = io::UnitSource::Setting;
    agrees_cm.declared_unit = core::DrawingUnit::Centimetre;
    CHECK(agrees_cm.transcript().find("not: Çizim santimetre olarak okundu (AYAR çizim_birimi); "
                                      "dosya başlığı da öyle diyor.") != std::string::npos);

    // A geodetic format has no unit question.
    io::ImportDiagnostics crs;
    crs.unit_source = io::UnitSource::Crs;
    CHECK(crs.empty());

    // The paper-space and skipped counts are fields too.
    io::ImportDiagnostics dropped;
    dropped.paper_space_skipped = 3;
    dropped.skipped             = 1;
    dropped.skipped_reason      = "tek noktalı halka";
    const std::string tail      = dropped.transcript();
    CHECK(tail.find("atlandı: 3 öğe kâğıt alanında") != std::string::npos);
    CHECK(tail.find("atlandı: 1 öğe geometrisi kullanılamadığı için atlandı. İlki: tek noktalı "
                    "halka") != std::string::npos);
}

TEST_CASE("TANI: merge iki okuyucunun sayımını ve notlarını toplar")
{
    io::ImportDiagnostics a;
    a.tally("LINE", 2, 0, 0);
    a.note(io::Severity::Info, "a");
    a.paper_space_skipped = 1;

    io::ImportDiagnostics b;
    b.tally("LINE", 3, 1, 0);
    b.note(io::Severity::Warning, "b");
    b.unit          = core::DrawingUnit::Millimetre;
    b.unit_source   = io::UnitSource::Setting;
    b.declared_unit = core::DrawingUnit::Centimetre;

    a.merge(b);
    REQUIRE_EQ(a.types.size(), 1u);
    CHECK_EQ(a.types[0].read, 5u);
    CHECK_EQ(a.types[0].skipped, 1u);
    CHECK_EQ(a.notes.size(), 2u);
    CHECK_EQ(a.paper_space_skipped, 1u);
    CHECK_EQ(a.unit, core::DrawingUnit::Millimetre);
    CHECK_EQ(a.unit_source, io::UnitSource::Setting);
    REQUIRE(a.declared_unit.has_value());
    CHECK_EQ(*a.declared_unit, core::DrawingUnit::Centimetre);
}

TEST_CASE("BİRİM: çizim birimi oranları kesin, yuvarlama tek yerden")
{
    using core::DrawingUnit;
    CHECK_EQ(core::mm_from_drawing_units(10.0, DrawingUnit::Inch), 254);
    CHECK_EQ(core::mm_from_drawing_units(1.5, DrawingUnit::Centimetre), 15);
    CHECK_EQ(core::mm_from_drawing_units(1.0, DrawingUnit::Inch), 25);      // 25.4 rounds down
    CHECK_EQ(core::mm_from_drawing_units(0.5, DrawingUnit::Millimetre), 1); // half away from zero
    CHECK_EQ(core::mm_from_drawing_units(-0.5, DrawingUnit::Millimetre), -1);
    CHECK_EQ(core::mm_from_drawing_units(1.0, DrawingUnit::Metre), 1000);
    CHECK_EQ(core::mm_from_drawing_units(2.0, DrawingUnit::Kilometre), 2000000);
    CHECK_EQ(core::mm_from_drawing_units(1500.0, DrawingUnit::Micron), 2); // 1.5 mm rounds up
    CHECK_EQ(core::mm_from_drawing_units(3.0, DrawingUnit::Foot), 914);    // 914.4

    CHECK_EQ(core::drawing_units_from_mm(254, DrawingUnit::Inch), 10.0);
    CHECK_EQ(core::drawing_units_from_mm(1000, DrawingUnit::Metre), 1.0);
    CHECK_EQ(core::drawing_units_from_mm(15, DrawingUnit::Centimetre), 1.5);

    // Metres through the drawing-unit path IS `mm_from_metres`, bit for bit, so
    // a GeoPackage read through it rounds exactly as it always did.
    for (const double metres : {422575.25, 4448118.45, -0.0005, 0.0005, 1.0e-7, 4503599627370.497})
        CHECK_EQ(core::mm_from_drawing_units(metres, DrawingUnit::Metre),
                 core::mm_from_metres(metres));

    // The setting's indices, and anything outside them is the setting's default.
    CHECK_EQ(core::drawing_unit_from_setting(0), DrawingUnit::Millimetre);
    CHECK_EQ(core::drawing_unit_from_setting(1), DrawingUnit::Centimetre);
    CHECK_EQ(core::drawing_unit_from_setting(2), DrawingUnit::Metre);
    CHECK_EQ(core::drawing_unit_from_setting(7), DrawingUnit::Metre);

    CHECK_EQ(std::string(core::drawing_unit_name(DrawingUnit::Centimetre)), "santimetre");
    CHECK_EQ(std::string(core::drawing_unit_name(DrawingUnit::Inch)), "inç");
}
