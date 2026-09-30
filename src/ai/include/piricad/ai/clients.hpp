// SPDX-License-Identifier: AGPL-3.0-or-later
// KentOSCad — ai: who has talked to this server, and who may not any more.
//
// WHY THIS FILE IS AGPL WHEN THE REST OF THE TREE IS GPL-3.0-or-later: see
// `jsonrpc.hpp`. This is the server's own bookkeeping about the clients it
// served, so it belongs with the server component (CLAUDE.md Article 2.1, §1).
//
// WHAT IT IS FOR. MCP 2026-07-28 is STATELESS: there is no `initialize`
// handshake, no session id, no connection to look at. So "who is connected"
// cannot be read off a socket table — the honest answer is "who has spoken
// here, and when", which is what this ledger records. A person opening the
// settings page wants three things from it, and TODOS M-08 names all three:
// which agents are using this drawing, what they have been refused, and a way to
// shut ONE of them out without changing the token on all of them.
//
// AND THAT LAST ONE IS THE POINT. Rotating the token is the blunt instrument: it
// locks out every agent, including the one the person is in the middle of
// working with. Revoking a single client is the sharp one, and it is what makes
// "I do not like what that thing is doing" a thing a person can act on in the
// second it takes to click, rather than a reason to turn the whole server off.
//
// NO SECRET IS IN HERE (CLAUDE.md 5.21). The label is the client's declared name
// plus its token's FINGERPRINT, exactly the string that reaches the audit
// record — never the token, never a header value that could hold one.
#pragma once

#include "kentos_cad/ai/endpoint.hpp"

#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::ai {

/// One client, as the settings page shows it.
struct ClientRecord
{
    /// The requester label: declared name plus token fingerprint. The same
    /// string the audit record carries, and the key everything else uses.
    std::string label;

    std::uint64_t calls{0};    ///< requests served, refusals included
    std::uint64_t refusals{0}; ///< how many of them were refused
    std::uint64_t plans{0};    ///< suggestions it filed

    /// The last method it called, for the "what is it doing" column.
    std::string last_method;

    /// Why its last refusal was refused, in the words the client was given.
    /// Empty when it has not been refused. This is TODOS M-08's "son hatalar",
    /// kept per client rather than as one global list: a person looking at a
    /// misbehaving agent wants ITS errors, not everybody's interleaved.
    std::string last_refusal;

    /// When it was first and last seen, as the transport reported it — seconds
    /// since the Unix epoch, or 0 when the transport did not say. The engine has
    /// no clock (it is sans-IO), so this can only ever be what it was told.
    std::uint64_t first_seen{0};
    std::uint64_t last_seen{0};

    /// Whether a person has shut this client out. A revoked client is answered
    /// `403` and reaches nothing: not a tool, not a resource, not the catalogue.
    bool revoked{false};
};

/// Every client this server has served, and the revocations a person has made.
///
/// BOUNDED, like every other store an outside caller can grow: a client that
/// invents a new name on every call must not grow the program's memory. A
/// REVOKED record is never evicted — forgetting a revocation would quietly let
/// the client back in, which is the one failure this type must not have.
class ClientLedger
{
public:
    /// Beyond this many records the oldest UNREVOKED one is dropped.
    static constexpr std::size_t kMaxClients = 64;

    /// Records one served request. `at` is the transport's clock, or 0.
    void note(const AuditNote& audit, std::uint64_t at);

    /// Whether this client has been shut out.
    bool revoked(std::string_view label) const;

    /// Shuts one client out. A label nobody has used is still accepted and
    /// recorded: a person may want to refuse an agent BEFORE it first calls,
    /// and refusing to record that would be refusing the only useful moment.
    core::Status revoke(std::string_view label);

    /// Lets it back in. Refuses a label the ledger does not hold, because
    /// "allowed" is the default and restoring an unknown client would be a
    /// no-op dressed as an action.
    core::Status restore(std::string_view label);

    /// Forgets every client, revocations included.
    ///
    /// WHAT A TOKEN ROTATION MEANS. Every label carries the old token's
    /// fingerprint, so after a rotation not one of them can recur — keeping them
    /// would be keeping a list of names nobody will answer to, and keeping their
    /// revocations would be shutting out labels that can never be presented.
    void forget_all();

    /// The record for one label, or null.
    const ClientRecord* find(std::string_view label) const;

    /// Every client, MOST RECENTLY SEEN FIRST and ties broken by label, so the
    /// settings table and a script's output are the same twice.
    std::vector<ClientRecord> clients() const;

    std::size_t size() const noexcept { return records_.size(); }

private:
    ClientRecord* mutable_find(std::string_view label);

    /// Insertion-ordered; eviction is by age. See `HandleScopes` for the same
    /// bargain made for the same reason.
    std::vector<ClientRecord> records_;
};

} // namespace kentos::ai
