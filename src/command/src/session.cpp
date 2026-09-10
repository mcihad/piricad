// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/session.hpp"

#include "kentos_cad/command/job.hpp"

#include "kentos_cad/command/bus.hpp"

namespace kentos::command {

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

void Session::fail(core::Error e)
{
    error_ = std::move(e);
    state_ = SessionState::Failed;
}

} // namespace kentos::command
