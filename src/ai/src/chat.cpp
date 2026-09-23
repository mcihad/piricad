// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/chat.hpp"

#include "kentos_cad/ai/arguments.hpp"
#include "kentos_cad/command/registry.hpp"

#include "kentos_cad/ai/redact.hpp"

namespace kentos::ai {
namespace {

using core::Json;

/// About this many bytes to a token; see `estimate_tokens`.
constexpr std::int64_t kBytesPerToken = 4;

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Appends `text` to the last block of `kind`, opening one when there is none.
/// Returns the block, so a caller can attach a payload to it.
Block& append_to_block(Message& message, BlockKind kind, std::string_view text)
{
    if (!message.blocks.empty() && message.blocks.back().kind == kind) {
        message.blocks.back().text.append(text);
        return message.blocks.back();
    }
    Block block;
    block.kind = kind;
    block.text = std::string(text);
    message.blocks.push_back(std::move(block));
    return message.blocks.back();
}

std::int64_t tokens_of_bytes(std::int64_t bytes)
{
    if (bytes <= 0) return 0;
    return (bytes + kBytesPerToken - 1) / kBytesPerToken;
}

} // namespace

const char* role_id(Role role)
{
    switch (role) {
    case Role::System: return "system";
    case Role::User: return "user";
    case Role::Assistant: return "assistant";
    case Role::Tool: return "tool";
    }
    return "user";
}

const char* block_kind_id(BlockKind kind)
{
    switch (kind) {
    case BlockKind::Text: return "metin";
    case BlockKind::Reasoning: return "dusunme";
    case BlockKind::ToolCall: return "arac_cagrisi";
    case BlockKind::ToolResult: return "arac_sonucu";
    }
    return "metin";
}

Block text_block(std::string text)
{
    Block block;
    block.kind = BlockKind::Text;
    block.text = std::move(text);
    return block;
}

Block reasoning_block(std::string text)
{
    Block block;
    block.kind = BlockKind::Reasoning;
    block.text = std::move(text);
    return block;
}

Block tool_call_block(std::string tool_id, std::string tool_name, std::string arguments)
{
    Block block;
    block.kind      = BlockKind::ToolCall;
    block.tool_id   = std::move(tool_id);
    block.tool_name = std::move(tool_name);
    block.arguments = std::move(arguments);
    return block;
}

Block tool_result_block(std::string tool_id, std::string tool_name, std::string text, bool failed)
{
    Block block;
    block.kind      = BlockKind::ToolResult;
    block.tool_id   = std::move(tool_id);
    block.tool_name = std::move(tool_name);
    block.text      = std::move(text);
    block.failed    = failed;
    return block;
}

bool is_image_attachment(const Attachment& attachment)
{
    return attachment.media_type.rfind("image/", 0) == 0;
}

bool is_text_attachment(const Attachment& attachment)
{
    return attachment.media_type.rfind("text/", 0) == 0 ||
           attachment.media_type == "application/json";
}

std::string attachment_base64(const Attachment& attachment)
{
    const std::vector<std::uint8_t>& in = attachment.bytes;
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);

    std::size_t at = 0;
    while (at + 3 <= in.size()) {
        const std::uint32_t triple = (static_cast<std::uint32_t>(in[at]) << 16) |
                                     (static_cast<std::uint32_t>(in[at + 1]) << 8) |
                                     static_cast<std::uint32_t>(in[at + 2]);
        out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[triple & 0x3F]);
        at += 3;
    }
    if (const std::size_t left = in.size() - at; left > 0) {
        std::uint32_t triple = static_cast<std::uint32_t>(in[at]) << 16;
        if (left == 2) triple |= static_cast<std::uint32_t>(in[at + 1]) << 8;
        out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
        out.push_back(left == 2 ? kBase64Alphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::string attachment_data_url(const Attachment& attachment)
{
    return "data:" + attachment.media_type + ";base64," + attachment_base64(attachment);
}

std::string Message::text() const
{
    std::string out;
    for (const Block& block : blocks) {
        if (block.kind != BlockKind::Text) continue;
        if (!out.empty()) out += "\n";
        out += block.text;
    }
    return out;
}

std::string Message::reasoning() const
{
    std::string out;
    for (const Block& block : blocks) {
        if (block.kind != BlockKind::Reasoning) continue;
        if (!out.empty()) out += "\n";
        out += block.text;
    }
    return out;
}

std::vector<const Block*> Message::tool_calls() const
{
    std::vector<const Block*> out;
    for (const Block& block : blocks)
        if (block.kind == BlockKind::ToolCall) out.push_back(&block);
    return out;
}

Message text_message(Role role, std::string text)
{
    Message message;
    message.role = role;
    message.blocks.push_back(text_block(std::move(text)));
    return message;
}

std::int64_t estimate_tokens(std::string_view text)
{
    return tokens_of_bytes(static_cast<std::int64_t>(text.size()));
}

// -------------------------------------------------------------- Conversation --

void Conversation::add(Message message)
{
    messages_.push_back(std::move(message));
    // The running account follows the conversation until a provider reports a
    // real number, so a meter shown before the first answer is not zero.
    if (!usage_.reported) usage_ = estimate();
}

void Conversation::clear()
{
    messages_.clear();
    usage_ = TokenUsage{};
}

Message* Conversation::last()
{
    return messages_.empty() ? nullptr : &messages_.back();
}

TokenUsage Conversation::estimate() const
{
    TokenUsage out;
    std::int64_t bytes = 0;
    for (const Message& message : messages_) {
        for (const Block& block : message.blocks) {
            // REASONING IS NOT COUNTED as input: three of the four dialects
            // refuse to take it back, so it is not part of the next request.
            if (block.kind == BlockKind::Reasoning) continue;
            bytes += static_cast<std::int64_t>(block.text.size());
            bytes += static_cast<std::int64_t>(block.arguments.size());
        }
        for (const Attachment& attachment : message.attachments) {
            // A TEXT ATTACHMENT IS QUOTED INTO THE PROMPT, so its bytes count.
            // AN IMAGE'S COST IS NOT ESTIMABLE HERE: every provider prices one by
            // its own tiling rule (a 1024×1024 image is not 1 MB of tokens), so
            // it is left out rather than guessed at by an order of magnitude.
            if (is_text_attachment(attachment))
                bytes += static_cast<std::int64_t>(attachment.bytes.size());
        }
    }
    out.input    = tokens_of_bytes(bytes);
    out.reported = false;
    return out;
}

void Conversation::reconcile(const TokenUsage& usage)
{
    usage_ = usage;
}

std::optional<double> Conversation::window_fill(const ProviderProfile& profile) const
{
    if (profile.context.tokens <= 0) return std::nullopt;
    const double used   = static_cast<double>(usage_.total());
    const double window = static_cast<double>(profile.context.tokens);
    return used / window;
}

// ------------------------------------------------------------- TurnAssembler --

TurnAssembler::TurnAssembler(Dialect dialect, std::string model)
{
    message_.role    = Role::Assistant;
    message_.dialect = dialect;
    message_.model   = std::move(model);
}

void TurnAssembler::feed(const StreamEvent& event)
{
    switch (event.kind) {
    case EventKind::TextDelta:
        append_to_block(message_, BlockKind::Text, event.text);
        text_bytes_ += static_cast<std::int64_t>(event.text.size());
        break;

    case EventKind::ReasoningDelta:
        append_to_block(message_, BlockKind::Reasoning, event.text);
        reasoning_bytes_ += static_cast<std::int64_t>(event.text.size());
        break;

    case EventKind::ToolCallStart: {
        // A RESTART OF THE SAME CALL IS NOT A SECOND CALL: a gateway that repeats
        // the opening chunk would otherwise put the same tool in the plan twice.
        for (Block& block : message_.blocks)
            if (block.kind == BlockKind::ToolCall && !event.tool_id.empty() &&
                block.tool_id == event.tool_id)
                return;
        message_.blocks.push_back(tool_call_block(event.tool_id, event.tool_name, std::string{}));
        break;
    }

    case EventKind::ToolCallArguments:
    case EventKind::ToolCallEnd: {
        Block* target = nullptr;
        for (Block& block : message_.blocks) {
            if (block.kind != BlockKind::ToolCall) continue;
            // By id when there is one, and otherwise the MOST RECENT call: a
            // dialect that streams fragments without repeating the id (a bare
            // gateway) is still appending to the call it just opened.
            if (event.tool_id.empty() || block.tool_id == event.tool_id) target = &block;
        }
        if (target == nullptr) {
            message_.blocks.push_back(
                tool_call_block(event.tool_id, event.tool_name, std::string{}));
            target = &message_.blocks.back();
        }
        if (event.kind == EventKind::ToolCallEnd)
            // THE END EVENT IS AUTHORITATIVE, because the decoder accumulated the
            // fragments and this program did not: a fragment lost to a reframing
            // bug would otherwise leave a half-JSON argument list that parses to
            // nothing and refuses silently.
            target->arguments = event.arguments;
        else
            target->arguments.append(event.arguments);
        if (target->tool_name.empty()) target->tool_name = event.tool_name;
        break;
    }

    case EventKind::Opaque: {
        // Attached to the reasoning block it belongs to, opening one with NO TEXT
        // when the payload has none — which is exactly a `redacted_thinking`
        // block: unreadable, and still mandatory on the next request.
        Block& block = append_to_block(message_, BlockKind::Reasoning, std::string_view{});
        block.opaque = event.payload;
        break;
    }

    case EventKind::Usage:
        if (event.usage.input != 0) usage_.input = event.usage.input;
        if (event.usage.output != 0) usage_.output = event.usage.output;
        if (event.usage.reasoning != 0) usage_.reasoning = event.usage.reasoning;
        if (event.usage.cached != 0) usage_.cached = event.usage.cached;
        usage_.reported = true;
        break;

    case EventKind::Done: done_ = true; break;

    case EventKind::Error: error_ = redact_text(event.text); break;

    case EventKind::None: break;
    }
}

void TurnAssembler::feed(std::span<const StreamEvent> events)
{
    for (const StreamEvent& event : events)
        feed(event);
}

Message TurnAssembler::take()
{
    Message out      = std::move(message_);
    message_         = Message{};
    message_.role    = Role::Assistant;
    message_.dialect = out.dialect;
    message_.model   = out.model;
    return out;
}

TokenUsage TurnAssembler::usage() const
{
    TokenUsage out = usage_;
    // THE ESTIMATE FILLS ONLY WHAT WAS NOT REPORTED. A provider that itemises
    // reasoning tokens has already counted them inside `output`, so the local
    // guess must not be added on top of a measured number.
    if (out.output == 0) out.output = tokens_of_bytes(text_bytes_ + reasoning_bytes_);
    if (out.reasoning == 0) out.reasoning = tokens_of_bytes(reasoning_bytes_);
    return out;
}

std::vector<Block> TurnAssembler::tool_calls() const
{
    std::vector<Block> out;
    for (const Block& block : message_.blocks)
        if (block.kind == BlockKind::ToolCall) out.push_back(block);
    return out;
}

// --------------------------------------------------------- the tool-call loop --

core::Result<PlanStep> plan_step_for(const Block& call, const Catalog& catalog,
                                     const command::Registry& registry, const HandleStore& handles,
                                     std::uint64_t revision)
{
    if (call.kind != BlockKind::ToolCall)
        return core::err(core::ErrorCode::InvalidArgument, "Bu blok bir araç çağrısı değil.");

    const ToolDef* tool = catalog.find(call.tool_name);
    if (tool == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Bu sürümde böyle bir araç yok: '" + call.tool_name + "'.");
    const command::CommandSpec* spec = registry.by_id(tool->command_id);
    if (spec == nullptr)
        return core::err(core::ErrorCode::NotFound, "'" + call.tool_name +
                                                        "' kayıtlı olmayan bir komuta bakıyor: '" +
                                                        tool->command_id + "'.");

    const std::string text = call.arguments.empty() ? "{}" : call.arguments;
    auto parsed            = Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "'" + call.tool_name +
                             "' çağrısının argümanları okunamadı: " + parsed.error().message);
    if (!parsed.value().is_object())
        return core::err(core::ErrorCode::ParseError,
                         "'" + call.tool_name + "' çağrısının argümanları bir nesne değil.");

    // THE COMPILER THE MCP SERVER USES (`ai/arguments.hpp`). This road used to
    // copy a handle's TEXT into the argument, where a command expecting points
    // found a word — so the chat could draw nothing and could not act on a
    // selection. The handle is resolved here, against the store the chat's own
    // read tools minted into, at the document's revision now.
    CompiledArguments compiled = compile_arguments(*spec, parsed.value(), handles, revision);
    if (!compiled.refusal.empty())
        return core::err(compiled.coordinate_literal || compiled.protocol_fault
                             ? core::ErrorCode::ValidationFailed
                             : core::ErrorCode::InvalidArgument,
                         std::move(compiled.refusal));

    PlanStep step;
    step.command_id    = tool->command_id;
    step.args          = std::move(compiled.args);
    step.handles       = std::move(compiled.handles);
    step.constructions = std::move(compiled.constructions);
    step.assumptions   = std::move(compiled.assumptions);
    step.line          = render_line(*spec, step.args);
    return step;
}

Message tool_result_message(const Block& call, std::string_view output, bool failed)
{
    Message message;
    message.role = Role::Tool;
    message.blocks.push_back(
        tool_result_block(call.tool_id, call.tool_name, std::string(output), failed));
    return message;
}

} // namespace kentos::ai
