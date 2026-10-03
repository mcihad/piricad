// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the bridge between Qt and the command bus.
//
// Constitution Article 1: the user interface is a CLIENT of the command bus and
// has no privileges. Every widget in this application reaches the document
// through this controller, and the controller reaches it only through the bus.
// There is no other path.
#pragma once

#include "piricad/app/ai_service.hpp"
#if PIRICAD_HAVE_MCP
#include "piricad/app/mcp_service.hpp"
#endif
#include "piricad/app/layout_templates.hpp"
#include "piricad/app/print_service.hpp"
#include "piricad/app/provider_service.hpp"
#include "piricad/app/python_api_info.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/document.hpp"
#include "piricad/domain/geodesy/crs_service.hpp"
#include "piricad/io/database.hpp"
#include "piricad/io/service.hpp"
#include "piricad/script/json_runner.hpp"

#if PIRICAD_HAVE_PYTHON
#include "piricad/script/python_runner.hpp"
#endif

#include <QObject>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QThread;

namespace piricad::app {

/// The outbound wire, held by pointer so the socket does not reach every
/// translation unit that includes this header; see ai_transport.hpp.
class AiTransport;

class Controller : public QObject
{
    Q_OBJECT

public:
    /// Builds the document, registry, journal, undo stack and bus, and registers
    /// every built-in command. One controller is one open drawing.
    explicit Controller(QObject* parent = nullptr);
    ~Controller() override;

    // ---- the only ways a widget may act on the document ----
    void runLine(const QString& line, command::Origin origin = command::Origin::CommandLine);

    /// Runs several lines as ONE gesture: one validation pass, one undo step,
    /// one line in the echo (`Bus::begin_batch`, piricad.md §10.4).
    ///
    /// WHAT IT IS FOR. Some things a user does in one click are several commands
    /// — hiding eleven picked layers is eleven `KATMANGÖRÜNÜM` calls — and eleven
    /// undo steps for one click is not what Ctrl+Z means. `label` is what the
    /// batch is called.
    ///
    /// IT IS NOT A PRIVATE ROAD. The lines are the lines a script would carry,
    /// through the same parser and the same bus, and a script already runs inside
    /// a batch for the same reason (ui.md P3, script.md). A failure part way
    /// through rolls the WHOLE batch back rather than leaving half of it applied
    /// (Article 1.6).
    void runLines(const QStringList& lines, const QString& label,
                  command::Origin origin = command::Origin::Gui);

    /// Runs one command line and returns its bus result to an interaction that
    /// must keep its dialog open on failure. Other UI clients use `runLine()`.
    core::Result<command::DispatchResult>
    runLineResult(const QString& line, command::Origin origin = command::Origin::CommandLine);
    void runCommand(const QString& line); ///< toolbar / menu — same road as a script

    /// Runs a Python snippet through `core.python`.
    ///
    /// THE COMMAND AND NOT THE RUNNER, even though this class owns the runner.
    /// A panel that called `PythonRunner` directly would be the private entry
    /// point Article 1.2 forbids: the snippet would miss the journal line the bus
    /// writes for the command, and the same act would be unreachable from the
    /// command line. The argument is passed as a built `Invocation` rather than
    /// as text, because Python source contains quotes and newlines and rendering
    /// it into a command line to be parsed back is a round trip that cannot be
    /// made lossless.
    void runPython(const QString& source);

    /// The `piricad.cad` callable names this build exposes, for completion and
    /// for the editor's highlighting.
    ///
    /// From the REGISTRY, so a command added today is completable today and there
    /// is no second list (CLAUDE.md 5.10). Answered in every build, including one
    /// with no Python: the names are a fact about the command set, not about the
    /// interpreter.
    QStringList pythonApiNames() const;

    /// Every callable with its keywords, for completion and the signature hint.
    ///
    /// THE SAME PROJECTION THE INTERPRETER USES, from the same registry and with
    /// the same type spelling (`script::python_type_name`). An editor that
    /// described a different surface from the one that runs would be worse than
    /// an editor with no hints at all.
    QVector<PythonCallable> pythonApi() const;

    /// The line that armed the running session, or empty. Read by
    /// `MainWindow::syncToolSelection` to light the exact button pressed.
    const QString& armedLine() const noexcept { return armedLine_; }

    /// WHAT STARTS THE LAST COMMAND AGAIN (`TERCİH son_komut`): a key — an
    /// empty Enter or Space — or a left click on empty ground.
    enum class Repeat {
        Key,
        Click,
    };

    /// Whether `how` starts the last command again under the user's
    /// `son_komut`: `enter` answers the key, `tik` the key and the click, and
    /// `kapali` neither.
    bool repeats(Repeat how) const;

    /// The line the last command that can come back was started with, as it
    /// comes back (`command::repeat_line`): `DAİRE yontem=3n`, never its points.
    /// Empty until one has run. A command that cannot come back — KAYDET, an
    /// import, a script — leaves it as it was, which is Netcad's rule: a
    /// command used once is not the one repeated.
    const QString& lastLine() const noexcept { return lastLine_; }

    /// RUNS A WHOLE LINE TO ITS END AT ONCE, the way a script runs it: a command
    /// that takes a run of points and is given them writes them and stops, rather
    /// than asking for more as a typed line does. For a line the program wrote
    /// complete — a command's offer (`command::Offer`). A waiting command is put
    /// down first, as typing another command puts it down; one whose job is out
    /// is not interrupted. The line is echoed as a typed one is.
    void runWhole(const QString& line, command::Origin origin);

    /// Starts the last command again, the way typing its line does: the same
    /// parser, the same bus, a journal line of its own (Article 1.2). The line
    /// is echoed as a typed one is. Nothing happens — and false comes back —
    /// while a command is running or a batch is open, with nothing to repeat,
    /// or when `son_komut` does not answer `how`.
    bool repeatLast(Repeat how);

    /// Dispatches a fully built invocation. This is `Bus::dispatch`, the same
    /// overload the JSON runner and the AI use (`.claude/command.md` R2): the
    /// canvas needs it because a rubber-band box carries `Point2` values that must
    /// not be round-tripped through formatted text to become a command line.
    void runInvocation(const command::Invocation& invocation);
    /// Starts a command the way a button does: the arguments the line carries are
    /// answered, everything else is asked for. `origin` is what the journal
    /// records — a typed line that then prompts is `CommandLine`.
    void beginInteractive(const QString& line, command::Origin origin = command::Origin::Gui);

    /// Starts a command for ONE edit that no tool owns: a grip clicked on the
    /// canvas (TODOS C-07). The same road as `beginInteractive` — same parser,
    /// same bus, same journal line — but when it ends nothing is re-armed and
    /// the selection stays, so the next grip is one click away, as in every
    /// CAD. `oneShot()` says so to the window deciding what to re-arm.
    void beginOneShot(const QString& line, command::Origin origin = command::Origin::Gui);

    /// Whether the session running — or the one that just ended — was started
    /// by `beginOneShot`.
    bool oneShot() const noexcept { return oneShot_; }

private:
    /// What both starts share; `one_shot` is what tells them apart.
    void startInteractive(const QString& line, command::Origin origin, bool one_shot);

public:
    /// Answers the running command's point prompt with a coordinate AS STATED —
    /// typed on the command line, or computed exactly by the shell (the point
    /// that lands an area on its target) — and kept exactly as given.
    void supplyPoint(core::Point2 world);

    /// Answers it with a place a HAND pointed at: the raw world point under a
    /// click, which the input aids in the command layer turn into the point meant
    /// (`command::Value::aimed_point`). Only a pointer makes one: the aperture is
    /// pixels, and a coordinate somebody wrote down is not a guess (TODOS F-03).
    void supplyAimedPoint(core::Point2 world);

    /// Answers the running command's prompt with a piece of TEXT.
    ///
    /// A command asks for what it needs — `ctx.point`, `ctx.number`, `ctx.text` —
    /// and the client answers in kind. Until this existed the shell could answer
    /// only points, so METİN took its anchor from the canvas and then waited for
    /// a string nothing could deliver: the prompt reached the command line's
    /// placeholder while focus stayed on the canvas, and the command hung.
    void supplyText(const QString& text);

    /// Answers the running command's prompt with a set of OBJECTS.
    ///
    /// The canvas calls this when the user presses Enter having picked what the
    /// command asked for; the picking itself went through `SEÇ`, exactly as it
    /// does when no command is running, so there is one selection road and not
    /// two (Article 1.2).
    void supplyObjects(const std::vector<std::int64_t>& ids);

    /// Hands the running command whatever is picked, if it is asking for objects.
    ///
    /// ONE BODY, because "I am done pointing" arrives from three places — Enter on
    /// the canvas, Enter on the command line, and a right click — and Qt sends a
    /// key to whatever holds focus, which after startup is the command line. A
    /// canvas-only Enter therefore committed nothing at all: the tools stayed
    /// armed forever, every further click just re-selected, and the six of them
    /// read as dead.
    ///
    /// Returns true when the gesture belonged to a running object prompt, so the
    /// caller knows whether to keep handling the key.
    bool supplyPickedObjects();

    /// Answers the running command's prompt with a NUMBER — a distance, a scale,
    /// an angle. The command line reaches for this when `Prompt::kind` says a
    /// quantity would satisfy the prompt; without it OFSET could be started and
    /// never finished.
    void supplyNumber(double value);

    /// What the running command is asking for, so a client can offer the right
    /// editor. `ParamKind::Point` when nothing is running, which is what the
    /// canvas does by default anyway.
    command::ParamKind promptKind() const;
    /// The kind of object the waiting prompt acts on, or `core::kNoKind` for
    /// any (`command::Prompt::pick_kind`).
    core::KindId promptPickKind() const;

    /// How many objects the waiting question takes, 0 for any number
    /// (`command::Prompt::pick_most`): one, and the click that names it answers.
    std::size_t promptPickMost() const;

    void cancelInteractive();

    /// Whether the running command's prompt can take its newest point back
    /// (`Prompt::can_retract`) — ÇİZGİ, ÇOKLUÇİZGİ, ALAN and SPLINE between two
    /// points. ⌫ and Ctrl+Z ask this before they reach for SİL and GERİAL.
    bool canRetract() const;

    /// TAKES THE NEWEST POINT BACK and leaves the rest of the run on the canvas
    /// (`Session::retract`). The same for a key, a menu and a typed `G`/`U`; false
    /// when the prompt has nothing to take back.
    bool retractPoint();

    /// Answers the running command's prompt with one of its WORDS (`command::Prompt::words`),
    /// the way the typed line `K` does: the keyboard and the ribbon's button reach the one
    /// `Session::choose`. False, with the refusal said, when the prompt takes no such word.
    bool chooseWord(const QString& id);

    /// FINISHES the running command the way the right mouse button means it: the
    /// open-ended shape closes on what it has, and the tool that started it stays
    /// armed for the next one. The same unwinding as `cancelInteractive` — the
    /// command takes its ESC path — with `dismissed` false on the way out, which
    /// is what tells the shell to arm the tool again rather than put it away.
    void finishInteractive();

    /// PUT IT ALL DOWN — Esc, and the right button outside a run of points: the
    /// running command ends the way Esc ends it (a run keeps what it made,
    /// nothing re-arms) and the selection goes with it, so the hand is back on
    /// the select tool with nothing picked. Netcad's rule, and the user's: the
    /// right button and Esc always let go of what is selected and of the edit
    /// under way.
    void cancelAll();

    /// Empties the selection through `SEÇ mod=TEMİZLE`, when there is one — the
    /// command, not a reach into the bus (Article 1.2).
    void clearSelection();

    /// Whether the waiting command is in the middle of a RUN of points with at
    /// least one of them fixed — the next corner of ALAN, the next point of ÖLÇ:
    /// what the right button FINISHES rather than cancels.
    bool inRun() const;

    /// The selection, resolved to dense slots for one frame. Recomputed only when
    /// the selection or the document changes, never per frame: model.md R2 keeps
    /// keys out of the frame path, and R44 keeps slots out of the selection.
    const std::vector<core::EntityId>& selectedSlots() const noexcept { return selected_slots_; }

    void refreshSelection();

    command::Session* session() const noexcept { return session_.get(); }

    /// How many interactive runs have started, ever. A deferred action — the
    /// tool column re-arming the tool that just finished — reads it when it is
    /// queued and again when it runs, and stands down if another run started in
    /// between: a re-arm that lands late must not cancel the command the user
    /// has since reached for.
    std::uint64_t sessionsBegun() const noexcept { return sessionsBegun_; }

    /// How far the running job has come, 0..1000, or -1 when no job is running
    /// or the job does not count (`Job::permille`).
    int jobPermille() const noexcept;

    bool awaitingInput() const;

    core::Document& document() noexcept { return document_; }

    const core::Document& document() const noexcept { return document_; }

    command::Bus& bus() noexcept { return bus_; }

    command::Registry& registry() noexcept { return registry_; }

    /// What the status strip prints about the spatial database: the redacted
    /// target when a connection is open, why not when it is not. Article 2.9
    /// makes PostGIS a store rather than an export target, so whether the
    /// connection is up belongs on screen next to the frame budget.
    const io::DatabaseService& database() const noexcept { return database_; }

    /// The print engine behind `YAZDIR`: the profiles, and the sheet renderer
    /// the toolbar menu, the settings page and the print dialog read.
    /// The AI layer's machinery: the plan store, the audit log, the handles and
    /// the tool catalogue. Read by the suggestion card, the chat dock and the
    /// MCP listener; written only through the bus and through `decide`.
    AiService& aiService() noexcept { return ai_; }

#if PIRICAD_HAVE_MCP
    /// The agent listener, or null in a build without Qt HttpServer. The status
    /// strip and the settings page both ask it for its state.
    McpService* mcpService() noexcept { return mcp_.get(); }
#endif

    const AiService& aiService() const noexcept { return ai_; }

    PrintService& printService() noexcept { return prints_; }

    const PrintService& printService() const noexcept { return prints_; }

    /// The model provider profiles behind `YAPAYZEKAMODELİ`: the store, the file
    /// they live in and the key store their API keys live in. Read by the
    /// settings page and the chat dock; written only through the bus.
    ProviderService& providerService() noexcept { return providers_; }

    const ProviderService& providerService() const noexcept { return providers_; }

    /// The office's layout templates: the folder they live in and the verbs of
    /// `PAFTAŞABLON` over it.
    LayoutTemplates& layoutTemplates() noexcept { return templates_; }

    /// The ONE outbound wire, shared by the connection test and the chat panel.
    ///
    /// ONE, BECAUSE A SECOND ONE IS A SECOND CREDENTIAL PATH. The transport is
    /// the only object in the program that puts a key on a request (CLAUDE.md
    /// 5.21) — it gets the value from `SecretResolver`, which is the only object
    /// that asks the operating system for one — and two of either would be two
    /// places to audit. It is
    /// told which profile it is serving immediately before each send, through
    /// `AiTransport::useProfile` — which is why `ProviderService` takes a binder
    /// rather than making that call itself.
    AiTransport& aiTransport() noexcept { return *transport_; }

    command::Journal& journal() noexcept { return journal_; }

    command::UndoStack& undoStack() noexcept { return undo_; }

    script::JsonRunner& scriptRunner() noexcept { return runner_; }

    /// The file the drawing currently belongs to, or empty when it has never been
    /// saved. Read by the window title and by the Save dialog's starting folder.
    /// NOT document state: never hashed, never journalled, never undoable
    /// (.claude/model.md R43).
    QString currentFile() const;

    /// Whether the drawing has changed since it was last written to disk.
    ///
    /// The document's own revision counter against the one the file was written
    /// from. It counts EVERY mutation, undo included — so a change made and then
    /// undone still reads as dirty, which is the honest answer: the two documents
    /// are equal but nothing has proved that, and asking one extra question is
    /// cheaper than losing somebody's afternoon.
    bool isDirty() const;

    /// Where a clipboard payload lives when nobody named a file — the io
    /// service's own scratch path, exposed so `PIRICAD_CLIP_PROBE` can delete it
    /// and prove the paste came out of the system clipboard.
    std::string clipboardPath() const;

    QString activeLayerName() const;

signals:
    /// Qt signals mirroring the bus observers, so widgets can connect the way Qt
    /// widgets expect while the bus stays free of Qt (Article 3.3). Everything the
    /// interface shows arrives through one of these, which is why a change made
    /// from the command line updates the screen exactly as a menu click does.
    void echoed(const QString& text);
    void documentChanged();
    void selectionChanged();
    void promptChanged(const QString& prompt);
    void undoStateChanged(bool canUndo, bool canRedo);

    /// An interactive command has ended: `id` is what ran, `mutated` is whether it
    /// wrote anything to the drawing, `dismissed` whether the USER put it away.
    ///
    /// `promptChanged("")` already says a command ended, but not WHICH, and not
    /// how. A modal tool needs both: it stays armed — re-arms itself — after a
    /// run that finished on its own or that the right button closed, so the next
    /// shape needs no trip to the tool column; and it goes away only when the
    /// user says so: Esc, the select arrow, or another tool. `dismissed` is also
    /// true for a run that never asked for anything, so a tool that runs through
    /// without a prompt cannot re-arm itself into a loop.
    void interactiveFinished(const QString& id, bool mutated, bool dismissed);

    /// A person asked for a command — typed it, pressed its button, chose it in the
    /// search — and it is a tool (`command::worth_remembering`). The id, before the
    /// command has run: a command that parks on a prompt or is cancelled was still
    /// the one reached for. Never emitted for a batch, a script, an agent or the shell's
    /// own set-up commands, which go to the bus without passing this door.
    void commandReached(const QString& id);

    /// A command FINISHED — typed, pressed, run by a script or by an agent —
    /// with its id and its structured report as JSON text (`Context::report`;
    /// `null` when it has none). How the shell follows an edit that spans
    /// commands, BLOKDÜZENLE's open and save, whichever client ran each step.
    void commandFinished(const QString& id, const QString& report);

    /// A command handed work to a thread (job.hpp): the status strip shows
    /// `label` and a Durdur, and stays live while the read runs.
    void jobStarted(const QString& label);
    void jobFinished();

    /// KAYDIR asks the canvas to slide so `from` lands on `to`.
    void panRequested(core::Point2 from, core::Point2 to);
    void settingChanged(const QString& id);

    /// A refusal that names its way out (`core::Error::remedy`, TODOS F-02):
    /// what was refused, and the command line that does what it could not —
    /// for the shell to offer as a button beside the message.
    void remedyOffered(const QString& message, const QString& remedy);

    /// A finished command's NEXT STEP (`command::Offer`): the line it would
    /// run, and the words a button says it with — for the shell to offer.
    void offerMade(const QString& title, const QString& text, const QString& label,
                   const QString& line);

private:
    /// Says a refusal on the transcript — and, when it names a way out, that
    /// line too, and `remedyOffered` for the shell.
    void refused(const core::Error& error);

    /// The one body behind `supplyPoint` and `supplyText`: feed the value in, then
    /// finish the command or re-prompt. One place, because a second answer path
    /// that forgot to emit `interactiveFinished` would leave the tool column lit
    /// on a command that had already ended.
    void supplyValue(command::Value value);

    /// After a value was supplied or a job returned: finishes the command,
    /// re-prompts, or leaves it parked on the next job. The one place that decides,
    /// so the tool column and the prompt cannot disagree with the session.
    void settleSession();

    /// Keeps `line` as `lastLine()` when it is a command that can come back.
    void remember(const QString& line);

    bool finishing_{false}; ///< inside `finishInteractive`: the end is a finish, not a dismissal
    bool asked_{false};     ///< the running session has prompted for at least one value

    /// `Bus::on_job_host`: runs the session's job on a worker thread and resumes
    /// the session on this thread when it returns (io.md P3).
    void hostJob(command::Session& session);
    void onJobFinished();
    /// Says, once, that a job holds the drawing and how to end it.
    void sayBusy();
    /// Whether a line naming `spec` is started as a session this controller
    /// keeps (`beginInteractive`) rather than run straight through.
    static bool startsAsSession(const command::CommandSpec& spec);

    void wireBus();
    void settle();

    core::Document document_;
    command::Registry registry_;
    command::Journal journal_;
    command::UndoStack undo_;
    command::Bus bus_;

    // Installs Bus::on_file_request, exactly as `runner_` installs
    // Bus::on_run_script. Declared after `bus_` so it is constructed after it and
    // destroyed before it — a file service must never outlive the bus it points at.
    io::FileService files_;

    // Installs Bus::on_database_request, for the same reason and with the same
    // lifetime rule. Constructing it costs nothing and opens no connection: it
    // only puts the hook in place, so `VERİTABANI` can answer instead of the bus
    // reporting that no engine is attached.
    io::DatabaseService database_;

    // Installs Bus::on_print_request, in the same shape and with the same
    // lifetime rule as the two above.
    PrintService prints_;
    AiService ai_;

    /// Built before `providers_`, because the service is handed this transport
    /// in the constructor body and a member built after it would not exist yet.
    std::unique_ptr<AiTransport> transport_;

    // Installs Bus::on_ai_provider_request, in the same shape and with the same
    // lifetime rule. It loads the provider profiles from the user's
    // configuration directory; the outbound transport is handed over later with
    // `ProviderService::setTransport`, because the wire is not needed to edit a
    // profile and a build without one still has to be able to.
    ProviderService providers_;

    // Installs Bus::on_layout_template_request, in the same shape and with the
    // same lifetime rule as the services above.
    LayoutTemplates templates_;
#if PIRICAD_HAVE_MCP
    std::unique_ptr<McpService> mcp_;
#endif

    /// Resolves a CRS id into its EPSG code and zone. Held as an optional because
    /// a build whose /data/crs package is missing has no catalogue to answer from,
    /// and answering wrong is worse than not answering (see crs_service.hpp).
    std::optional<piricad::domain::geodesy::CrsService> crs_;
    script::JsonRunner runner_;

#if PIRICAD_HAVE_PYTHON
    // The second host. Both are installed behind one BETİK and chosen by the
    // file's extension, so a user with a `.py` and a `.json` beside each other
    // does not have to tell the program which is which (script/python_runner.hpp).
    script::PythonRunner python_runner_;
#endif

    std::unique_ptr<command::Session> session_;
    std::uint64_t sessionsBegun_{0}; ///< see `sessionsBegun()`
    /// The exact line the running session was started with (`YAY yontem=3n`),
    /// empty when nothing is armed. The tool column lights the button whose line
    /// this is: five buttons send `core.arc_draw`, and only the line says which.
    QString armedLine_;

    /// The running session was started by `beginOneShot`: it re-arms nothing.
    bool oneShot_{false};

    /// See `lastLine()`.
    QString lastLine_;

    /// The worker running `session_`'s job, or null. Owned through Qt parenting;
    /// waited on before the session goes.
    QThread* jobThread_{nullptr};

    std::vector<core::EntityId> selected_slots_;
    std::uint64_t selection_revision_{0};
};

} // namespace piricad::app
