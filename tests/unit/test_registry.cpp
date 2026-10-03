// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — tests: the command registry as the program assembles it.
//
// Six modules register commands — the built-ins, geodesy, cadastre, surface, the
// processing tools and the AI layer's own — and four places put them on one
// registry: the application's controller, and the three generators (docgen,
// envanter, kapsam), in two different orders. A name two modules claim makes the
// later `Registry::add` fail, and four of the six registration functions drop
// that status: the command was simply not in the program, the build was clean
// and every test that registered one module at a time was green.
#include "piricad_test.hpp"

#include "piricad/ai/commands.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/domain/cadastre/commands.hpp"
#include "piricad/domain/geodesy/commands.hpp"
#include "piricad/domain/surface/commands.hpp"
#include "piricad/processing/registry.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace piricad;
using namespace piricad::command;

namespace {

using Register = void (*)(Registry&);

/// The order the application registers in (`Controller`).
constexpr Register kApplicationOrder[] = {
    &register_builtin_commands,
    &domain::geodesy::register_geodesy_commands,
    &domain::cadastre::register_cadastre_commands,
    &processing::register_processing_commands,
    &domain::surface::register_surface_commands,
    &ai::register_ai_commands,
};

/// The order the generators register in (docgen, envanter, kapsam).
constexpr Register kGeneratorOrder[] = {
    &register_builtin_commands,
    &processing::register_processing_commands,
    &domain::geodesy::register_geodesy_commands,
    &domain::cadastre::register_cadastre_commands,
    &domain::surface::register_surface_commands,
    &ai::register_ai_commands,
};

std::set<std::string> ids_of(const Registry& r)
{
    std::set<std::string> out;
    for (const CommandSpec& spec : r.all())
        out.insert(spec.id);
    return out;
}

std::string refusals(const Registry& r)
{
    std::string out;
    for (const std::string& line : r.refused())
        out += "\n  " + line;
    return out;
}

} // namespace

TEST_CASE("KAYIT: altı modülün komutları birlikte kurulur; hiçbiri bir ad çakışmasıyla düşmez")
{
    // What each module brings when it is alone on a registry.
    std::size_t alone = 0;
    for (const Register add : kApplicationOrder) {
        Registry one;
        add(one);
        INFO(refusals(one));
        CHECK(one.refused().empty());
        alone += one.size();
    }

    // All six together, as the program assembles them: nothing refused and
    // nothing lost — a collision would make the total smaller than the sum.
    Registry program;
    for (const Register add : kApplicationOrder)
        add(program);
    INFO("reddedilen:" << refusals(program));
    CHECK(program.refused().empty());
    CHECK(program.size() == alone);

    // The generators' order yields the same commands, so the reference and the
    // agent catalogue describe the program the user runs.
    Registry generated;
    for (const Register add : kGeneratorOrder)
        add(generated);
    CHECK(generated.refused().empty());
    CHECK(ids_of(generated) == ids_of(program));

    // And every declared name reaches its own command in the assembled registry.
    for (const CommandSpec& spec : program.all())
        for (const std::string& name : spec.names) {
            const CommandSpec* found = program.resolve(name);
            if (found == nullptr)
                FAIL_WITH("bildirilen ad çözülemiyor", spec.id + " / " + name);
            else if (found->id != spec.id)
                FAIL_WITH("ad başka bir komuta gidiyor",
                          spec.id + " / " + name + " -> " + found->id);
        }
}

TEST_CASE("KAYIT: reddedilen bir kayıt kaybolmaz; nedeniyle okunur")
{
    Registry r;
    register_builtin_commands(r);
    const std::size_t before = r.size();

    // A second command claiming `ÇİZGİ`'s name: refused, and kept on record
    // even when the caller drops the status, as four modules do.
    CommandSpec clash = *r.by_id("core.line");
    clash.id          = "test.cizgi_ikizi";
    (void)r.add(clash);
    CHECK(r.size() == before);
    REQUIRE(r.refused().size() == 1);
    CHECK(r.refused().front().rfind("test.cizgi_ikizi: ", 0) == 0);
    CHECK(r.refused().front().find("core.line") != std::string::npos);
}

TEST_CASE("KAYIT: son komut yalnız çizimde çalışan ve soru soran komut için döner")
{
    Registry program;
    for (const Register add : kApplicationOrder)
        add(program);
    REQUIRE(program.refused().empty());

    // THE METHOD COMES BACK, NOT THE PLACES (`rearm_line`), for a command that
    // asks and works in the drawing.
    CHECK(repeat_line(program, "DAİRE yontem=3n 0,0 10,0 0,10") == "DAİRE yontem=3n");
    CHECK(repeat_line(program, "L 0,0 100,0") == "ÇİZGİ");
    CHECK(repeat_line(program, "SİL") == "SİL");
    CHECK(repeat_line(program, "KAYDIR") == "KAYDIR");
    CHECK(repeat_line(program, "PARALEL mesafe=5000") == "OFSET mesafe=5000");

    // NEVER ONE THAT TOUCHES A FILE OR THE WORLD OUTSIDE: a stray Enter must not
    // save, open, import, paste from disk or run a script a second time.
    CHECK(repeat_line(program, "KAYDET").empty());
    CHECK(repeat_line(program, "İÇEAKTAR dosya=\"a.dxf\"").empty());
    CHECK(repeat_line(program, "BETİK \"a.json\"").empty());
    CHECK(repeat_line(program, "YAPIŞTIR").empty());
    CHECK(repeat_line(program, "MCPSUNUCU islem=baslat").empty());

    // NOR AN AGENT'S SUGGESTION: applying one is a deliberate act (5.7).
    CHECK(repeat_line(program, "ÖNERİ islem=uygula").empty());

    // A command that asks nothing ran whole the first time.
    CHECK(repeat_line(program, "YAKINLAŞ KAPSAM").empty());
    CHECK(repeat_line(program, "GERİAL").empty());
    CHECK(repeat_line(program, "yokboyle").empty());
}

TEST_CASE("ARAMA: cümle yazan kişi doğru komuta ulaşır (TODOS U-01, doğal dil)")
{
    Registry program;
    for (const Register add : kApplicationOrder)
        add(program);

    // THE ORDER THE PALETTE LISTS ITS ANSWERS IN: best tier first, ties in the order the
    // registry declared them (a stable sort, as `CommandPalette` does).
    const auto answers = [&program](const char* query) {
        std::vector<std::pair<int, const CommandSpec*>> hits;
        for (const CommandSpec& spec : program.all())
            if (const SearchMatch m = search_query(spec, query); m.tier != SearchMatch::kNone)
                hits.emplace_back(m.tier, &spec);
        std::stable_sort(hits.begin(), hits.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<std::string> ids;
        for (const auto& hit : hits)
            ids.push_back(hit.second->id);
        return ids;
    };
    const auto rank_of = [&answers](const char* query, const char* id) -> std::size_t {
        const std::vector<std::string> ids = answers(query);
        const auto at                      = std::ranges::find(ids, id);
        return at == ids.end() ? std::string::npos : static_cast<std::size_t>(at - ids.begin());
    };

    // The acceptance sentence of U-01: three spellings, one real tool, first in all three.
    for (const char* word : {"paralel", "ofset", "offset"})
        CHECK_MESSAGE(rank_of(word, "core.offset") == 0, word);

    // A sentence, with the endings Turkish puts on what is worked on and the verbs of
    // asking. (rank 0 is first.) `ifraz` and `alan` are in the domain modules, which is
    // why this runs on the assembled registry and not on the built-ins alone.
    struct Ask
    {
        const char* query;
        const char* id;
        std::size_t within; ///< the command is among the first `within` answers
    };

    for (const Ask& ask : {
             Ask{"çizgiyi paralel kaydır", "core.offset", 1},
             Ask{"bir çizgiyi paralel çizmek istiyorum", "core.offset", 1},
             Ask{"köşeyi yuvarla", "core.fillet", 1},
             Ask{"iki çizgiyi birleştir", "core.combine", 1},
             Ask{"metni değiştir", "core.edittext", 2},
             Ask{"nesneyi çoğalt", "core.copy", 2},
             Ask{"nesneyi çoğalt", "core.array", 2},
             Ask{"alanı hesapla", "core.measure_area", 2},
             Ask{"parseli ifraz et", "core.split_parcel", 1},
             Ask{"daire çiz", "core.circle_draw", 1},
             Ask{"offset a line", "core.offset", 3},
         })
        CHECK_MESSAGE(rank_of(ask.query, ask.id) < ask.within, ask.query << " -> " << ask.id);

    // A word still ranks as it always did: the name itself first, another program's word
    // under it, and nothing a sentence matcher does can put another command above them.
    CHECK(answers("kaydır").front() == "core.pan");
    CHECK(search_query(*program.by_id("core.pan"), "kaydır").tier == 0);
    CHECK(search_query(*program.by_id("core.rectangle"), "kutu").tier == 1);
    CHECK(search_query(*program.by_id("core.move"), "kaydır").tier == 1);

    // Another program's word found only through a sentence says whose word it was.
    const SearchMatch via = search_query(*program.by_id("core.move"), "nesneyi kaydır");
    CHECK(via.tier >= 7);
    REQUIRE(via.known != nullptr);
    CHECK(via.known->name == "KAYDIR");

    // A stem is not a prefix of anything (SİL is not what `silindir` asks for), a request of
    // nothing but particles asks for nothing, and a sentence no command answers is empty.
    {
        const std::vector<std::string> cylinder = answers("silindir");
        CHECK(std::ranges::find(cylinder, "core.erase") == cylinder.end());
    }
    CHECK(answers("ve için bir").empty());
    CHECK(answers("").empty());
    CHECK(answers("xqzwv yokboyle").empty());
    CHECK(answers("arazi kesiti çıkar").empty()); ///< no such tool yet: says so rather than guess
}

TEST_CASE("BİRİM: yardımı metre ya da milimetre diyen her uzunluk parametresi birimini bildirir (U-02)")
{
    Registry program;
    for (const Register add : kApplicationOrder)
        add(program);

    // A length is typed with its unit only where its parameter DECLARES one
    // (`Param::length_exponent`, command.md R29): `12.5 m` reaching a distance in millimetres as
    // 12500 depends on nothing but that declaration. The help text is where a person wrote the
    // unit down before there was a field for it, so the walk reads it: a parameter whose help
    // names metres or millimetres as its unit, and whose `unit` is empty, can never take
    // `1250 cm` and would say "bir uzunluk değil" to a person who typed a length.
    //
    // THE EXCEPTIONS ARE NAMED, each for a reason that is not a forgotten declaration: a
    // tolerance that is metres on a length and degrees on an angle, a value that is a ratio OR a
    // distance, and the areas and micrometres that merely contain the word.
    const std::set<std::string> kExempt{
        "core.dimension.tolerans",     "core.dimension.tolerans_ust", "core.dimension.tolerans_alt",
        "core.dimension_edit.tolerans", "core.dimension_edit.tolerans_ust",
        "core.dimension_edit.tolerans_alt",
        "core.point_along.deger", ///< a ratio or a distance, by the method
    };
    const auto says_length_unit = [](std::string help) {
        for (char& c : help)
            c = static_cast<char>(c >= 'A' && c <= 'Z' ? c | 0x20 : c);
        // Areas and micrometres contain the word without being a length in it.
        const bool areas = help.find("metrekare") != std::string::npos ||
                           help.find("mikrometre") != std::string::npos ||
                           help.find("mm²") != std::string::npos ||
                           help.find("m²") != std::string::npos;
        if (areas) return false;
        return help.find("milimetre") != std::string::npos || help.find(", metre") != std::string::npos ||
               help.find("(m)") != std::string::npos;
    };

    std::string missing;
    for (const CommandSpec& spec : program.all())
        for (const Param& p : spec.params) {
            if (p.kind != ParamKind::Number && p.kind != ParamKind::Integer) continue;
            if (!p.unit.empty() || kExempt.contains(spec.id + "." + p.name)) continue;
            if (says_length_unit(p.help)) missing += "\n  " + spec.id + "." + p.name + ": " + p.help;
        }
    INFO("birimi bildirilmemiş uzunluklar:" << missing);
    CHECK(missing.empty());

    // And what a declared unit means: metres and millimetres are lengths, a paper size is too, an
    // angle's unit or a missing one is not.
    CHECK(Param::number("a", Arity::optional()).measured_in("m").length_exponent() == 0);
    CHECK(Param::integer("a", Arity::optional()).measured_in("mm").length_exponent() == -3);
    CHECK(Param::integer("a", Arity::optional()).measured_in("kâğıt mm").length_exponent() == -3);
    CHECK_FALSE(Param::number("a", Arity::optional()).measured_in("derece").length_exponent());
    CHECK_FALSE(Param::number("a", Arity::optional()).length_exponent());
    CHECK_FALSE(Param::text("a", Arity::optional()).measured_in("m").length_exponent());
}
