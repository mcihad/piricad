// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the bridge between Qt and the command bus.
//
// Constitution Article 1: the user interface is a CLIENT of the command bus and
// has no privileges. Every widget in this application reaches the document
// through this controller, and the controller reaches it only through the bus.
// There is no other path.
#pragma once

#include "piricad/command/bus.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/document.hpp"
#include "piricad/io/service.hpp"
#include "piricad/script/json_runner.hpp"

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace piricad::app {

class Controller : public QObject
{
    Q_OBJECT

public:
    explicit Controller(QObject* parent = nullptr);
    ~Controller() override;

    // ---- the only ways a widget may act on the document ----
    void runLine(const QString& line, command::Origin origin = command::Origin::CommandLine);
    void runCommand(const QString& name); ///< toolbar / menu — same road as a script

    /// Dispatches a fully built invocation. This is `Bus::dispatch`, the same
    /// overload the JSON runner and the AI use (`.claude/command.md` R2): the
    /// canvas needs it because a rubber-band box carries `Point2` values that must
    /// not be round-tripped through formatted text to become a command line.
    void runInvocation(const command::Invocation& invocation);
    void beginInteractive(const QString& name);
    void supplyPoint(core::Point2 world);
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
    void echoed(const QString& text);
    void documentChanged();
    void selectionChanged();
    void promptChanged(const QString& prompt);
    void undoStateChanged(bool canUndo, bool canRedo);
    void viewRequested(const QString& mode, double factor);
    void settingChanged(const QString& id);

private:
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
    script::JsonRunner runner_;

    std::unique_ptr<command::Session> session_;

    std::vector<core::EntityId> selected_slots_;
    std::uint64_t selection_revision_{0};
};

} // namespace piricad::app
