// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/audit.hpp"

#include "kentos_cad/core/text.hpp"

#include <cstdio>

namespace kentos::ai {
namespace {

std::string format_id(std::uint64_t value)
{
    char buffer[20] = {};
    (void)std::snprintf(buffer, sizeof buffer, "d%016llx", static_cast<unsigned long long>(value));
    return std::string(buffer);
}

} // namespace

core::Json AuditRecord::to_json() const
{
    core::Json out;
    out.set("sürüm", core::Json::integer(1));
    out.set("kayit", core::Json::string(id));
    out.set("oneri", core::Json::string(plan_id));
    out.set("zaman_utc_ms", core::Json::integer(utc_ms));
    out.set("karar", core::Json::string(decision));

    // WHO OR WHAT DECIDED, WRITTEN EVERY TIME. Today only a person can (5.7), so
    // every line carries the same word — and that is exactly why it is written
    // now rather than when it first varies: a record that says nothing cannot be
    // told apart from one written after the rule changes, and S-06's rule that
    // an automatic action must never read as a click would then be
    // unenforceable in retrospect.
    out.set("karar_veren", core::Json::string(decided_by.empty() ? "insan" : decided_by));
    if (!policy.empty()) out.set("onay_politikasi", core::Json::string(policy));

    out.set("kullanici", core::Json::string(operator_name));
    out.set("isteyen", core::Json::string(requester));
    out.set("model", core::Json::string(model));
    out.set("uc_nokta", core::Json::string(endpoint));
    out.set("istem", core::Json::string(prompt));

    core::Json lines = core::Json::array({});
    for (const std::string& line : commands)
        lines.push(core::Json::string(line));
    out.set("komutlar", std::move(lines));

    if (!sources.empty()) {
        core::Json from = core::Json::array({});
        for (const std::string& one : sources)
            from.push(core::Json::string(one));
        out.set("konum_kaynagi", std::move(from));
    }

    if (!assumptions.empty()) {
        core::Json noted = core::Json::array({});
        for (const std::string& one : assumptions)
            noted.push(core::Json::string(one));
        out.set("varsayimlar", std::move(noted));
    }

    if (!outcome.empty()) out.set("sonuc", core::Json::string(outcome));

    if (!created.empty()) {
        // R7: an entity must resolve back to the record that authorised it. The
        // keys are persistent (model.md R5) rather than slots, so the link
        // survives a save and a reopen — which is the only kind of link worth
        // having in a legal record.
        core::Json keys = core::Json::array({});
        for (const std::int64_t key : created)
            keys.push(core::Json::integer(key));
        out.set("olusan_nesneler", std::move(keys));
    }
    return out;
}

AuditLog::AuditLog(Sink sink) : sink_(std::move(sink)) {}

std::string AuditLog::next_id()
{
    ++written_;
    std::uint64_t h = core::fnv1a("kentos.ai.audit");
    h               = core::fnv1a_int(static_cast<std::int64_t>(written_), h);
    return format_id(h);
}

std::string AuditLog::write(AuditRecord record)
{
    record.id = next_id();

    // WRITTEN BEFORE IT IS REMEMBERED. The sink is durable by contract, so a
    // crash between the decision and the edit leaves the decision on disk —
    // which is the right way round: a record of an edit that did not happen is
    // answerable, an edit with no record is not.
    if (sink_) sink_(record.to_json().dump() + "\n");
    recent_.push_back(std::move(record));
    return recent_.back().id;
}

std::string AuditLog::write_coordinate_refusal(std::string requester, std::string tool,
                                               std::string detail, std::int64_t utc_ms)
{
    // ai.md R10 asks for this one by name: a command carrying a coordinate that
    // does not trace to a tool-call result "MUST be rejected before validation,
    // with the rejection audit-logged". It never becomes a plan, so without this
    // the most interesting refusal the program makes would leave no trace.
    AuditRecord record;
    record.requester = std::move(requester);
    record.decision  = "koordinat_reddi";
    record.utc_ms    = utc_ms;
    record.commands  = {tool};
    record.outcome   = std::move(detail);
    record.prompt    = "(araç çağrısı doğrudan koordinat taşıdı)";
    return write(std::move(record));
}

std::string AuditLog::write_escalation_refusal(std::string requester, std::string tool,
                                               std::string detail, std::int64_t utc_ms)
{
    AuditRecord record;
    record.requester = std::move(requester);
    record.decision  = "yetki_reddi";
    record.utc_ms    = utc_ms;
    record.commands  = {std::move(tool)};
    record.outcome   = std::move(detail);
    record.prompt    = "(istemci kendi yetkisini genişletecek bir çağrı yaptı)";
    return write(std::move(record));
}

} // namespace kentos::ai
