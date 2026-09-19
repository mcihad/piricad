// SPDX-License-Identifier: GPL-3.0-or-later
//
// The agent's whole view of this program is a projection of the command
// registry, and these are the cases that hold the projection honest: that it is
// derived and not written, that its JSON Schema says what the bus will actually
// accept, that a coordinate cannot be expressed as a number in it, and that two
// runs serve the same bytes.
//
// Qt-free by construction — `ai::build_catalog` is a pure function over a
// registry (.claude/test.md: a protocol is proved by a function, not a socket).
#include "kentos_test.hpp"

#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/llmstxt.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/processing/registry.hpp"

using namespace kentos;
using namespace kentos::command;

namespace {

struct Rig
{
    Registry reg;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
    }
};

/// The schema of one parameter of one command, by name.
const core::Json* property_of(const ai::ToolDef& tool, const char* name)
{
    const core::Json* properties = tool.input_schema.find("properties");
    if (properties == nullptr) return nullptr;
    return properties->find(name);
}

std::string type_of(const core::Json* schema)
{
    if (schema == nullptr) return "(yok)";
    const core::Json* type = schema->find("type");
    return type != nullptr && type->is_string() ? type->as_string() : "(tür yok)";
}

} // namespace

TEST_CASE("AI kataloğu yalnız AiAccessible komutlardan üretilir")
{
    Rig f;
    const ai::Catalog catalog = ai::build_catalog(f.reg);

    std::size_t flagged = 0;
    for (const CommandSpec& spec : f.reg.all())
        if (has_flag(spec.flags, Flags::AiAccessible)) ++flagged;

    CHECK(flagged > 0);
    CHECK_EQ(catalog.tools.size(), flagged);
    CHECK_EQ(catalog.fingerprint, f.reg.fingerprint());

    // A command that is NOT exposed is genuinely absent. `core.select` is the
    // case the comment at pick.cpp:445 argues for: a changed highlight changes
    // what the next SİL removes.
    CHECK(catalog.find("core_select") == nullptr);
}

TEST_CASE("AI kataloğu iki çalıştırmada aynı baytları verir")
{
    Rig a;
    Rig b;
    CHECK_EQ(ai::build_catalog(a.reg).to_tools_list().dump(),
             ai::build_catalog(b.reg).to_tools_list().dump());

    // Sorted by wire name, so a registration reorder cannot change the bytes.
    const ai::Catalog catalog = ai::build_catalog(a.reg);
    for (std::size_t i = 1; i < catalog.tools.size(); ++i)
        CHECK(catalog.tools[i - 1].name < catalog.tools[i].name);
}

TEST_CASE("Araç adı komut kimliğinden gelir; R11 okuma araçları adlarını korur")
{
    Rig f;
    const CommandSpec* line = f.reg.by_id("core.line");
    REQUIRE(line != nullptr);
    CHECK_EQ(ai::tool_name_for(*line), std::string("core_line"));

    // The names .claude/ai.md R11 spells out by hand. They are not derived: a
    // client that read the rulebook must find the tool under the name the
    // rulebook prints.
    CommandSpec pretend;
    pretend.id = "core.view_info";
    CHECK_EQ(ai::tool_name_for(pretend), std::string("gorunum_bilgisi"));
    pretend.id = "core.layers";
    CHECK_EQ(ai::tool_name_for(pretend), std::string("katmanlari_listele"));
    pretend.id = "core.query";
    CHECK_EQ(ai::tool_name_for(pretend), std::string("sorgula"));

    // Every wire name is header-safe: the MCP transport mirrors it into
    // `Mcp-Name`, and a name outside visible ASCII would have to be Base64'd.
    for (const ai::ToolDef& tool : ai::build_catalog(f.reg).tools)
        for (const char c : tool.name)
            CHECK((c == '_' || c == '-' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));
}

TEST_CASE("Ajan şemasında koordinat YAZILAMAZ; insan şemasında yazılır")
{
    Rig f;
    const CommandSpec* line = f.reg.by_id("core.line");
    REQUIRE(line != nullptr);

    // CLAUDE.md 5.8 and ai.md R9/R10/P2 as a TYPE rather than a check: where a
    // point is declared, the agent-facing schema accepts a handle string and
    // nothing else, so a model has no way to express a number it invented.
    const ai::ToolDef agent = ai::tool_for(*line, ai::Style::Agent);
    const core::Json* pts   = property_of(agent, "noktalar");
    REQUIRE(pts != nullptr);
    CHECK_EQ(type_of(pts), std::string("string"));
    CHECK(pts->find("pattern") != nullptr);

    // The same command, described for a person: two integers in millimetres,
    // easting first (model.md R37a). The command line and the reference are read
    // by somebody whose numbers came off an instrument.
    const ai::ToolDef human     = ai::tool_for(*line, ai::Style::Human);
    const core::Json* human_pts = property_of(human, "noktalar");
    REQUIRE(human_pts != nullptr);
    CHECK_EQ(type_of(human_pts), std::string("array"));
}

TEST_CASE("Şema, veri yolunun kabul ettiğini söyler")
{
    Rig f;

    // A declared word list becomes an `enum`, and the bus now enforces the same
    // list (validation.cpp). A schema that offered a word the bus refuses would
    // send a model to write something that cannot work.
    const CommandSpec* label = f.reg.by_id("islem.uzunluk_yaz");
    REQUIRE(label != nullptr);
    const ai::ToolDef tool = ai::tool_for(*label, ai::Style::Agent);
    const core::Json* unit = property_of(tool, "birim");
    REQUIRE(unit != nullptr);
    const core::Json* words = unit->find("enum");
    REQUIRE(words != nullptr);
    REQUIRE(words->is_array());
    CHECK(words->as_array().size() >= 4);

    // An unknown argument is refused by the bus (command.md P15), so the schema
    // closes the object rather than implying an extra field is tolerated.
    const core::Json* extra = tool.input_schema.find("additionalProperties");
    REQUIRE(extra != nullptr);
    CHECK_FALSE(extra->as_bool(true));

    // An Integer parameter whose arity allows several IS a list, and the bus
    // repairs `[4, 4]` into an id list for exactly those. The schema mirrors it.
    const CommandSpec* area = f.reg.by_id("core.area");
    REQUIRE(area != nullptr);
    // The tool is held in a NAMED local: `property_of` hands back a pointer into
    // it, and reading that pointer after a temporary died is how this test first
    // reported a wrong type for a schema that was perfectly correct.
    const ai::ToolDef area_tool = ai::tool_for(*area, ai::Style::Agent);
    for (const Param& p : area->params) {
        if (p.kind != ParamKind::Integer) continue;
        const core::Json* schema = property_of(area_tool, p.name.c_str());
        REQUIRE(schema != nullptr);
        CHECK_EQ(type_of(schema), std::string(p.arity.max > 1 ? "array" : "integer"));
    }
}

TEST_CASE("Her araç dört annotation taşır ve onay kuralını söyler")
{
    Rig f;
    for (const ai::ToolDef& tool : ai::build_catalog(f.reg).tools) {
        // The MCP specification DEFAULTS destructiveHint and openWorldHint to
        // true, so an unannotated read tool is advertised as destructive. Every
        // tool therefore states all four.
        const core::Json list   = ai::Catalog{0, {tool}}.to_tools_list();
        const core::Json& entry = list.as_array().front();
        const core::Json* notes = entry.find("annotations");
        REQUIRE(notes != nullptr);
        for (const char* key :
             {"readOnlyHint", "destructiveHint", "idempotentHint", "openWorldHint"})
            CHECK(notes->find(key) != nullptr);

        // And the description says whether calling it does anything, because an
        // agent that does not know its write will wait for a person reads the
        // silence as a failure and tries again.
        CHECK(tool.description.find(tool.mutates ? "UYGULAMAZ" : "değiştirmez") !=
              std::string::npos);

        const core::Json* meta = entry.find("_meta");
        REQUIRE(meta != nullptr);
        const core::Json* approval = meta->find("cad.kentos/approval");
        REQUIRE(approval != nullptr);
        CHECK_EQ(approval->as_string(), std::string(tool.mutates ? "user-required" : "none"));
    }
}

TEST_CASE("llms.txt şemanın anlatamadığını anlatır ve üretilmiş olduğunu söyler")
{
    Rig f;
    const std::string index = ai::llms_txt(f.reg);

    CHECK(index.find(ai::kGeneratedNotice) == 0);
    for (const char* must :
         {"milimetre", "Sağa (Y)", "Yukarı (X)", "tutamak", "uygulamaz", "2026-07-28", "Ctrl+Z"})
        CHECK(index.find(must) != std::string::npos);

    // The fingerprint travels with it, so a stale copy is detectable.
    CHECK(index.find(std::to_string(f.reg.fingerprint())) != std::string::npos);

    // The long form carries every tool; the index does not.
    const std::string full = ai::llms_full_txt(f.reg);
    CHECK(full.size() > index.size() * 4);
    CHECK(full.find("core_line") != std::string::npos);
    CHECK(index.find("core_line") == std::string::npos);
}

TEST_CASE("Katalog: kâğıt ölçüsü serbest, zemin koordinatı tutamak ister")
{
    // TODOS A-03's whole point. A layout's `x` is a position on PAPER, which a
    // model may work out from a page size; a map's `pencere` is a coordinate on
    // the GROUND, which it may not invent at all (CLAUDE.md 5.8).
    //
    // The structural half was always there — a `Point` parameter accepts only a
    // handle — and what was missing is that the schema never SAID which was
    // which, so an agent had to guess.
    Registry reg;
    register_builtin_commands(reg);

    const CommandSpec* item = reg.by_id("core.layout_item");
    REQUIRE(item != nullptr);
    CHECK(has_flag(item->flags, Flags::AiAccessible));

    const auto described = [&](const char* name) {
        for (const Param& p : item->params)
            if (p.name == name) return p.unit;
        return std::string("<yok>");
    };

    // PAPER, and it says so.
    CHECK(described("x").find("kâğıt") != std::string::npos);
    CHECK(described("y").find("kâğıt") != std::string::npos);
    CHECK(described("genislik").find("kâğıt") != std::string::npos);

    // GROUND, and it says so loudly.
    CHECK(described("pencere").find("ZEMİN") != std::string::npos);

    // And the kinds still keep them apart structurally: an integer is a number a
    // caller may write, a point is one it may only hand over from a tool result.
    for (const Param& p : item->params) {
        if (p.name == "x" || p.name == "y") CHECK_EQ(p.kind, ParamKind::Integer);
        if (p.name == "pencere") CHECK_EQ(p.kind, ParamKind::PointList);
    }
}
