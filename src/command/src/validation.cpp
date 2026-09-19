// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/validation.hpp"

#include "kentos_cad/core/text.hpp"

namespace kentos::command {
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
    case ParamKind::Number: return v.kind() == Value::Kind::Number || v.kind() == Value::Kind::Int;
    case ParamKind::Integer:
        return v.kind() == Value::Kind::Int || (many && v.kind() == Value::Kind::IdList);
    case ParamKind::Text: return v.kind() == Value::Kind::Text;
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

core::Status Validator::run(const ValidationRequest& req) const
{
    auto st = check_against_spec(req.spec, req.args);
    if (!st) return st;

    for (const auto& rule : rules_) {
        auto r = rule->check(req);
        if (!r) {
            return core::err(r.error().code, "[" + rule->name() + "] " + r.error().message);
        }
    }
    return core::ok();
}

} // namespace kentos::command
