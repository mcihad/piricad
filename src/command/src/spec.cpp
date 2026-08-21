// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/spec.hpp"

namespace piricad::command {

const char* category_name(Category c)
{
    switch (c) {
    case Category::Draw: return "Çizim";
    case Category::Modify: return "Düzenleme";
    case Category::View: return "Görünüm";
    case Category::Layer: return "Katman";
    case Category::File: return "Dosya";
    case Category::Query: return "Sorgu";
    case Category::Script: return "Betik";
    case Category::System: return "Sistem";
    }
    return "?";
}

const char* param_kind_name(ParamKind k)
{
    switch (k) {
    case ParamKind::Point: return "point";
    case ParamKind::PointList: return "point_list";
    case ParamKind::Number: return "number";
    case ParamKind::Integer: return "integer";
    case ParamKind::Text: return "text";
    case ParamKind::Bool: return "bool";
    case ParamKind::Selection: return "selection";
    }
    return "?";
}

Param Param::points(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::PointList, a, std::move(help)};
}

Param Param::point(std::string name, std::string help)
{
    return Param{std::move(name), ParamKind::Point, Arity::exactly(1), std::move(help)};
}

Param Param::number(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::Number, a, std::move(help)};
}

Param Param::integer(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::Integer, a, std::move(help)};
}

Param Param::text(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::Text, a, std::move(help)};
}

Param Param::boolean(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::Bool, a, std::move(help)};
}

core::Json CommandSpec::to_schema() const
{
    using core::Json;

    Json out;
    out.set("id", Json::string(id));

    Json names_json;
    for (const auto& n : names)
        names_json.push(Json::string(n));
    if (names.empty()) names_json = Json::array({});
    out.set("names", std::move(names_json));

    out.set("category", Json::string(category_name(category)));
    out.set("summary", Json::string(summary));

    Json params_json = Json::array({});
    for (const auto& p : params) {
        Json pj;
        pj.set("name", Json::string(p.name));
        pj.set("type", Json::string(param_kind_name(p.kind)));
        pj.set("min", Json::integer(p.arity.min));
        pj.set("max", Json::integer(p.arity.max == 0xFFFFFFFFu ? -1 : std::int64_t(p.arity.max)));
        pj.set("required", Json::boolean(p.arity.min > 0));
        pj.set("help", Json::string(p.help));
        params_json.push(std::move(pj));
    }
    out.set("params", std::move(params_json));

    Json flags_json = Json::array({});
    if (has_flag(flags, Flags::Interactive)) flags_json.push(Json::string("interactive"));
    if (has_flag(flags, Flags::Scriptable)) flags_json.push(Json::string("scriptable"));
    if (has_flag(flags, Flags::AiAccessible)) flags_json.push(Json::string("ai_accessible"));
    if (has_flag(flags, Flags::Transparent)) flags_json.push(Json::string("transparent"));
    if (has_flag(flags, Flags::ReadOnly)) flags_json.push(Json::string("read_only"));
    out.set("flags", std::move(flags_json));

    const char* undo_name = undo == UndoPolicy::SingleTransaction ? "single_transaction"
                            : undo == UndoPolicy::None            ? "none"
                                                                  : "custom";
    out.set("undo", Json::string(undo_name));
    return out;
}

} // namespace piricad::command
