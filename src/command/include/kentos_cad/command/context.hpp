// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the execution context handed to a command body.
//
// kentoscad.md §2.4 — ctx.point() does not know where the input comes from: a mouse
// click, a typed coordinate, the script's next argument, or an AI-produced value.
// The same command code runs in all four contexts. This is the most critical
// detail of the architecture, so nothing in this header may ever expose Origin to
// the command body.
#pragma once

#include "kentos_cad/command/input.hpp"
#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/command/value.hpp"
#include "kentos_cad/core/json.hpp"

#include <coroutine>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {

/// One running command; see session.hpp. Declared rather than included so this
/// header stays cheap for every command body that includes it.
class Session;

/// Applies the input aids — object snap, `dik mod`, `kutupsal izleme` and
/// `ızgaraya yakalama` — to a point value on its way into a command body.
///
/// THIS IS THE ONE PLACE IT HAPPENS, and it sits on the path every `co_await
/// ctx.point(...)` takes, so a mouse click, a typed coordinate, a script argument
/// and an AI-produced point are aided identically (kentoscad.md §2.4, Article 1.2).
/// It does not ask, and cannot ask, which client supplied the value
/// (`.claude/command.md` P10) — it is handed a `Value` and a `Prompt`, and the
/// prompt's rubber-band origin is the previous point every direction constraint
/// measures from.
///
/// A non-point value is returned untouched. See `kentos_cad/command/aids.hpp` for
/// why a client with no view gets no object snap.
Value apply_input_aids(Session& session, const Prompt& prompt, Value v);

/// Awaits one input value. Fast path: if the source already holds the value
/// (script / CLI / AI / batch) the coroutine never suspends and never allocates.
template<class T> class InputAwaiter
{
public:
    /// Turns the serialisable `Value` the source supplied into the type the
    /// command body asked for. A function pointer, not a closure: there is one
    /// conversion per type and nothing to capture.
    using Convert = T (*)(const Value&);

    /// Built by `Context::point()` and friends; never constructed directly.
    InputAwaiter(Session& s, Param param, Prompt prompt, Convert conv)
        : session_(s), param_(std::move(param)), prompt_(std::move(prompt)), conv_(conv)
    {}

    /// True when the value is ALREADY available — a script argument, a typed
    /// coordinate, an AI tool result. The coroutine then never suspends and never
    /// allocates, which is what keeps script dispatch inside the 10 µs budget.
    bool await_ready();

    /// Parks the command until the value arrives. Only the interactive path
    /// reaches this.
    void await_suspend(std::coroutine_handle<> h);

    /// The converted value, or `nullopt` when the user cancelled. A command reads
    /// this as "stop" and returns; it never asks WHY, because ESC from a mouse and
    /// an exhausted argument list are the same fact to the body (kentoscad.md §2.4).
    std::optional<T> await_resume();

private:
    Session& session_;
    Param param_;
    Prompt prompt_;
    Convert conv_;
    std::optional<Value> ready_{};
    bool cancelled_{false};
};

/// How a point request should be presented, for the clients that present.
///
/// Purely a hint: a headless replay ignores it entirely, and the command body is
/// identical either way. It lives on the request rather than in the canvas so the
/// canvas does not have to know which command is running.
struct PointOptions
{
    bool rubber_band{false}; ///< draw a preview while the user aims
    Point2 rubber_origin{};  ///< where that preview starts

    /// What the preview draws. A command that encloses a face with two corners
    /// says so, and the canvas shows the face rather than its diagonal.
    RubberShape rubber_shape{RubberShape::Line};

    /// The points already fixed this run, for a command whose geometry cannot
    /// reach the document until it is complete. See `Prompt::rubber_chain`.
    std::vector<Point2> rubber_chain{};

    /// The kind payload the preview draws with. See `Prompt::rubber_payload`.
    std::vector<std::uint8_t> rubber_payload{};

    /// A click answers this number with its distance from `rubber_origin`, in
    /// metres. See `Prompt::pick_distance`. Read by `Context::number` only: an
    /// integer parameter is in a declared unit the canvas does not know.
    bool pick_distance{false};
};

class Context;

/// The objects a modify command is to work on, from whichever source has them.
///
/// THE ORDER IS THE POINT, and every modify command needs the same one:
///   1. the named argument, when a script, the CLI or the AI supplied it;
///   2. the live selection, when the user highlighted something first;
///   3. asked for, by pointing at them.
///
/// Step 3 is what makes a tool-column button behave like a CAD tool: press it
/// with nothing selected and it arms and asks, rather than refusing. Before it
/// existed the buttons answered an empty selection with a sentence in the status
/// line and did nothing at all, so the ordinary order of work — reach for the
/// tool, then point at the thing — produced a dead button.
///
/// Returns false when the user cancelled; the caller returns without doing
/// anything, exactly as it would for any other refused prompt.
/// `most` caps how many objects the caller can accept — 1 for BÖL, 2 for BUDA,
/// 0 for no limit. Enforced HERE rather than in the caller so that a refusal
/// leaves the parameter clear; a caller that judged the count itself would be
/// judging it after the awaiter had already recorded the answer.
/// `example` is shown only when there is nothing to work on — a script that
/// forgot its argument needs to see the form, and a user standing at a prompt
/// does not. Putting it in the PROMPT, which is what the callers used to do, put
/// "TAŞI nesneler=1 baslangic=0,0 bitis=10,0" on the command line while the user
/// was being asked to point at something.
Task<bool> want_objects(Context& ctx, std::string param, std::string message,
                        std::vector<std::int64_t>& out, std::size_t most = 0,
                        std::string example = {});

class Context
{
public:
    /// Built by the bus for one command run. Everything is held by reference:
    /// a context lives exactly as long as the command it serves.
    Context(Session& session, Transaction& tx, const core::Document& doc);

    // ---- input, source-agnostic ----
    InputAwaiter<Point2> point(std::string param, std::string message, PointOptions o = {});

    /// Asks for a number, and — with `o` — says what the canvas should keep on
    /// screen while it is typed.
    ///
    /// A GUIDE IS NOT ONLY FOR A POINT. The commands that take tape readings fix
    /// a reference first and then ask for numbers against it: a baseline and
    /// then `ayak`/`boy`, a station and then an angle and a side. That reference
    /// is not a document object, so once it was given it left the screen and the
    /// user was typing readings against a baseline they could no longer see.
    /// `RubberShape::Fixed` draws what is fixed and nothing that follows the
    /// cursor, which is the honest preview for a question the mouse is not
    /// answering.
    InputAwaiter<double> number(std::string param, std::string message, PointOptions o = {});
    InputAwaiter<std::int64_t> integer(std::string param, std::string message, PointOptions o = {});
    /// Asks for a word. `choices` are the words a client may OFFER — the blocks
    /// in the drawing, the patterns in the catalogue — never a restriction on
    /// what is acceptable (`Prompt::choices`).
    InputAwaiter<std::string> text(std::string param, std::string message,
                                   std::vector<std::string> choices = {});
    InputAwaiter<bool> boolean(std::string param, std::string message);

    /// Asks which objects the command is to act on.
    ///
    /// Answered without suspending when the client already said — a script's
    /// `nesneler=1 2`, a CLI line, an AI tool result — and by pointing when it did
    /// not: the canvas picks into the live selection and Enter hands it over. The
    /// body cannot tell which happened, which is the whole point (Article 1.2).
    InputAwaiter<Value::Ints> objects(std::string param, std::string message);

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
    /// runnable headless (kentoscad.md §14 journal replay).
    ///
    /// NOT FOR A REFUSAL. A line on the transcript reaches a person at the command
    /// line and nobody else: the bus still reports the dispatch as a success, so a
    /// script carries on, an agent's plan is told its step happened and
    /// `cad.trim(...)` returns instead of raising. Refuse with `refuse`.
    void echo(std::string message) const;

    /// Ends the command as a FAILURE carrying `message`, which is what a refusal
    /// is. The bus rolls the transaction back and returns the error, so every
    /// client learns it the same way: the command line prints it, a JSON script
    /// stops and rolls back, `cad.<name>(...)` raises, an agent's step fails.
    ///
    /// WHY THIS EXISTS, measured: the support matrix found 38 cells where a
    /// command wrote its refusal to the transcript with `echo`, ended its body,
    /// and the bus reported success — a refusal a person could read and an
    /// automated client could not see (`docs/nesneler/destek-matrisi.md`,
    /// TODOS A-07: a partial failure is never presented as a success).
    ///
    /// The body still returns afterwards; this records the outcome, it does not
    /// unwind the coroutine.
    void refuse(core::ErrorCode code, std::string message) const;

    /// The same, for an error something else already produced.
    void refuse(core::Error error) const;

    /// Records the effective value of a parameter so the journal entry replays
    /// identically no matter which client supplied it.
    void record(std::string param, Value v);

    /// The command's STRUCTURED answer, for a client that is not a person.
    ///
    /// `echo` is for the transcript and is prose; a query's answer is data, and
    /// an agent should not have to parse a Turkish sentence to read a layer's
    /// object count. A command reports at most once and MUST still echo the
    /// human sentence — this is an addition to what it says, never a
    /// replacement, because the person at the keyboard is still the main reader.
    ///
    /// It goes nowhere near the document: not hashed, not journalled, not
    /// undoable. It rides out on `DispatchResult::report`.
    void report(core::Json data) const;

    /// NAMES A FILE THIS CALL WROTE. Rides out on `DispatchResult::outputs`.
    ///
    /// A query answers with data and a drawing command answers with a drawing;
    /// `YAZDIR`, `DIŞAAKTAR` and `FARKLIKAYDET` answer with a FILE, and a client
    /// told only "tamam" has no way to find it or check it (TODOS C-03).
    void wrote(std::string path) const;

    /// SOMETHING THE CALL COULD NOT HONOUR WITHOUT FAILING. Rides out on
    /// `DispatchResult::warnings`.
    ///
    /// A sheet that printed with one broken map link, a table that did not fit
    /// its box. Not an error — the call did what it could — and not nothing: a
    /// client that reports success without it reports something untrue.
    void warn(std::string note) const;

    Session& session() noexcept { return session_; }

private:
    Session& session_;
    Transaction& tx_;
    const core::Document& doc_;
};

} // namespace kentos::command
