// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/gate.hpp"

namespace kentos::ai {

Gate::Gate(PlanStore& plans, AuditLog& audit, Runner runner)
    : plans_(plans), audit_(audit), runner_(std::move(runner))
{}

Approval Gate::approve(std::string plan_id, std::string operator_name, Decision decision,
                       std::int64_t utc_ms, std::string policy, std::uint64_t content,
                       std::string decided_by)
{
    return Approval(std::move(plan_id), std::move(operator_name), decision, utc_ms,
                    std::move(policy), content, std::move(decided_by));
}

core::Status Gate::decide(const Approval& approval)
{
    Plan* plan = plans_.find(approval.plan_id());
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir öneri yok: '" + approval.plan_id() + "'.");
    if (plan->state != PlanState::Pending)
        return core::err(core::ErrorCode::ValidationFailed, "Öneri '" + approval.plan_id() +
                                                                "' zaten " +
                                                                plan_state_name(plan->state) + ".");

    // ---- IS THIS STILL THE PLAN THE PERSON READ? (TODOS S-04) --------------
    //
    // The card showed a list of command lines and somebody pressed Uygula for
    // THOSE lines. The plan is looked up again here, and between the two moments
    // the client that filed it can have appended a step — that is how a sequence
    // becomes one undo entry (`PlanStore::append_for`). Applying the longer plan
    // would put work nobody read into the drawing, and the audit record would
    // say the engineer approved it.
    //
    // REFUSED, NOT TRIMMED. The honest answer is a fresh card showing what the
    // plan says now; silently applying the prefix would be a different kind of
    // lie.
    if (approval.content() != 0 && approval.content() != plan->content_fingerprint())
        return core::err(core::ErrorCode::Conflict,
                         "Öneri '" + approval.plan_id() +
                             "' karttaki hâlinden farklı: onaydan sonra adım eklenmiş. "
                             "Uygulanmadı; kartı kapatıp yeniden açın.");

    AuditRecord record;
    record.plan_id       = plan->id;
    record.prompt        = plan->prompt;
    record.model         = plan->model;
    record.endpoint      = plan->endpoint;
    record.requester     = plan->requester;
    record.operator_name = approval.operator_name();
    record.utc_ms        = approval.utc_ms();
    record.policy        = approval.policy();

    // WHO OR WHAT DECIDED. An `Approval` is made by two callers and no third
    // (`scripts/ci-gate-ai.sh`): the suggestion card, for a person, and the
    // policy road, which names the policy (`Approval::decided_by`). An automatic
    // application must never read as though somebody clicked (TODOS S-06), so
    // the person's case says `insan` out loud and the plan carries the same word
    // back to the client.
    record.decided_by =
        approval.decided_by().empty() ? std::string("insan") : approval.decided_by();
    plan->decided_by   = record.decided_by;
    record.assumptions = plan->assumptions();
    for (const PlanStep& step : plan->steps) {
        record.commands.push_back(step.line);
        for (const std::string& handle : step.handles)
            record.sources.push_back(handle);
        for (const std::string& built : step.constructions)
            record.sources.push_back(built);
    }

    if (approval.decision() == Decision::Reject) {
        record.decision = "reddet";
        record.outcome  = "Uygulanmadı; çizim değişmedi.";
        audit_.write(std::move(record));
        return plans_.settle(plan->id, PlanState::Rejected, "Kullanıcı reddetti.");
    }

    // THE EDIT ITSELF, and it is the runner's job because running a command
    // belongs to the bus and the bus belongs to the application's thread. What
    // this class guarantees is that nothing reaches the runner without an
    // `Approval`, and that whatever the runner reports is recorded.
    record.decision  = "uygula";
    core::Status ran = runner_ ? runner_(*plan)
                               : core::err(core::ErrorCode::Unsupported,
                                           "Öneri uygulayıcı bağlı değil; bu yapıda uygulanamaz.");
    if (!ran) {
        record.outcome = ran.error().message;
        audit_.write(std::move(record));
        // The runner rolled the batch back, so the document is bit-identical to
        // what it was before the attempt (ai.md R20). The plan is `Failed`
        // rather than `Rejected`: the person said yes and the program said no,
        // and those are different answers to record.
        (void)plans_.settle(plan->id, PlanState::Failed, ran.error().message);
        return ran;
    }

    record.outcome = "Tek işlem olarak uygulandı.";
    audit_.write(std::move(record));
    return plans_.settle(plan->id, PlanState::Applied);
}

} // namespace kentos::ai
