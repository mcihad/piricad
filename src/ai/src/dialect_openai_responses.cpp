// SPDX-License-Identifier: GPL-3.0-or-later
// The `/v1/responses` dialect.
//
// IT IS NOT `chat/completions` WITH A NEW NAME, and treating it as one produces a
// chat that works until the model starts reasoning. Three differences carry real
// consequences:
//
//   THE EVENTS ARE TYPED AND NUMBERED. Every record names itself
//   (`response.output_text.delta`) and carries a monotonic `sequence_number`,
//   which is the mechanism for de-duplicating a RESUMED stream — so the decoder
//   drops a record whose number it has already seen rather than appending the
//   same paragraph twice.
//
//   REASONING COMES BACK AS AN ITEM, NOT AS TEXT. What streams is a SUMMARY
//   (`response.reasoning_summary_text.delta`); the chain itself is returned as a
//   reasoning ITEM whose `encrypted_content` is opaque. With `store: false` —
//   which is what a cadastral office wants, since the alternative is leaving the
//   drawing's context on somebody else's server — that item MUST be replayed on
//   the next request, or the model starts the next tool call without its own
//   reasoning and contradicts itself.
//
//   TOOLS ARE INTERNALLY TAGGED. `{type:"function", name, parameters}`, not
//   `{type:"function", function:{…}}`. A body built for the older dialect is
//   rejected outright.
#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/redact.hpp"

namespace kentos::ai {
namespace {

using core::Json;

/// One `input_text` / `output_text` content part.
Json text_part(const char* type, std::string text)
{
    Json out = Json::object({});
    out.set("type", Json::string(type));
    out.set("text", Json::string(std::move(text)));
    return out;
}

/// A message item in the `input` array.
Json message_item(const char* role, Json content)
{
    Json out = Json::object({});
    out.set("type", Json::string("message"));
    out.set("role", Json::string(role));
    out.set("content", std::move(content));
    return out;
}

Json user_content(const Message& message)
{
    Json parts = Json::array({});
    if (!message.text().empty()) parts.push(text_part("input_text", message.text()));
    for (const Attachment& attachment : message.attachments) {
        if (is_image_attachment(attachment)) {
            Json part = Json::object({});
            part.set("type", Json::string("input_image"));
            // A DATA URL, NOT A FILE ID: uploading the file first would be a
            // second request from a module that owns no transport, and an
            // uploaded file outlives the turn on the provider's disk.
            part.set("image_url", Json::string(attachment_data_url(attachment)));
            parts.push(std::move(part));
            continue;
        }
        if (is_text_attachment(attachment)) {
            const std::string text(reinterpret_cast<const char*>(attachment.bytes.data()),
                                   attachment.bytes.size());
            parts.push(text_part("input_text", attachment.name + ":\n" + text));
            continue;
        }
        Json part = Json::object({});
        part.set("type", Json::string("input_file"));
        part.set("filename", Json::string(attachment.name));
        part.set("file_data", Json::string(attachment_data_url(attachment)));
        parts.push(std::move(part));
    }
    return parts;
}

void append_items(Json& input, const Message& message)
{
    switch (message.role) {
    case Role::System: input.push(message_item("system", user_content(message))); return;
    case Role::User: input.push(message_item("user", user_content(message))); return;

    case Role::Tool:
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolResult) continue;
            Json out = Json::object({});
            out.set("type", Json::string("function_call_output"));
            out.set("call_id", Json::string(block.tool_id));
            out.set("output", Json::string(block.text));
            input.push(std::move(out));
        }
        return;

    case Role::Assistant: break;
    }

    // THE REASONING ITEMS FIRST, AND VERBATIM. Order matters in this API: the
    // reasoning item precedes the message and the function call it led to, and
    // the `encrypted_content` inside it is replayed exactly as it arrived (see
    // the file header). Only when the payload belongs to THIS dialect — a
    // conversation that changed provider mid-session must not have an Anthropic
    // signature pushed into an OpenAI request (`chat.hpp`, `Message::dialect`).
    const bool replay = message.dialect == Dialect::OpenAiResponses;
    if (replay)
        for (const Block& block : message.blocks)
            if (block.kind == BlockKind::Reasoning && block.opaque.is_object())
                input.push(block.opaque);

    if (!message.text().empty()) {
        Json content = Json::array({});
        content.push(text_part("output_text", message.text()));
        input.push(message_item("assistant", std::move(content)));
    }

    for (const Block& block : message.blocks) {
        if (block.kind != BlockKind::ToolCall) continue;
        Json out = Json::object({});
        out.set("type", Json::string("function_call"));
        out.set("call_id", Json::string(block.tool_id));
        out.set("name", Json::string(block.tool_name));
        out.set("arguments", Json::string(block.arguments));
        input.push(std::move(out));
    }
}

TokenUsage usage_of(const Json& usage)
{
    TokenUsage out;
    if (const Json* v = usage.find("input_tokens")) out.input = v->as_int();
    if (const Json* v = usage.find("output_tokens")) out.output = v->as_int();
    if (const Json* v = usage.find("output_tokens_details"); v != nullptr)
        if (const Json* w = v->find("reasoning_tokens")) out.reasoning = w->as_int();
    if (const Json* v = usage.find("input_tokens_details"); v != nullptr)
        if (const Json* w = v->find("cached_tokens")) out.cached = w->as_int();
    out.reported = true;
    return out;
}

class OpenAiResponsesCodec final : public DialectCodec
{
public:
    Dialect dialect() const override { return Dialect::OpenAiResponses; }

    Framing framing() const override { return Framing::Sse; }

    std::vector<std::pair<std::string, std::string>>
    headers(const ProviderProfile& profile) const override
    {
        return {{"Content-Type", "application/json"},
                {"Accept", profile.stream ? "text/event-stream" : "application/json"}};
    }

    Json tools(const Catalog& catalog) const override
    {
        Json out = Json::array({});
        for (const ToolDef& tool : catalog.tools) {
            Json entry = Json::object({});
            entry.set("type", Json::string("function"));
            entry.set("name", Json::string(tool.name));
            entry.set("description", Json::string(tool.description));
            entry.set("parameters", tool.input_schema);
            // `strict` false deliberately: strict mode requires every property to
            // be `required` and every object to forbid extras, and the catalogue
            // is projected from the command registry where optional parameters
            // are the norm (`catalog.cpp`). A strict schema would refuse tools
            // this program legitimately publishes.
            entry.set("strict", Json::boolean(false));
            out.push(std::move(entry));
        }
        return out;
    }

    Json body(const ChatRequest& request) const override
    {
        const ProviderProfile& profile = *request.profile;

        Json input = Json::array({});
        for (const Message& message : request.chat->messages())
            append_items(input, message);

        Json out = Json::object({});
        out.set("model", Json::string(profile.model));
        out.set("input", std::move(input));
        out.set("stream", Json::boolean(profile.stream));
        // `store: false` IS A PRIVACY DECISION, not a default. The drawing's
        // context is in this request; leaving it on the provider's servers for
        // thirty days is not something an office can consent to on a surveyor's
        // behalf. It is also what makes replaying `encrypted_content` mandatory.
        out.set("store", Json::boolean(false));
        if (!request.system.empty()) out.set("instructions", Json::string(request.system));
        if (profile.max_tokens > 0) out.set("max_output_tokens", Json::integer(profile.max_tokens));
        if (profile.temperature) out.set("temperature", Json::number(*profile.temperature));

        if (profile.reasoning.mode == ReasoningMode::ResponsesSummary ||
            profile.reasoning.mode == ReasoningMode::Effort) {
            Json reasoning = Json::object({});
            reasoning.set("effort", Json::string(profile.reasoning.effort));
            if (profile.reasoning.mode == ReasoningMode::ResponsesSummary)
                reasoning.set("summary", Json::string(profile.reasoning.summary));
            out.set("reasoning", std::move(reasoning));

            // WITHOUT THIS THE ITEM COMES BACK WITHOUT ITS `encrypted_content`
            // and there is nothing to replay — the request succeeds and the model
            // loses its chain between tool calls, which reads as a model that
            // changed its mind rather than as a missing field.
            Json include = Json::array({});
            include.push(Json::string("reasoning.encrypted_content"));
            out.set("include", std::move(include));
        }

        if (request.catalog != nullptr && profile.tools && !request.catalog->tools.empty()) {
            out.set("tools", tools(*request.catalog));
            out.set("tool_choice", Json::string("auto"));
        }

        return merge_extra_body(std::move(out), profile.extra_body);
    }

    std::vector<StreamEvent> decode(const SseEvent& record, DecodeState& state) const override
    {
        std::vector<StreamEvent> out;
        if (record.done) {
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
            return out;
        }
        if (record.data.empty()) return out;

        auto parsed = Json::parse(record.data);
        if (!parsed) {
            out.push_back(StreamEvent{.kind = EventKind::Error,
                                      .text = "Sağlayıcıdan gelen akış kaydı okunamadı: " +
                                              redact_text(parsed.error().message)});
            return out;
        }
        const Json& event = parsed.value();

        // THE NUMBER IS WHY THIS API HAS ONE. A resumed stream repeats records it
        // already sent; anything not strictly newer is a replay and is dropped.
        if (const Json* sequence = event.find("sequence_number");
            sequence != nullptr && sequence->is_int()) {
            if (sequence->as_int() <= state.last_sequence) return out;
            state.last_sequence = sequence->as_int();
        }

        // The type is in the JSON and in the SSE `event:` line; the JSON is
        // authoritative, because a proxy that rewrites event names has been seen
        // and a proxy that rewrites bodies has not.
        std::string type = record.event;
        if (const Json* value = event.find("type"); value != nullptr && value->is_string())
            type = value->as_string();

        if (type == "response.output_text.delta") {
            if (const Json* delta = event.find("delta"); delta != nullptr && delta->is_string())
                out.push_back(
                    StreamEvent{.kind = EventKind::TextDelta, .text = delta->as_string()});
            return out;
        }
        if (type == "response.reasoning_summary_text.delta") {
            if (const Json* delta = event.find("delta"); delta != nullptr && delta->is_string())
                out.push_back(
                    StreamEvent{.kind = EventKind::ReasoningDelta, .text = delta->as_string()});
            return out;
        }
        if (type == "response.output_item.added") {
            decode_item_added(event, state, out);
            return out;
        }
        if (type == "response.function_call_arguments.delta") {
            decode_arguments_delta(event, state, out);
            return out;
        }
        if (type == "response.function_call_arguments.done") {
            decode_arguments_done(event, state, out);
            return out;
        }
        if (type == "response.output_item.done") {
            decode_item_done(event, state, out);
            return out;
        }
        if (type == "response.completed" || type == "response.incomplete") {
            decode_finished(event, state, out, type == "response.incomplete");
            return out;
        }
        if (type == "response.failed" || type == "error") {
            std::string message;
            if (const Json* response = event.find("response"); response != nullptr)
                if (const Json* error = response->find("error"); error != nullptr)
                    if (const Json* m = error->find("message")) message = m->as_string();
            if (message.empty())
                if (const Json* m = event.find("message"); m != nullptr && m->is_string())
                    message = m->as_string();
            out.push_back(
                StreamEvent{.kind = EventKind::Error,
                            .text = redact_text(message.empty() ? event.dump() : message)});
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
            return out;
        }

        // EVERY OTHER TYPE IS DROPPED ON PURPOSE. This API emits a record for the
        // creation of the response, for every item and part added and done, and
        // for each `.delta`'s matching `.done` — which repeats the text that was
        // already streamed. Passing them upward as special cases is how a decoder
        // ends up printing every answer twice.
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
    static int output_index_of(const Json& event)
    {
        if (const Json* value = event.find("output_index"); value != nullptr && value->is_int())
            return static_cast<int>(value->as_int());
        return 0;
    }

    static void decode_item_added(const Json& event, DecodeState& state,
                                  std::vector<StreamEvent>& out)
    {
        const Json* item = event.find("item");
        if (item == nullptr || !item->is_object()) return;
        const Json* type = item->find("type");
        if (type == nullptr || type->as_string() != "function_call") return;

        PartialToolCall& call = state.call_at(output_index_of(event));
        if (const Json* value = item->find("call_id")) call.id = value->as_string();
        if (const Json* value = item->find("name")) call.name = value->as_string();
        // `arguments` is present and empty on the added item; the fragments
        // follow. Assigning it here rather than appending avoids a duplicate when
        // a provider fills it in immediately.
        if (const Json* value = item->find("arguments"); value != nullptr && value->is_string())
            call.arguments = value->as_string();

        out.push_back(StreamEvent{.kind      = EventKind::ToolCallStart,
                                  .text      = {},
                                  .tool_id   = call.id,
                                  .tool_name = call.name,
                                  .arguments = {},
                                  .index     = call.index});
    }

    static void decode_arguments_delta(const Json& event, DecodeState& state,
                                       std::vector<StreamEvent>& out)
    {
        const Json* delta = event.find("delta");
        if (delta == nullptr || !delta->is_string() || delta->as_string().empty()) return;

        PartialToolCall& call = state.call_at(output_index_of(event));
        call.arguments += delta->as_string();
        out.push_back(StreamEvent{.kind      = EventKind::ToolCallArguments,
                                  .text      = {},
                                  .tool_id   = call.id,
                                  .tool_name = call.name,
                                  .arguments = delta->as_string(),
                                  .index     = call.index});
    }

    static void decode_arguments_done(const Json& event, DecodeState& state,
                                      std::vector<StreamEvent>& out)
    {
        PartialToolCall& call = state.call_at(output_index_of(event));
        // THE `done` RECORD CARRIES THE WHOLE ARGUMENT STRING, and it is
        // preferred over the accumulation: if a fragment was lost anywhere
        // between the socket and here, this is the copy that is right.
        if (const Json* value = event.find("arguments");
            value != nullptr && value->is_string() && !value->as_string().empty())
            call.arguments = value->as_string();
        call.ended = true;
        out.push_back(StreamEvent{.kind      = EventKind::ToolCallEnd,
                                  .text      = {},
                                  .tool_id   = call.id,
                                  .tool_name = call.name,
                                  .arguments = call.arguments,
                                  .index     = call.index});
    }

    static void decode_item_done(const Json& event, DecodeState& state,
                                 std::vector<StreamEvent>& out)
    {
        const Json* item = event.find("item");
        if (item == nullptr || !item->is_object()) return;
        const Json* type = item->find("type");
        if (type == nullptr || !type->is_string()) return;

        if (type->as_string() == "reasoning") {
            // THE ITEM, WHOLE AND UNREAD. Its `encrypted_content` is the part
            // that must be replayed (file header); this program neither parses
            // nor trims it, because a field trimmed out of an opaque blob is a
            // 400 on the next call.
            StreamEvent opaque;
            opaque.kind    = EventKind::Opaque;
            opaque.payload = *item;
            out.push_back(std::move(opaque));
            return;
        }
        if (type->as_string() == "function_call") {
            PartialToolCall& call = state.call_at(output_index_of(event));
            if (call.ended) return;
            if (const Json* value = item->find("arguments"); value != nullptr && value->is_string())
                call.arguments = value->as_string();
            call.ended = true;
            out.push_back(StreamEvent{.kind      = EventKind::ToolCallEnd,
                                      .text      = {},
                                      .tool_id   = call.id,
                                      .tool_name = call.name,
                                      .arguments = call.arguments,
                                      .index     = call.index});
        }
    }

    static void decode_finished(const Json& event, DecodeState& state,
                                std::vector<StreamEvent>& out, bool incomplete)
    {
        if (const Json* response = event.find("response");
            response != nullptr && response->is_object())
            if (const Json* usage = response->find("usage");
                usage != nullptr && usage->is_object()) {
                // USAGE EXISTS ONLY HERE in this dialect — there is no
                // `include_usage` switch and no separate final chunk — so a
                // decoder that ignored `response.completed` would never reconcile
                // the estimate.
                StreamEvent reported;
                reported.kind  = EventKind::Usage;
                reported.usage = usage_of(*usage);
                state.usage    = reported.usage;
                out.push_back(std::move(reported));
            }
        if (incomplete) {
            std::string why = "Yanıt tamamlanmadı.";
            if (const Json* response = event.find("response"); response != nullptr)
                if (const Json* details = response->find("incomplete_details"); details != nullptr)
                    if (const Json* reason = details->find("reason");
                        reason != nullptr && reason->is_string())
                        why += " Sebep: " + reason->as_string() + ".";
            out.push_back(StreamEvent{.kind = EventKind::Error, .text = why});
        }
        state.done = true;
        out.push_back(StreamEvent{.kind = EventKind::Done});
    }
};

const OpenAiResponsesCodec kOpenAiResponses;

} // namespace

/// The `openai_responses` codec.
const DialectCodec& openai_responses_codec()
{
    return kOpenAiResponses;
}

} // namespace kentos::ai
