// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/validation.hpp"

#include "piricad/core/text.hpp"

#include <algorithm>

namespace piricad::command {
namespace {

bool kind_accepts(const Param& p, const Value& v)
{
    // A parameter declared as more than one integer is a LIST of integers, and a
    // list arrives as an IdList. Without this an `Arity::at_least` integer could
    // only ever be given one value, which is not a list at all — the arity said
    // one thing and the type check said another.
    const bool many = p.arity.max > 1;

    switch (p.kind) {
    case ParamKind::Point:
        return v.kind() == Value::Kind::Point ||
               (v.kind() == Value::Kind::PointList && v.as_points().size() == 1);
    case ParamKind::PointList:
        return v.kind() == Value::Kind::PointList || v.kind() == Value::Kind::Point;
    // A RUN OF READINGS SATISFIES A NUMBER PARAMETER WHOSE ARITY ALLOWS MORE THAN
    // ONE, exactly as an id list satisfies such an `Integer`. `IdList` is
    // accepted for the same reason `Value::as_numbers` reads one: `[10, 30, 60]`
    // in a journal line is an array of whole numbers and nothing in it says
    // which of the two kinds wrote it, so refusing it here would mean a journal
    // written with whole-metre readings could not replay (Article 1.4).
    case ParamKind::Number:
        return v.kind() == Value::Kind::Number || v.kind() == Value::Kind::Int ||
               (many && (v.kind() == Value::Kind::NumberList || v.kind() == Value::Kind::IdList));
    case ParamKind::Integer:
        return v.kind() == Value::Kind::Int || (many && v.kind() == Value::Kind::IdList);
    // A ONE-WORD LIST AND A WORD ARE THE SAME THING TO A CALLER, and the arity
    // is what says whether more than one is allowed — the check below does that.
    case ParamKind::Text: return v.kind() == Value::Kind::Text || v.kind() == Value::Kind::TextList;
    case ParamKind::Bool: return v.kind() == Value::Kind::Bool || v.kind() == Value::Kind::Int;
    case ParamKind::Selection: return v.kind() == Value::Kind::IdList;
    }
    return false;
}

std::size_t multiplicity(const Value& v)
{
    switch (v.kind()) {
    case Value::Kind::Empty: return 0;
    case Value::Kind::Point: return 1;
    case Value::Kind::PointList: return v.as_points().size();
    case Value::Kind::IdList: return v.as_ids().size();
    case Value::Kind::TextList: return v.as_texts().size();
    case Value::Kind::NumberList: return v.as_numbers().size();
    default: return 1;
    }
}

} // namespace

core::Status Validator::check_against_spec(const CommandSpec& spec, const Args& args)
{
    using core::ErrorCode;

    for (const auto& p : spec.params) {
        const Value* v = args.find(p.name);

        if (!v || v->empty()) {
            if (p.arity.min == 0) continue;
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': zorunlu '" + p.name +
                                 "' parametresi eksik. Beklenen: " + param_kind_label(p.kind));
        }

        if (!kind_accepts(p, *v)) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': '" + p.name + "' parametresi " +
                                 param_kind_label(p.kind) +
                                 " bekliyor, başka türde bir değer geldi.");
        }

        const std::size_t n = multiplicity(*v);
        if (n < p.arity.min) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': '" + p.name + "' parametresi en az " +
                                 std::to_string(p.arity.min) + " değer istiyor, " +
                                 std::to_string(n) + " değer geldi.");
        }
        if (p.arity.max != 0xFFFFFFFFu && n > p.arity.max) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': '" + p.name + "' parametresi en fazla " +
                                 std::to_string(p.arity.max) + " değer alır, " + std::to_string(n) +
                                 " değer geldi.");
        }

        // A DECLARED WORD LIST IS CHECKED HERE, before the body runs, because that
        // is where every other part of the contract is checked (Article 1.3). The
        // comparison folds Turkish, so `yalnız` reaches `yalniz` and `GİZLE`
        // reaches `gizle` — the same folding the registry resolves a name with
        // (CLAUDE.md 5.6).
        if (!p.choices.empty() && v->kind() == Value::Kind::Text) {
            const std::string& given = v->as_text();
            bool known               = false;
            for (const std::string& word : p.choices)
                if (core::turkish_key_equals(given, word)) known = true;
            if (!known) {
                std::string list;
                for (const std::string& word : p.choices) {
                    if (!list.empty()) list += " / ";
                    list += word;
                }
                return core::err(ErrorCode::ValidationFailed,
                                 "'" + spec.id + "': '" + p.name + "' için tanınmayan değer '" +
                                     given + "'. Kabul edilenler: " + list);
            }
        }

        // And a declared range, for the same reason.
        if (p.bounded && (v->kind() == Value::Kind::Int || v->kind() == Value::Kind::Number)) {
            const std::int64_t got = v->as_int();
            if (got < p.low || got > p.high) {
                return core::err(ErrorCode::ValidationFailed,
                                 "'" + spec.id + "': '" + p.name + "' " + std::to_string(p.low) +
                                     " ile " + std::to_string(p.high) + " arasında olmalı, " +
                                     std::to_string(got) + " geldi.");
            }
        }
    }

    // Unknown arguments are rejected: a typo in a script must not be silently ignored.
    for (const auto& [name, value] : args.items()) {
        bool declared = false;
        for (const auto& p : spec.params) {
            if (p.name == name) {
                declared = true;
                break;
            }
        }
        if (!declared) {
            std::string known;
            for (const auto& p : spec.params) {
                if (!known.empty()) known += ", ";
                known += p.name;
            }
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': bilinmeyen parametre '" + name +
                                 "'. Tanımlı parametreler: " + (known.empty() ? "(yok)" : known));
        }
    }
    return core::ok();
}

void Validator::add_rule(std::shared_ptr<Rule> rule)
{
    if (rule) rules_.push_back(std::move(rule));
}

bool takes_draw_layer(const CommandSpec& spec) noexcept
{
    return std::ranges::any_of(spec.params,
                               [](const Param& p) { return p.amendable && p.name == "katman"; });
}

core::Result<core::LayerId> draw_layer_of(const CommandSpec& spec, const Args& args,
                                          const core::Document& doc)
{
    if (!takes_draw_layer(spec)) return core::kNoLayer;
    const Value* given = args.find("katman");
    if (given == nullptr || given->empty()) return core::kNoLayer;
    const std::string name = given->as_text();
    if (const core::LayerId found = doc.find_layer(name); found != core::kNoLayer) return found;

    // THE LAYERS THERE ARE, so the refusal says what would have worked. A
    // dozen is what a person reads; the rest are counted.
    constexpr std::size_t kShown = 12;
    std::string known;
    std::size_t listed = 0;
    for (const core::Layer& layer : doc.layers()) {
        if (listed == kShown) break;
        known += (known.empty() ? "" : ", ") + layer.name;
        ++listed;
    }
    if (doc.layers().size() > kShown)
        known += " ve " + std::to_string(doc.layers().size() - kShown) + " katman daha";
    return core::err(core::ErrorCode::NotFound,
                     "Katman bulunamadı: '" + name + "'. Çizimdeki katmanlar: " + known +
                         ". Yeni bir katmanı önce KATMAN ad=" + name + " ile oluşturun.");
}

core::Status Validator::run(const ValidationRequest& req) const
{
    auto st = check_against_spec(req.spec, req.args);
    if (!st) return st;
    if (auto layer = draw_layer_of(req.spec, req.args, req.document); !layer) return layer.error();

    for (const auto& rule : rules_) {
        auto r = rule->check(req);
        if (!r) {
            return core::err(r.error().code, "[" + rule->name() + "] " + r.error().message);
        }
    }
    return core::ok();
}

} // namespace piricad::command
