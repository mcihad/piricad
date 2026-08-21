// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/command/input.hpp"

namespace piricad::command {

const char* origin_name(Origin o)
{
    switch (o) {
    case Origin::Gui: return "gui";
    case Origin::CommandLine: return "cli";
    case Origin::Script: return "script";
    case Origin::Ai: return "ai";
    case Origin::Batch: return "batch";
    case Origin::Test: return "test";
    }
    return "?";
}

std::optional<Value> ArgInputSource::take(const Param& param)
{
    const Value* v = args_.find(param.name);
    if (!v || v->empty()) {
        exhausted_ = true;
        return std::nullopt;
    }

    std::size_t* pos = nullptr;
    for (auto& [name, index] : cursor_) {
        if (name == param.name) {
            pos = &index;
            break;
        }
    }
    if (!pos) {
        cursor_.emplace_back(param.name, 0);
        pos = &cursor_.back().second;
    }

    // A list-valued argument is drained one element per request, so a command
    // loop reads it exactly as it reads successive mouse clicks (piricad.md §2.4).
    if (v->kind() == Value::Kind::PointList || v->kind() == Value::Kind::Point) {
        const auto& pts = v->as_points();
        if (*pos >= pts.size()) {
            exhausted_ = true;
            return std::nullopt;
        }
        return Value::point(pts[(*pos)++]);
    }

    if (v->kind() == Value::Kind::IdList) {
        const auto& ids = v->as_ids();
        if (param.kind == ParamKind::Selection) {
            if (*pos > 0) {
                exhausted_ = true;
                return std::nullopt;
            }
            ++(*pos);
            return *v;
        }
        if (*pos >= ids.size()) {
            exhausted_ = true;
            return std::nullopt;
        }
        return Value::integer(ids[(*pos)++]);
    }

    if (*pos > 0) {
        exhausted_ = true;
        return std::nullopt;
    }
    ++(*pos);
    return *v;
}

} // namespace piricad::command
