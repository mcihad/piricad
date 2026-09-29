// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE LAYER A DRAWING GOES ON (plan open question 19).
//
// `katman=` on a drawing command puts what it draws on that layer and leaves
// the active one where it was, and it may be given while the command waits —
// typed, or by the `Nokta Girişi` tab's `Katmanı nesneden al` — with the same
// journal line as if the first line had carried it.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/validation.hpp"
#include "kentos_cad/core/document.hpp"

#include <string>

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

    core::LayerId layer_of(std::int64_t key) const
    {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
        return doc.entities().layer[e];
    }
};

} // namespace

TEST_CASE("katman=: çizim adı verilen katmana gider, etkin katman değişmez")
{
    Rig r;
    r.run("KATMAN ad=YOL");
    r.run("KATMAN ad=PARSEL"); ///< the active one from here on
    const core::LayerId road   = r.doc.find_layer("YOL");
    const core::LayerId parcel = r.doc.find_layer("PARSEL");

    r.run("ÇİZGİ katman=YOL 0,0 10,0");             ///< 1
    r.run("DAİRE merkez=5,5 cevre=6,5 katman=yol"); ///< 2, the name folded
    r.run("METİN 5,5 \"101\" katman=YOL");          ///< 3
    r.run("ALAN 0,0 4,0 4,4 0,4");                  ///< 4, no layer named
    CHECK(r.layer_of(1) == road);
    CHECK(r.layer_of(2) == road);
    CHECK(r.layer_of(3) == road);
    CHECK(r.layer_of(4) == parcel);
    CHECK(r.bus.active_layer() == parcel);

    // THE JOURNAL KEEPS THE NAME AS GIVEN, so a replay draws on it again.
    bool found = false;
    for (const auto& e : r.journal.entries())
        if (e.command_id == "core.line") {
            CHECK(e.args.get("katman").as_text() == "YOL");
            found = true;
        }
    CHECK(found);
}

TEST_CASE("katman=: olmayan katman ilk tıklamadan önce reddedilir; konumsal söz katman olmaz")
{
    Rig r;
    r.run("KATMAN ad=YOL");
    const auto typed = r.bus.execute_line("ÇİZGİ katman=YOLL 0,0 10,0", Origin::Test);
    REQUIRE_FALSE(typed.ok());
    CHECK(typed.error().message.find("Katman bulunamadı: 'YOLL'") != std::string::npos);
    CHECK(typed.error().message.find("Çizimdeki katmanlar: 0, YOL") != std::string::npos);
    CHECK(r.doc.live_entity_count() == 0);

    // INTERACTIVE, refused at the start, not after the corners were clicked.
    const auto started = r.bus.begin_interactive("ÇİZGİ katman=YOLL", Origin::Gui);
    REQUIRE_FALSE(started.ok());
    CHECK(started.error().message.find("Katman bulunamadı") != std::string::npos);

    // BY NAME ONLY: a word left over at the end of a line is not a layer.
    const auto spare = r.bus.execute_line("DAİRE 0,0 1,0 YOL", Origin::Test);
    CHECK_FALSE(spare.ok());
    CHECK(r.doc.live_entity_count() == 0);

    // A COMMAND THAT DRAWS ON NO LAYER OF ITS OWN takes none.
    CHECK(takes_draw_layer(*r.reg.resolve("ÇİZGİ")));
    CHECK_FALSE(takes_draw_layer(*r.reg.resolve("TAŞI")));
}

TEST_CASE("katman= soru sürerken: yazılan satır komutun katmanını kurar, soru sürer")
{
    Rig r;
    r.run("KATMAN ad=YOL");
    r.run("KATMAN ad=PARSEL");
    auto started = r.bus.begin_interactive("ÇİZGİ", Origin::Gui);
    REQUIRE(started.ok());
    auto& session = *started.value();
    REQUIRE(session.supply(Value::point(Point2{0, 0})).ok());

    // THE LINE IS THE COMMAND'S, NOT AN ANSWER: the corner is still asked for.
    const auto amended = amend_from_line(session, "katman=YOL");
    REQUIRE(amended.has_value());
    CHECK(amended->ok());
    CHECK(session.waiting());
    CHECK(r.said.find("Bu komutun çizdikleri 'YOL' katmanına gider; etkin katman değişmez.") !=
          std::string::npos);

    // A LAYER THERE IS NOT is refused and changes nothing; the run goes on.
    const auto wrong = amend_from_line(session, "katman=KANAL");
    REQUIRE(wrong.has_value());
    CHECK_FALSE(wrong->ok());
    CHECK(session.waiting());

    // NOT EVERY `ad=değer` IS AN AMENDMENT: one the command does not take so
    // is the prompt's answer as before.
    CHECK_FALSE(amend_from_line(session, "noktalar=5,5").has_value());
    CHECK_FALSE(amend_from_line(session, "10,0").has_value());

    REQUIRE(session.supply(Value::point(Point2{10'000, 0})).ok());
    session.cancel(); // ESC ends the run with what it has
    REQUIRE(r.bus.finish(session).ok());
    CHECK(r.layer_of(1) == r.doc.find_layer("YOL"));
    CHECK(r.bus.active_layer() == r.doc.find_layer("PARSEL"));
    CHECK(r.journal.entries().back().args.get("katman").as_text() == "YOL");
}

TEST_CASE("katman= soru sürerken: katman almayan komutta satır bir yanıttır")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");
    auto started = r.bus.begin_interactive("TAŞI nesneler=1", Origin::Gui);
    REQUIRE(started.ok());
    auto& session = *started.value();
    REQUIRE(session.waiting());
    CHECK_FALSE(amend_from_line(session, "katman=0").has_value());
    CHECK_FALSE(session.amend("katman", Value::text("0")).ok());
    session.cancel();
    (void)r.bus.finish(session);
}

TEST_CASE("katman=: ÇİFTÇİZGİ ekseni ve katmanı verilmemiş yanı katman='a çizer")
{
    Rig r;
    r.run("KATMAN ad=YOL");
    r.run("KATMAN ad=PARSEL");
    r.run("ÇİFTÇİZGİ noktalar=0,0 10,0 sol=2 sag=3 katman=YOL katman_sag=KALDIRIM");
    CHECK(r.layer_of(1) == r.doc.find_layer("YOL")); ///< the axis
    CHECK(r.layer_of(2) == r.doc.find_layer("YOL")); ///< the left side, no layer of its own
    CHECK(r.layer_of(3) == r.doc.find_layer("KALDIRIM"));
    CHECK(r.bus.active_layer() == r.doc.find_layer("PARSEL"));
}
