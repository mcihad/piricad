// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/bus.hpp"

#include <cmath>

#include "kentos_cad/command/log.hpp"
#include "kentos_cad/command/parser.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/text.hpp"

#include <chrono>

namespace kentos::command {

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

core::AngleConvention Bus::angle_convention() const
{
    return core::AngleConvention{
        .unit = core::angle_unit_from_setting(setting("core.aci.birim").as_enum()),
        .rule = core::angle_rule_from_setting(setting("core.aci.kural").as_enum()),
    };
}

ResolveContext Bus::resolve_context() const
{
    return ResolveContext{
        .convention  = angle_convention(),
        .named_point = [this](std::int64_t number) { return numbered_point(number); },
    };
}

std::optional<core::Point2> Bus::numbered_point(std::int64_t number) const
{
    const core::AttrId column = doc_.attributes().find("nokta_no");
    if (column == core::kNoAttr) return std::nullopt;

    const std::string wanted = std::to_string(number);
    for (core::EntityId e = 0; e < doc_.entities().size(); ++e) {
        if (!doc_.alive(e) || doc_.entities().kind[e] != core::kPointKind) continue;

        const auto cell = doc_.attribute(column, e);
        if (!cell || !cell.value().present || cell.value().text != wanted) continue;

        const core::RingSpan span = doc_.geometry().rings_of(doc_.entities().slot[e]);
        if (span.count == 0) continue;
        const auto xs = doc_.geometry().ring_xs(span.first);
        const auto ys = doc_.geometry().ring_ys(span.first);
        if (xs.empty()) continue;
        return core::Point2{xs[0], ys[0]};
    }
    return std::nullopt;
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
/// they share the grammar (kentoscad.md §3).
core::Result<Args> bind_tokens(const CommandSpec& spec, const std::vector<Token>& tokens,
                               const ResolveContext& ctx)
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
        // A RETIRED NAME (`Param::was`), so that a script or a journal line typed
        // before a parameter was renamed still says what it said. The value is
        // bound under the CURRENT name, so the body and the journal it writes see
        // one spelling and only one leaves this program (Article 1.4).
        for (const auto& p : spec.params)
            if (!p.was.empty() && core::turkish_iequals(p.was, name)) return &p;
        return nullptr;
    };

    // Which spelling each parameter was written with. Two spellings of one
    // argument in one line is a caller that does not know which it means, and
    // the scalar case would silently keep the last (command.md P15).
    std::vector<std::pair<const Param*, std::string>> spelled;

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
        } else if (p.kind == ParamKind::Text && p.arity.max > 1) {
            // A TEXT PARAMETER THAT TAKES MORE THAN ONE IS A LIST, and it
            // accumulates for the reason a selection does: `katmanlar=parsel
            // katmanlar=bina` means two layers, and keeping the last one would
            // draw one of them and silently drop the other (command.md P15).
            Value::Texts words = args.get(p.name).as_texts();
            for (const std::string& one : v.as_texts())
                words.push_back(one);
            args.set(p.name, Value::texts(std::move(words)));
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
            auto pt = resolve_point(t, last, ctx);
            if (!pt) return pt.error();
            last      = pt.value();
            have_last = true;
            return p.kind == ParamKind::PointList ? Value::points({pt.value()})
                                                  : Value::point(pt.value());
        }
        // QUOTING DELIMITS, IT DOES NOT RETYPE. `ÖLÇEK "500"` means what
        // `ÖLÇEK 500` means, and `KATMAN gorunur="evet"` means what
        // `gorunur=evet` means. A user quotes to keep a space, a comma or an `=`
        // out of the tokeniser's hands, and being told the value is now the wrong
        // KIND is a trap with no lesson in it.
        //
        // The number is read by `evaluate_expression`, which is the one grammar
        // this project has (CLAUDE.md 5.11) — not a second numeric parse that
        // could disagree with it about what `1e3` means.
        const auto numeric = [&t]() -> core::Result<double> {
            if (t.kind == Token::Kind::Number) return t.a;
            if (t.kind == Token::Kind::Text) return evaluate_expression(t.text);
            return core::err(ErrorCode::ParseError, "sayı değil");
        };

        switch (p.kind) {
        case ParamKind::Number:
            if (auto n = numeric(); n) return Value::number(n.value());
            break;
        case ParamKind::Integer:
            if (auto n = numeric(); n)
                return Value::integer(
                    static_cast<std::int64_t>(n.value() >= 0 ? n.value() + 0.5 : n.value() - 0.5));
            break;
        case ParamKind::Bool: {
            if (t.kind == Token::Kind::Number) return Value::boolean(t.a != 0.0);
            if (t.kind == Token::Kind::Word || t.kind == Token::Kind::Text) {
                // A declared keyword, so it folds like one: `hayır` and its ASCII
                // spelling `hayir` are the same word to a user and were not the
                // same word to `turkish_upper`, which raises the two i's apart.
                const std::string f =
                    core::turkish_fold_key(t.kind == Token::Kind::Word ? t.word : t.text);
                if (f == "EVET" || f == "YES" || f == "TRUE" || f == "1")
                    return Value::boolean(true);
                if (f == "HAYIR" || f == "NO" || f == "FALSE" || f == "0")
                    return Value::boolean(false);
            }
            break;
        }
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
            for (const auto& [seen, word] : spelled)
                if (seen == p && !core::turkish_iequals(word, t.word))
                    return core::err(ErrorCode::ParseError,
                                     "'" + spec.id + "': '" + word + "' ve '" + t.word +
                                         "' aynı parametrenin iki adı; ikisi birden verilmez. "
                                         "Yeni adı '" +
                                         p->name + "'.");
            spelled.emplace_back(p, t.word);

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

std::string redact_conninfo(std::string_view conninfo)
{
    static constexpr std::string_view kKey    = "password=";
    static constexpr std::string_view kHidden = "***";

    std::string out;
    out.reserve(conninfo.size());

    // ---- the URI form: scheme://user:SECRET@host/... ----
    //
    // The password sits between the first `:` after the scheme and the LAST `@`
    // before the authority ends. Last, not first: a password may itself contain
    // an `@`, and libpq's own parser takes the final one.
    if (const std::size_t scheme = conninfo.find("://"); scheme != std::string_view::npos) {
        const std::size_t authority = scheme + 3;
        std::size_t stop            = conninfo.find('/', authority);
        if (stop == std::string_view::npos) stop = conninfo.size();

        const std::size_t at = conninfo.rfind('@', stop);
        if (at != std::string_view::npos && at > authority) {
            if (const std::size_t colon = conninfo.find(':', authority);
                colon != std::string_view::npos && colon < at) {
                out.append(conninfo.substr(0, colon + 1));
                out.append(kHidden);
                out.append(conninfo.substr(at));
                return out;
            }
        }
    }

    // ---- the keyword form: `password=SECRET` or `password='SEC RET'` ----
    for (std::size_t i = 0; i < conninfo.size();) {
        // Only at a field boundary: a `dbname=my_password=thing` is one value and
        // must not be cut in half.
        const bool boundary = i == 0 || conninfo[i - 1] == ' ';
        if (!boundary || conninfo.compare(i, kKey.size(), kKey) != 0) {
            out += conninfo[i++];
            continue;
        }

        out.append(kKey);
        out.append(kHidden);
        i += kKey.size();

        if (i < conninfo.size() && conninfo[i] == '\'') {
            ++i;
            while (i < conninfo.size() && conninfo[i] != '\'')
                ++i;
            if (i < conninfo.size()) ++i;
        } else {
            while (i < conninfo.size() && conninfo[i] != ' ')
                ++i;
        }
    }
    return out;
}

void Bus::echo(std::string_view message) const
{
    // TEED, NOT REROUTED: the transcript still gets every line. The sink exists
    // so the CALLER can read what its own dispatch said (DispatchResult::lines).
    if (echo_sink_ != nullptr) echo_sink_->emplace_back(message);
    if (on_echo)
        on_echo(message);
    else
        log_info(message);
}

Bus::EchoCapture::EchoCapture(const Bus& bus, std::vector<std::string>& sink)
    : bus_(bus), previous_(bus.echo_sink_)
{
    bus_.echo_sink_ = &sink;
}

Bus::EchoCapture::~EchoCapture()
{
    bus_.echo_sink_ = previous_;
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

    // A RETIRED ARGUMENT NAME, read here and nowhere else.
    //
    // `Param::was` names what a parameter was called before it was renamed. A
    // journal line or a script written by an older build carries that name, and
    // Article 1.4 says a command invocation is data that round-trips — so the old
    // word is understood, moved onto the current one, and never written back.
    // Giving both at once is a refusal rather than a guess: two spellings of one
    // argument in one line is a caller that does not know which it means.
    for (const auto& p : spec->params) {
        if (p.was.empty()) continue;
        const Value* old_value = args->find(p.was);
        if (old_value == nullptr) continue;
        if (args->find(p.name) != nullptr)
            return core::err(ErrorCode::ValidationFailed,
                             "'" + spec->id + "': '" + p.was + "' ve '" + p.name +
                                 "' aynı parametrenin iki adı; ikisi birden verilmez. Yeni adı '" +
                                 p.name + "'.");
        if (args != &repaired) repaired = inv.args;
        repaired.rename(p.was, p.name);
        args = &repaired;
    }

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

    // TEXT FOR A POINT IS READ BY THE ONE GRAMMAR. A JSON script may write a
    // coordinate the way the command line does — `"0,0"`, `"@100<50"`, in
    // metres — and it means exactly what it means typed (CLAUDE.md 5.11, Article
    // 1.2): read by `parse_point` under the session's angle convention, chained
    // from one point to the next in declaration order as a typed line is, and
    // turned into points BEFORE validation so the validator and the body see
    // what they always saw. A word that is not a coordinate is refused with the
    // parser's own message. The AI and MCP layers never reach this branch: both
    // refuse a textual position while it is still JSON (CLAUDE.md 5.8, ai.md
    // R10), so nothing here lets a coordinate originate in model text.
    {
        core::Point2 last{};
        const ResolveContext ctx = resolve_context();
        for (const auto& p : spec->params) {
            if (p.kind != ParamKind::Point && p.kind != ParamKind::PointList) continue;
            const Value* v = args->find(p.name);
            if (v == nullptr) continue;
            if (v->kind() == Value::Kind::Point) {
                last = v->as_point();
                continue;
            }
            if (v->kind() == Value::Kind::PointList) {
                if (!v->as_points().empty()) last = v->as_points().back();
                continue;
            }
            if (v->kind() != Value::Kind::Text && v->kind() != Value::Kind::TextList) continue;

            Value::Points points;
            const auto read = [&](const std::string& text) -> core::Status {
                auto pt = parse_point(text, last, ctx);
                if (!pt) return pt.error();
                last = pt.value();
                points.push_back(pt.value());
                return core::ok();
            };
            if (v->kind() == Value::Kind::Text) {
                if (const auto st = read(v->as_text()); !st) return st.error();
            } else {
                for (const std::string& text : v->as_texts())
                    if (const auto st = read(text); !st) return st.error();
            }

            if (args != &repaired) repaired = inv.args;
            repaired.set(p.name, p.kind == ParamKind::Point && points.size() == 1
                                     ? Value::point(points.front())
                                     : Value::points(std::move(points)));
            args = &repaired;
        }
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

    std::vector<std::string> said;
    core::Result<DispatchResult> result = [&] {
        const EchoCapture capture(*this, said);
        return run_to_completion(session);
    }();
    if (result) {
        result.value().lines  = std::move(said);
        result.value().report = session.report();
    }
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

    auto args = bind_tokens(*spec, parsed.value().tokens, resolve_context());
    if (!args) return args.error();

    return dispatch(Invocation{spec->id, std::move(args.value()), origin});
}

core::Result<std::unique_ptr<Session>> Bus::begin_interactive(std::string_view line, Origin origin)
{
    // THE SAME PARSER THE TYPED LINE GOES THROUGH (CLAUDE.md 5.11). A bare name
    // parses to itself with no tokens, so every existing caller is unchanged; a
    // button that needs to say `SEÇ mod=KUTU` now can, and it means there exactly
    // what it means typed. Resolving the whole string as a name — which is what
    // this did — could only ever start a command that took no arguments, so a
    // GUI button was a strictly weaker client than the command line. Article 1.2
    // does not allow that ranking to exist in either direction.
    auto parsed = parse_line(line);
    if (!parsed) return parsed.error();

    const CommandSpec* spec = reg_.resolve(parsed.value().command);
    if (!spec)
        return core::err(ErrorCode::NotFound, "Bilinmeyen komut: '" + parsed.value().command +
                                                  "'. YARDIM yazarak komut listesini görün.");

    auto args = bind_tokens(*spec, parsed.value().tokens, resolve_context());
    if (!args) return args.error();

    auto tx = std::make_unique<Transaction>(doc_, spec->summary.empty() ? spec->id : spec->summary);
    auto session = std::make_unique<Session>(
        *this, *spec, std::make_unique<InteractiveInputSource>(std::move(args.value()), origin),
        std::move(tx));
    // The caller keeps this session and resumes it later, so a job it awaits may
    // be hosted (job.hpp). `dispatch` never sets this: it finishes in one call.
    session->set_client_driven(true);
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

    // A worker still owns part of this command. Finishing now would commit a
    // transaction the read has not yet filled; the host finishes it when the
    // job returns.
    if (session.state() == SessionState::Working)
        return core::err(ErrorCode::InvalidArgument,
                         "'" + spec.id +
                             "' komutu hâlâ çalışıyor; bitmesini bekleyin ya da "
                             "durdurun.");

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
    //
    // A BODY THAT RESOLVED NOTHING AND WROTE NOTHING DECLINED; it did not fail
    // validation. `BÖL` with two objects selected says "Bir seferde tek çizgi
    // bölünür" and returns before it ever asks for the split point — and the check
    // below then reported "zorunlu 'nokta' parametresi eksik" immediately after
    // the sentence that had just explained the problem in the user's own language.
    // Two messages for one refusal, the second one addressed to a programmer.
    //
    // This is not a loosening: §2.6 says this pass validates THE VALUES THE
    // COMMAND RESOLVED, and with none resolved there is nothing here to check.
    // Everything else — the journal entry, the observers, the result — is
    // unchanged, so a read-only command that reports and resolves nothing still
    // appears in the record exactly as before.
    // EMPTY VALUES DO NOT COUNT AS RESOLVED. A command that asked for objects,
    // was handed more than it can take and cleared the parameter again (see
    // `want_objects`) has a key in `resolved()` holding nothing — it resolved no
    // value, which is exactly the case this skips.
    std::size_t settled = 0;
    for (const auto& [name, value] : session.resolved().items())
        if (!value.empty()) ++settled;

    const bool declined = ops == 0 && settled == 0;
    if (!declined) {
        ValidationRequest req{spec, session.resolved(), session.input().origin(), doc_};
        if (auto st = validator_.run(req); !st) {
            session.transaction().rollback(); // no partial application, ever (§2.5)
            return st.error();
        }
    }

    // WHAT FOLLOWS FROM THE EDIT lands in the same transaction: a caption
    // attached to a line the command moved is re-placed here, once, at commit
    // (core/attach.hpp). Not per frame, not by the client — by the bus, for every
    // client alike, so the GUI, the command line and a script end with one
    // document (Article 1.2). A source that was erased takes its dependents with
    // it, and that is said, because a deletion the user did not name is the one
    // thing here they should hear about.
    if (!read_only) {
        const auto followed = session.transaction().settle_attachments();
        if (followed.erased != 0 && on_echo)
            on_echo("Silinen nesnelere bağlı " + std::to_string(followed.erased) +
                    " nesne de silindi.");
    }

    result.ops = session.owns_transaction() ? session.transaction().size() : 0;
    // A borrowed transaction belongs to a batch. Its single visible mutation is
    // reported by end_batch(), not once per nested command, so the canvas and
    // layer panel repaint only after the whole edit is coherent.
    result.mutated =
        session.owns_transaction() && doc_.revision() != session.document_revision_at_start();

    if (result.mutated && spec.undo == UndoPolicy::SingleTransaction &&
        session.owns_transaction()) {
        undo_.push(UndoEntry{result.label, session.transaction().release()});
        // NAMED, NOT COUNTED. A client that has to say "undo what I just did"
        // cannot count stack entries: a person at the workstation may have drawn
        // something in between (TODOS C-03).
        result.undo_label = result.label;
    }

    // THE REVISION AFTER, so a client can compose its next call against what is
    // now there. Asking afterwards is a race — another client can edit in the
    // gap — and C-04 refuses a plan composed against a revision that has moved.
    result.revision = doc_.revision();
    result.outputs  = session.outputs();
    result.warnings = session.warnings();

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

void Bus::document_replaced()
{
    undo_.clear();
    active_layer_ = 0;
    selection_.clear();

    if (batch_) {
        // `release` hands the inverse record away and clears it; the vector is
        // dropped here on purpose. `rollback` would be the wrong verb — it would
        // replay those inverses against the document that has just arrived.
        (void)batch_->release();
        batch_revision_at_start_ = doc_.revision();
    }
}

core::Status Bus::begin_batch(std::string label)
{
    if (batch_)
        return core::err(ErrorCode::InvalidArgument, "Toplu iş zaten açık: '" + batch_label_ + "'");

    batch_label_             = std::move(label);
    batch_commands_          = 0;
    batch_revision_at_start_ = doc_.revision();
    batch_                   = std::make_unique<Transaction>(doc_, batch_label_);
    return core::ok();
}

core::Result<DispatchResult> Bus::end_batch()
{
    if (!batch_) return core::err(ErrorCode::InvalidArgument, "Açık toplu iş yok");

    // The last word on what the batch moved (see `finish`).
    (void)batch_->settle_attachments();

    DispatchResult result;
    result.command_id = "core.batch";
    result.label      = batch_label_;
    result.ops        = batch_->size();
    result.mutated    = doc_.revision() != batch_revision_at_start_;

    if (result.mutated) undo_.push(UndoEntry{batch_label_, batch_->release()});

    batch_.reset();
    batch_revision_at_start_ = 0;
    result.message =
        batch_label_ + ": " + std::to_string(batch_commands_) + " komut, tek geri alma adımı";

    if (result.mutated && on_document_changed) on_document_changed();
    return result;
}

void Bus::abort_batch()
{
    if (!batch_) return;

    batch_->rollback();
    batch_.reset();
    batch_commands_          = 0;
    batch_revision_at_start_ = 0;
}

} // namespace kentos::command
