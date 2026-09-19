// SPDX-License-Identifier: AGPL-3.0-or-later
// AGPL rather than GPL for the reason `jsonrpc.hpp` states: this is part of a
// server component, and CLAUDE.md Article 2.1 puts those under AGPLv3.
#include "kentos_cad/ai/jsonrpc.hpp"

namespace kentos::ai {
namespace {

using core::Json;

} // namespace

core::Result<JsonRpcRequest> parse_rpc(const Json& body)
{
    if (!body.is_object())
        return core::err(core::ErrorCode::InvalidArgument,
                         "JSON-RPC isteği bir nesne olmalı; gelen: bir nesne değil.");

    // A BATCH IS NOT SUPPORTED, and saying so is better than half-supporting it.
    // JSON-RPC allows an array of requests; MCP 2026-07-28 answers one request
    // per POST, and a batch would have to choose a single HTTP status for
    // several outcomes. A client that sends one gets this sentence rather than
    // silence — which is the difference between a bug report and a mystery.
    const Json* version = body.find("jsonrpc");
    if (version == nullptr || !version->is_string() || version->as_string() != "2.0")
        return core::err(
            core::ErrorCode::InvalidArgument,
            "`jsonrpc` alanı \"2.0\" olmalı; gelen: " +
                (version == nullptr ? std::string("yok") : "'" + version->dump() + "'") +
                ". Toplu (batch) istek desteklenmiyor.");

    const Json* method = body.find("method");
    if (method == nullptr || !method->is_string() || method->as_string().empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "`method` alanı boş olmayan bir dize olmalı; gelen: " +
                             (method == nullptr ? std::string("yok") : method->dump()) + ".");

    JsonRpcRequest out;
    out.method = method->as_string();

    // ABSENT `id` IS THE TEST, not a null one. JSON-RPC 2.0 says a notification
    // is a request "without an id member"; a member explicitly set to null is a
    // request whose id happens to be null, and answering it is still correct.
    // Treating the two the same would drop an answer the client is waiting for.
    if (const Json* id = body.find("id"); id != nullptr)
        out.id = *id;
    else
        out.is_notification = true;

    if (const Json* params = body.find("params"); params != nullptr) {
        if (!params->is_object())
            return core::err(core::ErrorCode::InvalidArgument,
                             "`params` bir nesne olmalı; MCP konumsal parametre kullanmaz.");
        out.params = *params;
    } else {
        out.params = Json::object({});
    }

    if (const Json* meta = out.params.find("_meta"); meta != nullptr && meta->is_object()) {
        out.meta = *meta;
        if (const Json* declared = meta->find(kProtocolVersionMetaKey);
            declared != nullptr && declared->is_string())
            out.protocol_version = declared->as_string();
    }
    return out;
}

Json rpc_result(const Json& id, Json result)
{
    Json out;
    out.set("jsonrpc", Json::string("2.0"));
    out.set("id", id);
    out.set("result", std::move(result));
    return out;
}

Json rpc_error(const Json& id, int code, std::string message, Json data)
{
    Json error;
    error.set("code", Json::integer(code));
    error.set("message", Json::string(std::move(message)));
    if (!data.is_null()) error.set("data", std::move(data));

    Json out;
    out.set("jsonrpc", Json::string("2.0"));
    out.set("id", id);
    out.set("error", std::move(error));
    return out;
}

Json rpc_notification(std::string method, Json params)
{
    Json out;
    out.set("jsonrpc", Json::string("2.0"));
    out.set("method", Json::string(std::move(method)));
    if (!params.is_null()) out.set("params", std::move(params));
    return out;
}

} // namespace kentos::ai
