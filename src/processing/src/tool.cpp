// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/processing/tool.hpp"

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/core/document.hpp"

#include <utility>

namespace kentos::processing {

namespace {

/// The four parameters every tool's command starts with. Declared once here so
/// the panel, the docs and the CLI agree about their names and their order.
std::vector<command::Param> shared_params()
{
    using command::Arity;
    using command::Param;
    using command::ParamKind;
    return {
        Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
              "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz"},
        Param::text("kapsam", Arity{0, 1},
                    "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır"),
        Param::points("pencere", Arity{0, 2},
                      "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir"),
        Param::text("katman", Arity{0, 1},
                    "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman"),
    };
}

} // namespace

const char* applies_name(Applies one) noexcept
{
    switch (one) {
    case Applies::Points: return "nokta";
    case Applies::Lines: return "çizgi";
    case Applies::Faces: return "alan";
    case Applies::Curves: return "eğri";
    case Applies::Texts: return "yazı";
    case Applies::None: break;
    }
    return "";
}

ToolParam ToolParam::text(std::string name, std::string help, std::string fallback)
{
    ToolParam p;
    p.name     = std::move(name);
    p.kind     = command::ParamKind::Text;
    p.help     = std::move(help);
    p.fallback = std::move(fallback);
    return p;
}

ToolParam ToolParam::choice(std::string name, std::string help, std::vector<std::string> choices,
                            std::string fallback)
{
    ToolParam p;
    p.name     = std::move(name);
    p.kind     = command::ParamKind::Text;
    p.help     = std::move(help);
    p.choices  = std::move(choices);
    p.fallback = std::move(fallback);
    return p;
}

ToolParam ToolParam::integer(std::string name, std::string help, std::int64_t fallback,
                             std::int64_t low, std::int64_t high)
{
    ToolParam p;
    p.name     = std::move(name);
    p.kind     = command::ParamKind::Integer;
    p.help     = std::move(help);
    p.fallback = std::to_string(fallback);
    p.low      = low;
    p.high     = high;
    p.bounded  = low != 0 || high != 0;
    return p;
}

ToolParam ToolParam::integer_optional(std::string name, std::string help, std::int64_t low,
                                      std::int64_t high)
{
    ToolParam p;
    p.name    = std::move(name);
    p.kind    = command::ParamKind::Integer;
    p.help    = std::move(help);
    p.low     = low;
    p.high    = high;
    p.bounded = low != 0 || high != 0;
    return p;
}

ToolParam ToolParam::number(std::string name, std::string help, std::string fallback)
{
    ToolParam p;
    p.name     = std::move(name);
    p.kind     = command::ParamKind::Number;
    p.help     = std::move(help);
    p.fallback = std::move(fallback);
    return p;
}

command::Task<core::Status> ProcessingTool::interact(command::Context& /*ctx*/,
                                                     ToolInput& /*input*/) const
{
    co_return core::ok();
}

ToolParam ToolParam::point(std::string name, std::string help)
{
    ToolParam p;
    p.name = std::move(name);
    p.kind = command::ParamKind::Point;
    p.help = std::move(help);
    return p;
}

ToolParam ToolParam::boolean(std::string name, std::string help, bool fallback)
{
    ToolParam p;
    p.name     = std::move(name);
    p.kind     = command::ParamKind::Bool;
    p.help     = std::move(help);
    p.fallback = fallback ? "evet" : "hayır";
    return p;
}

command::CommandSpec ToolSpec::to_command_spec() const
{
    using command::Arity;
    using command::Param;
    using command::ParamKind;

    command::CommandSpec spec;
    spec.id    = id;
    spec.names = names;
    // A tool that changes objects in place is a MODIFY command to the menus and
    // the reference; one that makes new objects is an analysis.
    spec.category =
        output == OutputShape::InPlace ? command::Category::Modify : command::Category::Processing;
    spec.params = shared_params();
    for (const ToolParam& p : params) {
        // Every tool parameter is optional at the bus: the tool applies its
        // declared default. A choice is validated by the tool, which is the
        // one place that knows the words.
        std::string help = p.help;
        if (!p.choices.empty()) {
            help += " (";
            for (std::size_t i = 0; i < p.choices.size(); ++i)
                help += (i == 0 ? "" : " / ") + p.choices[i];
            help += ")";
        }
        if (!p.fallback.empty()) help += "; varsayılan " + p.fallback;
        switch (p.kind) {
        case ParamKind::Integer:
            spec.params.push_back(Param::integer(p.name, Arity{0, 1}, help));
            break;
        case ParamKind::Number:
            spec.params.push_back(Param::number(p.name, Arity{0, 1}, help));
            break;
        case ParamKind::Bool:
            spec.params.push_back(Param::boolean(p.name, Arity{0, 1}, help));
            break;
        case ParamKind::Point:
            spec.params.push_back(Param{p.name, ParamKind::Point, Arity{0, 1}, help});
            break;
        case ParamKind::PointList:
            spec.params.push_back(Param::points(p.name, Arity{0, 0xFFFFFFFFu}, help));
            break;
        case ParamKind::Text:
        case ParamKind::Selection:
            spec.params.push_back(Param::text(p.name, Arity{0, 1}, help));
            break;
        }
    }
    spec.undo  = output == OutputShape::Report ? command::UndoPolicy::None
                                               : command::UndoPolicy::SingleTransaction;
    spec.flags = command::Flags::Interactive | command::Flags::Scriptable |
                 command::Flags::AiAccessible |
                 (output == OutputShape::Report ? command::Flags::ReadOnly : command::Flags::None);
    spec.summary = summary;
    spec.run     = nullptr; // filled by the registry with the one generic body
    return spec;
}

void Progress::at(std::size_t done, std::size_t total) const noexcept
{
    if (permille == nullptr) return;
    if (total == 0) {
        permille->store(1000);
        return;
    }
    if (done > total) done = total;
    permille->store(static_cast<std::uint32_t>((done * 1000) / total));
}

Applies classify(const core::Document& doc, core::EntityId e)
{
    const core::EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e)) return Applies::None;
    switch (ents.kind[e]) {
    case core::kPointKind: return Applies::Points;
    case core::kPolylineKind: {
        if (doc.texts().has(ents.slot[e])) return Applies::Texts;
        const core::RingSpan rs = doc.geometry().rings_of(ents.slot[e]);
        if (rs.count == 0) return Applies::None;
        return doc.geometry().ring_role[rs.first] == core::RingRole::Open ? Applies::Lines
                                                                          : Applies::Faces;
    }
    case core::kHatchKind: return Applies::Faces;
    case core::kCircleKind:
    case core::kArcKind:
    case core::kEllipseKind:
    case core::kSplineKind:
    case core::kArcPolylineKind: return Applies::Curves;
    default: return Applies::None;
    }
}

core::Error cancelled()
{
    return core::err(core::ErrorCode::Cancelled, "İşlem durduruldu; çizim değişmedi.");
}

} // namespace kentos::processing
