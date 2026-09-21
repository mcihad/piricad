// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/catalog.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>

namespace kentos::ai {
namespace {

using core::Json;

/// The six read tools `.claude/ai.md` R11 names by hand, and the command id each
/// one is implemented by.
///
/// THE RULEBOOK SPELLS THESE NAMES OUT, so they are not derived from an id: R11
/// says `katmanlari_listele()`, and a client reading the rulebook must find that
/// tool under that name. Everything else takes its name from its id, which is
/// how the catalogue stays a projection rather than a list.
struct ReadToolName
{
    const char* command_id;
    const char* wire_name;
};

constexpr ReadToolName kReadToolNames[] = {
    {"core.layers", "katmanlari_listele"},
    {"core.attr_schema", "oznitelik_semasi"},
    {"core.query", "sorgula"},
    {"core.selection_info", "secimi_al"},
    {"core.view_info", "gorunum_bilgisi"},
    {"core.corpus_search", "mevzuat_ara"},
};

/// Turkish for what a parameter holds, for the description a model reads.
const char* kind_word(command::ParamKind kind)
{
    switch (kind) {
    case command::ParamKind::Point: return "nokta";
    case command::ParamKind::PointList: return "nokta listesi";
    case command::ParamKind::Number: return "sayı";
    case command::ParamKind::Integer: return "tam sayı";
    case command::ParamKind::Text: return "metin";
    case command::ParamKind::Bool: return "evet/hayır";
    case command::ParamKind::Selection: return "nesne seçimi";
    }
    return "değer";
}

/// A handle is a string, and the schema says so in a way a model can satisfy.
Json handle_schema(const char* what)
{
    Json out;
    out.set("type", Json::string("string"));
    out.set("pattern", Json::string("^@[0-9a-f]{16}(\\.[0-9]+)?$"));
    out.set("description",
            Json::string(std::string(what) +
                         " — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). "
                         "Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."));
    return out;
}

Json integer_schema()
{
    Json out;
    out.set("type", Json::string("integer"));
    return out;
}

/// `[easting, northing]`, in document millimetres, easting first — model.md R37a.
Json literal_point_schema()
{
    Json out;
    out.set("type", Json::string("array"));
    out.set("items", integer_schema());
    out.set("minItems", Json::integer(2));
    out.set("maxItems", Json::integer(2));
    out.set("description",
            Json::string("[Sağa (Y), Yukarı (X)] — tam sayı milimetre, doğuya doğru olan önce"));
    return out;
}

} // namespace

Json schema_for(const command::Param& param, Style style)
{
    const bool many  = param.arity.max > 1;
    const bool agent = style == Style::Agent;
    Json out;

    switch (param.kind) {
    case command::ParamKind::Point:
        out = agent ? handle_schema("nokta") : literal_point_schema();
        break;

    case command::ParamKind::PointList: {
        if (agent) {
            out = handle_schema("nokta listesi");
            break;
        }
        out.set("type", Json::string("array"));
        out.set("items", literal_point_schema());
        if (param.arity.min > 0) out.set("minItems", Json::integer(param.arity.min));
        if (param.arity.max != 0xFFFFFFFFu) out.set("maxItems", Json::integer(param.arity.max));
        break;
    }

    case command::ParamKind::Selection: {
        if (agent) {
            out = handle_schema("nesne seçimi");
            break;
        }
        // The persistent key, never the dense slot (model.md R5, P4).
        Json item;
        item.set("type", Json::string("integer"));
        item.set("minimum", Json::integer(1));
        out.set("type", Json::string("array"));
        out.set("items", std::move(item));
        out.set("description", Json::string("kalıcı nesne anahtarları (EntityKey), sıra değil"));
        break;
    }

    case command::ParamKind::Number:
        out.set("type", Json::string("number"));
        if (param.bounded) {
            out.set("minimum", Json::integer(param.low));
            out.set("maximum", Json::integer(param.high));
        }
        break;

    case command::ParamKind::Integer:
        // AN INTEGER PARAMETER WHOSE ARITY ALLOWS MORE THAN ONE IS A LIST, and
        // the bus already repairs `[4, 4]` into an id list for exactly these
        // parameters (bus.cpp). The schema mirrors that repair; a unit test pins
        // the two together, because a schema that disagreed with the bus would
        // send a model to write something the bus then rejects.
        if (many) {
            out.set("type", Json::string("array"));
            out.set("items", integer_schema());
            if (param.arity.min > 0) out.set("minItems", Json::integer(param.arity.min));
        } else {
            out.set("type", Json::string("integer"));
            if (param.bounded) {
                out.set("minimum", Json::integer(param.low));
                out.set("maximum", Json::integer(param.high));
            }
        }
        break;

    case command::ParamKind::Text: {
        // A TEXT PARAMETER THAT TAKES MORE THAN ONE IS A LIST OF WORDS, and the
        // schema has to say so or a client sends one string where several were
        // meant. The word list, when there is one, belongs to the ITEM.
        //
        // `many` is the function's own, declared at the top: this branch used to
        // re-declare it with the same expression, which shadowed it and said
        // nothing new.
        Json words = Json::array({});
        for (const std::string& word : param.choices)
            words.push(Json::string(word));

        if (many) {
            out.set("type", Json::string("array"));
            Json item;
            item.set("type", Json::string("string"));
            if (!param.choices.empty()) item.set("enum", std::move(words));
            out.set("items", std::move(item));
            if (param.arity.min > 0)
                out.set("minItems", Json::integer(static_cast<std::int64_t>(param.arity.min)));
            if (param.arity.max != 0xFFFFFFFFu)
                out.set("maxItems", Json::integer(static_cast<std::int64_t>(param.arity.max)));
        } else {
            out.set("type", Json::string("string"));
            if (!param.choices.empty()) out.set("enum", std::move(words));
        }
        break;
    }

    case command::ParamKind::Bool: out.set("type", Json::string("boolean")); break;
    }

    // The declared help is the description, with the kind named after it so a
    // model that ignores `type` still reads what the value is.
    std::string described = param.help;
    // THE UNIT FIRST, because it is the thing an agent gets wrong. A layout's `x`
    // is a position on paper and a map's window is a coordinate on the ground,
    // and a schema that says only "integer" leaves that to be guessed.
    if (!param.unit.empty()) {
        if (!described.empty()) described += " ";
        described += "[" + param.unit + "]";
    }
    if (const Json* had = out.find("description"); had != nullptr && had->is_string()) {
        if (!described.empty()) described += " — ";
        described += had->as_string();
    } else if (!described.empty()) {
        described += " (" + std::string(kind_word(param.kind)) + ")";
    }
    if (!described.empty()) out.set("description", Json::string(described));
    return out;
}

Json input_schema_for(const command::CommandSpec& spec, Style style)
{
    Json properties;
    Json required = Json::array({});

    for (const command::Param& p : spec.params) {
        properties.set(p.name, schema_for(p, style));
        if (p.arity.min > 0) required.push(Json::string(p.name));
    }
    if (spec.params.empty()) properties = Json::object({});

    Json out;
    out.set("type", Json::string("object"));
    out.set("properties", std::move(properties));
    out.set("required", std::move(required));
    // THE BUS REFUSES AN UNDECLARED ARGUMENT (command.md P15: a typo in a script
    // must not be silently ignored), so the schema says so rather than letting a
    // client believe an extra field would be tolerated.
    out.set("additionalProperties", Json::boolean(false));
    return out;
}

std::string tool_name_for(const command::CommandSpec& spec)
{
    for (const ReadToolName& named : kReadToolNames)
        if (spec.id == named.command_id) return named.wire_name;

    std::string out = spec.id;
    std::replace(out.begin(), out.end(), '.', '_');
    return out;
}

ToolDef tool_for(const command::CommandSpec& spec, Style style)
{
    ToolDef tool;
    tool.name       = tool_name_for(spec);
    tool.title      = spec.names.empty() ? spec.id : spec.names.front();
    tool.command_id = spec.id;
    tool.mutates    = !has_flag(spec.flags, command::Flags::NoEffect);

    // THE DESCRIPTION CARRIES WHAT A SCHEMA CANNOT: the Turkish name a user would
    // type, the aliases, and — the important part — whether calling this tool
    // does anything by itself. An agent that does not know its write will wait
    // for a person will read the silence as a failure and try again.
    std::string text = spec.summary;
    if (!spec.names.empty()) {
        text += "\nKomut: " + spec.names.front();
        if (spec.names.size() > 1) {
            text += " (";
            for (std::size_t i = 1; i < spec.names.size(); ++i)
                text += (i == 1 ? "" : ", ") + spec.names[i];
            text += ")";
        }
    }
    text += tool.mutates
                ? "\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut "
                  "satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar "
                  "bekler."
                : "\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.";
    tool.description = std::move(text);

    tool.input_schema = input_schema_for(spec, style);

    const bool no_effect = has_flag(spec.flags, command::Flags::NoEffect);
    tool.annotations     = ToolAnnotations{
            .read_only   = no_effect,
            .destructive = !no_effect,
            .idempotent  = no_effect,
        // Only a command that reaches the disk or the network touches anything
        // outside this program.
            .open_world = spec.category == command::Category::File,
    };

    Json meta;
    meta.set("cad.kentos/commandId", Json::string(spec.id));
    meta.set("cad.kentos/category", Json::string(command::category_name(spec.category)));
    meta.set("cad.kentos/approval", Json::string(tool.mutates ? "user-required" : "none"));
    Json aliases = Json::array({});
    for (const std::string& name : spec.names)
        aliases.push(Json::string(name));
    meta.set("cad.kentos/names", std::move(aliases));
    tool.meta = std::move(meta);

    return tool;
}

Catalog build_catalog(const command::Registry& registry, const CatalogOptions& options)
{
    Catalog out;
    out.fingerprint = registry.fingerprint();

    for (const command::CommandSpec& spec : registry.all()) {
        if (!has_flag(spec.flags, command::Flags::AiAccessible)) continue;
        ToolDef tool = tool_for(spec, options.style);
        if (tool.mutates && !options.include_mutating) continue;
        out.tools.push_back(std::move(tool));
    }

    // SORTED BY NAME, so two runs of the same build serve byte-identical bytes:
    // a client caches this list and a reordering would read as a change.
    std::sort(out.tools.begin(), out.tools.end(),
              [](const ToolDef& a, const ToolDef& b) { return a.name < b.name; });
    return out;
}

core::Json Catalog::to_tools_list() const
{
    Json tools_json = Json::array({});
    for (const ToolDef& tool : tools) {
        Json annotations;
        annotations.set("readOnlyHint", Json::boolean(tool.annotations.read_only));
        annotations.set("destructiveHint", Json::boolean(tool.annotations.destructive));
        annotations.set("idempotentHint", Json::boolean(tool.annotations.idempotent));
        annotations.set("openWorldHint", Json::boolean(tool.annotations.open_world));

        Json entry;
        entry.set("name", Json::string(tool.name));
        entry.set("title", Json::string(tool.title));
        entry.set("description", Json::string(tool.description));
        entry.set("inputSchema", tool.input_schema);
        entry.set("annotations", std::move(annotations));
        entry.set("_meta", tool.meta);
        tools_json.push(std::move(entry));
    }
    return tools_json;
}

const ToolDef* Catalog::find(std::string_view name) const
{
    for (const ToolDef& tool : tools)
        if (tool.name == name) return &tool;
    return nullptr;
}

std::size_t Catalog::mutating_count() const
{
    std::size_t n = 0;
    for (const ToolDef& tool : tools)
        if (tool.mutates) ++n;
    return n;
}

} // namespace kentos::ai
