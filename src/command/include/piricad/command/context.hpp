// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the execution context handed to a command body.
//
// piricad.md §2.4 — ctx.point() does not know where the input comes from: a mouse
// click, a typed coordinate, the script's next argument, or an AI-produced value.
// The same command code runs in all four contexts. This is the most critical
// detail of the architecture, so nothing in this header may ever expose Origin to
// the command body.
#pragma once

#include "piricad/command/input.hpp"
#include "piricad/command/task.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/command/value.hpp"

#include <coroutine>
#include <functional>
#include <optional>
#include <string>

namespace piricad::command {

class Session;

/// Awaits one input value. Fast path: if the source already holds the value
/// (script / CLI / AI / batch) the coroutine never suspends and never allocates.
template<class T> class InputAwaiter
{
public:
    using Convert = T (*)(const Value&);

    InputAwaiter(Session& s, Param param, Prompt prompt, Convert conv)
        : session_(s), param_(std::move(param)), prompt_(std::move(prompt)), conv_(conv)
    {}

    bool await_ready();
    void await_suspend(std::coroutine_handle<> h);
    std::optional<T> await_resume();

private:
    Session& session_;
    Param param_;
    Prompt prompt_;
    Convert conv_;
    std::optional<Value> ready_{};
    bool cancelled_{false};
};

struct PointOptions
{
    bool rubber_band{false};
    Point2 rubber_origin{};
};

class Context
{
public:
    Context(Session& session, Transaction& tx, const core::Document& doc);

    // ---- input, source-agnostic ----
    InputAwaiter<Point2> point(std::string param, std::string message, PointOptions o = {});
    InputAwaiter<double> number(std::string param, std::string message);
    InputAwaiter<std::int64_t> integer(std::string param, std::string message);
    InputAwaiter<std::string> text(std::string param, std::string message);
    InputAwaiter<bool> boolean(std::string param, std::string message);

    /// Whole-parameter fetch for non-interactive parameters (a script passing a
    /// full point list at once). Returns an empty Value when absent.
    Value argument(std::string_view name) const;
    bool has_argument(std::string_view name) const;

    // ---- mutation, always through the transaction ----
    Transaction& transaction() noexcept { return tx_; }

    // ---- read access ----
    const core::Document& document() const noexcept { return doc_; }

    core::LayerId active_layer() const;

    /// Writes a line to the transcript. Never a dialog: a command body must be
    /// runnable headless (piricad.md §14 journal replay).
    void echo(std::string message) const;

    /// Records the effective value of a parameter so the journal entry replays
    /// identically no matter which client supplied it.
    void record(std::string param, Value v);

    Session& session() noexcept { return session_; }

private:
    Session& session_;
    Transaction& tx_;
    const core::Document& doc_;
};

} // namespace piricad::command
