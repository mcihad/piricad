// SPDX-License-Identifier: AGPL-3.0-or-later
// KentOSCad — ai: the HTTP exchange as VALUES, so the engine never names a socket.
//
// WHY THIS FILE IS AGPL WHEN THE REST OF THE TREE IS GPL-3.0-or-later: see
// `jsonrpc.hpp`. CLAUDE.md Article 2.1 puts a server component under AGPLv3 and
// the maintainer confirmed it; these three files are that component.
//
// WHAT THIS FILE IS FOR. `/src/ai` may not open a socket — it links `command` and
// `core` and nothing else (Article 3.2, ai.md P10) — and the application owns the
// listener. So the engine takes a request as a bundle of string views and returns
// a bundle of bytes, headers and, when the answer is a stream, a PLAN the
// transport pumps. The transport decides nothing: it reads the status, writes the
// headers, writes the frames, and tells the engine when the client went away.
//
// This is the same seam `render::Backend` uses, and it buys the same thing: every
// status code, every header refusal and every stream this server can produce is
// reachable from a Qt-free, network-free doctest case.
#pragma once

#include "kentos_cad/core/json.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// One inbound HTTP request, exactly as the transport read it.
///
/// VIEWS, NOT STRINGS: the transport already owns the buffer it parsed, and the
/// engine's answer is built before any of it is released. Nothing here is stored
/// past the call.
struct HttpRequestView
{
    std::string_view method{"POST"}; ///< the HTTP verb; anything but POST is 405

    /// The request path, token and all. See `endpoint_match` for why a token may
    /// be in it and why that is a deliberate local decision.
    std::string_view path;

    /// `Origin`. Empty when the client sent none, which a native client does.
    std::string_view origin;

    std::string_view protocol_version; ///< `MCP-Protocol-Version`
    std::string_view mcp_method;       ///< `Mcp-Method`

    /// `Mcp-Name` — the tool name, resource URI or prompt name. May arrive in the
    /// `=?base64?…?=` form when it is not plain visible ASCII; see
    /// `decode_header_value`.
    std::string_view mcp_name;

    /// `Mcp-Session-Id`. READ AND IGNORED, and that is the specification's rule
    /// for this revision, not laziness: 2026-07-28 has no protocol-level
    /// sessions, so a server that minted or echoed one would be inviting a
    /// client to depend on state that does not exist.
    std::string_view session_id;

    /// `Last-Event-ID`. Also read and ignored: there is no resumable GET stream
    /// in this revision, so there is nothing for an event id to resume.
    std::string_view last_event_id;

    std::string_view accept;        ///< `Accept`; decides JSON against SSE
    std::string_view authorization; ///< `Authorization`; `Bearer <token>`
    std::string_view body;          ///< the raw body bytes

    /// WHEN THE TRANSPORT READ IT — seconds since the Unix epoch, or 0 when it
    /// did not say.
    ///
    /// The engine is sans-IO and therefore has NO CLOCK: it cannot ask what time
    /// it is, so a "last seen" column can only ever be what the transport told
    /// it. Passed in the request rather than read from a global for the same
    /// reason everything else here is a value — a test states the time and gets
    /// the same answer twice.
    std::uint64_t received_at{0};
};

/// How the transport must deliver the answer.
enum class Delivery : std::uint8_t {
    Json,     ///< one `application/json` object, in `HttpOutcome::body`
    Sse,      ///< a `text/event-stream`: write `stream`, then hold or close it
    Accepted, ///< HTTP 202 and no body at all: the answer to a notification
    Empty,    ///< a status and headers only: 403, 404, 405
};

/// One frame of an SSE response.
struct SseFrame
{
    std::string event;   ///< the `event:` name; empty for SSE's default, `message`
    std::string data;    ///< the `data:` payload, already serialised
    std::string comment; ///< a `:` line instead of an event: the keep-alive

    /// The transport closes the stream once it has written this frame. The final
    /// response SHOULD terminate its own stream, and closing is what tells the
    /// client the exchange is over rather than merely quiet.
    bool last{false};

    /// The bytes of this frame, its terminating blank line included.
    std::string render() const;
};

/// An SSE response, as data the transport pumps.
///
/// DATA RATHER THAN A CALLBACK, for two reasons. A test can read what the server
/// decided to send without opening a socket; and the transport stays dumb — it
/// writes `initial` in order, and if `keep_open` it holds the stream, emits
/// `sse_comment()` every `keep_alive_ms`, and writes whatever notification the
/// application hands it. It never chooses what to send.
struct StreamPlan
{
    std::vector<SseFrame> initial; ///< written immediately, in this order

    /// True only for `subscriptions/listen`: no final response terminates it, so
    /// the stream stays open until the client closes it or the program stops.
    bool keep_open{false};

    std::uint32_t keep_alive_ms{0}; ///< 0 when no keep-alive is needed

    /// The notification methods this stream carries, for a transport that fans a
    /// change out to the streams that asked for it.
    std::vector<std::string> subscriptions;

    /// The plan this stream was carrying, if any. When the client closes the
    /// stream the transport hands this back through `McpServer::stream_closed`:
    /// closing IS the cancellation signal in 2026-07-28.
    std::string plan_id;

    /// Which client the stream belonged to, so a closing stream withdraws only
    /// what that client filed. Without it, a client that closed its own stream
    /// would cancel whatever plan id it happened to name (TODOS M-07).
    std::string requester;
};

/// What the application must record about this exchange, whatever came back.
///
/// THE ENGINE CANNOT WRITE THE RECORD and must not pretend otherwise. An audit
/// record needs a UTC timestamp, the operator's name and a durable file
/// (`audit.hpp`), and a sans-IO module has no clock and no disk. So the engine
/// reports the facts only it knows and the application writes the record — which
/// is what makes ai.md R10's "the rejection audit-logged" a thing that happens
/// rather than a thing that was answered.
struct AuditNote
{
    std::string method; ///< the JSON-RPC method, for context
    std::string tool;   ///< the tool name, for a `tools/call`

    /// Who asked: a client label and a token FINGERPRINT. Never a token, never a
    /// key, never a header value that could hold one (ai.md P11).
    std::string requester;

    std::string plan_id; ///< the plan this call filed or appended a step to
    std::string detail;  ///< the refusal's reason, in the words the client was given

    /// A coordinate literal arrived where the schema declares a handle. Set so
    /// the application can call `AuditLog::write_coordinate_refusal`; the client
    /// has already been answered with `isError` (CLAUDE.md 5.8, ai.md R10).
    bool coordinate_refusal{false};

    /// The call would have widened the caller's own authority and was refused
    /// by name; the application writes `AuditLog::write_escalation_refusal`.
    bool escalation_refusal{false};
};

/// One HTTP response, and what the application must audit about it.
struct HttpOutcome
{
    int status{200};                   ///< the HTTP status the transport must send
    Delivery delivery{Delivery::Json}; ///< which of the four shapes below to write

    /// Response headers, in the order they were added. A vector of pairs rather
    /// than a map because two headers may legitimately share a name and because
    /// a stable order makes a test compare bytes.
    std::vector<std::pair<std::string, std::string>> headers;

    std::string body;  ///< the JSON body, for `Delivery::Json`
    StreamPlan stream; ///< the frames, for `Delivery::Sse`
    AuditNote audit;   ///< see `AuditNote`; filled on every path, refusals included

    /// The value of `name`, or empty when it was not set. For the transport and
    /// for the tests.
    std::string_view header(std::string_view name) const;
};

/// What the server will and will not serve.
struct ServerPolicy
{
    /// The ONE endpoint path. 2026-07-28 has a single path and POST is the only
    /// method on it; GET and DELETE are 405.
    std::string path{"/mcp"};

    /// The shared secret, when the maintainer configured one. Empty means this
    /// build was started without a token.
    std::string token;

    /// Refuse a request that carries no token at all. Left ON by default: the
    /// endpoint answers questions about, and files suggestions against, a
    /// cadastral document.
    bool require_token{true};

    /// Origins to accept beyond loopback, each written out whole
    /// (`https://example.invalid`). Loopback is always accepted; see
    /// `origin_allowed`.
    std::vector<std::string> allowed_origins;

    /// How often a held-open stream gets a keep-alive comment.
    std::uint32_t keep_alive_ms{15000};
};

/// Whether an `Origin` header may be served.
///
/// THE ATTACK THIS DEFENDS AGAINST IS DNS REBINDING. The server listens on
/// loopback, so it is unreachable from the internet — except that a page in the
/// user's own browser can be made to resolve its own hostname to 127.0.0.1 and
/// then POST to this endpoint with the browser's credentials and no CORS
/// preflight. The `Origin` header is the only thing that distinguishes that page
/// from a local client, which is why the specification says a server MUST
/// validate it and MUST answer 403 when it is present and invalid.
///
/// An ABSENT origin is accepted, and that is not a hole: a browser always sends
/// one on a cross-origin request, and a native client sends none at all.
bool origin_allowed(std::string_view origin, const ServerPolicy& policy);

/// Whether two strings are equal, in time that does not depend on WHERE they
/// first differ.
bool constant_time_equals(std::string_view a, std::string_view b);

/// What a request path turned out to be.
enum class PathMatch : std::uint8_t {
    None,     ///< some other path entirely; answered 404
    Bare,     ///< this server's endpoint, with nothing after it
    Token,    ///< the endpoint followed by the configured token
    BadToken, ///< the endpoint followed by something that is not the token
};

/// Whether `path` is this server's endpoint, and what it carried.
///
/// A TOKEN IN THE PATH IS NOT WHAT THE SPECIFICATION RECOMMENDS. It says plainly
/// that access tokens MUST NOT be placed in the URI, because a URI reaches proxy
/// logs, browser history and `Referer` headers. This build accepts one anyway, as
/// a deliberate local convenience the maintainer chose: the endpoint is on
/// loopback, and several MCP clients can be pointed at a URL but cannot be taught
/// to send a header. `Authorization: Bearer <token>` is accepted beside it and is
/// the form to prefer. Neither is ever written to the audit record (ai.md P11).
PathMatch endpoint_match(std::string_view path, const ServerPolicy& policy);

/// The token out of an `Authorization: Bearer …` header, or empty.
std::string_view bearer_token(std::string_view authorization);

/// Decodes the `=?base64?<base64 of UTF-8>?=` form an MCP header uses for a value
/// that cannot be written as plain visible ASCII. Anything else comes back
/// unchanged, so this is safe to apply to every header value.
///
/// HAND-ROLLED, AND ARTICLE 2.7 WANTS A REASON. There is no Base64 decoder in
/// this project's dependency set that `/src/ai` may reach: Qt is forbidden here
/// (ai.md P10), GDAL's `CPLBase64` lives behind `/src/io` which is a lateral
/// dependency (Article 3.2), and nlohmann/json has no such entry point. Adding a
/// pinned dependency for fourteen lines of table lookup would fail Article 9's
/// own test — the cost of the dependency exceeds the cost of the code — so this
/// is the exception that article asks to be stated out loud.
std::string decode_header_value(std::string_view raw);

/// Whether an `Accept` header asks for an SSE stream.
bool accepts_event_stream(std::string_view accept);

/// One SSE event, framed: an optional `event:` line, one `data:` line per line of
/// the payload, and the blank line that ends a frame.
std::string sse_event(std::string_view event, std::string_view data);

/// One SSE comment line — the keep-alive. It carries no event and no data, so a
/// conforming client ignores it entirely while every proxy between here and there
/// sees traffic.
std::string sse_comment(std::string_view text = "kentos");

/// A JSON response.
HttpOutcome http_json(int status, const core::Json& payload);

/// HTTP 202 with no body: the answer to a JSON-RPC notification.
HttpOutcome http_accepted();

/// A response that is a status and headers and nothing else.
HttpOutcome http_empty(int status);

/// An SSE response carrying `plan`, with the headers a stream needs — including
/// `X-Accel-Buffering: no`, which the specification asks for because a buffering
/// reverse proxy would hold every frame until the stream closed and turn a
/// progress notification into nothing at all.
HttpOutcome http_stream(StreamPlan plan);

} // namespace kentos::ai
