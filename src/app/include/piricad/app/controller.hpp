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
#include "piricad/script/json_runner.hpp"

#include <QObject>
#include <QString>

#include <memory>

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
    void beginInteractive(const QString& name);
    void supplyPoint(core::Point2 world);
    void cancelInteractive();

    command::Session* session() const noexcept { return session_.get(); }

    bool awaitingInput() const;

    core::Document& document() noexcept { return document_; }

    const core::Document& document() const noexcept { return document_; }

    command::Bus& bus() noexcept { return bus_; }

    command::Registry& registry() noexcept { return registry_; }

    command::Journal& journal() noexcept { return journal_; }

    command::UndoStack& undoStack() noexcept { return undo_; }

    script::JsonRunner& scriptRunner() noexcept { return runner_; }

    QString activeLayerName() const;

signals:
    void echoed(const QString& text);
    void documentChanged();
    void promptChanged(const QString& prompt);
    void undoStateChanged(bool canUndo, bool canRedo);
    void viewRequested(const QString& mode, double factor);

private:
    void wireBus();
    void settle();

    core::Document document_;
    command::Registry registry_;
    command::Journal journal_;
    command::UndoStack undo_;
    command::Bus bus_;
    script::JsonRunner runner_;

    std::unique_ptr<command::Session> session_;
};

} // namespace piricad::app
