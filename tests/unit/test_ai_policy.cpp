// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the policy engine's decisions, taken apart from any model.
//
// Every case here is a sentence out of TODOS §7.1: the user wrote what the three
// approval modes must do, and these are those rows.
#include "kentos_cad/ai/policy.hpp"

#include <doctest/doctest.h>

using kentos::ai::ApprovalPolicy;
using kentos::ai::ClientScope;
using kentos::ai::decide;
using kentos::ai::OverwritePolicy;
using kentos::ai::PolicyPreferences;
using kentos::ai::Verdict;
using kentos::command::Effect;

namespace {

/// The person at the keyboard: no client, nothing withheld.
ClientScope operator_scope()
{
    ClientScope s;
    s.may_write_external  = true;
    s.may_change_settings = true;
    return s;
}

/// A remote agent as it is handed to the server today: reads and edits, but does
/// not reach off the machine and cannot touch its own policy.
ClientScope agent_scope()
{
    ClientScope s;
    s.client = "deneme-istemcisi";
    return s;
}

PolicyPreferences with(ApprovalPolicy a)
{
    PolicyPreferences p;
    p.approval = a;
    return p;
}

} // namespace

TEST_CASE("Politika: okuma ve görünüm her modda doğrudan çalışır")
{
    // TODOS §7.1, the first two rows: all three columns say "Doğrudan". A read
    // that waits for a person is the thing .claude/ai.md R3 forbids outright.
    for (const ApprovalPolicy mode :
         {ApprovalPolicy::EveryChange, ApprovalPolicy::RiskyOnly, ApprovalPolicy::Automatic}) {
        CHECK_EQ(decide(Effect::Query, with(mode), agent_scope()).verdict, Verdict::Allow);
        CHECK_EQ(decide(Effect::Query | Effect::ViewChange, with(mode), agent_scope()).verdict,
                 Verdict::Allow);
    }
}

TEST_CASE("Politika: geri alınabilir düzenleme modlara göre ayrışır")
{
    // "Geri alınabilir belge/layout düzenleme": one approval per plan under
    // `her_degisiklikte`, straight through under the other two — because a
    // drawing edit is one Ctrl+Z away and the undo stack is the answer to it.
    const Effect edit = Effect::DocumentEdit;
    CHECK_EQ(decide(edit, with(ApprovalPolicy::EveryChange), agent_scope()).verdict,
             Verdict::ApprovalRequired);
    CHECK_EQ(decide(edit, with(ApprovalPolicy::RiskyOnly), agent_scope()).verdict, Verdict::Allow);
    CHECK_EQ(decide(edit, with(ApprovalPolicy::Automatic), agent_scope()).verdict, Verdict::Allow);
}

TEST_CASE("Politika: bu makinenin dışına yazmak riskli moddaki onayı da tetikler")
{
    // A printer and a database are not an undo stack. Under `riskli_islemlerde`
    // this is exactly what "riskli" was supposed to mean.
    PolicyPreferences prefs = with(ApprovalPolicy::RiskyOnly);
    const auto said         = decide(Effect::ExternalWrite, prefs, operator_scope());
    CHECK_EQ(said.verdict, Verdict::ApprovalRequired);
    CHECK(said.reason.find("dışına") != std::string::npos);
}

TEST_CASE("Politika: üzerine yazma 'sor' iken otomatik mod bile sorar")
{
    // `otomatik` is a statement about APPROVAL, not about the overwrite
    // preference. Somebody who chose `sor` chose it on purpose, and a mode that
    // quietly overrode it would make the narrower setting a lie.
    PolicyPreferences prefs = with(ApprovalPolicy::Automatic);
    prefs.overwrite         = OverwritePolicy::Ask;
    CHECK_EQ(decide(Effect::FileWrite, prefs, operator_scope(), {}, /*overwrites=*/true).verdict,
             Verdict::ApprovalRequired);

    // And with `yeni_ad_uret` there is nothing to ask about: the write lands on
    // a name nobody is using.
    prefs.overwrite = OverwritePolicy::FreshName;
    CHECK_EQ(decide(Effect::FileWrite, prefs, operator_scope(), {}, /*overwrites=*/true).verdict,
             Verdict::Allow);
}

TEST_CASE("Politika: kapsam dışı iş onayla açılmaz")
{
    // TODOS §7.1's last row, in all three columns: "Yetki gerekli sonucu; model
    // kendine yetki veremez". `Deny` rather than `ApprovalRequired`, because a
    // client told "ask a human" will ask for ever about something no human can
    // grant from that card.
    for (const ApprovalPolicy mode :
         {ApprovalPolicy::EveryChange, ApprovalPolicy::RiskyOnly, ApprovalPolicy::Automatic}) {
        const auto said = decide(Effect::ExternalWrite, with(mode), agent_scope());
        CHECK_EQ(said.verdict, Verdict::Deny);
        CHECK(said.reason.find("deneme-istemcisi") != std::string::npos);
    }

    // A client cannot widen its own policy either (S-04).
    CHECK_EQ(decide(Effect::SettingsChange, with(ApprovalPolicy::Automatic), agent_scope()).verdict,
             Verdict::Deny);
}

TEST_CASE("Politika: eksik zorunlu girdi onaydan önce gelir")
{
    // There is no call to say yes to yet. Asking for approval first and for the
    // argument afterwards is the loop §7.1 calls "sonsuz soru/yeniden deneme".
    const auto said = decide(Effect::DocumentEdit, with(ApprovalPolicy::EveryChange),
                             operator_scope(), {"kagit"});
    CHECK_EQ(said.verdict, Verdict::InputRequired);
    REQUIRE_EQ(said.missing.size(), std::size_t{1});
    CHECK_EQ(said.missing.front(), "kagit");
    CHECK(said.reason.find("kagit") != std::string::npos);
}

TEST_CASE("Politika: her karar bir gerekçe taşır")
{
    // A decision nobody can explain is a decision nobody can argue with — and it
    // is the one thing an audit record has to be able to print.
    for (const Effect effect : {Effect::Query, Effect::DocumentEdit, Effect::FileWrite,
                                Effect::ExternalWrite, Effect::SettingsChange})
        for (const ApprovalPolicy mode :
             {ApprovalPolicy::EveryChange, ApprovalPolicy::RiskyOnly, ApprovalPolicy::Automatic}) {
            CHECK(!decide(effect, with(mode), operator_scope()).reason.empty());
            CHECK(!decide(effect, with(mode), agent_scope()).reason.empty());
        }
}

TEST_CASE("Politika: tanınmayan ayar sözcüğü en dar davranışa düşer")
{
    // A word nobody recognises means the stored value is from a newer build or
    // was edited by hand, and neither is a licence to widen anything.
    using kentos::ai::approval_policy_from;
    using kentos::ai::overwrite_policy_from;
    using kentos::ai::question_policy_from;
    CHECK_EQ(approval_policy_from("zıpla"), ApprovalPolicy::EveryChange);
    CHECK_EQ(approval_policy_from(""), ApprovalPolicy::EveryChange);
    CHECK_EQ(approval_policy_from("otomatik"), ApprovalPolicy::Automatic);
    CHECK_EQ(overwrite_policy_from("zıpla"), OverwritePolicy::FreshName);
    CHECK_EQ(question_policy_from("zıpla"), kentos::ai::QuestionPolicy::OnlyRequired);

    // And the words round-trip, so the setting and the engine cannot drift.
    using kentos::ai::approval_policy_name;
    for (const ApprovalPolicy mode :
         {ApprovalPolicy::EveryChange, ApprovalPolicy::RiskyOnly, ApprovalPolicy::Automatic})
        CHECK_EQ(approval_policy_from(approval_policy_name(mode)), mode);
}
