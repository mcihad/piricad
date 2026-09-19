// SPDX-License-Identifier: GPL-3.0-or-later
// The `chat/completions` dialect — and, at the bottom, the plumbing every
// dialect shares.
//
// WHY THE SHARED PART IS IN THIS FILE. This change adds no `dialect.cpp`:
// `codec_for`, `StreamDecoder`, `request_for`, `merge_extra_body` and the
// `DecodeState` helpers are four screens of code that belong to no single
// dialect, and a fifth translation unit holding only them would be a file a
// reader opens by accident. They live here because `openai_chat` is the dialect
// eleven of the thirteen shipped providers speak, so this is the file a reader
// opens FIRST. The section is marked.
#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/redact.hpp"

#include <algorithm>

namespace kentos::ai {
namespace {

using core::Json;

/// The tool-calling shape of this dialect, which Ollama's native endpoint also
/// takes: `{type:"function", function:{name, description, parameters}}`.
Json function_tools(const Catalog& catalog)
{
    Json out = Json::array({});
    for (const ToolDef& tool : catalog.tools) {
        Json function = Json::object({});
        function.set("name", Json::string(tool.name));
        function.set("description", Json::string(tool.description));
        function.set("parameters", tool.input_schema);

        Json entry = Json::object({});
        entry.set("type", Json::string("function"));
        entry.set("function", std::move(function));
        out.push(std::move(entry));
    }
    return out;
}

/// The `content` of a user message: a plain string when there is nothing but
/// text, and the multi-part array when something is attached.
Json user_content(const Message& message)
{
    if (message.attachments.empty()) return Json::string(message.text());

    Json parts = Json::array({});
    if (!message.text().empty()) {
        Json part = Json::object({});
        part.set("type", Json::string("text"));
        part.set("text", Json::string(message.text()));
        parts.push(std::move(part));
    }
    for (const Attachment& attachment : message.attachments) {
        if (is_image_attachment(attachment)) {
            Json url = Json::object({});
            url.set("url", Json::string(attachment_data_url(attachment)));
            Json part = Json::object({});
            part.set("type", Json::string("image_url"));
            part.set("image_url", std::move(url));
            parts.push(std::move(part));
            continue;
        }
        if (is_text_attachment(attachment)) {
            // QUOTED INTO THE PROMPT, because this dialect's `file` part is
            // served by OpenAI and by almost none of the eleven others. A text
            // file quoted with its name is understood by every one of them.
            const std::string text(reinterpret_cast<const char*>(attachment.bytes.data()),
                                   attachment.bytes.size());
            Json part = Json::object({});
            part.set("type", Json::string("text"));
            part.set("text", Json::string(attachment.name + ":\n" + text));
            parts.push(std::move(part));
            continue;
        }
        Json file = Json::object({});
        file.set("filename", Json::string(attachment.name));
        file.set("file_data", Json::string(attachment_data_url(attachment)));
        Json part = Json::object({});
        part.set("type", Json::string("file"));
        part.set("file", std::move(file));
        parts.push(std::move(part));
    }
    return parts;
}

/// Appends `message` to `messages` in this dialect's shape. One message may
/// become several: a turn that ran three tools sends three `role:"tool"`
/// messages, each keyed by its own `tool_call_id`.
void append_message(Json& messages, const Message& message)
{
    if (message.role == Role::Tool) {
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolResult) continue;
            Json one = Json::object({});
            one.set("role", Json::string("tool"));
            one.set("tool_call_id", Json::string(block.tool_id));
            one.set("content", Json::string(block.text));
            messages.push(std::move(one));
        }
        return;
    }

    Json one = Json::object({});
    one.set("role", Json::string(role_id(message.role)));

    if (message.role == Role::Assistant) {
        one.set("content", Json::string(message.text()));
        // REASONING IS NOT REPLAYED. `delta.reasoning_content` is an OUTPUT
        // field: DeepSeek documents that sending it back is an error, and no
        // other provider in this dialect accepts it either. The block is kept in
        // the conversation for the user and the audit record, and left out here.
        Json calls = Json::array({});
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolCall) continue;
            Json function = Json::object({});
            function.set("name", Json::string(block.tool_name));
            function.set("arguments", Json::string(block.arguments));
            Json call = Json::object({});
            call.set("id", Json::string(block.tool_id));
            call.set("type", Json::string("function"));
            call.set("function", std::move(function));
            calls.push(std::move(call));
        }
        if (!calls.as_array().empty()) one.set("tool_calls", std::move(calls));
        messages.push(std::move(one));
        return;
    }

    if (message.role == Role::User)
        one.set("content", user_content(message));
    else
        one.set("content", Json::string(message.text()));
    messages.push(std::move(one));
}

/// The usage object of this dialect, which arrives only when
/// `stream_options.include_usage` was asked for.
TokenUsage usage_of(const Json& usage)
{
    TokenUsage out;
    if (const Json* v = usage.find("prompt_tokens")) out.input = v->as_int();
    if (const Json* v = usage.find("completion_tokens")) out.output = v->as_int();
    if (const Json* v = usage.find("completion_tokens_details"); v != nullptr)
        if (const Json* w = v->find("reasoning_tokens")) out.reasoning = w->as_int();
    if (const Json* v = usage.find("prompt_tokens_details"); v != nullptr)
        if (const Json* w = v->find("cached_tokens")) out.cached = w->as_int();
    out.reported = true;
    return out;
}

class OpenAiChatCodec final : public DialectCodec
{
public:
    Dialect dialect() const override { return Dialect::OpenAiChat; }

    Framing framing() const override { return Framing::Sse; }

    std::vector<std::pair<std::string, std::string>>
    headers(const ProviderProfile& profile) const override
    {
        return {{"Content-Type", "application/json"},
                {"Accept", profile.stream ? "text/event-stream" : "application/json"}};
    }

    Json tools(const Catalog& catalog) const override { return function_tools(catalog); }

    Json body(const ChatRequest& request) const override
    {
        const ProviderProfile& profile = *request.profile;

        Json messages = Json::array({});
        if (!request.system.empty()) {
            // `system`, not `developer`. The newest OpenAI models prefer the
            // latter and still accept this; the other ten providers in this
            // dialect accept only this. A profile that needs `developer` writes
            // it through `extra_body`.
            Json one = Json::object({});
            one.set("role", Json::string("system"));
            one.set("content", Json::string(request.system));
            messages.push(std::move(one));
        }
        for (const Message& message : request.chat->messages())
            append_message(messages, message);

        Json out = Json::object({});
        out.set("model", Json::string(profile.model));
        out.set("messages", std::move(messages));
        out.set("stream", Json::boolean(profile.stream));
        if (profile.stream) {
            // WITHOUT THIS THERE IS NO USAGE AT ALL on a streamed request, and
            // the context meter would run on the local estimate for ever.
            Json options = Json::object({});
            options.set("include_usage", Json::boolean(true));
            out.set("stream_options", std::move(options));
        }
        if (profile.max_tokens > 0) out.set("max_tokens", Json::integer(profile.max_tokens));
        if (profile.temperature) out.set("temperature", Json::number(*profile.temperature));

        switch (profile.reasoning.mode) {
        case ReasoningMode::Effort:
            out.set("reasoning_effort", Json::string(profile.reasoning.effort));
            break;
        case ReasoningMode::QwenBudget:
            // DashScope's pair. `enable_thinking` is REQUIRED on a streamed call
            // to a Qwen hybrid model: the default differs between models, so
            // leaving it out makes the answer's shape depend on the model id.
            out.set("enable_thinking", Json::boolean(true));
            if (profile.reasoning.budget_tokens > 0)
                out.set("thinking_budget", Json::integer(profile.reasoning.budget_tokens));
            break;
        case ReasoningMode::None:
        case ReasoningMode::ResponsesSummary:
        case ReasoningMode::AnthropicBudget:
            // The other two modes belong to other dialects. Ignored rather than
            // refused: a user who switched a profile's dialect should get a
            // working request, not an error about a field this one never sends.
            break;
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
            for (StreamEvent& event : close_calls(state))
                out.push_back(std::move(event));
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
        const Json& chunk = parsed.value();

        // A MID-STREAM ERROR IS A TOP-LEVEL OBJECT, not an HTTP status: the
        // response was already 200 when the model hit a content filter or the
        // account ran out of credit.
        if (const Json* error = chunk.find("error"); error != nullptr && error->is_object()) {
            std::string message;
            if (const Json* m = error->find("message")) message = m->as_string();
            out.push_back(
                StreamEvent{.kind = EventKind::Error,
                            .text = redact_text(message.empty() ? error->dump() : message)});
            return out;
        }

        if (const Json* choices = chunk.find("choices"); choices != nullptr && choices->is_array())
            for (const Json& choice : choices->as_array())
                decode_choice(choice, state, out);

        if (const Json* usage = chunk.find("usage"); usage != nullptr && usage->is_object()) {
            StreamEvent event;
            event.kind  = EventKind::Usage;
            event.usage = usage_of(*usage);
            state.usage = event.usage;
            out.push_back(std::move(event));
        }
        return out;
    }

    std::vector<StreamEvent> flush(DecodeState& state) const override
    {
        std::vector<StreamEvent> out = close_calls(state);
        if (!state.done) {
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
        }
        return out;
    }

private:
    /// Every call whose arguments are complete but whose closing record never
    /// came. A `finish_reason` closes them normally; this is the truncated case.
    static std::vector<StreamEvent> close_calls(DecodeState& state)
    {
        std::vector<StreamEvent> out;
        for (PartialToolCall& call : state.calls) {
            if (call.ended) continue;
            call.ended = true;
            StreamEvent event;
            event.kind      = EventKind::ToolCallEnd;
            event.tool_id   = call.id;
            event.tool_name = call.name;
            event.arguments = call.arguments;
            event.index     = call.index;
            out.push_back(std::move(event));
        }
        return out;
    }

    static void decode_choice(const Json& choice, DecodeState& state, std::vector<StreamEvent>& out)
    {
        if (const Json* delta = choice.find("delta"); delta != nullptr && delta->is_object()) {
            if (const Json* content = delta->find("content");
                content != nullptr && content->is_string() && !content->as_string().empty())
                out.push_back(
                    StreamEvent{.kind = EventKind::TextDelta, .text = content->as_string()});

            // `reasoning_content` is DeepSeek's and is what the providers that
            // copied it use; `reasoning` is what OpenRouter normalises to. Both
            // are read, because a profile does not know which one its gateway
            // will send.
            for (const char* field : {"reasoning_content", "reasoning"})
                if (const Json* value = delta->find(field);
                    value != nullptr && value->is_string() && !value->as_string().empty())
                    out.push_back(
                        StreamEvent{.kind = EventKind::ReasoningDelta, .text = value->as_string()});

            if (const Json* calls = delta->find("tool_calls");
                calls != nullptr && calls->is_array())
                for (const Json& call : calls->as_array())
                    decode_tool_call(call, state, out);
        }

        if (const Json* reason = choice.find("finish_reason");
            reason != nullptr && reason->is_string()) {
            state.stop_reason = reason->as_string();
            for (StreamEvent& event : close_calls(state))
                out.push_back(std::move(event));
        }
    }

    static void decode_tool_call(const Json& call, DecodeState& state,
                                 std::vector<StreamEvent>& out)
    {
        // THE INDEX IS THE IDENTITY HERE. The id and the name arrive in the first
        // fragment only, and every later fragment carries nothing but this index
        // and a piece of the argument string — so a decoder that keyed on the id
        // would drop every fragment after the first.
        int index = 0;
        if (const Json* value = call.find("index"); value != nullptr && value->is_int())
            index = static_cast<int>(value->as_int());

        PartialToolCall& partial = state.call_at(index);
        const bool was_new       = partial.name.empty();

        if (const Json* value = call.find("id"); value != nullptr && value->is_string())
            if (!value->as_string().empty()) partial.id = value->as_string();

        std::string fragment;
        if (const Json* function = call.find("function");
            function != nullptr && function->is_object()) {
            if (const Json* name = function->find("name");
                name != nullptr && name->is_string() && !name->as_string().empty())
                partial.name = name->as_string();
            if (const Json* arguments = function->find("arguments");
                arguments != nullptr && arguments->is_string())
                fragment = arguments->as_string();
        }

        if (was_new && !partial.name.empty()) {
            StreamEvent event;
            event.kind      = EventKind::ToolCallStart;
            event.tool_id   = partial.id;
            event.tool_name = partial.name;
            event.index     = index;
            out.push_back(std::move(event));
        }

        if (!fragment.empty()) {
            partial.arguments += fragment;
            StreamEvent event;
            event.kind      = EventKind::ToolCallArguments;
            event.tool_id   = partial.id;
            event.tool_name = partial.name;
            event.arguments = fragment;
            event.index     = index;
            out.push_back(std::move(event));
        }
    }
};

const OpenAiChatCodec kOpenAiChat;

} // namespace

/// The `openai_chat` codec, for the dialects that reuse its tool shape.
const DialectCodec& openai_chat_codec()
{
    return kOpenAiChat;
}

// ============================================================================
// SHARED PLUMBING — see the note at the top of this file for why it is here.
// ============================================================================

namespace {

/// Header names compare case-insensitively over ASCII, which is what HTTP says
/// and what lets a profile's `Anthropic-Version` replace a codec's
/// `anthropic-version` instead of sitting beside it.
bool header_name_equals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char la = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] + 32) : a[i];
        const char lb = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] + 32) : b[i];
        if (la != lb) return false;
    }
    return true;
}

} // namespace

const char* event_kind_label(EventKind kind)
{
    switch (kind) {
    case EventKind::None: return "yok";
    case EventKind::TextDelta: return "metin";
    case EventKind::ReasoningDelta: return "dusunme";
    case EventKind::ToolCallStart: return "arac_basladi";
    case EventKind::ToolCallArguments: return "arac_argumani";
    case EventKind::ToolCallEnd: return "arac_bitti";
    case EventKind::Opaque: return "sakli";
    case EventKind::Usage: return "kullanim";
    case EventKind::Done: return "bitti";
    case EventKind::Error: return "hata";
    }
    return "yok";
}

PartialToolCall& DecodeState::call_at(int index)
{
    for (PartialToolCall& call : calls)
        if (call.index == index) return call;
    PartialToolCall call;
    call.index = index;
    calls.push_back(std::move(call));
    return calls.back();
}

PartialToolCall* DecodeState::call_by_id(std::string_view id)
{
    for (PartialToolCall& call : calls)
        if (call.id == id) return &call;
    return nullptr;
}

core::Json merge_extra_body(core::Json body, const core::Json& extra)
{
    if (!extra.is_object()) return body;
    for (const auto& [name, value] : extra.as_object())
        body.set(name, value);
    return body;
}

HttpRequest request_for(const ChatRequest& request, const EndpointPermit& permit)
{
    HttpRequest out;
    out.method = "POST";
    out.url    = permit.url();
    // A BROKEN CONTRACT PRODUCES AN EMPTY REQUEST RATHER THAN A CRASH. Both
    // pointers are documented as never null (`ChatRequest`); a caller that got it
    // wrong gets a request the provider will refuse, which is findable, instead of
    // taking the editor down mid-turn.
    if (request.profile == nullptr || request.chat == nullptr) return out;

    const DialectCodec& codec = codec_for(permit.dialect());
    out.headers               = codec.headers(*request.profile);

    // THE PROFILE'S HEADERS OVERRIDE THE DIALECT'S, by name, rather than being
    // appended beside them: `anthropic-version` is sent by the Anthropic codec
    // because it is protocol, and an operator who has to pin a different version
    // writes it in `extra_headers` — which must REPLACE the codec's, not arrive
    // as a second header of the same name.
    for (const auto& [name, value] : request.profile->extra_headers) {
        const auto at = std::find_if(out.headers.begin(), out.headers.end(),
                                     [&name](const std::pair<std::string, std::string>& have) {
                                         return header_name_equals(have.first, name);
                                     });
        if (at != out.headers.end())
            at->second = value;
        else
            out.headers.emplace_back(name, value);
    }

    out.body = codec.body(request).dump();
    return out;
}

std::vector<StreamEvent> StreamDecoder::feed(std::string_view bytes)
{
    std::vector<StreamEvent> out;
    if (codec_->framing() == Framing::Sse) {
        for (const SseEvent& record : sse_.feed(bytes)) {
            std::vector<StreamEvent> events = codec_->decode(record, state_);
            out.insert(out.end(), std::make_move_iterator(events.begin()),
                       std::make_move_iterator(events.end()));
        }
        return out;
    }
    for (std::string& line : ndjson_.feed(bytes)) {
        SseEvent record;
        record.data                     = std::move(line);
        std::vector<StreamEvent> events = codec_->decode(record, state_);
        out.insert(out.end(), std::make_move_iterator(events.begin()),
                   std::make_move_iterator(events.end()));
    }
    return out;
}

std::vector<StreamEvent> StreamDecoder::finish()
{
    std::vector<StreamEvent> out;
    if (codec_->framing() == Framing::Sse) {
        for (const SseEvent& record : sse_.finish()) {
            std::vector<StreamEvent> events = codec_->decode(record, state_);
            out.insert(out.end(), std::make_move_iterator(events.begin()),
                       std::make_move_iterator(events.end()));
        }
    } else if (std::optional<std::string> line = ndjson_.finish(); line) {
        SseEvent record;
        record.data                     = std::move(*line);
        std::vector<StreamEvent> events = codec_->decode(record, state_);
        out.insert(out.end(), std::make_move_iterator(events.begin()),
                   std::make_move_iterator(events.end()));
    }

    std::vector<StreamEvent> remaining = codec_->flush(state_);
    out.insert(out.end(), std::make_move_iterator(remaining.begin()),
               std::make_move_iterator(remaining.end()));
    return out;
}

const DialectCodec& codec_for(Dialect dialect)
{
    switch (dialect) {
    case Dialect::OpenAiChat: return openai_chat_codec();
    case Dialect::OpenAiResponses: return openai_responses_codec();
    case Dialect::AnthropicMessages: return anthropic_messages_codec();
    case Dialect::OllamaNative: return ollama_native_codec();
    }
    return openai_chat_codec();
}

} // namespace kentos::ai
