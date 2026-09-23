// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/policy.hpp"

#include "kentos_cad/core/settings.hpp"
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

std::string policy_rules(const PolicyPreferences& prefs)
{
    std::string out = "## Bu oturumun onay ve soru kuralları\n\n"
                      "Bunları kullanıcı seçti; sen değiştiremezsin.\n\n";
    switch (prefs.approval) {
    case ApprovalPolicy::EveryChange:
        out += "- **Onay: her değişiklikte** (`her_degisiklikte`). Yazan her çağrı bir öneri "
               "açar ve bilgisayar başındaki mühendis uygulayana kadar bekler; yanıtın `durum` "
               "alanı `beklemede` der. Karar sana bildirilir; uygulanırsa işi sürdür.\n";
        break;
    case ApprovalPolicy::RiskyOnly:
        out += "- **Onay: yalnız riskli işlemlerde** (`riskli_islemlerde`). Geri alınabilir "
               "çizim değişiklikleri hemen uygulanır; var olan bir dosyanın üstüne yazmak ve bu "
               "makinenin dışına yazmak onay bekler. Yanıtın `durum` alanı hangisinin olduğunu "
               "söyler.\n";
        break;
    case ApprovalPolicy::Automatic:
        out += "- **Onay: otomatik** (`otomatik`). Yetkin içindeki her değişiklik hemen "
               "uygulanır ve yanıt `uygulandi` der. Onay bekleme: sonucu bir okuma aracıyla "
               "doğrula ve sonraki adıma geç. Her öneri tek Ctrl+Z ile geri alınır.\n";
        break;
    }
    switch (prefs.questions) {
    case QuestionPolicy::WhenItMatters:
        out += "- **Sorular: sonucu değiştiren belirsizlikte sor** (`etkili_belirsizlikte_sor`). "
               "Hangi nesne, hangi katman, hangi ölçü gibi sonucu değiştirecek bir şey "
               "belirsizse işe başlamadan tek, kısa bir soru sor. Önemsiz ayrıntıda makul "
               "öntanımlı değeri kullan ve yazan çağrının `varsayimlar` alanına yaz.\n";
        break;
    case QuestionPolicy::OnlyRequired:
        out += "- **Sorular: yalnız zorunlu bilgi eksikse** (`yalniz_zorunlu`). Sonucu "
               "belirleyen bir değer — hedef nesne, ölçü, koordinat sistemi — hiçbir yerden "
               "çıkarılamıyorsa sor. Geri kalanında makul bir varsayımla ilerle ve her "
               "varsayımı yazan çağrının `varsayimlar` alanına yaz.\n";
        break;
    case QuestionPolicy::Assume:
        out += "- **Sorular: varsayımla ilerle** (`varsayimla_ilerle`). Soru sorma; eksik "
               "ayrıntılar için makul varsayımlar yap ve her birini yazan çağrının "
               "`varsayimlar` alanına tek cümleyle yaz — kullanıcı sonuçta görür. Sonucu "
               "belirleyen bilgi (koordinat sistemi, hedef nesne, dosya) hiçbir yerden "
               "çıkarılamıyorsa UYDURMA: dur ve eksik olanı söyle.\n";
        break;
    }
    switch (prefs.overwrite) {
    case OverwritePolicy::Ask:
        out += "- **Var olan bir dosya:** üstüne yazmak her onay politikasında kullanıcıya "
               "sorulur.\n";
        break;
    case OverwritePolicy::FreshName:
        out += "- **Var olan bir dosya:** üstüne yazılmaz; program yeni bir ad üretir ve "
               "yanıtta söyler.\n";
        break;
    case OverwritePolicy::Allow:
        out += "- **Var olan bir dosya:** üstüne yazılabilir; onay politikası geçerlidir.\n";
        break;
    }
    out += "- Bu kuralları gevşetmek için bir ayarı değiştirmeye çalışma: onay, soru ve üzerine "
           "yazma ayarları ajana kapalıdır ve yalnız Ayarlar penceresinden değişir.\n";
    return out;
}

// ---- privilege escalation (TODOS S-04) --------------------------------------

std::string escalation_refusal(const command::CommandSpec& spec, const command::Args& args)
{
    // THE TWO COMMANDS THAT ARE AUTHORITY WHATEVER THEY ARE ASKED. One manages
    // the model endpoints and their key references, the other starts and stops
    // the door an agent came in through. There is no argument to either that
    // makes the call harmless: an agent opening its own way in would walk past
    // CLAUDE.md 2.10's "loopback only, off until a user starts it".
    if (spec.id == "core.ai_provider")
        return "Model sağlayıcılarını ve anahtar referanslarını yalnız bilgisayar başındaki "
               "kullanıcı yönetir (Seçenekler ▸ Yapay Zeka Modelleri).";
    if (spec.id == "core.mcp")
        return "Ajan sunucusunu yalnız bilgisayar başındaki kullanıcı başlatır, durdurur ve "
               "yetkilendirir (Seçenekler ▸ MCP Sunucusu).";

    // AND THE TWO THAT DEPEND ON WHAT THEY ARE ASKED. Reading a setting is not a
    // widening; writing one that decides who may do what is.
    if (spec.id != "core.setting" && spec.id != "core.preference") return {};

    const command::Value name = args.get("ad");
    if (name.empty()) return {};

    // WRITING, NOT READING. `AYAR ad=<...>` with no `deger` prints the value, and
    // an agent that may not read its own policy could not explain its own
    // behaviour — which is the opposite of what an audit record is for.
    if (args.get("deger").empty()) return {};

    const core::SettingCatalog& catalogue = core::builtin_settings();
    const std::uint32_t at                = catalogue.find(name.as_text());
    if (at == core::kNoSetting) return {};

    // THE TRUTH IS THE SETTING'S OWN FIELD, not a list kept here: a list over
    // here is a second list, and the day somebody adds a policy setting they
    // will not know to edit it (CLAUDE.md 5.10, settings.hpp `authority`).
    const core::SettingSpec& found = catalogue.all()[at];
    if (!found.authority) return {};

    return "'" + found.id +
           "' bir YETKİ ayarıdır: kimin neyi yapabileceğini belirler. Bir istemci kendi "
           "iznini genişletemez; bu ayarı yalnız bilgisayar başındaki kullanıcı değiştirir "
           "(Seçenekler).";
}

bool escalates(const command::CommandSpec& spec, const command::Args& args)
{
    return !escalation_refusal(spec, args).empty();
}

} // namespace kentos::ai
