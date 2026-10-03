// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/layer_state.hpp"

#include "piricad/core/text.hpp"

namespace piricad::core {

const LayerState* LayerStateStore::find(std::string_view name) const
{
    for (const LayerState& s : states_)
        if (turkish_key_equals(s.name, name)) return &s;
    return nullptr;
}

Status LayerStateStore::upsert(LayerState state)
{
    if (state.name.empty())
        return err(ErrorCode::InvalidArgument, "Katman durumunun adı boş olamaz.");

    for (LayerState& existing : states_)
        if (turkish_key_equals(existing.name, state.name)) {
            existing = std::move(state);
            return ok();
        }
    states_.push_back(std::move(state));
    return ok();
}

bool LayerStateStore::remove(std::string_view name)
{
    for (auto it = states_.begin(); it != states_.end(); ++it)
        if (turkish_key_equals(it->name, name)) {
            states_.erase(it);
            return true;
        }
    return false;
}

std::uint64_t LayerStateStore::fold(std::uint64_t seed) const
{
    if (states_.empty()) return seed;

    std::uint64_t h = fnv1a_int(0x6b61746d616e6475LL, seed); ///< "katmandu": its own seed
    for (const LayerState& s : states_) {
        h = fnv1a_int(static_cast<std::int64_t>(s.name.size()), h);
        h = fnv1a(s.name, h);
        h = fnv1a_int(static_cast<std::int64_t>(s.rows.size()), h);
        for (const LayerStateRow& r : s.rows) {
            h = fnv1a_int(static_cast<std::int64_t>(raw(r.key)), h);
            h = fnv1a_int((r.visible ? 1 : 0) | (r.locked ? 2 : 0) | (r.plottable ? 4 : 0) |
                              (r.selectable ? 8 : 0),
                          h);
        }
    }
    return h;
}

} // namespace piricad::core
