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

    /// Runs a command that changes NOTHING, now, on the bus thread, and returns
    /// what it said. Refuses anything without `command::Flags::NoEffect` — the
    /// check is here, at the door, rather than at each caller.
    ///
    /// `Flags::ReadOnly` is NOT the test, and that distinction matters: it means
    /// "skips the transaction path", and `core.undo`, `core.save` and
    /// `core.export` all carry it. An agent allowed to run them without approval
    /// could reverse the drawing or write over a file.
    virtual core::Result<ToolOutcome> run_read_only(const std::string& command_id,
                                                    const command::Args& args) = 0;

    /// Files a plan and shows it to the person at the workstation. Returns the
    /// plan's id. Applies nothing.
    virtual core::Result<std::string> propose(Plan plan) = 0;

    /// A plan's state, for a client that is waiting on a decision.
    virtual core::Result<Plan> plan_state(const std::string& id) const = 0;

    /// The client has gone (its stream closed): withdraw the plan rather than
    /// leaving it on the person's screen for ever. Closing the stream is the
    /// cancellation signal in MCP 2026-07-28, and this is what it means here.
    virtual void withdraw(const std::string& id) = 0;

    /// The document's revision now, so a handle can be told stale.
    virtual std::uint64_t revision() const = 0;

    /// What the viewport shows, or nothing when no viewport is attached — a
    /// headless run genuinely has no window, and `gorunum_bilgisi` says so
    /// rather than inventing a rectangle.
    virtual std::optional<command::ViewInfo> view() const = 0;

    /// The handles this session has been given. The store lives with the
    /// session, so two clients cannot use each other's handles.
    virtual HandleStore& handles() = 0;

    /// The catalogue as this session may see it, already filtered by policy.
    virtual const Catalog& catalog() const = 0;
};

} // namespace kentos::ai
