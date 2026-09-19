// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/spec.hpp"

namespace kentos::command {

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
    case Category::Processing: return "İşlem";
    }
    return "?";
}

/// Stable machine name, used by the generated AI tool schema and the JSON
/// documents. Never shown to a user — see param_kind_label for that.
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

const char* param_kind_label(ParamKind k)
{
    switch (k) {
    case ParamKind::Point: return "nokta";
    case ParamKind::PointList: return "nokta listesi";
    case ParamKind::Number: return "sayı";
    case ParamKind::Integer: return "tam sayı";
    case ParamKind::Text: return "metin";
    case ParamKind::Bool: return "evet/hayır";
    case ParamKind::Selection: return "nesne seçimi";
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

Param Param::choice(std::string name, Arity a, std::vector<std::string> choices, std::string help)
{
    Param p{std::move(name), ParamKind::Text, a, std::move(help)};
    p.choices = std::move(choices);
    return p;
}

Param Param::integer_range(std::string name, Arity a, std::int64_t low, std::int64_t high,
                           std::string help)
{
    Param p{std::move(name), ParamKind::Integer, a, std::move(help)};
    p.low     = low;
    p.high    = high;
    p.bounded = true;
    return p;
}

} // namespace kentos::command
