// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the outbound seam, and the reason there is no socket in here.
//
// WHY AN INTERFACE AND NOT A NETWORK CLIENT. The same two reasons `Dispatcher`
// gives for the inbound direction, in the other direction.
//
// QT. `.claude/ai.md` P10 forbids `<Q...>` anywhere in `/src/ai`, and the only
// HTTP client this program has is Qt's. So the application implements this
// interface with `QNetworkAccessManager` and hands it down, exactly as it hands
// `render::Backend` its painter (CLAUDE.md Article 8.5, same shape).
//
// TESTS. A protocol is proved by a function, not by a socket (`.claude/test.md`,
// P10: ai-eval never calls a live provider). With the wire behind this interface
// every dialect, every framer and the whole conversation model are exercised by
// feeding RECORDED BYTES to a test double, which is what `tests/unit/
// test_ai_chat.cpp` does.
//
// THE CREDENTIAL IS ADDED BY THE IMPLEMENTATION, NOT BY THE CALLER. Nothing in
// `/src/ai` ever holds a key (ai.md P11): the request built here carries every
// header EXCEPT the authorisation one, and the application's transport reads
// `ProviderProfile::auth_header` / `auth_scheme`, fetches the secret from the OS
// keychain by `key_ref`, and adds it as the bytes leave. That is also why
// `redact.hpp` exists: what the transport logs must pass through it first.
#pragma once

#include "kentos_cad/ai/provider.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// One HTTP request, as data — so a test can assert on it and an audit record can
/// quote it (after `redact_request`).
struct HttpRequest
{
    std::string method{"POST"}; ///< `POST` for every dialect; `GET` for a model list

    /// The absolute URL. Comes from `EndpointPermit::url()` rather than from a
    /// profile, so that what was permitted and what is sent cannot differ.
    std::string url;

    /// Headers in the order they were added, WITHOUT the credential (see the
    /// note at the top of this file).
    std::vector<std::pair<std::string, std::string>> headers;

    std::string body; ///< the serialised request body, empty for a `GET`
};

/// What a transport reports while a response arrives.
///
/// BYTES, NOT RECORDS. `on_chunk` is handed whatever the socket produced — a
/// chunk may split an SSE event mid-line or carry three events at once — and the
/// framers in `sse.hpp` are what make records out of it. A transport that tried
/// to deliver whole events would be reimplementing the framer, differently, in
/// the one layer that cannot be unit-tested.
class StreamSink
{
public:
    /// Virtual, because a transport holds the sink through this interface.
    virtual ~StreamSink() = default;

    /// Bytes exactly as they arrived, in order, with no reframing.
    virtual void on_chunk(std::string_view bytes) = 0;

    /// The request is over. `status` is the HTTP status code, or 0 when the
    /// request never got one (a DNS failure, a refused connection, a cancel).
    /// `error` is a Turkish sentence when something went wrong and empty when
    /// nothing did — and it has passed `redact_text`, because a provider's error
    /// body sometimes echoes the request headers back.
    virtual void on_finished(int status, std::string_view error) = 0;
};

/// The handle that stops an in-flight request.
///
/// ai.md R18 requires every AI call to be cancellable and the application to stay
/// usable while one is pending, and R20 requires a cancelled turn to leave the
/// document bit-identical. Cancelling is therefore expected rather than
/// exceptional: `cancel()` is idempotent, may be called from any thread, and the
/// sink still receives exactly one `on_finished` afterwards — a caller that had to
/// tell "cancelled" from "finished" by the absence of a callback would leak a
/// pending turn every time the user pressed Escape.
class Cancellation
{
public:
    /// Virtual: the handle is owned through a `shared_ptr` to this interface.
    virtual ~Cancellation() = default;

    /// Abandons the request. Safe to call twice, and safe to call after the
    /// request has already finished.
    virtual void cancel() = 0;

    /// Whether `cancel()` has been called.
    virtual bool cancelled() const = 0;
};

/// The one door out of this program to a model.
class HttpTransport
{
public:
    /// Virtual: the application's implementation is held through this interface.
    virtual ~HttpTransport() = default;

    /// Sends `request` and streams the answer into `sink`.
    ///
    /// THE PERMIT IS THE FIRST PARAMETER BECAUSE IT IS THE PRECONDITION. Only
    /// `ai::permit_for` can produce one, and it refuses a cloud endpoint when the
    /// project is marked sensitive (ai.md R14/P3) — so this signature is what
    /// makes that rule unforgettable rather than merely written down. `sink` must
    /// outlive the returned handle; the application owns both and the turn is what
    /// keeps them alive.
    virtual std::shared_ptr<Cancellation> send(const EndpointPermit& permit, HttpRequest request,
                                               StreamSink& sink) = 0;
};

} // namespace kentos::ai
