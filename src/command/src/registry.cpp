// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/registry.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::command {

core::Status Registry::add(CommandSpec spec)
{
    using core::ErrorCode;

    if (spec.id.empty()) return core::err(ErrorCode::InvalidArgument, "Komut kimliği boş olamaz.");
    if (spec.names.empty())
        return core::err(ErrorCode::InvalidArgument,
                         "'" + spec.id + "' komutu hiç ad tanımlamıyor.");
    if (!spec.run)
        return core::err(ErrorCode::InvalidArgument,
                         "'" + spec.id + "' komutunun çalıştırma işlevi yok.");
    if (by_id_.contains(spec.id))
        return core::err(ErrorCode::InvalidArgument, "Yinelenen komut kimliği: '" + spec.id + "'");

    std::vector<std::string> folded;
    folded.reserve(spec.names.size());
    for (const auto& n : spec.names) {
        std::string f = core::turkish_upper(n);
        if (auto it = by_name_.find(f); it != by_name_.end()) {
            return core::err(ErrorCode::InvalidArgument, "'" + n + "' komut adı zaten '" +
                                                             specs_[it->second].id +
                                                             "' komutuna ait.");
        }
        folded.push_back(std::move(f));
    }

    const std::size_t index = specs_.size();
    by_id_.emplace(spec.id, index);
    // The id itself always resolves, so scripts and the AI can use it directly.
    by_name_.emplace(core::turkish_upper(spec.id), index);
    for (auto& f : folded)
        by_name_.emplace(std::move(f), index);

    specs_.push_back(std::move(spec));
    return core::ok();
}

const CommandSpec* Registry::by_id(std::string_view id) const
{
    auto it = by_id_.find(std::string(id));
    return it == by_id_.end() ? nullptr : &specs_[it->second];
}

const CommandSpec* Registry::resolve(std::string_view typed) const
{
    if (typed.empty()) return nullptr;
    auto it = by_name_.find(core::turkish_upper(typed));
    return it == by_name_.end() ? nullptr : &specs_[it->second];
}

std::vector<std::string> Registry::complete(std::string_view prefix, std::size_t limit) const
{
    const std::string folded = core::turkish_upper(prefix);
    std::vector<std::string> out;

    for (const auto& spec : specs_) {
        for (const auto& n : spec.names) {
            if (core::turkish_upper(n).starts_with(folded)) {
                out.push_back(n);
                break; // one suggestion per command; aliases would drown the list
            }
        }
    }

    std::sort(out.begin(), out.end());
    if (out.size() > limit) out.resize(limit);
    return out;
}

core::Json Registry::ai_tool_schema() const
{
    core::Json tools = core::Json::array({});
    for (const auto& spec : specs_) {
        if (!has_flag(spec.flags, Flags::AiAccessible)) continue;
        tools.push(spec.to_schema());
    }

    core::Json out;
    out.set("version", core::Json::integer(1));
    out.set("generated_from", core::Json::string("piricad::command::Registry"));
    out.set("tools", std::move(tools));
    return out;
}

std::string Registry::markdown_reference() const
{
    std::string out = "# PiriCAD Command Reference\n\n";
    out += "> Generated from the command registry. Do not edit by hand.\n\n";
    out += "| Id | Names | Category | Undo | Flags | Summary |\n";
    out += "|---|---|---|---|---|---|\n";

    for (const auto& spec : specs_) {
        out += "| `" + spec.id + "` | ";
        for (std::size_t i = 0; i < spec.names.size(); ++i) {
            if (i) out += ", ";
            out += "`" + spec.names[i] + "`";
        }
        out += " | ";
        out += category_name(spec.category);
        out += " | ";
        out += spec.undo == UndoPolicy::SingleTransaction ? "tek işlem"
               : spec.undo == UndoPolicy::None            ? "yok"
                                                          : "özel";
        out += " | ";
        if (has_flag(spec.flags, Flags::Interactive)) out += "E";
        if (has_flag(spec.flags, Flags::Scriptable)) out += "S";
        if (has_flag(spec.flags, Flags::AiAccessible)) out += "A";
        if (has_flag(spec.flags, Flags::Transparent)) out += "T";
        if (has_flag(spec.flags, Flags::ReadOnly)) out += "R";
        out += " | " + spec.summary + " |\n";
    }
    return out;
}

Registry& registry()
{
    static Registry r;
    return r;
}

} // namespace piricad::command
