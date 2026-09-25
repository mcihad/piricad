// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/job.hpp"

#include "kentos_cad/command/session.hpp"

#include <algorithm>

namespace kentos::command {

bool JobAwaiter::await_suspend(std::coroutine_handle<> h)
{
    // A session that can be resumed later parks on the job and the host runs it;
    // one that cannot runs it right here, on this thread, and the coroutine
    // continues as if nothing had happened. The body of the command is the same
    // in both cases (Article 1.2).
    if (session_.park_job(h, job_)) return true;
    job_.work(job_.control());
    return false;
}

void JobControl::at(std::size_t done, std::size_t total) const noexcept
{
    at(done, total, 0, 1000);
}

void JobControl::at(std::size_t done, std::size_t total, std::uint32_t from,
                    std::uint32_t to) const noexcept
{
    if (permille == nullptr) return;
    to = std::max(to, from);
    if (total == 0 || done >= total) {
        permille->store(to);
        return;
    }
    const std::size_t span = to - from;
    permille->store(from + static_cast<std::uint32_t>((done * span) / total));
}

} // namespace kentos::command
