// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/dash_store.hpp"

#include "kentos_cad/core/text.hpp"

#include <string>

namespace kentos::core {

DashStore::DashStore()
{
    patterns_.push_back(Entry{DashPattern{}, std::string{}});
}

Result<DashId> DashStore::intern(const DashPattern& pattern, std::string_view origin)
{
    if (pattern.count == 0) return kSolidDash;

    if (pattern.count > kMaxDashSegments)
        return err(ErrorCode::ValidationFailed,
                   "Çizgi deseni en çok " + std::to_string(kMaxDashSegments) +
                       " parça taşıyabilir, verilen: " + std::to_string(pattern.count) + ".");

    if (pattern.count % 2 != 0)
        return err(ErrorCode::ValidationFailed,
                   "Çizgi deseni çizgi ve boşluk çiftlerinden oluşur; tek sayıda parça "
                   "verildi: " +
                       std::to_string(pattern.count) + ".");

    for (std::uint8_t i = 0; i < pattern.count; ++i)
        if (pattern.lengths[i] == 0)
            return err(ErrorCode::ValidationFailed,
                       "Çizgi deseninin " + std::to_string(i + 1) +
                           ". parçası sıfır uzunlukta; sıfır uzunlukta bir çizgi ya da "
                           "boşluk desenin kendisini anlamsız kılar.");

    for (std::size_t i = 1; i < patterns_.size(); ++i)
        if (patterns_[i].pattern == pattern) return static_cast<DashId>(i);

    if (patterns_.size() > 0xFFFE)
        return err(ErrorCode::ValidationFailed, "Bir çizimde en çok 65535 çizgi tipi olabilir.");

    patterns_.push_back(Entry{pattern, std::string(origin)});
    return static_cast<DashId>(patterns_.size() - 1);
}

const DashPattern& DashStore::at(DashId id) const noexcept
{
    static const DashPattern kSolid{};
    return id < patterns_.size() ? patterns_[id].pattern : kSolid;
}

std::string_view DashStore::origin(DashId id) const
{
    return id < patterns_.size() ? std::string_view(patterns_[id].origin) : std::string_view{};
}

std::uint64_t DashStore::fold(std::uint64_t seed) const
{
    std::uint64_t h = seed;
    // From slot 1: the sentinel carries nothing, and folding it would move every
    // fingerprint written before line types existed.
    for (std::size_t i = 1; i < patterns_.size(); ++i) {
        const DashPattern& p = patterns_[i].pattern;
        h = fnv1a(std::string_view(reinterpret_cast<const char*>(p.lengths), sizeof(p.lengths)), h);
        h = fnv1a(std::string_view(reinterpret_cast<const char*>(&p.count), 1), h);
    }
    return h;
}

} // namespace kentos::core
