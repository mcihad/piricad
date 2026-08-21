// SPDX-License-Identifier: GPL-3.0-or-later
#include "microtest.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"

using namespace piricad;
using namespace piricad::command;

namespace {

struct Fixture
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Fixture() { register_builtin_commands(reg); }
};

} // namespace

TEST_CASE("registry resolves turkish names, english names and abbreviations")
{
    Fixture f;

    CHECK(f.reg.resolve("ÇİZGİ") != nullptr);
    CHECK(f.reg.resolve("çizgi") != nullptr);
    CHECK(f.reg.resolve("LINE") != nullptr);
    CHECK(f.reg.resolve("l") != nullptr);
    CHECK(f.reg.resolve("core.line") != nullptr);
    CHECK_EQ(f.reg.resolve("Ç")->id, std::string("core.line"));
    CHECK(f.reg.resolve("YOKBÖYLEKOMUT") == nullptr);
}

TEST_CASE("registry refuses duplicate ids and shadowed names")
{
    Registry r;
    register_builtin_commands(r);

    CommandSpec clash;
    clash.id      = "test.clash";
    clash.names   = {"LINE"};
    clash.summary = "clash";
    clash.run     = r.by_id("core.line")->run;

    auto st = r.add(clash);
    CHECK(!st.ok());
}

TEST_CASE("ai tool schema is generated from the registry, not hand written")
{
    Fixture f;
    const core::Json schema = f.reg.ai_tool_schema();

    const core::Json* tools = schema.find("tools");
    CHECK(tools != nullptr);
    CHECK(tools->is_array());

    std::size_t flagged = 0;
    for (const auto& spec : f.reg.all())
        if (has_flag(spec.flags, Flags::AiAccessible)) ++flagged;

    CHECK_EQ(tools->as_array().size(), flagged);
    CHECK(flagged > 0);
}

TEST_CASE("parser handles absolute, relative, polar and inline expressions")
{
    auto parsed = parse_line("ÇİZGİ 485320.150,4310220.400 @50,30 @100<45 @(100*3),0");
    CHECK(parsed.ok());
    CHECK_EQ(parsed.value().command, std::string("ÇİZGİ"));
    CHECK_EQ(parsed.value().tokens.size(), std::size_t{4});

    const auto& t = parsed.value().tokens;
    CHECK(t[0].kind == Token::Kind::Absolute);
    CHECK(t[1].kind == Token::Kind::Relative);
    CHECK(t[2].kind == Token::Kind::Polar);
    CHECK(t[3].kind == Token::Kind::Relative);
    CHECK_EQ(t[3].a, 300.0);

    auto p0 = resolve_point(t[0], core::Point2{});
    CHECK(p0.ok());
    CHECK_EQ(p0.value(), (core::Point2{485320150, 4310220400}));

    auto p1 = resolve_point(t[1], p0.value());
    CHECK(p1.ok());
    CHECK_EQ(p1.value(), (core::Point2{485370150, 4310250400}));
}

TEST_CASE("expression evaluator respects precedence and reports errors")
{
    CHECK_EQ(evaluate_expression("100*3").value(), 300.0);
    CHECK_EQ(evaluate_expression("2+3*4").value(), 14.0);
    CHECK_EQ(evaluate_expression("(2+3)*4").value(), 20.0);
    CHECK_EQ(evaluate_expression("2^3^2").value(), 512.0); // right associative
    CHECK_EQ(evaluate_expression("-5+2").value(), -3.0);
    CHECK(!evaluate_expression("1/0").ok());
    CHECK(!evaluate_expression("(1+2").ok());
    CHECK(!evaluate_expression("abc").ok());
}

TEST_CASE("keyword arguments bind out of order and reject unknown names")
{
    Fixture f;

    auto ok = f.bus.execute_line("KATMAN ad=PARSEL gorunur=evet", Origin::Test);
    CHECK(ok.ok());
    CHECK(f.doc.find_layer("PARSEL") != core::kNoLayer);

    auto bad = f.bus.execute_line("KATMAN ad=YOL yokboyle=1", Origin::Test);
    CHECK(!bad.ok());
    CHECK(bad.error().message.find("yokboyle") != std::string::npos);
}

TEST_CASE("a quoted value survives after a keyword")
{
    // `ad="YOL KENARI"` opens its quote mid-token; the tokeniser must absorb the
    // quoted run into the same token instead of splitting on the inner space.
    Fixture f;

    CHECK(f.bus.execute_line("KATMAN ad=\"YOL KENARI\"", Origin::Test).ok());
    CHECK(f.doc.find_layer("YOL KENARI") != core::kNoLayer);

    auto parsed = parse_line("KATMAN ad=\"YOL KENARI\" gorunur=evet");
    CHECK(parsed.ok());
    if (parsed.ok()) CHECK_EQ(parsed.value().tokens.size(), std::size_t{2});

    CHECK(!f.bus.execute_line("KATMAN ad=\"kapanmamış", Origin::Test).ok());
}

TEST_CASE("validation rejects a polyline with fewer than two points")
{
    Fixture f;

    // Pre-dispatch validation runs on the bus for every client, so a script or an
    // AI passing one point is refused before the command body ever starts (§2.6).
    auto bad = f.bus.execute_line("ÇİZGİ 100,100", Origin::Test);
    CHECK(!bad.ok());
    if (!bad.ok()) {
        CHECK(bad.error().message.find("noktalar") != std::string::npos);
        CHECK(bad.error().message.find("en az 2") != std::string::npos);
    }
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("an interactive run that is cancelled at the first point is a clean no-op")
{
    Fixture f;

    auto started = f.bus.begin_interactive("ÇİZGİ");
    CHECK(started.ok());
    if (!started.ok()) return;

    auto& session = *started.value();
    CHECK(session.waiting());
    session.cancel(); // ESC before the first click

    auto done = f.bus.finish(session);
    CHECK(done.ok());
    if (done.ok()) CHECK(!done.value().mutated);

    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
    CHECK_EQ(f.journal.size(), std::size_t{0}); // nothing happened, nothing logged
}

TEST_CASE("error messages name what was expected and what arrived")
{
    Fixture f;

    auto bad = f.bus.execute_line("YOKBÖYLE 1,2", Origin::Test);
    CHECK(!bad.ok());
    CHECK(bad.error().message.find("YOKBÖYLE") != std::string::npos);
    CHECK(bad.error().message.find("YARDIM") != std::string::npos);
}

TEST_CASE("undo and redo walk the whole transaction")
{
    Fixture f;

    CHECK(f.bus.execute_line("ÇİZGİ 0,0 10,0 10,10", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2}); // two segments, one command
    const std::uint64_t drawn = f.doc.content_hash();

    CHECK(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});

    CHECK(f.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), drawn);
}

TEST_CASE("one command is one undo step")
{
    Fixture f;

    CHECK(f.bus.execute_line("ÇİZGİ 0,0 10,0 20,0 30,0", Origin::Test).ok());
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{3});

    CHECK(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("a batch collapses many commands into a single undo step")
{
    Fixture f;

    CHECK(f.bus.begin_batch("Toplu çizim").ok());
    for (int i = 0; i < 5; ++i) {
        const std::string line = "ÇİZGİ " + std::to_string(i) + ",0 " + std::to_string(i) + ",10";
        CHECK(f.bus.execute_line(line, Origin::Script).ok());
    }
    auto closed = f.bus.end_batch();
    CHECK(closed.ok());

    CHECK_EQ(f.doc.live_entity_count(), std::size_t{5});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});

    CHECK(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("erase is undoable and reports a missing entity")
{
    Fixture f;

    CHECK(f.bus.execute_line("ÇİZGİ 0,0 10,10", Origin::Test).ok());

    // `nesneler` carries persistent KEYS (model.md R5/P4), and keys start at 1
    // because EntityKey{0} is "none". Slot 0 of a fresh document is key 1.
    CHECK(f.bus.execute_line("SİL nesneler=1", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});

    CHECK(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});

    auto missing = f.bus.execute_line("SİL nesneler=99", Origin::Test);
    CHECK(missing.ok()); // reported to the transcript, nothing applied
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("a failing rule rolls the whole transaction back")
{
    struct RefuseRule final : Rule
    {
        std::string name() const override { return "test.refuse"; }

        core::Status check(const ValidationRequest& req) const override
        {
            if (req.spec.id == "core.line")
                return core::err(core::ErrorCode::ValidationFailed, "kural gereği reddedildi");
            return core::ok();
        }
    };

    Fixture f;
    f.bus.validator().add_rule(std::make_shared<RefuseRule>());

    auto blocked = f.bus.execute_line("ÇİZGİ 0,0 10,10 20,20", Origin::Test);
    CHECK(!blocked.ok());
    CHECK(blocked.error().message.find("test.refuse") != std::string::npos);

    // No partial application, ever (piricad.md §2.5).
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
}

TEST_CASE("read-only commands never become an undo step")
{
    Fixture f;

    CHECK(f.bus.execute_line("YARDIM", Origin::Test).ok());
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
    CHECK(f.bus.execute_line("YAKINLAŞ KAPSAM", Origin::Test).ok());
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
}

TEST_CASE("user-facing error messages are Turkish")
{
    // piricad.md §3 and §13: the users are Turkish surveying engineers, so an
    // error they can hit must be Turkish and actionable. A message that leaks an
    // English phrase from the implementation is a defect, and the user manual
    // quotes these strings verbatim (.claude/docs.md R11).
    Fixture f;

    const char* english[] = {"expects",  "is missing", "unknown parameter",
                             "at least", "at most",    "Unknown",
                             "Expected", "Empty",      "Duplicate"};

    const char* lines[] = {
        "YOKBÖYLEKOMUT",            // bilinmeyen komut
        "ÇİZGİ 100,100",            // yetersiz nokta
        "ÇİZGİ",                    // eksik zorunlu parametre
        "KATMAN ad=YOL yokboyle=1", // bilinmeyen parametre
        "ÇİZGİ abc",                // yanlış tip
        "ÇİZGİ @1,2,3",             // ayrıştırma hatası
        "YAKINLAŞ mod=OLMAYAN",     // geçersiz mod (transkripte gider)
    };

    for (const char* line : lines) {
        auto r = f.bus.execute_line(line, Origin::Test);
        if (r.ok()) continue;
        for (const char* word : english) {
            if (r.error().message.find(word) != std::string::npos) {
                ::microtest::report(__FILE__, __LINE__, line,
                                    "İngilizce sızıntı: \"" + r.error().message + "\"");
            }
        }
    }
}
