// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/context.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/session.hpp"

#include "kentos_cad/core/identity.hpp"

#include <string>
#include <utility>
#include <vector>

namespace kentos::command {
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

Value::Ints to_ids(const Value& v)
{
    return v.as_ids();
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
    prompt.rubber_shape    = o.rubber_shape;
    prompt.rubber_chain    = std::move(o.rubber_chain);
    return InputAwaiter<Point2>(session_, std::move(p), std::move(prompt), &to_point);
}

InputAwaiter<double> Context::number(std::string param, std::string message)
{
    Param p = Param::number(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Number, .param = param};
    return InputAwaiter<double>(session_, std::move(p), std::move(prompt), &to_number);
}

InputAwaiter<std::int64_t> Context::integer(std::string param, std::string message)
{
    Param p = Param::integer(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Integer, .param = param};
    return InputAwaiter<std::int64_t>(session_, std::move(p), std::move(prompt), &to_integer);
}

InputAwaiter<std::string> Context::text(std::string param, std::string message)
{
    Param p = Param::text(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Text, .param = param};
    return InputAwaiter<std::string>(session_, std::move(p), std::move(prompt), &to_text);
}

InputAwaiter<bool> Context::boolean(std::string param, std::string message)
{
    Param p = Param::boolean(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Bool, .param = param};
    return InputAwaiter<bool>(session_, std::move(p), std::move(prompt), &to_bool);
}

InputAwaiter<Value::Ints> Context::objects(std::string param, std::string message)
{
    // Arity starts at ONE: a modify command with nothing to modify is not a
    // command that ran, and the bus should say so rather than the body.
    Param p{param, ParamKind::Selection, Arity{1, 0xFFFFFFFFu}, "İşlem yapılacak nesneler"};
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Selection, .param = param};
    return InputAwaiter<Value::Ints>(session_, std::move(p), std::move(prompt), &to_ids);
}

Task<bool> want_objects(Context& ctx, std::string param, std::string message,
                        std::vector<std::int64_t>& out, std::size_t most)
{
    // A refusal must leave NOTHING behind. The awaiter records whatever it
    // resolved under the parameter it was given, and it does that before the
    // caller has had a chance to judge the count — so a BÖL that is handed three
    // lines and says "one at a time" would otherwise leave three ids sitting in
    // `nesne`, which post-run validation rejects with a second message about
    // arity that the user can do nothing with. An empty Value reads as "absent"
    // to `check_against_spec`, which is the truth: nothing was accepted.
    const auto refuse = [&ctx, &param](std::string why) {
        ctx.record(param, Value{});
        ctx.echo(std::move(why));
        return false;
    };

    const auto too_many = [&](std::size_t n) { return most != 0 && n > most; };

    // BOTH SHAPES AN ID ARRIVES IN. `nesneler=1 2` parses to an IdList, but a lone
    // `nesne=1` parses to an Int — there is nothing in "1" to say it is a list —
    // and reading only `as_ids()` therefore saw an empty selection and went on to
    // ask for one, with the object the script had already named sitting right
    // there. Every caller that used to unpack this by hand now gets it here.
    const Value arg = ctx.argument(param);
    out             = arg.as_ids();
    if (out.empty() && arg.kind() == Value::Kind::Int) out.push_back(arg.as_int());

    if (!out.empty()) {
        if (too_many(out.size()))
            co_return refuse("'" + param + "' en fazla " + std::to_string(most) + " nesne alır; " +
                             std::to_string(out.size()) + " verildi.");
        co_return true;
    }

    // The live selection, copied out as IDS: a replay must act on the same objects
    // whatever happens to be highlighted then (model.md R43). Not recorded here —
    // every caller records the parameter it actually resolved.
    for (core::EntityKey k : ctx.session().bus().selection().keys())
        out.push_back(static_cast<std::int64_t>(core::raw(k)));

    if (!out.empty()) {
        if (too_many(out.size()))
            co_return refuse("Bir seferde en fazla " + std::to_string(most) + " nesne; " +
                             std::to_string(out.size()) + " nesne seçili.");
        co_return true;
    }

    auto picked = co_await ctx.objects(param, std::move(message));
    if (!picked || picked->empty()) {
        // Said for BOTH clients, without asking which one this is (Article 1.2):
        // a user who pressed Esc reads it as confirmation, and a script that
        // forgot its argument reads it as the reason nothing happened.
        co_return refuse("İşlem yapılacak nesne yok: seçim boş ve '" + param + "' verilmedi.");
    }

    if (too_many(picked->size()))
        co_return refuse("Bir seferde en fazla " + std::to_string(most) + " nesne; " +
                         std::to_string(picked->size()) + " nesne seçildi.");

    out = std::move(*picked);
    co_return true;
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

} // namespace kentos::command
