// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the ribbon's tool drive (`PIRICAD_TOOL_DRIVE`, main_window_drive.cpp)
// found, each held here so it stays found: a tool pressed and answered the
// way a hand would must do its work, or say in words the user can act on why
// it cannot — never a sentence about a parameter the user never saw.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/identity.hpp"
#include "piricad/processing/registry.hpp"

#include <string>

using namespace piricad;
using namespace piricad::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

} // namespace

TEST_CASE("SÜRÜŞ: döndürülmüş dikdörtgenin reddi kullanıcının sözüyle, parametre adıyla değil")
{
    Rig r;
    auto started = r.bus.begin_interactive("DİKDÖRTGEN yontem=3n", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::point({10'000, 0})).ok());
    REQUIRE(s.waiting());
    (void)s.supply(Value::point({10'000, 0}));
    auto done = r.bus.finish(s);
    REQUIRE_FALSE(done.ok());
    MESSAGE(done.error().message);
    CHECK(done.error().message.find("parametresi eksik") == std::string::npos);
    CHECK(done.error().message.find("aynı nokta") != std::string::npos);
}

TEST_CASE("SÜRÜŞ: KÖŞESİL dört köşeli alanın bir köşesini siler")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,20 0,20");
    auto gone = r.bus.execute_line("KÖŞESİL nesne=1 kose=2", Origin::Test);
    if (!gone) MESSAGE(gone.error().message);
    CHECK(gone.ok());
}

TEST_CASE("SÜRÜŞ: nokta dizisinin ilk sorusunda boş cevap bir şey çizmiyor, hata demiyor")
{
    // An empty answer at a run's first question writes nothing and is not a
    // failure. (Enter there in the shell cancels outright — `Controller::
    // finishInteractive` — because a method given with the button, the rotated
    // rectangle's `yontem=3n`, is an argument the bus then holds the run to;
    // that road is the
    // shell's and is proved by the real-window probe.)
    for (const char* tool : {"DİKDÖRTGEN", "ÇİZGİ", "ÇOKLUÇİZGİ", "ALAN", "SPLINE", "ÖLÇ"}) {
        Rig r;
        auto started = r.bus.begin_interactive(tool, Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.waiting());
        (void)s.supply(Value{});
        auto done              = r.bus.finish(s);
        const std::string said = done.ok() ? std::string() : done.error().message;
        CHECK_MESSAGE(said.find("parametresi eksik") == std::string::npos, tool, ": ", said);
        CHECK_EQ(r.doc.live_entity_count(), 0u);
    }
}

TEST_CASE("SÜRÜŞ: STİLKOPYALA önce kaynağı (tek tıkla), sonra hedefleri soruyor")
{
    const auto drawing = [](Rig& r) {
        r.run("ALAN 0,0 10,0 10,10 0,10");
        r.run("RENK nesneler=1 renk=kırmızı");
        r.run("ALAN 20,0 30,0 30,10 20,10");
        r.run("ALAN 40,0 50,0 50,10 40,10");
    };
    // NOTHING SELECTED: the source is asked for, ONE object — a click answers
    // it (`Prompt::pick_most`) — and then the targets.
    Rig clicked;
    drawing(clicked);
    auto started = clicked.bus.begin_interactive("STİLKOPYALA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Selection);
    CHECK_EQ(s.prompt().param, std::string("kaynak"));
    CHECK_EQ(s.prompt().pick_most, 1u);
    REQUIRE(s.supply(Value::ids({1})).ok());
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("nesneler"));
    CHECK_EQ(s.prompt().pick_most, 0u);
    REQUIRE(s.supply(Value::ids({2, 3})).ok());
    REQUIRE(clicked.bus.finish(s).ok());
    const auto style_of = [](const Rig& r, std::uint64_t key) {
        return r.doc.entities().style[r.doc.slot_of(static_cast<core::EntityKey>(key))];
    };
    CHECK(style_of(clicked, 2) == style_of(clicked, 1));
    CHECK(style_of(clicked, 3) == style_of(clicked, 1));

    // ONE HIGHLIGHTED: that is the source, and only the targets are asked for.
    Rig picked;
    drawing(picked);
    picked.run("SEÇ mod=NESNE nesneler=1");
    auto pressed = picked.bus.begin_interactive("STİLKOPYALA", Origin::Gui);
    REQUIRE(pressed.ok());
    Session& p = *pressed.value();
    REQUIRE(p.waiting());
    CHECK_EQ(p.prompt().param, std::string("nesneler"));
    REQUIRE(p.supply(Value::ids({2, 3})).ok());
    REQUIRE(picked.bus.finish(p).ok());

    // AND THE LINE A SCRIPT SENDS makes the same drawing and the same record.
    Rig typed;
    drawing(typed);
    typed.run("STİLKOPYALA kaynak=1 nesneler=2 nesneler=3");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    CHECK_EQ(picked.doc.content_hash(), typed.doc.content_hash());
    CHECK(clicked.journal.entries().back().args.to_json().dump() ==
          typed.journal.entries().back().args.to_json().dump());
}

TEST_CASE("SÜRÜŞ: tek nesne soran sorular bunu söylüyor (KIR, UZUNLUK, BÖLÜMLE)")
{
    for (const char* tool : {"KIR", "UZUNLUK", "BÖLÜMLE"}) {
        Rig r;
        auto started = r.bus.begin_interactive(tool, Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.waiting());
        CHECK(s.prompt().kind == ParamKind::Selection);
        CHECK_MESSAGE(s.prompt().pick_most == 1u, tool);
    }
}

TEST_CASE(
    "SÜRÜŞ: KILAVUZ yon=yatay koordinat verilmezse noktayı soruyor; betik yine örneği duyuyor")
{
    Rig clicked;
    auto started = clicked.bus.begin_interactive("KILAVUZ yon=yatay", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Point);
    REQUIRE(s.supply(Value::point({10'000, 25'000})).ok());
    REQUIRE(clicked.bus.finish(s).ok());
    REQUIRE_EQ(clicked.doc.guides().size(), 1u);
    CHECK_EQ(clicked.doc.guides().coordinate(0), core::Mm{25'000});

    // The record is the coordinate, so a replay needs no click.
    Rig typed;
    typed.run("KILAVUZ yon=yatay deger=25000");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());

    Rig down;
    auto vertical = down.bus.begin_interactive("KILAVUZ yon=düşey", Origin::Gui);
    REQUIRE(vertical.ok());
    REQUIRE(vertical.value()->supply(Value::point({12'000, 3'000})).ok());
    REQUIRE(down.bus.finish(*vertical.value()).ok());
    CHECK_EQ(down.doc.guides().coordinate(0), core::Mm{12'000});

    // A call that gives neither is told the form, as before.
    Rig script;
    auto refused = script.bus.execute_line("KILAVUZ yon=yatay", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("Örnek: KILAVUZ yon=yatay deger=") != std::string::npos);
}

TEST_CASE("SÜRÜŞ: şeritten BAĞLA yazıların bağlanacağı nesneyi tek tıkla sorar")
{
    // Pressed on the ribbon, BAĞLA is the bare command: the captions are the
    // selection, and the object they hang off is asked for — one click, no
    // Enter — where it used to refuse "kaynak verilmedi". The answer is an
    // argument like any other, so the journal replays without a hand.
    Rig clicked;
    clicked.run("ÇİZGİ 0,0 10,0");      // 1
    clicked.run("METİN 5,2 \"kenar\""); // 2
    clicked.run("SEÇ mod=NESNE nesneler=2");
    auto started = clicked.bus.begin_interactive("BAĞLA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Selection);
    CHECK_EQ(s.prompt().param, std::string("kaynak"));
    CHECK_EQ(s.prompt().pick_most, std::size_t{1});
    REQUIRE(s.supply(Value::ids({1})).ok());
    auto done = clicked.bus.finish(s);
    if (!done) MESSAGE(done.error().message);
    REQUIRE(done.ok());
    const core::EntityId caption = clicked.doc.slot_of(core::EntityKey{2});
    REQUIRE(clicked.doc.attachments().has(caption));
    CHECK_EQ(core::raw(clicked.doc.attachments().get(caption)->source), std::uint64_t{1});

    // The command line naming both makes the same document and the same record.
    Rig typed;
    typed.run("ÇİZGİ 0,0 10,0");
    typed.run("METİN 5,2 \"kenar\"");
    typed.run("BAĞLA nesneler=2 kaynak=1");
    CHECK_EQ(clicked.doc.content_hash(), typed.doc.content_hash());
    REQUIRE_FALSE(clicked.journal.entries().empty());
    const JournalEntry& asked = clicked.journal.entries().back();
    CHECK_EQ(asked.command_id, std::string("islem.bagla"));
    CHECK_EQ(asked.args.get("kaynak").as_ids(), std::vector<std::int64_t>{1});
    CHECK(asked.args == typed.journal.entries().back().args);

    // A client that cannot point is told the form.
    Rig script;
    script.run("ÇİZGİ 0,0 10,0");
    script.run("METİN 5,2 \"kenar\"");
    auto refused = script.bus.execute_line("BAĞLA nesneler=2", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("BAĞLA kaynak=<kimlik>") != std::string::npos);
}
