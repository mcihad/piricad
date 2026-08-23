// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/bus.hpp"

#include <cmath>

#include "piricad/command/log.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/core/text.hpp"

#include <chrono>

namespace piricad::command {

core::Settings* Bus::store_for(std::string_view id) noexcept
{
    return const_cast<core::Settings*>(std::as_const(*this).store_for(id));
}

const core::Settings* Bus::store_for(std::string_view id) const noexcept
{
    const core::SettingCatalog& catalogue = core::builtin_settings();

    const std::uint32_t index = catalogue.find(id);
    if (index == core::kNoSetting) return nullptr;

    switch (catalogue.at(index).scope) {
    case core::SettingScope::App: return &app_settings_;
    case core::SettingScope::Project: return &project_settings_;
    case core::SettingScope::Session: return &session_settings_;
    }
    return nullptr;
}

core::SettingValue Bus::setting(std::string_view id) const
{
    const core::Settings* store = store_for(id);
    return store == nullptr ? core::SettingValue{} : store->get(id);
}

core::Result<core::SettingChange> Bus::set_setting(std::string_view id,
                                                   const core::SettingValue& value)
{
    core::Settings* store = store_for(id);
    if (store == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Bilinmeyen ayar: '" + std::string(id) +
                             "'. Tanımlı ayarları AYAR ya da TERCİH ile listeleyin.");

    auto changed = store->set(id, value);
    if (!changed) return changed;

    if (on_settings_changed) {
        const std::uint32_t index = core::builtin_settings().find(id);
        on_settings_changed(core::builtin_settings().at(index).scope);
    }
    return changed;
}

namespace {

using core::ErrorCode;

std::int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/// Folds the token stream of one command line into the command's declared
/// parameters. The CLI, macro playback and the script engine share this, because
/// they share the grammar (piricad.md §3).
core::Result<Args> bind_tokens(const CommandSpec& spec, const std::vector<Token>& tokens)
{
    Args args;
    core::Point2 last{};
    bool have_last = false;

    // Positional parameters are consumed in declaration order; a keyword argument
    // may fill any parameter out of order.
    std::size_t positional = 0;

    const auto param_by_name = [&](std::string_view name) -> const Param* {
        for (const auto& p : spec.params)
            if (core::turkish_iequals(p.name, name)) return &p;
        return nullptr;
    };

    // A LIST parameter accumulates; a scalar one is replaced. Both point lists
    // and selections are lists, and before this only point lists accumulated —
    // `SİL nesneler=1 nesneler=2` silently kept the last id and deleted one
    // object where the user asked for two. Dropping an argument on the floor is
    // exactly what `.claude/command.md` P15 forbids.
    const auto append = [&](const Param& p, Value v) {
        if (p.kind == ParamKind::PointList) {
            Value::Points pts = args.get(p.name).as_points();
            for (auto pt : v.as_points())
                pts.push_back(pt);
            args.set(p.name, Value::points(std::move(pts)));
        } else if (p.kind == ParamKind::Selection ||
                   (p.kind == ParamKind::Integer && p.arity.max > 1)) {
            // An Integer parameter whose arity allows more than one IS a list, and
            // it accumulates for the same reason a selection does. `ALAN … bolum=4
            // bolum=4` means two rings; keeping only the last one would draw the
            // boundary and silently drop the courtyard, which puts the wrong area
            // on the parcel.
            Value::Ints ids = args.get(p.name).as_ids();
            if (v.kind() == Value::Kind::Int)
                ids.push_back(v.as_int());
            else
                for (auto id : v.as_ids())
                    ids.push_back(id);
            args.set(p.name, Value::ids(std::move(ids)));
        } else {
            args.set(p.name, std::move(v));
        }
    };

    const auto value_from_token = [&](const Param& p, const Token& t) -> core::Result<Value> {
        if (is_coordinate(t)) {
            auto pt = resolve_point(t, last);
            if (!pt) return pt.error();
            last      = pt.value();
            have_last = true;
            return p.kind == ParamKind::PointList ? Value::points({pt.value()})
                                                  : Value::point(pt.value());
        }
        switch (p.kind) {
        case ParamKind::Number:
            if (t.kind == Token::Kind::Number) return Value::number(t.a);
            break;
        case ParamKind::Integer:
            if (t.kind == Token::Kind::Number)
                return Value::integer(static_cast<std::int64_t>(t.a >= 0 ? t.a + 0.5 : t.a - 0.5));
            break;
        case ParamKind::Bool:
            if (t.kind == Token::Kind::Number) return Value::boolean(t.a != 0.0);
            if (t.kind == Token::Kind::Word) {
                const std::string f = core::turkish_upper(t.word);
                if (f == "EVET" || f == "YES" || f == "TRUE" || f == "1")
                    return Value::boolean(true);
                if (f == "HAYIR" || f == "NO" || f == "FALSE" || f == "0")
                    return Value::boolean(false);
            }
            break;
        case ParamKind::Text:
            if (t.kind == Token::Kind::Text) return Value::text(t.text);
            if (t.kind == Token::Kind::Word) return Value::text(t.word);
            if (t.kind == Token::Kind::Number) {
                // `ÖZNİTELİK ada_no 1 1234` must put "1234" in the cell, not
                // "1234.000000". std::to_string on a double formats six decimals
                // unconditionally, so a whole number typed by a user arrived as a
                // decimal it never typed, and a schema expecting an integer then
                // refused the user's own input. An integral value is written
                // without a fractional part; a genuine decimal keeps its digits
                // but loses the trailing zeros that carry no information.
                if (t.a == std::floor(t.a) && std::abs(t.a) < 9.0e15)
                    return Value::text(std::to_string(static_cast<std::int64_t>(t.a)));
                std::string out = std::to_string(t.a);
                while (out.size() > 1 && out.back() == '0')
                    out.pop_back();
                if (!out.empty() && out.back() == '.') out.pop_back();
                return Value::text(out);
            }
            break;
        case ParamKind::Selection:
            if (t.kind == Token::Kind::Number) return Value::ids({static_cast<std::int64_t>(t.a)});
            break;
        case ParamKind::Point:
        case ParamKind::PointList: break;
        }
        return core::err(ErrorCode::ParseError, "'" + spec.id + "': '" + p.name + "' parametresi " +
                                                    param_kind_label(p.kind) +
                                                    " bekliyor. Girilen: " + describe(t));
    };

    for (const auto& t : tokens) {
        if (t.kind == Token::Kind::KeyValue) {
            const Param* p = param_by_name(t.word);
            if (!p) {
                std::string known;
                for (const auto& q : spec.params) {
                    if (!known.empty()) known += ", ";
                    known += q.name;
                }
                return core::err(
                    ErrorCode::ParseError,
                    "'" + spec.id + "': bilinmeyen parametre '" + t.word +
                        "'. Tanımlı parametreler: " + (known.empty() ? "(yok)" : known));
            }
            if (t.nested.empty())
                return core::err(ErrorCode::ParseError,
                                 "'" + t.word + "=' anahtarına değer verilmemiş.");
            auto v = value_from_token(*p, t.nested.front());
            if (!v) return v.error();
            append(*p, std::move(v.value()));
            continue;
        }

        // Positional: fill the current parameter until its arity is satisfied.
        while (positional < spec.params.size()) {
            const Param& p         = spec.params[positional];
            const std::size_t have = p.kind == ParamKind::PointList
                                         ? args.get(p.name).as_points().size()
                                         : (args.has(p.name) ? 1u : 0u);
            if (p.arity.max != 0xFFFFFFFFu && have >= p.arity.max) {
                ++positional;
                continue;
            }
            break;
        }
        if (positional >= spec.params.size()) {
            return core::err(ErrorCode::ParseError,
                             "'" + spec.id +
                                 "' daha fazla argüman almıyor. Fazlalık: " + describe(t));
        }

        const Param& p = spec.params[positional];
        auto v         = value_from_token(p, t);
        if (!v) return v.error();
        append(p, std::move(v.value()));
    }

    (void)have_last;
    return args;
}

} // namespace

Bus::Bus(core::Document& doc, Registry& reg, Journal& journal, UndoStack& undo)
    : doc_(doc), reg_(reg), journal_(journal), undo_(undo)
{}

void Bus::echo(std::string_view message) const
{
    if (on_echo)
        on_echo(message);
    else
        log_info(message);
}

core::Result<DispatchResult> Bus::dispatch(const Invocation& inv)
{
    const CommandSpec* spec = reg_.resolve(inv.name);
    if (!spec)
        return core::err(ErrorCode::NotFound, "Bilinmeyen komut: '" + inv.name +
                                                  "'. YARDIM yazarak komut listesini görün.");

    // A two-element JSON array is genuinely ambiguous: `[485320, 4310220]` is a
    // point and `[1, 2]` is a pair of object ids. `Value::from_json` has no spec
    // and reads both as a point, so a script that wrote `{"nesneler": [1, 2]}`
    // was told its ids were the wrong type. The SPEC settles it, and this is the
    // first place that has one.
    //
    // The copy happens only when the ambiguous form is actually present, so the
    // §10.4 dispatch budget pays a parameter-kind comparison and nothing else.
    Args repaired;
    const Args* args = &inv.args;
    for (const auto& p : spec->params) {
        // Selection is a list of integers, and so is an Integer parameter whose
        // arity allows more than one. Both are written `[4, 4]` in a script, and
        // both are read as a Point before a spec is in hand.
        const bool int_list = p.kind == ParamKind::Integer && p.arity.max > 1;
        if (p.kind != ParamKind::Selection && !int_list) continue;
        const Value* v = args->find(p.name);
        if (!v || v->kind() != Value::Kind::Point) continue;

        if (args != &repaired) repaired = inv.args;
        const core::Point2 pair = v->as_point();
        repaired.set(p.name, Value::ids({pair.x, pair.y}));
        args = &repaired;
    }

    // Validation runs on the bus, for every client, with no opt-out (§2.6).
    ValidationRequest req{*spec, *args, inv.origin, doc_};
    if (auto st = validator_.run(req); !st) return st.error();

    const bool borrow = batch_ && spec->undo == UndoPolicy::SingleTransaction;

    Session session(*this, *spec, std::make_unique<ArgInputSource>(*args, inv.origin),
                    borrow ? nullptr
                           : std::make_unique<Transaction>(
                                 doc_, spec->summary.empty() ? spec->id : spec->summary),
                    borrow ? batch_.get() : nullptr);

    auto result = run_to_completion(session);
    if (result && borrow) ++batch_commands_;
    return result;
}

core::Result<DispatchResult> Bus::execute_line(std::string_view line, Origin origin)
{
    auto parsed = parse_line(line);
    if (!parsed) return parsed.error();

    const CommandSpec* spec = reg_.resolve(parsed.value().command);
    if (!spec)
        return core::err(ErrorCode::NotFound, "Bilinmeyen komut: '" + parsed.value().command +
                                                  "'. YARDIM yazarak komut listesini görün.");

    auto args = bind_tokens(*spec, parsed.value().tokens);
    if (!args) return args.error();

    return dispatch(Invocation{spec->id, std::move(args.value()), origin});
}

core::Result<std::unique_ptr<Session>> Bus::begin_interactive(std::string_view name)
{
    const CommandSpec* spec = reg_.resolve(name);
    if (!spec)
        return core::err(ErrorCode::NotFound, "Bilinmeyen komut: '" + std::string(name) + "'");

    auto tx = std::make_unique<Transaction>(doc_, spec->summary.empty() ? spec->id : spec->summary);
    auto session = std::make_unique<Session>(
        *this, *spec, std::make_unique<InteractiveInputSource>(), std::move(tx));
    session->start();
    return session;
}

core::Result<DispatchResult> Bus::run_to_completion(Session& session)
{
    session.start();

    if (!session.finished())
        return core::err(ErrorCode::Internal,
                         "'" + session.spec().id +
                             "' komutu bütün argümanlar verilmiş olmasına rağmen girdi bekledi.");

    return finish(session);
}

core::Result<DispatchResult> Bus::finish(Session& session)
{
    const CommandSpec& spec = session.spec();

    if (session.state() == SessionState::Failed) {
        session.transaction().rollback();
        return session.error();
    }

    DispatchResult result;
    result.command_id = spec.id;
    result.label      = spec.summary.empty() ? spec.id : spec.summary;

    const bool read_only  = has_flag(spec.flags, Flags::ReadOnly);
    const std::size_t ops = session.transaction().size();

    // ESC before anything was drawn: nothing happened, so nothing is validated,
    // journalled or undoable. A command that merely produced no geometry is NOT
    // this case — it still validates and still appears in the journal, because a
    // state change such as the active layer must survive a replay.
    //
    // ReadOnly commands are in this case too, and used not to be. Pressing ESC at
    // the file dialog of AÇ or FARKLIKAYDET left `resolved()` empty, so the
    // post-run validation below reported "'dosya' parametresi 1 değer istiyor" —
    // a validation error for a user who simply changed their mind. Cancelling is
    // not a failure, whatever the command does to the document.
    if (ops == 0 && session.state() == SessionState::Cancelled) {
        result.mutated = false;
        result.message = "İptal edildi";
        if (on_command_finished) on_command_finished(result);
        return result;
    }

    // Post-run validation over the values the command actually resolved. This is
    // what catches a bad point that the AI or a script produced mid-run (§2.6).
    ValidationRequest req{spec, session.resolved(), session.input().origin(), doc_};
    if (auto st = validator_.run(req); !st) {
        session.transaction().rollback(); // no partial application, ever (§2.5)
        return st.error();
    }

    result.ops     = session.owns_transaction() ? ops : 0;
    result.mutated = ops > 0;

    if (result.mutated && spec.undo == UndoPolicy::SingleTransaction &&
        session.owns_transaction()) {
        undo_.push(UndoEntry{result.label, session.transaction().release()});
    }

    if (!read_only) journal_entry(session);

    if (result.mutated && on_document_changed) on_document_changed();
    if (on_command_finished) on_command_finished(result);
    return result;
}

void Bus::journal_entry(const Session& session)
{
    JournalEntry e;
    e.command_id   = session.spec().id;
    e.args         = session.resolved();
    e.origin       = session.input().origin();
    e.crs          = doc_.crs().id();
    e.timestamp_ms = now_ms();

    if (const core::Layer* l = doc_.layer(active_layer_)) e.layer = l->name;
    journal_.append(std::move(e));
}

core::Status Bus::begin_batch(std::string label)
{
    if (batch_)
        return core::err(ErrorCode::InvalidArgument, "Toplu iş zaten açık: '" + batch_label_ + "'");

    batch_label_    = std::move(label);
    batch_commands_ = 0;
    batch_          = std::make_unique<Transaction>(doc_, batch_label_);
    return core::ok();
}

core::Result<DispatchResult> Bus::end_batch()
{
    if (!batch_) return core::err(ErrorCode::InvalidArgument, "Açık toplu iş yok");

    DispatchResult result;
    result.command_id = "core.batch";
    result.label      = batch_label_;
    result.ops        = batch_->size();
    result.mutated    = result.ops > 0;

    if (result.mutated) undo_.push(UndoEntry{batch_label_, batch_->release()});

    batch_.reset();
    result.message =
        batch_label_ + ": " + std::to_string(batch_commands_) + " komut, tek geri alma adımı";

    if (result.mutated && on_document_changed) on_document_changed();
    return result;
}

} // namespace piricad::command
