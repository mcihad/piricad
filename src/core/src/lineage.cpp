// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/lineage.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cstring>

namespace kentos::core {
namespace {

// A NEW seed, carrying the program's present name (CLAUDE.md 0.5a froze only
// the seeds already folded into fixtures).
constexpr std::uint64_t kLineageSeed = fnv1a("kentos.core.lineage");

/// The layout byte an encoded origin starts with: a history origin's, and a
/// result's, which carries its revisions after its sources.
constexpr std::uint8_t kLayout       = 1;
constexpr std::uint8_t kLayoutResult = 2;
/// A result's, with how it was run after its revisions.
constexpr std::uint8_t kLayoutRerun = 3;

} // namespace

// ---------------------------------------------------------------- table ----

namespace {

/// A record's hash, for finding an equal one already held.
std::uint64_t hash_of(const Lineage& origin)
{
    std::uint64_t h = fnv1a(origin.operation, kLineageSeed);
    h               = fnv1a_int(static_cast<std::int64_t>(origin.sources.size()), h);
    for (const EntityKey k : origin.sources)
        h = fnv1a_int(static_cast<std::int64_t>(raw(k)), h);
    for (const std::uint64_t r : origin.revisions)
        h = fnv1a_int(static_cast<std::int64_t>(r), h);
    return fnv1a(origin.arguments, h);
}

} // namespace

const Lineage* LineageTable::get(EntityId e) const
{
    const auto it = rows_.find(e);
    return it == rows_.end() ? nullptr : &pool_[it->second];
}

void LineageTable::set(EntityId e, Lineage origin)
{
    if (origin.operation.empty()) {
        rows_.erase(e);
        return;
    }
    // SHARED, NOT COPIED: the thirty contours of one run name one record.
    const std::uint64_t h                 = hash_of(origin);
    std::vector<std::uint32_t>& same_hash = by_hash_[h];
    for (const std::uint32_t at : same_hash)
        if (pool_[at] == origin) {
            rows_[e] = at;
            return;
        }
    const auto at = static_cast<std::uint32_t>(pool_.size());
    pool_.push_back(std::move(origin));
    same_hash.push_back(at);
    rows_[e] = at;
}

std::vector<EntityId> LineageTable::derived() const
{
    std::vector<EntityId> out;
    out.reserve(rows_.size());
    for (const auto& [e, at] : rows_)
        out.push_back(e);
    return out;
}

void LineageTable::made_from(EntityKey source, std::vector<EntityId>& out) const
{
    out.clear();
    // Each record asked once, however many rows share it.
    std::vector<std::uint8_t> names(pool_.size(), 2); // 2 = not asked yet
    for (const auto& [e, at] : rows_) {
        if (names[at] == 2)
            names[at] = std::ranges::binary_search(pool_[at].sources, source) ? 1 : 0;
        if (names[at] == 1) out.push_back(e);
    }
}

std::uint32_t LineageTable::origin_of(EntityId e) const
{
    const auto it = rows_.find(e);
    return it == rows_.end() ? kNoOrigin : it->second;
}

std::uint64_t LineageTable::fold(std::uint64_t seed, std::span<const std::uint32_t> position) const
{
    if (rows_.empty()) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_.size()), seed ^ kLineageSeed);
    for (const auto& [e, at] : rows_) {
        const Lineage& origin = pool_[at];
        h = fnv1a_int(static_cast<std::int64_t>(e < position.size() ? position[e] : e), h);
        h = fnv1a(origin.operation, h);
        h = fnv1a_int(static_cast<std::int64_t>(origin.sources.size()), h);
        for (const EntityKey k : origin.sources)
            h = fnv1a_int(static_cast<std::int64_t>(raw(k)), h);
        // A RESULT'S EVIDENCE IS CONTENT, and folds only where there is some:
        // a history origin folds exactly as it did before results existed.
        if (!origin.revisions.empty()) {
            h = fnv1a_int(-3, h);
            for (const std::uint64_t r : origin.revisions)
                h = fnv1a_int(static_cast<std::int64_t>(r), h);
            if (!origin.arguments.empty()) {
                h = fnv1a_int(static_cast<std::int64_t>(origin.arguments.size()), h);
                h = fnv1a(origin.arguments, h);
            }
        }
    }
    return h;
}

std::vector<EntityKey> lineage_sources(std::span<const EntityKey> sources)
{
    std::vector<EntityKey> out(sources.begin(), sources.end());
    std::ranges::sort(out);
    out.erase(std::ranges::unique(out).begin(), out.end());
    return out;
}

// ----------------------------------------------------- is it up to date ----

const char* result_state_id(ResultState s) noexcept
{
    switch (s) {
    case ResultState::History: return "gecmis";
    case ResultState::Current: return "guncel";
    case ResultState::Stale: return "guncel_degil";
    case ResultState::Sourceless: return "kaynaksiz";
    }
    return "gecmis";
}

ResultCheck check_origin(const Document& doc, const Lineage& origin)
{
    ResultCheck out;
    if (!origin.result() || origin.revisions.size() != origin.sources.size()) return out;
    for (std::size_t i = 0; i < origin.sources.size(); ++i) {
        const EntityKey k = origin.sources[i];
        const EntityId e  = doc.slot_of(k);
        if (e == kNoEntity || !doc.alive(e)) {
            out.gone.push_back(k);
            continue;
        }
        if (doc.content_revision(e) != origin.revisions[i]) out.changed.push_back(k);
    }
    out.state = !out.changed.empty() ? ResultState::Stale
                : !out.gone.empty()  ? ResultState::Sourceless
                                     : ResultState::Current;
    return out;
}

ResultCheck check_result(const Document& doc, EntityId e)
{
    const Lineage* origin = doc.lineage().get(e);
    if (origin == nullptr) return {};
    return check_origin(doc, *origin);
}

std::vector<std::uint64_t> lineage_revisions(const Document& doc,
                                             std::span<const EntityKey> sources)
{
    std::vector<std::uint64_t> out;
    out.reserve(sources.size());
    for (const EntityKey k : sources)
        out.push_back(doc.content_revision(doc.slot_of(k)));
    return out;
}

// ---------------------------------------------------------------- bytes ----

std::vector<std::uint8_t> encode_lineage(const Lineage* origin)
{
    if (origin == nullptr || origin->operation.empty()) return {};
    const auto name   = static_cast<std::uint32_t>(origin->operation.size());
    const auto count  = static_cast<std::uint32_t>(origin->sources.size());
    const bool result = origin->result() && origin->revisions.size() == origin->sources.size();
    const bool rerun  = result && !origin->arguments.empty();
    const auto args   = static_cast<std::uint32_t>(rerun ? origin->arguments.size() : 0);
    std::vector<std::uint8_t> out(1 + 4 + name + 4 +
                                  ((result ? 16 : 8) * static_cast<std::size_t>(count)) +
                                  (rerun ? 4 + std::size_t{args} : 0));
    std::size_t at = 0;
    out[at++]      = rerun ? kLayoutRerun : result ? kLayoutResult : kLayout;
    std::memcpy(out.data() + at, &name, 4);
    at += 4;
    std::memcpy(out.data() + at, origin->operation.data(), name);
    at += name;
    std::memcpy(out.data() + at, &count, 4);
    at += 4;
    for (const EntityKey k : origin->sources) {
        const std::uint64_t key = raw(k);
        std::memcpy(out.data() + at, &key, 8);
        at += 8;
    }
    if (result)
        for (const std::uint64_t r : origin->revisions) {
            std::memcpy(out.data() + at, &r, 8);
            at += 8;
        }
    if (rerun) {
        std::memcpy(out.data() + at, &args, 4);
        at += 4;
        std::memcpy(out.data() + at, origin->arguments.data(), args);
    }
    return out;
}

Result<Lineage> decode_lineage(std::span<const std::uint8_t> bytes)
{
    Lineage out;
    if (bytes.empty()) return out;
    const auto refused = [] {
        return err(ErrorCode::InvalidArgument, "Köken kaydının baytları tanınmıyor.");
    };
    if ((bytes[0] != kLayout && bytes[0] != kLayoutResult && bytes[0] != kLayoutRerun) ||
        bytes.size() < 1 + 4)
        return refused();
    const bool rerun   = bytes[0] == kLayoutRerun;
    const bool result  = rerun || bytes[0] == kLayoutResult;
    std::uint32_t name = 0;
    std::memcpy(&name, bytes.data() + 1, 4);
    std::size_t at = 1 + 4;
    if (bytes.size() < at + name + 4) return refused();
    out.operation.assign(reinterpret_cast<const char*>(bytes.data() + at), name);
    at += name;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + at, 4);
    at += 4;
    const std::size_t body = at + ((result ? 16 : 8) * static_cast<std::size_t>(count));
    if ((rerun ? bytes.size() < body + 4 : bytes.size() != body) || (result && count == 0))
        return refused();
    out.sources.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i, at += 8) {
        std::uint64_t key = 0;
        std::memcpy(&key, bytes.data() + at, 8);
        out.sources.push_back(static_cast<EntityKey>(key));
    }
    if (result) {
        out.revisions.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i, at += 8) {
            std::uint64_t r = 0;
            std::memcpy(&r, bytes.data() + at, 8);
            out.revisions.push_back(r);
        }
    }
    if (rerun) {
        std::uint32_t args = 0;
        std::memcpy(&args, bytes.data() + at, 4);
        at += 4;
        if (bytes.size() != at + std::size_t{args} || args == 0) return refused();
        out.arguments.assign(reinterpret_cast<const char*>(bytes.data() + at), args);
    }
    return out;
}

} // namespace kentos::core
