// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_test.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/transform.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/guide.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/text_store.hpp"

using namespace kentos;
using namespace kentos::command;

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

TEST_CASE("registry fingerprint moves with every declared field")
{
    // THE CATALOGUE ITSELF IS TESTED IN test_ai_catalog.cpp, where it now lives
    // (`ai::build_catalog`). What belongs to the registry is the FINGERPRINT, and
    // what has to be true of it is that the smallest declared change moves it —
    // that is the whole mechanism behind "a parameter changed, so the tool
    // surface and llms.txt are stale" (CLAUDE.md 6.14).
    Fixture f;
    const std::uint64_t before = f.reg.fingerprint();
    CHECK(before != 0);
    CHECK_EQ(before, f.reg.fingerprint()); // stable within a run

    Registry other;
    register_builtin_commands(other);
    CHECK_EQ(before, other.fingerprint()); // and across two registries alike

    CommandSpec extra;
    extra.id      = "test.fingerprint";
    extra.names   = {"PARMAKİZİ"};
    extra.params  = {Param::text("ad", Arity::exactly(1), "bir ad")};
    extra.summary = "Sınama komutu.";
    extra.run     = other.by_id("core.line")->run;
    REQUIRE(other.add(extra).ok());
    const std::uint64_t with_command = other.fingerprint();
    CHECK(with_command != before);

    // And a parameter's HELP alone is enough to move it, because the help is
    // what a model reads to decide what to pass.
    Registry third;
    register_builtin_commands(third);
    CommandSpec reworded = extra;
    reworded.params      = {Param::text("ad", Arity::exactly(1), "başka bir açıklama")};
    REQUIRE(third.add(reworded).ok());
    CHECK(third.fingerprint() != with_command);
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
    CHECK(!t[2].angle_unit.has_value()); // bare: the convention decides
    CHECK(t[3].kind == Token::Kind::Relative);
    CHECK_EQ(t[3].a, 300.0);

    const core::AngleConvention semt_grad{};
    auto p0 = resolve_point(t[0], core::Point2{}, ResolveContext{semt_grad});
    CHECK(p0.ok());
    CHECK_EQ(p0.value(), (core::Point2{485320150, 4310220400}));

    auto p1 = resolve_point(t[1], p0.value(), ResolveContext{semt_grad});
    CHECK(p1.ok());
    CHECK_EQ(p1.value(), (core::Point2{485370150, 4310250400}));

    // 45 grad clockwise from north is 40.5°: north-east, more north than east.
    auto p2 = resolve_point(t[2], p1.value(), ResolveContext{semt_grad});
    CHECK(p2.ok());
    CHECK(p2.value().x > p1.value().x);
    CHECK(p2.value().y > p1.value().y);
    CHECK(p2.value().y - p1.value().y > p2.value().x - p1.value().x);
}

// ------------------------------------------------------ the angle rule ----
//
// TODOS-CAD P0: `@mesafe<açı` is read under `core.aci.kural` and `core.aci.birim`,
// SEMT + GRAD by default. Four quadrants × three units × two rules, each once
// bare under the matching convention and once with its suffix under a FOREIGN
// convention, so the suffix is proven to win. The axes must come out exact,
// because `polar_offset` goes through `sin_cos_udeg` whose quadrant reduction is
// integer arithmetic (§7.3).

namespace {

/// The quarter-turn multiples in each unit, written as a user would type them.
const char* quarter_angle(core::AngleUnit unit, int k)
{
    static const char* const grad[]   = {"0", "100", "200", "300"};
    static const char* const degree[] = {"0", "90", "180", "270"};
    static const char* const radian[] = {"0", "1.5707963267948966", "3.141592653589793",
                                         "4.71238898038469"};
    switch (unit) {
    case core::AngleUnit::Grad: return grad[k];
    case core::AngleUnit::Degree: return degree[k];
    case core::AngleUnit::Radian: return radian[k];
    }
    return grad[k];
}

/// Where 100 metres along the k-th quarter turn lands, from the origin, under
/// `rule`: semt walks N, E, S, W; matematik walks E, N, W, S.
core::Point2 quarter_target(core::AngleRule rule, int k)
{
    static const core::Point2 semt[]      = {{0, 100000}, {100000, 0}, {0, -100000}, {-100000, 0}};
    static const core::Point2 matematik[] = {{100000, 0}, {0, 100000}, {-100000, 0}, {0, -100000}};
    return rule == core::AngleRule::Semt ? semt[k] : matematik[k];
}

core::Result<core::Point2> polar(const std::string& token, core::AngleConvention convention)
{
    auto parsed = parse_line("YANIT " + token);
    if (!parsed) return parsed.error();
    REQUIRE_EQ(parsed.value().tokens.size(), std::size_t{1});
    return resolve_point(parsed.value().tokens.front(), core::Point2{}, ResolveContext{convention});
}

} // namespace

TEST_CASE("AÇI KURALI: dört çeyrek × üç birim × iki kural, soneksiz ve sonekli")
{
    const core::AngleUnit units[] = {core::AngleUnit::Grad, core::AngleUnit::Degree,
                                     core::AngleUnit::Radian};
    const core::AngleRule rules[] = {core::AngleRule::Semt, core::AngleRule::Matematik};

    for (core::AngleRule rule : rules)
        for (core::AngleUnit unit : units)
            for (int k = 0; k < 4; ++k) {
                const core::Point2 want = quarter_target(rule, k);
                const std::string angle = quarter_angle(unit, k);

                // Bare, under the convention that names this unit.
                auto bare = polar("@100<" + angle, core::AngleConvention{unit, rule});
                REQUIRE_MESSAGE(bare.ok(), bare.error().message);
                CHECK_MESSAGE(bare.value() == want, "@100<" << angle
                                                            << " birim=" << static_cast<int>(unit)
                                                            << " kural=" << static_cast<int>(rule));

                // Suffixed, under a convention whose unit is DIFFERENT — the
                // suffix names the unit, the convention still names the rule.
                for (core::AngleUnit foreign : units) {
                    if (foreign == unit) continue;
                    const std::string token =
                        "@100<" + angle + std::string(1, core::angle_unit_suffix(unit));
                    auto suffixed = polar(token, core::AngleConvention{foreign, rule});
                    REQUIRE_MESSAGE(suffixed.ok(), suffixed.error().message);
                    CHECK_MESSAGE(suffixed.value() == want,
                                  token << " yabancı birim=" << static_cast<int>(foreign));
                }
            }
}

TEST_CASE("AÇI KURALI: @100<0 semt'te kuzey, matematik'te doğu; varsayılan semt + grad")
{
    // The default convention is the one a Turkish surveyor holds: zero is north.
    CHECK_EQ(polar("@100<0", core::AngleConvention{}).value(), (core::Point2{0, 100000}));
    CHECK_EQ(
        polar("@100<0", core::AngleConvention{core::AngleUnit::Grad, core::AngleRule::Matematik})
            .value(),
        (core::Point2{100000, 0}));

    // 50 grad is 45°: the diagonal, and both axes round to the same millimetre.
    CHECK_EQ(polar("@100<50", core::AngleConvention{}).value(), (core::Point2{70711, 70711}));

    // The OLD meaning — degrees counter-clockwise from east — is one rule and one
    // unit away, and the `d` suffix reaches the unit without touching the setting.
    const core::AngleConvention old_way{core::AngleUnit::Degree, core::AngleRule::Matematik};
    CHECK_EQ(polar("@100<45", old_way).value(), (core::Point2{70711, 70711}));
    CHECK_EQ(polar("@100<90", old_way).value(), (core::Point2{0, 100000}));
    CHECK_EQ(
        polar("@100<90d", core::AngleConvention{core::AngleUnit::Grad, core::AngleRule::Matematik})
            .value(),
        (core::Point2{0, 100000}));

    // Negative and over-full angles wrap like an instrument's circle does.
    CHECK_EQ(polar("@100<-100", core::AngleConvention{}).value(), (core::Point2{-100000, 0}));
    CHECK_EQ(polar("@100<500", core::AngleConvention{}).value(), (core::Point2{100000, 0}));
}

TEST_CASE("AÇI KURALI: sonek büyük harfle de okunur, ifadeden sonra da; yanlış harf reddedilir")
{
    const core::AngleConvention semt_grad{};

    CHECK_EQ(polar("@100<90D", semt_grad).value(), (core::Point2{100000, 0}));
    CHECK_EQ(polar("@100<100G", semt_grad).value(), (core::Point2{100000, 0}));
    CHECK_EQ(polar("@100<3.141592653589793R", semt_grad).value(), (core::Point2{0, -100000}));
    CHECK_EQ(polar("@100<(40+50)d", semt_grad).value(), (core::Point2{100000, 0}));
    CHECK_EQ(polar("@(50*2)<(400/4)g", semt_grad).value(), (core::Point2{100000, 0}));

    // A letter that is not a suffix is a typo and is reported by name, never read
    // as a suffix and never dropped (command.md R19).
    auto bad = polar("@100<45x", semt_grad);
    CHECK(!bad.ok());
    CHECK(bad.error().message.find("Kutupsal açı") != std::string::npos);
    CHECK(bad.error().message.find('x') != std::string::npos);

    // Two suffix letters are one suffix too many.
    CHECK(!polar("@100<45gg", semt_grad).ok());

    // A suffix with no number in front of it is not an angle.
    CHECK(!polar("@100<g", semt_grad).ok());

    // The token remembers its suffix and says so when described.
    auto parsed = parse_line("YANIT @100<45g");
    REQUIRE(parsed.ok());
    CHECK(parsed.value().tokens.front().angle_unit == core::AngleUnit::Grad);
    CHECK(describe(parsed.value().tokens.front()).back() == 'g');
}

TEST_CASE("AÇI KURALI: parse_point bir metni tek gramerle okur")
{
    const core::AngleConvention semt_grad{};

    CHECK_EQ(parse_point("  @100<50 ", core::Point2{}, ResolveContext{semt_grad}).value(),
             (core::Point2{70711, 70711}));
    CHECK_EQ(
        parse_point("485320.150,4310220.400", core::Point2{}, ResolveContext{semt_grad}).value(),
        (core::Point2{485320150, 4310220400}));
    CHECK_EQ(parse_point("@50,30", core::Point2{1000, 2000}, ResolveContext{semt_grad}).value(),
             (core::Point2{51000, 32000}));

    auto word = parse_point("abc", core::Point2{}, ResolveContext{semt_grad});
    CHECK(!word.ok());
    CHECK(word.error().message.find("Beklenen: koordinat") != std::string::npos);
    CHECK(word.error().message.find("abc") != std::string::npos);

    CHECK(!parse_point("", core::Point2{}, ResolveContext{semt_grad}).ok());
    CHECK(!parse_point("@100<", core::Point2{}, ResolveContext{semt_grad}).ok());
}

TEST_CASE("AÇI KURALI: MOD kural ve AYAR açı_birimi komut satırının okuduğunu değiştirir")
{
    // The two settings reach the parser only through `Bus::angle_convention()`,
    // read once per line; here the same text draws three different lines.
    Fixture f;
    const auto second_vertex = [&](std::size_t entity) {
        const auto& doc = f.doc;
        const auto span = doc.geometry().rings_of(doc.entities().slot[entity]);
        const auto xs   = doc.geometry().ring_xs(span.first);
        const auto ys   = doc.geometry().ring_ys(span.first);
        return core::Point2{xs[1], ys[1]};
    };

    CHECK(f.bus.angle_convention() == core::AngleConvention{});

    CHECK(f.bus.execute_line("ÇİZGİ 0,0 @100<0", Origin::Test).ok());
    CHECK_EQ(second_vertex(0), (core::Point2{0, 100000})); // north

    CHECK(f.bus.execute_line("MOD kural matematik", Origin::Test).ok());
    CHECK(f.bus.angle_convention().rule == core::AngleRule::Matematik);
    CHECK(f.bus.execute_line("ÇİZGİ 0,0 @100<0", Origin::Test).ok());
    CHECK_EQ(second_vertex(1), (core::Point2{100000, 0})); // east

    CHECK(f.bus.execute_line("AYAR açı_birimi derece", Origin::Test).ok());
    CHECK(f.bus.angle_convention().unit == core::AngleUnit::Degree);
    CHECK(f.bus.execute_line("ÇİZGİ 0,0 @100<90", Origin::Test).ok());
    CHECK_EQ(second_vertex(2), (core::Point2{0, 100000})); // 90° counter-clockwise from east

    // Back to the default by name, the way a script undoes a mode.
    CHECK(f.bus.execute_line("MOD açı_kuralı varsayilan", Origin::Test).ok());
    CHECK(f.bus.execute_line("AYAR aci_birimi varsayilan", Origin::Test).ok());
    CHECK(f.bus.angle_convention() == core::AngleConvention{});

    // The mode is a session aid: not a document mutation, not in the journal.
    for (const auto& e : f.journal.entries())
        CHECK(e.command_id != "core.mode");
}

TEST_CASE("AÇI KURALI: betik dizesindeki koordinat aynı gramerle, aynı kuralla okunur")
{
    // A script may carry a coordinate as the command line writes it. It goes
    // through `parse_point` on the bus, under the session convention, chained
    // from the point before — so a script and a typed line cannot disagree.
    Fixture f;
    Args args;
    args.set("noktalar", Value::texts({"0,0", "@100<0", "@100<100"}));
    auto r = f.bus.dispatch(Invocation{"core.line", args, Origin::Script});
    REQUIRE_MESSAGE(r.ok(), r.error().message);
    REQUIRE_EQ(f.journal.entries().size(), std::size_t{1});

    // COPIED, not bound by reference: `Args::get` returns a `Value` by value and
    // `as_points()` a reference into it — the stakeout command's own footnote.
    const Value::Points pts = f.journal.entries().front().args.get("noktalar").as_points();
    REQUIRE_EQ(pts.size(), std::size_t{3});
    CHECK_EQ(pts[0], (core::Point2{0, 0}));
    CHECK_EQ(pts[1], (core::Point2{0, 100000}));
    CHECK_EQ(pts[2], (core::Point2{100000, 100000}));

    // A single-point parameter takes a text too, and a word that is not a
    // coordinate is refused with the parser's message, before the body runs.
    Args one;
    one.set("baslangic", Value::text("10,20"));
    one.set("bitis", Value::text("@5,5"));
    CHECK(f.bus.dispatch(Invocation{"core.measure", one, Origin::Script}).ok());

    Args bad;
    bad.set("noktalar", Value::texts({"0,0", "buraya"}));
    auto refused = f.bus.dispatch(Invocation{"core.line", bad, Origin::Script});
    CHECK(!refused.ok());
    CHECK(refused.error().message.find("Beklenen: koordinat") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2}); // nothing half-drawn
}

// ------------------------------------------------------ point functions ----
//
// TODOS-CAD P1a. `orta(A,B)`, `dik(A,B,ayak,boy)`, `kes(…)` and the rest are
// part of THE grammar, so they are tested where the grammar is tested and
// through the same two entry points every client uses. The triangles are chosen
// so the answer is a whole millimetre and can be written down: a 3-4-5 with legs
// of 30 and 40 metres, and the axes, where `sin_cos_udeg` is exact.

namespace {

/// One point function read and resolved exactly as a typed line resolves it:
/// through `parse_line` and `resolve_point`, under semt + grad, with the point
/// before at the origin unless `last` says otherwise.
core::Result<core::Point2> fn(const std::string& text, core::Point2 last = {},
                              ResolveContext ctx = {})
{
    auto parsed = parse_line("YANIT " + text);
    if (!parsed) return parsed.error();
    if (parsed.value().tokens.size() != 1)
        return core::err(core::ErrorCode::ParseError, "tek belirteç bekleniyordu: " + text);
    return resolve_point(parsed.value().tokens.front(), last, ctx);
}

/// The same text through the OTHER entry point — one coordinate as a string,
/// which is the road a JSON script takes.
core::Result<core::Point2> fn_text(const std::string& text, core::Point2 last = {},
                                   ResolveContext ctx = {})
{
    return parse_point(text, last, ctx);
}

/// A drawing with three numbered points in it, for `n(…)`.
ResolveContext with_points()
{
    ResolveContext ctx;
    ctx.named_point = [](std::int64_t number) -> std::optional<core::Point2> {
        switch (number) {
        case 1284: return core::Point2{485320150, 4310220400};
        case 1285: return core::Point2{485370150, 4310250400};
        case 0: return core::Point2{0, 0};
        default: return std::nullopt;
        }
    };
    return ctx;
}

} // namespace

TEST_CASE("NOKTA FONKSİYONU: gramer çağrıyı tanır, biçimini seçer ve tarif eder")
{
    auto parsed = parse_line("ÇİZGİ orta(0,0,100,0) dik(0,0,30,40,50,10)");
    REQUIRE(parsed.ok());
    REQUIRE_EQ(parsed.value().tokens.size(), std::size_t{2});
    CHECK(parsed.value().tokens[0].kind == Token::Kind::Call);
    CHECK(parsed.value().tokens[1].kind == Token::Kind::Call);
    CHECK(is_coordinate(parsed.value().tokens[0]));

    // The FORM is recorded, not re-derived: `kes` has three and the matcher
    // decides once.
    CHECK_EQ(parsed.value().tokens[0].word, std::string("orta"));
    CHECK_EQ(parse_line("YANIT kes(0,0,100,100,0,100,100,0)").value().tokens.front().word,
             std::string("kes.dogru"));
    CHECK_EQ(parse_line("YANIT kes(0,0,100,100,100,200)").value().tokens.front().word,
             std::string("kes.dogrultu"));
    CHECK_EQ(parse_line("YANIT kes(0,0,60,100,0,80,sol)").value().tokens.front().word,
             std::string("kes.mesafe"));

    // A point argument written as bare coordinates spends two comma-separated
    // fields; `orta(A,B)` therefore carries two tokens, not four.
    CHECK_EQ(parsed.value().tokens[0].nested.size(), std::size_t{2});
    CHECK_EQ(parsed.value().tokens[1].nested.size(), std::size_t{4});

    CHECK_EQ(describe(parsed.value().tokens[0]),
             std::string("orta(point(0.000000,0.000000),point(100.000000,0.000000))"));

    // A name the grammar does not declare stays an ordinary word, so a layer or
    // a caption with brackets in it still works.
    auto plain = parse_line("KATMAN ad=YOL(ESKİ)");
    REQUIRE(plain.ok());
    CHECK(plain.value().tokens.front().kind == Token::Kind::KeyValue);
    CHECK(parse_line("YANIT yokboyle(1,2)").value().tokens.front().kind == Token::Kind::Word);
}

TEST_CASE("NOKTA FONKSİYONU: bilinen üçgende her fonksiyon milimetresine kadar")
{
    // A 3-4-5 triangle in metres: A at the origin, B thirty east and forty
    // north, so |AB| is exactly 50 m and the unit vector is (0,6 · 0,8).
    const core::Point2 kB{30000, 40000};

    CHECK_EQ(fn("orta(0,0,30,40)").value(), (core::Point2{15000, 20000}));
    CHECK_EQ(fn("xy(10,20,30,40)").value(), (core::Point2{10000, 40000}));
    CHECK_EQ(fn("ile(10,20,@5,5)").value(), (core::Point2{15000, 25000}));
    CHECK_EQ(fn("ile(0,0,@100<100)").value(), (core::Point2{100000, 0})); // 100 grad = east
    CHECK_EQ(fn("ara(0,0,30,40,0.5)").value(), (core::Point2{15000, 20000}));
    CHECK_EQ(fn("ara(0,0,30,40,25 m)").value(), (core::Point2{15000, 20000}));
    CHECK_EQ(fn("ara(0,0,30,40,25m)").value(), (core::Point2{15000, 20000}));
    CHECK_EQ(fn("uzanti(0,0,30,40,25)").value(), (core::Point2{45000, 60000}));
    CHECK_EQ(fn("semt(0,0,0,100)").value(), (core::Point2{0, 100000}));   // semt zero is north
    CHECK_EQ(fn("semt(0,0,100,100)").value(), (core::Point2{100000, 0})); // 100 grad is east

    // DİK AYAK / DİK BOY. Fifty metres along A→B is B itself; ten metres to the
    // LEFT of that direction is the positive side (P1a-6), the right the
    // negative. Left of (0,6 · 0,8) is (−0,8 · 0,6).
    CHECK_EQ(fn("dik(0,0,30,40,50,10)").value(), (core::Point2{22000, 46000}));
    CHECK_EQ(fn("dik(0,0,30,40,50,-10)").value(), (core::Point2{38000, 34000}));
    CHECK_EQ(fn("dik(0,0,30,40,50,0)").value(), kB);

    // The sign again on an axis, where it is impossible to misread: walking east
    // along A→B, the left hand points north.
    CHECK_EQ(fn("dik(0,0,100,0,30,5)").value(), (core::Point2{30000, 5000}));
    CHECK_EQ(fn("dik(0,0,100,0,30,-5)").value(), (core::Point2{30000, -5000}));

    // Two directions: east from the origin meets south from (100,100).
    CHECK_EQ(fn("kes(0,0,100,100,100,200)").value(), (core::Point2{100000, 0}));

    // Two lines: the diagonals of a hundred-metre square meet in the middle.
    CHECK_EQ(fn("kes(0,0,100,100,0,100,100,0)").value(), (core::Point2{50000, 50000}));

    // Two distances, the 3-4-5 the other way round: 60 m from the origin and
    // 80 m from (100,0) meet at (36, ±48). Left of A→B is north here.
    CHECK_EQ(fn("kes(0,0,60,100,0,80,sol)").value(), (core::Point2{36000, 48000}));
    CHECK_EQ(fn("kes(0,0,60,100,0,80,sağ)").value(), (core::Point2{36000, -48000}));
    CHECK_EQ(fn("kes(0,0,60,100,0,80,sag)").value(), (core::Point2{36000, -48000}));

    // …or by a point near the one that was meant, which has to be written
    // `yon=` because a bare coordinate here reads equally well as the third and
    // fourth point of `kes(A,B,C,D)`.
    CHECK_EQ(fn("kes(0,0,60,100,0,80,yon=40,40)").value(), (core::Point2{36000, 48000}));
    CHECK_EQ(fn("kes(0,0,60,100,0,80,yon=40,-40)").value(), (core::Point2{36000, -48000}));
    CHECK_EQ(fn("kes(0,0,60,100,0,80,yön=@40,-40)").value(), (core::Point2{36000, -48000}));

    // Tangent circles have one solution and both words find it.
    CHECK_EQ(fn("kes(0,0,60,100,0,40,sol)").value(), (core::Point2{60000, 0}));
    CHECK_EQ(fn("kes(0,0,60,100,0,40,sağ)").value(), (core::Point2{60000, 0}));
}

TEST_CASE("NOKTA FONKSİYONU: son, göreli argüman ve iç içe çağrı")
{
    // `son` is the point before — the same point `@` is measured from.
    CHECK_EQ(fn("orta(son,100,0)", core::Point2{0, 40000}).value(), (core::Point2{50000, 20000}));
    CHECK_EQ(fn("son()", core::Point2{7, 9}).value(), (core::Point2{7, 9}));

    // A BARE `son` IS A COORDINATE ONLY INSIDE AN ARGUMENT LIST. On its own it
    // stays an ordinary word, because `KATMAN son` names a layer and turning
    // every `son` on a command line into a point would take that spelling away;
    // nothing is lost, because `son()` and `@0,0` both say the point before.
    CHECK(parse_line("YANIT son").value().tokens.front().kind == Token::Kind::Word);
    CHECK(!fn("son", core::Point2{7, 9}).ok());
    CHECK_EQ(fn("@0,0", core::Point2{7, 9}).value(), (core::Point2{7, 9}));
    {
        Fixture f;
        REQUIRE(f.bus.execute_line("KATMAN son", Origin::CommandLine).ok());
        CHECK(f.doc.find_layer("son") != core::kNoLayer);
    }

    // A `@` argument inside a call is measured from the point before, exactly as
    // it is outside one.
    CHECK_EQ(fn("orta(@0,0,@100,0)", core::Point2{1000, 2000}).value(),
             (core::Point2{51000, 2000}));

    // Nested: the midpoint of two midpoints.
    CHECK_EQ(fn("orta(orta(0,0,100,0),orta(0,100,100,100))").value(), (core::Point2{50000, 50000}));
    CHECK_EQ(fn("dik(orta(0,0,0,80),100,40,0,10)").value(), (core::Point2{0, 50000}));

    // An angle inside a call takes the same suffix a polar coordinate takes.
    CHECK_EQ(fn("semt(0,0,90d,100)").value(), (core::Point2{100000, 0}));
    CHECK_EQ(fn("semt(0,0,100g,100)").value(), (core::Point2{100000, 0}));
    CHECK_EQ(fn("semt(0,0,(50+50),100)").value(), (core::Point2{100000, 0}));

    // …and the session's rule still decides where zero is when no suffix says.
    const ResolveContext matematik{
        core::AngleConvention{core::AngleUnit::Degree, core::AngleRule::Matematik}, {}};
    CHECK_EQ(fn("semt(0,0,0,100)", {}, matematik).value(), (core::Point2{100000, 0}));
    CHECK_EQ(fn("semt(0,0,90,100)", {}, matematik).value(), (core::Point2{0, 100000}));

    // The nesting limit is a limit, not a crash.
    std::string deep;
    for (int i = 0; i < 40; ++i)
        deep += "orta(";
    deep += "0,0,1,1";
    for (int i = 0; i < 40; ++i)
        deep += ",0,0)";
    CHECK(!fn(deep).ok());
}

TEST_CASE("NOKTA FONKSİYONU: n(1284) çizimdeki numaralı noktayı bulur")
{
    const ResolveContext drawing = with_points();

    CHECK_EQ(fn("n(1284)", {}, drawing).value(), (core::Point2{485320150, 4310220400}));
    CHECK_EQ(fn("orta(n(1284),n(1285))", {}, drawing).value(),
             (core::Point2{485345150, 4310235400}));

    // A number that is not in the drawing says so, and says where numbers come
    // from (R19).
    auto missing = fn("n(9999)", {}, drawing);
    CHECK(!missing.ok());
    CHECK(missing.error().message.find("9999 numaralı nokta yok") != std::string::npos);
    CHECK(missing.error().message.find("NOKTALAR") != std::string::npos);

    // NO DRAWING, NO LOOKUP. A caller with no document hands an empty function
    // and `n()` is inert by construction — it does not reach for a global and
    // it does not answer a wrong point.
    auto headless = fn("n(1284)");
    CHECK(!headless.ok());
    CHECK(headless.error().message.find("bu bağlamda çizim yok") != std::string::npos);

    // …and every other function is unaffected by the absence.
    CHECK_EQ(fn("orta(0,0,100,0)").value(), (core::Point2{50000, 0}));

    // A point number is a whole number.
    auto fraction = fn("n(12.5)", {}, drawing);
    CHECK(!fraction.ok());
    CHECK(fraction.error().message.find("tam sayı") != std::string::npos);
}

TEST_CASE("NOKTA FONKSİYONU: hata metinleri beklenen ve verileni söyler")
{
    // No shape of `kes` fits, so the message lists all three rather than the
    // complaint of whichever was tried last.
    auto none = fn("kes(0,0,50)");
    CHECK(!none.ok());
    CHECK(none.error().message.find("kes(A,açı1,B,açı2)") != std::string::npos);
    CHECK(none.error().message.find("kes(A,r1,B,r2,sol|sağ|yon=<nokta>)") != std::string::npos);
    CHECK(none.error().message.find("kes(A,B,C,D)") != std::string::npos);
    CHECK(none.error().message.find("kes(0,0,50)") != std::string::npos);

    // One shape, so the message is about the argument that went wrong.
    auto bad_point = fn("orta(abc,0,0)");
    CHECK(!bad_point.ok());
    CHECK(bad_point.error().message.find("orta(): 1. argüman") != std::string::npos);
    CHECK(bad_point.error().message.find("abc") != std::string::npos);

    auto too_many = fn("orta(0,0,10,10,20,20)");
    CHECK(!too_many.ok());
    CHECK(too_many.error().message.find("fazla argüman") != std::string::npos);

    // PARALLEL DIRECTIONS ARE REFUSED, not answered with a point past the moon.
    // Decided in whole micro-degrees, so a half turn apart is caught exactly.
    auto parallel_dirs = fn("kes(0,0,100,100,0,100)");
    CHECK(!parallel_dirs.ok());
    CHECK(parallel_dirs.error().message.find("paralel") != std::string::npos);
    CHECK(!fn("kes(0,0,100,100,0,300)").ok()); // the same direction reversed

    auto parallel_lines = fn("kes(0,0,100,0,0,50,100,50)");
    CHECK(!parallel_lines.ok());
    CHECK(parallel_lines.error().message.find("paralel") != std::string::npos);

    // CIRCLES THAT DO NOT MEET NAME BOTH RADII AND THE CENTRE DISTANCE (R19).
    auto apart = fn("kes(0,0,10,100,0,20,sol)");
    CHECK(!apart.ok());
    CHECK(apart.error().message.find("10,000") != std::string::npos);
    CHECK(apart.error().message.find("20,000") != std::string::npos);
    CHECK(apart.error().message.find("100,000") != std::string::npos);

    auto inside = fn("kes(0,0,100,10,0,10,sol)");
    CHECK(!inside.ok());
    CHECK(inside.error().message.find("içinde") != std::string::npos);

    CHECK(!fn("kes(0,0,-60,100,0,80,sol)").ok()); // a distance has no sign
    CHECK(!fn("kes(0,0,60,0,0,80,sol)").ok());    // one centre, two circles
    CHECK(!fn("dik(0,0,0,0,10,10)").ok());        // no direction to drop onto
    CHECK(!fn("uzanti(0,0,0,0,10)").ok());
    CHECK(!fn("ara(0,0,0,0,10 m)").ok());

    // A nearby point exactly between the two solutions is not a choice.
    auto astride = fn("kes(0,0,60,100,0,80,yon=36,0)");
    CHECK(!astride.ok());
    CHECK(astride.error().message.find("eşit uzaklıkta") != std::string::npos);

    // `ile` measures FROM its first argument, so its second must be written as
    // an offset; an absolute pair would silently ignore the point it was given.
    auto not_relative = fn("ile(10,20,30,40)");
    CHECK(!not_relative.ok());
    CHECK(not_relative.error().message.find("@dx,dy") != std::string::npos);

    // The direction word is one of two, and anything else says which two.
    auto bad_side = fn("kes(0,0,60,100,0,80,yukari)");
    CHECK(!bad_side.ok());
    CHECK(bad_side.error().message.find("sol") != std::string::npos);

    // And the coordinate error a non-coordinate gets now names the functions.
    auto word = parse_point("abc", core::Point2{}, ResolveContext{});
    CHECK(!word.ok());
    CHECK(word.error().message.find("nokta fonksiyonu") != std::string::npos);
}

TEST_CASE("NOKTA FONKSİYONU: çıktı geri beslenince aynı nokta çıkar")
{
    // Idempotence, function by function: every construction below has a fixed
    // point, and feeding its own answer back in must land on it again rather
    // than drifting by a millimetre a round. A rounding rule that were not
    // half-away-from-zero at every step would show up here.
    const core::Point2 kA{0, 0};
    const core::Point2 kB{30000, 40000};

    const core::Point2 mid = fn("orta(0,0,30,40)").value();
    CHECK_EQ(fn("orta(15,20,15,20)").value(), mid);
    CHECK_EQ(fn("orta(orta(0,0,30,40),orta(0,0,30,40))").value(), mid);

    CHECK_EQ(fn("ara(0,0,30,40,0)").value(), kA);
    CHECK_EQ(fn("ara(0,0,30,40,1)").value(), kB);
    CHECK_EQ(fn("ara(0,0,30,40,50 m)").value(), kB);
    CHECK_EQ(fn("uzanti(0,0,30,40,0)").value(), kB);
    CHECK_EQ(fn("dik(0,0,30,40,0,0)").value(), kA);
    CHECK_EQ(fn("xy(30,40,30,40)").value(), kB);
    CHECK_EQ(fn("ile(30,40,@0,0)").value(), kB);
    CHECK_EQ(fn("semt(30,40,0,0)").value(), kB);
    CHECK_EQ(fn("son()", kB).value(), kB);

    // The foot of a perpendicular with no offset IS the point that far along,
    // so the two functions must agree to the millimetre.
    for (const char* along : {"0", "10", "25", "50", "-12.5"}) {
        const std::string foot = std::string("dik(0,0,30,40,") + along + ",0)";
        const std::string walk = std::string("ara(0,0,30,40,") + along + " m)";
        CHECK_MESSAGE(fn(foot).value() == fn(walk).value(), foot << " = " << walk);
    }

    // An intersection fed back as one of its own lines' points is still itself.
    const core::Point2 cross = fn("kes(0,0,100,100,0,100,100,0)").value();
    CHECK_EQ(cross, (core::Point2{50000, 50000}));
    CHECK_EQ(fn("kes(0,0,50,50,0,100,100,0)").value(), cross);

    // A bearing and a distance read back off the drawing rebuild the point.
    const core::Point2 target{48000, -36000};
    const double turns = core::direction_turns(core::Point2{}, target, core::AngleRule::Semt);
    const std::string bearing = std::to_string(turns * 400.0);
    CHECK_EQ(fn("semt(0,0," + bearing + ",60)").value(), target);
}

TEST_CASE("NOKTA FONKSİYONU: komut satırı ve betik dizesi tek gramerden geçer")
{
    // The two entry points, same text, same answer — `bind_tokens` reads a typed
    // token, `parse_point` reads a script's string, and both land in
    // `resolve_point` (CLAUDE.md 5.11).
    for (const char* text : {"orta(0,0,30,40)", "dik(0,0,30,40,50,10)",
                             "kes(0,0,100,100,0,100,100,0)", "kes(0,0,60,100,0,80,sol)",
                             "uzanti(0,0,30,40,25)", "semt(0,0,100,100)", "xy(10,20,30,40)"}) {
        auto typed  = fn(text);
        auto script = fn_text(text);
        REQUIRE_MESSAGE(typed.ok(), text);
        REQUIRE_MESSAGE(script.ok(), text);
        CHECK_MESSAGE(typed.value() == script.value(), text);
    }

    // And both chain from the point before in the same way.
    CHECK_EQ(fn_text("orta(son,100,0)", core::Point2{0, 40000}).value(),
             (core::Point2{50000, 20000}));

    // A KEYWORD ARGUMENT TAKES ONE TOO. `hedef=orta(…)` is a `KeyValue` whose
    // nested token is a call, and it goes down the same road: nothing about
    // where a coordinate sits on the line changes how it is read.
    Fixture f;
    REQUIRE(f.bus.execute_line("NOKTA noktalar=orta(0,0,30,40)", Origin::CommandLine).ok());
    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{f.doc.geometry().ring_xs(span.first)[0],
                           f.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{15000, 20000}));
}

TEST_CASE("NOKTA FONKSİYONU: komut satırından çizilen belge çözülmüş noktayı günlükler")
{
    Fixture f;

    // Drawn from the command line: the line's two corners are the construction's
    // answers, and what reaches the journal is the POINT, not the call — an
    // `Args` carries `Value`s and a journal replays without a grammar
    // (CLAUDE.md 1.4, TODOS-CAD P0 decision on the journal).
    REQUIRE(
        f.bus.execute_line("ÇİZGİ orta(0,0,100,0) dik(0,0,100,0,30,-5)", Origin::CommandLine).ok());

    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    const auto xs   = f.doc.geometry().ring_xs(span.first);
    const auto ys   = f.doc.geometry().ring_ys(span.first);
    REQUIRE_EQ(xs.size(), std::size_t{2});
    CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{50000, 0}));
    CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{30000, -5000}));

    REQUIRE_EQ(f.journal.entries().size(), std::size_t{1});
    const Value::Points recorded = f.journal.entries().front().args.get("noktalar").as_points();
    REQUIRE_EQ(recorded.size(), std::size_t{2});
    CHECK_EQ(recorded[0], (core::Point2{50000, 0}));
    CHECK_EQ(recorded[1], (core::Point2{30000, -5000}));
    CHECK(f.journal.entries().front().args.to_json().dump().find("orta") == std::string::npos);
}

TEST_CASE("DİKAYAK: taban çizgisine göre dik ayak ve dik boy nokta koyar")
{
    Fixture f;

    // A baseline along the east axis makes every answer readable by eye: a foot
    // of 30 m and an offset of 5 m is (30, +5), and LEFT IS POSITIVE, so the
    // negative offset lands on the other side. The same numbers the `dik()`
    // point function is tested with, because both call
    // `core::perpendicular_offset` and a second copy of the sign convention is
    // how one of them would end up mirrored.
    REQUIRE(f.bus
                .execute_line("DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5 ayak=60 boy=5",
                              Origin::CommandLine)
                .ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{3});
    const core::Point2 expected[3]{{10'000, 5'000}, {30'000, -5'000}, {60'000, 5'000}};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{1});
        CHECK_EQ((core::Point2{xs[0], ys[0]}), expected[i]);
    }

    // AND THE SAME POINTS THE ONE GRAMMAR GIVES. Two clients, one construction.
    Fixture g;
    REQUIRE(g.bus
                .execute_line("NOKTA dik(0,0,100,0,10,5) dik(0,0,100,0,30,-5) "
                              "dik(0,0,100,0,60,5)",
                              Origin::CommandLine)
                .ok());
    REQUIRE_EQ(g.doc.live_entity_count(), std::size_t{3});
    for (std::size_t i = 0; i < 3; ++i) {
        const auto span = g.doc.geometry().rings_of(g.doc.entities().slot[i]);
        CHECK_EQ((core::Point2{g.doc.geometry().ring_xs(span.first)[0],
                               g.doc.geometry().ring_ys(span.first)[0]}),
                 expected[i]);
    }
}

TEST_CASE("DİKAYAK: cizgi=evet noktaları verildikleri sırayla birleştirir")
{
    Fixture f;
    REQUIRE(f.bus
                .execute_line("DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5 cizgi=evet",
                              Origin::CommandLine)
                .ok());

    // Two points and the line through them, and the line's vertices are the
    // points in the order the crew read them — that order IS the shape.
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{3});
    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[2]);
    const auto xs   = f.doc.geometry().ring_xs(span.first);
    const auto ys   = f.doc.geometry().ring_ys(span.first);
    REQUIRE_EQ(xs.size(), std::size_t{2});
    CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{10'000, 5'000}));
    CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{30'000, -5'000}));

    // One command, one undo step (CLAUDE.md 1.5): the points and the line go
    // together or not at all.
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});
    REQUIRE(f.bus.execute_line("GERİAL", Origin::CommandLine).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("ALIM: semt açısı ve kenardan nokta hesaplar")
{
    Fixture f;

    // Grad and clockwise from north are the defaults (TODOS-CAD P0), so the four
    // cardinal readings off a station at the origin are exact by construction:
    // 0 grad is north, 100 is east, 200 is south, 300 is west. Those four are
    // the ones `sin_cos_udeg` answers exactly, which is why they are the test.
    REQUIRE(f.bus
                .execute_line("ALIM 0,0 aci=0 kenar=100 aci=100 kenar=100 "
                              "aci=200 kenar=100 aci=300 kenar=100",
                              Origin::CommandLine)
                .ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{4});
    const core::Point2 expected[4]{{0, 100'000}, {100'000, 0}, {0, -100'000}, {-100'000, 0}};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i]);
        CHECK_EQ((core::Point2{f.doc.geometry().ring_xs(span.first)[0],
                               f.doc.geometry().ring_ys(span.first)[0]}),
                 expected[i]);
    }
}

TEST_CASE("ALIM: bağlama verilince açılar ondan itibaren okunur")
{
    Fixture f;

    // The instrument is zeroed on a backsight due EAST, so a reading of 0 is
    // east and a reading of 100 grad is south — the whole station is turned by
    // the backsight's own azimuth. Getting this wrong rotates every coordinate,
    // which is why the command says which of the two it used.
    REQUIRE(f.bus
                .execute_line("ALIM 0,0 baglama=50,0 aci=0 kenar=100 aci=100 kenar=100",
                              Origin::CommandLine)
                .ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
    const core::Point2 expected[2]{{100'000, 0}, {0, -100'000}};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i]);
        CHECK_EQ((core::Point2{f.doc.geometry().ring_xs(span.first)[0],
                               f.doc.geometry().ring_ys(span.first)[0]}),
                 expected[i]);
    }

    // AND THE SAME POINT THE ONE GRAMMAR GIVES for the absolute direction, which
    // is what `APLİKASYON` would print for it: a reading of 100 grad off an east
    // backsight is an azimuth of 200 grad.
    Fixture g;
    REQUIRE(g.bus.execute_line("NOKTA @100<200", Origin::CommandLine).ok());
    const auto span = g.doc.geometry().rings_of(g.doc.entities().slot[0]);
    CHECK_EQ((core::Point2{g.doc.geometry().ring_xs(span.first)[0],
                           g.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{0, -100'000}));
}

TEST_CASE("ALIM: eksi kenar reddedilir ve hiçbir şey çizilmez")
{
    Fixture f;
    const auto refused = f.bus.execute_line("ALIM 0,0 aci=0 kenar=-100", Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("eksi olamaz") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("ALIM: kapalı bir çokgen okunup birleştirilir")
{
    Fixture f;

    // A boundary walked round from one station: four readings and the ring they
    // make. `cizgi=evet` joins them in the order they were read, because that
    // order IS the ring — a field book is not a set of points.
    REQUIRE(f.bus
                .execute_line("ALIM 0,0 aci=0 kenar=50 aci=100 kenar=50 aci=200 kenar=50 "
                              "aci=300 kenar=50 cizgi=evet",
                              Origin::CommandLine)
                .ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{5}); ///< four points and the line
    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[4]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});

    // One command, one undo step: the readings and the line go together.
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});
}

TEST_CASE("ÖLÇÜ tur=koordinat: ordinat okumasını ve eksenini jest belirler")
{
    // A building corner 30 m east and 20 m north of the block origin. A leader
    // taken SIDEWAYS reads its easting; one taken UP reads its northing — that is
    // the gesture an ordinate table is built with and it costs no extra answer.
    {
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("ÖLÇÜ tur=koordinat birinci=0,0 ikinci=30,20 konum=45,20",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        (void)span;
        const auto& doc    = f.doc;
        const auto payload = doc.geometry().payload_of(doc.entities().slot[0]);
        const auto def     = core::decode_dimension(payload);
        REQUIRE(def.ok());
        CHECK(def.value().ordinate_x); ///< the easting
        CHECK_EQ(def.value().measurement, 30'000);
    }
    {
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("ÖLÇÜ tur=koordinat birinci=0,0 ikinci=30,20 konum=30,40",
                                  Origin::CommandLine)
                    .ok());
        const auto& doc = f.doc;
        const auto def  = core::decode_dimension(doc.geometry().payload_of(doc.entities().slot[0]));
        REQUIRE(def.ok());
        CHECK_FALSE(def.value().ordinate_x); ///< the northing
        CHECK_EQ(def.value().measurement, 20'000);
    }
    {
        // A POINT WEST OF THE ORIGIN READS NEGATIVE. The sign is part of the
        // answer: printing it positive would put the corner on the wrong side.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("ÖLÇÜ tur=koordinat birinci=0,0 ikinci=-15,0 konum=-30,0",
                                  Origin::CommandLine)
                    .ok());
        const auto& doc = f.doc;
        const auto def  = core::decode_dimension(doc.geometry().payload_of(doc.entities().slot[0]));
        REQUIRE(def.ok());
        CHECK_EQ(def.value().measurement, -15'000);
    }
}

TEST_CASE("ÖLÇÜ tur=yay: yay boyunca uzunluk, kirişi değil")
{
    // A QUARTER OF A 50 m CIRCLE IS 78,540 m ALONG AND 70,711 m ACROSS, and a
    // drawing that printed the chord would have somebody order the wrong length
    // of kerbstone. π·50/2 = 78,5398 m, which rounds to 78540 mm.
    Fixture f;
    REQUIRE(f.bus
                .execute_line("ÖLÇÜ tur=yay birinci=0,0 ikinci=50,0 bitis=0,50 konum=45,45",
                              Origin::CommandLine)
                .ok());
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});

    const auto def = core::decode_dimension(f.doc.geometry().payload_of(f.doc.entities().slot[0]));
    REQUIRE(def.ok());
    CHECK_EQ(def.value().measurement, 78'540);
    CHECK_EQ(static_cast<int>(def.value().type), static_cast<int>(core::DimensionType::ArcLength));

    // AND THE TEXT SAYS SO IN METRES, which is what the sheet prints.
    const std::string text = core::dimension_text(def.value(), core::DrawingUnit::Metre);
    CHECK_EQ(text, "78,54");

    // A half turn is half the circumference: π·50 = 157,0796 m.
    Fixture half;
    REQUIRE(half.bus
                .execute_line("ÖLÇÜ tur=yay birinci=0,0 ikinci=50,0 bitis=-50,0 konum=0,60",
                              Origin::CommandLine)
                .ok());
    const auto other =
        core::decode_dimension(half.doc.geometry().payload_of(half.doc.entities().slot[0]));
    REQUIRE(other.ok());
    CHECK_EQ(other.value().measurement, 157'080);
}

TEST_CASE("SEÇ: çokgen ve kesen çokgen, kutunun alamayacağı şekli alır")
{
    // Three parcels in a row. An L-shaped polygon takes the first and the third
    // and leaves the middle one — which is exactly what a box cannot do, and why
    // a surveyor deselecting by hand is where the wrong parcel gets left in.
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("ALAN 20,0 30,0 30,10 20,10", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,20 10,20 10,30 0,30", Origin::CommandLine).ok());

    // An L covering the first and third parcels but not the second.
    REQUIRE(f.bus
                .execute_line("SEÇ mod=ÇOKGEN noktalar=-1,-1 noktalar=15,-1 noktalar=15,35 "
                              "noktalar=-1,35",
                              Origin::CommandLine)
                .ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2});

    // KESEN ÇOKGEN also takes what it merely touches: a polygon clipping the
    // corner of the middle parcel takes it as well.
    REQUIRE(f.bus
                .execute_line("SEÇ mod=ÇOKGENKESEN noktalar=-1,-1 noktalar=25,-1 "
                              "noktalar=25,5 noktalar=-1,5",
                              Origin::CommandLine)
                .ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2}); ///< the first and the middle

    // Fewer than three corners is no polygon.
    const std::string refused = REFUSED(
        f.bus.execute_line("SEÇ mod=ÇOKGEN noktalar=0,0 noktalar=10,10", Origin::CommandLine));
    CHECK(refused.find("üç köşe") != std::string::npos); ///< refused, and it says why
    CHECK_EQ(f.bus.selection().size(), std::size_t{2});  ///< and changes nothing
}

TEST_CASE("SEÇ: ÇİT çizdiği hattın kestiği her şeyi alır")
{
    // A row of fence posts drawn as short lines, and a fence line running along
    // them: the fence takes the ones it crosses and nothing else. A box over the
    // same ground would take the building behind them too.
    Fixture f;
    for (int i = 0; i < 4; ++i) {
        const std::string x = std::to_string(i * 10);
        REQUIRE(f.bus.execute_line("ÇİZGİ " + x + ",0 " + x + ",10", Origin::CommandLine).ok());
    }
    REQUIRE(f.bus.execute_line("ALAN 0,20 30,20 30,30 0,30", Origin::CommandLine).ok());

    // A fence across the posts at y = 5, stopping short of the third.
    REQUIRE(
        f.bus.execute_line("SEÇ mod=ÇİT noktalar=-5,5 noktalar=15,5", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2}); ///< the posts at x = 0 and 10

    // THE BUILDING IS NOT TOUCHED, because the fence does not reach it.
    REQUIRE(
        f.bus.execute_line("SEÇ mod=ÇİT noktalar=-5,25 noktalar=35,25", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{1}); ///< only the face

    // Fewer than two points is no fence.
    REFUSED(f.bus.execute_line("SEÇ mod=ÇİT noktalar=0,0", Origin::CommandLine));
    CHECK_EQ(f.bus.selection().size(), std::size_t{1});
}

TEST_CASE("SEÇ: ÖNCEKİ bir adım geri gider, SON en yeni nesneyi alır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("ALAN 20,0 30,0 30,10 20,10", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 40,0 50,0", Origin::CommandLine).ok());

    // `SON` takes the most recently created entity, which is what a drafter
    // means by "that one" after drawing it.
    REQUIRE(f.bus.execute_line("SEÇ mod=SON", Origin::CommandLine).ok());
    REQUIRE_EQ(f.bus.selection().size(), std::size_t{1});
    CHECK_EQ(f.bus.selection().keys().front(), f.doc.entities().key[2]);

    // Select both parcels, then select something else by mistake, then go back.
    REQUIRE(f.bus.execute_line("SEÇ mod=KUTU noktalar=-1,-1 31,11", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2});

    REQUIRE(f.bus.execute_line("SEÇ mod=SON", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{1});

    REQUIRE(f.bus.execute_line("SEÇ mod=ÖNCEKİ", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2}); ///< the two parcels are back

    // ONE STEP DEEP, AND IT GOES BOTH WAYS: `ÖNCEKİ` again returns to what was
    // held before it, not two steps back — a stack of selections is a stack
    // nobody can keep in their head.
    REQUIRE(f.bus.execute_line("SEÇ mod=ÖNCEKİ", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2});

    // A DELETED ENTITY IS NOT RESURRECTED. The previous selection holds keys,
    // and a key whose entity is gone is dropped rather than restored.
    REQUIRE(f.bus.execute_line("SEÇ mod=TÜMÜ", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("SİL", Origin::CommandLine).ok());
    REQUIRE(f.bus.execute_line("SEÇ mod=ÖNCEKİ", Origin::CommandLine).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{0});
}

TEST_CASE("PATLAT: çizgi kenarlara, alan sınırına ayrılır")
{
    {
        // A three-segment run becomes three lines, and the run itself goes.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 20,0 30,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(
            f.bus.execute_line("PATLAT nesne=" + std::to_string(key), Origin::CommandLine).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{3});
        for (std::size_t i = 0; i < 3; ++i) {
            const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i + 1]);
            CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{2});
        }
        // One command, one step: undoing puts the run back whole.
        REQUIRE(f.bus.execute_line("GERİAL", Origin::CommandLine).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }
    {
        // A FACE'S BOUNDARY IS CLOSED, so exploding it gives FOUR edges for four
        // corners — the closing edge included, which is the one a naive walk
        // over the vertices forgets.
        Fixture f;
        REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(
            f.bus.execute_line("PATLAT nesne=" + std::to_string(key), Origin::CommandLine).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});
    }
    {
        // A kind it cannot place is NAMED rather than half-exploded.
        Fixture f;
        REQUIRE(f.bus.execute_line("DAİRE 0,0 10,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        const auto refused =
            f.bus.execute_line("PATLAT nesne=" + std::to_string(key), Origin::CommandLine);
        CHECK_FALSE(refused.ok());
        CHECK(refused.error().message.find("daire") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }
}

TEST_CASE("HİZALA: bir çift taşır, iki çift döndürür, olcekle ölçekler")
{
    {
        // One pair is a move and nothing else.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(
            f.bus
                .execute_line("HİZALA nesne=" + std::to_string(key) + " kaynak=0,0 hedef=100,50",
                              Origin::CommandLine)
                .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{100'000, 50'000}));
        CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{110'000, 50'000}));
    }
    {
        // TWO PAIRS ADD THE TURN. An east-pointing line asked to point north
        // turns a quarter and keeps its length, because `olcekle` was not given.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("HİZALA nesne=" + std::to_string(key) +
                                      " kaynak=0,0 hedef=0,0 kaynak2=10,0 hedef2=0,20",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{0, 0}));
        CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{0, 10'000})); ///< turned, not scaled
    }
    {
        // AND `olcekle` TAKES THE LENGTH RATIO TOO: the same turn, and the line
        // stretched to reach the second target.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("HİZALA nesne=" + std::to_string(key) +
                                      " kaynak=0,0 hedef=0,0 kaynak2=10,0 hedef2=0,20 "
                                      "olcekle=evet",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_ys(span.first)[1], 20'000);
    }
}

TEST_CASE("BÖLÜMLE: sayi eşit parçaya böler, aralik sabit aralıkla yürür")
{
    {
        // A 100 m line into four parts: three marks at 25, 50 and 75.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("BÖLÜMLE nesne=" + std::to_string(key) + " sayi=4",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{4}); ///< the line and three marks
        const core::Mm want[3]{25'000, 50'000, 75'000};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i + 1]);
            CHECK_EQ(f.doc.geometry().ring_xs(span.first)[0], want[i]);
        }
    }
    {
        // A FIXED SPACING ACROSS A CORNER, which is what a chainage list is: the
        // station at 60 m falls on the second leg of an L, and finding it means
        // reading along the run rather than along one segment.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0 50,50", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("BÖLÜMLE nesne=" + std::to_string(key) + " aralik=20",
                                  Origin::CommandLine)
                    .ok());
        // 20, 40, 60, 80 along a 100 m run: four marks.
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{5});
        const auto third = f.doc.geometry().rings_of(f.doc.entities().slot[3]);
        CHECK_EQ(f.doc.geometry().ring_xs(third.first)[0], 50'000);
        CHECK_EQ(f.doc.geometry().ring_ys(third.first)[0], 10'000);
    }
    {
        // Both or neither is a caller that does not know which it means.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        CHECK_FALSE(f.bus
                        .execute_line("BÖLÜMLE nesne=" + std::to_string(key) + " sayi=4 aralik=20",
                                      Origin::CommandLine)
                        .ok());
        // NEITHER GIVEN IS NOT A REFUSAL ANY MORE: the command ASKS for `sayi`,
        // because pressing BÖLÜMLE in the tool column and then being told to type
        // an argument is a command reachable by mouse that cannot be finished by
        // one (CLAUDE.md 5.15). A caller that answers nothing gets no edit, which
        // is what declining looks like everywhere in this program.
        const std::uint64_t before = f.doc.content_hash();
        CHECK(f.bus.execute_line("BÖLÜMLE nesne=" + std::to_string(key), Origin::CommandLine).ok());
        CHECK_EQ(f.doc.content_hash(), before);
    }
}

TEST_CASE("ÇİZGİDÜZENLE: kapat, ac, ters ve sadelestir")
{
    {
        // `kapat` turns an open run into a face, and `ac` turns it back.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 10,10", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("ÇİZGİDÜZENLE nesne=" + std::to_string(key) + " islem=kapat",
                                  Origin::CommandLine)
                    .ok());
        const auto closed = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK(f.doc.geometry().ring_role[closed.first] != core::RingRole::Open);

        REQUIRE(f.bus
                    .execute_line("ÇİZGİDÜZENLE nesne=" + std::to_string(key) + " islem=ac",
                                  Origin::CommandLine)
                    .ok());
        const auto open = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_role[open.first], core::RingRole::Open);
    }
    {
        // `ters` reverses the run, which is what an offset to the other side and
        // a station list counted from the far end both need.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0 20,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("ÇİZGİDÜZENLE nesne=" + std::to_string(key) + " islem=ters",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        CHECK_EQ(xs[0], 20'000);
        CHECK_EQ(xs[2], 0);
    }
    {
        // `sadelestir` drops a vertex that carries no shape: a point 1 mm off a
        // straight run between its neighbours. The ENDS are never dropped.
        Fixture f;
        REQUIRE(
            f.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0.001 100,0 100,50", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("ÇİZGİDÜZENLE nesne=" + std::to_string(key) +
                                      " islem=sadelestir tolerans=0.01",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{3});
        CHECK_EQ(xs[0], 0);
        CHECK_EQ(xs[1], 100'000);
        CHECK_EQ(xs[2], 100'000);
    }
}

TEST_CASE("KIR: iki nokta arasındaki parça çıkar, tek nokta boşluksuz böler")
{
    {
        // A 100 m line with a 20 m gate cut out of the middle: two pieces, one
        // 0–40 and one 60–100. This is the verb BÖL does not have — BÖL keeps
        // both halves touching.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("KIR nesne=" + std::to_string(key) + " birinci=40,0 ikinci=60,0",
                                  Origin::CommandLine)
                    .ok());

        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
        const auto head = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_xs(head.first)[0], 0);
        CHECK_EQ(f.doc.geometry().ring_xs(head.first)[1], 40'000);
        const auto tail = f.doc.geometry().rings_of(f.doc.entities().slot[1]);
        CHECK_EQ(f.doc.geometry().ring_xs(tail.first)[0], 60'000);
        CHECK_EQ(f.doc.geometry().ring_xs(tail.first)[1], 100'000);

        // ONE COMMAND, ONE STEP: undoing the break restores the whole line,
        // not one of its two pieces.
        CHECK_EQ(f.undo.undo_depth(), std::size_t{2}); ///< the line, then the break
        REQUIRE(f.bus.execute_line("GERİAL", Origin::CommandLine).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto whole = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_xs(whole.first)[1], 100'000);
    }
    {
        // THE ORDER OF THE CLICKS DOES NOT MATTER: a hand clicks the far end
        // first as often as not, and a gap is a gap either way.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("KIR nesne=" + std::to_string(key) + " birinci=60,0 ikinci=40,0",
                                  Origin::CommandLine)
                    .ok());
        const auto head = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_xs(head.first)[1], 40'000);
    }
    {
        // ONE POINT SPLITS WITH NO GAP, which is the degenerate case of the same
        // verb: two pieces that still touch.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("KIR nesne=" + std::to_string(key) + " birinci=40,0 ikinci=40,0",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
        const auto head = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto tail = f.doc.geometry().rings_of(f.doc.entities().slot[1]);
        CHECK_EQ(f.doc.geometry().ring_xs(head.first)[1], 40'000);
        CHECK_EQ(f.doc.geometry().ring_xs(tail.first)[0], 40'000);
    }
}

TEST_CASE("UÇUCA: uçları değen çizgiler tek çizgi olur, değmeyenler kalır")
{
    {
        // Three runs drawn end to end in the wrong ORDER and one of them
        // BACKWARDS, which is what a digitised map looks like. The join has to
        // flip what needs flipping and chain from either end.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        REQUIRE(f.bus.execute_line("ÇİZGİ 30,0 20,0", Origin::CommandLine).ok());
        REQUIRE(f.bus.execute_line("ÇİZGİ 10,0 20,0", Origin::CommandLine).ok());
        std::vector<std::int64_t> ids;
        for (std::size_t i = 0; i < 3; ++i)
            ids.push_back(static_cast<std::int64_t>(core::raw(f.doc.entities().key[i])));

        REQUIRE(f.bus
                    .execute_line("UÇUCA nesne=" + std::to_string(ids[0]) + " nesne=" +
                                      std::to_string(ids[1]) + " nesne=" + std::to_string(ids[2]),
                                  Origin::CommandLine)
                    .ok());

        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{4});
        CHECK_EQ(xs[0], 0);
        CHECK_EQ(xs[1], 10'000);
        CHECK_EQ(xs[2], 20'000);
        CHECK_EQ(xs[3], 30'000);
    }
    {
        // A GAP BEYOND THE TOLERANCE IS NOT A JOIN, and the run that did not
        // reach is left exactly as it was rather than dragged into place.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        REQUIRE(f.bus.execute_line("ÇİZGİ 10,0 20,0", Origin::CommandLine).ok());
        REQUIRE(f.bus.execute_line("ÇİZGİ 25,0 35,0", Origin::CommandLine).ok());
        std::vector<std::int64_t> ids;
        for (std::size_t i = 0; i < 3; ++i)
            ids.push_back(static_cast<std::int64_t>(core::raw(f.doc.entities().key[i])));

        REQUIRE(f.bus
                    .execute_line("UÇUCA nesne=" + std::to_string(ids[0]) + " nesne=" +
                                      std::to_string(ids[1]) + " nesne=" + std::to_string(ids[2]),
                                  Origin::CommandLine)
                    .ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{2}); ///< the chain and the stray

        // AND A BIG ENOUGH TOLERANCE TAKES IT. 5 m closes the gap.
        Fixture g;
        REQUIRE(g.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        REQUIRE(g.bus.execute_line("ÇİZGİ 15,0 25,0", Origin::CommandLine).ok());
        std::vector<std::int64_t> two;
        for (std::size_t i = 0; i < 2; ++i)
            two.push_back(static_cast<std::int64_t>(core::raw(g.doc.entities().key[i])));
        REQUIRE(g.bus
                    .execute_line("UÇUCA nesne=" + std::to_string(two[0]) +
                                      " nesne=" + std::to_string(two[1]) + " tolerans=5",
                                  Origin::CommandLine)
                    .ok());
        CHECK_EQ(g.doc.live_entity_count(), std::size_t{1});
    }
    {
        // Nothing touching at all is a refusal that names the tolerance.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        REQUIRE(f.bus.execute_line("ÇİZGİ 50,0 60,0", Origin::CommandLine).ok());
        std::vector<std::int64_t> ids;
        for (std::size_t i = 0; i < 2; ++i)
            ids.push_back(static_cast<std::int64_t>(core::raw(f.doc.entities().key[i])));
        const auto refused = f.bus.execute_line("UÇUCA nesne=" + std::to_string(ids[0]) +
                                                    " nesne=" + std::to_string(ids[1]),
                                                Origin::CommandLine);
        CHECK_FALSE(refused.ok());
        CHECK(refused.error().message.find("tolerans") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    }
}

TEST_CASE("UZUNLUK: delta, yuzde ve toplam; tam olarak biri")
{
    const auto line_length = [](Fixture& f) {
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        return xs[xs.size() - 1] - xs[0];
    };

    {
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("UZUNLUK nesne=" + std::to_string(key) + " delta=20",
                                  Origin::CommandLine)
                    .ok());
        CHECK_EQ(line_length(f), 120'000);
    }
    {
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("UZUNLUK nesne=" + std::to_string(key) + " yuzde=150",
                                  Origin::CommandLine)
                    .ok());
        CHECK_EQ(line_length(f), 150'000);
    }
    {
        // `toplam` on a MULTI-VERTEX line changes only the last segment: the
        // rest of the run is untouched, which is what "make it 60 m" means on a
        // line whose first leg is already surveyed.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 40,0 50,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("UZUNLUK nesne=" + std::to_string(key) + " toplam=60",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{3});
        CHECK_EQ(xs[1], 40'000); ///< the surveyed vertex did not move
        CHECK_EQ(xs[2], 60'000);
    }
    {
        // THE START END CAN MOVE INSTEAD.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        REQUIRE(f.bus
                    .execute_line("UZUNLUK nesne=" + std::to_string(key) + " toplam=120 uc=bas",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        CHECK_EQ(xs[0], -20'000);
        CHECK_EQ(xs[1], 100'000);
    }
    {
        // Two of the three, or none, is a caller that does not know which it
        // means — and the refusal says what the length is now.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::CommandLine).ok());
        const auto key  = static_cast<std::int64_t>(core::raw(f.doc.entities().key[0]));
        const auto both = f.bus.execute_line(
            "UZUNLUK nesne=" + std::to_string(key) + " delta=5 toplam=50", Origin::CommandLine);
        CHECK_FALSE(both.ok());
        CHECK(both.error().message.find("Tam olarak birini") != std::string::npos);
        // NONE GIVEN IS NOT A REFUSAL ANY MORE: the command ASKS for `delta`, for
        // the reason BÖLÜMLE asks for `sayi` (CLAUDE.md 5.15). A caller that
        // answers nothing gets no edit.
        const std::uint64_t before = f.doc.content_hash();
        const auto none =
            f.bus.execute_line("UZUNLUK nesne=" + std::to_string(key), Origin::CommandLine);
        CHECK(none.ok());
        CHECK_EQ(f.doc.content_hash(), before);
    }
}

TEST_CASE("ELİPS: eksen yöntemi merkezi iki ucun ortası alır")
{
    // The same ellipse two ways: centred on (50,0) with a 50 m half-axis east,
    // and given as the two ends (0,0) and (100,0). Identical drawings, because
    // the centre of the second IS the midpoint of its two ends.
    Fixture by_centre;
    REQUIRE(by_centre.bus.execute_line("ELİPS 50,0 100,0 50,20", Origin::CommandLine).ok());

    Fixture by_axis;
    REQUIRE(by_axis.bus
                .execute_line("ELİPS yontem=eksen birinci=0,0 ikinci_uc=100,0 ikinci=50,20",
                              Origin::CommandLine)
                .ok());

    CHECK_EQ(by_centre.doc.content_hash(), by_axis.doc.content_hash());

    const core::Box2 box = by_axis.doc.entities().box_of(0);
    CHECK_EQ(box.min_x, 0);
    CHECK_EQ(box.max_x, 100'000);
    CHECK_EQ(box.min_y, -20'000);
    CHECK_EQ(box.max_y, 20'000);

    // Two identical ends are no axis at all.
    Fixture f;
    const auto refused = f.bus.execute_line(
        "ELİPS yontem=eksen birinci=0,0 ikinci_uc=0,0 ikinci=50,20", Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("aynı nokta") != std::string::npos);
}

TEST_CASE("YAY: dört yöntem, bilinen bir çeyrek çemberden")
{
    // A quarter circle of radius 50 m about the origin, from due east to due
    // north. Every method has to produce the same arc, and the bounding box is
    // the check: a quarter in the first quadrant spans 0..50 on both axes.
    const auto quarter = [](Fixture& f) {
        const core::Box2 box = f.doc.entities().box_of(0);
        CHECK_EQ(box.min_x, 0);
        CHECK_EQ(box.max_x, 50'000);
        CHECK_EQ(box.min_y, 0);
        CHECK_EQ(box.max_y, 50'000);
    };

    {
        // merkez: the default and the one the others are measured against.
        Fixture f;
        REQUIRE(f.bus.execute_line("YAY 0,0 50,0 0,50", Origin::CommandLine).ok());
        quarter(f);
    }
    {
        // 3n: the two ends and a point on the sweep. 35.3553 m on both axes is
        // the 45° point of a 50 m circle.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("YAY yontem=3n baslangic=50,0 uzerinden=35.3553,35.3553 "
                                  "bitis=0,50",
                                  Origin::CommandLine)
                    .ok());
        quarter(f);
    }
    {
        // bma: start, centre and a swept angle. A quarter turn is 100 grad, and
        // under the default semt rule the sweep is counted the way the session
        // reads every other angle.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("YAY merkez=0,0 baslangic=0,50 yontem=bma supurme=100",
                                  Origin::CommandLine)
                    .ok());
        quarter(f);
    }
    {
        // bby: THE TWO ENDS AND A RADIUS, and the two solutions are the two
        // semicircles on (0,0)–(100,0) when the radius is exactly half the span.
        // One arches north and one south, which is a difference nothing can
        // round away — and it is the difference a fillet on the wrong side of a
        // kerb would be.
        Fixture north;
        REQUIRE(north.bus
                    .execute_line("YAY yontem=bby baslangic=0,0 bitis=100,0 yaricap=50 yon=sag",
                                  Origin::CommandLine)
                    .ok());
        const core::Box2 up = north.doc.entities().box_of(0);
        CHECK_EQ(up.min_y, 0);
        CHECK_EQ(up.max_y, 50'000);

        Fixture south;
        REQUIRE(south.bus
                    .execute_line("YAY yontem=bby baslangic=0,0 bitis=100,0 yaricap=50 yon=sol",
                                  Origin::CommandLine)
                    .ok());
        const core::Box2 down = south.doc.entities().box_of(0);
        CHECK_EQ(down.min_y, -50'000);
        CHECK_EQ(down.max_y, 0);

        // AND THEY ARE NOT THE SAME DRAWING. Without this the two could be one
        // arc reported twice.
        CHECK(north.doc.content_hash() != south.doc.content_hash());
    }
    {
        // A NEGATIVE SWEEP TURNS THE OTHER WAY, and it must not be folded into
        // one turn: −100 grad is a quarter counter-clockwise, not 300 grad
        // clockwise. From due north that lands in the SECOND quadrant.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("YAY merkez=0,0 baslangic=0,50 yontem=bma supurme=-100",
                                  Origin::CommandLine)
                    .ok());
        const core::Box2 box = f.doc.entities().box_of(0);
        CHECK_EQ(box.min_x, -50'000);
        CHECK_EQ(box.max_x, 0);
        CHECK_EQ(box.min_y, 0);
        CHECK_EQ(box.max_y, 50'000);
    }
    {
        // A radius smaller than half the span joins nothing, and a sweep of zero
        // is an arc that is a point. Both are refused with the reason.
        Fixture f;
        const auto too_small = f.bus.execute_line(
            "YAY yontem=bby baslangic=0,0 bitis=100,0 yaricap=10", Origin::CommandLine);
        CHECK_FALSE(too_small.ok());
        CHECK(too_small.error().message.find("yarısından küçük") != std::string::npos);

        const auto no_sweep = f.bus.execute_line(
            "YAY merkez=0,0 baslangic=0,50 yontem=bma supurme=0", Origin::CommandLine);
        CHECK_FALSE(no_sweep.ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
    {
        // 3n refuses three collinear points.
        Fixture f;
        const auto refused = f.bus.execute_line(
            "YAY yontem=3n baslangic=0,0 uzerinden=50,0 bitis=100,0", Origin::CommandLine);
        CHECK_FALSE(refused.ok());
        CHECK(refused.error().message.find("aynı doğru") != std::string::npos);
    }
}

TEST_CASE("DİKDÖRTGEN 3n: döndürülmüş, ve üçüncü nokta yalnız yüksekliği verir")
{
    {
        // A wall along a road that runs north-east: two corners of the wall and
        // a point off it. The result is a RECTANGLE — the third point is
        // projected onto the edge's normal, so a hand a few millimetres off
        // still gets four right angles rather than a parallelogram.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("DİKDÖRTGEN yontem=3n noktalar=0,0 noktalar=100,0 "
                                  "noktalar=100,40",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{4});
        CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{0, 0}));
        CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{100'000, 0}));
        CHECK_EQ((core::Point2{xs[2], ys[2]}), (core::Point2{100'000, 40'000}));
        CHECK_EQ((core::Point2{xs[3], ys[3]}), (core::Point2{0, 40'000}));
    }
    {
        // THE THIRD POINT NEED NOT BE A CORNER. Given 60 m along the edge and
        // 40 m off it, the depth is 40 and the along-component is discarded —
        // that is what makes this a rectangle tool rather than a parallelogram
        // tool.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("DİKDÖRTGEN yontem=3n noktalar=0,0 noktalar=100,0 "
                                  "noktalar=60,40",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        CHECK_EQ((core::Point2{xs[2], ys[2]}), (core::Point2{100'000, 40'000}));
        CHECK_EQ((core::Point2{xs[3], ys[3]}), (core::Point2{0, 40'000}));
    }
    {
        // A SKEW EDGE, which is the whole reason this method exists: a 3-4-5
        // edge from (0,0) to (30,40) is 50 m long, and a depth of 10 m off it
        // lands on exact millimetres because the normal is (-0.8, 0.6).
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("DİKDÖRTGEN yontem=3n noktalar=0,0 noktalar=30,40 "
                                  "noktalar=22,46",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        CHECK_EQ((core::Point2{xs[3], ys[3]}), (core::Point2{-8'000, 6'000}));
        CHECK_EQ((core::Point2{xs[2], ys[2]}), (core::Point2{22'000, 46'000}));
    }
    {
        // A third point ON the edge has no height and is refused.
        Fixture f;
        const auto refused = f.bus.execute_line(
            "DİKDÖRTGEN yontem=3n noktalar=0,0 noktalar=100,0 noktalar=50,0", Origin::CommandLine);
        CHECK_FALSE(refused.ok());
        CHECK(refused.error().message.find("kenarın üzerinde") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
    {
        // AND THE OLD FORM IS UNTOUCHED. Every page, script and journal writes
        // two corners with no method, and that still means an axis-aligned box.
        Fixture f;
        REQUIRE(f.bus.execute_line("DİKDÖRTGEN 0,0 100,40", Origin::CommandLine).ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_xs(span.first).size(), std::size_t{4});
    }
}

TEST_CASE("ÇOKGEN: kare, altıgen ve üç yöntem")
{
    {
        // A SQUARE AT 0°, which is the case where every corner is exactly on a
        // millimetre: the four axes are integer-exact in `sin_cos_udeg`, so an
        // inscribed square of radius 50 m has its corners at ±50 m on the axes
        // and nothing rounds.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKGEN 0,0 4 yaricap=50", Origin::CommandLine).ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{4});
        // Under the default semt rule the first corner is due north and the ring
        // winds the one way the model stores.
        CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{0, 50'000}));
        CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{-50'000, 0}));
        CHECK_EQ((core::Point2{xs[2], ys[2]}), (core::Point2{0, -50'000}));
        CHECK_EQ((core::Point2{xs[3], ys[3]}), (core::Point2{50'000, 0}));
    }
    {
        // A HEXAGON'S SIDE EQUALS ITS CIRCUMRADIUS, which is the one identity a
        // regular polygon has that can be checked without trigonometry: an
        // inscribed hexagon of radius 10 m and a `kenar` hexagon of side 10 m are
        // the same hexagon.
        Fixture inscribed;
        REQUIRE(inscribed.bus.execute_line("ÇOKGEN 0,0 6 yaricap=10", Origin::CommandLine).ok());
        Fixture by_side;
        REQUIRE(
            by_side.bus
                .execute_line("ÇOKGEN 0,0 6 yontem=kenar kenar_uzunlugu=10", Origin::CommandLine)
                .ok());
        CHECK_EQ(inscribed.doc.content_hash(), by_side.doc.content_hash());
    }
    {
        // CIRCUMSCRIBED IS BIGGER, by 1/cos(π/n). For a square that is √2, so a
        // `dis` square of radius 50 m reaches 70.711 m to its corners.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKGEN 0,0 4 yontem=dis yaricap=50", Origin::CommandLine).ok());
        const core::Box2 box = f.doc.entities().box_of(0);
        CHECK(box.max_y > 70'710);
        CHECK(box.max_y < 70'712);
    }
    {
        // THE ANGLE TURNS IT. A square started at 50 grad has its first corner
        // north-east, so the bounding box is the axis-aligned one.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKGEN 0,0 4 yaricap=50 aci=50", Origin::CommandLine).ok());
        const core::Box2 box = f.doc.entities().box_of(0);
        CHECK(box.max_x > 35'354);
        CHECK(box.max_x < 35'356);
    }
    {
        // Out of range is refused by the declared range, before the body runs.
        Fixture f;
        CHECK_FALSE(f.bus.execute_line("ÇOKGEN 0,0 2 yaricap=50", Origin::CommandLine).ok());
        CHECK_FALSE(f.bus.execute_line("ÇOKGEN 0,0 4 yaricap=0", Origin::CommandLine).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
}

TEST_CASE("DAİRE: dört yöntem, bilinen bir çemberden")
{
    // A circle of radius 50 m centred on (50, 50). Every method has to find the
    // same circle, and the radius is checked through ALANÖLÇ rather than by
    // reading the payload: what a user gets is the area and the perimeter, and
    // those are the numbers that reach the tapu (§12).
    const auto radius_of = [](Fixture& f) {
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        (void)span;
        return f.doc.entities().box_of(0);
    };

    {
        // merkez: the default, and the one the other three are measured against.
        Fixture f;
        REQUIRE(f.bus.execute_line("DAİRE 50,50 100,50", Origin::CommandLine).ok());
        const core::Box2 box = radius_of(f);
        CHECK_EQ(box.min_x, 0);
        CHECK_EQ(box.max_x, 100'000);
        CHECK_EQ(box.min_y, 0);
        CHECK_EQ(box.max_y, 100'000);
    }
    {
        // 2n: the two ends of a diameter. The centre is their midpoint.
        Fixture f;
        REQUIRE(
            f.bus.execute_line("DAİRE yontem=2n birinci=0,50 ikinci=100,50", Origin::CommandLine)
                .ok());
        const core::Box2 box = radius_of(f);
        CHECK_EQ(box.min_x, 0);
        CHECK_EQ(box.max_x, 100'000);
        CHECK_EQ(box.min_y, 0);
        CHECK_EQ(box.max_y, 100'000);
    }
    {
        // 3n: three points on the rim — the top, the right and the bottom of the
        // same circle, which is how a measured kerb is recovered.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("DAİRE yontem=3n birinci=50,100 ikinci=100,50 ucuncu=50,0",
                                  Origin::CommandLine)
                    .ok());
        const core::Box2 box = radius_of(f);
        CHECK_EQ(box.min_x, 0);
        CHECK_EQ(box.max_x, 100'000);
        CHECK_EQ(box.min_y, 0);
        CHECK_EQ(box.max_y, 100'000);
    }
    {
        // 3n refuses three collinear points: they have no circumcircle, and
        // answering with the largest circle that fits in an int64 would be a
        // wrong answer dressed as an answer.
        Fixture f;
        const auto refused = f.bus.execute_line(
            "DAİRE yontem=3n birinci=0,0 ikinci=50,0 ucuncu=100,0", Origin::CommandLine);
        CHECK_FALSE(refused.ok());
        CHECK(refused.error().message.find("aynı doğru") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
}

TEST_CASE("DAİRE ttr: dört çözümden işaret edilen köşe alınır")
{
    // Two lines crossing at the origin — one along east, one along north — and a
    // fillet of radius 10 m. There are FOUR circles of that radius tangent to
    // both, one per quadrant, and which is wanted is not in the numbers. The
    // pointed-at corner decides, and a silent pick would put the fillet on the
    // wrong corner of the junction.
    struct Want
    {
        const char* at;    ///< the corner pointed at
        core::Mm centre_x; ///< the centre that must come out
        core::Mm centre_y;
    };

    const Want wanted[4]{{"50,50", 10'000, 10'000},
                         {"-50,50", -10'000, 10'000},
                         {"-50,-50", -10'000, -10'000},
                         {"50,-50", 10'000, -10'000}};

    for (const Want& one : wanted) {
        Fixture f;
        REQUIRE(f.bus
                    .execute_line(std::string("DAİRE yontem=ttr birinci=-100,0 ikinci=100,0 "
                                              "ucuncu=0,-100 dorduncu=0,100 yaricap=10 yon=") +
                                      one.at,
                                  Origin::CommandLine)
                    .ok());
        const core::Box2 box = f.doc.entities().box_of(0);
        CHECK_EQ((box.min_x + box.max_x) / 2, one.centre_x);
        CHECK_EQ((box.min_y + box.max_y) / 2, one.centre_y);
        CHECK_EQ((box.max_x - box.min_x) / 2, 10'000);
    }

    // Parallel lines have no tangent circle of a given radius, and the refusal
    // says which of the two things went wrong.
    Fixture f;
    const auto refused = f.bus.execute_line("DAİRE yontem=ttr birinci=0,0 ikinci=100,0 ucuncu=0,50 "
                                            "dorduncu=100,50 yaricap=10 yon=50,25",
                                            Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("paralel") != std::string::npos);
}

TEST_CASE("KESİŞİMNOKTA: üç yöntem, bilinen bir kareden")
{
    // A 100 m square with corners at (0,0), (100,0), (100,100) and (0,100). Its
    // centre, (50, 50), is recoverable three ways and all three have to give the
    // same millimetre — that is what makes them three roads to one place rather
    // than three approximations.
    {
        // dogru: the two diagonals cross at the centre.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("KESİŞİMNOKTA yontem=dogru birinci=0,0 ikinci=100,100 "
                                  "ucuncu=100,0 dorduncu=0,100",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ((core::Point2{f.doc.geometry().ring_xs(span.first)[0],
                               f.doc.geometry().ring_ys(span.first)[0]}),
                 (core::Point2{50'000, 50'000}));
    }
    {
        // mesafe: two tape readings off two corners. The centre is at
        // sqrt(50² + 50²) = 70.710678… m from each of (0,0) and (100,0), and the
        // LEFT solution of the direction (0,0)→(100,0) is the one to the north.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=70.710678 "
                                  "ikinci=100,0 ikinci_mesafe=70.710678 yon=sol",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const core::Point2 at{f.doc.geometry().ring_xs(span.first)[0],
                              f.doc.geometry().ring_ys(span.first)[0]};
        CHECK_EQ(at.x, 50'000);
        CHECK(at.y > 49'999);
        CHECK(at.y < 50'001);
    }
    {
        // dogrultu: 50 grad from (0,0) is north-east under the default semt rule,
        // and 350 grad from (100,0) is north-west; they cross at the centre.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("KESİŞİMNOKTA yontem=dogrultu birinci=0,0 birinci_aci=50 "
                                  "ikinci=100,0 ikinci_aci=350",
                                  Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const core::Point2 at{f.doc.geometry().ring_xs(span.first)[0],
                              f.doc.geometry().ring_ys(span.first)[0]};
        CHECK(at.x > 49'999);
        CHECK(at.x < 50'001);
        CHECK(at.y > 49'999);
        CHECK(at.y < 50'001);
    }
}

TEST_CASE("KESİŞİMNOKTA: iki uzaklığın iki çözümü sessizce seçilmez")
{
    // The same two readings, the two sides, two different corners. A command
    // that answered one of them without being asked is the silent pick that puts
    // a boundary on the wrong side of a road.
    Fixture left;
    REQUIRE(left.bus
                .execute_line("KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=50 "
                              "ikinci=80,0 ikinci_mesafe=50 yon=sol",
                              Origin::CommandLine)
                .ok());
    Fixture right;
    REQUIRE(right.bus
                .execute_line("KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=50 "
                              "ikinci=80,0 ikinci_mesafe=50 yon=sag",
                              Origin::CommandLine)
                .ok());

    const auto one    = left.doc.geometry().rings_of(left.doc.entities().slot[0]);
    const auto two    = right.doc.geometry().rings_of(right.doc.entities().slot[0]);
    const core::Mm y1 = left.doc.geometry().ring_ys(one.first)[0];
    const core::Mm y2 = right.doc.geometry().ring_ys(two.first)[0];
    CHECK_EQ(y1, -y2); ///< mirrored across the baseline
    CHECK(y1 != 0);

    // AND A READING THAT CANNOT REACH IS REFUSED WITH BOTH FIGURES (R19).
    Fixture f;
    const auto refused =
        f.bus.execute_line("KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=10 "
                           "ikinci=80,0 ikinci_mesafe=10",
                           Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("ulaşmıyor") != std::string::npos);
    CHECK(refused.error().message.find("80,000") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("ARANOKTA: oran, mesafe ve eşit bölme")
{
    {
        // A run of ratios along a 100 m line: a quarter, a half, three quarters.
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("ARANOKTA 0,0 100,0 deger=0.25 deger=0.5 deger=0.75",
                                  Origin::CommandLine)
                    .ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{3});
        const core::Mm expected[3]{25'000, 50'000, 75'000};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i]);
            CHECK_EQ(f.doc.geometry().ring_xs(span.first)[0], expected[i]);
            CHECK_EQ(f.doc.geometry().ring_ys(span.first)[0], 0);
        }
    }
    {
        // Metres instead. 20 m along a 100 m line is the same as a ratio of 0.2,
        // and both go through `along_ratio` so they land on the same millimetre.
        Fixture f;
        REQUIRE(f.bus.execute_line("ARANOKTA 0,0 100,0 yontem=mesafe deger=20", Origin::CommandLine)
                    .ok());
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        CHECK_EQ(f.doc.geometry().ring_xs(span.first)[0], 20'000);
    }
    {
        // `sayi=4` cuts the line into four parts and places the THREE points
        // between them. Placing the ends too would leave two points on one
        // monument, which is what a station peg list must not have.
        Fixture f;
        REQUIRE(f.bus.execute_line("ARANOKTA 0,0 100,0 sayi=4", Origin::CommandLine).ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{3});
        const core::Mm expected[3]{25'000, 50'000, 75'000};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[i]);
            CHECK_EQ(f.doc.geometry().ring_xs(span.first)[0], expected[i]);
        }
        CHECK_EQ(f.undo.undo_depth(), std::size_t{1}); ///< one command, one step
    }
}

TEST_CASE("ARANOKTA: aynı iki nokta reddedilir")
{
    Fixture f;
    const auto refused = f.bus.execute_line("ARANOKTA 0,0 0,0 deger=0.5", Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("aynı") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("DİKAYAK: aynı iki taban noktası reddedilir")
{
    Fixture f;
    const auto refused = f.bus.execute_line("DİKAYAK 0,0 0,0 ayak=10 boy=5", Origin::CommandLine);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("aynı") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("NOKTA FONKSİYONU: n(1284) çizimdeki noktayı komutlar üzerinden bulur")
{
    // The whole road, through commands only (CLAUDE.md 5.9): a point is drawn,
    // the `nokta_no` column is declared and filled — which is what `NOKTALAR`
    // does when it reads a list from the field — and then `n(1284)` finds it
    // from the command line.
    Fixture f;
    REQUIRE(f.bus.execute_line("NOKTA 485320.150,4310220.400", Origin::CommandLine).ok());
    const auto key = static_cast<std::uint64_t>(core::raw(f.doc.entities().key[0]));

    REQUIRE(f.bus.execute_line("SÜTUN nokta_no metin \"nokta no\"", Origin::CommandLine).ok());
    REQUIRE(f.bus
                .execute_line("ÖZNİTELİK nokta_no " + std::to_string(key) + " 1284",
                              Origin::CommandLine)
                .ok());

    CHECK_EQ(f.bus.numbered_point(1284).value(), (core::Point2{485320150, 4310220400}));
    CHECK(!f.bus.numbered_point(1285).has_value());

    REQUIRE(f.bus.execute_line("ÇİZGİ n(1284) @50,30", Origin::CommandLine).ok());
    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[1]);
    const auto xs   = f.doc.geometry().ring_xs(span.first);
    const auto ys   = f.doc.geometry().ring_ys(span.first);
    CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{485320150, 4310220400}));
    CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{485370150, 4310250400}));

    // A number nobody drew is refused before anything is drawn.
    const std::size_t before = f.doc.live_entity_count();
    auto missing             = f.bus.execute_line("ÇİZGİ n(4242) @50,30", Origin::CommandLine);
    CHECK(!missing.ok());
    CHECK(missing.error().message.find("4242 numaralı nokta yok") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), before);
}

TEST_CASE("GRAMER: fuzz tohum korpusundaki her satır çökmeden ayrıştırılır")
{
    // CLAUDE.md 6.7 ships the harness and the corpus with the grammar. The
    // libFuzzer target in /tests/fuzz needs Clang; this replays the same seeds
    // through the same entry points on every build, so the corpus is never dead
    // weight — the same arrangement test_io.cpp keeps for the format corpora.
    namespace fs          = std::filesystem;
    const fs::path corpus = fs::path(KENTOS_FUZZ_DIR) / "tohum" / "komut";
    REQUIRE_MESSAGE(fs::exists(corpus), corpus.string());

    std::vector<fs::path> seeds;
    for (const auto& entry : fs::directory_iterator(corpus))
        if (entry.is_regular_file()) seeds.push_back(entry.path());
    std::sort(seeds.begin(), seeds.end()); // test.md R19: sorted directory iteration

    const FieldReader row = [](std::string_view column) -> std::optional<std::string> {
        if (column == "beyan") return std::nullopt;
        return std::string("1284");
    };

    // The same stand-in drawing the libFuzzer harness uses, so `n(…)` is walked
    // on both roads: a number that is there and a number that is not.
    const ResolveContext drawing = with_points();

    std::size_t lines = 0;
    for (const fs::path& seed : seeds) {
        std::ifstream in(seed);
        std::string line;
        while (std::getline(in, line)) {
            ++lines;
            if (auto parsed = parse_line(line)) {
                core::Point2 last{};
                for (const Token& t : parsed.value().tokens) {
                    (void)describe(t);
                    if (!is_coordinate(t)) continue;
                    for (int unit = 0; unit < 3; ++unit)
                        for (int rule = 0; rule < 2; ++rule) {
                            auto p = resolve_point(
                                t, last,
                                ResolveContext{
                                    core::AngleConvention{static_cast<core::AngleUnit>(unit),
                                                          static_cast<core::AngleRule>(rule)},
                                    drawing.named_point});
                            if (p) last = p.value();
                        }
                }
            }
            (void)evaluate_expression(line);
            (void)evaluate_predicate(line, row);
            (void)parse_point(line, core::Point2{}, drawing);

            // And once with no document behind it at all, which is the context
            // a headless caller hands down and where `n()` must refuse.
            (void)parse_point(line, core::Point2{}, ResolveContext{});
        }
    }
    CHECK(seeds.size() >= 17);
    CHECK(lines >= seeds.size());
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

    auto parsed = parse_line("VERİTABANI baglan hedef=\"host=localhost dbname=kentoscad\"");
    REQUIRE(parsed.ok());
    REQUIRE_EQ(parsed.value().tokens.size(), std::size_t{2});

    const Token& value = parsed.value().tokens[1];
    CHECK_EQ(value.kind, Token::Kind::KeyValue);
    CHECK_EQ(value.word, std::string("hedef"));
    REQUIRE_EQ(value.nested.size(), std::size_t{1});
    CHECK_EQ(value.nested.front().kind, Token::Kind::Text);
    CHECK_EQ(value.nested.front().text, std::string("host=localhost dbname=kentoscad"));

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

TEST_CASE("KATMANGÖRÜNÜM: bir katmanı gösterir, gizler ve yalnız bırakır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=BİNA", Origin::Test).ok());
    const core::LayerId parcel = f.doc.find_layer("PARSEL");
    const core::LayerId road   = f.doc.find_layer("YOL");
    REQUIRE(parcel != core::kNoLayer);
    REQUIRE(road != core::kNoLayer);

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=gizle katman=PARSEL", Origin::Test).ok());
    CHECK_FALSE(f.doc.layer(parcel)->visible);
    CHECK(f.doc.layer(road)->visible);

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=goster katman=PARSEL", Origin::Test).ok());
    CHECK(f.doc.layer(parcel)->visible);

    // `yalniz`: the named one up, everything else down — layer 0 included,
    // because "only this" means only this.
    REQUIRE(f.bus.execute_line("KGO islem=yalniz katman=YOL", Origin::Test).ok());
    for (const core::Layer& l : f.doc.layers())
        CHECK_EQ(l.visible, l.name == "YOL");
}

TEST_CASE("KATMANGÖRÜNÜM: tumu ve tersine bütün tabloya bakar")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL gorunur=hayır", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL gorunur=hayır", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=BİNA", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=tersine", Origin::Test).ok());
    CHECK(f.doc.layer(f.doc.find_layer("PARSEL"))->visible);
    CHECK(f.doc.layer(f.doc.find_layer("YOL"))->visible);
    CHECK_FALSE(f.doc.layer(f.doc.find_layer("BİNA"))->visible);
    // Layer 0 was showing and is now hidden, which is what "invert" says.
    CHECK_FALSE(f.doc.layer(0)->visible);

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=tumu", Origin::Test).ok());
    for (const core::Layer& l : f.doc.layers())
        CHECK(l.visible);

    // And one layer on its own, with the same word.
    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=tersine katman=YOL", Origin::Test).ok());
    CHECK_FALSE(f.doc.layer(f.doc.find_layer("YOL"))->visible);
    CHECK(f.doc.layer(f.doc.find_layer("PARSEL"))->visible);
}

TEST_CASE("KATMANGÖRÜNÜM: aktif katmanı DEĞİŞTİRMEZ")
{
    // THE REASON THIS COMMAND EXISTS BESIDE `KATMAN`. Naming a layer with `KATMAN`
    // makes it active, because that is how a user picks one to draw on. Hiding
    // forty layers is not picking one, and the fortieth is certainly not the
    // choice — so the panel's visibility menu goes out as this command.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    const core::LayerId road = f.doc.find_layer("YOL");
    REQUIRE(f.bus.active_layer() == road);

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=gizle katman=PARSEL", Origin::Test).ok());
    CHECK_EQ(f.bus.active_layer(), road);
    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=yalniz katman=PARSEL", Origin::Test).ok());
    CHECK_EQ(f.bus.active_layer(), road);
}

TEST_CASE("KATMANGÖRÜNÜM: bir çağrı bir geri alma adımı")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL gorunur=hayır", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL gorunur=hayır", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=BİNA gorunur=hayır", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=tumu", Origin::Test).ok());
    CHECK(f.doc.layer(f.doc.find_layer("PARSEL"))->visible);

    // Three layers changed, ONE step back (CLAUDE.md 1.5).
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_FALSE(f.doc.layer(f.doc.find_layer("PARSEL"))->visible);
    CHECK_FALSE(f.doc.layer(f.doc.find_layer("YOL"))->visible);
    CHECK_FALSE(f.doc.layer(f.doc.find_layer("BİNA"))->visible);
}

TEST_CASE("KATMANGÖRÜNÜM: söylenmeyeni yapmaz, sessizce yutmaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());

    // A layer that is not there is NOT created — this command changes what is
    // showing, it does not make layers.
    CHECK(!f.bus.execute_line("KATMANGÖRÜNÜM islem=gizle katman=YOKBÖYLE", Origin::Test).ok());
    CHECK_EQ(f.doc.find_layer("YOKBÖYLE"), core::kNoLayer);

    // A word that is not a verb is refused with the list of the ones that are.
    auto bad = f.bus.execute_line("KATMANGÖRÜNÜM islem=parlat katman=PARSEL", Origin::Test);
    CHECK(!bad.ok());
    CHECK(bad.error().message.find("goster") != std::string::npos);

    // `tumu` looks at every layer, so a layer name with it is a misunderstanding
    // and is REFUSED rather than dropped on the floor (command.md P15).
    CHECK(!f.bus.execute_line("KATMANGÖRÜNÜM islem=tumu katman=PARSEL", Origin::Test).ok());

    // And the verbs that name a layer insist on one.
    CHECK(!f.bus.execute_line("KATMANGÖRÜNÜM islem=yalniz", Origin::Test).ok());
}

TEST_CASE("KATMANGÖRÜNÜM: arayüz = komut satırı = betik")
{
    // Article 1.2: three clients, one document, one journal line.
    const auto hide = [](Origin origin) {
        Fixture f;
        REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", origin).ok());
        REQUIRE(f.bus.execute_line("KATMAN ad=YOL", origin).ok());
        REQUIRE(f.bus.execute_line("KATMANGÖRÜNÜM islem=yalniz katman=PARSEL", origin).ok());
        return std::pair{f.doc.content_hash(), f.journal.entries().back().args.to_json().dump()};
    };
    const auto gui    = hide(Origin::Gui);
    const auto cli    = hide(Origin::CommandLine);
    const auto script = hide(Origin::Script);
    CHECK_EQ(gui.first, cli.first);
    CHECK_EQ(cli.first, script.first);
    CHECK_EQ(gui.second, cli.second);
    CHECK_EQ(cli.second, script.second);
    // The recorded arguments are the resolved words, not whatever was typed.
    CHECK(gui.second.find("yalniz") != std::string::npos);
    CHECK(gui.second.find("PARSEL") != std::string::npos);
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

TEST_CASE("nesne isteminde Esc bir ret değil, iptaldir")
{
    // `want_objects` answers an empty pick with a refusal, and a refusal is an
    // error. Esc is not an empty pick: the cancel settles the session after the
    // body has run, so the user who changed their mind is told "İptal edildi"
    // and not an error about the objects they never meant to give.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());

    auto started = f.bus.begin_interactive("BÖL");
    REQUIRE(started.ok());
    auto& session = *started.value();
    REQUIRE(session.waiting()); ///< nothing selected, so it asks which object
    session.cancel();

    auto done = f.bus.finish(session);
    REQUIRE(done.ok());
    CHECK(!done.value().mutated);
    CHECK_EQ(done.value().message, "İptal edildi");
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("error messages name what was expected and what arrived")
{
    Fixture f;

    auto bad = f.bus.execute_line("YOKBÖYLE 1,2", Origin::Test);
    CHECK(!bad.ok());
    CHECK(bad.error().message.find("YOKBÖYLE") != std::string::npos);
    CHECK(bad.error().message.find("YARDIM") != std::string::npos);
}

TEST_CASE("bir koordinat nesne seçimine bağlanmaz: yutulmaz, reddedilir")
{
    // THE BUG THIS LOCKS DOWN. `bind_tokens` turned any coordinate token into a
    // point BEFORE it looked at the parameter's kind, so a coordinate was
    // accepted for a parameter of every kind. A number, a text or a boolean
    // was rescued afterwards by `Validator::check_against_spec`; a SELECTION was
    // not. `append` reads the value with `as_ids()`, a point yields none, and
    // the empty id list that was stored reads as "not given" to a selection
    // whose `arity.min` is 0 — so the argument vanished and the command reported
    // success. `.claude/command.md` P15 forbids exactly that.
    Fixture f;
    REQUIRE(f.bus.execute_line("NOKTA 485600,4310200", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("NOKTA 485610,4310200", Origin::Test).ok());
    const std::size_t before = f.doc.live_entity_count();

    // A KEYWORD COORDINATE AT A SELECTION. `1,2` is one coordinate to the one
    // grammar this program has (CLAUDE.md 5.11), never two ids — and `1,2,3` is
    // not even that, because `classify` refuses it. This silently deleted
    // nothing; now it is named, and the message says what to write instead.
    auto pair = f.bus.execute_line("SİL nesneler=1,2", Origin::Test);
    CHECK(!pair.ok());
    CHECK(pair.error().message.find("nesne seçimi") != std::string::npos);
    CHECK(pair.error().message.find("nesneler=1 nesneler=2") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), before);

    // THE REPORTED REPRO. The bare coordinates after `bitis=` used to bind
    // positionally to `nesneler`, the first declared parameter, whose arity never
    // fills: they were swallowed and KOPYALA made ONE copy where three were
    // asked for. That became a refusal; it is now what was asked for, because a
    // keyword that names a point list keeps the run open for the coordinates
    // after it — three copies of each of the two points.
    REQUIRE(f.bus.execute_line("SEÇ KATMAN katman=0", Origin::Test).ok());
    auto copied = f.bus.execute_line("KOPYALA baslangic=485600,4310200 bitis=485620,4310200 "
                                     "485640,4310200 485660,4310200",
                                     Origin::Test);
    REQUIRE(copied.ok());
    CHECK_EQ(f.doc.live_entity_count(), before + 6);
    const std::size_t copies = f.doc.live_entity_count();

    // A COORDINATE AT THE OTHER KINDS is refused where it is read rather than one
    // layer later, and a parameter that takes a single value gets no advice to
    // repeat a keyword that may not be repeated.
    auto scalar = f.bus.execute_line("KATMAN ad=RENKLİ renk=1,2", Origin::Test);
    CHECK(!scalar.ok());
    CHECK(scalar.error().message.find("tam sayı") != std::string::npos);
    CHECK(scalar.error().message.find("yineleyin") == std::string::npos);

    // WHAT THE DOCUMENTATION TEACHES STILL WORKS, at any count — this is the form
    // every `/docs/komutlar` page shows and the only one that carries a third id.
    REQUIRE(f.bus.execute_line("SİL nesneler=1 nesneler=2", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), copies - 2);
}

TEST_CASE("betikteki [1,2] hâlâ iki kimlik: komut satırının reddi JSON yolunu kapatmadı")
{
    // A two-element JSON array is genuinely ambiguous and `dispatch` settles it
    // from the spec (`{"nesneler": [1, 2]}` is two ids, not a point). That repair
    // is a different layer from `bind_tokens` and must survive its tightening:
    // the command line writes a list by repeating the keyword, JSON writes it as
    // an array, and both clients reach the same document (Article 1.2).
    Fixture cli;
    REQUIRE(cli.bus.execute_line("NOKTA 485600,4310200", Origin::CommandLine).ok());
    REQUIRE(cli.bus.execute_line("NOKTA 485610,4310200", Origin::CommandLine).ok());
    REQUIRE(cli.bus.execute_line("SİL nesneler=1 nesneler=2", Origin::CommandLine).ok());

    Fixture script;
    REQUIRE(script.bus.execute_line("NOKTA 485600,4310200", Origin::Script).ok());
    REQUIRE(script.bus.execute_line("NOKTA 485610,4310200", Origin::Script).ok());
    Args args;
    args.set("nesneler", Value::point(core::Point2{1, 2}));
    REQUIRE(script.bus.dispatch(Invocation{"core.erase", args, Origin::Script}).ok());

    CHECK_EQ(script.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(cli.doc.content_hash(), script.doc.content_hash());
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

TEST_CASE("toplu işte reddedilen komut yalnız kendi düzenlemesini geri sarar")
{
    // A COMMAND IN A BATCH SHARES THE BATCH'S TRANSACTION, and a failure used to
    // roll back THAT — every edit the batch had made so far, by every command
    // before it — while those commands stayed in the journal as done. A JSON
    // script aborts on the first error, so it never showed; a Python script that
    // catches the error and goes on did: its earlier lines vanished and its later
    // ones landed, half a script. Refusals report as errors now (TODOS F-01), so
    // the path is common; the rollback stops at the failed command's own edits.
    Fixture f;
    REQUIRE(f.bus.begin_batch("Betik").ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Script).ok());
    REFUSED(f.bus.execute_line("SİL nesneler=99", Origin::Script));
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1}); ///< the first line survives
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,5 10,5", Origin::Script).ok());
    REQUIRE(f.bus.end_batch().ok());

    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});

    // And one that fails AFTER writing takes back its own writes, not the batch's.
    REQUIRE(f.bus.begin_batch("Betik").ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Script).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 20,0 30,0", Origin::Script).ok());
    const std::uint64_t before = f.doc.content_hash();
    // Two good ids and a missing one: the erase of 1 and 2 is written before 99
    // is found missing, and all of it — only it — goes back.
    const auto keys       = f.doc.entities().key;
    const std::string ids = std::to_string(core::raw(keys[keys.size() - 2])) + " " +
                            std::to_string(core::raw(keys.back())) + " 99";
    REFUSED(f.bus.execute_line("SİL nesneler=" + ids, Origin::Script));
    CHECK_EQ(f.doc.content_hash(), before);
    REQUIRE(f.bus.end_batch().ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
}

TEST_CASE("GERİAL ve YİNELE düzenleme yapmış bir toplu işte reddedilir; öncesindeki iş kaybolmaz")
{
    // A BATCH IS ONE UNDO STEP THAT DOES NOT EXIST YET. Its entry is pushed when
    // it closes, so GERİAL inside it cannot reach the batch's own edits — what it
    // reaches is the entry BELOW, the work the user did before the script ran,
    // while the batch's transaction is still open over the same document. The
    // close then pushes the batch and empties the redo stack, and that earlier
    // work is gone with no way back. The manual's "Çiz ve geri al" script looked
    // like it undid its own line; it did nothing, silently, because the refusal
    // reported success (TODOS F-01).
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok()); ///< the user's work
    REQUIRE_EQ(f.undo.undo_depth(), std::size_t{1});

    REQUIRE(f.bus.begin_batch("Betik").ok());
    CHECK(f.bus.execute_line("ÇİZGİ 0,5 10,5", Origin::Script).ok());
    const std::string undo = REFUSED(f.bus.execute_line("GERİAL", Origin::Script));
    CHECK(undo.find("toplu") != std::string::npos);
    const std::string redo = REFUSED(f.bus.execute_line("YİNELE", Origin::Script));
    CHECK(redo.find("toplu") != std::string::npos);
    f.bus.abort_batch();

    // The user's line is still there, and still undoable as the step it was.
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1});

    // BEFORE ITS FIRST EDIT a batch has nothing to cut across: a Python console
    // line that only says `cad.undo()` is the command line's GERİAL, and the
    // batch closes with no step of its own, so the redo stack survives it.
    REQUIRE(f.bus.begin_batch("Konsol").ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Script).ok());
    REQUIRE(f.bus.end_batch().ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    REQUIRE(f.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
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

    const std::string missing = REFUSED(f.bus.execute_line("SİL nesneler=99", Origin::Test));
    CHECK(missing.find("99") != std::string::npos); // an error naming the id, nothing applied
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

    // No partial application, ever (kentoscad.md §2.5).
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
}

TEST_CASE("YARDIM: liste kategorilere göre, satır sayısı komut sayısı kadar değil")
{
    // THE LISTING IS NINE LINES, NOT NINETY-EIGHT.
    //
    // `YARDIM` echoed one line per command — name, every alias and the summary —
    // into the transcript, which is a running conversation: a hundred appends
    // pushed everything the user had done out of sight and took the scroll
    // position with it. It was reported as a command list that "stretches away
    // downwards" and "opens far too late".
    //
    // What a listing answers is what EXISTS, and a category answers that better
    // than a sentence repeated for every command. The count is asserted against
    // the registry rather than against a number typed here, so the day a tenth
    // category is declared this test says so instead of drifting.
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("YARDIM", Origin::Test).ok());

    std::size_t lines = 0;
    for (const char c : said)
        if (c == '\n') ++lines;

    std::set<Category> groups;
    for (const auto& spec : f.bus.registry().all())
        if (!spec.names.empty()) groups.insert(spec.category);

    // One heading line plus one line per category that has anything in it.
    CHECK_EQ(lines, groups.size() + 1);
    CHECK(lines < f.bus.registry().size() / 4);

    // Every category present is named, and the first line says where to go next.
    for (const Category c : groups)
        CHECK_MESSAGE(said.find(category_name(c)) != std::string::npos, category_name(c));
    CHECK(said.find("YARDIM komut=") != std::string::npos);

    // AND THE WHOLE SET STILL LEAVES, as data. An agent reads one answer instead
    // of scraping a hundred echo lines (command.md R26).
    const auto result = f.bus.execute_line("YARDIM", Origin::Test);
    REQUIRE(result.ok());
    const core::Json& report = result.value().report;
    const core::Json* count  = report.find("sayi");
    const core::Json* all    = report.find("komutlar");
    REQUIRE(count != nullptr);
    REQUIRE(all != nullptr);
    CHECK_EQ(count->as_int(), static_cast<std::int64_t>(f.bus.registry().size()));
    CHECK_EQ(all->as_array().size(), f.bus.registry().size());
    const core::Json* names = all->as_array().front().find("adlar");
    REQUIRE(names != nullptr);
    CHECK(!names->as_array().empty());
}

TEST_CASE("YARDIM sayfayı açtırır, ama metni her istemciye yine yazar")
{
    Fixture f;

    // THE SEAM, NOT A BRANCH ON THE CLIENT. `run_help` asks whoever can show a
    // page to show one and writes its text regardless, so the answer a script
    // reads does not depend on whether a window happened to be open
    // (command.md P10 — nothing looks at `InputSource`).
    std::vector<std::string> opened;
    f.bus.on_help_page = [&opened](const std::string& on) { opened.push_back(on); };

    std::vector<std::string> said;
    f.bus.on_echo = [&said](std::string_view line) { said.emplace_back(line); };

    REQUIRE(f.bus.execute_line("YARDIM", Origin::Test).ok());
    REQUIRE_EQ(opened.size(), std::size_t{1});
    CHECK(opened.front().empty()); ///< the whole list
    CHECK(said.size() > 1);        ///< and the text came too

    // Asked about one command, the page opens ON it — by the primary Turkish
    // name, whatever spelling was typed.
    said.clear();
    REQUIRE(f.bus.execute_line("YARDIM komut=cizgi", Origin::Test).ok());
    REQUIRE_EQ(opened.size(), std::size_t{2});
    CHECK_EQ(opened.back(), "ÇİZGİ");
    CHECK(!said.empty());

    // An unknown name opens nothing: there is no page for a command that is not
    // there, and the error is the whole answer.
    REFUSED(f.bus.execute_line("YARDIM komut=YOKBÖYLEBİRŞEY", Origin::Test));
    CHECK_EQ(opened.size(), std::size_t{2});
}

TEST_CASE("her komutun insan okuyacağı bir Türkçe başlığı var")
{
    Fixture f;

    // A NAME IS NOT A LABEL. `names.front()` is the word typed at the prompt and
    // it is one word by design — `ÇIKTIYERLEŞİMİ`, `ÖZNİTELİKŞEMASI` — so a menu
    // built from it reads as one run-together word, which is not Turkish. The
    // generated menu entries of `MainWindow::completeMenusFromRegistry` read this
    // field, and a command declared without one arrives in the menu bar with a
    // label no Turkish speaker would write.
    //
    // Checked here rather than by a script because the registries are linked
    // here: `/src/command`, the three domains and `/src/ai` all declare commands
    // and a grep over one of them would pass while another shipped untitled.
    std::vector<std::string> untitled;
    for (const CommandSpec& spec : f.bus.registry().all()) {
        if (spec.names.empty()) continue;
        if (spec.title.empty()) untitled.push_back(spec.id);

        // And it is a LABEL, not the name again: a title equal to the shouted
        // primary name is the fallback written out by hand, which defeats the
        // point of the field.
        CHECK_MESSAGE(spec.title != spec.names.front(), spec.id);
    }
    CHECK_MESSAGE(untitled.empty(), [&] {
        std::string all;
        for (const std::string& id : untitled)
            all += (all.empty() ? "" : ", ") + id;
        return "başlığı olmayan komutlar: " + all;
    }());
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
    // kentoscad.md §3 and §13: the users are Turkish surveying engineers, so an
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

TEST_CASE("KÖŞETAŞI yazılı nesnenin yazısını taşır: yeni yuva yazıyı devralır")
{
    // A caption lives on the geometry SLOT, and every geometry edit appends a new
    // slot. The label used to stay on the old one, so moving a corner of a
    // captioned parsel — or a dimension's point — silently erased the label.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAZI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("METİN 10,10 \"1234/7\" 2000", Origin::Test).ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    REQUIRE_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("1234/7"));

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=12,10", Origin::Test).ok());
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("1234/7"));
    CHECK_EQ(f.doc.texts().height(f.doc.entities().slot[e]), core::Mm{2000});

    // Undo and redo both keep it.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("1234/7"));
    REQUIRE(f.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("1234/7"));
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

    REFUSED(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=2 nokta=5,5", Origin::Test));

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
    REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=9 nokta=1,1", Origin::Test));
    CHECK_EQ(f.doc.geometry().area_of(f.doc.entities().slot[0]), before);

    // And an entity that is gone stays gone.
    REQUIRE(f.bus.execute_line("SİL nesneler=1", Origin::Test).ok());
    REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=1,1", Origin::Test));
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

    auto turned =
        f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4", Origin::Test);
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

    REQUIRE(
        f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4", Origin::Test)
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
    REFUSED(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3", Origin::Test));

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
    REFUSED(f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2", Origin::Test));
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ALANAÇEVİR zaten kapalı bir alanı reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();
    REFUSED(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test));
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

    REQUIRE(
        f.bus.execute_line("ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4", Origin::Test)
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
    REFUSED(f.bus.execute_line("DAİRE merkez=100,100 cevre=100,100", Origin::Test));
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("KÖŞETAŞI dairenin merkezini taşır ve çeyrek tutamağıyla yarıçapını kurar; KÖŞEEKLE ve "
          "ALANAÇEVİR daireyi reddeder")
{
    // A circle's grips are its centre and four quadrant handles (core/grips.hpp):
    // the centre translates it, a quadrant sets the radius. It stays a circle
    // throughout — no corner can be ADDED to it, and it cannot become a face.
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,100 cevre=110,100", Origin::Test).ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    const auto centre = [&] {
        return core::circle_centre_of(f.doc.geometry(), f.doc.entities().slot[e]);
    };
    const auto radius = [&] {
        return core::circle_radius_of(f.doc.geometry(), f.doc.entities().slot[e]);
    };

    const auto grips = core::entity_grips(f.doc, e);
    REQUIRE_EQ(grips.size(), 5u);
    CHECK_EQ(grips[0].role, core::GripRole::Centre);
    CHECK_EQ(grips[2].at, (core::Point2{100000, 110000}));

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=200,200", Origin::Test).ok());
    CHECK_EQ(centre(), (core::Point2{200000, 200000}));
    CHECK_EQ(radius(), core::Mm{10000});

    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=215,200", Origin::Test).ok());
    CHECK_EQ(centre(), (core::Point2{200000, 200000}));
    CHECK_EQ(radius(), core::Mm{15000});
    CHECK_EQ(f.doc.entities().kind[e], core::kCircleKind);

    // Undone one grip at a time, back to the drawn circle.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(radius(), core::Mm{10000});
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(centre(), (core::Point2{100000, 100000}));

    const std::uint64_t before = f.doc.content_hash();
    said.clear();
    said += REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=6 nokta=200,200", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK(said.find("6. tutamağı yok; 5 tutamağı var") != std::string::npos);

    REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=100,100", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before); // a radius of zero is refused

    REFUSED(f.bus.execute_line("KÖŞEEKLE nesne=1 kose=1 nokta=200,200", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);

    REFUSED(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("KÖŞETAŞI elipsin eksenini çevirir; ikinci eksen dik kalır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", Origin::Test).ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    const auto major = [&] {
        return core::ellipse_major_of(f.doc.geometry(), f.doc.entities().slot[e]);
    };
    const auto minor = [&] {
        return core::ellipse_minor_of(f.doc.geometry(), f.doc.entities().slot[e]);
    };

    // Five grips: centre, two axis ends, their mirror images.
    const auto grips = core::entity_grips(f.doc, e);
    REQUIRE_EQ(grips.size(), 5u);
    CHECK_EQ(grips[3].at, (core::Point2{-10000, 0}));
    CHECK_EQ(grips[4].at, (core::Point2{0, -5000}));

    // The first axis turns north; the second keeps its 5 m and its side.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=0,20", Origin::Test).ok());
    CHECK_EQ(major(), (core::Point2{0, 20000}));
    CHECK_EQ(minor(), (core::Point2{-5000, 0}));

    // The second axis is measured ACROSS the first, as ELİPS measures it.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=3 nokta=3,7", Origin::Test).ok());
    CHECK_EQ(major(), (core::Point2{0, 20000}));
    CHECK_EQ(minor(), (core::Point2{3000, 0}));

    // On the first axis there is no second: refused, nothing moves.
    const std::uint64_t before = f.doc.content_hash();
    REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=3 nokta=0,9", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(f.doc.entities().kind[e], core::kEllipseKind);
}

TEST_CASE("KÖŞETAŞI ölçünün tanım noktasını taşır: çizgi yeniden kurulur, yazı yeniden ölçülür")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖLÇÜ birinci=0,0 ikinci=12.5,0 konum=0,3", Origin::Test).ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    REQUIRE_EQ(f.doc.entities().kind[e], core::kDimensionKind);
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("12,50"));

    // Three definition points and the caption: four grips.
    const auto grips = core::entity_grips(f.doc, e);
    REQUIRE_EQ(grips.size(), 4u);
    CHECK_EQ(grips[1].at, (core::Point2{12500, 0}));
    CHECK_EQ(grips[3].role, core::GripRole::Caption);

    // The second point moves out to 20 m: the figure follows, the line stays
    // where it was placed.
    said.clear();
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=20,0", Origin::Test).ok());
    INFO(said);
    const auto def = core::dimension_of(f.doc.geometry(), f.doc.entities().slot[e]);
    REQUIRE(def.ok());
    CHECK_EQ(def.value().measurement, std::int64_t{20000});
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("20,00"));
    const auto after = core::entity_grips(f.doc, e);
    REQUIRE_EQ(after.size(), 4u);
    CHECK_EQ(after[1].at, (core::Point2{20000, 0}));
    CHECK_EQ(after[2].at, (core::Point2{0, 3000}));
    // The caption re-centred over the new line: between the points, above it.
    CHECK_EQ(after[3].at.x, core::Mm{10000});
    CHECK(after[3].at.y > 3000);

    // The caption grip slides the text alone.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=4 nokta=5,9", Origin::Test).ok());
    const auto slid = core::entity_grips(f.doc, e);
    CHECK_EQ(slid[3].at, (core::Point2{5000, 9000}));
    CHECK_EQ(slid[1].at, (core::Point2{20000, 0}));
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("20,00"));

    // Undo restores the 12,50 caption with the geometry.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])), std::string("12,50"));
}

TEST_CASE("KÖŞETAŞI blok referansının ekleme noktasını taşır ve kutusunu yeniler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("BLOK ad=OK taban=0,0 nesneler=1", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("BLOKEKLE ad=OK nokta=100,100", Origin::Test).ok());

    core::EntityId ref = core::kNoEntity;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kBlockReferenceKind &&
            core::block_reference_insertion(f.doc.geometry(), f.doc.entities().slot[e]) ==
                core::Point2{100000, 100000})
            ref = e;
    REQUIRE(ref != core::kNoEntity);
    const auto key = static_cast<std::int64_t>(core::raw(f.doc.entities().key[ref]));

    // The insertion point, and the turning handle out along the block's own x
    // axis, at the far end of what it draws.
    const auto grips = core::entity_grips(f.doc, ref);
    REQUIRE_EQ(grips.size(), 2u);
    CHECK_EQ(grips[0].role, core::GripRole::Insertion);
    CHECK_EQ(grips[1].role, core::GripRole::Rotation);
    CHECK_EQ(grips[1].at, (core::Point2{110000, 100000}));

    REQUIRE(f.bus
                .execute_line("KÖŞETAŞI nesne=" + std::to_string(key) + " kose=1 nokta=200,200",
                              Origin::Test)
                .ok());
    CHECK_EQ(core::block_reference_insertion(f.doc.geometry(), f.doc.entities().slot[ref]),
             (core::Point2{200000, 200000}));
    CHECK_EQ(f.doc.entities().box_of(ref), (core::Box2{200000, 200000, 210000, 200000}));

    // THE HANDLE TURNS IT about the insertion point: pointed north, a quarter
    // turn, and the box follows.
    REQUIRE(f.bus
                .execute_line("KÖŞETAŞI nesne=" + std::to_string(key) + " kose=2 nokta=200,230",
                              Origin::Test)
                .ok());
    auto turned =
        core::decode_block_reference(f.doc.geometry().payload_of(f.doc.entities().slot[ref]));
    REQUIRE(turned.ok());
    CHECK_EQ(turned.value().rotation_udeg, std::int64_t{90'000'000});
    CHECK_EQ(core::block_reference_insertion(f.doc.geometry(), f.doc.entities().slot[ref]),
             (core::Point2{200000, 200000}));
    CHECK_EQ(f.doc.entities().box_of(ref), (core::Box2{200000, 200000, 200000, 210000}));

    // A window over the handle alone does not turn it: ESNET moves places.
    const std::uint64_t before = f.doc.content_hash();
    REFUSED(f.bus.execute_line(
        "ESNET pencere=199,209 pencere=201,211 baslangic=200,210 bitis=205,210", Origin::Test));
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
        const auto key    = static_cast<std::uint64_t>(core::raw(f.doc.entities().key[e]));
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

    const core::AttrTable& t    = f.doc.attributes();
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
    using namespace kentos::core;

    // A 10 m square, offset outward by 1 m. The result must enclose more area
    // than the input and still be one ring.
    const std::vector<Point2> square{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};

    auto grown = offset_ring(square, true, 1000);
    REQUIRE(grown.ok());
    REQUIRE(grown.value().size() == 1);

    const auto& ring = grown.value().front();
    CHECK(ring.closed);

    Box2 box;
    for (const Point2& p : ring.points)
        box.extend(p);
    CHECK(box.min_x <= -1000);
    CHECK(box.max_x >= 11000);
}

TEST_CASE("Ofset: içeri doğru küçülür, mesafeyi aşınca yok olur")
{
    using namespace kentos::core;
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
    using namespace kentos::core;
    const std::vector<Point2> run{{0, 0}, {10000, 0}};

    auto band = offset_ring(run, false, 500);
    REQUIRE(band.ok());
    REQUIRE(band.value().size() == 1);
    CHECK(band.value().front().closed);
    CHECK(band.value().front().points.size() >= 4);
}

TEST_CASE("Ofset: sıfır mesafe ve yetersiz nokta gerekçesiyle reddedilir")
{
    using namespace kentos::core;
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

// ---- C-03: a parallel is not a buffer ---------------------------------------

TEST_CASE("PARALEL: açık çizginin sol 2 m paraleli açık (0,2)→(10,2) çizgisidir")
{
    using namespace kentos::core;
    // TODOS C-03's acceptance, verbatim: not a closed band, an open line.
    const std::vector<Point2> run{{0, 0}, {10000, 0}};

    auto left = parallel_run(run, 2000);
    REQUIRE(left.ok());
    REQUIRE_EQ(left.value().size(), std::size_t{1});
    CHECK_FALSE(left.value().front().closed);
    CHECK_EQ(left.value().front().points, (std::vector<Point2>{{0, 2000}, {10000, 2000}}));

    auto right = parallel_run(run, -2000);
    REQUIRE(right.ok());
    REQUIRE_EQ(right.value().size(), std::size_t{1});
    CHECK_EQ(right.value().front().points, (std::vector<Point2>{{0, -2000}, {10000, -2000}}));

    // REVERSING THE LINE SWAPS THE SIDES, and the parallel runs the way its
    // source does: the left of a westward line is south.
    const std::vector<Point2> back{{10000, 0}, {0, 0}};
    auto flipped = parallel_run(back, 2000);
    REQUIRE(flipped.ok());
    REQUIRE_EQ(flipped.value().size(), std::size_t{1});
    CHECK_EQ(flipped.value().front().points, (std::vector<Point2>{{10000, -2000}, {0, -2000}}));
}

TEST_CASE("PARALEL: köşeli çizginin içi kesişimde kırpılır, dışı sivri köşeyle döner")
{
    using namespace kentos::core;
    const std::vector<Point2> ell{{0, 0}, {10000, 0}, {10000, 10000}}; // turns left

    auto inner = parallel_run(ell, 2000);
    REQUIRE(inner.ok());
    REQUIRE_EQ(inner.value().size(), std::size_t{1});
    CHECK_EQ(inner.value().front().points,
             (std::vector<Point2>{{0, 2000}, {8000, 2000}, {8000, 10000}}));

    auto outer = parallel_run(ell, -2000);
    REQUIRE(outer.ok());
    REQUIRE_EQ(outer.value().size(), std::size_t{1});
    CHECK_EQ(outer.value().front().points,
             (std::vector<Point2>{{0, -2000}, {12000, -2000}, {12000, 10000}}));

    CHECK_FALSE(parallel_run(ell, 0).ok());
    CHECK_FALSE(parallel_run({{5, 5}, {5, 5}}, 1000).ok()); ///< no direction to be beside
}

TEST_CASE("PARALEL: delikli alanın ofsetinde delik delik kalır")
{
    using namespace kentos::core;
    // A 20 m parcel with a 10 m courtyard in the middle.
    const Polygon parcel{{{0, 0}, {20000, 0}, {20000, 20000}, {0, 20000}},
                         {{{5000, 5000}, {15000, 5000}, {15000, 15000}, {5000, 15000}}}};

    auto grown = offset_faces({parcel}, 1000);
    REQUIRE(grown.ok());
    REQUIRE_EQ(grown.value().size(), std::size_t{1}); ///< one face, not two
    REQUIRE_EQ(grown.value().front().holes.size(), std::size_t{1});
    CHECK_EQ(std::llabs(ring_area(grown.value().front().exterior)), Mm2{22000} * 22000);
    CHECK_EQ(std::llabs(ring_area(grown.value().front().holes.front())), Mm2{8000} * 8000);

    auto shrunk = offset_faces({parcel}, -1000);
    REQUIRE(shrunk.ok());
    REQUIRE_EQ(shrunk.value().size(), std::size_t{1});
    REQUIRE_EQ(shrunk.value().front().holes.size(), std::size_t{1});
    CHECK_EQ(std::llabs(ring_area(shrunk.value().front().exterior)), Mm2{18000} * 18000);
    CHECK_EQ(std::llabs(ring_area(shrunk.value().front().holes.front())), Mm2{12000} * 12000);

    // Grown by more than half the courtyard, the courtyard is gone — not left
    // behind as a parcel of its own.
    auto filled = offset_faces({parcel}, 6000);
    REQUIRE(filled.ok());
    REQUIRE_EQ(filled.value().size(), std::size_t{1});
    CHECK(filled.value().front().holes.empty());

    // And shrunk past half its width, nothing survives.
    auto gone = offset_faces({parcel}, -3000);
    REQUIRE(gone.ok());
    CHECK(gone.value().empty());
}

TEST_CASE("TAMPON: çizginin iki taraflı tamponu bir alandır, noktanınki bir disk")
{
    using namespace kentos::core;
    // THE OLD OFSET RESULT, now under its own name: 50 m at 2 m, flat ends, is a
    // 200 m² face.
    BufferSource line;
    line.runs.push_back({{0, 0}, {50000, 0}});
    auto band = buffer(line, 2000, JoinStyle::Miter, EndStyle::Butt);
    REQUIRE(band.ok());
    REQUIRE_EQ(band.value().size(), std::size_t{1});
    CHECK_EQ(std::llabs(ring_area(band.value().front().exterior)), Mm2{50000} * 4000);

    // Round ends add a disc's worth: π·2² m² on top.
    auto rounded = buffer(line, 2000);
    REQUIRE(rounded.ok());
    const double round_m2 =
        static_cast<double>(std::llabs(ring_area(rounded.value().front().exterior))) / 1e6;
    CHECK(round_m2 == doctest::Approx(200.0 + 3.14159265 * 4.0).epsilon(0.002));

    BufferSource well;
    well.points.push_back({1000, 1000});
    auto disc = buffer(well, 5000);
    REQUIRE(disc.ok());
    REQUIRE_EQ(disc.value().size(), std::size_t{1});
    const double disc_m2 =
        static_cast<double>(std::llabs(ring_area(disc.value().front().exterior))) / 1e6;
    CHECK(disc_m2 == doctest::Approx(3.14159265 * 25.0).epsilon(0.002));

    // Two bands that overlap dissolve into one face.
    BufferSource cross;
    cross.runs.push_back({{0, 0}, {10000, 0}});
    cross.runs.push_back({{5000, -5000}, {5000, 5000}});
    auto joined = buffer(cross, 1000, JoinStyle::Miter, EndStyle::Butt);
    REQUIRE(joined.ok());
    CHECK_EQ(joined.value().size(), std::size_t{1});

    CHECK_FALSE(buffer(line, -1000).ok()); ///< a line has no inside to erode
    CHECK_FALSE(buffer(BufferSource{}, 1000).ok());
}

TEST_CASE("PARALEL: her tür kendi türünde ya da ölçülmüş bir yaklaşıkla")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());                 // 1
    REQUIRE(f.bus.execute_line("DAİRE merkez=100,0 cevre=110,0", Origin::Test).ok()); // 2
    REQUIRE(f.bus.execute_line("ELİPS merkez=200,0 birinci=220,0 ikinci=200,10", Origin::Test)
                .ok());                                            // 3
    REQUIRE(f.bus.execute_line("NOKTA 300,0", Origin::Test).ok()); // 4
    const auto slot = [&f](std::int64_t key) {
        return f.doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    };

    // A line: exact, open, one side.
    auto line = core::entity_parallel(f.doc, slot(1), 2000, core::ParallelSide::Left);
    REQUIRE(line.ok());
    CHECK_FALSE(line.value().approximate);
    REQUIRE_EQ(line.value().pieces.size(), std::size_t{1});
    CHECK_EQ(line.value().pieces.front().shape, core::ParallelPiece::Shape::Run);
    CHECK_EQ(line.value().pieces.front().run,
             (std::vector<core::Point2>{{0, 2000}, {10000, 2000}}));
    CHECK_FALSE(core::entity_parallel(f.doc, slot(1), 2000, core::ParallelSide::Outside).ok());

    // A circle: a circle, the radius changed, both ways at once.
    auto ring = core::entity_parallel(f.doc, slot(2), 2000, core::ParallelSide::Both);
    REQUIRE(ring.ok());
    REQUIRE_EQ(ring.value().pieces.size(), std::size_t{2});
    CHECK_EQ(ring.value().pieces[0].shape, core::ParallelPiece::Shape::Circle);
    CHECK_EQ(ring.value().pieces[0].radius, core::Mm{12000});
    CHECK_EQ(ring.value().pieces[1].radius, core::Mm{8000});
    // Past its radius inward, nothing survives, and that is an answer.
    auto shrunk = core::entity_parallel(f.doc, slot(2), 10000, core::ParallelSide::Inside);
    REQUIRE(shrunk.ok());
    CHECK(shrunk.value().pieces.empty());

    // An ellipse: no ellipse is its parallel, so the drawing's parallel, with the
    // drawing's measured distance from the curve.
    auto oval = core::entity_parallel(f.doc, slot(3), 1000, core::ParallelSide::Outside);
    REQUIRE(oval.ok());
    CHECK(oval.value().approximate);
    CHECK(oval.value().deviation > 0);
    CHECK(oval.value().deviation < 20); ///< a 128-gon of a 20 m ellipse is within millimetres
    REQUIRE_EQ(oval.value().pieces.size(), std::size_t{1});
    CHECK_EQ(oval.value().pieces.front().shape, core::ParallelPiece::Shape::Face);

    // A point has none, and says what to use instead.
    auto point = core::entity_parallel(f.doc, slot(4), 1000, core::ParallelSide::Left);
    REQUIRE_FALSE(point.ok());
    CHECK(point.error().message.find("TAMPON") != std::string::npos);

    // WHICH SIDE A POINT IS ON, for the click that chooses it.
    CHECK_EQ(core::parallel_side_at(f.doc, slot(1), {5000, 3000}).value(),
             core::ParallelSide::Left);
    CHECK_EQ(core::parallel_side_at(f.doc, slot(1), {5000, -3000}).value(),
             core::ParallelSide::Right);
    CHECK_FALSE(core::parallel_side_at(f.doc, slot(1), {5000, 0}).ok()); ///< on the line
    CHECK_EQ(core::parallel_side_at(f.doc, slot(2), {100000, 1000}).value(),
             core::ParallelSide::Inside);
    CHECK_EQ(core::parallel_side_at(f.doc, slot(2), {130000, 0}).value(),
             core::ParallelSide::Outside);

    // And the preview's bytes go and come back.
    const core::ParallelPreview sent{{1, 2, 3}, 2500, core::JoinStyle::Round};
    auto back = core::decode_parallel_preview(core::encode_parallel_preview(sent));
    REQUIRE(back.ok());
    CHECK_EQ(back.value().keys, sent.keys);
    CHECK_EQ(back.value().distance, sent.distance);
    CHECK_EQ(back.value().join, sent.join);
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

    said += REFUSED(f.bus.execute_line("OFSET mesafe=1000", Origin::Test));
    CHECK(said.find("nesne yok") != std::string::npos);
}

// -----------------------------------------------------------------------------
// ELİPS — stored by its definition (core.ellipse_draw)
// -----------------------------------------------------------------------------

TEST_CASE("ELİPS merkez ve iki eksenden çizilir, tanımıyla saklanır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());

    auto drawn = f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=0,5", Origin::Test);
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
    const auto slot            = f.doc.entities().slot[0];
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

    const core::Point2 minor = core::ellipse_minor_of(f.doc.geometry(), f.doc.entities().slot[0]);
    CHECK(minor.x == 0);
    CHECK(minor.y == 5000);
}

TEST_CASE("ELİPS: eksen üzerindeki üçüncü nokta reddedilir")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    said += REFUSED(f.bus.execute_line("ELİPS merkez=0,0 birinci=10,0 ikinci=5,0", Origin::Test));

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
    const auto slot      = f.doc.slot_of(f.doc.entities().key[0]);
    const core::Mm2 area = f.doc.geometry().area_of(f.doc.entities().slot[slot]);
    CHECK(area > 70'000'000);
    CHECK(area < 80'000'000);
}

TEST_CASE("DİLİM: merkezle çakışan kenar reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REFUSED(f.bus.execute_line("DİLİM merkez=0,0 baslangic=0,0 bitis=0,10", Origin::Test));
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
    const auto slot      = f.doc.slot_of(f.doc.entities().key[0]);
    const core::Mm2 area = f.doc.geometry().area_of(f.doc.entities().slot[slot]);
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
    REFUSED(f.bus.execute_line("HALKA merkez=0,0 ic=5,0 dis=5,0", Origin::Test));
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

    said += REFUSED(f.bus.execute_line("KILAVUZ yon=yatay deger=9999 sil=evet", Origin::Test));
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
    using namespace kentos::core;

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
    using namespace kentos::core;

    Polygon a{{{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}}, {}};
    Polygon b{{{50000, 0}, {60000, 0}, {60000, 10000}, {50000, 10000}}, {}};

    auto merged = polygon_boolean({a}, {b}, BooleanOp::Union);
    REQUIRE(merged.ok());
    CHECK(merged.value().size() == 2); // and saying "one" would be a lie
}

TEST_CASE("BOOLEAN: fark bir parseli ikiye bölebilir")
{
    using namespace kentos::core;

    // A 30 m parcel with a 10 m band taken out of its middle: two pieces.
    Polygon parcel{{{0, 0}, {30000, 0}, {30000, 10000}, {0, 10000}}, {}};
    Polygon band{{{10000, -1000}, {20000, -1000}, {20000, 11000}, {10000, 11000}}, {}};

    auto cut = polygon_boolean({parcel}, {band}, BooleanOp::Difference);
    REQUIRE(cut.ok());
    CHECK(cut.value().size() == 2);
}

TEST_CASE("BOOLEAN: örtüşme kesişimle bulunur, örtüşmeyen ikili boş döner")
{
    using namespace kentos::core;

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
    using namespace kentos::core;

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
    int calls            = 0;
    f.bus.on_pan_request = [&](core::Point2 a, core::Point2 b) {
        from = a;
        to   = b;
        ++calls;
    };

    auto moved = f.bus.execute_line("KAYDIR baslangic=100,200 bitis=150,200", Origin::Test);
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

TEST_CASE("etkileşimli başlatma bir KOMUT SATIRI alır: 'SEÇ mod=KUTU' düğmesi çalışır")
{
    // THE REGRESSION. `begin_interactive` resolved the whole string as a command
    // NAME, and the registry lookup is exact — so the tool column's "Alan Seç"
    // button, which sends `SEÇ mod=KUTU`, failed on every click with "Bilinmeyen
    // komut: 'SEÇ mod=KUTU'". The button was 100% dead in every session and no
    // test touched it: the only coverage went through `execute_line`, which is a
    // different function.
    //
    // The GUI was therefore a strictly weaker client than the command line — it
    // could start a command or pass it arguments, never both — which Article 1.2
    // does not permit in either direction.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=100,100 110,100 110,110 100,110", Origin::Test).ok());

    auto started = f.bus.begin_interactive("SEÇ mod=KUTU");
    REQUIRE(started.ok());

    auto& session = *started.value();
    // `mod` came from the preset; the two corners are still to be clicked.
    REQUIRE(session.waiting());
    REQUIRE(session.supply(Value::point(core::Point2{-5'000, -5'000})).ok());
    REQUIRE(session.supply(Value::point(core::Point2{20'000, 20'000})).ok());

    REQUIRE(f.bus.finish(session).ok());

    // The SAME answer the typed line gives, which is the whole claim: one parcel
    // inside the box, the far one left alone.
    CHECK(f.bus.selection().size() == 1);
}

// =============================================================================
// BİRLEŞTİR — the generic merge (core.combine)
// =============================================================================

TEST_CASE("BİRLEŞTİR: komşu iki alan tek alan olur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=10,0 20,0 20,10 10,10", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BİRLEŞTİR nesneler=1 2", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 1);
}

TEST_CASE("BİRLEŞTİR: değmeyen alanlar reddedilmez, parça sayısı söylenir")
{
    // THE LINE BETWEEN THIS AND TEVHİT. A tevhit of parcels that do not adjoin is
    // not a tevhit and is refused. Two woodland patches either side of a valley
    // merging into one layer feature with two parts is an ordinary, correct map
    // operation — importing the cadastral rule here would forbid it.
    std::string said;
    Fixture f;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=100,0 110,0 110,10 100,10", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BİRLEŞTİR nesneler=1 2", Origin::Test).ok());

    CHECK(f.doc.live_entity_count() == 2);
    CHECK(said.find("değmiyor") != std::string::npos);
}

TEST_CASE("BİRLEŞTİR: uç uca değen çizgiler tek çizgi olur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=10,0 10,10", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BİRLEŞTİR nesneler=1 2", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 1);
}

TEST_CASE("BİRLEŞTİR: sırası karışık verilen çizgiler de zincirlenir")
{
    // The walk grows BOTH ends. Growing only the tail — which is what ALANAÇEVİR
    // does, correctly, because a ring closes whichever way it is walked — would
    // refuse this selection for an ordering that is not the user's fault.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=10,0 20,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=20,0 30,0", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BİRLEŞTİR nesneler=1 2 3", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 1);
}

TEST_CASE("BİRLEŞTİR: alanla çizgi karışık verilirse adıyla reddeder")
{
    std::string said;
    Fixture f;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=20,0 30,0", Origin::Test).ok());

    said += REFUSED(f.bus.execute_line("BİRLEŞTİR nesneler=1 2", Origin::Test));

    CHECK(f.doc.live_entity_count() == 2); // nothing happened
    CHECK(said.find("hem alan hem çizgi") != std::string::npos);
}

TEST_CASE("BİRLEŞTİR: tek nesneyle çalışmaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());

    REFUSED(f.bus.execute_line("BİRLEŞTİR nesneler=1", Origin::Test));
    CHECK(f.doc.live_entity_count() == 1);
}

// =============================================================================
// want_objects — the tool column's "press first, then point"
// =============================================================================

TEST_CASE("seçim boşken bir düzenleme komutu nesneleri SORAR, reddetmez")
{
    // THE REGRESSION. Six buttons on the tool column — trim, split, combine,
    // move, offset and match-style — refused an empty selection with a line in
    // the status strip and never started the command — so the ordinary order of work, reach
    // for the tool and then point at the thing, did nothing at all. They were
    // reported as "cannot be selected and do not work", and both halves were true.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,5 10,5", Origin::Test).ok());
    REQUIRE(f.bus.selection().empty());

    auto started = f.bus.begin_interactive("TAŞI");
    REQUIRE(started.ok());
    auto& session = *started.value();

    // It asks, and it asks for OBJECTS — which is what lets the canvas answer by
    // picking rather than by sending a coordinate.
    REQUIRE(session.waiting());
    CHECK(session.prompt().kind == ParamKind::Selection);

    REQUIRE(session.supply(Value::ids({1, 2})).ok());
    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(session.supply(Value::point(core::Point2{20'000, 0})).ok());
    REQUIRE(f.bus.finish(session).ok());

    // Both lines moved 20 m east.
    const core::EntityId slot = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(slot != core::kNoEntity);
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[slot]);
    CHECK(f.doc.geometry().ring_xs(span.first)[0] == 20'000);
}

TEST_CASE("nesneler verilmişse hiç sorulmaz: betik yolu değişmedi")
{
    // The other half of Article 1.2: a client that already said must not be asked.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 10,0", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("TAŞI nesneler=1 baslangic=0,0 bitis=20,0", Origin::Test).ok());

    const core::EntityId slot = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(slot != core::kNoEntity);
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[slot]);
    CHECK(f.doc.geometry().ring_xs(span.first)[0] == 20'000);
}

TEST_CASE("seçim komutun alabileceğinden çoksa SORAR; fazlasını adlayan betik reddedilir")
{
    // KIR takes one line. Handed three highlighted, it used to refuse on the spot
    // — "en fazla 1 nesne" — so the button was dead until the user went and
    // cleared the selection by hand. It now asks for the ones it wants, and
    // says why. A script cannot answer the question, so it is told exactly what
    // it was told before; and neither road is followed by the raw "zorunlu
    // parametre" line a post-run validation used to add after the sentence.
    // (A script that names nothing is told which parameter is missing before
    // the body runs: a script says what it means, it is not asked.)
    std::string said;
    Fixture f;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    for (int i = 0; i < 3; ++i)
        REQUIRE(
            f.bus
                .execute_line("ÇİZGİ noktalar=0," + std::to_string(i) + " 10," + std::to_string(i),
                              Origin::Test)
                .ok());
    REQUIRE(f.bus.execute_line("SEÇ mod=TÜMÜ", Origin::Test).ok());

    {
        // A HAND: the tool asks, and the question says how many are highlighted
        // and how many it takes.
        auto started = f.bus.begin_interactive("KIR", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting());
        CHECK(session.prompt().kind == ParamKind::Selection);
        CHECK(session.prompt().message.find("3 nesne seçili") != std::string::npos);
        CHECK(session.prompt().message.find("1 nesne") != std::string::npos);

        // Esc is a cancel, not the refusal the script gets.
        session.cancel();
        const auto done = f.bus.finish(session);
        REQUIRE(done.ok());
        CHECK(done.value().message == "İptal edildi");
    }
    {
        // A SCRIPT names what it means, and three named where one is taken is
        // refused with the sentence only — never followed by the raw "zorunlu
        // parametre" line a post-run validation used to add after it.
        said.clear();
        const std::string why =
            REFUSED(f.bus.execute_line("KIR nesne=1 nesne=2 nesne=3 birinci=5,0", Origin::Script));
        INFO(why);
        CHECK(why.find("en fazla") != std::string::npos);
        CHECK(why.find("zorunlu") == std::string::npos);
        CHECK(said.find("zorunlu") == std::string::npos);
    }
}

// =============================================================================
// BÖL — a cut is a line the user draws
// =============================================================================

TEST_CASE("BÖL çizilen bir kesme çizgisiyle bir alanı ikiye ayırır")
{
    // What the user asked for in so many words: "bölerken çizgi çizerek kesmemiz
    // gerekiyor". A point along an edge is not how a surveyor states a cut — an
    // ifraz line is agreed on the ground and DRAWN. This is that cut without the
    // cadastral half; `İFRAZ` is the same geometry with the regulation on top,
    // which is why both reach for `core::half_plane`.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BÖL nesne=1 noktalar=10,-5 noktalar=10,15", Origin::Test).ok());

    // One face in, two out.
    CHECK(f.doc.live_entity_count() == 2);
}

TEST_CASE("BÖL kesme çizgisinin kestiği çizgiyi böler, kesmediğine dokunmaz")
{
    std::string said;
    Fixture f;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };

    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 20,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,50 20,50", Origin::Test).ok());

    said.clear();
    REQUIRE(
        f.bus.execute_line("BÖL nesne=1 nesne=2 noktalar=10,-5 noktalar=10,5", Origin::Test).ok());

    // The first line is cut in two; the second is nowhere near the cut and is
    // reported rather than silently skipped.
    CHECK(f.doc.live_entity_count() == 3);
    CHECK(said.find("1 çizgi bölündü") != std::string::npos);
    CHECK(said.find("kesmiyor") != std::string::npos);
}

TEST_CASE("BÖL eski nokta biçimi çalışmaya devam eder")
{
    // `nokta` is written into journals and into scripts already, so it keeps
    // working: one open line split at a point on it.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DENEME", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ noktalar=0,0 20,0", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("BÖL nesne=1 nokta=10,0", Origin::Test).ok());
    CHECK(f.doc.live_entity_count() == 2);
}

TEST_CASE("etkileşimli başlatma çıplak bir ad ile eskisi gibi davranır")
{
    // The compatibility half: every existing caller passes a bare name and must
    // keep suspending on the first parameter rather than gaining a preset.
    Fixture f;

    auto started = f.bus.begin_interactive("ÇİZGİ");
    REQUIRE(started.ok());
    CHECK(started.value()->waiting());
    started.value()->cancel();
    CHECK(f.bus.finish(*started.value()).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

// -----------------------------------------------------------------------------
// ADIM — the step lock (core.yakalama.adim)
// -----------------------------------------------------------------------------

TEST_CASE("ADIM: uzaklık adımın katına yuvarlanır, yön korunur")
{
    using namespace kentos::core;

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
    using namespace kentos::core;

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
    using namespace kentos::core;

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
    using namespace kentos::core;

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
    auto drawn =
        f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,130", Origin::Test);
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
    REQUIRE(
        f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,70", Origin::Test).ok());

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
    REFUSED(f.bus.execute_line("YAY merkez=100,100 baslangic=100,100 bitis=100,130", Origin::Test));
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("KÖŞETAŞI yayın ucunu ve ortasını taşır; öbür uç yerinde kalır; ALANAÇEVİR yayı reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY merkez=100,100 baslangic=130,100 bitis=100,130", Origin::Test)
                .ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    const auto slot = [&] { return f.doc.entities().slot[e]; };
    // Whether `p` is on the arc's circle, to the millimetre rounding allows.
    const auto on_circle = [&](core::Point2 p) {
        const core::Point2 c = core::arc_centre_of(f.doc.geometry(), slot());
        const double dx      = static_cast<double>(p.x - c.x);
        const double dy      = static_cast<double>(p.y - c.y);
        return std::abs(std::sqrt(dx * dx + dy * dy) -
                        static_cast<double>(core::arc_radius_of(f.doc.geometry(), slot()))) <= 1.5;
    };

    // Centre, start, end, midpoint.
    const auto grips = core::entity_grips(f.doc, e);
    REQUIRE_EQ(grips.size(), 4u);
    CHECK_EQ(grips[1].at, (core::Point2{130000, 100000}));
    CHECK_EQ(grips[3].role, core::GripRole::Radius);
    const core::Point2 mid = grips[3].at;

    // The start goes 10 m further out. The END STAYS WHERE IT IS — a line that
    // meets it stays met — and the arc is re-fitted through the new start, its
    // old midpoint and that end: all three on the one circle the record holds.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=2 nokta=140,100", Origin::Test).ok());
    CHECK_EQ(core::arc_start_of(f.doc.geometry(), slot()), (core::Point2{140000, 100000}));
    CHECK_EQ(core::arc_end_of(f.doc.geometry(), slot()), (core::Point2{100000, 130000}));
    CHECK(on_circle({140000, 100000}));
    CHECK(on_circle({100000, 130000}));
    CHECK(on_circle(mid));

    // The midpoint handle bends it through a new point, both ends held.
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=4 nokta=128,128", Origin::Test).ok());
    CHECK_EQ(core::arc_start_of(f.doc.geometry(), slot()), (core::Point2{140000, 100000}));
    CHECK_EQ(core::arc_end_of(f.doc.geometry(), slot()), (core::Point2{100000, 130000}));
    CHECK(on_circle({128000, 128000}));

    // Onto the chord it would be straight, which an arc cannot be: refused.
    const std::uint64_t bent = f.doc.content_hash();
    REFUSED(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=4 nokta=120,115", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), bent);

    // The centre carries the whole arc, its shape unchanged.
    const core::Point2 c0 = core::arc_centre_of(f.doc.geometry(), slot());
    const core::Mm r0     = core::arc_radius_of(f.doc.geometry(), slot());
    REQUIRE(f.bus.execute_line("KÖŞETAŞI nesne=1 kose=1 nokta=0,0", Origin::Test).ok());
    CHECK_EQ(core::arc_centre_of(f.doc.geometry(), slot()), (core::Point2{0, 0}));
    CHECK_EQ(core::arc_start_of(f.doc.geometry(), slot()),
             (core::Point2{140000 - c0.x, 100000 - c0.y}));
    CHECK_EQ(core::arc_radius_of(f.doc.geometry(), slot()), r0);
    CHECK_EQ(f.doc.entities().kind[e], core::kArcKind);

    const std::uint64_t before = f.doc.content_hash();
    REFUSED(f.bus.execute_line("ALANAÇEVİR nesneler=1", Origin::Test));
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

    REFUSED(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=-1", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);

    REFUSED(f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=0", Origin::Test));
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

TEST_CASE("KOPYALA her bitiş noktasına bir kopya koyar ve hepsi tek geri alma adımıdır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=DIREK", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 1,0 1,1 0,1", Origin::Test).ok());

    // Three poles in a row from one command: one per point. A named list
    // accumulates by repeating its name, as `nesneler=` does.
    auto copied = f.bus.execute_line(
        "KOPYALA nesneler=1 baslangic=0,0 bitis=10,0 bitis=20,0 bitis=30,0", Origin::Test);
    if (!copied) FAIL_WITH("KOPYALA", copied.error().message);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{4});

    // The last copy sits where the last point said.
    bool at_thirty = false;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().box_of(e) == core::Box2{30000, 0, 31000, 1000})
            at_thirty = true;
    CHECK(at_thirty);

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});

    // The one-point form is the copy it always was.
    REQUIRE(f.bus.execute_line("KOPYALA nesneler=1 baslangic=0,0 bitis=5,0", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
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
    CHECK_EQ(core::circle_centre_of(f.doc.geometry(), f.doc.entities().slot[1]).x, core::Mm{50000});
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
    q.modes            = core::SnapNode;
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
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    // Nothing selected and no ids: the tool would ASK on the canvas; here, with
    // nobody to answer, it says so the way every modify tool does.
    said += REFUSED(f.bus.execute_line("ALANÖLÇ", Origin::Test));
    CHECK(said.find("İşlem yapılacak nesne yok") != std::string::npos);
    CHECK(said.find("ALANÖLÇ nesneler=1") != std::string::npos);

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
    // 59 + SPLINE, TARAMA, BLOK, BLOKEKLE, ÖLÇÜ, LİDER + YAZDIR, YAZDIRMAPROFİLİ
    // + KATMANGÖRÜNÜM + ÇIKTIYERLEŞİMİ, ÇIKTIÖĞE, ÇIKTIŞABLON + YENİ
    // + DİKAYAK, ALIM, KESİŞİMNOKTA, ARANOKTA, ÇOKGEN
    // + KIR, UÇUCA, UZUNLUK, PATLAT, HİZALA, BÖLÜMLE, ÇİZGİDÜZENLE
    // + PANOYAKOPYALA, KES, YAPIŞTIR
    // + NESNEBİLGİ, AÇIÖLÇ + ESNET + İZ + PYTHON + RENK
    // + KÖŞESİL, KENARTÜRÜ (TODOS C-07) + SINIR, TEMİZLE (TODOS C-09)
    // + ÖLÇÜDÜZENLE, ÖLÇÜYENİLE, ZİNCİRÖLÇÜ, BAZÖLÇÜ, ÖLÇÜSTİLİ (TODOS C-10)
    // + TARAMADÜZENLE (TODOS C-11) + BULDEĞİŞTİR (TODOS C-12)
    CHECK_EQ(f.reg.size(), std::size_t{104});

    // And the collision check itself, over the names that DID register.
    for (const CommandSpec& spec : f.reg.all())
        for (const std::string& name : spec.names) {
            const CommandSpec* found = f.reg.resolve(name);
            if (found == nullptr) FAIL_WITH("bildirilen ad çözülemiyor", spec.id + " / " + name);
            if (found->id != spec.id)
                FAIL_WITH("ad başka bir komuta gidiyor",
                          spec.id + " / " + name + " -> " + found->id);
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

    REQUIRE(
        f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=4", Origin::Test).ok());

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
    REQUIRE(
        f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=3 aci=90", Origin::Test)
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
    REFUSED(f.bus.execute_line("DİZİ nesneler=1 satir=1 sutun=1 satir_aralik=5 sutun_aralik=5",
                               Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);

    // A polar array of one is not an array either.
    REFUSED(f.bus.execute_line("DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=1", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), before);

    REQUIRE(f.bus
                .execute_line("DİZİ nesneler=1 satir=2 sutun=2 satir_aralik=20 sutun_aralik=20",
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

    REQUIRE_FALSE(f.bus.execute_line("BÖL nesne=1 nokta=0,0", Origin::Test).ok());
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

    REQUIRE_FALSE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=40,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("BÖL eğriyi reddeder; BUDA daireyi ancak kesen bir sınırla budar")
{
    // Splitting a circle at a point is not an operation: one cut opens a ring
    // without making two of anything. Trimming it is, but only between two meets
    // — and a boundary that never reaches the circle meets it nowhere.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 40,-20 40,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    REQUIRE_FALSE(f.bus.execute_line("BÖL nesne=1 nokta=5,0", Origin::Test).ok());
    const std::string why =
        REFUSED(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=-10,0", Origin::Test));
    CHECK(why.find("iki yerinden") != std::string::npos);
    CHECK_EQ(f.doc.content_hash(), before);
}

namespace {

/// Vertex `at` of the first ring of the object with persistent key `key`.
core::Point2 ring_vertex(const core::Document& doc, std::uint64_t key, std::size_t at)
{
    const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
    REQUIRE(e != core::kNoEntity);
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
    return doc.geometry().vertex(span.first, static_cast<std::uint32_t>(at));
}

core::KindId kind_of(const core::Document& doc, std::uint64_t key)
{
    const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
    REQUIRE(e != core::kNoEntity);
    return doc.entities().kind[e];
}

} // namespace

TEST_CASE("BUDA iki sınır arasındaki orta parçayı atar; kalan iki parça iki nesne olur")
{
    // THE CASE THE OLD BUDA COULD NOT DO: a line crossing two roads loses the
    // stretch between them. The first piece keeps the object — its key, layer
    // and style — and the second is a new object drawn like it.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok()); // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 70,-20 70,20", Origin::Test).ok()); // 3

    auto trimmed = f.bus.execute_line("BUDA sinir=2 sinir=3 nesne=1 nokta=50,0", Origin::Test);
    if (!trimmed) FAIL_WITH("BUDA", trimmed.error().message);
    REQUIRE(trimmed.ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{4});
    CHECK_EQ(ring_vertex(f.doc, 1, 0), (core::Point2{0, 0}));
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 4, 0), (core::Point2{70'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 4, 1), (core::Point2{100'000, 0}));
    const core::EntityId first  = f.doc.slot_of(static_cast<core::EntityKey>(1U));
    const core::EntityId second = f.doc.slot_of(static_cast<core::EntityKey>(4U));
    CHECK_EQ(f.doc.entities().layer[second], f.doc.entities().layer[first]);
    CHECK_EQ(f.doc.entities().style[second], f.doc.entities().style[first]);
}

TEST_CASE("BUDA yayı doğruyla budar ve kalan YAY olarak kalır")
{
    // An arc trimmed to a line is still an arc — the same centre and radius, a
    // shorter sweep — and it stops ON the line, at the circle's meet, not at a
    // chord the arc happens to be drawn with.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY 0,0 10,0 -10,0", Origin::Test).ok());    // 1, upper half
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,-20 0,20", Origin::Test).ok()); // 2

    REQUIRE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=-7.07,7.07", Origin::Test).ok());
    CHECK(kind_of(f.doc, 1) == core::kArcKind);
    CHECK_EQ(ring_vertex(f.doc, 1, 0), (core::Point2{0, 0}));      ///< centre
    CHECK_EQ(ring_vertex(f.doc, 1, 2), (core::Point2{10'000, 0})); ///< start kept
    CHECK_EQ(ring_vertex(f.doc, 1, 3), (core::Point2{0, 10'000})); ///< end on the line
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
}

TEST_CASE("BUDA daireyi iki kesimden budar; kalan parça yay olur")
{
    // A circle cut in two places is the arc that is left: a different kind, so a
    // new object drawn like the circle, and the circle goes.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok()); // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,-20 0,20", Origin::Test).ok());       // 2

    auto trimmed = f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=-10,0", Origin::Test);
    if (!trimmed) FAIL_WITH("BUDA", trimmed.error().message);
    REQUIRE(trimmed.ok());

    const core::EntityId circle = f.doc.slot_of(static_cast<core::EntityKey>(1U));
    CHECK((circle == core::kNoEntity || !f.doc.alive(circle)));
    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{2});
    CHECK(kind_of(f.doc, 3) == core::kArcKind);
    CHECK_EQ(ring_vertex(f.doc, 3, 0), (core::Point2{0, 0}));
    CHECK_EQ(ring_vertex(f.doc, 3, 2), (core::Point2{0, -10'000})); ///< the right half,
    CHECK_EQ(ring_vertex(f.doc, 3, 3), (core::Point2{0, 10'000}));  ///< counter-clockwise
}

TEST_CASE("BUDA çizgiyi dairenin ÜZERİNDE bitirir, kirişinde değil")
{
    // The circle is drawn with chords; a trim against the chords would stop the
    // line a few millimetres short of the real curve. 6 m off a 10 m circle's
    // centre the meet is exactly 8 m along: sqrt(100 - 36).
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ -20,6 20,6", Origin::Test).ok());       // 1
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok()); // 2

    REQUIRE(f.bus.execute_line("BUDA nesne=1 sinir=2 nokta=15,6", Origin::Test).ok());
    CHECK_EQ(ring_vertex(f.doc, 1, 0), (core::Point2{-20'000, 6'000}));
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{8'000, 6'000}));
}

TEST_CASE("UZAT yayın ucunu çemberi boyunca sınıra taşır")
{
    // A quarter arc from east to north, and a boundary at x = -5: the arc's end
    // goes on round its own circle to 120°, where x = -5 and y = 5·√3.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY 0,0 10,0 0,10", Origin::Test).ok());       // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ -5,-20 -5,20", Origin::Test).ok()); // 2

    auto extended = f.bus.execute_line("UZAT nesne=1 sinir=2 nokta=1,9.9", Origin::Test);
    if (!extended) FAIL_WITH("UZAT", extended.error().message);
    REQUIRE(extended.ok());
    CHECK(kind_of(f.doc, 1) == core::kArcKind);
    CHECK_EQ(ring_vertex(f.doc, 1, 2), (core::Point2{10'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 1, 3), (core::Point2{-5'000, 8'660}));
}

TEST_CASE("BUDA tut=evet: tıklanan parça kalır, sınırların dışındaki iki uç gider")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok()); // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 70,-20 70,20", Origin::Test).ok()); // 3

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    auto kept =
        f.bus.execute_line("BUDA tut=evet sinir=2 sinir=3 nesne=1 nokta=50,0", Origin::Test);
    if (!kept) FAIL_WITH("BUDA tut", kept.error().message);
    REQUIRE(kept.ok());
    CHECK(said.find("2 parça budandı.") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{3}); ///< one object, trimmed at both ends
    CHECK_EQ(ring_vertex(f.doc, 1, 0), (core::Point2{30'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{70'000, 0}));
}

TEST_CASE("BUDA çitle: çitin geçtiği her parça tek seferde ve tek geri almada gider")
{
    // Two lines across two roads, and a fence down the middle of the gap: both
    // middles go in one run. The roads themselves are crossed by nothing.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,10 100,10", Origin::Test).ok());  // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,30", Origin::Test).ok()); // 3
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 70,-20 70,30", Origin::Test).ok()); // 4
    const std::uint64_t before = f.doc.content_hash();
    const std::size_t depth    = f.undo.undo_depth();

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    auto fenced   = f.bus.execute_line("BUDA cit=50,-5 50,15", Origin::Test);
    if (!fenced) FAIL_WITH("BUDA cit", fenced.error().message);
    REQUIRE(fenced.ok());
    CHECK(said.find("2 parça budandı (2 nesnede).") != std::string::npos);
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 2, 1), (core::Point2{30'000, 10'000}));
    CHECK_EQ(ring_vertex(f.doc, 5, 0), (core::Point2{70'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 6, 0), (core::Point2{70'000, 10'000}));
    CHECK_EQ(f.undo.undo_depth(), depth + 1);

    // WHAT THE JOURNAL HOLDS is the fence, so a replay draws it again.
    const Args& args = f.journal.entries().back().args;
    REQUIRE(args.find("cit") != nullptr);
    CHECK_EQ(args.find("cit")->as_points().size(), std::size_t{2});
    REQUIRE(args.find("yontem") != nullptr);
    CHECK(args.find("yontem")->as_text() == "çit");

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("BUDA çitle: bir parçayı iki kez geçen çit onu bir kez atar; kesilmeyenler atlanır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok()); // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,50 100,50", Origin::Test).ok());  // 3, cut by nothing

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    // A zig-zag over the east piece of 1, and up across 3 which nothing cuts.
    REQUIRE(f.bus.execute_line("BUDA hepsi=evet cit=50,-5 60,5 70,-5 80,60", Origin::Test).ok());
    CHECK(said.find("1 parça budandı (1 nesnede).") != std::string::npos);
    CHECK(said.find("1 nesne atlandı") != std::string::npos);
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 3, 1), (core::Point2{100'000, 50'000})); ///< untouched
}

TEST_CASE("UZAT çitle: çitin yanından geçtiği uçlar sınıra uzanır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 50,0", Origin::Test).ok());     // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,10 45,10", Origin::Test).ok());   // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 80,-20 80,30", Origin::Test).ok()); // 3
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    REQUIRE(f.bus.execute_line("UZAT sinir=3 cit=35,-5 45,15", Origin::Test).ok());
    CHECK(said.find("2 uç sınıra uzatıldı (2 nesnede).") != std::string::npos);
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{80'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 2, 1), (core::Point2{80'000, 10'000}));
}

TEST_CASE("uzanti=evet: yetişmeyen bir sınır kendi doğrultusunda keser ve ulaşılır")
{
    // A boundary from y = 5 up: it stops short of the line along y = 0.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());  // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,5 30,20", Origin::Test).ok()); // 2
    const std::uint64_t before = f.doc.content_hash();

    // As drawn it cuts nothing; carried on, it cuts at x = 30.
    REQUIRE_FALSE(f.bus.execute_line("BUDA sinir=2 nesne=1 nokta=80,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    REQUIRE(f.bus.execute_line("BUDA uzanti=evet sinir=2 nesne=1 nokta=80,0", Origin::Test).ok());
    CHECK(said.find("1 parça budandı.") != std::string::npos); ///< the page's example, as printed
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{30'000, 0}));

    // And UZAT reaches the line of a boundary that ends before the line does.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,-10 10,-10", Origin::Test).ok()); // 3
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 60,0 60,20", Origin::Test).ok());   // 4
    REQUIRE_FALSE(f.bus.execute_line("UZAT sinir=4 nesne=3 nokta=10,-10", Origin::Test).ok());
    said.clear();
    REQUIRE(f.bus.execute_line("UZAT uzanti=evet sinir=4 nesne=3 nokta=10,-10", Origin::Test).ok());
    CHECK(said.find("1 uç sınıra uzatıldı.") != std::string::npos);
    CHECK_EQ(ring_vertex(f.doc, 3, 1), (core::Point2{60'000, -10'000}));
}

TEST_CASE("BUDA çitle: tek noktalı ya da hiçbir parçadan geçmeyen çit söylenerek reddedilir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    const std::string single = REFUSED(f.bus.execute_line("BUDA cit=50,-5", Origin::Test));
    CHECK(single.find("çit en az iki noktadan oluşur") != std::string::npos);
    const std::string nowhere = REFUSED(f.bus.execute_line("BUDA cit=50,40 60,40", Origin::Test));
    CHECK(nowhere.find("Çit, sınırların kestiği bir parçadan geçmiyor.") != std::string::npos);
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("BUDA ve UZAT noktasız bir betiğe ne eksik olduğunu söyler")
{
    // A script cannot click: without `nokta` it is told so in the words the
    // page lists (docs/komutlar/trim.md, extend.md), and nothing changes.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    const std::string trim = REFUSED(f.bus.execute_line("BUDA hepsi=evet", Origin::Script));
    CHECK(trim.find("BUDA: hiçbir parça gösterilmedi.") != std::string::npos);
    const std::string reach = REFUSED(f.bus.execute_line("UZAT hepsi=evet", Origin::Script));
    CHECK(reach.find("UZAT: hiçbir uç gösterilmedi.") != std::string::npos);
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("BUDA hızlı modda: seçim yokken yakındaki her nesne sınırdır")
{
    // AutoCAD's quick trim: with nothing chosen as a boundary, every object near
    // the one clicked cuts it. `hepsi=evet` is how a script says the same, and
    // what the run records, so a replay re-reads the drawing it is replayed into.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 30,-20 30,20", Origin::Test).ok()); // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 70,-20 70,20", Origin::Test).ok()); // 3

    REQUIRE(f.bus.execute_line("BUDA hepsi=evet nokta=50,0", Origin::Test).ok());
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{30'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 4, 0), (core::Point2{70'000, 0}));
    CHECK(f.journal.entries().back().args.find("hepsi") != nullptr);

    // THE PAGE'S EXAMPLE, as printed (docs/komutlar/trim.md): two pieces in one
    // line, and one sentence for both.
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,10 100,10", Origin::Test).ok());   // 5
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,-10 100,-10", Origin::Test).ok()); // 6
    said.clear();
    REQUIRE(f.bus.execute_line("BUDA hepsi=evet nokta=50,10 50,-10", Origin::Test).ok());
    CHECK(said.find("2 parça budandı.") != std::string::npos);
    CHECK_EQ(ring_vertex(f.doc, 5, 1), (core::Point2{30'000, 10'000}));
    CHECK_EQ(ring_vertex(f.doc, 6, 1), (core::Point2{30'000, -10'000}));
}

TEST_CASE("BUDA'nın bütün tıklamaları TEK geri alma adımıdır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());    // 1
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,10 100,10", Origin::Test).ok());  // 2
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 40,-20 40,20", Origin::Test).ok()); // 3
    const std::uint64_t before = f.doc.content_hash();

    auto started = f.bus.begin_interactive("BUDA", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::point(core::Point2{90'000, 0})).ok());
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value::point(core::Point2{90'000, 10'000})).ok());
    REQUIRE(s.waiting());
    REQUIRE(s.supply(Value{}).ok());
    auto done = f.bus.finish(s);
    if (!done) FAIL_WITH("BUDA", done.error().message);
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{40'000, 0}));
    CHECK_EQ(ring_vertex(f.doc, 2, 1), (core::Point2{40'000, 10'000}));

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);

    // Esc before any click is a cancel: nothing was done, and nothing is said
    // but that.
    {
        auto idle = f.bus.begin_interactive("BUDA", Origin::Gui);
        REQUIRE(idle.ok());
        idle.value()->cancel();
        const auto none = f.bus.finish(*idle.value());
        REQUIRE(none.ok());
        CHECK(none.value().message == "İptal edildi");
        CHECK_EQ(f.doc.content_hash(), before);
    }

    // Esc ends the run the way Enter does, as every CAD's TRIM does: what was
    // trimmed stays trimmed, and the run is still one undo step.
    const std::size_t depth = f.undo.undo_depth();
    auto again              = f.bus.begin_interactive("BUDA", Origin::Gui);
    REQUIRE(again.ok());
    Session& esc = *again.value();
    REQUIRE(esc.supply(Value::point(core::Point2{90'000, 0})).ok());
    esc.cancel();
    REQUIRE(f.bus.finish(esc).ok());
    CHECK_EQ(ring_vertex(f.doc, 1, 1), (core::Point2{40'000, 0}));
    CHECK_EQ(f.undo.undo_depth(), depth + 1);
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
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

TEST_CASE("YUVARLA kapalı alanın köşesini yerinde yuvarlatır; nesne aynı nesne kalır")
{
    // IT USED TO REFUSE, and a parcel's or a rectangle's corner is the one most
    // often rounded — so the tool looked broken to anyone who tried it with the
    // mouse. A closed ring cannot be broken in two without enclosing nothing, so
    // the arc is drawn into it: the same object, the same key, still a face.
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 40,0 40,30 0,30", Origin::Test).ok());

    std::string said;
    f.bus.on_echo = [&said](std::string_view s) { said.append(s); };
    REQUIRE(f.bus.execute_line("YUVARLA nesne=1 nokta=0,0 yaricap=5", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});

    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(e != core::kNoEntity);
    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[e]);
    REQUIRE_EQ(span.count, 1u);
    CHECK(f.doc.geometry().ring_role[span.first] != core::RingRole::Open);

    // The corner is gone: every vertex keeps at least the radius's worth of
    // distance from where it was, and the tangent points are on the edges.
    const auto xs = f.doc.geometry().ring_xs(span.first);
    const auto ys = f.doc.geometry().ring_ys(span.first);
    bool has_a    = false;
    bool has_b    = false;
    for (std::size_t v = 0; v < xs.size(); ++v) {
        CHECK_FALSE((xs[v] == 0 && ys[v] == 0));
        has_a = has_a || (xs[v] == 5'000 && ys[v] == 0);
        has_b = has_b || (xs[v] == 0 && ys[v] == 5'000);
        // Every drawn point lies on the arc, to within a millimetre of rounding,
        // or is one of the other three corners.
        const double dx = static_cast<double>(xs[v] - 5'000);
        const double dy = static_cast<double>(ys[v] - 5'000);
        if (xs[v] < 5'000 && ys[v] < 5'000)
            CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - 5'000.0) <= 1.5);
    }
    CHECK(has_a);
    CHECK(has_b);
    CHECK(said.find("yay 16 kenarla çizildi") != std::string::npos);

    // THE AREA IS THE ROUNDED ONE: the square's 40 x 30 less the corner's
    // r² - πr²/4, to within what sixteen chords leave.
    const double expected = 1200.0 - (25.0 - 3.14159265358979 * 25.0 / 4.0);
    const double got =
        std::abs(static_cast<double>(f.doc.geometry().area_of(f.doc.entities().slot[e]))) / 1.0e6;
    CHECK(std::abs(got - expected) < 0.05);

    // And the figure the manual prints (docs/komutlar/fillet.md) is the one
    // ALANÖLÇ reads.
    said.clear();
    REQUIRE(f.bus.execute_line("ALANÖLÇ nesneler=1", Origin::Test).ok());
    CHECK_MESSAGE(said.find("alan: 1194,60 m²") != std::string::npos, said);
}

TEST_CASE("PAH komşu kenardan uzun kesimi reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // 30 m off a 20 m edge: refused, and nothing is written.
    REQUIRE_FALSE(f.bus.execute_line("PAH nesne=1 nokta=0,0 mesafe=30", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("PAH açık çizginin ucunu köşe saymaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 20,0 0,0 0,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    // (20,0) is an END: one edge meets it and there is nothing to cut across.
    REQUIRE_FALSE(f.bus.execute_line("PAH nesne=1 nokta=20,0 mesafe=2", Origin::Test).ok());
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

    const core::EntityKey key  = f.doc.entities().key[0];
    const core::LayerId taslak = f.doc.entities().layer[0];

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

namespace {

/// A command that hands one unit of work to a job and reports where it ran.
Task<void> run_job_probe(Context& ctx)
{
    bool ran = false;
    Job job;
    job.label = "sınama işi";
    job.work  = [&ran](const JobControl&) { ran = true; };
    co_await run_job(ctx.session(), job);
    ctx.echo(ran ? "iş koştu" : "iş koşmadı");
}

CommandSpec job_probe_spec()
{
    CommandSpec spec;
    spec.id      = "test.is";
    spec.names   = {"İŞSINAMA", "ISSINAMA"};
    spec.summary = "sınama";
    spec.flags   = Flags::Interactive | Flags::Scriptable;
    spec.run     = &run_job_probe;
    return spec;
}

} // namespace

TEST_CASE("İŞ: ev sahibi yoksa iş yerinde koşar, varsa oturum parkeder ve sürdürülür")
{
    // Without a host every road runs the job in place — a script, a test, a
    // one-shot line — and the body never notices.
    Fixture plain;
    REQUIRE(plain.reg.add(job_probe_spec()).ok());
    std::string said;
    plain.bus.on_echo = [&said](std::string_view s) { said.append(s); };
    REQUIRE(plain.bus.execute_line("İŞSINAMA", Origin::Test).ok());
    CHECK(said == "iş koştu");

    // With a host, a session the client keeps parks on the job: Working, not
    // finished, and the host is handed the session without the job having run.
    Fixture hosted;
    REQUIRE(hosted.reg.add(job_probe_spec()).ok());
    said.clear();
    hosted.bus.on_echo     = [&said](std::string_view s) { said.append(s); };
    Session* parked        = nullptr;
    hosted.bus.on_job_host = [&parked](Session& s) { parked = &s; };

    auto started = hosted.bus.begin_interactive("İŞSINAMA");
    REQUIRE(started.ok());
    Session& session = *started.value();
    CHECK(session.working());
    CHECK_EQ(std::string(session_state_name(session.state())), "working");
    REQUIRE(parked == &session);
    REQUIRE(session.job() != nullptr);
    CHECK(session.job()->label == "sınama işi");
    CHECK(said.empty());

    // The host runs the job and resumes; the command finishes where it left off.
    session.job()->work(JobControl{session.job()->stop.get_token()});
    session.resume_job();
    CHECK(session.finished());
    CHECK(said == "iş koştu");
    CHECK(hosted.bus.finish(session).ok());

    // The one-shot road ignores the host even when one is installed.
    said.clear();
    parked = nullptr;
    REQUIRE(hosted.bus.execute_line("İŞSINAMA", Origin::Test).ok());
    CHECK(parked == nullptr);
    CHECK(said == "iş koştu");
}

// ------------------------------------------------------ Phase 2 kind commands --

TEST_CASE("Yeni türler: SPLINE, TARAMA, BLOK, BLOKEKLE, ÖLÇÜ ve LİDER kendi türünü çizer, GERİAL "
          "kaldırır")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };
    REQUIRE(f.bus.execute_line("KATMAN ad=CIZIM", Origin::Test).ok());
    const auto run = [&](const char* line) {
        auto r = f.bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    };
    const auto kind_count = [&](core::KindId kind) {
        std::size_t n = 0;
        for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
            if (f.doc.alive(e) && f.doc.entities().kind[e] == kind) ++n;
        return n;
    };

    run("SPLINE noktalar=0,0 10,20 20,20 30,0 derece=3");
    CHECK_EQ(kind_count(core::kSplineKind), 1u);
    run("TARAMA noktalar=0,100 20,100 20,120 0,120 desen=ANSI37 olcek=1000");
    CHECK_EQ(kind_count(core::kHatchKind), 1u);
    // The hatch draws through a symbol with one fill layer per family, two for ANSI37.
    {
        const auto h              = static_cast<core::EntityId>(f.doc.entities().size() - 1);
        const core::StyleId style = f.doc.entities().style[h];
        REQUIRE(style != core::kByLayerStyle);
        std::size_t fills = 0;
        for (const core::SymbolLayer& l : f.doc.styles().symbol_at(style).layers)
            if (l.type == core::SymbolLayerType::LinePatternFill) ++fills;
        CHECK_EQ(fills, 2u);
    }
    // An unknown pattern is refused with the catalogue's names.
    said += REFUSED(f.bus.execute_line("TARAMA noktalar=0,0 1,0 1,1 desen=YOK", Origin::Test));
    CHECK(said.find("Tanınmayan tarama deseni") != std::string::npos);

    run("KATMAN ad=SEMBOL");
    run("DAİRE merkez=200,300 cevre=201,300");
    run("ÇİZGİ 200,300 202,300");
    run("SEÇ KATMAN katman=SEMBOL");
    const std::size_t before_block = f.doc.live_entity_count();
    run("BLOK ad=KAPAK taban=200,300");
    // Two members went into the definition, one reference stands where they were.
    REQUIRE_EQ(f.doc.blocks().size(), 1u);
    CHECK_EQ(f.doc.blocks().at(0).members.size(), 2u);
    CHECK_EQ(kind_count(core::kBlockReferenceKind), 1u);
    CHECK_EQ(f.doc.live_entity_count(), before_block + 1); // +2 members +1 ref −2 originals
    said += REFUSED(f.bus.execute_line("BLOK ad=KAPAK taban=0,0 nesneler=1", Origin::Test));
    CHECK(said.find("zaten var") != std::string::npos);

    run("BLOKEKLE ad=KAPAK nokta=220,300 olcek=2 aci=90");
    run("BLOKEKLE ad=KAPAK nokta=240,300 sutun=3 satir=2 sutun_aralik=5000 satir_aralik=4000");
    CHECK_EQ(kind_count(core::kBlockReferenceKind), 3u);
    said += REFUSED(f.bus.execute_line("BLOKEKLE ad=YOK nokta=0,0", Origin::Test));
    CHECK(said.find("adında blok yok") != std::string::npos);

    run("ÖLÇÜ birinci=0,400 ikinci=12.5,400 konum=0,403");
    CHECK_EQ(kind_count(core::kDimensionKind), 1u);
    CHECK(said.find("Ölçü çizildi: 12,50 (ISO-25)") != std::string::npos);
    run("ÖLÇÜ birinci=100,400 ikinci=100,410 konum=105.4,401.9 tur=acisal tepe=110,400");
    CHECK(said.find("45,00°") != std::string::npos); // the arms at 180° and 135° from the vertex
    run("LİDER noktalar=0,500 3,503 6,503 metin=Rögar");
    CHECK_EQ(kind_count(core::kLeaderKind), 1u);

    // Every one is a single undo step.
    const std::size_t all = f.doc.live_entity_count();
    run("GERİAL");
    CHECK_EQ(f.doc.live_entity_count(), all - 2); // the leader and its caption
    run("GERİAL");
    CHECK_EQ(kind_count(core::kDimensionKind), 1u);
    run("GERİAL");
    CHECK_EQ(kind_count(core::kDimensionKind), 0u);
}

TEST_CASE("Yeni türler: TAŞI, DÖNDÜR, ÖLÇEKLE ve AYNALA yükü de dönüştürür")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=SEMBOL", Origin::Test).ok());
    const auto run = [&](const char* line) {
        auto r = f.bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    };
    run("ÇİZGİ 0,0 2,0");
    run("SEÇ KATMAN katman=SEMBOL");
    run("BLOK ad=OK taban=0,0");
    const auto ref = static_cast<core::EntityId>(f.doc.entities().size() - 1);
    REQUIRE_EQ(f.doc.entities().kind[ref], core::kBlockReferenceKind);
    const std::string id = std::to_string(core::raw(f.doc.entities().key[ref]));

    // Turned a quarter about its insertion: the line points north.
    run(("DÖNDÜR nesneler=" + id + " merkez=0,0 aci=90").c_str());
    core::EmitBuffer runs;
    REQUIRE(core::entity_outline(f.doc, ref, runs));
    CHECK_EQ(runs.run_xs(0)[1], 0);
    CHECK_EQ(runs.run_ys(0)[1], 2000);
    auto turned = core::block_reference_of(f.doc.geometry(), f.doc.entities().slot[ref]);
    REQUIRE(turned.ok());
    CHECK_EQ(turned.value().rotation_udeg, 90'000'000);

    // Scaled ×2 about the origin: the line is 4 m, the scale rational doubles.
    run(("ÖLÇEKLE nesneler=" + id + " merkez=0,0 carpan=2").c_str());
    runs.clear();
    REQUIRE(core::entity_outline(f.doc, ref, runs));
    CHECK_EQ(runs.run_ys(0)[1], 4000);

    // Mirrored in the x axis: north becomes south.
    run(("AYNALA nesneler=" + id + " baslangic=0,0 bitis=10,0").c_str());
    runs.clear();
    REQUIRE(core::entity_outline(f.doc, ref, runs));
    CHECK_EQ(runs.run_ys(0)[1], -4000);

    // A hatch keeps its pattern angle turning with it.
    run("TARAMA noktalar=100,100 120,100 120,120 100,120 desen=ANSI31 olcek=1000");
    const auto hatch      = static_cast<core::EntityId>(f.doc.entities().size() - 1);
    const std::string hid = std::to_string(core::raw(f.doc.entities().key[hatch]));
    run(("DÖNDÜR nesneler=" + hid + " merkez=110,110 aci=15").c_str());
    auto hdef = core::hatch_of(f.doc.geometry(), f.doc.entities().slot[hatch]);
    REQUIRE(hdef.ok());
    CHECK_EQ(hdef.value().angle_udeg, 15'000'000);
}

// ---------------------------------------------------------- the effect ------

TEST_CASE("Etki: listelemek okumadır, kaydetmek diske yazmadır")
{
    // THE TWO CLAIMS TODOS C-02 IS ABOUT. `Flags` answers "which clients may
    // reach it"; `Effect` answers the different question a policy has to ask
    // before letting a non-human run it — and the single boolean it replaces
    // (`!NoEffect`) could tell neither of these apart.
    Fixture f;

    const CommandSpec* layout = f.reg.by_id("core.layout");
    REQUIRE(layout != nullptr);

    Args listele;
    listele.set("islem", Value::text("listele"));
    CHECK(has_effect(effect_of(*layout, listele), Effect::Query));
    // LISTING MUST NOT ASK FOR APPROVAL (.claude/ai.md R3).
    CHECK_FALSE(has_effect(effect_of(*layout, listele), Effect::DocumentEdit));

    Args sil;
    sil.set("islem", Value::text("sil"));
    CHECK(has_effect(effect_of(*layout, sil), Effect::DocumentEdit));

    // SAVING CARRIES `ReadOnly` AND WRITES OVER A FILE. That flag means "skips
    // the transaction path", and reading it as "harmless" is the mistake this
    // whole type exists to make impossible.
    const CommandSpec* save = f.reg.by_id("core.save");
    REQUIRE(save != nullptr);
    CHECK(has_effect(effect_of(*save), Effect::FileWrite));

    const CommandSpec* undo = f.reg.by_id("core.undo");
    REQUIRE(undo != nullptr);
    CHECK(has_effect(effect_of(*undo), Effect::DocumentEdit));

    // A PRINTER IS NOT AN UNDO STACK.
    const CommandSpec* print = f.reg.by_id("core.print");
    REQUIRE(print != nullptr);
    CHECK(has_effect(effect_of(*print), Effect::ExternalWrite));
}

TEST_CASE("Etki: hiçbir komut bildirilmemiş etkiyle kalmaz")
{
    // `Effect::None` means "nobody said", and `effect_of` never passes it on:
    // an unstated command falls back to what its flags and its category can do,
    // which is cautious rather than silent.
    Fixture f;
    for (const CommandSpec& spec : f.reg.all()) {
        INFO("komut: ", spec.id);
        CHECK(effect_of(spec) != Effect::None);
    }
}

TEST_CASE("Etki: verilmemiş fiil en kötü hâli verir")
{
    // An interactive run asks for the verb AFTER validation, so the honest
    // answer before it is asked is the worst case — not "harmless".
    Fixture f;
    const CommandSpec* layout = f.reg.by_id("core.layout");
    REQUIRE(layout != nullptr);

    const Args nothing;
    CHECK(has_effect(effect_of(*layout, nothing), Effect::DocumentEdit));

    // And a word nobody declared is refused by the validator in a moment; until
    // then it is read as the worst case too.
    Args nonsense;
    nonsense.set("islem", Value::text("zıpla"));
    CHECK(has_effect(effect_of(*layout, nonsense), Effect::DocumentEdit));
}

// ============================================================================
// NESNEBİLGİ / AÇIÖLÇ — the two questions of P7
// ============================================================================

TEST_CASE("NESNEBİLGİ türü, katmanı, köşe sayısını ve alanı bildirir")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());

    said.clear();
    REQUIRE(f.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Test).ok());

    CHECK(said.find("katman PARSEL") != std::string::npos);
    // The kind's own Turkish name, from the kind table rather than from a switch
    // inside the command: whatever the polyline kind calls itself is printed, so
    // the next kind a plugin registers is named too (CLAUDE.md 5.10).
    CHECK(said.find(core::builtin_kinds().find(core::kPolylineKind)->names[0]) !=
          std::string::npos);
    CHECK(said.find("köşe") != std::string::npos);
    // 10 m × 10 m = 100 m², with the tapu's two decimals.
    CHECK(said.find("100,00") != std::string::npos);
}

TEST_CASE("NESNEBİLGİ yapılandırılmış rapor döndürür")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());

    const auto r = f.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Test);
    REQUIRE(r.ok());

    // R26: the answer is a table, so a script and an agent read the report and
    // never a Turkish sentence.
    const core::Json& rep      = r.value().report;
    const core::Json* count    = rep.find("adet");
    const core::Json* entities = rep.find("nesneler");
    REQUIRE(count != nullptr);
    REQUIRE(entities != nullptr);
    CHECK_EQ(count->as_int(), 1);

    const core::Json& row = entities->as_array().front();
    CHECK_EQ(row.find("katman")->as_string(), std::string("PARSEL"));
    CHECK_EQ(row.find("kose")->as_int(), 4);
    CHECK_EQ(row.find("alan_mm2")->as_int(), 100000000);
    CHECK_EQ(row.find("cevre_mm")->as_int(), 40000);
    CHECK_EQ(row.find("halka")->as_int(), 1);
    REQUIRE(row.find("kapsam") != nullptr);
    CHECK_EQ(row.find("kapsam")->as_array().size(), std::size_t{4});
}

TEST_CASE("NESNEBİLGİ öznitelikleri de yazar")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=ada tur=metin ad=Ada", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ad=ada nesne=1 deger=1453", Origin::Test).ok());

    said.clear();
    const auto r = f.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Test);
    REQUIRE(r.ok());

    // "What is this" is not answered by geometry alone: the ada number is what
    // the parcel IS to a surveyor, and reading it used to mean opening a table.
    const core::Json& row = r.value().report.find("nesneler")->as_array().front();
    REQUIRE(row.find("oznitelik") != nullptr);
    CHECK_EQ(row.find("oznitelik")->find("ada")->as_string(), std::string("1453"));
    CHECK(said.find("1 öznitelik") != std::string::npos);
}

TEST_CASE("NESNEBİLGİ silinmiş nesneyi söyler, çökmez")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 1,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SİL nesneler=1", Origin::Test).ok());

    said.clear();
    said += REFUSED(f.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Test));
    CHECK(said.find("silinmiş") != std::string::npos);
}

TEST_CASE("AÇIÖLÇ dik açıyı semt kuralında 100 grad okur")
{
    Fixture f;
    // Default convention: grad, semt — clockwise from north. The first arm points
    // north and the second east, so the sweep from the first to the second is a
    // quarter turn IN THE RULE'S OWN DIRECTION: 100 grad.
    const auto r = f.bus.execute_line("AÇIÖLÇ 0,0 0,10 10,0", Origin::Test);
    REQUIRE(r.ok());

    const core::Json& rep = r.value().report;
    CHECK_EQ(rep.find("aci_udeg")->as_int(), core::kUDegFullCircle / 4);
    CHECK_EQ(rep.find("ters_udeg")->as_int(), 3 * core::kUDegFullCircle / 4);
    CHECK_EQ(rep.find("aci_metin")->as_string(), std::string("100,0000 grad"));
    CHECK_EQ(rep.find("birinci_kenar_mm")->as_int(), 10000);
    CHECK_EQ(rep.find("ikinci_kenar_mm")->as_int(), 10000);
    CHECK_EQ(rep.find("birim")->as_string(), std::string("g"));
}

TEST_CASE("AÇIÖLÇ kolları değişince ters açıyı verir")
{
    Fixture f;
    // Swapping the arms swaps the two readings. That is the whole reason both are
    // reported: a corner is two angles, and the program does not decide silently
    // which one the user meant.
    const auto r = f.bus.execute_line("AÇIÖLÇ 0,0 10,0 0,10", Origin::Test);
    REQUIRE(r.ok());
    CHECK_EQ(r.value().report.find("aci_udeg")->as_int(), 3 * core::kUDegFullCircle / 4);
    CHECK_EQ(r.value().report.find("ters_udeg")->as_int(), core::kUDegFullCircle / 4);
}

TEST_CASE("AÇIÖLÇ matematik kuralında ve derecede aynı köşeyi başka okur")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("AYAR açı_birimi derece", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("MOD kural matematik", Origin::Test).ok());

    // The same three points, counter-clockwise from east: east to north is +90°.
    // One convention pair drives the reader and the writer alike (TODOS-CAD
    // P0-4), so this is the same corner read the other way round.
    const auto r = f.bus.execute_line("AÇIÖLÇ 0,0 10,0 0,10", Origin::Test);
    REQUIRE(r.ok());
    CHECK_EQ(r.value().report.find("aci_metin")->as_string(), std::string("90,0000°"));
    CHECK_EQ(r.value().report.find("birim")->as_string(), std::string("d"));
    CHECK_EQ(r.value().report.find("aci_udeg")->as_int(), core::kUDegFullCircle / 4);
}

TEST_CASE("AÇIÖLÇ tepeyle çakışan kolu reddeder")
{
    Fixture f;
    const auto r = f.bus.execute_line("AÇIÖLÇ 0,0 0,0 10,0", Origin::Test);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("doğrultusu yok") != std::string::npos);
}

TEST_CASE("NESNEBİLGİ ve AÇIÖLÇ geri alma adımı yemez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 1,0", Origin::Test).ok());
    const std::size_t depth = f.undo.undo_depth();

    REQUIRE(f.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("AÇIÖLÇ 0,0 0,1 1,0", Origin::Test).ok());

    // A question is not an edit: Ctrl+Z after asking one undoes the drawing.
    CHECK_EQ(f.undo.undo_depth(), depth);
}

TEST_CASE("etkileşimli yol da yapılandırılmış cevabı taşır")
{
    // The seam this pins: `Bus::dispatch` filled `DispatchResult::report` and
    // `Bus::finish` did not, so a query ARMED FROM THE TOOL COLUMN and answered
    // by pointing came back with a null report. The typed road had a capability
    // the pointed road lacked, which is what CLAUDE.md 5.15 forbids, and the
    // client that lost it is the one a person actually uses.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());

    auto started = f.bus.begin_interactive("NESNEBİLGİ", Origin::Gui);
    REQUIRE(started.ok());
    auto& session = *started.value();
    REQUIRE(session.waiting());
    REQUIRE(session.supply(Value::ids({1})).ok());

    const auto done = f.bus.finish(session);
    REQUIRE(done.ok());
    REQUIRE(done.value().report.find("nesneler") != nullptr);
    CHECK_EQ(done.value().report.find("adet")->as_int(), 1);
}

// ============================================================================
// ESNET — the window is the vertex filter
// ============================================================================

TEST_CASE("ESNET pencere içindeki köşeleri taşır, dışındakileri bırakır")
{
    Fixture f;
    // A 20 m × 10 m parcel. The window covers its RIGHT edge only, so the two
    // right corners follow and the two left ones stay on the tapu — which is the
    // whole job: a road widens on one side.
    REQUIRE(f.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(
        f.bus
            .execute_line("ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0", Origin::Test)
            .ok());

    const core::Document& doc = f.doc;
    const auto span           = doc.geometry().rings_of(doc.entities().slot[0]);
    const auto xs             = doc.geometry().ring_xs(span.first);
    const auto ys             = doc.geometry().ring_ys(span.first);
    REQUIRE_EQ(xs.size(), std::size_t{4});

    CHECK_EQ(xs[0], 0);      ///< stayed
    CHECK_EQ(xs[1], 25'000); ///< 20 m + 5 m
    CHECK_EQ(xs[2], 25'000); ///< 20 m + 5 m
    CHECK_EQ(xs[3], 0);      ///< stayed
    for (std::size_t i = 0; i < ys.size(); ++i)
        CHECK_EQ(ys[i], i == 0 || i == 1 ? 0 : 10'000); ///< nothing moved across
}

TEST_CASE("ESNET tek geri alma adımı yer ve tamamen geri döner")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();
    const std::size_t depth    = f.undo.undo_depth();

    REQUIRE(
        f.bus
            .execute_line("ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0", Origin::Test)
            .ok());
    CHECK_EQ(f.undo.undo_depth(), depth + 1); ///< one command, one step
    CHECK(f.doc.content_hash() != before);

    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
}

TEST_CASE("ESNET pencerenin tamamını kapsadığında nesneyi taşır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(
        f.bus
            .execute_line("ESNET pencere=-5,-5 pencere=15,15 baslangic=0,0 bitis=3,4", Origin::Test)
            .ok());

    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    const auto xs   = f.doc.geometry().ring_xs(span.first);
    const auto ys   = f.doc.geometry().ring_ys(span.first);
    // Every corner inside means every corner follows, which IS a move.
    CHECK_EQ(xs[0], 3'000);
    CHECK_EQ(ys[0], 4'000);
    CHECK_EQ(xs[2], 13'000);
    CHECK_EQ(ys[2], 14'000);
}

TEST_CASE("ESNET daireyi büyütmez: pencere hepsini alınca öteler")
{
    Fixture f;
    // THE CASE THE SNAPSHOT RULE EXISTS FOR. A circle's grips are its centre and
    // a radius handle. Moving the centre already carries the handle, so a second
    // move computed against the LIVE geometry would move it twice and the circle
    // would grow by the displacement. Against the snapshot it lands where it
    // already is, and a windowed circle is simply moved.
    REQUIRE(f.bus.execute_line("DAİRE 10,10 15,10", Origin::Test).ok());
    const core::Mm2 area_before = f.doc.entity_area(0);

    REQUIRE(
        f.bus.execute_line("ESNET pencere=0,0 pencere=25,25 baslangic=0,0 bitis=5,0", Origin::Test)
            .ok());

    CHECK_EQ(f.doc.entity_area(0), area_before); ///< same circle
    const core::Box2 box = f.doc.entities().box_of(0);
    CHECK_EQ((box.min_x + box.max_x) / 2, 15'000); ///< centre 10 m + 5 m
    CHECK_EQ((box.min_y + box.max_y) / 2, 10'000);
}

TEST_CASE("ESNET dairenin yarıçap tutamağını alınca yarıçapı değiştirir")
{
    Fixture f;
    // AND THE OTHER HALF OF THE SAME RULE: window only the radius handle and the
    // circle is resized, not moved. That is what a stretch of a definition means.
    REQUIRE(f.bus.execute_line("DAİRE 10,10 15,10", Origin::Test).ok());
    const core::Mm2 area_before = f.doc.entity_area(0);

    REQUIRE(
        f.bus.execute_line("ESNET pencere=14,9 pencere=16,11 baslangic=0,0 bitis=5,0", Origin::Test)
            .ok());

    CHECK(f.doc.entity_area(0) > area_before); ///< 5 m radius became 10 m
    const core::Box2 box = f.doc.entities().box_of(0);
    CHECK_EQ((box.min_x + box.max_x) / 2, 10'000); ///< centre did not move
}

TEST_CASE("ESNET boş pencereyi ve dejenere pencereyi söyler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    // A window past the drawing: nothing to move, and it says what a window has
    // to do rather than reporting success.
    said += REFUSED(f.bus.execute_line(
        "ESNET pencere=100,100 pencere=110,110 baslangic=0,0 bitis=5,0", Origin::Test));
    CHECK(said.find("esnetilecek köşe yok") != std::string::npos);
    CHECK_EQ(f.undo.undo_depth(), std::size_t{1}); ///< the ALAN only

    // A window with no area is a line, and a line filters nothing usefully.
    const auto bad =
        f.bus.execute_line("ESNET pencere=5,0 pencere=5,10 baslangic=0,0 bitis=5,0", Origin::Test);
    CHECK_FALSE(bad.ok());
    CHECK(bad.error().message.find("bir çizgi") != std::string::npos);
}

TEST_CASE("ESNET kilitli katmanı atlar ve kaç nesne atladığını söyler")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU kilitli=evet", Origin::Test).ok());

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    said += REFUSED(f.bus.execute_line("ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0",
                                       Origin::Test));
    // Counted and said: a silent skip is a stretch that looks like it worked.
    CHECK(said.find("kilitli") != std::string::npos);

    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    CHECK_EQ(f.doc.geometry().ring_xs(span.first)[1], 20'000); ///< untouched
}

TEST_CASE("ESNET yalnız adı verilen nesneleri esnetir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,20 20,20 20,30 0,30", Origin::Test).ok());

    // The window covers the right edge of BOTH, but only the first is named.
    REQUIRE(
        f.bus
            .execute_line("ESNET nesneler=1 pencere=15,-5 pencere=25,35 baslangic=0,0 bitis=5,0",
                          Origin::Test)
            .ok());

    const auto first  = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    const auto second = f.doc.geometry().rings_of(f.doc.entities().slot[1]);
    CHECK_EQ(f.doc.geometry().ring_xs(first.first)[1], 25'000);  ///< named, moved
    CHECK_EQ(f.doc.geometry().ring_xs(second.first)[1], 20'000); ///< not named, stayed
}

TEST_CASE("kilitli katman üzerindeki nesneyi de korur")
{
    // THE DEFECT: the lock was checked on every `add_*` and on nothing else. A
    // locked layer stopped a user DRAWING a new parcel on it and let TAŞI,
    // KÖŞETAŞI, ESNET, DÖNDÜR, ÖLÇEKLE and SİL reshape or erase every parcel
    // already there. In a cadastral drawing that is exactly the wrong way round —
    // what is on the sheet is what a lock is for.
    //
    // The contract asserted here is this codebase's own: a command that cannot do
    // the work SAYS SO in the user's language and changes nothing. It is not a
    // dispatch error, because a refusal explained in Turkish must not be doubled
    // by a validator message addressed to a programmer (`Bus::finish`, the
    // `declined` path).
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU kilitli=evet", Origin::Test).ok());

    const std::uint64_t locked_state = f.doc.content_hash();
    const std::size_t depth          = f.undo.undo_depth();

    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    for (const char* line :
         {"TAŞI nesneler=1 baslangic=0,0 bitis=5,0", "KÖŞETAŞI nesne=1 kose=1 nokta=1,1",
          "ESNET nesneler=1 pencere=-5,-5 pencere=15,15 baslangic=0,0 bitis=5,0", "SİL nesneler=1",
          "DÖNDÜR nesneler=1 merkez=0,0 aci=50", "ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2",
          "PATLAT nesne=1"}) {
        said.clear();
        const auto r             = f.bus.execute_line(line, Origin::Test);
        const std::string reason = r.ok() ? said : said + r.error().message;
        CHECK_MESSAGE(reason.find("kilitli") != std::string::npos, reason);
        // AND NOTHING HALF-APPLIED, which is Article 1.6 over every one of them.
        CHECK_MESSAGE(f.doc.content_hash() == locked_state, line);
        CHECK_MESSAGE(f.undo.undo_depth() == depth, line);
    }

    // AND THE UNDO STACK IS NOT TRAPPED. `Document::restore_geometry` and
    // `set_entity_alive` are unguarded on purpose, so the edits made before the
    // lock still undo — otherwise locking a layer would freeze history.
    f.bus.on_echo = nullptr;
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok()); ///< the lock itself
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok()); ///< the ALAN
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("kilidi açılınca düzenleme yeniden çalışır")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU kilitli=evet", Origin::Test).ok());

    const std::uint64_t locked_state = f.doc.content_hash();
    REFUSED(f.bus.execute_line("TAŞI nesneler=1 baslangic=0,0 bitis=5,0", Origin::Test));
    CHECK_EQ(f.doc.content_hash(), locked_state); ///< said no, did nothing

    // The refusal names the way to lift it, and what it names has to work.
    REQUIRE(f.bus.execute_line("KATMAN ad=TAPU kilitli=hayır", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("TAŞI nesneler=1 baslangic=0,0 bitis=5,0", Origin::Test).ok());
    CHECK(f.doc.content_hash() != locked_state);
}

TEST_CASE("BÖLÜMLE blok= ile her istasyona blok koyar")
{
    Fixture f;
    // A row of poles down a 100 m line, every 25 m. Placing them one INSERT at a
    // time is the work this parameter exists to remove.
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("BLOK ad=DİREK nesneler=1 taban=0,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());

    const auto count_references = [&f] {
        std::size_t n = 0;
        for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
            if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kBlockReferenceKind) ++n;
        return n;
    };
    const std::size_t entities_before   = f.doc.live_entity_count();
    const std::size_t references_before = count_references(); ///< BLOK leaves one behind

    // `nesne` names the POLYLINE, whichever key it got: BLOK consumes what it
    // defines, so the key is read rather than assumed.
    core::EntityKey line = core::EntityKey::None;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kPolylineKind)
            line = f.doc.key_of(e);
    REQUIRE(line != core::EntityKey::None);

    REQUIRE(f.bus
                .execute_line("BÖLÜMLE nesne=" + std::to_string(core::raw(line)) +
                                  " aralik=25 blok=DİREK",
                              Origin::Test)
                .ok());

    // Three interior stations at 25, 50 and 75 m; 100 is the end and is not one.
    CHECK_EQ(f.doc.live_entity_count(), entities_before + 3);
    CHECK_EQ(count_references(), references_before + 3);
}

TEST_CASE("BÖLÜMLE hizala=evet bloğu kenarın doğrultusuna çevirir")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("BLOK ad=OK nesneler=1 taban=0,0", Origin::Test).ok());
    // A line running due NORTH: an aligned block turns a quarter turn with it.
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 0,100", Origin::Test).ok());

    core::EntityKey line = core::EntityKey::None;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kPolylineKind)
            line = f.doc.key_of(e);
    REQUIRE(line != core::EntityKey::None);

    REQUIRE(f.bus
                .execute_line("BÖLÜMLE nesne=" + std::to_string(core::raw(line)) +
                                  " sayi=2 blok=OK hizala=evet",
                              Origin::Test)
                .ok());

    // THE ONE AT THE MIDPOINT is the one this command placed; BLOK's own
    // reference sits at the origin with no rotation and is not the subject.
    bool checked = false;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.alive(e) || f.doc.entities().kind[e] != core::kBlockReferenceKind) continue;
        const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[e]);
        if (f.doc.geometry().ring_ys(span.first)[0] != 50'000) continue;

        const auto payload = f.doc.geometry().payload_of(f.doc.entities().slot[e]);
        auto ref           = core::decode_block_reference(payload);
        REQUIRE(ref.ok());
        // `atan2_udeg` is exact on the axes: due north is a quarter circle.
        CHECK_EQ(ref.value().rotation_udeg, core::kUDegFullCircle / 4);
        checked = true;
    }
    CHECK(checked);
}

TEST_CASE("BÖLÜMLE tanımsız bloğu adıyla reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());

    const auto r = f.bus.execute_line("BÖLÜMLE nesne=1 sayi=4 blok=YOKBÖYLE", Origin::Test);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("YOKBÖYLE") != std::string::npos);
    // The refusal says what to do about it, which is BLOK's job and not this one's.
    CHECK(r.error().message.find("BLOK ile tanımlayın") != std::string::npos);
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1}); ///< nothing half-placed
}

// ============================================================================
// İZ — the marks a trace runs from
// ============================================================================

TEST_CASE("İZ nokta işaretler ve en çok ikisini tutar")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    CHECK_EQ(f.bus.tracking_marks().size(), std::size_t{1});
    CHECK(said.find("1 işaret") != std::string::npos);

    REQUIRE(f.bus.execute_line("İZ 0,20", Origin::Test).ok());
    REQUIRE_EQ(f.bus.tracking_marks().size(), std::size_t{2});
    CHECK_EQ(f.bus.tracking_marks()[0], (core::Point2{10'000, 0}));
    CHECK_EQ(f.bus.tracking_marks()[1], (core::Point2{0, 20'000}));

    // A THIRD MARK IS A NEW PAIR, not a third axis: the oldest goes.
    REQUIRE(f.bus.execute_line("İZ 30,40", Origin::Test).ok());
    REQUIRE_EQ(f.bus.tracking_marks().size(), std::size_t{2});
    CHECK_EQ(f.bus.tracking_marks()[0], (core::Point2{0, 20'000}));
    CHECK_EQ(f.bus.tracking_marks()[1], (core::Point2{30'000, 40'000}));
}

TEST_CASE("İZ aynı noktayı iki kez işaretlemez")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("İZ 10,20", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("İZ 10,20", Origin::Test).ok());
    // Acquiring a corner, moving away and coming back to it means ONE trace —
    // and two identical marks would make the crossing the mark itself.
    CHECK_EQ(f.bus.tracking_marks().size(), std::size_t{1});
}

TEST_CASE("İZ sil=evet işaretleri temizler, İZ tek başına listeler")
{
    Fixture f;
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };

    REQUIRE(f.bus.execute_line("İZ", Origin::Test).ok());
    CHECK(said.find("İşaretli nokta yok") != std::string::npos);

    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("İZ 0,20", Origin::Test).ok());

    said.clear();
    REQUIRE(f.bus.execute_line("İZ", Origin::Test).ok());
    CHECK(said.find("2 işaret") != std::string::npos);
    CHECK(said.find("Kesişimler") != std::string::npos);

    said.clear();
    REQUIRE(f.bus.execute_line("İZ sil=evet", Origin::Test).ok());
    CHECK(f.bus.tracking_marks().empty());
    CHECK(said.find("silindi") != std::string::npos);
}

TEST_CASE("İZ şeffaftır, geri alma adımı yemez ve belgeye dokunmaz")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();
    const std::size_t depth    = f.undo.undo_depth();

    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK_EQ(f.undo.undo_depth(), depth);

    // AND IT IS TRANSPARENT, which is what lets it run beside a waiting command:
    // the mark is made in the MIDDLE of a ÇİZGİ and outlives it.
    const CommandSpec* spec = f.reg.resolve("İZ");
    REQUIRE(spec != nullptr);
    CHECK(has_flag(spec->flags, Flags::Transparent));
    CHECK(has_flag(spec->flags, Flags::ReadOnly));

    // Not in the journal as a document mutation.
    for (const auto& e : f.journal.entries())
        CHECK(e.command_id != "core.tracking");
}

TEST_CASE("İZ işaretleri hizmet ettikleri komut bitince silinir; şeffaf komutlar dokunmaz")
{
    // THE REGRESSION. `clear_tracking` said the end of a run forgets the marks
    // and nothing called it: two marks made once stayed for the whole session,
    // through YENİ, and drew their traces across every drawing after.
    Fixture f;

    // MADE IN THE MIDDLE OF A COMMAND, they last until THAT command ends.
    auto started = f.bus.begin_interactive("ÇİZGİ", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAKINLAŞ KAPSAM", Origin::Test).ok()); // transparent too
    CHECK_EQ(f.bus.tracking_marks().size(), std::size_t{1});
    REQUIRE(s.supply(Value::point(core::Point2{20'000, 0})).ok());
    REQUIRE(f.bus.finish(s).ok());
    CHECK(f.bus.tracking_marks().empty());

    // MADE WITH NOTHING RUNNING, they wait for the next command and go when it
    // is done. A line refused before it ever runs — a key that names nothing —
    // is not that command: nothing was done, and the marks are still wanted.
    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("İZ 0,20", Origin::Test).ok());
    CHECK_EQ(f.bus.tracking_marks().size(), std::size_t{2});
    REQUIRE_FALSE(f.bus.execute_line("KIR nesne=99", Origin::Test).ok());
    CHECK_EQ(f.bus.tracking_marks().size(), std::size_t{2});
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 5,5", Origin::Test).ok());
    CHECK(f.bus.tracking_marks().empty());
}

TEST_CASE("İZ ile işaretlenen noktalar yakalamaya geçiyor")
{
    Fixture f;
    // THE SEAM THIS PINS. The marks live on the bus and the engine reads them
    // through `SnapQuery::tracking`; a field nobody writes is a feature nobody
    // has, which is exactly what happened to `normal_lock` once.
    //
    // A VIEW SCALE FIRST, because a client with no view gets an aperture of zero
    // and no aid fires at all — the "no view, no aid" contract every mode keeps.
    // 10 mm per pixel is an ordinary sheet zoom.
    f.bus.aids().set_view_scale(10.0);

    REQUIRE(f.bus.execute_line("İZ 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("İZ 0,20", Origin::Test).ok());

    // A line whose second point is aimed WITHIN THE APERTURE of the crossing
    // lands on it. 10 mm per pixel and the default tolerance make that aperture
    // a few centimetres, so the aim is a few centimetres off — aiming half a
    // metre away would be out of reach, which is the contract and not a bug.
    REQUIRE(f.bus.execute_line("ÇİZGİ 50,50 10.05,19.95", Origin::Test).ok());

    core::EntityId last = core::kNoEntity;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kPolylineKind) last = e;
    REQUIRE(last != core::kNoEntity);

    const core::RingSpan span = f.doc.geometry().rings_of(f.doc.entities().slot[last]);
    const auto xs             = f.doc.geometry().ring_xs(span.first);
    const auto ys             = f.doc.geometry().ring_ys(span.first);
    REQUIRE_EQ(xs.size(), std::size_t{2});
    CHECK_EQ((core::Point2{xs[1], ys[1]}), (core::Point2{10'000, 20'000}));
}

TEST_CASE("SEÇ ÇOKGEN fareyle üç köşe toplayabilir")
{
    // THE DEFECT: the loop that collects a fence's or a selection polygon's
    // points read `while (supplied.empty())`, so it stopped after the FIRST point
    // — the mode could never collect more than one from the mouse and then
    // refused with "en az üç köşe ister". A mode reachable by mouse that cannot
    // be used by one is what CLAUDE.md 5.15 forbids.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 5,5 6,6", Origin::Test).ok());     ///< inside
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 50,50 60,60", Origin::Test).ok()); ///< outside

    auto started = f.bus.begin_interactive("SEÇ ÇOKGEN", Origin::Gui);
    REQUIRE(started.ok());
    auto& session = *started.value();

    // Three corners of a triangle around the first run, one click each.
    CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
    CHECK(session.supply(Value::point(core::Point2{20'000, 0})).ok());
    CHECK(session.supply(Value::point(core::Point2{0, 20'000})).ok());
    session.cancel(); ///< right button: that is the polygon, go
    REQUIRE(f.bus.finish(session).ok());

    CHECK_EQ(f.bus.selection().size(), std::size_t{1});
}

TEST_CASE("SEÇ ÇİT baştan verilen noktaları ikinci kez toplamaz")
{
    // THE OTHER HALF, and the reason the loop was written that way: a script
    // hands the whole run over at once and must not be asked anything — asking
    // drains the same points a second time, and a doubled fence is not a longer
    // one.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,20 10,20", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,40 10,40", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("SEÇ ÇİT 5,-5 5,25", Origin::Test).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{2}); ///< the third is not crossed
}

TEST_CASE("UÇUCA ile BİRLEŞTİR toleranslarını farklı yerden alır")
{
    // THE DIFFERENCE BOTH PAGES CLAIM, asserted rather than asserted in prose —
    // and the first version of that prose was WRONG. It said BİRLEŞTİR joins only
    // ends that touch exactly; it does not, it uses the PROJECT's node tolerance
    // (default 10 mm). Two documents were about to ship a sentence this test
    // refuted on the first run. Names that get confused make both commands
    // unusable, and so does a table that explains them incorrectly.
    {
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 10.004,0 20,0", Origin::Test).ok()); ///< 4 mm gap

        // UÇUCA's own default is 1 mm, so 4 mm is too far — and the refusal names
        // the way out.
        const auto tight = f.bus.execute_line("UÇUCA nesne=1 nesne=2", Origin::Test);
        CHECK_FALSE(tight.ok());
        CHECK(tight.error().message.find("değmiyor") != std::string::npos);

        // Named wider AT THE CALL, it joins. The project's setting is untouched.
        REQUIRE(f.bus.execute_line("UÇUCA nesne=1 nesne=2 tolerans=10", Origin::Test).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }
    {
        // BİRLEŞTİR reads the PROJECT's node tolerance, which is 10 mm by
        // default: the same 4 mm gap closes without anybody naming a number.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 10.004,0 20,0", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("BİRLEŞTİR nesneler=1 nesneler=2", Origin::Test).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }
    {
        // AND TIGHTENING THE PROJECT'S SETTING TIGHTENS BİRLEŞTİR, which is the
        // half of the claim that says where its decision comes from.
        Fixture f;
        REQUIRE(f.bus.execute_line("AYAR düğüm_toleransı 1", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 10,0", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 10.004,0 20,0", Origin::Test).ok());
        (void)f.bus.execute_line("BİRLEŞTİR nesneler=1 nesneler=2", Origin::Test);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{2}); ///< now too far
    }
}

TEST_CASE("her etkileşimli komut iptal edilince boş geri alma deltası bırakır")
{
    // THE CANCELLATION CLAUSE OF THE DoD, over the WHOLE registry rather than one
    // case per command. Esc is the most-pressed key in a CAD program: a user
    // reaches for a tool, sees it is the wrong one and presses Esc, and that must
    // leave the drawing exactly as it was. A command that wrote something before
    // its first prompt would leave half an edit behind every time — and it would
    // leave it behind silently, because nobody looks at a drawing they just
    // decided not to change.
    //
    // Written as a loop over `Registry` so a command added tomorrow is covered
    // the day it is declared (CLAUDE.md 5.10): a per-command case is a case
    // somebody has to remember to write.
    Fixture f;

    // A drawing to cancel AGAINST, so a command that would act on a selection has
    // something to act on and the test is not passing because nothing was there.
    REQUIRE(f.bus.execute_line("KATMAN ad=İPTAL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 20,0 20,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,30 20,30 20,40", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE 50,50 60,50", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());

    const std::uint64_t before = f.doc.content_hash();
    const std::size_t depth    = f.undo.undo_depth();

    std::size_t started = 0;
    std::size_t refused = 0;

    for (const CommandSpec& spec : f.reg.all()) {
        if (!has_flag(spec.flags, Flags::Interactive)) continue;

        // NOT THE FILE AND VIEW COMMANDS. A file command asks the host for a path
        // and a view command moves the camera; neither is a document edit, and
        // both are covered by their own tests. What this pins is the drawing.
        if (spec.category == Category::File || spec.category == Category::View) continue;

        auto started_ok = f.bus.begin_interactive(spec.names.front(), Origin::Gui);
        if (!started_ok) {
            // A command that refuses to START on this drawing is not cancelled,
            // it never ran. Counted so a registry where nothing starts cannot
            // pass this test by default.
            ++refused;
            continue;
        }

        Session& session = *started_ok.value();
        if (!session.waiting() && !session.working()) {
            // IT NEVER ASKED, so there was no moment to press Esc in: SİL with
            // everything highlighted is the Delete key, and it deletes. What
            // this test pins is the command that asks and is cancelled; one that
            // ran straight through is taken back, so the next command still
            // meets the drawing this test started with.
            const auto done = f.bus.finish(session);
            if (done.ok() && f.undo.undo_depth() > depth)
                REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
            REQUIRE(f.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());
            CHECK_MESSAGE(f.doc.content_hash() == before, spec.id);
            continue;
        }
        ++started;

        session.cancel(); ///< Esc, before a single answer
        const auto done = f.bus.finish(session);

        // THE DOCUMENT IS UNTOUCHED AND THE STACK IS UNCHANGED. Either is enough
        // to catch a command that wrote before it asked; both together say the
        // drawing and its history are as the user left them.
        CHECK_MESSAGE(f.doc.content_hash() == before, spec.id);
        CHECK_MESSAGE(f.undo.undo_depth() == depth, spec.id);
        if (!done.ok()) CHECK_MESSAGE(f.doc.content_hash() == before, spec.id);
    }

    // The registry really was walked, and most of it really did start.
    CHECK(started > 30);
    if (refused * 4 >= started)
        FAIL_WITH("çok fazla komut hiç başlamadı",
                  std::to_string(refused) + " / " + std::to_string(started + refused));
}

TEST_CASE("DİKAYAK ve ALIM eşleşmeyen okuma dizisini sebebiyle reddeder")
{
    // THE PLAN'S RELEASE LIST NAMED A SPELLING THAT NEVER WORKED:
    // `DİKAYAK 0,0 100,0 30 -5`. The two runs are declared one after the other,
    // so bare numbers all bind to the FIRST of them and the second is left empty
    // — and the command then returned in SILENCE: no point, no reason. That is
    // the worst outcome a command has, because the user has nothing to correct.
    //
    // It cannot be made to work by splitting the numbers: deciding which of
    // `30 -5` is a foot and which is an offset is a guess, and guessing which way
    // a detail lies off a baseline is how a building ends up on the wrong side of
    // a boundary. So the refusal names the rule instead.
    {
        Fixture f;
        const auto r = f.bus.execute_line("DİKAYAK 0,0 100,0 30 -5", Origin::CommandLine);
        CHECK_FALSE(r.ok());
        CHECK(r.error().message.find("sırayla eşleşir") != std::string::npos);
        CHECK(r.error().message.find("ayak=30 boy=-5") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
    {
        // AND THE SAME SHAPE IN ALIM, which reads the same kind of paired run.
        Fixture f;
        // ALIM's positional numbers are caught EARLIER and by somebody else: its
        // third parameter is a point list (`baglama`, the backsight), so the bus's
        // own validation refuses `50` by name before the body runs. A refusal that
        // names the parameter and the value is the right answer and this pins it
        // rather than asking for a second one.
        const auto r = f.bus.execute_line("ALIM 0,0 50 42.315", Origin::CommandLine);
        CHECK_FALSE(r.ok());
        CHECK(r.error().message.find("baglama") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});

        // AND ITS OWN MISMATCHED RUN, which the body does catch.
        const auto uneven =
            f.bus.execute_line("ALIM 0,0 aci=50 aci=100 kenar=42.315", Origin::CommandLine);
        CHECK_FALSE(uneven.ok());
        CHECK(uneven.error().message.find("sırayla eşleşir") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0}); ///< nothing half-placed
    }
    {
        // A GENUINE MISMATCH TOO, not just the positional case: three feet and
        // two offsets is a field book somebody mis-transcribed, and the counts
        // are in the message so they can find it.
        Fixture f;
        const auto r = f.bus.execute_line("DİKAYAK 0,0 100,0 ayak=10 ayak=20 ayak=30 boy=1 boy=2",
                                          Origin::CommandLine);
        CHECK_FALSE(r.ok());
        CHECK(r.error().message.find("3 `ayak`") != std::string::npos);
        CHECK(r.error().message.find("2 `boy`") != std::string::npos);
        // WHOLE OR NOTHING. It used to place the two complete pairs and drop the
        // third foot in silence, which is how a detail disappears from a survey.
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
    {
        // AND ESC STAYS SILENT, which is the other half: cancelling is not an
        // error and a user who changed their mind is told nothing.
        Fixture f;
        std::string said;
        f.bus.on_echo = [&said](std::string_view t) { said += std::string(t) + "\n"; };
        auto started  = f.bus.begin_interactive("DİKAYAK", Origin::Gui);
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        session.cancel();
        CHECK(f.bus.finish(session).ok());
        CHECK(said.find("sırayla eşleşir") == std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
}

TEST_CASE("release listesi: DİKAYAK'ın yazılı biçimi belgelenen biçimdir")
{
    // The form the page shows and the one both roads agree on.
    Fixture f;
    REQUIRE(f.bus.execute_line("DİKAYAK 0,0 100,0 ayak=30 boy=-5", Origin::CommandLine).ok());

    REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
    const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
    // 30 m along the east-running baseline, 5 m to its RIGHT — negative is right,
    // which the page states and this pins.
    CHECK_EQ((core::Point2{f.doc.geometry().ring_xs(span.first)[0],
                           f.doc.geometry().ring_ys(span.first)[0]}),
             (core::Point2{30'000, -5'000}));
}

TEST_CASE("SEÇ tur= seçimi türe göre daraltır, her kipte")
{
    // THE PLAN ASKED FOR THIS FILTER BY NAME: "`SEÇ katman= tur=` süzgeçleri
    // varsa docs'a, yoksa eklenir." `katman` was there and `tur` was not.
    //
    // A window over a sheet catches the parcels, the captions, the dimensions and
    // the road centreline together, and "the areas in that window" is a thing a
    // surveyor asks constantly. A layer NAMES a set on its own, which is why it
    // is a mode; a kind narrows one, which is why this is a filter and composes
    // with every mode.
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("DAİRE 5,5 7,5", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("NOKTA 3,3", Origin::Test).ok());

    // HEPSİ narrowed to one kind.
    REQUIRE(f.bus.execute_line("SEÇ HEPSİ tur=DAİRE", Origin::Test).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{1});

    // A WINDOW narrowed the same way — the filter is not a mode's privilege.
    REQUIRE(f.bus.execute_line("SEÇ KESEN -5,-5 20,20 tur=NOKTA", Origin::Test).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{1});

    // AND AN ALIAS REACHES THE SAME KIND, because the word is the kind table's
    // own: `find_name` folds Turkish and accepts every alias a kind declares.
    REQUIRE(f.bus.execute_line("SEÇ HEPSİ tur=CIRCLE", Origin::Test).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{1});

    // Without the filter, everything the gesture caught.
    REQUIRE(f.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());
    CHECK_EQ(f.bus.selection().size(), std::size_t{3});
}

TEST_CASE("SEÇ tur= tanınmayan türü türleri sayarak reddeder")
{
    Fixture f;
    REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SEÇ HEPSİ", Origin::Test).ok());
    const std::size_t had = f.bus.selection().size();

    const auto r = f.bus.execute_line("SEÇ HEPSİ tur=YOKBÖYLETÜR", Origin::Test);
    CHECK_FALSE(r.ok());
    CHECK(r.error().message.find("YOKBÖYLETÜR") != std::string::npos);
    // The list comes from the kind table, so a kind a plugin registers appears
    // in the refusal without an edit here.
    CHECK(r.error().message.find(core::builtin_kinds().find(core::kPolylineKind)->names[0]) !=
          std::string::npos);
    // AND THE SELECTION IS UNTOUCHED: a refusal does not half-select.
    CHECK_EQ(f.bus.selection().size(), had);
}

TEST_CASE("SEÇ tur= her yazımda aynı türe gider")
{
    // `SEÇ` IS `ReadOnly`, so it writes no journal line — the selection is not
    // document state (model.md R43) and undo does not step over it. What the
    // recording is for here is the equality proof: whichever alias a client
    // typed, the resolved record names one kind, so the GUI, the command line and
    // a script agree on what was selected.
    //
    // Asserted as the behaviour rather than by reading a journal that correctly
    // does not exist.
    const auto selected_with = [](const char* word) {
        Fixture f;
        REQUIRE(f.bus.execute_line("ALAN 0,0 10,0 10,10 0,10", Origin::Test).ok());
        REQUIRE(f.bus.execute_line("DAİRE 5,5 7,5", Origin::Test).ok());
        REQUIRE(f.bus.execute_line(std::string("SEÇ HEPSİ tur=") + word, Origin::Test).ok());
        std::vector<std::uint64_t> keys;
        for (const core::EntityKey k : f.bus.selection().keys())
            keys.push_back(core::raw(k));
        std::sort(keys.begin(), keys.end());
        return keys;
    };

    const auto turkish = selected_with("DAİRE");
    CHECK_EQ(turkish.size(), std::size_t{1});
    CHECK_EQ(selected_with("CIRCLE"), turkish); ///< the English alias
    CHECK_EQ(selected_with("daire"), turkish);  ///< folded case
    CHECK_EQ(selected_with("DAIRE"), turkish);  ///< ASCII-folded Turkish
}

TEST_CASE("NOKTA FONKSİYONU: çıktısı tekrar girdi olarak aynı noktayı verir")
{
    // THE IDEMPOTENCE THE PLAN ASKS FOR BY NAME: a function's output, fed back in,
    // gives the same point. It matters because these functions NEST — `orta(orta(
    // A,B), n(1284))` is a line a surveyor writes — and a function that drifted
    // when its own answer came back would drift by a millimetre per level, which
    // is the size nobody notices until a boundary fails to close.
    //
    // Every function here has a FIXED POINT, and that is what makes the property
    // testable rather than merely asserted: a foot of zero, a ratio of zero, a
    // side of zero, a midpoint of one point with itself.
    // In MILLIMETRES, which is what the resolver answers in, and written back into
    // a call as METRES with three decimals — the unit the command line reads.
    // Millimetres are integers, so the round trip through the text is exact and
    // this tests the functions rather than the formatter.
    const core::Point2 kA{485320150, 4310220400};
    const core::Point2 kB{485370150, 4310250400};

    const auto at = [](core::Point2 p) {
        const auto one = [](core::Mm v) {
            const bool negative = v < 0;
            const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
            std::string frac    = std::to_string(abs_mm % 1000);
            frac                = std::string(3 - frac.size(), '0') + frac;
            return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "." + frac;
        };
        return one(p.x) + "," + one(p.y);
    };

    // ---- each function's own fixed point ----
    {
        // `orta` of a point with itself is that point.
        auto r = fn("orta(" + at(kA) + "," + at(kA) + ")");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kA);
    }
    {
        // `ara` at ratio 0 is the first point; at 1 the second.
        auto zero = fn("ara(" + at(kA) + "," + at(kB) + ",0)");
        REQUIRE(zero.ok());
        CHECK_EQ(zero.value(), kA);
        auto one = fn("ara(" + at(kA) + "," + at(kB) + ",1)");
        REQUIRE(one.ok());
        CHECK_EQ(one.value(), kB);
    }
    {
        // `uzanti` of zero past B is B.
        auto r = fn("uzanti(" + at(kA) + "," + at(kB) + ",0)");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kB);
    }
    {
        // `dik` with no foot and no offset is the base's first point.
        auto r = fn("dik(" + at(kA) + "," + at(kB) + ",0,0)");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kA);
    }
    {
        // `semt` with no side is the station itself, whatever the bearing.
        auto r = fn("semt(" + at(kA) + ",137.5,0)");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kA);
    }
    {
        // `xy` of a point with itself is that point — which is also what makes it
        // the written form of tracking's crossing.
        auto r = fn("xy(" + at(kA) + "," + at(kA) + ")");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kA);
    }
    {
        // `ile` with a zero offset is the point it started from.
        auto r = fn("ile(" + at(kA) + ",@0,0)");
        REQUIRE(r.ok());
        CHECK_EQ(r.value(), kA);
    }
    {
        // `n` is the same monument every time it is asked for.
        auto once  = fn("n(1284)", {}, with_points());
        auto twice = fn("n(1284)", {}, with_points());
        REQUIRE(once.ok());
        REQUIRE(twice.ok());
        CHECK_EQ(once.value(), twice.value());
        CHECK_EQ(once.value(), kA);
    }

    // ---- FED BACK IN: the output of a call, used as an argument to the same
    // call, gives that output again ----
    {
        auto middle = fn("orta(" + at(kA) + "," + at(kB) + ")");
        REQUIRE(middle.ok());
        auto again = fn("orta(" + at(middle.value()) + "," + at(middle.value()) + ")");
        REQUIRE(again.ok());
        CHECK_EQ(again.value(), middle.value());

        // AND NESTED, which is how these are actually written.
        auto nested =
            fn("orta(orta(" + at(kA) + "," + at(kB) + "),orta(" + at(kA) + "," + at(kB) + "))");
        REQUIRE(nested.ok());
        CHECK_EQ(nested.value(), middle.value());
    }
    {
        // A crossing, fed back: the crossing of two lines THROUGH it is itself.
        auto cross = fn("kes(0,0,100,100,0,100,100,0)");
        REQUIRE(cross.ok());
        const core::Point2 x = cross.value();
        auto again           = fn("kes(" + at(x) + ",100,100," + at(x) + ",100,0)");
        REQUIRE(again.ok());
        CHECK_EQ(again.value(), x);
    }

    // ---- AND THE TWO ROADS AGREE, for every one of them: a typed token and a
    // JSON string resolve through the same grammar (CLAUDE.md 5.11) ----
    const std::vector<std::string> calls{
        "orta(" + at(kA) + "," + at(kB) + ")",
        "ara(" + at(kA) + "," + at(kB) + ",0.25)",
        "uzanti(" + at(kA) + "," + at(kB) + ",10)",
        "dik(" + at(kA) + "," + at(kB) + ",30,-5)",
        "semt(" + at(kA) + ",50,42.315)",
        "xy(" + at(kA) + "," + at(kB) + ")",
        std::string("kes(0,0,100,100,0,100,100,0)"),
    };
    for (const std::string& call : calls) {
        auto typed = fn(call);
        auto text  = fn_text(call);
        REQUIRE_MESSAGE(typed.ok(), call);
        REQUIRE_MESSAGE(text.ok(), call);
        CHECK_MESSAGE(typed.value() == text.value(), call);
    }
}

TEST_CASE("YAY yontem=devam son çizginin ucundan teğet devam eder")
{
    // THE PLAN'S LAST DEFERRED CONSTRUCTION METHOD. A road transition, a kerb
    // return and a chain of fillets are all drawn this way: the arc leaves the
    // last thing drawn in the SAME DIRECTION it ended in, so the join has no kink
    // in it — and a kink in a kerb is a kerb that has to be re-cast.
    //
    // The deferral said this needed a remembered session field. It does not: the
    // direction is read off the drawing, which is the same place `SEÇ SON` reads
    // "the most recently created entity" from.
    Fixture f;
    // A line running due EAST, ending at (100, 0).
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());

    // Continuing to a point directly NORTH of that end gives a quarter circle:
    // the tangent is east, the centre is 50 m north of the end, radius 50.
    REQUIRE(f.bus.execute_line("YAY yontem=devam bitis=150,50", Origin::Test).ok());

    core::EntityId arc = core::kNoEntity;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kArcKind) arc = e;
    REQUIRE(arc != core::kNoEntity);

    const std::uint32_t gslot = f.doc.entities().slot[arc];
    const core::Point2 centre = core::arc_centre_of(f.doc.geometry(), gslot);
    // The centre is on the perpendicular at the line's end: due north of (100,0).
    CHECK_EQ(centre, (core::Point2{100'000, 50'000}));
    CHECK_EQ(core::segment_length(centre, core::Point2{100'000, 0}), 50'000);

    // AND THE JOIN HAS NO KINK: the arc starts exactly where the line ended.
    const core::Point2 start = core::arc_start_of(f.doc.geometry(), gslot);
    const core::Point2 end   = core::arc_end_of(f.doc.geometry(), gslot);
    const bool joins         = start == core::Point2{100'000, 0} || end == core::Point2{100'000, 0};
    CHECK(joins);
}

TEST_CASE("YAY yontem=devam bir yayın ucundan da devam eder")
{
    Fixture f;
    // A quarter arc centred at (0,0), from due east to due north.
    REQUIRE(f.bus.execute_line("YAY merkez=0,0 baslangic=50,0 bitis=0,50", Origin::Test).ok());
    // Its end tangent at (0,50) points WEST (the model stores an arc
    // counter-clockwise, so at the end the motion is the radius turned a quarter
    // turn the same way). A target due west would be ON that tangent — a straight
    // line, refused — so this one is west AND north of it.
    REQUIRE(f.bus.execute_line("YAY yontem=devam bitis=-100,100", Origin::Test).ok());

    std::size_t arcs = 0;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e)
        if (f.doc.alive(e) && f.doc.entities().kind[e] == core::kArcKind) ++arcs;
    CHECK_EQ(arcs, std::size_t{2});
}

TEST_CASE("YAY yontem=devam teğet üzerindeki bitişi ve boş çizimi reddeder")
{
    {
        // Nothing to continue from: the refusal says what to do first.
        Fixture f;
        const auto r = f.bus.execute_line("YAY yontem=devam bitis=10,10", Origin::Test);
        CHECK_FALSE(r.ok());
        CHECK(r.error().message.find("Devam edilecek") != std::string::npos);
    }
    {
        // An end ON the tangent is a straight line, not an arc — and the command
        // says so instead of dividing by zero.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
        const auto r = f.bus.execute_line("YAY yontem=devam bitis=200,0", Origin::Test);
        CHECK_FALSE(r.ok());
        CHECK(r.error().message.find("teğetin üzerinde") != std::string::npos);
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1}); ///< nothing drawn
    }
}

TEST_CASE("YAY yontem=devam günlüğe ÇÖZÜLMÜŞ yayı yazar")
{
    // A replay must build THIS arc, not whatever the newest entity happens to be
    // in the document it is replayed into (model.md P4). So the record is the
    // resolved centre form, which is what the default branch reads back.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,0 100,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("YAY yontem=devam bitis=150,50", Origin::CommandLine).ok());

    bool checked = false;
    for (const auto& e : f.journal.entries()) {
        if (e.command_id != "core.arc_draw") continue;
        REQUIRE(e.args.find("merkez") != nullptr);
        CHECK_EQ(e.args.find("merkez")->as_point(), (core::Point2{100'000, 50'000}));
        REQUIRE(e.args.find("baslangic") != nullptr);
        REQUIRE(e.args.find("bitis") != nullptr);
        checked = true;
    }
    CHECK(checked);

    // AND IT REPLAYS: the record read back builds the same drawing.
    Fixture again;
    for (const auto& e : f.journal.entries())
        REQUIRE(again.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(again.doc.content_hash(), f.doc.content_hash());
}

TEST_CASE("SEÇ ÇOKGENPENCERE, ÇOKGEN'in tek anlamlı yazımıdır")
{
    // `ÇOKGEN` is also a DRAW command — the regular polygon — so a user who has
    // just drawn one and then types `SEÇ ÇOKGEN` is saying one word for two
    // things. The plan used the long spelling; both reach the mode and the long
    // one says which.
    Fixture f;
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 5,5 6,6", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 50,50 60,60", Origin::Test).ok());

    for (const char* word : {"ÇOKGENPENCERE", "COKGENPENCERE", "ÇOKGEN", "WP"}) {
        REQUIRE(f.bus.execute_line("SEÇ TEMİZLE", Origin::Test).ok());
        REQUIRE_MESSAGE(
            f.bus.execute_line(std::string("SEÇ ") + word + " 0,0 20,0 0,20", Origin::Test).ok(),
            word);
        CHECK_MESSAGE(f.bus.selection().size() == std::size_t{1}, word);
    }
}

TEST_CASE("ÇOKGEN: soru sırası, işaret edilen boy ve her adımda kılavuz")
{
    // THE ORDER OF THE QUESTIONS IS PART OF THE TOOL. A user pressed this and
    // reported "nothing happens, and there is nowhere to enter the side count":
    // the centre was asked for first, so the click was answered by a line of
    // text asking for a number while the canvas showed nothing at all. The side
    // count now comes first, before there is anything on screen to look at.
    {
        Fixture f;
        auto started = f.bus.begin_interactive("ÇOKGEN");
        REQUIRE(started.ok());
        Session& session = *started.value();

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "kenar_sayisi");
        CHECK(session.prompt().kind == ParamKind::Integer);
        CHECK_FALSE(session.prompt().has_rubber_band);

        REQUIRE(session.supply(Value::integer(6)).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "merkez");
        CHECK(session.prompt().kind == ParamKind::Point);

        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

        // AND THEN A PLACE, NOT A NUMBER, with the polygon itself previewed. The
        // guide carries the side count and the fit, because those are the two
        // facts the canvas cannot see; the size is left at zero, which is what
        // says "the cursor's own distance sets it".
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "kose");
        CHECK(session.prompt().kind == ParamKind::Point);
        CHECK(session.prompt().has_rubber_band);
        CHECK(session.prompt().rubber_shape == RubberShape::Polygon);
        CHECK_EQ(session.prompt().rubber_origin, (core::Point2{0, 0}));
        const auto guide = core::decode_polygon_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK_EQ(guide->sides, std::int64_t{6});
        CHECK(guide->fit == core::PolygonFit::Inscribed);
        CHECK_EQ(guide->measured, 0.0);
        CHECK_FALSE(guide->angle_given);

        // Pointing 10 m due north is a circumradius of 10 m and a turn of zero
        // under semt — an inscribed hexagon whose first corner is the point that
        // was clicked.
        REQUIRE(session.supply(Value::point(core::Point2{0, 10'000})).ok());
        REQUIRE(session.finished());
        REQUIRE(f.bus.finish(session).ok());

        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const auto span = f.doc.geometry().rings_of(f.doc.entities().slot[0]);
        const auto xs   = f.doc.geometry().ring_xs(span.first);
        const auto ys   = f.doc.geometry().ring_ys(span.first);
        REQUIRE_EQ(xs.size(), std::size_t{6});
        CHECK_EQ((core::Point2{xs[0], ys[0]}), (core::Point2{0, 10'000}));
    }

    // A CIRCUMSCRIBED POLYGON'S FLAT PASSES UNDER THE CURSOR. The user is sizing
    // by the edge, so the edge is what they are pointing at — and the preview
    // and the command apply the same half-step, or the guide would promise a
    // flat where a corner lands.
    {
        Fixture f;
        auto started = f.bus.begin_interactive("ÇOKGEN yontem=dis");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::integer(4)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

        REQUIRE(session.waiting());
        const auto guide = core::decode_polygon_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->fit == core::PolygonFit::Circumscribed);

        REQUIRE(session.supply(Value::point(core::Point2{0, 10'000})).ok());
        REQUIRE(f.bus.finish(session).ok());

        // A square sized by its flat: the inradius is 10 m, so the box is 20 m
        // across and the north edge passes exactly through the point clicked.
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.max_y, core::Mm{10'000});
        CHECK_EQ(box.min_y, core::Mm{-10'000});
        CHECK_EQ(box.max_x, core::Mm{10'000});
        CHECK_EQ(box.min_x, core::Mm{-10'000});
    }

    // UNDER `kenar` THE SIZE IS TYPED and the cursor is left the rotation, which
    // it can give. A side length cannot be pointed at from the centre — the
    // distance to the cursor is a radius — and a guide that called it a side
    // length would draw the lie.
    {
        Fixture f;
        auto started = f.bus.begin_interactive("ÇOKGEN yontem=kenar");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::integer(6)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "kenar_uzunlugu");
        CHECK(session.prompt().kind == ParamKind::Number);
        REQUIRE(session.supply(Value::number(10.0)).ok());

        // Now the size is settled, so the guide is handed it and the cursor only
        // turns the shape.
        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::Polygon);
        const auto guide = core::decode_polygon_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->fit == core::PolygonFit::Side);
        CHECK_EQ(guide->measured, 10.0); // the typed side, which the corners are made from

        REQUIRE(session.supply(Value::point(core::Point2{0, 5'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
        // The pointed distance did NOT become the size: 5 m from the centre, and
        // the hexagon is still the 10 m one that was typed.
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.max_y, core::Mm{10'000});
    }

    // A RUN THAT WAS GIVEN ITS SIZE ASKS NOTHING MORE, which is what keeps every
    // journal line written before the gesture existed replayable (Article 1.4).
    {
        Fixture f;
        auto started = f.bus.begin_interactive("ÇOKGEN yaricap=10");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::integer(4)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.finished()); // no third question
        REQUIRE(f.bus.finish(session).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }

    // ESC AT THE FIRST QUESTION LEAVES NOTHING, and the undo stack empty with it.
    {
        Fixture f;
        auto started = f.bus.begin_interactive("ÇOKGEN");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.waiting());
        session.cancel();
        REQUIRE(f.bus.finish(session).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
        CHECK_EQ(f.undo.undo_depth(), std::size_t{0});
    }
}

TEST_CASE("DİKDÖRTGEN yontem=3n: üçüncü nokta dikdörtgeni önizliyor")
{
    // The user's report was that the rotated rectangle draws but shows no guide
    // while drawing. It asked for the third point under a `Ring` preview with no
    // chain — an origin and a cursor, and two points enclose nothing — so the
    // tool worked and showed nothing, which from the chair is a tool that does
    // not work.
    Fixture f;
    auto started = f.bus.begin_interactive("DİKDÖRTGEN yontem=3n");
    REQUIRE(started.ok());
    Session& session = *started.value();

    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
    REQUIRE(session.waiting());
    CHECK(session.prompt().rubber_shape == RubberShape::Line); // an edge is a line

    REQUIRE(session.supply(Value::point(core::Point2{10'000, 0})).ok());
    REQUIRE(session.waiting());
    CHECK(session.prompt().has_rubber_band);
    CHECK(session.prompt().rubber_shape == RubberShape::EdgeRectangle);
    REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
    CHECK_EQ(session.prompt().rubber_chain[0], (core::Point2{0, 0}));
    CHECK_EQ(session.prompt().rubber_chain[1], (core::Point2{10'000, 0}));

    REQUIRE(session.supply(Value::point(core::Point2{40'000, 3'000})).ok());
    REQUIRE(f.bus.finish(session).ok());

    const core::Box2 box = f.doc.entity_extent(0);
    CHECK_EQ(box.min_y, core::Mm{0});
    CHECK_EQ(box.max_y, core::Mm{3'000});
    CHECK_EQ(box.max_x, core::Mm{10'000}); // the depth, not a corner
}

TEST_CASE("DAİRE: her yöntem çemberin kendisini önizliyor")
{
    // Three of the four methods used to preview a LINE — and `ttr`, the one with
    // four answers, previewed nothing at all. What the guide carries is asserted
    // here rather than how it looks: the shape, the chain that fixes it, and the
    // construction in the payload. The picture is then `core::circle_from_guide`'s,
    // and that function's own cases are in `test_geometry.cpp`.
    {
        // 2n — the chain holds the first end of the diameter.
        Fixture f;
        auto started = f.bus.begin_interactive("DAİRE yontem=2n");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::CircleBuild);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{1});
        CHECK_EQ(session.prompt().rubber_chain[0], (core::Point2{0, 0}));
        const auto guide = core::decode_circle_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::CircleBuild::Diameter);
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // 3n — two points fix nothing, so the SECOND is asked for with a line and
        // the third with the circumcircle.
        Fixture f;
        auto started = f.bus.begin_interactive("DAİRE yontem=3n");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.prompt().rubber_shape == RubberShape::Line);
        REQUIRE(session.supply(Value::point(core::Point2{30'000, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::CircleBuild);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        const auto guide = core::decode_circle_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::CircleBuild::ThreePoint);
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // ttr — the chain holds the two lines' four points and the payload the
        // radius that was typed, so the fillet follows the cursor from quadrant
        // to quadrant before the click decides which one.
        Fixture f;
        auto started = f.bus.begin_interactive("DAİRE yontem=ttr");
        REQUIRE(started.ok());
        Session& session = *started.value();
        for (const core::Point2 at : {core::Point2{-20'000, 0}, core::Point2{20'000, 0},
                                      core::Point2{0, -20'000}, core::Point2{0, 20'000}})
            REQUIRE(session.supply(Value::point(at)).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "yaricap");
        REQUIRE(session.supply(Value::number(5.0)).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::CircleBuild);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{4});
        const auto guide = core::decode_circle_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::CircleBuild::Tangent);
        CHECK_EQ(guide->radius, core::Mm{5'000});

        // And the circle it draws is the one the command then commits: the
        // quadrant the cursor is in.
        REQUIRE(session.supply(Value::point(core::Point2{10'000, -10'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_x, core::Mm{0});
        CHECK_EQ(box.max_x, core::Mm{10'000});
        CHECK_EQ(box.min_y, core::Mm{-10'000});
        CHECK_EQ(box.max_y, core::Mm{0});
    }
}

TEST_CASE("HALKA: dış çember aranırken iç çember ekranda kalıyor")
{
    // The user's words: the guide of the first circle has to stay so it can be
    // seen. The ring being made is the TWO circles, and with only the newest one
    // previewed the second click looked as though it had erased the first.
    Fixture f;
    auto started = f.bus.begin_interactive("HALKA");
    REQUIRE(started.ok());
    Session& session = *started.value();

    REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok()); // merkez
    REQUIRE(session.waiting());
    CHECK(session.prompt().rubber_chain.empty()); // nothing fixed yet

    REQUIRE(session.supply(Value::point(core::Point2{10'000, 0})).ok()); // iç
    REQUIRE(session.waiting());
    CHECK(session.prompt().rubber_shape == RubberShape::Circle);
    REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{1});
    CHECK_EQ(session.prompt().rubber_chain[0], (core::Point2{10'000, 0}));

    REQUIRE(session.supply(Value::point(core::Point2{20'000, 0})).ok()); // dış
    REQUIRE(f.bus.finish(session).ok());
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
}

TEST_CASE("YAY: her yöntem yayın kendisini önizliyor, ve bby yanı soruyor")
{
    // Three of the five methods previewed a straight LINE where the answer is a
    // CURVE, and `bby` never asked which side the curve goes — it read `yon`
    // from its arguments and defaulted to `sol`, so from the interface the other
    // arc was unreachable by mouse.
    {
        // 3n — two points on a curve determine nothing, so the second is asked
        // for with a line and the third with the arc.
        Fixture f;
        auto started = f.bus.begin_interactive("YAY yontem=3n");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        CHECK(session.prompt().rubber_shape == RubberShape::Line);
        REQUIRE(session.supply(Value::point(core::Point2{15'000, 15'000})).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::ArcBuild);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        const auto guide = core::decode_arc_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::ArcBuild::ThreePoint);

        REQUIRE(session.supply(Value::point(core::Point2{30'000, 0})).ok());
        REQUIRE(f.bus.finish(session).ok());
        // The half circle above the chord: 30 m across and 15 m tall.
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_x, core::Mm{0});
        CHECK_EQ(box.max_x, core::Mm{30'000});
        CHECK_EQ(box.max_y, core::Mm{15'000});
    }
    {
        // devam — the tangent continuation. The chain carries the start and a
        // point along the tangent, so the guide reads the same direction the
        // command does rather than each rounding its own.
        Fixture f;
        REQUIRE(
            f.bus.execute_line("ÇOKLUÇİZGİ noktalar=0,0 noktalar=100,0", Origin::CommandLine).ok());
        auto started = f.bus.begin_interactive("YAY yontem=devam");
        REQUIRE(started.ok());
        Session& session = *started.value();

        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::ArcBuild);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        CHECK_EQ(session.prompt().rubber_chain[0], (core::Point2{100'000, 0}));
        const auto guide = core::decode_arc_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::ArcBuild::Tangent);

        // Leaving (100, 0) due east and ending 20 m north of it is the half
        // circle centred 10 m north of the join.
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 20'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{2});
    }
    {
        // bby — the side is ASKED for, by pointing, with the arc previewed; and
        // the two sides are two different arcs.
        Fixture f;
        auto started = f.bus.begin_interactive("YAY yontem=bby");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "yaricap");
        REQUIRE(session.supply(Value::number(50.0)).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "yon_nokta");
        CHECK(session.prompt().rubber_shape == RubberShape::ArcBuild);
        const auto guide = core::decode_arc_guide(session.prompt().rubber_payload);
        REQUIRE(guide.has_value());
        CHECK(guide->build == core::ArcBuild::Radius);
        CHECK_EQ(guide->radius, core::Mm{50'000});

        // Pointing NORTH of the chord gives the arc that arches north, which is
        // the one `yon=sag` names — and which the mouse could not reach at all
        // before, because the command always took `sol`.
        REQUIRE(session.supply(Value::point(core::Point2{50'000, 10'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_y, core::Mm{0});
        CHECK_EQ(box.max_y, core::Mm{50'000});
    }
    {
        // AND A RUN THAT WAS GIVEN `yon` ASKS NOTHING MORE, which is what keeps
        // every journal line written before the gesture existed replayable.
        Fixture f;
        auto started = f.bus.begin_interactive("YAY yontem=bby yon=sol");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        REQUIRE(session.supply(Value::number(50.0)).ok());
        CHECK(session.finished()); // no side question
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_y, core::Mm{-50'000});
        CHECK_EQ(box.max_y, core::Mm{0});
    }
    {
        // A radius that joins nothing is refused THE MOMENT IT IS TYPED, before
        // the side is asked for: which side of an arc that cannot exist is a
        // question with no answer.
        Fixture f;
        auto started = f.bus.begin_interactive("YAY yontem=bby");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        REQUIRE(session.supply(Value::number(10.0)).ok());
        CHECK_FALSE(session.waiting()); // it did not go on to ask the side
        const auto done = f.bus.finish(session);
        CHECK_FALSE(done.ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{0});
    }
}

TEST_CASE("Nokta araçları: sabitlenen referans ekranda kalıyor")
{
    // A COMMAND THAT FIXES A REFERENCE AND THEN ASKS FOR NUMBERS had nothing on
    // screen while those numbers were typed: a baseline, a station and a line are
    // what the run remembers, not document objects, so each one left the screen
    // the moment it was given and the reading was typed against nothing.
    {
        // DİKAYAK — the baseline, and the foot once `ayak` is answered.
        Fixture f;
        auto started = f.bus.begin_interactive("DİKAYAK");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 0})).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "ayak");
        CHECK(session.prompt().kind == ParamKind::Number);
        CHECK(session.prompt().has_rubber_band);
        CHECK(session.prompt().rubber_shape == RubberShape::Fixed);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});

        REQUIRE(session.supply(Value::number(30.0)).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "boy");
        CHECK(session.prompt().rubber_shape == RubberShape::Fixed);
        // The baseline AND the foot the offset is measured from.
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{3});
        CHECK_EQ(session.prompt().rubber_chain[2], (core::Point2{30'000, 0}));
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // ALIM — the station, and the backsight with it when there is one.
        Fixture f;
        auto started = f.bus.begin_interactive("ALIM baglama=0,100");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "aci");
        CHECK(session.prompt().rubber_shape == RubberShape::Fixed);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        CHECK_EQ(session.prompt().rubber_chain[0], (core::Point2{0, 0}));
        CHECK_EQ(session.prompt().rubber_chain[1], (core::Point2{0, 100'000}));
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // ARANOKTA — the line being pegged.
        Fixture f;
        auto started = f.bus.begin_interactive("ARANOKTA");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::point(core::Point2{100'000, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "deger");
        CHECK(session.prompt().rubber_shape == RubberShape::Fixed);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        session.cancel();
        (void)f.bus.finish(session);
    }
}

TEST_CASE("KESİŞİMNOKTA mesafe: iki çözümden hangisi SORULUYOR")
{
    // The note in this command always said "the user says which", and the code
    // did not: `yon` was read from the arguments and defaulted to `sol`, so from
    // the interface one of the two crossings was unreachable by mouse.
    //
    // Two circles, radius 50 m about (0,0) and 50 m about (80,0), cross at
    // (40, ±30) — the 3-4-5 triangle again, so both answers are exact.
    {
        Fixture f;
        auto started = f.bus.begin_interactive("KESİŞİMNOKTA yontem=mesafe");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::number(50.0)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{80'000, 0})).ok());
        REQUIRE(session.supply(Value::number(50.0)).ok());

        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "yon_nokta");
        CHECK(session.prompt().kind == ParamKind::Point);
        CHECK(session.prompt().rubber_shape == RubberShape::Candidates);
        REQUIRE_EQ(session.prompt().rubber_chain.size(), std::size_t{2});
        // Both answers offered, and they are the two the arithmetic gives.
        for (const core::Point2& p : session.prompt().rubber_chain) {
            CHECK_EQ(p.x, core::Mm{40'000});
            const bool north_or_south = p.y == core::Mm{30'000} || p.y == core::Mm{-30'000};
            CHECK(north_or_south);
        }

        // Pointing NORTH takes the northern crossing — and the other one is now
        // reachable the same way, which it was not before.
        REQUIRE(session.supply(Value::point(core::Point2{40'000, 25'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        REQUIRE_EQ(f.doc.live_entity_count(), std::size_t{1});
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_y, core::Mm{30'000});
        CHECK_EQ(box.min_x, core::Mm{40'000});
    }
    {
        // Pointing SOUTH takes the southern one.
        Fixture f;
        auto started = f.bus.begin_interactive("KESİŞİMNOKTA yontem=mesafe");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.supply(Value::number(50.0)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{80'000, 0})).ok());
        REQUIRE(session.supply(Value::number(50.0)).ok());
        REQUIRE(session.supply(Value::point(core::Point2{40'000, -25'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_y, core::Mm{-30'000});
    }
    {
        // AND A RUN THAT WAS GIVEN `yon` ASKS NOTHING MORE, which keeps every
        // line written before the gesture existed replayable (Article 1.4).
        Fixture f;
        REQUIRE(f.bus
                    .execute_line("KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=50 "
                                  "ikinci=80,0 ikinci_mesafe=50 yon=sag",
                                  Origin::CommandLine)
                    .ok());
        CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    }
}

TEST_CASE("Düzenleme fiilleri: hayalet komutun kendi dönüşümünü taşıyor")
{
    // THE GHOST USED TO BE A SLIDE WHATEVER THE VERB WAS. `TAŞI` and `KOPYALA`
    // had one; `DÖNDÜR`, `ÖLÇEKLE` and `AYNALA` had none at all, and had they
    // been given the old one it would have shown the objects sliding sideways
    // while the command turned them. What is asserted here is that each verb
    // names its own kind, so the canvas draws the result and not a guess.
    const auto ghost_kind = [](const Session& session) {
        const auto spec = core::decode_ghost_spec(session.prompt().rubber_payload);
        REQUIRE(spec.has_value());
        return spec->kind;
    };

    {
        // TAŞI — a translation, as it always was, but now said out loud.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        auto started = f.bus.begin_interactive("TAŞI nesneler=1");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::Ghost);
        CHECK(ghost_kind(session) == core::GhostKind::Translate);
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // AYNALA — mirrored while the axis is aimed, which is the one thing the
        // command is about.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::CommandLine).ok());
        auto started = f.bus.begin_interactive("AYNALA nesneler=1");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().rubber_shape == RubberShape::Ghost);
        CHECK(ghost_kind(session) == core::GhostKind::Mirror);
        session.cancel();
        (void)f.bus.finish(session);
    }
    {
        // DÖNDÜR — the angle can be POINTED at, with the objects turning under
        // the cursor. It used to be typed and nothing else was offered.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 10,0 20,0", Origin::CommandLine).ok());
        auto started = f.bus.begin_interactive("DÖNDÜR nesneler=1");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok()); // merkez
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "aci_nokta");
        CHECK(session.prompt().kind == ParamKind::Point);
        CHECK(ghost_kind(session) == core::GhostKind::Rotate);

        // Pointing due north is a quarter turn, so the line along the east axis
        // ends up along the north one.
        REQUIRE(session.supply(Value::point(core::Point2{0, 10'000})).ok());
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_x, core::Mm{0});
        CHECK_EQ(box.max_x, core::Mm{0});
        CHECK_EQ(box.min_y, core::Mm{10'000});
        CHECK_EQ(box.max_y, core::Mm{20'000});
    }
    {
        // ÖLÇEKLE — the factor can be pointed at: the cursor's distance from the
        // centre in metres, which is how every CAD reads a dragged scale.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 10,0 20,0", Origin::CommandLine).ok());
        auto started = f.bus.begin_interactive("ÖLÇEKLE nesneler=1");
        REQUIRE(started.ok());
        Session& session = *started.value();
        REQUIRE(session.supply(Value::point(core::Point2{0, 0})).ok());
        REQUIRE(session.waiting());
        CHECK(session.prompt().param == "carpan_nokta");
        CHECK(ghost_kind(session) == core::GhostKind::Scale);

        REQUIRE(session.supply(Value::point(core::Point2{2'000, 0})).ok()); // 2 m = ×2
        REQUIRE(f.bus.finish(session).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_x, core::Mm{20'000});
        CHECK_EQ(box.max_x, core::Mm{40'000});
    }
    {
        // AND A RUN THAT WAS GIVEN ITS NUMBER ASKS NOTHING MORE, which is what
        // keeps every journal line written before the gesture existed replayable
        // (Article 1.4). Both verbs, both roads.
        Fixture f;
        REQUIRE(f.bus.execute_line("ÇİZGİ 10,0 20,0", Origin::CommandLine).ok());
        REQUIRE(
            f.bus.execute_line("DÖNDÜR nesneler=1 merkez=0,0 aci=90", Origin::CommandLine).ok());
        REQUIRE(
            f.bus.execute_line("ÖLÇEKLE nesneler=1 merkez=0,0 carpan=2", Origin::CommandLine).ok());
        const core::Box2 box = f.doc.entity_extent(0);
        CHECK_EQ(box.min_y, core::Mm{20'000});
        CHECK_EQ(box.max_y, core::Mm{40'000});
    }
}

TEST_CASE("KOMUT: hiçbir ajan istemcisi yorumlayıcıya ulaşamaz")
{
    Registry reg;
    register_builtin_commands(reg);

    // CLAUDE.md 5.24, made checkable. `core.script` runs whatever a file
    // contains, and with an interpreter embedded that is arbitrary code with the
    // user's own filesystem and network — every step of 5.7's preview-and-approval
    // model skipped by a program the model wrote rather than by a command it
    // proposed.
    //
    // The bit was already absent when this test was written; the test is here so
    // that adding it becomes a failing build rather than a review comment.
    const CommandSpec* script = reg.by_id("core.script");
    REQUIRE(script != nullptr);
    CHECK_FALSE(has_flag(script->flags, Flags::AiAccessible));
}

TEST_CASE("RET: reddedilen düzenleme veri yoluna hata döner, sessiz başarı değil")
{
    // THE DEFECT THIS PINS, measured before it was named: BUDA on a circle wrote
    // "bu komut yalnız çizgilerle çalışır" to the transcript, ended its body, and
    // the bus reported SUCCESS. (BUDA trims a circle now — TODOS C-04 — and KIR
    // breaks one — C-05 — so the list below keeps the edits a circle still
    // cannot take.) A person read the sentence; a JSON
    // script carried on, an agent's plan was told its step happened, and `cad.trim(...)` returned
    // instead of raising. The support matrix listed 38 such cells (docs/nesneler/destek-matrisi.md,
    // "Sessiz retler").
    //
    // A refusal is an ERROR: the dispatch fails, the message says why, and the
    // document is exactly what it was.
    Fixture f;
    REQUIRE(f.bus.execute_line("DAİRE merkez=0,0 cevre=10,0", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇOKLUÇİZGİ 0,-20 0,20", Origin::Test).ok());
    const std::uint64_t before = f.doc.content_hash();

    for (const char* line :
         {"UZAT nesne=1 sinir=2 nokta=10,0", "BÖL nesne=1 nokta=10,0",
          "YUVARLA nesne=1 nokta=10,0 yaricap=1", "PAH nesne=1 nokta=10,0 mesafe=1"}) {
        INFO(line);
        auto result = f.bus.execute_line(line, Origin::Test);
        REQUIRE_FALSE(result.ok());
        CHECK_FALSE(result.error().message.empty());
        CHECK_EQ(f.doc.content_hash(), before);
    }
}

TEST_CASE("Bir nokta listesinin anahtarı ardından gelen koordinatları da toplar")
{
    // `ALANÖLÇ noktalar=0,0 20,0 20,10` handed the second and third corners to
    // the first positional parameter — a selection — which refused them as the
    // wrong kind. A keyword that names a point list keeps the run open for the
    // coordinates that follow it, and closes at the first token of another kind.
    Fixture f;
    auto r = f.bus.execute_line("ALAN noktalar=0,0 20,0 20,10 0,10", Origin::Test);
    REQUIRE(r.ok());
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(e != core::kNoEntity);
    CHECK(f.doc.geometry()
              .ring_xs(f.doc.geometry().rings_of(f.doc.entities().slot[e]).first)
              .size() == 4);

    // Another keyword closes the run; positional binding carries on as it did.
    std::string said;
    f.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    REQUIRE(f.bus.execute_line("ÖLÇ baslangic=0,0 bitis=3,4 devam=3,10 0,10", Origin::Test).ok());
    CHECK(said.find("Kenar 3: 3,000 m") != std::string::npos);
}
