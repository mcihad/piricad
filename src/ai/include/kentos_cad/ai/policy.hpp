// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the decision, taken apart from the model that asked.
//
// WHAT THIS IS FOR. A plan arrives from somewhere — the chat, an MCP client, a
// replayed journal — and something has to answer one question about each step:
// may this run now, does a person have to say yes first, is something missing,
// or is it out of bounds entirely. Today that answer is spread across the gate,
// the dispatcher, the chat loop and a prompt, and four places that answer the
// same question will eventually answer it four ways.
//
// SO IT IS ONE PURE FUNCTION. `decide` takes the effect (`command::effect_of`),
// the user's stated preferences and the client's scope, and returns a
// `PolicyDecision`. No model, no Qt, no clock, no I/O: the same inputs give the
// same answer in the chat, over the socket and in a test — which is what TODOS
// S-01 asks for when it says the chat, MCP and a reconnection must all reach
// the same decision from the same inputs.
//
// WHAT THIS IS NOT. It does not apply anything and it does not hold the
// authority to. `Gate::apply` still demands an `Approval` that only a person at
// the workstation can produce (CLAUDE.md 5.7, ai.md R3/P1); this function tells
// the caller WHETHER it needs one. Wiring a decision of `Allow` straight through
// to an application is a separate change that amends 5.7, and it is not made
// here.
#pragma once

#include "kentos_cad/command/spec.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::ai {

/// What the engine decided about one step.
enum class Verdict : std::uint8_t {
    Allow,            ///< inside the stated policy and the client's scope
    ApprovalRequired, ///< a person has to say yes to this one
    InputRequired,    ///< something the caller must supply is missing
    Deny,             ///< outside the client's scope; no approval can widen it
};

/// The verdict's Turkish word, for a client's answer and for the card.
const char* verdict_name(Verdict v);

/// HOW OFTEN A PERSON IS ASKED TO SAY YES.
///
/// `core.ai.onay_politikasi`. The three words are the user's, from TODOS §7.1.
enum class ApprovalPolicy : std::uint8_t {
    EveryChange, ///< `her_degisiklikte` — one approval per plan, for any change
    RiskyOnly,   ///< `riskli_islemlerde` — overwriting and outward acts only
    Automatic,   ///< `otomatik` — anything the scope already permits
};

/// HOW OFTEN A PERSON IS ASKED A QUESTION.
///
/// `core.ai.soru_politikasi`. Separate from approval on purpose: "do not ask me
/// what paper size" and "ask me before you overwrite" are different wishes, and
/// one control for both is how a user ends up unable to have either.
enum class QuestionPolicy : std::uint8_t {
    WhenItMatters, ///< `etkili_belirsizlikte_sor`
    OnlyRequired,  ///< `yalniz_zorunlu`
    Assume,        ///< `varsayimla_ilerle`
};

/// WHAT TO DO WHEN THE FILE IS ALREADY THERE. `core.ai.uzerine_yazma`.
enum class OverwritePolicy : std::uint8_t {
    Ask,       ///< `sor`
    FreshName, ///< `yeni_ad_uret`
    Allow,     ///< `izin_ver`
};

/// The preferences, as the engine reads them.
struct PolicyPreferences
{
    /// How often a person is asked to say yes. `core.ai.onay_politikasi`.
    ApprovalPolicy approval{ApprovalPolicy::EveryChange};

    /// How often a person is asked a question. `core.ai.soru_politikasi`.
    QuestionPolicy questions{QuestionPolicy::OnlyRequired};

    /// What to do when the file is already there. `core.ai.uzerine_yazma`.
    OverwritePolicy overwrite{OverwritePolicy::FreshName};
};

/// WHAT THIS PARTICULAR CALLER IS ALLOWED TO REACH.
///
/// Scope is not a preference: a preference says how often to ask the person, and
/// scope says what the answer may be about. A client cannot widen its own scope
/// by any answer, which is why `Deny` is not `ApprovalRequired` (TODOS S-04).
struct ClientScope
{
    /// The authenticated client, as the audit record will name it. Empty means
    /// the person at the keyboard, who is not scoped.
    std::string client;

    /// Whether this caller may touch the document at all.
    bool may_edit_document{true};

    /// Whether it may read files the user names.
    bool may_read_files{true};

    /// Whether it may write files.
    bool may_write_files{true};

    /// Whether it may reach off this machine — a database, a printer, a peer.
    bool may_write_external{false};

    /// Whether it may change stored preferences. A client that could would be
    /// able to widen its own policy, which S-04 forbids.
    bool may_change_settings{false};
};

/// One decision, with the reason it was reached.
struct PolicyDecision
{
    /// What was decided. The cautious one until something says otherwise.
    Verdict verdict{Verdict::ApprovalRequired};

    /// Why, in Turkish, for the card, the client's answer and the audit record.
    /// Never empty: a decision nobody can explain is a decision nobody can argue
    /// with.
    std::string reason;

    /// Which effect bits drove it, so a caller can say "because it writes a file"
    /// rather than "because policy".
    command::Effect because{command::Effect::None};

    /// What is missing, when the verdict is `InputRequired`.
    std::vector<std::string> missing;
};

/// THE ONE ANSWER.
///
/// `effect` is what this call leaves changed — `command::effect_of(spec, args)`,
/// never a guess taken from the command's name or category at the call site.
/// `missing` names required arguments the caller could not supply; when it is
/// non-empty the verdict is `InputRequired` whatever the preferences say,
/// because there is nothing to approve yet.
///
/// `overwrites` says the write would land on a file that already exists. It is
/// asked of the caller rather than of the filesystem so that this stays pure:
/// the same inputs give the same decision in a test with no disk.
PolicyDecision decide(command::Effect effect, const PolicyPreferences& prefs,
                      const ClientScope& scope, const std::vector<std::string>& missing = {},
                      bool overwrites = false);

/// The setting words, so `core.ai.onay_politikasi` and the engine cannot drift
/// apart. Each returns the policy for a word, or the default when it is unknown.
ApprovalPolicy approval_policy_from(std::string_view word);
QuestionPolicy question_policy_from(std::string_view word);
OverwritePolicy overwrite_policy_from(std::string_view word);

/// The inverse, for writing a setting back and for the audit record.
const char* approval_policy_name(ApprovalPolicy p);
const char* question_policy_name(QuestionPolicy p);
const char* overwrite_policy_name(OverwritePolicy p);

} // namespace kentos::ai
