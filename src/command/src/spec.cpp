// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/spec.hpp"

#include <algorithm>

#include "kentos_cad/core/text.hpp"

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

// ------------------------------------------------------------- the effect --

const char* effect_name(Effect one)
{
    switch (one) {
    case Effect::None: return "bildirilmemis";
    case Effect::Query: return "sorgu";
    case Effect::ViewChange: return "gorunum";
    case Effect::DocumentEdit: return "belge_duzenleme";
    case Effect::FileRead: return "dosya_okuma";
    case Effect::FileWrite: return "dosya_yazma";
    case Effect::ExternalWrite: return "dis_yazma";
    case Effect::SettingsChange: return "ayar_degisikligi";
    }
    return "bilinmiyor";
}

namespace {

/// The answer when a spec states nothing. NOT a guess at harmlessness: the only
/// flag that claims a command changed nothing is `NoEffect`, and everything else
/// falls to what its category can do.
Effect from_flags(const CommandSpec& spec)
{
    const auto carries = [&](Flags bit) {
        return (static_cast<std::uint32_t>(spec.flags) & static_cast<std::uint32_t>(bit)) != 0U;
    };

    if (carries(Flags::NoEffect))
        return carries(Flags::Transparent) ? Effect::Query | Effect::ViewChange : Effect::Query;

    // THE CATEGORY, NOT THE FLAG. `ReadOnly` without `NoEffect` is the trap this
    // whole type exists for — `core.save`, `core.saveas` and `core.export` all
    // carry it and all write over a file — so reading anything into it beyond
    // "skips the transaction path" is how the wrong answer gets produced
    // confidently. The category is what the command SAYS it is about; a command
    // whose category misleads declares `.effect` itself.
    switch (spec.category) {
    case Category::View: return Effect::Query | Effect::ViewChange;
    case Category::Query: return Effect::Query;
    case Category::File: return Effect::Query | Effect::FileRead | Effect::FileWrite;
    case Category::Draw:
    case Category::Modify:
    case Category::Layer:
    case Category::Processing: return Effect::DocumentEdit;
    case Category::Script:
    case Category::System: break;
    }
    // A system or script command that opens no transaction still changes
    // something a person would notice, and nobody has said what. The cautious
    // answer names both the stored preference and the drawing.
    if (carries(Flags::ReadOnly)) return Effect::Query | Effect::SettingsChange;
    return Effect::DocumentEdit;
}

} // namespace

Effect effect_of(const CommandSpec& spec)
{
    Effect worst = spec.effect == Effect::None ? from_flags(spec) : spec.effect;
    for (const VerbEffect& one : spec.verb_effects)
        worst = worst | one.effect;
    return worst;
}

Effect effect_of(const CommandSpec& spec, const Args& args)
{
    const Effect stated = spec.effect == Effect::None ? from_flags(spec) : spec.effect;
    if (spec.effect_verb.empty() || spec.verb_effects.empty()) return stated;

    const Value* given = args.find(spec.effect_verb);
    // NOT GIVEN IS NOT "HARMLESS". An interactive run asks for the verb after
    // validation, so the honest answer before it is asked is the worst case.
    if (given == nullptr || given->empty()) return effect_of(spec);

    const std::string word = core::turkish_fold_key(given->as_text());
    for (const VerbEffect& one : spec.verb_effects)
        if (core::turkish_fold_key(one.word) == word) return one.effect;

    // A WORD NOBODY DECLARED. The validator will refuse it in a moment, but
    // until it does the cautious answer is the command's worst case.
    return effect_of(spec);
}

std::string python_callable_name(const CommandSpec& spec)
{
    if (!spec.python.empty()) return spec.python;

    const std::size_t dot = spec.id.find('.');
    if (dot == std::string::npos) return spec.id;

    // `core` IS THE DEFAULT NAMESPACE and drops out, which is what a Python
    // package does with the module everything lives in. Keeping it would put
    // `core_` in front of 111 of the 117 names in order to disambiguate six.
    if (spec.id.compare(0, dot, "core") == 0) return spec.id.substr(dot + 1);

    std::string out = spec.id;
    std::replace(out.begin(), out.end(), '.', '_');
    return out;
}

} // namespace kentos::command
