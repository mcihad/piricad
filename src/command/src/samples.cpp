// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/samples.hpp"

#include "piricad/command/drawing_catalogs.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"

#include <filesystem>

namespace piricad::command {

using core::err;
using core::ErrorCode;

namespace {

constexpr const char* kIndex = "data/ornekler/ornekler.json";

std::string text_of(const core::Json& object, std::string_view key)
{
    const core::Json* v = object.find(key);
    return v != nullptr && v->is_string() ? v->as_string() : std::string();
}

} // namespace

const Sample* SampleCatalog::find(std::string_view name) const noexcept
{
    for (const Sample& s : samples)
        if (core::turkish_key_equals(s.id, name) || core::turkish_key_equals(s.title, name))
            return &s;
    return nullptr;
}

core::Result<SampleCatalog> load_samples()
{
    const std::string index = resolve_catalog_path(kIndex);
    if (index.empty())
        return err(ErrorCode::NotFound,
                   "Örnek projeler bulunamadı: 'data/ornekler/ornekler.json' yok. Kurulumda "
                   "eksikse PIRICAD_DATA ile veri dizinini gösterin.");
    auto text = read_catalog_text(kIndex);
    if (!text) return text.error();
    auto json = core::Json::parse(text.value());
    if (!json || !json.value().is_object())
        return err(ErrorCode::ParseError, "'" + index + "' geçerli bir örnek proje dizini değil.");

    const core::Json* list = json.value().find("projeler");
    if (list == nullptr || !list->is_array())
        return err(ErrorCode::ParseError, "'" + index + "' içinde 'projeler' listesi yok.");

    const std::filesystem::path folder = std::filesystem::path(index).parent_path();
    SampleCatalog out;
    out.package_version = text_of(json.value(), "package_version");
    for (const core::Json& one : list->as_array()) {
        Sample s;
        s.id      = text_of(one, "id");
        s.title   = text_of(one, "baslik");
        s.tagline = text_of(one, "kisa");
        s.summary = text_of(one, "ozet");
        s.layout  = text_of(one, "yerlesim");
        if (const core::Json* scale = one.find("olcek"); scale != nullptr)
            s.scale = scale->as_int();
        const std::string file = text_of(one, "betik");
        if (s.id.empty() || s.title.empty() || file.empty())
            return err(ErrorCode::ParseError,
                       "'" + index + "' içinde id, başlık ya da betiği eksik bir proje var.");
        s.script = (folder / file).string();
        if (const core::Json* steps = one.find("deneyin"); steps != nullptr && steps->is_array())
            for (const core::Json& step : steps->as_array())
                s.steps.push_back({text_of(step, "komut"), text_of(step, "aciklama")});
        out.samples.push_back(std::move(s));
    }
    return out;
}

} // namespace piricad::command
