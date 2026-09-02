// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/selection.hpp"

#include <algorithm>

namespace kentos::command {

bool Selection::add(core::EntityKey key)
{
    if (key == core::EntityKey::None) return false;

    const auto at = std::lower_bound(keys_.begin(), keys_.end(), key);
    if (at != keys_.end() && *at == key) return false;

    keys_.insert(at, key);
    ++revision_;
    return true;
}

bool Selection::remove(core::EntityKey key)
{
    const auto at = std::lower_bound(keys_.begin(), keys_.end(), key);
    if (at == keys_.end() || *at != key) return false;

    keys_.erase(at);
    ++revision_;
    return true;
}

bool Selection::toggle(core::EntityKey key)
{
    if (remove(key)) return false;
    return add(key);
}

void Selection::clear()
{
    if (keys_.empty()) return;
    keys_.clear();
    ++revision_;
}

bool Selection::contains(core::EntityKey key) const
{
    return std::binary_search(keys_.begin(), keys_.end(), key);
}

} // namespace kentos::command
