// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/lineage.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cstring>

namespace kentos::core {
namespace {

// A NEW seed, carrying the program's present name (CLAUDE.md 0.5a froze only
// the seeds already folded into fixtures).
constexpr std::uint64_t kLineageSeed = fnv1a("kentos.core.lineage");

/// The layout byte an encoded origin starts with.
constexpr std::uint8_t kLayout = 1;

} // namespace

// ---------------------------------------------------------------- table ----

const Lineage* LineageTable::get(EntityId e) const
{
    const auto it = rows_.find(e);
    return it == rows_.end() ? nullptr : &it->second;
}

void LineageTable::set(EntityId e, Lineage origin)
{
    if (origin.operation.empty()) {
        rows_.erase(e);
        return;
    }
    rows_[e] = std::move(origin);
}

std::vector<EntityId> LineageTable::derived() const
{
    std::vector<EntityId> out;
    out.reserve(rows_.size());
    for (const auto& [e, origin] : rows_)
        out.push_back(e);
    return out;
}

void LineageTable::made_from(EntityKey source, std::vector<EntityId>& out) const
{
    out.clear();
    for (const auto& [e, origin] : rows_)
        if (std::ranges::binary_search(origin.sources, source)) out.push_back(e);
}

std::uint64_t LineageTable::fold(std::uint64_t seed, std::span<const std::uint32_t> position) const
{
    if (rows_.empty()) return seed;
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(rows_.size()), seed ^ kLineageSeed);
    for (const auto& [e, origin] : rows_) {
        h = fnv1a_int(static_cast<std::int64_t>(e < position.size() ? position[e] : e), h);
        h = fnv1a(origin.operation, h);
        h = fnv1a_int(static_cast<std::int64_t>(origin.sources.size()), h);
        for (const EntityKey k : origin.sources)
            h = fnv1a_int(static_cast<std::int64_t>(raw(k)), h);
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

// ---------------------------------------------------------------- bytes ----

std::vector<std::uint8_t> encode_lineage(const Lineage* origin)
{
    if (origin == nullptr || origin->operation.empty()) return {};
    const auto name  = static_cast<std::uint32_t>(origin->operation.size());
    const auto count = static_cast<std::uint32_t>(origin->sources.size());
    std::vector<std::uint8_t> out(1 + 4 + name + 4 + (8 * static_cast<std::size_t>(count)));
    std::size_t at = 0;
    out[at++]      = kLayout;
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
    return out;
}

Result<Lineage> decode_lineage(std::span<const std::uint8_t> bytes)
{
    Lineage out;
    if (bytes.empty()) return out;
    const auto refused = [] {
        return err(ErrorCode::InvalidArgument, "Köken kaydının baytları tanınmıyor.");
    };
    if (bytes[0] != kLayout || bytes.size() < 1 + 4) return refused();
    std::uint32_t name = 0;
    std::memcpy(&name, bytes.data() + 1, 4);
    std::size_t at = 1 + 4;
    if (bytes.size() < at + name + 4) return refused();
    out.operation.assign(reinterpret_cast<const char*>(bytes.data() + at), name);
    at += name;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + at, 4);
    at += 4;
    if (bytes.size() != at + (8 * static_cast<std::size_t>(count))) return refused();
    out.sources.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i, at += 8) {
        std::uint64_t key = 0;
        std::memcpy(&key, bytes.data() + at, 8);
        out.sources.push_back(static_cast<EntityKey>(key));
    }
    return out;
}

} // namespace kentos::core
