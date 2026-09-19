// SPDX-License-Identifier: AGPL-3.0-or-later
// AGPL rather than GPL for the reason `jsonrpc.hpp` states: CLAUDE.md Article 2.1
// puts a server component under AGPLv3, and this is part of one.
#include "kentos_cad/ai/endpoint.hpp"

#include <optional>

namespace kentos::ai {
namespace {

using core::Json;

/// Decodes standard Base64, or nothing when the text is not valid Base64.
///
/// `=` ends the data rather than being validated as padding: a header value is
/// produced by a client library and the only question that matters here is what
/// the bytes decode to. A malformed value falls through to `std::nullopt` and
/// `decode_header_value` then hands back the raw text, which fails the header
/// comparison — which is the correct outcome for a non-conforming client.
std::optional<std::string> decode_base64(std::string_view text)
{
    static constexpr std::string_view kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    std::uint32_t bits = 0;
    unsigned held      = 0;

    for (const char c : text) {
        if (c == '=') break;
        const std::size_t at = kAlphabet.find(c);
        if (at == std::string_view::npos) return std::nullopt;
        bits = (bits << 6) | static_cast<std::uint32_t>(at);
        held += 6;
        if (held >= 8) {
            held -= 8;
            out.push_back(static_cast<char>((bits >> held) & 0xFFu));
        }
    }
    return out;
}

/// ASCII-only lower-casing, for an HTTP scheme name.
///
/// NOT `std::tolower`, which CLAUDE.md 5.6 bans outright, and not the Turkish
/// folding table either: `Bearer` is an RFC 7235 token in US-ASCII whose case is
/// declared insensitive by that RFC, and folding `I` to `I` is exactly what is
/// wanted here. Turkish never reaches this function.
char ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/// Whether every character of `text` is a decimal digit. A port, and nothing else.
bool all_digits(std::string_view text)
{
    if (text.empty()) return false;
    for (const char c : text)
        if (c < '0' || c > '9') return false;
    return true;
}

} // namespace

std::string SseFrame::render() const
{
    return comment.empty() ? sse_event(event, data) : sse_comment(comment);
}

std::string_view HttpOutcome::header(std::string_view name) const
{
    for (const auto& [key, value] : headers)
        if (key == name) return value;
    return {};
}

bool origin_allowed(std::string_view origin, const ServerPolicy& policy)
{
    // No `Origin` at all: a native client, a script, this project's own tests.
    // A browser always sends one on a cross-origin request, so its absence is
    // evidence rather than a gap.
    if (origin.empty()) return true;

    // `null` is what a sandboxed frame or a `file://` document sends. It names no
    // origin, so it cannot be checked against one, so it is refused.
    if (origin == "null") return false;

    for (const std::string& allowed : policy.allowed_origins)
        if (origin == allowed) return true;

    static constexpr std::string_view kLoopback[] = {
        "http://localhost",
        "http://127.0.0.1",
        "http://[::1]",
    };

    for (const std::string_view host : kLoopback) {
        if (origin == host) return true;

        // THE CHARACTER AFTER THE HOST MUST BE A COLON. Without that check
        // `http://localhost.attacker.example` passes a prefix test, and that is
        // precisely the domain an attacker registers to walk through this gate.
        if (origin.size() <= host.size()) continue;
        if (origin.substr(0, host.size()) != host) continue;
        if (origin[host.size()] != ':') continue;
        if (all_digits(origin.substr(host.size() + 1))) return true;
    }
    return false;
}

bool constant_time_equals(std::string_view a, std::string_view b)
{
    // THE LENGTH IS NOT SECRET and cannot be hidden: the answer must be false
    // when the lengths differ. What must not leak is WHERE two equal-length
    // strings first differ, because a client that can time a few thousand
    // requests could otherwise recover the token one byte at a time. So every
    // byte is compared and the differences are OR'd into one answer.
    if (a.size() != b.size()) return false;

    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff =
            static_cast<unsigned char>(static_cast<unsigned>(diff) |
                                       (static_cast<unsigned>(static_cast<unsigned char>(a[i])) ^
                                        static_cast<unsigned>(static_cast<unsigned char>(b[i]))));
    return diff == 0;
}

PathMatch endpoint_match(std::string_view path, const ServerPolicy& policy)
{
    // A query string is stripped and never read. A token does not travel in one
    // (see the header's note on why one travels in the path at all), and no other
    // parameter means anything to this endpoint.
    if (const std::size_t query = path.find('?'); query != std::string_view::npos)
        path = path.substr(0, query);

    while (path.size() > 1 && path.back() == '/')
        path.remove_suffix(1);

    std::string_view base = policy.path;
    while (base.size() > 1 && base.back() == '/')
        base.remove_suffix(1);

    if (path == base) return PathMatch::Bare;

    if (path.size() <= base.size() + 1) return PathMatch::None;
    if (path.substr(0, base.size()) != base) return PathMatch::None;
    if (path[base.size()] != '/') return PathMatch::None;

    const std::string_view rest = path.substr(base.size() + 1);

    // One segment, and only one. `/mcp/<token>/anything` is not this endpoint,
    // and answering it as though it were would make the token a path prefix that
    // a client could hang arbitrary routes off.
    if (rest.find('/') != std::string_view::npos) return PathMatch::None;

    // No token configured, yet a segment arrived: this is not the endpoint. It
    // is deliberately NOT `BadToken`, because a build with no token has no token
    // to be wrong about.
    if (policy.token.empty()) return PathMatch::None;

    return constant_time_equals(rest, policy.token) ? PathMatch::Token : PathMatch::BadToken;
}

std::string_view bearer_token(std::string_view authorization)
{
    static constexpr std::string_view kScheme = "bearer ";
    if (authorization.size() <= kScheme.size()) return {};

    for (std::size_t i = 0; i < kScheme.size(); ++i)
        if (ascii_lower(authorization[i]) != kScheme[i]) return {};

    std::string_view token = authorization.substr(kScheme.size());
    while (!token.empty() && (token.front() == ' ' || token.front() == '\t'))
        token.remove_prefix(1);
    while (!token.empty() && (token.back() == ' ' || token.back() == '\t'))
        token.remove_suffix(1);
    return token;
}

std::string decode_header_value(std::string_view raw)
{
    static constexpr std::string_view kPrefix = "=?base64?";
    static constexpr std::string_view kSuffix = "?=";

    if (raw.size() < kPrefix.size() + kSuffix.size()) return std::string(raw);
    if (raw.substr(0, kPrefix.size()) != kPrefix) return std::string(raw);
    if (raw.substr(raw.size() - kSuffix.size()) != kSuffix) return std::string(raw);

    const std::string_view payload =
        raw.substr(kPrefix.size(), raw.size() - kPrefix.size() - kSuffix.size());

    const std::optional<std::string> decoded = decode_base64(payload);
    return decoded ? *decoded : std::string(raw);
}

bool accepts_event_stream(std::string_view accept)
{
    return accept.find("text/event-stream") != std::string_view::npos;
}

std::string sse_event(std::string_view event, std::string_view data)
{
    std::string out;
    if (!event.empty()) {
        out += "event: ";
        out += event;
        out += '\n';
    }

    // EVERY LINE OF THE PAYLOAD GETS ITS OWN `data:` LINE. A compact JSON dump
    // holds no newline today, but an SSE parser splits on one, and a payload
    // that ever grew a line break would arrive at the client truncated at it.
    std::size_t at = 0;
    while (true) {
        const std::size_t end = data.find('\n', at);
        const std::size_t to  = end == std::string_view::npos ? data.size() : end;
        out += "data: ";
        out += data.substr(at, to - at);
        out += '\n';
        if (end == std::string_view::npos) break;
        at = end + 1;
    }

    out += '\n';
    return out;
}

std::string sse_comment(std::string_view text)
{
    std::string out = ": ";
    out += text;
    out += "\n\n";
    return out;
}

HttpOutcome http_json(int status, const Json& payload)
{
    HttpOutcome out;
    out.status   = status;
    out.delivery = Delivery::Json;
    out.body     = payload.dump();
    out.headers.emplace_back("Content-Type", "application/json");
    return out;
}

HttpOutcome http_accepted()
{
    HttpOutcome out;
    out.status   = 202;
    out.delivery = Delivery::Accepted;
    return out;
}

HttpOutcome http_empty(int status)
{
    HttpOutcome out;
    out.status   = status;
    out.delivery = Delivery::Empty;
    return out;
}

HttpOutcome http_stream(StreamPlan plan)
{
    HttpOutcome out;
    out.status   = 200;
    out.delivery = Delivery::Sse;
    out.stream   = std::move(plan);
    out.headers.emplace_back("Content-Type", "text/event-stream");
    out.headers.emplace_back("Cache-Control", "no-cache");
    out.headers.emplace_back("Connection", "keep-alive");
    // Asked for by the specification, and the reason is worth keeping: a
    // buffering reverse proxy holds every frame until the stream closes, which
    // turns a progress notification into nothing at all and a held-open
    // subscription into a hang.
    out.headers.emplace_back("X-Accel-Buffering", "no");
    return out;
}

} // namespace kentos::ai
