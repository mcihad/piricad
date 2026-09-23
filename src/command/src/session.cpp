// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/session.hpp"

#include "kentos_cad/command/job.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"

#include "kentos_cad/core/text.hpp"

namespace kentos::command {

bool asks_retract(const Registry& registry, std::string_view line)
{
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
        line.remove_prefix(1);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        line.remove_suffix(1);
    if (line.empty() || line.find_first_of(" \t") != std::string_view::npos) return false;

    const std::string word = core::turkish_fold_key(line);
    if (word == core::turkish_fold_key("GERİ") || word == core::turkish_fold_key("G")) return true;
    const CommandSpec* spec = registry.resolve(line);
    return spec != nullptr && spec->id == "core.undo";
}

std::string rearm_line(const Registry& registry, std::string_view line)
{
    // The words of the line as typed, a quoted caption kept whole.
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
        if (i >= line.size()) break;
        const std::size_t from = i;
        bool quoted            = false;
        while (i < line.size() && (quoted || (line[i] != ' ' && line[i] != '\t'))) {
            if (line[i] == '"') quoted = !quoted;
            ++i;
        }
        words.push_back(line.substr(from, i - from));
    }
    if (words.empty()) return {};
    const CommandSpec* spec = registry.resolve(words.front());
    if (spec == nullptr || spec->names.empty()) return {};

    // THE PRIMARY NAME, so the line matches the button that carries the same
    // method (`DR yontem=3n` is the tool `DAİRE yontem=3n`).
    std::string out = spec->names.front();
    for (std::size_t w = 1; w < words.size(); ++w) {
        const std::string_view word = words[w];
        const std::size_t eq        = word.find('=');
        if (eq == std::string_view::npos || eq == 0) continue; ///< positional: a place
        const std::string key = core::turkish_fold_key(word.substr(0, eq));
        for (const Param& p : spec->params) {
            if (core::turkish_fold_key(p.name) != key) continue;
            if (p.kind == ParamKind::Point || p.kind == ParamKind::PointList ||
                p.kind == ParamKind::Selection)
                break;
            out += ' ';
            out += p.name;
            out += word.substr(eq);
            break;
        }
    }
    return out;
}

const char* session_state_name(SessionState s)
{
    switch (s) {
    case SessionState::Ready: return "ready";
    case SessionState::Running: return "running";
    case SessionState::Waiting: return "waiting";
    case SessionState::Working: return "working";
    case SessionState::Completed: return "completed";
    case SessionState::Cancelled: return "cancelled";
    case SessionState::Failed: return "failed";
    }
    return "?";
}

Session::Session(Bus& bus, const CommandSpec& spec, std::unique_ptr<InputSource> input,
                 std::unique_ptr<Transaction> owned_tx, Transaction* borrowed_tx)
    : bus_(bus), spec_(&spec), input_(std::move(input)), owned_tx_(std::move(owned_tx))
{
    tx_                         = owned_tx_ ? owned_tx_.get() : borrowed_tx;
    ctx_                        = std::make_unique<Context>(*this, *tx_, bus.document());
    document_revision_at_start_ = bus.document().revision();
    transaction_mark_           = tx_->size();

    // Start from whatever the client supplied up front. A command that answers a
    // prompt overwrites the entry; a command that reads an argument directly
    // leaves it in place. Either way the journal records the effective bundle,
    // which is what makes a replay reproduce the run (kentoscad.md §2.2).
    if (const Args* preset = input_->preset()) resolved_ = *preset;
}

Session::~Session() = default;

bool Session::finished() const noexcept
{
    return state_ == SessionState::Completed || state_ == SessionState::Cancelled ||
           state_ == SessionState::Failed;
}

void Session::start()
{
    if (state_ != SessionState::Ready) return;
    state_ = SessionState::Running;

    task_ = spec_->run(*ctx_);
    resume_once();
}

void Session::resume_once()
{
    try {
        task_.resume();
    } catch (const std::exception& e) {
        fail(core::err(core::ErrorCode::Internal,
                       std::string("'") + spec_->id + "' komutu istisna fırlattı: " + e.what()));
        return;
    } catch (...) {
        fail(core::err(core::ErrorCode::Internal,
                       std::string("'") + spec_->id + "' komutu bilinmeyen bir istisna fırlattı."));
        return;
    }

    if (task_.done()) {
        if (state_ == SessionState::Running || state_ == SessionState::Waiting)
            state_ = SessionState::Completed;
        parked_ = {};
    }
}

bool Session::park(std::coroutine_handle<> h, Prompt p)
{
    parked_ = h;
    prompt_ = std::move(p);
    state_  = SessionState::Waiting;
    if (bus_.on_prompt) bus_.on_prompt(prompt_);
    return true;
}

Value Session::take_supplied()
{
    Value v = supplied_ ? std::move(*supplied_) : Value{};
    supplied_.reset();
    return v;
}

core::Status Session::supply(Value v)
{
    if (state_ != SessionState::Waiting)
        return core::err(core::ErrorCode::InvalidArgument,
                         std::string("'") + spec_->id + "' komutu girdi beklemiyor (durum: " +
                             session_state_name(state_) + ")");

    // A POINT IS NOT A NUMBER. A click at a prompt for a number used to be read
    // as zero — `as_number` of a coordinate — so pressing the canvas at "Ofset
    // mesafesi" offset by nothing and the command refused a value the user never
    // typed. Said here, before anything resumes, so the prompt stays open for the
    // number it wants; a prompt that takes a distance by pointing says so and is
    // let through (`Prompt::pick_distance`).
    if ((prompt_.kind == ParamKind::Number || prompt_.kind == ParamKind::Integer) &&
        v.kind() == Value::Kind::Point && !prompt_.pick_distance && !prompt_.pick_sweep)
        return core::err(core::ErrorCode::InvalidArgument,
                         "\"" + prompt_.message +
                             "\" bir sayı bekliyor; tıklamak yerine komut satırına yazın.");

    // A COUNT HAS NO FRACTION: `4,5` at a prompt for a whole number is said to
    // be wrong here, while the prompt is still open for the right one.
    if (prompt_.kind == ParamKind::Integer && v.kind() == Value::Kind::Number &&
        v.as_number() != static_cast<double>(static_cast<std::int64_t>(v.as_number())))
        return core::err(core::ErrorCode::InvalidArgument,
                         "\"" + prompt_.message + "\" bir tam sayı bekliyor.");

    // NOR IS IT A WORD. A click at a prompt for a word arrived as the empty
    // string — `as_text` of a coordinate — so pressing the canvas while RENK
    // asked for a colour failed the command with "Tanınmayan renk: ''", and a
    // layer name, a caption or a verb took nothing for an answer the same way.
    if (prompt_.kind == ParamKind::Text && v.kind() == Value::Kind::Point)
        return core::err(core::ErrorCode::InvalidArgument,
                         "\"" + prompt_.message + "\" bir sözcük bekliyor; tıklamak yerine " +
                             (prompt_.choices.empty() ? "komut satırına yazın."
                                                      : "komut satırına yazın ya da listeden "
                                                        "seçin."));

    supplied_ = std::move(v);
    state_    = SessionState::Running;

    auto h  = parked_;
    parked_ = {};
    if (h) {
        try {
            h.resume();
        } catch (const std::exception& e) {
            fail(
                core::err(core::ErrorCode::Internal,
                          std::string("'") + spec_->id + "' komutu istisna fırlattı: " + e.what()));
            return error_;
        }
        if (task_.done() && state_ == SessionState::Running) state_ = SessionState::Completed;
    }
    return core::ok();
}

core::Status Session::retract()
{
    if (state_ != SessionState::Waiting || !prompt_.can_retract)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Bu istemde geri alınacak bir nokta yok.");

    // OUT OF THE RECORD TOO, so the journal keeps the run as it stands: the
    // point was recorded when it was given (`record_awaited`), and a run the
    // user corrected must replay corrected.
    if (const Value* had = resolved_.find(prompt_.param);
        had != nullptr && had->kind() == Value::Kind::PointList) {
        Value::Points run = had->as_points();
        if (!run.empty()) run.pop_back();
        resolved_.set(prompt_.param, Value::points(std::move(run)));
    }

    retracted_ = true;
    return supply(Value{});
}

bool Session::park_job(std::coroutine_handle<> h, Job& job)
{
    if (!client_driven_ || !bus_.on_job_host) return false;
    parked_ = h;
    job_    = &job;
    state_  = SessionState::Working;
    bus_.on_job_host(*this);
    return true;
}

void Session::resume_job()
{
    if (state_ != SessionState::Working) return;
    job_   = nullptr;
    state_ = SessionState::Running;

    auto h  = parked_;
    parked_ = {};
    if (!h) return;
    try {
        h.resume();
    } catch (const std::exception& e) {
        fail(core::err(core::ErrorCode::Internal,
                       std::string("'") + spec_->id + "' komutu istisna fırlattı: " + e.what()));
        return;
    } catch (...) {
        fail(core::err(core::ErrorCode::Internal,
                       std::string("'") + spec_->id + "' komutu bilinmeyen bir istisna fırlattı."));
        return;
    }
    if (task_.done() && state_ == SessionState::Running) state_ = SessionState::Completed;
}

void Session::cancel()
{
    if (finished()) return;

    // A WORKER OWNS THE JOB. The coroutine cannot be resumed from here while the
    // worker is inside `work`; the stop is requested, the host resumes the
    // command when the worker returns, and the command unwinds on the cancelled
    // outcome it finds — the same ESC path, one job later.
    if (state_ == SessionState::Working) {
        if (job_ != nullptr) job_->stop.request_stop();
        cancel_requested_ = true;
        return;
    }

    if (auto* live = dynamic_cast<InteractiveInputSource*>(input_.get())) live->cancel();

    // Resume with an empty value: the awaiter reports nullopt, so the command body
    // takes its normal ESC path and unwinds cleanly. No forced destruction.
    if (state_ == SessionState::Waiting && parked_) {
        supplied_ = Value{};
        state_    = SessionState::Running;
        auto h    = parked_;
        parked_   = {};
        h.resume();
    }

    state_ = SessionState::Cancelled;
}

void Session::record(std::string param, Value v)
{
    resolved_.set(std::move(param), std::move(v));
}

namespace {

/// Whether the SPEC declares `name` as a parameter that holds a RUN, and of what.
///
/// The awaiter cannot read this off its own `Param`: `Context::point` builds a
/// throwaway `Param::point(name)` because one await asks for one point, whatever
/// the parameter it is filling holds. The declaration is the only thing that
/// tells `merkez`, which is a point, from `noktalar`, which is a run of them —
/// and `ayak`, which is a run of readings, from `mesafe`, which is one.
ParamKind declared_run_kind(const CommandSpec& spec, const std::string& name)
{
    for (const Param& p : spec.params) {
        if (p.name != name) continue;
        if (p.arity.max <= 1) break;
        if (p.kind == ParamKind::PointList || p.kind == ParamKind::Number) return p.kind;
        break;
    }
    return ParamKind::Bool; ///< "not a run": no run is ever declared as a boolean
}

} // namespace

void Session::record_awaited(const std::string& param, Value v)
{
    const ParamKind run_of = declared_run_kind(*spec_, param);

    // NOT A RUN, or an answer of the wrong shape for the run it declares: kept
    // as it came. A command that asked for one point into `merkez` records one
    // point, and a `cizgi=evet` into a boolean records a boolean.
    const bool of_run = (run_of == ParamKind::PointList && v.kind() == Value::Kind::Point) ||
                        (run_of == ParamKind::Number &&
                         (v.kind() == Value::Kind::Number || v.kind() == Value::Kind::Int));
    if (!of_run) {
        resolved_.set(param, std::move(v));
        return;
    }

    bool begun = false;
    for (const std::string& name : runs_begun_)
        if (name == param) begun = true;
    if (!begun) runs_begun_.push_back(param);

    // GROWN ONE AWAIT AT A TIME, and `set` keeps a replaced argument's POSITION,
    // so a run cannot reorder the journal line it will be written to.
    //
    // A RUN OF READINGS ACCUMULATES FOR THE REASON A RUN OF POINTS DOES. It did
    // not, and `DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5` journalled the
    // LAST pair: two details went into the drawing and one came out of the
    // journal, so the replay drew a different drawing (Article 6.4). Exactly the
    // defect `NOKTA` had before `record_awaited` existed.
    const Value* had = begun ? resolved_.find(param) : nullptr;
    if (run_of == ParamKind::PointList) {
        Value::Points run;
        if (had != nullptr && !had->empty()) run = had->as_points();
        run.push_back(v.as_point());
        resolved_.set(param, Value::points(std::move(run)));
        return;
    }

    Value::Numbers run;
    if (had != nullptr && !had->empty()) run = had->as_numbers();
    run.push_back(v.as_number());
    resolved_.set(param, Value::numbers(std::move(run)));
}

void Session::fail(core::Error e)
{
    error_ = std::move(e);
    state_ = SessionState::Failed;
}

} // namespace kentos::command
