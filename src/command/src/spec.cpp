// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/spec.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include "piricad/core/text.hpp"

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
    case Category::Processing: return "İşlem";
    }
    return "?";
}

const char* target_name(Targets one)
{
    switch (one) {
    case Targets::Points: return "nokta";
    case Targets::Lines: return "çizgi";
    case Targets::Faces: return "alan";
    case Targets::Curves: return "eğri";
    case Targets::Texts: return "yazı";
    case Targets::Hatches: return "tarama";
    case Targets::Dimensions: return "ölçü";
    case Targets::Blocks: return "blok";
    case Targets::Leaders: return "kılavuz çizgi";
    case Targets::None:
    case Targets::Any: break;
    }
    return "?";
}

Targets targets_of(const CommandSpec& spec, const Args& args)
{
    for (const VerbTargets& row : spec.verb_targets) {
        const Value v = args.get(row.param);
        if (v.kind() != Value::Kind::Text) continue;
        if (core::turkish_fold_key(v.as_text()) == core::turkish_fold_key(row.word))
            return row.targets;
    }
    return spec.targets;
}

std::string target_names(Targets set)
{
    std::string out;
    for (const Targets one : {
             Targets::Points,
             Targets::Lines,
             Targets::Faces,
             Targets::Curves,
             Targets::Texts,
             Targets::Hatches,
             Targets::Dimensions,
             Targets::Blocks,
             Targets::Leaders,
         })
        if (has_target(set, one)) out += (out.empty() ? "" : ", ") + std::string(target_name(one));
    return out;
}

std::string targets_sentence(const CommandSpec& spec)
{
    if (spec.targets == Targets::Any) return {};
    std::string out = target_names(spec.targets);
    // Rows that narrow one parameter to the same classes are one clause: four
    // methods that all walk an edge are said once, not four times.
    for (std::size_t i = 0; i < spec.verb_targets.size();) {
        const VerbTargets& first = spec.verb_targets[i];
        std::size_t end          = i + 1;
        while (end < spec.verb_targets.size() && spec.verb_targets[end].param == first.param &&
               spec.verb_targets[end].targets == first.targets)
            ++end;
        out += "; ";
        for (std::size_t k = i; k < end; ++k) {
            if (k > i) out += k + 1 == end ? " ya da " : ", ";
            out += "`" + spec.verb_targets[k].param + "=" + spec.verb_targets[k].word + "`";
        }
        out += " ile " + target_names(first.targets);
        i = end;
    }
    return out;
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

std::optional<int> Param::length_exponent() const
{
    if (kind != ParamKind::Number && kind != ParamKind::Integer) return std::nullopt;
    // The words the declarations use: `m`, `mm`, and `kâğıt mm` for a size on the sheet.
    // Folded, so `KAĞIT MM` and `kağıt mm` are the same declaration.
    const std::string folded = core::turkish_fold_key(unit);
    if (folded == "M") return 0;
    if (folded == "MM") return -3;
    // `kâğıt mm`: a size on the sheet, whichever way the first word is spelled (the circumflex
    // is not one of the letters the fold maps).
    if (folded.size() > 3 && folded.front() == 'K' && folded.ends_with(" MM")) return -3;
    return std::nullopt;
}

Param Param::text(std::string name, Arity a, std::string help)
{
    return Param{std::move(name), ParamKind::Text, a, std::move(help)};
}

Param Param::draw_layer()
{
    Param p = Param::text("katman", Arity::optional(),
                          "Çizilenlerin katmanı, adıyla; verilmezse etkin katman. Etkin katmanı "
                          "değiştirmez; komut soru sorarken de yazılabilir")
                  .en("layer");
    p.amendable = true;
    return p;
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

Args canonical_arguments(const CommandSpec& spec, Args args)
{
    std::vector<std::string> declared;
    declared.reserve(spec.params.size());
    for (const Param& p : spec.params)
        declared.push_back(p.name);
    args.reorder_like(declared);
    for (const Param& p : spec.params)
        if (const Value* held = args.find(p.name); held != nullptr && held->empty())
            args.erase(p.name);
    for (const Param& p : spec.params) {
        const Value* held = args.find(p.name);
        if (held == nullptr) continue;
        if (p.kind == ParamKind::Number && held->kind() == Value::Kind::Int)
            args.set(p.name, Value::number(held->as_number()));
        else if (p.kind == ParamKind::Integer && held->kind() == Value::Kind::Number)
            args.set(p.name, Value::integer(held->as_int()));
    }
    return args;
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

} // namespace piricad::command
