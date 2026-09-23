// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/context.hpp"

#include "kentos_cad/command/aids.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/session.hpp"

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/units.hpp"

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

namespace {

/// The aids proper: object snap, grid, direction locks and tracking, on a point
/// or a run of them. Anything else passes through untouched.
Value snap_value(Session& session, const Prompt& prompt, Value v, bool up_front)
{
    // Only a point is aimed; a number, a name or a flag is typed exactly.
    if (v.kind() != Value::Kind::Point && v.kind() != Value::Kind::PointList) return v;

    Bus& bus = session.bus();
    // The aids THIS prompt takes (`aids_for`): dik mod has no say over a
    // rectangle's opposite corner.
    const AidSettings set = aids_for(bus.aid_settings(), prompt);

    const bool object_snap = set.snap_radius > 0 && (set.modes & core::SnapObjectMask) != 0;
    const bool grid        = set.grid_step > 0 && (set.modes & core::SnapGrid) != 0;
    // AIMED FROM THE ORIGIN only when the origin is a base (`Prompt::rubber_base`).
    const bool based     = aimed_from_origin(prompt);
    const bool direction = based && (set.ortho || set.normal_lock ||
                                     (set.polar_step > 0 && (set.modes & core::SnapPolar) != 0));
    // A TRACE NEEDS A MARK AND NOT A VIEW, so it is its own gate: with the
    // aperture at zero and every object mode off, a marked corner still tracks.
    const bool tracking = set.tracking_reach > 0 && (set.modes & core::SnapTracking) != 0 &&
                          !bus.tracking_marks().empty();
    if (!object_snap && !grid && !direction && !tracking) return v;

    const core::Document& doc = bus.document();

    // THE RUN'S OWN CORNERS for a single point ANSWERED at a run's prompt; a
    // whole list handed over at once, and a point that came with the invocation,
    // are resolved against the aids alone (`apply_input_aids`).
    const PendingRun run =
        v.kind() == Value::Kind::Point && !up_front ? pending_run(prompt) : PendingRun{};
    const auto resolve = [&](core::Point2 aim, bool has_base, core::Point2 base) {
        const core::SnapResult r =
            bus.aids().resolve(doc, set, aim, has_base, base, bus.tracking_marks(), run);
        bus.aids().remember(r);
        return r.point;
    };

    if (v.kind() == Value::Kind::Point)
        return Value::point(resolve(v.as_point(), based, prompt.rubber_origin));

    // A whole point list arrives when a script hands one over at once. Each point
    // is resolved against the one before it, exactly as an interactive run would,
    // so a scripted polyline and a drawn polyline agree vertex for vertex.
    Value::Points points = v.as_points();
    bool has_base        = based;
    core::Point2 base    = prompt.rubber_origin;

    for (auto& p : points) {
        p        = resolve(p, has_base, base);
        base     = p;
        has_base = true;
    }
    return Value::points(std::move(points));
}

} // namespace

Value apply_input_aids(Session& session, const Prompt& prompt, Value v, bool up_front)
{
    // A DISTANCE SHOWN RATHER THAN TYPED (`Prompt::pick_distance`): the click is
    // snapped like any other — a chamfer taken to a corner of the next parcel
    // lands exactly there — and what is handed on, and journalled, is its
    // distance from the prompt's origin in metres. A replay re-supplies the
    // number, which is what the command asked for.
    if (prompt.pick_distance && prompt.kind == ParamKind::Number &&
        v.kind() == Value::Kind::Point) {
        const core::Point2 at = snap_value(session, prompt, std::move(v), up_front).as_point();
        return Value::number(core::mm_to_metres(core::segment_length(prompt.rubber_origin, at)));
    }
    return snap_value(session, prompt, std::move(v), up_front);
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
    prompt.rubber_base     = o.rubber_base;
    prompt.rubber_shape    = o.rubber_shape;
    prompt.rubber_chain    = std::move(o.rubber_chain);
    prompt.rubber_payload  = std::move(o.rubber_payload);
    prompt.can_retract     = o.can_retract;
    return InputAwaiter<Point2>(session_, std::move(p), std::move(prompt), &to_point);
}

bool Context::took_back() noexcept
{
    return session_.take_retract();
}

InputAwaiter<double> Context::number(std::string param, std::string message, PointOptions o)
{
    Param p = Param::number(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Number, .param = param};
    prompt.has_rubber_band = o.rubber_band;
    prompt.rubber_origin   = o.rubber_origin;
    prompt.rubber_base     = o.rubber_base;
    prompt.rubber_shape    = o.rubber_shape;
    prompt.rubber_chain    = std::move(o.rubber_chain);
    prompt.rubber_payload  = std::move(o.rubber_payload);
    prompt.pick_distance   = o.pick_distance;
    return InputAwaiter<double>(session_, std::move(p), std::move(prompt), &to_number);
}

InputAwaiter<std::int64_t> Context::integer(std::string param, std::string message, PointOptions o)
{
    Param p = Param::integer(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message), .kind = ParamKind::Integer, .param = param};
    prompt.has_rubber_band = o.rubber_band;
    prompt.rubber_origin   = o.rubber_origin;
    prompt.rubber_base     = o.rubber_base;
    prompt.rubber_shape    = o.rubber_shape;
    prompt.rubber_chain    = std::move(o.rubber_chain);
    prompt.rubber_payload  = std::move(o.rubber_payload);
    return InputAwaiter<std::int64_t>(session_, std::move(p), std::move(prompt), &to_integer);
}

InputAwaiter<std::string> Context::text(std::string param, std::string message,
                                        std::vector<std::string> choices)
{
    Param p = Param::text(param, Arity::exactly(1));
    Prompt prompt{.message = std::move(message),
                  .kind    = ParamKind::Text,
                  .param   = param,
                  .choices = std::move(choices)};
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
                        std::vector<std::int64_t>& out, std::size_t most, std::string example)
{
    // A refusal must leave NOTHING behind, and it is an ERROR: the caller's
    // command fails with this sentence, which is what a script, an agent and
    // Python are told (TODOS F-01). The awaiter records whatever it resolved
    // under the parameter it was given, before the caller has judged the count,
    // so the record is cleared as well — a BÖL handed three lines and told "one
    // at a time" must not leave three ids sitting in `nesne` for anything that
    // reads the resolved bundle afterwards.
    const auto refuse = [&ctx, &param](std::string why) {
        ctx.record(param, Value{});
        ctx.refuse(core::ErrorCode::InvalidArgument, std::move(why));
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

    // A SELECTION TOO BIG FOR THE TOOL IS ASKED PAST, not refused. KIR, UZUNLUK
    // and BÖLÜMLE take one object; pressing one of them with two highlighted used
    // to answer "en fazla 1 nesne" and do nothing, so the button was dead until
    // the user went and cleared the selection by hand. Now the tool asks for the
    // one it wants and says why — and a client that cannot point (a script, an
    // agent) is told exactly what it was told before, because it cannot answer
    // the question and the refusal below is the same sentence.
    std::size_t crowded = 0;
    if (!out.empty()) {
        if (!too_many(out.size())) co_return true;
        crowded = out.size();
        out.clear();
        message = std::to_string(crowded) + " nesne seçili; bu araç bir seferde " +
                  std::to_string(most) + " nesneyle çalışır. " + message;
    }

    auto picked = co_await ctx.objects(param, std::move(message));
    if (crowded != 0 && (!picked || picked->empty()))
        co_return refuse("Bir seferde en fazla " + std::to_string(most) + " nesne; " +
                         std::to_string(crowded) + " nesne seçili.");
    if (!picked || picked->empty()) {
        // Said for BOTH clients, without asking which one this is (Article 1.2):
        // a script that forgot its argument is told why nothing happened, and a
        // user who confirmed an empty pick reads the same sentence. A user who
        // pressed Esc is told "İptal edildi" instead, by the bus — the cancel
        // settles the session after this line runs, and that is the truth about
        // what they did.
        co_return refuse("İşlem yapılacak nesne yok: seçim boş ve '" + param + "' verilmedi." +
                         (example.empty() ? std::string{} : "\n  Örnek: " + example));
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

void Context::mark(const MeasureMark& m) const
{
    if (const auto& hook = session_.bus().on_measure_mark) hook(m);
}

void Context::refuse(core::ErrorCode code, std::string message) const
{
    refuse(core::Error{code, std::move(message)});
}

void Context::refuse(core::Error error) const
{
    session_.fail(std::move(error));
}

void Context::record(std::string param, Value v)
{
    session_.record(std::move(param), std::move(v));
}

void Context::report(core::Json data) const
{
    session_.set_report(std::move(data));
}

void Context::wrote(std::string path) const
{
    session_.add_output(std::move(path));
}

void Context::warn(std::string note) const
{
    session_.add_warning(std::move(note));
}

} // namespace kentos::command
