// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: bytes into records, incrementally.
//
// WHY THIS IS ITS OWN FILE AND ITS OWN TEST. A streamed answer arrives in
// whatever pieces the network produced. One `on_chunk` may carry three events, or
// half of one, or a single `\r` whose `\n` comes in the next packet forty
// milliseconds later. Every decoder above this line assumes it is handed WHOLE
// records, and the one bug that breaks a chat for good is a framer that loses the
// tail of a split event: the text arrives with a hole in it and nothing anywhere
// reports an error.
//
// So the framers are pure, they hold their own buffer, and the test feeds them
// deliberately hostile splits (`tests/unit/test_ai_chat.cpp`).
//
// TWO FRAMINGS, BECAUSE THE PROVIDERS USE TWO. Three dialects speak
// Server-Sent Events (W3C `text/event-stream`); Ollama's native endpoint writes
// newline-delimited JSON. They are kept general rather than shaped to those four
// callers, because the MCP half of this module streams its own records out
// through the same two shapes.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::ai {

/// One assembled Server-Sent Event.
struct SseEvent
{
    /// The `event:` field, empty when the stream sends none. The Responses API
    /// puts its event type here AND in the JSON; Anthropic puts it here and in
    /// the JSON too; `chat/completions` sends no event name at all.
    std::string event;

    /// Every `data:` line of this event, JOINED WITH `\n` — which is the
    /// specification's rule and not a convenience: a provider that pretty-prints
    /// its JSON payload sends it as several `data:` lines, and concatenating them
    /// without the newline would still parse but would corrupt any string
    /// literal that spanned the break.
    std::string data;

    /// The `id:` field, kept because a resumable stream is identified by it.
    std::string id;

    /// The `data: [DONE]` sentinel of the OpenAI dialects: a record that carries
    /// no JSON and means the stream is over. `data` is empty when this is set.
    bool done{false};
};

/// The incremental Server-Sent-Events framer.
///
/// Fed arbitrary byte chunks; returns the events those bytes COMPLETED. It
/// handles what real streams contain: `\n` and `\r\n` line endings, a value with
/// or without the single optional space after the colon, multi-line `data:`,
/// `event:`, `id:`, the `retry:` field (accepted and ignored), and COMMENT lines
/// beginning with a colon — which are how every provider keeps a connection alive
/// through a proxy, so treating one as malformed would fail a long turn at
/// exactly the point it was trying to stay open.
class SseParser
{
public:
    /// Feeds bytes and returns the events they completed, in order. A chunk that
    /// completes nothing returns an empty vector and keeps the partial line.
    std::vector<SseEvent> feed(std::string_view bytes);

    /// What a stream that ended without its final blank line left behind.
    ///
    /// NOT A PEDANTIC CASE: a provider that closes the connection right after the
    /// last `data:` line is common, and dropping that event loses the end of the
    /// answer. Returns an empty vector when nothing is pending.
    std::vector<SseEvent> finish();

    /// Whether any bytes are held back waiting for their line ending. For the
    /// test, and for a diagnostic that wants to say a stream was truncated.
    bool pending() const noexcept { return !buffer_.empty() || has_fields_; }

private:
    /// Consumes one complete line (its terminator already removed).
    void take_line(std::string_view line, std::vector<SseEvent>& out);

    /// Emits the event assembled so far, if any, and resets the field state.
    void dispatch(std::vector<SseEvent>& out);

    std::string buffer_;     ///< bytes since the last line ending
    SseEvent current_{};     ///< the event being assembled
    bool has_fields_{false}; ///< whether `current_` has received anything
    bool saw_data_{false};   ///< whether a `data:` line has been seen for `current_`
};

/// The incremental newline-delimited-JSON framer, for Ollama's native endpoint.
///
/// One object per line, and the SAME split problem: `/api/chat` writes a complete
/// JSON object per write, but a proxy or a slow read can still cut one in half.
class NdjsonParser
{
public:
    /// Feeds bytes and returns the complete lines they finished, blank lines
    /// dropped. The JSON itself is not parsed here — the dialect does that, and it
    /// reports a malformed record with its own message.
    std::vector<std::string> feed(std::string_view bytes);

    /// The last line when the stream ended without a newline, or nothing.
    std::optional<std::string> finish();

    /// Whether bytes are held back waiting for their newline.
    bool pending() const noexcept { return !buffer_.empty(); }

private:
    std::string buffer_;
};

} // namespace kentos::ai
