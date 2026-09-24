// SPDX-License-Identifier: GPL-3.0-or-later
//
// WHAT A DIMENSION MEASURES AND WHAT IT SAYS (TODOS C-10, 2nd stage).
//
// The unit, the decimals, the tolerance, the prefix and the suffix are how a
// measured figure is WRITTEN; a figure typed by hand is not a measured one and
// is never passed off as one; and a dimension's sizes are for a sheet scale,
// so another sheet can have them at the same size on paper. Every figure here
// is asserted as the exact string the sheet prints, because that string is
// what a licensed engineer signs.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <array>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::DimensionDef;
using core::DimensionType;
using core::DimTolerance;
using core::DrawingUnit;
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

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    core::EntityId slot(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    DimensionDef def(std::int64_t key) const
    {
        auto d = core::dimension_of(doc.geometry(), doc.entities().slot[slot(key)]);
        REQUIRE(d.ok());
        return d.value();
    }

    std::string caption(std::int64_t key) const
    {
        return std::string(doc.texts().text(doc.entities().slot[slot(key)]));
    }

    core::Mm height(std::int64_t key) const
    {
        return doc.texts().height(doc.entities().slot[slot(key)]);
    }
};

DimensionDef metres(std::int64_t mm)
{
    DimensionDef d;
    d.type        = DimensionType::Aligned;
    d.measurement = mm;
    return d;
}

} // namespace

// ------------------------------------------------------------------ model ----

TEST_CASE("ÖLÇÜ YAZISI: önek, sonek, tolerans, sınır ve birim ölçülen değerin çevresine yazılır")
{
    DimensionDef d = metres(12'500);
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("12,50"));

    d.prefix = "R";
    d.suffix = " m";
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("R12,50 m"));

    d.tolerance      = DimTolerance::Symmetric;
    d.tolerance_plus = d.tolerance_minus = 50;
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("R12,50±0,05 m"));

    d.tolerance       = DimTolerance::Deviation;
    d.tolerance_minus = 20;
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("R12,50+0,05/-0,02 m"));

    d.tolerance = DimTolerance::Limits;
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("R12,55/12,48 m"));

    // Its own unit, whatever the drawing's is.
    DimensionDef cm = metres(12'500);
    cm.unit         = static_cast<std::uint8_t>(DrawingUnit::Centimetre) + 1;
    CHECK_EQ(core::dimension_text(cm, DrawingUnit::Metre), std::string("1250,00"));
    CHECK_EQ(core::dimension_unit(cm, DrawingUnit::Metre), DrawingUnit::Centimetre);

    // An angle's tolerance is an angle.
    DimensionDef a;
    a.type            = DimensionType::Angular3P;
    a.measurement     = 90'000'000;
    a.tolerance       = DimTolerance::Symmetric;
    a.tolerance_plus  = 500'000;
    a.tolerance_minus = 500'000;
    CHECK_EQ(core::dimension_text(a, DrawingUnit::Metre), std::string("90,00°±0,50°"));
}

TEST_CASE("ÖLÇÜ YAZISI: <> ölçülen değerdir; <> taşımayan metin elle yazılmıştır ve süslenmez")
{
    DimensionDef d  = metres(20'000);
    d.prefix        = "≈";
    d.override_text = "<> (tapu)";
    CHECK_FALSE(core::dimension_text_is_manual(d));
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("≈20,00 (tapu)"));

    d.override_text = "12,50";
    CHECK(core::dimension_text_is_manual(d));
    // Exactly as typed: a prefix around a typed number would dress it up as a
    // measured one.
    CHECK_EQ(core::dimension_text(d, DrawingUnit::Metre), std::string("12,50"));
    // And the measured figure is still there to be asked for.
    CHECK_EQ(core::dimension_value_text(d, DrawingUnit::Metre), std::string("20,00"));
}

TEST_CASE("ÖLÇÜ YÜKÜ: yeni alanı olmayan ölçü eski baytlarıyla yazılır; olan ikinci düzenle gidip "
          "gelir")
{
    const DimensionDef plain = metres(12'500);
    const auto v1            = core::encode_dimension(plain);
    REQUIRE(v1.size() >= 2);
    CHECK_EQ(v1[0], 1); // layout 1, byte for byte what it always was
    auto back = core::decode_dimension(v1);
    REQUIRE(back.ok());
    CHECK(back.value() == plain);

    DimensionDef full    = plain;
    full.prefix          = "R";
    full.suffix          = " m";
    full.tolerance       = DimTolerance::Deviation;
    full.tolerance_plus  = 50;
    full.tolerance_minus = 20;
    full.unit            = static_cast<std::uint8_t>(DrawingUnit::Centimetre) + 1;
    full.scale_basis     = 500;
    const auto v2        = core::encode_dimension(full);
    CHECK_EQ(v2[0], 2);
    auto again = core::decode_dimension(v2);
    REQUIRE(again.ok());
    CHECK(again.value() == full);

    // A layout-2 payload cut short, or with a tolerance code past the last, is
    // refused rather than misread.
    CHECK_FALSE(core::decode_dimension(std::span(v2).first(v2.size() - 3)).ok());
    std::vector<std::uint8_t> bad = v2;
    // From the end: the scale (8), the unit (1), the two deviations (8 + 8),
    // then the tolerance byte.
    bad[bad.size() - 8 - 1 - 8 - 8 - 1] = 9;
    CHECK_FALSE(core::decode_dimension(bad).ok());
}

// ----------------------------------------------------------------- ÖLÇÜ ----

TEST_CASE("ÖLÇÜ: önek, sonek, tolerans ve ondalık çizerken verilir, günlüğe yazılır")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3 onek=R sonek=\" m\" tolerans=0.05 "
          "hassasiyet=3");
    CHECK_EQ(r.caption(1), std::string("R20,000±0,050 m"));
    CHECK(r.said.find("Ölçü çizildi: R20,000±0,050 m") != std::string::npos);
    const DimensionDef d = r.def(1);
    CHECK_EQ(d.scale_basis, 1000); // the plan scale it was sized for
    CHECK_EQ(d.tolerance, DimTolerance::Symmetric);

    // What was typed replays to the same sheet.
    Rig replay;
    for (const auto& e : r.journal.entries())
        REQUIRE(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), r.doc.content_hash());
}

// ------------------------------------------------------------ ÖLÇÜDÜZENLE ----

TEST_CASE("ÖLÇÜDÜZENLE: elle yazılan değer elle diye söylenir, gerçek ölçü yanında durur; sıfırla "
          "ölçüye döndürür")
{
    Rig r;
    r.run("ÇİZGİ 0,0 20,0");                           // 1
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 2
    r.said.clear();
    r.run("ÖLÇÜDÜZENLE nesneler=2 metin=\"19,99\"");
    CHECK_EQ(r.caption(2), std::string("19,99"));
    CHECK(core::dimension_text_is_manual(r.def(2)));
    CHECK(r.said.find("elle yazılmış; ölçülen 20,00") != std::string::npos);

    r.said.clear();
    r.run("NESNEBİLGİ nesneler=2");
    CHECK(r.said.find("ölçtüğü 20,00, yazdığı \"19,99\" — ELLE YAZILMIŞ") != std::string::npos);

    // It does not follow its corner, and that is said.
    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    CHECK_EQ(r.caption(2), std::string("19,99"));
    CHECK_EQ(r.def(2).measurement, 25'000);
    CHECK(r.said.find("yazısı elle yazılmış; yeniden ölçülen değeri göstermiyor") !=
          std::string::npos);

    r.run("ÖLÇÜDÜZENLE nesneler=2 sifirla=metin");
    CHECK_EQ(r.caption(2), std::string("25,00"));
    CHECK_FALSE(core::dimension_text_is_manual(r.def(2)));
}

TEST_CASE("ÖLÇÜDÜZENLE: şablon, birim, ondalık, sapma ve sınır; hepsi tek geri alma adımı")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1
    r.run("ÖLÇÜDÜZENLE nesneler=1 metin=\"<> (tapu)\"");
    CHECK_EQ(r.caption(1), std::string("20,00 (tapu)"));
    CHECK_FALSE(core::dimension_text_is_manual(r.def(1)));

    r.run("ÖLÇÜDÜZENLE nesneler=1 birim=cm hassasiyet=0");
    CHECK_EQ(r.caption(1), std::string("2000 (tapu)"));

    r.run("ÖLÇÜDÜZENLE nesneler=1 sifirla=hepsi hassasiyet=2 tolerans_ust=0.05 tolerans_alt=0.02");
    CHECK_EQ(r.caption(1), std::string("20,00+0,05/-0,02"));
    r.run("ÖLÇÜDÜZENLE nesneler=1 tolerans_bicim=sinir");
    CHECK_EQ(r.caption(1), std::string("20,05/19,98"));

    const std::size_t depth = r.undo.undo_depth();
    r.run("ÖLÇÜDÜZENLE nesneler=1 onek=R sonek=\" m\" tolerans=0");
    CHECK_EQ(r.caption(1), std::string("R20,00 m"));
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    r.run("GERİAL");
    CHECK_EQ(r.caption(1), std::string("20,05/19,98"));
}

TEST_CASE("ÖLÇÜDÜZENLE: elle yerleştirilen yazı yerinde kalır, ölçü izleyince onunla taşınır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 20,0");                           // 1
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 2
    r.run("ÖLÇÜDÜZENLE nesneler=2 yazi_yeri=4,-6");
    const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]);
    CHECK_EQ(r.doc.geometry().vertex(span.first, 0), (Point2{4'000, -6'000}));
    CHECK(r.def(2).user_text_position);

    // The line moved up two metres, both ends: the caption goes with it.
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=0,2");
    const core::RingSpan moved = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(2)]);
    CHECK_EQ(r.doc.geometry().vertex(moved.first, 0), (Point2{4'000, -4'000}));

    r.run("ÖLÇÜDÜZENLE nesneler=2 sifirla=yazi_yeri");
    CHECK_FALSE(r.def(2).user_text_position);
}

TEST_CASE("YAZIDÜZENLE bir ölçünün yazısını ölçünün kendi modeline yazar")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1
    r.run("YAZIDÜZENLE nesneler=1 yazi=\"20 m (yaklaşık)\"");
    CHECK_EQ(r.def(1).override_text, std::string("20 m (yaklaşık)"));
    CHECK(core::dimension_text_is_manual(r.def(1)));
    r.run("YAZIDÜZENLE nesneler=1 yazi=\"<>\"");
    CHECK_EQ(r.caption(1), std::string("20,00"));
}

// ------------------------------------------------------------ ÖLÇÜYENİLE ----

TEST_CASE("ÖLÇÜYENİLE: başka paftada kâğıttaki boy korunur; ikinci çağrı bir şey yapmaz")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1, sized for 1/1000
    const core::Mm arrow  = r.def(1).arrow_size;
    const core::Mm height = r.height(1);
    CHECK_EQ(arrow, 2'500); // 2,5 mm on paper at 1/1000

    r.said.clear();
    r.run("ÖLÇÜYENİLE olcek=500");
    CHECK_EQ(r.def(1).arrow_size, arrow / 2);
    CHECK_EQ(r.height(1), height / 2);
    CHECK_EQ(r.def(1).scale_basis, 500);
    // The same size ON PAPER: ground over scale did not move.
    CHECK_EQ(r.height(1) * 1000 / 500, height * 1000 / 1000);
    CHECK(r.said.find("1 ölçü 1/1000'den 1/500 paftasına uyarlandı") != std::string::npos);
    CHECK_EQ(r.caption(1), std::string("20,00")); // the figure is the figure

    r.said.clear();
    r.run("ÖLÇÜYENİLE olcek=500");
    CHECK(r.said.find("1 ölçü zaten 1/500 için") != std::string::npos);

    r.run("ÖLÇÜYENİLE olcek=5000");
    CHECK_EQ(r.def(1).arrow_size, arrow * 5);
}

TEST_CASE("ÖLÇÜYENİLE: ölçeği bilinmeyen ölçü söylenir; eski_olcek ya da stili ölçeğini verir")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3"); // 1
    // As if it came from a file written before the scale was recorded, drawn
    // at 1/1000 with an unknown style.
    DimensionDef old = r.def(1);
    old.scale_basis  = 0;
    old.style        = "BILINMEYEN";
    {
        Transaction tx(r.doc, "eski");
        const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(1)]);
        const std::vector<Point2> base{r.doc.geometry().vertex(span.first, 0),
                                       r.doc.geometry().vertex(span.first, 1)};
        std::vector<Point2> defs;
        for (std::uint32_t v = 0; v < r.doc.geometry().ring_count[span.first + 1]; ++v)
            defs.push_back(r.doc.geometry().vertex(span.first + 1, v));
        const std::array<core::RingGeometry::RingInput, 2> rings{
            core::RingGeometry::RingInput{base, core::RingRole::Open, 0},
            core::RingGeometry::RingInput{defs, core::RingRole::Open, 0}};
        REQUIRE(tx.set_kind_geometry(r.slot(1), rings, core::encode_dimension(old)).ok());
        (void)tx.release();
    }
    r.said.clear();
    r.run("ÖLÇÜYENİLE olcek=500");
    CHECK(r.said.find("1 ölçünün hangi pafta ölçeği için çizildiği bilinmiyor") !=
          std::string::npos);
    CHECK_EQ(r.def(1).arrow_size, 2'500);

    r.run("ÖLÇÜYENİLE olcek=500 eski_olcek=1000");
    CHECK_EQ(r.def(1).arrow_size, 1'250);
    CHECK_EQ(r.def(1).scale_basis, 500);
}

TEST_CASE("AYAR plan_ölçeği değişince ölçülerin eski ölçeğe göre boyutlu olduğu söylenir")
{
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    r.said.clear();
    r.run("AYAR core.plan.olcek 500");
    CHECK(r.said.find("1 ölçü 1/1000 paftası için boyutlandırılmış; 1/500 paftasında") !=
          std::string::npos);
    CHECK(r.said.find("ÖLÇÜYENİLE") != std::string::npos);
}

// ------------------------------------------------------------------ proof ----

TEST_CASE("ÖLÇÜDÜZENLE KANIT: arayüz, komut satırı, betik ve oynatma aynı ölçüyü bırakır")
{
    Rig gui;
    gui.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    {
        auto started = gui.bus.begin_interactive("ÖLÇÜDÜZENLE nesneler=1", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting()); // nothing named: the caption is asked for
        CHECK(session.supply(Value::text("<> (tapu)")).ok());
        CHECK(gui.bus.finish(session).ok());
    }
    REQUIRE(gui.bus.execute_line("ÖLÇÜYENİLE olcek=500", Origin::Gui).ok());

    Rig cli;
    cli.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    REQUIRE(cli.bus.execute_line("ÖLÇÜDÜZENLE nesneler=1 metin=\"<> (tapu)\"", Origin::CommandLine)
                .ok());
    REQUIRE(cli.bus.execute_line("ÖLÇÜYENİLE olcek=500", Origin::CommandLine).ok());

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        REQUIRE(runner
                    .run_text(R"json({"ad":"Kanıt","komutlar":[
                      {"cmd":"core.dimension","args":{"birinci":[0,0],"ikinci":[20000,0],
                        "konum":[10000,-3000]}},
                      {"cmd":"core.dimension_edit","args":{"nesneler":[1],"metin":"<> (tapu)"}},
                      {"cmd":"core.dimension_refresh","args":{"olcek":500}}]})json")
                    .ok());
    }
    CHECK_EQ(gui.caption(1), std::string("20,00 (tapu)"));
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    Rig replay;
    for (const auto& e : gui.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), gui.doc.content_hash());
}

// ------------------------------------------------------------ the manual ----

TEST_CASE("ÖLÇÜDÜZENLE ve ÖLÇÜYENİLE: kılavuzdaki örnekler kelimesi kelimesine")
{
    // docs/komutlar/dimension_edit.md and dimension_refresh.md print these lines
    // (CLAUDE.md 11.6: every example runs exactly as printed).
    const auto transcript = [](std::initializer_list<const char*> lines) {
        Rig r;
        for (const char* line : lines)
            r.run(line);
        return r.said;
    };
    const std::string radius = transcript(
        {"DAİRE merkez=0,0 cevre=5,0", "ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=8,2",
         "ÖLÇÜDÜZENLE nesneler=2 onek=R sonek=\" m\" tolerans=0.05"});
    CHECK(radius.find("Ölçü düzenlendi: 1 ölçü; yazısı \"R5,00±0,05 m\".\n") != std::string::npos);

    const std::string typed = transcript(
        {"ÇİZGİ 0,0 20,0", "ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3",
         "ÖLÇÜDÜZENLE nesneler=2 metin=\"19,99\"", "ÖLÇÜDÜZENLE nesneler=2 sifirla=metin"});
    CHECK(typed.find("Ölçü düzenlendi: 1 ölçü; yazısı \"19,99\" (elle yazılmış; ölçülen 20,00).\n"
                     "Ölçü düzenlendi: 1 ölçü; yazısı \"20,00\".\n") != std::string::npos);

    const std::string noted = transcript(
        {"ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3", "ÖLÇÜDÜZENLE nesneler=1 metin=\"<> (tapu)\"",
         "ÖLÇÜDÜZENLE nesneler=1 birim=cm hassasiyet=0",
         "ÖLÇÜDÜZENLE nesneler=1 sifirla=hepsi tolerans_ust=0.05 tolerans_alt=0.02 "
         "tolerans_bicim=sinir"});
    CHECK(noted.find("Ölçü düzenlendi: 1 ölçü; yazısı \"20,00 (tapu)\".\n"
                     "Ölçü düzenlendi: 1 ölçü; yazısı \"2000 (tapu)\".\n"
                     "Ölçü düzenlendi: 1 ölçü; yazısı \"20,05/19,98\".\n") != std::string::npos);

    const std::string refreshed = transcript({"ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3",
                                              "ÖLÇÜYENİLE olcek=500", "ÖLÇÜYENİLE olcek=500"});
    CHECK(refreshed.find("1 ölçü 1/1000'den 1/500 paftasına uyarlandı: oklar, uzatma çizgileri ve "
                         "yazılar kâğıtta aynı boyda kalır (yazı 2,5 mm).\n1 ölçü zaten 1/500 "
                         "için.\n") != std::string::npos);

    const std::string drawn = transcript({"ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3 onek=R "
                                          "sonek=\" m\" tolerans=0.05 hassasiyet=3"});
    CHECK(drawn.find("Ölçü çizildi: R20,000±0,050 m (ISO-25).\n") != std::string::npos);
}
