// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: which model, on which machine, spoken to in which dialect.
//
// ONE ABSTRACTION, FOUR WIRE LANGUAGES. `.claude/ai.md` R13 requires llama.cpp,
// an in-institution vLLM/Ollama server and a cloud API to sit behind ONE provider
// abstraction chosen by CONFIGURATION, with local support mandatory rather than
// optional. A survey of what the office might actually point this at — OpenAI,
// DeepSeek, Qwen/DashScope, Gemini, Mistral, Groq, OpenRouter, xAI, Anthropic,
// Ollama, llama.cpp, LM Studio, vLLM — produced exactly four wire dialects, not
// thirteen: every one of them but Anthropic and Ollama's native endpoint speaks
// `chat/completions`. So a provider is DATA (this file) and a dialect is CODE
// (dialect.hpp), and adding a vendor is normally a profile, not a class.
//
// THE KEY IS NOT HERE, AND THAT IS THE POINT. A profile carries `key_ref` — the
// NAME of an entry in the operating system's keychain — and never the secret.
// CLAUDE.md and ai.md P11 forbid a credential in a settings value, a journal, a
// log or an audit record, and the cheapest way to keep a rule like that is to
// build a type that cannot hold the thing it forbids. `upsert` goes one step
// further and REFUSES a `key_ref` that looks like a key, because the mistake this
// guards against is a user pasting `sk-…` into the box labelled "anahtar adı".
//
// SENSITIVITY IS A TYPE, NOT AN `if`. ai.md R14/P3 say that when the project is
// marked sensitive, nothing may leave for a cloud provider, "enforced in
// `/src/ai`, not in UI code". `HttpTransport::send` therefore demands an
// `EndpointPermit`, and `permit_for` is the only thing in the program that can
// make one. A panel that forgot the check cannot compile a call.
#pragma once

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// Which wire language an endpoint speaks. The set is closed on purpose: it is
/// the number of genuinely different protocols, and a new vendor that speaks one
/// of them needs a profile rather than an enumerator.
enum class Dialect : std::uint8_t {
    OpenAiChat,        ///< `POST {base}/chat/completions`, SSE of `chat.completion.chunk`
    OpenAiResponses,   ///< `POST {base}/responses`, typed SSE events with a sequence number
    AnthropicMessages, ///< `POST {base}/messages`, the fixed block event order
    OllamaNative,      ///< `POST {base}/api/chat`, NDJSON, one object per line
};

/// The dialect's stable wire id, which is what the JSON file stores: `openai_chat`,
/// `openai_responses`, `anthropic_messages`, `ollama_native`.
const char* dialect_id(Dialect dialect);

/// The dialect of that id, or nothing when the id is not one this build knows —
/// a file written by a later version may name a dialect that does not exist yet,
/// and that is a reported profile rather than a crash.
std::optional<Dialect> dialect_from_id(std::string_view id);

/// Every dialect, in the order a chooser lists them.
std::span<const Dialect> dialects();

/// WHAT A DIALECT CAN ACTUALLY DO, so the chat does not send it something it
/// cannot read.
///
/// Every one of these is a FACT ABOUT THE WIRE LANGUAGE, not about a vendor or a
/// model: `ollama_native` frames NDJSON and carries tool calls in
/// `message.tool_calls`, whatever model is behind it. A model that cannot use
/// tools is a different question, and it belongs to the profile (TODOS A-07).
struct DialectCapabilities
{
    /// Whether the dialect can be handed a tool catalogue at all. All four can;
    /// the field exists so a fifth that cannot is describable rather than
    /// silently broken.
    bool tools{true};

    /// Whether a response can be streamed as it is produced.
    bool streaming{true};

    /// Whether an image can be put in a message. The chat's visual preview
    /// (TODOS A-05) offers one only where this is true, and falls back to the
    /// structural report everywhere else.
    bool vision{false};

    /// Whether the dialect has a place for a reasoning/thinking block that is
    /// separate from the answer.
    bool reasoning{false};

    /// Whether it can be asked for a response matching a JSON Schema.
    bool structured_output{false};

    /// Whether it reports token usage the program can trust, as opposed to the
    /// chat's own estimate. `ContextMeter` settles on this when it arrives.
    bool reports_usage{true};
};

/// What that dialect can do. Stated in one place so the chat, the provider
/// dialog and a future eval harness cannot each hold their own opinion.
DialectCapabilities capabilities_of(Dialect dialect);

/// Where a request actually goes, computed from the URL's host and never from
/// anything a profile claims about itself.
///
/// This is the whole mechanism behind ai.md R14. A profile is user-written data,
/// so a field saying "this endpoint is local" is a CLAIM; the host in the URL is
/// a fact. `permit_for` reads the fact.
enum class Reach : std::uint8_t {
    Loopback,       ///< `localhost`, `127.0.0.0/8`, `::1` — the machine in front of the user
    PrivateNetwork, ///< RFC 1918 / RFC 4193 / `.local` / a bare hostname — the institution's own
                    ///< network
    Internet,       ///< anything else: somebody else's computer
};

/// What the host of `url` says about where the request goes. An unparsable URL is
/// `Internet`, because the safe answer to "I cannot tell" is "assume it leaves".
Reach reach_of(std::string_view url);

/// Turkish for a reach, for the chooser and for a refusal message.
const char* reach_label(Reach reach);

/// Which reasoning knob the endpoint actually has.
///
/// A BOOLEAN WOULD BE A LIE. The four families do not merely spell one setting
/// differently, they take different settings: OpenAI chat, Groq and Gemini take a
/// `reasoning_effort` word; the Responses API takes an object with an effort AND a
/// summary verbosity; Anthropic takes a token BUDGET and rejects a temperature
/// beside it; Qwen takes a switch plus a budget. A profile that stored "reasoning:
/// on" would have to guess the rest, and guessing produces a 400 from a provider
/// the user cannot debug.
enum class ReasoningMode : std::uint8_t {
    None,             ///< send nothing; the model either does not reason or decides for itself
    Effort,           ///< `reasoning_effort: "minimal"|"low"|"medium"|"high"`
    ResponsesSummary, ///< `reasoning: {effort, summary}` plus `include:
                      ///< ["reasoning.encrypted_content"]`
    AnthropicBudget,  ///< `thinking: {type:"enabled", budget_tokens}`
    QwenBudget,       ///< `enable_thinking: true` with `thinking_budget`
};

/// How much thinking to ask for, in whichever currency the provider bills it.
struct Reasoning
{
    ReasoningMode mode{
        ReasoningMode::None}; ///< which knob, and therefore which fields below matter

    /// `Effort` and `ResponsesSummary`: `minimal`, `low`, `medium` or `high`,
    /// passed through as written so a provider that adds a word needs no release.
    std::string effort{"medium"};

    /// `ResponsesSummary`: `auto`, `concise` or `detailed`. The Responses API
    /// streams reasoning as SUMMARY text and never as the raw chain, so this is
    /// the only control over how much of it arrives.
    std::string summary{"auto"};

    /// `AnthropicBudget` and `QwenBudget`: how many tokens the model may spend
    /// thinking. Anthropic requires this to be smaller than `max_tokens`.
    std::int64_t budget_tokens{0};

    /// Whether the reasoning TEXT is shown to the user, which is a separate
    /// question from whether it is requested: a summary is still useful to the
    /// audit record when the panel keeps it folded away.
    bool show_text{false};

    friend bool operator==(const Reasoning&, const Reasoning&) = default;
};

/// The reasoning mode's wire id, as the JSON file stores it.
const char* reasoning_mode_id(ReasoningMode mode);

/// The mode of that id, or nothing when this build does not know it.
std::optional<ReasoningMode> reasoning_mode_from_id(std::string_view id);

/// Who says how big the context window is.
///
/// NOBODY CAN HARD-CODE THIS. OpenAI's `/v1/models` returns an id, an owner and a
/// creation date and NO context length, so a table of windows compiled into this
/// program would be stale the week after it shipped and wrong for every OpenRouter
/// alias. Ollama's `/api/show` does report it. So the number travels with a marker
/// saying where it came from, and the panel can be honest about the difference
/// between "the server told us" and "somebody typed this".
enum class ContextSource : std::uint8_t {
    Unknown,  ///< nobody has said; only the local estimate exists
    Builtin,  ///< this program's shipped starting value for a well-known model
    User,     ///< the operator typed it into the profile
    Reported, ///< the endpoint's own API answered it
};

/// How much the model can hold, and who claims so.
struct ContextWindow
{
    std::int64_t tokens{0};                       ///< 0 means unknown, never "no context"
    ContextSource source{ContextSource::Unknown}; ///< who claims that number

    friend bool operator==(const ContextWindow&, const ContextWindow&) = default;
};

/// The context source's wire id, as the JSON file stores it.
const char* context_source_id(ContextSource source);

/// The Turkish WORDS for that source, for a settings page or a listing.
///
/// SEPARATE FROM THE ID ON PURPOSE. `context_source_id` is the wire form stored
/// in `ai-modelleri.json` and read back by `context_source_from_id`, so it is
/// ASCII and it never changes; this one is read by a person and is spelled the
/// way Turkish is spelled. Showing the wire id to a user is how `gömülü` reached
/// a settings page as `gomulu`.
const char* context_source_label(ContextSource source);

/// The source of that id, or nothing when this build does not know it.
std::optional<ContextSource> context_source_from_id(std::string_view id);

/// One named endpoint: everything needed to speak to a model except the secret.
struct ProviderProfile
{
    std::string name;                     ///< what the user calls it; unique under Turkish folding
    Dialect dialect{Dialect::OpenAiChat}; ///< which wire language this endpoint speaks

    /// Scheme, host, port and any prefix the vendor puts before the endpoint —
    /// `https://api.openai.com/v1`, `http://localhost:11434`. Stored without a
    /// trailing slash; `endpoint_url` joins it to `path`.
    std::string base_url;

    /// The endpoint under the base: `/chat/completions`, `/messages`,
    /// `/api/chat`. A field rather than a constant per dialect because
    /// gateways relocate it — OpenRouter, LiteLLM and a corporate reverse proxy
    /// all serve the same dialect under their own prefix.
    std::string path;

    /// The model id exactly as the endpoint names it. vLLM matches the SERVED
    /// name and refuses anything else, so this cannot be normalised.
    std::string model;

    /// Which header carries the credential — `Authorization` almost everywhere,
    /// `x-api-key` for Anthropic, `api-key` for an Azure-shaped gateway.
    std::string auth_header{"Authorization"};

    /// What precedes the credential in that header, `Bearer` by convention.
    /// EMPTY IS MEANINGFUL: `x-api-key` takes the bare key, and sending
    /// `Bearer` in front of it is a 401 nobody can read.
    std::string auth_scheme{"Bearer"};

    /// Headers the vendor requires beyond the credential. Anthropic's
    /// `anthropic-version` lives here, as does OpenRouter's attribution pair.
    std::vector<std::pair<std::string, std::string>> extra_headers;

    /// Arbitrary JSON merged over the request body, shallowly, LAST — so it wins.
    ///
    /// EVERY PROVIDER SURVEYED HAS AT LEAST ONE NON-STANDARD KNOB: DashScope's
    /// `enable_search`, Groq's `reasoning_format`, OpenRouter's `provider`
    /// routing block, vLLM's `chat_template_kwargs`, Gemini's `safety_settings`.
    /// Without this field each one is a release of this program; with it they are
    /// a line in a settings file.
    core::Json extra_body;

    /// Whether to ask for a streamed answer. OFF is a real configuration: a
    /// corporate proxy that buffers the whole response makes a stream pointless,
    /// and some gateways refuse `stream:true` outright.
    bool stream{true};

    Reasoning reasoning;

    /// The output cap. 0 MEANS "DO NOT SEND THE FIELD", which is how a profile
    /// for a model that rejects it (OpenAI's reasoning models want
    /// `max_completion_tokens` in `extra_body` instead) stays usable. Anthropic
    /// REQUIRES the field, so that dialect substitutes a floor and says so.
    std::int64_t max_tokens{0};

    /// Absent means "do not send a temperature". Not a default of 1.0: the
    /// o-series and gpt-5 REJECT any value but the default, and Anthropic
    /// refuses a temperature beside extended thinking, so "unset" has to be
    /// expressible.
    std::optional<double> temperature;

    ContextWindow context;

    /// Whether the endpoint can take a tool catalogue at all. A profile pointed
    /// at a small local model that has never been tool-trained sets this false,
    /// and the chat then asks in prose instead of sending tools the server will
    /// choke on.
    bool tools{true};

    /// WHICH KEYCHAIN ENTRY HOLDS THE KEY — never the key (ai.md P11). Empty is
    /// correct and common: a local llama.cpp or Ollama needs no credential.
    std::string key_ref;

    /// Field-by-field equality, NOT defaulted: `core::Json` carries no
    /// comparison, so `extra_body` is compared by its serialisation. That is an
    /// exact comparison rather than a heuristic — `Json::dump` is deterministic
    /// and preserves key order (`core/json.hpp`) — and it is what lets a test
    /// prove that the settings file round-trips.
    friend bool operator==(const ProviderProfile& a, const ProviderProfile& b);
};

/// The URL a request goes to: `base_url` and `path` joined with exactly one
/// slash between them.
std::string endpoint_url(const ProviderProfile& profile);

/// One line describing a profile, for the chooser and the listing:
/// `Ollama — ollama_native, http://localhost:11434/api/chat, llama3.1, yerel`.
std::string describe_provider(const ProviderProfile& profile);

/// Permission to speak to one endpoint, which only `permit_for` can grant.
///
/// A VALUE THAT CANNOT BE FORGED. The constructor is private and `permit_for` is
/// the only friend, so a caller cannot reach `HttpTransport::send` without having
/// passed the sensitivity test — the check is in the type system rather than in an
/// `if` somebody can forget (ai.md R14, P3). It carries the resolved URL and
/// dialect so the transport has no reason to re-derive them from a profile that
/// might have been edited in between.
class EndpointPermit
{
public:
    /// The URL this permit was granted for. A transport that sends anywhere else
    /// is a defect, not a configuration.
    const std::string& url() const noexcept { return url_; }

    /// Which dialect the body was written in, so a transport can set the right
    /// `Accept` and frame the response.
    Dialect dialect() const noexcept { return dialect_; }

    /// Where the endpoint sits, as `reach_of` computed it — recorded so the audit
    /// record can say the request stayed inside the institution.
    Reach reach() const noexcept { return reach_; }

private:
    /// Only `permit_for` may mint one.
    EndpointPermit() = default;

    friend core::Result<EndpointPermit> permit_for(const ProviderProfile& profile, bool sensitive);

    std::string url_;
    Dialect dialect_{Dialect::OpenAiChat};
    Reach reach_{Reach::Internet};
};

/// Permission to call `profile`, or the Turkish refusal a panel shows.
///
/// `sensitive` is the project's sensitivity flag (ai.md R14). When it is set,
/// only a loopback or private-network endpoint is permitted, judged by the URL's
/// host — so marking a project sensitive and then pointing a profile called
/// "Yerel" at `api.openai.com` refuses, which is exactly the mistake a
/// UI-level check would have waved through.
core::Result<EndpointPermit> permit_for(const ProviderProfile& profile, bool sensitive);

/// The named endpoints one installation knows, and which one is the default.
///
/// SHAPED AFTER `io::PrintProfiles` deliberately: same `find`/`upsert`/
/// `remove`/`set_default`/`listing` surface, same "exactly one is the default"
/// invariant, same JSON round trip. Two stores of user-named records in one
/// program should not be two different ideas.
///
/// IT DOES NO FILE I/O, which is the one place it departs from that model. This
/// module may not touch the disk (`.claude/ai.md` P10 keeps `/src/ai` free of Qt
/// and of every module that owns a path), so the application reads and writes the
/// file and hands the text to `from_json` / takes it from `to_json`.
class ProviderProfiles
{
public:
    /// The version written into the JSON, and refused when it is from the future.
    ///
    /// `.claude/io.md` P5 — "NEVER write a format without a version field" —
    /// binds every format this program writes, not only the ones under `/src/io`.
    static constexpr std::int64_t kFormatVersion = 1;

    // NO `builtin()`. It used to hold fifteen endpoints as C++ literals; the
    // starting profiles now come from the shipped vendor catalogue through
    // `ai::seed_profiles` (`provider_catalog.hpp`), so a renamed model or a moved
    // base URL is a data release rather than a rebuild — and so that the shipped
    // defaults, the dialog's templates and the model lists are three views of one
    // file instead of three lists that drift (CLAUDE.md 5.10).

    /// Parses the store's JSON text. A version newer than `kFormatVersion` is an
    /// error naming the required application version rather than a silent partial
    /// read (io.md R9's rule, for the same reason).
    static core::Result<ProviderProfiles> from_json(std::string_view text);

    /// The store as the application writes it, pretty-printed because a human
    /// edits this file.
    std::string to_json() const;

    /// Every profile, in the order the user put them in.
    std::span<const ProviderProfile> all() const noexcept { return profiles_; }

    bool empty() const noexcept { return profiles_.empty(); }

    /// The profile of that name, Turkish-folded, or null.
    const ProviderProfile* find(std::string_view name) const;

    /// The default profile, or null when the store is empty.
    const ProviderProfile* fallback() const;

    const std::string& default_name() const noexcept { return default_; }

    /// Adds `p`, or replaces the profile of the same name in place. Refuses an
    /// empty name, model or URL, a URL that is not `http`/`https`, a path that
    /// does not start with a slash, a negative cap or budget, a temperature
    /// outside 0–2 — and a `key_ref` that looks like an actual key, which is
    /// ai.md P11 caught at the door.
    core::Status upsert(ProviderProfile p);

    /// Removes the profile of that name. Refuses the last one — a chat with no
    /// endpoint cannot be configured back into existence from inside itself — and
    /// moves the default when the default went.
    core::Status remove(std::string_view name);

    /// Makes the profile of that name the default. Refuses an unknown name.
    core::Status set_default(std::string_view name);

    /// The Turkish listing, one profile per line, the default marked with `*`.
    std::string listing() const;

private:
    std::vector<ProviderProfile> profiles_;
    std::string default_;
};

} // namespace kentos::ai
