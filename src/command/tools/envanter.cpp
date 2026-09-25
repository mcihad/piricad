// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the capability inventory, taken from the live registries.
//
// WHY THIS IS NOT A COUNT IN A COMMENT. The question "how much of this program
// can an agent actually reach" was being answered by grepping `return
// CommandSpec{...}` in the tree — which misses every command a module registers
// through a loop or a helper, and cannot see a parameter's arity, its word list
// or its range. A number taken that way is a guess with a decimal point on it.
//
// This walks the SAME registries the program builds at startup and prints one
// JSON line per command, so the gate that reads it is reading the surface a
// client is really served (CLAUDE.md 5.10: the one list is `Registry`).
//
// It is an executable at the top of the dependency graph, so it may link the
// processing and domain modules the command library must not (Article 3.2a).
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <cstdio>
#include <exception>
#include <fstream>
#include <ostream>
#include <string>

namespace {

using kentos::command::CommandSpec;
using kentos::command::Effect;
using kentos::command::Flags;
using kentos::command::Param;
using kentos::command::Registry;
using kentos::command::UndoPolicy;
using kentos::core::Json;

bool has(Flags value, Flags bit)
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(bit)) != 0U;
}

const char* undo_name(UndoPolicy u)
{
    switch (u) {
    case UndoPolicy::SingleTransaction: return "tek_islem";
    case UndoPolicy::None: return "yok";
    case UndoPolicy::Custom: return "kendi";
    }
    return "bilinmiyor";
}

Json param_json(const Param& p)
{
    Json out;
    out.set("ad", Json::string(p.name));
    out.set("tur", Json::string(kentos::command::param_kind_name(p.kind)));
    out.set("en_az", Json::integer(p.arity.min));
    out.set("en_cok", Json::integer(p.arity.max));
    if (!p.was.empty()) out.set("eski_ad", Json::string(p.was));
    if (!p.choices.empty()) {
        std::vector<Json> words;
        words.reserve(p.choices.size());
        for (const std::string& one : p.choices)
            words.push_back(Json::string(one));
        out.set("secenekler", Json::array(std::move(words)));
    }
    if (p.bounded) {
        out.set("alt", Json::integer(p.low));
        out.set("ust", Json::integer(p.high));
    }
    return out;
}

Json command_json(const CommandSpec& spec)
{
    Json out;
    out.set("kimlik", Json::string(spec.id));

    std::vector<Json> names;
    names.reserve(spec.names.size());
    for (const std::string& one : spec.names)
        names.push_back(Json::string(one));
    out.set("adlar", Json::array(std::move(names)));

    out.set("kategori", Json::string(kentos::command::category_name(spec.category)));
    out.set("geri_alma", Json::string(undo_name(spec.undo)));

    std::vector<Json> params;
    params.reserve(spec.params.size());
    for (const Param& p : spec.params)
        params.push_back(param_json(p));
    out.set("parametreler", Json::array(std::move(params)));

    // THE FLAGS, EACH BY NAME. A single "ai: true" would hide the distinction
    // `NoEffect` exists to make: `ReadOnly` says "skips the transaction path",
    // which `core.save` and `core.export` both carry while writing over a file.
    Json flags;
    flags.set("etkilesimli", Json::boolean(has(spec.flags, Flags::Interactive)));
    flags.set("betiklenebilir", Json::boolean(has(spec.flags, Flags::Scriptable)));
    flags.set("yapay_zeka", Json::boolean(has(spec.flags, Flags::AiAccessible)));
    flags.set("seffaf", Json::boolean(has(spec.flags, Flags::Transparent)));
    flags.set("salt_okunur", Json::boolean(has(spec.flags, Flags::ReadOnly)));
    flags.set("etkisiz", Json::boolean(has(spec.flags, Flags::NoEffect)));
    flags.set("uzun_is", Json::boolean(has(spec.flags, Flags::LongRunning)));
    out.set("bayraklar", std::move(flags));

    // WHAT IT LEAVES CHANGED, which is a different question from which client
    // may reach it. `effect_of` is the one place that answers it, so the
    // inventory, a policy and an audit record cannot disagree (CLAUDE.md 5.10).
    const Effect worst = kentos::command::effect_of(spec);
    std::vector<Json> etkiler;
    for (const Effect bit :
         {Effect::Query, Effect::ViewChange, Effect::DocumentEdit, Effect::FileRead,
          Effect::FileWrite, Effect::ExternalWrite, Effect::SettingsChange})
        if (kentos::command::has_effect(worst, bit))
            etkiler.push_back(Json::string(kentos::command::effect_name(bit)));
    out.set("etki", Json::array(std::move(etkiler)));

    if (!spec.effect_verb.empty()) {
        Json per;
        for (const kentos::command::VerbEffect& one : spec.verb_effects) {
            std::vector<Json> words;
            for (const Effect bit :
                 {Effect::Query, Effect::ViewChange, Effect::DocumentEdit, Effect::FileRead,
                  Effect::FileWrite, Effect::ExternalWrite, Effect::SettingsChange})
                if (kentos::command::has_effect(one.effect, bit))
                    words.push_back(Json::string(kentos::command::effect_name(bit)));
            per.set(one.word, Json::array(std::move(words)));
        }
        out.set("fiil_parametresi", Json::string(spec.effect_verb));
        out.set("fiil_etkileri", std::move(per));
    }

    out.set("ozet", Json::string(spec.summary));
    return out;
}

int run(int argc, char** argv)
{
    Registry reg;
    kentos::command::register_builtin_commands(reg);
    kentos::processing::register_processing_commands(reg);
    kentos::domain::geodesy::register_geodesy_commands(reg);
    kentos::domain::cadastre::register_cadastre_commands(reg);
    kentos::domain::surface::register_surface_commands(reg);
    kentos::ai::register_ai_commands(reg);

    // SORTED BY ID, so two runs on two machines produce the same bytes and a
    // diff in the gate means a real change (Article 2.5, §7.3).
    std::vector<std::size_t> order(reg.all().size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return reg.all()[a].id < reg.all()[b].id; });

    std::vector<Json> rows;
    rows.reserve(order.size());
    for (const std::size_t at : order)
        rows.push_back(command_json(reg.all()[at]));

    Json root;
    root.set("surum", Json::integer(1));
    root.set("komut_sayisi", Json::integer(static_cast<std::int64_t>(rows.size())));
    root.set("komutlar", Json::array(std::move(rows)));

    const std::string text = root.dump_pretty(2) + "\n";
    if (argc < 2) {
        (void)std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }
    std::ofstream out(argv[1], std::ios::out | std::ios::binary);
    if (!out) {
        (void)std::fprintf(stderr, "envanter: '%s' yazılamadı\n", argv[1]);
        return 1;
    }
    out << text;
    (void)std::fprintf(stdout, "envanter: %zu komut -> %s\n", order.size(), argv[1]);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        (void)std::fprintf(stderr, "envanter: %s\n", e.what());
        return 1;
    } catch (...) {
        (void)std::fprintf(stderr, "envanter: bilinmeyen hata\n");
        return 1;
    }
}
