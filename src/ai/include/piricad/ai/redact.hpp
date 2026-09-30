// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: nothing secret reaches a log, a message or a transcript.
//
// THE RULE. `.claude/ai.md` P11: "NEVER write API keys, tokens or endpoint
// credentials into the audit record, the `Journal`, or the repository." CLAUDE.md
// says the same of a settings value and a log line. Those are prohibitions on
// OUTPUT, and output happens in a dozen places — a failed request's error
// message, a debug log of the headers that were sent, an audit record quoting the
// endpoint, a transcript the user exports to send to us. A rule enforced at a
// dozen call sites is a rule that will be missed at the thirteenth, so there is
// one funnel and everything that leaves goes through it.
//
// TWO TESTS, NOT ONE. By HEADER NAME, because `Authorization` carries a
// credential whatever it looks like; and by SHAPE, because a key gets pasted into
// places no header name covers — a URL query, a JSON body a gateway wants it in,
// an error string the provider echoed back. Neither test alone is enough and the
// shape test is deliberately eager: a false positive costs a masked hash in a log
// line, a false negative costs a leaked key.
//
// IT RUNS ON THE WAY OUT, NEVER ON THE WAY TO THE PROVIDER. The Anthropic
// `signature` and the Responses `encrypted_content` are long opaque base64 blobs
// that the shape test will happily mask — and they MUST be replayed byte-identically
// or the next call is a 400 (dialect.hpp). So redaction belongs on the logging
// path only. A redacted request is for reading, and it is never sent.
#pragma once

#include "kentos_cad/ai/transport.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// What replaces a secret. Not Turkish and not English: this string ends up in
/// log lines, audit records and pasted transcripts, and it should read the same
/// way in all of them.
inline constexpr std::string_view kMask = "***";

/// Whether a header of this name carries a credential — matched
/// case-insensitively over ASCII, because a header name is ASCII and a provider
/// may spell it `X-Api-Key` or `x-api-key` on different days.
///
/// The list is the one every provider surveyed actually uses: `Authorization`,
/// `x-api-key` (Anthropic), `api-key` (Azure-shaped gateways),
/// `Proxy-Authorization`, `Cookie` and `Set-Cookie` (a session is a credential),
/// plus the `x-goog-api-key` a Google endpoint accepts.
bool is_secret_header(std::string_view name);

/// Whether `text` looks like a credential rather than a name, a model id or a
/// sentence.
///
/// Two grounds: a known vendor PREFIX (`sk-`, `sk-ant-`, `sk-or-v1-`, `gsk_`,
/// `xai-`, `AIza`, `hf_`, `ghp_`, `Bearer …`), or a long unbroken run of
/// base64/hex characters mixing letters and digits, which is what every token
/// without a prefix looks like. `upsert` calls this on `ProviderProfile::key_ref`
/// so that a user who pasted the key where the keychain ENTRY NAME belongs is
/// told, instead of having the secret written into their settings file.
bool looks_like_secret(std::string_view text);

/// `text` with every credential-shaped run masked, and with the value of any
/// obvious key-bearing JSON member (`api_key`, `apiKey`, `token`, `authorization`,
/// `password`, `secret`) masked as well.
///
/// It is NOT a JSON parser and does not need to be: it works on bytes so that it
/// can also clean an error body that is HTML, a URL with the key in the query, or
/// a provider's prose. Ordinary text passes through unchanged.
std::string redact_text(std::string_view text);

/// The headers as they may be logged: a secret-named header keeps its NAME — that
/// a request carried an `Authorization` is exactly what a reader debugging a 401
/// needs to know — and loses its value.
std::vector<std::pair<std::string, std::string>>
redact_headers(const std::vector<std::pair<std::string, std::string>>& headers);

/// The request as it may be logged or quoted in an audit record: headers masked
/// by name, and the URL and body run through `redact_text` for anything that got
/// past the header test.
HttpRequest redact_request(HttpRequest request);

/// One line naming a request, for a log: method, URL and header names only, never
/// a body. The body of a chat request is the drawing's context, and ai.md P9
/// forbids dumping that anywhere it is not needed.
std::string describe_request(const HttpRequest& request);

} // namespace kentos::ai
