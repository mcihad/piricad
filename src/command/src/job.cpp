// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/job.hpp"

#include "kentos_cad/command/session.hpp"

namespace kentos::command {

bool JobAwaiter::await_suspend(std::coroutine_handle<> h)
{
    // A session that can be resumed later parks on the job and the host runs it;
    // one that cannot runs it right here, on this thread, and the coroutine
    // continues as if nothing had happened. The body of the command is the same
    // in both cases (Article 1.2).
    if (session_.park_job(h, job_)) return true;
    job_.work(JobControl{job_.stop.get_token()});
    return false;
}

} // namespace kentos::command
