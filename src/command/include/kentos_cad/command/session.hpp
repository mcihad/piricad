// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: one running command instance.
//
// A Session owns the coroutine, its transaction and its input source. The bus
// drives it; the GUI feeds it. Non-interactive clients run it to completion in a
// single call, interactive clients park it between mouse clicks. Both take the
// same path — no client has a private route (Constitution Article 1).
#pragma once

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/input.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"

#include <coroutine>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace kentos::command {

/// Where one running command is in its life.
///
/// `Waiting` is the state that makes an interactive CAD command possible at all:
/// the coroutine is parked on a prompt, the editor is responsive, and the same
/// body would have run straight through had the arguments been supplied up front.
enum class SessionState : std::uint8_t {
    Ready, ///< created, not started
    Running,
    Waiting, ///< suspended on a Prompt, needs supply()
    Completed,
    Cancelled,
    Failed,
};

/// Stable machine name, for messages and tests.
const char* session_state_name(SessionState s);

/// The command bus; see bus.hpp. Declared rather than included because the bus
/// includes this header.
class Bus;

class Session
{
public:
    /// Exactly one of `owned_tx` / `borrowed_tx` must be supplied. A batch run
    /// borrows the bus's batch transaction so N commands collapse into one undo
    /// step (kentoscad.md §10.4); every other run owns its own.
    Session(Bus& bus, const CommandSpec& spec, std::unique_ptr<InputSource> input,
            std::unique_ptr<Transaction> owned_tx, Transaction* borrowed_tx = nullptr);
    ~Session();

    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;

    /// Runs until the command completes or suspends on a prompt.
    void start();

    /// Feeds one value into a waiting command and resumes it.
    core::Status supply(Value v);

    /// ESC. The command sees the input source as exhausted and returns normally,
    /// exactly as it would at the end of a script's argument list.
    void cancel();

    SessionState state() const noexcept { return state_; }

    bool waiting() const noexcept { return state_ == SessionState::Waiting; }

    bool finished() const noexcept;

    const Prompt& prompt() const noexcept { return prompt_; }

    const CommandSpec& spec() const noexcept { return *spec_; }

    Transaction& transaction() noexcept { return *tx_; }

    bool owns_transaction() const noexcept { return owned_tx_ != nullptr; }

    InputSource& input() noexcept { return *input_; }

    const InputSource& input() const noexcept { return *input_; }

    Bus& bus() noexcept { return bus_; }

    /// The document version observed before this command began. Some sanctioned
    /// additive edits (such as creating a layer) deliberately have no inverse
    /// Op, but the shell must still refresh when they change the document.
    std::uint64_t document_revision_at_start() const noexcept
    {
        return document_revision_at_start_;
    }

    /// Arguments as actually resolved, in declaration order. This is what the
    /// journal records, so a replay reproduces the run bit for bit.
    const Args& resolved() const noexcept { return resolved_; }

    void record(std::string param, Value v);

    const core::Error& error() const noexcept { return error_; }

    void fail(core::Error e);

    // ---- used by InputAwaiter ----
    bool park(std::coroutine_handle<> h, Prompt p);
    Value take_supplied();

private:
    void resume_once();

    Bus& bus_;
    const CommandSpec* spec_;
    std::unique_ptr<InputSource> input_;
    std::unique_ptr<Transaction> owned_tx_;
    Transaction* tx_{nullptr};
    std::unique_ptr<Context> ctx_;
    Task<void> task_{};

    SessionState state_{SessionState::Ready};
    Prompt prompt_{};
    std::coroutine_handle<> parked_{};
    std::optional<Value> supplied_{};
    Args resolved_{};
    core::Error error_{};
    std::uint64_t document_revision_at_start_{0};
};

// ---- InputAwaiter, defined here because it needs the full Session ----

template<class T> bool InputAwaiter<T>::await_ready()
{
    if (auto v = session_.input().take(param_)) {
        ready_ = std::move(v);
        return true;
    }
    if (session_.input().exhausted()) {
        cancelled_ = true;
        return true; // ESC / end of arguments — resume immediately with nullopt
    }
    return false;
}

/// Parks the command on its prompt. Only the interactive path reaches this: a
/// script, the command line and the AI all answered `await_ready` with true.
template<class T> void InputAwaiter<T>::await_suspend(std::coroutine_handle<> h)
{
    session_.park(h, prompt_);
}

/// Hands the command its value, aids applied, or `nullopt` when it was cancelled.
template<class T> std::optional<T> InputAwaiter<T>::await_resume()
{
    if (cancelled_) return std::nullopt;

    Value v = ready_ ? std::move(*ready_) : session_.take_supplied();
    if (v.empty()) return std::nullopt;

    // The input aids run BEFORE the value is recorded, so the journal keeps the
    // point that was actually drawn rather than the one that was aimed at. A
    // replay then re-supplies a resolved point, and every rule in the snap engine
    // is idempotent so re-resolving it changes nothing (core/snap.hpp).
    v = apply_input_aids(session_, prompt_, std::move(v));

    session_.record(param_.name, v);
    return conv_(v);
}

} // namespace kentos::command
