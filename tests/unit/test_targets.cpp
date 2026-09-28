// SPDX-License-Identifier: GPL-3.0-or-later
//
// What a command acts on (`CommandSpec::targets`, command/targets.hpp): every
// object sorts into the class a user sees it as — a line, an area, a curve, a
// caption — whatever kind id holds it, and a selection is offered a tool only
// when every object in it is one the tool takes. The ribbon greys by this
// (`.claude/ui.md` R54); the reference says it; the class a parcel with an arc
// edge is in must not change because its kind did (model.md R9b).
#include "kentos_test.hpp"

#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/targets.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <cstdint>
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

    Rig()
    {
        register_builtin_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        processing::register_processing_commands(reg);
    }

    void run(const char* line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }

    core::EntityId slot(std::uint64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(key));
    }

    Targets of(std::uint64_t key) const { return target_of(doc, slot(key)); }

    Held held(std::initializer_list<std::uint64_t> keys) const
    {
        std::vector<core::EntityKey> ks;
        for (const std::uint64_t k : keys)
            ks.push_back(static_cast<core::EntityKey>(k));
        return held_by(doc, ks);
    }

    Targets declared(const char* word) const
    {
        const CommandSpec* spec = reg.resolve(word);
        REQUIRE(spec != nullptr);
        return spec->targets;
    }
};

} // namespace

TEST_CASE("HEDEF: her nesne kullanıcının gördüğü sınıfta — çizgi, alan, eğri, yazı, nokta")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");              // 1
    r.run("ALAN 0,0 20,0 20,10 0,10");    // 2
    r.run("DAİRE 50,50 60,50");           // 3
    r.run("METİN 0,30 \"1234/7\" 2000");  // 4
    r.run("NOKTA 5,5");                   // 5
    r.run("ALAN 40,0 60,0 60,10 40,10");  // 6
    r.run("ÇOKLUÇİZGİ 0,40 10,40 10,50"); // 7

    CHECK(r.of(1) == Targets::Lines);
    CHECK(r.of(2) == Targets::Faces);
    CHECK(r.of(3) == Targets::Curves);
    CHECK(r.of(4) == Targets::Texts);
    CHECK(r.of(5) == Targets::Points);
    CHECK(r.of(7) == Targets::Lines);

    // A PARCEL WITH AN ARC EDGE IS STILL AN AREA: its kind changed in place
    // (model.md R9b), the class a user sees did not.
    r.run("KENARTÜRÜ nesne=6 kenar=1 tur=yay nokta=50,-3");
    CHECK(r.doc.entities().kind[r.slot(6)] == core::kArcPolylineKind);
    CHECK(r.of(6) == Targets::Faces);

    // A DEAD OBJECT IS OF NO CLASS, and a selection does not count it.
    r.run("SİL nesneler=5");
    CHECK(r.of(5) == Targets::None);
    CHECK_EQ(r.held({1, 5}).count, 1u);
}

TEST_CASE("HEDEF: araç yalnız seçimin tamamı kendi nesnesiyse sunuluyor")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");             // 1, a line
    r.run("ALAN 0,0 20,0 20,10 0,10");   // 2, an area
    r.run("METİN 0,30 \"1234/7\" 2000"); // 3, a caption

    const Targets fillet = r.declared("YUVARLA");
    const Targets ifraz  = r.declared("İFRAZ");
    CHECK(acts_on_all(fillet, r.held({1})));
    CHECK(acts_on_all(fillet, r.held({1, 2})));
    // One caption in it is enough: YUVARLA refuses a run with a caption in it.
    CHECK_FALSE(acts_on_all(fillet, r.held({1, 3})));
    CHECK_FALSE(acts_on_all(fillet, r.held({3})));
    // İFRAZ takes an area and nothing else.
    CHECK(acts_on_all(ifraz, r.held({2})));
    CHECK_FALSE(acts_on_all(ifraz, r.held({1})));
    // Every tool is offered with nothing selected — it asks for its objects —
    // and a command declared for any object takes any selection.
    CHECK(acts_on_all(ifraz, r.held({})));
    CHECK(acts_on_all(r.declared("TAŞI"), r.held({1, 2, 3})));
}

TEST_CASE("HEDEF: Araçlar işlemleri sınıflarını komut olarak da bildiriyor")
{
    Rig r;
    // KÖŞENUMARALA numbers the corners of an area; a line has corners too.
    const Targets corners = r.declared("islem.kose_numarala");
    CHECK(has_target(corners, Targets::Faces));
    CHECK_FALSE(has_target(corners, Targets::Texts));
    // What the reference prints for it, in the manual's words.
    CHECK(target_names(Targets::Lines | Targets::Faces) == "çizgi, alan");
    CHECK(target_names(Targets::Leaders) == "kılavuz çizgi");
}

TEST_CASE("HEDEF: sınıf bildiren her komut bir nesne parametresi taşıyor")
{
    // A tool is greyed for a selection only if it acts on one: a command that
    // declared targets but took no objects would be greyed for nothing.
    Rig r;
    for (const CommandSpec& spec : r.reg.all()) {
        if (spec.targets == Targets::Any) continue;
        bool takes = false;
        for (const Param& p : spec.params)
            takes = takes || p.kind == ParamKind::Selection;
        if (!takes) FAIL_WITH(spec.id, "hedef bildiriyor ama nesne almıyor");
        CHECK(takes);
    }
}

TEST_CASE("HEDEF: BİRLEŞTİR yaylı kenarlı alanı kirişe çevirmek yerine reddediyor")
{
    // The one gap R28 names (TODOS O-3): an area with an arc edge is an area,
    // and the combine command's union over vertex rings would hand the arc back
    // as a chord. It refuses, in words, rather than doing that silently.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("ALAN 20,0 40,0 40,10 20,10");
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3");
    auto refused = r.bus.execute_line("BİRLEŞTİR nesneler=1 nesneler=2", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("yaylı kenarlı") != std::string::npos);
    CHECK_EQ(r.doc.live_entity_count(), 2u);
}

TEST_CASE("HEDEF: yöntemin daralttığı sınıflar — BÖL alanı yalnız kesme çizgisiyle alıyor")
{
    Rig r;
    const CommandSpec* split = r.reg.resolve("BÖL");
    REQUIRE(split != nullptr);
    Args plain;
    CHECK(targets_of(*split, plain) == (Targets::Lines | Targets::Faces | Targets::Curves));
    Args cut;
    cut.set("yontem", Value::text("cizgi"));
    CHECK(has_target(targets_of(*split, cut), Targets::Faces));
    for (const char* word : {"nokta", "kesisim", "esit", "mesafe"}) {
        Args along;
        along.set("yontem", Value::text(word));
        CHECK_FALSE_MESSAGE(has_target(targets_of(*split, along), Targets::Faces), word);
        CHECK(has_target(targets_of(*split, along), Targets::Lines));
    }
    // The word is matched folded: `EŞİT` is `esit`.
    Args loud;
    loud.set("yontem", Value::text("EŞİT"));
    CHECK_FALSE(has_target(targets_of(*split, loud), Targets::Faces));

    // ONE SENTENCE SAYS IT, the four methods that walk an edge named together,
    // and the reference and the agent's tool description both carry it.
    const std::string takes = targets_sentence(*split);
    CHECK_EQ(takes, std::string("çizgi, alan, eğri; `yontem=nokta`, `yontem=kesisim`, "
                                "`yontem=esit` ya da `yontem=mesafe` ile çizgi, eğri"));
    CHECK(ai::tool_for(*split, ai::Style::Agent).description.find(takes) != std::string::npos);
    const CommandSpec* move = r.reg.resolve("TAŞI");
    REQUIRE(move != nullptr);
    CHECK(targets_sentence(*move).empty());
}
