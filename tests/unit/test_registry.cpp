// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: the command registry as the program assembles it.
//
// Six modules register commands — the built-ins, geodesy, cadastre, surface, the
// processing tools and the AI layer's own — and four places put them on one
// registry: the application's controller, and the three generators (docgen,
// envanter, kapsam), in two different orders. A name two modules claim makes the
// later `Registry::add` fail, and four of the six registration functions drop
// that status: the command was simply not in the program, the build was clean
// and every test that registered one module at a time was green.
#include "kentos_test.hpp"

#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <set>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

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
