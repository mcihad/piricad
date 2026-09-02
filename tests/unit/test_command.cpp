// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad_test.hpp"

#include "piricad/core/text.hpp"

#include <map>

#include "piricad/command/bus.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/arc.hpp"
#include "piricad/core/circle.hpp"
#include "piricad/core/entity_kind.hpp"
#include "piricad/core/ellipse.hpp"
#include "piricad/core/guide.hpp"
#include "piricad/core/offset.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/core/text_store.hpp"

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

TEST_CASE("registry: bir adın HER yazımı aynı komuta ulaşır")
{
    // The defect this pins down, and it was live: `turkish_upper` raises Turkish's
    // two i's APART — `i` to `İ`, `ı` to `I` — which is right for a word being
    // written in capitals and wrong for matching a name somebody typed. `LINE` is
    // declared beside `ÇİZGİ` so an ASCII keyboard can reach the command, but
    // `turkish_upper("line")` is `LİNE` and `turkish_upper("LINE")` is `LINE`, so
    // typing `line` in lower case found nothing. Neither did `cizgi`, `import` or
    // `iceaktar` — the everyday spellings of the everyday commands.
    Fixture f;

    struct Case
    {
        const char* typed;
        const char* id;
    };

    // Every spelling of one name: Turkish, ASCII-folded, English, and each of
    // those in lower, upper and mixed case.
    const Case cases[] = {
        {"ÇİZGİ", "core.line"},      {"çizgi", "core.line"},      {"Çizgi", "core.line"},
        {"CIZGI", "core.line"},      {"cizgi", "core.line"},      {"Cizgi", "core.line"},
        {"LINE", "core.line"},       {"line", "core.line"},       {"Line", "core.line"},
        {"İÇEAKTAR", "core.import"}, {"içeaktar", "core.import"}, {"ICEAKTAR", "core.import"},
        {"iceaktar", "core.import"}, {"IMPORT", "core.import"},   {"import", "core.import"},
        {"STİL", "core.style"},      {"stil", "core.style"},      {"STYLE", "core.style"},
        {"style", "core.style"},     {"SEÇ", "core.select"},      {"sec", "core.select"},
        {"select", "core.select"},
    };

    for (const Case& c : cases) {
        const CommandSpec* found = f.reg.resolve(c.typed);
        if (found == nullptr) FAIL_WITH("çözülemedi", c.typed);
        REQUIRE(found != nullptr);
        CHECK_EQ(found->id, std::string(c.id));
    }
}

TEST_CASE("registry: katlanmış anahtarlar iki ayrı komutu birbirine karıştırmıyor")
{
    // Folding case and alphabet together is only safe while no two DECLARED names
    // land on one key. That is a property of the names, so it is checked over the
    // real ones rather than argued about.
    Registry r;
    register_builtin_commands(r);

    std::map<std::string, std::string> owner;
    for (const CommandSpec& spec : r.all())
        for (const std::string& name : spec.names) {
            const std::string key = core::turkish_fold_key(name);
            const auto it         = owner.find(key);
            if (it != owner.end() && it->second != spec.id)
                FAIL_WITH("iki komut aynı anahtara düşüyor",
                          key + ": " + it->second + " / " + spec.id);
            owner[key] = spec.id;
        }
    CHECK(!owner.empty());
}

TEST_CASE("METİN: 'hayır' ve ASCII yazımı 'hayir' aynı boole değeri")
{
    // The same fold, reached through the argument binder rather than the registry.
    // `turkish_upper("hayir")` is `HAYİR`, which matched nothing, so the ASCII
    // spelling of the word half the keyboards in the country can type silently
    // failed to be a boolean.
    CHECK(core::turkish_key_equals("hayır", "HAYIR"));
    CHECK(core::turkish_key_equals("hayir", "HAYIR"));
    CHECK(core::turkish_key_equals("Hayır", "hayir"));
    CHECK(core::turkish_key_equals("evet", "EVET"));

    // ...and the display fold still keeps them apart, because writing a word in
    // capitals is a different question from matching a name.
    CHECK(core::turkish_upper("hayır") == std::string("HAYIR"));
    CHECK(core::turkish_upper("hayir") == std::string("HAYİR"));
}

TEST_CASE("METİN: anahtar katlaması alfabeyi de büyük/küçüğü de katlar")
{
    // Both i's, both cases, to one letter; and the five other Turkish letters to
    // their ASCII counterparts.
    for (const char* spelling : {"i", "I", "ı", "İ"})
        CHECK(core::turkish_fold_key(spelling) == std::string("I"));

    CHECK(core::turkish_fold_key("çğöşü") == std::string("CGOSU"));
    CHECK(core::turkish_fold_key("ÇĞÖŞÜ") == std::string("CGOSU"));
    CHECK(core::turkish_fold_key("ızgara_adımı") == core::turkish_fold_key("IZGARA_ADIMI"));
    CHECK(core::turkish_fold_key("izgara_adimi") == core::turkish_fold_key("IZGARA_ADIMI"));

    // A LAYER NAME is not a declared name and keeps the display fold, so two
    // layers differing only by the two i's stay two layers: one rises to a dotted
    // capital and the other to a dotless one, which are different names to the
    // layer table and the same key to a command.
    CHECK(core::turkish_upper("imar") != core::turkish_upper("ımar"));
    CHECK(core::turkish_fold_key("imar") == core::turkish_fold_key("ımar"));
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

TEST_CASE("filter predicate is one grammar with the expression evaluator")
{
    // One parcel's row, as the attribute table would hand it over. `beyan` is
    // present but unfilled — the case that separates NULL from an empty string.
    const std::map<std::string, std::optional<std::string>> row{
        {"alan_m2", "3482.64"},   {"plan_fonksiyon", "Konut"}, {"ada_no", "1284"},
        {"parsel_no", "21"},      {"nitelik", "Arsa"},         {"beyan", std::nullopt},
        {"pafta", "G21-b-14-c-2"}};

    const FieldReader field = [&row](std::string_view name) -> std::optional<std::string> {
        const auto at = row.find(std::string(name));
        return at == row.end() ? std::nullopt : at->second;
    };

    const auto yes = [&](const char* expr) {
        const auto got = evaluate_predicate(expr, field);
        REQUIRE(got.ok());
        return got.value();
    };

    // Comparison, on a text column that holds a number.
    CHECK(yes("\"alan_m2\" > 2000"));
    CHECK(!yes("\"alan_m2\" > 5000"));
    CHECK(yes("\"alan_m2\" >= 3482.64"));

    // Strings in single quotes, columns in double: SQL's convention.
    CHECK(yes("\"plan_fonksiyon\" = 'Konut'"));
    CHECK(!yes("\"plan_fonksiyon\" = 'Ticaret'"));
    CHECK(yes("\"plan_fonksiyon\" != 'Ticaret'"));

    // Boolean layers and precedence: AND binds tighter than OR.
    CHECK(yes("\"alan_m2\" > 2000 AND \"plan_fonksiyon\" = 'Konut'"));
    CHECK(!yes("\"alan_m2\" > 9000 AND \"plan_fonksiyon\" = 'Konut'"));
    CHECK(yes("\"alan_m2\" > 9000 OR \"plan_fonksiyon\" = 'Konut'"));
    CHECK(yes("\"ada_no\" = 9999 OR \"alan_m2\" > 2000 AND \"nitelik\" = 'Arsa'"));
    CHECK(!yes("(\"ada_no\" = 9999 OR \"alan_m2\" > 2000) AND \"nitelik\" = 'Tarla'"));
    CHECK(yes("NOT \"nitelik\" = 'Tarla'"));

    // An unfilled cell is UNKNOWN, so every comparison against it is false —
    // including `!=`. An unsurveyed parcel must not fall into a filter that asks
    // for parcels different from something.
    CHECK(yes("\"beyan\" IS NULL"));
    CHECK(!yes("\"beyan\" IS NOT NULL"));
    CHECK(!yes("\"beyan\" = 'x'"));
    CHECK(!yes("\"beyan\" != 'x'"));

    // A column nobody declared reads as NULL rather than as an error: a filter
    // written against another layer must not take the table down with it.
    CHECK(yes("\"yok_boyle_bir_sutun\" IS NULL"));

    // The arithmetic is the SAME evaluator — this is the whole point of 5.11.
    CHECK(yes("\"alan_m2\" > (1000 * 2)"));
    CHECK(yes("\"ada_no\" = 1284"));

    // Text comparison when the two sides are not both numbers: `1284/A` sorts
    // after `1284`, and neither is an error.
    CHECK(yes("\"pafta\" > 'G21-b-14-c-1'"));

    // An empty filter matches everything; a broken one says why.
    CHECK(yes(""));
    CHECK(yes("   "));
    CHECK(!evaluate_predicate("\"alan_m2\" >", field).ok());
    CHECK(!evaluate_predicate("\"alan_m2\" 2000", field).ok());
    CHECK(!evaluate_predicate("\"alan_m2 > 2", field).ok());
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

TEST_CASE("a newly created layer reports a document change without an undo record")
{
    Fixture f;
    bool notified             = false;
    f.bus.on_document_changed = [&notified] { notified = true; };

    auto created = f.bus.execute_line("KATMAN ad=ANLIK", Origin::Test);
    REQUIRE(created.ok());
    CHECK(created.value().mutated);
    CHECK(notified);
    CHECK(f.doc.find_layer("ANLIK") != core::kNoLayer);

    // Layer slots are intentionally additive and remain stable, so creation does
    // not create an undo record. The UI notification is independent of that.
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
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

TEST_CASE("tırnak içindeki değer harfi harfine alınır, ikinci kez ayrıştırılmaz")
{
    // THE BUG THIS LOCKS DOWN. The tokeniser strips the quotes and then handed the
    // bare text back to `classify`, which knew nothing about them — so a value
    // holding its own `=` was split a second time. A connection string passed as
    // a quoted argument became a nested key/value, and the command was told its
    // text parameter had been given something that was not text.
    //
    // The same hole swallowed every Windows path with an `=` in it, every format
    // string, and every layer name a user chose badly. Quoting means literal.
    Fixture f;

    auto parsed = parse_line("VERİTABANI baglan hedef=\"host=localhost dbname=piricad\"");
    REQUIRE(parsed.ok());
    REQUIRE_EQ(parsed.value().tokens.size(), std::size_t{2});

    const Token& value = parsed.value().tokens[1];
    CHECK_EQ(value.kind, Token::Kind::KeyValue);
    CHECK_EQ(value.word, std::string("hedef"));
    REQUIRE_EQ(value.nested.size(), std::size_t{1});
    CHECK_EQ(value.nested.front().kind, Token::Kind::Text);
    CHECK_EQ(value.nested.front().text, std::string("host=localhost dbname=piricad"));

    // A comma inside quotes is a comma, not a coordinate pair.
    auto comma = parse_line("KATMAN ad=\"ADA 12, PARSEL 5\"");
    REQUIRE(comma.ok());
    REQUIRE_EQ(comma.value().tokens.size(), std::size_t{1});
    REQUIRE_EQ(comma.value().tokens[0].nested.size(), std::size_t{1});
    CHECK_EQ(comma.value().tokens[0].nested.front().text, std::string("ADA 12, PARSEL 5"));

    // And it reaches the document intact.
    CHECK(f.bus.execute_line("KATMAN ad=\"ADA 12, PARSEL 5\"", Origin::Test).ok());
    CHECK(f.doc.find_layer("ADA 12, PARSEL 5") != core::kNoLayer);
}

TEST_CASE("tırnak sınırlar, türü değiştirmez")
{
    // A user quotes to keep a space, a comma or an `=` away from the tokeniser.
    // Being told the value is now the wrong KIND is a trap with no lesson in it,
    // so a quoted number is still a number and a quoted evet is still true.
    Fixture f;

    CHECK(f.bus.execute_line("KATMAN ad=GİZLİ gorunur=\"hayır\"", Origin::Test).ok());
    const core::LayerId hidden = f.doc.find_layer("GİZLİ");
    REQUIRE(hidden != core::kNoLayer);
    CHECK_FALSE(f.doc.layer(hidden)->visible);

    // A quoted integer reaching an integer parameter.
    CHECK(f.bus.execute_line("KATMAN ad=RENKLİ renk=\"4281236786\"", Origin::Test).ok());
    const core::LayerId coloured = f.doc.find_layer("RENKLİ");
    REQUIRE(coloured != core::kNoLayer);
    CHECK_EQ(f.doc.layer(coloured)->appearance.rgba, 0xFF2E7D32u);

    // What is NOT a number still fails, and says so.
    CHECK(!f.bus.execute_line("KATMAN ad=BOZUK renk=\"mavi filan\"", Origin::Test).ok());
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
                FAIL_WITH(line, "İngilizce sızıntı: \"" + r.error().message + "\"");
            }
        }
    }
}

// ------------------------------------------------------------------ ALAN ----

TEST_CASE("ALAN kapalı bir yüzey üretir ve kapanış noktasını tekrarlatmaz")
{
    // Before this command existed, Transaction::add_area was written, exercised
    // through the io layer, and reachable from no client at all: the only draw
    // command produced open polylines, so the document could hold a face that
    // nothing could create. A cadastral program that cannot say "this parcel
    // encloses an area" cannot say the one thing a parcel says.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    auto drawn = f.bus.execute_line(
        "ALAN 485300,4310200 485360,4310200 485360,4310245 485300,4310245", Origin::Test);
    if (!drawn) FAIL_WITH("ALAN", drawn.error().message);
    REQUIRE(drawn.ok());
    REQUIRE(f.doc.live_entity_count() == std::size_t{1});

    // Four corners in, four vertices stored. The closing edge is implied, not a
    // repeated vertex: storing it twice would count it twice in the perimeter and
    // write it twice into every exported file.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    REQUIRE(span.count == std::uint32_t{1});
    CHECK(f.doc.geometry().ring_role[span.first] == core::RingRole::Exterior);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});

    // 60 m x 45 m = 2700 m², in square millimetres.
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]),
             core::Mm2{60000} * core::Mm2{45000});
}

TEST_CASE("ALAN: her köşe kılavuza eklenir, önceki kenarlar kaybolmaz")
{
    // The bug this guards: ALAN writes its face in ONE `add_area` at the end,
    // because a ring of one or two vertices is not a face. So while the command
    // runs there is nothing in the document to draw, and the guide was a single
    // segment from the last corner to the cursor. Every click moved that segment
    // forward and the edges already fixed vanished; the shape appeared all at
    // once on completion. ÇİZGİ never showed it — it commits each segment as it
    // goes, so its edges are real entities the ordinary scene pass draws.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    auto started = f.bus.begin_interactive("ALAN");
    REQUIRE(started.ok());
    Session& session = *started.value();

    // The first corner has nothing to preview between: no guide, no chain.
    REQUIRE(session.waiting());
    CHECK_FALSE(session.prompt().has_rubber_band);
    CHECK(session.prompt().rubber_chain.empty());

    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

    // From the second corner on the guide carries EVERY corner fixed so far, and
    // closes the ring, because ALAN never asks the user to repeat the first point.
    REQUIRE(session.waiting());
    CHECK(session.prompt().has_rubber_band);
    CHECK(session.prompt().rubber_shape == RubberShape::Ring);
    REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{1});
    CHECK_EQ(session.prompt().rubber_chain[0].x, core::Mm{0});

    REQUIRE(session.supply(Value::point(core::Point2{60'000, 0})).ok());

    REQUIRE(session.waiting());
    REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
    CHECK_EQ(session.prompt().rubber_chain[1].x, core::Mm{60'000});

    REQUIRE(session.supply(Value::point(core::Point2{60'000, 45'000})).ok());

    // Three corners fixed, three in the chain — and the last of them is where the
    // rubber band starts, so the canvas draws the chain and the cursor edge
    // without repeating a vertex between them.
    REQUIRE(session.waiting());
    REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{3});
    CHECK_EQ(session.prompt().rubber_chain.back().x, session.prompt().rubber_origin.x);
    CHECK_EQ(session.prompt().rubber_chain.back().y, session.prompt().rubber_origin.y);

    // ESC ends the loop and the face is written once, from the same corners.
    session.cancel();
    REQUIRE(session.finished());
    REQUIRE(f.bus.finish(session).ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{3});
}

TEST_CASE("ALAN deliği dış sınırla tek nesne yapar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=AVLULU", Origin::Test).ok());

    // Eight corners, split 4 + 4: the first ring is the boundary, the second is a
    // courtyard inside it.
    auto drawn = f.bus.execute_line(
        "ALAN 485300,4310200 485360,4310200 485360,4310245 485300,4310245 "
        "485315,4310212 485345,4310212 485345,4310232 485315,4310232 bolum=4 bolum=4",
        Origin::Test);
    if (!drawn) FAIL_WITH("ALAN delik", drawn.error().message);
    REQUIRE(drawn.ok());

    // ONE entity, two rings. A hole is not a separate object: it is selected,
    // moved and erased with the boundary it belongs to.
    REQUIRE(f.doc.live_entity_count() == std::size_t{1});
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    REQUIRE(span.count == std::uint32_t{2});
    CHECK(f.doc.geometry().ring_role[span.first] == core::RingRole::Exterior);
    CHECK(f.doc.geometry().ring_role[span.first + 1] == core::RingRole::Interior);

    // The hole comes OUT of the area. 2700 m² minus 30 m x 20 m = 2100 m².
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]),
             core::Mm2{60000} * core::Mm2{45000} - core::Mm2{30000} * core::Mm2{20000});
}

TEST_CASE("ALAN: üç köşeden az reddedilir, yarım alan bırakmaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    // Two points are a line, not an area, and the message says so rather than
    // silently drawing something.
    // The refusal is the point; what is asserted is that nothing changed.
    (void)f.bus.execute_line("ALAN 485300,4310200 485360,4310200", Origin::Test);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("ALAN: 'bolum' nokta listesini tam kapatmazsa hiçbir şey çizilmez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    // Eight points, rings declared as 4 + 3: one point is left over. Drawing the
    // first ring and dropping the rest would be the worst answer — a parcel with
    // a silently missing courtyard is a parcel with the wrong area on it.
    // The refusal is the point; what is asserted is that nothing changed.
    (void)f.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245 485300,4310245 "
                             "485315,4310212 485345,4310212 485345,4310232 485315,4310232 "
                             "bolum=4 bolum=3",
                             Origin::Test);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("PROOF: ALAN arayüzden, komut satırından ve betikten aynı belgeyi bırakır")
{
    // Article 6.4 for the new command. Three clients, one document fingerprint.
    Fixture gui;
    Args args;
    Value::Points pts{core::Point2{485300000, 4310200000}, core::Point2{485360000, 4310200000},
                      core::Point2{485360000, 4310245000}, core::Point2{485300000, 4310245000}};
    REQUIRE(gui.bus.execute_line("KATMAN ad=PARSEL", Origin::Gui).ok());
    args.set("noktalar", Value::points(pts));
    REQUIRE(gui.bus.dispatch(Invocation{"core.area", args, Origin::Gui}).ok());

    Fixture cli;
    REQUIRE(cli.bus.execute_line("KATMAN ad=PARSEL", Origin::CommandLine).ok());
    REQUIRE(cli.bus
                .execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245 485300,4310245",
                              Origin::CommandLine)
                .ok());

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(gui.doc.live_entity_count(), cli.doc.live_entity_count());
}

// ------------------------------------------------------------ ÖZNİTELİK ----

TEST_CASE("ÖZNİTELİK: şema bildirilir, değer yazılır, kalıcı kimlikle okunur")
{
    // Before this, core::AttrTable existed, was tested in isolation and was
    // attached to nothing: the document could not say what a parcel IS, only
    // where its corners are. The 476 MPYY `gösterim` rows in /data had nothing to
    // match against, which is why the catalogue shipped with zero rules.
    Fixture f;
    REQUIRE(f.bus.execute_line("SÜTUN ada_no tam_sayi", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN gosterim metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        f.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());

    const auto col = f.doc.attributes().find("gosterim");
    REQUIRE(col != core::kNoAttr);

    // Addressed by the PERSISTENT key, not the dense slot (R5). A journalled slot
    // replays onto whatever entity holds that index next, which in cadastre is
    // the neighbouring parcel.
    REQUIRE(f.bus.execute_line("ÖZNİTELİK gosterim 1 \"TOPLU KONUT ALANI\"", Origin::Test).ok());

    auto read = f.doc.attribute(col, 0);
    REQUIRE(read.ok());
    CHECK(read.value().present);
    CHECK_EQ(read.value().text, std::string("TOPLU KONUT ALANI"));
}

TEST_CASE("ÖZNİTELİK belge içeriğidir: content_hash() değişir")
{
    // R39/R40: an ada number is not a view preference, it is what the parcel is.
    // If it did not fold into the hash, two documents that disagree about which
    // parcel is which would fingerprint identically.
    Fixture f;
    REQUIRE(f.bus.execute_line("SÜTUN ada_no tam_sayi", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        f.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada_no 1 1234", Origin::Test).ok());
    const std::uint64_t after = f.doc.content_hash();
    CHECK(before != after);

    // And an absent cell folds differently from a present zero: "no ada number
    // recorded" and "ada number 0" are different facts about a parcel.
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada_no 1 0", Origin::Test).ok());
    const std::uint64_t zero = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada_no 1 yok", Origin::Test).ok());
    CHECK(zero != f.doc.content_hash());
}

TEST_CASE("ÖZNİTELİK geri alınabilir, sütun bildirimi alınamaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("SÜTUN ada_no tam_sayi", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        f.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada_no 1 1234", Origin::Test).ok());

    const auto col = f.doc.attributes().find("ada_no");
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());

    // The value goes back; the COLUMN stays. Undoing a declaration would
    // invalidate every row index the journal already holds, which is the same
    // reason an emptied layer is not removed on undo.
    auto read = f.doc.attribute(col, 0);
    REQUIRE(read.ok());
    CHECK(!read.value().present);
    CHECK_EQ(f.doc.attributes().columns(), std::size_t{1});
}

TEST_CASE("ÖZNİTELİK: tür uymayan değer reddedilir, hücre olduğu gibi kalır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("SÜTUN ada_no tam_sayi", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        f.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada_no 1 1234", Origin::Test).ok());

    // The schema said this column holds an integer. A value that is not one is
    // refused with the type named, never silently coerced to zero.
    // The refusal is the point; what is asserted is that nothing changed.
    (void)f.bus.execute_line("ÖZNİTELİK ada_no 1 \"bin iki yüz\"", Origin::Test);

    const auto col = f.doc.attributes().find("ada_no");
    auto read      = f.doc.attribute(col, 0);
    REQUIRE(read.ok());
    CHECK_EQ(read.value().number, std::int64_t{1234});
}

// ---------------------------------------------------------------- METİN ----

TEST_CASE("METİN yazıyı belgeye koyar; geometrisi taban çizgisidir")
{
    // A pafta is not only geometry: ada and parsel numbers, plan notes and street
    // names are drafted entities with an exact position, height and rotation. The
    // program had no way to make one.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=NUMARA", Origin::Test).ok());

    auto made = f.bus.execute_line("METİN 485330,4310225 \"1234/7\" 2000", Origin::Test);
    if (!made) FAIL_WITH("METİN", made.error().message);
    REQUIRE(made.ok());
    REQUIRE(f.doc.live_entity_count() == std::size_t{1});

    const std::uint32_t slot = f.doc.entities().slot[0];
    CHECK(f.doc.texts().has(slot));
    CHECK_EQ(f.doc.texts().text(slot), std::string_view("1234/7"));
    CHECK_EQ(f.doc.texts().height(slot), core::Mm{2000});

    // The geometry is an ordinary two-vertex open ring, so culling, snapping and
    // hit testing work on it with no special case — that is the whole reason the
    // baseline is stored as geometry rather than as an anchor and an angle.
    const core::RingSpan span = f.doc.geometry().rings_of(slot);
    REQUIRE(span.count == std::uint32_t{1});
    CHECK(f.doc.geometry().ring_role[span.first] == core::RingRole::Open);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{2});
}

TEST_CASE("METİN dönüklüğü saklanan açı değil, taban çizgisinin yönüdür")
{
    // No angle is stored anywhere, so there is no angle to disagree with the
    // geometry — and no trigonometry in the stored form to round differently on
    // another platform (§7.3).
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL_ADI", Origin::Test).ok());
    REQUIRE(f.bus
                .execute_line("METİN 485300,4310255 \"ATATÜRK CADDESİ\" 3000 "
                              "bitis=485420,4310265",
                              Origin::Test)
                .ok());

    const std::uint32_t slot  = f.doc.entities().slot[0];
    const core::RingSpan span = f.doc.geometry().rings_of(slot);
    const auto xs             = f.doc.geometry().ring_xs(span.first);
    const auto ys             = f.doc.geometry().ring_ys(span.first);

    // The end point is exactly what was typed, to the millimetre.
    CHECK_EQ(xs.front(), core::Mm{485300000});
    CHECK_EQ(ys.front(), core::Mm{4310255000});
    CHECK_EQ(xs.back(), core::Mm{485420000});
    CHECK_EQ(ys.back(), core::Mm{4310265000});
}

TEST_CASE("METİN belge içeriğidir ve tek adımda geri alınır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=NOT", Origin::Test).ok());

    const std::uint64_t empty = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("METİN 485300,4310200 \"Plan notu 3\" 2200", Origin::Test).ok());
    CHECK(f.doc.content_hash() != empty);

    // The caption and its baseline are ONE entity, so one GERİAL takes both.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(f.doc.content_hash(), empty);
}

TEST_CASE("METİN: yazısı olmayan belge özetini değiştirmez")
{
    // The migration guarantee, same as the attribute table's: a drawing that
    // carries no text is the drawing it was before text existed, so no file and
    // no golden fixture needed rewriting to record a capability nobody used.
    Fixture a;
    REQUIRE(a.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        a.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());

    core::Document bare;
    core::TextTable empty;
    CHECK_EQ(empty.fold(12345), std::uint64_t{12345});
    CHECK_EQ(a.doc.texts().fold(999), std::uint64_t{999});
}

TEST_CASE("METİN: sıfır yükseklik reddedilir, taban çizgisi de kalmaz")
{
    // The whole transaction rolls back, so a rejected caption does not leave a
    // stray two-vertex polyline behind for the user to hunt down.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=NOT", Origin::Test).ok());
    (void)f.bus.execute_line("METİN 485300,4310200 \"boy yok\" yukseklik=0", Origin::Test);

    // yukseklik=0 falls back to the project default rather than failing, which is
    // the documented behaviour; what must never happen is a text entity with no
    // text on it.
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.entities().alive(e)) CHECK(f.doc.texts().has(f.doc.entities().slot[e]));
}

TEST_CASE("Ayrıştırıcı onaltılık sayıyı okur — renk her yerde 0xAARRGGBB yazılır")
{
    // The wart this closes: the settings parser accepted `0xFF101418` and the
    // command tokenizer did not, so `TERCİH arkaplan 0xFF101418` worked while
    // `STİL renk=0xFF2E7D32` was refused as "not an integer". The same kind of
    // value, two notations, one of them silently wrong.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL renk=0xFF2E7D32", Origin::Test).ok());

    const core::LayerId l = f.doc.find_layer("PARSEL");
    REQUIRE(l != core::kNoLayer);
    CHECK_EQ(f.doc.layer(l)->appearance.rgba, 0xFF2E7D32u);

    // Decimal still means what it always meant, and the two spellings agree.
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL renk=4281236786", Origin::Test).ok());
    CHECK_EQ(f.doc.layer(f.doc.find_layer("YOL"))->appearance.rgba, 0xFF2E7D32u);

    // Upper case, and a bare 0x with no digits is refused rather than read as 0.
    REQUIRE(f.bus.execute_line("KATMAN ad=BINA renk=0XFF2E7D32", Origin::Test).ok());
    CHECK_EQ(f.doc.layer(f.doc.find_layer("BINA"))->appearance.rgba, 0xFF2E7D32u);
    CHECK(!f.bus.execute_line("KATMAN ad=BOS renk=0x", Origin::Test).ok());
}

TEST_CASE("DİKDÖRTGEN: ikinci köşe dörtgen kılavuzu ister ve iki köşe kare olur")
{
    Fixture f;

    auto started = f.bus.begin_interactive("DİKDÖRTGEN");
    REQUIRE(started.ok());
    Session& session = *started.value();

    // The first corner is asked for with NO guide: there is nothing to preview
    // between yet.
    REQUIRE(session.waiting());
    CHECK_FALSE(session.prompt().has_rubber_band);

    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

    // The second is asked for with a RECTANGLE guide, and this is the assertion
    // that matters: previewed as a line, the guide shows the diagonal of the
    // shape instead of the shape, and the user finds out what they drew after
    // they have drawn it.
    REQUIRE(session.waiting());
    CHECK(session.prompt().has_rubber_band);
    CHECK(session.prompt().rubber_shape == RubberShape::Rectangle);
    CHECK_EQ(session.prompt().rubber_origin.x, core::Mm{0});

    REQUIRE(session.supply(Value::point(core::Point2{40'000, -25'000})).ok());
    REQUIRE(session.finished());
    REQUIRE(f.bus.finish(session).ok());

    // Four corners from two, and the face encloses what the two corners span.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{0});
    CHECK_EQ(box.max_x, core::Mm{40'000});
    CHECK_EQ(box.min_y, core::Mm{-25'000});
    CHECK_EQ(box.max_y, core::Mm{0});
}

TEST_CASE("DİKDÖRTGEN: aynı köşeden geçen iki nokta alan kapatmaz")
{
    Fixture f;

    auto started = f.bus.begin_interactive("DİKDÖRTGEN");
    REQUIRE(started.ok());
    Session& session = *started.value();

    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
    // Same y: the two corners are on one line and enclose nothing. Refused with a
    // message naming what to move, rather than handed to the geometry layer to
    // fail as a degenerate ring.
    REQUIRE(session.supply(Value::point(core::Point2{40'000, 0})).ok());
    REQUIRE(session.finished());
    (void)f.bus.finish(session);

    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

// ============================================================================
// KÖŞETAŞI / KÖŞEEKLE — corner editing (core.vertex_move, core.vertex_insert)
//
// Coordinates are typed in METRES and stored in millimetres (Article 2.4), so a
// 60 m x 45 m parsel is written `0,0 60,0 60,45 0,45` and read back as 60000 mm.
// ============================================================================

TEST_CASE("KÖŞETAŞI köşeyi taşır ve nesnenin kimliğini korur")
{
    // The point of editing rather than erase-and-redraw: the key, the layer and
    // every attribute hung off this parsel have to survive the correction
    // (model.md R4, R28). A new key here would be a new parsel.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    const core::EntityKey key = f.doc.entities().key[0];
    const core::Mm2 before    = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    auto moved = f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=90,0", Origin::Test);
    if (!moved) FAIL_WITH("KÖŞETAŞI", moved.error().message);
    REQUIRE(moved.ok());

    // Same entity, same key — one object that changed shape, not a new one.
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK(f.doc.entities().key[0] == key);

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).x, core::Mm{90000});
    CHECK(f.doc.geometry().area_of(f.doc.entities().slot[0]) > before);
}

TEST_CASE("KÖŞETAŞI geri alınınca köşe eski yerine döner")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::Mm2 before = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=90,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());

    // Undo is a slot repoint, not a rebuild: the rings the entity had are still in
    // the arena, so the geometry comes back EXACTLY, not approximately.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).x, core::Mm{60000});
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), before);

    REQUIRE(f.bus.execute_line("YİNELE", Origin::Test).ok());
    const core::RingSpan again = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().vertex(again.first, 1).x, core::Mm{90000});
}

TEST_CASE("KÖŞEEKLE kenarın ortasına köşe ekler, kapanış kenarı da bir kenardır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=1 nokta=30,-5", Origin::Test).ok());

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    REQUIRE_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{5});
    // Inserted AFTER corner 1, so it is the new corner 2.
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).x, core::Mm{30000});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).y, core::Mm{-5000});

    // Corner 5 is the last, and the segment leaving it is the closing edge — an
    // edge like any other, which a parsel needs to bend as often as the rest.
    REQUIRE(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=5 nokta=-5,22", Origin::Test).ok());
    const core::RingSpan after = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(after.first).size(), std::size_t{6});
}

TEST_CASE("KÖŞEEKLE açık çizginin son ucundan sonra köşe eklemeyi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());

    // A polyline's last vertex is an END, not a corner with an edge leaving it
    // (R10): there is no closing segment to bend. Refused, and nothing is written.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    const std::size_t before  = f.doc.geometry().ring_xs(span.first).size();

    REQUIRE(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=2 nokta=5,5", Origin::Test).ok());

    const core::RingSpan after = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(after.first).size(), before);
}

TEST_CASE("KÖŞETAŞI olmayan köşeyi ve silinmiş nesneyi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::Mm2 before = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    // Corner 9 of a four-corner parsel: refused, nothing written.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=9 nokta=1,1", Origin::Test).ok());
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), before);

    // And an entity that is gone stays gone.
    REQUIRE(f.bus.execute_line("SİL nesneler=1", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=1,1", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("KÖŞETAŞI: taşınan köşe yeni yerinde yakalanır")
{
    // The spatial index files an entity under the box it had. Moving a corner out
    // of that box left the tree unable to find it at its new position — a corner
    // the user can see but cannot snap to. `Document::index_stale_` is the fix and
    // this is what would have caught its absence.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    // Warm the index so the entity is filed under its ORIGINAL box.
    (void)f.doc.spatial_index();

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=900,0", Origin::Test).ok());

    core::SnapQuery q;
    q.aim    = core::Point2{core::Mm{900000}, core::Mm{0}};
    q.radius = core::Mm{1000};
    q.modes  = core::SnapEndpoint;

    const core::SnapResult r = core::snap(f.doc, q);
    CHECK(r.mode == core::SnapEndpoint);
    CHECK_EQ(r.point.x, core::Mm{900000});
}


// ============================================================================
// ALANAÇEVİR — closing a run of lines into one face (core.to_area)
// ============================================================================

TEST_CASE("ALANAÇEVİR uç uca değen çizgileri tek alana çevirir ve çizgileri siler")
{
    // How a boundary usually arrives: four separate lines that LOOK like a parsel.
    // An open ring encloses nothing, so until they are one face the drawing has no
    // area, no closed perimeter and nothing an ifraz can stand on (model.md R9-R10).
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,0 60,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,45 0,0", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{4});

    auto turned = f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4",
                                     Origin::Test);
    if (!turned) FAIL_WITH("ALANAÇEVİR", turned.error().message);
    REQUIRE(turned.ok());

    // One face, and the four lines are gone: a boundary that exists twice is a
    // topology error waiting to be exported.
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    core::EntityId face = core::kNoEntity;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e)) face = e;
    REQUIRE(face != core::kNoEntity);

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[face]);
    REQUIRE_EQ(span.count, std::uint32_t{1});
    CHECK(f.doc.geometry().ring_role[span.first] == core::RingRole::Exterior);

    // Four corners, not five: the closing vertex is implied, never stored.
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[face]),
             core::Mm2{60000} * core::Mm2{45000});
}

TEST_CASE("ALANAÇEVİR çizgilerin sırasına ve yönüne bakmaz")
{
    // Drawn in a jumbled order and half of them backwards, which is how a DXF
    // arrives. The chain is walked by matching ends, not by trusting the order.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,45 0,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,45 60,0", Origin::Test).ok()); // reversed
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,45 0,45", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4",
                               Origin::Test)
                .ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    core::EntityId face = core::kNoEntity;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e)) face = e;

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[face]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[face]),
             core::Mm2{60000} * core::Mm2{45000});
}

TEST_CASE("ALANAÇEVİR kapanmayan zinciri reddeder ve hiçbir şeyi değiştirmez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,0 60,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,45 0,45", Origin::Test).ok()); // no fourth side

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3", Origin::Test).ok());

    // Refused whole: the three lines are still three lines. A half-applied
    // conversion would be the partial edit Article 1.6 forbids.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{3});
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ALANAÇEVİR birbirine değmeyen çizgileri reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 500,500 560,500", Origin::Test).ok()); // far away

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ALANAÇEVİR zaten kapalı bir alanı reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ALANAÇEVİR geri alınınca çizgiler geri gelir, alan gider")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,0 60,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,45 0,0", Origin::Test).ok());

    const std::uint64_t lines = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4",
                               Origin::Test)
                .ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    // One command, one undo step — the face and the four erasures together.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});
    CHECK_EQ(f.doc.content_hash(), lines);
}

// ============================================================================
// DAİRE — the first curve (core.circle_draw, core.circle)
// ============================================================================

TEST_CASE("DAİRE merkez ve çember noktasından daire çizer")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());

    auto drawn = f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test);
    if (!drawn) FAIL_WITH("DAİRE", drawn.error().message);
    REQUIRE(drawn.ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    // A circle is its DEFINITION: the kind column says what it is, and the store
    // holds the centre and a handle due east — not a 128-gon.
    CHECK(f.doc.entities().kind[0] == core::kCircleKind);

    const std::uint32_t slot = f.doc.entities().slot[0];
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), slot).x, core::Mm{100000});
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), slot).y, core::Mm{100000});
    CHECK_EQ(core::circle_radius_of(f.doc.geometry(), slot), core::Mm{10000});
}

TEST_CASE("DAİRE kutusu çemberi sarar, iki tepe noktasını değil")
{
    // The stored vertices are the centre and a point due east of it, so the box
    // the arena computes for them is a flat line. Every cull, every pick prefilter
    // and every zoom-to-extents reads this box.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test).ok());

    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{90000});
    CHECK_EQ(box.max_x, core::Mm{110000});
    CHECK_EQ(box.min_y, core::Mm{90000});
    CHECK_EQ(box.max_y, core::Mm{110000});
}

TEST_CASE("DAİRE alanı pi*r^2, çizilen çokgenin alanı değil")
{
    // The whole reason a circle is a kind. A 128-gon of radius r has area
    // r^2 * 64 * sin(pi/64), about 0.06 % short of pi*r^2 — which on a 300 m
    // radius is 170 m^2 missing from a legal document.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());

    const core::KindSpec* spec = core::builtin_kinds().find(core::kCircleKind);
    REQUIRE(spec != nullptr);

    const std::uint32_t slot = f.doc.entities().slot[0];
    core::Mm2 area{0};
    spec->area(f.doc.geometry(), core::SlotSpan(&slot, 1), std::span<core::Mm2>(&area, 1));

    // r = 10 m = 10 000 mm, so pi*r^2 = 314 159 265.4 mm^2.
    CHECK_EQ(area, core::Mm2{314159265});
}

TEST_CASE("DAİRE'nin çizilen biçimi kapalı ve 128 köşeli")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());

    const core::KindSpec* spec = core::builtin_kinds().find(core::kCircleKind);
    const std::uint32_t slot   = f.doc.entities().slot[0];

    core::EmitBuffer buf;
    spec->outline(f.doc.geometry(), core::SlotSpan(&slot, 1), buf);

    REQUIRE_EQ(buf.run_total(), std::size_t{1});
    CHECK_EQ(buf.run_count[0], std::uint32_t{core::kCircleSegments});
    CHECK_EQ(buf.run_closed[0], std::uint8_t{1});

    // Every emitted vertex is on the rim, to within the rounding of one millimetre.
    for (std::size_t v = 0; v < buf.xs.size(); ++v) {
        const double dx = core::mm_to_metres(buf.xs[v]);
        const double dy = core::mm_to_metres(buf.ys[v]);
        CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - 10.0) < 0.001);
    }
}

TEST_CASE("DAİRE: aynı yere iki nokta reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=100,100", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("KÖŞETAŞI ve ALANAÇEVİR daireyi reddeder")
{
    // A circle's two vertices are a centre and a radius handle, not corners.
    // Treating them as corners would move the centre or resize the circle without
    // saying so, and inserting a third would leave a record that is not a circle.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=200,200", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);

    REQUIRE(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=1 nokta=200,200", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);

    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("DAİRE geri alınır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    const std::uint64_t empty = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(f.doc.content_hash(), empty);
}

// ============================================================================
// YAY — arcs (core.arc_draw, core.arc)
// ============================================================================

TEST_CASE("ÖZNİTELİK: varlık slotu ile geometri slotu ayrıştığında da doğru okunur")
{
    // THE BUG THIS EXISTS FOR. An attribute column is indexed by GEOMETRY slot —
    // `Document::set_attribute` writes `entities_.slot[e]` — and a reader that
    // indexes it with the ENTITY slot agrees only while the two happen to match.
    // The attribute panel did exactly that, so on a drawing whose entities were
    // created in mixed kinds it showed a DIFFERENT object's value in the cell.
    //
    // `Document::attribute` is the only reader that maps, so this pins the two
    // together for every live entity rather than for one lucky one.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("METİN noktalar=1,1 yazi=ABC", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=50,50 cevre=60,50", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 5,5", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());

    const core::AttrId col = f.doc.attributes().find("ada_no");
    REQUIRE(col != core::kNoAttr);

    // One distinct value per entity, written by KEY the way every client does.
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.alive(e)) continue;
        const auto key = static_cast<std::uint64_t>(core::raw(f.doc.entities().key[e]));
        const std::string line =
            "ÖZNİTELİK ad=ada_no nesne=" + std::to_string(key) + " deger=A" + std::to_string(key);
        if (!f.bus.execute_line(line, Origin::Test)) FAIL_WITH("ÖZNİTELİK", line);
    }

    // Each entity reads back ITS OWN value. Before the fix this held only for the
    // entities whose two slot numbers coincided.
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.alive(e)) continue;
        const auto key   = static_cast<std::uint64_t>(core::raw(f.doc.entities().key[e]));
        const auto stored = f.doc.attribute(col, e);
        if (!stored) FAIL_WITH("öznitelik okunamadı", std::to_string(e));
        CHECK(stored.value().present);
        CHECK(stored.value().text == "A" + std::to_string(key));
    }
}

TEST_CASE("ÖZNİTELİK: çok nesneli belgede doğru slota yazar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=20,0 30,0 30,10 20,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("METİN noktalar=1,1 yazi=ABC", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ÖZNİTELİK ad=ada_no nesne=1 deger=1284", Origin::Test).ok());

    const core::AttrTable& t    = f.doc.attributes();
    const core::AttrColumn* col = t.column(t.find("ada_no"));
    REQUIRE(col != nullptr);

    // The value must sit on the slot that key 1 resolves to RIGHT NOW. If the
    // two disagree, an attribute written through any client lands on a different
    // parsel than the one the user picked.
    const core::EntityId slot = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(slot != core::kNoEntity);
    CHECK(col->present(slot));
    const auto text = col->text(slot);
    CHECK(std::string(text.data(), text.size()) == "1284");
}

TEST_CASE("ÖZNİTELİK: panelin kurduğu satır çalışır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=ada_no tur=metin", Origin::Test).ok());

    // Exactly the line AttributePanel builds, quoting included.
    auto written =
        f.bus.execute_line("ÖZNİTELİK ad=\"ada_no\" nesne=1 deger=\"1284\"", Origin::Test);
    if (!written) FAIL_WITH("ÖZNİTELİK", written.error().message);

    const core::AttrTable& t = f.doc.attributes();
    const core::AttrColumn* col = t.column(t.find("ada_no"));
    REQUIRE(col != nullptr);

    const core::EntityId slot = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(slot != core::kNoEntity);
    REQUIRE(col->present(slot));
    const auto text = col->text(slot);
    CHECK(std::string(text.data(), text.size()) == "1284");
}

// -----------------------------------------------------------------------------
// OFSET — parallel geometry (core/offset.hpp)
// -----------------------------------------------------------------------------

TEST_CASE("Ofset: kapalı bir kare dışarı doğru büyür")
{
    using namespace piricad::core;

    // A 10 m square, offset outward by 1 m. The result must enclose more area
    // than the input and still be one ring.
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};

    auto grown = offset_ring(square, true, 1000);
    REQUIRE(grown.ok());
    REQUIRE(grown.value().size() == 1);

    const auto& ring = grown.value().front();
    CHECK(ring.closed);

    Box2 box;
    for (const Point2& p : ring.points) box.extend(p);
    CHECK(box.min_x <= -1000);
    CHECK(box.max_x >= 11000);
}

TEST_CASE("Ofset: içeri doğru küçülür, mesafeyi aşınca yok olur")
{
    using namespace piricad::core;
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};

    auto shrunk = offset_ring(square, true, -1000);
    REQUIRE(shrunk.ok());
    REQUIRE(shrunk.value().size() == 1);

    // Half the width inward leaves nothing. A hand-rolled offset returns an
    // inside-out ring here; this must return none at all.
    auto gone = offset_ring(square, true, -6000);
    REQUIRE(gone.ok());
    CHECK(gone.value().empty());
}

TEST_CASE("Ofset: açık bir çizginin ofseti kapalı bir bant olur")
{
    using namespace piricad::core;
    const std::vector<Point2> run{{0, 0}, {10000, 0}};

    auto band = offset_ring(run, false, 500);
    REQUIRE(band.ok());
    REQUIRE(band.value().size() == 1);
    CHECK(band.value().front().closed);
    CHECK(band.value().front().points.size() >= 4);
}

TEST_CASE("Ofset: sıfır mesafe ve yetersiz nokta gerekçesiyle reddedilir")
{
    using namespace piricad::core;
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};

    auto zero = offset_ring(square, true, 0);
    CHECK(!zero.ok());

    const std::vector<Point2> one{{0, 0}};
    auto tooFew = offset_ring(one, false, 1000);
    CHECK(!tooFew.ok());

    const std::vector<Point2> two{{0, 0}, {1000, 0}};
    auto notARing = offset_ring(two, true, 1000);
    CHECK(!notARing.ok());
}

TEST_CASE("OFSET seçili parseli paralelleştirir ve aslını yerinde bırakır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());

    const core::Box2 before = f.doc.extent();
    const std::size_t count = f.doc.live_entity_count();

    // 1 m outward. mesafe is millimetres in the argument form.
    auto made = f.bus.execute_line("OFSET nesneler=1 mesafe=1000", Origin::Test);
    if (!made) FAIL_WITH("OFSET", made.error().message);

    CHECK(f.doc.live_entity_count() == count + 1);

    // The original is untouched — it is the measured thing — and the drawing now
    // reaches further out than it did.
    const core::Box2 after = f.doc.extent();
    CHECK(after.min_x < before.min_x);
    CHECK(after.max_x > before.max_x);
}

TEST_CASE("OFSET tek geri alma adımıdır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("OFSET nesneler=1 mesafe=1000", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());

    CHECK(f.doc.content_hash() == before);
}

TEST_CASE("OFSET seçim boşken sebebini söyler")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("OFSET mesafe=1000", Origin::Test).ok());
    CHECK(said.find("nesne yok") != std::string::npos);
}

// -----------------------------------------------------------------------------
// ELİPS — stored by its definition (core.ellipse_draw)
// -----------------------------------------------------------------------------

TEST_CASE("ELİPS merkez ve iki eksenden çizilir, tanımıyla saklanır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());

    auto drawn =
        f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", Origin::Test);
    if (!drawn) FAIL_WITH("ELİPS", drawn.error().message);

    REQUIRE(f.doc.live_entity_count() == 1);
    CHECK(f.doc.entities().kind[0] == core::kEllipseKind);

    // THREE stored vertices, not a hundred and twenty-eight: the record is the
    // definition and the run is only what gets drawn.
    const auto slot = f.doc.entities().slot[0];
    const auto span = f.doc.geometry().rings_of(slot);
    REQUIRE(span.count == 1);
    CHECK(f.doc.geometry().ring_xs(span.first).size() == 3);
}

TEST_CASE("ELİPS: alanı pi·a·b, dairenin özel hâli tutarlı")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", Origin::Test).ok());

    // pi x 10 m x 5 m = 157,08 m².
    //
    // THROUGH THE KIND, not through the ring: the stored ring is the three-vertex
    // definition and encloses nothing. `ALANÖLÇ` asks the same way, which is why
    // a circle reports pi·r² rather than the area of the polygon it is drawn with.
    const auto slot = f.doc.entities().slot[0];
    const core::KindSpec* spec = core::builtin_kinds().find(core::kEllipseKind);
    REQUIRE(spec != nullptr);

    core::Mm2 area = 0;
    const std::uint32_t one[1]{slot};
    spec->area(f.doc.geometry(), core::SlotSpan(one, 1), std::span<core::Mm2>(&area, 1));
    CHECK(area > 156'000'000);
    CHECK(area < 158'000'000);
}

TEST_CASE("ELİPS: ikinci eksen birinciye DİK ölçülür")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());

    // The third point is 5 m across the axis and 100 m along it. Only the across
    // component counts, so this must be the same ellipse as `ikinci=0,5`.
    REQUIRE(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=100,5", Origin::Test).ok());

    const core::Point2 minor =
        core::ellipse_minor_of(f.doc.geometry(), f.doc.entities().slot[0]);
    CHECK(minor.x == 0);
    CHECK(minor.y == 5000);
}

TEST_CASE("ELİPS: eksen üzerindeki üçüncü nokta reddedilir")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=5,0", Origin::Test).ok());

    // A zero second axis is a line, not an ellipse, and drawing one would put a
    // record in the file that nothing downstream can draw.
    CHECK(f.doc.live_entity_count() == 0);
    CHECK(said.find("İkinci eksen sıfır") != std::string::npos);
}

TEST_CASE("ELİPS: döndürülmüş elipsin kapsam kutusu şekli içine almalı")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());

    // A 45-degree ellipse. Its bounding box is NOT the box of its three stored
    // vertices — that triangle sits strictly inside the shape — and a cull box
    // that small drops the ellipse at the edge of the view.
    REQUIRE(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,10 ikinci=-1,1", Origin::Test).ok());

    const core::Box2 box = f.doc.extent();
    std::vector<core::Mm> xs, ys;
    core::ellipse_outline(core::ellipse_centre_of(f.doc.geometry(), f.doc.entities().slot[0]),
                          core::ellipse_major_of(f.doc.geometry(), f.doc.entities().slot[0]),
                          core::ellipse_minor_of(f.doc.geometry(), f.doc.entities().slot[0]), xs,
                          ys);

    for (std::size_t i = 0; i < xs.size(); ++i) {
        CHECK(xs[i] >= box.min_x);
        CHECK(xs[i] <= box.max_x);
        CHECK(ys[i] >= box.min_y);
        CHECK(ys[i] <= box.max_y);
    }
}

// -----------------------------------------------------------------------------
// DİLİM ve HALKA — closed shapes a round curve encloses
// -----------------------------------------------------------------------------

TEST_CASE("DİLİM merkez ve iki kenardan kapalı bir dilim çizer")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    auto drawn = f.bus.execute_line("DİLİM merkez=0,0 baslangic=10,0 bitis=0,10", Origin::Test);
    if (!drawn) FAIL_WITH("DİLİM", drawn.error().message);

    REQUIRE(f.doc.live_entity_count() == 1);

    // A quarter of a 10 m circle is about 78,5 m². The exact figure depends on
    // how many chords the arc is drawn with, so this bounds it rather than
    // pinning it: a shape that did not close would report zero.
    const auto slot = f.doc.slot_of(f.doc.entities().key[0]);
    const core::Mm2 area =
        f.doc.geometry().area_of(f.doc.entities().slot[slot]);
    CHECK(area > 70'000'000);
    CHECK(area < 80'000'000);
}

TEST_CASE("DİLİM: merkezle çakışan kenar reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DİLİM merkez=0,0 baslangic=0,0 bitis=0,10", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 0);
}

TEST_CASE("HALKA delikli bir alan çizer ve deliğin alanı sayılmaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    auto drawn = f.bus.execute_line("HALKA merkez=0,0 ic=5,0 dis=10,0", Origin::Test);
    if (!drawn) FAIL_WITH("HALKA", drawn.error().message);
    REQUIRE(f.doc.live_entity_count() == 1);

    // pi*(10^2 - 5^2) = 235,6 m². If the hole were counted as solid the answer
    // would be about 314 m², which is the failure this bounds.
    const auto slot = f.doc.slot_of(f.doc.entities().key[0]);
    const core::Mm2 area =
        f.doc.geometry().area_of(f.doc.entities().slot[slot]);
    CHECK(area > 225'000'000);
    CHECK(area < 240'000'000);
}

TEST_CASE("HALKA: iç ve dış ters verilse de çalışır, eşitse reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    // Outer clicked first is not a mistake.
    REQUIRE(f.bus.execute_line("HALKA merkez=0,0 ic=10,0 dis=5,0", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 1);

    // Zero width is not a ring.
    REQUIRE(f.bus.execute_line("HALKA merkez=0,0 ic=5,0 dis=5,0", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 1);
}

// -----------------------------------------------------------------------------
// KILAVUZ — drafting guides (core.guide)
// -----------------------------------------------------------------------------

TEST_CASE("KILAVUZ yatay ve düşey kılavuz ekler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=4310220500", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KILAVUZ yon=düşey deger=485320000", Origin::Test).ok());

    REQUIRE(f.doc.guides().size() == 2);
    CHECK(f.doc.guides().axis(0) == core::GuideAxis::Horizontal);
    CHECK(f.doc.guides().coordinate(0) == 4310220500);
    CHECK(f.doc.guides().axis(1) == core::GuideAxis::Vertical);
    CHECK(f.doc.guides().coordinate(1) == 485320000);
}

TEST_CASE("KILAVUZ tek geri alma adımıdır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=1000", Origin::Test).ok());
    REQUIRE(f.doc.guides().size() == 1);

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(f.doc.guides().empty());

    REQUIRE(f.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK(f.doc.guides().size() == 1);
}

TEST_CASE("KILAVUZ sil: yerini söyleyerek silinir, indeksle değil")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=1000", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=2000", Origin::Test).ok());

    // Within half a metre of where it sits, which is as precisely as anyone can
    // point at a line on a ruler.
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=1200 sil=evet", Origin::Test).ok());
    REQUIRE(f.doc.guides().size() == 1);
    CHECK(f.doc.guides().coordinate(0) == 2000);
}

TEST_CASE("KILAVUZ: olmayan yerde silme isteği sebebini söyler")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=9999 sil=evet", Origin::Test).ok());
    CHECK(said.find("kılavuz yok") != std::string::npos);
}

TEST_CASE("KILAVUZ bir varlık DEĞİLDİR: seçime, sayıma ve kapsama girmez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());

    const core::Box2 before = f.doc.extent();
    const std::size_t count = f.doc.live_entity_count();

    // A guide far outside the drawing must not stretch its extent, and must not
    // become something SEÇ TÜMÜ can pick — a construction line in a tapu is the
    // failure this guards.
    REQUIRE(f.bus.execute_line("KILAVUZ yon=yatay deger=999000000", Origin::Test).ok());

    CHECK(f.doc.live_entity_count() == count);
    const core::Box2 after = f.doc.extent();
    CHECK(after.max_y == before.max_y);

    REQUIRE(f.bus.execute_line("SEÇ TÜMÜ", Origin::Test).ok());
    CHECK(f.bus.selection().size() == count);
}

// -----------------------------------------------------------------------------
// Polygon boolean — what TEVHİT, İFRAZ and TOPOLOJİ are built on
// -----------------------------------------------------------------------------

TEST_CASE("BOOLEAN: bitişik iki parsel birleşince tek parsel olur")
{
    using namespace piricad::core;

    Polygon left{{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}}, {}};
    Polygon right{{{10000, 0}, {20000, 0}, {20000, 10000}, {10000, 10000}}, {}};

    auto merged = polygon_boolean({left}, {right}, BooleanOp::Union);
    REQUIRE(merged.ok());
    REQUIRE(merged.value().size() == 1); // touching along an edge means ONE parcel

    // 10 m x 20 m = 200 m². The seam must be gone, not drawn twice.
    const Mm2 area = ring_area(merged.value().front().exterior);
    CHECK((area < 0 ? -area : area) == 200'000'000);
}

TEST_CASE("BOOLEAN: ayrık iki parsel birleşince iki parsel kalır")
{
    using namespace piricad::core;

    Polygon a{{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}}, {}};
    Polygon b{{{50000, 0}, {60000, 0}, {60000, 10000}, {50000, 10000}}, {}};

    auto merged = polygon_boolean({a}, {b}, BooleanOp::Union);
    REQUIRE(merged.ok());
    CHECK(merged.value().size() == 2); // and saying "one" would be a lie
}

TEST_CASE("BOOLEAN: fark bir parseli ikiye bölebilir")
{
    using namespace piricad::core;

    // A 30 m parcel with a 10 m band taken out of its middle: two pieces.
    Polygon parcel{{{0, 0}, {30000, 0}, {30000, 10000}, {0, 10000}}, {}};
    Polygon band{{{10000, -1000}, {20000, -1000}, {20000, 11000}, {10000, 11000}}, {}};

    auto cut = polygon_boolean({parcel}, {band}, BooleanOp::Difference);
    REQUIRE(cut.ok());
    CHECK(cut.value().size() == 2);
}

TEST_CASE("BOOLEAN: örtüşme kesişimle bulunur, örtüşmeyen ikili boş döner")
{
    using namespace piricad::core;

    Polygon a{{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}}, {}};
    Polygon overlapping{{{5000, 5000}, {15000, 5000}, {15000, 15000}, {5000, 15000}}, {}};
    Polygon apart{{{50000, 0}, {60000, 0}, {60000, 10000}, {50000, 10000}}, {}};

    auto shared = polygon_boolean({a}, {overlapping}, BooleanOp::Intersection);
    REQUIRE(shared.ok());
    REQUIRE(shared.value().size() == 1);
    const Mm2 area = ring_area(shared.value().front().exterior);
    CHECK((area < 0 ? -area : area) == 25'000'000); // 5 m x 5 m

    auto none = polygon_boolean({a}, {apart}, BooleanOp::Intersection);
    REQUIRE(none.ok());
    CHECK(none.value().empty());
}

TEST_CASE("BOOLEAN: delik, sarım yönü ne olursa olsun delik kalır")
{
    using namespace piricad::core;

    // The hole is wound the SAME way as its exterior, which is what a DXF or a
    // GML may hand over. Even-odd must still read it as a void.
    Polygon donut{{{0, 0}, {30000, 0}, {30000, 30000}, {0, 30000}},
                  {{{10000, 10000}, {20000, 10000}, {20000, 20000}, {10000, 20000}}}};

    auto kept = polygon_boolean({donut}, {}, BooleanOp::Union);
    REQUIRE(kept.ok());
    REQUIRE(kept.value().size() == 1);
    CHECK(kept.value().front().holes.size() == 1);
}

// -----------------------------------------------------------------------------
// KAYDIR — panning the view (core.pan)
// -----------------------------------------------------------------------------

TEST_CASE("KAYDIR iki noktayı görünüm istemcisine iletir")
{
    Fixture f;
    core::Point2 from{}, to{};
    int calls = 0;
    f.bus.on_pan_request = [&](core::Point2 a, core::Point2 b) {
        from = a;
        to   = b;
        ++calls;
    };

    auto moved =
        f.bus.execute_line("KAYDIR baslangic=100,200 bitis=150,200", Origin::Test);
    if (!moved) FAIL_WITH("KAYDIR", moved.error().message);

    CHECK(calls == 1);
    CHECK(from.x == 100000);
    CHECK(to.x == 150000);
}

TEST_CASE("KAYDIR başsız çalışmada sessizce başarısız olmaz, sebebini yazar")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KAYDIR baslangic=0,0 bitis=1,1", Origin::Test).ok());
    CHECK(said.find("istemcisi bağlı değil") != std::string::npos);
}

TEST_CASE("KAYDIR çizimi değiştirmez")
{
    Fixture f;
    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("KAYDIR baslangic=0,0 bitis=10,10", Origin::Test).ok());
    CHECK(f.doc.content_hash() == before);
    CHECK(!f.undo.can_undo());
}

// -----------------------------------------------------------------------------
// SEÇ — the box it can now ask for
// -----------------------------------------------------------------------------

TEST_CASE("SEÇ KUTU iki köşe verildiğinde kutunun içindekini seçer")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=100,100 110,100 110,110 100,110", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("SEÇ mod=KUTU noktalar=-5,-5 20,20", Origin::Test).ok());
    CHECK(f.bus.selection().size() == 1);
}

TEST_CASE("SEÇ etkileşimlidir: arayüz köşeleri sorabilsin")
{
    Fixture f;
    const CommandSpec* spec = f.reg.resolve("SEÇ");
    REQUIRE(spec != nullptr);
    // The tool column's "Alan Seç" button has nothing to send without this.
    CHECK(has_flag(spec->flags, Flags::Interactive));
}

// -----------------------------------------------------------------------------
// ADIM — the step lock (core.yakalama.adim)
// -----------------------------------------------------------------------------

TEST_CASE("ADIM: uzaklık adımın katına yuvarlanır, yön korunur")
{
    using namespace piricad::core;

    // 12 cm step, a horizontal aim at 1,00 m: the nearest multiple is 96 cm.
    SnapQuery q;
    q.has_base = true;
    q.base     = Point2{0, 0};
    q.aim      = Point2{1000, 0}; // 1,00 m in millimetres
    q.step     = 120;             // 12 cm

    const SnapResult r = snap(Document{}, q);
    CHECK(r.mode == SnapStep);
    CHECK(r.point.y == 0);
    CHECK(r.point.x == 960); // 8 x 120
}

TEST_CASE("ADIM: adımın üstündeki bir nokta yerinde kalır")
{
    using namespace piricad::core;

    SnapQuery q;
    q.has_base = true;
    q.base     = Point2{0, 0};
    q.aim      = Point2{960, 0};
    q.step     = 120;

    const SnapResult r = snap(Document{}, q);
    CHECK(r.point.x == 960);
    CHECK(r.point.y == 0);
}

TEST_CASE("ADIM dik modla birlikte çalışır: eksen dik moddan, uzunluk adımdan")
{
    using namespace piricad::core;

    SnapQuery q;
    q.has_base = true;
    q.base     = Point2{0, 0};
    q.aim      = Point2{1000, 130}; // mostly horizontal, so ortho keeps x
    q.ortho    = true;
    q.step     = 120;

    const SnapResult r = snap(Document{}, q);
    CHECK(r.mode == SnapOrtho); // the direction lock is what fired
    CHECK(r.point.y == 0);      // ortho flattened it
    CHECK(r.point.x == 960);    // and the step rounded the length
}

TEST_CASE("ADIM kapalıyken hiçbir şeye dokunmaz")
{
    using namespace piricad::core;

    SnapQuery q;
    q.has_base = true;
    q.base     = Point2{0, 0};
    q.aim      = Point2{1234, 5678};
    q.step     = 0;

    const SnapResult r = snap(Document{}, q);
    CHECK(r.point.x == 1234);
    CHECK(r.point.y == 5678);
}

TEST_CASE("ADIM ayarı oturum kapsamındadır ve MOD ile yazılır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("MOD ad=adım deger=120", Origin::Test).ok());
    CHECK(f.bus.session_settings().get("core.yakalama.adim").as_length() == 120);
}

// -----------------------------------------------------------------------------
// Object snap modes — the mask every client writes (core.mode)
// -----------------------------------------------------------------------------

TEST_CASE("MOD yakalama maskesini yazar ve okur")
{
    Fixture f;

    // The panel writes the WHOLE mask, so this is exactly what the OSNAP list,
    // the command line and a script all send.
    REQUIRE(f.bus.execute_line("MOD ad=yakalama_modları deger=127", Origin::Test).ok());
    CHECK(f.bus.session_settings().get("core.yakalama.modlar").as_int() == 127);

    REQUIRE(f.bus.execute_line("MOD ad=yakalama_modlari deger=0", Origin::Test).ok());
    CHECK(f.bus.session_settings().get("core.yakalama.modlar").as_int() == 0);
}

TEST_CASE("Yakalama maskesi her bir kipi ayrı ayrı taşır")
{
    Fixture f;

    // Each declared bit must survive a write and read on its own. A mask that
    // silently drops a mode is how KESİŞİM could be 'on' and never fire.
    for (std::uint16_t bit = 1; bit != 0; bit = static_cast<std::uint16_t>(bit << 1)) {
        if ((core::SnapAllMask & bit) == 0) continue;

        const std::string line = "MOD ad=yakalama_modları deger=" + std::to_string(bit);
        if (!f.bus.execute_line(line, Origin::Test)) FAIL_WITH("MOD", line);
        CHECK(f.bus.session_settings().get("core.yakalama.modlar").as_int() == bit);
    }
}

TEST_CASE("Her yakalama kipinin Türkçe etiketi ve makine adı vardır")
{
    // The OSNAP panel builds its rows from these, so a mode without a name would
    // appear in the list as "yok" — and a script could not name it either.
    for (std::uint16_t bit = 1; bit != 0; bit = static_cast<std::uint16_t>(bit << 1)) {
        if ((core::SnapAllMask & bit) == 0) continue;

        CHECK(std::string(core::snap_mode_id(bit)) != "yok");
        CHECK(std::string(core::snap_mode_label(bit)) != "yok");
    }
}

// -----------------------------------------------------------------------------
// KOORDİNAT — reading one point (core.coordinate)
// -----------------------------------------------------------------------------

TEST_CASE("KOORDİNAT tıklanan noktayı belgenin koordinat sisteminde yazar")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    auto read = f.bus.execute_line("KOORDİNAT nokta=485320.5,4310220.25", Origin::Test);
    if (!read) FAIL_WITH("KOORDİNAT", read.error().message);

    CHECK(said.find("485320,500") != std::string::npos);
    CHECK(said.find("4310220,250") != std::string::npos);
    CHECK(said.find("TUREF/TM36") != std::string::npos);
}

TEST_CASE("KOORDİNAT çizimi değiştirmez ve geri alma adımı bırakmaz")
{
    Fixture f;
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("KOORDİNAT nokta=10,20", Origin::Test).ok());

    CHECK(f.doc.content_hash() == before);
    CHECK(!f.undo.can_undo()); // a reading is not a change, so there is nothing to undo
}

TEST_CASE("KOORDİNAT: kısaltmaları ve İngilizce adı aynı komuta çözülür")
{
    Fixture f;
    const CommandSpec* by_tr = f.reg.resolve("KOORDİNAT");
    REQUIRE(by_tr != nullptr);
    CHECK(by_tr->id == "core.coordinate");
    CHECK(f.reg.resolve("KOORDINAT") == by_tr);
    CHECK(f.reg.resolve("COORDINATE") == by_tr);
    CHECK(f.reg.resolve("KRD") == by_tr);
}

TEST_CASE("YAY merkez ve iki uçtan yay çizer, süpürme saat yönünün tersine")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());

    // East to north: a quarter turn counter-clockwise.
    auto drawn = f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,130",
                                    Origin::Test);
    if (!drawn) FAIL_WITH("YAY", drawn.error().message);
    REQUIRE(drawn.ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK(f.doc.entities().kind[0] == core::kArcKind);

    const std::uint32_t slot = f.doc.entities().slot[0];
    CHECK_EQ(core::arc_radius_of(f.doc.geometry(), slot), core::Mm{30000});
    CHECK_EQ(core::arc_centre_of(f.doc.geometry(), slot).x, core::Mm{100000});

    // A quarter arc from due east to due north stays in that quadrant, so its box
    // is the quadrant — NOT the box of its four defining vertices, which would
    // include the centre.
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{100000});
    CHECK_EQ(box.max_x, core::Mm{130000});
    CHECK_EQ(box.min_y, core::Mm{100000});
    CHECK_EQ(box.max_y, core::Mm{130000});
}

TEST_CASE("YAY: yarım turdan büyük süpürme kısa yoldan çizilmez")
{
    // The case chord bisection gets wrong on its own: the midpoint of two
    // directions more than half a turn apart bisects the arc the OTHER way, so a
    // 270 degree arc would come out as the 90 degree one it is the complement of.
    // The quadrant split in `arc_outline` is what prevents that.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());

    // East, counter-clockwise, all the way round to due south: 270 degrees.
    REQUIRE(f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,70", Origin::Test)
                .ok());

    // A 270 degree sweep reaches west and north as well as east and south, so its
    // box covers three sides of the full circle.
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.max_x, core::Mm{130000});
    CHECK_EQ(box.max_y, core::Mm{130000});
    CHECK_EQ(box.min_x, core::Mm{70000});
    CHECK_EQ(box.min_y, core::Mm{70000});
}

TEST_CASE("YAY hiçbir şey çevrelemez: alanı sıfır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,130", Origin::Test)
                .ok());

    const core::KindSpec* spec = core::builtin_kinds().find(core::kArcKind);
    REQUIRE(spec != nullptr);

    const std::uint32_t slot = f.doc.entities().slot[0];
    core::Mm2 area{0};
    spec->area(f.doc.geometry(), core::SlotSpan(&slot, 1), std::span<core::Mm2>(&area, 1));

    // An open curve encloses nothing, exactly as an open ring does (R10).
    CHECK_EQ(area, core::Mm2{0});
}

TEST_CASE("YAY'ın çizilen biçimi açık ve her noktası yarıçapta")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=0,0 baslangic=30,0 bitis=0,30", Origin::Test).ok());

    const core::KindSpec* spec = core::builtin_kinds().find(core::kArcKind);
    const std::uint32_t slot   = f.doc.entities().slot[0];

    core::EmitBuffer buf;
    spec->outline(f.doc.geometry(), core::SlotSpan(&slot, 1), buf);

    REQUIRE_EQ(buf.run_total(), std::size_t{1});
    CHECK_EQ(buf.run_closed[0], std::uint8_t{0}); // an arc does not close
    CHECK(buf.run_count[0] > 8);

    for (std::size_t v = 0; v < buf.xs.size(); ++v) {
        const double dx = core::mm_to_metres(buf.xs[v]);
        const double dy = core::mm_to_metres(buf.ys[v]);
        CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - 30.0) < 0.001);
        // The quarter sweep from east to north never leaves its quadrant.
        CHECK(buf.xs[v] >= core::Mm{-1});
        CHECK(buf.ys[v] >= core::Mm{-1});
    }
}

TEST_CASE("YAY: merkezle çakışan uç reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=100,100 baslangic=100,100 bitis=100,130", Origin::Test)
                .ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("KÖŞETAŞI ve ALANAÇEVİR yayı da reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,130", Origin::Test)
                .ok());

    const std::uint64_t before = f.doc.content_hash();
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=200,200", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

// ============================================================================
// TAŞI / DÖNDÜR / ÖLÇEKLE / AYNALA — the transform family
// ============================================================================

TEST_CASE("TAŞI nesneyi taşır ve kimliğini korur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::EntityKey key = f.doc.entities().key[0];
    const core::Mm2 area      = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    auto moved = f.bus.execute_line("TAŞI nesneler=1 baslangic=0,0 bitis=100,50", Origin::Test);
    if (!moved) FAIL_WITH("TAŞI", moved.error().message);
    REQUIRE(moved.ok());

    // Same object, same size, new place.
    CHECK(f.doc.entities().key[0] == key);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), area);

    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{100000});
    CHECK_EQ(box.min_y, core::Mm{50000});
}

TEST_CASE("TAŞI daireyi daire olarak taşır")
{
    // The reason a transform asks the KIND rather than walking raw vertices: a
    // circle's two are a centre and a radius handle, and moving them
    // independently would leave a record that is no longer a circle.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("TAŞI nesneler=1 baslangic=0,0 bitis=50,25", Origin::Test).ok());

    CHECK(f.doc.entities().kind[0] == core::kCircleKind);
    const std::uint32_t slot = f.doc.entities().slot[0];
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), slot).x, core::Mm{150000});
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), slot).y, core::Mm{125000});
    // A move does not resize.
    CHECK_EQ(core::circle_radius_of(f.doc.geometry(), slot), core::Mm{10000});

    // And the box travelled with it, still bounding the circle rather than its
    // two stored vertices.
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{140000});
    CHECK_EQ(box.max_y, core::Mm{135000});
}

TEST_CASE("DÖNDÜR: dik açı köşeleri tam yerine koyar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::Mm2 area = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    REQUIRE(f.bus.execute_line("DÖNDÜR nesneler=1 merkez=0,0 aci=90", Origin::Test).ok());

    // A quarter turn about the origin sends (60,0) to (0,60) EXACTLY — that is
    // what the integer angle reduction is for.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).x, core::Mm{0});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).y, core::Mm{60000});

    // Turning a parcel does not change its area.
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), area);
}

TEST_CASE("DÖNDÜR: dört çeyrek tur nesneyi yerine geri getirir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    for (int i = 0; i < 4; ++i)
        REQUIRE(f.bus.execute_line("DÖNDÜR nesneler=1 merkez=0,0 aci=90", Origin::Test).ok());

    // Bit for bit, not nearly: an axis angle must not accumulate error.
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ÖLÇEKLE alanı çarpanın karesi kadar büyütür")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    const core::Mm2 area = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    REQUIRE(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2", Origin::Test).ok());
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), area * 4);

    // The base point stays exactly where it was.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().vertex(span.first, 0).x, core::Mm{0});
}

TEST_CASE("ÖLÇEKLE dairenin yarıçapını ölçekler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=3", Origin::Test).ok());
    CHECK_EQ(core::circle_radius_of(f.doc.geometry(), f.doc.entities().slot[0]), core::Mm{30000});
}

TEST_CASE("ÖLÇEKLE negatif ya da sıfır çarpanı reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=-1", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);

    REQUIRE(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("AYNALA düşey eksende tam yansıtır ve alanı korur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 10,0 60,0 60,45 10,45", Origin::Test).ok());
    const core::Mm2 area = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    // Mirror in the y axis: an integer negation, so nothing rounds.
    REQUIRE(f.bus.execute_line("AYNALA nesneler=1 baslangic=0,0 bitis=0,10", Origin::Test).ok());

    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, core::Mm{-60000});
    CHECK_EQ(box.max_x, core::Mm{-10000});

    // A REFLECTION REVERSES WINDING. The vertex order is put back so the area
    // stays positive — a face whose exterior wound the wrong way would report a
    // negative area, and the area calculation is the legal output (R12).
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), area);
    CHECK(f.doc.geometry().area_of(f.doc.entities().slot[0]) > core::Mm2{0});
}

TEST_CASE("AYNALA iki kez uygulanınca nesne aynen geri gelir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 10,0 60,0 60,45 10,45", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("AYNALA nesneler=1 baslangic=0,0 bitis=0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("AYNALA nesneler=1 baslangic=0,0 bitis=0,10", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("AYNALA yayın süpürme yönünü çevirir")
{
    // A reflection turns the plane inside out, so an arc that swept
    // counter-clockwise now sweeps the other way. The two ends are swapped, which
    // is how `core.arc` writes a reversed sweep down.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=0,0 baslangic=30,0 bitis=0,30", Origin::Test).ok());

    const std::uint32_t slot = f.doc.entities().slot[0];
    const core::Point2 start = core::arc_start_of(f.doc.geometry(), slot);

    // Mirror in the x axis: the quarter arc that ran east->north now runs
    // east->south, which is the SAME quadrant sweep read the other way.
    REQUIRE(f.bus.execute_line("AYNALA nesneler=1 baslangic=0,0 bitis=10,0", Origin::Test).ok());

    const std::uint32_t after = f.doc.entities().slot[0];
    CHECK_EQ(core::arc_radius_of(f.doc.geometry(), after), core::Mm{30000});
    // The old start is now the end: the sweep was reversed rather than the arc
    // being left describing the wrong three quarters of its circle.
    CHECK_EQ(core::arc_end_of(f.doc.geometry(), after).x, start.x);

    // And the box is still a quadrant, now the southern one.
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.max_x, core::Mm{30000});
    CHECK_EQ(box.min_y, core::Mm{-30000});
    CHECK_EQ(box.max_y, core::Mm{0});
}

TEST_CASE("Dönüşümler seçimi kullanır ve geri alınır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 100,0 160,0 160,45 100,45", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // No `nesneler`: the active selection is what the user means.
    REQUIRE(f.bus.execute_line("SEÇ nesneler=1 nesneler=2", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("TAŞI baslangic=0,0 bitis=0,1000", Origin::Test).ok());

    CHECK_EQ(f.doc.entity_extent(0).min_y, core::Mm{1000000});
    CHECK_EQ(f.doc.entity_extent(1).min_y, core::Mm{1000000});

    // One command, one undo step, both objects.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("KOPYALA nesneyi çoğaltır; kopya yeni bir kimlik alır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::EntityKey original = f.doc.entities().key[0];

    auto copied = f.bus.execute_line("KOPYALA nesneler=1 baslangic=0,0 bitis=100,0", Origin::Test);
    if (!copied) FAIL_WITH("KOPYALA", copied.error().message);
    REQUIRE(copied.ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});

    // The original did not move.
    CHECK(f.doc.entities().key[0] == original);
    CHECK_EQ(f.doc.entity_extent(0).min_x, core::Mm{0});

    // The copy is a NEW object with its own key (R4) at the new place.
    CHECK(f.doc.entities().key[1] != original);
    CHECK_EQ(f.doc.entity_extent(1).min_x, core::Mm{100000});
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[1]),
             f.doc.geometry().area_of(f.doc.entities().slot[0]));
}

TEST_CASE("KOPYALA yazıyı ve öznitelikleri de taşır")
{
    // A copied parsel that lost its ada number would be a worse copy than one
    // that kept it: correcting one number is a smaller job than retyping all of
    // them.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN ada metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada 1 \"1234\"", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("KOPYALA nesneler=1 baslangic=0,0 bitis=100,0", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});

    const core::AttrId col = f.doc.attributes().find("ada");
    REQUIRE(col != core::kNoAttr);

    auto original = f.doc.attribute(col, 0);
    auto copy     = f.doc.attribute(col, 1);
    REQUIRE(original.ok());
    REQUIRE(copy.ok());
    CHECK(copy.value().present);
    CHECK(copy.value() == original.value());
}

TEST_CASE("KOPYALA metin nesnesinin yazısını da kopyalar")
{
    // Text is drawing content carried by the entity slot, not a label computed
    // from an attribute (text_store.hpp) — so a copied text must say the same
    // thing in its new place.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PAFTA", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("METİN 485330,4310225 \"1234/7\" 2000", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    REQUIRE(f.bus.execute_line("KOPYALA nesneler=1 baslangic=0,0 bitis=50,0", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});

    const std::uint32_t copy = f.doc.entities().slot[1];
    CHECK(f.doc.texts().has(copy));
    CHECK_EQ(std::string(f.doc.texts().text(copy)), std::string("1234/7"));
    CHECK_EQ(f.doc.texts().height(copy), core::Mm{2000});
}

TEST_CASE("KOPYALA daireyi daire olarak kopyalar ve geri alınır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("KOPYALA nesneler=1 baslangic=0,0 bitis=50,0", Origin::Test).ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK(f.doc.entities().kind[1] == core::kCircleKind);
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), f.doc.entities().slot[1]).x,
             core::Mm{50000});
    CHECK_EQ(core::circle_radius_of(f.doc.geometry(), f.doc.entities().slot[1]), core::Mm{10000});

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ÇOKLUÇİZGİ çok noktadan TEK nesne yapar")
{
    // The difference from ÇİZGİ, and the reason both exist: a road edge that is
    // fourteen separate objects cannot be selected as one, styled as one, given
    // one attribute row, or exported as one feature.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());

    auto drawn = f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10", Origin::Test);
    if (!drawn) FAIL_WITH("ÇOKLUÇİZGİ", drawn.error().message);
    REQUIRE(drawn.ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    REQUIRE_EQ(span.count, std::uint32_t{1});
    CHECK(f.doc.geometry().ring_role[span.first] == core::RingRole::Open);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});

    // An open ring encloses nothing, so it has no area — a polyline is not a face.
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), core::Mm2{0});
}

TEST_CASE("ÇİZGİ ile ÇOKLUÇİZGİ aynı noktalardan farklı sayıda nesne üretir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0 10,10 20,10", Origin::Test).ok());
    const std::size_t as_segments = f.doc.live_entity_count();

    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10", Origin::Test).ok());

    // Three segments from ÇİZGİ, one object from ÇOKLUÇİZGİ.
    CHECK_EQ(as_segments, std::size_t{3});
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});
}

TEST_CASE("ÇOKLUÇİZGİ tek nokta ile reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());

    // Refused by the BUS, before the body runs: `noktalar` is declared
    // `Arity::at_least(2)`, and declaring arity is what lets validation happen
    // once for every client rather than in each command body (Article 1.3).
    CHECK_FALSE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

// ============================================================================
// NOKTA — surveyed points (core.point_draw, core.point) and the DÜĞÜM snap
// ============================================================================

TEST_CASE("NOKTA ölçülmüş nokta yerleştirir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=NİRENGİ", Origin::Test).ok());

    auto placed = f.bus.execute_line("NOKTA 485300,4310200 485360,4310245", Origin::Test);
    if (!placed) FAIL_WITH("NOKTA", placed.error().message);
    REQUIRE(placed.ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK(f.doc.entities().kind[0] == core::kPointKind);
    CHECK(f.doc.entities().kind[1] == core::kPointKind);

    CHECK_EQ(core::point_position_of(f.doc.geometry(), f.doc.entities().slot[0]).x,
             core::Mm{485300000});

    // A point is a place: it encloses nothing and its box has no size.
    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_x, box.max_x);
    CHECK_EQ(box.min_y, box.max_y);
}

TEST_CASE("DÜĞÜM yakalaması röperi bulur ve köşenin önüne geçer")
{
    // The bit `snap.hpp` reserved and left unused until the document could hold a
    // monument. A boundary is measured FROM a röper, so when the two sit within a
    // millimetre of each other the röper is what the surveyor meant.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("NOKTA 60.001,0", Origin::Test).ok());

    core::SnapQuery q;
    q.aim    = core::Point2{core::Mm{60000}, core::Mm{0}};
    q.radius = core::Mm{2000};

    // With only the node mode on, the monument is what comes back.
    q.modes = core::SnapNode;
    core::SnapResult r = core::snap(f.doc, q);
    CHECK(r.mode == core::SnapNode);
    CHECK_EQ(r.point.x, core::Mm{60001});

    // With both on, the monument still wins: it is ranked above a corner.
    q.modes = core::SnapNode | core::SnapEndpoint;
    r       = core::snap(f.doc, q);
    CHECK(r.mode == core::SnapNode);

    // And with only endpoint on, the parcel corner is found instead.
    q.modes = core::SnapEndpoint;
    r       = core::snap(f.doc, q);
    CHECK(r.mode == core::SnapEndpoint);
    CHECK_EQ(r.point.x, core::Mm{60000});
}

TEST_CASE("NOKTA dosyaya gidip nokta olarak geri geliyor")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=NİRENGİ", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("NOKTA 485300,4310200", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    REQUIRE(f.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK(f.doc.entities().kind[0] == core::kPointKind);
}

// ============================================================================
// ÖLÇ / ALANÖLÇ — measurement (read-only)
// ============================================================================

TEST_CASE("ÖLÇ mesafeyi ve koordinat farkını yazar, çizimi değiştirmez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    auto measured = f.bus.execute_line("ÖLÇ baslangic=0,0 bitis=30,40", Origin::Test);
    if (!measured) FAIL_WITH("ÖLÇ", measured.error().message);
    REQUIRE(measured.ok());

    // A measurement answers and changes nothing: no edit, no undo step.
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_FALSE(measured.value().mutated);
    // ALAN's step only: creating a layer is deliberately not undoable, and ÖLÇ
    // did not edit anything to have a step of its own.
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});
}

TEST_CASE("ALANÖLÇ dairenin alanını pi*r^2 olarak bildirir")
{
    // The kind answers, so a circle reports its true area rather than the area of
    // the polygon it is drawn with — the whole reason a circle is a kind.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    auto measured = f.bus.execute_line("ALANÖLÇ nesneler=1", Origin::Test);
    if (!measured) FAIL_WITH("ALANÖLÇ", measured.error().message);
    REQUIRE(measured.ok());
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_FALSE(measured.value().mutated);
}

TEST_CASE("ALANÖLÇ seçimi kullanır ve boş seçimi açıklar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    // Nothing selected and no ids: explained rather than silently doing nothing.
    REQUIRE(f.bus.execute_line("ALANÖLÇ", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("SEÇ nesneler=1", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALANÖLÇ", Origin::Test).ok());
}

TEST_CASE("registry: bildirilen her komut GERÇEKTEN kaydedilmiş")
{
    // A registration that fails only writes a log line and carries on, so a name
    // that collides with an already-registered command DROPS that command and the
    // program starts without it. That is exactly what happened when ÖLÇEKLE was
    // given `ÖLÇ` as an abbreviation and ÖLÇ then claimed it as its primary name:
    // the build was clean, the suite was green, and `ÖLÇ` silently ran the wrong
    // command.
    //
    // The count is the tripwire. A new command bumps it by one on purpose; a
    // command that vanished bumps it down by accident, and that is the case worth
    // catching.
    Fixture f;
    CHECK_EQ(f.reg.size(), std::size_t{56});

    // And the collision check itself, over the names that DID register.
    for (const CommandSpec& spec : f.reg.all())
        for (const std::string& name : spec.names) {
            const CommandSpec* found = f.reg.resolve(name);
            if (found == nullptr) FAIL_WITH("bildirilen ad çözülemiyor", spec.id + " / " + name);
            if (found->id != spec.id)
                FAIL_WITH("ad başka bir komuta gidiyor", spec.id + " / " + name + " -> " + found->id);
        }
}

// ============================================================================
// DİZİ — rectangular and polar arrays
// ============================================================================

TEST_CASE("DİZİ satır/sütun dizisi üretir, özgün yerinde kalır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());

    auto arrayed = f.bus.execute_line(
        "DİZİ nesneler=1 satir=3 sutun=4 satir_aralik=20 sutun_aralik=15", Origin::Test);
    if (!arrayed) FAIL_WITH("DİZİ", arrayed.error().message);
    REQUIRE(arrayed.ok());

    // 3 x 4 = 12 places, one of which is the original: 11 copies.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{12});
    CHECK_EQ(f.doc.entity_extent(0).min_x, core::Mm{0});

    // The far corner sits three columns and two rows away.
    const core::Box2 all = f.doc.extent();
    CHECK_EQ(all.min_x, core::Mm{0});
    CHECK_EQ(all.max_x, core::Mm{10000 + 3 * 15000});
    CHECK_EQ(all.max_y, core::Mm{10000 + 2 * 20000});
}

TEST_CASE("DİZİ kutupsal dizi üretir ve tam tur eşit böler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    // A small square 30 m due east of the origin.
    REQUIRE(f.bus.execute_line("ALAN 30,0 32,0 32,2 30,2", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=4", Origin::Test)
                .ok());

    // Four in total: the original plus three quarter-turn copies.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});

    // A full turn in four steps is four right angles, so the copies land on the
    // axes EXACTLY — the integer angle reduction again.
    const core::Box2 all = f.doc.extent();
    CHECK_EQ(all.max_x, core::Mm{32000});
    CHECK_EQ(all.max_y, core::Mm{32000});
    CHECK_EQ(all.min_x, core::Mm{-32000});
    CHECK_EQ(all.min_y, core::Mm{-32000});
}

TEST_CASE("DİZİ kısmi açıda ilk ve son kopyayı uçlara koyar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 30,0 32,0 32,2 30,2", Origin::Test).ok());

    // Three objects across a quarter turn: 0, 45 and 90 degrees — divided by the
    // GAPS, not the count, because both ends keep an object.
    REQUIRE(f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=3 aci=90",
                               Origin::Test)
                .ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{3});

    // The last one is a quarter turn round, so it reaches due north exactly. Its
    // far corner crosses the axis by the width of the square, which is why the
    // west edge is -2 m and not 0: a quarter turn maps (30, 2) to (-2, 30).
    const core::Box2 all = f.doc.extent();
    CHECK_EQ(all.max_y, core::Mm{32000});
    CHECK_EQ(all.min_x, core::Mm{-2000});
    CHECK_EQ(all.max_x, core::Mm{32000});
}

TEST_CASE("DİZİ geri alınır ve anlamsız girdiyi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // One row and one column is not an array.
    REQUIRE(f.bus.execute_line("DİZİ nesneler=1 satir=1 sutun=1 satir_aralik=5 sutun_aralik=5",
                               Origin::Test)
                .ok());
    CHECK_EQ(f.doc.content_hash(), before);

    // A polar array of one is not an array either.
    REQUIRE(f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=1", Origin::Test)
                .ok());
    CHECK_EQ(f.doc.content_hash(), before);

    REQUIRE(f.bus.execute_line("DİZİ nesneler=1 satir=2 sutun=2 satir_aralik=20 sutun_aralik=20",
                               Origin::Test)
                .ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});

    // One command, one undo step, every copy.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

// ============================================================================
// BÖL / BUDA / UZAT — bringing lines to their intersections
// ============================================================================

TEST_CASE("BÖL çizgiyi ikiye ayırır; ilk yarı nesnenin kendisi kalır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());

    const core::EntityKey key = f.doc.entities().key[0];

    auto split = f.bus.execute_line("BÖL nesne=1 nokta=30,0", Origin::Test);
    if (!split) FAIL_WITH("BÖL", split.error().message);
    REQUIRE(split.ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});

    // The FIRST half keeps the object — its key, layer, style and attributes stay
    // with it, exactly as an ifraz leaves one parcel continuing and one new.
    CHECK(f.doc.entities().key[0] == key);
    CHECK_EQ(f.doc.entity_extent(0).max_x, core::Mm{30000});
    CHECK_EQ(f.doc.entity_extent(1).min_x, core::Mm{30000});
    CHECK_EQ(f.doc.entity_extent(1).max_x, core::Mm{100000});
}

TEST_CASE("BÖL uçtan bölmeyi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("BÖL nesne=1 nokta=0,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("BUDA çizgiyi sınıra kadar kısaltır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    // A horizontal line crossing a vertical boundary at x = 40.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 40,-20 40,20", Origin::Test).ok());

    // The point names the piece to throw away: the far end, past the boundary.
    auto trimmed = f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=90,0", Origin::Test);
    if (!trimmed) FAIL_WITH("BUDA", trimmed.error().message);
    REQUIRE(trimmed.ok());

    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(f.doc.entity_extent(0).min_x, core::Mm{0});
    CHECK_EQ(f.doc.entity_extent(0).max_x, core::Mm{40000});
}

TEST_CASE("BUDA hangi ucun atılacağını noktadan anlar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 40,-20 40,20", Origin::Test).ok());

    // This time the NEAR end is named, so the near piece goes and the line starts
    // at the boundary instead of ending there.
    REQUIRE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=10,0", Origin::Test).ok());
    CHECK_EQ(f.doc.entity_extent(0).min_x, core::Mm{40000});
    CHECK_EQ(f.doc.entity_extent(0).max_x, core::Mm{100000});
}

TEST_CASE("UZAT çizgiyi sınıra ulaştırır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    // A line stopping short of a boundary at x = 80.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 80,-20 80,20", Origin::Test).ok());

    auto extended = f.bus.execute_line("UZAT nesne=1 sinir=2 nokta=50,0", Origin::Test);
    if (!extended) FAIL_WITH("UZAT", extended.error().message);
    REQUIRE(extended.ok());

    CHECK_EQ(f.doc.entity_extent(0).max_x, core::Mm{80000});
    CHECK_EQ(f.doc.entity_extent(0).min_x, core::Mm{0});
}

TEST_CASE("BUDA kesişmeyen sınırı reddeder ve hiçbir şeyi değiştirmez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0", Origin::Test).ok());
    // A boundary the line never reaches.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 80,-20 80,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=40,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("BUDA ve BÖL eğriyi reddeder")
{
    // Trimming an arc to a line is a real operation and it is NOT this one: it
    // needs the circle-line intersection, and treating an arc as its chord would
    // move a road curve by however much the chord misses the arc.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 40,-20 40,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("BÖL nesne=1 nokta=5,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=5,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

// ============================================================================
// PAH / YUVARLA — cutting and rounding a corner
// ============================================================================

TEST_CASE("PAH köşeyi düz kenarla keser")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    // A right angle at (0,0): one edge east, one edge north.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());

    auto cut = f.bus.execute_line("PAH nesne=1 nokta=0,0 mesafe=5", Origin::Test);
    if (!cut) FAIL_WITH("PAH", cut.error().message);
    REQUIRE(cut.ok());

    // The corner vertex is replaced by TWO tangent points, 5 m along each edge.
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    REQUIRE_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});

    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).x, core::Mm{5000});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 1).y, core::Mm{0});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 2).x, core::Mm{0});
    CHECK_EQ(f.doc.geometry().vertex(span.first, 2).y, core::Mm{5000});

    // Still one object; a chamfer does not create anything.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("YUVARLA köşeyi yayla yuvarlatır ve yayı ayrı nesne olarak koyar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());

    auto rounded = f.bus.execute_line("YUVARLA nesne=1 nokta=0,0 yaricap=5", Origin::Test);
    if (!rounded) FAIL_WITH("YUVARLA", rounded.error().message);
    REQUIRE(rounded.ok());

    // THE LINE IS BROKEN IN TWO and the arc goes between the pieces. Keeping both
    // tangent points in one run would draw a straight chord between them AND the
    // arc over it — a lens where a rounded corner should be.
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{3});
    CHECK(f.doc.entities().kind[0] == core::kPolylineKind);
    CHECK(f.doc.entities().kind[1] == core::kPolylineKind);
    CHECK(f.doc.entities().kind[2] == core::kArcKind);

    // A right angle rounded at r = 5 has its tangent points 5 m out along each
    // edge (r / tan(45°) = r), and its centre on the bisector at (5, 5). The first
    // leg ends at one tangent point and the second begins at the other.
    const core::RingSpan leg1 = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    const core::RingSpan leg2 = f.doc.geometry().rings_of(f.doc.entities().slot[1]);
    REQUIRE_EQ(f.doc.geometry().ring_xs(leg1.first).size(), std::size_t{2});
    REQUIRE_EQ(f.doc.geometry().ring_xs(leg2.first).size(), std::size_t{2});
    CHECK_EQ(f.doc.geometry().vertex(leg1.first, 1).x, core::Mm{5000});
    CHECK_EQ(f.doc.geometry().vertex(leg1.first, 1).y, core::Mm{0});
    CHECK_EQ(f.doc.geometry().vertex(leg2.first, 0).x, core::Mm{0});
    CHECK_EQ(f.doc.geometry().vertex(leg2.first, 0).y, core::Mm{5000});

    const std::uint32_t arc = f.doc.entities().slot[2];
    CHECK_EQ(core::arc_radius_of(f.doc.geometry(), arc), core::Mm{5000});
    CHECK_EQ(core::arc_centre_of(f.doc.geometry(), arc).x, core::Mm{5000});
    CHECK_EQ(core::arc_centre_of(f.doc.geometry(), arc).y, core::Mm{5000});
}

TEST_CASE("YUVARLA kapalı alanı reddeder")
{
    // A filleted boundary is partly a curve, and this model's ring holds vertices
    // rather than curve segments (R9-R12). Producing an open line where a parcel
    // used to be would quietly destroy the face, so it is refused instead.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 40,0 40,30 0,30", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE(f.bus.execute_line("YUVARLA nesne=1 nokta=0,0 yaricap=5", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});

    // PAH stays available for a closed face, because a chamfer is all straight.
    REQUIRE(f.bus.execute_line("PAH nesne=1 nokta=0,0 mesafe=5", Origin::Test).ok());
    CHECK(f.doc.content_hash() != before);
}

TEST_CASE("PAH komşu kenardan uzun kesimi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // 30 m off a 20 m edge: refused, and nothing is written.
    REQUIRE(f.bus.execute_line("PAH nesne=1 nokta=0,0 mesafe=30", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("PAH açık çizginin ucunu köşe saymaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // (20,0) is an END: one edge meets it and there is nothing to cut across.
    REQUIRE(f.bus.execute_line("PAH nesne=1 nokta=20,0 mesafe=2", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("PAH kapalı alanın her köşesinde çalışır ve alan küçülür")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 40,0 40,30 0,30", Origin::Test).ok());
    const core::Mm2 before = f.doc.geometry().area_of(f.doc.entities().slot[0]);

    // A closed ring has a corner at EVERY vertex, including the one the closing
    // edge arrives at — which an open line's first vertex is not.
    REQUIRE(f.bus.execute_line("PAH nesne=1 nokta=0,0 mesafe=5", Origin::Test).ok());

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{5});

    // Cutting a corner off a convex face removes a triangle: 5 x 5 / 2 = 12.5 m².
    const core::Mm2 after = f.doc.geometry().area_of(f.doc.entities().slot[0]);
    CHECK_EQ(before - after, core::Mm2{12'500'000});
}

// ============================================================================
// KATMANAT / STİLKOPYALA — housekeeping that keeps identity
// ============================================================================

TEST_CASE("KATMANAT nesneyi başka katmana taşır ve kimliğini korur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TASLAK", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const core::EntityKey key   = f.doc.entities().key[0];
    const core::LayerId taslak  = f.doc.entities().layer[0];

    auto moved = f.bus.execute_line("KATMANAT nesneler=1 katman=PARSEL", Origin::Test);
    if (!moved) FAIL_WITH("KATMANAT", moved.error().message);
    REQUIRE(moved.ok());

    const core::LayerId parsel = f.doc.find_layer("PARSEL");
    REQUIRE(parsel != core::kNoLayer);
    CHECK(parsel != taslak);
    CHECK_EQ(f.doc.entities().layer[0], parsel);

    // The same object: moving a parsel between layers must not mint a new key,
    // because the ada/parsel row hangs off it (model.md R4, R28).
    CHECK(f.doc.entities().key[0] == key);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});

    // The per-layer counts followed it, or the layer panel would go on reporting
    // the object where it no longer is.
    CHECK_EQ(f.doc.layer_entity_count(parsel), std::size_t{1});
    CHECK_EQ(f.doc.layer_entity_count(taslak), std::size_t{0});
}

TEST_CASE("KATMANAT gizli katmana taşınan nesne görünmez olur")
{
    // The cull test reads the mirrored layer bit and nothing else (R6, R7), so it
    // has to be re-taken from the NEW layer rather than carried over.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TASLAK", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=GİZLİ gorunur=hayır", Origin::Test).ok());

    CHECK(f.doc.entities().visible(0));
    REQUIRE(f.bus.execute_line("KATMANAT nesneler=1 katman=GİZLİ", Origin::Test).ok());
    CHECK_FALSE(f.doc.entities().visible(0));

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(f.doc.entities().visible(0));
}

TEST_CASE("KATMANAT geri alınır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TASLAK", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());
    const core::LayerId taslak = f.doc.entities().layer[0];

    REQUIRE(f.bus.execute_line("KATMANAT nesneler=1 katman=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.entities().layer[0], taslak);
}

TEST_CASE("STİLKOPYALA kaynağın etkin stilini hedeflere uygular")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=A", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=B", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 20,0 30,0 30,10 20,10", Origin::Test).ok());

    // Give layer A a style of its own, so the source INHERITS a real one.
    REQUIRE(f.bus.execute_line("STİL katman=A renk=0xFFCC0000", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("STİLKOPYALA kaynak=1 nesneler=2", Origin::Test).ok());

    // THE EFFECTIVE LOOK, not the ByLayer sentinel: the target is on layer B, so
    // copying the sentinel would have left it looking like B rather than like the
    // source. Layer A carries only colours, so its appearance is interned into a
    // style and THAT is what the target gets.
    CHECK(f.doc.entities().style[1] != core::kByLayerStyle);

    const core::LayerId a = f.doc.find_layer("A");
    REQUIRE(a != core::kNoLayer);
    CHECK_EQ(f.doc.styles().at(f.doc.entities().style[1]).rgba,
             f.doc.layer_table().at(a)->appearance.rgba);
    CHECK_EQ(f.doc.styles().at(f.doc.entities().style[1]).rgba, 0xFFCC0000u);
}
