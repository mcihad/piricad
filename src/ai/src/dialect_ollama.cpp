// SPDX-License-Identifier: GPL-3.0-or-later
// Ollama's native `/api/chat` dialect — the LOCAL road, which `.claude/ai.md` R13
// makes mandatory rather than optional.
//
// WHY NOT SIMPLY USE OLLAMA'S `/v1` SHIM. Because the shim is lossy in the two
// places this program cares about. The native endpoint reports thinking in its
// own `message.thinking` field and takes a `think` switch to control it, and it
// answers `/api/tags` and `/api/show` with the INSTALLED models and their context
// lengths — which is the only endpoint in this whole file that can report a
// context window at all (`provider.hpp`, `ContextSource::Reported`). Both
// profiles ship; this is the default.
//
// THREE SHAPES DIFFER FROM THE OPENAI DIALECTS:
//
//   NDJSON, not SSE. One JSON object per line, `done: true` on the last, and no
//   `[DONE]` sentinel.
//
//   TOOL CALLS ARRIVE COMPLETE. `message.tool_calls` is a finished array with the
//   arguments as an OBJECT, not a streamed string — so this decoder emits all
//   three tool-call events from one record.
//
//   NO CALL ID. Ollama matches a result back to its call by NAME, so the id below
//   is minted locally and deterministically from the call's position; a test and
//   a replay must produce the same one twice.
#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/redact.hpp"

namespace kentos::ai {
namespace {

using core::Json;

/// The local id for the call at `index`; see the file header.
std::string local_call_id(int index)
{
    return "ollama_" + std::to_string(index);
}

void append_message(Json& messages, const Message& message)
{
    if (message.role == Role::Tool) {
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolResult) continue;
            Json one = Json::object({});
            one.set("role", Json::string("tool"));
            one.set("content", Json::string(block.text));
            // `tool_name` IS HOW THIS DIALECT MATCHES A RESULT to its call, there
            // being no id. Sent whenever it is known.
            if (!block.tool_name.empty()) one.set("tool_name", Json::string(block.tool_name));
            messages.push(std::move(one));
        }
        return;
    }

    Json one = Json::object({});
    one.set("role", Json::string(role_id(message.role)));
    one.set("content", Json::string(message.text()));

    if (message.role == Role::Assistant) {
        Json calls = Json::array({});
        for (const Block& block : message.blocks) {
            if (block.kind != BlockKind::ToolCall) continue;
            auto parsed   = Json::parse(block.arguments.empty() ? "{}" : block.arguments);
            Json function = Json::object({});
            function.set("name", Json::string(block.tool_name));
            // AN OBJECT, not the argument string: this dialect parses tool
            // arguments itself and refuses a string here.
            function.set("arguments",
                         parsed && parsed.value().is_object() ? parsed.value() : Json::object({}));
            Json call = Json::object({});
            call.set("function", std::move(function));
            calls.push(std::move(call));
        }
        if (!calls.as_array().empty()) one.set("tool_calls", std::move(calls));
    }

    // IMAGES GO ON THE MESSAGE, AS BARE BASE64. There is no content-part array in
    // this dialect and no `data:` prefix — one `images` array beside the text.
    // Anything that is not an image has no carrier at all, so a text attachment
    // is quoted into the content and everything else is named rather than
    // silently dropped.
    Json images = Json::array({});
    std::string extra;
    for (const Attachment& attachment : message.attachments) {
        if (is_image_attachment(attachment)) {
            images.push(Json::string(attachment_base64(attachment)));
            continue;
        }
        if (is_text_attachment(attachment)) {
            const std::string text(reinterpret_cast<const char*>(attachment.bytes.data()),
                                   attachment.bytes.size());
            extra += "\n\n" + attachment.name + ":\n" + text;
            continue;
        }
        extra += "\n\n(" + attachment.name + " — " + attachment.media_type +
                 ": bu sağlayıcıya gönderilemedi)";
    }
    if (!extra.empty()) one.set("content", Json::string(message.text() + extra));
    if (!images.as_array().empty()) one.set("images", std::move(images));

    messages.push(std::move(one));
}

class OllamaCodec final : public DialectCodec
{
public:
    Dialect dialect() const override { return Dialect::OllamaNative; }

    Framing framing() const override { return Framing::Ndjson; }

    std::vector<std::pair<std::string, std::string>>
    headers(const ProviderProfile& profile) const override
    {
        (void)profile;
        // `application/x-ndjson` whether or not the answer is streamed: a
        // non-streamed `/api/chat` returns ONE object, which is a one-line NDJSON
        // document and decodes through the same framer.
        return {{"Content-Type", "application/json"}, {"Accept", "application/x-ndjson"}};
    }

    Json tools(const Catalog& catalog) const override
    {
        // BORROWED, NOT COPIED. Ollama took the `{type:"function", function:{…}}`
        // array from `chat/completions` unchanged, and a second projection of the
        // same catalogue would be a second thing to keep right (CLAUDE.md 5.10,
        // in spirit).
        return openai_chat_codec().tools(catalog);
    }

    Json body(const ChatRequest& request) const override
    {
        const ProviderProfile& profile = *request.profile;

        Json messages = Json::array({});
        if (!request.system.empty()) {
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

        // `think` IS A SWITCH HERE, not a budget or an effort word: the local
        // runtime either lets the model emit a thinking section or it does not.
        // Any reasoning mode but `None` means the user asked for thinking, and
        // whichever currency they asked in does not reach this dialect.
        out.set("think", Json::boolean(profile.reasoning.mode != ReasoningMode::None));

        // THE SAMPLER LIVES UNDER `options`, which is where this dialect keeps
        // everything the OpenAI ones put at the top level. `num_predict` is the
        // output cap and `num_ctx` is the context window the model is LOADED
        // with — the one place a profile's context number is an instruction
        // rather than a note.
        Json options = Json::object({});
        if (profile.temperature) options.set("temperature", Json::number(*profile.temperature));
        if (profile.max_tokens > 0) options.set("num_predict", Json::integer(profile.max_tokens));
        if (profile.context.tokens > 0 && profile.context.source == ContextSource::User)
            options.set("num_ctx", Json::integer(profile.context.tokens));
        if (!options.as_object().empty()) out.set("options", std::move(options));

        if (request.catalog != nullptr && profile.tools && !request.catalog->tools.empty())
            out.set("tools", tools(*request.catalog));

        return merge_extra_body(std::move(out), profile.extra_body);
    }

    std::vector<StreamEvent> decode(const SseEvent& record, DecodeState& state) const override
    {
        std::vector<StreamEvent> out;
        if (record.data.empty()) return out;

        auto parsed = Json::parse(record.data);
        if (!parsed) {
            out.push_back(StreamEvent{.kind = EventKind::Error,
                                      .text = "Yerel modelden gelen satır okunamadı: " +
                                              redact_text(parsed.error().message)});
            return out;
        }
        const Json& line = parsed.value();

        // A FAILURE IS A LINE, NOT A STATUS. `/api/chat` answers 200 and then
        // writes `{"error": "model 'x' not found"}` when the model was never
        // pulled — which is the single most common thing to go wrong with a local
        // endpoint, and the one a user can fix.
        if (const Json* error = line.find("error"); error != nullptr && error->is_string()) {
            out.push_back(
                StreamEvent{.kind = EventKind::Error, .text = redact_text(error->as_string())});
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
            return out;
        }

        if (const Json* message = line.find("message");
            message != nullptr && message->is_object()) {
            if (const Json* content = message->find("content");
                content != nullptr && content->is_string() && !content->as_string().empty())
                out.push_back(
                    StreamEvent{.kind = EventKind::TextDelta, .text = content->as_string()});

            if (const Json* thinking = message->find("thinking");
                thinking != nullptr && thinking->is_string() && !thinking->as_string().empty())
                out.push_back(
                    StreamEvent{.kind = EventKind::ReasoningDelta, .text = thinking->as_string()});

            if (const Json* calls = message->find("tool_calls");
                calls != nullptr && calls->is_array())
                for (const Json& call : calls->as_array())
                    decode_tool_call(call, state, out);
        }

        if (const Json* done = line.find("done"); done != nullptr && done->as_bool()) {
            if (const Json* reason = line.find("done_reason");
                reason != nullptr && reason->is_string())
                state.stop_reason = reason->as_string();

            TokenUsage usage;
            if (const Json* v = line.find("prompt_eval_count")) usage.input = v->as_int();
            if (const Json* v = line.find("eval_count")) usage.output = v->as_int();
            usage.reported = true;
            state.usage    = usage;

            StreamEvent reported;
            reported.kind  = EventKind::Usage;
            reported.usage = usage;
            out.push_back(std::move(reported));

            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
        }
        return out;
    }

    std::vector<StreamEvent> flush(DecodeState& state) const override
    {
        std::vector<StreamEvent> out;
        // NOTHING TO CLOSE: a tool call arrives complete in this dialect, so the
        // only thing a truncated stream owes the caller is the `Done` that never
        // came — without which the panel waits for an answer that has stopped.
        if (!state.done) {
            state.done = true;
            out.push_back(StreamEvent{.kind = EventKind::Done});
        }
        return out;
    }

private:
    static void decode_tool_call(const Json& call, DecodeState& state,
                                 std::vector<StreamEvent>& out)
    {
        const Json* function = call.find("function");
        if (function == nullptr || !function->is_object()) return;

        std::string name;
        if (const Json* value = function->find("name"); value != nullptr && value->is_string())
            name = value->as_string();
        if (name.empty()) return;

        const int index       = static_cast<int>(state.calls.size());
        PartialToolCall& slot = state.call_at(index);
        slot.name             = name;
        slot.id               = local_call_id(index);

        std::string arguments = "{}";
        if (const Json* value = function->find("arguments"); value != nullptr)
            // SERIALISED BACK TO TEXT, because every other dialect delivers the
            // arguments as a string and `Block::arguments` keeps what the model
            // wrote verbatim. `core::Json::dump` is deterministic and preserves
            // key order, so the round trip does not reorder what is audited.
            arguments = value->dump();
        slot.arguments = arguments;
        slot.ended     = true;

        out.push_back(StreamEvent{.kind      = EventKind::ToolCallStart,
                                  .text      = {},
                                  .tool_id   = slot.id,
                                  .tool_name = slot.name,
                                  .arguments = {},
                                  .index     = index});
        out.push_back(StreamEvent{.kind      = EventKind::ToolCallArguments,
                                  .text      = {},
                                  .tool_id   = slot.id,
                                  .tool_name = slot.name,
                                  .arguments = arguments,
                                  .index     = index});
        out.push_back(StreamEvent{.kind      = EventKind::ToolCallEnd,
                                  .text      = {},
                                  .tool_id   = slot.id,
                                  .tool_name = slot.name,
                                  .arguments = arguments,
                                  .index     = index});
    }
};

const OllamaCodec kOllama;

} // namespace

/// The `ollama_native` codec.
const DialectCodec& ollama_native_codec()
{
    return kOllama;
}

} // namespace kentos::ai
