// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: one conversation, four wire languages, one normalised event.
//
// WHAT A CODEC DOES. Two jobs and no others: it writes the REQUEST BODY for a
// conversation plus the tool catalogue projected into its own tool shape, and it
// decodes one framed record into normalised events. It holds no socket, no
// buffer and no key; the framing is `sse.hpp`'s, the bytes are the application
// transport's, and the credential is added after this layer is finished
// (`transport.hpp`).
//
// WHY NORMALISE AT ALL. Because the panel, the plan compiler, the context meter
// and the audit record must not each learn four protocols. The normalised set is
// small on purpose — text, reasoning, a tool call's three stages, an opaque
// payload, usage, done, error — and everything a provider says that does not map
// onto it is dropped by the decoder rather than leaked upward as a special case.
//
// THE FOUR DIALECTS, AS VERIFIED — the facts each implementation is written
// against, kept here because a reader of `dialect_anthropic.cpp` alone cannot
// tell which of its oddities are the protocol's and which are ours:
//
//   openai_chat        `POST {base}/chat/completions`. SSE of `chat.completion.chunk`
//                      objects terminated by `data: [DONE]`. USAGE ARRIVES ONLY
//                      when `stream_options: {"include_usage": true}` is sent, and
//                      then as one extra chunk after the final choice. Tools are
//                      `{type:"function", function:{name, description, parameters}}`.
//                      Tool-call arguments stream as concatenated fragments of a
//                      JSON string keyed by `tool_calls[].index`, so nothing is
//                      parseable until the last fragment. DeepSeek and its
//                      imitators put visible reasoning in `delta.reasoning_content`.
//
//   openai_responses   `POST /v1/responses`. Typed SSE events, each carrying a
//                      monotonic `sequence_number` — which is how a resumed
//                      stream is de-duplicated. Text in
//                      `response.output_text.delta`, reasoning SUMMARIES in
//                      `response.reasoning_summary_text.delta`, usage only inside
//                      `response.completed`. Tools are INTERNALLY TAGGED
//                      (`{type:"function", name, parameters}`) rather than nested
//                      under a `function` object. A reasoning item's
//                      `encrypted_content` MUST be replayed on the next request
//                      when `store:false`, or the model loses its own chain.
//
//   anthropic_messages `POST /v1/messages`, headers `x-api-key` and
//                      `anthropic-version: 2023-06-01`. A FIXED EVENT ORDER:
//                      `message_start` (initial usage) → per block
//                      `content_block_start`, N `content_block_delta`
//                      (`text_delta` | `thinking_delta` | `signature_delta` |
//                      `input_json_delta`, whose `partial_json` accumulates and is
//                      only parseable at the block's stop), `content_block_stop` →
//                      `message_delta` (incremental usage, stop reason) →
//                      `message_stop`. A `thinking` block carries a `signature`
//                      that MUST be replayed BYTE-IDENTICALLY or the next call is
//                      a 400. Tools are `{name, description, input_schema}`.
//
//   ollama_native      `POST {base}/api/chat`. NDJSON, one object per line,
//                      `done: true` on the last one, tool calls arriving COMPLETE
//                      in `message.tool_calls` rather than streamed. `/api/tags`
//                      lists the installed models.
//
// WHICH IS WHY A MESSAGE KEEPS AN OPAQUE PAYLOAD. Two of those four protocols
// hand back a piece of the assistant's turn that this program can neither
// interpret nor reconstruct — Anthropic's thinking `signature` and the Responses
// API's `encrypted_content` — and both must come back unchanged on the next
// request or the provider refuses it. So a block stores the provider's own JSON
// beside our rendered text, and the codec replays that JSON verbatim. It is also
// why a message records WHICH DIALECT its payloads belong to (`chat.hpp`): an
// Anthropic signature replayed into an OpenAI request is a 400 at best and a
// leaked provider-internal blob at worst.
#pragma once

#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/sse.hpp"
#include "kentos_cad/ai/tool.hpp"
#include "kentos_cad/ai/transport.hpp"

#include "kentos_cad/core/json.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// Declared here and defined in `chat.hpp`, which includes this file: a codec
/// takes a whole conversation but the conversation is assembled from the events
/// below, and the include has to point one way.
class Conversation;

/// What a turn cost, in the only currency every provider reports.
///
/// `reported` IS THE HONEST BIT. During a stream these numbers are this
/// program's own estimate (`estimate_tokens`, `chat.hpp`) and they are wrong —
/// tokenisers differ, Turkish costs more per character than English, and no
/// client can know the provider's system-prompt overhead. At the end the
/// provider sends its real count and `Conversation::reconcile` replaces the
/// estimate with it. A context meter that never said which of the two it was
/// showing would be lying for most of every turn.
struct TokenUsage
{
    std::int64_t input{0};     ///< prompt tokens, including whatever the provider adds
    std::int64_t output{0};    ///< completion tokens
    std::int64_t reasoning{0}; ///< thinking tokens, when the provider itemises them
    std::int64_t cached{0};    ///< prompt tokens served from the provider's cache
    bool reported{false};      ///< true when the provider said so, false when we estimated

    /// The total a context meter shows.
    std::int64_t total() const noexcept { return input + output; }

    friend bool operator==(const TokenUsage&, const TokenUsage&) = default;
};

/// What one decoded record means. The whole normalised set.
enum class EventKind : std::uint8_t {
    None,              ///< a record that carries nothing this program uses: a ping, a keep-alive
    TextDelta,         ///< visible answer text
    ReasoningDelta,    ///< thinking text or a reasoning summary
    ToolCallStart,     ///< a tool call has begun; its name and id are known
    ToolCallArguments, ///< a fragment of that call's arguments, as written
    ToolCallEnd,       ///< the call's arguments are complete
    Opaque,            ///< a provider payload to be replayed unchanged (see the file header)
    Usage,             ///< the provider's own token counts
    Done,              ///< the turn is over
    Error,             ///< the provider reported a failure mid-stream
};

/// Turkish for an event kind, for a diagnostic line and the tests.
const char* event_kind_label(EventKind kind);

/// One decoded record. Which fields are set depends on `kind`, and a field that
/// does not apply is left empty rather than defaulted to something plausible.
struct StreamEvent
{
    EventKind kind{EventKind::None}; ///< which of the ten this record is

    /// `TextDelta` / `ReasoningDelta`: the text. `Error`: the message, already
    /// passed through `redact_text` because a provider's error body sometimes
    /// quotes the request back.
    std::string text;

    std::string tool_id;   ///< the provider's own call id, replayed in the result
    std::string tool_name; ///< the wire tool name, as the catalogue published it

    /// `ToolCallArguments`: this fragment. `ToolCallEnd`: the whole argument
    /// text, accumulated — which is the first moment it can be parsed as JSON.
    std::string arguments;

    /// Which call this record belongs to. `chat/completions` keys its fragments
    /// by this index and sends the id only once, so it is the only way to tell
    /// two concurrent calls apart.
    int index{-1};

    TokenUsage usage;   ///< `Usage` only
    core::Json payload; ///< `Opaque` only: the provider's own JSON, verbatim
};

/// How a dialect's response stream is framed.
enum class Framing : std::uint8_t {
    Sse,    ///< `text/event-stream`, framed by `SseParser`
    Ndjson, ///< one JSON object per line, framed by `NdjsonParser`
};

/// One tool call while its arguments are still arriving.
struct PartialToolCall
{
    int index{-1};         ///< the provider's index for this call
    std::string id;        ///< the provider's call id
    std::string name;      ///< the wire tool name
    std::string arguments; ///< everything received so far
    bool ended{false};     ///< whether `ToolCallEnd` has already been emitted
};

/// What a decoder must remember between records.
///
/// EVERY DIALECT NEEDS STATE, and pretending otherwise is what makes streaming
/// decoders wrong. `chat/completions` sends a tool call's name once and its
/// arguments in twenty later chunks; Anthropic identifies a block by an index set
/// in an earlier event and never repeats it; the Responses API numbers its events
/// so a resumed stream can drop what it already saw. The state is a public struct
/// rather than a codec member so that a codec instance is shared, `const` and
/// thread-safe, and one decode run owns exactly one of these.
struct DecodeState
{
    std::vector<PartialToolCall> calls; ///< in the order the provider opened them
    TokenUsage usage;                   ///< the tally so far, merged across records
    bool done{false};                   ///< whether the turn has ended
    std::string stop_reason;            ///< the provider's own word for why it stopped

    /// The index of the content block currently open, and what kind it is —
    /// Anthropic only, where a delta says nothing about which block it belongs
    /// to beyond an index (`text`, `thinking`, `redacted_thinking`, `tool_use`).
    int block_index{-1};
    std::string block_kind;

    std::string thinking;   ///< the open thinking block's text
    std::string signature;  ///< that block's signature, kept for byte-identical replay
    std::string block_data; ///< a `redacted_thinking` block's opaque data

    /// The highest `sequence_number` seen, for the Responses API. A record whose
    /// number is not greater is a REPLAY of something already decoded — which is
    /// what the field is for — and is dropped.
    std::int64_t last_sequence{-1};

    /// The call at that index, opened if this is the first record for it.
    PartialToolCall& call_at(int index);

    /// The call with that provider id, or null.
    PartialToolCall* call_by_id(std::string_view id);
};

/// Everything a codec needs to write one request.
struct ChatRequest
{
    const ProviderProfile* profile{nullptr}; ///< never null
    const Conversation* chat{nullptr};       ///< never null

    /// The tool catalogue, or null when this turn offers no tools — which is
    /// what `ProviderProfile::tools == false` and a read-only session both mean.
    const Catalog* catalog{nullptr};

    /// The system prompt. A separate field because two of the four dialects do
    /// not carry it as a message: Anthropic has a top-level `system` and the
    /// Responses API has `instructions`.
    std::string system;
};

/// One wire language: how to ask, and how to read the answer.
class DialectCodec
{
public:
    /// Virtual, because the application holds codecs through this interface.
    virtual ~DialectCodec() = default;

    /// Which dialect this codec speaks. Compared against a message's recorded
    /// dialect before an opaque payload is replayed.
    virtual Dialect dialect() const = 0;

    /// How this dialect's response stream is framed.
    virtual Framing framing() const = 0;

    /// The request body.
    virtual core::Json body(const ChatRequest& request) const = 0;

    /// The headers this dialect requires beyond the profile's own — and NEVER the
    /// credential, which the application adds (`transport.hpp`).
    virtual std::vector<std::pair<std::string, std::string>>
    headers(const ProviderProfile& profile) const = 0;

    /// The catalogue in this dialect's tool shape.
    virtual core::Json tools(const Catalog& catalog) const = 0;

    /// One framed record, decoded. For an NDJSON dialect the record arrives as an
    /// `SseEvent` whose `data` is the line and whose `event` is empty, so that
    /// one decoder signature serves both framings.
    virtual std::vector<StreamEvent> decode(const SseEvent& record, DecodeState& state) const = 0;

    /// What a stream that ENDED EARLY still owes the caller: a tool call whose
    /// arguments were complete but whose closing record never arrived, and the
    /// `Done` that a truncated stream never sent. A turn that was cut off must
    /// still finish in this program, or the panel waits for ever.
    virtual std::vector<StreamEvent> flush(DecodeState& state) const = 0;
};

/// The codec for a dialect. One shared, stateless, `const` instance per dialect.
const DialectCodec& codec_for(Dialect dialect);

/// The four codecs by name, for a caller that wants one directly.
///
/// Named accessors rather than only `codec_for` because one dialect BORROWS from
/// another: Ollama's native endpoint takes the same `{type:"function", …}` tool
/// array as `chat/completions`, and reusing that projection is better than a
/// second copy of it that can drift (CLAUDE.md 5.10, in spirit).
const DialectCodec& openai_chat_codec();
const DialectCodec& openai_responses_codec();
const DialectCodec& anthropic_messages_codec();
const DialectCodec& ollama_native_codec();

/// `body` with `extra` laid over it, SHALLOWLY: a member of `extra` replaces the
/// member of that name outright rather than being merged into it.
///
/// Shallow on purpose. A provider knob is a top-level field (`top_k`,
/// `reasoning_format`, `chat_template_kwargs`), and a deep merge would make
/// `{"reasoning": {"effort": "high"}}` silently keep a `summary` the user was
/// trying to replace. Last writer wins, and the user's `extra_body` is the last
/// writer — which is what makes it an escape hatch rather than a suggestion.
core::Json merge_extra_body(core::Json body, const core::Json& extra);

/// The whole HTTP request for one turn: the permitted URL, the dialect's headers
/// followed by the profile's own, and the serialised body. Everything except the
/// credential (`transport.hpp`).
HttpRequest request_for(const ChatRequest& request, const EndpointPermit& permit);

/// Bytes in, normalised events out: the framer and the codec, wired together with
/// one decode state.
///
/// This is what the application's `StreamSink` drives. It exists so that no
/// caller has to know that three dialects are SSE and one is NDJSON, and so that
/// a test can prove a decoder by feeding it recorded bytes in hostile splits.
class StreamDecoder
{
public:
    /// Decodes with `codec`, which must outlive the decoder — it is the shared
    /// instance `codec_for` returns.
    explicit StreamDecoder(const DialectCodec& codec) : codec_(&codec) {}

    /// Feeds bytes and returns every event they completed, in order.
    std::vector<StreamEvent> feed(std::string_view bytes);

    /// What the end of the stream completes: the framer's pending record, then
    /// the codec's own `flush`.
    std::vector<StreamEvent> finish();

    /// The decode state, for a diagnostic and for the tests.
    const DecodeState& state() const noexcept { return state_; }

private:
    const DialectCodec* codec_;
    SseParser sse_;
    NdjsonParser ndjson_;
    DecodeState state_;
};

} // namespace kentos::ai
