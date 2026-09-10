// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: work a command hands to a worker thread.
//
// io.md P3: no file is parsed on the UI thread. An import used to run its whole
// read inside the command's coroutine, on whatever thread ran the bus — which in
// the application is the GUI thread, so a 48 MB DXF froze the window for the
// length of the read and `Durdur` had nothing to press. A `Job` is the read
// lifted out: a callable the command describes, a stop it honours, and an
// awaitable that either hands it to a host thread or runs it in place.
//
// WHERE THE WORK RUNS IS NOT DECIDED BY THE CLIENT (Article 1.2). The command
// body is one body; `JobAwaiter::await_suspend` asks the SESSION whether it can
// be resumed later — the same question `InputAwaiter::await_ready` asks about
// input — and a session that cannot (a script, a test, the command line inside
// `run_to_completion`) runs the job in place and continues. The document and the
// journal come out identical either way, and that identity is what the proof
// tests check.
#pragma once

#include <coroutine>
#include <functional>
#include <stop_token>
#include <string>

namespace kentos::command {

/// The command's session; see session.hpp. Declared rather than included because
/// the session includes this header.
class Session;

/// What a worker is handed: the stop it must honour (io.md R15: within 100 ms).
struct JobControl
{
    std::stop_token stop; ///< requested by `Durdur`, ESC or the session's teardown
};

/// One unit of work a command hands off. The command owns it for the length of
/// the await; the worker reads `work` and `stop` and writes nothing else.
struct Job
{
    std::string label;                           ///< what the status strip says while it runs
    std::function<void(const JobControl&)> work; ///< the work itself; must not touch the document
    std::stop_source stop;                       ///< the worker's stop, requested by cancel()
};

/// `co_await run_job(session, job)`: runs the job and continues when it is done.
class JobAwaiter
{
public:
    /// Binds the job to the session whose command awaits it.
    JobAwaiter(Session& session, Job& job) noexcept : session_(session), job_(job) {}

    /// Never ready: whether to suspend is decided in `await_suspend`.
    bool await_ready() const noexcept { return false; }

    /// Parks the session on the job when a host can run it and resume the session
    /// afterwards; otherwise runs the job here and continues without suspending.
    bool await_suspend(std::coroutine_handle<> h);

    /// Nothing to hand back: the job wrote its outcome where the command told it.
    void await_resume() const noexcept {}

private:
    Session& session_;
    Job& job_;
};

/// The awaitable a command uses to hand `job` off.
inline JobAwaiter run_job(Session& session, Job& job) noexcept
{
    return JobAwaiter{session, job};
}

} // namespace kentos::command
