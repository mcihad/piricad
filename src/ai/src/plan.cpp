// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/plan.hpp"

#include "kentos_cad/core/text.hpp"

#include <cstdio>

namespace kentos::ai {
namespace {

std::string format_id(std::uint64_t value)
{
    char buffer[20] = {};
    (void)std::snprintf(buffer, sizeof buffer, "p%016llx", static_cast<unsigned long long>(value));
    return std::string(buffer);
}

} // namespace

const char* plan_state_name(PlanState state)
{
    switch (state) {
    case PlanState::Pending: return "beklemede";
    case PlanState::Applied: return "uygulandi";
    case PlanState::Rejected: return "reddedildi";
    case PlanState::Withdrawn: return "geri_cekildi";
    case PlanState::Failed: return "basarisiz";
    case PlanState::Running: return "uygulaniyor";
    }
    return "bilinmiyor";
}

core::Json Plan::to_json() const
{
    core::Json out;
    out.set("oneri", core::Json::string(id));
    out.set("durum", core::Json::string(plan_state_name(state)));

    // THE COMMAND LINES, NOT THE ARGUMENTS. A line is what a person reads on the
    // suggestion card and what they could type themselves, so it is also the
    // honest thing to hand back to the client: the same words, and no private
    // encoding the two sides could come to disagree about (Article 1.2, 1.4).
    core::Json lines = core::Json::array({});
    for (const PlanStep& step : steps)
        lines.push(core::Json::string(step.line));
    out.set("adimlar", std::move(lines));

    if (!refusal.empty()) out.set("gerekce", core::Json::string(refusal));

    // HOW FAR IT HAS GOT, while it is running. Said as two counts rather than as
    // a percentage: the step is the unit a person approved and the unit a client
    // can name (M-06).
    if (state == PlanState::Running) {
        out.set("biten_adim", core::Json::integer(static_cast<std::int64_t>(done_steps)));
        out.set("toplam_adim", core::Json::integer(static_cast<std::int64_t>(steps.size())));
    }

    // WHAT APPLYING IT LEFT BEHIND, said rather than inferred. A client that has
    // to read "uygulandı" and then guess whether a file appeared is a client that
    // will guess wrong (TODOS C-03).
    if (applied_revision != 0)
        out.set("yeni_surum", core::Json::integer(static_cast<std::int64_t>(applied_revision)));
    if (!undo_label.empty()) out.set("geri_alma", core::Json::string(undo_label));
    if (!decided_by.empty()) out.set("karar_veren", core::Json::string(decided_by));
    if (!outputs.empty()) {
        core::Json files = core::Json::array({});
        for (const std::string& one : outputs)
            files.push(core::Json::string(one));
        out.set("yazilan_dosyalar", std::move(files));
    }
    if (!warnings.empty()) {
        core::Json notes = core::Json::array({});
        for (const std::string& one : warnings)
            notes.push(core::Json::string(one));
        out.set("uyarilar", std::move(notes));
    }

    // SAID EVERY TIME, in the answer itself. An agent that does not know its
    // write is waiting for a person reads the empty result as a failure and
    // tries again — which is how a careful protocol turns into eleven duplicate
    // suggestions on somebody's screen.
    // AND A RUNNING PLAN SAYS SO IN WORDS. A client reading a list of files
    // while the work is still going would read a partial result as a finished
    // one — which is exactly what M-06 forbids, and the reason `yazilan_dosyalar`
    // is only ever filled once the batch has closed.
    const char* note = "Bu önerinin durumu yukarıda; uygulama kararı kullanıcıya aittir.";
    if (state == PlanState::Pending)
        note = "Bu öneri uygulanmadı. Çizimi değiştirmek için bilgisayar başındaki "
               "mühendisin onaylaması gerekir; onaylanırsa tamamı tek bir işlem ve tek "
               "Ctrl+Z olur.";
    else if (state == PlanState::Running)
        note = "Bu öneri ŞU AN uygulanıyor; henüz bitmedi. Sonuç — yazılan dosyalar, yeni "
               "sürüm, uyarılar — ancak durum 'uygulandi' olduğunda tamamdır. Yarım bir "
               "çıktı bitmiş sayılmaz.";
    out.set("aciklama", core::Json::string(note));
    return out;
}

std::uint64_t Plan::content_fingerprint() const
{
    std::uint64_t h = core::fnv1a("kentos.ai.plan.content");
    for (const PlanStep& step : steps) {
        h = core::fnv1a(step.command_id, h);
        // THE ARGUMENTS, NOT THE LINE. The line is what a person reads; the
        // arguments are what runs, and two plans that read the same while running
        // differently must not share a fingerprint.
        h = core::fnv1a(step.args.to_json().dump(), h);
    }
    return h;
}

std::string PlanStore::add(Plan plan)
{
    ++filed_;
    std::uint64_t h = core::fnv1a("kentos.ai.plan");
    h               = core::fnv1a_int(static_cast<std::int64_t>(filed_), h);
    plan.id         = format_id(h);
    plan.state      = PlanState::Pending;

    // A CLIENT THAT COMPOSES AND NEVER WAITS must not fill the person's screen.
    // The oldest PENDING plan is withdrawn — not the oldest plan, because an
    // applied one is a record somebody may still be reading.
    std::size_t open = 0;
    for (const Plan& held : plans_)
        if (held.state == PlanState::Pending) ++open;
    if (open >= kMaxPending) {
        for (Plan& held : plans_) {
            if (held.state != PlanState::Pending) continue;
            held.state   = PlanState::Withdrawn;
            held.refusal = "Çok fazla bekleyen öneri: en eskisi geri çekildi.";
            break;
        }
    }

    plans_.push_back(std::move(plan));
    return plans_.back().id;
}

core::Status PlanStore::append(std::string_view id, PlanStep step)
{
    Plan* plan = find(id);
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir öneri yok: '" + std::string(id) + "'.");
    if (plan->state != PlanState::Pending)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Öneri '" + std::string(id) + "' artık beklemiyor (" +
                             plan_state_name(plan->state) + "); yeni bir öneri açın.");
    plan->steps.push_back(std::move(step));
    return core::ok();
}

bool PlanStore::owned_by(const Plan& plan, std::string_view requester)
{
    return requester.empty() || plan.requester == requester;
}

const Plan* PlanStore::find_for(std::string_view id, std::string_view requester) const
{
    const Plan* plan = find(id);
    return plan != nullptr && owned_by(*plan, requester) ? plan : nullptr;
}

core::Status PlanStore::append_for(std::string_view id, std::string_view requester, PlanStep step)
{
    if (find_for(id, requester) == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir öneri yok: '" + std::string(id) + "'.");
    return append(id, std::move(step));
}

Plan* PlanStore::find(std::string_view id)
{
    for (Plan& plan : plans_)
        if (plan.id == id) return &plan;
    return nullptr;
}

const Plan* PlanStore::find(std::string_view id) const
{
    for (const Plan& plan : plans_)
        if (plan.id == id) return &plan;
    return nullptr;
}

core::Status PlanStore::begin_apply(std::string_view id)
{
    Plan* plan = find(id);
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir öneri yok: '" + std::string(id) + "'.");
    if (plan->state != PlanState::Pending)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Öneri '" + std::string(id) + "' zaten " + plan_state_name(plan->state) +
                             "; yeniden uygulanamaz.");
    plan->state      = PlanState::Running;
    plan->done_steps = 0;
    return core::ok();
}

void PlanStore::report_progress(std::string_view id, std::size_t done)
{
    // ONLY A RUNNING PLAN HAS PROGRESS. Writing a count onto a settled plan
    // would be describing work that is over, and a client polling afterwards
    // would read it as work still going.
    if (Plan* plan = find(id); plan != nullptr && plan->state == PlanState::Running)
        plan->done_steps = done;
}

const Plan* PlanStore::find_by_key(std::string_view key, std::string_view requester) const
{
    if (key.empty()) return nullptr;
    for (const Plan& plan : plans_)
        if (plan.idempotency_key == key && plan.requester == requester) return &plan;
    return nullptr;
}

core::Status PlanStore::settle(std::string_view id, PlanState state, std::string refusal)
{
    Plan* plan = find(id);
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir öneri yok: '" + std::string(id) + "'.");

    // ONE DECISION PER PLAN. A second one would mean either applying something
    // twice or recording two different answers for one question, and the audit
    // record is supposed to answer "what did the engineer decide" without
    // ambiguity (ai.md R6).
    //
    // `Running` PASSES, because it is not a decision: it is the stretch between
    // the decision and its outcome, and the outcome is what lands here (M-06).
    if (plan->state != PlanState::Pending && plan->state != PlanState::Running)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Öneri '" + std::string(id) + "' zaten " + plan_state_name(plan->state) +
                             "; kararı değiştirilemez.");

    plan->state   = state;
    plan->refusal = std::move(refusal);
    return core::ok();
}

std::vector<const Plan*> PlanStore::pending() const
{
    std::vector<const Plan*> out;
    for (const Plan& plan : plans_)
        if (plan.state == PlanState::Pending) out.push_back(&plan);
    return out;
}

} // namespace kentos::ai
