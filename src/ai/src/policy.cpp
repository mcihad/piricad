// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/policy.hpp"

#include "kentos_cad/core/text.hpp"

namespace kentos::ai {

using command::Effect;
using command::has_effect;

const char* verdict_name(Verdict v)
{
    switch (v) {
    case Verdict::Allow: return "izin";
    case Verdict::ApprovalRequired: return "onay_gerekli";
    case Verdict::InputRequired: return "girdi_gerekli";
    case Verdict::Deny: return "yetki_yok";
    }
    return "bilinmiyor";
}

const char* approval_policy_name(ApprovalPolicy p)
{
    switch (p) {
    case ApprovalPolicy::EveryChange: return "her_degisiklikte";
    case ApprovalPolicy::RiskyOnly: return "riskli_islemlerde";
    case ApprovalPolicy::Automatic: return "otomatik";
    }
    return "her_degisiklikte";
}

const char* question_policy_name(QuestionPolicy p)
{
    switch (p) {
    case QuestionPolicy::WhenItMatters: return "etkili_belirsizlikte_sor";
    case QuestionPolicy::OnlyRequired: return "yalniz_zorunlu";
    case QuestionPolicy::Assume: return "varsayimla_ilerle";
    }
    return "yalniz_zorunlu";
}

const char* overwrite_policy_name(OverwritePolicy p)
{
    switch (p) {
    case OverwritePolicy::Ask: return "sor";
    case OverwritePolicy::FreshName: return "yeni_ad_uret";
    case OverwritePolicy::Allow: return "izin_ver";
    }
    return "yeni_ad_uret";
}

namespace {

/// THE DEFAULT IS THE CAUTIOUS ONE. An unreadable setting must not quietly widen
/// what a client may do; a word nobody recognises means the stored value is from
/// a newer build or was edited by hand, and neither is a licence.
bool word_is(std::string_view given, const char* known)
{
    return core::turkish_key_equals(std::string(given), known);
}

} // namespace

ApprovalPolicy approval_policy_from(std::string_view word)
{
    if (word_is(word, "otomatik")) return ApprovalPolicy::Automatic;
    if (word_is(word, "riskli_islemlerde")) return ApprovalPolicy::RiskyOnly;
    return ApprovalPolicy::EveryChange;
}

QuestionPolicy question_policy_from(std::string_view word)
{
    if (word_is(word, "varsayimla_ilerle")) return QuestionPolicy::Assume;
    if (word_is(word, "etkili_belirsizlikte_sor")) return QuestionPolicy::WhenItMatters;
    return QuestionPolicy::OnlyRequired;
}

OverwritePolicy overwrite_policy_from(std::string_view word)
{
    if (word_is(word, "izin_ver")) return OverwritePolicy::Allow;
    if (word_is(word, "sor")) return OverwritePolicy::Ask;
    return OverwritePolicy::FreshName;
}

PolicyDecision decide(Effect effect, const PolicyPreferences& prefs, const ClientScope& scope,
                      const std::vector<std::string>& missing, bool overwrites)
{
    PolicyDecision out;
    out.because = effect;

    // ---- 1. NOTHING TO DECIDE YET ------------------------------------------
    //
    // A missing required argument is not a thing a person can approve: there is
    // no call to say yes to. Asking for approval first and for the argument
    // afterwards is the loop TODOS §7.1 calls "sonsuz soru/yeniden deneme".
    if (!missing.empty()) {
        out.verdict = Verdict::InputRequired;
        out.missing = missing;
        out.reason  = "Zorunlu girdi eksik: " + missing.front();
        for (std::size_t i = 1; i < missing.size(); ++i)
            out.reason += ", " + missing[i];
        return out;
    }

    // ---- 2. SCOPE, WHICH NO ANSWER WIDENS ----------------------------------
    //
    // A client cannot approve its own way past its scope, and neither can the
    // person: approving is saying yes to THIS call, not granting an access the
    // caller was not given (TODOS S-04). That is why this is `Deny` and not
    // `ApprovalRequired` — the two mean different things to a client, and a
    // client told "ask a human" will ask, forever, about something no human can
    // grant from that card.
    const auto denied = [&](const char* what) {
        out.verdict = Verdict::Deny;
        out.reason  = std::string(what) + " bu istemcinin yetki kapsamı dışında";
        if (!scope.client.empty()) out.reason += " (" + scope.client + ")";
        out.reason += ".";
        return out;
    };
    if (has_effect(effect, Effect::DocumentEdit) && !scope.may_edit_document)
        return denied("Belgeyi değiştirmek");
    if (has_effect(effect, Effect::FileRead) && !scope.may_read_files)
        return denied("Dosya okumak");
    if (has_effect(effect, Effect::FileWrite) && !scope.may_write_files)
        return denied("Dosya yazmak");
    if (has_effect(effect, Effect::ExternalWrite) && !scope.may_write_external)
        return denied("Bu makinenin dışına yazmak");
    if (has_effect(effect, Effect::SettingsChange) && !scope.may_change_settings)
        return denied("Ayar değiştirmek");

    // ---- 3. A READ IS A READ -----------------------------------------------
    //
    // Reading and moving the view leave nothing changed, so they run under every
    // policy including the strictest. `.claude/ai.md` R3 is explicit that a read
    // must never wait for a person, and TODOS §7.1's table says the same in all
    // three columns.
    const bool changes_something =
        has_effect(effect, Effect::DocumentEdit) || has_effect(effect, Effect::FileWrite) ||
        has_effect(effect, Effect::ExternalWrite) || has_effect(effect, Effect::SettingsChange);
    if (!changes_something) {
        out.verdict = Verdict::Allow;
        out.reason = has_effect(effect, Effect::ViewChange) ? "Görünümü değiştirir, belgeyi değil."
                                                            : "Yalnız okur.";
        return out;
    }

    // ---- 4. WHAT COUNTS AS RISKY -------------------------------------------
    //
    // Not "anything that changes something": a drawing edit is one Ctrl+Z away
    // and the undo stack is the product's own answer to it. Risky is what an
    // undo cannot reach — a file written over, and anything that leaves this
    // machine. Overwriting is separately governed, because `yeni_ad_uret` turns
    // the whole question into a filename choice (TODOS §7.1).
    const bool outward          = has_effect(effect, Effect::ExternalWrite);
    const bool destructive      = overwrites && prefs.overwrite != OverwritePolicy::Allow;
    const bool touches_settings = has_effect(effect, Effect::SettingsChange);

    const auto needs_approval = [&](const char* why) {
        out.verdict = Verdict::ApprovalRequired;
        out.reason  = why;
        return out;
    };

    switch (prefs.approval) {
    case ApprovalPolicy::EveryChange: return needs_approval("Her değişiklikte onay isteniyor.");

    case ApprovalPolicy::RiskyOnly:
        if (outward)
            return needs_approval("Bu makinenin dışına yazar; geri alma yığını oraya ulaşmaz.");
        if (destructive)
            return needs_approval(
                prefs.overwrite == OverwritePolicy::Ask
                    ? "Var olan bir dosyanın üstüne yazar."
                    : "Var olan bir dosyanın üstüne yazar (yeni ad üretilebilir).");
        if (touches_settings) return needs_approval("Saklanan bir ayarı değiştirir.");
        out.verdict = Verdict::Allow;
        out.reason  = "Geri alınabilir bir değişiklik; riskli işlemlerde onay seçili.";
        return out;

    case ApprovalPolicy::Automatic:
        // EVEN HERE, OVERWRITING IS ASKED WHEN THE USER ASKED FOR IT. `otomatik`
        // is a statement about approval, not about the overwrite preference:
        // somebody who chose `sor` for overwriting chose it on purpose, and a
        // mode that quietly overrode it would make the narrower setting a lie.
        if (overwrites && prefs.overwrite == OverwritePolicy::Ask)
            return needs_approval("Var olan bir dosyanın üstüne yazar ve üzerine yazma "
                                  "tercihi 'sor'.");
        out.verdict = Verdict::Allow;
        out.reason  = "Otomatik mod; yetki kapsamı içinde ve girdileri tam.";
        return out;
    }

    return needs_approval("Bilinmeyen onay politikası; en dar davranış uygulandı.");
}

} // namespace kentos::ai
