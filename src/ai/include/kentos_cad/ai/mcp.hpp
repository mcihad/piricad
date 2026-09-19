// SPDX-License-Identifier: AGPL-3.0-or-later
// KentOSCad — ai: the MCP protocol engine, revision 2026-07-28 and nothing else.
//
// WHY THIS FILE IS AGPL WHEN THE REST OF THE TREE IS GPL-3.0-or-later: see
// `jsonrpc.hpp`. CLAUDE.md Article 2.1 puts a server component under AGPLv3 and
// the maintainer confirmed the reading; `jsonrpc`, `endpoint` and `mcp` are that
// component, and their test file with them.
//
// MODERN ONLY, AND ON PURPOSE. 2026-07-28 is STATELESS: no `initialize`
// handshake, no protocol-level session, no GET stream, no `Last-Event-ID`
// resumability. Every request states its own protocol version in `_meta` and
// repeats it in a header. The legacy era is not implemented and no code path here
// degrades into it — a legacy client fails, which is what the specification's own
// compatibility table says happens.
//
// WHAT THIS ENGINE IS NOT. It does not apply anything. A tool whose command
// changes the document is compiled into an `ai::Plan` and handed back as a
// suggestion with its command lines; the drawing is untouched until somebody at
// the workstation approves it through `ai::Gate`. That is CLAUDE.md 5.7 and
// ai.md R3/P1, and no token, header, setting or client claim can stand in for
// that person.
//
// THE ERROR RULE, stated once because every method below follows it. A PROTOCOL
// error is a JSON-RPC error: a missing header, a header that disagrees with the
// body, an unknown method, params of the wrong shape. A DOMAIN error is a
// successful JSON-RPC result carrying `isError: true` and a text block: an
// unknown layer, nothing selected, a stale handle, a refusal by validation. The
// difference matters to a client, which retries the second kind with different
// arguments and must not retry the first kind at all.
#pragma once

// `catalog.hpp` FIRST, and the order is load-bearing rather than alphabetical.
// It defines `struct Catalog`, while `dispatcher.hpp` names the same type
// `const class Catalog&`; a translation unit that meets the `class` spelling
// first gets `-Wmismatched-tags` on the definition that follows, and a warning
// is a defect here (CLAUDE.md 6.3). Defining it first makes the later spelling a
// mere reference to a known type. The real fix belongs in `dispatcher.hpp`.
#include "kentos_cad/ai/catalog.hpp"

#include "kentos_cad/ai/dispatcher.hpp"
#include "kentos_cad/ai/endpoint.hpp"
#include "kentos_cad/ai/jsonrpc.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/spec.hpp"

#include <optional>
#include <string>
#include <vector>

namespace kentos::ai {

/// What this server says it is.
struct ServerInfo
{
    std::string name{"KentOSCad"}; ///< the program's name, as a client displays it

    /// The build's version. EMPTY BY DEFAULT and filled by the application,
    /// which is the only target `KENTOS_VERSION` is defined for
    /// (`src/app/CMakeLists.txt`). A copy of the number here would be a second
    /// place the version lives, and those two places disagree eventually.
    std::string version;

    /// What `server/discover` answers with. Turkish, because every command,
    /// parameter and value in this program is; `default_instructions()` when
    /// left empty.
    std::string instructions;
};

/// The three things an agent must know before its first call, in Turkish:
/// coordinates come from handles, a write becomes a suggestion a person applies,
/// and units are integer millimetres with easting written first.
std::string default_instructions();

/// The wire name of the one tool this protocol layer implements itself.
///
/// NOT A REGISTERED COMMAND, and it cannot be one. This tool hands back
/// `llms.txt`, which DESCRIBES the tool surface — and the tool surface is
/// generated from the command registry (ai.md R12). A command that described the
/// catalogue would be a member of the catalogue it describes, so the fingerprint
/// would cover a document written from the fingerprint. The maintainer asked for
/// `llms.txt` to be reachable as a tool as well as a resource; this is where that
/// lives, and `tools/list` adds it in sorted position so the served bytes stay
/// deterministic.
inline constexpr const char* kLlmsToolName = "llms_txt";

/// The index resource: `ai::llms_txt`, generated on every read.
inline constexpr const char* kLlmsUri = "kentoscad://llms.txt";

/// The long form: `ai::llms_full_txt`, also generated on every read.
inline constexpr const char* kLlmsFullUri = "kentoscad://llms-full.txt";

/// The `_meta` key a client appends to an existing pending suggestion with.
///
/// TWO SPELLINGS ARE ACCEPTED — this one and the bare `plan` — because MCP asks
/// for a reverse-DNS prefix on any `_meta` key it did not define, while the
/// shorter name is what the brief and the manual print. Reading both costs one
/// line and spares a client a guess.
inline constexpr const char* kPlanMetaKey = "cad.kentos/plan";

/// The `_meta` key a client may name itself in, for the audit record.
inline constexpr const char* kClientMetaKey = "cad.kentos/client";

/// The protocol engine: one function from an HTTP request to an HTTP answer.
class McpServer
{
public:
    /// Builds the engine over the document door and the registry. Both are held
    /// by reference: the application owns them and outlives the server.
    McpServer(Dispatcher& dispatcher, const command::Registry& registry, ServerInfo info = {},
              ServerPolicy policy = {});

    /// Answers one request. The single entry point; a transport needs nothing
    /// else to serve this protocol.
    HttpOutcome handle(const HttpRequestView& request);

    /// The client's stream closed, which is how 2026-07-28 spells cancellation.
    /// Withdraws the plan the stream was carrying, when it was carrying one, so
    /// a suggestion from a client that went away does not sit on somebody's
    /// screen for ever.
    void stream_closed(const StreamPlan& stream);

    /// The frame a transport writes to every open `subscriptions/listen` stream
    /// when the tool surface changes. Long-lived change notifications travel on
    /// that stream and nowhere else in this revision.
    static SseFrame tools_list_changed();

    /// Every protocol revision this build can serve. One, and an unsupported
    /// version is answered with this list.
    static std::vector<std::string> supported_versions();

    /// What the server will and will not serve; see `ServerPolicy`.
    const ServerPolicy& policy() const noexcept { return policy_; }

    /// What the server says it is; see `ServerInfo`.
    const ServerInfo& info() const noexcept { return info_; }

private:
    // One method's answer, before `handle` frames it as JSON or as a stream.
    struct Answer
    {
        core::Json payload; // a whole JSON-RPC response object
        int status{200};
        AuditNote audit;
        std::vector<SseFrame> before; // notifications sent ahead of it on a stream
        bool keep_open{false};
        std::vector<std::string> subscriptions;
        std::string plan_id;
    };

    // What compiling one tool call's arguments produced.
    struct Compiled
    {
        command::Args args;
        std::vector<std::string> handles; // which handles the arguments came from
        std::string refusal;              // non-empty when it could not be compiled
        bool coordinate_literal{false};   // a number arrived where a handle belongs
        bool protocol_fault{false};       // the refusal is the client's schema mistake
    };

    Answer fault(const core::Json& id, int code, std::string message,
                 core::Json data = core::Json::null()) const;

    std::optional<Answer> header_fault(const HttpRequestView& http,
                                       const JsonRpcRequest& rpc) const;

    std::string requester_label(const HttpRequestView& http, const JsonRpcRequest& rpc) const;

    Answer discover(const JsonRpcRequest& rpc) const;
    Answer tools_list(const JsonRpcRequest& rpc) const;
    Answer tools_call(const JsonRpcRequest& rpc, std::string requester);
    Answer resources_list(const JsonRpcRequest& rpc) const;
    Answer resources_read(const JsonRpcRequest& rpc) const;
    Answer subscriptions_listen(const JsonRpcRequest& rpc) const;

    Compiled compile(const command::CommandSpec& spec, const core::Json& arguments);

    Dispatcher& dispatcher_;
    const command::Registry& registry_;
    ServerInfo info_;
    ServerPolicy policy_;
};

} // namespace kentos::ai
