// SPDX-License-Identifier: AGPL-3.0-or-later
// KentOSCad — ai: JSON-RPC 2.0, as much of it as MCP 2026-07-28 uses.
//
// WHY THIS FILE IS AGPL WHEN THE REST OF THE TREE IS GPL-3.0-or-later. CLAUDE.md
// Article 2.1 rules "GPLv3-or-later; AGPLv3 for server/web components", and this
// file is the protocol engine of a server: it answers requests that arrive over a
// network from software this program's user does not control. The maintainer
// confirmed the reading. The direction is safe — AGPLv3 code may link GPLv3
// code — and nothing outside the three MCP files changes licence.
//
// SANS-IO, and that is the whole reason this is a file of its own. Parsing a body
// and framing an answer are total functions over `core::Json`; they hold no
// socket, no clock and no document. A protocol is proved by a function, not by a
// socket (.claude/test.md), and every status code this server can produce is
// reachable from a doctest case because of this split.
#pragma once

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <string>

namespace kentos::ai {

/// The JSON-RPC error codes this server answers with.
namespace rpc_error_code {

/// The body was not JSON at all.
inline constexpr int kParseError = -32700;

/// It was JSON, but not a JSON-RPC 2.0 request object.
inline constexpr int kInvalidRequest = -32600;

/// The `method` is not one this server implements; answered with HTTP 404.
inline constexpr int kMethodNotFound = -32601;

/// The `params` are not the shape the method declared — a missing `name`, an
/// argument of the wrong JSON type, an argument the command never declared.
inline constexpr int kInvalidParams = -32602;

/// Something inside this server went wrong. A client can only retry.
inline constexpr int kInternalError = -32603;

/// A required MCP header is missing, or disagrees with the body; HTTP 400.
///
/// THE ONE CODE THIS REVISION ADDS, and it is not a nicety. In 2026-07-28 the
/// method and the name are duplicated into headers so that a proxy can route,
/// meter and authorise a call WITHOUT parsing the body. A header that disagrees
/// with the body is therefore a request two intermediaries would treat as two
/// different calls, and the safe answer is to serve neither of them. The
/// specification's own table gives this error the name `HeaderMismatch`, the code
/// -32020 and the status 400.
inline constexpr int kHeaderMismatch = -32020;

} // namespace rpc_error_code

/// The `_meta` key every 2026-07-28 request states its protocol version in.
///
/// IN `_meta` RATHER THAN IN A HANDSHAKE, because the revision is STATELESS:
/// there is no `initialize` call, so there is no earlier turn in which the two
/// sides could have agreed on a version. Every request carries its own, and the
/// `MCP-Protocol-Version` header must repeat it.
inline constexpr const char* kProtocolVersionMetaKey = "io.modelcontextprotocol/protocolVersion";

/// The `_meta` key `server/discover` answers with the server's identity in.
inline constexpr const char* kServerInfoMetaKey = "io.modelcontextprotocol/serverInfo";

/// One parsed request, or one parsed notification.
struct JsonRpcRequest
{
    /// The caller's id, echoed verbatim in the answer. Null for a notification,
    /// and also what an error answer carries when the id could not be read.
    core::Json id;

    std::string method; ///< `tools/call`, `server/discover`, `subscriptions/listen`
    core::Json params;  ///< the method's arguments; an empty object when it took none

    /// The `_meta` object inside `params`, or null. Where the protocol version
    /// lives, and where this server reads the optional plan id and the client's
    /// own label from.
    core::Json meta;

    /// The protocol version taken out of `meta`; empty when the client sent none.
    std::string protocol_version;

    /// No `id` member at all: the client wants no answer, and the specification
    /// says the transport must reply HTTP 202 with no body.
    bool is_notification{false};
};

/// Reads a body into a request, or says what is wrong with it.
///
/// EVERY FAILURE HERE IS `kInvalidRequest`. The distinction JSON-RPC draws is
/// between "the bytes were not JSON" (-32700, which `core::Json::parse` reports
/// before this function is reached) and "the JSON was not a request" (-32600,
/// which is every way this function can fail). The caller does not have to
/// inspect the `ErrorCode` to choose between them.
core::Result<JsonRpcRequest> parse_rpc(const core::Json& body);

/// A successful answer to `id`.
core::Json rpc_result(const core::Json& id, core::Json result);

/// A failed answer to `id`. `data` is left out of the object when it is null,
/// because an empty `data` member tells a client nothing and still costs bytes.
core::Json rpc_error(const core::Json& id, int code, std::string message,
                     core::Json data = core::Json::null());

/// A server-to-client notification: a method, params, and deliberately no id —
/// which is what makes it unanswerable and therefore free to send on a stream.
core::Json rpc_notification(std::string method, core::Json params);

} // namespace kentos::ai
