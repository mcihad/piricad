// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/sse.hpp"

namespace kentos::ai {
namespace {

/// The sentinel the OpenAI dialects end a stream with. It is not JSON, so a
/// decoder that parsed every `data:` payload would report the last record of
/// every successful turn as malformed.
constexpr std::string_view kDoneSentinel = "[DONE]";

/// Splits a field line into its name and value, applying the specification's
/// rule that ONE optional space after the colon belongs to the syntax and every
/// further space belongs to the value.
std::pair<std::string_view, std::string_view> split_field(std::string_view line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) return {line, {}};
    std::string_view value = line.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    return {line.substr(0, colon), value};
}

} // namespace

void SseParser::dispatch(std::vector<SseEvent>& out)
{
    if (!has_fields_) return;

    // A BLANK LINE AFTER NOTHING BUT AN `id:` IS NOT AN EVENT. The specification
    // dispatches only when a data buffer exists, and a provider that sends a
    // bare `id:` to mark a resume point would otherwise produce a phantom record
    // for every decoder above to puzzle over.
    if (saw_data_ || current_.done) out.push_back(current_);

    current_    = SseEvent{};
    has_fields_ = false;
    saw_data_   = false;
}

void SseParser::take_line(std::string_view line, std::vector<SseEvent>& out)
{
    if (line.empty()) {
        dispatch(out);
        return;
    }

    // A COMMENT, WHICH IS THE KEEP-ALIVE. Every provider behind a proxy sends
    // `: ping` or a bare `:` every few seconds to stop an idle connection being
    // closed; it is a legal record that carries nothing, and treating it as
    // malformed would break exactly the long turns it exists to protect.
    if (line.front() == ':') return;

    const auto [name, value] = split_field(line);

    if (name == "data") {
        has_fields_ = true;
        if (value == kDoneSentinel) {
            current_.done = true;
            return;
        }
        if (saw_data_) current_.data.push_back('\n');
        current_.data.append(value);
        saw_data_ = true;
        return;
    }
    if (name == "event") {
        has_fields_    = true;
        current_.event = std::string(value);
        return;
    }
    if (name == "id") {
        has_fields_ = true;
        current_.id = std::string(value);
        return;
    }
    // `retry:` and any field a provider invents are accepted and ignored, which
    // is what the specification requires of an unknown field.
}

std::vector<SseEvent> SseParser::feed(std::string_view bytes)
{
    std::vector<SseEvent> out;
    buffer_.append(bytes);

    std::size_t start = 0;
    while (true) {
        const std::size_t newline = buffer_.find('\n', start);
        if (newline == std::string::npos) break;

        std::size_t end = newline;
        // `\r\n`, `\n` and a lone `\r` are all line endings in this format. The
        // lone `\r` is the one that cannot be handled here: it would be
        // indistinguishable from a chunk boundary that split a `\r\n`, so it is
        // only stripped as part of a pair — which is what every provider sends.
        if (end > start && buffer_[end - 1] == '\r') --end;

        take_line(std::string_view(buffer_).substr(start, end - start), out);
        start = newline + 1;
    }
    buffer_.erase(0, start);
    return out;
}

std::vector<SseEvent> SseParser::finish()
{
    std::vector<SseEvent> out;
    if (!buffer_.empty()) {
        std::string_view line(buffer_);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        take_line(line, out);
        buffer_.clear();
    }
    dispatch(out);
    return out;
}

std::vector<std::string> NdjsonParser::feed(std::string_view bytes)
{
    std::vector<std::string> out;
    buffer_.append(bytes);

    std::size_t start = 0;
    while (true) {
        const std::size_t newline = buffer_.find('\n', start);
        if (newline == std::string::npos) break;

        std::size_t end = newline;
        if (end > start && buffer_[end - 1] == '\r') --end;
        if (end > start) out.emplace_back(buffer_, start, end - start);
        start = newline + 1;
    }
    buffer_.erase(0, start);
    return out;
}

std::optional<std::string> NdjsonParser::finish()
{
    if (buffer_.empty()) return std::nullopt;
    std::string_view line(buffer_);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    std::optional<std::string> out;
    if (!line.empty()) out = std::string(line);
    buffer_.clear();
    return out;
}

} // namespace kentos::ai
