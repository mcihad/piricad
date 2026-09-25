// SPDX-License-Identifier: GPL-3.0-or-later
//
// MULTI-LINE TEXT, NINE ALIGNMENTS, LINE SPACING (TODOS C-12).
//
// The layout rules are checked with a ruler of the suite's own — every letter
// six tenths of a height wide — because the rules are the point: where the
// second line goes and where a centred line starts must not depend on which
// backend drew it, and a backend is exactly what this suite cannot construct.
// The same function answers the GPU canvas and the QPainter path the PDF is
// printed through (render/text_layout.hpp).
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/render/text_layout.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::Point2;
using core::TextAnchor;

namespace {

/// Every letter six tenths of a capital height: a ruler, not a font.
float ruler(std::string_view run)
{
    return 0.6f * static_cast<float>(core::text_characters(run));
}

std::vector<render::TextLine> lay(std::string_view text, TextAnchor anchor,
                                  core::TextLines lines = {})
{
    std::vector<render::TextLine> out;
    render::lay_out_text(text, 10.0f, anchor, lines.spacing, &ruler, out);
    return out;
}

/// The lines `core::text_lines` sets `text` in at a height of one metre.
std::vector<std::string> broken(std::string_view text, core::Mm width, bool wrap = true)
{
    std::vector<std::string_view> views;
    core::text_lines(text, 1000, core::TextLines{1000, wrap}, width, views);
    return {views.begin(), views.end()};
}

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

    bool refused(const std::string& line) { return !bus.execute_line(line, Origin::Test).ok(); }

    /// The newest text entity.
    core::EntityId text() const
    {
        core::EntityId last = core::kNoEntity;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.texts().has(doc.entities().slot[e])) last = e;
        return last;
    }

    std::uint32_t slot(core::EntityId e) const { return doc.entities().slot[e]; }

    std::array<Point2, 2> baseline(core::EntityId e) const
    {
        const core::RingSpan span = doc.geometry().rings_of(slot(e));
        return {doc.geometry().vertex(span.first, 0), doc.geometry().vertex(span.first, 1)};
    }
};

} // namespace

// ------------------------------------------------------------------ layout ----

TEST_CASE("YAZI DÜZENİ: satırlar 5/3 yükseklik arayla; boş satır kalır; satır aralığı çarpar")
{
    // "3 on 5", DXF MTEXT's own pitch: a text 10 px tall steps 16,67 px.
    const auto one = lay("ADA\n\nPARSEL", TextAnchor::TopLeft);
    REQUIRE_EQ(one.size(), std::size_t{3});
    CHECK(one[1].text.empty()); // a blank line is how two paragraphs stand apart
    CHECK(one[0].v == doctest::Approx(10.0f));
    CHECK(one[1].v - one[0].v == doctest::Approx(50.0f / 3.0f));
    CHECK(one[2].v - one[1].v == doctest::Approx(50.0f / 3.0f));

    const auto wide = lay("A\nB", TextAnchor::TopLeft, core::TextLines{1500, false});
    CHECK(wide[1].v - wide[0].v == doctest::Approx(25.0f));
}

TEST_CASE("YAZI DÜZENİ: dokuz hizalama — sütun satır başına, sıra bloğa göre")
{
    // Rows: TOP puts the first line's capital tops on the anchor, MIDDLE the
    // block's middle, BASELINE the last line's baseline — DXF MTEXT's rows, and
    // for one line TEXT's own baseline, middle and top.
    for (const TextAnchor a : {TextAnchor::TopLeft, TextAnchor::TopCentre, TextAnchor::TopRight}) {
        const auto l = lay("İKİ\nSATIR", a);
        CHECK(l[0].v == doctest::Approx(10.0f));
    }
    for (const TextAnchor a :
         {TextAnchor::MiddleLeft, TextAnchor::MiddleCentre, TextAnchor::MiddleRight}) {
        const auto l    = lay("İKİ\nSATIR", a);
        const float top = l[0].v - 10.0f; // the first line's capital tops
        CHECK(top == doctest::Approx(-l[1].v));
    }
    for (const TextAnchor a :
         {TextAnchor::BaselineLeft, TextAnchor::BaselineCentre, TextAnchor::BaselineRight}) {
        const auto l = lay("İKİ\nSATIR", a);
        CHECK(l[1].v == doctest::Approx(0.0f)); // the last baseline on the anchor
        CHECK(l[0].v < 0.0f);                   // and the first above it
    }
    // One line: middle is half a capital height down from the top.
    CHECK(lay("ADA", TextAnchor::MiddleLeft)[0].v == doctest::Approx(5.0f));

    // Columns, line by line: each line of a centred block is centred on its own.
    const auto centred = lay("ADA\nPARSELLER", TextAnchor::TopCentre);
    CHECK(centred[0].u == doctest::Approx(-centred[0].advance * 0.5f));
    CHECK(centred[1].u == doctest::Approx(-centred[1].advance * 0.5f));
    const auto right = lay("ADA\nPARSELLER", TextAnchor::BaselineRight);
    CHECK(right[0].u + right[0].advance == doctest::Approx(0.0f));
    CHECK(right[1].u + right[1].advance == doctest::Approx(0.0f));
    CHECK(lay("ADA", TextAnchor::BaselineLeft)[0].u == doctest::Approx(0.0f));
}

TEST_CASE("YAZI DÜZENİ: genişliğe göre kelime sınırından kırılır; tek uzun kelime kendi satırında")
{
    // THE ONE PLACE A LINE BREAKS is the core (`core::text_lines`), by the face's
    // own advances (TODOS C-18). At a height of one metre "bu plan" is
    // 580+568+236+580+272+534+568 = 3338 units of a 698-unit capital, 4782 mm;
    // "bu plan notu" is 5621 units, 8053 mm. A 5 m width takes the first two
    // words and puts "notu" under them.
    const auto l = broken("bu plan notu", 5000);
    REQUIRE_EQ(l.size(), std::size_t{2});
    CHECK_EQ(l[0], std::string("bu plan"));
    CHECK_EQ(l[1], std::string("notu"));

    // EXACT, in integers: 3338 units at 1000 mm need 3338000 / 698 = 4782,2 mm,
    // so 4782 is a hair too narrow for "bu plan" and 4783 holds it.
    CHECK_EQ(broken("bu plan notu", 4782).size(), std::size_t{3});
    CHECK_EQ(broken("bu plan notu", 4783).size(), std::size_t{2});

    // A word wider than the width is not broken inside itself.
    const auto long_word = broken("imar yapılaşmakoşulları", 5000);
    REQUIRE_EQ(long_word.size(), std::size_t{2});
    CHECK_EQ(long_word[1], std::string("yapılaşmakoşulları"));

    // Without wrap the width breaks nothing; an explicit break still breaks, and
    // a blank line between two paragraphs is kept.
    CHECK_EQ(broken("bu plan notu", 5000, false).size(), std::size_t{1});
    CHECK_EQ(broken("bir\niki", 50'000).size(), std::size_t{2});
    CHECK_EQ(broken("bir\n\niki", 50'000).size(), std::size_t{3});
    CHECK_EQ(core::text_line_count("bu plan notu", 1000, core::TextLines{1000, true}, 5000),
             std::size_t{2});
}

// ------------------------------------------------------------------- model ----

TEST_CASE("YAZI MODELİ: varsayılan satır düzeni izi değiştirmez; aralık ve kırılma değiştirir")
{
    // A drawing of one-line captions hashes as it did before line layout
    // existed: every fixture written before stays what it was.
    core::TextTable plain;
    core::TextTable spaced;
    plain.resize(1);
    spaced.resize(1);
    REQUIRE(plain.set(0, "ADA", 2500, TextAnchor::BaselineLeft).ok());
    REQUIRE(spaced.set(0, "ADA", 2500, TextAnchor::BaselineLeft, core::TextLines{}).ok());
    CHECK_EQ(plain.fold(1), spaced.fold(1));

    REQUIRE(
        spaced.set(0, "ADA", 2500, TextAnchor::BaselineLeft, core::TextLines{1500, false}).ok());
    CHECK_NE(plain.fold(1), spaced.fold(1));
    CHECK_EQ(spaced.lines(0).spacing, 1500);

    // Out of DXF's range, and past the ninth anchor: refused, not stored.
    CHECK_FALSE(spaced.set(0, "ADA", 2500, TextAnchor::BaselineLeft, core::TextLines{100, false}));
    CHECK_FALSE(spaced.set(0, "ADA", 2500, static_cast<TextAnchor>(9)));

    // Letters, not bytes: "ŞİŞLİ" is five letters and ten bytes.
    CHECK_EQ(core::text_characters("ŞİŞLİ"), std::size_t{5});
    // Each letter at the face's own advance over its 698-unit capital: Ş 581,
    // İ 400, L 501 — 2463 units, 3528,7 mm at a metre. The longer of two lines
    // is the width; ADA is 1953 units.
    CHECK_EQ(core::text_width("ŞİŞLİ\nADA", 1000), core::Mm{3529});
    CHECK_EQ(core::text_width("şişli", 1000), core::Mm{2501}); // ş 487, i 250, l 272
}

TEST_CASE("YAZI MODELİ: seçim kutusu hizalamayı ve satırları izler")
{
    // A centred caption is picked where it is drawn — around its point, not to
    // the right of it — and a hanging two-line note below its point.
    Rig r;
    r.run("METİN noktalar=100,100 yazi=\"ADA 12\" yukseklik=2000 hizalama=merkez");
    std::array<Point2, 4> quad{};
    REQUIRE(core::text_quad(r.doc, r.text(), quad));
    core::Box2 box;
    for (const Point2 p : quad)
        box.extend(p);
    CHECK(box.min_x < 100'000);
    CHECK(box.max_x > 100'000);
    CHECK((box.min_x + box.max_x) / 2 == doctest::Approx(100'000).epsilon(0.001));

    r.run("METİN noktalar=0,0 yazi=\"PLAN\\nNOTU\" yukseklik=1000 hizalama=ust_sol");
    REQUIRE(core::text_quad(r.doc, r.text(), quad));
    core::Box2 note;
    for (const Point2 p : quad)
        note.extend(p);
    CHECK_EQ(note.max_y, 0); // hanging from the point
    // Two lines: the capital height, one pitch and half a height for descenders.
    CHECK(-note.min_y == doctest::Approx(1000 + 5000.0 / 3.0 + 500).epsilon(0.002));
}

TEST_CASE("YAZI KUTUSU: taban çizgisi her düzenlemeden sonra kelimeler kadar; tutamakla çekilen "
          "uç yalnız yönü verir")
{
    // TODOS C-18: a text's box is its words. The baseline of a text that does
    // not wrap is as long as `core::text_width` says, whatever edit touched it
    // — the bus settles it at commit (`Transaction::settle_texts`), for every
    // client alike.
    Rig r;
    r.run("METİN noktalar=0,0 yazi=ADA yukseklik=1000");
    const core::EntityId e = r.text();
    CHECK_EQ(r.baseline(e)[1], (Point2{2'798, 0})); // 1953 units over a 698-unit capital

    // The end grip dragged far up and to the right: the words turn toward it,
    // and the box stays the words — 2798 mm along the diagonal, not 42 m.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=30,30");
    const auto turned = r.baseline(e);
    CHECK_EQ(turned[0], (Point2{0, 0}));
    CHECK_EQ(turned[1], (Point2{1'978, 1'978})); // 2798 / √2, rounded
    CHECK_LE(std::llabs(core::text_baseline_length(turned[0], turned[1]) - 2'798), 1);

    // Scaled, the height and the words' width grow together.
    r.run("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2");
    CHECK_EQ(r.doc.texts().height(r.slot(e)), 2'000);
    const auto grown = r.baseline(e);
    CHECK_LE(
        std::llabs(core::text_baseline_length(grown[0], grown[1]) - core::text_width("ADA", 2'000)),
        1);

    // A second commit that touches it writes nothing: within a millimetre is so.
    const std::size_t depth = r.undo.undo_depth();
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=1,0");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    const auto moved = r.baseline(e);
    CHECK_EQ(moved[1].x - moved[0].x, grown[1].x - grown[0].x);
    CHECK_EQ(moved[1].y - moved[0].y, grown[1].y - grown[0].y);
}

// ---------------------------------------------------------------- commands ----

TEST_CASE("METİN: dokuz hizalama sözcüğü, satır aralığı ve genişlik; bilinmeyen sözcük reddedilir")
{
    Rig r;
    for (const char* word : {"sol", "orta", "sag", "merkez", "ust_sol", "ust_orta", "ust_sag",
                             "orta_sol", "orta_sag"}) {
        r.run(std::string("METİN noktalar=0,0 yazi=A hizalama=") + word);
        CHECK_EQ(std::string(core::text_anchor_name(r.doc.texts().anchor(r.slot(r.text())))),
                 std::string(word));
    }
    CHECK(r.refused("METİN noktalar=0,0 yazi=A hizalama=yukari"));

    r.run("METİN noktalar=0,0 yazi=\"uzun bir plan notu\" yukseklik=2000 satir_araligi=1.5 "
          "genislik=12");
    const core::EntityId note   = r.text();
    const core::TextLines lines = r.doc.texts().lines(r.slot(note));
    CHECK_EQ(lines.spacing, 1500);
    CHECK(lines.wrap);
    // The baseline runs for the width the lines break to.
    const auto base = r.baseline(note);
    CHECK_EQ(base[1].x - base[0].x, 12'000);
    CHECK(r.refused("METİN noktalar=0,0 yazi=A satir_araligi=5"));

    // The width of a text that does not wrap is its letters at the face's own
    // advances (TODOS C-18): Ş 581, İ 400, Ş 581, L 501, İ 400 — 2463 units of
    // a 698-unit capital, 3528,7 mm at a metre.
    r.run("METİN noktalar=0,0 yazi=ŞİŞLİ yukseklik=1000");
    CHECK_EQ(r.baseline(r.text())[1].x, 3'529);
}

TEST_CASE("YAZIDÜZENLE: satır aralığı ve genişlik değişir; taban çizgisi yeni yazıya uzar; geri "
          "alma tek adım")
{
    Rig r;
    r.run("METİN noktalar=0,0 yazi=ADA yukseklik=1000");
    const core::EntityId e = r.text();
    CHECK_EQ(r.baseline(e)[1].x, 2'798); // A 641, D 671, A 641: 1953 units over 698

    r.run("YAZIDÜZENLE nesneler=1 yazi=\"ADA 128 PARSEL 4\"");
    CHECK_EQ(r.baseline(e)[1].x, 12'340); // 8613 units: nine capitals, four digits, three spaces
    const std::size_t depth = r.undo.undo_depth();
    r.run("YAZIDÜZENLE nesneler=1 satir_araligi=2 genislik=5");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    CHECK_EQ(r.doc.texts().lines(r.slot(e)).spacing, 2000);
    CHECK(r.doc.texts().lines(r.slot(e)).wrap);
    CHECK_EQ(r.baseline(e)[1].x, 5'000);

    r.run("YAZIDÜZENLE nesneler=1 genislik=0");
    CHECK_FALSE(r.doc.texts().lines(r.slot(e)).wrap);
    CHECK_EQ(r.doc.texts().lines(r.slot(e)).spacing, 2000); // not asked, not changed

    r.run("GERİAL");
    r.run("GERİAL");
    CHECK_EQ(r.doc.texts().lines(r.slot(e)).spacing, 1000);
    CHECK_FALSE(r.doc.texts().lines(r.slot(e)).wrap);
}

TEST_CASE("METİN KANIT: çok satırlı, hizalı yazı komut satırından ve betikten aynı belge ve "
          "aynı günlük")
{
    // CLAUDE.md 6.4: the same caption from the command line and from a JSON
    // script — the newline, the alignment, the spacing, the width — leaves one
    // document and one journal line.
    Rig line;
    line.run("METİN noktalar=10,20 yazi=\"PLAN\\nNOTU\" yukseklik=1500 hizalama=ust_orta "
             "satir_araligi=1.25 genislik=8");

    Rig scr;
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"json({"komutlar":[{"cmd":"core.text","args":{
        "noktalar":[[10000,20000]],"yazi":"PLAN\nNOTU","yukseklik":1500,
        "hizalama":"ust_orta","satir_araligi":1.25,"genislik":8}}]})json");
    CHECK(ran.ok());

    CHECK_EQ(line.doc.content_hash(), scr.doc.content_hash());
    REQUIRE_EQ(line.journal.entries().size(), scr.journal.entries().size());
    CHECK_EQ(line.journal.entries().back().args.to_json().dump(),
             scr.journal.entries().back().args.to_json().dump());
    CHECK_EQ(std::string(line.doc.texts().text(line.slot(line.text()))), std::string("PLAN\nNOTU"));
}
