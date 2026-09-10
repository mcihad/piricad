// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/foreign_table.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <limits>

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

void ForeignTable::clear()
{
    records_.clear();
    tags_.clear();
    pool_.clear();
}

} // namespace kentos::core
