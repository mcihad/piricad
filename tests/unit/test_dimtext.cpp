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
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <array>
#include <cstdlib>
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

// --------------------------------------------------- the fit (TODOS C-18) ----

TEST_CASE("ÖLÇÜ YAZISI SIĞDIRMA: <> (tapu) yazılmış 20 m'lik ölçünün yazısı çizildiği genişlikle "
          "ölçülür; sığmaz, dışarı çıkar ve uzatma çizgisine değmez")
{
    // THE ACCEPTANCE. "20,00 (tapu)" at a 2,5 m capital is 5611 units of the
    // face over a 698-unit capital: 20,097 m, wider than the 20 m it stands
    // between. The core used to guess six tenths of a height a letter — 18 m —
    // judged it to fit, and the sheet drew 20,1 m of words across both
    // extension lines. It is measured now as it is drawn (the atlas's own case
    // holds the drawing to this measure letter by letter), so it goes out.
    Rig r;
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    r.run("ÖLÇÜDÜZENLE nesneler=1 metin=\"<> (tapu)\"");
    const std::string caption = r.caption(1);
    REQUIRE_EQ(caption, std::string("20,00 (tapu)"));
    const core::Mm wide = core::text_width(caption, r.height(1));
    CHECK_EQ(wide, core::Mm{20'097});

    const auto box_of = [&r](std::int64_t key) {
        std::array<Point2, 4> quad{};
        REQUIRE(core::text_quad(r.doc, r.slot(key), quad));
        core::Box2 box;
        for (const Point2 p : quad)
            box.extend(p);
        return box;
    };

    // Its box is the drawn width, and it stands past the extension line at
    // 20 m, a gap clear of it — the heads fit inside, so nothing else between.
    const core::Box2 out = box_of(1);
    CHECK_LE(std::llabs((out.max_x - out.min_x) - wide), 1);
    CHECK_EQ(out.min_x, 20'000 + 625);

    // On 30 m the same words fit between the lines, a gap clear of each, and
    // stay centred over the line.
    r.run("ÖLÇÜ birinci=0,10 ikinci=30,10 konum=15,7");
    r.run("ÖLÇÜDÜZENLE nesneler=2 metin=\"<> (tapu)\"");
    const core::Box2 in = box_of(2);
    CHECK_EQ(r.caption(2), std::string("30,00 (tapu)"));
    CHECK_GE(in.min_x, 625);
    CHECK_LE(in.max_x, 30'000 - 625);
    CHECK_LE(std::llabs((in.min_x + in.max_x) - 30'000), 1);
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

// ------------------------------------------------------- chain, baseline ----

TEST_CASE("ZİNCİRÖLÇÜ: her ölçü bir öncekinin ucundan, aynı çizgide; toplamı söylenir")
{
    Rig r;
    r.run("ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3"); // 1
    r.said.clear();
    r.run("ZİNCİRÖLÇÜ noktalar=12.5,0 16.75,0"); // 2, 3
    CHECK_EQ(r.def(2).measurement, 7'500);
    CHECK_EQ(r.def(3).measurement, 4'250);
    CHECK(r.said.find("Zincir ölçü: 2 ölçü eklendi (7,50 · 4,25); toplam 11,75.") !=
          std::string::npos);
    // One line: every dimension line point three metres below.
    for (const std::int64_t key : {1, 2, 3}) {
        const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(key)]);
        CHECK_EQ(r.doc.geometry().vertex(span.first + 1, 2).y, -3'000);
    }
    // One undo step for the whole row.
    r.run("GERİAL");
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("BAZÖLÇÜ: her ölçü ilk noktadan, çizgiler stilin aralığıyla üst üste")
{
    Rig r;
    r.run("ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3"); // 1
    r.said.clear();
    r.run("BAZÖLÇÜ noktalar=12.5,0 16.75,0"); // 2, 3
    CHECK_EQ(r.def(2).measurement, 12'500);
    CHECK_EQ(r.def(3).measurement, 16'750);
    CHECK(r.said.find("Baz ölçü: 2 ölçü eklendi (12,50 · 16,75).") != std::string::npos);
    // ISO-25 stacks lines 3,75 mm apart on paper: 3,75 m on a 1/1000 sheet, away
    // from the measured points.
    const auto line_y = [&r](std::int64_t key) {
        const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[r.slot(key)]);
        return r.doc.geometry().vertex(span.first + 1, 2).y;
    };
    CHECK_EQ(line_y(2), -6'750);
    CHECK_EQ(line_y(3), -10'500);
}

TEST_CASE("ZİNCİRÖLÇÜ: hizalı ölçünün doğrultusunda sürer; ölçüler köşelere bağlanır ve izler")
{
    Rig r;
    r.run("ÇİZGİ 0,0 3,3");                                     // 1
    r.run("ÇİZGİ 3,3 6,6");                                     // 2
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=3,3 konum=-1,1"); // 3
    r.run("ZİNCİRÖLÇÜ noktalar=6,6");                           // 4
    const DimensionDef d = r.def(4);
    CHECK_EQ(d.type, DimensionType::Linear);
    CHECK_EQ(d.rotation_udeg, 45'000'000);
    CHECK_EQ(d.measurement, 4'243); // 3√2 m along the row
    // Tied to the corners it was drawn on, so it follows the second line.
    r.run("KÖŞETAŞI nesne=2 kose=2 nokta=9,9");
    CHECK_EQ(r.def(4).measurement, 8'485);
}

TEST_CASE("BAĞLI ÖLÇÜ: doğrusal ölçü izlerken doğrultusunu korur, yatay dikeye dönmez")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");                                       // 1
    r.run("ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=10,0 konum=5,-1"); // 2, horizontal
    CHECK_EQ(r.def(2).rotation_udeg, 0);
    // Far to the right and a little up: the line's place now lies BESIDE the
    // points, which is where a new linear dimension would read vertically.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=40,1");
    CHECK_EQ(r.def(2).rotation_udeg, 0);
    CHECK_EQ(r.def(2).measurement, 40'000);
}

TEST_CASE("ÖLÇÜSTİLİ: stiller kâğıttaki ve bu paftadaki boylarıyla; varsayılan AYAR ile değişir")
{
    Rig r;
    r.run("ÖLÇÜSTİLİ");
    CHECK(r.said.find("ISO-25 (varsayılan) — yazı 2,5 mm, kapalı ok 2,5 mm") != std::string::npos);
    CHECK(r.said.find("ISO-35 — yazı 3,5 mm") != std::string::npos);
    CHECK(r.said.find("1/1000 paftada yazı zeminde 2,50 m") != std::string::npos);

    r.run("AYAR ölçü_stili MIMARI");
    r.run("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    CHECK_EQ(r.def(1).style, std::string("MIMARI"));
    CHECK_EQ(r.def(1).arrow, core::ArrowStyle::Tick);
    r.said.clear();
    r.run("ÖLÇÜSTİLİ ad=MIMARI");
    CHECK(r.said.find("MIMARI (varsayılan)") != std::string::npos);
    CHECK(r.said.find("ISO-25") == std::string::npos);
}

TEST_CASE("ZİNCİRÖLÇÜ KANIT: arayüz, komut satırı, betik ve oynatma aynı sırayı bırakır")
{
    Rig gui;
    gui.run("ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3");
    {
        auto started = gui.bus.begin_interactive("ZİNCİRÖLÇÜ", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(Point2{12'500, 0})).ok());
        CHECK(session.supply(Value::point(Point2{16'750, 0})).ok());
        CHECK(gui.bus.finish(session).ok());
    }
    Rig cli;
    cli.run("ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3");
    REQUIRE(cli.bus.execute_line("ZİNCİRÖLÇÜ noktalar=12.5,0 16.75,0", Origin::CommandLine).ok());
    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        REQUIRE(runner
                    .run_text(R"json({"ad":"Kanıt","komutlar":[
                      {"cmd":"core.dimension","args":{"tur":"dogrusal","birinci":[0,0],
                        "ikinci":[5000,0],"konum":[2500,-3000]}},
                      {"cmd":"core.dimension_continue","args":{"noktalar":[[12500,0],
                        [16750,0]]}}]})json")
                    .ok());
    }
    CHECK_EQ(gui.doc.live_entity_count(), std::size_t{3});
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    Rig replay;
    for (const auto& e : gui.journal.entries())
        CHECK(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), gui.doc.content_hash());
}
