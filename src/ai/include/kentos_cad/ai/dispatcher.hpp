// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the only door from this module to the running document.
//
// WHY AN INTERFACE AND NOT A `Bus&`. Two reasons, and both are structural.
//
// THREADS. A request arrives on a network thread; `Bus`, `Session`, `Document`,
// `Registry` and `UndoStack` take no locks, and `Document::spatial_index()`
// rebuilds a cache through `mutable` members even though it is `const`. Touching
// any of it off the bus thread is a data race with a five-million-row table. The
// application's implementation of this interface is the one place that crosses
// the thread boundary, and it crosses it with a queued call — the idiom
// `Controller::hostJob` already uses in the other direction.
//
// TESTS. A protocol is proved by a function, not by a socket (.claude/test.md).
// With the document behind this interface, the MCP engine, the plan compiler and
// the chat loop are all exercised by a test double with no Qt, no network and no
// display — which is why almost all of this module is Qt-free doctest.
//
// WHAT IT DOES NOT DO. It does not apply a plan. Nothing in `/src/ai` can:
// applying needs an `ai::Approval`, and only the application's suggestion card
// can make one (gate.hpp). This interface reads the document and runs the
// commands that change nothing.
#pragma once

#include "kentos_cad/ai/plan.hpp"
#include "kentos_cad/ai/tool.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <string>
#include <vector>

namespace kentos::ai {

/// What a command answered: its prose, its structured report, and whether it
/// changed anything.
struct ToolOutcome
{
    std::vector<std::string> lines; ///< every line the command echoed
    core::Json report;              ///< its structured answer, when it gave one
    bool mutated{false};            ///< whether the document changed
    std::string command_id;         ///< what ran

    /// Handles minted from this command's report, ready for the client to refer
    /// to in a later call.
    ///
    /// MINTED HERE AND NOT BY THE COMMAND, deliberately. A handle is an
    /// AI-layer bookkeeping device: a person typing `SORGULA` at the command
    /// line wants the answer, not a token, and a command that minted one would
    /// be a command with an AI-only concept in it (Article 1.2). So the read
    /// commands report plain data — keys, corners, counts — and the dispatcher,
    /// which owns the store and knows the document's revision, turns that data
    /// into handles for the client that asked.
    std::vector<std::string> minted;
};

class Dispatcher
{
public:
    /// Virtual: the application holds the dispatcher through this interface.
    virtual ~Dispatcher() = default;

    // ---- WHO IS ASKING ------------------------------------------------------
    //
    // Every door below takes a `requester`, and it is not decoration. Two agents
    // on one loopback port are two strangers. A handle is minted from what one
    // client read out of the document and a plan is composed from handles, so a
    // client that could name another's handle would be drawing from numbers it
    // was never shown, and one that could reach another's plan could append a
    // step to a suggestion somebody is about to approve — and the approval a
    // person gives is for the lines they READ (TODOS M-07).
    //
    // THE LABEL IS THE SCOPE. For the MCP server it is the client's declared
    // name plus its token's fingerprint (`McpServer::requester_label`); for the
    // chat it is `sohbet` and the profile. It is the same string that reaches
    // the audit record, and it never contains a credential (ai.md P11).
    //
    // AN EMPTY LABEL IS THE PERSON AT THE KEYBOARD, who is not scoped: the
    // suggestion panel lists every pending plan whoever filed it, because the
    // operator is the one who applies them (`ClientScope::client`, policy.hpp).
    // Anything that arrived over a socket carries a non-empty one.

    /// Runs a command that changes NOTHING, now, on the bus thread, and returns
    /// what it said. Refuses anything without `command::Flags::NoEffect` — the
    /// check is here, at the door, rather than at each caller.
    ///
    /// `Flags::ReadOnly` is NOT the test, and that distinction matters: it means
    /// "skips the transaction path", and `core.undo`, `core.save` and
    /// `core.export` all carry it. An agent allowed to run them without approval
    /// could reverse the drawing or write over a file.
    ///
    /// NEITHER IS THE FLAG ALONE. Some commands both edit and answer — the
    /// layout command adds pages AND reports what will print wrong — so the test
    /// is `command::effect_of(spec, args)`, which reads the verb out of these
    /// arguments (C-02). Moving the view counts as no effect, in CLAUDE.md
    /// 2.10's own words: an agent "may read anything and move the view".
    ///
    /// Handles minted from the answer land in `requester`'s store, which is why
    /// the label is needed even by a call that only reads.
    virtual core::Result<ToolOutcome> run_read_only(const std::string& command_id,
                                                    const command::Args& args,
                                                    const std::string& requester) = 0;

    /// Files a plan and shows it to the person at the workstation. Returns the
    /// plan's id. Applies nothing.
    ///
    /// `Plan::requester` carries the label here: it is part of the record, so a
    /// second parameter would be a second place for one fact.
    virtual core::Result<std::string> propose(Plan plan) = 0;

    /// The plan `requester` has already filed under `key`, or an empty string.
    ///
    /// ASKED BEFORE `propose`, because afterwards the two cases cannot be told
    /// apart: proposing with a key that is already held answers with the SAME
    /// id, which is what makes a retry safe and what makes it invisible. A
    /// client that retried after a dropped connection should be told its retry
    /// was recognised rather than that a second suggestion was opened (M-06).
    virtual std::string existing_plan(const std::string& key,
                                      const std::string& requester) const = 0;

    /// A plan's state, for a client that is waiting on a decision. Refuses a
    /// plan `requester` does not own.
    virtual core::Result<Plan> plan_state(const std::string& id,
                                          const std::string& requester) const = 0;

    /// The client has gone (its stream closed): withdraw the plan rather than
    /// leaving it on the person's screen for ever. Closing the stream is the
    /// cancellation signal in MCP 2026-07-28, and this is what it means here.
    /// A plan `requester` does not own is left alone.
    virtual void withdraw(const std::string& id, const std::string& requester) = 0;

    /// The document's revision now, so a handle can be told stale.
    virtual std::uint64_t revision() const = 0;

    /// What the viewport shows, or nothing when no viewport is attached — a
    /// headless run genuinely has no window, and `gorunum_bilgisi` says so
    /// rather than inventing a rectangle.
    virtual std::optional<command::ViewInfo> view() const = 0;

    /// The handles `requester` has been given. One store per client, so a
    /// handle from one session means nothing in another — which is the shape
    /// `HandleStore::next_id` was written for.
    virtual HandleStore& handles(const std::string& requester) = 0;

    /// The catalogue as this session may see it, already filtered by policy.
    virtual const Catalog& catalog() const = 0;
};

} // namespace kentos::ai
