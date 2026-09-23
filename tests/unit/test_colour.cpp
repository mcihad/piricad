// SPDX-License-Identifier: GPL-3.0-or-later
//
// RENK — the stroke and fill colour of the objects in hand.
//
// The two colour chips at the foot of the tool column answered "RENK komutu Faz
// 2" for as long as they existed. This file pins the command behind them: what
// it changes, what it hands back to the layer, what it leaves alone in a symbol
// stack, and what it refuses.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/colour.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <string>

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

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r.ok()) FAIL_WITH(line, r.error().message);
    }

    core::EntityId slot(std::int64_t key) const
    {
        const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
        REQUIRE(e != core::kNoEntity);
        return e;
    }

    const core::Appearance& look(std::int64_t key) const
    {
        return doc.styles().at(doc.entities().style[slot(key)]);
    }

    const core::Symbol& symbol(std::int64_t key) const
    {
        return doc.styles().symbol_at(doc.entities().style[slot(key)]);
    }
};

} // namespace

TEST_CASE("RENK: adlanan nesnenin çizgi rengini değiştirir, ötekine dokunmaz")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1
    r.run("ÇİZGİ 0,5 10,5"); // 2
    r.run("RENK nesneler=1 renk=#C0392B");

    CHECK_EQ(r.look(1).rgba, 0xFFC0392Bu);
    CHECK(r.look(1).src_colour == core::Source::Explicit);
    // The rest of the look still follows the layer: only the colour was said.
    CHECK(r.look(1).src_width == core::Source::ByLayer);
    CHECK(r.look(1).src_fill == core::Source::ByLayer);
    CHECK_EQ(r.doc.entities().style[r.slot(2)], core::kByLayerStyle);
    CHECK(r.echoed.find("1 nesnenin rengi değişti (çizgi #C0392B).") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), 3u);
}

TEST_CASE("RENK: renk yazımları — #RRGGBB, #AARRGGBB, 0xAARRGGBB")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1
    r.run("RENK nesneler=1 renk=#00FF00");
    CHECK_EQ(r.look(1).rgba, 0xFF00FF00u);
    // The command line reads 0x… as a number; its value is the integer form
    // STİL takes, and it is kept because it carries an alpha byte.
    r.run("RENK nesneler=1 renk=0x800000FF");
    CHECK_EQ(r.look(1).rgba, 0x800000FFu);
    r.run("RENK nesneler=1 renk=#40FF8800");
    CHECK_EQ(r.look(1).rgba, 0x40FF8800u);
    r.run("RENK nesneler=1 renk=#ff8800");
    CHECK_EQ(r.look(1).rgba, 0xFFFF8800u);
    r.run("RENK nesneler=1 renk=4290787627"); // STİL's integer spelling of #C0392B
    CHECK_EQ(r.look(1).rgba, 0xFFC0392Bu);

    // A BARE NUMBER WITHOUT AN ALPHA BYTE could be hex that lost its `#`: the
    // line turned `112233` into a number, and taking it as one would paint a
    // colour nobody typed. Refused, and the message says why.
    auto bare = r.bus.execute_line("RENK nesneler=1 renk=112233", Origin::Test);
    REQUIRE_FALSE(bare.ok());
    CHECK(bare.error().message.find("Sayı olarak okundu; rengi # ile yazın.") != std::string::npos);
    auto hex = r.bus.execute_line("RENK nesneler=1 renk=FF0000", Origin::Test);
    REQUIRE_FALSE(hex.ok());
    CHECK(hex.error().message.find("#RRGGBB") != std::string::npos);
    CHECK_EQ(r.look(1).rgba, 0xFFC0392Bu);
}

TEST_CASE("RENK: nesne katmanın çizdiği görünümden başlar — kalınlık ve öteki renk yerinde")
{
    // THE RENDERER DRAWS A STYLE'S VALUES AS THEY STAND (model.md R14). An
    // object that stops inheriting has to carry what its layer drew, or giving
    // a red parcel a green fill would also turn its boundary black and hairline.
    Rig r;
    r.run("KATMAN ad=YOL");
    r.run("STİL katman=YOL renk=0xFFCC0000 kalinlik=700");
    r.run("ALAN 0,0 10,0 10,10 0,10"); // 1, on YOL, inheriting
    REQUIRE_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);

    r.run("RENK nesneler=1 dolgu=#00FF00");
    CHECK_EQ(r.look(1).fill_rgba, 0xFF00FF00u);
    CHECK_EQ(r.look(1).rgba, 0xFFCC0000u);
    CHECK_EQ(r.look(1).width_um, 700);

    // And handing the fill back makes it the layer's object again.
    r.run("RENK nesneler=1 dolgu=katman");
    CHECK_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);
}

TEST_CASE("RENK: renk adları her yazımla okunur, günlüğe onaltılık yazılır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1
    for (const char* spelled : {"kırmızı", "KIRMIZI", "kirmizi", "Kırmızı"}) {
        r.run(std::string("RENK nesneler=1 renk=siyah"));
        r.echoed.clear();
        r.run(std::string("RENK nesneler=1 renk=") + spelled);
        CHECK_MESSAGE(r.look(1).rgba == 0xFFFF0000u, spelled);
        CHECK_MESSAGE(r.echoed.find("(çizgi kırmızı (#FF0000))") != std::string::npos, spelled);
        CHECK_EQ(r.journal.entries().back().args.get("renk").as_text(), std::string("#FF0000"));
    }
}

TEST_CASE("renk sözcükleri: çözümleme, yazım ve ad listesi")
{
    CHECK_EQ(parse_colour("#C0392B").value_or(0), 0xFFC0392Bu);
    CHECK_EQ(parse_colour("#80C0392B").value_or(0), 0x80C0392Bu);
    CHECK_EQ(parse_colour("0xff00ff00").value_or(0), 0xFF00FF00u);
    CHECK_EQ(parse_colour("4290787627").value_or(0), 0xFFC0392Bu);
    CHECK_EQ(parse_colour("camgöbeği").value_or(0), 0xFF00FFFFu);
    CHECK_EQ(parse_colour("CAMGOBEGI").value_or(0), 0xFF00FFFFu);
    CHECK_FALSE(parse_colour("112233").has_value()); // hex that lost its '#'
    CHECK_FALSE(parse_colour("C0392B").has_value());
    CHECK_FALSE(parse_colour("#C0392").has_value());
    CHECK_FALSE(parse_colour("#GG0000").has_value());
    CHECK_FALSE(parse_colour("").has_value());
    CHECK_FALSE(parse_colour("katman").has_value()); // the command's word, not a colour

    CHECK_EQ(colour_hex(0xFFC0392Bu), std::string("#C0392B"));
    CHECK_EQ(colour_hex(0x80C0392Bu), std::string("#80C0392B"));
    CHECK_EQ(colour_word(0xFF0000FFu), std::string_view("mavi"));
    CHECK(colour_word(0xFFC0392Bu).empty());

    // EVERY NAME READS BACK AS ITSELF, and none is spelled two ways: the chips
    // and the command line share this one list.
    for (const NamedColour& n : named_colours()) {
        CHECK_EQ(parse_colour(n.word).value_or(0), n.rgba);
        CHECK_EQ(colour_word(n.rgba), n.word);
        CHECK_EQ(n.rgba >> 24U, 0xFFu);
    }
}

TEST_CASE("RENK: etkin seçimin hepsine uygulanır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    r.run("ÇİZGİ 0,5 10,5");
    r.run("ÇİZGİ 0,9 10,9");
    r.run("SEÇ HEPSİ");
    r.run("RENK renk=#2E86C1");
    for (std::int64_t k = 1; k <= 3; ++k)
        CHECK_EQ(r.look(k).rgba, 0xFF2E86C1u);
    CHECK(r.echoed.find("3 nesnenin rengi değişti") != std::string::npos);
}

TEST_CASE("RENK: dolgu bir dolgu katmanıyla verilir, kaldırılır; çizgi yerinde kalır")
{
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10"); // 1
    r.run("RENK nesneler=1 renk=#000000 dolgu=#803366CC");

    // THE INSIDE IS PAINTED BY A FILL LAYER, under the line: the scene draws a
    // plain line's fill colour nowhere, so a colour alone would change nothing
    // anybody could see.
    const core::Symbol& filled = r.symbol(1);
    REQUIRE_EQ(filled.layers.size(), 2u);
    CHECK(filled.layers[0].type == core::SymbolLayerType::SimpleFill);
    CHECK_EQ(filled.layers[0].look.fill_rgba, 0x803366CCu);
    CHECK(filled.layers[1].type == core::SymbolLayerType::SimpleLine);
    CHECK_EQ(filled.layers[1].look.rgba, 0xFF000000u);
    // The summary a file format reads says the same.
    CHECK_EQ(r.look(1).fill_rgba, 0x803366CCu);
    CHECK(r.look(1).src_fill == core::Source::Explicit);

    r.run("RENK nesneler=1 dolgu=yok");
    REQUIRE_EQ(r.symbol(1).layers.size(), 1u);
    CHECK(r.symbol(1).layers[0].type == core::SymbolLayerType::SimpleLine);
    CHECK_EQ(r.look(1).fill_rgba, 0u);
    CHECK(r.look(1).src_fill == core::Source::Explicit); // "no fill" is said, not inherited
    CHECK(r.look(1).src_colour == core::Source::Explicit);
    CHECK(r.echoed.find("(dolgu yok)") != std::string::npos);
}

TEST_CASE("RENK: verilen dolgu tuvale ulaşır")
{
    // THE PROOF THAT MATTERS TO A HAND: the scene the canvas draws holds a
    // polygon batch in the colour asked for. The first version of RENK set the
    // colour on the line layer, every check above it passed, and the parcel on
    // the screen stayed white.
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    r.run("RENK nesneler=1 dolgu=yeşil");

    render::ViewTransform view;
    view.set_viewport(800, 600);
    view.fit(r.doc.extent());
    render::DrawList draw;
    render::build_scene(r.doc, view, {}, draw);

    bool painted = false;
    for (const render::PolygonBatch& batch : draw.polygons)
        painted = painted || (batch.rgba == 0xFF00FF00u && !batch.runs.empty());
    CHECK(painted);
}

TEST_CASE("RENK: yalnız dolgusu olan bir sembole çizgi rengi verilince çizgi eklenir")
{
    Rig r;
    r.run("KATMAN ad=LEKE");
    r.run("STİL katman=LEKE tip=dolgu dolgu=805568546");
    r.run("ALAN 0,0 10,0 10,10 0,10"); // 1, a lekesi with no boundary line

    r.run("RENK nesneler=1 renk=kırmızı");
    const core::Symbol& sym = r.symbol(1);
    REQUIRE_EQ(sym.layers.size(), 2u);
    CHECK(core::draws_fill(sym.layers[0].type)); // the fill is still there…
    CHECK(sym.layers[1].type == core::SymbolLayerType::SimpleLine);
    CHECK_EQ(sym.layers[1].look.rgba, 0xFFFF0000u); // …with the red line on top
}

TEST_CASE("RENK: katman yazılınca renk katmana döner ve nesne katmanını izler")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1
    r.run("RENK nesneler=1 renk=#C0392B dolgu=#00FF00");
    REQUIRE(r.doc.entities().style[r.slot(1)] != core::kByLayerStyle);

    r.run("RENK nesneler=1 renk=katman");
    CHECK(r.look(1).src_colour == core::Source::ByLayer);
    CHECK(r.look(1).src_fill == core::Source::Explicit);

    // Both handed back: the object is the object it was before RENK, down to
    // the style id — it follows its layer again, whatever the layer becomes.
    r.run("RENK nesneler=1 dolgu=katman");
    CHECK_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);

    // Already its layer's: nothing to do, said rather than refused.
    r.echoed.clear();
    r.run("RENK nesneler=1 renk=katman");
    CHECK(r.echoed.find("0 nesnenin rengi değişti") != std::string::npos);
    CHECK(r.echoed.find("1 nesne zaten böyleydi") != std::string::npos);
}

TEST_CASE("RENK: çok katmanlı bir sembolde yalnız ilgili katmanlar boyanır")
{
    Rig r;
    r.run("KATMAN ad=ORMAN");
    r.run("STİL katman=ORMAN tip=dolgu dolgu=805568546");
    r.run("STİL katman=ORMAN ekle=evet tip=cizgi renk=4280645666");
    r.run("ALAN 0,0 10,0 10,10 0,10"); // 1, on ORMAN

    // The object inherits: what it is drawn with is its LAYER's stack.
    REQUIRE_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);
    const core::Layer* orman = r.doc.layer_table().at(r.doc.entities().layer[r.slot(1)]);
    REQUIRE(orman != nullptr);
    const core::Symbol before = r.doc.styles().symbol_at(orman->style);
    REQUIRE(before.layers.size() == 2);

    // THE FILL GOES TO WHAT FILLS. The boundary stroke keeps its colour and the
    // stack keeps both layers — recolouring must not flatten a gösterim.
    r.run("RENK nesneler=1 dolgu=#00FF00");
    const core::Symbol& filled = r.symbol(1);
    REQUIRE(filled.layers.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        const core::SymbolLayer& l = filled.layers[i];
        if (core::draws_fill(l.type)) CHECK_EQ(l.look.fill_rgba, 0xFF00FF00u);
        if (core::draws_stroke(l.type)) CHECK_EQ(l.look.rgba, before.layers[i].look.rgba);
    }

    r.run("RENK nesneler=1 renk=#FF0000");
    const core::Symbol& stroked = r.symbol(1);
    REQUIRE(stroked.layers.size() == 2);
    for (const core::SymbolLayer& l : stroked.layers) {
        if (core::draws_stroke(l.type)) CHECK_EQ(l.look.rgba, 0xFFFF0000u);
        if (core::draws_fill(l.type)) CHECK_EQ(l.look.fill_rgba, 0xFF00FF00u);
    }
}

TEST_CASE("RENK: seçim yokken nesne, renk verilmediyse renk sorar")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1
    auto started = r.bus.begin_interactive("RENK", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("nesneler"));
    REQUIRE(s.supply(Value::ids({1})).ok());
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("renk"));
    CHECK(s.prompt().kind == ParamKind::Text);
    REQUIRE(s.supply(Value::text("#1ABC9C")).ok());
    auto done = r.bus.finish(s);
    if (!done.ok()) FAIL_WITH("RENK", done.error().message);
    CHECK_EQ(r.look(1).rgba, 0xFF1ABC9Cu);

    // THE JOURNAL HOLDS WHAT WAS DONE, in the canonical spelling: a replay
    // does not depend on how the colour was typed.
    const JournalEntry& last = r.journal.entries().back();
    CHECK_EQ(last.command_id, std::string("core.colour"));
    CHECK_EQ(last.args.get("renk").as_text(), std::string("#1ABC9C"));
}

TEST_CASE("RENK: tanınmayan renk, kilitli katman ve renksiz betik reddedilir")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0"); // 1

    auto bad = r.bus.execute_line("RENK nesneler=1 renk=turuncu", Origin::Test);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().message.find("Tanınmayan renk: 'turuncu'") != std::string::npos);
    // The refusal lists the names that work, so the next try can be right.
    CHECK(bad.error().message.find("Renk adları: siyah, kırmızı") != std::string::npos);

    auto bad_fill = r.bus.execute_line("RENK nesneler=1 dolgu=#12345", Origin::Test);
    REQUIRE_FALSE(bad_fill.ok());
    CHECK(bad_fill.error().message.find("Tanınmayan dolgu") != std::string::npos);

    // A SCRIPT CANNOT ANSWER, so it is told what it could have written.
    auto silent = r.bus.execute_line("RENK nesneler=1", Origin::Test);
    REQUIRE_FALSE(silent.ok());
    CHECK(silent.error().message.find("renk=#RRGGBB") != std::string::npos);

    r.run("KATMAN ad=TAPU");
    r.run("ÇİZGİ 0,5 10,5"); // 2, on TAPU
    r.run("KATMAN ad=TAPU kilitli=evet");
    auto locked = r.bus.execute_line("RENK nesneler=2 renk=#FF0000", Origin::Test);
    REQUIRE_FALSE(locked.ok());
    CHECK_EQ(r.doc.entities().style[r.slot(2)], core::kByLayerStyle);

    // NOTHING WAS HALF-APPLIED: a refusal on the second object leaves the first
    // one as it was (Article 1.6).
    auto both = r.bus.execute_line("RENK nesneler=1 nesneler=2 renk=#FF0000", Origin::Test);
    REQUIRE_FALSE(both.ok());
    CHECK_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);
}

TEST_CASE("RENK: tek geri alma adımı, geri alınınca eski renk")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    r.run("ÇİZGİ 0,5 10,5");
    const std::size_t depth = r.undo.undo_depth();
    r.run("RENK nesneler=1 nesneler=2 renk=#FF0000");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    CHECK_EQ(r.look(2).rgba, 0xFFFF0000u);
    r.run("GERİAL");
    CHECK_EQ(r.doc.entities().style[r.slot(1)], core::kByLayerStyle);
    CHECK_EQ(r.doc.entities().style[r.slot(2)], core::kByLayerStyle);
}
