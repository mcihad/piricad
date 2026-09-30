// SPDX-License-Identifier: AGPL-3.0-or-later
//
// THIS FILE IS AGPL AND THE REST OF THE TREE IS GPL, deliberately. CLAUDE.md
// Article 2.1, following §1 of kentoscad.md, puts any SERVER component under the
// GNU Affero General Public License; this file and the protocol engine it drives
// (`ai::McpServer`) ARE that server. The list of AGPL files is in /NOTICE and
// `scripts/ci-gate-ai.sh` checks that the list and the files agree. The two
// licences are compatible in both directions by their own §13.
//
// KentOSCad — app: the socket half of the agent server, and nothing else.
//
// EVERYTHING THAT IS PROTOCOL LIVES IN `/src/ai`: the JSON-RPC envelope, the
// header rules, the status codes, the tool catalogue, the plan compiler. This
// class owns a `QTcpServer` and a `QHttpServer`, turns a Qt request into an
// `ai::HttpRequestView`, hands it to `ai::McpServer::handle`, and writes back
// what it is told. That split is what lets every conformance rule be a Qt-free
// doctest (test.md R23) — and it is the same shape `render::Backend` uses.
//
// IT LISTENS ON THE GUI THREAD ON PURPOSE. `Bus`, `Session`, `Document`,
// `Registry` and `UndoStack` take no locks, and `Document::spatial_index()`
// rebuilds a cache through `mutable` members even when it is `const`; a request
// handled on another thread would be a data race with a five-million-row table.
// `QHttpServer` is asynchronous, so a request is served between events rather
// than beside them. The cost is stated plainly here: a slow tool would hold the
// window, so anything slow must become a `command::Job`.
#pragma once

#include "kentos_cad/ai/clients.hpp"
#include "kentos_cad/ai/endpoint.hpp"
#include "kentos_cad/ai/mcp.hpp"
#include "kentos_cad/command/bus.hpp"

#include <QObject>
#include <QString>

#include <memory>

class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponder;
class QTcpServer;

namespace kentos::app {

/// The dispatcher the protocol engine talks to; see ai_service.hpp.
class AiService;

/// Starts and stops the listener, and answers `MCPSUNUCU`.
class McpService : public QObject
{
    Q_OBJECT

public:
    /// Takes the AI service as the dispatcher the protocol engine talks to, and
    /// registers itself as the handler for the four server verbs of
    /// `Bus::on_ai_request` (the AI service owns that hook and delegates).
    McpService(command::Bus& bus, AiService& ai, QObject* parent = nullptr);
    ~McpService() override;

    McpService(const McpService&)            = delete;
    McpService& operator=(const McpService&) = delete;

    /// Whether the listener is up.
    bool listening() const noexcept;

    /// The port it is on, or 0 when it is down.
    quint16 port() const noexcept { return port_; }

    /// Whether a token is required. False means the endpoint is open to every
    /// process on this machine, which is why the status strip says so in words.
    bool tokenRequired() const noexcept;

    /// The token, for the settings page's copy field ONLY.
    ///
    /// NEVER LOGGED, never echoed, never journalled, never in an audit record
    /// (CLAUDE.md 5.21). Everything that talks about the token elsewhere uses
    /// `tokenFingerprint()`.
    const QString& token() const noexcept { return token_; }

    /// The first six characters and the length — enough for a person to tell two
    /// tokens apart in a transcript, useless to anyone who intercepts it.
    QString tokenFingerprint() const;

    /// Starts on `port` (0 = the configured one). Idempotent: starting an
    /// already-running listener answers with its state rather than failing.
    core::Result<QString> start(quint16 port);

    /// Stops and closes every open stream.
    void stop();

    /// WHO HAS TALKED TO THIS SERVER, and whom a person has shut out.
    ///
    /// HELD HERE RATHER THAN IN THE ENGINE because the two have different lives:
    /// `ai::McpServer` is built on start and destroyed on stop, and a revocation
    /// must survive somebody changing the port. It does NOT survive a token
    /// rotation, and that is deliberate — every label carries the old token's
    /// fingerprint, so after a rotation not one of them can recur.
    const ai::ClientLedger& clients() const noexcept { return clients_; }

    /// Shuts one client out, or lets it back in. The settings page and
    /// `MCPSUNUCU islem=iptal` go through the same two functions, because a
    /// button must not be a privilege (Article 1.2, CLAUDE.md 5.15).
    core::Status revokeClient(const QString& label);
    core::Status restoreClient(const QString& label);

    /// Makes one REAL request to this server over a REAL socket and reports what
    /// came back.
    ///
    /// NOT A SELF-CHECK OF THE ENGINE. Calling `ai::McpServer::handle` directly
    /// would prove the protocol layer works and nothing else — and what a person
    /// clicking "sına" is asking about is the half the engine cannot see: that
    /// the port is bound, that the token in the address is the one the listener
    /// wants, and that nothing on this machine is between them. So it opens a
    /// socket to 127.0.0.1 and speaks HTTP.
    core::Result<QString> probe();

signals:
    /// The listener came up or went down; the status strip and the settings page
    /// both follow this.
    void stateChanged();

    /// A request was served, or a person changed who may send one.
    ///
    /// SEPARATE FROM `stateChanged` because the two move at different rates: the
    /// listener changes when somebody starts or stops it, and the client list
    /// changes on every call an agent makes. A settings page open while an agent
    /// works must show the counts moving — without this it showed whatever was
    /// true when the page was built, which on a fresh start is an empty table
    /// that never fills.
    void clientsChanged();

private:
    /// Handles one request on the GUI thread. Separate from the lambda so the
    /// Qt-facing conversion is one readable function.
    /// Declared with the Qt types forward-declared at GLOBAL scope above, not
    /// with an elaborated specifier here: `const class QHttpServerRequest&`
    /// inside a namespace declares a NEW class in that namespace, and the error
    /// that follows names an incomplete `kentos::app::QHttpServerResponder`.
    void serve(const QHttpServerRequest& request, QHttpServerResponder& responder);

    /// Answers the four server verbs of `MCPSUNUCU`.
    core::Result<std::string> handleVerb(const command::Bus::AiRequest& request);

    /// A fresh token: 32 hex characters from the system's own generator.
    static QString mintToken();

    command::Bus& bus_;
    AiService& ai_;

    std::unique_ptr<QHttpServer> http_;

    /// A HANDLE, NOT OWNERSHIP: `QAbstractHttpServer::bind` reparents the socket
    /// to the server, so the server destroys it. Owning it here as well is a
    /// double free on `stop()`.
    QTcpServer* tcp_{nullptr};
    std::unique_ptr<ai::McpServer> mcp_;

    quint16 port_{0};
    QString token_;
    bool require_token_{true};

    /// See `clients()`. Outlives `mcp_`, which is handed a pointer to it.
    ai::ClientLedger clients_;

    /// Whether a probe is already running. `probe()` spins a nested event loop
    /// so that this server — which serves on THIS thread — can answer, and a
    /// second probe started from inside that loop would be a reentrancy nobody
    /// asked for. The flag makes it a refusal instead.
    bool probing_{false};
};

} // namespace kentos::app
