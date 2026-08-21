// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/validation.hpp"

namespace piricad::command {
namespace {

bool kind_accepts(ParamKind expected, const Value& v)
{
    switch (expected) {
    case ParamKind::Point:
        return v.kind() == Value::Kind::Point ||
               (v.kind() == Value::Kind::PointList && v.as_points().size() == 1);
    case ParamKind::PointList:
        return v.kind() == Value::Kind::PointList || v.kind() == Value::Kind::Point;
    case ParamKind::Number: return v.kind() == Value::Kind::Number || v.kind() == Value::Kind::Int;
    case ParamKind::Integer: return v.kind() == Value::Kind::Int;
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
                             "'" + spec.id + "': required parameter '" + p.name +
                                 "' is missing. Expected: " + param_kind_name(p.kind));
        }

        if (!kind_accepts(p.kind, *v)) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': parameter '" + p.name + "' expects " +
                                 param_kind_name(p.kind) + ", got a different value type");
        }

        const std::size_t n = multiplicity(*v);
        if (n < p.arity.min) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': parameter '" + p.name + "' needs at least " +
                                 std::to_string(p.arity.min) + " value(s), got " +
                                 std::to_string(n));
        }
        if (p.arity.max != 0xFFFFFFFFu && n > p.arity.max) {
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec.id + "': parameter '" + p.name + "' accepts at most " +
                                 std::to_string(p.arity.max) + " value(s), got " +
                                 std::to_string(n));
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
                             "'" + spec.id + "': unknown parameter '" + name +
                                 "'. Declared parameters: " + (known.empty() ? "(none)" : known));
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

} // namespace piricad::command
