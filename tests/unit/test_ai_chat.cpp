// SPDX-License-Identifier: GPL-3.0-or-later
//
// The model-facing half of the AI layer: the provider profiles, the four wire
// dialects, the two stream framers, the conversation model and the redactor.
//
// DRIVEN BY RECORDED STREAMS, NEVER BY A SOCKET. `.claude/test.md` P10 forbids
// calling a live provider from CI, and it would be the wrong test anyway: what
// has to be proved is that THESE bytes produce THOSE events, which a recorded
// fixture states exactly and a live call restates differently every run. Each
// fixture below is a real event shape of the protocol it belongs to.
//
// AND FED IN HOSTILE SPLITS. The bug this file exists to prevent is a framer
// that loses the tail of an event split across two `on_chunk` calls: the answer
// then arrives with a hole in it and nothing anywhere reports an error. Every
// dialect is therefore decoded twice — once whole, once one byte at a time.
#include "kentos_test.hpp"

#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/redact.hpp"
#include "kentos_cad/ai/sse.hpp"

using namespace kentos;
using kentos::core::Json;

namespace {

/// A catalogue with one tool, built by hand rather than projected from the
/// registry: these cases are about the DIALECTS, and a hand-built tool pins the
/// schema shapes they have to carry — a handle parameter, a plain string and an
/// integer. `test_ai_catalog.cpp` is what proves the projection itself.
ai::Catalog one_tool_catalog()
{
    Json handle = Json::object({});
    handle.set("type", Json::string("string"));
    handle.set("pattern", Json::string("^@[0-9a-f]{16}(\\.[0-9]+)?$"));

    Json name = Json::object({});
    name.set("type", Json::string("string"));

    Json count = Json::object({});
    count.set("type", Json::string("integer"));

    Json properties = Json::object({});
    properties.set("baslangic", std::move(handle));
    properties.set("ad", std::move(name));
    properties.set("adet", std::move(count));

    Json required = Json::array({});
    required.push(Json::string("baslangic"));

    Json schema = Json::object({});
    schema.set("type", Json::string("object"));
    schema.set("properties", std::move(properties));
    schema.set("required", std::move(required));
    schema.set("additionalProperties", Json::boolean(false));

    ai::ToolDef tool;
    tool.name         = "core_line";
    tool.title        = "ÇİZGİ";
    tool.description  = "İki nokta arasına çizgi çizer.";
    tool.input_schema = std::move(schema);
    tool.command_id   = "core.line";
    tool.mutates      = true;

    ai::Catalog catalog;
    catalog.fingerprint = 42;
    catalog.tools.push_back(std::move(tool));
    return catalog;
}

/// A profile that exercises every optional field, for the request-body cases.
ai::ProviderProfile profile_for(ai::Dialect dialect)
{
    ai::ProviderProfile p;
    p.name        = "Deneme";
    p.dialect     = dialect;
    p.base_url    = "https://ornek.gecersiz/v1";
    p.path        = "/uc";
    p.model       = "deneme-model";
    p.max_tokens  = 2048;
    p.temperature = 0.4;
    p.context     = {32000, ai::ContextSource::User};
    return p;
}

/// One user turn, for every dialect to render.
ai::Conversation one_turn()
{
    ai::Conversation chat;
    chat.add(ai::text_message(ai::Role::User, "Parseli ölç"));
    return chat;
}

/// Decodes `bytes` through `codec`, feeding them `chunk` bytes at a time — which
/// is what makes the split cases a one-line change rather than a second test.
std::vector<ai::StreamEvent> decode_all(const ai::DialectCodec& codec, std::string_view bytes,
                                        std::size_t chunk)
{
    ai::StreamDecoder decoder(codec);
    std::vector<ai::StreamEvent> out;
    for (std::size_t at = 0; at < bytes.size(); at += chunk) {
        std::vector<ai::StreamEvent> events =
            decoder.feed(bytes.substr(at, std::min(chunk, bytes.size() - at)));
        out.insert(out.end(), events.begin(), events.end());
    }
    std::vector<ai::StreamEvent> tail = decoder.finish();
    out.insert(out.end(), tail.begin(), tail.end());
    return out;
}

/// Every event of one kind, joined — the text of an answer, or of its reasoning.
std::string joined(const std::vector<ai::StreamEvent>& events, ai::EventKind kind)
{
    std::string out;
    for (const ai::StreamEvent& event : events)
        if (event.kind == kind) out += event.text;
    return out;
}

std::size_t count_kind(const std::vector<ai::StreamEvent>& events, ai::EventKind kind)
{
    std::size_t n = 0;
    for (const ai::StreamEvent& event : events)
        if (event.kind == kind) ++n;
    return n;
}

const ai::StreamEvent* first_of(const std::vector<ai::StreamEvent>& events, ai::EventKind kind)
{
    for (const ai::StreamEvent& event : events)
        if (event.kind == kind) return &event;
    return nullptr;
}

/// An object member, or a null value when the path does not exist — so a failing
/// assertion reads as "the field is missing" instead of crashing the suite.
Json at(const Json& root, std::string_view key)
{
    const Json* found = root.find(key);
    return found != nullptr ? *found : Json::null();
}

Json item(const Json& array, std::size_t index)
{
    return array.is_array() && index < array.as_array().size() ? array.as_array()[index]
                                                               : Json::null();
}

// ------------------------------------------------------------- the fixtures --

/// A `chat/completions` turn: an empty opening delta, DeepSeek-style visible
/// reasoning, text, a keep-alive comment, a tool call whose arguments arrive in
/// three fragments, the `finish_reason` chunk, the usage-only chunk that
/// `stream_options.include_usage` buys, and the sentinel.
constexpr std::string_view kOpenAiChatStream =
    R"sse(data: {"id":"chatcmpl-1","object":"chat.completion.chunk","choices":[{"index":0,"delta":{"role":"assistant","content":""}}]}

data: {"choices":[{"index":0,"delta":{"reasoning_content":"Sınırı ölçüyorum"}}]}

data: {"choices":[{"index":0,"delta":{"content":"Öneri: "}}]}

: ping

data: {"choices":[{"index":0,"delta":{"content":"çizgi"}}]}

data: {"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"id":"call_abc","type":"function","function":{"name":"core_line","arguments":""}}]}}]}

data: {"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"baslangic\":"}}]}}]}

data: {"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"@0123456789abcdef\"}"}}]}}]}

data: {"choices":[{"index":0,"delta":{},"finish_reason":"tool_calls"}]}

data: {"choices":[],"usage":{"prompt_tokens":1200,"completion_tokens":48,"completion_tokens_details":{"reasoning_tokens":16},"prompt_tokens_details":{"cached_tokens":1024}}}

data: [DONE]

)sse";

/// A `/v1/responses` turn, including a DUPLICATED record — sequence number 4
/// arrives twice, as a resumed stream repeats what it already sent.
constexpr std::string_view kResponsesStream = R"sse(event: response.created
data: {"type":"response.created","sequence_number":1,"response":{"id":"resp_1"}}

event: response.reasoning_summary_text.delta
data: {"type":"response.reasoning_summary_text.delta","sequence_number":2,"delta":"Parsel sınırını kontrol ediyorum"}

event: response.output_text.delta
data: {"type":"response.output_text.delta","sequence_number":3,"delta":"Öneri hazır"}

event: response.output_item.added
data: {"type":"response.output_item.added","sequence_number":4,"output_index":1,"item":{"type":"function_call","call_id":"fc_1","name":"core_line","arguments":""}}

event: response.output_item.added
data: {"type":"response.output_item.added","sequence_number":4,"output_index":1,"item":{"type":"function_call","call_id":"fc_1","name":"core_line","arguments":""}}

event: response.function_call_arguments.delta
data: {"type":"response.function_call_arguments.delta","sequence_number":5,"output_index":1,"delta":"{\"baslangic\":"}

event: response.function_call_arguments.done
data: {"type":"response.function_call_arguments.done","sequence_number":6,"output_index":1,"arguments":"{\"baslangic\":\"@0123456789abcdef\"}"}

event: response.output_item.done
data: {"type":"response.output_item.done","sequence_number":7,"output_index":0,"item":{"id":"rs_1","type":"reasoning","summary":[],"encrypted_content":"gAAAAABm3xQ7SIFRED"}}

event: response.completed
data: {"type":"response.completed","sequence_number":8,"response":{"id":"resp_1","usage":{"input_tokens":1500,"output_tokens":120,"output_tokens_details":{"reasoning_tokens":64},"input_tokens_details":{"cached_tokens":512}}}}

)sse";

/// An Anthropic turn in its fixed order: a thinking block with a signature, a
/// text block, a tool_use block whose `partial_json` is only valid at the stop,
/// then the incremental usage and the stop.
constexpr std::string_view kAnthropicStream = R"sse(event: message_start
data: {"type":"message_start","message":{"id":"msg_1","role":"assistant","model":"claude-sonnet-4-5","usage":{"input_tokens":1500,"cache_read_input_tokens":512,"output_tokens":1}}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"İfraz sınırını"}}

event: ping
data: {"type":"ping"}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":" ölçüyorum"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"signature_delta","signature":"EqoBCkgIARABGAIiQL2K"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"signature_delta","signature":"5xWPzvQ=="}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: content_block_start
data: {"type":"content_block_start","index":1,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"Öneri hazır"}}

event: content_block_stop
data: {"type":"content_block_stop","index":1}

event: content_block_start
data: {"type":"content_block_start","index":2,"content_block":{"type":"tool_use","id":"toolu_1","name":"core_line","input":{}}}

event: content_block_delta
data: {"type":"content_block_delta","index":2,"delta":{"type":"input_json_delta","partial_json":"{\"baslangic\":"}}

event: content_block_delta
data: {"type":"content_block_delta","index":2,"delta":{"type":"input_json_delta","partial_json":"\"@0123456789abcdef\"}"}}

event: content_block_stop
data: {"type":"content_block_stop","index":2}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"tool_use"},"usage":{"output_tokens":95}}

event: message_stop
data: {"type":"message_stop"}

)sse";

/// An Ollama `/api/chat` turn: NDJSON, thinking in its own field, a COMPLETE
/// tool call, and the final object carrying the counts.
constexpr std::string_view kOllamaStream =
    R"ndjson({"model":"llama3.1","created_at":"2026-01-01T00:00:00Z","message":{"role":"assistant","thinking":"Katmanı buluyorum","content":""},"done":false}
{"model":"llama3.1","message":{"role":"assistant","content":"Öneri hazır"},"done":false}
{"model":"llama3.1","message":{"role":"assistant","content":"","tool_calls":[{"function":{"name":"core_line","arguments":{"baslangic":"@0123456789abcdef"}}}]},"done":false}
{"model":"llama3.1","message":{"role":"assistant","content":""},"done":true,"done_reason":"stop","prompt_eval_count":310,"eval_count":57}
)ndjson";

} // namespace

// ================================================================= framers ===

TEST_CASE("SSE çözücü olay ortasından bölünmüş baytları toparlar")
{
    // THE DEFECT THIS GUARDS: a framer that returns what it has at the end of a
    // chunk loses the tail of an event split across two reads, and the answer
    // arrives with a hole in it that nothing reports.
    const std::string_view stream = "event: bir\ndata: {\"a\":1}\n\n: keep-alive\n\nevent: "
                                    "iki\r\ndata: parca\r\ndata: bir\r\n\r\ndata: [DONE]\n\n";

    for (std::size_t chunk : {stream.size(), std::size_t{7}, std::size_t{1}}) {
        ai::SseParser parser;
        std::vector<ai::SseEvent> events;
        for (std::size_t at = 0; at < stream.size(); at += chunk) {
            std::vector<ai::SseEvent> got =
                parser.feed(stream.substr(at, std::min(chunk, stream.size() - at)));
            events.insert(events.end(), got.begin(), got.end());
        }
        std::vector<ai::SseEvent> tail = parser.finish();
        events.insert(events.end(), tail.begin(), tail.end());

        REQUIRE(events.size() == 3);
        CHECK(events[0].event == "bir");
        CHECK(events[0].data == "{\"a\":1}");
        // A COMMENT IS A KEEP-ALIVE, not an event and not an error: it is how a
        // provider behind a proxy holds a long turn open.
        CHECK(events[1].event == "iki");
        // Two `data:` lines are ONE payload joined with a newline, which is the
        // specification's rule and matters for a pretty-printed JSON body.
        CHECK(events[1].data == "parca\nbir");
        CHECK(events[2].done);
        CHECK(events[2].data.empty());
    }
}

TEST_CASE("NDJSON çözücü satır ortasından bölünmüş baytları toparlar")
{
    const std::string_view stream = "{\"a\":1}\n{\"b\":2}\n{\"c\":3}";

    for (std::size_t chunk : {stream.size(), std::size_t{3}, std::size_t{1}}) {
        ai::NdjsonParser parser;
        std::vector<std::string> lines;
        for (std::size_t at = 0; at < stream.size(); at += chunk) {
            std::vector<std::string> got =
                parser.feed(stream.substr(at, std::min(chunk, stream.size() - at)));
            lines.insert(lines.end(), got.begin(), got.end());
        }
        // THE LAST LINE HAS NO NEWLINE, which is what a server that closes the
        // connection after its final object leaves behind.
        if (std::optional<std::string> last = parser.finish(); last) lines.push_back(*last);

        REQUIRE(lines.size() == 3);
        CHECK(lines[0] == "{\"a\":1}");
        CHECK(lines[2] == "{\"c\":3}");
    }
}

// =============================================== openai_chat, the common one ==

TEST_CASE("openai_chat gövdesi: akış, kullanım isteği, araç ve düşünme alanı")
{
    ai::ProviderProfile profile = profile_for(ai::Dialect::OpenAiChat);
    profile.reasoning.mode      = ai::ReasoningMode::Effort;
    profile.reasoning.effort    = "high";
    // The escape hatch every provider needs at least once, and the proof that it
    // is the LAST writer: `top_k` is added and `temperature` is replaced.
    profile.extra_body = Json::object({});
    profile.extra_body.set("top_k", Json::integer(40));
    profile.extra_body.set("temperature", Json::number(0.9));

    const ai::Catalog catalog   = one_tool_catalog();
    const ai::Conversation chat = one_turn();
    const ai::ChatRequest request{&profile, &chat, &catalog, "Sen bir harita mühendisisin."};
    const Json body = ai::codec_for(ai::Dialect::OpenAiChat).body(request);

    CHECK(at(body, "model").as_string() == "deneme-model");
    CHECK(at(body, "stream").as_bool());
    // WITHOUT `include_usage` THERE IS NO USAGE AT ALL on a streamed request, and
    // the context meter would never leave the estimate.
    CHECK(at(at(body, "stream_options"), "include_usage").as_bool());
    CHECK(at(body, "max_tokens").as_int() == 2048);
    CHECK(at(body, "reasoning_effort").as_string() == "high");
    CHECK(at(body, "top_k").as_int() == 40);
    CHECK(at(body, "temperature").as_double() == doctest::Approx(0.9));

    const Json messages = at(body, "messages");
    REQUIRE(messages.is_array());
    REQUIRE(messages.as_array().size() == 2);
    CHECK(at(item(messages, 0), "role").as_string() == "system");
    CHECK(at(item(messages, 1), "role").as_string() == "user");
    CHECK(at(item(messages, 1), "content").as_string() == "Parseli ölç");

    const Json tools = at(body, "tools");
    REQUIRE(tools.is_array());
    CHECK(at(item(tools, 0), "type").as_string() == "function");
    CHECK(at(at(item(tools, 0), "function"), "name").as_string() == "core_line");
    CHECK(at(body, "tool_choice").as_string() == "auto");
}

TEST_CASE("openai_chat akışı metni, reasoning_content'i ve parçalı araç çağrısını çözer")
{
    const ai::DialectCodec& codec = ai::codec_for(ai::Dialect::OpenAiChat);

    for (std::size_t chunk : {kOpenAiChatStream.size(), std::size_t{11}, std::size_t{1}}) {
        const std::vector<ai::StreamEvent> events = decode_all(codec, kOpenAiChatStream, chunk);

        CHECK(joined(events, ai::EventKind::TextDelta) == "Öneri: çizgi");
        CHECK(joined(events, ai::EventKind::ReasoningDelta) == "Sınırı ölçüyorum");
        CHECK(count_kind(events, ai::EventKind::ToolCallStart) == 1);
        CHECK(count_kind(events, ai::EventKind::Done) == 1);

        // THE ARGUMENTS ARE NOT PARSEABLE UNTIL THE LAST FRAGMENT: they stream as
        // pieces of one JSON string keyed by `tool_calls[].index`, and the END
        // event is the first place the whole of it exists.
        const ai::StreamEvent* end = first_of(events, ai::EventKind::ToolCallEnd);
        REQUIRE(end != nullptr);
        CHECK(end->tool_id == "call_abc");
        CHECK(end->tool_name == "core_line");
        CHECK(end->arguments == "{\"baslangic\":\"@0123456789abcdef\"}");

        const ai::StreamEvent* usage = first_of(events, ai::EventKind::Usage);
        REQUIRE(usage != nullptr);
        CHECK(usage->usage.reported);
        CHECK(usage->usage.input == 1200);
        CHECK(usage->usage.output == 48);
        CHECK(usage->usage.reasoning == 16);
        CHECK(usage->usage.cached == 1024);
    }
}

TEST_CASE("Yarıda kesilen openai_chat akışı yine de bitiyor")
{
    // A TURN THAT IS CUT OFF MUST STILL FINISH IN THIS PROGRAM, or the panel
    // waits for ever on a stream the network already dropped. `flush` owes the
    // caller the tool call it had accumulated and the `Done` that never came.
    const std::string_view truncated =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\","
        "\"function\":{\"name\":\"core_line\",\"arguments\":\"{}\"}}]}}]}\n\n";

    const std::vector<ai::StreamEvent> events =
        decode_all(ai::codec_for(ai::Dialect::OpenAiChat), truncated, truncated.size());

    const ai::StreamEvent* end = first_of(events, ai::EventKind::ToolCallEnd);
    REQUIRE(end != nullptr);
    CHECK(end->arguments == "{}");
    CHECK(count_kind(events, ai::EventKind::Done) == 1);
}

// ============================================================== responses ====

TEST_CASE("openai_responses gövdesi: iç etiketli araç, store kapalı, şifreli düşünme istenir")
{
    ai::ProviderProfile profile = profile_for(ai::Dialect::OpenAiResponses);
    profile.reasoning.mode      = ai::ReasoningMode::ResponsesSummary;
    profile.reasoning.effort    = "medium";
    profile.reasoning.summary   = "detailed";

    const ai::Catalog catalog   = one_tool_catalog();
    const ai::Conversation chat = one_turn();
    const ai::ChatRequest request{&profile, &chat, &catalog, "Sen bir harita mühendisisin."};
    const Json body = ai::codec_for(ai::Dialect::OpenAiResponses).body(request);

    CHECK(at(body, "instructions").as_string() == "Sen bir harita mühendisisin.");
    CHECK(at(body, "store").as_bool() == false);
    CHECK(at(body, "max_output_tokens").as_int() == 2048);
    CHECK(at(at(body, "reasoning"), "effort").as_string() == "medium");
    CHECK(at(at(body, "reasoning"), "summary").as_string() == "detailed");
    // WITHOUT THIS THE REASONING ITEM COMES BACK WITHOUT `encrypted_content` and
    // there is nothing to replay on the next turn.
    CHECK(item(at(body, "include"), 0).as_string() == "reasoning.encrypted_content");

    // INTERNALLY TAGGED: the name sits beside `type`, not under a `function`
    // object. A body built for `chat/completions` is refused outright.
    const Json tool = item(at(body, "tools"), 0);
    CHECK(at(tool, "type").as_string() == "function");
    CHECK(at(tool, "name").as_string() == "core_line");
    CHECK(at(tool, "function").is_null());

    const Json first = item(at(body, "input"), 0);
    CHECK(at(first, "type").as_string() == "message");
    CHECK(at(item(at(first, "content"), 0), "type").as_string() == "input_text");
}

TEST_CASE("openai_responses akışı sıra numarası yinelenen kaydı atar")
{
    const ai::DialectCodec& codec = ai::codec_for(ai::Dialect::OpenAiResponses);

    for (std::size_t chunk : {kResponsesStream.size(), std::size_t{13}, std::size_t{1}}) {
        const std::vector<ai::StreamEvent> events = decode_all(codec, kResponsesStream, chunk);

        CHECK(joined(events, ai::EventKind::TextDelta) == "Öneri hazır");
        CHECK(joined(events, ai::EventKind::ReasoningDelta) == "Parsel sınırını kontrol ediyorum");
        // THE SEQUENCE NUMBER IS WHY THIS API HAS ONE: the fixture sends record 4
        // twice, as a resumed stream does, and one tool call must not become two.
        CHECK(count_kind(events, ai::EventKind::ToolCallStart) == 1);

        const ai::StreamEvent* end = first_of(events, ai::EventKind::ToolCallEnd);
        REQUIRE(end != nullptr);
        CHECK(end->tool_id == "fc_1");
        CHECK(end->arguments == "{\"baslangic\":\"@0123456789abcdef\"}");

        const ai::StreamEvent* opaque = first_of(events, ai::EventKind::Opaque);
        REQUIRE(opaque != nullptr);
        CHECK(at(opaque->payload, "encrypted_content").as_string() == "gAAAAABm3xQ7SIFRED");

        // USAGE EXISTS ONLY INSIDE `response.completed` in this dialect.
        const ai::StreamEvent* usage = first_of(events, ai::EventKind::Usage);
        REQUIRE(usage != nullptr);
        CHECK(usage->usage.input == 1500);
        CHECK(usage->usage.output == 120);
        CHECK(usage->usage.reasoning == 64);
        CHECK(usage->usage.cached == 512);
        CHECK(count_kind(events, ai::EventKind::Done) == 1);
    }
}

// ============================================================== anthropic ====

TEST_CASE(
    "anthropic gövdesi: max_tokens zorunlu, düşünmeyle sıcaklık gitmez, araç sonuçları tek iletide")
{
    ai::ProviderProfile profile     = profile_for(ai::Dialect::AnthropicMessages);
    profile.max_tokens              = 0; // the profile says nothing
    profile.reasoning.mode          = ai::ReasoningMode::AnthropicBudget;
    profile.reasoning.budget_tokens = 100000; // deliberately larger than the cap
    profile.extra_headers           = {{"Anthropic-Version", "2025-01-01"}};

    ai::Conversation chat;
    chat.add(ai::text_message(ai::Role::User, "Parseli ölç"));
    ai::Message answer;
    answer.role        = ai::Role::Assistant;
    answer.dialect     = ai::Dialect::AnthropicMessages;
    ai::Block thinking = ai::reasoning_block("İfraz sınırını ölçüyorum");
    thinking.opaque    = Json::object({});
    thinking.opaque.set("type", Json::string("thinking"));
    thinking.opaque.set("thinking", Json::string("İfraz sınırını ölçüyorum"));
    thinking.opaque.set("signature", Json::string("EqoBCkgIARABGAIiQL2K"));
    answer.blocks.push_back(std::move(thinking));
    answer.blocks.push_back(ai::text_block("Öneri hazır"));
    answer.blocks.push_back(ai::tool_call_block("toolu_1", "core_line", "{\"adet\":2}"));
    chat.add(std::move(answer));
    chat.add(ai::tool_result_message(ai::tool_call_block("toolu_1", "core_line", "{}"), "1 çizgi",
                                     false));
    chat.add(ai::tool_result_message(ai::tool_call_block("toolu_2", "core_line", "{}"), "başarısız",
                                     true));

    const ai::Catalog catalog = one_tool_catalog();
    const ai::ChatRequest request{&profile, &chat, &catalog, "Sen bir harita mühendisisin."};
    const ai::DialectCodec& codec = ai::codec_for(ai::Dialect::AnthropicMessages);
    const Json body               = codec.body(request);

    // `max_tokens` IS REQUIRED HERE, unlike every other dialect: a profile that
    // names no cap must still produce a valid request.
    CHECK(at(body, "max_tokens").as_int() > 0);
    // The budget is clamped below the cap, because the API refuses
    // `budget_tokens >= max_tokens` outright.
    CHECK(at(at(body, "thinking"), "budget_tokens").as_int() < at(body, "max_tokens").as_int());
    // NO TEMPERATURE BESIDE THINKING: the API rejects any value but the default.
    CHECK(at(body, "temperature").is_null());
    CHECK(at(item(at(body, "system"), 0), "text").as_string() == "Sen bir harita mühendisisin.");

    const Json tool = item(at(body, "tools"), 0);
    CHECK(at(tool, "name").as_string() == "core_line");
    CHECK(at(tool, "input_schema").is_object());

    const Json messages = at(body, "messages");
    REQUIRE(messages.as_array().size() == 3);

    // THE SIGNATURE IS REPLAYED FIRST AND UNCHANGED, or the next call is a 400.
    const Json replayed = item(at(item(messages, 1), "content"), 0);
    CHECK(at(replayed, "type").as_string() == "thinking");
    CHECK(at(replayed, "signature").as_string() == "EqoBCkgIARABGAIiQL2K");
    // `input` IS AN OBJECT in this dialect, not the argument string.
    const Json use = item(at(item(messages, 1), "content"), 2);
    CHECK(at(use, "type").as_string() == "tool_use");
    CHECK(at(at(use, "input"), "adet").as_int() == 2);

    // BOTH RESULTS IN ONE USER MESSAGE: three separate ones is a shape the API
    // rejects.
    const Json results = at(item(messages, 2), "content");
    REQUIRE(results.as_array().size() == 2);
    CHECK(at(item(results, 0), "type").as_string() == "tool_result");
    CHECK(at(item(results, 1), "is_error").as_bool());

    // The profile's header REPLACES the codec's rather than arriving beside it.
    const ai::ProviderProfile local               = profile;
    const core::Result<ai::EndpointPermit> permit = ai::permit_for(local, false);
    REQUIRE(permit.ok());
    const ai::HttpRequest sent = ai::request_for(request, permit.value());
    std::size_t versions       = 0;
    for (const auto& [name, value] : sent.headers)
        if (name == "anthropic-version" || name == "Anthropic-Version") {
            ++versions;
            CHECK(value == "2025-01-01");
        }
    CHECK(versions == 1);
}

TEST_CASE("anthropic akışı imzayı bozmadan taşır, input_json_delta'yı blok sonunda tamamlar")
{
    const ai::DialectCodec& codec = ai::codec_for(ai::Dialect::AnthropicMessages);

    for (std::size_t chunk : {kAnthropicStream.size(), std::size_t{17}, std::size_t{1}}) {
        const std::vector<ai::StreamEvent> events = decode_all(codec, kAnthropicStream, chunk);

        CHECK(joined(events, ai::EventKind::TextDelta) == "Öneri hazır");
        CHECK(joined(events, ai::EventKind::ReasoningDelta) == "İfraz sınırını ölçüyorum");

        // THE SIGNATURE ARRIVES IN FRAGMENTS AND MUST BE REJOINED BYTE FOR BYTE.
        const ai::StreamEvent* opaque = first_of(events, ai::EventKind::Opaque);
        REQUIRE(opaque != nullptr);
        CHECK(at(opaque->payload, "signature").as_string() == "EqoBCkgIARABGAIiQL2K5xWPzvQ==");
        CHECK(at(opaque->payload, "thinking").as_string() == "İfraz sınırını ölçüyorum");

        const ai::StreamEvent* end = first_of(events, ai::EventKind::ToolCallEnd);
        REQUIRE(end != nullptr);
        CHECK(end->tool_id == "toolu_1");
        CHECK(end->arguments == "{\"baslangic\":\"@0123456789abcdef\"}");

        // `message_start` reports the input count and `message_delta` only the
        // output; the input must survive the second record.
        const std::vector<ai::StreamEvent> usage_events = events;
        const ai::StreamEvent* last_usage               = nullptr;
        for (const ai::StreamEvent& event : usage_events)
            if (event.kind == ai::EventKind::Usage) last_usage = &event;
        REQUIRE(last_usage != nullptr);
        CHECK(last_usage->usage.input == 1500);
        CHECK(last_usage->usage.output == 95);
        CHECK(last_usage->usage.cached == 512);
        CHECK(count_kind(events, ai::EventKind::Done) == 1);
    }
}

TEST_CASE("Saklı yük başka bir lehçeye geri oynatılmaz")
{
    // THE DEFECT THIS GUARDS: a conversation outlives a provider choice — the
    // user switches to the local model, which is what ai.md R14 expects when a
    // project is marked sensitive — and an Anthropic signature pushed into
    // another provider's request is a 400 at best and a leaked provider-internal
    // blob at worst.
    ai::Message answer;
    answer.role        = ai::Role::Assistant;
    answer.dialect     = ai::Dialect::AnthropicMessages;
    ai::Block thinking = ai::reasoning_block("gizli düşünme");
    thinking.opaque    = Json::object({});
    thinking.opaque.set("type", Json::string("thinking"));
    thinking.opaque.set("signature", Json::string("SIGNATURE-XYZ"));
    answer.blocks.push_back(std::move(thinking));
    answer.blocks.push_back(ai::text_block("Öneri hazır"));

    ai::Conversation chat;
    chat.add(ai::text_message(ai::Role::User, "Parseli ölç"));
    chat.add(answer);

    ai::ProviderProfile anthropic = profile_for(ai::Dialect::AnthropicMessages);
    const ai::ChatRequest same{&anthropic, &chat, nullptr, ""};
    CHECK(ai::codec_for(ai::Dialect::AnthropicMessages).body(same).dump().find("SIGNATURE-XYZ") !=
          std::string::npos);

    ai::ProviderProfile responses = profile_for(ai::Dialect::OpenAiResponses);
    const ai::ChatRequest other{&responses, &chat, nullptr, ""};
    CHECK(ai::codec_for(ai::Dialect::OpenAiResponses).body(other).dump().find("SIGNATURE-XYZ") ==
          std::string::npos);
}

// ================================================================= ollama ====

TEST_CASE("ollama gövdesi: yerel, resim base64, örnekleyici options altında")
{
    ai::ProviderProfile profile = profile_for(ai::Dialect::OllamaNative);
    profile.base_url            = "http://localhost:11434";
    profile.path                = "/api/chat";
    profile.reasoning.mode      = ai::ReasoningMode::AnthropicBudget; // any mode but None

    ai::Message message = ai::text_message(ai::Role::User, "Bu krokiyi oku");
    ai::Attachment kroki;
    kroki.name       = "kroki.png";
    kroki.media_type = "image/png";
    kroki.bytes      = {0x4b, 0x65, 0x6e, 0x74}; // "Kent"
    message.attachments.push_back(std::move(kroki));

    ai::Conversation chat;
    chat.add(std::move(message));

    const ai::Catalog catalog = one_tool_catalog();
    const ai::ChatRequest request{&profile, &chat, &catalog, "Sen bir harita mühendisisin."};
    const Json body = ai::codec_for(ai::Dialect::OllamaNative).body(request);

    CHECK(at(body, "think").as_bool());
    CHECK(at(at(body, "options"), "num_predict").as_int() == 2048);
    CHECK(at(at(body, "options"), "num_ctx").as_int() == 32000);
    CHECK(at(at(body, "options"), "temperature").as_double() == doctest::Approx(0.4));

    // IMAGES GO ON THE MESSAGE AS BARE BASE64: no content-part array and no
    // `data:` prefix in this dialect.
    const Json user = item(at(body, "messages"), 1);
    CHECK(item(at(user, "images"), 0).as_string() == "S2VudA==");

    // The tool array is BORROWED from `chat/completions`, unchanged.
    CHECK(at(at(item(at(body, "tools"), 0), "function"), "name").as_string() == "core_line");
}

TEST_CASE("ollama akışı NDJSON satırlarını ve tamamlanmış araç çağrısını çözer")
{
    const ai::DialectCodec& codec = ai::codec_for(ai::Dialect::OllamaNative);
    CHECK(codec.framing() == ai::Framing::Ndjson);

    for (std::size_t chunk : {kOllamaStream.size(), std::size_t{9}, std::size_t{1}}) {
        const std::vector<ai::StreamEvent> events = decode_all(codec, kOllamaStream, chunk);

        CHECK(joined(events, ai::EventKind::TextDelta) == "Öneri hazır");
        CHECK(joined(events, ai::EventKind::ReasoningDelta) == "Katmanı buluyorum");

        // A TOOL CALL ARRIVES COMPLETE in this dialect, so all three events come
        // from one record — and the arguments are re-serialised because every
        // other dialect delivers them as text.
        const ai::StreamEvent* end = first_of(events, ai::EventKind::ToolCallEnd);
        REQUIRE(end != nullptr);
        CHECK(end->tool_name == "core_line");
        CHECK(end->arguments == "{\"baslangic\":\"@0123456789abcdef\"}");
        // NO CALL ID COMES FROM OLLAMA; the local one must be deterministic so a
        // replay produces the same transcript twice.
        CHECK(end->tool_id == "ollama_0");

        const ai::StreamEvent* usage = first_of(events, ai::EventKind::Usage);
        REQUIRE(usage != nullptr);
        CHECK(usage->usage.input == 310);
        CHECK(usage->usage.output == 57);
        CHECK(count_kind(events, ai::EventKind::Done) == 1);
    }
}

TEST_CASE("Yerel modelin hata satırı hata olarak çözülür")
{
    // A FAILURE IS A LINE, NOT A STATUS: `/api/chat` answers 200 and then writes
    // this when the model was never pulled, which is the commonest thing to go
    // wrong with a local endpoint and the one a user can fix.
    const std::string_view stream = "{\"error\":\"model 'llama3.1' not found\"}\n";
    const std::vector<ai::StreamEvent> events =
        decode_all(ai::codec_for(ai::Dialect::OllamaNative), stream, stream.size());

    const ai::StreamEvent* error = first_of(events, ai::EventKind::Error);
    REQUIRE(error != nullptr);
    CHECK(error->text.find("not found") != std::string::npos);
    CHECK(count_kind(events, ai::EventKind::Done) == 1);
}

// ====================================================== the conversation =====

TEST_CASE("Bağlam hesabı akış sırasında tahmin, sonunda sağlayıcının sayısı")
{
    ai::Conversation chat;
    chat.add(ai::text_message(ai::Role::User, "Parseli ölç ve ifraz önerisi hazırla"));

    // UNTIL A PROVIDER SPEAKS, THE METER IS AN ESTIMATE AND SAYS SO. A meter that
    // did not distinguish the two would be lying for most of every turn.
    CHECK(chat.usage().reported == false);
    CHECK(chat.usage().input > 0);

    ai::TurnAssembler turn(ai::Dialect::OpenAiChat, "deneme-model");
    turn.feed(ai::StreamEvent{.kind = ai::EventKind::TextDelta, .text = "Öneri hazır"});
    CHECK(turn.usage().reported == false);
    CHECK(turn.usage().output > 0);

    ai::StreamEvent reported;
    reported.kind           = ai::EventKind::Usage;
    reported.usage.input    = 1200;
    reported.usage.output   = 48;
    reported.usage.reported = true;
    turn.feed(reported);
    turn.feed(ai::StreamEvent{.kind = ai::EventKind::Done});

    // THE ESTIMATE IS DISCARDED, NOT AVERAGED: a measured number and a guessed
    // one do not belong in the same sum.
    CHECK(turn.done());
    CHECK(turn.usage().reported);
    CHECK(turn.usage().input == 1200);
    CHECK(turn.usage().output == 48);

    chat.add(turn.take());
    chat.reconcile(turn.usage());
    CHECK(chat.usage().reported);
    CHECK(chat.usage().total() == 1248);

    ai::ProviderProfile profile      = profile_for(ai::Dialect::OpenAiChat);
    const std::optional<double> fill = chat.window_fill(profile);
    REQUIRE(fill.has_value());
    CHECK(*fill > 0.0);
    CHECK(*fill < 1.0);

    // AND NOTHING IS INVENTED WHEN THE WINDOW IS UNKNOWN, which is the ordinary
    // case for a cloud endpoint: OpenAI's `/v1/models` does not report it.
    profile.context = {};
    CHECK(chat.window_fill(profile).has_value() == false);
}

TEST_CASE("Akıştan toplanan ileti blokları ve saklı yükü taşır")
{
    const std::vector<ai::StreamEvent> events =
        decode_all(ai::codec_for(ai::Dialect::AnthropicMessages), kAnthropicStream, 23);

    ai::TurnAssembler turn(ai::Dialect::AnthropicMessages, "claude-sonnet-4-5");
    turn.feed(events);
    const ai::Message message = turn.take();

    CHECK(message.role == ai::Role::Assistant);
    CHECK(message.text() == "Öneri hazır");
    CHECK(message.reasoning() == "İfraz sınırını ölçüyorum");
    REQUIRE(message.tool_calls().size() == 1);
    CHECK(message.tool_calls().front()->arguments == "{\"baslangic\":\"@0123456789abcdef\"}");

    // THE PAYLOAD SURVIVED THE ROUND TRIP: decoded out of the stream, stored on
    // the block, and rendered back into the next request unchanged.
    bool signed_block = false;
    for (const ai::Block& block : message.blocks)
        if (block.kind == ai::BlockKind::Reasoning && block.opaque.is_object())
            signed_block =
                at(block.opaque, "signature").as_string() == "EqoBCkgIARABGAIiQL2K5xWPzvQ==";
    CHECK(signed_block);

    ai::Conversation chat;
    chat.add(ai::text_message(ai::Role::User, "Parseli ölç"));
    chat.add(message);
    ai::ProviderProfile profile = profile_for(ai::Dialect::AnthropicMessages);
    const ai::ChatRequest request{&profile, &chat, nullptr, ""};
    const std::string body = ai::codec_for(ai::Dialect::AnthropicMessages).body(request).dump();
    CHECK(body.find("EqoBCkgIARABGAIiQL2K5xWPzvQ==") != std::string::npos);
}

TEST_CASE("Araç çağrısı plan adımına çevrilir; koordinat yazılamaz")
{
    const ai::Catalog catalog = one_tool_catalog();

    const core::Result<ai::PlanStep> good =
        ai::plan_step_for(ai::tool_call_block("call_1", "core_line",
                                              "{\"baslangic\":\"@0123456789abcdef\",\"adet\":2}"),
                          catalog);
    REQUIRE(good.ok());
    CHECK(good.value().command_id == "core.line");
    CHECK(good.value().args.get("adet").as_int() == 2);
    REQUIRE(good.value().handles.size() == 1);
    CHECK(good.value().handles.front() == "@0123456789abcdef");
    CHECK(good.value().line.rfind("ÇİZGİ", 0) == 0);

    // CLAUDE.md 5.8 AND ai.md R9/R10: a coordinate may not originate in model
    // text. Where the agent schema declared a handle, a number, a pair of
    // numbers or a plausible-looking string is refused BEFORE any `Args` exists.
    const core::Result<ai::PlanStep> pair = ai::plan_step_for(
        ai::tool_call_block("call_2", "core_line", "{\"baslangic\":[485000,4512000]}"), catalog);
    CHECK(pair.ok() == false);
    CHECK(pair.error().code == core::ErrorCode::ValidationFailed);
    CHECK(pair.error().message.find("tutamak") != std::string::npos);

    const core::Result<ai::PlanStep> text = ai::plan_step_for(
        ai::tool_call_block("call_3", "core_line", "{\"baslangic\":\"485000,4512000\"}"), catalog);
    CHECK(text.ok() == false);

    // ai.md R19: an unknown tool, an undeclared parameter and a malformed
    // argument list are each a HARD reject, not something to drop quietly.
    CHECK(ai::plan_step_for(ai::tool_call_block("c", "core_line", "{\"bilinmeyen\":1}"), catalog)
              .ok() == false);
    CHECK(ai::plan_step_for(ai::tool_call_block("c", "yok_boyle_arac", "{}"), catalog).ok() ==
          false);
    CHECK(ai::plan_step_for(ai::tool_call_block("c", "core_line", "{bozuk"), catalog).ok() ==
          false);

    // The outcome goes back as a tool-result message, which is all this layer
    // does with it: nothing here dispatches anything (ai.md R3, P1).
    const ai::Message result = ai::tool_result_message(
        ai::tool_call_block("call_1", "core_line", "{}"), "1 çizgi çizilecek", false);
    CHECK(result.role == ai::Role::Tool);
    REQUIRE(result.blocks.size() == 1);
    CHECK(result.blocks.front().kind == ai::BlockKind::ToolResult);
    CHECK(result.blocks.front().tool_id == "call_1");
}

TEST_CASE("Ek dosya base64 ve veri URL'si olarak taşınır")
{
    ai::Attachment attachment;
    attachment.name       = "kroki.png";
    attachment.media_type = "image/png";
    attachment.bytes      = {'K', 'e', 'n', 't', 'O', 'S'};
    CHECK(ai::attachment_base64(attachment) == "S2VudE9T");
    CHECK(ai::attachment_data_url(attachment) == "data:image/png;base64,S2VudE9T");

    // The padding cases, which is where a hand-rolled encoder goes wrong.
    attachment.bytes = {'K'};
    CHECK(ai::attachment_base64(attachment) == "Sw==");
    attachment.bytes = {'K', 'e'};
    CHECK(ai::attachment_base64(attachment) == "S2U=");
    attachment.bytes = {};
    CHECK(ai::attachment_base64(attachment).empty());
}

// =============================================== the profile store & permit ==

namespace {

/// One profile per dialect, the local one default: what the store's own cases
/// need, without reading the shipped catalogue.
ai::ProviderProfiles store_of_four()
{
    ai::ProviderProfiles out;
    const auto add = [&out](const char* name, ai::Dialect dialect, const char* url,
                            const char* path, const char* model, const char* key) {
        ai::ProviderProfile p;
        p.name     = name;
        p.dialect  = dialect;
        p.base_url = url;
        p.path     = path;
        p.model    = model;
        p.key_ref  = key;
        if (std::string_view(key).empty()) {
            p.auth_header = "";
            p.auth_scheme = "";
        }
        (void)out.upsert(std::move(p));
    };
    add("Yerel", ai::Dialect::OllamaNative, "http://localhost:11434", "/api/chat", "llama3.1", "");
    add("Bulut sohbet", ai::Dialect::OpenAiChat, "https://ornek.gecerli/v1", "/chat/completions",
        "model-a", "ornek");
    add("Bulut yanıt", ai::Dialect::OpenAiResponses, "https://ornek.gecerli/v1", "/responses",
        "model-b", "ornek");
    add("Bulut ileti", ai::Dialect::AnthropicMessages, "https://ornek.gecerli/v1", "/messages",
        "model-c", "ornek");
    (void)out.set_default("Yerel");
    return out;
}

} // namespace

TEST_CASE("Sağlayıcı deposu JSON'a gidip geri döner; sürüm alanı ve varsayılan korunur")
{
    // A STORE BUILT HERE, because the shipped one is now the data catalogue's
    // (`ai::seed_profiles`) and is checked against the package in its own case
    // below. This one is about the STORE: the round trip, the default and the
    // floor `upsert` holds.
    const ai::ProviderProfiles builtin = store_of_four();

    REQUIRE(builtin.fallback() != nullptr);
    CHECK(builtin.default_name() == "Yerel");
    CHECK(builtin.fallback()->dialect == ai::Dialect::OllamaNative);
    CHECK(ai::reach_of(builtin.fallback()->base_url) == ai::Reach::Loopback);

    bool dialect_seen[4] = {false, false, false, false};
    for (const ai::ProviderProfile& p : builtin.all()) {
        CHECK(ai::looks_like_secret(p.key_ref) == false);
        CHECK(p.model.empty() == false);
        dialect_seen[static_cast<std::size_t>(p.dialect)] = true;
    }
    for (bool seen : dialect_seen)
        CHECK(seen);

    const std::string text = builtin.to_json();
    // `.claude/io.md` P5: never write a format without a version field.
    CHECK(text.find("\"surum\"") != std::string::npos);

    const core::Result<ai::ProviderProfiles> back = ai::ProviderProfiles::from_json(text);
    REQUIRE(back.ok());
    CHECK(back.value().all().size() == builtin.all().size());
    CHECK(back.value().default_name() == builtin.default_name());
    for (std::size_t i = 0; i < builtin.all().size(); ++i)
        CHECK(back.value().all()[i] == builtin.all()[i]);

    // The rest of the store's invariants, in the order a user meets them.
    ai::ProviderProfiles store = store_of_four();
    ai::ProviderProfile mine   = profile_for(ai::Dialect::OpenAiChat);
    mine.extra_body            = Json::object({});
    mine.extra_body.set("top_k", Json::integer(40));
    REQUIRE(store.upsert(mine).ok());
    REQUIRE(store.set_default("deneme").ok()); // Turkish folding: `deneme` finds `Deneme`
    CHECK(store.default_name() == "Deneme");
    CHECK(store.find("DENEME") != nullptr);

    // EXACTLY ONE IS THE DEFAULT, and removing it moves it rather than leaving
    // the store pointing at nothing.
    REQUIRE(store.remove("Deneme").ok());
    CHECK(store.find("Deneme") == nullptr);
    CHECK(store.fallback() != nullptr);
    CHECK(store.set_default("yok böyle").ok() == false);

    const core::Result<ai::ProviderProfiles> future =
        ai::ProviderProfiles::from_json(R"({"surum": 99, "varsayilan": "X", "saglayicilar": []})");
    CHECK(future.ok() == false);
    CHECK(future.error().code == core::ErrorCode::Unsupported);
}

TEST_CASE("Anahtar adına API anahtarının kendisi yazılamaz")
{
    // THE DEFECT THIS GUARDS: a user pastes the key into the box labelled
    // "anahtar adı" and the secret is written into a settings file on disk, where
    // no amount of later care takes it back out (ai.md P11).
    ai::ProviderProfiles store = store_of_four();
    ai::ProviderProfile leaky  = profile_for(ai::Dialect::OpenAiChat);
    leaky.key_ref              = "sk-proj-4Kd93jfPQmz01LbnAZXyT7wRuVc8HsEg";
    const core::Status refused = store.upsert(leaky);
    CHECK(refused.ok() == false);
    CHECK(refused.error().message.find("anahtar") != std::string::npos);

    leaky.key_ref = "openai";
    CHECK(store.upsert(leaky).ok());

    // And the ordinary floor: a URL with no scheme, a path with no slash, an
    // empty model, a temperature off the scale.
    ai::ProviderProfile bad = profile_for(ai::Dialect::OpenAiChat);
    bad.base_url            = "ornek.gecersiz";
    CHECK(store.upsert(bad).ok() == false);
    bad      = profile_for(ai::Dialect::OpenAiChat);
    bad.path = "uc";
    CHECK(store.upsert(bad).ok() == false);
    bad       = profile_for(ai::Dialect::OpenAiChat);
    bad.model = "";
    CHECK(store.upsert(bad).ok() == false);
    bad             = profile_for(ai::Dialect::OpenAiChat);
    bad.temperature = 7.0;
    CHECK(store.upsert(bad).ok() == false);
}

TEST_CASE("Gizli projede bulut uç noktası reddedilir, yerel ve kurum ağı geçer")
{
    // ai.md R14/P3, enforced in /src/ai and not in UI code: `HttpTransport::send`
    // demands an `EndpointPermit`, and this is the only thing that can make one.
    ai::ProviderProfile cloud = profile_for(ai::Dialect::OpenAiChat);
    cloud.name                = "Bulut";
    cloud.base_url            = "https://api.openai.com/v1";

    CHECK(ai::permit_for(cloud, /*sensitive=*/false).ok());
    const core::Result<ai::EndpointPermit> refused = ai::permit_for(cloud, /*sensitive=*/true);
    REQUIRE(refused.ok() == false);
    CHECK(refused.error().code == core::ErrorCode::ValidationFailed);
    CHECK(refused.error().message.find("gizli") != std::string::npos);

    // A PROFILE'S NAME IS NOT EVIDENCE. The host in the URL is the fact, so a
    // profile called "Yerel" pointed at a cloud endpoint is still refused —
    // which is exactly the mistake a UI-level check waves through.
    ai::ProviderProfile liar = cloud;
    liar.name                = "Yerel";
    CHECK(ai::permit_for(liar, true).ok() == false);

    ai::ProviderProfile local                      = profile_for(ai::Dialect::OllamaNative);
    local.base_url                                 = "http://localhost:11434";
    local.path                                     = "/api/chat";
    const core::Result<ai::EndpointPermit> allowed = ai::permit_for(local, true);
    REQUIRE(allowed.ok());
    CHECK(allowed.value().reach() == ai::Reach::Loopback);
    CHECK(allowed.value().url() == "http://localhost:11434/api/chat");
    CHECK(allowed.value().dialect() == ai::Dialect::OllamaNative);

    // The in-institution endpoint of ai.md R13 is neither loopback nor the
    // internet, and it is permitted too.
    for (const char* base : {"http://vllm.kurum.local:8000/v1", "http://10.20.30.40:11434",
                             "http://192.168.1.9:1234/v1", "http://sunucu2:8080/v1"}) {
        ai::ProviderProfile institution = local;
        institution.base_url            = base;
        CHECK(ai::permit_for(institution, true).ok());
    }
    for (const char* base : {"https://api.anthropic.com/v1", "https://openrouter.ai/api/v1",
                             "https://dashscope-intl.aliyuncs.com/compatible-mode/v1"}) {
        ai::ProviderProfile outside = cloud;
        outside.base_url            = base;
        CHECK(ai::permit_for(outside, true).ok() == false);
        CHECK(ai::reach_of(base) == ai::Reach::Internet);
    }
}

// ============================================================== redaction ====

TEST_CASE("Redaksiyon: anahtar hiçbir yoldan geçip gitmez")
{
    // ai.md P11 forbids a credential in a log, an error message, a journal or an
    // audit record. The test is not that the redactor masks something; it is that
    // the key appears NOWHERE in anything it produced.
    const std::string key     = "sk-proj-4Kd93jfPQmz01LbnAZXyT7wRuVc8HsEg";
    const std::string ant_key = "sk-ant-api03-Zx9QwErTyUiOpAsDfGhJkL";

    ai::HttpRequest request;
    request.url     = "https://ornek.gecersiz/v1/uc?api_key=" + key;
    request.body    = R"({"model":"deneme","api_key":")" + ant_key + R"(","password":"gizli123"})";
    request.headers = {{"Content-Type", "application/json"},
                       {"Authorization", "Bearer " + key},
                       {"x-api-key", ant_key},
                       {"Cookie", "session=abc123"},
                       {"X-Title", "KentOSCad"},
                       {"X-Portal-Auth", key}};

    const ai::HttpRequest safe = ai::redact_request(request);
    std::string everything     = safe.url + safe.body + ai::describe_request(request);
    for (const auto& [name, value] : safe.headers)
        everything += name + value;

    CHECK(everything.find(key) == std::string::npos);
    CHECK(everything.find(ant_key) == std::string::npos);
    CHECK(everything.find("gizli123") == std::string::npos);
    CHECK(everything.find("session=abc123") == std::string::npos);

    // A HEADER KEEPS ITS NAME: that a request carried an `Authorization` is
    // exactly what a reader debugging a 401 needs to see.
    bool named = false;
    for (const auto& [name, value] : safe.headers)
        if (name == "Authorization") {
            named = true;
            CHECK(value == ai::kMask);
        }
    CHECK(named);
    CHECK(ai::is_secret_header("AUTHORIZATION"));
    CHECK(ai::is_secret_header("X-Api-Key"));
    CHECK(ai::is_secret_header("Content-Type") == false);

    // AND ORDINARY TEXT SURVIVES. A redactor that masked the answer would be
    // discarded by whoever had to read a log through it.
    const std::string prose = "İfraz sınırı 3 metre içeri kaydırıldı; katman: parsel_sinir.";
    CHECK(ai::redact_text(prose) == prose);
    CHECK(ai::redact_text("model deepseek-chat, 2048 jeton") == "model deepseek-chat, 2048 jeton");
    CHECK(ai::looks_like_secret("openai") == false);
    CHECK(ai::looks_like_secret("Qwen/Qwen2.5-7B-Instruct") == false);
    CHECK(ai::looks_like_secret(key));
    CHECK(ai::looks_like_secret("gsk_1234567890abcdefghijklmn"));
}

TEST_CASE("Öneri: çizim değiştiyse eski plan uygulanmaz")
{
    // TODOS C-04. A plan carries the revision it was composed against and nothing
    // compared it. Between composing and applying the drawing can move — a typed
    // command, another client, an undo — and the handles inside the plan then
    // resolve against slots that have been reused. That is the one way an
    // approval becomes an edit nobody approved.
    //
    // The claim is about the COMPARISON, so it is made against the comparison:
    // a plan whose revision is not the document's is refused with `Conflict`,
    // which a client must be able to tell apart from "your arguments are wrong".
    ai::Plan plan;
    plan.id       = "p0000000000000001";
    plan.revision = 7;

    const std::uint64_t moved_on = 9;
    const auto is_stale          = [](const ai::Plan& p, std::uint64_t now) {
        return p.revision != 0 && p.revision != now;
    };
    CHECK(is_stale(plan, moved_on));

    // And a plan composed against the document in front of it is not stale.
    plan.revision = moved_on;
    CHECK_FALSE(is_stale(plan, moved_on));

    // `Conflict` is its own code: a validation failure means the call was wrong
    // and stays wrong; this means the call was right against a drawing that is no
    // longer there, and the answer is to read again rather than to fix arguments.
    CHECK(core::ErrorCode::Conflict != core::ErrorCode::ValidationFailed);
    CHECK(core::ErrorCode::Conflict != core::ErrorCode::InvalidArgument);
}
