// SPDX-License-Identifier: GPL-3.0-or-later
//
// TRANSFORMS THAT DO NOT QUIETLY BREAK WHAT THEY CARRY (TODOS C-08).
//
// A copy is the same kind of thing as its source; a caption scaled with the
// drawing has letters of the new size; a caption or a dimension mirrored still
// reads; HİZALA carries every kind the other verbs carry. Each case below is a
// defect this program had, held down by its closed-form numbers.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/spline.hpp"

#include <cmath>
#include <string>
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

    core::KindId kind(std::int64_t key) const { return doc.entities().kind[slot(key)]; }

    std::vector<Point2> ring(std::int64_t key, std::size_t which = 0) const
    {
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot(key)]);
        const auto xs = doc.geometry().ring_xs(span.first + static_cast<std::uint32_t>(which));
        const auto ys = doc.geometry().ring_ys(span.first + static_cast<std::uint32_t>(which));
        std::vector<Point2> out;
        for (std::size_t v = 0; v < xs.size(); ++v)
            out.push_back(Point2{xs[v], ys[v]});
        return out;
    }

    core::Mm text_height(std::int64_t key) const
    {
        return doc.texts().height(doc.entities().slot[slot(key)]);
    }

    std::string text(std::int64_t key) const
    {
        return std::string(doc.texts().text(doc.entities().slot[slot(key)]));
    }
};

/// The key of the first live object of `kind` outside a block definition, or 0.
std::int64_t first_of(const Rig& r, core::KindId kind)
{
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        if (r.doc.alive(e) && r.doc.entities().kind[e] == kind &&
            (r.doc.entities().flags[e] & core::FlagInBlock) == 0)
            return static_cast<std::int64_t>(core::raw(r.doc.key_of(e)));
    return 0;
}

double gap(Point2 a, Point2 b)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

TEST_CASE("C-08: KOPYALA her türü kendi türünde kopyalar; spline, elips, blok, ölçü bozulmaz")
{
    Rig r;
    r.run("KATMAN ad=A");
    r.run("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5");                       // 1
    r.run("SPLINE noktalar=20,0 noktalar=25,5 noktalar=30,0 noktalar=35,5"); // 2
    r.run("ÇİZGİ 40,0 50,0");                                                // 3
    r.run("BLOK ad=B taban=40,0 nesneler=3");                                // 3 → ref 4
    r.run("ÖLÇÜ tur=hizali birinci=0,20 ikinci=10,20 konum=0,22");           // 5
    r.run("NOKTA 60,0");                                                     // 6
    r.run("SEÇ HEPSİ");
    r.run("KOPYALA baslangic=0,0 bitis=0,100");
    CHECK(r.said.find("nesne kopyalandı") != std::string::npos);

    // EVERY KIND TWICE, the copy 100 m north of its source and of its kind.
    std::vector<std::pair<core::KindId, std::int64_t>> alive;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        if (r.doc.alive(e) && (r.doc.entities().flags[e] & core::FlagInBlock) == 0)
            alive.emplace_back(r.doc.entities().kind[e],
                               static_cast<std::int64_t>(core::raw(r.doc.key_of(e))));
    const auto two_of = [&alive](core::KindId kind) {
        std::vector<std::int64_t> keys;
        for (const auto& [k, key] : alive)
            if (k == kind) keys.push_back(key);
        return keys;
    };
    for (const core::KindId kind :
         {core::kEllipseKind, core::kSplineKind, core::kBlockReferenceKind, core::kDimensionKind,
          core::kPointKind})
        CHECK_MESSAGE(two_of(kind).size() == 2u, "kind " << kind);
    const auto ellipses = two_of(core::kEllipseKind);
    REQUIRE_EQ(ellipses.size(), 2u);
    CHECK_EQ(r.ring(ellipses[1])[0], (Point2{0, 100'000}));
    const auto splines = two_of(core::kSplineKind);
    REQUIRE_EQ(splines.size(), 2u);
    CHECK_EQ(r.ring(splines[1]),
             (std::vector<Point2>{
                 {20'000, 100'000}, {25'000, 105'000}, {30'000, 100'000}, {35'000, 105'000}}));
    const auto blocks = two_of(core::kBlockReferenceKind);
    REQUIRE_EQ(blocks.size(), 2u);
    CHECK_EQ(
        core::block_reference_insertion(r.doc.geometry(), r.doc.entities().slot[r.slot(blocks[1])]),
        (Point2{40'000, 100'000}));
    const auto dims = two_of(core::kDimensionKind);
    REQUIRE_EQ(dims.size(), 2u);
    CHECK_EQ(r.text(dims[1]), r.text(dims[0]));
}

TEST_CASE("C-08: DİZİ spline'ı spline olarak çoğaltır")
{
    Rig r;
    r.run("SPLINE noktalar=0,0 noktalar=5,5 noktalar=10,0 noktalar=15,5");
    r.run("DİZİ nesneler=1 satir=1 sutun=3 satir_aralik=0 sutun_aralik=20");
    CHECK_EQ(r.kind(2), core::kSplineKind);
    CHECK_EQ(r.kind(3), core::kSplineKind);
    CHECK_EQ(r.ring(3)[0], (Point2{40'000, 0}));
}

TEST_CASE("C-08: ÖLÇEKLE yazının harflerini de büyütür; AYNALA yazıyı okunur bırakır")
{
    Rig r;
    r.run("METİN noktalar=10,10 yazi=ADA yukseklik=2");
    const core::Mm h = r.text_height(1);
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2");
    CHECK_EQ(r.text_height(1), 2 * h);
    CHECK_EQ(r.ring(1)[0], (Point2{20'000, 20'000}));

    // Mirrored in a vertical line: the anchor goes across, and the caption
    // still runs left to right rather than upside down.
    r.run("AYNALA nesneler=1 baslangic=50,0 bitis=50,10");
    const auto base = r.ring(1);
    CHECK_EQ(base[0], (Point2{80'000, 20'000}));
    CHECK(base[1].x > base[0].x);
    CHECK_EQ(base[1].y, base[0].y);
}

TEST_CASE("C-08: yarım dönen ya da aynalanan ölçünün yazısı okunur kalır; ölçeklenen ölçü yeni "
          "sayıyı söyler")
{
    Rig r;
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=10,0 konum=0,2");
    const std::string said = r.text(1);
    CHECK(said.find("10") != std::string::npos);
    const core::Mm h = r.text_height(1);

    r.run("DÖNDÜR nesneler=1 merkez=0,0 aci=180");
    auto caption = r.ring(1, 0);
    CHECK(caption[1].x >= caption[0].x); ///< reads left to right, not upside down
    CHECK_EQ(r.text(1), said);

    r.run("AYNALA nesneler=1 baslangic=0,-50 bitis=0,50");
    caption = r.ring(1, 0);
    CHECK(caption[1].x >= caption[0].x);
    CHECK_EQ(r.text(1), said);

    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2");
    CHECK(r.text(1).find("20") != std::string::npos);
    CHECK_EQ(r.text_height(1), 2 * h);
}

TEST_CASE("C-08: HİZALA her türü hizalar; daire ölçeklenince daire kalır, yazı döner")
{
    Rig r;
    r.run("DAİRE merkez=10,0 cevre=12,0");                 // 1: radius 2
    r.run("METİN noktalar=0,0 yazi=KUZEY yukseklik=1000"); // 2
    r.run("SEÇ HEPSİ");
    // (0,0)→(0,0), (10,0)→(0,20): a quarter turn and twice the size.
    r.run("HİZALA kaynak=0,0 hedef=0,0 kaynak2=10,0 hedef2=0,20 olcekle=evet");
    CHECK_EQ(r.kind(1), core::kCircleKind);
    CHECK_EQ(core::circle_centre_of(r.doc.geometry(), r.doc.entities().slot[r.slot(1)]),
             (Point2{0, 20'000}));
    CHECK_EQ(core::circle_radius_of(r.doc.geometry(), r.doc.entities().slot[r.slot(1)]),
             core::Mm{4'000});
    const auto base = r.ring(2);
    CHECK_EQ(base[0], (Point2{0, 0}));
    CHECK(std::abs(base[1].x) <= 1); ///< the caption now runs north
    CHECK(base[1].y > 0);
    CHECK(r.said.find("taşındı, döndürüldü ve ölçeklendi") != std::string::npos);
}

TEST_CASE("C-08: üç noktalı HİZALA, üçüncü hedef öbür yandaysa nesneyi ters çevirir")
{
    Rig r;
    r.run("ALAN 0,0 10,0 0,5");
    r.run("HİZALA nesne=1 kaynak=0,0 hedef=100,0 kaynak2=10,0 hedef2=110,0 kaynak3=0,5 "
          "hedef3=100,-5");
    const auto face = r.ring(1);
    bool below      = false;
    for (const Point2 p : face)
        below = below || p == Point2{100'000, -5'000};
    CHECK(below);
    // Still a face with a positive area: the winding was put back.
    CHECK(r.doc.geometry().area_of(r.doc.entities().slot[r.slot(1)]) > 0);
    CHECK(r.said.find("ters çevrildi") != std::string::npos);

    // On the same side, no turn over.
    Rig same;
    same.run("ALAN 0,0 10,0 0,5");
    same.run("HİZALA nesne=1 kaynak=0,0 hedef=100,0 kaynak2=10,0 hedef2=110,0 kaynak3=0,5 "
             "hedef3=100,5");
    bool above = false;
    for (const Point2 p : same.ring(1))
        above = above || p == Point2{100'000, 5'000};
    CHECK(above);
}

TEST_CASE("C-08: eksi ölçek reddedilir; tekrarlı kopya her noktaya bir kopya koyar")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    const std::uint64_t before = r.doc.content_hash();
    auto negative = r.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=-1", Origin::Test);
    REQUIRE_FALSE(negative.ok());
    CHECK(negative.error().message.find("AYNALA") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), before);

    r.run("KOPYALA nesneler=1 baslangic=0,0 bitis=0,10 bitis=0,20 bitis=0,30");
    CHECK_EQ(r.doc.live_entity_count(), 4u);
    CHECK_EQ(r.ring(4)[0], (Point2{0, 30'000}));
    CHECK(r.said.find("3 nesne kopyalandı (3 yere).") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), 2u); ///< the line and ONE copy step
}

// =============================================================================
// By reference, unevenly, and as a copy
// =============================================================================

TEST_CASE("C-08: DÖNDÜR referansla — referans doğrultu yenisine döner")
{
    // A wall along 30° turned onto 90°: a turn of 60°.
    Rig typed;
    typed.run("ÇİZGİ 10,0 20,0");
    typed.run("DÖNDÜR nesneler=1 merkez=0,0 referans=30 aci=90");
    const auto a = typed.ring(1);
    CHECK(gap(a[0], {5'000, 8'660}) <= 1.5); ///< (10, 0) turned 60°

    // The reference shown by two points along the wall (45°), onto 90°: 45°.
    Rig shown;
    shown.run("ÇİZGİ 10,0 20,0");
    shown.run("DÖNDÜR nesneler=1 merkez=0,0 yontem=referans referans_nokta=0,0 "
              "referans_nokta=10,10 aci=90");
    CHECK(gap(shown.ring(1)[0], {7'071, 7'071}) <= 1.5);
    // The journal holds the turn, not how it was found.
    const std::string line = shown.journal.entries().back().args.to_json().dump();
    CHECK(line.find("\"aci\":45") != std::string::npos);
    CHECK(line.find("referans") == std::string::npos);
}

TEST_CASE("C-08: ÖLÇEKLE referansla — referans uzunluk yenisi olur")
{
    Rig r;
    r.run("ÇİZGİ 0,0 5,0");
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 referans=5 yeni=12");
    CHECK_EQ(r.ring(1)[1], (Point2{12'000, 0}));
    CHECK(r.journal.entries().back().args.to_json().dump().find("\"carpan\":2.4") !=
          std::string::npos);
}

TEST_CASE("C-08: eşit olmayan ölçek daireyi aynı kimlikle elipse, yayı eliptik yaya çevirir")
{
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=10,0");                // 1: r = 10
    r.run("YAY merkez=30,0 baslangic=40,0 bitis=30,10"); // 2: a quarter
    r.run("ÖLÇEKLE nesneler=1 2 merkez=0,0 carpan=1 carpan_y=2");
    // The circle is the ellipse it now is, by the same key: its first axis
    // the longer, up; its second across, both perpendicular.
    REQUIRE_EQ(r.kind(1), core::kEllipseKind);
    const auto e = r.ring(1);
    CHECK_EQ(e[0], (Point2{0, 0}));
    CHECK(gap(e[0], e[1]) == doctest::Approx(20'000.0).epsilon(1e-6));
    CHECK(gap(e[0], e[2]) == doctest::Approx(10'000.0).epsilon(1e-6));
    const double dot = static_cast<double>(e[1].x - e[0].x) * static_cast<double>(e[2].x - e[0].x) +
                       static_cast<double>(e[1].y - e[0].y) * static_cast<double>(e[2].y - e[0].y);
    CHECK(std::abs(dot) <= 1.0e4);
    // The quarter arc became the matching elliptic arc: from (40,0) up to (30,20).
    REQUIRE_EQ(r.kind(2), core::kEllipseKind);
    const auto arc = core::ellipse_arc_of(r.doc.geometry(), r.doc.entities().slot[r.slot(2)]);
    REQUIRE(arc.has_value());
    CHECK(r.said.find("yazıların yüksekliği") != std::string::npos);
}

TEST_CASE("C-08: eşit olmayan ölçek yaylı çizgiyi ve dönük bloğu açıklayarak reddeder")
{
    Rig r;
    r.run("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10");
    r.run("YUVARLA nesne=1 hepsi=evet yaricap=2"); // an arc polyline
    const std::uint64_t before = r.doc.content_hash();
    auto bent =
        r.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=1 carpan_y=2", Origin::Test);
    REQUIRE_FALSE(bent.ok());
    CHECK(bent.error().message.find("yaylı bir çoklu çizgi") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), before);

    Rig b;
    b.run("ÇİZGİ 0,0 10,0");
    b.run("BLOK ad=OK taban=0,0 nesneler=1");
    const std::int64_t symbol = first_of(b, core::kBlockReferenceKind);
    REQUIRE(symbol != 0);
    const std::string named = "nesneler=" + std::to_string(symbol);
    // Square: a stretch it can hold, across its own axes.
    b.run("ÖLÇEKLE " + named + " merkez=0,0 carpan=2 carpan_y=3");
    auto ref = core::block_reference_of(b.doc.geometry(), b.doc.entities().slot[b.slot(symbol)]);
    REQUIRE(ref.ok());
    CHECK_EQ(static_cast<double>(ref.value().sx.num) / static_cast<double>(ref.value().sx.den),
             doctest::Approx(2.0));
    CHECK_EQ(static_cast<double>(ref.value().sy.num) / static_cast<double>(ref.value().sy.den),
             doctest::Approx(3.0));
    // Turned 30°: it would lean, and says so.
    b.run("DÖNDÜR " + named + " merkez=0,0 aci=30");
    auto leaning =
        b.bus.execute_line("ÖLÇEKLE " + named + " merkez=0,0 carpan=2 carpan_y=1", Origin::Test);
    REQUIRE_FALSE(leaning.ok());
    CHECK(leaning.error().message.find("döndürülmüş bir blok") != std::string::npos);
}

TEST_CASE("C-08: kopya=evet özgünü yerinde bırakır, dönüşmüş kopyayı ekler")
{
    Rig r;
    r.run("ÇİZGİ 10,0 20,0");
    r.run("DÖNDÜR nesneler=1 merkez=0,0 aci=90 kopya=evet");
    CHECK_EQ(r.doc.live_entity_count(), 2u);
    CHECK_EQ(r.ring(1)[0], (Point2{10'000, 0})); ///< the original stays
    CHECK_EQ(r.ring(2)[0], (Point2{0, 10'000}));
    CHECK(r.said.find("döndürülmüş kopyası çizildi") != std::string::npos);
    r.run("AYNALA nesneler=1 baslangic=0,-5 bitis=0,5 kopya=evet");
    CHECK_EQ(r.doc.live_entity_count(), 3u);
    CHECK_EQ(r.ring(3)[0], (Point2{-10'000, 0}));
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2 kopya=evet");
    CHECK_EQ(r.ring(4)[1], (Point2{40'000, 0}));
    CHECK_EQ(r.undo.undo_depth(), 4u); ///< each one step
}

TEST_CASE("C-08: DİZİ yol boyunca — kopyalar eşit aralıkla dizilir, yolun doğrultusuna döner")
{
    // A straight path and a pole at its start: three copies at 10, 20, 30 m.
    Rig straight;
    straight.run("ÇİZGİ 0,0 30,0");
    straight.run("ÇİZGİ 0,0 0,2");
    straight.run("DİZİ nesneler=2 mod=yol yol=1 sayi=4");
    CHECK_EQ(straight.doc.live_entity_count(), 5u);
    CHECK_EQ(straight.ring(5), (std::vector<Point2>{{30'000, 0}, {30'000, 2'000}}));
    CHECK(straight.said.find("3 kopya yol boyunca dizildi") != std::string::npos);

    // A quarter arc: halfway round the copy is turned 45° and points outward
    // still; at the end it points north.
    Rig bend;
    bend.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    bend.run("ÇİZGİ 10,0 12,0");
    bend.run("DİZİ nesneler=2 mod=yol yol=1 sayi=3");
    const auto half = bend.ring(3);
    CHECK(gap(half[0], {7'071, 7'071}) <= 2.0);
    CHECK(gap(half[1], {8'485, 8'485}) <= 2.0); ///< radial, 2 m further out
    const auto end = bend.ring(4);
    CHECK(gap(end[0], {0, 10'000}) <= 2.0);
    CHECK(gap(end[1], {0, 12'000}) <= 2.0);

    // By spacing, and without turning.
    Rig spaced;
    spaced.run("YAY merkez=0,0 baslangic=10,0 bitis=0,10");
    spaced.run("ÇİZGİ 10,0 12,0");
    spaced.run("DİZİ nesneler=2 mod=yol yol=1 aralik=5 hizala=hayir");
    // The arc is 15.708 m: stations at 0, 5, 10 and 15 m — three copies,
    // each still pointing east.
    CHECK_EQ(spaced.doc.live_entity_count(), 5u);
    const auto kept = spaced.ring(5);
    CHECK_EQ(kept[1].y, kept[0].y);
    CHECK_EQ(kept[1].x - kept[0].x, 2'000);
}

TEST_CASE("C-08: dönüşüm sayfalarının yeni örnekleri yazıldığı gibi çalışır")
{
    // docs/komutlar/scale.md, rotate.md, mirror.md, array.md and align.md print
    // these lines and these answers; each example starts a new drawing.
    const auto said = [](std::initializer_list<const char*> lines) {
        Rig r;
        for (const char* line : lines)
            r.run(line);
        return r.said;
    };
    CHECK(said({"ÇİZGİ 0,0 5,0", "ÖLÇEKLE nesneler=1 merkez=0,0 referans=5 yeni=12"})
              .find("1 nesne ölçeklendi.") != std::string::npos);
    CHECK(said({"DAİRE merkez=0,0 cevre=10,0", "ÖLÇEKLE nesneler=1 merkez=0,0 carpan=1 carpan_y=2"})
              .find("1 nesne ölçeklendi.") != std::string::npos);
    CHECK(said({"ÇİZGİ 10,0 20,0", "DÖNDÜR nesneler=1 merkez=0,0 referans=30 aci=90"})
              .find("1 nesne döndürüldü.") != std::string::npos);
    CHECK(said({"ÇİZGİ 10,0 20,0", "AYNALA nesneler=1 baslangic=0,-5 bitis=0,5 kopya=evet"})
              .find("1 nesnenin aynalanmış kopyası çizildi.") != std::string::npos);
    CHECK(said({"ÇİZGİ 0,0 30,0", "ÇİZGİ 0,0 0,2", "DİZİ nesneler=2 mod=yol yol=1 sayi=4"})
              .find("3 kopya yol boyunca dizildi, her biri yolun doğrultusuna döndürüldü.") !=
          std::string::npos);
    CHECK(said({"ALAN 0,0 10,0 0,5",
                "HİZALA nesne=1 kaynak=0,0 hedef=100,0 kaynak2=10,0 hedef2=110,0 kaynak3=0,5 "
                "hedef3=100,-5"})
              .find("1 nesne hizalandı (taşındı ve döndürüldü; üçüncü nokta öbür yana düştüğü "
                    "için ters çevrildi).") != std::string::npos);
}

TEST_CASE(
    "C-08: tam sayı istemine yazılan sayı tam sayı olarak kaydedilir; kesirli olan reddedilir")
{
    // The command line hands every typed figure over as a number — `3` as 3.0.
    // Recorded so, the check after the body refused the run it had just
    // finished and DİZİ rolled back every copy it had made.
    Rig r;
    r.run("ÇİZGİ 0,0 0,2");
    auto started =
        r.bus.begin_interactive("DİZİ nesneler=1 satir_aralik=5 sutun_aralik=5", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "satir");
    // A fraction at a prompt for a count is said to be wrong, and the prompt stays.
    auto half = s.supply(Value::number(2.5));
    REQUIRE_FALSE(half.ok());
    CHECK(half.error().message.find("tam sayı") != std::string::npos);
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::number(2.0)).ok());
    REQUIRE(s.supply(Value::number(3.0)).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(r.doc.live_entity_count(), 6u);
    const std::string line = r.journal.entries().back().args.to_json().dump();
    CHECK(line.find("\"satir\":2,") != std::string::npos);
    CHECK(line.find("\"sutun\":3") != std::string::npos);
}
