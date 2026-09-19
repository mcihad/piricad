// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — the policy engine's decisions, taken apart from any model.
//
// Every case here is a sentence out of TODOS §7.1: the user wrote what the three
// approval modes must do, and these are those rows.
#include "kentos_cad/ai/policy.hpp"

#include "kentos_cad/core/settings.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

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

TEST_CASE("S-04: bir istemci kendi iznini genişletemez")
{
    using kentos::ai::escalates;
    using kentos::ai::escalation_refusal;
    using kentos::command::Args;
    using kentos::command::CommandSpec;
    using kentos::command::Value;

    // THE FAILURE THIS GUARD EXISTS FOR, in TODOS S-04's own words: a model that
    // hits a refusal and, trying to be helpful, turns the approval policy to
    // `otomatik` so the refusal goes away. It is not malice; it is an agent
    // removing an obstacle. The answer is that the obstacle is out of reach.
    CommandSpec pref;
    pref.id = "core.preference";

    Args widen;
    widen.set("ad", Value::text("onay_politikası"));
    widen.set("deger", Value::text("otomatik"));
    CHECK(escalates(pref, widen));
    // THE REFUSAL NAMES THE SETTING AND WHO MAY CHANGE IT. "Yetkiniz yok" sends
    // an agent round the houses; this ends the attempt.
    CHECK(escalation_refusal(pref, widen).find("core.ai.onay_politikasi") != std::string::npos);
    CHECK(escalation_refusal(pref, widen).find("kullanıcı") != std::string::npos);

    // THE ID WORKS TOO, and so does every declared alias: an agent that wrote
    // the id instead of the Turkish name must not slip through.
    Args by_id;
    by_id.set("ad", Value::text("core.ai.onay_politikasi"));
    by_id.set("deger", Value::text("otomatik"));
    CHECK(escalates(pref, by_id));

    Args ascii;
    ascii.set("ad", Value::text("approval_policy"));
    ascii.set("deger", Value::text("otomatik"));
    CHECK(escalates(pref, ascii));

    // READING IS NOT WIDENING. An agent that could not read its own policy could
    // not explain its own behaviour, which is the opposite of what an audit
    // record is for.
    Args read;
    read.set("ad", Value::text("onay_politikası"));
    CHECK_FALSE(escalates(pref, read));

    // AN ORDINARY PREFERENCE IS ORDINARY. The guard must not become a second,
    // vaguer ban on settings: the distinction is authority, not importance.
    Args ordinary;
    ordinary.set("ad", Value::text("core.ai.dusunme_goster"));
    ordinary.set("deger", Value::text("evet"));
    CHECK_FALSE(escalates(pref, ordinary));

    // EVERY SETTING THAT DECIDES WHO MAY DO WHAT, by id, so a future addition
    // that forgets the flag shows up here as a failing row rather than as a
    // hole. The list is the TEST's; the truth is `SettingSpec::authority`.
    for (const char* id :
         {"core.ai.onay_politikasi", "core.ai.soru_politikasi", "core.ai.uzerine_yazma",
          "core.ai.hassas", "core.mcp.port", "core.mcp.belirtec_zorunlu", "core.mcp.otomatik"}) {
        CAPTURE(id);
        Args one;
        one.set("ad", Value::text(id));
        one.set("deger", Value::text("1"));
        CHECK(escalates(pref, one));

        CommandSpec project;
        project.id = "core.setting";
        CHECK(escalates(project, one));
    }

    // AND THE TWO COMMANDS THAT ARE AUTHORITY WHATEVER THEY ARE ASKED: an agent
    // opening its own way in would walk past CLAUDE.md 2.10's "loopback only,
    // off until a user starts it".
    CommandSpec server;
    server.id = "core.mcp";
    CHECK(escalates(server, Args{}));

    CommandSpec provider;
    provider.id = "core.ai_provider";
    CHECK(escalates(provider, Args{}));

    // A DRAWING COMMAND IS NOT ONE. The guard is narrow by design: it refuses a
    // widening, not "anything that sounds administrative".
    CommandSpec line;
    line.id = "core.line";
    CHECK_FALSE(escalates(line, Args{}));

    // ---- AND IT IS NOT A VERDICT THE PREFERENCES CAN REACH -----------------
    //
    // `decide` answers "ask, allow or refuse"; a widening never becomes allowed
    // however the preferences are set. That is why the two are separate
    // functions and why `Deny` is not `ApprovalRequired`.
    const kentos::ai::PolicyDecision automatic =
        decide(Effect::SettingsChange, with(ApprovalPolicy::Automatic), agent_scope());
    CHECK_EQ(automatic.verdict, Verdict::Deny);
}

TEST_CASE("S-04: yeni bir yetki ayarı işaretsiz eklenemez")
{
    // THE HOLE A LIST OF IDS CANNOT CLOSE. The case above names the seven
    // settings that decide who may do what, so losing the flag on one of them
    // fails loudly. What it cannot catch is the EIGHTH — a policy setting added
    // next year whose author never heard of `SettingSpec::authority`.
    //
    // So this case comes at it from the other side: inside the two namespaces
    // where authority lives, every setting is authority UNLESS it is named here
    // as a preference. A new `core.ai.` or `core.mcp.` setting therefore fails
    // this test on the day it is written, and whoever wrote it decides which
    // kind it is — which is the decision, made once, in the open.
    const std::vector<std::string> known_preferences = {
        // What the chat panel shows, which changes nothing about permission.
        "core.ai.dusunme_goster",
        // Who signs the audit record. It is a NAME, and naming yourself is not
        // widening: a wrong name is a wrong record, not a wider permission.
        "core.ai.sorumlu",
    };

    for (const kentos::core::SettingSpec& spec : kentos::core::builtin_settings().all()) {
        const bool in_scope =
            spec.id.rfind("core.ai.", 0) == 0 || spec.id.rfind("core.mcp.", 0) == 0;
        if (!in_scope) continue;

        const bool excused = std::find(known_preferences.begin(), known_preferences.end(),
                                       spec.id) != known_preferences.end();
        CAPTURE(spec.id);
        const bool decided = spec.authority || excused;
        CHECK_MESSAGE(decided, "yeni bir yetki ayarı: ya `.authority = true` yazın ya da bu "
                               "testteki tercih listesine gerekçesiyle ekleyin -> "
                                   << spec.id);
        const bool both = spec.authority && excused;
        CHECK_MESSAGE(both == false, "hem yetki hem tercih olamaz -> " << spec.id);
    }
}
