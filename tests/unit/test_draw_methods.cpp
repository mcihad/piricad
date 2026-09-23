// SPDX-License-Identifier: GPL-3.0-or-later
//
// DRAWING METHODS, AS A HAND USES THEM (TODOS C-02).
//
// A drawing tool is finished when a wrong corner costs that corner and not the
// drawing, when the ghost under the cursor is the object the click makes, and
// when every method gives the same object whether it is pointed at or typed.
// This file pins those, by driving each command the way the canvas does.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/spline.hpp"

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
