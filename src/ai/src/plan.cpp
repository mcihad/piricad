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

    // SAID EVERY TIME, in the answer itself. An agent that does not know its
    // write is waiting for a person reads the empty result as a failure and
    // tries again — which is how a careful protocol turns into eleven duplicate
    // suggestions on somebody's screen.
    out.set("aciklama",
            core::Json::string(state == PlanState::Pending
                                   ? "Bu öneri uygulanmadı. Çizimi değiştirmek için bilgisayar "
                                     "başındaki mühendisin onaylaması gerekir; onaylanırsa "
                                     "tamamı tek bir işlem ve tek Ctrl+Z olur."
                                   : "Bu önerinin durumu yukarıda; uygulama kararı kullanıcıya "
                                     "aittir."));
    return out;
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
    if (plan->state != PlanState::Pending)
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
