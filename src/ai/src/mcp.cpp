// SPDX-License-Identifier: AGPL-3.0-or-later
// AGPL rather than GPL for the reason `jsonrpc.hpp` states: CLAUDE.md Article 2.1
// puts a server component under AGPLv3, and this file is the heart of one.
#include "kentos_cad/ai/mcp.hpp"

#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/llmstxt.hpp"

#include "kentos_cad/core/text.hpp"

#include <cstdio>
#include <locale>
#include <sstream>

namespace kentos::ai {
namespace {

using core::Json;
using rpc_error_code::kHeaderMismatch;
using rpc_error_code::kInternalError;
using rpc_error_code::kInvalidParams;
using rpc_error_code::kInvalidRequest;
using rpc_error_code::kMethodNotFound;
using rpc_error_code::kParseError;

/// The one notification method this server ever sends on its own initiative.
constexpr const char* kToolsListChanged = "notifications/tools/list_changed";

/// The HTTP status a JSON-RPC error deserves.
///
/// THE MAPPING IS THE SPECIFICATION'S, not a convention this file invented: a
/// header or version problem is 400, an unknown method is 404, and a server fault
/// is 500. Everything else a client got wrong is 400 too, which keeps the rule a
/// client has to remember down to "4xx means do not retry this unchanged".
int status_for(int code)
{
    switch (code) {
    case kMethodNotFound: return 404;
    case kInternalError: return 500;
    default: return 400;
    }
}

/// One `{"type":"text"}` content block, which is the only block shape this
/// server produces.
Json text_block(std::string text)
{
    Json block;
    block.set("type", Json::string("text"));
    block.set("text", Json::string(std::move(text)));
    return block;
}

/// A `tools/call` result: one text block, optional structured content, and the
/// `isError` flag that distinguishes a domain refusal from a protocol error.
Json call_result(std::string text, Json structured, bool is_error, Json meta)
{
    Json content = Json::array({});
    content.push(text_block(std::move(text)));

    Json out;
    out.set("content", std::move(content));
    if (!structured.is_null()) out.set("structuredContent", std::move(structured));
    out.set("isError", Json::boolean(is_error));
    if (!meta.is_null()) out.set("_meta", std::move(meta));
    return out;
}

/// A `notifications/message` frame, for a stream that has something to say
/// before the final response.
SseFrame message_frame(const char* level, std::string text)
{
    Json params;
    params.set("level", Json::string(level));
    params.set("logger", Json::string("kentoscad"));
    params.set("data", Json::string(std::move(text)));

    SseFrame frame;
    frame.event = "message";
    frame.data  = rpc_notification("notifications/message", std::move(params)).dump();
    return frame;
}

/// Locale-independent formatting of a real number.
///
/// The idiom is `parser.cpp`'s, and for its reason: the default stream locale
/// would write `3,5` on a Turkish system, and a command line a person can retype
/// must not depend on where they live.
std::string format_number(double v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << v;
    return out.str();
}

/// A coordinate as the command line writes it: easting first, integer
/// millimetres, no decimal point (model.md R37a, and `Mm` is an int64).
std::string format_point(core::Point2 p)
{
    return std::to_string(p.x) + "," + std::to_string(p.y);
}

/// Whether a text value has to be quoted to survive the command line's tokeniser.
bool needs_quotes(const std::string& text)
{
    if (text.empty()) return true;
    for (const char c : text)
        if (c == ' ' || c == '\t' || c == '=' || c == '"') return true;
    return false;
}

/// The command line a person reads on the suggestion card — and could type.
///
/// THE SAME WORDS THEY WOULD TYPE, because the GUI, the command line, a script
/// and this server are equal clients (Article 1.2) and a suggestion the engineer
/// cannot read is a suggestion they cannot be responsible for. Points go out as
/// bare `Y,X` tokens and everything else as `name=value`, which is exactly what
/// `command/parser.hpp` accepts; a value holding a space or an `=` is quoted, the
/// form `ad="YOL KENARI"` the tokeniser already understands.
std::string render_line(const command::CommandSpec& spec, const command::Args& args)
{
    std::string out = spec.names.empty() ? spec.id : spec.names.front();

    for (const auto& [name, value] : args.items()) {
        switch (value.kind()) {
        case command::Value::Kind::Empty: break;

        case command::Value::Kind::Point:
            out += ' ';
            out += format_point(value.as_point());
            break;

        case command::Value::Kind::PointList:
            for (const core::Point2& p : value.as_points()) {
                out += ' ';
                out += format_point(p);
            }
            break;

        case command::Value::Kind::IdList:
            for (const std::int64_t id : value.as_ids()) {
                out += ' ' + name + '=' + std::to_string(id);
            }
            break;

        case command::Value::Kind::Bool:
            out += ' ' + name + '=' + (value.as_bool() ? "evet" : "hayır");
            break;

        case command::Value::Kind::Int:
            out += ' ' + name + '=' + std::to_string(value.as_int());
            break;

        case command::Value::Kind::Number:
            out += ' ' + name + '=' + format_number(value.as_number());
            break;

        case command::Value::Kind::Text: {
            const std::string& text = value.as_text();
            out += ' ' + name + '=';
            out += needs_quotes(text) ? '"' + text + '"' : text;
            break;
        }

        case command::Value::Kind::TextList:
            // THE KEY WRITTEN ONCE PER WORD, which is how the tokeniser reads a
            // list — the same shape `pencere=` and `nesneler=` already use.
            for (const std::string& word : value.as_texts()) {
                out += ' ' + name + '=';
                out += needs_quotes(word) ? '"' + word + '"' : word;
            }
            break;
        }
    }
    return out;
}

/// A few valid tool names, for the message a client gets when it invents one.
///
/// NAMES RATHER THAN A COUNT. A client that guessed a tool name needs a nudge
/// towards the real surface, and `tools/list` is the whole answer — but printing
/// three names it can recognise turns a dead end into a next step.
std::string sample_names(const Catalog& catalog, std::size_t limit)
{
    std::string out;
    std::size_t taken = 0;
    for (const ToolDef& tool : catalog.tools) {
        if (taken >= limit) break;
        if (taken > 0) out += ", ";
        out += tool.name;
        ++taken;
    }
    return out;
}

/// The `tools/list` entry for the protocol layer's own `llms_txt` tool.
Json llms_tool_entry()
{
    Json annotations;
    annotations.set("readOnlyHint", Json::boolean(true));
    annotations.set("destructiveHint", Json::boolean(false));
    annotations.set("idempotentHint", Json::boolean(true));
    annotations.set("openWorldHint", Json::boolean(false));

    Json schema;
    schema.set("type", Json::string("object"));
    schema.set("properties", Json::object({}));
    schema.set("required", Json::array({}));
    schema.set("additionalProperties", Json::boolean(false));

    Json meta;
    meta.set("cad.kentos/approval", Json::string("none"));
    // Says out loud that this one is not projected from the registry, so a client
    // comparing the list against the fingerprint is not surprised by it.
    meta.set("cad.kentos/builtin", Json::boolean(true));

    Json entry;
    entry.set("name", Json::string(kLlmsToolName));
    entry.set("title", Json::string("llms.txt"));
    entry.set("description",
              Json::string("KentOSCad'in kullanim kilavuzunu dondurur: birimler, eksen adlari, "
                           "tutamak kurali, uygulama kurali ve araclarin kullanim sirasi. "
                           "Bu arac hicbir seyi degistirmez; dogrudan calisir. "
                           "Ayni metin `kentoscad://llms.txt` kaynagi olarak da okunabilir."));
    entry.set("inputSchema", std::move(schema));
    entry.set("annotations", std::move(annotations));
    entry.set("_meta", std::move(meta));
    return entry;
}

/// One `resources/list` entry.
Json resource_entry(const char* uri, const char* name, std::string title, std::string description)
{
    Json out;
    out.set("uri", Json::string(uri));
    out.set("name", Json::string(name));
    out.set("title", Json::string(std::move(title)));
    out.set("description", Json::string(std::move(description)));
    out.set("mimeType", Json::string("text/markdown"));
    return out;
}

/// Every line of a command's echo, as one text block's worth of text.
std::string join_lines(const std::vector<std::string>& lines)
{
    std::string out;
    for (const std::string& line : lines) {
        if (!out.empty()) out += '\n';
        out += line;
    }
    return out;
}

} // namespace

std::string default_instructions()
{
    // TURKISH, because the audience is a machine but the vocabulary is the
    // product's: a model that read these rules in English still has to write
    // `KATMANGÖRÜNÜM islem=gizle` (CLAUDE.md 2.6).
    return R"(KentOSCad: Türkiye odaklı CBS + CAD masaüstü programı. Bu sunucu programın komut
yüzeyini sunar. Üç şeyi bilmeden çağrı yapmayın.

1. KONUM UYDURULAMAZ. Nokta, nokta listesi ya da nesne seçimi isteyen her parametre
   yalnız bir okuma aracının döndürdüğü TUTAMAĞI kabul eder: `@0123456789abcdef` ya da
   listenin bir elemanı için `@0123456789abcdef.3`. Oraya sayı, sayı çifti ya da dizi
   yazarsanız çağrı reddedilir ve reddediliş kayda geçer. Tutamak üreten araçlar:
   `gorunum_bilgisi`, `katmanlari_listele`, `oznitelik_semasi`, `sorgula`, `secimi_al`.
   Bir tutamak alındığı çizim sürümüne bağlıdır; çizim değişirse yeniden okuyun.

2. YAZAN BİR ARAÇ ÇAĞRILDIĞINDA UYGULANMAZ. Çizimi ya da diski değiştiren bir araç
   çağrısı bir ÖNERİ kaydı açar, uygulanacak komut satırlarını döndürür ve bilgisayar
   başındaki harita mühendisi uygulayana kadar hiçbir şey değişmez. Öneri uygulanırsa
   tamamı tek bir işlemdir ve tek `Ctrl+Z` ile geri alınır. Kadastro ve imar çıktısı
   hukuki belgedir; imzayı yapay zeka atamaz. Bunu atlayan bir yol, bir başlık ya da
   bir ayar yoktur. Bir diziyi tek uygulamada toplamak için `_meta` içinde `plan`
   alanına bekleyen önerinin kimliğini verin.

3. BİRİM TAM SAYI MİLİMETREDİR. Bütün koordinatlar `int64` sabit noktalı milimetre;
   ondalık yoktur, 485320.15 metre `485320150` demektir. Alan milimetrekaredir. Eksen
   adları: `Sağa (Y)` doğuya doğru, `Yukarı (X)` kuzeye doğru — ve bir nokta DOĞU ÖNCE
   yazılır.

Hiçbir şeyi değiştirmeyen araçlar doğrudan çalışır ve sonucunu döndürür. Tam şema:
`tools/list`. Kılavuz: `kentoscad://llms.txt` kaynağı ya da `llms_txt` aracı.)";
}

McpServer::McpServer(Dispatcher& dispatcher, const command::Registry& registry, ServerInfo info,
                     ServerPolicy policy)
    : dispatcher_(dispatcher), registry_(registry), info_(std::move(info)),
      policy_(std::move(policy))
{}

std::vector<std::string> McpServer::supported_versions()
{
    // ONE, AND ONLY ONE. The legacy era is not implemented, and a list that named
    // it would promise a negotiation this server cannot carry out.
    return {Catalog::kProtocolVersion};
}

SseFrame McpServer::tools_list_changed()
{
    SseFrame frame;
    frame.event = "message";
    frame.data  = rpc_notification(kToolsListChanged, Json::null()).dump();
    return frame;
}

void McpServer::stream_closed(const StreamPlan& stream)
{
    if (!stream.plan_id.empty()) dispatcher_.withdraw(stream.plan_id);
}

McpServer::Answer McpServer::fault(const Json& id, int code, std::string message, Json data) const
{
    Answer out;
    out.payload = rpc_error(id, code, std::move(message), std::move(data));
    out.status  = status_for(code);
    return out;
}

std::string McpServer::requester_label(const HttpRequestView& http, const JsonRpcRequest& rpc) const
{
    std::string label = "MCP istemcisi";
    if (const Json* named = rpc.meta.find(kClientMetaKey);
        named != nullptr && named->is_string() && !named->as_string().empty())
        label = named->as_string();

    // THE TOKEN'S FINGERPRINT, NEVER THE TOKEN (ai.md P11). The audit record is
    // read by people and kept for years; a secret written into it is a secret
    // leaked for as long as the record exists. Eight hex digits are enough to
    // tell two clients apart and not enough to reconstruct anything.
    std::string_view presented = bearer_token(http.authorization);
    if (presented.empty() && endpoint_match(http.path, policy_) == PathMatch::Token)
        presented = policy_.token;

    if (!presented.empty()) {
        char buffer[24] = {};
        (void)std::snprintf(buffer, sizeof buffer, " #%08llx",
                            static_cast<unsigned long long>(core::fnv1a(presented) & 0xFFFFFFFFu));
        label += buffer;
    }
    return label;
}

std::optional<McpServer::Answer> McpServer::header_fault(const HttpRequestView& http,
                                                         const JsonRpcRequest& rpc) const
{
    // WHY THE HEADERS ARE CHECKED AGAINST THE BODY AT ALL. In this revision the
    // method and the name are duplicated into headers so that a proxy can route,
    // meter and authorise a call without parsing the body. A header that
    // disagrees with the body is therefore a request that two intermediaries
    // would treat as two different calls, and the only safe answer is to serve
    // neither. The specification gives the error the name `HeaderMismatch`, the
    // code -32020 and the status 400.
    const std::string_view header_version = http.protocol_version;

    if (header_version.empty())
        return fault(rpc.id, kHeaderMismatch,
                     "`MCP-Protocol-Version` başlığı yok. Bu başlık zorunludur ve gövdedeki "
                     "`_meta[\"" +
                         std::string(kProtocolVersionMetaKey) + "\"]` değeriyle aynı olmalıdır.");

    if (rpc.protocol_version.empty())
        return fault(rpc.id, kHeaderMismatch,
                     "Gövdede `_meta[\"" + std::string(kProtocolVersionMetaKey) +
                         "\"]` yok. Bu sürüm oturumsuzdur: her istek kendi sürümünü söyler.");

    if (header_version != rpc.protocol_version)
        return fault(rpc.id, kHeaderMismatch,
                     "Başlık ve gövde farklı sürüm söylüyor: `MCP-Protocol-Version: " +
                         std::string(header_version) + "`, `_meta` ise '" + rpc.protocol_version +
                         "'.");

    if (header_version != Catalog::kProtocolVersion) {
        // THE SPECIFICATION NAMES THIS ERROR BUT NOT A CODE, so no code is
        // invented in the MCP-reserved band: the name travels in `data`, where a
        // client is told to look, and the code stays the standard
        // `InvalidRequest`. Inventing -32021 would be a number this server means
        // one thing by and the next revision may mean another.
        Json versions = Json::array({});
        for (const std::string& version : supported_versions())
            versions.push(Json::string(version));

        Json data;
        data.set("error", Json::string("UnsupportedProtocolVersionError"));
        data.set("supportedVersions", std::move(versions));

        return fault(rpc.id, kInvalidRequest,
                     "Desteklenmeyen MCP sürümü: '" + std::string(header_version) +
                         "'. Bu sunucu yalnız '" + Catalog::kProtocolVersion +
                         "' sürümünü "
                         "konuşur; eski (legacy) akış hiç uygulanmadı.",
                     std::move(data));
    }

    if (http.mcp_method.empty())
        return fault(rpc.id, kHeaderMismatch,
                     "`Mcp-Method` başlığı yok. Gövdedeki `method` ile aynı olmalıdır ('" +
                         rpc.method + "').");

    if (decode_header_value(http.mcp_method) != rpc.method)
        return fault(rpc.id, kHeaderMismatch,
                     "`Mcp-Method: " + std::string(http.mcp_method) +
                         "` gövdedeki `method` ile aynı değil: '" + rpc.method + "'.");

    // `Mcp-Name` is required for exactly the three methods that name a thing.
    const bool names_a_thing =
        rpc.method == "tools/call" || rpc.method == "resources/read" || rpc.method == "prompts/get";
    if (!names_a_thing) return std::nullopt;

    const char* field    = rpc.method == "resources/read" ? "uri" : "name";
    const Json* declared = rpc.params.find(field);
    const std::string wants =
        declared != nullptr && declared->is_string() ? declared->as_string() : std::string();

    // THE BODY IS THE AUTHORITY ON WHAT IS WRONG. When the body names nothing,
    // the defect is the missing `params.name`, not the header, and the method
    // itself answers with -32602 naming the field. Reporting a header mismatch
    // here would send a client to fix the one thing that was correct.
    if (wants.empty()) return std::nullopt;

    if (http.mcp_name.empty())
        return fault(rpc.id, kHeaderMismatch,
                     "`Mcp-Name` başlığı yok. '" + rpc.method + "' için zorunludur ve `params." +
                         field + "` ile aynı olmalıdır ('" + wants + "').");

    // DECODED BEFORE COMPARING. A value that cannot be written as plain visible
    // ASCII arrives as `=?base64?<base64 of UTF-8>?=`; every tool name in this
    // program is ASCII, but a resource URI need not be, and comparing the
    // encoded form against the decoded body would refuse a conforming client.
    if (decode_header_value(http.mcp_name) != wants)
        return fault(rpc.id, kHeaderMismatch,
                     "`Mcp-Name: " + std::string(http.mcp_name) + "` gövdedeki `params." + field +
                         "` ile aynı değil: '" + wants + "'.");

    return std::nullopt;
}

McpServer::Answer McpServer::discover(const JsonRpcRequest& rpc) const
{
    Json versions = Json::array({});
    for (const std::string& version : supported_versions())
        versions.push(Json::string(version));

    Json tools;
    tools.set("listChanged", Json::boolean(true));

    Json subscriptions = Json::array({});
    subscriptions.push(Json::string(kToolsListChanged));

    Json listen;
    listen.set("methods", std::move(subscriptions));

    Json capabilities;
    capabilities.set("tools", std::move(tools));
    // NO `subscribe` AND NO `listChanged` ON RESOURCES, deliberately: both
    // resources are generated on every read, so there is nothing to subscribe to
    // and no moment at which the list changes.
    capabilities.set("resources", Json::object({}));
    capabilities.set("subscriptions", std::move(listen));

    Json server_info;
    server_info.set("name", Json::string(info_.name));
    server_info.set("version", Json::string(info_.version));

    Json meta;
    meta.set(kServerInfoMetaKey, std::move(server_info));
    // A DECIMAL STRING, not an integer: the fingerprint is a uint64 and JSON's
    // integer is signed, so half the range would arrive negative or rounded.
    meta.set("cad.kentos/fingerprint",
             Json::string(std::to_string(dispatcher_.catalog().fingerprint)));
    meta.set(kProtocolVersionMetaKey, Json::string(Catalog::kProtocolVersion));

    Json result;
    result.set("supportedVersions", std::move(versions));
    result.set("capabilities", std::move(capabilities));
    result.set("instructions", Json::string(info_.instructions.empty() ? default_instructions()
                                                                       : info_.instructions));
    // Five minutes. The discovery answer changes only when the build or the
    // command catalogue does, and the fingerprint travels with it so a client
    // that cached it longer can still tell.
    result.set("ttlMs", Json::integer(300000));
    // `server`, because this revision is stateless: every client gets the same
    // answer and there is no session a cache could be scoped to.
    result.set("cacheScope", Json::string("server"));
    result.set("_meta", std::move(meta));

    Answer out;
    out.payload = rpc_result(rpc.id, std::move(result));
    return out;
}

McpServer::Answer McpServer::tools_list(const JsonRpcRequest& rpc) const
{
    const Catalog& catalog = dispatcher_.catalog();

    // THE BUILT-IN GOES IN SORTED POSITION. `build_catalog` sorts by wire name so
    // that two runs of one build serve byte-identical bytes, and appending our
    // own tool at the end would break exactly that guarantee for the one client
    // that caches the list and compares it.
    Json merged       = Json::array({});
    bool placed       = false;
    const Json served = catalog.to_tools_list();
    for (const Json& tool : served.as_array()) {
        const Json* name = tool.find("name");
        const std::string wire =
            name != nullptr && name->is_string() ? name->as_string() : std::string();
        if (!placed && std::string(kLlmsToolName) < wire) {
            merged.push(llms_tool_entry());
            placed = true;
        }
        merged.push(tool);
    }
    if (!placed) merged.push(llms_tool_entry());

    Json meta;
    meta.set("cad.kentos/fingerprint", Json::string(std::to_string(catalog.fingerprint)));
    meta.set(kProtocolVersionMetaKey, Json::string(Catalog::kProtocolVersion));

    Json result;
    result.set("tools", std::move(merged));
    // NO `nextCursor`. This server does not paginate: the whole catalogue is a
    // few hundred kilobytes of generated text and a cursor would be a second
    // piece of state in a protocol revision that has none.
    result.set("_meta", std::move(meta));

    Answer out;
    out.payload = rpc_result(rpc.id, std::move(result));
    return out;
}

McpServer::Compiled McpServer::compile(const command::CommandSpec& spec, const Json& arguments)
{
    Compiled out;
    const std::uint64_t revision = dispatcher_.revision();

    for (const command::Param& param : spec.params) {
        const Json* given = arguments.find(param.name);
        if (given == nullptr || given->is_null()) {
            if (param.arity.min > 0) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` parametresi zorunlu ve verilmedi. Beklenen: " +
                              command::param_kind_label(param.kind) + ".";
                return out;
            }
            continue;
        }

        switch (param.kind) {
        case command::ParamKind::Point:
        case command::ParamKind::PointList:
        case command::ParamKind::Selection: {
            // THE REFUSAL THAT HAPPENS BEFORE EVERYTHING ELSE. CLAUDE.md 5.8 and
            // ai.md R9/R10: a coordinate may not originate in model text. The
            // agent-facing schema declares a handle STRING here with a `pattern`,
            // so anything else — a number, a pair of numbers, an array, a bare
            // coordinate written as text — is a non-conforming client, and the
            // rejection happens while the arguments are still JSON, before an
            // `Args` exists. That is what makes "rejected before validation"
            // literally true rather than a matter of ordering code carefully.
            // PARSED ONCE, and the parse IS the guard: two calls would let the
            // refusal and the resolution disagree about the same string.
            const std::optional<HandleRef> ref =
                given->is_string() ? HandleRef::parse(given->as_string()) : std::nullopt;
            if (!ref) {
                out.coordinate_literal = true;
                out.refusal            = "`" + param.name +
                              "` bir TUTAMAK bekler; koordinat ya da anahtar yazılamaz. Gelen: " +
                              given->dump() +
                              ". Konum her zaman bir okuma aracının sonucundan gelir: " +
                              (param.kind == command::ParamKind::Selection
                                   ? "nesneler için `secimi_al` ya da `sorgula` çağırın"
                                   : "noktalar için `sorgula`, `secimi_al` ya da `gorunum_bilgisi` "
                                     "çağırın") +
                              " ve dönen `@0123456789abcdef` biçimindeki tutamağı buraya yazın.";
                return out;
            }

            core::Result<HandleValue> resolved = dispatcher_.handles().resolve(*ref, revision);
            if (!resolved) {
                // A DOMAIN REFUSAL, not a protocol one: the reference was
                // well-formed and the drawing moved on under it. A client fixes
                // this by reading again, which is a different action from fixing
                // a malformed request.
                out.refusal = resolved.error().message;
                return out;
            }

            const HandleValue& value = resolved.value();
            out.handles.push_back(value.id);

            if (param.kind == command::ParamKind::Selection) {
                if (value.kind != HandleKind::Entities) {
                    out.refusal = "`" + param.name + "` nesne tutamağı bekler; '" + value.id +
                                  "' nesne taşımıyor. `secimi_al` ya da `sorgula` çağırın.";
                    return out;
                }
                out.args.set(param.name, command::Value::ids(value.entities));
                break;
            }

            if (value.kind != HandleKind::Points) {
                out.refusal = "`" + param.name + "` nokta tutamağı bekler; '" + value.id +
                              "' nokta taşımıyor. Pencere tutamağı bir dikdörtgendir, "
                              "köşe listesi değildir.";
                return out;
            }

            if (param.kind == command::ParamKind::Point) {
                if (value.points.size() != 1) {
                    out.refusal = "`" + param.name + "` tek nokta bekler; '" + value.id + "' " +
                                  std::to_string(value.points.size()) +
                                  " nokta taşıyor. Birini seçmek için `" + value.id +
                                  ".0` biçiminde yazın.";
                    return out;
                }
                out.args.set(param.name, command::Value::point(value.points.front()));
            } else {
                out.args.set(param.name, command::Value::points(value.points));
            }
            break;
        }

        case command::ParamKind::Number:
            if (!given->is_number()) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` sayı bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::number(given->as_double()));
            break;

        case command::ParamKind::Integer:
            // AN INTEGER PARAMETER WHOSE ARITY ALLOWS SEVERAL IS A LIST, and the
            // published schema says `array` for exactly those (catalog.cpp). The
            // two have to agree or a model writes what the bus then refuses.
            if (param.arity.max > 1) {
                if (!given->is_array()) {
                    out.protocol_fault = true;
                    out.refusal        = "`" + param.name +
                                  "` tam sayı dizisi bekler; gelen: " + given->dump() + ".";
                    return out;
                }
                command::Value::Ints ids;
                for (const Json& element : given->as_array()) {
                    if (!element.is_int()) {
                        out.protocol_fault = true;
                        out.refusal        = "`" + param.name +
                                      "` yalnız tam sayı taşır; gelen: " + element.dump() + ".";
                        return out;
                    }
                    ids.push_back(element.as_int());
                }
                out.args.set(param.name, command::Value::ids(std::move(ids)));
            } else {
                if (!given->is_int()) {
                    out.protocol_fault = true;
                    out.refusal =
                        "`" + param.name + "` tam sayı bekler; gelen: " + given->dump() + ".";
                    return out;
                }
                out.args.set(param.name, command::Value::integer(given->as_int()));
            }
            break;

        case command::ParamKind::Text:
            if (!given->is_string()) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` metin bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::text(given->as_string()));
            break;

        case command::ParamKind::Bool:
            if (!given->is_bool()) {
                out.protocol_fault = true;
                out.refusal =
                    "`" + param.name + "` evet/hayır bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::boolean(given->as_bool()));
            break;
        }
    }

    // AN UNDECLARED ARGUMENT IS REFUSED HERE, with its name. The bus refuses one
    // too (command.md P15) and the published schema closes the object
    // (`additionalProperties: false`), but a refusal from three layers down does
    // not say which field was wrong, and a client cannot fix what it cannot see.
    for (const auto& [key, value] : arguments.as_object()) {
        (void)value;
        if (key == "_meta") continue;
        bool declared = false;
        for (const command::Param& param : spec.params)
            if (param.name == key) declared = true;
        if (!declared) {
            out.protocol_fault = true;
            out.refusal        = "Bilinmeyen parametre: `" + key +
                          "`. Bu araç yalnız şemasında "
                          "yazan parametreleri kabul eder.";
            return out;
        }
    }
    return out;
}

McpServer::Answer McpServer::tools_call(const JsonRpcRequest& rpc, std::string requester)
{
    Answer out;
    out.audit.method    = rpc.method;
    out.audit.requester = std::move(requester);

    const Json* named = rpc.params.find("name");
    if (named == nullptr || !named->is_string() || named->as_string().empty()) {
        Answer bad = fault(rpc.id, kInvalidParams,
                           "`params.name` boş olmayan bir dize olmalı: hangi araç "
                           "çağrılıyor? Araç listesi için `tools/list`.");
        bad.audit  = out.audit;
        return bad;
    }

    const std::string tool_name = named->as_string();
    out.audit.tool              = tool_name;

    // Attaches this call's audit facts to an answer built by `fault`.
    const auto with_audit = [&out](Answer answer) {
        answer.audit = out.audit;
        return answer;
    };

    Json arguments = Json::object({});
    if (const Json* given = rpc.params.find("arguments"); given != nullptr && !given->is_null()) {
        if (!given->is_object())
            return with_audit(
                fault(rpc.id, kInvalidParams,
                      "`params.arguments` bir nesne olmalı; gelen: " + given->dump() + "."));
        arguments = *given;
    }

    // THE PROTOCOL LAYER'S OWN TOOL, handled before the catalogue is consulted.
    // It describes the tool surface, so it cannot be one of the commands the
    // surface is generated from (see `kLlmsToolName` in the header).
    if (tool_name == kLlmsToolName) {
        Json meta;
        meta.set("cad.kentos/builtin", Json::boolean(true));
        meta.set("cad.kentos/fingerprint",
                 Json::string(std::to_string(dispatcher_.catalog().fingerprint)));
        out.payload = rpc_result(
            rpc.id, call_result(llms_txt(registry_), Json::null(), false, std::move(meta)));
        return out;
    }

    const Catalog& catalog = dispatcher_.catalog();
    const ToolDef* tool    = catalog.find(tool_name);
    if (tool == nullptr)
        return with_audit(fault(rpc.id, kInvalidParams,
                                "Bilinmeyen araç: '" + tool_name + "'. Geçerli adlardan birkaçı: " +
                                    sample_names(catalog, 4) + ". Tam liste: `tools/list`."));

    const command::CommandSpec* spec = registry_.by_id(tool->command_id);
    if (spec == nullptr)
        return with_audit(fault(rpc.id, kInternalError,
                                "Araç '" + tool_name + "' kayıtlı olmayan bir komuta bakıyor: '" +
                                    tool->command_id + "'."));

    Compiled compiled = compile(*spec, arguments);

    if (compiled.coordinate_literal) {
        // ANSWERED AS A RESULT AND REPORTED FOR THE AUDIT. The client gets
        // `isError` and a sentence naming the read tool that mints a handle; the
        // application gets `coordinate_refusal` and writes the record ai.md R10
        // requires for a rejection.
        out.audit.coordinate_refusal = true;
        out.audit.detail             = compiled.refusal;
        out.payload =
            rpc_result(rpc.id, call_result(compiled.refusal, Json::null(), true, Json::null()));
        return out;
    }

    if (compiled.protocol_fault) return with_audit(fault(rpc.id, kInvalidParams, compiled.refusal));

    if (!compiled.refusal.empty()) {
        out.audit.detail = compiled.refusal;
        out.payload =
            rpc_result(rpc.id, call_result(compiled.refusal, Json::null(), true, Json::null()));
        return out;
    }

    // ---------------------------------------------------------------- reading --
    if (!tool->mutates) {
        core::Result<ToolOutcome> ran = dispatcher_.run_read_only(tool->command_id, compiled.args);
        if (!ran) {
            // A DOMAIN ERROR: an unknown layer, an empty selection, a refusal by
            // validation. A result with `isError`, never a JSON-RPC error, so a
            // client knows to retry with different arguments rather than to stop.
            out.audit.detail = ran.error().message;
            out.payload      = rpc_result(
                rpc.id, call_result(ran.error().message, Json::null(), true, Json::null()));
            return out;
        }

        const ToolOutcome& outcome = ran.value();

        Json handles = Json::array({});
        for (const std::string& id : outcome.minted) {
            const HandleValue* value = dispatcher_.handles().find(id);
            handles.push(value != nullptr ? HandleStore::describe(*value) : Json::string(id));
        }

        Json structured;
        bool have_structured = false;
        if (outcome.report.is_object()) {
            structured      = outcome.report;
            have_structured = true;
        } else if (!outcome.report.is_null()) {
            // `Json::set` would turn a non-object into an object and throw the
            // value away, so a scalar report is wrapped rather than assigned.
            structured.set("sonuc", outcome.report);
            have_structured = true;
        }
        if (!outcome.minted.empty()) {
            structured.set("tutamaklar", handles);
            have_structured = true;
        }

        std::string text = join_lines(outcome.lines);
        if (text.empty()) text = "Komut çalıştı ve yazılı bir satır döndürmedi.";
        if (!outcome.minted.empty()) {
            // NAMED IN THE TEXT AS WELL AS IN THE STRUCTURE. A client that reads
            // only the text block would otherwise never learn which handles it
            // may now refer to, and a handle it cannot see is a write it cannot
            // compose.
            text += "\nYeni tutamaklar:";
            for (const std::string& id : outcome.minted)
                text += " " + id;
        }

        Json meta;
        meta.set("cad.kentos/commandId",
                 Json::string(outcome.command_id.empty() ? tool->command_id : outcome.command_id));
        out.payload = rpc_result(rpc.id, call_result(std::move(text),
                                                     have_structured ? structured : Json::null(),
                                                     false, std::move(meta)));
        return out;
    }

    // ---------------------------------------------------------------- writing --
    // NOTHING IS RUN. CLAUDE.md 5.7 and ai.md R3/P1: a write becomes a suggestion
    // and waits for a person, and no token, header or setting can stand in for
    // that person. The client is answered immediately with the plan's id, its
    // state and the exact command lines it holds.
    PlanStep step;
    step.command_id = tool->command_id;
    step.args       = compiled.args;
    step.line       = render_line(*spec, compiled.args);
    step.handles    = compiled.handles;

    // A CLIENT MAY COMPOSE A SEQUENCE ONE APPROVAL APPLIES (ai.md R4), by naming
    // a pending plan in `_meta`. Two spellings are read; see `kPlanMetaKey`.
    std::string target;
    if (const Json* asked = rpc.meta.find("plan"); asked != nullptr && asked->is_string())
        target = asked->as_string();
    if (target.empty())
        if (const Json* asked = rpc.meta.find(kPlanMetaKey); asked != nullptr && asked->is_string())
            target = asked->as_string();

    Plan plan;
    // THE CONTRACT THIS RELIES ON, stated where it is used: `Dispatcher::propose`
    // with a non-empty `Plan::id` naming a PENDING plan APPENDS the steps to that
    // plan and returns the same id; with an empty id it files a new plan.
    // `PlanStore::append` is the operation behind it. The returned id is checked
    // against the requested one below, so an implementation that ignored the
    // field produces a visible refusal rather than a silent second suggestion.
    plan.id        = target;
    plan.requester = out.audit.requester;
    // The client's own words for what it asked, which is the tool call itself:
    // there is no prompt in an MCP exchange. Small by construction — it is the
    // arguments, never the drawing (ai.md P9).
    plan.prompt   = "tools/call " + tool_name + " " + arguments.dump();
    plan.endpoint = "mcp " + std::string(Catalog::kProtocolVersion) + " " + policy_.path;
    // `model` stays empty on purpose: an MCP client does not tell us which model
    // is driving it, and a guess written into a legal record is a false fact.
    plan.revision = dispatcher_.revision();
    plan.steps.push_back(std::move(step));

    core::Result<std::string> filed = dispatcher_.propose(std::move(plan));
    if (!filed) {
        out.audit.detail = filed.error().message;
        out.payload      = rpc_result(
            rpc.id, call_result(filed.error().message, Json::null(), true, Json::null()));
        return out;
    }

    const std::string plan_id = filed.value();
    if (!target.empty() && plan_id != target) {
        out.audit.detail = "Ekleme yapılamadı.";
        out.payload =
            rpc_result(rpc.id, call_result("Öneri '" + target +
                                               "' genişletilemedi; bu yapı öneriye adım eklemiyor. "
                                               "Yeni bir öneri açmak için `_meta` içindeki `plan` "
                                               "alanını boş bırakın.",
                                           Json::null(), true, Json::null()));
        return out;
    }

    out.audit.plan_id = plan_id;
    out.plan_id       = plan_id;

    core::Result<Plan> state = dispatcher_.plan_state(plan_id);
    std::vector<std::string> lines;
    if (state) {
        for (const PlanStep& held : state.value().steps)
            lines.push_back(held.line);
    } else {
        lines.push_back(render_line(*spec, compiled.args));
    }

    std::string text =
        std::string("Öneri ") + (target.empty() ? "kaydı açıldı" : "genişletildi") + ": " +
        plan_id + " (durum: " + plan_state_name(state ? state.value().state : PlanState::Pending) +
        ").";
    text += "\nUygulanacak komut satırları:";
    for (const std::string& line : lines)
        text += "\n  " + line;
    text += "\nÇizim değişmedi. Bu satırları bilgisayar başındaki harita mühendisi uygulayana "
            "kadar hiçbir şey uygulanmaz; uygulanırsa tamamı tek bir işlem ve tek `Ctrl+Z` olur.";

    Json structured = state ? state.value().to_json() : Json::object({});
    if (!state) {
        structured.set("oneri", Json::string(plan_id));
        structured.set("durum", Json::string(plan_state_name(PlanState::Pending)));
    }
    if (!compiled.handles.empty()) {
        Json used = Json::array({});
        for (const std::string& id : compiled.handles)
            used.push(Json::string(id));
        structured.set("tutamaklar", std::move(used));
    }

    Json meta;
    meta.set("cad.kentos/commandId", Json::string(tool->command_id));
    meta.set(kPlanMetaKey, Json::string(plan_id));
    meta.set("cad.kentos/approval", Json::string("user-required"));

    out.before.push_back(message_frame(
        "info",
        "Öneri " + plan_id + " açıldı ve uygulanmadı; karar bilgisayar başındaki mühendise ait."));
    out.payload = rpc_result(
        rpc.id, call_result(std::move(text), std::move(structured), false, std::move(meta)));
    return out;
}

McpServer::Answer McpServer::resources_list(const JsonRpcRequest& rpc) const
{
    Json resources = Json::array({});
    // BOTH ARE GENERATED ON EVERY READ, never a checked-in copy: a document that
    // describes the command surface and is stored beside it is a document that
    // will disagree with it (CLAUDE.md 5.10, ai.md P7).
    resources.push(resource_entry(kLlmsUri, "llms.txt", "Kullanım kılavuzu (özet)",
                                  "Birimler, eksen adları, tutamak kuralı, uygulama kuralı ve "
                                  "araçların kullanım sırası. Komut kataloğundan üretilir."));
    resources.push(resource_entry(kLlmsFullUri, "llms-full.txt", "Kullanım kılavuzu (tam)",
                                  "Aynı özet, ardından her aracın adı, açıklaması ve "
                                  "parametre tablosu. Komut kataloğundan üretilir."));

    Json result;
    result.set("resources", std::move(resources));

    Answer out;
    out.payload = rpc_result(rpc.id, std::move(result));
    return out;
}

McpServer::Answer McpServer::resources_read(const JsonRpcRequest& rpc) const
{
    const Json* asked = rpc.params.find("uri");
    if (asked == nullptr || !asked->is_string())
        return fault(rpc.id, kInvalidParams,
                     "`params.uri` bir dize olmalı. Bu sunucu iki kaynak sunar: `" +
                         std::string(kLlmsUri) + "` ve `" + kLlmsFullUri + "`.");

    const std::string uri = asked->as_string();
    std::string text;
    if (uri == kLlmsUri)
        text = llms_txt(registry_);
    else if (uri == kLlmsFullUri)
        text = llms_full_txt(registry_);
    else
        return fault(rpc.id, kInvalidParams,
                     "Bilinmeyen kaynak: '" + uri + "'. Bu sunucu iki kaynak sunar: `" +
                         std::string(kLlmsUri) + "` ve `" + kLlmsFullUri + "`.");

    Json entry;
    entry.set("uri", Json::string(uri));
    entry.set("mimeType", Json::string("text/markdown"));
    entry.set("text", Json::string(std::move(text)));

    Json contents = Json::array({});
    contents.push(std::move(entry));

    Json result;
    result.set("contents", std::move(contents));

    Answer out;
    out.payload = rpc_result(rpc.id, std::move(result));
    return out;
}

McpServer::Answer McpServer::subscriptions_listen(const JsonRpcRequest& rpc) const
{
    std::vector<std::string> asked;
    if (const Json* list = rpc.params.find("subscriptions"); list != nullptr && list->is_array())
        for (const Json& element : list->as_array())
            if (element.is_string()) asked.push_back(element.as_string());

    // Asking for nothing in particular means asking for what there is.
    if (asked.empty()) asked.emplace_back(kToolsListChanged);

    std::vector<std::string> served;
    for (const std::string& method : asked)
        if (method == kToolsListChanged) served.push_back(method);

    if (served.empty())
        return fault(rpc.id, kInvalidParams,
                     "Bu sunucu yalnız `" + std::string(kToolsListChanged) +
                         "` bildirimini yayınlar. Belge değişikliklerini izlemek için "
                         "okuma araçlarını yeniden çağırın.");

    Answer out;
    // NO FINAL RESPONSE, and that is what makes this stream long-lived: the final
    // response SHOULD terminate its own stream, so a subscription that answered
    // would close the moment it opened. It stays open until the client closes it,
    // which is also how a cancellation is spelled in this revision.
    out.payload       = Json::null();
    out.keep_open     = true;
    out.subscriptions = served;
    out.before.push_back(message_frame("info", "Abonelik açık: araç yüzeyi değişirse `" +
                                                   std::string(kToolsListChanged) +
                                                   "` bu akışta gelir."));
    return out;
}

HttpOutcome McpServer::handle(const HttpRequestView& request)
{
    // ---- 1. THE VERB. One path, POST only. A GET is not a client asking for the
    // long-lived stream this revision removed; it is a legacy client, and the
    // specification's own compatibility table says a legacy client fails. A
    // DELETE would be terminating a session, and there are no sessions.
    if (request.method != "POST") {
        HttpOutcome out = http_empty(405);
        out.headers.emplace_back("Allow", "POST");
        return out;
    }

    // ---- 2. THE ORIGIN, before anything in the request is read. It is a
    // security boundary, not a validation step: see `origin_allowed` for the DNS
    // rebinding attack it exists to stop. Checked ahead of the path so that a
    // rebinding probe cannot learn which paths this server answers.
    if (!origin_allowed(request.origin, policy_)) return http_empty(403);

    // ---- 3. THE PATH AND THE TOKEN.
    const PathMatch matched = endpoint_match(request.path, policy_);
    if (matched == PathMatch::None) return http_empty(404);

    const auto unauthorised = []() {
        HttpOutcome out = http_empty(401);
        out.headers.emplace_back("WWW-Authenticate", "Bearer realm=\"KentOSCad\"");
        return out;
    };

    // A WRONG TOKEN IN THE PATH IS REFUSED WHATEVER ELSE ARRIVED. A client that
    // asserted a token and got it wrong is a client that believes something
    // false about this server, and serving it anyway because a second credential
    // happened to be valid would hide that.
    if (matched == PathMatch::BadToken) return unauthorised();

    if (policy_.require_token) {
        const std::string_view bearer = bearer_token(request.authorization);
        const bool authorised         = !policy_.token.empty() &&
                                (matched == PathMatch::Token ||
                                 (!bearer.empty() && constant_time_equals(bearer, policy_.token)));
        if (!authorised) return unauthorised();
    }

    // `Mcp-Session-Id` and `Last-Event-ID` are read into the request view and
    // deliberately never used: this revision has no sessions and no resumable
    // stream, and minting or echoing either would invite a client to depend on
    // state that does not exist.
    (void)request.session_id;
    (void)request.last_event_id;

    // ---- 4. THE BODY.
    core::Result<Json> body = core::Json::parse(request.body);
    if (!body)
        return http_json(status_for(kParseError),
                         rpc_error(Json::null(), kParseError,
                                   "Gövde JSON olarak okunamadı: " + body.error().message));

    core::Result<JsonRpcRequest> parsed = parse_rpc(body.value());
    if (!parsed)
        return http_json(status_for(kInvalidRequest),
                         rpc_error(Json::null(), kInvalidRequest, parsed.error().message));

    const JsonRpcRequest& rpc = parsed.value();

    // ---- 5. A NOTIFICATION IS ACKNOWLEDGED AND NOT ANSWERED.
    //
    // HEADERS ARE VALIDATED FOR REQUESTS. A notification has no id, so a
    // JSON-RPC error would have no addressee, and the specification's answer to
    // one is 202 with no body whatever it contained. The single check kept is
    // the cheap one: a `Mcp-Method` that disagrees with the body is a broken
    // client, and a 400 tells it so instead of a 202 that means nothing.
    if (rpc.is_notification) {
        if (!request.mcp_method.empty() && decode_header_value(request.mcp_method) != rpc.method)
            return http_json(status_for(kHeaderMismatch),
                             rpc_error(Json::null(), kHeaderMismatch,
                                       "`Mcp-Method: " + std::string(request.mcp_method) +
                                           "` gövdedeki `method` ile aynı değil: '" + rpc.method +
                                           "'."));
        return http_accepted();
    }

    // ---- 6. THE HEADERS, against the body.
    const std::string requester = requester_label(request, rpc);

    Answer answer;
    if (std::optional<Answer> refused = header_fault(request, rpc); refused) {
        answer = std::move(*refused);
    } else if (rpc.method == "server/discover") {
        answer = discover(rpc);
    } else if (rpc.method == "tools/list") {
        answer = tools_list(rpc);
    } else if (rpc.method == "tools/call") {
        answer = tools_call(rpc, requester);
    } else if (rpc.method == "resources/list") {
        answer = resources_list(rpc);
    } else if (rpc.method == "resources/read") {
        answer = resources_read(rpc);
    } else if (rpc.method == "subscriptions/listen") {
        answer = subscriptions_listen(rpc);
    } else {
        // NOT IMPLEMENTED IS NOT THE SAME AS NOT EXISTING, and the answer is the
        // same either way: `initialize`, `ping`, `logging/setLevel`,
        // `sampling/createMessage`, `roots/list` and `resources/subscribe` are
        // all methods this server deliberately does not serve.
        answer = fault(rpc.id, kMethodNotFound,
                       "Bilinmeyen yöntem: '" + rpc.method +
                           "'. Bu sunucu şunları karşılar: `server/discover`, `tools/list`, "
                           "`tools/call`, `resources/list`, `resources/read`, "
                           "`subscriptions/listen`.");
    }

    if (answer.audit.method.empty()) answer.audit.method = rpc.method;
    if (answer.audit.requester.empty()) answer.audit.requester = requester;

    // ---- 7. FRAMING. A held-open subscription is always a stream. Any other
    // answer becomes one when the client said it accepts `text/event-stream`,
    // and then the notifications go out first and the response last — the final
    // response SHOULD terminate the stream, and closing after it is what tells
    // the client the exchange is over rather than merely quiet.
    const bool wants_stream = accepts_event_stream(request.accept);
    HttpOutcome out;

    if (answer.keep_open || (wants_stream && answer.status == 200)) {
        StreamPlan plan;
        plan.initial       = std::move(answer.before);
        plan.keep_open     = answer.keep_open;
        plan.keep_alive_ms = answer.keep_open ? policy_.keep_alive_ms : 0;
        plan.subscriptions = std::move(answer.subscriptions);
        plan.plan_id       = answer.plan_id;

        if (!answer.payload.is_null()) {
            SseFrame final_frame;
            final_frame.event = "message";
            final_frame.data  = answer.payload.dump();
            final_frame.last  = true;
            plan.initial.push_back(std::move(final_frame));
        }
        out = http_stream(std::move(plan));
    } else {
        out = http_json(answer.status, answer.payload);
    }

    // Echoed on every answer so that a proxy and a client can both see which
    // revision produced it without parsing the body.
    out.headers.emplace_back("MCP-Protocol-Version", Catalog::kProtocolVersion);
    out.audit = std::move(answer.audit);
    return out;
}

} // namespace kentos::ai
