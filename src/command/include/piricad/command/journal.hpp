// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the append-only command journal.
//
// kentoscad.md §2.2 — walking this log forward and backward is undo/redo; writing it
// to a file is macro recording; replaying it is the regression suite and crash
// recovery; receiving it over a socket is the remote API.
//
// §10.4 — the journal is written asynchronously on its own thread. The user never
// waits on a disk flush.
#pragma once

#include "kentos_cad/command/input.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace kentos::command {

/// One line of the journal: everything needed to replay one command.
///
/// The journal is the audit record of a legal document, so an entry carries the
/// CONTEXT a replay needs and not only the call: which CRS the coordinates were
/// in and which layer was active, because both change what the same arguments
/// mean (kentoscad.md §2.2).
struct JournalEntry
{
    std::uint64_t seq{0};         ///< position in the journal, from 1
    std::string command_id;       ///< canonical id, never the alias that was typed
    Args args;                    ///< the RESOLVED arguments, after any defaults
    Origin origin{Origin::Test};  ///< which client ran it; not compared in the proof
    std::string crs;              ///< the CRS the coordinates were expressed in
    std::string layer;            ///< the active layer at the time
    std::int64_t timestamp_ms{0}; ///< excluded from the canonical form

    /// Canonical serialisation. `with_timestamp` is false for the byte-identity
    /// proof (CLAUDE.md 6.4): three clients running the same command produce the
    /// same line, and a clock is the one thing they cannot agree on.
    core::Json to_json(bool with_timestamp) const;

    /// The inverse, for replaying a journal file.
    static core::Result<JournalEntry> from_json(const core::Json& j);
};

class Journal
{
public:
    /// An empty in-memory journal with no file sink.
    Journal();

    /// Flushes and closes the sink. A journal that lost its tail on exit would be
    /// an audit record with a hole in it.
    ~Journal();

    /// Non-copyable: a journal owns a file handle and a writer thread, and two
    /// journals appending to one file would interleave lines.
    Journal(const Journal&)            = delete;
    Journal& operator=(const Journal&) = delete;

    /// Appends in memory and, when a file sink is open, queues an async write.
    void append(JournalEntry e);

    /// Appends the SECOND line kind: `{kind:"meta", ...}` (`.claude/command.md`
    /// R20). A non-command record — a script's sandbox level and consent
    /// (`.claude/script.md` R11, R12), a plugin's id, hash and the user's decision
    /// (`.claude/plugin-api.md` R10).
    ///
    /// Kept in its own list rather than as a `JournalEntry` with a flag, because
    /// the two are not the same thing: an entry replays and this does not. R20
    /// says replay applies the first kind and IGNORES the second, and a record
    /// that cannot be replayed has no `command_id`, no `args` and no `seq` to
    /// carry. `kind` is written first and set here, so no caller can forget it or
    /// spell it something else.
    ///
    /// NOT part of `canonical()`. That rendering exists for one purpose — the
    /// byte-identity proof of CLAUDE.md 6.4, where the same command run from the
    /// GUI, the command line and a script must produce the same bytes. Only the
    /// script run has a sandbox level, so including it here would make the proof
    /// fail on a difference that is not a difference in what was done.
    void append_meta(const core::Json& record);

    const std::vector<JournalEntry>& entries() const noexcept { return entries_; }

    /// The `{kind:"meta"}` records, in the order they were appended.
    const std::vector<core::Json>& meta_records() const noexcept { return metas_; }

    std::size_t size() const noexcept { return entries_.size(); }

    void clear();

    /// Deterministic, timestamp-free rendering. Two runs of the same commands from
    /// different clients must produce byte-identical output — that equality is the
    /// Phase-0 keystone proof (kentoscad.md §16.5).
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
    std::vector<core::Json> metas_;
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

} // namespace kentos::command
