// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the bridge between Qt and the command bus.
//
// Constitution Article 1: the user interface is a CLIENT of the command bus and
// has no privileges. Every widget in this application reaches the document
// through this controller, and the controller reaches it only through the bus.
// There is no other path.
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/domain/geodesy/crs_service.hpp"
#include "kentos_cad/io/database.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/script/json_runner.hpp"

#if KENTOS_HAVE_LUA
#include "kentos_cad/script/lua_runner.hpp"
#endif

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace kentos::app {

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

    /// Runs one command line and returns its bus result to an interaction that
    /// must keep its dialog open on failure. Other UI clients use `runLine()`.
    core::Result<command::DispatchResult>
    runLineResult(const QString& line, command::Origin origin = command::Origin::CommandLine);
    void runCommand(const QString& line); ///< toolbar / menu — same road as a script

    /// Dispatches a fully built invocation. This is `Bus::dispatch`, the same
    /// overload the JSON runner and the AI use (`.claude/command.md` R2): the
    /// canvas needs it because a rubber-band box carries `Point2` values that must
    /// not be round-tripped through formatted text to become a command line.
    void runInvocation(const command::Invocation& invocation);
    void beginInteractive(const QString& line);
    void supplyPoint(core::Point2 world);

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

    void cancelInteractive();

    /// The selection, resolved to dense slots for one frame. Recomputed only when
    /// the selection or the document changes, never per frame: model.md R2 keeps
    /// keys out of the frame path, and R44 keeps slots out of the selection.
    const std::vector<core::EntityId>& selectedSlots() const noexcept { return selected_slots_; }

    void refreshSelection();

    command::Session* session() const noexcept { return session_.get(); }

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

    command::Journal& journal() noexcept { return journal_; }

    command::UndoStack& undoStack() noexcept { return undo_; }

    script::JsonRunner& scriptRunner() noexcept { return runner_; }

    /// The file the drawing currently belongs to, or empty when it has never been
    /// saved. Read by the window title and by the Save dialog's starting folder.
    /// NOT document state: never hashed, never journalled, never undoable
    /// (.claude/model.md R43).
    QString currentFile() const;

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
    /// wrote anything to the drawing.
    ///
    /// `promptChanged("")` already says a command ended, but not WHICH, and not
    /// whether it drew. A modal tool needs both: it re-arms itself after a shape
    /// is finished so the next one can be drawn without going back to the tool
    /// column, and it must NOT re-arm after a run that drew nothing, or the second
    /// Esc — the one that means "put this tool away" — would arm it again.
    void interactiveFinished(const QString& id, bool mutated);
    void viewRequested(const QString& mode, double factor);

    /// KAYDIR asks the canvas to slide so `from` lands on `to`.
    void panRequested(core::Point2 from, core::Point2 to);
    void settingChanged(const QString& id);

private:
    /// The one body behind `supplyPoint` and `supplyText`: feed the value in, then
    /// finish the command or re-prompt. One place, because a second answer path
    /// that forgot to emit `interactiveFinished` would leave the tool column lit
    /// on a command that had already ended.
    void supplyValue(command::Value value);

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

    /// Resolves a CRS id into its EPSG code and zone. Held as an optional because
    /// a build whose /data/crs package is missing has no catalogue to answer from,
    /// and answering wrong is worse than not answering (see crs_service.hpp).
    std::optional<kentos::domain::geodesy::CrsService> crs_;
    script::JsonRunner runner_;

#if KENTOS_HAVE_LUA
    // The second host. Both are installed behind one BETİK and chosen by the
    // file's extension, so a user with a `.lua` and a `.json` beside each other
    // does not have to tell the program which is which (see script/lua_runner.hpp).
    script::LuaRunner lua_runner_;
#endif

    std::unique_ptr<command::Session> session_;

    std::vector<core::EntityId> selected_slots_;
    std::uint64_t selection_revision_{0};
};

} // namespace kentos::app
