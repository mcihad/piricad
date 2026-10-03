// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE HAND ON THE DRAWING (TODOS U-03, U-04): what a pointer can reach and what a selection writes.
//
//   * THE ONE-SHOT OBJECT SNAP: `orta` typed where a point is asked looks for midpoints alone, for
//     the next point only — the words (the modes' own ids and labels, folded the Turkish way, the
//     English three-letter names), what a prompt does with the override (aids_for), that it moves
//     an AIMED point and no other, that the point that answers is spent and the next question
//     starts without it, and that the journal records the point it put and never the word.
//   * WHAT IS UNDER THE POINTER: a hundred objects on one spot are each reachable by the line a
//     script sends; a locked layer is selected and snapped to unless the person said otherwise, a
//     hidden one never; the aperture is the same number of PIXELS at every zoom.
//   * ONE WRITE FOR SEVERAL OBJECTS: `ÖZNİTELİK nesneler=` is one transaction, one undo step, all
//   or
//     nothing.
#include "piricad_test.hpp"

#include "piricad/command/aids.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/snap.hpp"

#include <cmath>
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

// =============================================================================
// ONE HUNDRED OBJECTS ON ONE SPOT (TODOS U-03): the wanted one can be taken
// =============================================================================

TEST_CASE("SEÇİM: yüz nesne üst üste iken istenen nesne komutla seçilebilir")
{
    // The acceptance of U-03: with a hundred objects under one click, the wanted one is reachable —
    // and by the line a script and the keyboard send, not by a mouse gesture alone
    // (CLAUDE.md 5.15).
    Rig r;
    r.run("KATMAN ad=YIGIN");
    for (int i = 0; i < 100; ++i)
        r.run("ÇİZGİ 0,0 10,0");
    REQUIRE(r.doc.live_entity_count() == 100);

    // The list a click sees: every one of them, once, in the order `sira` counts (nearest, then the
    // lower slot — all are equally near, so the order of drawing).
    std::vector<core::EntityId> under;
    core::pick_all(r.doc, core::Point2{5'000, 0}, 100, under);
    REQUIRE(under.size() == 100);
    for (std::size_t i = 1; i < under.size(); ++i)
        CHECK(under[i - 1] < under[i]);

    // Every row of the list is reachable by `sira`, and selects exactly that object.
    for (int row = 1; row <= 100; ++row) {
        r.run("SEÇ TEMİZLE");
        r.run("SEÇ mod=NOKTA noktalar=5,0 tolerans=0.1 sira=" + std::to_string(row));
        const auto& keys = r.bus.selection().keys();
        REQUIRE_MESSAGE(keys.size() == 1, "sıra " << row);
        const core::EntityId wanted = under[static_cast<std::size_t>(row - 1)];
        CHECK_MESSAGE(r.doc.slot_of(keys.front()) == wanted, "sıra " << row);
    }

    // And one past the end is refused with the count, not wrapped round.
    r.run("SEÇ TEMİZLE");
    auto past =
        r.bus.execute_line("SEÇ mod=NOKTA noktalar=5,0 tolerans=0.1 sira=101", Origin::Test);
    CHECK_FALSE(past.ok());
}

// =============================================================================
// LOCKED AND HIDDEN LAYERS under the hand (TODOS U-03)
// =============================================================================

TEST_CASE("SEÇİM: kilitli katman varsayılanda seçilir ve yakalanır; ayar kapatınca atlanır")
{
    Rig r;
    r.bus.aids().set_view_scale(100.0); ///< 100 mm a pixel: the aperture is 1.6 m
    r.run("KATMAN ad=SINIR");
    r.run("ÇİZGİ 0,0 10,0");
    r.run("KATMAN ad=SINIR kilitli=evet");

    const auto selected_by_point = [&r] {
        r.run("SEÇ TEMİZLE");
        r.run("SEÇ mod=NOKTA noktalar=5,0 tolerans=0.1");
        return r.bus.selection().keys().size();
    };
    const auto snapped_to = [&r](core::Point2 aim) {
        core::SnapQuery q;
        q.aim              = aim;
        q.radius           = 1'600;
        q.modes            = core::SnapEndpoint;
        q.on_locked_layers = r.bus.aid_settings().snap_locked;
        return core::snap(r.doc, q);
    };

    // THE DEFAULTS ARE WHAT THE PROGRAM DID: a locked boundary is the one thing a point is taken
    // FROM, and it can be selected to be read and measured though it cannot be edited.
    CHECK(r.bus.aid_settings().snap_locked);
    CHECK(r.bus.aid_settings().select_locked);
    CHECK(selected_by_point() == 1);
    CHECK(snapped_to(core::Point2{10'400, 300}).mode == core::SnapEndpoint);

    // Said no, they leave the hand alone — selection and snap each by its own setting.
    r.run("TERCİH core.secim.kilitli_katman hayır");
    CHECK(selected_by_point() == 0);
    CHECK(snapped_to(core::Point2{10'400, 300}).mode == core::SnapEndpoint); ///< snap is its own
    r.run("TERCİH core.yakalama.kilitli_katman hayır");
    CHECK_FALSE(r.bus.aid_settings().snap_locked);
    CHECK(snapped_to(core::Point2{10'400, 300}).mode == core::SnapNone);

    // The layer mode is a gesture too; an object named by id is not, and a script that names it
    // gets it.
    r.run("SEÇ TEMİZLE");
    r.run("SEÇ mod=KATMAN katman=SINIR");
    CHECK(r.bus.selection().keys().empty());
    r.run("SEÇ TEMİZLE");
    r.run("SEÇ mod=NESNE nesneler=1");
    CHECK(r.bus.selection().keys().size() == 1);

    // Unlocked, it is back whatever the settings say.
    r.run("KATMAN ad=SINIR kilitli=hayır");
    CHECK(selected_by_point() == 1);
    CHECK(snapped_to(core::Point2{10'400, 300}).mode == core::SnapEndpoint);
}

TEST_CASE("SEÇİM: gizli katman ne seçilir ne yakalanır, ayardan bağımsız")
{
    Rig r;
    r.bus.aids().set_view_scale(100.0);
    r.run("KATMAN ad=GIZLI");
    r.run("ÇİZGİ 0,0 10,0");
    r.run("KATMAN ad=GIZLI gorunur=hayır");

    r.run("SEÇ TEMİZLE");
    r.run("SEÇ mod=NOKTA noktalar=5,0 tolerans=0.1");
    CHECK(r.bus.selection().keys().empty());

    core::SnapQuery q;
    q.aim    = core::Point2{10'400, 300};
    q.radius = 1'600;
    q.modes  = core::SnapEndpoint;
    CHECK(core::snap(r.doc, q).mode == core::SnapNone);
}

// =============================================================================
// THE APERTURE IS PIXELS (TODOS U-03): the same reach at every zoom
// =============================================================================

TEST_CASE("YAKALAMA: tolerans ekran pikselidir, yakınlaştırma ne olursa olsun aynı hissi verir")
{
    // The acceptance says the pixel tolerance feels constant when the zoom changes. It does when
    // the aperture is pixels (`core.yakalama.tolerans`, 16 by default) turned into ground at THIS
    // zoom: a point 14 pixels from a corner is taken at 1 mm a pixel and at 2 km a pixel, a point
    // 20 pixels away is taken at neither, and the corner it lands on is the same corner.
    const double scales[] = {0.25, 1.0, 7.0, 50.0, 333.0, 2'000.0};
    for (const double mm_per_pixel : scales) {
        INFO("mm/piksel " << mm_per_pixel);
        Rig r;
        r.bus.aids().set_view_scale(mm_per_pixel);
        r.run("MOD ad=yakalama_modları deger=" + std::to_string(core::SnapEndpoint));
        r.run("ÇİZGİ 0,0 10000,0"); ///< the far end is (10 km, 0): corners at both ends
        const core::Point2 corner{10'000'000, 0};

        const auto answered = [&](core::Point2 aim) {
            auto started = r.bus.begin_interactive("ÇİZGİ");
            REQUIRE(started.ok());
            Session& s = *started.value();
            REQUIRE(s.supply(Value::aimed_point(aim)).ok());
            const core::Point2 landed = s.prompt().rubber_origin;
            s.cancel();
            (void)r.bus.finish(s);
            return landed;
        };
        const auto px = [mm_per_pixel](double pixels) {
            return static_cast<core::Mm>(std::llround(pixels * mm_per_pixel));
        };

        // 14 pixels out, along and across: taken.
        CHECK(answered(core::Point2{corner.x + px(14), 0}) == corner);
        CHECK(answered(core::Point2{corner.x, px(10)}) == corner);
        // 20 pixels out: left where it was aimed.
        const core::Point2 far{corner.x + px(20), 0};
        CHECK(answered(far) == far);
    }
}

// =============================================================================
// SEVERAL OBJECTS, ONE ATTRIBUTE WRITE (TODOS U-04)
// =============================================================================

TEST_CASE("ÖZNİTELİK: nesneler= birçok nesneye tek işlemde yazar, tek geri alma adımı")
{
    Rig r;
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    for (int i = 0; i < 4; ++i)
        r.run("ALAN " + std::to_string(i * 20) + ",0 " + std::to_string(i * 20 + 10) + ",0 " +
              std::to_string(i * 20 + 10) + ",10 " + std::to_string(i * 20) + ",10");

    const auto value_of = [&r](std::int64_t key) {
        const auto slot = r.doc.slot_of(static_cast<core::EntityKey>(key));
        return r.doc.attribute(r.doc.attributes().find("ada"), slot);
    };
    const std::size_t depth = r.undo.undo_depth();

    // Three of the four in one write.
    r.run("ÖZNİTELİK ad=ada nesneler=1 nesneler=2 nesneler=4 deger=77");
    for (const std::int64_t key : {1, 2, 4}) {
        REQUIRE(value_of(key).ok());
        CHECK(value_of(key).value().present);
        CHECK(value_of(key).value().number == 77);
    }
    REQUIRE(value_of(3).ok());
    CHECK_FALSE(value_of(3).value().present); ///< the one not named is untouched
    CHECK(r.undo.undo_depth() - depth == 1);  ///< one step for the three

    // ONE UNDO takes all three back.
    r.run("GERİAL");
    for (const std::int64_t key : {1, 2, 4})
        CHECK_FALSE(value_of(key).value().present);

    // The journal holds the line a script would write: the same ids, the same value.
    const auto& last = r.journal.entries().back();
    CHECK(last.command_id == "core.attribute");
    CHECK(last.args.find("nesneler") != nullptr);
    CHECK(last.args.find("nesneler")->as_ids() == std::vector<std::int64_t>{1, 2, 4});

    // ATOMIC: one bad id and none is written.
    auto bad = r.bus.execute_line("ÖZNİTELİK ad=ada nesneler=1 nesneler=2 nesneler=999 deger=5",
                                  Origin::Test);
    CHECK_FALSE(bad.ok());
    CHECK_FALSE(value_of(1).value().present);
    CHECK_FALSE(value_of(2).value().present);

    // A value that is no value for the column is refused before anything is written.
    auto wrong =
        r.bus.execute_line("ÖZNİTELİK ad=ada nesneler=1 nesneler=2 deger=abc", Origin::Test);
    CHECK_FALSE(wrong.ok());
    CHECK_FALSE(value_of(1).value().present);

    // And the single-object form still works as it always did.
    r.run("ÖZNİTELİK ad=ada nesne=3 deger=9");
    CHECK(value_of(3).value().number == 9);
}
