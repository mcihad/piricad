// SPDX-License-Identifier: AGPL-3.0-or-later
//
// AGPL rather than the tree's GPL-3.0-or-later because this file is part of the
// server component it tests: CLAUDE.md Article 2.1 puts a server under AGPLv3,
// `src/ai/{jsonrpc,endpoint,mcp}` carry that licence, and a test that quotes
// their payloads travels with them.
//
// WHAT THESE CASES GUARD. The MCP engine is where an outside program touches a
// cadastral drawing, so most of what is asserted below is a REFUSAL: a request
// whose headers disagree with its body, an origin that is not loopback, a token
// that is wrong, a coordinate written where a handle was declared, and — the one
// that matters most — a write tool that was called and did not run.
//
// Qt-free, socket-free, clock-free. `McpServer::handle` is a function from a
// bundle of string views to a bundle of bytes, and the document sits behind
// `ai::Dispatcher`, so every status code this server can produce is reachable
// from a doctest case (.claude/test.md: a protocol is proved by a function, not
// by a socket).
#include "kentos_test.hpp"

#include "kentos_cad/ai/arguments.hpp"
#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/clients.hpp"
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/mcp.hpp"
#include "kentos_cad/ai/policy_path.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/text.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using namespace kentos;

namespace {

using core::Json;

/// The document door, faked.
///
/// A TEST DOUBLE RATHER THAN A REAL BUS, and `dispatcher.hpp` says why: `Bus`,
/// `Document` and `UndoStack` take no locks and belong to the application's
/// thread. What the protocol has to be proved to do is decide correctly — run a
/// read tool, refuse a coordinate, file a suggestion — and that is exactly what
/// this interface makes observable.
struct FakeDispatcher final : ai::Dispatcher
{
    command::Registry reg;
    ai::Catalog cat;

    /// ONE STORE PER CLIENT, exactly as `AiService` holds them: the scoping is
    /// `ai::HandleScopes` either way, so what these cases exercise is the real
    /// code and not a double's idea of it.
    ai::HandleScopes scopes;
    ai::PlanStore plans;
    std::uint64_t rev{7};

    /// THE REAL POLICY ROAD, when a case asks for it: the gate, the audit log
    /// and `ai::decide_by_policy` exactly as `AiService` wires them, with a
    /// runner that changes nothing — what is under test is the decision and
    /// what the client is told, not the drawing.
    bool apply_by_policy{false};
    std::vector<std::string> audit_lines;
    ai::AuditLog audit{[this](const std::string& line) { audit_lines.push_back(line); }};
    ai::Gate gate{plans, audit, [](const ai::Plan&) { return core::ok(); }};

    /// Every requester label `run_read_only` was called with, in order. A read
    /// that arrived without one would mint into the wrong client's store.
    std::vector<std::string> asked_by;

    /// Every command id `run_read_only` was asked to run, in order. The point of
    /// the write cases is that this list stays EMPTY.
    std::vector<std::string> ran;

    /// Makes the next read fail the way a real one does when the layer is not
    /// there — a domain error, not a protocol error.
    bool read_fails{false};

    core::Result<ai::ToolOutcome> run_read_only(const std::string& command_id,
                                                const command::Args& args,
                                                const std::string& requester) override
    {
        ran.push_back(command_id);
        asked_by.push_back(requester);
        if (read_fails)
            return core::err(core::ErrorCode::NotFound,
                             "Böyle bir katman yok: 'YOKKATMAN'. Katmanları KATMANLAR ile "
                             "listeleyin.");

        ai::ToolOutcome out;
        out.command_id = command_id;
        out.lines.push_back("2 nesne bulundu.");
        out.report.set("adet", Json::integer(2));
        out.report.set("katman", Json::string(args.get("katman").as_text()));
        out.minted.push_back(
            scopes.for_client(requester).mint_entities({11, 12}, "sorgula", rev).id);
        return out;
    }

    ai::PolicyPreferences preferences() const override { return prefs; }

    ai::PolicyPreferences prefs; ///< what the person chose; the default until a test says

    core::Result<std::string> propose(ai::Plan plan) override
    {
        // THE CONTRACT THE ENGINE RELIES ON, implemented here because the
        // interface has no `append`: a non-empty `Plan::id` naming a PENDING plan
        // appends the steps to it and returns that same id, and an empty id files
        // a new plan. See the note in `mcp.cpp`; the engine checks the returned
        // id against the requested one, so an implementation that ignored the
        // field would produce a visible refusal rather than a second suggestion.
        // THE SAME REQUEST ASKED TWICE IS THE SAME SUGGESTION (M-06). Mirrors
        // `AiService::propose`: a retry after a dropped connection must not put
        // a second identical card on somebody's screen.
        if (!plan.idempotency_key.empty())
            if (const ai::Plan* already = plans.find_by_key(plan.idempotency_key, plan.requester);
                already != nullptr)
                return already->id;

        if (!plan.id.empty()) {
            const std::string target = plan.id;
            for (const ai::PlanStep& step : plan.steps) {
                // OWNERSHIP-CHECKED, like the real one: a client may extend only
                // a suggestion it filed, because the approval a person gives is
                // for the lines they read (M-07).
                const core::Status appended = plans.append_for(target, plan.requester, step);
                if (!appended) return appended.error();
            }
            return target;
        }
        const std::string filed = plans.add(std::move(plan));
        if (apply_by_policy)
            if (ai::Plan* held = plans.find(filed); held != nullptr) {
                auto effect = command::Effect::None;
                for (const ai::PlanStep& step : held->steps)
                    if (const command::CommandSpec* spec = reg.by_id(step.command_id))
                        effect = effect | command::effect_of(*spec, step.args);
                ai::ClientScope scope;
                scope.client = held->requester;
                auto decided = ai::decide_by_policy(gate, *held, prefs, scope, effect, "sınama", 0);
                if (decided && !decided.value().applied)
                    held->waiting_reason = decided.value().reason;
            }
        return filed;
    }

    std::string existing_plan(const std::string& key, const std::string& requester) const override
    {
        const ai::Plan* held = plans.find_by_key(key, requester);
        return held != nullptr ? held->id : std::string();
    }

    core::Result<ai::Plan> plan_state(const std::string& id,
                                      const std::string& requester) const override
    {
        const ai::Plan* plan = plans.find_for(id, requester);
        if (plan == nullptr)
            return core::err(core::ErrorCode::NotFound, "Böyle bir öneri yok: '" + id + "'.");
        return *plan;
    }

    void withdraw(const std::string& id, const std::string& requester) override
    {
        if (plans.find_for(id, requester) == nullptr) return;
        (void)plans.settle(id, ai::PlanState::Withdrawn, "İstemci akışı kapattı.");
    }

    std::uint64_t revision() const override { return rev; }

    std::optional<command::ViewInfo> view() const override { return std::nullopt; }

    ai::HandleStore& handles(const std::string& requester) override
    {
        return scopes.for_client(requester);
    }

    const ai::Catalog& catalog() const override { return cat; }
};

/// A well-formed request, which each case then breaks in exactly one way.
///
/// The defaults are the happy path — POST, the token in the path, the current
/// protocol version in the header — so that a case which sets one field is
/// visibly testing that one field.
struct Req
{
    std::string method{"POST"};
    std::string path{"/mcp/gizli-anahtar"};
    std::string origin;
    std::string version{ai::Catalog::kProtocolVersion};
    std::string mcp_method;
    std::string mcp_name;
    std::string session_id;
    std::string last_event_id;
    std::string accept{"application/json"};
    std::string authorization;
    std::string body;

    ai::HttpRequestView view() const
    {
        ai::HttpRequestView out;
        out.method           = method;
        out.path             = path;
        out.origin           = origin;
        out.protocol_version = version;
        out.mcp_method       = mcp_method;
        out.mcp_name         = mcp_name;
        out.session_id       = session_id;
        out.last_event_id    = last_event_id;
        out.accept           = accept;
        out.authorization    = authorization;
        out.body             = body;
        return out;
    }
};

struct Rig
{
    FakeDispatcher disp;
    ai::ServerInfo info;
    ai::ServerPolicy policy;

    Rig()
    {
        command::register_builtin_commands(disp.reg);
        ai::register_ai_commands(disp.reg);
        disp.cat     = ai::build_catalog(disp.reg);
        info.version = "0.1.0-test";
        policy.token = "gizli-anahtar";
    }

    /// A server over this rig. Returned by value, which C++17 builds in place —
    /// `McpServer` holds references into this rig and must not outlive it.
    ai::McpServer server() { return ai::McpServer(disp, disp.reg, info, policy); }

    /// WHAT THE SERVER WILL CALL A CLIENT that presents this rig's token and
    /// names itself `who`. Mirrors `McpServer::requester_label`, and a handle
    /// has to be minted into the store of the client that will use it — which
    /// is the point of M-07 and the reason this helper exists at all.
    std::string requester(const char* who = nullptr) const
    {
        std::string label = who != nullptr ? std::string(who) : std::string("MCP istemcisi");
        char buffer[24]   = {};
        (void)std::snprintf(
            buffer, sizeof buffer, " #%08llx",
            static_cast<unsigned long long>(core::fnv1a(policy.token) & 0xFFFFFFFFu));
        return label + buffer;
    }

    /// A points handle over two corners, minted the way a read tool would, into
    /// `who`'s own store.
    std::string points_handle(const char* who = nullptr)
    {
        return disp.scopes.for_client(requester(who))
            .mint_points({core::Point2{0, 0}, core::Point2{10000, 10000}}, "sorgula", disp.rev)
            .id;
    }

    /// An entity handle, for a command that takes a selection.
    std::string entity_handle(const char* who = nullptr)
    {
        return disp.scopes.for_client(requester(who)).mint_entities({11}, "secimi_al", disp.rev).id;
    }
};

/// A JSON-RPC body. `id` null leaves the member out entirely, which is what makes
/// a notification a notification.
std::string rpc_body(const char* method, Json params = Json::object({}), Json id = Json::integer(1),
                     bool declare_version = true)
{
    if (declare_version) {
        Json meta;
        meta.set(ai::kProtocolVersionMetaKey, Json::string(ai::Catalog::kProtocolVersion));
        params.set("_meta", std::move(meta));
    }

    Json body;
    body.set("jsonrpc", Json::string("2.0"));
    if (!id.is_null()) body.set("id", std::move(id));
    body.set("method", Json::string(method));
    body.set("params", std::move(params));
    return body.dump();
}

/// A `tools/call` body, with the headers a caller would have to send with it.
Req tool_call(const char* tool, Json arguments, Json meta_extra = Json::null())
{
    Json params;
    params.set("name", Json::string(tool));
    params.set("arguments", std::move(arguments));

    Json meta;
    meta.set(ai::kProtocolVersionMetaKey, Json::string(ai::Catalog::kProtocolVersion));
    if (meta_extra.is_object())
        for (const auto& [key, value] : meta_extra.as_object())
            meta.set(key, value);
    params.set("_meta", std::move(meta));

    Json body;
    body.set("jsonrpc", Json::string("2.0"));
    body.set("id", Json::integer(1));
    body.set("method", Json::string("tools/call"));
    body.set("params", std::move(params));

    Req req;
    req.mcp_method = "tools/call";
    req.mcp_name   = tool;
    req.body       = body.dump();
    return req;
}

Json parse_body(const ai::HttpOutcome& out)
{
    core::Result<Json> body = core::Json::parse(out.body);
    REQUIRE(body.ok());
    return body.value();
}

Json result_of(const ai::HttpOutcome& out)
{
    const Json body    = parse_body(out);
    const Json* result = body.find("result");
    REQUIRE(result != nullptr);
    return *result;
}

Json error_of(const ai::HttpOutcome& out)
{
    const Json body   = parse_body(out);
    const Json* error = body.find("error");
    REQUIRE(error != nullptr);
    return *error;
}

std::int64_t code_of(const ai::HttpOutcome& out)
{
    const Json error = error_of(out);
    const Json* code = error.find("code");
    REQUIRE(code != nullptr);
    return code->as_int();
}

/// The text of the first (and only) content block of a `tools/call` result.
std::string text_of(const Json& result)
{
    const Json* content = result.find("content");
    REQUIRE(content != nullptr);
    REQUIRE(content->is_array());
    REQUIRE_FALSE(content->as_array().empty());
    const Json* text = content->as_array().front().find("text");
    REQUIRE(text != nullptr);
    return text->as_string();
}

bool is_error(const Json& result)
{
    const Json* flag = result.find("isError");
    REQUIRE(flag != nullptr);
    return flag->as_bool(true);
}

} // namespace

TEST_CASE("POST dışındaki her yöntem 405 döner ve Allow başlığını taşır")
{
    Rig f;
    ai::McpServer server = f.server();

    // One path, POST only. A GET is not a client asking for the long-lived
    // stream this revision removed; it is a legacy client, and the specification
    // says a legacy client fails. A DELETE would be ending a session, and there
    // are no sessions. Both used to be answered by the same body as a POST in an
    // earlier draft of this engine, which is the defect this case pins.
    for (const char* verb : {"GET", "DELETE", "PUT", "HEAD"}) {
        Req req;
        req.method     = verb;
        req.mcp_method = "tools/list";
        req.body       = rpc_body("tools/list");

        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 405);
        CHECK_EQ(out.delivery, ai::Delivery::Empty);
        CHECK_EQ(out.header("Allow"), std::string_view("POST"));
        CHECK(out.body.empty());
    }
}

TEST_CASE("Origin doğrulanır: yabancı köken 403, olmayan köken kabul")
{
    Rig f;
    ai::McpServer server = f.server();

    const auto send = [&](const char* origin) {
        Req req;
        req.origin     = origin;
        req.mcp_method = "tools/list";
        req.body       = rpc_body("tools/list");
        return server.handle(req.view()).status;
    };

    // The attack is DNS rebinding: a page in the user's own browser resolves its
    // own hostname to 127.0.0.1 and POSTs here with no CORS preflight. `Origin`
    // is the only thing that tells that page apart from a local client.
    CHECK_EQ(send("https://kotu.example"), 403);
    CHECK_EQ(send("null"), 403);
    // The prefix trap, which is the domain an attacker actually registers.
    CHECK_EQ(send("http://localhost.kotu.example"), 403);
    CHECK_EQ(send("http://127.0.0.1.kotu.example"), 403);

    CHECK_EQ(send("http://localhost:5173"), 200);
    CHECK_EQ(send("http://127.0.0.1"), 200);
    CHECK_EQ(send("http://[::1]:8080"), 200);

    // ABSENT IS ACCEPTED, and that is not a hole: a browser always sends an
    // origin on a cross-origin request, so its absence is evidence that the
    // caller is a native client.
    Req bare;
    bare.mcp_method = "tools/list";
    bare.body       = rpc_body("tools/list");
    CHECK_EQ(server.handle(bare.view()).status, 200);

    // A configured origin is served, so an in-house web client is possible
    // without turning the check off.
    f.policy.allowed_origins.push_back("https://kurum.example");
    ai::McpServer permissive = f.server();
    Req allowed;
    allowed.origin     = "https://kurum.example";
    allowed.mcp_method = "tools/list";
    allowed.body       = rpc_body("tools/list");
    CHECK_EQ(permissive.handle(allowed.view()).status, 200);
}

TEST_CASE("Belirteç yolda ve Bearer başlığında kabul edilir; yanlışı reddedilir")
{
    Rig f;
    ai::McpServer server = f.server();

    const auto send = [&](const char* path, const char* authorization) {
        Req req;
        req.path          = path;
        req.authorization = authorization;
        req.mcp_method    = "tools/list";
        req.body          = rpc_body("tools/list");
        return server.handle(req.view());
    };

    // The path form: a deliberate local convenience, because several clients can
    // be pointed at a URL and cannot be taught to send a header. The
    // specification says a token MUST NOT be in the URI, so the header form is
    // accepted beside it and is the one to prefer.
    CHECK_EQ(send("/mcp/gizli-anahtar", "").status, 200);
    CHECK_EQ(send("/mcp", "Bearer gizli-anahtar").status, 200);
    // The scheme is case-insensitive per RFC 7235; the token is not.
    CHECK_EQ(send("/mcp", "bearer gizli-anahtar").status, 200);

    CHECK_EQ(send("/mcp", "").status, 401);
    CHECK_EQ(send("/mcp", "Bearer yanlis").status, 401);
    CHECK_EQ(send("/mcp/yanlis", "").status, 401);
    // A wrong token in the path is refused even when a correct header came with
    // it: a client that asserted a credential and got it wrong believes
    // something false about this server, and serving it would hide that.
    CHECK_EQ(send("/mcp/yanlis", "Bearer gizli-anahtar").status, 401);
    CHECK_EQ(send("/mcp", "Bearer gizli-anahtar").header("WWW-Authenticate"), std::string_view(""));
    CHECK_EQ(send("/mcp", "Bearer yanlis").header("WWW-Authenticate"),
             std::string_view("Bearer realm=\"KentOSCad\""));

    // Some other path entirely is 404 and not 401: there is nothing here to
    // authenticate against.
    CHECK_EQ(send("/baska/yol", "Bearer gizli-anahtar").status, 404);
}

TEST_CASE("Başlıklar gövdeyle karşılaştırılır; uyuşmazlık -32020 ve 400")
{
    Rig f;
    ai::McpServer server = f.server();

    // WHY THIS IS CHECKED AT ALL: in this revision the method and the name are
    // duplicated into headers so a proxy can route and meter without parsing the
    // body. A header that disagrees with the body is a request two intermediaries
    // would treat as two different calls.
    SUBCASE("MCP-Protocol-Version başlığı yok")
    {
        Req req;
        req.version               = "";
        req.mcp_method            = "tools/list";
        req.body                  = rpc_body("tools/list");
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("gövdede _meta sürümü yok")
    {
        Req req;
        req.mcp_method = "tools/list";
        req.body       = rpc_body("tools/list", Json::object({}), Json::integer(1), false);
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("başlık ve gövde farklı sürüm söylüyor")
    {
        Req req;
        req.version               = "2025-06-18";
        req.mcp_method            = "tools/list";
        req.body                  = rpc_body("tools/list");
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("Mcp-Method başlığı yok")
    {
        Req req;
        req.body                  = rpc_body("tools/list");
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("Mcp-Method gövdedeki method ile aynı değil")
    {
        Req req;
        req.mcp_method            = "tools/call";
        req.body                  = rpc_body("tools/list");
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("tools/call için Mcp-Name yok")
    {
        Req req                   = tool_call("sorgula", Json::object({}));
        req.mcp_name              = "";
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("Mcp-Name gövdedeki params.name ile aynı değil")
    {
        Req req                   = tool_call("sorgula", Json::object({}));
        req.mcp_name              = "katmanlari_listele";
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 400);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("Base64 kodlu Mcp-Name çözülüp karşılaştırılır")
    {
        // A value that cannot be written as plain visible ASCII arrives encoded.
        // Every tool name in this program is ASCII, but a resource URI need not
        // be — and comparing the encoded form against the decoded body would
        // refuse a perfectly conforming client. `sorgula` is `c29yZ3VsYQ==`.
        Req req                   = tool_call("sorgula", Json::object({}));
        req.mcp_name              = "=?base64?c29yZ3VsYQ==?=";
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 200);
        CHECK_FALSE(is_error(result_of(out)));

        // And a wrong one still fails after decoding.
        Req wrong      = tool_call("sorgula", Json::object({}));
        wrong.mcp_name = "=?base64?Y2l6Z2k=?=";
        CHECK_EQ(code_of(server.handle(wrong.view())), ai::rpc_error_code::kHeaderMismatch);
    }

    SUBCASE("gövdesi adsız bir tools/call başlığı değil params'ı suçlar")
    {
        // THE BODY IS THE AUTHORITY ON WHAT IS WRONG. When `params.name` is
        // missing the defect is the body, and reporting a header mismatch would
        // send the client to fix the one thing that was correct.
        Json params;
        params.set("arguments", Json::object({}));
        Req req                   = Req{};
        req.mcp_method            = "tools/call";
        req.mcp_name              = "sorgula";
        req.body                  = rpc_body("tools/call", std::move(params));
        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(code_of(out), ai::rpc_error_code::kInvalidParams);
    }
}

TEST_CASE("Desteklenmeyen sürüm 400 ve desteklenen sürüm listesini döner")
{
    Rig f;
    ai::McpServer server = f.server();

    Json meta;
    meta.set(ai::kProtocolVersionMetaKey, Json::string("2025-03-26"));
    Json params;
    params.set("_meta", std::move(meta));

    Json body;
    body.set("jsonrpc", Json::string("2.0"));
    body.set("id", Json::integer(1));
    body.set("method", Json::string("tools/list"));
    body.set("params", std::move(params));

    Req req;
    req.version    = "2025-03-26";
    req.mcp_method = "tools/list";
    req.body       = body.dump();

    const ai::HttpOutcome out = server.handle(req.view());
    CHECK_EQ(out.status, 400);

    const Json error = error_of(out);
    const Json* data = error.find("data");
    REQUIRE(data != nullptr);

    const Json* named = data->find("error");
    REQUIRE(named != nullptr);
    CHECK_EQ(named->as_string(), std::string("UnsupportedProtocolVersionError"));

    const Json* versions = data->find("supportedVersions");
    REQUIRE(versions != nullptr);
    REQUIRE(versions->is_array());
    REQUIRE_EQ(versions->as_array().size(), 1u);
    CHECK_EQ(versions->as_array().front().as_string(), std::string(ai::Catalog::kProtocolVersion));
}

TEST_CASE("Bilinmeyen yöntem 404 ve -32601 döner")
{
    Rig f;
    ai::McpServer server = f.server();

    // `initialize`, `ping`, `logging/setLevel`, `sampling/createMessage` and
    // `resources/subscribe` are all methods this server deliberately does not
    // serve, and a client must be told so rather than left waiting.
    for (const char* method : {"initialize", "ping", "logging/setLevel", "resources/subscribe"}) {
        Req req;
        req.mcp_method = method;
        req.body       = rpc_body(method);

        const ai::HttpOutcome out = server.handle(req.view());
        CHECK_EQ(out.status, 404);
        CHECK_EQ(code_of(out), ai::rpc_error_code::kMethodNotFound);
    }
}

TEST_CASE("Bildirim 202 ile ve gövdesiz karşılanır")
{
    Rig f;
    ai::McpServer server = f.server();

    Req req;
    req.mcp_method = "notifications/cancelled";
    req.body       = rpc_body("notifications/cancelled", Json::object({}), Json::null());

    const ai::HttpOutcome out = server.handle(req.view());
    CHECK_EQ(out.status, 202);
    CHECK_EQ(out.delivery, ai::Delivery::Accepted);
    CHECK(out.body.empty());

    // A notification has no id, so a JSON-RPC error would have no addressee —
    // but a `Mcp-Method` that disagrees with the body is a broken client, and a
    // 400 says so where a 202 would say nothing.
    Req mismatched;
    mismatched.mcp_method = "tools/list";
    mismatched.body       = rpc_body("notifications/cancelled", Json::object({}), Json::null());
    const ai::HttpOutcome refused = server.handle(mismatched.view());
    CHECK_EQ(refused.status, 400);
    CHECK_EQ(code_of(refused), ai::rpc_error_code::kHeaderMismatch);
}

TEST_CASE("Oturum ve olay kimliği başlıkları yok sayılır, asla yankılanmaz")
{
    Rig f;
    ai::McpServer server = f.server();

    Req req;
    req.session_id    = "eski-oturum-42";
    req.last_event_id = "17";
    req.mcp_method    = "tools/list";
    req.body          = rpc_body("tools/list");

    // 2026-07-28 has no protocol-level sessions and no resumable stream. Echoing
    // a session id would invite a client to depend on state that does not exist.
    const ai::HttpOutcome out = server.handle(req.view());
    CHECK_EQ(out.status, 200);
    CHECK_EQ(out.header("Mcp-Session-Id"), std::string_view(""));
    CHECK_EQ(out.header("MCP-Protocol-Version"), std::string_view(ai::Catalog::kProtocolVersion));
}

TEST_CASE("server/discover zorunlu alanların tamamını döner")
{
    Rig f;
    ai::McpServer server = f.server();

    Req req;
    req.mcp_method = "server/discover";
    req.body       = rpc_body("server/discover");

    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);
    const Json result = result_of(out);

    // Mandatory in this revision, every one of them: a client that cannot read
    // these cannot decide whether to talk to us at all.
    for (const char* field :
         {"supportedVersions", "capabilities", "instructions", "ttlMs", "cacheScope", "_meta"})
        CHECK(result.find(field) != nullptr);

    const Json* meta = result.find("_meta");
    REQUIRE(meta != nullptr);
    const Json* server_info = meta->find(ai::kServerInfoMetaKey);
    REQUIRE(server_info != nullptr);
    REQUIRE(server_info->find("name") != nullptr);
    REQUIRE(server_info->find("version") != nullptr);
    CHECK_EQ(server_info->find("version")->as_string(), std::string("0.1.0-test"));

    // The instructions carry the three things an agent must know before its
    // first call, and each of them is a rule somebody could otherwise break
    // expensively.
    const Json* instructions = result.find("instructions");
    REQUIRE(instructions != nullptr);
    const std::string& text = instructions->as_string();
    CHECK(text.find("TUTAMAĞI") != std::string::npos);
    CHECK(text.find("BİR ÖNERİ AÇAR") != std::string::npos);
    CHECK(text.find("MİLİMETREDİR") != std::string::npos);
    CHECK(text.find("DOĞU ÖNCE") != std::string::npos);

    // AND THE RULES THE PERSON CHOSE, in the words the chat is told (A-03): the
    // default policy, so a write waits for the engineer.
    CHECK(text.find("Bu oturumun onay ve soru kuralları") != std::string::npos);
    CHECK(text.find("her_degisiklikte") != std::string::npos);
    const Json* discovered = result.find("_meta");
    REQUIRE(discovered != nullptr);
    const Json* policy = discovered->find("cad.kentos/policy");
    REQUIRE(policy != nullptr);
    CHECK_EQ(policy->find("onay")->as_string(), std::string("her_degisiklikte"));
}

TEST_CASE("tools/list parmak izini ve her aracın dört annotation'ını taşır")
{
    Rig f;
    ai::McpServer server = f.server();

    Req req;
    req.mcp_method = "tools/list";
    req.body       = rpc_body("tools/list");

    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);
    const Json result = result_of(out);

    const Json* meta = result.find("_meta");
    REQUIRE(meta != nullptr);
    const Json* fingerprint = meta->find("cad.kentos/fingerprint");
    REQUIRE(fingerprint != nullptr);
    // A DECIMAL STRING, not an integer: the fingerprint is a uint64 and JSON's
    // integer is signed, so half of its range would arrive negative.
    CHECK_EQ(fingerprint->as_string(), std::to_string(f.disp.reg.fingerprint()));

    // NOT PAGINATED, and saying so is part of the contract: a cursor would be a
    // second piece of state in a revision that has none.
    CHECK(result.find("nextCursor") == nullptr);

    const Json* tools = result.find("tools");
    REQUIRE(tools != nullptr);
    REQUIRE(tools->is_array());
    CHECK_EQ(tools->as_array().size(), f.disp.cat.tools.size() + 1);

    std::string previous;
    bool found_builtin = false;
    for (const Json& tool : tools->as_array()) {
        const Json* name = tool.find("name");
        REQUIRE(name != nullptr);
        // Sorted by wire name, the built-in included, so a client that caches
        // this list and compares it sees a change only when there is one.
        CHECK(previous < name->as_string());
        previous = name->as_string();
        if (name->as_string() == ai::kLlmsToolName) found_builtin = true;

        // The specification DEFAULTS destructiveHint and openWorldHint to true,
        // so a tool published with no annotations is advertised as destructive —
        // which would make a careful client refuse to call the safest thing in
        // the program.
        const Json* notes = tool.find("annotations");
        REQUIRE(notes != nullptr);
        for (const char* key :
             {"readOnlyHint", "destructiveHint", "idempotentHint", "openWorldHint"}) {
            const Json* hint = notes->find(key);
            REQUIRE(hint != nullptr);
            CHECK(hint->is_bool());
        }
        CHECK(tool.find("inputSchema") != nullptr);
    }
    CHECK(found_builtin);
}

TEST_CASE("llms_txt aracı protokol katmanının kendi aracıdır")
{
    Rig f;
    ai::McpServer server = f.server();

    // IT CANNOT BE A REGISTERED COMMAND: it hands back the document that
    // DESCRIBES the tool surface, and the tool surface is generated from the
    // registry. A command that described the catalogue would be a member of the
    // catalogue it describes.
    CHECK(f.disp.reg.by_id("core.llms_txt") == nullptr);
    CHECK(f.disp.cat.find(ai::kLlmsToolName) == nullptr);

    const Req req             = tool_call(ai::kLlmsToolName, Json::object({}));
    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);

    const Json result = result_of(out);
    CHECK_FALSE(is_error(result));
    const std::string text = text_of(result);
    CHECK(text.find("milimetre") != std::string::npos);
    CHECK(text.find("tutamak") != std::string::npos);

    // Nothing was dispatched: this tool never reaches the bus.
    CHECK(f.disp.ran.empty());
}

TEST_CASE("Okuma aracı hemen çalışır ve tutamaklarını bildirir")
{
    Rig f;
    ai::McpServer server = f.server();

    Json arguments;
    arguments.set("katman", Json::string("PARSEL"));

    const Req req             = tool_call("sorgula", std::move(arguments));
    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);

    // `NoEffect` is the test, not `ReadOnly`: `core.undo` and `core.save` carry
    // `ReadOnly` and neither is harmless. A tool that changes nothing runs now.
    REQUIRE_EQ(f.disp.ran.size(), 1u);
    CHECK_EQ(f.disp.ran.front(), std::string("core.query"));

    const Json result = result_of(out);
    CHECK_FALSE(is_error(result));
    CHECK(text_of(result).find("2 nesne") != std::string::npos);

    const Json* structured = result.find("structuredContent");
    REQUIRE(structured != nullptr);
    CHECK_EQ(structured->find("adet")->as_int(), 2);
    // The arguments really reached the command rather than being dropped.
    CHECK_EQ(structured->find("katman")->as_string(), std::string("PARSEL"));

    // THE MINTED HANDLES ARE NAMED, in the structure and in the text. A client
    // that reads only the text block would otherwise never learn which handles
    // it may refer to, and a handle it cannot see is a write it cannot compose.
    const Json* handles = structured->find("tutamaklar");
    REQUIRE(handles != nullptr);
    REQUIRE(handles->is_array());
    REQUIRE_EQ(handles->as_array().size(), 1u);
    const Json* id = handles->as_array().front().find("tutamak");
    REQUIRE(id != nullptr);
    CHECK(text_of(result).find(id->as_string()) != std::string::npos);
    // The coordinates themselves are NOT handed over (ai.md P9).
    CHECK(handles->as_array().front().find("noktalar") == nullptr);
}

TEST_CASE("Yazma aracı ÇALIŞMAZ: öneri kimliği ve komut satırları döner")
{
    Rig f;
    ai::McpServer server = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));

    const Req req             = tool_call("core_line", std::move(arguments));
    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);

    // THE CASE THIS WHOLE FILE EXISTS FOR. CLAUDE.md 5.7 and ai.md R3/P1: a
    // write becomes a suggestion and waits for a person. Nothing ran.
    CHECK(f.disp.ran.empty());
    REQUIRE_EQ(f.disp.plans.pending().size(), 1u);

    const Json result = result_of(out);
    CHECK_FALSE(is_error(result));

    const std::string plan_id = f.disp.plans.pending().front()->id;
    CHECK_EQ(out.audit.plan_id, plan_id);
    CHECK_EQ(out.audit.tool, std::string("core_line"));
    CHECK_FALSE(out.audit.coordinate_refusal);
    // The requester is named for the audit record, with the token's FINGERPRINT
    // and never the token itself (ai.md P11).
    CHECK(out.audit.requester.find("gizli-anahtar") == std::string::npos);
    CHECK(out.audit.requester.find('#') != std::string::npos);

    const std::string text = text_of(result);
    CHECK(text.find(plan_id) != std::string::npos);
    // The exact command line a person reads and could type themselves — in
    // METRES, the unit the command line reads. It said `10000,10000`, which typed
    // back would have drawn ten kilometres out.
    CHECK(text.find("ÇİZGİ 0,0 10,10") != std::string::npos);
    // THE ANSWER SAYS WHICH STATE IT IS IN. A pending plan says the drawing is
    // unchanged and names BOTH roads it could take — the card, or the policy the
    // user set beforehand (§5.2.1). It must never claim the client applied it.
    CHECK(text.find("Çizim değişmedi") != std::string::npos);
    CHECK(text.find("onay politikası") != std::string::npos);

    const Json* structured = result.find("structuredContent");
    REQUIRE(structured != nullptr);
    CHECK_EQ(structured->find("oneri")->as_string(), plan_id);
    CHECK_EQ(structured->find("durum")->as_string(), std::string("beklemede"));

    const Json* meta = result.find("_meta");
    REQUIRE(meta != nullptr);
    CHECK_EQ(meta->find("cad.kentos/approval")->as_string(), std::string("user-required"));
    CHECK_EQ(meta->find(ai::kPlanMetaKey)->as_string(), plan_id);

    // And the plan holds the resolved arguments, not the handle string: the
    // handle was resolved while the arguments were still JSON.
    const ai::Plan* plan = f.disp.plans.find(plan_id);
    REQUIRE(plan != nullptr);
    REQUIRE_EQ(plan->steps.size(), 1u);
    CHECK_EQ(plan->steps.front().command_id, std::string("core.line"));
    CHECK_EQ(plan->steps.front().args.get("noktalar").as_points().size(), 2u);
    REQUIRE_EQ(plan->steps.front().handles.size(), 1u);
}

TEST_CASE("Nesne seçimi de tutamakla verilir ve anahtarlara çözülür")
{
    Rig f;
    ai::McpServer server = f.server();

    Json arguments;
    arguments.set("nesneler", Json::string(f.entity_handle()));

    const ai::HttpOutcome out = server.handle(tool_call("core_erase", std::move(arguments)).view());
    REQUIRE_EQ(out.status, 200);
    CHECK_FALSE(is_error(result_of(out)));
    CHECK(f.disp.ran.empty());

    // A SELECTION IS PERSISTENT KEYS, never dense slots (model.md R5): the handle
    // resolves to the key the read tool minted, and the command line a person
    // reads says the same number.
    const ai::Plan* plan = f.disp.plans.find(out.audit.plan_id);
    REQUIRE(plan != nullptr);
    REQUIRE_EQ(plan->steps.size(), 1u);
    CHECK_EQ(plan->steps.front().command_id, std::string("core.erase"));
    REQUIRE_EQ(plan->steps.front().args.get("nesneler").as_ids().size(), 1u);
    CHECK_EQ(plan->steps.front().args.get("nesneler").as_ids().front(), 11);
    CHECK_EQ(plan->steps.front().line, std::string("SİL nesneler=11"));

    // A POINTS handle where a selection was declared is refused rather than
    // quietly reinterpreted: two corners are not two objects.
    Json wrong;
    wrong.set("nesneler", Json::string(f.points_handle()));
    const ai::HttpOutcome mixed = server.handle(tool_call("core_erase", std::move(wrong)).view());
    CHECK_EQ(mixed.status, 200);
    CHECK(is_error(result_of(mixed)));
    CHECK_FALSE(mixed.audit.coordinate_refusal);
}

TEST_CASE("İkinci adım aynı öneriye eklenir: bir uygulama, bir Ctrl+Z")
{
    Rig f;
    ai::McpServer server = f.server();

    Json first;
    first.set("noktalar", Json::string(f.points_handle()));
    const ai::HttpOutcome opened = server.handle(tool_call("core_line", std::move(first)).view());
    REQUIRE_EQ(opened.status, 200);
    const std::string plan_id = opened.audit.plan_id;
    REQUIRE_FALSE(plan_id.empty());

    // ai.md R4: one approved suggestion is ONE undo step, so an agent composing
    // a sequence must be able to add to the plan a person will apply rather than
    // filling their screen with eleven separate cards.
    Json again;
    again.set("noktalar", Json::string(f.points_handle()));
    Json meta_extra;
    meta_extra.set("plan", Json::string(plan_id));

    const ai::HttpOutcome appended =
        server.handle(tool_call("core_line", std::move(again), std::move(meta_extra)).view());
    REQUIRE_EQ(appended.status, 200);
    CHECK_FALSE(is_error(result_of(appended)));
    CHECK_EQ(appended.audit.plan_id, plan_id);

    CHECK_EQ(f.disp.plans.pending().size(), 1u);
    const ai::Plan* plan = f.disp.plans.find(plan_id);
    REQUIRE(plan != nullptr);
    CHECK_EQ(plan->steps.size(), 2u);
    // Both lines come back, so the person sees the whole sequence they are
    // deciding about.
    const std::string text = text_of(result_of(appended));
    CHECK(text.find("genişletildi") != std::string::npos);

    // An unknown plan is a domain refusal, not a protocol error: the client fixes
    // it by opening a new suggestion.
    Json orphan;
    orphan.set("noktalar", Json::string(f.points_handle()));
    Json bad_meta;
    bad_meta.set("plan", Json::string("p0000000000000000"));
    const ai::HttpOutcome refused =
        server.handle(tool_call("core_line", std::move(orphan), std::move(bad_meta)).view());
    CHECK_EQ(refused.status, 200);
    CHECK(is_error(result_of(refused)));
    CHECK(f.disp.ran.empty());
}

TEST_CASE("Koordinat yazmak isError ile reddedilir ve denetim için işaretlenir")
{
    Rig f;
    ai::McpServer server = f.server();

    // CLAUDE.md 5.8 and ai.md R9/R10/P2. The agent-facing schema declares a
    // handle string here, so a number is a non-conforming client — and the
    // refusal happens while the arguments are still JSON, before an `Args`
    // exists, which is what makes "rejected before validation" literal.
    const auto attempt = [&](Json value) {
        Json arguments;
        arguments.set("noktalar", std::move(value));
        return server.handle(tool_call("core_line", std::move(arguments)).view());
    };

    Json pair = Json::array({});
    Json one  = Json::array({});
    one.push(Json::integer(485320150));
    one.push(Json::integer(4310220000));
    pair.push(one);
    pair.push(one);

    for (Json written : {pair, Json::integer(485320150), Json::string("485320150,4310220000"),
                         Json::string("yol kenari")}) {
        const ai::HttpOutcome out = attempt(std::move(written));
        CHECK_EQ(out.status, 200);

        const Json result = result_of(out);
        CHECK(is_error(result));
        // The message names the read tools that mint a POINT handle, and the
        // relative form that measures from one, because a refusal a client
        // cannot act on is a refusal it will simply retry.
        const std::string text = text_of(result);
        CHECK(text.find("KONUM") != std::string::npos);
        CHECK(text.find("gorunum_bilgisi") != std::string::npos);
        CHECK(text.find("nesne_noktalari") != std::string::npos);
        CHECK(text.find("taban") != std::string::npos);

        // REPORTED SO THE APPLICATION CAN AUDIT-LOG IT. R10 requires the
        // rejection to be recorded, not merely answered, and a sans-IO engine
        // has no clock and no file — so it hands the fact back.
        CHECK(out.audit.coordinate_refusal);
        CHECK_EQ(out.audit.tool, std::string("core_line"));
        CHECK_FALSE(out.audit.detail.empty());

        // Nothing ran and nothing was filed.
        CHECK(f.disp.ran.empty());
        CHECK(f.disp.plans.pending().empty());
    }
}

TEST_CASE("Alan hatası isError sonucu, protokol hatası JSON-RPC hatasıdır")
{
    Rig f;
    ai::McpServer server = f.server();

    // A DOMAIN ERROR — an unknown layer, an empty selection, a refusal by
    // validation — is a RESULT with `isError`. A client retries it with
    // different arguments, which is a different action from giving up.
    f.disp.read_fails = true;
    Json arguments;
    arguments.set("katman", Json::string("YOKKATMAN"));
    const ai::HttpOutcome domain = server.handle(tool_call("sorgula", std::move(arguments)).view());
    CHECK_EQ(domain.status, 200);
    CHECK(is_error(result_of(domain)));
    CHECK(text_of(result_of(domain)).find("YOKKATMAN") != std::string::npos);

    // A PROTOCOL ERROR is a JSON-RPC error. An invented tool name, an argument
    // of the wrong JSON type and an undeclared argument are all the client's
    // schema mistakes, and retrying them unchanged cannot work.
    f.disp.read_fails = false;

    const ai::HttpOutcome unknown =
        server.handle(tool_call("boyle_bir_arac_yok", Json::object({})).view());
    CHECK_EQ(unknown.status, 400);
    CHECK_EQ(code_of(unknown), ai::rpc_error_code::kInvalidParams);
    // The message points at the real surface instead of ending the conversation.
    CHECK(error_of(unknown).find("message")->as_string().find("tools/list") != std::string::npos);

    Json wrong_type;
    wrong_type.set("katman", Json::integer(7));
    const ai::HttpOutcome typed = server.handle(tool_call("sorgula", std::move(wrong_type)).view());
    CHECK_EQ(code_of(typed), ai::rpc_error_code::kInvalidParams);

    Json undeclared;
    undeclared.set("boyle_bir_parametre_yok", Json::string("x"));
    const ai::HttpOutcome extra = server.handle(tool_call("sorgula", std::move(undeclared)).view());
    CHECK_EQ(code_of(extra), ai::rpc_error_code::kInvalidParams);
    CHECK(error_of(extra).find("message")->as_string().find("boyle_bir_parametre_yok") !=
          std::string::npos);

    // A missing REQUIRED argument is the same kind of mistake.
    const ai::HttpOutcome missing = server.handle(tool_call("core_line", Json::object({})).view());
    CHECK_EQ(code_of(missing), ai::rpc_error_code::kInvalidParams);

    // A body that is not JSON at all, and a body that is JSON but not a request.
    Req broken;
    broken.mcp_method = "tools/list";
    broken.body       = "{ bu JSON degil";
    CHECK_EQ(code_of(server.handle(broken.view())), ai::rpc_error_code::kParseError);

    Req not_rpc;
    not_rpc.mcp_method = "tools/list";
    not_rpc.body       = R"({"method":"tools/list","id":1})";
    CHECK_EQ(code_of(server.handle(not_rpc.view())), ai::rpc_error_code::kInvalidRequest);
}

TEST_CASE("Eski bir tutamak reddedilir: çizim o zamandan beri değişti")
{
    Rig f;
    ai::McpServer server = f.server();

    const std::string stale = f.points_handle();
    // A handle is only as true as the drawing it was read from. Between the read
    // and the write somebody moved what it names, and drawing at those
    // coordinates would put a line where nothing is any more.
    f.disp.rev = 8;

    Json arguments;
    arguments.set("noktalar", Json::string(stale));
    const ai::HttpOutcome out = server.handle(tool_call("core_line", std::move(arguments)).view());

    CHECK_EQ(out.status, 200);
    CHECK(is_error(result_of(out)));
    // A DOMAIN refusal and not a coordinate one: the reference was well formed
    // and the document moved under it, so this must not be audit-logged as a
    // hallucinated coordinate.
    CHECK_FALSE(out.audit.coordinate_refusal);
    CHECK(f.disp.plans.pending().empty());
}

TEST_CASE("Her kaynak okunur ve üretilmiş metni döner")
{
    Rig f;
    ai::McpServer server = f.server();

    Req listed;
    listed.mcp_method = "resources/list";
    listed.body       = rpc_body("resources/list");

    const Json list       = result_of(server.handle(listed.view()));
    const Json* resources = list.find("resources");
    REQUIRE(resources != nullptr);
    // FOUR NOW: the two generated documents, and the two live ones that TODOS
    // M-05 asks for — the document summary and the layout preflight.
    REQUIRE_EQ(resources->as_array().size(), 4u);

    const auto read = [&](const char* uri) {
        Json params;
        params.set("uri", Json::string(uri));
        Req req;
        req.mcp_method = "resources/read";
        req.mcp_name   = uri;
        req.body       = rpc_body("resources/read", std::move(params));
        return server.handle(req.view());
    };

    for (const char* uri : {ai::kLlmsUri, ai::kLlmsFullUri}) {
        const ai::HttpOutcome out = read(uri);
        REQUIRE_EQ(out.status, 200);
        // The result is held in a NAMED local: `find` hands back a pointer into
        // it, and reading that pointer after a temporary died is how this case
        // first reported an empty `contents` for a perfectly correct answer.
        const Json result    = result_of(out);
        const Json* contents = result.find("contents");
        REQUIRE(contents != nullptr);
        REQUIRE_EQ(contents->as_array().size(), 1u);
        const Json& entry = contents->as_array().front();
        CHECK_EQ(entry.find("uri")->as_string(), std::string(uri));
        CHECK_EQ(entry.find("mimeType")->as_string(), std::string("text/markdown"));
        // GENERATED, never a checked-in copy: the notice is the first thing in
        // the file precisely so a hand edit is visible at a glance.
        CHECK(entry.find("text")->as_string().find("ÜRETİLMİŞ DOSYA") != std::string::npos);
    }

    // The long form really is the long form.
    CHECK(result_of(read(ai::kLlmsFullUri)).dump().size() >
          result_of(read(ai::kLlmsUri)).dump().size());

    const ai::HttpOutcome unknown = read("kentoscad://boyle-bir-kaynak-yok.txt");
    CHECK_EQ(code_of(unknown), ai::rpc_error_code::kInvalidParams);
}

TEST_CASE("subscriptions/listen akış olarak yanıtlanır ve açık kalır")
{
    Rig f;
    ai::McpServer server = f.server();

    Json params;
    Json wanted = Json::array({});
    wanted.push(Json::string("notifications/tools/list_changed"));
    params.set("subscriptions", std::move(wanted));

    Req req;
    req.mcp_method = "subscriptions/listen";
    req.accept     = "application/json, text/event-stream";
    req.body       = rpc_body("subscriptions/listen", std::move(params));

    const ai::HttpOutcome out = server.handle(req.view());
    CHECK_EQ(out.status, 200);
    CHECK_EQ(out.delivery, ai::Delivery::Sse);
    CHECK_EQ(out.header("Content-Type"), std::string_view("text/event-stream"));
    // Asked for by the specification: a buffering reverse proxy would hold every
    // frame until the stream closed, which turns a notification into nothing.
    CHECK_EQ(out.header("X-Accel-Buffering"), std::string_view("no"));

    // NO FINAL RESPONSE, and that is what makes it long-lived: the final response
    // SHOULD terminate its own stream, so a subscription that answered would
    // close the instant it opened.
    CHECK(out.stream.keep_open);
    CHECK(out.stream.keep_alive_ms > 0);
    REQUIRE_EQ(out.stream.initial.size(), 1u);
    CHECK_FALSE(out.stream.initial.front().last);
    CHECK_EQ(out.stream.subscriptions.size(), 1u);

    // The frame a transport writes when the surface changes, and the keep-alive
    // that keeps the proxies between here and there interested.
    const ai::SseFrame changed = ai::McpServer::tools_list_changed();
    CHECK(changed.render().find("notifications/tools/list_changed") != std::string::npos);
    CHECK(changed.render().find("data: ") != std::string::npos);
    CHECK_EQ(ai::sse_comment("kentos"), std::string(": kentos\n\n"));

    // A subscription this server does not publish is refused rather than opened
    // and left silent for ever.
    Json other;
    Json unpublished = Json::array({});
    unpublished.push(Json::string("notifications/resources/updated"));
    other.set("subscriptions", std::move(unpublished));
    Req rejected;
    rejected.mcp_method = "subscriptions/listen";
    rejected.accept     = "text/event-stream";
    rejected.body       = rpc_body("subscriptions/listen", std::move(other));
    CHECK_EQ(code_of(server.handle(rejected.view())), ai::rpc_error_code::kInvalidParams);
}

TEST_CASE("SSE isteyen istemciye yanıt akış olarak çerçevelenir")
{
    Rig f;
    ai::McpServer server = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));
    Req req    = tool_call("core_line", std::move(arguments));
    req.accept = "text/event-stream";

    const ai::HttpOutcome out = server.handle(req.view());
    CHECK_EQ(out.status, 200);
    CHECK_EQ(out.delivery, ai::Delivery::Sse);
    CHECK(out.body.empty());

    // The notification goes out first and the response last, and the transport
    // closes after the last frame — which is what tells the client the exchange
    // is over rather than merely quiet.
    REQUIRE_EQ(out.stream.initial.size(), 2u);
    CHECK_FALSE(out.stream.initial.front().last);
    CHECK(out.stream.initial.front().data.find("notifications/message") != std::string::npos);
    CHECK(out.stream.initial.back().last);
    CHECK(out.stream.initial.back().data.find("\"result\"") != std::string::npos);
    CHECK_FALSE(out.stream.keep_open);

    // One frame's bytes, framed the way SSE wants them.
    const std::string framed = out.stream.initial.back().render();
    CHECK(framed.rfind("event: message\n", 0) == 0);
    CHECK(framed.find("\n\n") != std::string::npos);
}

TEST_CASE("Akış kapanması iptaldir: bekleyen öneri geri çekilir")
{
    Rig f;
    ai::McpServer server = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));
    Req req    = tool_call("core_line", std::move(arguments));
    req.accept = "text/event-stream";

    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);
    const std::string plan_id = out.audit.plan_id;
    REQUIRE_FALSE(plan_id.empty());
    CHECK_EQ(out.stream.plan_id, plan_id);
    REQUIRE_EQ(f.disp.plans.pending().size(), 1u);

    // Closing the stream IS the cancellation signal in 2026-07-28. A suggestion
    // from a client that went away must not sit on somebody's screen for ever.
    server.stream_closed(out.stream);
    CHECK(f.disp.plans.pending().empty());
    const ai::Plan* plan = f.disp.plans.find(plan_id);
    REQUIRE(plan != nullptr);
    CHECK_EQ(plan->state, ai::PlanState::Withdrawn);

    // And nothing was ever run, cancelled or not.
    CHECK(f.disp.ran.empty());
}

TEST_CASE("Belirteç istenmeyen yapıda da yol doğrulanır")
{
    Rig f;
    // A build started without a token: the endpoint still answers only its own
    // path, and a segment after it is not silently treated as one.
    f.policy.token         = "";
    f.policy.require_token = false;
    ai::McpServer server   = f.server();

    const auto send = [&](const char* path) {
        Req req;
        req.path       = path;
        req.mcp_method = "tools/list";
        req.body       = rpc_body("tools/list");
        return server.handle(req.view()).status;
    };

    CHECK_EQ(send("/mcp"), 200);
    CHECK_EQ(send("/mcp/"), 200);
    CHECK_EQ(send("/mcp?x=1"), 200);
    CHECK_EQ(send("/mcp/ne-olursa"), 404);
    CHECK_EQ(send("/"), 404);

    // Constant-time comparison is still equality; the timing is what differs.
    CHECK(ai::constant_time_equals("abc", "abc"));
    CHECK_FALSE(ai::constant_time_equals("abc", "abd"));
    CHECK_FALSE(ai::constant_time_equals("abc", "ab"));

    // And the header decoder leaves an ordinary value alone.
    CHECK_EQ(ai::decode_header_value("sorgula"), std::string("sorgula"));
    CHECK_EQ(ai::decode_header_value("=?base64?c29yZ3VsYQ==?="), std::string("sorgula"));
    CHECK_EQ(ai::decode_header_value("=?base64?!!!?="), std::string("=?base64?!!!?="));
}

TEST_CASE("MCP: araç yüzeyi sohbetinkiyle aynı katalogdan gelir")
{
    // TODOS M-03. The surface is PROJECTED from `Registry` rather than listed
    // anywhere (CLAUDE.md 5.10, 5.20), so opening a command to agents puts it in
    // MCP by construction — and this is the test that says so out loud, because
    // "by construction" is a claim that stops being true the day somebody adds a
    // second list.
    kentos::command::Registry reg;
    kentos::command::register_builtin_commands(reg);
    kentos::ai::register_ai_commands(reg);

    const kentos::ai::Catalog catalog = kentos::ai::build_catalog(reg);

    const auto has_tool = [&](std::string_view id) {
        return std::any_of(catalog.tools.begin(), catalog.tools.end(),
                           [&](const kentos::ai::ToolDef& t) { return t.command_id == id; });
    };

    // THE ONES A-02 OPENED, all four of them.
    CHECK(has_tool("core.layout"));
    CHECK(has_tool("core.layout_item"));
    CHECK(has_tool("core.layout_template"));
    CHECK(has_tool("core.print"));

    // THE CONTEXT TOOL A-01 ADDED.
    CHECK(has_tool("core.context"));

    // AND THE ONES DELIBERATELY KEPT OUT stay out: a client cannot open its own
    // way in (`core.mcp`), manage its own provider (`core.ai_provider`), or run
    // an arbitrary file as a command sequence (`core.script`).
    CHECK_FALSE(has_tool("core.mcp"));
    CHECK_FALSE(has_tool("core.ai_provider"));
    CHECK_FALSE(has_tool("core.script"));

    // Every tool the catalogue carries names a command that exists: a surface
    // describing something the program does not have is worse than a smaller one.
    for (const kentos::ai::ToolDef& one : catalog.tools)
        CHECK(reg.by_id(one.command_id) != nullptr);
}

TEST_CASE("MCP: taşınmayan bir bildirim kabiliyeti ilan edilmiyor")
{
    // TODOS M-02 says it plainly: do not advertise a notification capability the
    // program does not carry. The protocol layer BUILDS `tools/list_changed` frames
    // and the live transport cannot deliver them — `mcp_service.cpp` writes an
    // SSE response once and closes it — so a client that subscribed would hold a
    // socket waiting for something that can never arrive.
    //
    // Declaring a capability the program does not have is worse than lacking it:
    // a client that trusts the declaration stops polling.
    //
    // THIS TEST IS ALSO THE REMINDER. When `McpService` grows a stored responder
    // and a keep-alive timer, this fails — and that failure is the prompt to turn
    // the declaration back on in the same change.
    Rig f;
    ai::McpServer server = f.server();

    Req req;
    req.mcp_method = "server/discover";
    req.body       = rpc_body("server/discover");

    const ai::HttpOutcome out = server.handle(req.view());
    REQUIRE_EQ(out.status, 200);
    const Json answer = result_of(out);

    const Json* caps = answer.find("capabilities");
    REQUIRE(caps != nullptr);

    const Json* tools = caps->find("tools");
    REQUIRE(tools != nullptr);
    REQUIRE(tools->find("listChanged") != nullptr);
    CHECK_FALSE(tools->find("listChanged")->as_bool());

    // And no subscriptions capability at all, for the same reason.
    CHECK(caps->find("subscriptions") == nullptr);
}

TEST_CASE("MCP: canlı kaynaklar aynı komutlardan besleniyor")
{
    // TODOS M-05. A resource that computed a document summary of its own would be
    // a second answer to one question, and the two would drift (CLAUDE.md 5.10).
    // These run the very commands that already answer them, through the same
    // read-only door every other caller uses.
    Rig f;
    ai::McpServer server = f.server();

    const auto read = [&](const char* uri) {
        Json params;
        params.set("uri", Json::string(uri));
        Req req;
        req.mcp_method = "resources/read";
        req.mcp_name   = uri;
        req.body       = rpc_body("resources/read", std::move(params));
        return server.handle(req.view());
    };

    for (const char* uri : {ai::kContextUri, ai::kPreflightUri}) {
        const ai::HttpOutcome out = read(uri);
        REQUIRE_EQ(out.status, 200);
        const Json result    = result_of(out);
        const Json* contents = result.find("contents");
        REQUIRE(contents != nullptr);
        REQUIRE_EQ(contents->as_array().size(), 1u);
        const Json& entry = contents->as_array()[0];
        CHECK_EQ(entry.find("uri")->as_string(), uri);
        // JSON, NOT PROSE. An agent should not have to parse a Turkish sentence
        // to learn a layer count.
        CHECK_EQ(entry.find("mimeType")->as_string(), "application/json");
        CHECK(!entry.find("text")->as_string().empty());
    }

    // AND A URI NOBODY SERVES IS NAMED, WITH WHAT IS SERVED. A client that asked
    // for the wrong thing should not have to guess what the right thing is.
    Json params;
    params.set("uri", Json::string("kentoscad://yok"));
    Req bad;
    bad.mcp_method                = "resources/read";
    bad.mcp_name                  = "kentoscad://yok";
    bad.body                      = rpc_body("resources/read", std::move(params));
    const ai::HttpOutcome refused = server.handle(bad.view());
    // `400` WITH `-32602`, which is what this revision says an invalid parameter
    // is — and the message names every URI that IS served, so a client that asked
    // for the wrong thing does not have to guess the right one.
    CHECK_EQ(refused.status, 400);
    CHECK(refused.body.find("kentoscad://belge/ozet") != std::string::npos);
    CHECK(refused.body.find("kentoscad://yerlesim/denetim") != std::string::npos);
}

TEST_CASE("M-07: iki istemci birbirinin tutamağını kullanamaz")
{
    Rig f;
    ai::McpServer server = f.server();

    // TWO AGENTS ON ONE LOOPBACK PORT ARE TWO STRANGERS. They present the same
    // token — it is the workstation's token, not a per-agent one — and name
    // themselves in `_meta`, so what tells them apart is the requester label.
    const auto call_as = [&](const char* who, const char* tool, Json arguments,
                             Json extra = Json::null()) {
        Json meta = extra.is_object() ? extra : Json::object({});
        meta.set(ai::kClientMetaKey, Json::string(who));
        return server.handle(tool_call(tool, std::move(arguments), std::move(meta)).view());
    };

    // A reads the drawing. The handle is minted into A's store, and the label
    // the server derives has to be the one the read arrived under — otherwise a
    // client could not use the handle it was just given.
    Json ask;
    ask.set("katman", Json::string("0"));
    const ai::HttpOutcome read = call_as("Ajan A", "sorgula", std::move(ask));
    REQUIRE_EQ(read.status, 200);
    REQUIRE_EQ(f.disp.asked_by.size(), 1u);
    CHECK_EQ(f.disp.asked_by.front(), f.requester("Ajan A"));

    const std::string a_handle = f.points_handle("Ajan A");

    // A draws with it: accepted, and filed as a suggestion under A's name.
    Json mine;
    mine.set("noktalar", Json::string(a_handle));
    const ai::HttpOutcome drew = call_as("Ajan A", "core_line", std::move(mine));
    REQUIRE_EQ(drew.status, 200);
    REQUIRE_FALSE(is_error(result_of(drew)));
    const std::string plan_id = drew.audit.plan_id;
    REQUIRE_FALSE(plan_id.empty());

    // ---- B NAMES A'S HANDLE -------------------------------------------------
    //
    // It is not a protocol error — the string is well formed — and it is not
    // silently accepted either. B never read the drawing, so it holds no
    // promise about those coordinates (CLAUDE.md 5.8), and the answer is the
    // same one an unknown handle already gets: read first.
    Json stolen;
    stolen.set("noktalar", Json::string(a_handle));
    const ai::HttpOutcome theft = call_as("Ajan B", "core_line", std::move(stolen));
    CHECK_EQ(theft.status, 200);
    CHECK(is_error(result_of(theft)));
    CHECK(text_of(result_of(theft)).find("Böyle bir tutamak yok") != std::string::npos);

    // AND NOTHING WAS FILED FOR B. The refusal happened while the arguments were
    // still JSON, so there is no half-composed suggestion on anybody's screen.
    CHECK_EQ(f.disp.plans.pending().size(), 1u);

    // ---- THE COUNTER IS THE REASON THIS MATTERS ----------------------------
    //
    // `HandleStore::next_id` hashes a per-store counter, so B's first handle has
    // exactly the id A's first handle has. Two clients sharing one store would
    // therefore collide rather than merely overlap; with a store each, the same
    // id means a different thing to each of them, which is what the comment in
    // `next_id` always claimed.
    ai::HandleScopes scopes;
    const std::string first_of_a =
        scopes.for_client("A").mint_points({core::Point2{1, 1}}, "sorgula", 1).id;
    const std::string first_of_b =
        scopes.for_client("B").mint_points({core::Point2{9, 9}}, "sorgula", 1).id;
    CHECK_EQ(first_of_a, first_of_b);
    REQUIRE(scopes.peek("A") != nullptr);
    REQUIRE(scopes.peek("B") != nullptr);
    CHECK_EQ(scopes.peek("A")->find(first_of_a)->points.front().x, 1);
    CHECK_EQ(scopes.peek("B")->find(first_of_b)->points.front().x, 9);
}

TEST_CASE("M-07: iki istemci birbirinin planını kullanamaz")
{
    Rig f;
    ai::McpServer server = f.server();

    const auto call_as = [&](const char* who, const char* tool, Json arguments,
                             Json extra = Json::null()) {
        Json meta = extra.is_object() ? extra : Json::object({});
        meta.set(ai::kClientMetaKey, Json::string(who));
        return server.handle(tool_call(tool, std::move(arguments), std::move(meta)).view());
    };

    Json first;
    first.set("noktalar", Json::string(f.points_handle("Ajan A")));
    const ai::HttpOutcome opened = call_as("Ajan A", "core_line", std::move(first));
    REQUIRE_EQ(opened.status, 200);
    const std::string plan_id = opened.audit.plan_id;
    REQUIRE_FALSE(plan_id.empty());

    // ---- B APPENDS TO A'S SUGGESTION ---------------------------------------
    //
    // THIS IS THE ONE THAT MATTERS. A plan id travels in A's answer, so it is
    // not a secret; if an append were allowed on the strength of naming the id,
    // B's step would be applied by the engineer who read A's lines and clicked
    // Uygula. The audit record would name A for a step A never composed, and
    // the drawing would carry a line nobody approved (ai.md R6, R8).
    Json push;
    push.set("noktalar", Json::string(f.points_handle("Ajan B")));
    Json aim;
    aim.set("plan", Json::string(plan_id));
    const ai::HttpOutcome intruded =
        call_as("Ajan B", "core_line", std::move(push), std::move(aim));
    CHECK_EQ(intruded.status, 200);
    CHECK(is_error(result_of(intruded)));

    const ai::Plan* plan = f.disp.plans.find(plan_id);
    REQUIRE(plan != nullptr);
    CHECK_EQ(plan->steps.size(), 1u);
    CHECK_EQ(plan->requester, f.requester("Ajan A"));

    // ---- B CANNOT READ IT EITHER, and gets the same words an absent plan
    // gets: a different refusal would be an oracle telling B that A is
    // composing something, and a plan id is short enough to walk.
    CHECK(f.disp.plan_state(plan_id, f.requester("Ajan B"))
              .error()
              .message.find("Böyle bir öneri yok") != std::string::npos);
    CHECK(f.disp.plan_state(plan_id, f.requester("Ajan A")).ok());

    // ---- B'S STREAM CLOSING DOES NOT CANCEL A'S SUGGESTION -----------------
    //
    // Closing a stream is how 2026-07-28 spells cancellation, so an unscoped
    // withdrawal would let any client cancel any pending plan by naming its id.
    ai::StreamPlan theirs;
    theirs.plan_id   = plan_id;
    theirs.requester = f.requester("Ajan B");
    server.stream_closed(theirs);
    CHECK_EQ(f.disp.plans.find(plan_id)->state, ai::PlanState::Pending);

    // A's own close does withdraw it, which is the behaviour the scoping had to
    // preserve rather than trade away.
    ai::StreamPlan ours;
    ours.plan_id   = plan_id;
    ours.requester = f.requester("Ajan A");
    server.stream_closed(ours);
    CHECK_EQ(f.disp.plans.find(plan_id)->state, ai::PlanState::Withdrawn);

    // ---- AND THE PERSON AT THE KEYBOARD SEES EVERYTHING --------------------
    //
    // An empty label is the operator, who applies the plans and therefore has to
    // be able to list them whoever filed them (`ClientScope::client`).
    CHECK(f.disp.plan_state(plan_id, std::string{}).ok());
}

TEST_CASE("M-08: defter kimin konuştuğunu ve neyle reddedildiğini tutar")
{
    ai::ClientLedger ledger;
    CHECK_EQ(ledger.size(), 0u);

    // A CALL WITHOUT A REQUESTER IS NOT A CLIENT. Nothing on the socket path
    // produces one, but a caller that built an `AuditNote` by hand might, and an
    // empty row in the settings table is worse than no row.
    ai::AuditNote nameless;
    nameless.method = "tools/list";
    ledger.note(nameless, 100);
    CHECK_EQ(ledger.size(), 0u);

    ai::AuditNote call;
    call.requester = "Ajan A #0000beef";
    call.method    = "tools/call";
    call.tool      = "sorgula";
    ledger.note(call, 100);

    const ai::ClientRecord* held = ledger.find("Ajan A #0000beef");
    REQUIRE(held != nullptr);
    CHECK_EQ(held->calls, 1u);
    CHECK_EQ(held->refusals, 0u);
    CHECK_EQ(held->first_seen, 100u);
    CHECK_EQ(held->last_seen, 100u);
    CHECK_EQ(held->last_method, std::string("tools/call"));

    // A REFUSAL IS COUNTED AND ITS REASON KEPT — per client, because a person
    // looking at a misbehaving agent wants ITS errors and not everybody's
    // interleaved (TODOS M-08's "son hatalar").
    call.detail = "Böyle bir katman yok: 'YOKKATMAN'.";
    ledger.note(call, 140);
    CHECK_EQ(ledger.find("Ajan A #0000beef")->calls, 2u);
    CHECK_EQ(ledger.find("Ajan A #0000beef")->refusals, 1u);
    CHECK_EQ(ledger.find("Ajan A #0000beef")->last_refusal,
             std::string("Böyle bir katman yok: 'YOKKATMAN'."));
    CHECK_EQ(ledger.find("Ajan A #0000beef")->last_seen, 140u);
    CHECK_EQ(ledger.find("Ajan A #0000beef")->first_seen, 100u);

    // A filed suggestion is counted separately from a call: the interesting
    // number is how much this agent is asking a person to sign, not how chatty
    // it is.
    ai::AuditNote filed;
    filed.requester = "Ajan B #0000beef";
    filed.method    = "tools/call";
    filed.plan_id   = "p0000000000000001";
    ledger.note(filed, 200);
    CHECK_EQ(ledger.find("Ajan B #0000beef")->plans, 1u);

    // MOST RECENTLY SEEN FIRST, so the table and a script agree and so the agent
    // somebody is watching right now is at the top.
    const std::vector<ai::ClientRecord> listed = ledger.clients();
    REQUIRE_EQ(listed.size(), 2u);
    CHECK_EQ(listed.front().label, std::string("Ajan B #0000beef"));
}

TEST_CASE("M-08: tek bir istemcinin yetkisi kaldırılır, belirteç değişmez")
{
    Rig f;
    ai::ClientLedger ledger;
    ai::McpServer server = f.server();
    server.set_ledger(&ledger);

    const auto list_as = [&](const char* who) {
        Json params;
        Json meta;
        meta.set(ai::kProtocolVersionMetaKey, Json::string(ai::Catalog::kProtocolVersion));
        meta.set(ai::kClientMetaKey, Json::string(who));
        params.set("_meta", std::move(meta));

        Json body;
        body.set("jsonrpc", Json::string("2.0"));
        body.set("id", Json::integer(1));
        body.set("method", Json::string("tools/list"));
        body.set("params", std::move(params));

        Req req;
        req.mcp_method = "tools/list";
        req.body       = body.dump();
        return server.handle(req.view());
    };

    CHECK_EQ(list_as("Ajan A").status, 200);
    CHECK_EQ(list_as("Ajan B").status, 200);
    CHECK_EQ(ledger.size(), 2u);

    // ---- ONE CLIENT, NOT ALL OF THEM ---------------------------------------
    //
    // Rotating the token is the blunt instrument and it locks out the agent the
    // person is working with too. This is the sharp one: B stops, A does not
    // notice, and the token on the settings page is untouched.
    REQUIRE(ledger.revoke(f.requester("Ajan B")));

    const ai::HttpOutcome refused = list_as("Ajan B");
    CHECK_EQ(refused.status, 403);
    CHECK_EQ(list_as("Ajan A").status, 200);

    // SAID IN WORDS, not as a bare 403: the token is still valid, so an agent
    // told only "forbidden" would retry it for ever. And the answer must not
    // carry the token — the label holds its fingerprint and nothing else
    // (CLAUDE.md 5.21).
    CHECK(refused.body.find("yetkisi") != std::string::npos);
    CHECK(refused.body.find("-32001") != std::string::npos);
    CHECK(refused.body.find(f.policy.token) == std::string::npos);

    // THE REFUSAL IS RECORDED against the client it was aimed at, so the page
    // can show that the revocation is actually biting.
    const ai::ClientRecord* held = ledger.find(f.requester("Ajan B"));
    REQUIRE(held != nullptr);
    CHECK(held->revoked);
    CHECK_FALSE(held->last_refusal.empty());

    // AND IT IS REVERSIBLE, because a person who shut the wrong one out has to
    // be able to say so.
    REQUIRE(ledger.restore(f.requester("Ajan B")));
    CHECK_EQ(list_as("Ajan B").status, 200);

    // Restoring something that was never revoked is a refusal rather than a
    // silent no-op: it is almost always a typo in a name.
    CHECK_FALSE(ledger.restore("kimse"));
    CHECK_FALSE(ledger.revoke(""));

    // A CLIENT THAT HAS NOT CALLED YET can still be shut out — the useful
    // moment, not an edge case.
    REQUIRE(ledger.revoke("Ajan C #ffffffff"));
    CHECK(ledger.revoked("Ajan C #ffffffff"));

    // ---- A TOKEN ROTATION FORGETS EVERYTHING -------------------------------
    //
    // Every label carries the old token's fingerprint, so after a rotation not
    // one of them can be presented again. Keeping them would be keeping names
    // nobody will answer to.
    ledger.forget_all();
    CHECK_EQ(ledger.size(), 0u);
    CHECK_FALSE(ledger.revoked("Ajan C #ffffffff"));
}

TEST_CASE("M-08: defter sınırlıdır ama yetkisizliği unutmaz")
{
    ai::ClientLedger ledger;
    REQUIRE(ledger.revoke("yasakli"));

    // A CALLER THAT INVENTS A NAME PER CALL must not grow the program's memory,
    // and — this is the half that matters — must not be able to push a person's
    // revocation out of the list by doing so. Evicting the oldest UNREVOKED
    // record is what makes flooding useless as an attack.
    for (std::size_t i = 0; i < ai::ClientLedger::kMaxClients * 2; ++i) {
        ai::AuditNote call;
        call.requester = "gecici-" + std::to_string(i);
        call.method    = "tools/list";
        ledger.note(call, 1000 + i);
    }

    CHECK(ledger.size() <= ai::ClientLedger::kMaxClients);
    CHECK(ledger.revoked("yasakli"));
}

TEST_CASE("M-06: aynı istek iki kez gelirse ikinci bir öneri açılmaz")
{
    Rig f;
    ai::McpServer server = f.server();

    const auto draw = [&](const char* key) {
        Json arguments;
        arguments.set("noktalar", Json::string(f.points_handle()));
        Json meta;
        meta.set("idempotency", Json::string(key));
        return server.handle(tool_call("core_line", std::move(arguments), std::move(meta)).view());
    };

    // A CLIENT WHOSE CONNECTION DROPS MID-CALL cannot tell whether the call
    // arrived; retrying is the only thing it can do. Without a key the retry
    // files a SECOND suggestion and the person at the workstation gets two
    // identical cards for one piece of work (TODOS M-06).
    const ai::HttpOutcome first = draw("is-1");
    REQUIRE_EQ(first.status, 200);
    REQUIRE_FALSE(is_error(result_of(first)));
    const std::string plan_id = first.audit.plan_id;
    REQUIRE_FALSE(plan_id.empty());
    CHECK_EQ(f.disp.plans.pending().size(), 1u);

    const ai::HttpOutcome retry = draw("is-1");
    REQUIRE_EQ(retry.status, 200);
    CHECK_FALSE(is_error(result_of(retry)));
    CHECK_EQ(retry.audit.plan_id, plan_id);
    CHECK_EQ(f.disp.plans.pending().size(), 1u);
    CHECK_EQ(f.disp.plans.find(plan_id)->steps.size(), 1u);

    // AND THE CLIENT IS TOLD which of the three things happened: a retry that
    // read "kaydı açıldı" would leave the agent believing it had asked for two
    // pieces of work.
    CHECK(text_of(result_of(retry)).find("aynı istek") != std::string::npos);
    CHECK(text_of(result_of(first)).find("aynı istek") == std::string::npos);

    // A DIFFERENT KEY IS A DIFFERENT JOB. The guard must not collapse two real
    // requests into one.
    const ai::HttpOutcome other = draw("is-2");
    REQUIRE_EQ(other.status, 200);
    CHECK_NE(other.audit.plan_id, plan_id);
    CHECK_EQ(f.disp.plans.pending().size(), 2u);

    // AND THE KEY IS SCOPED TO THE CLIENT. Two agents may use the same word for
    // two different jobs, and one must not be handed another's plan by guessing
    // a key (M-07).
    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle("Ajan B")));
    Json meta;
    meta.set("idempotency", Json::string("is-1"));
    meta.set(ai::kClientMetaKey, Json::string("Ajan B"));
    const ai::HttpOutcome stranger =
        server.handle(tool_call("core_line", std::move(arguments), std::move(meta)).view());
    REQUIRE_EQ(stranger.status, 200);
    CHECK_NE(stranger.audit.plan_id, plan_id);
}

TEST_CASE("M-06: uygulanmakta olan bir öneri bitmiş görünmez")
{
    ai::PlanStore plans;

    ai::Plan one;
    one.requester = "Ajan A";
    for (int i = 0; i < 3; ++i) {
        ai::PlanStep step;
        step.command_id = "core.layer";
        step.line       = "KATMAN ad=k" + std::to_string(i);
        one.steps.push_back(step);
    }
    const std::string id = plans.add(std::move(one));

    // BEFORE: waiting for a person, and the answer says exactly that.
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Pending);
    CHECK(plans.find(id)->to_json().find("aciklama")->as_string().find("uygulanmadı") !=
          std::string::npos);

    // DURING: a client polling mid-application was told `beklemede` — "still
    // waiting for a person" — which is false the moment the person has clicked,
    // and sends the agent to ask the user why they have not decided yet.
    REQUIRE(plans.begin_apply(id));
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Running);
    CHECK_FALSE(plans.pending().size() == 1u); // no longer on anybody's screen

    plans.report_progress(id, 2);
    const core::Json running = plans.find(id)->to_json();
    CHECK_EQ(running.find("durum")->as_string(), std::string("uygulaniyor"));
    CHECK_EQ(running.find("biten_adim")->as_int(), 2);
    CHECK_EQ(running.find("toplam_adim")->as_int(), 3);
    // SAID IN WORDS, because a half-finished result read as a finished one is
    // the failure M-06 names: the file list is only complete at `uygulandi`.
    CHECK(running.find("aciklama")->as_string().find("henüz bitmedi") != std::string::npos);
    CHECK(running.find("yazilan_dosyalar") == nullptr);

    // A SECOND APPLY IS REFUSED while one is running: the person said yes once.
    CHECK_FALSE(plans.begin_apply(id));

    // AFTER: `Running` is not a decision, so settling from it is allowed — but
    // only once.
    REQUIRE(plans.settle(id, ai::PlanState::Applied));
    CHECK_EQ(plans.find(id)->state, ai::PlanState::Applied);
    CHECK_FALSE(plans.settle(id, ai::PlanState::Rejected));

    // And progress stops being written once it is over: a count on a settled
    // plan would read as work still going.
    plans.report_progress(id, 99);
    CHECK_EQ(plans.find(id)->done_steps, 2u);
}

// ============================================================================
// A-03 — the approval policy the person chose, on the MCP road
// ============================================================================

TEST_CASE("A-03: otomatik politikada yazan çağrı UYGULANIR ve yanıt bunu söyler")
{
    Rig f;
    f.disp.prefs.approval  = ai::ApprovalPolicy::Automatic;
    f.disp.apply_by_policy = true;
    ai::McpServer server   = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));
    const ai::HttpOutcome out = server.handle(tool_call("core_line", std::move(arguments)).view());
    REQUIRE_EQ(out.status, 200);

    const Json result = result_of(out);
    CHECK_FALSE(is_error(result));
    const std::string text = text_of(result);
    // NOT "çizim değişmedi": under `otomatik` that sentence was a lie about the
    // drawing the client is working on.
    CHECK(text.find("BU SATIRLAR UYGULANDI") != std::string::npos);
    CHECK(text.find("Çizim değişmedi") == std::string::npos);

    const Json* structured = result.find("structuredContent");
    REQUIRE(structured != nullptr);
    CHECK_EQ(structured->find("durum")->as_string(), std::string("uygulandi"));
    CHECK_EQ(structured->find("karar_veren")->as_string(), std::string("politika:otomatik"));
    const Json* meta = result.find("_meta");
    REQUIRE(meta != nullptr);
    CHECK_EQ(meta->find("cad.kentos/approval")->as_string(), std::string("policy-applied"));

    // THE AUDIT RECORD SAYS THE POLICY DECIDED, never a person (S-06).
    REQUIRE_EQ(f.disp.audit_lines.size(), 1u);
    CHECK(f.disp.audit_lines.front().find("\"karar_veren\":\"politika:otomatik\"") !=
          std::string::npos);
}

TEST_CASE("A-03: her değişiklikte politikasında yazan çağrı bekler ve sebebini söyler")
{
    Rig f;
    f.disp.apply_by_policy = true; ///< the default policy: `her_degisiklikte`
    ai::McpServer server   = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));
    const Json result =
        result_of(server.handle(tool_call("core_line", std::move(arguments)).view()));
    const std::string text = text_of(result);
    CHECK(text.find("Çizim değişmedi") != std::string::npos);
    CHECK(text.find("Onay bekleme sebebi: Her değişiklikte onay isteniyor.") != std::string::npos);
    CHECK_EQ(result.find("_meta")->find("cad.kentos/approval")->as_string(),
             std::string("user-required"));
    CHECK(f.disp.audit_lines.empty()); ///< nothing decided yet
}

TEST_CASE("A-03: uygulanmış bir öneriye eklenen adım yeni bir öneri olarak açılır")
{
    // UNDER `otomatik` a sequence composed one call at a time outran its own
    // approval: the second call named a plan that was already applied, and was
    // refused — so every multi-call job failed at its second step.
    Rig f;
    f.disp.prefs.approval  = ai::ApprovalPolicy::Automatic;
    f.disp.apply_by_policy = true;
    ai::McpServer server   = f.server();

    Json first_args;
    first_args.set("noktalar", Json::string(f.points_handle()));
    const Json first =
        result_of(server.handle(tool_call("core_line", std::move(first_args)).view()));
    const std::string first_id = first.find("structuredContent")->find("oneri")->as_string();

    Json second_args;
    second_args.set("noktalar", Json::string(f.points_handle()));
    Json meta;
    meta.set("plan", Json::string(first_id));
    const Json second = result_of(
        server.handle(tool_call("core_line", std::move(second_args), std::move(meta)).view()));
    CHECK_FALSE(is_error(second));
    const std::string text = text_of(second);
    CHECK(text.find("zaten uygulanmıştı") != std::string::npos);
    const std::string second_id = second.find("structuredContent")->find("oneri")->as_string();
    CHECK(second_id != first_id);
    CHECK_EQ(second.find("structuredContent")->find("durum")->as_string(),
             std::string("uygulandi"));
}

TEST_CASE("A-03: bildirilen varsayımlar yanıta, öneriye ve denetim kaydına taşınır")
{
    Rig f;
    f.disp.prefs.approval  = ai::ApprovalPolicy::Automatic;
    f.disp.prefs.questions = ai::QuestionPolicy::Assume;
    f.disp.apply_by_policy = true;
    ai::McpServer server   = f.server();

    Json arguments;
    arguments.set("noktalar", Json::string(f.points_handle()));
    Json noted = Json::array({});
    noted.push(Json::string("Katman söylenmediği için etkin katmana çizildi."));
    arguments.set(ai::kAssumptions, std::move(noted));
    const Json result =
        result_of(server.handle(tool_call("core_line", std::move(arguments)).view()));
    CHECK_FALSE(is_error(result));

    const std::string text = text_of(result);
    CHECK(text.find("Katman söylenmediği için etkin katmana çizildi.") != std::string::npos);
    const Json* listed = result.find("structuredContent")->find("varsayimlar");
    REQUIRE(listed != nullptr);
    CHECK_EQ(listed->as_array().size(), 1u);
    REQUIRE_EQ(f.disp.audit_lines.size(), 1u);
    CHECK(f.disp.audit_lines.front().find("\"varsayimlar\"") != std::string::npos);

    // A READING TOOL takes no assumptions: there is nothing for them to explain,
    // and the field is refused like any other the schema does not declare.
    Json read;
    read.set(ai::kAssumptions, Json::string("hiçbiri"));
    const Json refused =
        error_of(server.handle(tool_call("katmanlari_listele", std::move(read)).view()));
    REQUIRE(refused.find("message") != nullptr);
    CHECK(refused.find("message")->as_string().find("varsayimlar") != std::string::npos);
}
