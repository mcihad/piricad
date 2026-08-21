// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: THE COMMAND BUS.
//
//     GUI button ─┐
//     Command line ┤
//     Script ──────┼──►  BUS  ──►  Validation ──►  Transaction ──►  Document
//     AI ──────────┤                                    │
//     Batch ───────┘                                    └──►  Journal
//
// piricad.md §2.1 — "Everything that mutates application state is a command.
// The user interface is only one client of the command bus."
// No client on that diagram has a privilege over any other.
#pragma once

#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/command/validation.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/settings.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace piricad::command {

struct Invocation
{
    std::string name; ///< command id or any declared alias
    Args args;
    Origin origin{Origin::Test};
};

struct DispatchResult
{
    std::string command_id;
    std::string label;
    std::size_t ops{0}; ///< primitive edits recorded
    bool mutated{false};
    std::string message; ///< user-facing summary, Turkish
};

class Bus
{
public:
    Bus(core::Document& doc, Registry& reg, Journal& journal, UndoStack& undo);

    // ---- the single entry point for every client ----
    core::Result<DispatchResult> dispatch(const Invocation& inv);

    /// Parses and dispatches one command line. Used by the CLI widget, by macro
    /// playback and by the script engine — one grammar, one path (§3).
    core::Result<DispatchResult> execute_line(std::string_view line, Origin origin);

    /// Starts an interactive command that will ask the user for input.
    /// Only the GUI uses this, and it buys the GUI no privileges: the session
    /// runs the same coroutine, validation and transaction as every other client.
    core::Result<std::unique_ptr<Session>> begin_interactive(std::string_view name);

    /// Called by Session when a command finishes. Validates, commits or rolls
    /// back, and journals.
    core::Result<DispatchResult> finish(Session& session);

    // ---- batch mode (§10.4): one validation pass, one undo step ----
    core::Status begin_batch(std::string label);
    core::Result<DispatchResult> end_batch();

    bool in_batch() const noexcept { return batch_ != nullptr; }

    // ---- accessors ----
    core::Document& document() noexcept { return doc_; }

    const core::Document& document() const noexcept { return doc_; }

    Registry& registry() noexcept { return reg_; }

    Journal& journal() noexcept { return journal_; }

    UndoStack& undo_stack() noexcept { return undo_; }

    Validator& validator() noexcept { return validator_; }

    core::LayerId active_layer() const noexcept { return active_layer_; }

    void set_active_layer(core::LayerId l) { active_layer_ = l; }

    // ---- settings, one store per SCOPE (model.md R39, R41) ----
    //
    // PHASE-0 SEAM. R39 puts the PROJECT store inside the document: it travels
    // with the file, it is undoable, and it is part of content_hash(). That needs
    // `Document::settings()`, an `Op::SetSetting` variant and
    // `Transaction::set_setting()`, none of which exist yet — so the store lives
    // one level out, on the bus that owns exactly one document.
    //
    // What it must NOT be is a process-wide static, which is what it was: every
    // bus in the process shared one store, so the project CRS set in one drawing
    // leaked into the next File > New, and the AYAR tests were order-coupled
    // through a hidden global rather than isolated by their fixtures.
    //
    // The APP store is per user and machine (R39) and stays here permanently; the
    // application shell reads and writes it through this accessor so that
    // `TERCİH tema koyu` and the Görünüm menu are the same write (R38, CLAUDE.md
    // 5.10 — there is no second settings list).
    core::Settings& project_settings() noexcept { return project_settings_; }

    const core::Settings& project_settings() const noexcept { return project_settings_; }

    core::Settings& app_settings() noexcept { return app_settings_; }

    const core::Settings& app_settings() const noexcept { return app_settings_; }

    // ---- observers. The UI subscribes; it never reaches around the bus. ----
    std::function<void(std::string_view)> on_echo;
    std::function<void(const Prompt&)> on_prompt;
    std::function<void()> on_document_changed;
    std::function<void(const DispatchResult&)> on_command_finished;

    /// A setting changed. The shell listens so that writing a preference from the
    /// command line, from a script or from the AI has the same visible effect as
    /// using the menu — the menu is not a privileged client (Article 1.2).
    std::function<void(std::string_view id, core::SettingScope scope)> on_setting_changed;

    /// View state is not document state, so it is not undoable and does not go
    /// through a transaction. The command still travels the bus, so a script and
    /// a toolbar button reach the viewport by the same route.
    std::function<void(std::string_view mode, double factor)> on_view_request;

    /// Installed by the script layer. Keeps the dependency direction intact:
    /// script depends on command, never the reverse (Constitution Article 3).
    std::function<core::Status(const std::string& path)> on_run_script;

    void echo(std::string_view message) const;

private:
    core::Result<DispatchResult> run_to_completion(Session& session);
    void journal_entry(const Session& session);

    core::Document& doc_;
    Registry& reg_;
    Journal& journal_;
    UndoStack& undo_;
    Validator validator_;
    core::LayerId active_layer_{0};

    core::Settings project_settings_{core::builtin_settings(), core::SettingScopeMask::Project};
    core::Settings app_settings_{core::builtin_settings(), core::SettingScopeMask::App};

    std::unique_ptr<Transaction> batch_;
    std::string batch_label_;
    std::size_t batch_commands_{0};
};

} // namespace piricad::command
