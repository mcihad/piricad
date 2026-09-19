// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the outbound wire, and the only place the program's own
// requests leave the machine.
//
// THE OTHER HALF OF `ai::HttpTransport`. `/src/ai` builds the request as data —
// url, headers, body — and cannot send it: `.claude/ai.md` P10 keeps Qt out of
// that module, so the socket lives here, exactly as the QPainter backend lives
// here while `render::Backend` does not (CLAUDE.md Article 8.5).
//
// IT ADDS THE CREDENTIAL AND NOTHING ELSE CAN. The request that arrives carries
// every header except the authorisation one. This class reads the profile's
// `auth_header` and `auth_scheme`, fetches the secret by `key_ref` from the key
// store, and adds it as the bytes leave — so no key is ever in a `Value`, a
// journal line, an audit record or a log (CLAUDE.md 5.21, ai.md P11). Anything
// this class says about a request has been through `ai::redact_request` first.
//
// AND IT NEVER BLOCKS THE UI (ai.md R18, P8). `QNetworkAccessManager` is
// asynchronous; bytes arrive on `readyRead` and go straight to the sink with no
// reframing, because the framers in `ai/sse.hpp` are the tested half and a
// transport that delivered "whole events" would be a second framer in the one
// layer no unit test can reach.
//
// THE CREDENTIAL IS PART OF THAT PROMISE AND DID NOT USED TO BE. Fetching it
// means calling the platform key store, which is allowed to wait for an
// authorisation prompt the user may never answer — over two minutes, measured,
// in a headless run — and `send` used to make that call inline, freezing the
// window before a single byte had left. It now goes through `SecretResolver`
// (secret_resolver.hpp), which answers from memory or off the GUI thread, so
// `send` returns at once in both cases. A request whose key is not resolved yet
// gets its handle FIRST and its socket afterwards; cancelling in that window is
// ordinary and answers the sink exactly once, like every other ending.
#pragma once

#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/transport.hpp"

#include <QObject>

#include <memory>

class QNetworkAccessManager;

namespace kentos::app {

/// The key store the credential comes from, without a wait; see
/// secret_resolver.hpp.
class SecretResolver;

/// `ai::HttpTransport` over Qt Network.
class AiTransport : public QObject, public ai::HttpTransport
{
    Q_OBJECT

public:
    /// `secrets` may be null in a build or a test with no key store at all; a
    /// request that needs a credential then fails with a sentence naming the
    /// entry it would have read, rather than being sent unauthenticated.
    explicit AiTransport(SecretResolver* secrets, QObject* parent = nullptr);
    ~AiTransport() override;

    AiTransport(const AiTransport&)            = delete;
    AiTransport& operator=(const AiTransport&) = delete;

    /// The profile a following `send` belongs to.
    ///
    /// SEPARATE FROM `send` BECAUSE THE INTERFACE IS SANS-KEY. `ai::HttpTransport`
    /// deliberately passes no profile and no secret — that is what keeps the key
    /// out of `/src/ai` — so the caller names the profile here, immediately
    /// before sending, and this class resolves the credential itself.
    void useProfile(const ai::ProviderProfile& profile);

    /// Sends and streams. See `ai::HttpTransport::send`.
    ///
    /// RETURNS BEFORE THE REQUEST NECESSARILY EXISTS. When the profile's key has
    /// not been resolved this session, the handle comes back immediately and the
    /// socket opens once the key store answers; the sink is told either way, and
    /// exactly once. A null return still means "refused outright, and the sink
    /// has already been told why".
    std::shared_ptr<ai::Cancellation> send(const ai::EndpointPermit& permit,
                                           ai::HttpRequest request, ai::StreamSink& sink) override;

private:
    std::unique_ptr<QNetworkAccessManager> net_;
    SecretResolver* secrets_{nullptr};
    ai::ProviderProfile profile_{};
    bool have_profile_{false};
};

} // namespace kentos::app
