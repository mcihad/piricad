// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: where a coordinate is allowed to come from.
//
// THE RULE THIS FILE IS. CLAUDE.md 5.8: "NEVER let a coordinate originate in
// model text; every `Mm`/`Point2` in a generated command traces to a recorded
// tool-call result." .claude/ai.md R10 goes further: a command carrying an
// untraceable coordinate "MUST be rejected before validation, with the rejection
// audit-logged".
//
// AND IT IS A TYPE, NOT A CHECK. The agent-facing schema declares a point, a
// point list and a selection as a HANDLE STRING (`ai::Style::Agent`,
// catalog.cpp), so a model has no way to write a number where a coordinate
// belongs: the refusal happens while the arguments are still JSON, before an
// `Args` exists, which is what makes R10's "before validation" literally true
// rather than a matter of ordering code carefully.
//
// WHERE A HANDLE COMES FROM. Only a read tool mints one, and a read tool reads
// the document: `secimi_al` hands back what the user has selected, `sorgula`
// what matched, `gorunum_bilgisi` the corners of the window. The numbers are the
// document's own. The one other road is an ATTESTED measurement list — a
// surveyor's own coordinates, shown to that surveyor, and turned into handles
// only when they accept them — which is a person vouching for data, not a model
// inventing it.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/json.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::ai {

/// What a handle points at.
enum class HandleKind : std::uint8_t {
    Points,   ///< one or more coordinates
    Entities, ///< persistent entity keys (never dense slots — model.md R5)
    Window,   ///< a rectangle: the visible area, a query's extent
};

/// Where the numbers came from, recorded so an audit can answer "why is this
/// line here?" (ai.md R7).
enum class Provenance : std::uint8_t {
    Document, ///< read out of the open drawing by a read tool
    Computed, ///< derived from document geometry by a calculation tool
    Attested, ///< a measurement list a HUMAN reviewed and accepted
};

/// One minted result: what a read tool answered, kept so a later call can refer
/// to it instead of repeating the numbers.
struct HandleValue
{
    std::string id;                              ///< `@` + 16 hex digits
    HandleKind kind{HandleKind::Points};         ///< what the handle points at
    Provenance provenance{Provenance::Document}; ///< where its numbers came from
    std::string tool;                            ///< which tool minted it
    std::uint64_t revision{0};                   ///< the document revision it was read at

    std::vector<core::Point2> points;   ///< for Points
    std::vector<std::int64_t> entities; ///< for Entities, persistent keys
    core::Box2 window{};                ///< for Window
};

/// A reference written by a client: `@0123456789abcdef` or `@0123456789abcdef.3`
/// for one element of a list.
struct HandleRef
{
    std::string id;                   ///< the `@…` part
    std::optional<std::size_t> index; ///< the `.N` part, when given

    /// Reads a reference out of a string, or nothing when it is not one. The
    /// shape is fixed by the schema's own `pattern`, so a string that fails here
    /// is a string no conforming client should have sent.
    static std::optional<HandleRef> parse(std::string_view text);
};

/// Every handle one client session has been given.
///
/// PER SESSION AND BOUNDED. A handle is only as true as the document it was read
/// from, so it carries the revision it was minted at and `resolve` refuses one
/// that has gone stale. The store is capped: a client that never stops asking
/// must not grow the program's memory without limit.
class HandleStore
{
public:
    /// The oldest handles are dropped once this many are held.
    static constexpr std::size_t kCapacity = 256;

    /// Mints a handle over points read out of the document.
    const HandleValue& mint_points(std::vector<core::Point2> points, std::string tool,
                                   std::uint64_t revision,
                                   Provenance provenance = Provenance::Document);

    /// Mints a handle over persistent entity keys.
    const HandleValue& mint_entities(std::vector<std::int64_t> keys, std::string tool,
                                     std::uint64_t revision);

    /// Mints a handle over a rectangle.
    const HandleValue& mint_window(core::Box2 window, std::string tool, std::uint64_t revision);

    /// The handle, or null when it was never minted or has been dropped.
    const HandleValue* find(std::string_view id) const;

    /// What `ref` names, as a value the command layer can take — or the Turkish
    /// refusal a client is answered with. `revision` is the document's revision
    /// NOW: a handle minted before an edit no longer describes the drawing, and
    /// silently using it would put a line where nothing is any more.
    core::Result<HandleValue> resolve(const HandleRef& ref, std::uint64_t revision) const;

    /// How many handles are held; for the audit record and the tests.
    std::size_t size() const noexcept { return values_.size(); }

    /// The JSON a read tool returns: the handle id, its kind, how many values it
    /// holds and a short summary a model can reason about — never the raw
    /// coordinate list, which is what P9 forbids dumping into a prompt.
    static core::Json describe(const HandleValue& value);

private:
    /// Deterministic ids: the seed plus a counter, hashed. Not random, because a
    /// test and a journal replay must produce the same handle twice.
    std::string next_id(HandleKind kind);

    std::vector<HandleValue> values_;
    std::uint64_t minted_{0};
};

/// ONE STORE PER CLIENT, keyed by the requester label.
///
/// WHY THIS TYPE EXISTS RATHER THAN ONE SHARED STORE. A handle is a promise that
/// a coordinate came out of the document a client READ (CLAUDE.md 5.8). Two
/// agents on one loopback port sharing a store could name each other's promises
/// — and `HandleStore::next_id` is a counter, so the second handle minted by one
/// client has exactly the id the second handle minted by another would, which
/// means a client could hold a handle to geometry it was never shown without
/// even guessing. Scoping is what makes `next_id`'s own comment true (TODOS
/// M-07).
///
/// AN EMPTY LABEL IS A CLIENT TOO, here. The person at the keyboard is unscoped
/// about PLANS — they apply them, so they see all of them — but they never hold
/// a handle: handles are minted for whoever ran the read tool. So the empty
/// label simply gets its own store like any other name.
class HandleScopes
{
public:
    /// Beyond this many clients the OLDEST store is dropped — the first one
    /// opened, not the largest and not the least recently touched: a program
    /// that evicted whichever store was cheapest to evict would be unpredictable,
    /// and a test that runs twice has to know which one went.
    ///
    /// A client whose store was dropped is not refused. It mints again, and its
    /// older handles are simply unknown — the same answer a stale handle already
    /// gets, and the same recovery: read again.
    static constexpr std::size_t kMaxClients = 16;

    /// The store `requester` owns, created on first use.
    HandleStore& for_client(const std::string& requester);

    /// The store `requester` owns, or null when it has none yet. For a caller
    /// that must not create one by asking.
    const HandleStore* peek(std::string_view requester) const;

    /// How many clients are held; for the tests and the connection page.
    std::size_t clients() const noexcept { return stores_.size(); }

private:
    /// INSERTION-ORDERED: a vector rather than a map because eviction is by age
    /// and because a fixed order keeps a test's output the same twice.
    std::vector<std::pair<std::string, HandleStore>> stores_;
};

} // namespace kentos::ai
