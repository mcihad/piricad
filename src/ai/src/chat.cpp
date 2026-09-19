// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/chat.hpp"

#include "kentos_cad/ai/redact.hpp"

namespace kentos::ai {
namespace {

using core::Json;

/// About this many bytes to a token; see `estimate_tokens`.
constexpr std::int64_t kBytesPerToken = 4;

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Whether `text` is a handle reference: `@` + sixteen lower-case hex digits,
/// optionally `.N` for one element of a list.
///
/// THE SHAPE ONLY. `HandleStore::resolve` (handles.hpp) is what turns one into
/// coordinates, and it lives with the session that minted it; here the question
/// is merely whether the model wrote a reference where the schema demanded one,
/// which is what makes a coordinate literal impossible rather than merely
/// forbidden (CLAUDE.md 5.8).
bool is_handle_ref(std::string_view text)
{
    if (text.size() < 17 || text.front() != '@') return false;
    for (std::size_t i = 1; i < 17; ++i) {
        const char ch  = text[i];
        const bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        if (!hex) return false;
    }
    if (text.size() == 17) return true;
    if (text[17] != '.' || text.size() == 18) return false;
    for (std::size_t i = 18; i < text.size(); ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

/// Whether this property's schema demands a handle — recognised by a `pattern`
/// anchored on `@`, which is the one `catalog.cpp` writes for a point, a point
/// list and a selection under `Style::Agent`.
bool demands_handle(const Json& schema)
{
    const Json* pattern = schema.find("pattern");
    if (pattern == nullptr || !pattern->is_string()) return false;
    return pattern->as_string().rfind("^@", 0) == 0;
}

/// The readable half of a plan step: what one argument looks like on a command
/// line. The CANONICAL line is rendered by the command layer when the plan is
/// applied; this is what a person reads in the preview.
std::string readable_value(const Json& value)
{
    switch (value.type()) {
    case Json::Type::String: return value.as_string();
    case Json::Type::Bool: return value.as_bool() ? "evet" : "hayır";
    case Json::Type::Int:
    case Json::Type::Double: return value.dump();
    case Json::Type::Array: {
        std::string out;
        for (const Json& item : value.as_array()) {
            if (!out.empty()) out += ",";
            out += readable_value(item);
        }
        return out;
    }
    case Json::Type::Null:
    case Json::Type::Object: break;
    }
    return value.dump();
}

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

core::Result<PlanStep> plan_step_for(const Block& call, const Catalog& catalog)
{
    if (call.kind != BlockKind::ToolCall)
        return core::err(core::ErrorCode::InvalidArgument, "Bu blok bir araç çağrısı değil.");

    const ToolDef* tool = catalog.find(call.tool_name);
    if (tool == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Bu sürümde böyle bir araç yok: '" + call.tool_name + "'.");

    const std::string text = call.arguments.empty() ? "{}" : call.arguments;
    auto parsed            = Json::parse(text);
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "'" + call.tool_name +
                             "' çağrısının argümanları okunamadı: " + parsed.error().message);
    const Json& args_json = parsed.value();
    if (!args_json.is_object())
        return core::err(core::ErrorCode::ParseError,
                         "'" + call.tool_name + "' çağrısının argümanları bir nesne değil.");

    const Json* properties = tool->input_schema.find("properties");

    PlanStep step;
    step.command_id  = tool->command_id;
    std::string line = tool->title;

    for (const auto& [name, value] : args_json.as_object()) {
        // A NULL IS AN ABSENT ARGUMENT, not a value. Several models write one for
        // an optional parameter they decided not to use.
        if (value.is_null()) continue;

        const Json* schema = properties != nullptr ? properties->find(name) : nullptr;
        if (schema == nullptr)
            // AI.MD R19: an undeclared parameter is a hard reject, not something
            // to drop quietly — the bus refuses it too (command.md P15), and a
            // model told nothing would keep writing it.
            return core::err(core::ErrorCode::ValidationFailed, "'" + tool->name + "' aracının '" +
                                                                    name +
                                                                    "' diye bir parametresi yok.");

        command::Value out;
        if (demands_handle(*schema)) {
            // THE COORDINATE DEFENCE (CLAUDE.md 5.8, ai.md R9/R10). This
            // parameter is a position, and a position may only arrive as a handle
            // minted by a read tool. A number, a pair of numbers or a list of
            // them is refused HERE, while the arguments are still JSON and before
            // any `Args` exists — which is what makes R10's "before validation"
            // literally true.
            if (!value.is_string() || !is_handle_ref(value.as_string()))
                return core::err(core::ErrorCode::ValidationFailed,
                                 "'" + name +
                                     "' bir konum: yalnızca bir okuma aracının döndürdüğü "
                                     "tutamak (@…) yazılabilir, koordinat yazılamaz. Gelen: " +
                                     value.dump());
            step.handles.push_back(value.as_string());
            out = command::Value::text(value.as_string());
        } else {
            switch (value.type()) {
            case Json::Type::String: out = command::Value::text(value.as_string()); break;
            case Json::Type::Bool: out = command::Value::boolean(value.as_bool()); break;
            case Json::Type::Int: out = command::Value::integer(value.as_int()); break;
            case Json::Type::Double: out = command::Value::number(value.as_double()); break;
            case Json::Type::Array: {
                command::Value::Ints ids;
                for (const Json& item : value.as_array()) {
                    if (!item.is_int())
                        return core::err(core::ErrorCode::ValidationFailed,
                                         "'" + name +
                                             "' listesi yalnızca tam sayı anahtar taşıyabilir; "
                                             "koordinat bir tutamakla verilir.");
                    ids.push_back(item.as_int());
                }
                out = command::Value::ids(std::move(ids));
                break;
            }
            case Json::Type::Object:
                return core::err(core::ErrorCode::ValidationFailed,
                                 "'" + name + "' için nesne değer verilemez.");
            case Json::Type::Null: continue;
            }
        }

        step.args.set(name, std::move(out));
        line += " " + name + "=" + readable_value(value);
    }

    step.line = std::move(line);
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
