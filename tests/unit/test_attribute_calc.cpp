// SPDX-License-Identifier: GPL-3.0-or-later
//
// The row expression language and the field calculator (TODOS G-03): NULL is not zero and not
// empty, text keeps its leading zeros, a calculation is computed whole before it is written, it
// previews, it is one undo step, and the typed line, the window's session and a script leave one
// drawing.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/expression.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/document.hpp"
#include "piricad/script/json_runner.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>

using namespace piricad;
using namespace piricad::command;

namespace {

/// One row, as the table would hand it over.
FieldReader row_of(std::map<std::string, std::optional<std::string>> cells)
{
    return [cells = std::move(cells)](std::string_view name) -> std::optional<std::string> {
        const auto at = cells.find(std::string(name));
        return at == cells.end() ? std::nullopt : at->second;
    };
}

/// The text an expression gives on `row`, or the failure's message.
std::string say(const char* expr, const FieldReader& row)
{
    auto compiled = Expression::compile(expr);
    if (!compiled) return "DERLEME: " + compiled.error().message;
    auto value = compiled.value().evaluate(row);
    if (!value) return "HATA: " + value.error().message;
    if (value.value().is_null()) return "NULL";
    return value.value().as_text();
}

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

    bool run(const std::string& line, Origin origin = Origin::Test)
    {
        said.clear();
        auto r = bus.execute_line(line, origin);
        if (!r) said += "[hata] " + r.error().message + "\n";
        return r.ok();
    }

    /// The cell as the table shows it, or "boş".
    std::string cell(const char* column, std::int64_t key) const
    {
        const core::AttrId col    = doc.attributes().find(column);
        const core::EntityId slot = doc.slot_of(static_cast<core::EntityKey>(key));
        auto got                  = doc.attribute(col, slot);
        return got && got.value().present
                   ? core::attr_display(got.value(), core::DecimalMark::Point)
                   : std::string("boş");
    }
};

/// Three parcels, 1200, 600 and 300 m², with their ada and parsel numbers; the third has no parsel
/// number (NULL, not zero).
void three_parcels(Rig& r)
{
    REQUIRE(r.run("KATMAN ad=PARSEL"));
    REQUIRE(r.run("ALAN noktalar=0,0 40,0 40,30 0,30"));     // 1200 m²
    REQUIRE(r.run("ALAN noktalar=50,0 80,0 80,20 50,20"));   // 600 m²
    REQUIRE(r.run("ALAN noktalar=90,0 110,0 110,15 90,15")); // 300 m²
    REQUIRE(r.run("SÜTUN kimlik=ada_no tur=tam_sayi"));
    REQUIRE(r.run("SÜTUN kimlik=parsel_no tur=tam_sayi"));
    REQUIRE(r.run("SÜTUN kimlik=alan_m2 tur=ondalik basamak=2"));
    REQUIRE(r.run("SÜTUN kimlik=etiket tur=metin"));
    REQUIRE(r.run("SÜTUN kimlik=taks tur=ondalik basamak=2"));
    REQUIRE(r.run("SÜTUN kimlik=kat tur=tam_sayi"));
    for (const char* line :
         {"ÖZNİTELİK ada_no 1 1284", "ÖZNİTELİK ada_no 2 1284", "ÖZNİTELİK ada_no 3 1284",
          "ÖZNİTELİK parsel_no 1 7", "ÖZNİTELİK parsel_no 2 12"})
        REQUIRE(r.run(line));
}

} // namespace

TEST_CASE("ifade dili: NULL sıfır değildir, boş metin NULL değildir")
{
    const FieldReader row = row_of({{"a", "5"}, {"b", std::nullopt}, {"bos", ""}, {"ad", "Konut"}});
    CHECK_EQ(say("\"a\" + 2", row), "7");
    CHECK_EQ(say("\"b\" + 2", row), "NULL"); // arithmetic with NULL is NULL
    CHECK_EQ(say("\"b\" IS NULL", row), "evet");
    CHECK_EQ(say("\"bos\" IS NULL", row), "hayır"); // an empty text is a value
    CHECK_EQ(say("\"b\" = 0", row), "hayır");       // NULL is not zero
    CHECK_EQ(say("\"b\" != 0", row), "hayır");      // ... and a comparison with it is false
    CHECK_EQ(say("coalesce(\"b\", 0)", row), "0");
    CHECK_EQ(say("coalesce(\"b\", \"bos\", 'x')", row), ""); // the empty text answers, not 'x'
    CHECK_EQ(say("\"ad\" || '-' || \"b\"", row), "NULL");    // || with NULL is NULL
    CHECK_EQ(say("\"ad\" || '-' || coalesce(\"b\", 'yok')", row), "Konut-yok");
    CHECK_EQ(say("nullif(\"a\", 5)", row), "NULL");
    CHECK_EQ(say("nullif(\"a\", 6)", row), "5");
}

TEST_CASE("ifade dili: sayılar, metinler, işlevler ve CASE")
{
    const FieldReader row = row_of({{"alan", "3482.64"},
                                    {"ada", "0012"},
                                    {"parsel", "7"},
                                    {"ad", "çiçek İş"},
                                    {"f", "1284/A"}});
    // Arithmetic, precedence and the power operator's right associativity.
    CHECK_EQ(say("1 + 2 * 3", row), "7");
    CHECK_EQ(say("(1 + 2) * 3", row), "9");
    CHECK_EQ(say("2 ^ 3 ^ 2", row), "512");
    CHECK_EQ(say("-\"parsel\" + 10", row), "3");
    CHECK_EQ(say("10 % 4", row), "2");
    CHECK_EQ(say("round(\"alan\" * 0.4, 2)", row), "1393.06");
    CHECK_EQ(say("round(2.5)", row), "3"); // half away from zero
    CHECK_EQ(say("floor(2.9) + ceil(2.1) + abs(-1)", row), "6");
    CHECK_EQ(say("min(3, 1, 2) + max(3, 1, 2)", row), "4");
    CHECK_EQ(say("sqrt(16) + pow(2, 3)", row), "12");

    // A CELL KEEPS ITS DIGITS: `0012` is the number twelve to arithmetic and `0012` to a
    // concatenation.
    CHECK_EQ(say("\"ada\" + 1", row), "13");
    CHECK_EQ(say("\"ada\" || '/' || \"parsel\"", row), "0012/7");
    CHECK_EQ(say("lpad(\"parsel\", 4, '0')", row), "0007");
    CHECK_EQ(say("rpad('ab', 4, '.')", row), "ab..");

    // Text: Turkish casing from the shared table, code points and not bytes.
    CHECK_EQ(say("upper(\"ad\")", row), "ÇİÇEK İŞ");
    CHECK_EQ(say("lower('IİÇŞ')", row), "ıiçş");
    CHECK_EQ(say("length(\"ad\")", row), "8");
    CHECK_EQ(say("left(\"ad\", 3)", row), "çiç");
    CHECK_EQ(say("right(\"ad\", 2)", row), "İş");
    CHECK_EQ(say("substr(\"ad\", 2, 3)", row), "içe");
    CHECK_EQ(say("replace(\"f\", '/', '-')", row), "1284-A");
    CHECK_EQ(say("trim('  a b ')", row), "a b");
    CHECK_EQ(say("contains(\"f\", 'A')", row), "evet");
    CHECK_EQ(say("starts_with(\"f\", '12')", row), "evet");
    CHECK_EQ(say("ends_with(\"f\", '/B')", row), "hayır");
    CHECK_EQ(say("to_number('12.5') + 1", row), "13.5");
    CHECK_EQ(say("to_number('abc')", row), "NULL"); // not a number is NULL, and `coalesce` answers
    CHECK_EQ(say("to_text(12) || 'm'", row), "12m");

    // CASE, IF and short-circuit (the branch not taken is not evaluated, so 1/0 there is no error).
    CHECK_EQ(say("CASE WHEN \"alan\" > 5000 THEN 'Büyük' WHEN \"alan\" > 3000 THEN 'Orta' ELSE "
                 "'Küçük' END",
                 row),
             "Orta");
    CHECK_EQ(say("CASE WHEN 1 = 2 THEN 'x' END", row), "NULL");
    CHECK_EQ(say("if(\"parsel\" > 5, 'evet', 1 / 0)", row), "evet");
    CHECK_EQ(say("\"parsel\" > 5 OR 1 / 0 > 1", row), "evet");

    // QUOTES: a single quote doubled writes itself.
    CHECK_EQ(say("'O''Hara'", row), "O'Hara");
}

TEST_CASE("ifade dili: hatalar yeri ve nedeni söyler")
{
    const FieldReader row = row_of({{"a", "5"}, {"s", "abc"}});
    CHECK(say("1 / 0", row).starts_with("HATA: sıfıra bölme"));
    CHECK(say("\"s\" + 1", row).find("'abc' bir sayı değil") != std::string::npos);
    CHECK(say("sqrt(-1)", row).find("negatif") != std::string::npos);
    CHECK(say("round(1.5, 20)", row).find("0–12") != std::string::npos);
    CHECK(say("Konut = 1", row).find("tek tırnak") != std::string::npos); // the commonest mistake
    CHECK(say("nope(1)", row).find("bilinmeyen işlev 'nope'") != std::string::npos);
    CHECK(say("round()", row).find("1–2 değer ister") != std::string::npos);
    CHECK(say("(1 + 2", row).find("Kapanmayan parantez") != std::string::npos);
    CHECK(say("\"a", row).find("Kapanmayan sütun") != std::string::npos);
    CHECK(say("'a", row).find("Kapanmayan metin") != std::string::npos);
    CHECK(say("1 +", row).find("beklenmedik biçimde bitti") != std::string::npos);
    CHECK(say("12abc", row).find("sayının hemen ardından") != std::string::npos);
    CHECK(say("1 2", row).find("beklenmeyen") != std::string::npos);
    CHECK(say("CASE 1 END", row).find("WHEN") != std::string::npos);

    // DEEPLY NESTED INPUT is refused, not recursed into: a hostile or corrupt expression must not
    // take the stack down (the filter bar and the fuzz harness both feed this arbitrary text).
    const std::string nested = std::string(5000, '(') + "1" + std::string(5000, ')');
    CHECK(say(nested.c_str(), row).find("çok iç içe") != std::string::npos);
    const std::string nots = [] {
        std::string s;
        for (int i = 0; i < 5000; ++i)
            s += "NOT ";
        return s + "1";
    }();
    CHECK(say(nots.c_str(), row).find("çok iç içe") != std::string::npos);
}

TEST_CASE("ifade dili: bir kez derlenir, çok kez çalışır; sütun adları bilinir")
{
    auto compiled = Expression::compile("\"a\" * \"b\" + $alan");
    REQUIRE(compiled.ok());
    REQUIRE_EQ(compiled.value().columns().size(), std::size_t{3});
    CHECK_EQ(compiled.value().columns()[0], std::string("a"));
    CHECK_EQ(compiled.value().columns()[2], std::string("$alan"));
    for (int i = 1; i <= 3; ++i) {
        const FieldReader row = row_of({{"a", std::to_string(i)}, {"b", "10"}, {"$alan", "0.5"}});
        auto value            = compiled.value().evaluate(row);
        REQUIRE(value.ok());
        CHECK_EQ(value.value().as_text(), std::to_string(i * 10) + ".5");
    }
}

TEST_CASE(
    "ÖZNİTELİKHESAPLA: geometriden hesaplar, tek geri alma adımıdır, önceki değerler geri gelir")
{
    Rig r;
    three_parcels(r);
    const std::size_t depth = r.undo.undo_depth();

    REQUIRE_MESSAGE(r.run("ÖZNİTELİKHESAPLA ad=alan_m2 ifade=$alan katman=PARSEL"), r.said);
    CHECK_EQ(r.cell("alan_m2", 1), "1200.00");
    CHECK_EQ(r.cell("alan_m2", 2), "600.00");
    CHECK_EQ(r.cell("alan_m2", 3), "300.00");
    CHECK_EQ(r.undo.undo_depth() - depth, std::size_t{1});
    CHECK(r.said.find("Hesaplandı: alan_m2 = $alan") != std::string::npos);
    CHECK(r.said.find("3 satır değişti") != std::string::npos);
    CHECK(r.said.find("[1] boş → 1200.00") != std::string::npos);

    // ONE STEP BACK puts every cell back to empty — NULL again, not zero.
    REQUIRE(r.run("GERİAL"));
    CHECK_EQ(r.cell("alan_m2", 1), "boş");
    CHECK_EQ(r.cell("alan_m2", 3), "boş");
    REQUIRE(r.run("YİNELE"));
    CHECK_EQ(r.cell("alan_m2", 2), "600.00");

    // A SECOND RUN changes nothing and says so: no rows written, no revision bump, no undo step.
    const auto hash = r.doc.content_hash();
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=alan_m2 ifade=$alan katman=PARSEL"));
    CHECK(r.said.find("0 satır değişti · 3 aynı kaldı") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);
}

TEST_CASE("ÖZNİTELİKHESAPLA: ifade satırın hücrelerini, NULL'u ve metni doğru işler")
{
    Rig r;
    three_parcels(r);
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=alan_m2 ifade=$alan katman=PARSEL"));

    // Arithmetic over a column, rounded to the column's own two decimals.
    REQUIRE_MESSAGE(
        r.run("ÖZNİTELİKHESAPLA ad=taks ifade=\"round(\\\"alan_m2\\\" / 3000, 2)\" katman=PARSEL"),
        r.said);
    CHECK_EQ(r.cell("taks", 1), "0.40");
    CHECK_EQ(r.cell("taks", 2), "0.20");
    CHECK_EQ(r.cell("taks", 3), "0.10");

    // Text, with the NULL handled where it is: parcel 3 has no parsel number.
    REQUIRE_MESSAGE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade=\"\\\"ada_no\\\" || '/' || "
                          "coalesce(to_text(\\\"parsel_no\\\"), '?')\" "
                          "katman=PARSEL"),
                    r.said);
    CHECK_EQ(r.cell("etiket", 1), "1284/7");
    CHECK_EQ(r.cell("etiket", 3), "1284/?");

    // Without the coalesce the NULL propagates: the label is EMPTY (NULL), not "1284/".
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade=\"\\\"ada_no\\\" || '/' || "
                  "to_text(\\\"parsel_no\\\")\" katman=PARSEL"));
    CHECK_EQ(r.cell("etiket", 3), "boş");
    CHECK(r.said.find("1 boşaltıldı") != std::string::npos);

    // A NUMBER INTO A WHOLE-NUMBER COLUMN is checked: 1.5 is not a kat.
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.run("ÖZNİTELİKHESAPLA ad=kat ifade=\"\\\"alan_m2\\\" / 800\" katman=PARSEL"));
    CHECK(r.said.find("tam sayı sütunu bir kesir tutamaz") != std::string::npos);
    CHECK(r.said.find("nesne 1") !=
          std::string::npos);             // the first row that cannot be made is named
    CHECK_EQ(r.doc.content_hash(), hash); // ... and NOTHING was written
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=kat ifade=\"ceil(\\\"alan_m2\\\" / 800)\" katman=PARSEL"));
    CHECK_EQ(r.cell("kat", 1), "2");
    CHECK_EQ(r.cell("kat", 3), "1");
}

TEST_CASE(
    "ÖZNİTELİKHESAPLA: bir satır başarısız olursa hiçbiri yazılmaz; hata satırı adıyla söyler")
{
    Rig r;
    three_parcels(r);
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=alan_m2 ifade=$alan katman=PARSEL"));
    // parsel_no is 7, 12, NULL: dividing by (parsel_no - 12) fails on parcel 2 AFTER parcel 1 was
    // fine.
    const auto hash  = r.doc.content_hash();
    const auto steps = r.undo.undo_depth();
    CHECK_FALSE(r.run("ÖZNİTELİKHESAPLA ad=taks ifade=\"round(10 / (\\\"parsel_no\\\" - 12), 2)\" "
                      "katman=PARSEL"));
    CHECK(r.said.find("nesne 2") != std::string::npos);
    CHECK(r.said.find("sıfıra bölme") != std::string::npos);
    CHECK(r.said.find("ifade:") != std::string::npos);
    CHECK_EQ(r.cell("taks", 1), "boş");
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), steps);
}

TEST_CASE("ÖZNİTELİKHESAPLA: süzgeç, önizleme ve kapsam")
{
    Rig r;
    three_parcels(r);
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=alan_m2 ifade=$alan katman=PARSEL"));

    // THE FILTER is the table's own language: only the parcels over 500 m².
    REQUIRE_MESSAGE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade='BÜYÜK' katman=PARSEL "
                          "filtre=\"\\\"alan_m2\\\" > 500\""),
                    r.said);
    CHECK_EQ(r.cell("etiket", 1), "BÜYÜK");
    CHECK_EQ(r.cell("etiket", 2), "BÜYÜK");
    CHECK_EQ(r.cell("etiket", 3), "boş");
    CHECK(r.said.find("1 süzgece uymadı") != std::string::npos);

    // THE PREVIEW writes nothing and says what a run would do.
    const auto hash  = r.doc.content_hash();
    const auto steps = r.undo.undo_depth();
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade='YENİ' katman=PARSEL onizle=evet"));
    CHECK(r.said.find("Önizleme (hiçbir şey yazılmadı)") != std::string::npos);
    CHECK(r.said.find("3 satır değişecek") != std::string::npos);
    CHECK(r.said.find("[1] BÜYÜK → YENİ") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), steps);
    CHECK_EQ(r.cell("etiket", 1), "BÜYÜK");

    // SCOPE: named objects beat everything; a selection beats the active layer.
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade='A' nesneler=3"));
    CHECK_EQ(r.cell("etiket", 3), "A");
    CHECK_EQ(r.cell("etiket", 1), "BÜYÜK");
    REQUIRE(r.run("SEÇ NESNE nesneler=1 nesneler=2"));
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade='S'"));
    CHECK(r.said.find("kapsam: seçim (2 nesne)") != std::string::npos);
    CHECK_EQ(r.cell("etiket", 1), "S");
    CHECK_EQ(r.cell("etiket", 3), "A");
    REQUIRE(r.run("SEÇ TEMİZLE"));
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade='L'"));
    CHECK(r.said.find("'PARSEL' katmanı (3 nesne)") != std::string::npos);
    CHECK_EQ(r.cell("etiket", 3), "L");

    // WHAT CANNOT BE DONE says why and how.
    CHECK_FALSE(r.run("ÖZNİTELİKHESAPLA ad=yok_boyle ifade=1 katman=PARSEL"));
    CHECK(r.said.find("Bilinmeyen sütun: 'yok_boyle'") != std::string::npos);
    CHECK(r.said.find("SÜTUN kimlik=") != std::string::npos);
    CHECK_FALSE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade=Konut katman=PARSEL"));
    CHECK(r.said.find("tek tırnak") != std::string::npos);
    CHECK_FALSE(r.run("ÖZNİTELİKHESAPLA ad=etiket ifade=1 katman=YOKTUR"));
    CHECK(r.said.find("Bilinmeyen katman") != std::string::npos);
}

TEST_CASE("PROOF: ÖZNİTELİKHESAPLA komut satırından, betikten ve günlük oynatmasından aynı belgeyi "
          "bırakır")
{
    Rig typed, scripted, replayed; // `replayed` starts EMPTY: the whole journal builds it
    for (Rig* r : {&typed, &scripted})
        three_parcels(*r);
    const std::string line =
        "ÖZNİTELİKHESAPLA ad=alan_m2 ifade=\"round($alan / 1000, 3)\" katman=PARSEL";
    REQUIRE(typed.run(line, Origin::CommandLine));
    script::JsonRunner runner(scripted.bus, script::Sandbox::Project);
    auto ran = runner.run_text(
        R"json({"ad": "Hesapla", "komutlar": [{"cmd": "core.attribute_calc", "args": {
        "ad": "alan_m2", "ifade": "round($alan / 1000, 3)", "katman": "PARSEL"}}]})json");
    REQUIRE_MESSAGE(ran.ok(), (ran.ok() ? std::string() : ran.error().message));
    CHECK_EQ(typed.doc.content_hash(), scripted.doc.content_hash());

    // THE JOURNAL REPLAYS: the line is in it with the column's canonical id, the expression and the
    // scope as it was resolved — not a second command, not a hidden state.
    for (const auto& e : typed.journal.entries()) {
        auto replay = replayed.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        if (!replay) FAIL_WITH(e.command_id, replay.error().message);
    }
    CHECK_EQ(replayed.doc.content_hash(), typed.doc.content_hash());
    bool found = false;
    for (const auto& e : typed.journal.entries())
        if (e.command_id == "core.attribute_calc") {
            found = true;
            CHECK_EQ(e.args.get("ad").as_text(), std::string("alan_m2"));
            CHECK_EQ(e.args.get("ifade").as_text(), std::string("round($alan / 1000, 3)"));
            CHECK_EQ(e.args.get("katman").as_text(), std::string("PARSEL"));
        }
    CHECK(found);

    // A SELECTION is session state and is never journalled; the replay needs the objects it held.
    Rig sel, back; // `back` starts empty too
    three_parcels(sel);
    REQUIRE(sel.run("SEÇ NESNE nesneler=1 nesneler=2"));
    REQUIRE(sel.run("ÖZNİTELİKHESAPLA ad=etiket ifade='X'"));
    for (const auto& e : sel.journal.entries())
        if (e.command_id == "core.attribute_calc") {
            CHECK(e.args.has("nesneler"));
            CHECK_FALSE(e.args.has("katman"));
        }
    for (const auto& e : sel.journal.entries()) {
        if (e.command_id == "core.select") continue; // the selection is not part of the record
        auto replay = back.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        if (!replay) FAIL_WITH(e.command_id, replay.error().message);
    }
    CHECK_EQ(back.doc.content_hash(), sel.doc.content_hash());
}

TEST_CASE("ÖZNİTELİKHESAPLA: istem sırasında Esc hiçbir şey bırakmaz")
{
    Rig r;
    three_parcels(r);
    const auto hash  = r.doc.content_hash();
    const auto steps = r.undo.undo_depth();
    auto started     = r.bus.begin_interactive("ÖZNİTELİKHESAPLA", Origin::CommandLine);
    REQUIRE(started.ok());
    auto& session = *started.value();
    CHECK(session.waiting()); // it asks which column
    session.cancel();         // ESC
    CHECK(r.bus.finish(session).ok());
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), steps);
}

TEST_CASE("ÖZNİTELİKHESAPLA: uzunluk sütununa metre yazılır, milimetre saklanır; yazı olarak "
          "verilen milimetredir")
{
    Rig r;
    three_parcels(r);
    REQUIRE(r.run("SÜTUN kimlik=cephe tur=uzunluk"));
    // `$uzunluk` is the perimeter in METRES (1200 m² parcel 40 x 30: 140 m), the column shows
    // metres.
    REQUIRE_MESSAGE(r.run("ÖZNİTELİKHESAPLA ad=cephe ifade=$uzunluk katman=PARSEL"), r.said);
    CHECK_EQ(r.cell("cephe", 1), "140");
    CHECK_EQ(r.cell("cephe", 3), "70");
    // The store holds 140 000 millimetres.
    const core::AttrId col = r.doc.attributes().find("cephe");
    auto held              = r.doc.attribute(col, r.doc.slot_of(static_cast<core::EntityKey>(1)));
    REQUIRE(held.ok());
    CHECK_EQ(held.value().number, std::int64_t{140000});
    // Millimetre precision survives: 12.3456 m is 12 346 mm.
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=cephe ifade=12.3456 nesneler=1"));
    CHECK_EQ(r.doc.attribute(col, r.doc.slot_of(static_cast<core::EntityKey>(1))).value().number,
             std::int64_t{12346});
    // A TEXT is millimetres, as `ÖZNİTELİK deger=` reads it.
    REQUIRE(r.run("ÖZNİTELİKHESAPLA ad=cephe ifade=\"'2500'\" nesneler=2"));
    CHECK_EQ(r.doc.attribute(col, r.doc.slot_of(static_cast<core::EntityKey>(2))).value().number,
             std::int64_t{2500});
}

TEST_CASE("ifade dili: pencerenin ve kılavuzun listeleri ayrıştırıcının kendi tablosundan gelir")
{
    // EVERY FUNCTION THE WINDOW LISTS IS ONE THE PARSER KNOWS: calling it with the wrong number of
    // arguments is refused for its arity, never as an unknown word.
    const auto functions = expression_functions();
    REQUIRE(functions.size() >= 20);
    for (const FunctionHelp& f : functions) {
        CHECK_FALSE(f.help.empty());
        CHECK_MESSAGE(f.usage.rfind(f.name + "(", 0) == 0, f.usage);
        auto wrong = Expression::compile(f.name + "()");
        // `()` is right for none of them, so a refusal is expected; what it must NOT say is
        // "bilinmeyen işlev".
        REQUIRE_FALSE(wrong.ok());
        CHECK_MESSAGE(wrong.error().message.find("bilinmeyen işlev") == std::string::npos, f.name);
    }
    // And a word the table does not hold IS reported as unknown, so the check above can fail.
    auto unknown = Expression::compile("hicbir_islev(1)");
    REQUIRE_FALSE(unknown.ok());
    CHECK_NE(unknown.error().message.find("bilinmeyen işlev"), std::string::npos);

    // EVERY `$` WORD THE WINDOW LISTS IS ANSWERED BY A ROW (not NULL, not an error) on a closed
    // shape.
    Rig r;
    three_parcels(r);
    REQUIRE(r.run("SÜTUN kimlik=deneme tur=metin"));
    for (const FunctionHelp& word : expression_pseudo_columns()) {
        REQUIRE_MESSAGE(
            r.run("ÖZNİTELİKHESAPLA ad=deneme ifade=to_text(" + word.usage + ") nesneler=1"),
            word.usage << ": " << r.said);
        CHECK_MESSAGE(r.cell("deneme", 1) != "boş", word.usage);
    }
}

TEST_CASE("kılavuz: ifade dili sayfası dilin her işlevini ve her $ sözcüğünü söyler")
{
    // The manual's tables are written by hand, so this is what keeps them honest: a function added
    // to the parser's table and not to the page fails here, in the same run.
    std::ifstream in(std::string(PIRICAD_DOCS_DIR) + "/veri/ifade-dili.md");
    REQUIRE(in.good());
    const std::string page((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (const FunctionHelp& f : expression_functions())
        CHECK_MESSAGE(page.find("`" + f.usage + "`") != std::string::npos, f.usage);
    for (const FunctionHelp& w : expression_pseudo_columns())
        CHECK_MESSAGE(page.find("`" + w.usage + "`") != std::string::npos, w.usage);
}
