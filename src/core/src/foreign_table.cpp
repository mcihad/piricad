// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/foreign_table.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace kentos::core {

std::size_t ForeignTable::locate(std::uint32_t slot, std::string_view tag,
                                 bool& found) const noexcept
{
    // Binary search on (slot, tag name): the records are kept in that order.
    std::size_t lo = 0;
    std::size_t hi = records_.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const Record& r       = records_[mid];
        const bool before     = r.slot < slot || (r.slot == slot && tags_[r.tag] < tag);
        if (before)
            lo = mid + 1;
        else
            hi = mid;
    }
    found = lo < records_.size() && records_[lo].slot == slot && tags_[records_[lo].tag] == tag;
    return lo;
}

std::uint32_t ForeignTable::intern(std::string_view tag)
{
    for (std::size_t i = 0; i < tags_.size(); ++i)
        if (tags_[i] == tag) return static_cast<std::uint32_t>(i);
    tags_.emplace_back(tag);
    return static_cast<std::uint32_t>(tags_.size() - 1);
}

Status ForeignTable::attach(std::uint32_t slot, std::string_view tag,
                            std::span<const std::uint8_t> bytes)
{
    if (tag.empty()) return err(ErrorCode::InvalidArgument, "Yabancı veri etiketi boş olamaz.");
    if (bytes.empty())
        return err(ErrorCode::InvalidArgument,
                   "Yabancı veri boş olamaz; hiçbir şey eklemek bir kayıt değildir.");
    if (bytes.size() > std::numeric_limits<std::uint32_t>::max())
        return err(ErrorCode::ValidationFailed, "Yabancı veri 4294967295 bayt sınırını aşıyor.");

    bool found           = false;
    const std::size_t at = locate(slot, tag, found);
    if (found)
        return err(ErrorCode::ValidationFailed,
                   std::to_string(slot) + ". yuvada '" + std::string(tag) +
                       "' etiketli yabancı veri zaten var; kaynak bir tane taşıyordu.");

    Record r{};
    r.slot  = slot;
    r.tag   = intern(tag);
    r.start = static_cast<std::uint64_t>(pool_.size());
    r.bytes = static_cast<std::uint32_t>(bytes.size());
    pool_.insert(pool_.end(), bytes.begin(), bytes.end());
    records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(at), r);
    return ok();
}

bool ForeignTable::detach(std::uint32_t slot, std::string_view tag)
{
    bool found           = false;
    const std::size_t at = locate(slot, tag, found);
    if (!found) return false;
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(at));
    return true;
}

std::span<const std::uint8_t> ForeignTable::bytes(std::uint32_t slot,
                                                  std::string_view tag) const noexcept
{
    bool found           = false;
    const std::size_t at = locate(slot, tag, found);
    if (!found) return {};
    const Record& r = records_[at];
    return {pool_.data() + r.start, r.bytes};
}

std::size_t ForeignTable::count_for(std::uint32_t slot) const noexcept
{
    bool found        = false;
    std::size_t at    = locate(slot, std::string_view{}, found);
    std::size_t count = 0;
    while (at < records_.size() && records_[at].slot == slot) {
        ++count;
        ++at;
    }
    return count;
}

std::uint64_t ForeignTable::fold(std::uint64_t seed) const
{
    std::uint64_t h = seed;
    for (const Record& r : records_) {
        h = fnv1a_int(static_cast<std::int64_t>(r.slot), h);
        h = fnv1a(tags_[r.tag], h);
        h = fnv1a(std::string_view(reinterpret_cast<const char*>(pool_.data() + r.start), r.bytes),
                  h);
    }
    return h;
}

std::uint64_t ForeignTable::fold(std::uint64_t seed, std::span<const std::uint32_t> slots) const
{
    if (records_.empty()) return seed;
    std::uint64_t h = seed;
    for (std::size_t position = 0; position < slots.size(); ++position) {
        const std::uint32_t slot = slots[position];
        auto at                  = std::ranges::lower_bound(records_, slot, {}, &Record::slot);
        for (; at != records_.end() && at->slot == slot; ++at) {
            h = fnv1a_int(static_cast<std::int64_t>(position), h);
            h = fnv1a(tags_[at->tag], h);
            h = fnv1a(std::string_view(reinterpret_cast<const char*>(pool_.data() + at->start),
                                       at->bytes),
                      h);
        }
    }
    return h;
}

Status ForeignTable::copy_slot(std::uint32_t from, std::uint32_t to)
{
    // Copied out first: `attach` grows the pool, and a span into it would dangle.
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> held;
    for (const Record& r : records_)
        if (r.slot == from)
            held.emplace_back(tags_[r.tag],
                              std::vector<std::uint8_t>(pool_.data() + r.start,
                                                        pool_.data() + r.start + r.bytes));
    for (const auto& [tag, bytes] : held)
        if (auto st = attach(to, tag, bytes); !st) return st;
    return ok();
}

ForeignTable::Tail ForeignTable::tail(std::uint32_t slot_total) const noexcept
{
    return Tail{slot_total, tags_.size(), pool_.size()};
}

void ForeignTable::truncate(const Tail& t)
{
    std::erase_if(records_, [&t](const Record& r) { return r.slot >= t.slot_total; });
    const bool tags_free =
        std::ranges::none_of(records_, [&t](const Record& r) { return r.tag >= t.tags; });
    if (tags_free && t.tags < tags_.size()) tags_.resize(t.tags);
    const bool pool_free = std::ranges::none_of(
        records_, [&t](const Record& r) { return r.start + r.bytes > t.pool; });
    if (pool_free && t.pool < pool_.size()) pool_.resize(t.pool);
}

void ForeignTable::clear()
{
    records_.clear();
    tags_.clear();
    pool_.clear();
}

} // namespace kentos::core
