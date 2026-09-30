// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the conversation, as data.
//
// A MESSAGE IS BLOCKS, NOT A STRING. Every provider that can reason or call a
// tool returns a turn made of parts — visible text, thinking, a tool call, and on
// the way back a tool result — and a model of the conversation that stored a
// single string would have to re-derive those parts from prose every time it
// needed one. Worse, it could not replay what it did not understand: two of the
// four dialects return a piece of the turn that MUST come back unchanged on the
// next request (`dialect.hpp`, the note on opaque payloads), and a string has
// nowhere to keep it.
//
// THE TOOL LOOP IS DATA, AND THAT IS THE WHOLE POINT. Nothing here dispatches
// anything. A decoded tool call becomes a `PlanStep` — the same shape the MCP
// server produces — and the application runs it through the preview and the
// explicit approval that CLAUDE.md 5.7 and ai.md R3/P1 require, then hands the
// outcome back as a tool-result message. A model in this program composes; a
// person applies. So this file produces and consumes values, and the one thing it
// refuses to do is the one thing a chat loop usually does.
//
// AND IT REFUSES A COORDINATE. `plan_step_for` rejects a number where the
// agent-facing schema declared a handle, before any `Args` exists — CLAUDE.md 5.8
// and ai.md R9/R10 made a parse failure rather than a policy check.
#pragma once

#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/handles.hpp"
#include "kentos_cad/ai/plan.hpp"
#include "kentos_cad/ai/tool.hpp"

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {
class Registry; ///< where a tool call finds the command it names
} // namespace kentos::command

namespace kentos::ai {

/// Who a message is from.
enum class Role : std::uint8_t {
    System,    ///< the instructions, carried as a message by two dialects and as a field by two
    User,      ///< the engineer at the workstation
    Assistant, ///< the model
    Tool,      ///< the result of a tool call, going back to the model
};

/// The role's wire word, which is what three of the four dialects put in a
/// message object.
const char* role_id(Role role);

/// What one part of one message holds.
enum class BlockKind : std::uint8_t {
    Text,       ///< what the user reads
    Reasoning,  ///< thinking text or a reasoning summary; shown only when the profile says so
    ToolCall,   ///< the model asked for a tool, with the arguments it wrote
    ToolResult, ///< what running it produced, on its way back to the model
};

/// The block kind's wire word, for a transcript and for the tests.
const char* block_kind_id(BlockKind kind);

/// One part of one message.
struct Block
{
    BlockKind kind{BlockKind::Text}; ///< which of the four parts this is

    /// `Text` and `Reasoning`: the prose. `ToolResult`: what the command echoed,
    /// as the model will read it.
    std::string text;

    std::string tool_id;   ///< `ToolCall` / `ToolResult`: the provider's own call id
    std::string tool_name; ///< `ToolCall` / `ToolResult`: the wire tool name

    /// `ToolCall`: the arguments exactly as the model wrote them, as JSON TEXT
    /// rather than parsed. Kept verbatim because the audit record has to show
    /// what was asked for, including a malformed request that was refused.
    std::string arguments;

    bool failed{false}; ///< `ToolResult`: whether the call was refused

    /// THE PROVIDER'S OWN FORM OF THIS BLOCK, replayed unchanged on the next
    /// request. An Anthropic `thinking` block with its `signature`, or a
    /// Responses reasoning item with its `encrypted_content`: both are opaque to
    /// this program and both are mandatory on the following call (`dialect.hpp`).
    /// Null when the dialect needs nothing replayed.
    core::Json opaque;
};

/// A block holding text. A free function rather than a constructor so that the
/// KIND is written at every call site, which is the same reason `command::Value`
/// is built this way.
Block text_block(std::string text);

/// A block holding reasoning text.
Block reasoning_block(std::string text);

/// A block holding one tool call.
Block tool_call_block(std::string tool_id, std::string tool_name, std::string arguments);

/// A block holding one tool result.
Block tool_result_block(std::string tool_id, std::string tool_name, std::string text, bool failed);

/// One file the user attached to a message.
struct Attachment
{
    std::string name;       ///< the file's own name, shown in the transcript
    std::string media_type; ///< `image/png`, `application/pdf`, `text/plain`

    /// The bytes. NOT A PATH: this module does no file I/O (`.claude/ai.md` P10),
    /// so the application reads the file and hands the content over.
    std::vector<std::uint8_t> bytes;
};

/// Whether this attachment is an image, which is the only kind every dialect can
/// carry as something other than text.
bool is_image_attachment(const Attachment& attachment);

/// Whether this attachment is text, and can therefore simply be quoted into the
/// prompt when a dialect has no file part for it.
bool is_text_attachment(const Attachment& attachment);

/// The attachment's bytes, base64-encoded.
///
/// HAND-ROLLED, AND SAID OUT LOUD (CLAUDE.md 5.16, Article 9 step 3). Every
/// mature base64 this program could reach is in a library this module may not
/// link: Qt's is banned here by ai.md P10, and neither `kentos_core` nor
/// `kentos_command` has one today. It is twenty lines of table lookup with a
/// round-trip test, which is the exception that rule allows and not a licence to
/// hand-roll the next thing.
std::string attachment_base64(const Attachment& attachment);

/// The attachment as an RFC 2397 data URL: `data:<media_type>;base64,<bytes>`.
/// Two of the four dialects want this form and two want the raw base64, which is
/// why both exist.
std::string attachment_data_url(const Attachment& attachment);

/// One message of the conversation.
struct Message
{
    Role role{Role::User};               ///< who it is from
    std::vector<Block> blocks;           ///< the parts of the turn, in the order they arrived
    std::vector<Attachment> attachments; ///< what the user sent with it; see the carrying rules

    /// WHICH DIALECT THE OPAQUE PAYLOADS BELONG TO. A conversation outlives a
    /// provider choice — the user switches from a cloud model to the local one
    /// mid-session, which is exactly what ai.md R14 expects to happen when a
    /// project is marked sensitive — and an Anthropic signature replayed into an
    /// OpenAI request is a 400 at best. A codec therefore replays a block's
    /// `opaque` only when this field is its own dialect.
    Dialect dialect{Dialect::OpenAiChat};

    /// Which model produced it, for the audit record (ai.md R6 requires model
    /// identity and version). Empty on a user message.
    std::string model;

    /// Every `Text` block joined with a newline: what a transcript shows and what
    /// the context estimate measures. Reasoning is deliberately excluded — it is
    /// not the answer, and three of the four dialects refuse to take it back.
    std::string text() const;

    /// The `Reasoning` blocks joined, for a panel that shows them folded away.
    std::string reasoning() const;

    /// The tool calls this message asked for, in order.
    std::vector<const Block*> tool_calls() const;
};

/// A message with one text block.
Message text_message(Role role, std::string text);

/// How many tokens a piece of text is, roughly.
///
/// AN ESTIMATE, AND IT SAYS SO WHEREVER IT IS USED. Four BYTES per token: about
/// right for Latin prose, and deliberately measured in bytes rather than
/// characters so that Turkish — where `ğ`, `ı`, `ş` and their capitals cost two
/// bytes each in UTF-8 — estimates HIGH rather than low. A context meter that
/// under-reports is the one that lets a turn fail at the provider. The real
/// number arrives with the provider's usage record and replaces this
/// (`Conversation::reconcile`).
std::int64_t estimate_tokens(std::string_view text);

/// The messages of one chat, and what they have cost.
class Conversation
{
public:
    /// Appends a message.
    void add(Message message);

    /// Every message, oldest first — the order every dialect requires.
    std::span<const Message> messages() const noexcept { return messages_; }

    bool empty() const noexcept { return messages_.empty(); }

    std::size_t size() const noexcept { return messages_.size(); }

    /// Forgets everything, including the accounting. What "yeni sohbet" does.
    void clear();

    /// The last message, for a caller appending to a turn in progress. Null when
    /// the conversation is empty.
    Message* last();

    /// What this conversation is believed to cost: the provider's numbers when it
    /// has reported any, and `estimate()` until then. `reported` says which.
    const TokenUsage& usage() const noexcept { return usage_; }

    /// This program's own count over every message. Honest but wrong by a few
    /// percent, and never presented as anything else.
    TokenUsage estimate() const;

    /// Replaces the running account with what the provider reported at the end of
    /// a turn.
    ///
    /// THE RECONCILIATION IS THE POINT. During a stream the meter moves on our
    /// estimate, because there is nothing else; when the usage record arrives the
    /// estimate is DISCARDED rather than averaged, since a measured number and a
    /// guessed one do not belong in the same sum. An unreported usage (`reported
    /// == false`) is accepted too — that is what an interrupted turn leaves.
    void reconcile(const TokenUsage& usage);

    /// How much of the model's context is used, 0–1, or nothing when the window
    /// is unknown — which is the ordinary case for a cloud endpoint, because
    /// OpenAI's `/v1/models` does not report it (`provider.hpp`, `ContextSource`).
    std::optional<double> window_fill(const ProviderProfile& profile) const;

private:
    std::vector<Message> messages_;
    TokenUsage usage_;
};

/// Folds the decoded events of one turn into one assistant message.
///
/// WHY A SEPARATE OBJECT. The events arrive in fragments and the message has to
/// be readable while they do — the panel shows the answer as it is typed — so
/// something has to own the half-built message. Keeping that outside
/// `Conversation` means a cancelled turn is discarded by dropping this object,
/// and the conversation is never left holding half an answer (ai.md R20: a
/// cancelled suggestion leaves everything as it was).
class TurnAssembler
{
public:
    /// Assembles a turn from `dialect`, recording `model` on the message so the
    /// audit record can name what produced it.
    TurnAssembler(Dialect dialect, std::string model);

    /// Folds one event in.
    void feed(const StreamEvent& event);

    /// Folds a batch in, which is what a decoder returns.
    void feed(std::span<const StreamEvent> events);

    /// The message as it stands. Safe to read mid-stream; that is what it is for.
    const Message& message() const noexcept { return message_; }

    /// Hands the message over, leaving this assembler empty.
    Message take();

    /// What the turn cost: the provider's numbers once it reported them, and the
    /// local estimate over the text received so far until then.
    TokenUsage usage() const;

    /// Whether the provider has reported the turn finished.
    bool done() const noexcept { return done_; }

    /// The provider's failure message, empty when there was none. Already
    /// redacted (`redact.hpp`).
    const std::string& error() const noexcept { return error_; }

    /// The completed tool calls, in the order the model asked for them.
    std::vector<Block> tool_calls() const;

private:
    Message message_;
    TokenUsage usage_;
    std::int64_t text_bytes_{0};
    std::int64_t reasoning_bytes_{0};
    bool done_{false};
    std::string error_;
};

/// The plan step one tool call becomes, or the Turkish refusal it earns.
///
/// THIS IS THE STRICT SCHEMA CHECK ai.md R19 REQUIRES — an unknown tool, an
/// undeclared parameter or a malformed argument is a HARD REJECT — and it is the
/// coordinate defence of R9/R10 at the same time, in the compiler the MCP server
/// shares (`compile_arguments`): a position is a handle from `handles`, or a
/// handle moved by a stated dimension, and never a number a model wrote. The
/// handles are resolved at `revision`, so the step holds the points they name.
/// It DISPATCHES NOTHING; the returned step goes to the approval path.
core::Result<PlanStep> plan_step_for(const Block& call, const Catalog& catalog,
                                     const command::Registry& registry, const HandleStore& handles,
                                     std::uint64_t revision);

/// The message that carries one tool's outcome back to the model. `failed` marks
/// a refusal, which every dialect can express and which a model needs in order to
/// stop retrying the same call.
Message tool_result_message(const Block& call, std::string_view output, bool failed);

} // namespace kentos::ai
