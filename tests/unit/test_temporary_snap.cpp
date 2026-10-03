// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE ONE-SHOT OBJECT SNAP (TODOS U-03): `orta` typed where a point is asked looks for midpoints
// alone, for the next point only.
//
// What is pinned: the words (the modes' own ids and labels, folded the Turkish way, the English
// three-letter names), what a prompt does with the override (aids_for), that it moves an AIMED
// point and no other, that the point that answers is spent and the next question starts without
// it, and that the journal records the point it put and never the word.
#include "piricad_test.hpp"

#include "piricad/command/aids.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/core/snap.hpp"

#include <string>
#include <vector>

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
        bus.on_echo = [](std::string_view) {};
    }

    void run(const std::string& line)
    {
        auto done = bus.execute_line(line, Origin::Test);
        INFO(line << " -> " << (done.ok() ? "ok" : done.error().message));
        REQUIRE(done.ok());
    }
};

} // namespace

TEST_CASE("GEÇİCİ YAKALAMA: sözcük modun kendi adıdır, Türkçe katlanarak")
{
    const struct
    {
        const char* word;
        std::uint32_t bit;
    } cases[] = {
        {"orta", core::SnapMidpoint},
        {"ORTA", core::SnapMidpoint},
        {"Orta nokta", core::SnapMidpoint},
        {"ortanokta", core::SnapMidpoint},
        {"uç", core::SnapEndpoint},
        {"UÇ", core::SnapEndpoint},
        {"uc", core::SnapEndpoint},
        {"uç nokta", core::SnapEndpoint},
        {"merkez", core::SnapCenter},
        {"ağırlık merkezi", core::SnapCentroid},
        {"agirlik_merkezi", core::SnapCentroid},
        {"ağırlık", core::SnapCentroid},
        {"kesişim", core::SnapIntersection},
        {"KESİSİM", core::SnapIntersection},
        {"dik", core::SnapPerpendicular},
        {"dik ayak", core::SnapPerpendicular},
        {"yakın", core::SnapNearest},
        {"en yakın", core::SnapNearest},
        {"düğüm", core::SnapNode},
        {"çeyrek", core::SnapQuadrant},
        {"çeyrek nokta", core::SnapQuadrant},
        {"teğet", core::SnapTangent},
        {"uzantı", core::SnapExtension},
        {"paralel", core::SnapParallel},
        {"uzatılmış kesişim", core::SnapApparent},
        {"ekleme", core::SnapInsertion},
        {"END", core::SnapEndpoint},
        {"mid", core::SnapMidpoint},
        {"CEN", core::SnapCenter},
        {"int", core::SnapIntersection},
        {"PER", core::SnapPerpendicular},
        {"nea", core::SnapNearest},
        {"QUA", core::SnapQuadrant},
        {"tan", core::SnapTangent},
    };

    for (const auto& c : cases)
        CHECK_MESSAGE(snap_mode_from_word(c.word) == c.bit, c.word);

    // Not an object snap: the grid, polar and tracking need no geometry, and anything else is not a
    // snap at all.
    for (const char* word : {"", "  ", "ızgara", "kutupsal", "izleme", "xyz", "ORTAX", "5", "12.5"})
        CHECK_MESSAGE(snap_mode_from_word(word) == 0, word);
}

TEST_CASE("GEÇİCİ YAKALAMA: istem yalnız o modu arar, yönlü kilitler ve ızgara kalır")
{
    AidSettings set;
    set.modes       = core::SnapEndpoint | core::SnapGrid | core::SnapPolar;
    set.snap_radius = 100;
    set.ortho       = true;

    Prompt line;
    line.kind            = ParamKind::Point;
    line.has_rubber_band = true;
    line.rubber_shape    = RubberShape::Line;

    const AidSettings got = aids_for(set, line, core::SnapMidpoint);
    CHECK((got.modes & core::SnapObjectMask) == core::SnapMidpoint);
    CHECK((got.modes & core::SnapGrid) != 0);
    CHECK((got.modes & core::SnapPolar) != 0);
    CHECK(got.ortho);

    // Without the override the settings are untouched.
    CHECK(aids_for(set, line, 0).modes == set.modes);

    // A constructed mode asked for by name is found even where the settings gave it no reach.
    const AidSettings far = aids_for(set, line, core::SnapExtension);
    CHECK((far.modes & core::SnapObjectMask) == core::SnapExtension);
    CHECK(far.reach == 100 * 40);

    // A prompt that takes no aid at all has nothing for the word to act on.
    Prompt seed = line;
    seed.aids   = false;
    CHECK((aids_for(set, seed, core::SnapMidpoint).modes & core::SnapObjectMask) == 0);
    Prompt trim       = line;
    trim.rubber_shape = RubberShape::Trim;
    CHECK((aids_for(set, trim, core::SnapMidpoint).modes & core::SnapObjectMask) == 0);
}

TEST_CASE("GEÇİCİ YAKALAMA: nişanlanan nokta orta noktaya oturur, bir sonraki oturmaz")
{
    Rig r;
    r.bus.aids().set_view_scale(100.0); ///< 100 mm a pixel: the aperture is 1.6 m
    // The running snaps are the end points alone, so a click near the middle finds nothing.
    r.run("MOD ad=yakalama_modları deger=" + std::to_string(core::SnapEndpoint));
    r.run("ÇİZGİ 0,0 10,0");
    r.run("ÇİZGİ 20,0 30,0");

    auto started = r.bus.begin_interactive("ÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::aimed_point(core::Point2{0, 5'000})).ok());

    // 40 cm off the midpoint of the 10 m line, 5 m from the base: UÇ finds nothing in 1.6 m.
    const core::Point2 near_middle{5'400, 300};
    s.set_temporary_snap(core::SnapMidpoint);
    REQUIRE(s.supply(Value::aimed_point(near_middle)).ok());
    CHECK(s.prompt().rubber_origin == (core::Point2{5'000, 0}));
    CHECK(s.temporary_snap() == 0); ///< spent by the point it was for

    // The next aimed point, near the middle of the OTHER line, is no longer helped.
    const core::Point2 near_other{25'400, 300};
    REQUIRE(s.supply(Value::aimed_point(near_other)).ok());
    CHECK(s.prompt().rubber_origin == near_other);

    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    // The journal holds the points, never the word.
    CHECK(r.journal.entries().back().args.find("noktalar") != nullptr);
    CHECK(r.journal.entries().back().args.to_json().dump().find("orta") == std::string::npos);
}

TEST_CASE("GEÇİCİ YAKALAMA: yazılan koordinat dokunulmaz, sonraki soru sözcüksüz başlar")
{
    Rig r;
    r.bus.aids().set_view_scale(100.0);
    r.run("MOD ad=yakalama_modları deger=" + std::to_string(core::SnapEndpoint));
    r.run("ÇİZGİ 0,0 10,0");

    auto started = r.bus.begin_interactive("ÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();

    // A STATED coordinate is exact, the override or not (F-03).
    s.set_temporary_snap(core::SnapMidpoint);
    REQUIRE(s.supply(Value::point(core::Point2{5'400, 300})).ok());
    CHECK(s.prompt().rubber_origin == (core::Point2{5'400, 300}));

    // And a word set and never used is dropped by the next question rather than carried on.
    s.set_temporary_snap(core::SnapMidpoint);
    REQUIRE(s.supply(Value::point(core::Point2{9'000, 9'000})).ok());
    CHECK(s.temporary_snap() == 0);
    s.cancel();
    (void)r.bus.finish(s);
}
