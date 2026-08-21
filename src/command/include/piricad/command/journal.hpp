// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the append-only command journal.
//
// piricad.md §2.2 — walking this log forward and backward is undo/redo; writing it
// to a file is macro recording; replaying it is the regression suite and crash
// recovery; receiving it over a socket is the remote API.
//
// §10.4 — the journal is written asynchronously on its own thread. The user never
// waits on a disk flush.
#pragma once

#include "piricad/command/input.hpp"
#include "piricad/command/value.hpp"
#include "piricad/core/result.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace piricad::command {

struct JournalEntry
{
    std::uint64_t seq{0};
    std::string command_id;
    Args args;
    Origin origin{Origin::Test};
    std::string crs;
    std::string layer;
    std::int64_t timestamp_ms{0}; ///< excluded from the canonical form

    core::Json to_json(bool with_timestamp) const;
    static core::Result<JournalEntry> from_json(const core::Json& j);
};

class Journal
{
public:
    Journal();
    ~Journal();

    Journal(const Journal&)            = delete;
    Journal& operator=(const Journal&) = delete;

    /// Appends in memory and, when a file sink is open, queues an async write.
    void append(JournalEntry e);

    const std::vector<JournalEntry>& entries() const noexcept { return entries_; }

    std::size_t size() const noexcept { return entries_.size(); }

    void clear();

    /// Deterministic, timestamp-free rendering. Two runs of the same commands from
    /// different clients must produce byte-identical output — that equality is the
    /// Phase-0 keystone proof (piricad.md §16.5).
    std::string canonical() const;

    /// Opens an async JSONL sink. Writes happen on the journal's own thread.
    core::Status open_sink(const std::string& path);
    void close_sink();
    /// Blocks until every queued line has reached the file. Tests and shutdown only.
    void flush();

    static core::Result<std::vector<JournalEntry>> read_jsonl(const std::string& path);

private:
    void writer_loop();

    std::vector<JournalEntry> entries_;
    std::uint64_t next_seq_{1};

    // ---- async sink ----
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    std::string sink_path_;
    bool sink_open_{false};
    bool stop_{false};
    std::uint64_t written_{0};
    std::uint64_t queued_{0};
    std::thread writer_;
};

} // namespace piricad::command
