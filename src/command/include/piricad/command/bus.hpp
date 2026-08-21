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

#include "piricad/command/aids.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/selection.hpp"
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

/// One file operation, asked for by a file command and carried out by /src/io.
///
/// The seam exists for the reason `on_run_script` exists: Article 3.2 lets io
/// depend on command and never the reverse, but io.md R4 wants every import and
/// export to be a registered command, and the registry — with the CLI help, the
/// AI schema and `piricad_docgen` behind it — lives here. So the `CommandSpec`
/// and the body live in `commands/file.cpp`, and the work arrives through
/// `Bus::on_file_request`, which `io::FileService` installs. `Category::File` was
/// reserved in `spec.hpp` from the start for exactly these commands.
struct FileRequest
{
    enum class Verb : std::uint8_t {
        Open,   ///< replace the document with a native project file
        Save,   ///< write the native project file the document belongs to
        SaveAs, ///< write it somewhere else and belong there from now on
        Import, ///< merge an external dataset into the current document
        Export, ///< write the current document out in an external format
    };

    Verb verb{Verb::Open};
    std::string path;   ///< empty on Save when the document already has a path
    std::string format; ///< driver id for Import/Export; empty = infer from the path

    /// The calling command's own transaction, so an import is ONE undo step and
    /// rolls back whole (io.md R17). Null for the verbs that do not mutate the
    /// document through a transaction.
    Transaction* tx{nullptr};
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

    // The SESSION store holds the input aids — snap modes, ortho, polar step,
    // snap-to-grid. R39 makes them transient: never written to the file, never
    // written to the preferences file, gone when the process exits. They live on
    // the bus rather than in the canvas because R43 does not make them private to
    // the mouse: a script and the AI aim with the same aids the hand does
    // (CLAUDE.md 1.2). Before this they were declared with nowhere to live, so no
    // client at all could read or write them.
    core::Settings& session_settings() noexcept { return session_settings_; }

    const core::Settings& session_settings() const noexcept { return session_settings_; }

    // ---- selection and input aids: session state, never document state ----
    //
    // model.md R43 keeps both out of `content_hash()` and out of the journal as
    // document mutations, and R44 stores the selection as `EntityKey` so it
    // survives a save, a reorder and a reload. They live on the bus rather than
    // in the canvas for the same reason the session settings do: a script and the
    // AI select and aim with the same machinery the hand does (Article 1.2).
    Selection& selection() noexcept { return selection_; }

    const Selection& selection() const noexcept { return selection_; }

    InputAids& aids() noexcept { return aids_; }

    const InputAids& aids() const noexcept { return aids_; }

    /// The aid settings in force, assembled from the app and session stores and
    /// memoised against their revision counters (see `aids.hpp`). The reference is
    /// valid until the next settings write or view-scale change.
    const AidSettings& aid_settings() const
    {
        return aids_.settings(app_settings_, session_settings_);
    }

    // ---- observers. The UI subscribes; it never reaches around the bus. ----
    std::function<void(std::string_view)> on_echo;
    std::function<void(const Prompt&)> on_prompt;
    std::function<void()> on_document_changed;
    std::function<void(const DispatchResult&)> on_command_finished;

    /// The selection changed. The canvas listens so that selecting from the
    /// command line, from a script or from a rubber-band drag all light the same
    /// entities up — the mouse is not a privileged client (Article 1.2).
    std::function<void()> on_selection_changed;

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

    /// Installed by `io::FileService`, for the same reason and in the same shape.
    /// Returns the Turkish line the command echoes, or the error the user sees.
    /// Unset means no file engine is attached, and the file commands say so
    /// rather than pretending the save happened.
    std::function<Task<core::Result<std::string>>(const FileRequest&)> on_file_request;

    /// Asked by `core.saveas` and `core.export` before they build their request:
    /// the file the document currently belongs to, so the transcript and the GUI
    /// dialog can start where the user last was. NOT document state (model.md
    /// R43) — never hashed, never journalled, never undoable.
    std::function<std::string()> on_current_file;

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
    core::Settings session_settings_{core::builtin_settings(), core::SettingScopeMask::Session};

    Selection selection_{};
    InputAids aids_{};

    std::unique_ptr<Transaction> batch_;
    std::string batch_label_;
    std::size_t batch_commands_{0};
};

} // namespace piricad::command
