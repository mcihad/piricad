// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/redact.hpp"

#include <array>
#include <span>
#include <string>

namespace kentos::ai {
namespace {

/// ASCII lower-casing. ASCII ONLY, and deliberately not `core::turkish_fold_key`:
/// everything this file matches — header names, JSON member names, vendor key
/// prefixes — is ASCII by protocol, and folding Turkish letters into it would
/// make `anahtarı` match `anahtari` in a place where no Turkish word belongs.
std::string ascii_lower(std::string_view text)
{
    std::string out(text);
    for (char& ch : out)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return out;
}

/// The characters a token is made of. `.` and `/` are DELIBERATELY ABSENT: with
/// them, `https://api.openai.com/v1/chat/completions` is one 42-character run
/// containing letters and digits, and the length test would mask every URL in
/// every log line. Without them a JWT is masked segment by segment, which is the
/// same outcome for the reader.
bool is_token_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-' || ch == '+' || ch == '=';
}

/// The vendor prefixes a credential is recognisable by, as of the providers
/// `provider.cpp` ships profiles for.
constexpr std::array<std::string_view, 12> kKeyPrefixes = {
    "sk-",         // OpenAI, DeepSeek, Mistral, llama.cpp convention, and `sk-ant-`/`sk-or-v1-`
    "gsk_",        // Groq
    "xai-",        // xAI
    "aiza",        // Google (`AIza…`), matched lower-cased
    "hf_",         // Hugging Face
    "ghp_",        // GitHub personal token
    "gho_",        //
    "github_pat_", //
    "xoxb-",       // Slack bot token
    "xoxp-",       //
    "akia",        // AWS access key id
    "asia",        //
};

/// JSON member and parameter names whose VALUE is a credential whatever it looks
/// like. `key` alone is absent on purpose: it names the keychain ENTRY in this
/// program's own settings file (`ProviderProfile::key_ref`), and masking that
/// would hide the one field a reader needs to see when a key cannot be found.
constexpr std::array<std::string_view, 12> kSecretNames = {
    "bearer",       "api_key",       "apikey",        "api-key",  "x-api-key", "token",
    "access_token", "refresh_token", "authorization", "password", "secret",    "client_secret",
};

bool contains(std::span<const std::string_view> list, std::string_view word)
{
    for (std::string_view entry : list)
        if (entry == word) return true;
    return false;
}

/// Whether only punctuation that could stand between a member name and its value
/// lies in `between` — `": "`, `"=\""`, `":"`. Anything else means the two runs
/// are unrelated words in a sentence.
bool only_assignment(std::string_view between)
{
    for (char ch : between)
        if (ch != ':' && ch != '=' && ch != '"' && ch != '\'' && ch != ' ' && ch != '\t' &&
            ch != ',' && ch != '(')
            return false;
    return true;
}

} // namespace

bool is_secret_header(std::string_view name)
{
    static constexpr std::array<std::string_view, 7> kHeaders = {
        "authorization", "x-api-key",  "api-key",        "proxy-authorization",
        "cookie",        "set-cookie", "x-goog-api-key",
    };
    const std::string folded = ascii_lower(name);
    return contains(kHeaders, folded);
}

bool looks_like_secret(std::string_view text)
{
    // Trim, because a pasted key arrives with the newline the user copied.
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' ||
                             text.front() == '\r'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' ||
                             text.back() == '\r'))
        text.remove_suffix(1);
    if (text.size() < 8) return false;

    const std::string folded = ascii_lower(text);
    if (folded.rfind("bearer ", 0) == 0) return true;
    for (std::string_view prefix : kKeyPrefixes)
        if (folded.rfind(prefix, 0) == 0) return true;

    // NO PREFIX: judge by shape. A long unbroken run of token characters holding
    // both letters and digits is a token; a Turkish sentence, a model id and a
    // keychain entry name are not.
    if (text.size() < 32) return false;
    bool letter = false;
    bool digit  = false;
    for (char ch : text) {
        if (!is_token_char(ch)) return false;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) letter = true;
        if (ch >= '0' && ch <= '9') digit = true;
    }
    return letter && digit;
}

std::string redact_text(std::string_view text)
{
    std::string out;
    out.reserve(text.size());

    std::string previous_word;    // the last token run seen, lower-cased
    std::size_t previous_end = 0; // where it ended, to inspect what lies between

    std::size_t at = 0;
    while (at < text.size()) {
        if (!is_token_char(text[at])) {
            out.push_back(text[at]);
            ++at;
            continue;
        }
        const std::size_t start = at;
        while (at < text.size() && is_token_char(text[at]))
            ++at;
        const std::string_view run = text.substr(start, at - start);

        const bool named_secret = !previous_word.empty() && contains(kSecretNames, previous_word) &&
                                  only_assignment(text.substr(previous_end, start - previous_end));

        if (named_secret || looks_like_secret(run))
            out.append(kMask);
        else
            out.append(run);

        previous_word = ascii_lower(run);
        previous_end  = at;
    }
    return out;
}

std::vector<std::pair<std::string, std::string>>
redact_headers(const std::vector<std::pair<std::string, std::string>>& headers)
{
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(headers.size());
    for (const auto& [name, value] : headers) {
        if (is_secret_header(name))
            out.emplace_back(name, std::string(kMask));
        else
            // A HEADER THAT IS NOT NAMED AFTER A CREDENTIAL MAY STILL CARRY ONE:
            // a gateway that wants the key in `X-Portal-Auth` is a configuration
            // this program cannot enumerate, so the shape test runs on every
            // value as well.
            out.emplace_back(name, redact_text(value));
    }
    return out;
}

HttpRequest redact_request(HttpRequest request)
{
    request.headers = redact_headers(request.headers);
    // THE URL TOO: a key in a query string is the one place a provider's own
    // documentation sometimes puts it, and CLAUDE.md's privacy rule against
    // credentials in URLs does not stop a gateway's from arriving that way.
    request.url  = redact_text(request.url);
    request.body = redact_text(request.body);
    return request;
}

std::string describe_request(const HttpRequest& request)
{
    std::string out = request.method + " " + redact_text(request.url);
    if (request.headers.empty()) return out;
    out += " [";
    bool first = true;
    for (const auto& [name, value] : request.headers) {
        (void)value;
        if (!first) out += ", ";
        out += name;
        first = false;
    }
    out += "]";
    return out;
}

} // namespace kentos::ai
