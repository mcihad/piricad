// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/context.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/session.hpp"

namespace piricad::command {
namespace {

Point2 to_point(const Value& v)
{
    return v.as_point();
}

double to_number(const Value& v)
{
    return v.as_number();
}

std::int64_t to_integer(const Value& v)
{
    return v.as_int();
}

std::string to_text(const Value& v)
{
    return v.as_text();
}

bool to_bool(const Value& v)
{
    return v.as_bool();
}

} // namespace

Value apply_input_aids(Session& session, const Prompt& prompt, Value v)
{
    // Only a point is aimed; a number, a name or a flag is typed exactly.
    if (v.kind() != Value::Kind::Point && v.kind() != Value::Kind::PointList) return v;

    Bus& bus               = session.bus();
    const AidSettings& set = bus.aid_settings();

    const bool object_snap = set.snap_radius > 0 && (set.modes & core::SnapObjectMask) != 0;
    const bool grid        = set.grid_step > 0 && (set.modes & core::SnapGrid) != 0;
    const bool direction =
        prompt.has_rubber_band &&
        (set.ortho || (set.polar_step > 0 && (set.modes & core::SnapPolar) != 0));
    if (!object_snap && !grid && !direction) return v;

    const core::Document& doc = bus.document();

    const auto resolve = [&](core::Point2 aim, bool has_base, core::Point2 base) {
        const core::SnapResult r = bus.aids().resolve(doc, set, aim, has_base, base);
        bus.aids().remember(r);
        return r.point;
    };

    if (v.kind() == Value::Kind::Point)
        return Value::point(resolve(v.as_point(), prompt.has_rubber_band, prompt.rubber_origin));

    // A whole point list arrives when a script hands one over at once. Each point
    // is resolved against the one before it, exactly as an interactive run would,
    // so a scripted polyline and a drawn polyline agree vertex for vertex.
    Value::Points points = v.as_points();
    bool has_base        = prompt.has_rubber_band;
    core::Point2 base    = prompt.rubber_origin;

    for (auto& p : points) {
        p        = resolve(p, has_base, base);
        base     = p;
        has_base = true;
    }
    return Value::points(std::move(points));
}

Context::Context(Session& session, Transaction& tx, const core::Document& doc)
    : session_(session), tx_(tx), doc_(doc)
{}

InputAwaiter<Point2> Context::point(std::string param, std::string message, PointOptions o)
{
    Param p = Param::point(param);
    Prompt prompt;
    prompt.message         = std::move(message);
    prompt.kind            = ParamKind::Point;
    prompt.param           = param;
    prompt.has_rubber_band = o.rubber_band;
    prompt.rubber_origin   = o.rubber_origin;
    return InputAwaiter<Point2>(session_, std::move(p), std::move(prompt), &to_point);
}

InputAwaiter<double> Context::number(std::string param, std::string message)
{
    Param p = Param::number(param, Arity::exactly(1));
    Prompt prompt{std::move(message), ParamKind::Number, param, false, {}};
    return InputAwaiter<double>(session_, std::move(p), std::move(prompt), &to_number);
}

InputAwaiter<std::int64_t> Context::integer(std::string param, std::string message)
{
    Param p = Param::integer(param, Arity::exactly(1));
    Prompt prompt{std::move(message), ParamKind::Integer, param, false, {}};
    return InputAwaiter<std::int64_t>(session_, std::move(p), std::move(prompt), &to_integer);
}

InputAwaiter<std::string> Context::text(std::string param, std::string message)
{
    Param p = Param::text(param, Arity::exactly(1));
    Prompt prompt{std::move(message), ParamKind::Text, param, false, {}};
    return InputAwaiter<std::string>(session_, std::move(p), std::move(prompt), &to_text);
}

InputAwaiter<bool> Context::boolean(std::string param, std::string message)
{
    Param p = Param::boolean(param, Arity::exactly(1));
    Prompt prompt{std::move(message), ParamKind::Bool, param, false, {}};
    return InputAwaiter<bool>(session_, std::move(p), std::move(prompt), &to_bool);
}

Value Context::argument(std::string_view name) const
{
    // Pre-supplied bundles are read directly; an interactive run fills the bundle
    // as the user answers, which is what resolved() holds. Neither path tells the
    // command body which client it is serving.
    if (const Args* preset = session_.input().preset())
        if (const Value* v = preset->find(name)) return *v;
    return session_.resolved().get(name);
}

bool Context::has_argument(std::string_view name) const
{
    return !argument(name).empty();
}

core::LayerId Context::active_layer() const
{
    return session_.bus().active_layer();
}

void Context::echo(std::string message) const
{
    session_.bus().echo(message);
}

void Context::record(std::string param, Value v)
{
    session_.record(std::move(param), std::move(v));
}

} // namespace piricad::command
