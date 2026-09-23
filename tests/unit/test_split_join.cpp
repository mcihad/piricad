// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPLIT, BREAK AND JOIN ON REAL GEOMETRY (TODOS C-05).
//
// A piece of an arc is an arc; the pieces of a line add up to the line; a
// road cut in two is still that road on both sides; and whoever tracks an
// object by its key is told what the key became. Every expected length below
// is a closed form — r·θ, a straight run — asserted to the millimetre a cut
// may round.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <cstdlib>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string echoed;
    core::Json reported;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    /// Runs `line` and keeps what it reported.
    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
        reported = r.value().report;
    }
};

/// The live objects, in key order, as the kinds they are and the paths they
/// walk.
struct Live
{
    std::int64_t key{0};
    core::KindId kind{core::kNoKind};
    core::CurvePath path;
};

std::vector<Live> live(const core::Document& doc)
{
    std::vector<Live> out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        Live one;
        one.key  = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
        one.kind = doc.entities().kind[e];
        if (auto p = core::path_of(doc, e)) one.path = *p;
        out.push_back(std::move(one));
    }
    return out;
}

core::Mm total_length(const std::vector<Live>& all)
{
    core::Mm sum = 0;
    for (const Live& l : all)
        sum += core::path_length(l.path);
    return sum;
}

/// An open arc-polyline drawn into the rig: (0,0)→(10,0), a counter-clockwise
/// half turn of radius 5 m up to (10,10) through (15,5), then (10,10)→(0,10).
std::int64_t add_kerb(Rig& r)
{
    const core::LayerId layer = r.doc.ensure_layer("0");
    const std::vector<core::Point2> v{{0, 0}, {10'000, 0}, {10'000, 10'000}, {0, 10'000}};
    core::ArcPolyline def;
    def.arcs.push_back(core::ArcPolyline::Arc{1, core::Point2{10'000, 5'000}, 5'000, true});
    const core::RingGeometry::RingInput input{v, core::RingRole::Open, 0};
    core::Op op;
    auto made = r.doc.add_kind(layer, core::kArcPolylineKind, {&input, 1},
                               core::encode_arc_polyline(def), op);
    REQUIRE(made.ok());
    return static_cast<std::int64_t>(core::raw(r.doc.key_of(made.value())));
}

bool has_arc(const core::CurvePath& path)
{
    for (const core::PathPiece& p : path.pieces)
        if (p.kind == core::PathPiece::Kind::Arc) return true;
    return false;
}

} // namespace

// =============================================================================
// BÖL
// =============================================================================

TEST_CASE("C-05: BÖL eşit parçaya — yay üç yay olur, toplamı yayın kendisi")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10"); ///< a quarter turn, r = 10 m
    const core::Mm whole = total_length(live(r.doc));
    r.run("BÖL nesne=1 yontem=esit sayi=3");

    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{3});
    for (const Live& l : all) {
        CHECK(l.kind == core::kArcKind); ///< arcs, never chords
        CHECK(std::abs(core::path_length(l.path) - whole / 3) <= 2);
    }
    CHECK(std::abs(total_length(all) - whole) <= 3);
    CHECK(r.echoed.find("3 parça:") != std::string::npos);

    // THE KEY STAYED WITH THE FIRST PIECE, and the report says what became of it.
    CHECK_EQ(all.front().key, 1);
    REQUIRE(r.reported.is_array());
    CHECK(r.reported.dump() == R"([{"kaynak":1,"sonuc":[1,2,3]}])");
}

TEST_CASE("C-05: BÖL daireyi iki noktadan iki yaya böler; tek noktadan bölmez")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=10,0");
    r.run("BÖL nesne=1 yontem=nokta noktalar=0,10 0,-10");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    for (const Live& l : all) {
        CHECK(l.kind == core::kArcKind);
        CHECK(std::abs(core::path_length(l.path) - 31'416) <= 1); ///< π · 10 m
    }
    // A circle's pieces are arcs, a different kind: every one is new.
    CHECK(r.reported.dump() == R"([{"kaynak":1,"sonuc":[2,3]}])");

    Rig one;
    one.run("DAİRE merkez=0,0 cevre=10,0");
    auto refused = one.bus.execute_line("BÖL nesne=1 yontem=nokta noktalar=0,10", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("tek noktada bölünmez") != std::string::npos);
    CHECK_EQ(live(one.doc).size(), std::size_t{1});
}

TEST_CASE("C-05: BÖL kesişimlerden — seçilen çizgiler birbirini kestikleri her yerde kırılır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    r.run("ÇOKLUÇİZGİ 10,-5 10,5");
    r.run("ÇOKLUÇİZGİ 15,-5 15,5");
    const core::Mm whole = total_length(live(r.doc));
    r.run("BÖL nesne=1 2 3 yontem=kesisim");

    // 3 + 2 + 2 pieces, and not a millimetre lost at a junction.
    CHECK_EQ(live(r.doc).size(), std::size_t{7});
    CHECK_EQ(total_length(live(r.doc)), whole);
    CHECK(r.echoed.find("3 nesne bölündü, 7 parça") != std::string::npos);
}

TEST_CASE("C-05: BÖL baştan uzaklıktan; uzaklık çizginin dışındaysa söyler")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    r.run("BÖL nesne=1 yontem=mesafe mesafe=7");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    CHECK_EQ(core::path_length(all[0].path), core::Mm{7'000});
    CHECK_EQ(core::path_length(all[1].path), core::Mm{13'000});

    Rig far;
    far.run("ÇOKLUÇİZGİ 0,0 20,0");
    auto refused = far.bus.execute_line("BÖL nesne=1 yontem=mesafe mesafe=25", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("20,000 m uzunluğunda") != std::string::npos);
}

TEST_CASE("C-05: yaylı çoklu çizgi bölününce yayları yay kalır")
{
    Rig r;
    const std::int64_t kerb = add_kerb(r);
    const core::Mm whole    = total_length(live(r.doc));
    r.run("BÖL nesne=" + std::to_string(kerb) + " yontem=esit sayi=2");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    // The cut falls on the half turn, so both pieces hold a piece of it — as an
    // arc of the same circle, never a chord.
    for (const Live& l : all) {
        CHECK(l.kind == core::kArcPolylineKind);
        CHECK(has_arc(l.path));
    }
    CHECK(std::abs(total_length(all) - whole) <= 2);
}

TEST_CASE("C-05: bölünen parçaların hepsi katmanı, stili ve öznitelikleri taşır")
{
    Rig r;
    r.run("KATMAN ad=YOL");
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    r.run("SÜTUN kimlik=ad tur=metin");
    r.run("ÖZNİTELİK ad=ad nesne=1 deger=ATATÜRK");
    r.run("BÖL nesne=1 yontem=esit sayi=4");

    const core::AttrId column = r.doc.attributes().find("ad");
    REQUIRE(column != core::kNoAttr);
    const core::LayerId road = r.doc.find_layer("YOL");
    std::size_t pieces       = 0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        ++pieces;
        CHECK_EQ(r.doc.entities().layer[e], road);
        auto cell = r.doc.attribute(column, e);
        REQUIRE(cell.ok());
        CHECK(cell.value().present);
        CHECK(cell.value().text == "ATATÜRK");
    }
    CHECK_EQ(pieces, std::size_t{4});
}

TEST_CASE("C-05: BÖL alanı kenarından açmaz; kesme çizgisini gösterir")
{
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    auto refused = r.bus.execute_line("BÖL nesne=1 yontem=esit sayi=2", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("bir alan") != std::string::npos);
    CHECK(refused.error().message.find("yontem=cizgi") != std::string::npos);
}

TEST_CASE("C-05: BÖL'ün kesme çizgisi eğriyi her kesişiminde böler")
{
    // A circle crossed by a cut line comes apart at BOTH crossings — it used to
    // be refused as "not a line".
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=10,0");
    r.run("BÖL nesne=1 noktalar=-20,0 20,0");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    for (const Live& l : all)
        CHECK(l.kind == core::kArcKind);
}

TEST_CASE("C-05: BÖL noktadan arayüzde — parçalar Enter'dan önce görünür, son nokta geri alınır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    auto started = r.bus.begin_interactive("BÖL yontem=nokta nesne=1", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::Split);
    REQUIRE(s.supply(Value::point({5'000, 0})).ok());
    REQUIRE(s.supply(Value::point({17'000, 3})).ok()); ///< a wrong one
    CHECK(s.prompt().can_retract);
    REQUIRE(s.retract().ok());
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{1});
    REQUIRE(s.supply(Value::point({12'000, 0})).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());

    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{3});
    CHECK_EQ(core::path_length(all[0].path), core::Mm{5'000});
    CHECK_EQ(core::path_length(all[1].path), core::Mm{7'000});
    CHECK_EQ(core::path_length(all[2].path), core::Mm{8'000});
    // The journal holds the points the user kept, not the one taken back.
    const Value* points = r.journal.entries().back().args.find("noktalar");
    REQUIRE(points != nullptr);
    CHECK(points->as_points() == Value::Points{{5'000, 0}, {12'000, 0}});
}

TEST_CASE("C-05: BÖL eşit parçaya tek geri alma adımıdır ve oynatılınca aynı çizimdir")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    const std::uint64_t before = r.doc.content_hash();
    r.run("BÖL nesne=1 yontem=esit sayi=5");
    const std::uint64_t after = r.doc.content_hash();

    Rig replay;
    for (const auto& e : r.journal.entries())
        REQUIRE(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), after);

    r.run("GERİAL");
    CHECK_EQ(r.doc.content_hash(), before);
}

TEST_CASE("C-05: öznitelik bir geometri düzenlemesinde kaybolmaz; geri alma eskisini getirir")
{
    // THE REGRESSION this work found: cells are indexed by geometry slot, every
    // geometry edit gives the entity a new slot, and the caption was carried to
    // it while the attributes were not — so a moved parcel, a dragged corner
    // and a trimmed road silently lost every attribute (model.md P11).
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    r.run("SÜTUN kimlik=ad tur=metin");
    r.run("ÖZNİTELİK ad=ad nesne=1 deger=YOL");
    const core::AttrId column = r.doc.attributes().find("ad");
    const auto cell           = [&r, column] {
        auto had = r.doc.attribute(column, 0);
        return had.ok() && had.value().present ? had.value().text : std::string("(yok)");
    };

    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=5,5");
    CHECK(cell() == "YOL");
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=30,5");
    CHECK(cell() == "YOL");
    r.run("ÖZNİTELİK ad=ad nesne=1 deger=CADDE");
    r.run("UZUNLUK nesne=1 delta=2");
    CHECK(cell() == "CADDE");

    // Undone step by step, each geometry brings back the value it had.
    r.run("GERİAL");
    CHECK(cell() == "CADDE");
    r.run("GERİAL");
    CHECK(cell() == "YOL");
    r.run("GERİAL");
    r.run("GERİAL");
    CHECK(cell() == "YOL");
}

// =============================================================================
// KIR
// =============================================================================

TEST_CASE("C-05: KIR yayın iki nokta arasındaki parçasını çıkarır; kalanlar yaydır")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10"); ///< a quarter turn, r = 10 m
    // Cuts at 30° and 60°: the middle third goes, two thirds stay as arcs.
    r.run("KIR nesne=1 birinci=8.660,5 ikinci=5,8.660");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    for (const Live& l : all) {
        CHECK(l.kind == core::kArcKind);
        CHECK(std::abs(core::path_length(l.path) - 5'236) <= 1); ///< 10 m · π/6
    }
    CHECK(r.echoed.find("iki parça kaldı") != std::string::npos);
    CHECK(r.reported.dump() == R"([{"kaynak":1,"sonuc":[1,2]}])");
}

TEST_CASE("C-05: KIR daireden bir parça çıkarır, kalan yaydır; tek noktadan kırmaz")
{
    // From the east round to the north, the way a circle is walked: a quarter
    // goes and three quarters stay, as one arc.
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=10,0");
    r.run("KIR nesne=1 birinci=10,0 ikinci=0,10");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{1});
    CHECK(all.front().kind == core::kArcKind);
    CHECK(std::abs(core::path_length(all.front().path) - 47'124) <= 1); ///< ¾ · 2π · 10 m
    CHECK(r.echoed.find("Kapalı şekil kırıldı") != std::string::npos);

    Rig once;
    once.run("DAİRE merkez=0,0 cevre=10,0");
    auto refused = once.bus.execute_line("KIR nesne=1 birinci=10,0 ikinci=10,0", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("tek noktadan kırılmaz") != std::string::npos);
}

TEST_CASE("C-05: KIR alanı kırmaz, nasıl açılacağını söyler")
{
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    auto refused = r.bus.execute_line("KIR nesne=1 birinci=5,0 ikinci=10,5", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("alan kırılmaz") != std::string::npos);
    CHECK(refused.error().message.find("ÇİZGİDÜZENLE islem=ac") != std::string::npos);
}

TEST_CASE("C-05: KIR yaylı çoklu çizginin yayından parça çıkarınca yaylar yay kalır")
{
    Rig r;
    const std::int64_t kerb = add_kerb(r);
    // Both cuts on the half turn round (10,5): (15,5) is its east point.
    r.run("KIR nesne=" + std::to_string(kerb) + " birinci=13.536,1.464 ikinci=15,5");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{2});
    for (const Live& l : all)
        CHECK(has_arc(l.path));
}

TEST_CASE("C-05: KIR'dan kalan iki parça da öznitelikleri taşır")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 20,0");
    r.run("SÜTUN kimlik=ad tur=metin");
    r.run("ÖZNİTELİK ad=ad nesne=1 deger=ÇİT");
    r.run("KIR nesne=1 birinci=8,0 ikinci=12,0");
    const core::AttrId column = r.doc.attributes().find("ad");
    std::size_t carrying      = 0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e) {
        if (!r.doc.alive(e)) continue;
        auto cell = r.doc.attribute(column, e);
        if (cell.ok() && cell.value().present && cell.value().text == "ÇİT") ++carrying;
    }
    CHECK_EQ(carrying, std::size_t{2});
}

// =============================================================================
// UÇUCA
// =============================================================================

TEST_CASE("C-05: UÇUCA çizgiyle yayı yaylı çoklu çizgi yapar; yay yay kalır, uzunluk korunur")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ -10,0 10,0");
    r.run("YAY merkez=10,5 baslangic=10,0 bitis=10,10"); ///< a half turn up the east side
    const core::Mm whole = total_length(live(r.doc));
    r.run("UÇUCA nesne=1 2");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{1});
    CHECK(all.front().kind == core::kArcPolylineKind);
    CHECK(has_arc(all.front().path));
    CHECK_EQ(total_length(all), whole);
    // A polyline cannot hold an arc: the result is new, and both keys say so.
    CHECK(r.reported.dump() == R"([{"kaynak":1,"sonuc":[3]},{"kaynak":2,"sonuc":[3]}])");
}

TEST_CASE("C-05: UÇUCA bir çemberin uç uca iki yayını tek yay yapar")
{
    Rig r;
    r.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    r.run("YAY merkez=0,0 baslangic=0,10 bitis=-10,0");
    r.run("UÇUCA nesne=1 2");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{1});
    CHECK(all.front().kind == core::kArcKind);
    CHECK(std::abs(core::path_length(all.front().path) - 31'416) <= 1); ///< a half turn
}

TEST_CASE("C-05: UÇUCA boşluğu gizlemez — kapatır ve söyler, çizileni oynatmaz")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0");
    r.run("ÇOKLUÇİZGİ 10.004,0 20,0"); ///< 4 mm short
    r.run("UÇUCA nesne=1 2 tolerans=0.01");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{1});
    // Both lines exactly where they were drawn, and the 4 mm bridged.
    CHECK(core::path_vertices(all.front().path) ==
          std::vector<core::Point2>{{0, 0}, {10'000, 0}, {10'004, 0}, {20'000, 0}});
    CHECK(r.echoed.find("1 boşluk doğru parçasıyla kapatıldı; en büyüğü 4 mm (tolerans 10 mm)") !=
          std::string::npos);
}

TEST_CASE("C-05: UÇUCA ilk çizginin yönünde birleştirir")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 10,0 0,0");  ///< drawn westwards
    r.run("ÇOKLUÇİZGİ 10,0 20,0"); ///< drawn eastwards, touching its start
    r.run("UÇUCA nesne=1 2");
    const auto all = live(r.doc);
    REQUIRE_EQ(all.size(), std::size_t{1});
    CHECK(core::path_vertices(all.front().path) ==
          std::vector<core::Point2>{{20'000, 0}, {10'000, 0}, {0, 0}});
}

TEST_CASE("C-05: UÇUCA'da katman ve öznitelik çatışması kurala bağlıdır")
{
    const auto setup = [](Rig& r) {
        r.run("KATMAN ad=YOL");
        r.run("ÇOKLUÇİZGİ 0,0 10,0");
        r.run("KATMAN ad=REFUJ");
        r.run("ÇOKLUÇİZGİ 10,0 20,0");
        r.run("SÜTUN kimlik=ad tur=metin");
        r.run("ÖZNİTELİK ad=ad nesne=1 deger=ATATÜRK");
        r.run("ÖZNİTELİK ad=ad nesne=2 deger=İNÖNÜ");
    };

    // THE DEFAULT: the first line's layer and values, and what differed said.
    Rig keep;
    setup(keep);
    keep.run("UÇUCA nesne=1 2");
    REQUIRE_EQ(keep.doc.live_entity_count(), std::size_t{1});
    core::EntityId e = 0;
    while (!keep.doc.alive(e))
        ++e;
    CHECK_EQ(keep.doc.entities().layer[e], keep.doc.find_layer("YOL"));
    auto cell = keep.doc.attribute(keep.doc.attributes().find("ad"), e);
    REQUIRE(cell.ok());
    CHECK(cell.value().text == "ATATÜRK");
    CHECK(keep.echoed.find("başka katmandaydı; sonuç ilk çizginin katmanında (YOL)") !=
          std::string::npos);
    CHECK(keep.echoed.find("Öznitelikleri farklıydı: ad") != std::string::npos);

    // `cakisma=reddet`: nothing is joined, and the refusal names the conflicts.
    Rig strict;
    setup(strict);
    const std::uint64_t before = strict.doc.content_hash();
    auto refused = strict.bus.execute_line("UÇUCA nesne=1 2 cakisma=reddet", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("katman, ad") != std::string::npos);
    CHECK_EQ(strict.doc.content_hash(), before);
}

TEST_CASE("C-05: UÇUCA kapalı şekli eklemez; uçları buluşan zinciri söyler")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=10,0");
    r.run("ÇOKLUÇİZGİ 10,0 20,0");
    auto refused = r.bus.execute_line("UÇUCA nesne=1 2", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("kapalı") != std::string::npos);

    Rig loop;
    loop.run("ÇOKLUÇİZGİ 0,0 10,0");
    loop.run("ÇOKLUÇİZGİ 10,0 10,10");
    loop.run("ÇOKLUÇİZGİ 10,10 0,0");
    loop.run("UÇUCA nesne=1 2 3");
    CHECK(loop.echoed.find("Zincirin iki ucu buluşuyor") != std::string::npos);
    CHECK_EQ(loop.doc.live_entity_count(), std::size_t{1});
}

// =============================================================================
// ÇİZGİDÜZENLE on a polyline whose edges bend
// =============================================================================

TEST_CASE("C-05: ÇİZGİDÜZENLE yaylı çoklu çizgiyi kapatır, açar ve ters çevirir; yay kalır")
{
    Rig r;
    const std::int64_t kerb         = add_kerb(r);
    const std::string id            = std::to_string(kerb);
    const auto the_path             = [&r] { return live(r.doc).front().path; };
    const core::CurvePath open_path = the_path();
    REQUIRE_EQ(open_path.pieces.size(), std::size_t{3});

    r.run("ÇİZGİDÜZENLE nesne=" + id + " islem=kapat");
    core::CurvePath closed = the_path();
    CHECK(closed.closed);
    CHECK_EQ(closed.pieces.size(), std::size_t{4}); ///< the straight closing edge
    CHECK(has_arc(closed));
    CHECK(live(r.doc).front().kind == core::kArcPolylineKind);

    r.run("ÇİZGİDÜZENLE nesne=" + id + " islem=ac");
    CHECK(the_path() == open_path); ///< the closing edge gone again, nothing else moved

    r.run("ÇİZGİDÜZENLE nesne=" + id + " islem=ters");
    const core::CurvePath turned = the_path();
    CHECK(turned == core::reversed(open_path));
    CHECK(turned.pieces[1].sweep_udeg < 0); ///< the arc walked the other way

    auto refused = r.bus.execute_line("ÇİZGİDÜZENLE nesne=" + id + " islem=sadelestir tolerans=1",
                                      Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("yaylarının uçlarını atardı") != std::string::npos);
}
