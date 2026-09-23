// SPDX-License-Identifier: GPL-3.0-or-later
//
// DRAWING METHODS, AS A HAND USES THEM (TODOS C-02).
//
// A drawing tool is finished when a wrong corner costs that corner and not the
// drawing, when the ghost under the cursor is the object the click makes, and
// when every method gives the same object whether it is pointed at or typed.
// This file pins those, by driving each command the way the canvas does.
#include "kentos_test.hpp"

#include "kentos_cad/command/aids.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/ghost.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/spline.hpp"

#include <algorithm>
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
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }
};

/// The vertices of the only ring of the object with persistent key `key`.
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

/// The run the journal's last line holds under `noktalar`.
Value::Points journalled_run(const Journal& journal)
{
    if (journal.entries().empty()) return {};
    const Value* run = journal.entries().back().args.find("noktalar");
    return run != nullptr ? run->as_points() : Value::Points{};
}

constexpr core::Point2 kA{0, 0};
constexpr core::Point2 kB{10'000, 0};
constexpr core::Point2 kC{10'000, 10'000};
constexpr core::Point2 kD{0, 10'000};
constexpr core::Point2 kWrong{73'000, 41'000};

} // namespace

// =============================================================================
// A wrong last point is taken back without losing the drawing
// =============================================================================

TEST_CASE("C-02: ÇİZGİ'de yanlış son nokta geri alınır, çizim kaybolmaz")
{
    Rig r;
    auto started = r.bus.begin_interactive("ÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();

    REQUIRE(s.supply(Value::point(kA)).ok());
    REQUIRE(s.supply(Value::point(kB)).ok());
    REQUIRE(s.supply(Value::point(kWrong)).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().can_retract);
    CHECK(s.prompt().message.find("⌫") != std::string::npos);
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{3});
    // HELD, NOT WRITTEN: the run is on the canvas and nowhere else yet.
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{0});

    REQUIRE(s.retract().ok());
    REQUIRE(s.waiting());
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{2});
    CHECK(s.prompt().rubber_origin == kB);

    REQUIRE(s.supply(Value::point(kC)).ok());
    REQUIRE(s.supply(Value{}).ok()); ///< Enter: the end of the run
    auto done = r.bus.finish(s);
    REQUIRE(done.ok());

    CHECK_EQ(r.doc.live_entity_count(), std::size_t{2});
    CHECK(ring_of(r.doc, 1) == std::vector<core::Point2>{kA, kB});
    CHECK(ring_of(r.doc, 2) == std::vector<core::Point2>{kB, kC});
    CHECK_EQ(r.undo.undo_depth(), std::size_t{1});
    REQUIRE_EQ(r.journal.size(), std::size_t{1});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC});
    CHECK(r.echoed.find("2 çizgi çizildi") != std::string::npos);
}

TEST_CASE("C-02: ÇİZGİ Esc'te o ana kadar çizileni yazar")
{
    // The run is held until it ends now, so Esc is where it is written — the
    // way it was written segment by segment before, and still one undo step.
    Rig r;
    auto started = r.bus.begin_interactive("ÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(kA)).ok());
    REQUIRE(s.supply(Value::point(kB)).ok());
    REQUIRE(s.supply(Value::point(kC)).ok());
    s.cancel();
    auto done = r.bus.finish(s);
    REQUIRE(done.ok());
    CHECK(done.value().mutated);
    CHECK_EQ(r.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(r.undo.undo_depth(), std::size_t{1});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC});
}

TEST_CASE("C-02: ÇOKLUÇİZGİ'de geri alınan köşe nesnede de günlükte de yok")
{
    Rig r;
    auto started = r.bus.begin_interactive("ÇOKLUÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const core::Point2 p : {kA, kB, kWrong})
        REQUIRE(s.supply(Value::point(p)).ok());
    REQUIRE(s.retract().ok());
    // Twice in a row: the corner before it goes too, and the run still stands.
    REQUIRE(s.supply(Value::point(kWrong)).ok());
    REQUIRE(s.supply(Value::point(kWrong)).ok());
    REQUIRE(s.retract().ok());
    REQUIRE(s.retract().ok());
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{2});
    for (const core::Point2 p : {kC, kD})
        REQUIRE(s.supply(Value::point(p)).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());

    CHECK_EQ(r.doc.live_entity_count(), std::size_t{1});
    CHECK(ring_of(r.doc, 1) == std::vector<core::Point2>{kA, kB, kC, kD});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC, kD});
}

TEST_CASE("C-02: ilk noktayı geri almak ilk noktayı yeniden sorar")
{
    Rig r;
    auto started = r.bus.begin_interactive("ÇOKLUÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(kWrong)).ok());
    REQUIRE(s.retract().ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().message == "İlk nokta");
    CHECK_FALSE(s.prompt().can_retract); ///< nothing left to take back
    CHECK_FALSE(s.prompt().has_rubber_band);

    REQUIRE(s.supply(Value::point(kA)).ok());
    REQUIRE(s.supply(Value::point(kB)).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK(ring_of(r.doc, 1) == std::vector<core::Point2>{kA, kB});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB});
}

TEST_CASE("C-02: ALAN'da yanlış köşe geri alınır; alan doğru köşelerle kapanır")
{
    Rig r;
    auto started = r.bus.begin_interactive("ALAN");
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const core::Point2 p : {kA, kB, kC, kWrong})
        REQUIRE(s.supply(Value::point(p)).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().can_retract);
    CHECK(s.prompt().rubber_shape == RubberShape::Ring);
    REQUIRE(s.retract().ok());
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{3});
    REQUIRE(s.supply(Value::point(kD)).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());

    CHECK(ring_of(r.doc, 1) == std::vector<core::Point2>{kA, kB, kC, kD});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC, kD});
}

TEST_CASE("C-02: SPLINE'da yanlış kontrol noktası geri alınır")
{
    Rig r;
    auto started = r.bus.begin_interactive("SPLINE");
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const core::Point2 p : {kA, kB, kWrong})
        REQUIRE(s.supply(Value::point(p)).ok());
    REQUIRE(s.retract().ok());
    REQUIRE(s.supply(Value::point(kC)).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());

    CHECK(ring_of(r.doc, 1) == std::vector<core::Point2>{kA, kB, kC});
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC});
}

TEST_CASE("C-02: geri alınacak nokta olmayan istemde geri alma reddedilir")
{
    // `DAİRE`'s centre is ONE point, and there is no run to take a point back
    // from: the refusal is a sentence and the prompt stays exactly where it was.
    Rig r;
    auto started = r.bus.begin_interactive("DAİRE");
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    CHECK_FALSE(s.prompt().can_retract);
    const std::string asked = s.prompt().message;

    const core::Status refused = s.retract();
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("geri alınacak bir nokta yok") != std::string::npos);
    CHECK(s.waiting());
    CHECK(s.prompt().message == asked);
    s.cancel();
    (void)r.bus.finish(s);
}

TEST_CASE("C-02: G, GERİ ve GERİAL'in adları çalışma ortasında son noktayı ister")
{
    Rig r;
    for (const char* word : {"G", "g", "GERİ", "geri", "GERI", "U", "u", "UNDO", "GERİAL", " G "})
        CHECK_MESSAGE(asks_retract(r.reg, word), word);
    for (const char* line : {"", "ÇİZGİ", "G 1", "GERİAL 1", "YİNELE", "10,0", "@5<50"})
        CHECK_FALSE_MESSAGE(asks_retract(r.reg, line), line);
}

// =============================================================================
// The run's own corners are snapped to before they are in the document
// =============================================================================

TEST_CASE("C-02: yakalama çalışmanın kendi köşelerini ve kenarlarını bulur")
{
    // Closing a boundary on its own first corner is the snap a parcel needs
    // most, and that corner is not in the document until the run ends.
    Rig r;
    r.bus.aids().set_view_scale(1.0); ///< 1 mm a pixel: the aperture is 16 mm
    REQUIRE((r.bus.aid_settings().modes & core::SnapEndpoint) != 0);
    REQUIRE((r.bus.aid_settings().modes & core::SnapMidpoint) != 0);

    auto started = r.bus.begin_interactive("ÇOKLUÇİZGİ");
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const core::Point2 p : {kA, kB, kC})
        REQUIRE(s.supply(Value::point(p)).ok());

    // 9 mm from the run's first corner: UÇ takes it.
    REQUIRE(s.supply(Value::point(core::Point2{5, 8})).ok());
    CHECK(s.prompt().rubber_origin == kA);

    // 7 mm off the middle of the run's first edge: ORTA takes it.
    REQUIRE(s.supply(Value::point(core::Point2{5'003, 7})).ok());
    CHECK(s.prompt().rubber_origin == (core::Point2{5'000, 0}));

    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK(journalled_run(r.journal) == Value::Points{kA, kB, kC, kA, core::Point2{5'000, 0}});
}

TEST_CASE("C-02: yakalama bir eğri çalışmasının köşelerini alır, kirişlerini almaz")
{
    // A spline's control polygon is not on the curve, so its edges are not
    // offered — only the control points themselves.
    Rig r;
    r.bus.aids().set_view_scale(1.0);
    auto started = r.bus.begin_interactive("SPLINE");
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const core::Point2 p : {kA, kB, kC})
        REQUIRE(s.supply(Value::point(p)).ok());
    REQUIRE(s.supply(Value::point(core::Point2{5'003, 7})).ok());
    CHECK(s.prompt().rubber_origin == (core::Point2{5'003, 7}));
    s.cancel();
    (void)r.bus.finish(s);
}

TEST_CASE("C-02: komutla birlikte yazılan noktalar çalışmanın kendi köşelerine çekilmez")
{
    // THE REGRESSION the real-window probe caught: with the run's corners on
    // offer, `ÇOKLUÇİZGİ 0,0 20,0 20,12` typed at a view where the aperture is
    // metres wide pulled its third corner onto its second, and the corner a
    // later YUVARLA clicked was gone. A point written down WITH the command was
    // written before the run existed; only an answer at the prompt is aimed at it.
    Rig r;
    r.bus.aids().set_view_scale(1'000.0); ///< 1 m a pixel: the aperture is 16 m
    auto started = r.bus.begin_interactive("ÇOKLUÇİZGİ 0,0 20,0 20,12");
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting()); ///< the run stays open for the hand, as a typed start does
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());
    CHECK(ring_of(r.doc, 1) ==
          std::vector<core::Point2>{kA, core::Point2{20'000, 0}, core::Point2{20'000, 12'000}});
}

// =============================================================================
// The ghost is the object, and every method gives one object pointed or typed
// =============================================================================

namespace {

/// The runs the renderer draws every live object with, in key order: the
/// kind's own outline, or a polyline's and a face's rings.
std::vector<GhostRun> drawn(const core::Document& doc)
{
    std::vector<GhostRun> out;
    const core::EntityTable& entities = doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!doc.alive(e)) continue;
        core::EmitBuffer buf;
        if (core::entity_outline(doc, e, buf)) {
            for (std::size_t r = 0; r < buf.run_start.size(); ++r) {
                GhostRun run;
                run.closed = buf.run_closed[r] != 0;
                for (std::uint32_t v = 0; v < buf.run_count[r]; ++v)
                    run.points.push_back(
                        core::Point2{buf.xs[buf.run_start[r] + v], buf.ys[buf.run_start[r] + v]});
                out.push_back(std::move(run));
            }
            continue;
        }
        const core::RingSpan span = doc.geometry().rings_of(entities.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            GhostRun run;
            run.closed    = doc.geometry().ring_role[r] != core::RingRole::Open;
            const auto xs = doc.geometry().ring_xs(r);
            const auto ys = doc.geometry().ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                run.points.push_back(core::Point2{xs[v], ys[v]});
            out.push_back(std::move(run));
        }
    }
    return out;
}

/// Whether two runs are one drawn shape: an open run vertex for vertex, a
/// closed one from any starting corner and in either winding — the geometry
/// layer stores a face counter-clockwise whichever way it was clicked.
bool same_shape(const GhostRun& a, const GhostRun& b)
{
    if (a.closed != b.closed || a.points.size() != b.points.size()) return false;
    if (!a.closed) return a.points == b.points;
    const std::size_t n = a.points.size();
    for (int direction : {1, -1})
        for (std::size_t shift = 0; shift < n; ++shift) {
            bool all = true;
            for (std::size_t i = 0; i < n && all; ++i) {
                const std::size_t j = direction == 1 ? (i + shift) % n : (shift + n - i % n) % n;
                all                 = a.points[i] == b.points[j];
            }
            if (all) return true;
        }
    return false;
}

/// Where the canvas draws the ghost for a cursor at `at`: the point the snap
/// preview resolves to, by the same aids, base, marks and run the command uses
/// when the click lands (`MapCanvas::updateSnapPreview`).
core::Point2 aimed(Rig& r, const Prompt& p, core::Point2 at)
{
    const AidSettings aids = aids_for(r.bus.aid_settings(), p);
    const core::SnapResult s =
        r.bus.aids().resolve(r.doc, aids, at, aimed_from_origin(p), p.rubber_origin,
                             r.bus.tracking_marks(), pending_run(p));
    return s.mode == core::SnapNone ? at : s.point;
}

Value pt(core::Mm x, core::Mm y)
{
    return Value::point(core::Point2{x, y});
}

/// One drawing method, as a hand runs it and as it is typed.
struct Method
{
    const char* name;          ///< for the report
    const char* setup;         ///< a line run first on every road, or nullptr
    const char* start;         ///< what starts the run — a button's line
    std::vector<Value> before; ///< the answers before the prompt the ghost is read at
    core::Point2 at;           ///< the answer the ghost is read at, POINTED
    bool run_of_points;        ///< Enter ends the run after it (ÇOKLUÇİZGİ, ALAN, SPLINE)
    const char* typed;         ///< the same object in one typed line, or nullptr
};

std::vector<Method> methods()
{
    return {
        {"DAİRE merkez",
         nullptr,
         "DAİRE",
         {pt(0, 0)},
         {3'217, 7'001},
         false,
         "DAİRE merkez=0,0 cevre=3.217,7.001"},
        {"DAİRE 2n",
         nullptr,
         "DAİRE yontem=2n",
         {pt(-4'000, 1'000)},
         {6'000, 3'000},
         false,
         "DAİRE yontem=2n birinci=-4,1 ikinci=6,3"},
        {"DAİRE 3n",
         nullptr,
         "DAİRE yontem=3n",
         {pt(0, 0), pt(10'000, 0)},
         {4'000, 7'000},
         false,
         "DAİRE yontem=3n birinci=0,0 ikinci=10,0 ucuncu=4,7"},
        {"DAİRE ttr",
         nullptr,
         "DAİRE yontem=ttr",
         {pt(0, 0), pt(20'000, 0), pt(0, -5'000), pt(0, 15'000), Value::number(3.0)},
         {6'000, 5'000},
         false,
         "DAİRE yontem=ttr birinci=0,0 ikinci=20,0 ucuncu=0,-5 dorduncu=0,15 yaricap=3 yon=6,5"},
        {"YAY merkez",
         nullptr,
         "YAY",
         {pt(0, 0), pt(10'000, 0)},
         {-3'000, 8'000},
         false,
         "YAY merkez=0,0 baslangic=10,0 bitis=-3,8"},
        {"YAY 3n",
         nullptr,
         "YAY yontem=3n",
         {pt(0, 0), pt(5'000, 4'000)},
         {12'000, 1'000},
         false,
         "YAY yontem=3n baslangic=0,0 uzerinden=5,4 bitis=12,1"},
        {"YAY bma, süpürme gösterilerek",
         nullptr,
         "YAY yontem=bma",
         {pt(0, 0), pt(0, 10'000)},
         {10'000, 0},
         false,
         "YAY yontem=bma merkez=0,0 baslangic=0,10 supurme=100"},
        {"YAY bby",
         nullptr,
         "YAY yontem=bby",
         {pt(0, 0), pt(10'000, 0), Value::number(8.0)},
         {5'000, 3'000},
         false,
         "YAY yontem=bby baslangic=0,0 bitis=10,0 yaricap=8 yon=sag"},
        {"YAY devam",
         "ÇİZGİ 0,0 10,0",
         "YAY yontem=devam",
         {},
         {18'000, 6'000},
         false,
         "YAY yontem=devam bitis=18,6"},
        {"ELİPS merkez",
         nullptr,
         "ELİPS",
         {pt(0, 0), pt(10'000, 0)},
         {2'000, 4'000},
         false,
         "ELİPS merkez=0,0 birinci=10,0 ikinci=2,4"},
        {"ELİPS eksen",
         nullptr,
         "ELİPS yontem=eksen",
         {pt(-10'000, 0), pt(10'000, 0)},
         {0, 5'000},
         false,
         "ELİPS yontem=eksen birinci=-10,0 ikinci_uc=10,0 ikinci=0,5"},
        {"ELİPS kısmi",
         nullptr,
         "ELİPS baslangic=0 bitis=120",
         {pt(0, 0), pt(10'000, 0)},
         {0, 4'000},
         false,
         "ELİPS baslangic=0 bitis=120 merkez=0,0 birinci=10,0 ikinci=0,4"},
        {"DİKDÖRTGEN 2n",
         nullptr,
         "DİKDÖRTGEN",
         {pt(0, 0)},
         {8'000, 5'000},
         false,
         "DİKDÖRTGEN 0,0 8,5"},
        {"DİKDÖRTGEN 2n, sağ alta",
         nullptr,
         "DİKDÖRTGEN",
         {pt(0, 0)},
         {8'000, -5'000},
         false,
         "DİKDÖRTGEN 0,0 8,-5"},
        {"DİKDÖRTGEN 3n",
         nullptr,
         "DİKDÖRTGEN yontem=3n",
         {pt(0, 0), pt(8'000, 6'000)},
         {-3'000, 4'000},
         false,
         "DİKDÖRTGEN yontem=3n noktalar=0,0 8,6 -3,4"},
        {"ÇOKGEN ic",
         nullptr,
         "ÇOKGEN",
         {Value::integer(6), pt(0, 0)},
         {0, 10'000},
         false,
         "ÇOKGEN kenar_sayisi=6 merkez=0,0 yaricap=10 aci=0"},
        {"ÇOKGEN dis",
         nullptr,
         "ÇOKGEN yontem=dis",
         {Value::integer(4), pt(0, 0)},
         {3'000, 10'000},
         false,
         nullptr},
        {"ÇOKGEN kenar",
         nullptr,
         "ÇOKGEN yontem=kenar",
         {Value::integer(6), pt(0, 0), Value::number(10.0)},
         {0, 5'000},
         false,
         "ÇOKGEN yontem=kenar kenar_sayisi=6 merkez=0,0 kenar_uzunlugu=10 aci=0"},
        {"ÇOKGEN, aci verilmiş",
         nullptr,
         "ÇOKGEN aci=25",
         {Value::integer(5), pt(0, 0)},
         {7'000, 7'000},
         false,
         nullptr},
        {"SPLINE",
         nullptr,
         "SPLINE",
         {pt(0, 0), pt(10'000, 10'000)},
         {20'000, 0},
         true,
         "SPLINE 0,0 10,10 20,0"},
        {"ÇOKLUÇİZGİ",
         nullptr,
         "ÇOKLUÇİZGİ",
         {pt(0, 0), pt(10'000, 0)},
         {10'000, 7'000},
         true,
         "ÇOKLUÇİZGİ 0,0 10,0 10,7"},
        {"ALAN",
         nullptr,
         "ALAN",
         {pt(0, 0), pt(10'000, 0), pt(10'000, 7'000)},
         {0, 7'000},
         true,
         "ALAN 0,0 10,0 10,7 0,7"},
    };
}

/// Drives `m` the way the canvas does, reading the ghost at its last prompt,
/// and returns it. The rig's document then holds what the click made.
std::vector<GhostRun> run_by_hand(Rig& r, const Method& m)
{
    if (m.setup != nullptr) REQUIRE(r.bus.execute_line(m.setup, Origin::Test).ok());
    auto started = r.bus.begin_interactive(m.start, Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    for (const Value& v : m.before)
        REQUIRE(s.supply(v).ok());
    REQUIRE(s.waiting());
    const Prompt asked = s.prompt();
    REQUIRE(asked.has_rubber_band);
    std::vector<GhostRun> ghost =
        ghost_outline(asked, aimed(r, asked, m.at), r.bus.angle_convention());
    REQUIRE(s.supply(Value::point(m.at)).ok());
    if (m.run_of_points) REQUIRE(s.supply(Value{}).ok());
    REQUIRE(s.finished());
    REQUIRE(r.bus.finish(s).ok());
    return ghost;
}

/// The drawn runs of what `m` made, the setup's own object left out.
std::vector<GhostRun> made_by(const Rig& r, const Method& m)
{
    std::vector<GhostRun> all = drawn(r.doc);
    if (m.setup != nullptr && !all.empty()) all.erase(all.begin());
    return all;
}

} // namespace

TEST_CASE("C-02: her çizim yönteminde hayalet, tıklamanın yazdığı nesnedir")
{
    // HEADLESS AND WITH A VIEW: with no view the aids are inert and the point
    // is exact; at one millimetre a pixel the snap is live — and the ghost is
    // read where the snap preview would put it, so the two still agree.
    for (const double scale : {0.0, 1.0})
        for (const Method& m : methods()) {
            INFO(m.name << " (görünüm " << scale << " mm/piksel)");
            Rig r;
            if (scale > 0.0) r.bus.aids().set_view_scale(scale);
            const std::vector<GhostRun> ghost = run_by_hand(r, m);
            const std::vector<GhostRun> made  = made_by(r, m);
            REQUIRE_EQ(ghost.size(), std::size_t{1});
            REQUIRE_EQ(made.size(), std::size_t{1});
            CHECK_MESSAGE(same_shape(ghost.front(), made.front()),
                          "hayalet " << ghost.front().points.size() << " köşe, nesne "
                                     << made.front().points.size() << " köşe");
        }
}

TEST_CASE("C-02: her çizim yöntemi işaretlenince, yazılınca ve oynatılınca aynı nesneyi verir")
{
    for (const Method& m : methods()) {
        INFO(m.name);
        Rig pointed;
        (void)run_by_hand(pointed, m);

        // THE JOURNAL LINE IS THE TYPED FORM OF THE GESTURE: replayed through
        // the bus it draws the same document, keys and all (Article 6.4).
        Rig replay;
        for (const auto& e : pointed.journal.entries())
            REQUIRE(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
        CHECK_EQ(replay.doc.content_hash(), pointed.doc.content_hash());

        // AND THE LINE A PERSON WOULD TYPE, where its numbers are the ones the
        // hand gave. ÇOKGEN sized by a pointed flat or rotated by a given `aci`
        // derives a measurement no one types to the last bit, so those two are
        // proved by the replay alone.
        if (m.typed == nullptr) continue;
        Rig typed;
        if (m.setup != nullptr) REQUIRE(typed.bus.execute_line(m.setup, Origin::Test).ok());
        auto line = typed.bus.execute_line(m.typed, Origin::CommandLine);
        REQUIRE_MESSAGE(line.ok(), (line.ok() ? std::string() : line.error().message));
        CHECK_EQ(typed.doc.content_hash(), pointed.doc.content_hash());
    }
}

TEST_CASE("C-02: ÇİZGİ'nin hayaleti çalışmanın bütün parçalarıdır")
{
    Rig r;
    auto started = r.bus.begin_interactive("ÇİZGİ", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(kA)).ok());
    REQUIRE(s.supply(Value::point(kB)).ok());
    const std::vector<GhostRun> ghost = ghost_outline(s.prompt(), kC, r.bus.angle_convention());
    REQUIRE(s.supply(Value::point(kC)).ok());
    REQUIRE(s.supply(Value{}).ok());
    REQUIRE(r.bus.finish(s).ok());

    // Two objects, one ghost: the run the ghost draws is the segments laid end
    // to end, which is the one thing that tells ÇİZGİ from ÇOKLUÇİZGİ afterwards.
    const std::vector<GhostRun> made = drawn(r.doc);
    REQUIRE_EQ(made.size(), std::size_t{2});
    REQUIRE_EQ(ghost.size(), std::size_t{1});
    CHECK(ghost.front().points == std::vector<core::Point2>{kA, kB, kC});
    CHECK(made[0].points == std::vector<core::Point2>{kA, kB});
    CHECK(made[1].points == std::vector<core::Point2>{kB, kC});
}

TEST_CASE("C-02: ÇOKGEN aci verilince hayalet de o açıda durur, imlece dönmez")
{
    // THE DRIFT this file's ghost fixed: with `aci` given the canvas turned the
    // polygon towards the cursor while the click drew it at `aci`.
    Rig r;
    auto started = r.bus.begin_interactive("ÇOKGEN aci=25", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::integer(5)).ok());
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    const auto guide = core::decode_polygon_guide(s.prompt().rubber_payload);
    REQUIRE(guide.has_value());
    CHECK(guide->angle_given);
    CHECK_EQ(guide->angle_udeg, core::udeg_from_angle(25.0, r.bus.angle_convention().unit));

    // Two cursors in two directions, one rotation: only the size follows them.
    const auto east  = ghost_outline(s.prompt(), {10'000, 0}, r.bus.angle_convention());
    const auto north = ghost_outline(s.prompt(), {0, 10'000}, r.bus.angle_convention());
    REQUIRE_EQ(east.size(), std::size_t{1});
    REQUIRE_EQ(north.size(), std::size_t{1});
    CHECK(east.front().points == north.front().points);
    s.cancel();
    (void)r.bus.finish(s);
}

TEST_CASE("C-02: YAY bma'nın süpürmesi gösterilebilir; tıklama açıyı verir")
{
    Rig r;
    auto started = r.bus.begin_interactive("YAY yontem=bma", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(s.supply(Value::point(core::Point2{0, 10'000})).ok());
    REQUIRE(s.waiting());
    CHECK(s.prompt().kind == ParamKind::Number);
    CHECK(s.prompt().pick_sweep);
    CHECK(s.prompt().rubber_shape == RubberShape::ArcSweep);

    // A quarter turn clockwise from north under the default semt + grad: the
    // click east of the centre answers 100 grad, and that is what is recorded.
    REQUIRE(s.supply(Value::point(core::Point2{10'000, 0})).ok());
    REQUIRE(r.bus.finish(s).ok());
    REQUIRE_EQ(r.journal.size(), std::size_t{1});
    const Value* sweep = r.journal.entries().back().args.find("supurme");
    REQUIRE(sweep != nullptr);
    CHECK_EQ(sweep->as_number(), 100.0);

    // And the number is still a number: typed, it gives the same arc.
    Rig typed;
    REQUIRE(typed.bus
                .execute_line("YAY yontem=bma merkez=0,0 baslangic=0,10 supurme=100",
                              Origin::CommandLine)
                .ok());
    CHECK_EQ(typed.doc.content_hash(), r.doc.content_hash());
}

TEST_CASE("C-02: DAİRE ttr ve YAY bby'de sabitlenen çizgiler istem boyunca görünür")
{
    // Every prompt after the first line shows it: the second line's first
    // point, its second point, and the radius — the prompts that used to show
    // nothing of what the circle would be tangent to.
    Rig r;
    auto started = r.bus.begin_interactive("DAİRE yontem=ttr", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 0})).ok());
    CHECK(s.prompt().has_rubber_band);
    CHECK(s.prompt().rubber_shape == RubberShape::Fixed);
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{2});
    REQUIRE(s.supply(Value::point(core::Point2{0, -5'000})).ok());
    CHECK(s.prompt().rubber_shape == RubberShape::CircleBuild);
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{3});
    REQUIRE(s.supply(Value::point(core::Point2{0, 15'000})).ok());
    CHECK(s.prompt().kind == ParamKind::Number);
    CHECK(s.prompt().has_rubber_band);
    CHECK_EQ(s.prompt().rubber_chain.size(), std::size_t{4});
    // No circle yet — there is no radius — so the ghost is empty, not a dot.
    CHECK(ghost_outline(s.prompt(), {6'000, 5'000}, r.bus.angle_convention()).empty());
    s.cancel();
    (void)r.bus.finish(s);

    Rig b;
    auto arc = b.bus.begin_interactive("YAY yontem=bby", Origin::Gui);
    REQUIRE(arc.ok());
    Session& a = *arc.value();
    REQUIRE(a.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(a.supply(Value::point(core::Point2{10'000, 0})).ok());
    CHECK(a.prompt().kind == ParamKind::Number);
    CHECK(a.prompt().rubber_shape == RubberShape::Fixed);
    CHECK(a.prompt().rubber_chain == std::vector<core::Point2>{{0, 0}, {10'000, 0}});
    a.cancel();
    (void)b.bus.finish(a);
}

// =============================================================================
// A typed tool comes back with its method
// =============================================================================

TEST_CASE("C-02: yazılan bir aracın yöntemi tekrarda korunur, noktaları korunmaz")
{
    Rig r;
    CHECK(rearm_line(r.reg, "DAİRE yontem=3n birinci=0,0") == "DAİRE yontem=3n");
    CHECK(rearm_line(r.reg, "DR yontem=3n") == "DAİRE yontem=3n"); ///< the primary name
    CHECK(rearm_line(r.reg, "ÇOKGEN kenar_sayisi=6 yontem=dis merkez=0,0") ==
          "ÇOKGEN kenar_sayisi=6 yontem=dis");
    CHECK(rearm_line(r.reg, "YAY yontem=bby yon=sag yaricap=8 baslangic=0,0") ==
          "YAY yontem=bby yon=sag yaricap=8");
    CHECK(rearm_line(r.reg, "SPLINE derece=2 kapali=evet 0,0 10,10") ==
          "SPLINE derece=2 kapali=evet");
    CHECK(rearm_line(r.reg, "ÇİZGİ 0,0 10,0") == "ÇİZGİ");
    CHECK(rearm_line(r.reg, "yokboyle yontem=3n").empty());

    // AND THE LINE IS THE METHOD: started again, it asks what the method asks
    // — three rim points, not a centre.
    auto again = r.bus.begin_interactive(rearm_line(r.reg, "DAİRE yontem=3n birinci=0,0"));
    REQUIRE(again.ok());
    REQUIRE(again.value()->waiting());
    CHECK(again.value()->prompt().message == "Çember üzerinde birinci nokta");
    again.value()->cancel();
    (void)r.bus.finish(*again.value());
}
