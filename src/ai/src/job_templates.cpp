// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/job_templates.hpp"

#include <utility>

namespace kentos::ai {
namespace {

using core::Json;

std::string text_of(const Json& object, const char* key)
{
    const Json* held = object.find(key);
    return held != nullptr && held->is_string() ? held->as_string() : std::string();
}

} // namespace

std::vector<std::string> JobTemplate::render(const core::Json& values) const
{
    std::vector<std::string> out;
    out.reserve(steps.size());
    for (const std::string& step : steps) {
        std::string line = step;
        for (const JobParam& param : params) {
            const Json* given = values.find(param.name);
            if (given == nullptr || !given->is_string() || given->as_string().empty()) continue;

            // A PLACEHOLDER WITH NO VALUE IS LEFT WHERE IT IS. Blanking it would
            // produce `ad=` — a command line that looks finished and is not, and
            // that the bus would refuse with a message about the wrong thing.
            const std::string mark = "<" + param.name + ">";
            for (std::size_t at = line.find(mark); at != std::string::npos;
                 at             = line.find(mark, at + given->as_string().size()))
                line.replace(at, mark.size(), given->as_string());
        }
        out.push_back(std::move(line));
    }
    return out;
}

core::Json JobTemplate::to_json(bool with_steps) const
{
    Json out;
    out.set("sablon", Json::string(id));
    out.set("ad", Json::string(title));
    out.set("ozet", Json::string(summary));
    out.set("surum", Json::string(version));
    out.set("adim_sayisi", Json::integer(static_cast<std::int64_t>(steps.size())));

    Json blanks = Json::array({});
    for (const JobParam& param : params) {
        Json one;
        one.set("ad", Json::string(param.name));
        one.set("aciklama", Json::string(param.help));
        one.set("zorunlu", Json::boolean(param.required));
        if (!param.example.empty()) one.set("ornek", Json::string(param.example));
        blanks.push(std::move(one));
    }
    out.set("parametreler", std::move(blanks));

    if (with_steps) {
        Json lines = Json::array({});
        for (const std::string& step : steps)
            lines.push(Json::string(step));
        out.set("adimlar", std::move(lines));
        if (!notes.empty()) out.set("notlar", Json::string(notes));
    }
    return out;
}

const JobTemplate* JobTemplateCatalog::find(std::string_view id) const
{
    for (const JobTemplate& one : templates)
        if (one.id == id) return &one;
    return nullptr;
}

core::Result<JobTemplateCatalog> JobTemplateCatalog::from_json(std::string_view text)
{
    core::Result<Json> parsed = core::Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "İş şablonu paketi okunamadı: " + parsed.error().message);

    const Json& root = parsed.value();
    if (!root.is_object())
        return core::err(core::ErrorCode::ParseError, "İş şablonu paketi bir JSON nesnesi olmalı.");

    JobTemplateCatalog out;
    if (const Json* version = root.find("schema_version"); version != nullptr && version->is_int())
        out.schema_version = version->as_int();
    out.package_version = text_of(root, "package_version");

    const Json* listed = root.find("sablonlar");
    if (listed == nullptr || !listed->is_array())
        return core::err(core::ErrorCode::ParseError,
                         "İş şablonu paketinde `sablonlar` dizisi yok.");

    for (const Json& entry : listed->as_array()) {
        if (!entry.is_object()) continue;

        JobTemplate one;
        one.id      = text_of(entry, "id");
        one.title   = text_of(entry, "ad");
        one.summary = text_of(entry, "ozet");
        one.version = text_of(entry, "surum");
        one.notes   = text_of(entry, "notlar");

        // AN UNNAMED TEMPLATE IS A DEFECT IN THE PACKAGE, not a row to skip: a
        // client asks for a template BY ID, so one without an id could never be
        // reached and its presence would be a silent lie about what is shipped.
        if (one.id.empty())
            return core::err(core::ErrorCode::ValidationFailed,
                             "İş şablonu paketinde kimliksiz bir şablon var.");

        if (const Json* blanks = entry.find("parametreler");
            blanks != nullptr && blanks->is_array())
            for (const Json& blank : blanks->as_array()) {
                if (!blank.is_object()) continue;
                JobParam param;
                param.name    = text_of(blank, "ad");
                param.help    = text_of(blank, "aciklama");
                param.example = text_of(blank, "ornek");
                if (const Json* need = blank.find("zorunlu"); need != nullptr && need->is_bool())
                    param.required = need->as_bool();
                if (!param.name.empty()) one.params.push_back(std::move(param));
            }

        if (const Json* lines = entry.find("adimlar"); lines != nullptr && lines->is_array())
            for (const Json& line : lines->as_array())
                if (line.is_string()) one.steps.push_back(line.as_string());

        if (one.steps.empty())
            return core::err(core::ErrorCode::ValidationFailed,
                             "İş şablonu '" + one.id + "' hiç adım taşımıyor.");

        out.templates.push_back(std::move(one));
    }

    if (out.templates.empty())
        return core::err(core::ErrorCode::ValidationFailed, "İş şablonu paketi boş.");
    return out;
}

} // namespace kentos::ai
