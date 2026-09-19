// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the record that answers "why is this line here?"
//
// WHAT IT IS FOR. kentoscad.md §5.2.3 and .claude/ai.md R6: every AI turn leaves
// a record carrying the prompt, the model's identity and version, the provider
// and endpoint, the command sequence, the user's decision and a UTC timestamp —
// for approvals AND for rejections. R7 goes one step further: every entity an
// approved suggestion created must resolve back to its record, because the
// question a surveying engineer will actually be asked, in front of a
// municipality or a court, is who put this boundary here and on what basis.
//
// WHY NOT THE JOURNAL. The journal has two line shapes and both are fixed: a
// command line (`JournalEntry`) and a meta line (`Journal::append_meta`). An
// audit record is neither — it holds a prompt and a decision, it exists for
// REJECTED suggestions that never became commands, and it must survive a crash
// the moment the decision is taken. So it is its own append-only JSONL file, and
// one `{"kind":"meta"}` journal line points at it so a replay of the drawing's
// own history still says "an AI suggestion was applied here, record X".
//
// AND NO CREDENTIALS, EVER (ai.md P11). The record names the endpoint and the
// model; it never carries a key, a token or a password. `ai::redact` is applied
// to anything that came off the wire before it can reach this file.
#pragma once

#include "kentos_cad/ai/plan.hpp"

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace kentos::ai {

/// One decision, with the six fields R6 demands.
struct AuditRecord
{
    std::string id;      ///< `d` + 16 hex digits; what an entity points back at
    std::string plan_id; ///< the suggestion this decided

    std::string prompt;    ///< what was asked, as the client stated it
    std::string model;     ///< model identity and version, or empty for a plain MCP client
    std::string endpoint;  ///< provider kind and endpoint, or the client's declared name
    std::string requester; ///< who asked: the client name and its token fingerprint

    std::vector<std::string> commands; ///< the command lines, exactly as they would run
    std::string decision;              ///< `uygula` / `reddet` / `geri_cek` / `koordinat_reddi`
    std::string operator_name;         ///< who was at the workstation
    std::int64_t utc_ms{0};            ///< when, UTC

    std::string outcome;               ///< what happened when it was applied, or why it was not
    std::vector<std::int64_t> created; ///< persistent keys the approved plan created (R7)

    /// One JSONL line. `sürüm` first, because a file without a version is a file
    /// that cannot be read by a later program (.claude/io.md P5).
    core::Json to_json() const;
};

/// The append-only log.
///
/// THE SINK IS INJECTED, and that is what makes this testable: the application
/// hands it a function that appends a line to a file in the user's configuration
/// directory and flushes it, a test hands it a function that appends to a vector.
/// Nothing in `/src/ai` opens a file (ai.md P10, and `/src/ai` may not link io).
class AuditLog
{
public:
    /// Takes one finished JSONL line, including its newline. Must be durable by
    /// the time it returns: a decision that is lost in a crash is a decision
    /// nobody can be held to.
    using Sink = std::function<void(const std::string& line)>;

    explicit AuditLog(Sink sink);

    /// Records a decision and returns the record's id, which the caller stores
    /// on whatever the plan created (R7).
    std::string write(AuditRecord record);

    /// Records a refusal that never became a plan: a client that wrote a
    /// coordinate literal where a handle was declared (ai.md R10 requires the
    /// rejection to be audit-logged, not merely answered).
    std::string write_coordinate_refusal(std::string requester, std::string tool,
                                         std::string detail, std::int64_t utc_ms);

    /// Every record written in this run, newest last. For the panel and the
    /// tests; the file is the durable copy.
    const std::vector<AuditRecord>& recent() const noexcept { return recent_; }

private:
    std::string next_id();

    Sink sink_;
    std::vector<AuditRecord> recent_;
    std::uint64_t written_{0};
};

} // namespace kentos::ai
