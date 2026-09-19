// SPDX-License-Identifier: GPL-3.0-or-later
// The `/v1/messages` dialect.
//
// THE SIGNATURE IS THE WHOLE REASON THIS FILE IS CAREFUL. When extended thinking
// is on, an assistant turn comes back with a `thinking` block carrying a
// `signature`, and the next request MUST replay that block — text and signature,
// byte for byte — or the API answers 400. Not "degrades": refuses. So the block
// is stored as the provider's own JSON (`Block::opaque`) and pushed back into the
// next request untouched, and the decoder accumulates `signature_delta` fragments
// without interpreting them.
//
// THE EVENT ORDER IS FIXED, which is what makes a stateful decoder tractable
// here: `message_start` (with the initial usage) → per block
// `content_block_start`, N `content_block_delta`, `content_block_stop` →
// `message_delta` (the incremental usage and the stop reason) → `message_stop`.
// A tool call's arguments arrive as `input_json_delta.partial_json` fragments
// that are NOT valid JSON until the block stops, so nothing may try to parse them
// before `content_block_stop`.
//
// AND THE HEADERS ARE PART OF THE PROTOCOL. `x-api-key` rather than a bearer
// token, and `anthropic-version: 2023-06-01` on every request — without the
// version header the API refuses the call rather than choosing a default.
#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/redact.hpp"

namespace kentos::ai {
namespace {

using core::Json;

/// What this dialect must send when the profile names no cap. `max_tokens` is
/// REQUIRED here — unlike every other dialect, where leaving it out means "the
/// model's default" — so a profile with 0 gets this rather than a 400.
constexpr std::int64_t kMaxTokensFloor = 4096;

/// Anthropic refuses `budget_tokens >= max_tokens`, and it also refuses a budget
/// below this. A profile outside the range is clamped rather than sent, because
/// the alternative is a request this program already knows will be rejected.
constexpr std::int64_t kMinThinkingBudget = 1024;

Json text_block_json(std::string text)
{
    Json out = Json::object({});
    out.set("type", Json::string("text"));
    out.set("text", Json::string(std::move(text)));
    return out;
}

/// The content blocks of a user message: text, then whatever was attached.
Json user_content(const Message& message)
{
    Json blocks = Json::array({});
    if (!message.text().empty()) blocks.push(text_block_json(message.text()));

    for (const Attachment& attachment : message.attachments) {
        if (is_text_attachment(attachment)) {
            const std::string text(reinterpret_cast<const char*>(attachment.bytes.data()),
                                   attachment.bytes.size());
            blocks.push(text_block_json(attachment.name + ":\n" + text));
            continue;
        }
        // THE RAW BASE64, NOT A DATA URL: this dialect names the media type in
        // its own field, and a `data:` prefix inside `data` is an error.
        Json source = Json::object({});
        source.set("type", Json::string("base64"));
        source.set("media_type", Json::string(attachment.media_type));
        source.set("data", Json::string(attachment_base64(attachment)));

        Json block = Json::object({});
        block.set("type", Json::string(is_image_attachment(attachment) ? "image" : "document"));
        block.set("source", std::move(source));
        blocks.push(std::move(block));
    }
    return blocks;
}

/// The assistant's turn, as this dialect wants it back.
Json assistant_content(const Message& message)
{
    Json blocks = Json::array({});

    // THINKING FIRST, VERBATIM, AND ONLY FROM THIS DIALECT. The order is the
    // API's: a thinking block precedes the text and the tool use it led to. The
    // dialect check is `Message::dialect` (`chat.hpp`): a conversation that
    // changed provider mid-session must not push an OpenAI reasoning item in
    // here, and must not push this signature into an OpenAI request either.
    if (message.dialect == Dialect::AnthropicMessages)
        for (const Block& block : message.blocks)
            if (block.kind == BlockKind::Reasoning && block.opaque.is_object())
                blocks.push(block.opaque);

    if (!message.text().empty()) blocks.push(text_block_json(message.text()));

    for (const Block& block : message.blocks) {
        if (block.kind != BlockKind::ToolCall) continue;
        Json use = Json::object({});
        use.set("type", Json::string("tool_use"));
        use.set("id", Json::string(block.tool_id));
        use.set("name", Json::string(block.tool_name));
        // `input` IS AN OBJECT HERE, not the argument STRING the OpenAI dialects
        // send. Arguments that do not parse become an empty object: the call was
        // already refused by `plan_step_for`, and the turn still has to be
        // replayable so the model can see what it wrote and correct itself.
        auto parsed = Json::parse(block.arguments.empty() ? "{}" : block.arguments);
        use.set("input", parsed && parsed.value().is_object() ? parsed.value() : Json::object({}));
        blocks.push(std::move(use));
    }
    return blocks;
}

/// One user message carrying every tool result of one assistant turn.
///
/// MERGED ON PURPOSE. This API wants all of a turn's `tool_result` blocks at the
/// START of a single user message; three separate user messages for three results
/// is a shape it rejects.
Json tool_results_message(std::span<const Message> run)
{
    Json blocks = Json::array({});
    for (const Message& message : run)
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolResult) continue;
            Json result = Json::object({});
            result.set("type", Json::string("tool_result"));
            result.set("tool_use_id", Json::string(block.tool_id));
            result.set("content", Json::string(block.text));
            if (block.failed) result.set("is_error", Json::boolean(true));
            blocks.push(std::move(result));
        }

    Json out = Json::object({});
    out.set("role", Json::string("user"));
    out.set("content", std::move(blocks));
    return out;
}

TokenUsage usage_of(const Json& usage)
{
    TokenUsage out;
    if (const Json* v = usage.find("input_tokens")) out.input = v->as_int();
    if (const Json* v = usage.find("output_tokens")) out.output = v->as_int();
    if (const Json* v = usage.find("cache_read_input_tokens")) out.cached = v->as_int();
    out.reported = true;
    return out;
}

class AnthropicCodec final : public DialectCodec
{
public:
    Dialect dialect() const override { return Dialect::AnthropicMessages; }

    Framing framing() const override { return Framing::Sse; }

    std::vector<std::pair<std::string, std::string>>
    headers(const ProviderProfile& profile) const override
    {
        return {{"Content-Type", "application/json"},
                {"Accept", profile.stream ? "text/event-stream" : "application/json"},
                // PROTOCOL, NOT CONFIGURATION — which is why the codec sends it
                // rather than relying on a profile to. A profile that needs
                // another version overrides it by name (`request_for`).
                {"anthropic-version", "2023-06-01"}};
    }

    Json tools(const Catalog& catalog) const override
    {
        Json out = Json::array({});
        for (const ToolDef& tool : catalog.tools) {
            Json entry = Json::object({});
            entry.set("name", Json::string(tool.name));
            entry.set("description", Json::string(tool.description));
            // `input_schema`, spelled out in full: this dialect takes the JSON
            // Schema directly and has no `parameters` wrapper.
            entry.set("input_schema", tool.input_schema);
            out.push(std::move(entry));
        }
        return out;
    }

    Json body(const ChatRequest& request) const override
    {
        const ProviderProfile& profile = *request.profile;

        std::string system = request.system;
        Json messages      = Json::array({});

        const std::span<const Message> all = request.chat->messages();
        for (std::size_t at = 0; at < all.size();) {
            const Message& message = all[at];

            if (message.role == Role::System) {
                // THE SYSTEM PROMPT IS NOT A MESSAGE in this dialect: it is a
                // top-level field, so a system message in the conversation is
                // folded into it rather than dropped.
                if (!system.empty()) system += "\n\n";
                system += message.text();
                ++at;
                continue;
            }
            if (message.role == Role::Tool) {
                std::size_t end = at;
                while (end < all.size() && all[end].role == Role::Tool)
                    ++end;
                messages.push(tool_results_message(all.subspan(at, end - at)));
                at = end;
                continue;
            }

            Json one = Json::object({});
            one.set("role", Json::string(message.role == Role::Assistant ? "assistant" : "user"));
            one.set("content", message.role == Role::Assistant ? assistant_content(message)
                                                               : user_content(message));
            messages.push(std::move(one));
            ++at;
        }

        const std::int64_t max_tokens =
            profile.max_tokens > 0 ? profile.max_tokens : kMaxTokensFloor;

        Json out = Json::object({});
        out.set("model", Json::string(profile.model));
        out.set("max_tokens", Json::integer(max_tokens));
        out.set("messages", std::move(messages));
        out.set("stream", Json::boolean(profile.stream));
        if (!system.empty()) {
            // The ARRAY form rather than the string, because it is the form that
            // can later carry `cache_control` on the system prompt — and the
            // system prompt of this program is the tool catalogue's preamble,
            // which is identical on every turn.
            Json blocks = Json::array({});
            blocks.push(text_block_json(system));
            out.set("system", std::move(blocks));
        }

        const bool thinking = profile.reasoning.mode == ReasoningMode::AnthropicBudget;
        if (thinking) {
            std::int64_t budget = profile.reasoning.budget_tokens;
            if (budget < kMinThinkingBudget) budget = kMinThinkingBudget;
            if (budget >= max_tokens)
                budget = max_tokens > kMinThinkingBudget ? max_tokens - kMinThinkingBudget / 2
                                                         : kMinThinkingBudget;
            Json block = Json::object({});
            block.set("type", Json::string("enabled"));
            block.set("budget_tokens", Json::integer(budget));
            out.set("thinking", std::move(block));
        }
        // NO TEMPERATURE BESIDE THINKING. The API refuses any value but the
        // default when extended thinking is enabled, so a profile carrying both
        // would make every request fail.
        if (profile.temperature && !thinking)
            out.set("temperature", Json::number(*profile.temperature));

        if (request.catalog != nullptr && profile.tools && !request.catalog->tools.empty()) {
            out.set("tools", tools(*request.catalog));
            Json choice = Json::object({});
            choice.set("type", Json::string("auto"));
            out.set("tool_choice", std::move(choice));
        }

        return merge_extra_body(std::move(out), profile.extra_body);
    }

    std::vector<StreamEvent> decode(const SseEvent& record, DecodeState& state) const override
    {
        std::vector<StreamEvent> out;
        if (record.data.empty()) return out;

        auto parsed = Json::parse(record.data);
        if (!parsed) {
            out.push_back(StreamEvent{.kind = EventKind::Error,
                                      .text = "Sağlayıcıdan gelen akış kaydı okunamadı: " +
                                              redact_text(parsed.error().message)});
            return out;
        }
        const Json& event = parsed.value();

        std::string type = record.event;
        if (const Json* value = event.find("type"); value != nullptr && value->is_string())
            type = value->as_string();

        if (type == "message_start") {
            if (const Json* message = event.find("message"); message != nullptr)
                if (const Json* usage = message->find("usage");
                    usage != nullptr && usage->is_object()) {
                    // THE INPUT COUNT ARRIVES FIRST AND IS REAL. It is the one
                    // provider number available before the answer exists, so the
                    // context meter can stop estimating the prompt immediately.
                    StreamEvent reported;
                    reported.kind  = EventKind::Usage;
                    reported.usage = usage_of(*usage);
                    state.usage    = reported.usage;
                    out.push_back(std::move(reported));
                }
            return out;
        }
        if (type == "content_block_start") {
            decode_block_start(event, state, out);
            return out;
        }
        if (type == "content_block_delta") {
            decode_block_delta(event, state, out);
            return out;
        }
        if (type == "content_block_stop") {
            decode_block_stop(event, state, out);
            return out;
        }
        if (type == "message_delta") {
            if (const Json* delta = event.find("delta"); delta != nullptr)
                if (const Json* reason = delta->find("stop_reason");
                    reason != nullptr && reason->is_string())
                    state.stop_reason = reason->as_string();
            if (const Json* usage = event.find("usage"); usage != nullptr && usage->is_object()) {
                // INCREMENTAL, and the input count is not repeated — so the
                // reported input from `message_start` is kept and only the output
                // is taken from here.
                StreamEvent reported;
                reported.kind         = EventKind::Usage;
                reported.usage        = usage_of(*usage);
                reported.usage.input  = state.usage.input;
                reported.usage.cached = state.usage.cached;
                state.usage           = reported.usage;
                out.push_back(std::move(reported));
            }
            return out;
        }
        if (type == "message_stop") {
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
            return out;
        }
        if (type == "error") {
            std::string message;
            if (const Json* error = event.find("error"); error != nullptr)
                if (const Json* m = error->find("message"); m != nullptr && m->is_string())
                    message = m->as_string();
            out.push_back(
                StreamEvent{.kind = EventKind::Error,
                            .text = redact_text(message.empty() ? event.dump() : message)});
            return out;
        }
        // `ping` and anything a later API version adds: a keep-alive carries
        // nothing, and an unknown event is not an error (the version header pins
        // what this program understands).
        return out;
    }

    std::vector<StreamEvent> flush(DecodeState& state) const override
    {
        std::vector<StreamEvent> out;
        for (PartialToolCall& call : state.calls) {
            if (call.ended) continue;
            call.ended = true;
            out.push_back(StreamEvent{.kind      = EventKind::ToolCallEnd,
                                      .text      = {},
                                      .tool_id   = call.id,
                                      .tool_name = call.name,
                                      .arguments = call.arguments,
                                      .index     = call.index});
        }
        if (!state.done) {
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
        }
        return out;
    }

private:
    static int index_of(const Json& event)
    {
        if (const Json* value = event.find("index"); value != nullptr && value->is_int())
            return static_cast<int>(value->as_int());
        return 0;
    }

    static void decode_block_start(const Json& event, DecodeState& state,
                                   std::vector<StreamEvent>& out)
    {
        const Json* block = event.find("content_block");
        if (block == nullptr || !block->is_object()) return;
        const Json* type = block->find("type");
        if (type == nullptr || !type->is_string()) return;

        state.block_index = index_of(event);
        state.block_kind  = type->as_string();
        state.thinking.clear();
        state.signature.clear();
        state.block_data.clear();

        if (state.block_kind == "redacted_thinking") {
            // NO READABLE TEXT AT ALL, and it still has to be replayed: the whole
            // block arrives here in one piece, so it is kept now rather than at
            // the stop event.
            if (const Json* data = block->find("data"); data != nullptr && data->is_string())
                state.block_data = data->as_string();
            return;
        }
        if (state.block_kind == "tool_use") {
            PartialToolCall& call = state.call_at(state.block_index);
            if (const Json* value = block->find("id")) call.id = value->as_string();
            if (const Json* value = block->find("name")) call.name = value->as_string();
            out.push_back(StreamEvent{.kind      = EventKind::ToolCallStart,
                                      .text      = {},
                                      .tool_id   = call.id,
                                      .tool_name = call.name,
                                      .arguments = {},
                                      .index     = call.index});
        }
    }

    static void decode_block_delta(const Json& event, DecodeState& state,
                                   std::vector<StreamEvent>& out)
    {
        const Json* delta = event.find("delta");
        if (delta == nullptr || !delta->is_object()) return;
        const Json* type = delta->find("type");
        if (type == nullptr || !type->is_string()) return;
        const std::string& kind = type->as_string();

        if (kind == "text_delta") {
            if (const Json* text = delta->find("text"); text != nullptr && text->is_string())
                out.push_back(StreamEvent{.kind = EventKind::TextDelta, .text = text->as_string()});
            return;
        }
        if (kind == "thinking_delta") {
            if (const Json* text = delta->find("thinking"); text != nullptr && text->is_string()) {
                state.thinking += text->as_string();
                out.push_back(
                    StreamEvent{.kind = EventKind::ReasoningDelta, .text = text->as_string()});
            }
            return;
        }
        if (kind == "signature_delta") {
            // ACCUMULATED AND NEVER SHOWN. It is not text; it is the token that
            // makes the thinking block replayable, and the user has no use for it
            // (file header).
            if (const Json* value = delta->find("signature");
                value != nullptr && value->is_string())
                state.signature += value->as_string();
            return;
        }
        if (kind == "input_json_delta") {
            if (const Json* value = delta->find("partial_json");
                value != nullptr && value->is_string() && !value->as_string().empty()) {
                PartialToolCall& call = state.call_at(index_of(event));
                call.arguments += value->as_string();
                out.push_back(StreamEvent{.kind      = EventKind::ToolCallArguments,
                                          .text      = {},
                                          .tool_id   = call.id,
                                          .tool_name = call.name,
                                          .arguments = value->as_string(),
                                          .index     = call.index});
            }
            return;
        }
    }

    static void decode_block_stop(const Json& event, DecodeState& state,
                                  std::vector<StreamEvent>& out)
    {
        const int index = index_of(event);

        if (state.block_kind == "thinking") {
            // THE BLOCK, REBUILT EXACTLY AS THE API WILL WANT IT BACK: the
            // accumulated thinking text and the accumulated signature, in the
            // shape the request takes. Anything else here is a 400 on the next
            // call.
            Json payload = Json::object({});
            payload.set("type", Json::string("thinking"));
            payload.set("thinking", Json::string(state.thinking));
            payload.set("signature", Json::string(state.signature));
            StreamEvent opaque;
            opaque.kind    = EventKind::Opaque;
            opaque.payload = std::move(payload);
            out.push_back(std::move(opaque));
        } else if (state.block_kind == "redacted_thinking") {
            Json payload = Json::object({});
            payload.set("type", Json::string("redacted_thinking"));
            payload.set("data", Json::string(state.block_data));
            StreamEvent opaque;
            opaque.kind    = EventKind::Opaque;
            opaque.payload = std::move(payload);
            out.push_back(std::move(opaque));
        } else if (state.block_kind == "tool_use") {
            PartialToolCall& call = state.call_at(index);
            if (!call.ended) {
                call.ended = true;
                // ONLY NOW IS THE ARGUMENT TEXT VALID JSON. The fragments are
                // pieces of one string and nothing may parse them earlier
                // (file header).
                out.push_back(StreamEvent{.kind      = EventKind::ToolCallEnd,
                                          .text      = {},
                                          .tool_id   = call.id,
                                          .tool_name = call.name,
                                          .arguments = call.arguments,
                                          .index     = call.index});
            }
        }

        state.block_kind.clear();
        state.block_index = -1;
    }
};

const AnthropicCodec kAnthropic;

} // namespace

/// The `anthropic_messages` codec.
const DialectCodec& anthropic_messages_codec()
{
    return kAnthropic;
}

} // namespace kentos::ai
