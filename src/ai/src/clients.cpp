// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kentos_cad/ai/clients.hpp"

#include <algorithm>

namespace kentos::ai {

ClientRecord* ClientLedger::mutable_find(std::string_view label)
{
    for (ClientRecord& record : records_)
        if (record.label == label) return &record;
    return nullptr;
}

const ClientRecord* ClientLedger::find(std::string_view label) const
{
    for (const ClientRecord& record : records_)
        if (record.label == label) return &record;
    return nullptr;
}

void ClientLedger::note(const AuditNote& audit, std::uint64_t at)
{
    if (audit.requester.empty()) return;

    ClientRecord* record = mutable_find(audit.requester);
    if (record == nullptr) {
        // THE OLDEST UNREVOKED RECORD GOES, never a revoked one: dropping a
        // revocation would let a client back in silently, and a caller that
        // invented names until the ledger overflowed would have found a way to
        // undo a person's decision.
        if (records_.size() >= kMaxClients) {
            const auto victim = std::find_if(records_.begin(), records_.end(),
                                             [](const ClientRecord& r) { return !r.revoked; });
            if (victim != records_.end()) records_.erase(victim);
        }
        // AND WHEN EVERY RECORD IS REVOKED the ledger simply stops growing. A
        // new client is then unrecorded rather than unserved: the ledger is a
        // record of what happened, and refusing service because a LIST is full
        // would be inventing a policy nobody asked for.
        if (records_.size() >= kMaxClients) return;

        records_.push_back(ClientRecord{});
        record             = &records_.back();
        record->label      = audit.requester;
        record->first_seen = at;
    }

    ++record->calls;
    if (!audit.method.empty()) record->last_method = audit.method;
    if (!audit.plan_id.empty()) ++record->plans;
    if (!audit.detail.empty()) {
        ++record->refusals;
        record->last_refusal = audit.detail;
    }
    if (at != 0) record->last_seen = at;
}

bool ClientLedger::revoked(std::string_view label) const
{
    const ClientRecord* record = find(label);
    return record != nullptr && record->revoked;
}

core::Status ClientLedger::revoke(std::string_view label)
{
    if (label.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "Yetkisi kaldırılacak istemcinin adı boş olamaz.");

    if (ClientRecord* record = mutable_find(label); record != nullptr) {
        if (record->revoked)
            return core::err(core::ErrorCode::ValidationFailed,
                             "'" + std::string(label) + "' zaten yetkisiz.");
        record->revoked = true;
        return core::ok();
    }

    // A CLIENT THAT HAS NOT CALLED YET can still be shut out, and that is the
    // useful moment rather than an edge case: a person who has just read a name
    // in a transcript should not have to wait for it to call again.
    records_.push_back(ClientRecord{});
    records_.back().label   = std::string(label);
    records_.back().revoked = true;
    return core::ok();
}

core::Status ClientLedger::restore(std::string_view label)
{
    ClientRecord* record = mutable_find(label);
    if (record == nullptr)
        return core::err(core::ErrorCode::NotFound,
                         "Böyle bir istemci yok: '" + std::string(label) + "'.");
    if (!record->revoked)
        return core::err(core::ErrorCode::ValidationFailed,
                         "'" + std::string(label) + "' zaten yetkili.");
    record->revoked = false;
    return core::ok();
}

void ClientLedger::forget_all()
{
    records_.clear();
}

std::vector<ClientRecord> ClientLedger::clients() const
{
    std::vector<ClientRecord> out = records_;
    std::stable_sort(out.begin(), out.end(), [](const ClientRecord& a, const ClientRecord& b) {
        if (a.last_seen != b.last_seen) return a.last_seen > b.last_seen;
        return a.label < b.label;
    });
    return out;
}

} // namespace kentos::ai
