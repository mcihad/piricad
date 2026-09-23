// SPDX-License-Identifier: GPL-3.0-or-later
//
// WHAT A HAND SEES AND WHAT A HAND CAN START.
//
// Two complaints, both about the program as it is used rather than as it is
// scripted: tools in the column and the menus that answered a press with a
// refusal ("nesne belirtilmedi", "zorunlu parametre eksik") instead of asking,
// and prompts that asked for a second point with nothing on the canvas to say
// what the click would make. This file pins both, by driving every interactive
// command the way the canvas does and reading the prompts it puts up.
#include "kentos_test.hpp"

#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/break_run.hpp"
#include "kentos_cad/core/corner.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/transform.hpp"
#include "kentos_cad/core/trim_curve.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <algorithm>
#include <map>
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

    Rig()
    {
        register_builtin_commands(reg);
        domain::geodesy::register_geodesy_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::surface::register_surface_commands(reg);
        processing::register_processing_commands(reg);
        ai::register_ai_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    /// A drawing with something of every shape the tools act on.
    void seed()
    {
        REQUIRE(bus.execute_line("ÇOKLUÇİZGİ 0,0 20,0 20,10", Origin::Test).ok());  // 1
        REQUIRE(bus.execute_line("ALAN 30,0 50,0 50,20 30,20", Origin::Test).ok()); // 2
        REQUIRE(bus.execute_line("DAİRE 70,10 75,10", Origin::Test).ok());          // 3
        REQUIRE(bus.execute_line("ÇİZGİ 0,40 40,40", Origin::Test).ok());           // 4
        REQUIRE(bus.execute_line("NOKTA 10,30", Origin::Test).ok());                // 5
    }
};

core::Point2 vertex_of(const core::Document& doc, std::int64_t key, std::size_t at)
{
    const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
    REQUIRE(e != core::kNoEntity);
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
    return core::Point2{doc.geometry().ring_xs(span.first)[at],
                        doc.geometry().ring_ys(span.first)[at]};
}

std::size_t vertex_count(const core::Document& doc, std::int64_t key)
{
    const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
    REQUIRE(e != core::kNoEntity);
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
    return doc.geometry().ring_xs(span.first).size();
}

} // namespace

// =============================================================================
// Every point after the first has a preview
// =============================================================================

TEST_CASE("ÖNİZLEME: bir çalıştırmanın ikinci ve sonraki her nokta isteminde önizleme vardır")
{
    // A POINT ASKED FOR AFTER ANOTHER ONE IS A POINT MEASURED FROM SOMETHING,
    // and the canvas can show what the click will make only if the prompt says
    // what that is. Every interactive command is driven the way the canvas
    // drives it, with generic answers, and every point prompt after the first of
    // its run is read. A prompt with no preview there fails, unless it is one of
    // the few below whose silence is the right answer, each with its reason.
    const std::map<std::string, std::string> silent_by_design{
        // Each NOKTA is its own object; the second point is not measured from
        // the first, and a line between them would promise a segment that is
        // never drawn.
        {"core.point_draw|noktalar", "her nokta ayrı bir nesnedir"},
        // Each pair's LOCAL point starts a new pair: it is not measured from the
        // map point before it. (The map point is, and is previewed from it.)
        {"core.fit|noktalar#yerel", "her çiftin yerel noktası yeni bir ilk noktadır"},
    };

    const std::vector<core::Point2> points{
        {10'000, 0},      {20'000, 5'000}, {14'000, 13'000}, {4'000, 9'000},
        {26'000, 2'000},  {8'000, -6'000}, {33'000, 11'000}, {41'000, 17'000},
        {12'000, 22'000}, {-5'000, 4'000}, {18'000, 30'000}, {2'000, 2'000},
    };

    Rig probe;
    std::size_t asked_after_first = 0;
    for (const CommandSpec& spec : probe.reg.all()) {
        if (!has_flag(spec.flags, Flags::Interactive)) continue;
        if (spec.category == Category::File) continue;

        Rig r;
        r.seed();
        auto started = r.bus.begin_interactive(spec.names.front(), Origin::Gui);
        if (!started) continue;

        Session& s        = *started.value();
        std::size_t given = 0;
        for (std::size_t step = 0; s.waiting() && step < 14; ++step) {
            const Prompt p  = s.prompt();
            core::Status st = core::ok();
            switch (p.kind) {
            case ParamKind::Point:
            case ParamKind::PointList: {
                if (given > 0) {
                    ++asked_after_first;
                    std::string key = spec.id + "|" + p.param;
                    if (spec.id == "core.fit" && given % 2 == 0) key += "#yerel";
                    if (!p.has_rubber_band && !silent_by_design.contains(key))
                        FAIL_WITH("önizlemesiz nokta istemi", key + " — \"" + p.message + "\"");
                }
                st = s.supply(Value::point(points[given % points.size()]));
                ++given;
                break;
            }
            case ParamKind::Number: st = s.supply(Value::number(2.0)); break;
            case ParamKind::Integer: st = s.supply(Value::integer(6)); break;
            case ParamKind::Text:
                st =
                    s.supply(Value::text(p.choices.empty() ? std::string("A") : p.choices.front()));
                break;
            case ParamKind::Bool: st = s.supply(Value::boolean(true)); break;
            case ParamKind::Selection: st = s.supply(Value::ids({1})); break;
            }
            if (!st.ok()) break;
        }
        if (s.waiting()) s.cancel();
        (void)r.bus.finish(s);
    }

    // The walk really reached prompts after a first point, and plenty of them;
    // a registry where every command stopped at its first answer would pass
    // the loop above by default.
    CHECK(asked_after_first > 60);
}

// =============================================================================
// A click is a number only where a distance is asked for
// =============================================================================

TEST_CASE("Bir sayı isteminde tıklama sıfır değildir: reddedilir ve istem açık kalır")
{
    // A POINT IS NOT A NUMBER. A click at a prompt for a number used to be read
    // as zero — `as_number` of a coordinate — so pressing the canvas at a side
    // count or an offset distance gave the command a value nobody typed.
    Rig r;
    auto started = r.bus.begin_interactive("ÇOKGEN", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.prompt().kind == ParamKind::Integer);

    const auto st = s.supply(Value::point(core::Point2{5'000, 5'000}));
    REQUIRE_FALSE(st.ok());
    CHECK(st.error().message.find("bir sayı bekliyor") != std::string::npos);
    CHECK(s.waiting()); ///< the question is still open
    CHECK(s.prompt().kind == ParamKind::Integer);

    CHECK(s.supply(Value::integer(6)).ok()); ///< and the typed answer still works
    s.cancel();
    (void)r.bus.finish(s);
}

TEST_CASE("Bir sözcük isteminde tıklama boş sözcük değildir: reddedilir ve istem açık kalır")
{
    // The same defect one kind over: a click at RENK's colour prompt arrived as
    // the empty string and failed the command with "Tanınmayan renk: ''".
    Rig r;
    r.seed();
    auto started = r.bus.begin_interactive("RENK nesneler=1", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.prompt().kind == ParamKind::Text);

    const auto st = s.supply(Value::point(core::Point2{5'000, 5'000}));
    REQUIRE_FALSE(st.ok());
    CHECK(st.error().message.find("bir sözcük bekliyor") != std::string::npos);
    CHECK(st.error().message.find("listeden seçin") != std::string::npos); ///< RENK offers words
    CHECK(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Text);

    REQUIRE(s.supply(Value::text("mavi")).ok()); ///< and the typed answer still works
    REQUIRE(r.bus.finish(s).ok());
    const core::EntityId e = r.doc.slot_of(static_cast<core::EntityKey>(1));
    CHECK_EQ(r.doc.styles().at(r.doc.entities().style[e]).rgba, 0xFF0000FFu);
}

// =============================================================================
// ESNET: the window in sight, the result under the cursor
// =============================================================================

TEST_CASE("ESNET: başlangıç sorulurken pencere görünür; bitişte sonuç imleci izler")
{
    // The base point used to be asked with nothing on the canvas — the window
    // that decides which corners follow had gone — and the end with a bare line
    // from the base. Now the window stays drawn, and the end prompt carries the
    // window so the canvas draws each object AS IT WILL BE, by the call the
    // command makes with the click.
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 10,10", Origin::Test).ok()); // 1
    auto started = r.bus.begin_interactive("ESNET", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{8'000, -2'000})).ok());
    REQUIRE(s.supply(Value::point(core::Point2{12'000, 12'000})).ok());

    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("baslangic"));
    CHECK(s.prompt().has_rubber_band);
    CHECK(s.prompt().rubber_shape == RubberShape::Fixed);
    REQUIRE_EQ(s.prompt().rubber_chain.size(), 5u);
    CHECK(s.prompt().rubber_chain.front() == s.prompt().rubber_chain.back());

    REQUIRE(s.supply(Value::point(core::Point2{10'000, 0})).ok());
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("bitis"));
    CHECK(s.prompt().rubber_shape == RubberShape::Stretch);
    auto guide = core::decode_stretch_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().window == core::Box2{8'000, -2'000, 12'000, 12'000});
    CHECK(guide.value().keys.empty()); ///< the window's own pick decides

    // THE PREVIEW IS THE RESULT: what the canvas draws for a cursor 3 m east is
    // exactly what the click there writes.
    const core::EntityId line = r.doc.slot_of(static_cast<core::EntityKey>(1));
    auto shown                = core::stretch_entity(r.doc, line, guide.value().window, 3'000, 0);
    REQUIRE(shown.ok());
    REQUIRE(shown.value().has_value());
    CHECK_EQ(shown.value()->moved, 2u);
    const std::vector<core::Point2> expected{{0, 0}, {13'000, 0}, {13'000, 10'000}};
    CHECK(shown.value()->edit.points.front() == expected);

    REQUIRE(s.supply(Value::point(core::Point2{13'000, 0})).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(vertex_of(r.doc, 1, 1), (core::Point2{13'000, 0}));
    CHECK_EQ(vertex_of(r.doc, 1, 2), (core::Point2{13'000, 10'000}));
}

TEST_CASE("KIR: ikinci nokta aranırken gidecek parça işaretli; tıklama tam onu götürür")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok()); // 1
    auto started = r.bus.begin_interactive("KIR", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::ids({1})).ok());
    REQUIRE(
        s.supply(Value::point(core::Point2{30'000, 2'000})).ok()); // off the line: its foot counts

    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("ikinci"));
    CHECK(s.prompt().rubber_shape == RubberShape::Break);
    auto guide = core::decode_break_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK_EQ(guide.value().key, 1);

    // What the canvas marks for a cursor at 70 m is the piece the click removes.
    const core::CurvePath run = *core::path_of(r.doc, r.doc.slot_of(core::EntityKey{1}));
    auto shown = core::break_path(run, s.prompt().rubber_origin, core::Point2{70'000, -1'000});
    REQUIRE(shown.ok());
    CHECK(core::path_vertices(shown.value().gap) ==
          std::vector<core::Point2>{{30'000, 0}, {70'000, 0}});
    REQUIRE_EQ(shown.value().kept.size(), 2u);
    CHECK(core::path_vertices(shown.value().kept[0]) ==
          std::vector<core::Point2>{{0, 0}, {30'000, 0}});
    CHECK(core::path_vertices(shown.value().kept[1]) ==
          std::vector<core::Point2>{{70'000, 0}, {100'000, 0}});

    REQUIRE(s.supply(Value::point(core::Point2{70'000, -1'000})).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(vertex_count(r.doc, 1), 2u);
    CHECK_EQ(vertex_of(r.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(vertex_of(r.doc, 2, 0), (core::Point2{70'000, 0}));

    // Given in either order along the line, the same gap.
    auto reversed = core::break_path(run, core::Point2{70'000, 0}, core::Point2{30'000, 0});
    REQUIRE(reversed.ok());
    CHECK(reversed.value().gap == shown.value().gap);
}

TEST_CASE("BUDA/UZAT: atılacak parça ve eklenecek uzantı imleç üzerindeyken hesaplanır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok()); // 1, the line
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 7,-5 7,5", Origin::Test).ok()); // 2, the boundary
    REQUIRE(r.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());

    auto started = r.bus.begin_interactive("BUDA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::Trim);
    CHECK_FALSE(s.prompt().rubber_base); ///< nothing is aimed from its origin
    auto guide = core::decode_trim_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK_FALSE(guide.value().extend);
    CHECK_FALSE(guide.value().every); ///< the selection is the edges
    CHECK(guide.value().keys == std::vector<std::int64_t>{1, 2});

    // THE CURSOR ON THE LINE'S EAST END: the canvas asks what the click asks —
    // the object under it, the run's edges, `trim_curve` — and marks the piece
    // past the boundary as going.
    const core::Point2 cursor{9'000, 0};
    const core::EntityId line = core::pick_nearest(r.doc, cursor, r.bus.aid_settings().pick_radius);
    REQUIRE(line == r.doc.slot_of(static_cast<core::EntityKey>(1U)));
    const auto path = core::path_of(r.doc, line);
    REQUIRE(path.has_value());
    const auto edges = core::cutting_edges(r.doc, line, guide.value());
    CHECK_EQ(edges.size(), 1u); ///< the line never cuts itself
    auto shown = core::trim_curve(*path, edges, cursor);
    REQUIRE(shown.ok());
    REQUIRE_EQ(shown.value().removed.size(), 1u);
    CHECK(core::path_vertices(shown.value().removed.front()) ==
          std::vector<core::Point2>{{7'000, 0}, {10'000, 0}});
    REQUIRE_EQ(shown.value().cuts.size(), 1u); ///< the candidate the canvas marks
    CHECK_EQ(shown.value().cuts.front().point, (core::Point2{7'000, 0}));

    REQUIRE(s.supply(Value::point(cursor)).ok());
    REQUIRE(s.waiting()); ///< the next piece, until Enter
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(vertex_of(r.doc, 1, 1), (core::Point2{7'000, 0})); ///< what was shown is what went

    // UZAT: the reach it adds, from the end to the boundary, and the run's own
    // edit leaves the line exactly where the preview drew it.
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,2 5,2", Origin::Test).ok()); // 3, short of it
    REQUIRE(r.bus.execute_line("SEÇ TEMİZLE", Origin::Test).ok());
    auto extending = r.bus.begin_interactive("UZAT", Origin::Gui);
    REQUIRE(extending.ok());
    Session& x = *extending.value();
    REQUIRE(x.waiting());
    auto reach_guide = core::decode_trim_guide(x.prompt().rubber_payload);
    REQUIRE(reach_guide.ok());
    CHECK(reach_guide.value().extend);
    CHECK(reach_guide.value().every); ///< nothing selected: every object near it

    const core::Point2 near_end{4'500, 2'000};
    const core::EntityId short_line = r.doc.slot_of(static_cast<core::EntityKey>(3U));
    const auto short_path           = core::path_of(r.doc, short_line);
    REQUIRE(short_path.has_value());
    auto reach = core::extend_curve(
        *short_path, core::cutting_edges(r.doc, short_line, reach_guide.value()), near_end);
    REQUIRE(reach.ok());
    CHECK(core::path_vertices(reach.value().added) ==
          std::vector<core::Point2>{{5'000, 2'000}, {7'000, 2'000}});

    REQUIRE(x.supply(Value::point(near_end)).ok());
    REQUIRE(x.supply(Value{}).ok());
    REQUIRE(r.bus.finish(x).ok());
    CHECK_EQ(vertex_of(r.doc, 3, 1), (core::Point2{7'000, 2'000}));
}

TEST_CASE("BUDA çitle: çitin bütün işi Enter'dan önce tuvalde, uygulananla aynı")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,10 100,10", Origin::Test).ok());  // 2
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,30", Origin::Test).ok()); // 3
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 70,-20 70,30", Origin::Test).ok()); // 4

    auto started = r.bus.begin_interactive("BUDA yontem=çit", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().message.find("Çitin ilk noktası") != std::string::npos);
    REQUIRE(s.supply(Value::point(core::Point2{50'000, -5'000})).ok());

    // THE SECOND CORNER'S PROMPT carries the fence so far and the run's rules;
    // the canvas runs the fence on to the cursor and plans it.
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::TrimFence);
    CHECK(s.prompt().rubber_chain == std::vector<core::Point2>{{50'000, -5'000}});
    auto guide = core::decode_trim_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().every);
    const std::vector<core::Point2> fence{{50'000, -5'000}, {50'000, 15'000}};
    const core::FencePlan plan = core::plan_fence(r.doc, fence, guide.value());
    REQUIRE_EQ(plan.edits.size(), 2u);
    for (const core::FenceEdit& edit : plan.edits) {
        REQUIRE_EQ(edit.cut.removed.size(), 1u);
        CHECK_EQ(core::path_length(edit.cut.removed.front()), core::Mm{40'000});
    }

    REQUIRE(s.supply(Value::point(fence.back())).ok());
    REQUIRE(s.waiting()); ///< another corner, or Enter
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    // WHAT WAS SHOWN IS WHAT WENT: the planned pieces, and nothing else.
    CHECK_EQ(vertex_of(r.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(vertex_of(r.doc, 2, 1), (core::Point2{30'000, 10'000}));
    CHECK_EQ(vertex_of(r.doc, 3, 1), (core::Point2{30'000, 30'000}));
}

TEST_CASE("Tabanı olmayan bir önizleme DİK kilidini saptırmaz")
{
    // A preview that measures from nothing must not steer the answer: with dik
    // mod on, ESNET's base point used to be bent onto a ray through the
    // window's corner, and BUDA's pick onto one through the drawing's zero.
    Rig r;
    REQUIRE(r.bus.execute_line("MOD dik_mod evet", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 10,10", Origin::Test).ok());
    auto started = r.bus.begin_interactive("ESNET", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{8'000, -2'000})).ok());
    REQUIRE(s.supply(Value::point(core::Point2{12'000, 12'000})).ok());
    if (!s.waiting()) FAIL_WITH("ESNET durdu", s.error().message);
    CHECK_FALSE(s.prompt().rubber_base);
    REQUIRE(s.supply(Value::point(core::Point2{10'000, 3'000})).ok()); // not level with a corner
    REQUIRE(s.waiting());
    // The end IS aimed from the base, so dik mod applies there as it always did.
    CHECK(s.prompt().rubber_base);
    REQUIRE(s.supply(Value::point(core::Point2{13'000, 3'400})).ok());
    REQUIRE(r.bus.finish(s).ok());

    const JournalEntry& last = r.journal.entries().back();
    CHECK(last.args.get("baslangic").as_point() == core::Point2{10'000, 3'000}); ///< untouched
    CHECK(last.args.get("bitis").as_point() == core::Point2{13'000, 3'000});     ///< levelled
}

TEST_CASE("DİK mod bir dikdörtgenin karşı köşesini eksene kilitlemez")
{
    // Locked to an axis through the first corner the opposite one makes a
    // rectangle with no width or no height: with dik mod on nobody could drag a
    // window or draw a rectangle at all. Everywhere else dik mod still holds.
    Rig r;
    REQUIRE(r.bus.execute_line("MOD dik_mod evet", Origin::Test).ok());
    auto started = r.bus.begin_interactive("DİKDÖRTGEN", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::Rectangle);
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 12'000})).ok());
    REQUIRE(r.bus.finish(s).ok());
    const core::EntityId e = r.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(e != core::kNoEntity);
    CHECK(r.doc.entities().box_of(e) == core::Box2{0, 0, 20'000, 12'000});

    // And a line's next point is still levelled, as it always was.
    REQUIRE(r.bus.execute_line("ÇİZGİ 0,30 20,31", Origin::Test).ok());
    CHECK_EQ(vertex_of(r.doc, 2, 1), (core::Point2{20'000, 30'000}));
}

TEST_CASE("HİZALA: her adımda nesneler imleçle gider; ikinci çift arayüzden de sorulur")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok()); // 1
    auto started = r.bus.begin_interactive("HİZALA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::ids({1})).ok());
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());

    // The first target: the objects themselves, by key, carried by the move.
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::Ghost);
    auto first = core::decode_ghost_spec(s.prompt().rubber_payload);
    REQUIRE(first.has_value());
    CHECK(first->keys == std::vector<std::int64_t>{1});
    REQUIRE(s.supply(Value::point(core::Point2{50'000, 50'000})).ok());

    // The second source is ASKED, with the first pair kept in sight and nothing
    // aimed from it.
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("kaynak2"));
    CHECK(s.prompt().rubber_shape == RubberShape::Fixed);
    CHECK_FALSE(s.prompt().rubber_base);
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 0})).ok());

    // The second target turns the objects under the cursor — by the transform
    // the click applies.
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().param, std::string("hedef2"));
    auto turning = core::decode_ghost_spec(s.prompt().rubber_payload);
    REQUIRE(turning.has_value());
    CHECK(turning->kind == core::GhostKind::Align);
    const core::Point2 cursor{50'000, 70'000}; // a quarter turn counter-clockwise
    const core::Xform shown         = core::ghost_xform(*turning, s.prompt().rubber_origin, cursor);
    const core::Point2 corner_there = core::transformed(shown, core::Point2{20'000, 10'000});

    REQUIRE(s.supply(Value::point(cursor)).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK_EQ(vertex_of(r.doc, 1, 2), corner_there); ///< the ghost was the result
    CHECK_EQ(vertex_of(r.doc, 1, 2), (core::Point2{40'000, 70'000}));
}

TEST_CASE("move_grips: tutamaklar sırayla, her biri öncekinin bıraktığı şekle göre taşınır")
{
    // What moving them one by one in the document would do, computed without
    // touching it. A circle's centre carries its handle, so the handle's own
    // move then lands where it already is: a translation, not a growth.
    Rig r;
    REQUIRE(r.bus.execute_line("DAİRE 0,0 2,0", Origin::Test).ok()); // centre (0,0), r = 2 m
    const core::EntityId circle = r.doc.slot_of(static_cast<core::EntityKey>(1));
    const std::vector<core::GripMove> both{{0, {5'000, 0}}, {1, {7'000, 0}}};
    auto moved = core::move_grips(r.doc, circle, both);
    REQUIRE(moved.ok());
    CHECK(moved.value().points[0][0] == core::Point2{5'000, 0});
    CHECK(moved.value().points[0][1] == core::Point2{7'000, 0}); ///< r still 2 m

    // A handle alone sets the radius.
    const std::vector<core::GripMove> rim{{1, {4'000, 0}}};
    auto grown = core::move_grips(r.doc, circle, rim);
    REQUIRE(grown.ok());
    CHECK(grown.value().points[0][1] == core::Point2{4'000, 0});

    // The document is untouched by either.
    CHECK_EQ(r.undo.undo_depth(), 1u);
    const std::uint32_t slot = r.doc.entities().slot[circle];
    CHECK_EQ(r.doc.geometry().ring_xs(r.doc.geometry().rings_of(slot).first)[1], 2'000);
}

// =============================================================================
// PAH and YUVARLA: one click on the corner
// =============================================================================

TEST_CASE(
    "PAH: seçim yokken köşeye tek tıklama nesneyi ve köşeyi seçer; tıklanan uzaklık mesafedir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 20,0 20,10", Origin::Test).ok());

    auto started = r.bus.begin_interactive("PAH", Origin::Gui);
    REQUIRE(started.ok()); ///< it used to refuse here: "PAH için nesne belirtilmedi"
    Session& s = *started.value();

    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Point);
    CHECK(s.prompt().message.find("köşeye tıklayın") != std::string::npos);
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 0})).ok());

    // THE SIZE: typed or shown, with the cut drawn at the cursor.
    REQUIRE(s.waiting());
    const Prompt size = s.prompt();
    CHECK(size.kind == ParamKind::Number);
    CHECK(size.pick_distance);
    CHECK(size.has_rubber_band);
    CHECK(size.rubber_shape == RubberShape::Corner);
    CHECK(size.rubber_origin == core::Point2{20'000, 0});
    auto guide = core::decode_corner_preview(size.rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().key == 1);
    CHECK(guide.value().at == 1);
    CHECK_FALSE(guide.value().fillet);

    // A click 3 m up the next edge answers 3 m.
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 3'000})).ok());
    auto done = r.bus.finish(s);
    REQUIRE(done.ok());

    REQUIRE(vertex_count(r.doc, 1) == 4);
    CHECK(vertex_of(r.doc, 1, 1) == core::Point2{17'000, 0});
    CHECK(vertex_of(r.doc, 1, 2) == core::Point2{20'000, 3'000});

    // AND THE JOURNAL HOLDS THE NUMBER, not the click: a replay supplies what
    // the command asked for.
    const auto& last = r.journal.entries().back();
    CHECK(last.command_id == "core.chamfer");
    CHECK(last.args.get("mesafe").as_number() == doctest::Approx(3.0));
    CHECK(last.args.get("nesne").as_ids() == Value::Ints{1});
}

TEST_CASE("YUVARLA: tek tıklama ve yazılan yarıçap; kapalı alanı sebebiyle reddeder")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 20,0 20,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ALAN 30,0 50,0 50,20 30,20", Origin::Test).ok());
    {
        auto started = r.bus.begin_interactive("YUVARLA", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.supply(Value::point(core::Point2{20'000, 0})).ok());
        REQUIRE(s.waiting());
        CHECK(s.prompt().rubber_shape == RubberShape::Corner);
        REQUIRE(s.supply(Value::number(2.0)).ok());
        REQUIRE(r.bus.finish(s).ok());
        // Two legs and the arc between them.
        CHECK_EQ(r.doc.live_entity_count(), std::size_t{4});
    }
    {
        // A PARCEL CORNER is rounded in place: the parcel stays one object.
        const std::size_t before = r.doc.live_entity_count();
        auto started             = r.bus.begin_interactive("YUVARLA", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.supply(Value::point(core::Point2{50'000, 20'000})).ok());
        REQUIRE(s.waiting());
        CHECK(s.prompt().rubber_shape == RubberShape::Corner);
        REQUIRE(s.supply(Value::number(2.0)).ok());
        REQUIRE(r.bus.finish(s).ok());
        CHECK_EQ(r.doc.live_entity_count(), before);
    }
    {
        // A CLICK ON NOTHING says so.
        auto started = r.bus.begin_interactive("PAH", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.supply(Value::point(core::Point2{90'000, 90'000})).ok());
        const std::string why = REFUSED(r.bus.finish(s));
        CHECK(why.find("köşesi kesilecek bir çizgi ya da alan yok") != std::string::npos);
    }
}

// =============================================================================
// KÖŞETAŞI and KÖŞEEKLE: one click names the object and the corner
// =============================================================================

TEST_CASE("KÖŞETAŞI: köşeye tıklama nesneyi ve köşeyi seçer; önizleme nesnenin yeni biçimidir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    // A VIEW, so a click has the pick box a canvas gives it: 50 mm a pixel.
    r.bus.aids().set_view_scale(50.0);

    auto started = r.bus.begin_interactive("KÖŞETAŞI", Origin::Gui);
    REQUIRE(started.ok()); ///< it used to refuse: "Düzenlenecek nesne belirtilmedi"
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "yer");
    REQUIRE(s.supply(Value::point(core::Point2{20'100, 9'900})).ok());

    REQUIRE(s.waiting());
    const Prompt aim = s.prompt();
    CHECK(aim.param == "nokta");
    CHECK(aim.rubber_shape == RubberShape::Grip);
    CHECK(aim.rubber_origin == core::Point2{20'000, 10'000});
    auto guide = core::decode_grip_guide(aim.rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().key == 1);
    CHECK(guide.value().index == 2);
    CHECK_FALSE(guide.value().insert);

    REQUIRE(s.supply(Value::point(core::Point2{25'000, 12'000})).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK(vertex_of(r.doc, 1, 2) == core::Point2{25'000, 12'000});

    // The corner the click named is what the journal holds; the click itself
    // stood in for it and is not kept.
    const auto& last = r.journal.entries().back();
    CHECK(last.args.get("kose").as_int() == 3);
    CHECK(last.args.get("yer").empty());
}

TEST_CASE("KÖŞEEKLE: kenara tıklama kenarı seçer ve yeni köşe imleci izler")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    r.bus.aids().set_view_scale(50.0);

    auto started = r.bus.begin_interactive("KÖŞEEKLE", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().message.find("kenara tıklayın") != std::string::npos);
    REQUIRE(s.supply(Value::point(core::Point2{10'000, 100})).ok()); ///< on the bottom edge

    REQUIRE(s.waiting());
    auto guide = core::decode_grip_guide(s.prompt().rubber_payload);
    REQUIRE(guide.ok());
    CHECK(guide.value().insert);
    CHECK(guide.value().index == 0);

    REQUIRE(s.supply(Value::point(core::Point2{10'000, -3'000})).ok());
    REQUIRE(r.bus.finish(s).ok());
    REQUIRE(vertex_count(r.doc, 1) == 5);
    CHECK(vertex_of(r.doc, 1, 1) == core::Point2{10'000, -3'000});
    CHECK(r.journal.entries().back().args.get("kose").as_int() == 1);
}

// =============================================================================
// The tools that refused at the press now ask
// =============================================================================

TEST_CASE("Seçim yokken basılan araçlar reddetmek yerine sorar")
{
    // Every one of these answered a press with nothing highlighted by refusing:
    // "seçim boş", "Seçili: 0", "zorunlu parametre eksik". Each now puts up the
    // question its first missing answer is.
    struct Case
    {
        const char* line;
        ParamKind kind;
        const char* says;
    };

    const Case cases[] = {
        {"SİL", ParamKind::Selection, "Silinecek nesneleri seçin"},
        {"TEVHİT", ParamKind::Selection, "Birleştirilecek parselleri seçin"},
        {"İFRAZ", ParamKind::Selection, "İfraz edilecek parseli seçin"},
        {"ALANİFRAZ", ParamKind::Selection, "ifraz edilecek parseli seçin"},
        {"ALANAÇEVİR", ParamKind::Selection, "Alana çevrilecek çizgileri seçin"},
        {"YAZIDÜZENLE", ParamKind::Selection, "Düzenlenecek yazıyı seçin"},
        {"NESNENOKTALARI", ParamKind::Selection, "Noktaları istenen nesneleri seçin"},
        {"PAH", ParamKind::Point, "köşeye tıklayın"},
        {"YUVARLA", ParamKind::Point, "köşeye tıklayın"},
        {"KÖŞETAŞI", ParamKind::Point, "köşeye tıklayın"},
        {"KÖŞEEKLE", ParamKind::Point, "kenara tıklayın"},
        {"HACİM", ParamKind::Number, "Karşılaştırma kotu"},
        {"OTURT", ParamKind::Point, "1. çiftin çizimdeki noktası"},
        {"ARAÇARA", ParamKind::Text, "Aranan sözcük"},
        {"İŞŞABLONU", ParamKind::Text, "listele / goster"},
    };
    for (const Case& c : cases) {
        Rig r;
        r.seed();
        auto started = r.bus.begin_interactive(c.line, Origin::Gui);
        if (!started) {
            FAIL_WITH(c.line, started.error().message);
            continue;
        }
        Session& s = *started.value();
        CHECK_MESSAGE(s.waiting(), c.line);
        if (!s.waiting()) {
            (void)r.bus.finish(s);
            continue;
        }
        CHECK_MESSAGE(s.prompt().kind == c.kind, c.line);
        const std::string said = std::string(c.line) + ": " + s.prompt().message;
        CHECK_MESSAGE(s.prompt().message.find(c.says) != std::string::npos, said);
        s.cancel();
        const auto done = r.bus.finish(s);
        CHECK_MESSAGE(done.ok(), c.line);
    }
}

TEST_CASE("SİL ve KES seçim yokken sorulan nesneleri alır; betik adsız çağrıda sebebi duyar")
{
    Rig r;
    r.seed();
    {
        auto started = r.bus.begin_interactive("SİL", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.waiting());
        REQUIRE(s.supply(Value::ids({4})).ok());
        REQUIRE(r.bus.finish(s).ok());
        const core::EntityId gone = r.doc.slot_of(static_cast<core::EntityKey>(4));
        CHECK((gone == core::kNoEntity || !r.doc.alive(gone)));
    }
    {
        // A script that names nothing and has nothing highlighted is told so.
        const std::string why = REFUSED(r.bus.execute_line("SİL", Origin::Script));
        CHECK(why.find("seçim boş") != std::string::npos);
        CHECK(why.find("SİL nesneler=1") != std::string::npos);
    }
}

TEST_CASE("OTURT: çiftler tek tek sorulur; Enter bitirir ve çizim oturtulur")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());

    auto started = r.bus.begin_interactive("OTURT", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();

    const core::Point2 answers[] = {
        {0, 0}, {1'000'000, 2'000'000}, {10'000, 0}, {1'010'000, 2'000'000}};
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(s.waiting());
        CHECK(s.prompt().kind == ParamKind::Point);
        // The map point of a pair is aimed FROM its local point.
        CHECK(s.prompt().has_rubber_band == (i % 2 == 1));
        REQUIRE(s.supply(Value::point(answers[i])).ok());
    }
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value{}).ok()); ///< Enter: no third pair
    REQUIRE(r.bus.finish(s).ok());

    CHECK(vertex_of(r.doc, 1, 0) == core::Point2{1'000'000, 2'000'000});
    CHECK(vertex_of(r.doc, 1, 1) == core::Point2{1'010'000, 2'000'000});
}

TEST_CASE("DÖNÜŞTÜR hedef sistemi veri paketinin dilimlerini önererek sorar")
{
    Rig r;
    r.seed();
    auto started = r.bus.begin_interactive("DÖNÜŞTÜR", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    if (!s.waiting()) {
        // A build without PROJ says so before it asks anything.
        const std::string why = REFUSED(r.bus.finish(s));
        CHECK(why.find("PROJ") != std::string::npos);
        return;
    }
    CHECK(s.prompt().kind == ParamKind::Text);
    CHECK(s.prompt().message.find("Hedef koordinat sistemi") != std::string::npos);
    const auto& offered = s.prompt().choices;
    CHECK(std::find(offered.begin(), offered.end(), "TUREF/TM30") != offered.end());
    s.cancel();
    (void)r.bus.finish(s);
}

TEST_CASE("YAZIDÜZENLE değişiklik verilmemişse yeni metni, eskisini önererek sorar")
{
    Rig r;
    REQUIRE(r.bus.execute_line("METİN 0,0 yazi=\"101 ada\"", Origin::Test).ok());

    auto started = r.bus.begin_interactive("YAZIDÜZENLE nesneler=1", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Text);
    CHECK(s.prompt().param == "yazi");
    REQUIRE(s.prompt().choices.size() == 1);
    CHECK(s.prompt().choices.front() == "101 ada");

    REQUIRE(s.supply(Value::text("101 ada 4 parsel")).ok());
    REQUIRE(r.bus.finish(s).ok());
    const core::EntityId e = r.doc.slot_of(static_cast<core::EntityKey>(1));
    CHECK(r.doc.texts().text(r.doc.entities().slot[e]) == "101 ada 4 parsel");
}

TEST_CASE("ÇİZGİDÜZENLE tanınmayan işlemi hiçbir şeyi düzenlemeden reddeder")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 20,0 20,10", Origin::Test).ok());
    const std::uint64_t before = r.doc.content_hash();

    auto started = r.bus.begin_interactive("ÇİZGİDÜZENLE nesne=1", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::text("boyle")).ok());
    r.echoed.clear();
    const std::string why = REFUSED(r.bus.finish(s));
    CHECK(why.find("Tanınmayan işlem: 'boyle'") != std::string::npos);
    // It used to say "düzenlendi" first and be refused after.
    CHECK(r.echoed.find("düzenlendi") == std::string::npos);
    CHECK(r.doc.content_hash() == before);
}

// =============================================================================
// core: the corner cut and the grip edits the previews draw
// =============================================================================

TEST_CASE("core::cut_corner pahı, yuvarlatmayı ve sınırları hesaplar")
{
    const std::vector<core::Point2> run{{0, 0}, {20'000, 0}, {20'000, 10'000}};

    auto chamfer = core::cut_corner(run, false, 1, 3'000, false);
    REQUIRE(chamfer.ok());
    CHECK(chamfer.value().kept ==
          std::vector<core::Point2>{{0, 0}, {17'000, 0}, {20'000, 3'000}, {20'000, 10'000}});
    CHECK_FALSE(chamfer.value().arc);

    // A right angle's fillet: the tangent points are the radius back along each
    // edge and the centre is the radius in from both.
    auto fillet = core::cut_corner(run, false, 1, 2'000, true);
    REQUIRE(fillet.ok());
    CHECK(fillet.value().arc);
    CHECK(fillet.value().centre == core::Point2{18'000, 2'000});
    CHECK(fillet.value().radius == 2'000);
    CHECK(fillet.value().kept.back() == core::Point2{18'000, 0});
    CHECK(fillet.value().second.front() == core::Point2{20'000, 2'000});

    // Too long for the edge and an end are refused.
    CHECK_FALSE(core::cut_corner(run, false, 1, 12'000, false).ok());
    CHECK_FALSE(core::cut_corner(run, false, 0, 1'000, false).ok());
    const std::vector<core::Point2> square{{0, 0}, {10'000, 0}, {10'000, 10'000}, {0, 10'000}};
    CHECK(core::cut_corner(square, true, 2, 1'000, false).ok());

    // A CLOSED RING IS ROUNDED IN PLACE: one ring, the corner replaced by the
    // two tangent points and the arc drawn between them in the ring's order —
    // both for a counter-clockwise ring and for its mirror image.
    for (const bool reversed : {false, true}) {
        std::vector<core::Point2> ring = square;
        if (reversed) std::reverse(ring.begin(), ring.end());
        const std::size_t corner = reversed ? 1 : 2; // (10000, 10000) either way
        auto round               = core::cut_corner(ring, true, corner, 1'000, true);
        REQUIRE(round.ok());
        CHECK_FALSE(round.value().arc);
        CHECK(round.value().rounded);
        CHECK(round.value().second.empty());
        CHECK_EQ(round.value().edges, 16u);
        CHECK(round.value().deviation <= 2);
        const auto& kept = round.value().kept;
        REQUIRE_EQ(kept.size(), square.size() - 1 + 17);
        CHECK(kept[corner] == round.value().cut_a);
        CHECK(kept[corner + 16] == round.value().cut_b);
        // The arc runs FROM the tangent point toward the previous vertex: the
        // ring does not fold back on itself.
        CHECK(core::distance_squared(kept[corner], ring[corner - 1]) <
              core::distance_squared(kept[corner + 16], ring[corner - 1]));
    }

    // The nearest vertex names the corner; an end names nothing.
    CHECK(core::nearest_corner(run, false, {19'000, 1'000}) == std::optional<std::size_t>{1});
    CHECK_FALSE(core::nearest_corner(run, false, {1'000, 0}).has_value());
    CHECK(core::nearest_corner(square, true, {100, 100}) == std::optional<std::size_t>{0});

    // The preview payload survives its round trip and refuses strange bytes.
    const auto bytes = core::encode_corner_preview({.key = 42, .at = 3, .fillet = true});
    auto back        = core::decode_corner_preview(bytes);
    REQUIRE(back.ok());
    CHECK(back.value().key == 42);
    CHECK(back.value().at == 3);
    CHECK(back.value().fillet);
    CHECK_FALSE(core::decode_corner_preview(std::vector<std::uint8_t>{2, 0}).ok());
}

TEST_CASE("core: köşe ekleme, en yakın tutamak ve en yakın kenar")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,20 20,20", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("DAİRE 50,50 55,50", Origin::Test).ok());
    const core::EntityId face = r.doc.slot_of(static_cast<core::EntityKey>(1));
    const core::EntityId open = r.doc.slot_of(static_cast<core::EntityKey>(2));
    const core::EntityId disc = r.doc.slot_of(static_cast<core::EntityKey>(3));

    CHECK(core::nearest_grip(r.doc, face, {19'000, 9'000}) == std::optional<std::size_t>{2});
    CHECK(core::nearest_edge(r.doc, face, {10'000, 100}) == std::optional<std::size_t>{0});
    // THE CLOSING EDGE IS AN EDGE: from the last corner back to the first.
    CHECK(core::nearest_edge(r.doc, face, {100, 5'000}) == std::optional<std::size_t>{3});
    CHECK_FALSE(core::nearest_edge(r.doc, disc, {50'000, 55'000}).has_value());

    auto bent = core::insert_vertex(r.doc, face, 3, {-2'000, 5'000});
    REQUIRE(bent.ok());
    CHECK(bent.value().points[0].size() == 5);
    CHECK(bent.value().points[0].back() == core::Point2{-2'000, 5'000});

    // No edge leaves an open run's last vertex, and a curve takes no corner.
    CHECK_FALSE(core::insert_vertex(r.doc, open, 1, {10'000, 25'000}).ok());
    CHECK_FALSE(core::insert_vertex(r.doc, disc, 0, {60'000, 50'000}).ok());

    core::EmitBuffer drawn;
    CHECK(core::insert_preview(r.doc, face, 0, {10'000, -3'000}, drawn));
    CHECK(drawn.run_total() == 1);

    const auto bytes = core::encode_grip_guide({.key = 7, .index = 2, .insert = true});
    auto back        = core::decode_grip_guide(bytes);
    REQUIRE(back.ok());
    CHECK(back.value().key == 7);
    CHECK(back.value().index == 2);
    CHECK(back.value().insert);
    CHECK_FALSE(core::decode_grip_guide(std::vector<std::uint8_t>{1}).ok());
}

// =============================================================================
// Measuring: a run, a face by its corners, and the answer left on the canvas
// =============================================================================

TEST_CASE("ÖLÇ noktadan noktaya ölçer: her kenar, toplam ve tuvalde kalan işaret")
{
    Rig r;
    std::vector<MeasureMark> marks;
    r.bus.on_measure_mark = [&marks](const MeasureMark& m) { marks.push_back(m); };

    auto started = r.bus.begin_interactive("ÖLÇ", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().rubber_shape == RubberShape::MeasureRun);
    REQUIRE(s.supply(Value::point(core::Point2{3'000, 4'000})).ok()); ///< 5 m

    // IT DOES NOT STOP AT TWO: the next point is asked for, with the run so far.
    REQUIRE(s.waiting());
    CHECK(s.prompt().param == "devam");
    CHECK(s.prompt().rubber_shape == RubberShape::MeasureRun);
    CHECK(s.prompt().rubber_chain.size() == 2);
    REQUIRE(s.supply(Value::point(core::Point2{3'000, 10'000})).ok()); ///< 6 m more
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value{}).ok()); ///< Enter
    REQUIRE(r.bus.finish(s).ok());

    CHECK(r.echoed.find("Mesafe: 5,000 m") != std::string::npos);
    CHECK(r.echoed.find("Kenar 2: 6,000 m") != std::string::npos);
    CHECK(r.echoed.find("Toplam: 11,000 m") != std::string::npos);
    CHECK(r.echoed.find("Toplam uzunluk: 11,000 m") != std::string::npos);

    REQUIRE(marks.size() == 1);
    CHECK(marks[0].shape == MeasureMark::Shape::Run);
    CHECK(marks[0].points.size() == 3);
    CHECK(marks[0].labels == std::vector<std::string>{"5,000 m", "6,000 m", "toplam 11,000 m"});
}

TEST_CASE("ÖLÇ iki noktayla eskisi gibi tek kenar verir; betik devam= ile sürdürür")
{
    {
        Rig r;
        REQUIRE(r.bus.execute_line("ÖLÇ 0,0 3,4", Origin::Script).ok());
        CHECK(r.echoed.find("Mesafe: 5,000 m") != std::string::npos);
        CHECK(r.echoed.find("Toplam uzunluk") == std::string::npos);
    }
    {
        Rig r;
        REQUIRE(r.bus.execute_line("ÖLÇ 0,0 3,4 devam=3,10 devam=0,10", Origin::Script).ok());
        CHECK(r.echoed.find("Kenar 3: 3,000 m") != std::string::npos);
        CHECK(r.echoed.find("Toplam uzunluk: 14,000 m   (3 kenar)") != std::string::npos);
    }
}

TEST_CASE("ALANÖLÇ köşelerden ölçer: alan imleçle birlikte, sonuç tuvalde")
{
    Rig r;
    std::vector<MeasureMark> marks;
    r.bus.on_measure_mark = [&marks](const MeasureMark& m) { marks.push_back(m); };

    auto started = r.bus.begin_interactive("ALANÖLÇ yontem=nokta", Origin::Gui);
    REQUIRE(started.ok());
    Session& s                   = *started.value();
    const core::Point2 corners[] = {{0, 0}, {20'000, 0}, {20'000, 10'000}, {0, 10'000}};
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(s.waiting());
        CHECK(s.prompt().param == "noktalar");
        // The face follows the cursor from the second corner on.
        CHECK((s.prompt().rubber_shape == RubberShape::MeasureRing) == (i > 0));
        REQUIRE(s.supply(Value::point(corners[i])).ok());
    }
    REQUIRE(s.supply(Value{}).ok()); ///< Enter
    REQUIRE(r.bus.finish(s).ok());

    CHECK(r.echoed.find("Alan: 200,00 m²   çevre: 60,000 m   (4 köşe)") != std::string::npos);
    REQUIRE(marks.size() == 1);
    CHECK(marks[0].shape == MeasureMark::Shape::Ring);
    CHECK(marks[0].points.size() == 4);

    // A SCRIPT's corners say which method it means.
    Rig scripted;
    REQUIRE(scripted.bus.execute_line("ALANÖLÇ noktalar=0,0 20,0 20,10 0,10", Origin::Script).ok());
    CHECK(scripted.echoed.find("Alan: 200,00 m²") != std::string::npos);

    // Two corners enclose nothing.
    Rig few;
    const std::string why =
        REFUSED(few.bus.execute_line("ALANÖLÇ yontem=nokta noktalar=0,0 20,0", Origin::Script));
    CHECK(why.find("en az üç köşe") != std::string::npos);
}

TEST_CASE("ALANÖLÇ açık bir çizgiye alan değil uzunluk der")
{
    // "alan: 0,00 m²" read as a measured empty parcel; the figure the user
    // wanted was printed under the name `çevre`.
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 30,0 30,40", Origin::Test).ok());
    r.echoed.clear();
    REQUIRE(r.bus.execute_line("ALANÖLÇ nesneler=1", Origin::Script).ok());
    CHECK(r.echoed.find("kapalı değil, alanı yok; uzunluk: 70,000 m") != std::string::npos);
    CHECK(r.echoed.find("0,00 m²") == std::string::npos);
}

TEST_CASE("KOORDİNAT ve AÇIÖLÇ okumalarını tuvalde bırakır")
{
    Rig r;
    std::vector<MeasureMark> marks;
    r.bus.on_measure_mark = [&marks](const MeasureMark& m) { marks.push_back(m); };
    REQUIRE(r.bus.execute_line("KOORDİNAT 485320.150,4310220.400", Origin::Script).ok());
    REQUIRE(r.bus.execute_line("AÇIÖLÇ 0,0 10,0 0,10", Origin::Script).ok());
    REQUIRE(marks.size() == 2);
    CHECK(marks[0].shape == MeasureMark::Shape::Point);
    CHECK(marks[0].labels.front() == "Y 485320,150 m  X 4310220,400 m");
    CHECK(marks[1].shape == MeasureMark::Shape::Angle);
    CHECK(marks[1].points.size() == 3);
    CHECK_FALSE(marks[1].labels.front().empty());
}

// =============================================================================
// OFSET: the real parallel, on the side that is shown (TODOS C-03)
// =============================================================================

namespace {

/// The vertices of the only ring of the entity with `key`.
std::vector<core::Point2> ring_of(const core::Document& doc, std::int64_t key)
{
    std::vector<core::Point2> out;
    const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
    if (e == core::kNoEntity) return out;
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
    const auto xs             = doc.geometry().ring_xs(span.first);
    const auto ys             = doc.geometry().ring_ys(span.first);
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(core::Point2{xs[v], ys[v]});
    return out;
}

} // namespace

TEST_CASE("OFSET: açık çizginin sol 2 m paraleli açık (0,2)→(10,2) çizgisidir, bant değil")
{
    Rig r;
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=2000 taraf=sol", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() == 2);
    const core::EntityId made = r.doc.slot_of(static_cast<core::EntityKey>(2));
    CHECK(
        r.doc.geometry().ring_role[r.doc.geometry().rings_of(r.doc.entities().slot[made]).first] ==
        core::RingRole::Open);
    CHECK(ring_of(r.doc, 2) == std::vector<core::Point2>{{0, 2'000}, {10'000, 2'000}});

    // RIGHT IS THE OTHER SIDE, and reversing the line reverses both.
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=2000 taraf=sag", Origin::Test).ok());
    CHECK(ring_of(r.doc, 3) == std::vector<core::Point2>{{0, -2'000}, {10'000, -2'000}});
    Rig back;
    REQUIRE(back.bus.execute_line("ÇOKLUÇİZGİ 10,0 0,0", Origin::Test).ok());
    REQUIRE(back.bus.execute_line("OFSET nesneler=1 mesafe=2000 taraf=sol", Origin::Test).ok());
    CHECK(ring_of(back.doc, 2) == std::vector<core::Point2>{{10'000, -2'000}, {0, -2'000}});
}

TEST_CASE("OFSET: daire daire kalır, yarıçapı değişir; çöken sonuç açıklanır")
{
    Rig r;
    REQUIRE(r.bus.execute_line("DAİRE 0,0 5,0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=2000", Origin::Test).ok()); ///< plus: out
    const core::EntityId grown = r.doc.slot_of(static_cast<core::EntityKey>(2));
    REQUIRE(grown != core::kNoEntity);
    CHECK(r.doc.entities().kind[grown] == core::kCircleKind);
    CHECK(r.doc.entities().box_of(grown).max_x == 7'000);

    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=-2000", Origin::Test).ok()); ///< minus: in
    CHECK(r.doc.entities().box_of(r.doc.slot_of(static_cast<core::EntityKey>(3))).max_x == 3'000);

    const std::string why =
        REFUSED(r.bus.execute_line("OFSET nesneler=1 mesafe=6000 taraf=ic", Origin::Test));
    CHECK(why.find("paraleli kalmıyor") != std::string::npos);
}

TEST_CASE("OFSET: delikli alanın paraleli deliğiyle birlikte alandır")
{
    Rig r;
    REQUIRE(r.bus
                .execute_line("ALAN 0,0 20,0 20,20 0,20 bolum=4 5,5 15,5 15,15 5,15 bolum=4",
                              Origin::Test)
                .ok());
    const core::EntityId src = r.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(r.doc.geometry().rings_of(r.doc.entities().slot[src]).count == 2);
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=1000 taraf=dis", Origin::Test).ok());
    const core::EntityId made = r.doc.slot_of(static_cast<core::EntityKey>(2));
    REQUIRE(made != core::kNoEntity);
    CHECK(r.doc.geometry().rings_of(r.doc.entities().slot[made]).count == 2); ///< the hole stays
}

TEST_CASE("OFSET arayüzde mesafeyi sorar, tarafı imleçle gösterir; Esc hiçbir şey çizmez")
{
    {
        Rig r;
        REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
        REQUIRE(r.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());
        auto started = r.bus.begin_interactive("OFSET", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.waiting());
        CHECK(s.prompt().kind == ParamKind::Number);
        REQUIRE(s.supply(Value::number(2.0)).ok());

        REQUIRE(s.waiting());
        CHECK(s.prompt().param == "nokta");
        CHECK(s.prompt().rubber_shape == RubberShape::Parallel);
        auto guide = core::decode_parallel_preview(s.prompt().rubber_payload);
        REQUIRE(guide.ok());
        CHECK(guide.value().keys == std::vector<std::int64_t>{1});
        CHECK(guide.value().distance == 2'000);

        REQUIRE(s.supply(Value::point(core::Point2{5'000, -3'000})).ok()); ///< below: right
        REQUIRE(r.bus.finish(s).ok());
        CHECK(ring_of(r.doc, 2) == std::vector<core::Point2>{{0, -2'000}, {10'000, -2'000}});
        CHECK(r.journal.entries().back().args.get("nokta").as_point() ==
              core::Point2{5'000, -3'000});
    }
    {
        Rig r;
        REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
        const std::uint64_t before = r.doc.content_hash();
        auto started               = r.bus.begin_interactive("OFSET nesneler=1", Origin::Gui);
        REQUIRE(started.ok());
        Session& s = *started.value();
        REQUIRE(s.supply(Value::number(2.0)).ok());
        REQUIRE(s.waiting());
        s.cancel(); ///< Esc at the side
        const auto done = r.bus.finish(s);
        REQUIRE(done.ok());
        CHECK(done.value().message == "İptal edildi");
        CHECK(r.doc.content_hash() == before);
    }
}

TEST_CASE("OFSET: tam bir satır iki yanı verir; kaynak=sil ve ozellik=aktif söyleneni yapar")
{
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("KATMAN ad=CEKME", Origin::Test).ok());

    // An open line given neither side nor point: both sides, on the source's layer.
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=2000", Origin::Test).ok());
    CHECK(r.doc.live_entity_count() == 3);
    const core::EntityId one = r.doc.slot_of(static_cast<core::EntityKey>(2));
    CHECK(r.doc.layers()[r.doc.entities().layer[one]].name == "YOL");

    // On the active layer when asked, and the source gone when asked.
    REQUIRE(r.bus
                .execute_line("OFSET nesneler=1 mesafe=3000 taraf=sol ozellik=aktif kaynak=sil",
                              Origin::Test)
                .ok());
    const core::EntityId last = r.doc.slot_of(static_cast<core::EntityKey>(4));
    REQUIRE(last != core::kNoEntity);
    CHECK(r.doc.layers()[r.doc.entities().layer[last]].name == "CEKME");
    const core::EntityId source = r.doc.slot_of(static_cast<core::EntityKey>(1));
    CHECK((source == core::kNoEntity || !r.doc.alive(source)));
}

TEST_CASE("OFSET: kaynağın öznitelikleri paralele aktarılır; oznitelik=aktarma aktarmaz")
{
    // C-03's last clause. The kerb lines of a road axis are that road's, so the
    // road's name travels with them, as it does with KOPYALA and with both
    // halves of a BÖL. A setback line inside a parcel is not the parcel, and
    // `aktarma` keeps its ada number from turning up twice.
    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0", Origin::Test).ok()); // 1
    REQUIRE(r.bus.execute_line("SÜTUN kimlik=yol_adi tur=metin", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("ÖZNİTELİK ad=yol_adi nesne=1 deger=ATATURK", Origin::Test).ok());
    const core::AttrId col = r.doc.attributes().find("yol_adi");
    REQUIRE(col != core::kNoAttr);

    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=3500 taraf=sol", Origin::Test).ok()); // 2
    const core::EntityId kerb = r.doc.slot_of(static_cast<core::EntityKey>(2));
    REQUIRE(kerb != core::kNoEntity);
    auto carried = r.doc.attribute(col, kerb);
    REQUIRE(carried.ok());
    CHECK(carried.value().present);
    CHECK(carried.value() ==
          r.doc.attribute(col, r.doc.slot_of(static_cast<core::EntityKey>(1))).value());

    REQUIRE(
        r.bus.execute_line("OFSET nesneler=1 mesafe=5000 taraf=sag oznitelik=aktarma",
                           Origin::Test)
            .ok()); // 3
    const core::EntityId bare = r.doc.slot_of(static_cast<core::EntityKey>(3));
    REQUIRE(bare != core::kNoEntity);
    auto left = r.doc.attribute(col, bare);
    REQUIRE(left.ok());
    CHECK_FALSE(left.value().present);

    // Written down as said, so a replay makes the same choice.
    CHECK_EQ(r.journal.entries().back().args.get("oznitelik").as_text(), std::string("aktarma"));

    // AND WITH THE SOURCE GONE the parallel carries on what it was: a boundary
    // moved by its setback keeps its data.
    REQUIRE(r.bus.execute_line("OFSET nesneler=1 mesafe=1000 taraf=sol kaynak=sil", Origin::Test)
                .ok()); // 4
    const core::EntityId moved = r.doc.slot_of(static_cast<core::EntityKey>(4));
    REQUIRE(moved != core::kNoEntity);
    CHECK(r.doc.attribute(col, moved).value().present);
}

TEST_CASE("OFSET: paraleli olmayan nesne ve yaklaşık eğri sebebiyle söylenir")
{
    Rig r;
    REQUIRE(r.bus.execute_line("NOKTA 0,0", Origin::Test).ok());
    const std::string why =
        REFUSED(r.bus.execute_line("OFSET nesneler=1 mesafe=1000", Origin::Test));
    CHECK(why.find("Nesne 1") != std::string::npos);

    Rig curve;
    REQUIRE(curve.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", Origin::Test).ok());
    curve.echoed.clear();
    REQUIRE(curve.bus.execute_line("OFSET nesneler=1 mesafe=1000", Origin::Test).ok());
    CHECK(curve.echoed.find("kendi türünde paraleli olmayan") != std::string::npos);
    CHECK(curve.echoed.find("sapar") != std::string::npos);
}
