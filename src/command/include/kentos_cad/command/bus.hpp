// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: THE COMMAND BUS.
//
//     GUI button ─┐
//     Command line ┤
//     Script ──────┼──►  BUS  ──►  Validation ──►  Transaction ──►  Document
//     AI ──────────┤                                    │
//     Batch ───────┘                                    └──►  Journal
//
// kentoscad.md §2.1 — "Everything that mutates application state is a command.
// The user interface is only one client of the command bus."
// No client on that diagram has a privilege over any other.
#pragma once

#include "kentos_cad/command/aids.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/selection.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/command/validation.hpp"
#include "kentos_cad/core/crs.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/style_library.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace kentos::command {

/// One request to run one command: what, with which arguments, from whom.
///
/// This IS the serialisable form Article 1.4 requires — a journal line, a script
/// step and an AI tool call are all this struct — which is why undo, replay,
/// scripting and audit come out of one mechanism rather than four.
struct Invocation
{
    std::string name;            ///< command id or any declared alias
    Args args;                   ///< arguments, validated before the body runs
    Origin origin{Origin::Test}; ///< recorded in the journal, never branched on
};

/// One file operation, asked for by a file command and carried out by /src/io.
///
/// The seam exists for the reason `on_run_script` exists: Article 3.2 lets io
/// depend on command and never the reverse, but io.md R4 wants every import and
/// export to be a registered command, and the registry — with the CLI help, the
/// AI schema and `kentos_docgen` behind it — lives here. So the `CommandSpec`
/// and the body live in `commands/file.cpp`, and the work arrives through
/// `Bus::on_file_request`, which `io::FileService` installs. `Category::File` was
/// reserved in `spec.hpp` from the start for exactly these commands.
struct FileRequest
{
    /// What kind of file operation this is.
    enum class Verb : std::uint8_t {
        Open,        ///< replace the document with a native project file
        Save,        ///< write the native project file the document belongs to
        SaveAs,      ///< write it somewhere else and belong there from now on
        Import,      ///< merge an external dataset into the current document
        Export,      ///< write the current document out in an external format
        ExportStyle, ///< write ONE layer's symbology as a QGIS QML style file

        /// Read a surveyed point list — `nokta no, Y, X, [Z], [kod]` — and put a
        /// point entity in the drawing for each row. The first file a Turkish
        /// surveyor opens; see `io/point_list.hpp`.
        ImportPoints,

        /// Write the drawing's points back out in the same shape.
        ExportPoints,
    };

    Verb verb{Verb::Open}; ///< which operation to carry out
    std::string path;      ///< empty on Save when the document already has a path
    std::string format;    ///< driver id for Import/Export; empty = infer from the path
    std::string layer;     ///< ExportStyle: which layer's symbology to write

    /// ImportPoints / ExportPoints: true when the file's columns run
    /// `no X Y` instead of the Turkish `no Y X`. Stated by the user, never
    /// guessed — no heuristic can tell a 485 320 easting from a northing.
    bool swapped_axes{false};

    /// ExportPoints: the entities whose CORNERS are written, one row per vertex,
    /// numbered `key.n`. Empty means the drawing's point entities, which is what
    /// a surveyor taking a list back to the field wants; a parcel's corners are
    /// what the same surveyor wants for a stake-out, and both are the same file.
    std::vector<std::uint64_t> entities;

    /// Import: the layers to read, empty meaning all of them. What the import
    /// wizard's tick boxes become — and, because it is an ordinary parameter, what
    /// a script or the command line can state just as well (Article 1.2).
    std::vector<std::string> layers;

    /// Import: the attribute FIELDS to read as columns, by the names the file
    /// gives them; a single `*` means every field, and empty means none — the
    /// behaviour every drawing imported so far was written with. A Shapefile or a
    /// GeoPackage carries a table beside its geometry, and a parcel that arrives
    /// without its ada and parsel numbers is half a parcel.
    std::vector<std::string> fields;

    /// The calling command's own transaction, so an import is ONE undo step and
    /// rolls back whole (io.md R17). Null for the verbs that do not mutate the
    /// document through a transaction.
    Transaction* tx{nullptr};

    /// The calling command's session, so the file work can be handed to a job
    /// host (job.hpp) when there is one. Null runs the read in place.
    Session* session{nullptr};

    /// Export: the DXF release year to write (`surum=2013`); 0 means the default.
    int version{0};
};

/// One database operation, asked for by `VERİTABANI` and carried out by /src/io.
///
/// A SEPARATE STRUCT FROM `FileRequest`, not extra verbs on it, because the two
/// address different things and sharing a field would mean naming one `path` and
/// meaning "table" half the time. A database target is a connection string, a
/// table, or a stored project's name; a file target is a path. Same seam, same
/// reason (Article 3.2), different nouns.
struct DatabaseRequest
{
    /// What to do. Deliberately small: this is the command surface a script and
    /// the AI see, and each verb is one sentence a user would say out loud.
    enum class Verb : std::uint8_t {
        Connect,     ///< open a connection and report what the server is
        Disconnect,  ///< close it
        Tables,      ///< list the spatial tables the connection can see
        WriteLayer,  ///< write one layer out as an ordinary spatial table
        SaveProject, ///< store the whole drawing under a name
        OpenProject, ///< replace the drawing with a stored one
        Projects,    ///< list what is stored
        DropProject, ///< remove one stored project
    };

    Verb verb{Verb::Connect}; ///< which operation to carry out

    /// Connect: the libpq connection string. WriteLayer: the table to write.
    /// SaveProject / OpenProject: the name to store under or read back.
    std::string target;

    /// WriteLayer: which layer. Empty means the active one.
    std::string layer;

    /// The calling command's transaction, for the verbs that mutate the document.
    /// Null for everything else, exactly as `FileRequest` uses it.
    Transaction* tx{nullptr};
};

/// What a PRINT is: a window of the drawing onto a sheet of paper, or the profile
/// that describes such a sheet.
///
/// The same seam as `FileRequest` and `DatabaseRequest`, for the same reason:
/// `/src/command` owns the commands (`YAZDIR`, `YAZDIRMAPROFİLİ`) and knows
/// nothing about Qt, printers or PDF; the application owns the WORK — a
/// `QPdfWriter`, a `QPrinter`, the profile file in the user's configuration
/// directory — and installs `Bus::on_print_request`. A headless client has no
/// engine and the commands say so.
///
/// A PROFILE is a named sheet: paper, orientation, resolution, margin. Exactly
/// one is the default, and it is what the toolbar's plain YAZDIR uses. Profiles
/// are APPLICATION state, like a printer preference (model.md R39): they do not
/// travel with the document, and the journal records what a print RESOLVED
/// them to (profile name plus every explicit override) rather than the sheet's
/// bytes.
struct PrintRequest
{
    /// What to do. Deliberately small, like `DatabaseRequest::Verb`: this is the
    /// command surface a script and the AI see, and each verb is one sentence a
    /// user would say out loud.
    enum class Verb : std::uint8_t {
        Profiles,      ///< list the profiles, the default marked
        SetProfile,    ///< add a profile, or replace the one of that name
        RemoveProfile, ///< remove one; the last one cannot go
        SetDefault,    ///< make one the default
        ToPdf,         ///< write `window` onto a sheet, as a PDF at `path`
        ToPrinter,     ///< send `window` onto a sheet, to `printer`
    };

    Verb verb{Verb::Profiles}; ///< which operation to carry out

    /// The profile named — to add, remove or make default; for a print, the one
    /// to start from (empty = the default).
    std::string profile;

    /// WHICH PAFTA TO PRINT, or empty for the plain window-onto-a-sheet print.
    ///
    /// A LAYOUT REPLACES THE PROFILE AND THE WINDOW BOTH: it carries its own
    /// paper, its own margin and a map frame that already knows where it looks
    /// (`core/layout.hpp`). So `pafta=` and `pencere=`/`merkez=` are alternatives,
    /// and giving both is refused rather than silently letting one win.
    std::string layout;

    // ---- the sheet, as a profile field or as a print's override --------------
    // An empty string, a zero and a `-1` each mean NOT GIVEN: the profile's own
    // value stands. Paper dimensions are the PORTRAIT ones; `landscape` turns them.
    std::string paper;          ///< `A4`, `A3`, `A2`, `A1`, `A0`, `A5`, or `ozel`
    std::int64_t width_mm{0};   ///< for `ozel`; otherwise the paper's
    std::int64_t height_mm{0};  ///< for `ozel`; otherwise the paper's
    std::int8_t landscape{-1};  ///< 1 yatay, 0 dikey, -1 not given
    std::int64_t dpi{0};        ///< output resolution
    std::int64_t margin_mm{-1}; ///< the same on all four sides

    // ---- the print -----------------------------------------------------------
    core::Box2 window; ///< the part of the drawing that goes on the sheet

    /// THE OTHER WAY TO SAY THE SAME THING, and the one a surveyor means: the
    /// sheet's CENTRE and its SCALE. A pafta is "1:1000, centred on this
    /// corner", not "these two arbitrary corners" — and only the print engine
    /// can turn the pair into a window, because the third number is the
    /// profile's printable size in millimetres of paper.
    ///
    /// When `has_centre` is set the window is computed from these and
    /// `window` is ignored.
    bool has_centre{false};
    core::Point2 centre{};
    std::int64_t scale{0}; ///< the denominator of 1:N; 0 = the project's plan scale
    std::string path;      ///< ToPdf: where the file goes
    std::string printer;   ///< ToPrinter: the printer's name; empty = the system default
    std::string title;     ///< PDF metadata
    std::string author;    ///< PDF metadata

    /// PDF encryption. Never journalled: `YAZDIR` reads them and records
    /// nothing, exactly as `VERİTABANI` redacts its password (`redact_conninfo`).
    std::string user_password;  ///< needed to OPEN the file; empty = none
    std::string owner_password; ///< needed to change permissions; empty = none
    bool allow_print{true};     ///< what a reader without the owner password may do
    bool allow_copy{true};
    bool allow_modify{true};
};

/// What `YAPAYZEKAMODELİ` asks the application to do with a model profile.
///
/// THE SAME SEAM AS `PrintRequest`, AND FOR THE SAME REASON. A provider profile
/// is an endpoint, a dialect and the NAME of a keychain entry (`ai/provider.hpp`):
/// `/src/ai` owns that record and can neither read a file nor open a socket
/// (`.claude/ai.md` P10), and `/src/command` owns the command, its parameters and
/// what the journal records. The application owns the machinery — the JSON file in
/// the user's configuration directory, the operating system's key store, the
/// network — and installs `Bus::on_ai_provider_request`. A build with nothing
/// attached says so rather than pretending it saved a profile.
///
/// THE KEY IS NOT IN HERE, IN EITHER DIRECTION, and that is the whole point.
/// `key_ref` is the NAME of the entry that holds the credential; the credential
/// itself never travels as a command argument, a `Value`, a journal line or an
/// answer (CLAUDE.md 5.21, ai.md P11). The API key is typed into the key store
/// through the settings page and read at use by the transport.
struct AiProviderRequest
{
    /// What to do. Deliberately small, like `PrintRequest::Verb`: each verb is
    /// one sentence a user would say out loud, and the words are the command's
    /// own `islem` values.
    enum class Verb : std::uint8_t {
        Profiles,      ///< list the profiles, the default marked
        SetProfile,    ///< add a profile, or replace the one of that name
        RemoveProfile, ///< remove one; the last one cannot go
        SetDefault,    ///< make one the default
        Test,          ///< speak to the endpoint and report what came back
    };

    Verb verb{Verb::Profiles}; ///< which operation to carry out

    /// The profile named — to add, remove, make default or test.
    std::string name;

    // ---- the profile's fields, as `SetProfile` overrides ---------------------
    // An empty string, a zero and a `-1` each mean NOT GIVEN, exactly as
    // `PrintRequest` spells it: the built-in default stands for anything the
    // line did not say.

    /// The dialect's wire id — `openai_chat`, `openai_responses`,
    /// `anthropic_messages`, `ollama_native`. A STRING rather than an enum
    /// because Article 3.2 forbids `/src/command` from naming `/src/ai`, and the
    /// four words are `ai::dialect_id`'s own (the command resolves the word
    /// before sending, so an unknown one never reaches here).
    std::string dialect;

    std::string base_url; ///< scheme, host, port and the vendor's prefix
    std::string path;     ///< the endpoint under the base: `/chat/completions`
    std::string model;    ///< the model id exactly as the endpoint names it

    /// WHICH KEY-STORE ENTRY HOLDS THE KEY — never the key (ai.md P11).
    std::string key_ref;

    std::int64_t context{0}; ///< context window in tokens; 0 = not given

    /// The output cap. `-1` IS "NOT GIVEN" HERE, not 0: a profile's 0 means "do
    /// not send the field at all" (`ai::ProviderProfile::max_tokens`), so the
    /// two cannot share a value.
    std::int64_t max_tokens{-1};

    /// Sampling temperature, or a negative number for NOT GIVEN — which is a
    /// real configuration of its own, because the o-series and gpt-5 reject any
    /// value but the default. The accepted span is 0–2, so -1 cannot collide.
    double temperature{-1.0};

    std::int8_t stream{-1};         ///< 1 streamed, 0 whole, -1 not given
    std::int8_t reasoning_text{-1}; ///< 1 show the thinking text, 0 hide it, -1 not given
    std::int8_t tools{-1};          ///< 1 send the tool catalogue, 0 do not, -1 not given
};

/// A PostgreSQL connection string with its password taken out.
///
/// ONE implementation, shared by everything that shows or stores a connection
/// string: `VERİTABANI` before it journals, and the database window before it
/// displays. A journal is a plain JSONL file that gets attached to bug reports
/// and committed beside projects, and a screenshot travels further than either.
///
/// Handles both forms libpq accepts, because the second one is easy to forget:
///
///   `host=x password=SECRET`            -> `password=***`
///   `host=x password='SEC RET'`         -> `password=***`
///   `postgresql://user:SECRET@host/db`  -> `postgresql://user:***@host/db`
///
/// Replaces the VALUE and keeps the FIELD. A replay that silently dropped the
/// password would look like a connection that never needed one.
std::string redact_conninfo(std::string_view conninfo);

/// What happened when a command ran.
///
/// Returned to every client identically. `mutated` is what the shell watches to
/// know whether to repaint, and `ops` is what the undo stack watches to know
/// whether there is anything to undo.
struct DispatchResult
{
    std::string command_id; ///< canonical id of what ran
    std::string label;      ///< the label the undo entry will carry
    std::size_t ops{0};     ///< primitive edits recorded
    bool mutated{false};    ///< whether the document changed at all
    std::string message;    ///< user-facing summary, Turkish

    /// EVERY LINE THE COMMAND SAID, in order, captured for THIS dispatch only.
    ///
    /// `Bus::on_echo` is one sink for the whole program: it puts a line on the
    /// transcript and tells the caller nothing. So a client that is not a person
    /// — a script collecting a report, an agent reading a tool result — had no
    /// way to learn what a read-only command answered, and `message` is empty on
    /// success. These are the same strings the transcript shows, teed.
    std::vector<std::string> lines;

    /// The command's own STRUCTURED answer, when it has one (`Context::report`).
    ///
    /// Empty for almost every command, and that is right: a drawing command's
    /// answer is the drawing. A query answers with data, and prose is a poor
    /// carrier for data — an agent should not have to parse a Turkish sentence to
    /// learn a layer's object count.
    core::Json report;
};

/// What the viewport is showing, answered by the shell.
///
/// NOT DOCUMENT STATE (model.md R43): never hashed, never journalled, never
/// undoable. It is here because it is the one thing a client cannot work out for
/// itself — the document does not know how big the window is — and because
/// "which corner coordinates am I looking at" is the first question any agent,
/// script or macro asks before it draws anything.
struct ViewInfo
{
    core::Box2 window{};    ///< the visible rectangle in document millimetres
    core::Point2 centre{};  ///< its centre
    double mm_per_pixel{0}; ///< document millimetres per screen pixel
    std::int64_t scale{0};  ///< denominator of the drawing scale, 1:N, 0 when unknown
    int width_px{0};        ///< viewport width, in pixels
    int height_px{0};       ///< viewport height, in pixels
    std::string crs;        ///< the CRS the coordinates are expressed in
};

class Bus
{
public:
    /// Builds the bus over the four things every command needs. All held by
    /// reference: the controller owns them and outlives the bus.
    Bus(core::Document& doc, Registry& reg, Journal& journal, UndoStack& undo);

    // ---- the single entry point for every client ----
    core::Result<DispatchResult> dispatch(const Invocation& inv);

    /// Parses and dispatches one command line. Used by the CLI widget, by macro
    /// playback and by the script engine — one grammar, one path (§3).
    core::Result<DispatchResult> execute_line(std::string_view line, Origin origin);

    /// Starts an interactive command that will ask the user for what it still
    /// needs. Takes a full command LINE, parsed by the one parser, so a button may
    /// say `SEÇ mod=KUTU` and have the mode answered while the corners are
    /// clicked; a bare name behaves exactly as before.
    ///
    /// Only the GUI uses this, and it buys the GUI no privileges: the session
    /// runs the same coroutine, validation and transaction as every other client.
    ///
    /// `origin` is what the journal records for the run: a button is `Gui`, a
    /// typed line `CommandLine`. It buys the session nothing else.
    core::Result<std::unique_ptr<Session>> begin_interactive(std::string_view line,
                                                             Origin origin = Origin::Gui);

    /// Called by Session when a command finishes. Validates, commits or rolls
    /// back, and journals.
    core::Result<DispatchResult> finish(Session& session);

    // ---- batch mode (§10.4): one validation pass, one undo step ----
    core::Status begin_batch(std::string label);
    core::Result<DispatchResult> end_batch();
    /// Discards every edit made since begin_batch(). Used when a GUI composite
    /// edit cannot finish, so Apply is all-or-nothing rather than half a symbol.
    void abort_batch();

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

    // ---- the settings service -------------------------------------------
    //
    // ONE road to a setting, whatever its scope. Before this every caller had to
    // know which of the three stores held the id it wanted, so adding a setting
    // meant finding every reader and telling it which drawer to open — and a
    // reader that guessed wrong read a default and reported it as a value.
    //
    // The scope is not the caller's business. It is DECLARED on the SettingSpec
    // (R39-R42) and the declaration is what decides where the value lives, so
    // this resolves it and the caller states only what it wants.

    /// The store a declared setting lives in, or null when nothing declares `id`.
    core::Settings* store_for(std::string_view id) noexcept;

    const core::Settings* store_for(std::string_view id) const noexcept;

    /// The value of a declared setting. An undeclared id returns an empty value —
    /// the same answer `Settings::get` gives, because a typo in an id is a caller
    /// bug and not a reason to take the program down mid-frame.
    core::SettingValue setting(std::string_view id) const;

    /// Writes a declared setting into whichever store its scope names.
    ///
    /// Calls `on_settings_changed` with that scope afterwards, so whoever owns
    /// persistence writes it out. Nothing here knows what a preferences file is:
    /// the APP store is per user and machine and the shell owns the file, exactly
    /// as `io::FileService` owns the ones under /src/io.
    /// Returns what actually changed — the canonical id, the value before, the
    /// value after any R42 clamping, and whether it was clamped. A caller that
    /// echoed the value it asked for would report a number the store refused.
    core::Result<core::SettingChange> set_setting(std::string_view id,
                                                  const core::SettingValue& value);

    /// Fired after a setting changes, with the scope that changed.
    ///
    /// The seam that fixes an asymmetry Article 1.2 forbids: preferences used to
    /// be written only when the main window was destroyed, so `TERCİH` from a
    /// SCRIPT changed the value for that run and lost it. Every client now
    /// persists the same way, because none of them does it — the owner does.
    std::function<void(core::SettingScope)> on_settings_changed;

    const core::Settings& session_settings() const noexcept { return session_settings_; }

    // ---- selection and input aids: session state, never document state ----
    //
    // model.md R43 keeps both out of `content_hash()` and out of the journal as
    // document mutations, and R44 stores the selection as `EntityKey` so it
    // survives a save, a reorder and a reload. They live on the bus rather than
    // in the canvas for the same reason the session settings do: a script and the
    // AI select and aim with the same machinery the hand does (Article 1.2).
    /// The browsable symbol shelf, for as long as this session lives.
    ///
    /// SESSION state, not document state (model.md R43): which symbols a user can
    /// pick from is a property of what they have installed, not of the drawing.
    /// A drawing carries the symbols it actually uses, interned in its own style
    /// table, so it opens the same on a machine with no library at all.
    core::StyleLibrary& style_library() noexcept { return style_library_; }

    const core::StyleLibrary& style_library() const noexcept { return style_library_; }

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

    /// Runs the job a session is parked on, OFF this thread, and calls
    /// `Session::resume_job` on this thread when it is done (job.hpp). The GUI
    /// installs it; a bus without one runs every job in place, which is what a
    /// script, the command line and a test get — the same command body, the
    /// same result (Article 1.2). Never resume the session from inside the hook.
    std::function<void(Session&)> on_job_host;
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

    /// Resolves a CRS id into a populated `core::Crs`.
    ///
    /// Installed by the geodesy module, which owns the zone catalogue; the same
    /// shape as `on_file_request`, and for the same reason. Without it a CRS keeps
    /// its id and stays unresolved, which is honest: a build with no geodesy module
    /// genuinely does not know that TM30 is EPSG:5254.
    std::function<core::Crs(std::string_view id)> on_crs_resolve;

    /// View state is not document state, so it is not undoable and does not go
    /// through a transaction. The command still travels the bus, so a script and
    /// a toolbar button reach the viewport by the same route.
    std::function<void(std::string_view mode, double factor)> on_view_request;

    /// WHAT THE VIEW IS SHOWING. The write hooks above move it; nothing could
    /// read it, so no command could answer "where am I looking" and no agent
    /// could frame a window before drawing in it. Unset means no viewport is
    /// attached — a headless run — and the reading command says so rather than
    /// inventing a rectangle.
    std::function<ViewInfo()> on_view_query;

    /// KAYDIR asks the view to move so that `from` ends up where `to` is.
    ///
    /// A separate hook rather than another `on_view_request` mode, because this
    /// one carries two document points: pushing them through a string mode and a
    /// double would be inventing a second, lossy encoding for a coordinate
    /// (Article 1.4). A headless client leaves it unset and the command says so.
    std::function<void(core::Point2 from, core::Point2 to)> on_pan_request;

    /// What an AI-facing command asks the application to do.
    ///
    /// TWO FAMILIES BEHIND ONE SHAPE, for the reason `PrintRequest` has one: the
    /// command layer owns the words, the parameters and what the journal
    /// records, and the application owns the machinery — the plan store, the
    /// suggestion card, the listener's socket. A build with nothing attached
    /// says so rather than pretending (`on_ai_request` unset).
    struct AiRequest
    {
        /// What is being asked. The words are the command's own `islem` values.
        enum class Verb : std::uint8_t {
            SuggestionApply,  ///< ÖNERİ islem=uygula
            SuggestionReject, ///< ÖNERİ islem=reddet
            SuggestionState,  ///< ÖNERİ islem=durum
            SuggestionList,   ///< ÖNERİ islem=listele
            ServerStart,      ///< MCPSUNUCU islem=baslat
            ServerStop,       ///< MCPSUNUCU islem=durdur
            ServerState,      ///< MCPSUNUCU islem=durum
            ServerToken,      ///< MCPSUNUCU islem=belirtec — mints a new one
        };

        Verb verb{Verb::SuggestionList}; ///< which of the eight this request is
        std::string plan;                ///< which suggestion, for the four suggestion verbs
        std::int64_t port{0};            ///< an override for this start only; 0 = the setting
    };

    /// Installed by `app::AiService`. Returns the Turkish line the command
    /// echoes, or the refusal the user sees.
    ///
    /// A TOKEN IS NEVER IN HERE, in either direction: minting one returns its
    /// fingerprint and where to read it, never the secret (CLAUDE.md 5.21).
    /// What both AI hooks answer with: the Turkish line the command echoes, or
    /// the refusal the user sees.
    using AiReply = Task<core::Result<std::string>>;

    /// The suggestion and server hook itself.
    std::function<AiReply(const AiRequest&)> on_ai_request;

    /// Installed by `app::ProviderService`. Returns the Turkish line
    /// `YAPAYZEKAMODELİ` echoes, or the refusal the user sees.
    ///
    /// A SECOND HOOK RATHER THAN A VERB ON `AiRequest`, because the two address
    /// different things and sharing a field would mean naming one `plan` and
    /// meaning "profile name" half the time — the reason `DatabaseRequest` is not
    /// extra verbs on `FileRequest`. It is also a different OWNER: the plan store
    /// and the listener belong to `AiService`, the profile file and the key store
    /// to `ProviderService`, and one `std::function` has one installer.
    ///
    /// NO CREDENTIAL CROSSES IT (CLAUDE.md 5.21): a profile carries the NAME of
    /// its key-store entry, and the answer names the endpoint and the model.
    /// Named, because the unaliased type wraps and a wrapped declaration reads
    /// as two: the continuation line looks like a declaration of its own to a
    /// reader and to `scripts/ci-gate-comments.sh` alike.
    using AiProviderHandler = std::function<AiReply(const AiProviderRequest&)>;

    /// The hook itself. See the paragraphs above for who installs it and why it
    /// is separate from `on_ai_request`.
    AiProviderHandler on_ai_provider_request;

    /// Installed by the script layer. Keeps the dependency direction intact:
    /// script depends on command, never the reverse (Constitution Article 3).
    std::function<core::Status(const std::string& path)> on_run_script;

    /// Installed by `io::DatabaseService`. Unset means this build has no database
    /// engine attached — either it was compiled without PostGIS or nothing wired
    /// the service — and `VERİTABANI` says so rather than pretending it connected.
    std::function<Task<core::Result<std::string>>(const DatabaseRequest&)> on_database_request;

    /// Installed by `io::FileService`, for the same reason and in the same shape.
    /// Returns the Turkish line the command echoes, or the error the user sees.
    /// Unset means no file engine is attached, and the file commands say so
    /// rather than pretending the save happened.
    std::function<Task<core::Result<std::string>>(const FileRequest&)> on_file_request;

    /// Installed by the application's print service (`app::PrintService`), in the
    /// same shape. Unset means nothing can print here — a headless test, a build
    /// without a printing engine — and `YAZDIR` says so rather than pretending
    /// a sheet came out.
    std::function<Task<core::Result<std::string>>(const PrintRequest&)> on_print_request;

    /// Asked by `core.saveas` and `core.export` before they build their request:
    /// the file the document currently belongs to, so the transcript and the GUI
    /// dialog can start where the user last was. NOT document state (model.md
    /// R43) — never hashed, never journalled, never undoable.
    std::function<std::string()> on_current_file;

    void echo(std::string_view message) const;

    /// Collects every `echo` into `sink` until the returned guard dies.
    ///
    /// A GUARD RATHER THAN A FLAG, because dispatches nest: a batch runs commands
    /// inside a command, and a sink left switched on would hand the outer caller
    /// the inner command's lines. The guard restores whatever was there before.
    class EchoCapture
    {
    public:
        /// Starts collecting into `sink`; both outlive the guard.
        EchoCapture(const Bus& bus, std::vector<std::string>& sink);
        ~EchoCapture();
        /// Not copyable: two guards over one sink would restore it twice.
        EchoCapture(const EchoCapture&) = delete;
        /// Not assignable, for the same reason.
        EchoCapture& operator=(const EchoCapture&) = delete;

    private:
        const Bus& bus_;
        std::vector<std::string>* previous_{nullptr};
    };

private:
    core::Result<DispatchResult> run_to_completion(Session& session);
    void journal_entry(const Session& session);

    core::Document& doc_;
    Registry& reg_;
    Journal& journal_;
    UndoStack& undo_;
    Validator validator_;
    core::LayerId active_layer_{0};

    /// Where `echo` also writes while an `EchoCapture` is alive. `mutable`
    /// because `echo` is const and saying a line is not a change to the bus.
    mutable std::vector<std::string>* echo_sink_{nullptr};

    core::Settings project_settings_{core::builtin_settings(), core::SettingScopeMask::Project};
    core::Settings app_settings_{core::builtin_settings(), core::SettingScopeMask::App};
    core::Settings session_settings_{core::builtin_settings(), core::SettingScopeMask::Session};

    Selection selection_{};
    InputAids aids_{};
    core::StyleLibrary style_library_{};

    std::unique_ptr<Transaction> batch_;
    std::string batch_label_;
    std::size_t batch_commands_{0};
    std::uint64_t batch_revision_at_start_{0};
};

} // namespace kentos::command
